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


# BIP65: an operand below this is a BLOCK HEIGHT, at or above it a UNIX TIME, and CLTV
# refuses to compare one against the other. Core's own name for it is LOCKTIME_THRESHOLD.
LOCKTIME_THRESHOLD = 500000000


def refund_leaf_script(ns, pk_bytes, locktime):
    """`<locktime> OP_CHECKLOCKTIMEVERIFY OP_DROP <sha256(pk)> OP_CHECKLASSIGVERIFY OP_1`.

    eprint 2020/845 Sec 4.1 requires this branch: "both transactions have timeouts t_i such
    that, once t_i elapses, u_i can redeem c_i if the counterparty does not continue to the
    exchange", with "the timelock, t_2, on u_2's transaction ... shorter (i.e., t_2 < t_1)".
    The claim leaf alone implements only the path where the swap completes.

    THE REFUND PATH IS ALSO LAS, DELIBERATELY. A secp256k1 Schnorr key would make a smaller,
    more conventional leaf, but it would put a classically-breakable authorisation on a coin
    this project exists to protect post-quantum: the recovery path would become the one
    thing Shor still reaches. So the same opcode guards both branches. WHICH key goes here
    is the caller's decision — see `taproot_for_refundable`.

    ABSOLUTE, NOT RELATIVE. `OP_CHECKLOCKTIMEVERIFY` (BIP65) compares against the
    transaction's `nLockTime`, an absolute deadline. `OP_CHECKSEQUENCEVERIFY` would count
    from confirmation, which happens at a different moment on each chain and cannot be
    compared with the other leg's deadline. Two legs on two chains need deadlines that mean
    the same thing on both.

    CLTV has VERIFY semantics and leaves its operand on the stack, hence the OP_DROP; the
    trailing OP_1 is there for the same cleanstack reason as the claim leaf.
    """
    if locktime <= 0:
        die("--refund-locktime must be positive; 0 is satisfied by every transaction and "
            "would make the refund branch spendable immediately")
    h = hashlib.sha256(pk_bytes).digest()
    return ns["CScript"]([locktime, ns["OP_CHECKLOCKTIMEVERIFY"], ns["OP_DROP"],
                          h, ns["CScriptOp"](OP_CHECKLASSIGVERIFY), ns["OP_1"]])


def taproot_for_refundable(ns, pk_bytes, refund_pk, refund_locktime):
    """The TWO-leaf tree: the normal LAS claim path plus a timelocked LAS refund path.

    A SEPARATE ENTRY POINT, NOT AN OPTION ON `taproot_for`. Adding a leaf changes the Merkle
    root, hence the output key, hence the ADDRESS. `run_btc_las_node.sh` and
    `run_btc_two_leg.sh` have pinned evidence against the single-leaf address, so
    `taproot_for` is left exactly as it was rather than given a mode flag — the same
    discipline that keeps `LASVerifierOptD2.sol` a copy of the D3 verifier instead of a
    parameterisation of it. A caller that does not ask for a refund branch cannot get a
    different address by accident, because it never reaches this function.

    The claim leaf keeps the name `las` in BOTH trees, so `get_leaf(info, "las")` selects
    the same branch either way and the claim path needs no special-casing.

    ⚠ KNOWN DEVIATION, AND IT MUST TRAVEL WITH ANY RESULT THIS PRODUCES. Because the claim
    leaf is spendable directly by the funder — it is a single-key check under the funder's
    own public key, that being the key the pre-signature was made under — the timelocked
    refund is NOT an exclusive recovery path. In a real Bitcoin network, observing the
    adapted signature before confirmation may also expose the witness while a conflicting
    funder spend is still possible: the funder can extract from the mempool-visible
    signature, replace the claiming transaction with one of its own, keep this coin AND go
    on to claim the other leg with the extracted witness. eprint 2020/845 Sec 4.1 analyses
    fairness from a "valid signature published on a blockchain", which abstracts that race
    away; a concrete Bitcoin implementation does not get to.

    Therefore this tree does not establish full atomic-swap fairness. It implements and
    lets us test an explicit timeout refund branch, and nothing here may be reported as
    more than that. Closing the gap needs a claim condition the funder cannot satisfy
    alone — a second LAS check under the claimant's key, which the consensus opcode does
    support, since it pops a FIXED 1 + n_pk + n_sig elements and therefore composes —
    deliberately NOT done here, and not claimed.
    """
    internal = bytes.fromhex(NUMS_INTERNAL_KEY_HEX)
    claim = leaf_script(ns, pk_bytes)
    refund = refund_leaf_script(ns, refund_pk, refund_locktime)
    info = ns["taproot_construct"](internal, [("las", claim), ("refund", refund)])
    return info, claim, refund


def build_unsigned(ns, txid, vout, value_sat, fee_sat, dest_spk,
                   locktime=0, sequence=0xFFFFFFFF):
    """`dest_spk` is where the coin GOES, which is not in general where it came from.

    The single-leg experiment pays back to the same LAS address, because there the
    question is only whether the node verifies the signature. A swap leg pays its
    BENEFICIARY, and that difference is load-bearing: a leg that paid itself would
    settle without transferring anything, so two settled legs would evidence two
    accepted signatures rather than an exchange of coins.

    `locktime`/`sequence` DEFAULT TO WHAT THIS FUNCTION ALWAYS PRODUCED, so every existing
    caller builds a byte-identical transaction and the pinned evidence does not move. Only
    the refund path overrides them, and it MUST override BOTH: BIP65 makes
    `OP_CHECKLOCKTIMEVERIFY` fail outright when the input is final (`nSequence` =
    0xFFFFFFFF), and it compares its operand against `nLockTime`, so a refund transaction
    left at these defaults could never satisfy the refund leaf no matter how much time had
    passed. Both fields are part of the sighash, so they must be set HERE, before the
    signature is made, and not patched in afterwards.
    """
    tx = ns["CTransaction"]()
    for name in ("version", "nVersion"):
        if hasattr(tx, name):
            setattr(tx, name, 2)
            break
    else:
        die("CTransaction has neither .version nor .nVersion")
    tx.nLockTime = locktime
    tx.vin = [ns["CTxIn"](ns["COutPoint"](int(txid, 16), vout), b"", sequence)]
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


def select_tree(ns, pk, refund_pk_path, refund_locktime, path):
    """Decide the tree AND the leaf in ONE place, for both phases.

    `sighash` signs one leaf's script and `assemble` must reveal that same leaf: if the two
    phases chose independently, a mismatch would surface as a signature that verifies
    against nothing, with no hint of which half was wrong. So both call this, `assemble`
    feeding it the values the state file recorded rather than its own flags.

    Returns (info, script, leaf_name, is_refund).
    """
    if refund_pk_path:
        refund_pk = open(refund_pk_path, "rb").read()
        info, claim, refund = taproot_for_refundable(ns, pk, refund_pk, refund_locktime)
        if path == "refund":
            return info, refund, "refund", True
        return info, claim, "las", False
    if path == "refund":
        die("--path refund needs --refund-pk and --refund-locktime: the single-leaf tree "
            "has no refund branch, and silently claiming instead would settle the swap "
            "rather than recover the coin")
    info, script = taproot_for(ns, pk)
    return info, script, "las", False


def cmd_sighash(args, ns):
    pk = open(args.pk, "rb").read()
    info, script, leaf_name, is_refund = select_tree(
        ns, pk, args.refund_pk, args.refund_locktime, args.path)
    spk, _ = attr_any(info, ["scriptPubKey", "script_pubkey"], "taproot scriptPubKey")
    leaf_ver = ns["LEAF_VERSION_TAPSCRIPT"]

    if args.address_only:
        outkey, _ = attr_any(info, ["output_pubkey", "output_key"], "taproot output key")
        return {"address": ns["output_key_to_p2tr"](outkey),
                "scriptPubKey_hex": bytes(spk).hex(),
                "pk_sha256": hashlib.sha256(pk).hexdigest(),
                "leaf_script_hex": bytes(script).hex(),
                "internal_key": NUMS_INTERNAL_KEY_HEX,
                "leaf": leaf_name,
                "refundable": bool(args.refund_pk),
                "refund_locktime": args.refund_locktime if args.refund_pk else None}

    # Absent `--dest-spk` the coin returns to the same LAS address, which is what the
    # single-leg experiment does and must keep doing byte-for-byte.
    dest_spk = bytes.fromhex(args.dest_spk) if args.dest_spk else bytes(spk)
    # BOTH fields, and BEFORE signing: BIP65 rejects a CLTV script outright when the input
    # is final, and compares its operand against nLockTime. Both are covered by the sighash,
    # so setting them afterwards would invalidate the signature rather than enable the spend.
    locktime, sequence = (args.refund_locktime, 0xFFFFFFFE) if is_refund else (0, 0xFFFFFFFF)
    # THE ONE CONTROL THAT NEEDS THESE TWO DECOUPLED. "Refund refused before the deadline"
    # has TWO possible causes and only one of them is about CLTV: with nLockTime == the
    # deadline the transaction is simply NOT FINAL yet, and block validation rejects it
    # before any script runs — which every node does, patched or not, so it evidences
    # nothing about the new leaf. Setting nLockTime BELOW the deadline instead makes the
    # transaction final (0 is final unconditionally) while CLTV still refuses it, which is
    # attributable to the refund leaf. Applied HERE, before the sighash, because nLockTime
    # is covered by it: patched in afterwards it would break the signature and the refusal
    # would be about that instead.
    if args.tx_locktime is not None:
        if not is_refund:
            die("--tx-locktime only means something on --path refund")
        locktime = args.tx_locktime
    tx = build_unsigned(ns, args.txid, args.vout, args.value_sat, args.fee_sat, dest_spk,
                        locktime=locktime, sequence=sequence)
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
    # The refund fields go into the STATE for the same reason `dest_spk_hex` does: an
    # assemble run that forgot the flag would otherwise rebuild a DIFFERENT transaction —
    # here one with no locktime and a final input — whose sighash no longer matches the
    # signature, and would report that as a signature failure.
    state = {"txid": args.txid, "vout": args.vout,
             "value_sat": args.value_sat, "fee_sat": args.fee_sat,
             "sighash_hex": sh.hex(), "claimed_value_sat": claimed,
             "pk_path": os.path.abspath(args.pk), "mutate": args.mutate,
             "dest_spk_hex": dest_spk.hex(),
             "alt_txid": args.alt_txid, "alt_vout": args.alt_vout,
             "path": args.path,
             "refund_pk_path": os.path.abspath(args.refund_pk) if args.refund_pk else None,
             "refund_locktime": args.refund_locktime,
             "locktime": locktime, "sequence": sequence}
    open(args.state, "w").write(json.dumps(state, indent=2, sort_keys=True))
    return {"sighash": sh.hex(), "claimed_value_sat": claimed, "mutate": args.mutate,
            "dest_spk_hex": dest_spk.hex(),
            "pays_self": dest_spk == bytes(spk),
            "leaf": leaf_name, "locktime": locktime, "sequence": sequence}


def cmd_assemble(args, ns):
    state = json.load(open(args.state))
    # `--mutate` defaults to None here, NOT "none": a truthy default would silently shadow
    # whatever the sighash phase recorded, and mutations chosen at signing time
    # (wrong_prevout_amt) would be quietly downgraded to a clean build that then "passed".
    mutate = args.mutate if args.mutate is not None else state.get("mutate", "none")

    pk_committed = open(state["pk_path"], "rb").read()

    sig = open(args.sig, "rb").read()
    # FROM THE STATE, never from a flag — and defaulting to the legacy single-leaf tree, so
    # a state file written before the refund branch existed still describes what it meant.
    # The TREE is always built from the claim key; `refund_pk_path` is a separate input.
    info, script, leaf_name, _is_refund = select_tree(
        ns, pk_committed, state.get("refund_pk_path"), state.get("refund_locktime"),
        state.get("path", "claim"))
    spk, _ = attr_any(info, ["scriptPubKey", "script_pubkey"], "taproot scriptPubKey")
    leaf_ver = ns["LEAF_VERSION_TAPSCRIPT"]

    # WHICH KEY THE WITNESS CARRIES FOLLOWS THE LEAF, because each leaf pushes its OWN
    # sha256 commitment and the opcode compares that against the key rebuilt from the
    # witness chunks. Presenting the claim key against the refund leaf fails the commitment
    # check — the refund would never spend, and the failure would look like a bad signature
    # rather than the wrong key. Chosen BEFORE `wrong_pubkey`, so that mutation corrupts
    # whichever key this path is supposed to present.
    if leaf_name == "refund":
        pk_presented = open(state["refund_pk_path"], "rb").read()
    else:
        pk_presented = pk_committed
    if mutate == "wrong_pubkey":
        # Same length, different bytes: isolates the commitment check from the length checks.
        pk_presented = bytes((b + 1) & 0xFF for b in pk_presented)

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
    tx = build_unsigned(ns, txid, vout, state["value_sat"], state["fee_sat"], dest_spk,
                        locktime=state.get("locktime", 0),
                        sequence=state.get("sequence", 0xFFFFFFFF))

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

    control = control_block(info, get_leaf(info, leaf_name), leaf_ver)
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
        "leaf": leaf_name,
        "locktime": tx.nLockTime,
        "sequence": tx.vin[0].nSequence,
        # Recorded rather than enforced: BIP65 reads the SAME number as a block height or a
        # UNIX time depending on which side of the threshold it falls, and only the caller
        # knows which it meant. Saying so in the evidence is what lets a reader check that
        # the deadline the runner chose is the kind of deadline it thought it chose.
        "locktime_kind": (None if not tx.nLockTime else
                          "unix-time" if tx.nLockTime >= LOCKTIME_THRESHOLD
                          else "block-height"),
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
    # The refund branch. ALL THREE ABSENT reproduces the single-leaf tree byte-for-byte,
    # which is what `run_btc_las_node.sh` and `run_btc_two_leg.sh` have pinned evidence
    # against, so their addresses cannot move because this option exists.
    ap.add_argument("--path", choices=("claim", "refund"), default="claim",
                    help="which tapleaf to spend; `refund` requires --refund-pk and "
                         "--refund-locktime")
    ap.add_argument("--refund-pk", default=None,
                    help="LAS public key of the party who recovers the coin after the "
                         "timeout. Its presence is what selects the two-leaf tree, and it "
                         "therefore CHANGES THE ADDRESS.")
    ap.add_argument("--refund-locktime", type=int, default=None,
                    help="absolute BIP65 deadline: a block height below %d, a UNIX time at "
                         "or above it. CLTV will not compare one kind against the other."
                         % LOCKTIME_THRESHOLD)
    ap.add_argument("--tx-locktime", type=int, default=None,
                    help="CONTROL ONLY: set the transaction's nLockTime independently of "
                         "the leaf's CLTV operand. Exists so a test can build a FINAL "
                         "transaction whose nLockTime is below the deadline, isolating "
                         "CLTV's refusal from the separate non-final rejection.")
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

    # THE PAIR IS ALL-OR-NOTHING, and the silent half is the dangerous one: `--refund-pk`
    # alone would reach `refund_leaf_script` with locktime None and die on a type error,
    # but `--refund-locktime` alone falls through to the single-leaf tree and is IGNORED —
    # the caller asks for a deadline and gets an output with no refund branch at all, with
    # nothing said. Refuse both halves here rather than let either happen.
    if bool(args.refund_pk) != (args.refund_locktime is not None):
        die("--refund-pk and --refund-locktime must be supplied together: one without the "
            "other would either fail obscurely or silently build a tree with no refund "
            "branch")

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
