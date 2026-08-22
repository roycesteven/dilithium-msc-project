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
        from matplotlib.ticker import LogLocator
    except ImportError:
        sys.stderr.write("matplotlib not installed: pip install matplotlib\n")
        return 1

    plt.rcParams.update({
        "font.size": 11, "axes.labelsize": 11.5, "axes.titlesize": 11.5,
        "xtick.labelsize": 10.5, "ytick.labelsize": 10.5, "legend.fontsize": 10.5,
        "axes.spines.top": False, "axes.spines.right": False,
        "axes.grid": False, "figure.dpi": 200,
    })

    # rows top-to-bottom: classical, floor, [single-transaction verify], verify.
    # The optimised row is inserted only when the log carries it, so this script
    # still reproduces the pre-optimisation figure from an older evidence log.
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

    fig, ax = plt.subplots(figsize=(9.2, 0.78 * len(values) + 0.75))
    left = 1e4  # log-axis floor: bars start here, not at 0 (0 is undefined on a log axis)
    ax.barh(ypos, [v - left for v in values], left=left, height=0.62,
            color=colors, zorder=3)

    ax.set_xscale("log")
    ax.set_xlim(left, 1.6e8)
    ax.set_ylim(-0.6, len(values) - 0.4)
    ax.set_yticks(ypos)
    ax.set_yticklabels(labels)
    ax.set_xlabel("on-chain settlement gas (log scale; EVM gas is deterministic)")
    ax.xaxis.set_major_locator(LogLocator(base=10))
    ax.grid(axis="x", which="major", alpha=0.25, linewidth=0.6, zorder=0)

    # value + ratio labels at the bar ends
    for y, v, r in zip(ypos, values, ratios):
        ax.text(v * 1.15, y, "{:,}".format(v) + "  (" + r + ")",
                va="center", ha="left", fontsize=10)

    # EIP-7825 per-transaction gas cap threshold
    ax.axvline(EIP7825_CAP, color="#B00020", linestyle="--", linewidth=1.4, zorder=4)
    ax.text(EIP7825_CAP * 0.92, len(values) - 0.5,
            "EIP-7825 per-transaction cap\n{:,} gas".format(EIP7825_CAP),
            color="#B00020", ha="right", va="top", fontsize=9.5)

    out_dir = args.out or (Path(__file__).resolve().parent.parent
                           / "report" / "latex" / "figures")
    out_dir.mkdir(parents=True, exist_ok=True)
    for ext in ("pdf", "png"):
        fig.savefig(out_dir / ("fig_onchain." + ext), bbox_inches="tight", dpi=200)
    plt.close(fig)
    sys.stderr.write("wrote %s/fig_onchain.{pdf,png}\n" % out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
