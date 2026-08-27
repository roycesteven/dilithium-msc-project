#!/usr/bin/env python3
"""Build the CARRIAGE transaction (A3): a real Bitcoin transaction whose witness carries a
real LAS signature and public key, spendable on a stock, unmodified Bitcoin Core regtest.

WHAT THIS DOES AND DOES NOT ESTABLISH
-------------------------------------
It establishes only that the bytes FIT and TRAVEL: that a transaction of this shape can be
constructed, serialised, offered to a node's policy checks, broadcast and mined, and what
its real `vsize`/`weight` are. Stock Bitcoin Script has no lattice verification, so the
spend here is authorised by an ORDINARY BIP340 Schnorr signature and the LAS bytes are
simply dropped from the stack. NOTHING HERE VERIFIES A LAS SIGNATURE, and no output of
this program may be described as if it did. Verification is Stage 3's question, against a
patched node.

PROVENANCE: the transaction is built with BITCOIN CORE'S OWN test/functional helpers —
keys, taproot construction, the BIP341 sighash, witness assembly and serialisation — never
with a re-implementation. Consensus-adjacent crypto written a second time is exactly where
a subtly wrong result still looks right. Which helpers were used is recorded in the output.

HOW VERSION DRIFT IS HANDLED, RATHER THAN GUESSED
-------------------------------------------------
Core's test framework is internal API: attribute names have changed across releases
(`CTransaction.nVersion` -> `.version`; `TaprootSignatureHash`'s script keyword; whether a
leaf carries a ready-made control block). Nothing here is written from memory of one
release. Every symbol and every attribute whose name has moved is resolved through
`load_core_helpers()` / `attr_any()` against the PINNED source tree, and a mismatch fails immediately
naming exactly what was looked for. The control block is assembled from its BIP341 parts
(`leaf_version | parity`, internal key, Merkle branch) rather than read from a convenience
attribute that may not exist. The txid is not computed here at all — the node reports it,
which removes the accessor question entirely.

WHY THE LAS BYTES ARE CHUNKED, AND WHY THEY ARE WITNESS ITEMS
-------------------------------------------------------------
`SIGNATURE_BYTES` (6,736) and `PUBLIC_KEY_BYTES` (4,416) each exceed the 520-byte stack
element size, so neither can be a single push, and 11,152 bytes of constants cannot sit in
a leaf script. They are carried as INITIAL WITNESS STACK ITEMS, chunked at 520 B:
ceil(6736/520) = 13 signature chunks and ceil(4416/520) = 9 public-key chunks, 22 in all.
Chunks of 520 B exceed the 80-byte tapscript standardness limit, which is precisely why the
runner asks two nodes — one at default policy, one permissive — and records both answers
rather than predicting either.

THE SIZE MODEL IS DELIBERATELY OURS
-----------------------------------
`vsize`/`weight` are recomputed here from BIP141/BIP144 field arithmetic over the ACTUAL
witness stack, not read from a Core helper. Comparing a Core helper against Core's own
consensus code would test nothing; the point is to validate THIS PROJECT'S size model —
the arithmetic `scripts/gen_bitcoin_tx_data.py` uses for the report — against a real client.

Usage:
  btc_carriage.py --core-src DIR --sig F --pk F --address-only --out J
  btc_carriage.py --core-src DIR --sig F --pk F --txid HEX --vout N \
                  --value-sat N --fee-sat N --out J
"""
import argparse
import hashlib
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from btc_sizes import model_sizes  # noqa: E402

MAX_ELEMENT = 520  # MAX_SCRIPT_ELEMENT_SIZE (Bitcoin Core src/script/script.h): chunk width
SIGHASH_DEFAULT = 0  # BIP341: signs all inputs and outputs; no sighash-type byte appended
NO_CODESEPARATOR = 0xFFFFFFFF  # BIP341 `codesep_pos` when no OP_CODESEPARATOR was executed


def die(msg):
    print("FATAL: " + msg, file=sys.stderr)
    sys.exit(1)


def attr_any(obj, names, what):
    """First attribute of `obj` present among `names`, else a precise failure.

    Used for every name that has differed between Core releases, so a pinned tree whose
    API does not match fails HERE, saying what was looked for, instead of surfacing as a
    confusing error later.
    """
    for n in names:
        if hasattr(obj, n):
            return getattr(obj, n), n
    die("%s: none of %s exists on %s in the pinned Bitcoin Core source tree.\n"
        "       The bitcoind binary and the source tag must be the SAME release; see\n"
        "       bitcoin/README.md." % (what, "/".join(names), type(obj).__name__))


def set_attr_any(obj, names, value, what):
    for n in names:
        if hasattr(obj, n):
            setattr(obj, n, value)
            return n
    die("%s: none of %s exists on %s in the pinned Bitcoin Core source tree."
        % (what, "/".join(names), type(obj).__name__))


def load_core_helpers(core_src):
    """Import exactly the Core helpers used, naming any that are missing."""
    fdir = os.path.join(core_src, "test", "functional")
    if not os.path.isdir(fdir):
        die("no test/functional under %s — is --core-src a Bitcoin Core source tree?" % core_src)
    sys.path.insert(0, fdir)

    ns, used, missing = {}, [], []

    def take(module, names):
        try:
            m = __import__(module, fromlist=names)
        except Exception as exc:  # noqa: BLE001 — report, never mask
            missing.append("%s (import failed: %s)" % (module, exc))
            return
        for n in names:
            if hasattr(m, n):
                ns[n] = getattr(m, n)
                used.append("%s.%s" % (module, n))
            else:
                missing.append("%s.%s" % (module, n))

    take("test_framework.messages",
         ["CTransaction", "CTxIn", "CTxOut", "COutPoint", "CTxInWitness"])
    take("test_framework.script",
         ["CScript", "CScriptOp", "taproot_construct", "TaprootSignatureHash",
          "LEAF_VERSION_TAPSCRIPT", "OP_DROP", "OP_CHECKSIG", "OP_1",
          # Needed only by btc_las_spend.py's refund leaf. Listed here because
          # `load_core_helpers` is the one place that states what the pinned Core tree must
          # expose, and a symbol missing from it must fail with that named diagnosis rather
          # than as a KeyError deep inside script construction.
          "OP_CHECKLOCKTIMEVERIFY"])
    take("test_framework.key", ["ECKey", "compute_xonly_pubkey", "sign_schnorr"])
    take("test_framework.address", ["output_key_to_p2tr"])

    if missing:
        die("this Bitcoin Core source tree does not expose the helpers this script needs.\n"
            "       Missing: %s\n"
            "       The pinned source tag and the bitcoind binary must be the SAME release;\n"
            "       see bitcoin/README.md." % ", ".join(missing))
    return ns, used


# --------------------------------------------------------------------------- size model
# `compact_size` / `model_sizes` live in btc_sizes.py so btc_model_check.py can reuse
# the SAME arithmetic without importing Core's test framework. One definition only:
# two copies of a size model is how the checked model and the reported model drift.


def chunk(data, width=MAX_ELEMENT):
    return [data[i:i + width] for i in range(0, len(data), width)]


# --------------------------------------------------------------------------- construction


def derive_key(ns, seed=b"las-carriage-internal-key-v1"):
    """A deterministic key, so a run is reproducible. It authorises the carriage spend and
    is unrelated to the LAS key pair."""
    priv = ns["ECKey"]()
    priv.set(hashlib.sha256(seed).digest(), True)
    xonly, _parity = ns["compute_xonly_pubkey"](priv.get_bytes())
    return priv, xonly


def leaf_script(ns, xonly_pub, n_chunks):
    """`n_chunks × OP_DROP` then `<pubkey> OP_CHECKSIG`.

    Witness order is [schnorr_sig, chunk_1 … chunk_n], so the chunks sit ABOVE the
    signature on the stack and the drops uncover it. The spend is authorised by an ordinary
    Schnorr signature — stock Script can do nothing else with the LAS bytes, and pretending
    otherwise is the one claim this program must never support.
    """
    return ns["CScript"]([ns["OP_DROP"]] * n_chunks + [xonly_pub, ns["OP_CHECKSIG"]])


def control_block(info, leaf, leaf_ver):
    """Assemble BIP341's control block from its parts.

    Deliberately NOT read from a convenience attribute: whether a leaf object carries a
    ready-made control block has differed across Core releases. The parts themselves are
    fixed by BIP341 — `leaf_version | output-key parity`, the 32-byte internal key, then
    the Merkle branch — so building it from them is both version-robust and checkable
    against the spec.
    """
    parity, _ = attr_any(info, ["negflag", "parity"], "taproot output-key parity")
    internal, _ = attr_any(info, ["internal_pubkey", "inner_pubkey"], "taproot internal key")
    branch, _ = attr_any(leaf, ["merklebranch", "merkle_branch"], "taproot Merkle branch")
    version = getattr(leaf, "version", leaf_ver)
    if len(internal) != 32:
        die("taproot internal key is %d bytes, expected 32 (x-only)" % len(internal))
    if len(branch) % 32 != 0:
        die("taproot Merkle branch is %d bytes, not a multiple of 32" % len(branch))
    return bytes([version | int(bool(parity))]) + bytes(internal) + bytes(branch)


def get_leaf(info, name):
    leaves, _ = attr_any(info, ["leaves"], "taproot leaves")
    if name not in leaves:
        die("taproot leaf %r missing; taproot_construct returned %s" % (name, list(leaves)))
    return leaves[name]


def build(args, ns):
    sig = open(args.sig, "rb").read()
    pk = open(args.pk, "rb").read()
    if not sig or not pk:
        die("empty LAS signature or public key file")

    chunks = chunk(sig) + chunk(pk)
    priv, xonly = derive_key(ns)
    script = leaf_script(ns, xonly, len(chunks))
    leaf_ver = ns["LEAF_VERSION_TAPSCRIPT"]

    # taproot_construct does BIP341's tweaking and Merkle bookkeeping with Core's own code.
    info = ns["taproot_construct"](xonly, [("carriage", script)])
    spk, _ = attr_any(info, ["scriptPubKey", "script_pubkey"], "taproot scriptPubKey")
    outkey, _ = attr_any(info, ["output_pubkey", "output_key"], "taproot output key")

    common = {
        "chunks": len(chunks),
        "sig_chunks": len(chunk(sig)),
        "pk_chunks": len(chunk(pk)),
        "sig_bytes": len(sig),
        "pk_bytes": len(pk),
        "chunk_width": MAX_ELEMENT,
        "leaf_script_bytes": len(bytes(script)),
        "authorised_by": "ordinary BIP340 Schnorr signature — NOT a LAS verification",
    }

    if args.address_only:
        common.update({
            "address": ns["output_key_to_p2tr"](outkey),
            "scriptPubKey_hex": bytes(spk).hex(),
        })
        return common

    for need in ("txid", "vout", "value_sat", "fee_sat"):
        if getattr(args, need) is None:
            die("--%s is required unless --address-only" % need.replace("_", "-"))

    tx = ns["CTransaction"]()
    # Renamed between releases; resolved rather than assumed.
    ver_field = set_attr_any(tx, ["version", "nVersion"], 2, "CTransaction version field")
    tx.vin = [ns["CTxIn"](ns["COutPoint"](int(args.txid, 16), args.vout), b"", 0xFFFFFFFF)]
    out_value = args.value_sat - args.fee_sat
    if out_value <= 0:
        die("fee (%d sat) is not less than the input value (%d sat)"
            % (args.fee_sat, args.value_sat))
    tx.vout = [ns["CTxOut"](out_value, spk)]
    tx.wit.vtxinwit = [ns["CTxInWitness"]()]

    # BIP341 sighash, script path, SIGHASH_DEFAULT — Core's own implementation. The leaf
    # keyword has been spelled differently across releases, so try each and report if none
    # is accepted, rather than silently signing the wrong message.
    #
    # `codeseparator_pos` is passed EXPLICITLY as BIP341's sentinel for "no
    # OP_CODESEPARATOR was executed", 0xFFFFFFFF. The framework's own default is -1, which
    # this release then encodes with `to_bytes(4, "little", signed=False)` and raises
    # OverflowError on. Relying on the default would be relying on a bug; the spec value is
    # both correct and version-stable.
    spent = [ns["CTxOut"](args.value_sat, spk)]
    sighash = None
    sighash_kw = None
    for kw in ("leaf_script", "script"):
        try:
            sighash = ns["TaprootSignatureHash"](
                tx, spent, SIGHASH_DEFAULT, input_index=0,
                scriptpath=True, leaf_ver=leaf_ver,
                codeseparator_pos=NO_CODESEPARATOR, **{kw: script},
            )
            sighash_kw = kw
            break
        except TypeError:
            continue
    if sighash is None:
        die("TaprootSignatureHash in the pinned source accepts neither `leaf_script=` nor "
            "`script=` for a script-path sighash — the pinned tag's signature has changed.")
    if len(sighash) != 32:
        die("BIP341 sighash is %d bytes, expected 32" % len(sighash))

    schnorr_sig = ns["sign_schnorr"](priv.get_bytes(), sighash)
    control = control_block(info, get_leaf(info, "carriage"), leaf_ver)

    stack = [schnorr_sig] + chunks + [bytes(script), control]
    tx.wit.vtxinwit[0].scriptWitness.stack = stack

    raw = tx.serialize_with_witness()
    model = model_sizes([0], [len(bytes(spk))], [[len(x) for x in stack]])
    if len(raw) != model["total_size"]:
        die("our size model says total_size=%d but Core serialised %d bytes — the model is "
            "wrong and no size claim from this run would be trustworthy"
            % (model["total_size"], len(raw)))

    common.update({
        "raw_hex": raw.hex(),
        "model": model,
        "witness_items": len(stack),
        "witness_item_lengths": [len(x) for x in stack],
        "control_block_bytes": len(control),
        "schnorr_sig_bytes": len(schnorr_sig),
        "sighash_type": "SIGHASH_DEFAULT (BIP341)",
        "resolved_api": {
            "tx_version_field": ver_field,
            "taproot_sighash_leaf_kwarg": sighash_kw,
        },
        # The txid is deliberately NOT computed here: the node reports it, which removes a
        # version-dependent accessor from this script entirely.
        "txid": None,
    })
    return common


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--core-src", required=True, help="pinned Bitcoin Core source tree")
    ap.add_argument("--sig", required=True, help="packed LAS signature (raw bytes)")
    ap.add_argument("--pk", required=True, help="packed LAS public key (raw bytes)")
    ap.add_argument("--txid")
    ap.add_argument("--vout", type=int)
    ap.add_argument("--value-sat", type=int)
    ap.add_argument("--fee-sat", type=int)
    ap.add_argument("--address-only", action="store_true")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    ns, used = load_core_helpers(args.core_src)
    result = build(args, ns)
    result["core_helpers_used"] = sorted(used)
    result["core_src"] = os.path.abspath(args.core_src)
    with open(args.out, "w") as f:
        json.dump(result, f, indent=2, sort_keys=True)
    print(json.dumps({k: v for k, v in result.items() if k != "raw_hex"},
                     indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
