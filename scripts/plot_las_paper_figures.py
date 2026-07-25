#!/usr/bin/env python3
"""
plot_las_paper_figures.py -- build ONLY the Stage-1 LAS paper-figure package
(Meeting-4) from the EXISTING benchmark CSVs. It reads the CSVs that
scripts/plot_las_benchmarks.py already wrote from the captured logs; it never runs
a benchmark, never touches protocol code, and invents/edits no numbers.

Research question:
  How much computation and communication/storage overhead does LAS (a post-quantum
  exotic adaptor signature) add over the basic simplified Dilithium-style
  signature?

Inputs (read-only, from --input-dir; default evidence/latest, else evidence/):
  parameter_sets.csv          (level, display, meaning, n, ell, M, kappa, gamma, N, Q)
  primary_timing.csv          (level, n, ell, kappa, operation, mean_us, sd_us)
  communication_components.csv(level, component, bytes, pct_of_signature)
  adaptor_overhead.csv        (level, pair, adaptor_us, base_us, overhead_pct)
  rejection_sampling.csv      (optional; level, operation, avg_attempts, acceptance_pct, ...)

Outputs (main package -> --output-dir; default = input dir):
  parameter_sets_paper.tex
  per_operation_timing_paper.pdf / .png        (Figure 1: basic vs LAS computation)
  communication_components_paper.pdf / .png     (Figure 2: communication component sizes)
  paper_figure_manifest.csv
  KEY_FINDINGS.md
The MAIN package is the single-setting basesig-vs-LAS comparison (computation +
communication) -- the research question. The multi-setting overhead sweep is NOT
in the main package and is NOT a security-parameter comparison.
Appendix output -> --appendix-dir (default = --output-dir):
  adaptor_overhead_paper.pdf / .png             (multi-setting overhead sweep; scaling context only)
  rejection_sampling_paper.pdf / .png           (only if rejection_sampling.csv exists)

Style: colourblind-safe (Okabe-Ito) palette, concise labels with units, no baked-in
long titles, no machine footers, no caption paragraphs inside figures; vector PDF
plus PNG. paper / L2-like / L3-like / L5-like are engineering benchmark settings
only -- NOT formal NIST / ML-DSA security-equivalence claims.
"""
import argparse
import csv
import sys
from pathlib import Path

HEADLINE = "L3"
LEVEL_ORDER = ["paper", "L2", "L3", "L5"]
# Self-explanatory, scientific setting names (Meeting-4: labels must define the
# parameter set). "paper" is the LAS-2020/845 reference parameter set; L2/L3/L5 are
# LAS parameter sets derived from the simplified Dilithium-II/III/V dimensions.
SHORT = {"paper": "LAS-2020/845 reference", "L2": "Simplified Dilithium-II",
         "L3": "Simplified Dilithium-III", "L5": "Simplified Dilithium-V"}

# Okabe-Ito colourblind-safe palette (+ a neutral grey).
OI = {
    "black": "#000000", "orange": "#E69F00", "skyblue": "#56B4E9",
    "green": "#009E73", "yellow": "#F0E442", "blue": "#0072B2",
    "vermillion": "#D55E00", "purple": "#CC79A7", "grey": "#7F7F7F",
}
ORD_COL = OI["skyblue"]   # basic signature operations / objects
LAS_COL = OI["blue"]      # LAS adaptor operations


def _style():
    """One shared matplotlib style (Meeting-6: label/legend fonts must stay
    readable once the figure is shrunk to \\linewidth in the report).
    Recessive axes (no top/right spine, light grid), >=10pt text everywhere."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    plt.rcParams.update({
        "font.size": 11,
        "axes.labelsize": 11.5,
        "axes.titlesize": 11.5,
        "xtick.labelsize": 10.5,
        "ytick.labelsize": 10.5,
        "legend.fontsize": 10.5,
        "axes.spines.top": False,
        "axes.spines.right": False,
        "axes.grid": False,
        "grid.alpha": 0.25,
        "grid.linewidth": 0.6,
        "figure.dpi": 200,
    })
    return plt


# ---------------------------------------------------------------------------
# read existing CSVs (numbers come only from these; nothing is computed/invented)
# ---------------------------------------------------------------------------

def _read(in_dir, name, required=True):
    p = in_dir / name
    if not p.exists():
        if required:
            sys.exit("ERROR: missing required input CSV: %s\n"
                     "Run scripts/plot_las_benchmarks.py first to produce it." % p)
        return None
    with open(p, newline="") as f:
        return list(csv.DictReader(f))


def load_all(in_dir):
    params = {r["level"]: r for r in _read(in_dir, "parameter_sets.csv")}
    timing = {}
    for r in _read(in_dir, "primary_timing.csv"):
        # Tolerant: older CSVs name the operation "Ext / Extract"; normalise to "Ext".
        op = "Ext" if r["operation"].startswith("Ext") else r["operation"]
        timing.setdefault(r["level"], {})[op] = (
            float(r["mean_us"]), float(r["sd_us"]))
    comm = {}
    for r in _read(in_dir, "communication_components.csv"):
        # Tolerant: older CSVs name the witness component "r' = y_witness".
        comp = "r'" if r["component"] in ("r'", "r' = y_witness") else r["component"]
        comm.setdefault(r["level"], {})[comp] = int(r["bytes"])
    overhead = {}
    for r in _read(in_dir, "adaptor_overhead.csv"):
        if (r.get("overhead_pct") or "").strip():
            overhead.setdefault(r["level"], {})[r["pair"]] = float(r["overhead_pct"])
    rej_rows = _read(in_dir, "rejection_sampling.csv", required=False)
    rej, rej_full = None, None
    if rej_rows:
        rej, rej_full = {}, {}
        for r in rej_rows:
            rej.setdefault(r["level"], {})[r["operation"]] = float(r["acceptance_pct"])
            rej_full.setdefault(r["level"], {})[r["operation"]] = r
    return params, timing, comm, overhead, rej, rej_full


def _save(fig, out_dir, name):
    import matplotlib.pyplot as plt
    for ext in ("pdf", "png"):                 # PDF is vector for LaTeX; PNG is preview
        fig.savefig(out_dir / ("%s.%s" % (name, ext)), bbox_inches="tight", dpi=200)
    plt.close(fig)


def _xtick(params, lvl):
    p = params[lvl]
    return "%s\nn=%s, ℓ=%s, κ=%s" % (SHORT.get(lvl, lvl), p["n"], p["ell"], p["kappa"])


# ---------------------------------------------------------------------------
# Table 1 -- parameter settings
# ---------------------------------------------------------------------------

def write_param_tex(params, levels, out_dir):
    out = [
        "% parameter_sets_paper.tex -- generated by plot_las_paper_figures.py from parameter_sets.csv.",
        "\\begin{tabular}{l r r r r r r r}",
        "\\hline",
        "Setting & $n$ & $\\ell$ & $M=n+\\ell$ & $\\kappa$ & $\\gamma$ & $d$ & $Q$ \\\\",
        "\\hline",
    ]
    for lvl in levels:
        p = params[lvl]
        out.append("%s & %s & %s & %s & %s & %s & %s & %s \\\\" % (
            SHORT.get(lvl, lvl), p["n"], p["ell"], p["M"], p["kappa"], p["gamma"],
            p["N"], p["Q"]))
    out += [
        "\\hline",
        "\\multicolumn{8}{l}{\\footnotesize $d$ = ring degree, $Q$ = modulus (both fix the bit-packed serialization). The \\emph{LAS-2020/845 reference} set is the} \\\\",
        "\\multicolumn{8}{l}{\\footnotesize paper's parameter set; the \\emph{Simplified Dilithium-II/III/V} sets reuse the Dilithium mode-2/3/5 dimensions and are engineering} \\\\",
        "\\multicolumn{8}{l}{\\footnotesize benchmark settings only -- they do not claim formal NIST / ML-DSA security equivalence.} \\\\",
        "\\hline",
        "\\end{tabular}",
    ]
    (out_dir / "parameter_sets_paper.tex").write_text("\n".join(out) + "\n")


# ---------------------------------------------------------------------------
# Figure 1 -- per-operation timing as the BASE-vs-LAS overhead, paired
# ---------------------------------------------------------------------------
# The headline Stage-1 result is the adaptor overhead: each LAS operation beside
# the basic operation it mirrors (Meeting-4 14.1/14.3 -- "make the base bars blue
# and the LAS bars orange", don't make the reader infer the overhead). The bars are
# paired so the overhead is read directly off the page (the orange LAS bar sits just
# above its blue basic partner); the percentage above each orange bar is the exact
# overhead. Single setting (the headline), so colour encodes base-vs-LAS, NOT the
# parameter sweep -- the multi-setting version is the appendix scaling check
# (fig_overhead). KeyGen/Sign/Verify are reused unchanged, so KeyGen is shown once
# (shared); Extract has no basic analogue and is shown LAS-only.
BASE_COL = OI["blue"]      # base signature operations (Meeting-4: base = blue)
ADPT_COL = OI["orange"]    # LAS adaptor operations    (Meeting-4: LAS  = orange)


def fig_per_op(timing, overhead, params, hl, out_dir):
    plt = _style()
    from matplotlib.patches import Patch

    t = timing[hl]
    o = overhead.get(hl, {})
    # (group label, basic op or None, LAS op or None, overhead-pair key or None)
    groups = [
        ("KeyGen\n(shared)",    "KeyGen", None,        None),
        ("Sign /\nPreSign",     "Sign",   "PreSign",   "PreSign vs Sign"),
        ("Verify /\nPreVerify", "Verify", "PreVerify", "PreVerify vs Verify"),
        ("Verify /\nAdapt",     "Verify", "Adapt",     "Adapt vs Verify"),
        ("Extract\n(LAS only)", None,     "Ext",       None),
    ]
    w = 0.38
    fig, ax = plt.subplots(figsize=(9.4, 5.2))
    xticks, xlabels, tops = [], [], []
    for gi, (lbl, bop, lop, okey) in enumerate(groups):
        xticks.append(gi)
        xlabels.append(lbl)
        if bop and lop:                                   # paired base + LAS
            bm, bs = t[bop]
            lm, ls = t[lop]
            ax.bar(gi - w / 2, bm, w, yerr=bs, capsize=3, color=BASE_COL,
                   edgecolor="black", linewidth=0.4)
            ax.bar(gi + w / 2, lm, w, yerr=ls, capsize=3, color=ADPT_COL,
                   edgecolor="black", linewidth=0.4)
            tops += [bm + bs, lm + ls]
            ax.text(gi - w / 2, bm + bs, "%.0f" % bm, ha="center", va="bottom",
                    fontsize=9.5)
            ax.text(gi + w / 2, lm + ls, "%.0f" % lm, ha="center", va="bottom",
                    fontsize=9.5)
            if okey in o:                                 # exact overhead, on the orange bar
                ax.annotate("+%.1f%%" % o[okey], xy=(gi + w / 2, lm + ls),
                            xytext=(0, 15), textcoords="offset points", ha="center",
                            va="bottom", fontsize=11, fontweight="bold",
                            color=OI["vermillion"])
        else:                                             # single bar (shared / LAS-only)
            op = bop or lop
            col = BASE_COL if bop else ADPT_COL
            m, s = t[op]
            ax.bar(gi, m, w, yerr=s, capsize=3, color=col, edgecolor="black",
                   linewidth=0.4)
            tops.append(m + s)
            ax.text(gi, m + s, "%.0f" % m, ha="center", va="bottom", fontsize=9.5)
    top = max(tops)
    ax.set_ylim(0, top * 1.30)
    ax.set_xticks(xticks)
    ax.set_xticklabels(xlabels)
    ax.set_ylabel("time per operation (microseconds)")
    ax.legend(handles=[Patch(facecolor=BASE_COL, edgecolor="black",
                             label="basic signature"),
                       Patch(facecolor=ADPT_COL, edgecolor="black",
                             label="LAS adaptor")],
              loc="upper right", framealpha=0.95)
    p = params[hl]
    ax.text(0.0, 1.015, "%s setting    n=%s, ℓ=%s, M=%s, κ=%s, γ=%s, d=%s, q=%s"
            % (SHORT.get(hl, hl), p["n"], p["ell"], p["M"], p["kappa"], p["gamma"],
               p["N"], p["Q"]),
            transform=ax.transAxes, ha="left", va="bottom", fontsize=10,
            color="#444444")
    ax.grid(axis="y", alpha=0.3)
    _save(fig, out_dir, "per_operation_timing_paper")


# ---------------------------------------------------------------------------
# Figure 2 -- communication / serialized component sizes
# ---------------------------------------------------------------------------

def fig_comm(comm, params, hl, out_dir):
    plt = _style()

    c = comm[hl]
    sig = OI["grey"]                         # the three signature objects share one colour
    rows = [
        ("public key  (pk = t)", c["pk = t"], OI["blue"]),
        ("secret key  (sk = r)", c["sk = r"], OI["green"]),
        ("statement  Y = t'", c["Y = t'"], OI["vermillion"]),
        ("challenge  c", c["c"], OI["orange"]),
        ("response  z", c["z"], OI["skyblue"]),
        ("pre-signature response  ẑ", c["z_hat"], OI["skyblue"]),
        ("basic signature  (c, z)", c["signature (c,z)"], sig),
        ("pre-signature  (c, ẑ)", c["pre-signature (c,z_hat)"], sig),
        ("adapted signature  (c, z)", c["final adapted sig (c,z)"], sig),
    ]
    ys = list(range(len(rows)))[::-1]
    fig, ax = plt.subplots(figsize=(10.5, 5.2))
    ax.barh(ys, [r[1] for r in rows], color=[r[2] for r in rows], edgecolor="black",
            linewidth=0.4, height=0.66)
    maxv = max(r[1] for r in rows)
    for y, r in zip(ys, rows):
        ax.text(r[1] + maxv * 0.01, y, "%d bytes" % r[1], va="center", fontsize=10,
                fontweight="bold")
    ax.set_xlim(0, maxv * 1.22)
    ax.set_yticks(ys)
    ax.set_yticklabels([r[0] for r in rows])
    ax.set_xlabel("serialized size (bytes)")
    p = params[hl]
    ax.text(0.0, 1.02, "%s setting\nn=%s, ℓ=%s, M=%s, κ=%s, γ=%s, d=%s, q=%s"
        % (
            SHORT.get(hl, hl),
            p["n"], p["ell"], p["M"], p["kappa"],
            p["gamma"], p["N"], p["Q"]
        ), transform=ax.transAxes,
            ha="left", va="bottom", fontsize=10, color="#444444")
    ax.grid(axis="x", alpha=0.3)
    _save(fig, out_dir, "communication_components_paper")


# ---------------------------------------------------------------------------
# Figure 3 -- adaptor overhead vs the matching basic operation, across settings
# ---------------------------------------------------------------------------

def fig_overhead(overhead, params, levels, out_dir):
    plt = _style()

    pairs = [("PreSign vs Sign", "PreSign versus Sign", OI["blue"]),
             ("PreVerify vs Verify", "PreVerify versus Verify", OI["orange"]),
             ("Adapt vs Verify", "Adapt versus Verify", OI["green"])]
    xs = list(range(len(levels)))
    w = 0.26
    allv = []
    fig, ax = plt.subplots(figsize=(8.8, 5.2))
    for i, (key, lbl, col) in enumerate(pairs):
        vals = [overhead[lvl][key] for lvl in levels]
        allv += vals
        bars = ax.bar([x + (i - 1) * w for x in xs], vals, w, label=lbl, color=col,
                      edgecolor="black", linewidth=0.3)
        for b, v in zip(bars, vals):
            ax.text(b.get_x() + b.get_width() / 2, v + (0.12 if v >= 0 else -0.3),
                    "%+.1f" % v, ha="center", fontsize=8.5)
    ax.axhline(0, color="black", linewidth=0.6)
    ax.set_ylim(bottom=min(0.0, min(allv)) - 0.5, top=max(allv) * 1.32)
    ax.set_xticks(xs)
    ax.set_xticklabels([_xtick(params, lvl) for lvl in levels], fontsize=9)
    ax.set_xlabel("parameter setting (engineering benchmark setting)")
    ax.set_ylabel("overhead vs basic operation (percent)")
    ax.legend(loc="upper right")
    ax.grid(axis="y", alpha=0.3)
    _save(fig, out_dir, "adaptor_overhead_paper")


# ---------------------------------------------------------------------------
# optional appendix -- rejection-sampling acceptance (explains timing variance)
# ---------------------------------------------------------------------------

def fig_rejection(rej, params, levels, out_dir):
    plt = _style()

    xs = list(range(len(levels)))
    eul = 100.0 / 2.718281828459045
    base = [rej[lvl]["Base Sign"] for lvl in levels]
    las = [rej[lvl]["LAS PreSign"] for lvl in levels]
    w = 0.36
    fig, ax = plt.subplots(figsize=(8.4, 4.6))
    ax.bar([x - w / 2 for x in xs], base, w, label="basic Sign", color=ORD_COL,
           edgecolor="black", linewidth=0.3)
    ax.bar([x + w / 2 for x in xs], las, w, label="LAS PreSign", color=LAS_COL,
           edgecolor="black", linewidth=0.3)
    ax.axhline(eul, color=OI["vermillion"], linestyle="--", linewidth=1.2,
               label="1/e = %.1f percent" % eul)
    ax.set_xticks(xs)
    ax.set_xticklabels([_xtick(params, lvl) for lvl in levels], fontsize=9)
    ax.set_xlabel("parameter setting (engineering benchmark setting)")
    ax.set_ylabel("acceptance per attempt (percent)")
    ax.set_ylim(0, max(base + las) * 1.35)
    ax.legend()
    ax.grid(axis="y", alpha=0.3)
    _save(fig, out_dir, "rejection_sampling_paper")


# ---------------------------------------------------------------------------
# MAIN figure -- cumulative probability of acceptance at the headline setting
# ---------------------------------------------------------------------------
# Meeting-7 ruling (Wang, 2026-07-24): the per-k probability-mass presentation
# (fig_attempts_dist below) is misread by readers, because P(exactly k) is
# HIGHEST at k=1 and decays -- which looks backwards to anyone who expects
# "more attempts -> more likely to have succeeded". Report the CUMULATIVE
# probability of acceptance within k attempts instead: it rises from the
# single-attempt acceptance rate (~36.8% = 1/e at the target setting) and
# flattens towards 100%, and the flattening is itself the message.
#
# The curve is the closed-form geometric model, derived from the parameter set
# alone; every overlaid statistic is MEASURED and parsed from the CSV. Nothing
# here invents a number.

def _acc_per_attempt(params, hl):
    """Single-attempt acceptance probability for (Sign bound, PreSign bound)."""
    p = params[hl]
    n, ell = int(p["n"]), int(p["ell"])
    kappa, gamma, d = int(p["kappa"]), int(p["gamma"]), int(p["N"])

    def acc(bound):
        return ((2.0 * bound + 1.0) / (2.0 * gamma + 1.0)) ** ((n + ell) * d)

    return acc(gamma - kappa), acc(gamma - kappa - 1)


def fig_acceptance_cdf(params, rej_full, hl, out_dir):
    plt = _style()

    p = params[hl]
    acc_sign, acc_presign = _acc_per_attempt(params, hl)
    series = [
        ("basic Sign", acc_sign, rej_full[hl]["Base Sign"], BASE_COL, "o"),
        ("LAS PreSign", acc_presign, rej_full[hl]["LAS PreSign"], ADPT_COL, "s"),
    ]
    ks = list(range(1, 16))
    fig, ax = plt.subplots(figsize=(9.0, 5.0))

    for lbl, pa, row, col, mk in series:
        cdf = [100.0 * (1.0 - (1.0 - pa) ** k) for k in ks]
        ax.plot(ks, cdf, color=col, linewidth=2.0, marker=mk, markersize=6,
                label="%s: geometric model (%.1f%% on the first attempt, "
                      "mean %.3f attempts)" % (lbl, 100.0 * pa, 1.0 / pa))

    # measured statistics (C driver distribution sample, straight from the CSV)
    for i, (lbl, _, row, col, _) in enumerate(series):
        ax.text(0.98, 0.34 - 0.07 * i,
                "%s: measured mean %s, p50 %s, p95 %s, max %s (2000 calls)"
                % (lbl, row["avg_attempts"], row["p50"], row["p95"], row["max"]),
                transform=ax.transAxes, ha="right", va="top", fontsize=10,
                color=col)
    ax.text(0.98, 0.41, "measured (C implementation):", transform=ax.transAxes,
            ha="right", va="top", fontsize=10, color="#444444")

    ax.set_xticks(ks)
    ax.set_ylim(0, 104)
    ax.set_xlabel("attempts allowed (k)")
    ax.set_ylabel("probability of acceptance within k attempts (percent)")
    ax.text(0.0, 1.015, "%s setting    n=%s, ℓ=%s, κ=%s, γ=%s, d=%s"
            % (SHORT.get(hl, hl), p["n"], p["ell"], p["kappa"], p["gamma"],
               p["N"]),
            transform=ax.transAxes, ha="left", va="bottom", fontsize=10,
            color="#444444")
    ax.legend(loc="lower right")
    ax.grid(axis="y", alpha=0.3)
    _save(fig, out_dir, "rejection_acceptance_cdf_paper")


# ---------------------------------------------------------------------------
# APPENDIX figure -- per-attempt probability mass at the headline setting
# ---------------------------------------------------------------------------
# Superseded as a BODY figure by fig_acceptance_cdf (Meeting-7 ruling above).
# Retained for the appendix: it is the distribution the measured tail
# statistics (p50/p95/max) actually describe, and it shows the geometric decay
# directly. Body text must cite the cumulative figure, not this one.

def fig_attempts_dist(params, rej_full, hl, out_dir):
    plt = _style()

    p = params[hl]
    acc_sign, acc_presign = _acc_per_attempt(params, hl)
    series = [
        ("basic Sign", acc_sign, rej_full[hl]["Base Sign"], BASE_COL, "o"),
        ("LAS PreSign", acc_presign, rej_full[hl]["LAS PreSign"], ADPT_COL, "s"),
    ]
    ks = list(range(1, 16))
    fig, ax = plt.subplots(figsize=(9.0, 5.0))
    mean_y = {0: 0.72, 1: 0.64}            # stagger the two mean labels (axes coords)
    for i, (lbl, pa, row, col, mk) in enumerate(series):
        pmf = [100.0 * pa * (1.0 - pa) ** (k - 1) for k in ks]
        ax.plot(ks, pmf, color=col, linewidth=2.0, marker=mk, markersize=6,
                label="%s: geometric model (mean %.3f)" % (lbl, 1.0 / pa))
        m = float(row["avg_attempts"])
        ax.axvline(m, color=col, linestyle="--", linewidth=1.4, alpha=0.8)
        ax.text(0.30, mean_y[i], "%s measured mean %.3f" % (lbl, m),
                transform=ax.transAxes, ha="left", va="top",
                fontsize=10, color=col)
    # measured tail statistics (C driver distribution sample, from the CSV)
    for i, (lbl, _, row, col, _) in enumerate(series):
        ax.text(0.98, 0.50 - 0.07 * i,
                "%s: p50 %s, p95 %s, max %s (2000 calls)"
                % (lbl, row["p50"], row["p95"], row["max"]),
                transform=ax.transAxes, ha="right", va="top", fontsize=10,
                color="#444444")
    ax.text(0.98, 0.57, "measured tail (C driver):", transform=ax.transAxes,
            ha="right", va="top", fontsize=10, color="#444444")
    ax.set_xticks(ks)
    ax.set_xlabel("attempts until acceptance (k)")
    ax.set_ylabel("probability of exactly k attempts (percent)")
    ax.text(0.0, 1.015, "%s setting    n=%s, ℓ=%s, κ=%s, γ=%s, d=%s"
            % (SHORT.get(hl, hl), p["n"], p["ell"], p["kappa"], p["gamma"],
               p["N"]),
            transform=ax.transAxes, ha="left", va="bottom", fontsize=10,
            color="#444444")
    ax.legend(loc="upper right")
    ax.grid(axis="y", alpha=0.3)
    _save(fig, out_dir, "rejection_attempts_distribution_paper")


# ---------------------------------------------------------------------------
# KEY_FINDINGS.md  (2-3 concise sentences; numbers parsed from the CSVs)
# ---------------------------------------------------------------------------

def write_key_findings(overhead, comm, timing, hl, out_dir):
    o = overhead[hl]
    c = comm[hl]
    ps = o["PreSign vs Sign"]
    pv = o["PreVerify vs Verify"]
    ad = o["Adapt vs Verify"]
    ext = timing[hl]["Ext"][0]
    sig = c["signature (c,z)"]
    presig = c["pre-signature (c,z_hat)"]
    adapt = c["final adapted sig (c,z)"]
    same = ("the same %d bytes" % sig if sig == presig == adapt
            else "different sizes (%d / %d / %d bytes)" % (sig, presig, adapt))
    lines = [
        "# Key findings (Stage-1 LAS benchmark; headline = %s setting)"
        % SHORT.get(hl, hl),
        "",
        "1. **Extra computation.** Compared with the basic simplified "
        "Dilithium-style signature, LAS adds PreSign (%+.1f%% over Sign), PreVerify "
        "(%+.1f%% over Verify) and Adapt (%+.1f%% over Verify), plus Ext, which "
        "extracts the witness s = z - ẑ in about %.0f microseconds; the adaptor "
        "therefore costs roughly one extra signing pass plus a few verification-scale "
        "operations." % (ps, pv, ad, ext),
        "",
        "2. **The final signature does not grow.** The basic signature, the "
        "pre-signature and the adapted signature have %s, because Adapt sets "
        "z = ẑ + r' (it changes the response value, not the serialized structure)."
        % same,
        "",
        "3. **Extra communication.** The one extra object LAS puts on the wire is the "
        "public statement Y = t' (%d bytes, the same size as the public key, %d bytes); "
        "its witness r' (%d bytes) is the signer's private companion and is never "
        "published. The signature itself is unchanged. The LAS-2020/845 reference and "
        "the Simplified Dilithium-II/III/V settings are engineering benchmark settings "
        "only, not formal NIST / ML-DSA security levels." % (c["Y = t'"], c["pk = t"], c["r'"]),
    ]
    (out_dir / "KEY_FINDINGS.md").write_text("\n".join(lines) + "\n")


# ---------------------------------------------------------------------------
# manifest -- records this script's outputs and classifies the rest as non-main
# ---------------------------------------------------------------------------

def write_manifest(out_dir, generated):
    H = ["output", "role", "source_csv", "metric", "setting_filter", "units", "script"]
    S = "plot_las_paper_figures.py"
    rows = [
        ["parameter_sets_paper.tex", "Table 1 (main)", "parameter_sets.csv",
         "n, ell, M=n+ell, kappa, gamma, N, Q", "all settings", "dimensionless", S],
        ["per_operation_timing_paper.pdf/.png", "Figure 1 (main)", "primary_timing.csv",
         "mean time per operation (independent ops, not cumulative)",
         "Simplified Dilithium-III (headline)", "microseconds", S],
        ["communication_components_paper.pdf/.png", "Figure 2 (main)",
         "communication_components.csv", "serialized object size",
         "Simplified Dilithium-III (headline)", "bytes", S],
    ]
    if "overhead" in generated:
        rows.append(
            ["adaptor_overhead_paper.pdf/.png",
             "appendix (multi-setting scaling; NOT main, NOT a security comparison)",
             "adaptor_overhead.csv",
             "overhead of LAS operation vs the matching basic operation",
             "all settings", "percent", S])
    if "rejection" in generated:
        rows.append(["rejection_sampling_paper.pdf/.png",
                     "appendix (optional, supporting)", "rejection_sampling.csv",
                     "acceptance per attempt", "all settings", "percent", S])
    if "acceptance_cdf" in generated:
        rows.append(["rejection_acceptance_cdf_paper.pdf/.png",
                     "main (cumulative acceptance within k attempts; "
                     "Meeting-7 replacement for the per-attempt mass figure)",
                     "parameter_sets.csv + rejection_sampling.csv",
                     "P(accepted within k) model; measured mean/p50/p95/max annotated",
                     "Simplified Dilithium-III (headline)", "percent", S])
    if "attempts_dist" in generated:
        rows.append(["rejection_attempts_distribution_paper.pdf/.png",
                     "appendix (per-attempt mass function; superseded in the body "
                     "by rejection_acceptance_cdf_paper)",
                     "parameter_sets.csv + rejection_sampling.csv",
                     "P(exactly k attempts) model; measured mean/p50/p95/max overlaid",
                     "Simplified Dilithium-III (headline)", "percent", S])
    rows.append(["KEY_FINDINGS.md", "main (text summary)",
                 "adaptor_overhead.csv + communication_components.csv + primary_timing.csv",
                 "2-3 sentence overhead summary", "Simplified Dilithium-III (headline)", "text", S])

    # Existing figures from plot_las_benchmarks.py -> classified OUT of the main set.
    other = "plot_las_benchmarks.py"
    excluded = [
        ("timing_timeline_base_vs_las.pdf/.png", "appendix/debug (cumulative timing; not main)",
         "primary_timing.csv", "cumulative path time", "all settings", "microseconds"),
        ("protocol_step_timeline.pdf/.png", "appendix/debug (cumulative timing; not main)",
         "primary_timing.csv", "cumulative step time", "all settings", "microseconds"),
        ("computation_component_absolute.pdf/.png", "debug (internal component attribution)",
         "computation_components.csv", "inner-step time", "L3-like", "microseconds"),
        ("component_scaling_vs_level.pdf/.png", "debug (internal component attribution)",
         "computation_components.csv", "inner-step scaling", "all settings", "microseconds"),
        ("verify_ext_attribution_vs_level.pdf/.png", "debug (internal component attribution)",
         "computation_components.csv", "Verify/Ext decomposition", "all settings", "microseconds"),
        ("timing_overhead_clean.pdf/.png", "debug (superseded by Figure 3)",
         "adaptor_overhead.csv", "overhead pairs", "L3-like", "percent"),
        ("communication_summary_clean.pdf/.png", "debug (mixes Stage-1 sizes with Stage-2 payloads)",
         "communication_components.csv", "payload totals", "L3-like", "bytes"),
        ("acceptance_vs_level.pdf/.png", "appendix (superseded by rejection_sampling_paper)",
         "rejection_sampling.csv", "acceptance per attempt", "all settings", "percent"),
        ("application_atomic_swap_payload_breakdown.pdf/.png", "appendix (Stage-2 application)",
         "application_atomic_swap.csv", "atomic-swap payload", "L3-like", "bytes"),
        ("application_multihop_payload_vs_k.pdf/.png", "appendix (Stage-2 AMHL/multi-hop)",
         "application_multihop_amhl.csv", "multi-hop payload vs K", "L3-like", "bytes"),
        ("application_multihop_presign_time_vs_k.pdf/.png", "appendix (Stage-2 AMHL/multi-hop)",
         "application_multihop_amhl.csv", "multi-hop pre-sign time vs K", "L3-like", "milliseconds"),
        ("application_multihop_norm_vs_k.pdf/.png", "appendix (Stage-2 AMHL/multi-hop)",
         "application_multihop_amhl.csv", "witness norm vs bound vs K", "L3-like", "infinity-norm"),
        ("per_operation_timing_report.pdf/.png", "debug (duplicate of Figure 1)",
         "primary_timing.csv", "per-operation time", "all settings", "microseconds"),
        ("communication_components_clean_report.pdf/.png", "debug (duplicate of Figure 2)",
         "communication_components.csv", "component sizes", "L3-like", "bytes"),
        ("adaptor_overhead_vs_level_report.pdf/.png", "debug (duplicate of Figure 3)",
         "adaptor_overhead.csv", "overhead", "all settings", "percent"),
        ("parameter_sets_report.pdf/.png", "debug (parameter PNG; use Table 1 instead)",
         "parameter_sets.csv", "parameter table image", "all settings", "dimensionless"),
        ("parameter_sets.pdf/.png", "debug (parameter PNG; use Table 1 instead)",
         "parameter_sets.csv", "parameter table image", "all settings", "dimensionless"),
    ]
    for fn, role, src, metric, sf, units in excluded:
        rows.append([fn, role, src, metric, sf, units, other])

    with open(out_dir / "paper_figure_manifest.csv", "w", newline="") as f:
        wr = csv.writer(f)
        wr.writerow(H)
        wr.writerows(rows)


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def resolve_input_dir(repo_root, arg):
    if arg:
        p = Path(arg)
        return p if p.is_absolute() else (repo_root / p)
    latest = repo_root / "evidence" / "latest"
    return latest if latest.exists() else (repo_root / "evidence")


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Build the Stage-1 LAS paper-figure package from existing CSVs.")
    ap.add_argument("--input-dir", default=None,
                    help="folder with the source CSVs (default: evidence/latest, else evidence/)")
    ap.add_argument("--output-dir", default=None,
                    help="where the main paper package is written (default: same as --input-dir)")
    ap.add_argument("--appendix-dir", default=None,
                    help="where the optional rejection-sampling appendix figure is written "
                         "(default: same as --output-dir)")
    args = ap.parse_args(argv)

    repo = Path(__file__).resolve().parents[1]
    in_dir = resolve_input_dir(repo, args.input_dir)
    out_dir = Path(args.output_dir).resolve() if args.output_dir else in_dir
    app_dir = Path(args.appendix_dir).resolve() if args.appendix_dir else out_dir
    if not in_dir.exists():
        sys.exit("ERROR: input dir does not exist: %s" % in_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    if app_dir != out_dir:
        app_dir.mkdir(parents=True, exist_ok=True)

    params, timing, comm, overhead, rej, rej_full = load_all(in_dir)
    levels = [l for l in LEVEL_ORDER if l in params] or sorted(params)
    if not levels:
        sys.exit("ERROR: no parameter settings found in parameter_sets.csv")
    hl = HEADLINE if HEADLINE in levels else levels[0]

    # text artefacts first (written even if matplotlib is unavailable)
    write_param_tex(params, levels, out_dir)
    write_key_findings(overhead, comm, timing, hl, out_dir)

    generated = set()
    try:
        # MAIN package: the research-question comparison -- basic signature vs
        # LAS adaptor, computation (Fig 1) and communication (Fig 2), single setting.
        fig_per_op(timing, overhead, params, hl, out_dir)
        fig_comm(comm, params, hl, out_dir)
        # Appendix: the multi-setting overhead sweep and rejection acceptance are
        # scaling/diagnostic context only -- NOT the main story and NOT a
        # security-parameter comparison.
        fig_overhead(overhead, params, levels, app_dir)
        generated.add("overhead")
        if rej:
            fig_rejection(rej, params, levels, app_dir)   # optional appendix figure
            generated.add("rejection")
        if rej_full and hl in rej_full:
            # MAIN: cumulative acceptance within k attempts (Meeting-7 ruling)
            fig_acceptance_cdf(params, rej_full, hl, out_dir)
            generated.add("acceptance_cdf")
            # APPENDIX: the per-attempt mass function it supersedes
            fig_attempts_dist(params, rej_full, hl, app_dir)
            generated.add("attempts_dist")
    except ImportError as e:
        print("WARNING: matplotlib unavailable (%s); wrote .tex/.md and will write "
              "the manifest, but skipped the figures. Install with: pip install matplotlib"
              % e, file=sys.stderr)

    write_manifest(out_dir, generated)

    print("Input dir     : %s" % in_dir)
    print("Output dir    : %s" % out_dir)
    print("Headline      : %s" % SHORT.get(hl, hl))
    print("Settings      : %s" % ", ".join(SHORT.get(l, l) for l in levels))
    print("Main package  : parameter_sets_paper.tex (Table 1); "
          "per_operation_timing_paper (Fig 1); communication_components_paper (Fig 2)"
          + ("; rejection_acceptance_cdf_paper (cumulative acceptance)"
             if "acceptance_cdf" in generated else ""))
    print("Appendix      : adaptor_overhead_paper (multi-setting sweep) -> %s%s"
          % (app_dir,
             ("; rejection_sampling_paper" if rej
              else "  [rejection_sampling.csv absent -> rejection figure skipped]")))
    print("Also wrote    : paper_figure_manifest.csv, KEY_FINDINGS.md")


if __name__ == "__main__":
    main()
