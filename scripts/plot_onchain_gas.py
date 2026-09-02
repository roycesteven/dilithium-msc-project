#!/usr/bin/env python3
"""plot_onchain_gas.py -- the on-chain settlement-gas figure (Stage-2 result).

Renders report/latex/figures/fig_onchain.{pdf,png}: a horizontal, log-scaled
comparison of the three atomic-swap claim paths measured on a local EVM (Foundry
`forge test --gas-report`; gas is deterministic for the fixed bytecode, EVM revision,
inputs, and contract state, and independent of the host CPU), with the EIP-7825
per-transaction gas cap drawn as the threshold the full verifier overshoots.

The gas values are PARSED FROM A CAPTURED forge --gas-report LOG -- never typed into
this script. Hardcoding them is what let this figure drift out of step with the
implementation when the FIPS 204 c_tilde alignment changed the signature width, and
with it the calldata gas of claimLAS/claimLASVerified.

Usage (normally via scripts/run_onchain_gas.sh, which captures the log first):
    python3 scripts/plot_onchain_gas.py --log evidence/onchain/latest/gas_report.log
                                        [--out report/latex/figures]
"""
import sys
from pathlib import Path

import argparse
import re

EIP7825_CAP = 16_777_216  # per-transaction gas cap (2**24), a protocol constant


def parse_gas_report(path):
    """Pull the three claim-path costs out of a `forge test --gas-report` log.

    Rows look like:  | claimClassical | min | avg | median | max | #calls |
    We take the MAX column: the worst case a settlement can cost, which is the
    number the per-transaction cap must be judged against.
    """
    want = {"claimClassical": None, "claimLAS": None, "claimLASVerified": None,
            # OPTIONAL: the single-transaction verifier (LASVerifyOpt). Absent from
            # logs captured before it existed, so it must not be a hard requirement --
            # an old evidence log has to keep replotting.
            "claimLASVerifiedOpt": None}
    required = ("claimClassical", "claimLAS", "claimLASVerified")
    for line in path.read_text(errors="replace").splitlines():
        if not line.lstrip().startswith("|"):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) < 6 or cells[0] not in want:
            continue
        nums = [c for c in cells[1:] if re.fullmatch(r"[0-9]+", c)]
        if len(nums) >= 4:
            want[cells[0]] = int(nums[3])   # min, avg, median, MAX
    missing = [k for k in required if want[k] is None]
    if missing:
        raise SystemExit("plot_onchain_gas.py: %s not found in %s -- was the log "
                         "captured with `forge test --gas-report`?"
                         % (", ".join(missing), path))
    return (want["claimClassical"], want["claimLAS"], want["claimLASVerified"],
            want["claimLASVerifiedOpt"])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--log", required=True, type=Path,
                    help="captured `forge test --gas-report` output")
    ap.add_argument("--out", type=Path, default=None,
                    help="figure output directory (default: report/latex/figures)")
    args = ap.parse_args()
    CLASSICAL, LAS_FLOOR, LAS_VERIFY, LAS_VERIFY_OPT = parse_gas_report(args.log)

    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        from matplotlib.ticker import LogLocator, FuncFormatter
    except ImportError:
        sys.stderr.write("matplotlib not installed: pip install matplotlib\n")
        return 1

    # The only consumer of this figure is the report, so it is drawn at the report's
    # printed size: 12pt type (the body size -- no figure may carry type smaller than
    # the paragraph around it) on a canvas exactly \textwidth wide, saved UNCROPPED.
    # Cropping would let \includegraphics rescale the canvas and the type with it;
    # see the PRINT MODE note in scripts/plot_las_paper_figures.py.
    PRINT_PT = 12.0
    TEXT_WIDTH_IN = 5.71          # muthesis, 12pt option: \textwidth = 145mm
    plt.rcParams.update({
        # Same TYPEFACE as the body, not just the same size: 12pt DejaVu Sans has a
        # much larger x-height than the report's Latin Modern Roman and reads as
        # bigger than the paragraph beside it. lmodern is what report.tex loads.
        #
        # "Latin Modern Roman" is a family of OPTICAL SIZES -- lmroman9, 10, 12 and
        # 17 all answer to that one name and matplotlib picks among them, so the
        # face is NOT pinned by naming the family: on this machine the bare family
        # resolves to lmroman10, which at 12pt is drawn wider and heavier than the
        # body and is what made this figure read as larger than the paragraph.
        # The optical size must therefore be named.
        #
        # It is named "LM Roman 17", not the body's own lmroman12, because the
        # reference is the OTHER PLOTTED FIGURES, not the paragraph in isolation
        # (Royce, 2026-09-02): fig_timing / fig_overhead / fig_rejection_cdf all
        # carry lmroman17, so an lmroman12 figure sat visibly heavier than its
        # neighbours.  17 scaled to 12pt is the lighter, narrower of the two and is
        # the one Royce read as closest to the paragraph on the page.
        "font.family": "serif",
        "font.serif": ["LM Roman 17", "Latin Modern Roman", "DejaVu Serif"],
        "mathtext.fontset": "cm",
        "font.size": PRINT_PT, "axes.labelsize": PRINT_PT,
        "axes.titlesize": PRINT_PT,
        "xtick.labelsize": PRINT_PT, "ytick.labelsize": PRINT_PT,
        "legend.fontsize": PRINT_PT,
        "axes.spines.top": False, "axes.spines.right": False,
        "axes.grid": False, "figure.dpi": 200,
    })

    # rows top-to-bottom: classical, floor, [single-transaction verify], verify.
    # The optimised row is inserted only when the log carries it, so this script
    # still reproduces the pre-optimisation figure from an older evidence log.
    # Two lines: Latin Modern Roman is a good deal narrower than the sans face, so
    # "LAS full verify, optimised" fits on one line -- and keeping the rows at two
    # lines keeps the figure short enough to share its page with fig:evmtx, which a
    # three-line version pushed onto a float page of its own.
    labels = ["Classical ECDSA\n(claimClassical)",
              "LAS floor\n(claimLAS, no verify)"]
    values = [CLASSICAL, LAS_FLOOR]
    colors = ["#4C72B0", "#DD9A54"]  # classical blue, floor light-orange
    if LAS_VERIFY_OPT is not None:
        labels.append("LAS full verify, optimised\n(claimLASVerifiedOpt)")
        values.append(LAS_VERIFY_OPT)
        colors.append("#3E8E5A")  # green: the row that clears the cap
    labels.append("LAS full verify, baseline\n(claimLASVerified)")
    values.append(LAS_VERIFY)
    colors.append("#D1622B")  # orange: the row that does not

    # ratios DERIVED from the measured values, never written by hand
    ratios = ["1$\\times$"] + ["%.3g$\\times$" % (v / CLASSICAL) for v in values[1:]]
    ypos = list(range(len(values) - 1, -1, -1))  # first label at the top

    fig, ax = plt.subplots(figsize=(TEXT_WIDTH_IN, 0.72 * len(values) + 1.05))
    left = 1e4  # log-axis floor: bars start here, not at 0 (0 is undefined on a log axis)
    ax.barh(ypos, [v - left for v in values], left=left, height=0.62,
            color=colors, zorder=3)

    ax.set_xscale("log")
    ax.set_xlim(left, 1.6e8)
    # Extra headroom at the top: the cap annotation is drawn downwards from
    # the top of the axes and crowded the first bar's value label without it.
    ax.set_ylim(-0.6, len(values) - 0.02)
    ax.set_yticks(ypos)
    ax.set_yticklabels(labels)
    # Two lines: at 12pt this runs ~340pt against a 411pt canvas and was clipped.
    ax.set_xlabel("on-chain settlement gas\n(log scale; EVM gas is deterministic)")
    ax.xaxis.set_major_locator(LogLocator(base=10))
    # Plain-text magnitudes instead of 10^n: matplotlib sets a mathtext exponent at
    # 0.7x the base size, which put 8.4pt digits on a page whose body is 12pt. Same
    # tick positions, same log axis -- only the label text changes.
    ax.xaxis.set_major_formatter(FuncFormatter(
        lambda v, _pos: ("%gM" % (v / 1e6)) if v >= 1e6 else
                        ("%gk" % (v / 1e3)) if v >= 1e3 else "%g" % v))
    ax.grid(axis="x", which="major", alpha=0.25, linewidth=0.6, zorder=0)

    # Value + ratio labels. At 12pt a label is ~30% of the axis, so a label hung off
    # the end of a long bar runs past the right edge; long bars therefore carry it
    # INSIDE in white, short bars outside as before. No figure is abbreviated to fit:
    # the full gas counts stay on the page.
    # The inside labels are set REGULAR, not bold: bold Latin Modern is ~15% wider
    # per character and much heavier, so at the same 12pt it still read as bigger
    # than the paragraph (Royce, 2026-08-28).  White on a saturated bar carries
    # enough contrast without the extra weight.
    import math
    lo, hi = math.log10(left), math.log10(1.6e8)
    for y, v, r in zip(ypos, values, ratios):
        txt = "{:,}".format(v) + "  (" + r + ")"
        if (math.log10(v) - lo) / (hi - lo) > 0.55:
            ax.text(v * 0.93, y, txt, va="center", ha="right", fontsize=PRINT_PT,
                    color="white", zorder=5)
        else:
            ax.text(v * 1.15, y, txt, va="center", ha="left", fontsize=PRINT_PT)

    # EIP-7825 per-transaction gas cap threshold
    ax.axvline(EIP7825_CAP, color="#B00020", linestyle="--", linewidth=1.4, zorder=4)
    ax.text(EIP7825_CAP * 0.92, len(values) - 0.07,
            "EIP-7825 per-transaction cap\n{:,} gas".format(EIP7825_CAP),
            color="#B00020", ha="right", va="top", fontsize=PRINT_PT)

    out_dir = args.out or (Path(__file__).resolve().parent.parent
                           / "report" / "latex" / "figures")
    out_dir.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    for ext in ("pdf", "png"):
        # NO bbox_inches="tight": the canvas width IS the printed width, and cropping
        # it would make LaTeX rescale the figure -- and its 12pt type -- on include.
        fig.savefig(out_dir / ("fig_onchain." + ext), dpi=200)
    plt.close(fig)
    sys.stderr.write("wrote %s/fig_onchain.{pdf,png}\n" % out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
