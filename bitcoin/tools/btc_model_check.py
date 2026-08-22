#!/usr/bin/env python3
"""Check this project's size model against a real client's answer for one transaction.

Reads a `decoderawtransaction` result, recomputes base/total/weight/vsize from the field
LENGTHS alone with `btc_sizes.model_sizes`, and compares against the `vsize`/`weight` the
node reported. The node's numbers are never used as an input to the model — that would make
the comparison circular — only as the thing being matched.

This is the gate the carriage experiment turns on. It is NOT "does A1 equal 110 vB": a DER
signature's length varies (low-S, leading zeros), so the reference constant is not the right
oracle. A real client's answer for THIS transaction is.

Exit status 0 on agreement, 1 on disagreement (with the differing fields named).

Usage: btc_model_check.py --decoded FILE.json --label A1 [--json-out FILE]
"""
import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from btc_sizes import model_sizes, shape_from_decoded  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--decoded", required=True, help="decoderawtransaction JSON")
    ap.add_argument("--label", required=True)
    ap.add_argument("--json-out")
    args = ap.parse_args()

    decoded = json.load(open(args.decoded))
    script_sigs, spks, witnesses = shape_from_decoded(decoded)
    model = model_sizes(script_sigs, spks, witnesses)

    node = {
        "vsize": decoded.get("vsize"),
        "weight": decoded.get("weight"),
        "total_size": decoded.get("size"),
    }

    print("[%s] inputs=%d outputs=%d witness_items=%s"
          % (args.label, len(script_sigs), len(spks), [len(w) for w in witnesses]))
    rows, mismatched = [], []
    for field in ("total_size", "weight", "vsize"):
        m, n = model[field], node.get(field)
        ok = (n is not None and int(n) == int(m))
        rows.append((field, m, n, ok))
        if n is None:
            mismatched.append("%s: the node did not report it" % field)
        elif not ok:
            mismatched.append("%s: model %d, node %d" % (field, m, int(n)))
        print("  %-11s model %-8d node %-8s %s"
              % (field, m, n, "OK" if ok else "MISMATCH"))

    if args.json_out:
        with open(args.json_out, "w") as f:
            json.dump({"label": args.label, "model": model, "node": node,
                       "agree": not mismatched,
                       "witness_item_lengths": witnesses,
                       "script_sig_lengths": script_sigs,
                       "script_pubkey_lengths": spks},
                      f, indent=2, sort_keys=True)

    if mismatched:
        print("\n[%s] SIZE MODEL DISAGREES WITH THE CLIENT:" % args.label)
        for m in mismatched:
            print("  - " + m)
        print("The report's byte counts come from this model, so a disagreement means no")
        print("size claim from this run may be used.")
        return 1
    print("[%s] size model agrees with the client" % args.label)
    return 0


if __name__ == "__main__":
    sys.exit(main())
