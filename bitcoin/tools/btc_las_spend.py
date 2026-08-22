#!/usr/bin/env python3
"""Build a spend whose authorisation is a LAS signature verified BY THE NODE.

This is the Stage 3 counterpart of `btc_carriage.py`, and the difference is the whole
point: there the LAS bytes rode along while an ordinary Schnorr signature authorised the
spend; here nothing authorises the spend except `OP_CHECKLASSIGVERIFY` accepting the LAS
signature, so if the patched consensus rule does not work, the transaction does not settle.

    tapleaf:  <sha256(pk)> OP_CHECKLASSIGVERIFY OP_1
    witness:  [ sig_1 … sig_13, pk_1 … pk_9, <leaf script>, <control block> ]

WITNESS ORDER IS LOAD-BEARING. Witness items become the initial stack bottom-to-top, and
the leaf then pushes the commitment on top, so when the opcode runs the stack is

    top -> sha256(pk) | pk_9 … pk_1 | sig_13 … sig_1 <- bottom

which is exactly the order the opcode pops. Concatenation is witness order = byte order,
and every chunk length is fixed (520 B except the last of each object), so a short or
transposed chunk cannot silently shift the reconstruction into a different message.

TWO PHASES, because the message is a digest of the transaction:

    sighash   build the unsigned transaction, emit its BIP341 sighash (32 bytes)
    assemble  take the LAS signature over that sighash and finish the transaction

Between them the runner calls `las_btc_tool sign`, which signs under the SAME consensus
parameters the node verifies with. Splitting it this way keeps the signed message honest:
it is produced from the transaction, not chosen.

NEGATIVE CONTROLS. `--mutate` implements only mutations this script can actually perform,
each applied AFTER signing so the signature stays genuine and only what it commits to
moves. Controls that require a DIFFERENT SIGNATURE (one made for another transaction, or
over something that is not a sighash at all) are not listed here: they are produced by the
runner supplying a different `--sig`, because a mutation this file could not carry out
would otherwise sit in the list quietly doing nothing and reporting success.
"""
import argparse
import hashlib
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from btc_sizes import model_sizes  # noqa: E402
from btc_carriage import (  # noqa: E402
    MAX_ELEMENT, NO_CODESEPARATOR, SIGHASH_DEFAULT,
    attr_any, chunk, control_block, die, get_leaf, load_core_helpers,
)

OP_CHECKLASSIGVERIFY = 0xBB

# BIP341's suggested NUMS point, taken from Core's own `test_framework/key.py::H_POINT`
# rather than invented here: an arbitrary 32-byte hash is not guaranteed to be the
# x-coordinate of a curve point, and an internal key that is not a valid point would fail
# for a reason that has nothing to do with LAS.
NUMS_INTERNAL_KEY_HEX = "50929b74c1a04954b78b4b6035e97a5e078a5a0f28ec96d547bfee9ace803ac0"

# Mutations this script performs itself. `foreign_signature` and `non_sighash_msg` are
# deliberately absent — see the module docstring.
MUTATIONS = (
    "none",
    "output_amount",      # the payment's value changed after signing
    "output_recipient",   # the payment's destination changed after signing
    "input_outpoint",     # a DIFFERENT REAL coin spent than the one signed for
    "wrong_prevout_amt",  # sighash computed against a false input value
    "chunk_truncated",    # one witness chunk one byte short
    "chunk_reordered",    # two witness chunks transposed
    "wrong_pubkey",       # a key the output never committed to
)


def leaf_script(ns, pk_bytes):
    """`<sha256(pk)> OP_CHECKLASSIGVERIFY OP_1`.

    The opcode must be a `CScriptOp`: a bare `int` in a `CScript` is encoded as a NUMBER
    PUSH, so `0xBB` would silently become data rather than the opcode, and the script would
    fail for a reason that looks like a consensus bug.

    The output commits to a HASH of the public key, revealed only when spent — the P2WSH
    discipline, and the reason a 4,416-byte key costs nothing until the coin moves. The
    trailing OP_1 is required because OP_CHECKLASSIGVERIFY has VERIFY semantics: it consumes
    its operands and pushes nothing, while tapscript's cleanstack rule wants exactly one
    truthy element left.
    """
    h = hashlib.sha256(pk_bytes).digest()
    return ns["CScript"]([h, ns["CScriptOp"](OP_CHECKLASSIGVERIFY), ns["OP_1"]])


def taproot_for(ns, pk_bytes):
    internal = bytes.fromhex(NUMS_INTERNAL_KEY_HEX)
    script = leaf_script(ns, pk_bytes)
    info = ns["taproot_construct"](internal, [("las", script)])
    return info, script


def build_unsigned(ns, txid, vout, value_sat, fee_sat, dest_spk):
    """`dest_spk` is where the coin GOES, which is not in general where it came from.

    The single-leg experiment pays back to the same LAS address, because there the
    question is only whether the node verifies the signature. A swap leg pays its
    BENEFICIARY, and that difference is load-bearing: a leg that paid itself would
    settle without transferring anything, so two settled legs would evidence two
    accepted signatures rather than an exchange of coins.
    """
    tx = ns["CTransaction"]()
    for name in ("version", "nVersion"):
        if hasattr(tx, name):
            setattr(tx, name, 2)
            break
    else:
        die("CTransaction has neither .version nor .nVersion")
    tx.vin = [ns["CTxIn"](ns["COutPoint"](int(txid, 16), vout), b"", 0xFFFFFFFF)]
    out_value = value_sat - fee_sat
    if out_value <= 0:
        die("fee is not less than the input value")
    tx.vout = [ns["CTxOut"](out_value, dest_spk)]
    tx.wit.vtxinwit = [ns["CTxInWitness"]()]
    return tx


def sighash_for(ns, tx, spent_utxos, script, leaf_ver):
    for kw in ("leaf_script", "script"):
        try:
            return ns["TaprootSignatureHash"](
                tx, spent_utxos, SIGHASH_DEFAULT, input_index=0, scriptpath=True,
                leaf_ver=leaf_ver, codeseparator_pos=NO_CODESEPARATOR, **{kw: script})
        except TypeError:
            continue
    die("TaprootSignatureHash accepts neither `leaf_script=` nor `script=`")


def cmd_sighash(args, ns):
    pk = open(args.pk, "rb").read()
    info, script = taproot_for(ns, pk)
    spk, _ = attr_any(info, ["scriptPubKey", "script_pubkey"], "taproot scriptPubKey")
    leaf_ver = ns["LEAF_VERSION_TAPSCRIPT"]

    if args.address_only:
        outkey, _ = attr_any(info, ["output_pubkey", "output_key"], "taproot output key")
        return {"address": ns["output_key_to_p2tr"](outkey),
                "scriptPubKey_hex": bytes(spk).hex(),
                "pk_sha256": hashlib.sha256(pk).hexdigest(),
                "leaf_script_hex": bytes(script).hex(),
                "internal_key": NUMS_INTERNAL_KEY_HEX}

    # Absent `--dest-spk` the coin returns to the same LAS address, which is what the
    # single-leg experiment does and must keep doing byte-for-byte.
    dest_spk = bytes.fromhex(args.dest_spk) if args.dest_spk else bytes(spk)
    tx = build_unsigned(ns, args.txid, args.vout, args.value_sat, args.fee_sat, dest_spk)
    # The sighash uses the REAL prevout value, except where `wrong_prevout_amt`
    # deliberately lies — that control exists to show the node computes the sighash from
    # the chain's view of the input, not from the spender's claim about it.
    claimed = args.value_sat + (1000 if args.mutate == "wrong_prevout_amt" else 0)
    # The SPENT output keeps the LAS scriptPubKey whatever the destination is: BIP341
    # commits to the coin being spent, which is the LAS-locked one, not to where it goes.
    spent = [ns["CTxOut"](claimed, spk)]
    sh = sighash_for(ns, tx, spent, script, leaf_ver)
    open(args.out_sighash, "wb").write(sh)

    # `dest_spk_hex` is recorded so `assemble` rebuilds the SAME transaction. Were it
    # only a command-line flag, an assemble run that forgot it would silently build a
    # self-paying transaction whose sighash no longer matched the signature.
    state = {"txid": args.txid, "vout": args.vout,
             "value_sat": args.value_sat, "fee_sat": args.fee_sat,
             "sighash_hex": sh.hex(), "claimed_value_sat": claimed,
             "pk_path": os.path.abspath(args.pk), "mutate": args.mutate,
             "dest_spk_hex": dest_spk.hex(),
             "alt_txid": args.alt_txid, "alt_vout": args.alt_vout}
    open(args.state, "w").write(json.dumps(state, indent=2, sort_keys=True))
    return {"sighash": sh.hex(), "claimed_value_sat": claimed, "mutate": args.mutate,
            "dest_spk_hex": dest_spk.hex(),
            "pays_self": dest_spk == bytes(spk)}


def cmd_assemble(args, ns):
    state = json.load(open(args.state))
    # `--mutate` defaults to None here, NOT "none": a truthy default would silently shadow
    # whatever the sighash phase recorded, and mutations chosen at signing time
    # (wrong_prevout_amt) would be quietly downgraded to a clean build that then "passed".
    mutate = args.mutate if args.mutate is not None else state.get("mutate", "none")

    pk_committed = open(state["pk_path"], "rb").read()
    pk_presented = pk_committed
    if mutate == "wrong_pubkey":
        # Same length, different bytes: isolates the commitment check from the length checks.
        pk_presented = bytes((b + 1) & 0xFF for b in pk_committed)

    sig = open(args.sig, "rb").read()
    info, script = taproot_for(ns, pk_committed)
    spk, _ = attr_any(info, ["scriptPubKey", "script_pubkey"], "taproot scriptPubKey")
    leaf_ver = ns["LEAF_VERSION_TAPSCRIPT"]

    txid, vout = state["txid"], state["vout"]
    if mutate == "input_outpoint":
        # A DIFFERENT REAL OUTPUT, supplied by the runner. Pointing at a nonexistent
        # outpoint would be rejected as `missing-inputs` — a failure of bookkeeping, not of
        # signature binding — and would prove nothing about the sighash covering inputs.
        if not state.get("alt_txid"):
            die("input_outpoint needs --alt-txid/--alt-vout: a REAL second output, or the "
                "rejection would be missing-inputs rather than a signature failure")
        txid, vout = state["alt_txid"], int(state["alt_vout"])

    # From the STATE, never from a flag: see cmd_sighash. An older state file without the
    # field is a self-paying spend, which is what it meant when it was written.
    dest_spk = bytes.fromhex(state["dest_spk_hex"]) if state.get("dest_spk_hex") else bytes(spk)
    tx = build_unsigned(ns, txid, vout, state["value_sat"], state["fee_sat"], dest_spk)

    # These change the transaction AFTER the signature was produced, so the signature stays
    # genuine and only what it commits to has moved.
    if mutate == "output_amount":
        tx.vout[0].nValue -= 50000
    elif mutate == "output_recipient":
        # Mutates the DESTINATION the signature committed to, which is the beneficiary's
        # script on a swap leg. Flipping the LAS script here instead would leave the
        # output the signature actually covers untouched.
        b = bytearray(dest_spk)
        b[-1] ^= 0x01
        tx.vout[0].scriptPubKey = ns["CScript"](bytes(b))

    chunks = chunk(sig) + chunk(pk_presented)
    if mutate == "chunk_truncated":
        chunks[0] = chunks[0][:-1]
    elif mutate == "chunk_reordered":
        chunks[0], chunks[1] = chunks[1], chunks[0]

    control = control_block(info, get_leaf(info, "las"), leaf_ver)
    stack = chunks + [bytes(script), control]
    tx.wit.vtxinwit[0].scriptWitness.stack = stack

    raw = tx.serialize_with_witness()
    # The OUTPUT's script length, which is the destination's — a P2WPKH beneficiary is
    # 22 bytes against a P2TR's 34, and using the input's length would make the model
    # disagree with Core for a reason that has nothing to do with LAS.
    model = model_sizes([0], [len(tx.vout[0].scriptPubKey)], [[len(x) for x in stack]])
    if mutate == "none" and len(raw) != model["total_size"]:
        die("size model says %d bytes, Core serialised %d" % (model["total_size"], len(raw)))

    return {
        "raw_hex": raw.hex(), "model": model, "mutate": mutate,
        "dest_spk_hex": dest_spk.hex(),
        "pays_self": dest_spk == bytes(spk),
        "out_value_sat": tx.vout[0].nValue,
        "witness_items": len(stack),
        "witness_item_lengths": [len(x) for x in stack],
        "sig_chunks": len(chunk(sig)), "pk_chunks": len(chunk(pk_presented)),
        "sig_bytes": len(sig), "pk_bytes": len(pk_presented),
        "leaf_script_bytes": len(bytes(script)),
        "control_block_bytes": len(control),
        "chunk_width": MAX_ELEMENT,
        "authorised_by": "OP_CHECKLASSIGVERIFY — a LAS signature verified by consensus",
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("phase", choices=("sighash", "assemble"))
    ap.add_argument("--core-src", required=True)
    ap.add_argument("--pk")
    ap.add_argument("--sig")
    ap.add_argument("--txid")
    ap.add_argument("--vout", type=int)
    ap.add_argument("--value-sat", type=int)
    ap.add_argument("--fee-sat", type=int)
    ap.add_argument("--alt-txid", default=None)
    ap.add_argument("--alt-vout", type=int, default=None)
    # Where the coin goes. Omitted = back to the same LAS address, the single-leg
    # experiment's behaviour, which stays byte-for-byte what it was.
    ap.add_argument("--dest-spk", default=None,
                    help="destination scriptPubKey, hex (default: pay back to the LAS address)")
    ap.add_argument("--address-only", action="store_true")
    ap.add_argument("--out-sighash")
    ap.add_argument("--state")
    # default None, so `assemble` can tell "not specified" from an explicit "none".
    ap.add_argument("--mutate", choices=MUTATIONS, default=None)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    ns, used = load_core_helpers(args.core_src)
    if args.phase == "sighash":
        if not args.pk:
            die("--pk is required")
        if args.mutate is None:
            args.mutate = "none"
        if not args.address_only and not (args.out_sighash and args.state):
            die("--out-sighash and --state are required unless --address-only")
        # Checked HERE rather than at use: a malformed destination would otherwise be
        # signed over and only fail at assembly, after the signature exists.
        if args.dest_spk is not None:
            try:
                raw_dest = bytes.fromhex(args.dest_spk)
            except ValueError:
                die("--dest-spk is not valid hex")
            if not raw_dest:
                die("--dest-spk is empty; omit it to pay back to the LAS address")
        result = cmd_sighash(args, ns)
    else:
        if not (args.state and args.sig):
            die("--state and --sig are required for assemble")
        # `assemble` takes the destination from the state file, so accepting it here
        # would suggest it could be changed after signing, which it cannot.
        if args.dest_spk is not None:
            die("--dest-spk applies to the sighash phase; assemble reads the destination "
                "from --state, because the signature commits to it")
        result = cmd_assemble(args, ns)

    result["core_helpers_used"] = sorted(used)
    with open(args.out, "w") as f:
        json.dump(result, f, indent=2, sort_keys=True)
    print(json.dumps({k: v for k, v in result.items() if k != "raw_hex"},
                     indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
