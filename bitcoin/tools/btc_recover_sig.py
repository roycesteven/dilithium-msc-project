#!/usr/bin/env python3
"""Recover a LAS signature from the witness of a MINED Bitcoin transaction.

This is the step that makes a two-leg swap a swap rather than two settlements. After u1
settles leg B, u2 must learn the witness `y` from what the LEDGER published — not from a
copy anyone handed it. So the bytes fed to `Ext` have to come back out of the mined
transaction, and this program is what pulls them out.

NOTHING HERE IS ASSUMED FROM THE LOCAL COPY
-------------------------------------------
The obvious implementation slices the first `SIGNATURE_BYTES` bytes of the concatenated
witness. That is exactly the mistake the EVM runner avoids when it decodes the ABI head
instead of using a constant offset: it would keep "working" if the layout changed, and it
would silently agree with a local expectation rather than reading the chain. Here the
split between signature and public key is recovered FROM THE TRANSACTION ITSELF:

  1. The last two witness items are the tapscript leaf and its BIP341 control block.
  2. The leaf must be exactly `<32-byte push> OP_CHECKLASSIGVERIFY OP_1`. That shape is
     checked, not assumed — it is also what establishes that the coin was locked by the
     new opcode (0xbb) and not by something else that happens to carry bytes.
  3. The 32 bytes the leaf pushes are `sha256(pk)`, the commitment the OUTPUT made when it
     was funded, long before this transaction existed.
  4. The remaining items concatenate to `sig || pk`. The split point is found by testing
     which suffix hashes to that commitment. The chain's own commitment therefore locates
     the public key, and everything before it is the signature.
  5. The recovered split is then re-chunked at 520 B and compared against the witness item
     lengths actually observed. If the framing the node carried is not the framing these
     two objects produce, the recovery is rejected rather than reported.

A UNIQUE split is required. Were two suffixes to hash to the commitment, the boundary
would be ambiguous and no honest answer exists; that is a failure, never a choice.

Note what is NOT claimed: this does not verify the signature. It recovers bytes. Whether
they satisfy the predicate is the node's answer, already given by the fact that the
transaction was mined under `OP_CHECKLASSIGVERIFY`.

Usage:
  btc_recover_sig.py --tx-json MINED.json --out-sig F --out-pk F --out REPORT.json
"""
import argparse
import hashlib
import json
import sys

MAX_ELEMENT = 520          # MAX_SCRIPT_ELEMENT_SIZE — the width witness chunks are cut at
OP_CHECKLASSIGVERIFY = 0xBB
OP_1 = 0x51
PUSH32 = 0x20              # a bare 32-byte data push


def die(msg):
    print("FATAL: " + msg, file=sys.stderr)
    sys.exit(1)


def chunk(data, width=MAX_ELEMENT):
    return [data[i:i + width] for i in range(0, len(data), width)]


def parse_leaf(leaf):
    """`<sha256(pk)> OP_CHECKLASSIGVERIFY OP_1`, or a precise failure.

    Checked byte by byte because this is what attributes the spend to the new rule. A leaf
    of some other shape means the coin was not locked by OP_CHECKLASSIGVERIFY, and any
    bytes recovered from such a transaction would say nothing about LAS verification.
    """
    if len(leaf) != 35:
        die("tapleaf is %d bytes, expected 35 for `<32-byte push> OP_CHECKLASSIGVERIFY "
            "OP_1` — this spend was not locked by the LAS rule" % len(leaf))
    if leaf[0] != PUSH32:
        die("tapleaf does not begin with a 32-byte push (first byte 0x%02x)" % leaf[0])
    if leaf[33] != OP_CHECKLASSIGVERIFY:
        die("tapleaf opcode is 0x%02x, not OP_CHECKLASSIGVERIFY (0x%02x): the spend was "
            "not authorised by the patched consensus rule" % (leaf[33], OP_CHECKLASSIGVERIFY))
    if leaf[34] != OP_1:
        die("tapleaf does not end with OP_1 (last byte 0x%02x)" % leaf[34])
    return leaf[1:33]


def recover(items):
    """Split `sig || pk` using only the commitment the leaf carries."""
    if len(items) < 3:
        die("witness has %d items; a LAS spend carries chunks plus leaf plus control block"
            % len(items))
    control, leaf = items[-1], items[-2]
    if len(control) % 32 != 1:
        die("control block is %d bytes, not 1 + 32k as BIP341 requires" % len(control))
    committed = parse_leaf(leaf)

    data_items = items[:-2]
    oversized = [i for i, it in enumerate(data_items) if len(it) > MAX_ELEMENT]
    if oversized:
        die("witness items %s exceed the %d-byte stack element limit" % (oversized, MAX_ELEMENT))
    data = b"".join(data_items)

    # Every split is tried; the commitment decides. Cheap at this size, and it keeps the
    # boundary a fact read off the chain rather than a length known in advance.
    splits = [i for i in range(len(data) + 1)
              if hashlib.sha256(data[i:]).digest() == committed]
    if not splits:
        die("no suffix of the witness data hashes to the public-key commitment the output "
            "made (sha256 %s) — the witness does not carry the committed key" % committed.hex())
    if len(splits) > 1:
        die("the public-key boundary is ambiguous: %d suffixes hash to the commitment" % len(splits))

    i = splits[0]
    sig, pk = data[:i], data[i:]
    if not sig:
        die("the recovered signature is empty — the witness carried the key and nothing else")

    # The framing must be the one these two objects produce. This is what rules out a
    # coincidental split that happens to satisfy the hash but does not match how the
    # transaction was actually built.
    expected = [len(c) for c in chunk(sig)] + [len(c) for c in chunk(pk)]
    observed = [len(x) for x in data_items]
    if expected != observed:
        die("recovered objects re-chunk to %s but the witness carried %s — the recovered "
            "split is not the framing the transaction used" % (expected, observed))
    return sig, pk, committed, i


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--tx-json", required=True,
                    help="getrawtransaction <txid> true <blockhash> output")
    ap.add_argument("--out-sig", required=True)
    ap.add_argument("--out-pk", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--vin", type=int, default=0)
    args = ap.parse_args()

    tx = json.load(open(args.tx_json))

    # An unconfirmed transaction is not the ledger's word. Reading a signature out of one
    # would prove only that this run built it, which is the thing being tested.
    if not tx.get("blockhash"):
        die("the transaction carries no blockhash — it is not mined, so its witness is "
            "not something the chain published")
    if int(tx.get("confirmations", 0)) < 1:
        die("the transaction reports %s confirmations" % tx.get("confirmations"))

    vin = tx["vin"]
    if args.vin >= len(vin):
        die("input %d requested; the transaction has %d" % (args.vin, len(vin)))
    wit = vin[args.vin].get("txinwitness")
    if not wit:
        die("input %d carries no witness" % args.vin)

    items = [bytes.fromhex(h) for h in wit]
    sig, pk, committed, split = recover(items)

    open(args.out_sig, "wb").write(sig)
    open(args.out_pk, "wb").write(pk)

    report = {
        "txid": tx.get("txid"),
        "blockhash": tx.get("blockhash"),
        "confirmations": tx.get("confirmations"),
        "vin": args.vin,
        "witness_items": len(items),
        "witness_item_lengths": [len(x) for x in items],
        "tapleaf_hex": items[-2].hex(),
        "control_block_bytes": len(items[-1]),
        "pk_commitment_sha256": committed.hex(),
        "split_offset": split,
        "sig_bytes": len(sig),
        "pk_bytes": len(pk),
        "sig_sha256": hashlib.sha256(sig).hexdigest(),
        "pk_sha256": hashlib.sha256(pk).hexdigest(),
        "opcode": "OP_CHECKLASSIGVERIFY (0xbb) — verified present in the tapleaf",
        "boundary_source": "sha256(pk) committed by the funding output's tapleaf, "
                           "not a locally known signature length",
    }
    with open(args.out, "w") as f:
        json.dump(report, f, indent=2, sort_keys=True)
    print(json.dumps(report, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
