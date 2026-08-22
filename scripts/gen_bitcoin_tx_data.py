#!/usr/bin/env python3
"""Project the Stage-2 swap's settled transaction onto Bitcoin's real wire format.

Meeting 8 asked a question the report could not answer: what does a transaction
actually *contain*, which of its components does the adaptor layer change, and
would the result fit in a real Bitcoin transaction? Answering it needs the
transaction broken down field by field, in Bitcoin's own serialisation, with
weight and virtual size -- the number a UTXO chain actually charges for.

This script computes that breakdown. It is deliberately *not* a benchmark:

  * the **object sizes** (signature, public key, per configuration) are parsed
    from a captured ``bench_swap`` log -- measured, never typed;
  * the **field layout** is Bitcoin's, from BIP141/BIP144/BIP341 and
    ``developer.bitcoin.org``, which is format rather than measurement;
  * everything else is arithmetic over those two, done here so that no byte
    count in the report is ever hand-computed.

The arithmetic is checked against two independently published figures before it
is allowed to emit anything: a 1-in/1-out P2WPKH spend is 110 vB and a 1-in/1-out
P2TR key-path spend is 111 vB. If either self-check fails the script refuses to
write, on the principle that a size model which cannot reproduce the two sizes
everyone already knows must not be trusted with the one nobody knows.

Usage::

    python3 scripts/gen_bitcoin_tx_data.py \
        --log evidence/stage2/latest/bench_swap.log \
        --out report/latex/generated/btcmacros.tex \
        --tab report/latex/generated/tab_btctx.tex

See ``docs/02-methodology/BITCOIN_TX_STRUCTURE.md`` for the sources and for the
narrative behind every constant used here.
"""

from __future__ import annotations

import argparse
import math
import re
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# Bitcoin format constants. Sources in docs/02-methodology/BITCOIN_TX_STRUCTURE.md.
# ---------------------------------------------------------------------------

VERSION_BYTES = 4
LOCKTIME_BYTES = 4
OUTPOINT_BYTES = 36          # 32-byte txid + 4-byte vout
SEQUENCE_BYTES = 4
VALUE_BYTES = 8
MARKER_FLAG_BYTES = 2        # BIP144: 0x00 0x01, present only when a witness is

SPK_P2WPKH = 22              # OP_0 <20-byte HASH160>
SPK_P2TR = 34                # OP_1 <32-byte x-only key>

# Consensus and policy limits (Bitcoin Core src/policy/policy.h, BIP141).
MAX_SCRIPT_ELEMENT_SIZE = 520          # consensus: max stack item
MAX_STANDARD_STACK_ITEM_SIZE = 80      # policy: P2WSH / tapscript stack item
MAX_STANDARD_TX_WEIGHT = 400_000       # policy: max weight Core relays or mines
MAX_BLOCK_WEIGHT = 4_000_000           # consensus

# Canonical reference spends, used as the self-check.
REF_P2WPKH_SIG = 71          # DER signature + sighash byte
REF_P2WPKH_PK = 33           # compressed public key
REF_P2TR_SIG = 64            # BIP340 Schnorr, SIGHASH_DEFAULT
EXPECT_P2WPKH_VSIZE = 110
EXPECT_P2TR_VSIZE = 111


def compact_size(n: int) -> int:
    """Byte length of a compactSize-encoded ``n`` (developer.bitcoin.org)."""
    if n <= 252:
        return 1
    if n <= 0xFFFF:
        return 3
    if n <= 0xFFFF_FFFF:
        return 5
    return 9


class TxProjection:
    """A 1-in/1-out SegWit spend, broken into the fields BIP144 serialises.

    ``witness_items`` is the input's witness stack: the objects a spender must
    reveal. For P2WPKH that is ``[signature, public key]``; for a P2TR key-path
    spend just ``[signature]``, the key being committed in the output itself.
    """

    def __init__(self, spk_bytes: int, witness_items: list[int],
                 n_in: int = 1, n_out: int = 1) -> None:
        self.spk_bytes = spk_bytes
        self.witness_items = [i for i in witness_items if i]
        self.n_in = n_in
        self.n_out = n_out

        # --- base (non-witness) serialisation: billed at 4 weight units/byte.
        self.f_version = VERSION_BYTES
        self.f_in_count = compact_size(n_in)
        self.f_inputs = n_in * (OUTPOINT_BYTES + compact_size(0) + SEQUENCE_BYTES)
        self.f_out_count = compact_size(n_out)
        self.f_outputs = n_out * (VALUE_BYTES + compact_size(spk_bytes) + spk_bytes)
        self.f_locktime = LOCKTIME_BYTES
        self.base = (self.f_version + self.f_in_count + self.f_inputs
                     + self.f_out_count + self.f_outputs + self.f_locktime)

        # --- witness serialisation: billed at 1 weight unit/byte.
        self.f_marker_flag = MARKER_FLAG_BYTES
        per_input = compact_size(len(self.witness_items))
        for item in self.witness_items:
            per_input += compact_size(item) + item
        self.f_witness = n_in * per_input
        self.witness = self.f_marker_flag + self.f_witness

    @property
    def total(self) -> int:
        return self.base + self.witness

    @property
    def weight(self) -> int:
        """BIP141: base_size * 3 + total_size."""
        return self.base * 3 + self.total

    @property
    def vsize(self) -> int:
        """BIP141: weight / 4, rounded up. This is what fees are charged on."""
        return math.ceil(self.weight / 4)

    @property
    def per_block(self) -> int:
        return MAX_BLOCK_WEIGHT // self.weight

    @property
    def pct_standard_limit(self) -> float:
        return 100.0 * self.weight / MAX_STANDARD_TX_WEIGHT

    @property
    def marginal_input_weight(self) -> int:
        """Weight of adding one more input: base bytes at 4 WU, witness at 1."""
        base_per_input = OUTPOINT_BYTES + compact_size(0) + SEQUENCE_BYTES
        return 4 * base_per_input + (self.f_witness // self.n_in)

    @property
    def max_inputs_standard(self) -> int:
        """How many such inputs fit under MAX_STANDARD_TX_WEIGHT."""
        fixed = self.weight - self.n_in * self.marginal_input_weight
        return (MAX_STANDARD_TX_WEIGHT - fixed) // self.marginal_input_weight


def self_check() -> None:
    """Refuse to run if the model cannot reproduce two known-good sizes."""
    p2wpkh = TxProjection(SPK_P2WPKH, [REF_P2WPKH_SIG, REF_P2WPKH_PK])
    p2tr = TxProjection(SPK_P2TR, [REF_P2TR_SIG])
    failures = []
    if p2wpkh.vsize != EXPECT_P2WPKH_VSIZE:
        failures.append(f"P2WPKH vsize {p2wpkh.vsize} != {EXPECT_P2WPKH_VSIZE}")
    if p2tr.vsize != EXPECT_P2TR_VSIZE:
        failures.append(f"P2TR vsize {p2tr.vsize} != {EXPECT_P2TR_VSIZE}")
    if failures:
        sys.exit("size model self-check FAILED: " + "; ".join(failures))


# ---------------------------------------------------------------------------
# Parsing the measured object sizes out of a captured bench_swap log.
# ---------------------------------------------------------------------------

CONFIG_RE = re.compile(r"^Configuration (\d+):", re.M)
PK_RE = re.compile(r"^\s*public key\s+(\d+) B", re.M)
SIG_RE = re.compile(r"^\s*signature\s+(\d+) B", re.M)
PRESIG_RE = re.compile(r"^\s*signature\s+\d+ B\s+pre-signature\s+(\d+) B", re.M)
STMT_RE = re.compile(r"^\s*statement\s+(\d+) B", re.M)

ROMAN = {1: "One", 2: "Two", 3: "Three"}


def parse_log(text: str) -> dict[int, dict]:
    """Extract per-configuration object sizes. Absent figures are fatal."""
    blocks: dict[int, dict] = {}
    marks = list(CONFIG_RE.finditer(text))
    if not marks:
        sys.exit("no 'Configuration N:' blocks found -- is this a bench_swap log?")
    for i, m in enumerate(marks):
        end = marks[i + 1].start() if i + 1 < len(marks) else len(text)
        body = text[m.start():end]
        idx = int(m.group(1))

        def one(rx: re.Pattern, what: str) -> int:
            hit = rx.search(body)
            if not hit:
                sys.exit(f"configuration {idx}: no {what} in the log")
            return int(hit.group(1))

        blocks[idx] = {
            "pk": one(PK_RE, "public key size"),
            "sig": one(SIG_RE, "signature size"),
            "presig": one(PRESIG_RE, "pre-signature size"),
            "statement": one(STMT_RE, "statement size"),
        }
    return blocks


# ---------------------------------------------------------------------------
# Emission
# ---------------------------------------------------------------------------

HEADER = (
    "% AUTO-GENERATED by scripts/gen_bitcoin_tx_data.py -- DO NOT EDIT.\n"
    "% Object sizes parsed from a captured bench_swap log; field layout from\n"
    "% BIP141/BIP144/BIP341; everything else derived. Never hand-type a size.\n"
    "% Sources and derivation: docs/02-methodology/BITCOIN_TX_STRUCTURE.md\n"
)


def fmt(n: int) -> str:
    """Thousands separators LaTeX will not break across a line."""
    return f"{n:,}".replace(",", "{,}")


def emit_macros(cfgs: dict[int, dict], ref: TxProjection,
                proj: dict[int, TxProjection]) -> str:
    out = [HEADER, ""]
    add = out.append

    add("% --- reference: an ordinary 1-in/1-out P2WPKH spend -----------------")
    add(r"\newcommand{\btcRefVsize}{%s}" % fmt(ref.vsize))
    add(r"\newcommand{\btcRefWeight}{%s}" % fmt(ref.weight))
    add(r"\newcommand{\btcRefTotal}{%s}" % fmt(ref.total))
    add(r"\newcommand{\btcRefBase}{%s}" % fmt(ref.base))
    add(r"\newcommand{\btcRefPerBlock}{%s}" % fmt(ref.per_block))
    add("")

    add("% --- per configuration ---------------------------------------------")
    for idx in sorted(proj):
        p, c, r = proj[idx], cfgs[idx], ROMAN[idx]
        add(r"\newcommand{\btc%sBase}{%s}" % (r, fmt(p.base)))
        add(r"\newcommand{\btc%sWitness}{%s}" % (r, fmt(p.witness)))
        add(r"\newcommand{\btc%sTotal}{%s}" % (r, fmt(p.total)))
        add(r"\newcommand{\btc%sWeight}{%s}" % (r, fmt(p.weight)))
        add(r"\newcommand{\btc%sVsize}{%s}" % (r, fmt(p.vsize)))
        add(r"\newcommand{\btc%sPerBlock}{%s}" % (r, fmt(p.per_block)))
        add(r"\newcommand{\btc%sPctStd}{%.1f}" % (r, p.pct_standard_limit))
        add(r"\newcommand{\btc%sSig}{%s}" % (r, fmt(c["sig"])))
        add(r"\newcommand{\btc%sPk}{%s}" % (r, fmt(c["pk"])))
        add("")

    # The headline: what the witness discount is worth. Compare the fully
    # post-quantum configuration against the classical one.
    pq, cl = proj[3], proj[1]
    add("% --- the witness discount, and the limits ---------------------------")
    add(r"\newcommand{\btcRawRatio}{%.1f}" % (pq.total / cl.total))
    add(r"\newcommand{\btcVsizeRatio}{%.1f}" % (pq.vsize / cl.vsize))
    add(r"\newcommand{\btcWeightRatio}{%.1f}" % (pq.weight / cl.weight))
    add(r"\newcommand{\btcDiscountSaving}{%.0f}" % (
        100.0 * (1 - (pq.vsize / cl.vsize) / (pq.total / cl.total))))
    add(r"\newcommand{\btcMaxStdWeight}{%s}" % fmt(MAX_STANDARD_TX_WEIGHT))
    add(r"\newcommand{\btcMaxBlockWeight}{%s}" % fmt(MAX_BLOCK_WEIGHT))
    add(r"\newcommand{\btcMaxElement}{%s}" % fmt(MAX_SCRIPT_ELEMENT_SIZE))
    add(r"\newcommand{\btcMaxStdElement}{%s}" % fmt(MAX_STANDARD_STACK_ITEM_SIZE))
    add(r"\newcommand{\btcSigOverElement}{%.1f}" % (
        cfgs[3]["sig"] / MAX_SCRIPT_ELEMENT_SIZE))
    add(r"\newcommand{\btcSigOverStdElement}{%.0f}" % (
        cfgs[3]["sig"] / MAX_STANDARD_STACK_ITEM_SIZE))
    add(r"\newcommand{\btcPkOverElement}{%.1f}" % (
        cfgs[3]["pk"] / MAX_SCRIPT_ELEMENT_SIZE))
    add(r"\newcommand{\btcMaxInputsStd}{%s}" % fmt(pq.max_inputs_standard))
    add("")
    return "\n".join(out) + "\n"


def emit_table(cfgs: dict[int, dict], proj: dict[int, TxProjection]) -> str:
    """The field-by-field breakdown: standard spend beside the LAS spend."""
    cl, pq = proj[1], proj[3]

    def row(label: str, a: int, b: int, indent: bool = True) -> str:
        lead = r"\quad " if indent else ""
        return f"    {lead}{label} & {fmt(a)} & {fmt(b)} \\\\"

    lines = [
        HEADER,
        r"% Field-by-field breakdown of one settled swap transaction.",
        r"\begin{tabular}{@{}l r r@{}}",
        r"  \toprule",
        r"  Field & Classical (B) & LAS (B) \\",
        r"  \midrule",
        r"  \multicolumn{3}{@{}l}{\textit{Base data --- billed at 4 weight units per byte}} \\",
        row(r"\texttt{version}", cl.f_version, pq.f_version),
        row(r"input count", cl.f_in_count, pq.f_in_count),
        row(r"input: outpoint $+$ empty \texttt{scriptSig} $+$ \texttt{sequence}",
            cl.f_inputs, pq.f_inputs),
        row(r"output count", cl.f_out_count, pq.f_out_count),
        row(r"output: value $+$ \texttt{scriptPubKey}", cl.f_outputs, pq.f_outputs),
        row(r"\texttt{locktime}", cl.f_locktime, pq.f_locktime),
        r"  \addlinespace[2pt]",
        r"    \quad\textbf{base size} & \textbf{%s} & \textbf{%s} \\" % (
            fmt(cl.base), fmt(pq.base)),
        r"  \midrule",
        r"  \multicolumn{3}{@{}l}{\textit{Witness data --- billed at 1 weight unit per byte}} \\",
        row(r"\texttt{marker} $+$ \texttt{flag}", cl.f_marker_flag, pq.f_marker_flag),
        row(r"witness stack: signature $\sigma$ $+$ public key",
            cl.f_witness, pq.f_witness),
        r"  \addlinespace[2pt]",
        r"    \quad\textbf{witness size} & \textbf{%s} & \textbf{%s} \\" % (
            fmt(cl.witness), fmt(pq.witness)),
        r"  \midrule",
        r"  \textbf{total size} & \textbf{%s} & \textbf{%s} \\" % (
            fmt(cl.total), fmt(pq.total)),
        r"  weight (WU) & %s & %s \\" % (fmt(cl.weight), fmt(pq.weight)),
        r"  \textbf{virtual size (vB)} & \textbf{%s} & \textbf{%s} \\" % (
            fmt(cl.vsize), fmt(pq.vsize)),
        r"  \bottomrule",
        r"\end{tabular}",
    ]
    return "\n".join(lines) + "\n"


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--log", required=True, type=Path,
                    help="captured bench_swap log (measured object sizes)")
    ap.add_argument("--out", required=True, type=Path, help="macro file to write")
    ap.add_argument("--tab", required=True, type=Path, help="table file to write")
    args = ap.parse_args()

    self_check()

    cfgs = parse_log(args.log.read_text(encoding="utf-8"))
    if set(cfgs) != {1, 2, 3}:
        sys.exit(f"expected configurations 1..3, found {sorted(cfgs)}")

    # Configuration 1 is an elliptic-curve spend: P2WPKH, key revealed in the
    # witness. Configurations 2 and 3 share one signature scheme, so they share
    # one projection: a Taproot-shaped output committing to a 32-byte hash of
    # the public key, which the witness then reveals alongside the signature.
    #
    # THE CLASSICAL WITNESS IS DER-SHAPED, NOT THE HARNESS'S 64 BYTES.
    # `bench_swap` reports libsecp256k1's 64-byte compact signature, which is the
    # right number for a communication measurement but is NOT a valid P2WPKH
    # witness item: Bitcoin requires DER plus a sighash byte. Projecting the
    # compact form understated the classical baseline and so inflated every
    # post-quantum ratio below.
    #
    # This is settled by measurement rather than by argument. In
    # `evidence/btc_regtest/latest`, a 1-in/1-out P2WPKH spend signed BY A REAL
    # BITCOIN CORE NODE reports total 191 B / weight 437 / vsize 110 vB — exactly
    # what REF_P2WPKH_SIG (71) produces here, and exactly what the self-check
    # already required. Use the reference witness shape; keep the measured pk.
    proj = {
        1: TxProjection(SPK_P2WPKH, [REF_P2WPKH_SIG, cfgs[1]["pk"]]),
        2: TxProjection(SPK_P2TR, [cfgs[2]["sig"], cfgs[2]["pk"]]),
        3: TxProjection(SPK_P2TR, [cfgs[3]["sig"], cfgs[3]["pk"]]),
    }
    ref = TxProjection(SPK_P2WPKH, [REF_P2WPKH_SIG, REF_P2WPKH_PK])

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(emit_macros(cfgs, ref, proj), encoding="utf-8")
    args.tab.write_text(emit_table(cfgs, proj), encoding="utf-8")

    print(f"self-check OK (P2WPKH {ref.vsize} vB, P2TR {EXPECT_P2TR_VSIZE} vB)")
    for idx in sorted(proj):
        p = proj[idx]
        print(f"  config {idx}: sig {cfgs[idx]['sig']:>5} B  pk {cfgs[idx]['pk']:>5} B"
              f"  -> total {p.total:>6} B  weight {p.weight:>6} WU"
              f"  vsize {p.vsize:>5} vB  ({p.pct_standard_limit:.2f}% of std limit)")
    print(f"wrote {args.out}")
    print(f"wrote {args.tab}")


if __name__ == "__main__":
    main()
