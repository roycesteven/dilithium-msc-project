#!/usr/bin/env python3
"""plot_onchain_gas.py -- the on-chain settlement-gas figure (Stage-2 result).

Renders report/latex/figures/fig_onchain.{pdf,png}: a horizontal, log-scaled
comparison of the three atomic-swap claim paths measured on a local EVM (Foundry
`forge test --gas-report`; gas is deterministic for the fixed bytecode, EVM revision,
inputs, and contract state, and independent of the host CPU), with the EIP-7825
per-transaction gas cap drawn as the threshold the full verifier overshoots.

The gas values are MEASURED (not from a CSV; they are `forge --gas-report`
outputs, deterministic for the EVM), recorded here with their source function:

    claimClassical    75,751       full ECDSA verify via the ecrecover precompile
    claimLAS          289,930      floor: calldata for the 6720-byte sig + 1 keccak, NO verify
    claimLASVerified  56,538,682   COMPLETE native base_verify in Solidity (evm/src/LASVerifier.sol)
    EIP-7825 cap      16,777,216   max gas one Ethereum transaction may use (2**24)

Style mirrors scripts/plot_las_paper_figures.py (_style): >=10pt text, recessive
axes, vector PDF at \\linewidth. Usage:  python3 scripts/plot_onchain_gas.py
"""
import sys
from pathlib import Path

# measured gas (forge --gas-report; via_ir; deterministic EVM gas)
CLASSICAL = 75_751        # claimClassical  (full ecrecover verify)
LAS_FLOOR = 289_930       # claimLAS         (floor; no lattice verify)
LAS_VERIFY = 56_538_682   # claimLASVerified (complete native base_verify)
EIP7825_CAP = 16_777_216  # per-transaction gas cap (2**24)


def main():
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

    # rows top-to-bottom: classical, floor, verify
    labels = ["Classical ECDSA\n(claimClassical)",
              "LAS floor\n(claimLAS, no verify)",
              "LAS full verify\n(claimLASVerified)"]
    values = [CLASSICAL, LAS_FLOOR, LAS_VERIFY]
    ratios = ["1$\\times$", "3.8$\\times$", "746$\\times$"]
    colors = ["#4C72B0", "#DD9A54", "#D1622B"]  # classical blue, floor light-orange, verify orange
    ypos = [2, 1, 0]  # verify at bottom so it sits nearest the x-axis label

    fig, ax = plt.subplots(figsize=(9.2, 3.1))
    left = 1e4  # log-axis floor: bars start here, not at 0 (0 is undefined on a log axis)
    ax.barh(ypos, [v - left for v in values], left=left, height=0.62,
            color=colors, zorder=3)

    ax.set_xscale("log")
    ax.set_xlim(left, 1.6e8)
    ax.set_ylim(-0.6, 2.6)
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
    ax.text(EIP7825_CAP * 0.92, 2.5,
            "EIP-7825 per-transaction cap\n{:,} gas".format(EIP7825_CAP),
            color="#B00020", ha="right", va="top", fontsize=9.5)

    out_dir = Path(__file__).resolve().parent.parent / "report" / "latex" / "figures"
    out_dir.mkdir(parents=True, exist_ok=True)
    for ext in ("pdf", "png"):
        fig.savefig(out_dir / ("fig_onchain." + ext), bbox_inches="tight", dpi=200)
    plt.close(fig)
    sys.stderr.write("wrote %s/fig_onchain.{pdf,png}\n" % out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
