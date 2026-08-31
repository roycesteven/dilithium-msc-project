#!/usr/bin/env python3
"""Field-by-field transaction sizes taken from MINED transactions, not projected.

Royce, 2026-08-30: chapter 3 may contain no projected number. The projection in
gen_bitcoin_tx_data.py laid measured OBJECT sizes onto Bitcoin's wire format and computed
what a settlement would weigh; this reads what two real transactions actually weighed, as
the client reported them.

  classical  A1 from the carriage run  -- an ordinary P2WPKH payment
  LAS        leg A of the two-leg swap -- a settled swap leg on the patched node

⚠ SCOPE, and it travels with every figure: the LAS leg settled on a REGTEST node carrying
  the experimental verification rule. It is not a mainnet spend, and a patched node is not
  Bitcoin.

⚠ NO PER-BLOCK CAPACITY HERE, deliberately. `block_weight // tx_weight` is a bound on what
  a block could hold, not something measured -- no block was mined containing 344 swap
  legs. Under the no-projection rule it does not belong in this chapter, and neither does
  any "N settlements per block" sentence built from it.

⚠ Both legs pay to a P2WPKH output, so the two transactions have the SAME 82 B base. The
  projection had assumed a taproot output for the LAS column (94 B base, 43 B output) and
  so reported an output that grows; on what was actually mined it does not. Royce ruled
  2026-08-30 to report what was mined and drop that claim, so no row here may reintroduce
  it. The measurement shows something stronger: the base is byte-identical, and the entire
  post-quantum penalty is witness data.

⚠ Two quantities share the word "witness" and must not be merged: the witness STACK (the
  serialized items -- their count varint, each CompactSize length, and for the LAS leg the
  tapscript and control block as well as the signature and key, so never label it
  "sigma + pk"), and the witness SIZE that BIP141 discounts, which is the stack plus
  BIP144's two-byte marker and flag. Every total below is asserted to close.
"""
import argparse
import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def die(msg: str) -> None:
    sys.exit(f"gen_btc_measured_tx: {msg}")


def fmt(n: int) -> str:
    return f"{n:,}".replace(",", "{,}")


def measure(path: pathlib.Path, what: str) -> dict:
    """Every field of one mined transaction, from the client's own decoding."""
    tx = json.loads(path.read_text())
    for k in ("vsize", "weight", "size", "vin", "vout"):
        if k not in tx:
            die(f"{what}: {path.name} has no '{k}' -- not a decoded mined transaction")
    if len(tx["vin"]) != 1 or len(tx["vout"]) != 1:
        die(f"{what}: expected one input and one output, got "
            f"{len(tx['vin'])}/{len(tx['vout'])}; the field table assumes the simple shape")

    spk = len(tx["vout"][0]["scriptPubKey"]["hex"]) // 2
    field = {
        "version": 4,
        "in_count": 1,
        "input": 32 + 4 + 1 + 4,          # outpoint, index, empty scriptSig len, sequence
        "out_count": 1,
        "output": 8 + 1 + spk,            # value, compactSize, scriptPubKey
        "locktime": 4,
    }
    base = sum(field.values())

    # The client reports size and weight independently; BIP141 fixes weight = 3*base+size,
    # so this is a real cross-check of the field arithmetic, not a restatement of it.
    if 3 * base + tx["size"] != tx["weight"]:
        die(f"{what}: fields sum to base {base}, but the client's weight {tx['weight']} "
            f"and size {tx['size']} imply {(tx['weight'] - tx['size']) / 3}. The field "
            "layout does not describe this transaction; do not emit it.")

    stack = tx["size"] - base - 2         # minus BIP144's marker and flag
    if base + 2 + stack != tx["size"]:
        die(f"{what}: base + marker/flag + stack != size")

    return dict(field, base=base, stack=stack, witness=stack + 2, spk=spk,
                total=tx["size"], weight=tx["weight"], vsize=tx["vsize"],
                items=len(tx["vin"][0].get("txinwitness", [])))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--classical", type=pathlib.Path,
                    default=ROOT / "evidence/btc_regtest/20260806_142224/A1_mined.json")
    ap.add_argument("--las", type=pathlib.Path,
                    default=ROOT / "evidence/btc_twoleg/20260817_194046/legA_mined.json")
    ap.add_argument("--macros", type=pathlib.Path,
                    default=ROOT / "report/latex/generated/btcmeasmacros.tex")
    ap.add_argument("--table", type=pathlib.Path,
                    default=ROOT / "report/latex/generated/tab_btctx_measured.tex")
    a = ap.parse_args()

    c = measure(a.classical, "classical")
    l = measure(a.las, "LAS")

    if c["base"] != l["base"]:
        die(f"the two bases differ ({c['base']} vs {l['base']}). The prose states they are "
            "byte-identical, so that sentence must be rewritten before these are emitted.")

    raw = l["total"] / c["total"]
    vsz = l["vsize"] / c["vsize"]

    m = [
        ("btcMeasClVsize", fmt(c["vsize"])), ("btcMeasClWeight", fmt(c["weight"])),
        ("btcMeasClTotal", fmt(c["total"])), ("btcMeasClStack", fmt(c["stack"])),
        ("btcMeasLasVsize", fmt(l["vsize"])), ("btcMeasLasWeight", fmt(l["weight"])),
        ("btcMeasLasTotal", fmt(l["total"])), ("btcMeasLasStack", fmt(l["stack"])),
        ("btcMeasBase", fmt(c["base"])),          # identical in both, asserted above
        # ratios are DERIVED FROM two measured transactions; the prose must say so
        ("btcMeasRawRatio", f"{raw:.1f}"),
        ("btcMeasVsizeRatio", f"{vsz:.1f}"),
        ("btcMeasDiscountSaving", f"{(1 - vsz / raw) * 100:.0f}"),
        # against Bitcoin's own standardness limit, a protocol constant, not a projection
        ("btcMeasPctStd", f"{l['weight'] / 400_000 * 100:.1f}"),
    ]
    a.macros.write_text(
        "% AUTO-GENERATED by scripts/gen_btc_measured_tx.py -- DO NOT EDIT.\n"
        "% Every figure is read from a MINED transaction as the client decoded it; the\n"
        "% ratios are derived from two such transactions and must be called derived.\n"
        "% The LAS leg settled on a regtest node carrying the experimental rule.\n"
        + "\n".join(f"\\newcommand{{\\{k}}}{{{v}}}" for k, v in m) + "\n")

    def row(label, x, y, bold=False):
        f = (lambda s: f"\\textbf{{{s}}}") if bold else (lambda s: s)
        return f"    \\quad {f(label)} & {f(fmt(x))} & {f(fmt(y))} \\\\\n"

    t = ["% AUTO-GENERATED by scripts/gen_btc_measured_tx.py -- DO NOT EDIT.\n",
         "\\begin{tabular}{@{}l r r@{}}\n  \\toprule\n",
         "  Field & Classical (B) & LAS (B) \\\\\n  \\midrule\n",
         "  \\multicolumn{3}{@{}l}{\\textit{Base data --- billed at 4 weight units per byte}} \\\\\n"]
    for lab, key in (("\\texttt{version}", "version"), ("input count", "in_count"),
                     ("input: outpoint $+$ empty \\texttt{scriptSig} $+$ \\texttt{sequence}", "input"),
                     ("output count", "out_count"),
                     ("output: value $+$ \\texttt{scriptPubKey}", "output"),
                     ("\\texttt{locktime}", "locktime")):
        t.append(row(lab, c[key], l[key]))
    t.append("  \\addlinespace[2pt]\n")
    t.append(row("base size", c["base"], l["base"], bold=True))
    t.append("  \\midrule\n  \\multicolumn{3}{@{}l}{\\textit{Witness data --- billed at 1 "
             "weight unit per byte}} \\\\\n")
    t.append(row("\\texttt{marker} $+$ \\texttt{flag}", 2, 2))
    t.append(row(f"witness stack ({c['items']} items / {l['items']} items)",
                 c["stack"], l["stack"]))
    t.append("  \\addlinespace[2pt]\n")
    t.append(row("witness size", c["witness"], l["witness"], bold=True))
    t.append("  \\midrule\n")
    t.append(f"  \\textbf{{total size}} & \\textbf{{{fmt(c['total'])}}} & "
             f"\\textbf{{{fmt(l['total'])}}} \\\\\n")
    t.append(f"  weight (WU) & {fmt(c['weight'])} & {fmt(l['weight'])} \\\\\n")
    t.append(f"  \\textbf{{virtual size (vB)}} & \\textbf{{{fmt(c['vsize'])}}} & "
             f"\\textbf{{{fmt(l['vsize'])}}} \\\\\n")
    t.append("  \\bottomrule\n\\end{tabular}\n")
    a.table.write_text("".join(t))

    print(f"classical {c['vsize']} vB / LAS {l['vsize']} vB, shared base {c['base']} B; "
          f"wrote {a.macros.name} and {a.table.name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
