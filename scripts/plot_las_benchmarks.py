#!/usr/bin/env python3
"""
plot_las_benchmarks.py -- turn ONE benchmark-suite run folder into report-ready
CSV tables and a small set of clean figures, written back into that SAME folder.

It is driven by scripts/run_benchmark_suite.sh, which builds/runs the benchmarks
and then calls:

    python3 scripts/plot_las_benchmarks.py --input-dir <run> --output-dir <run>

Inputs (read from --input-dir):
  * fair_paper.log fair_l2.log fair_l3.log fair_l5.log   (PRIMARY fair-benchmark)
  * application_benchmark.log                            (optional; L3-like)

Outputs (written to --output-dir == the same run folder by default):
  * CSV tables (one row per level / per K / per metric)
  * a few report-quality PNG+PDF figures (quality over quantity)
  * report_figure_manifest.csv  (which outputs are main / appendix / table-only)

It does NOT create repo-level tables/ or figures/ folders, does NOT run any
benchmark, and invents no numbers -- every value is parsed from a log. Fair-log
parsing fails loudly if a required value is missing or inconsistent; the optional
application log is skipped with a clear warning if absent or malformed (without
failing the fair-benchmark outputs).

Paper notation: pp=(A,H), pk=t, sk=r, statement Y=t', witness r'(=y_witness),
signing mask y_mask, commitment w=A*y_mask (hashed into c, NOT transmitted),
pre-signature response z_hat, final response z, Ext/Extract s=z-z_hat. The base
path and the LAS adaptor path are two separate protocols and are never summed.
"""
import argparse
import csv
import os
import re
import sys
from pathlib import Path

# ---------------------------------------------------------------------------
# parsing
# ---------------------------------------------------------------------------

FLOAT = r"[-+]?\d+(?:\.\d+)?"

LEVEL_FILES = [
    ("fair_paper.log", "paper"),
    ("fair_l2.log", "L2"),
    ("fair_l3.log", "L3"),
    ("fair_l5.log", "L5"),
]
APP_FILE = "application_benchmark.log"
HEADLINE = "L3"   # Dilithium-3-aligned: the project's stated target set


class ParseError(RuntimeError):
    pass


def _section(lines, start_key, end_key=None):
    start = None
    for i, ln in enumerate(lines):
        if start_key in ln:
            start = i
            break
    if start is None:
        raise ParseError("section start not found: %r" % start_key)
    if end_key is None:
        return lines[start:]
    for j in range(start + 1, len(lines)):
        if end_key in lines[j]:
            return lines[start:j]
    return lines[start:]


def _find(lines, key):
    for ln in lines:
        if key in ln:
            return ln
    raise ParseError("line not found: %r" % key)


def _mean_sd(line, what):
    m = re.search(r"(%s)\s*\+/-\s*(%s)" % (FLOAT, FLOAT), line)
    if not m:
        raise ParseError("no 'mean +/- sd' on line for %r: %r" % (what, line.strip()))
    return float(m.group(1)), float(m.group(2))


def _anchored_mean_sd(lines, label_regex, what):
    pat = re.compile(r"^\s*%s\s+(%s)\s*\+/-\s*(%s)" % (label_regex, FLOAT, FLOAT))
    for ln in lines:
        m = pat.match(ln)
        if m:
            return float(m.group(1)), float(m.group(2))
    raise ParseError("timing line not found for %r" % what)


def _last_eq_int(line, what):
    """The packed total on a payload line is the '= <int>' that precedes any '['
    bracket annotation (the bracket may itself contain '= 2*(c, z)')."""
    head = line.split("[")[0]
    matches = re.findall(r"=\s*(\d+)", head)
    if not matches:
        raise ParseError("no '= <int>' on line for %r: %r" % (what, line.strip()))
    return int(matches[-1])


def parse_log(path):
    """Parse one fair_*.log into a dict of all values the plots/CSVs need."""
    text = Path(path).read_text(errors="replace")
    lines = text.splitlines()
    name = os.path.basename(path)
    d = {"_file": name}

    def fail(msg):
        raise ParseError("%s: %s" % (name, msg))

    try:
        hdr = _find(lines, "LAS parameter set:")
        m = re.search(
            r"n=(\d+)\s+ell=(\d+)\s+kappa=(\d+)\s+gamma=(\d+)\s*\(N=(\d+),\s*Q=(\d+)\)",
            hdr,
        )
        if not m:
            fail("could not parse parameter header: %r" % hdr.strip())
        d["n"], d["ell"], d["kappa"], d["gamma"], d["N"], d["Q"] = (
            int(m.group(i)) for i in range(1, 7)
        )
        d["M"] = int(re.search(r"M = n \+ ell = (\d+)", _find(lines, "M = n + ell =")).group(1))

        for key, label in (("compiler", "compiler="), ("git", "git=")):
            try:
                d[key] = _find(lines, label).strip()
            except ParseError:
                d[key] = ""

        prim = _section(lines, "--- COMPUTATION", "Adaptor overhead")
        d["t_setup"] = _anchored_mean_sd(prim, r"Setup", "Setup")
        d["t_keygen"] = _anchored_mean_sd(prim, r"KeyGen", "KeyGen")
        d["t_sign"] = _anchored_mean_sd(prim, r"Sign", "Sign")
        d["t_verify"] = _anchored_mean_sd(prim, r"Verify", "Verify")
        d["t_presign"] = _anchored_mean_sd(prim, r"PreSign", "PreSign")
        d["t_preverify"] = _anchored_mean_sd(prim, r"PreVerify", "PreVerify")
        d["t_adapt"] = _anchored_mean_sd(prim, r"Adapt", "Adapt")
        d["t_ext"] = _anchored_mean_sd(prim, r"Ext\s*/\s*Extract", "Ext / Extract")

        over = _section(lines, "Adaptor overhead", "# DIAGNOSTICS")

        def overhead(line_rx, what):
            rx = re.compile(line_rx)
            ln = next((l for l in over if rx.match(l)), None)
            if ln is None:
                fail("overhead line not found for %s" % what)
            m2 = re.search(r"(%s)\s+vs\s+(%s)\s+\((%s)%%\)" % (FLOAT, FLOAT, FLOAT), ln)
            if not m2:
                fail("could not parse overhead line for %s: %r" % (what, ln.strip()))
            return float(m2.group(1)), float(m2.group(2)), float(m2.group(3))

        d["o_presign"] = overhead(r"^\s*PreSign\s+vs\s+Sign", "PreSign vs Sign")
        d["o_preverify"] = overhead(r"^\s*PreVerify\s+vs\s+Verify", "PreVerify vs Verify")
        d["o_adapt"] = overhead(r"^\s*Adapt\s+vs\s+Verify", "Adapt vs Verify")

        rej = _section(lines, "--- A. REJECTION", "--- B.")

        def rejrow(key, what):
            ln = _find(rej, key)
            m3 = re.search(
                r"(%s)\s+(%s)%%\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)" % (FLOAT, FLOAT), ln
            )
            if not m3:
                fail("could not parse rejection row for %s: %r" % (what, ln.strip()))
            return {
                "avg": float(m3.group(1)), "accept_pct": float(m3.group(2)),
                "min": int(m3.group(3)), "max": int(m3.group(4)),
                "p50": int(m3.group(5)), "p95": int(m3.group(6)),
            }

        d["rej_base"] = rejrow("Base Sign", "Base Sign")
        d["rej_las"] = rejrow("LAS PreSign", "LAS PreSign")

        comm = _section(lines, "--- C. COMMUNICATION", "--- D.")

        def grab_int(pattern, what):
            rx = re.compile(pattern)
            for ln in comm:
                m4 = rx.search(ln)
                if m4:
                    return int(m4.group(1))
            fail("communication value not found: %s" % what)

        d["sz_pk"] = grab_int(r"pk = t\s+(\d+)", "pk")
        d["sz_sk"] = grab_int(r"sk = r\s+(\d+)", "sk")
        d["sz_Y"] = grab_int(r"Y = t'\s+(\d+)", "Y")
        d["sz_ywit"] = grab_int(r"r' = y_witness\s+(\d+)", "y_witness")
        d["sz_c"] = grab_int(r"^\s*challenge\s+c\s+(\d+)", "c")
        d["sz_z"] = grab_int(r"z \(final\)\s+(\d+)", "z")
        d["sz_zhat"] = grab_int(r"z_hat \(pre-sig\)\s+(\d+)", "z_hat")
        d["sz_sig"] = grab_int(r"^\s*signature\s+\(c, z\)\s+(\d+)", "signature")
        d["sz_presig"] = grab_int(r"^\s*pre-signature\s+\(c, z_hat\)\s+(\d+)", "pre-signature")
        d["sz_adapted"] = grab_int(r"final adapted sig\s+\(c, z\)\s+(\d+)", "adapted")
        zln = _find(comm, "z (final)")
        zp = re.search(r"\((%s)%%" % FLOAT, zln)
        d["z_pct_printed"] = float(zp.group(1)) if zp else None
        # The three numeric atomic-swap payloads live ONLY inside the
        # "atomic-swap payload" sub-block.  Anchor to that sub-block first, so that
        # protocol-component catalogue / limitation rows above it (e.g. a
        # "pi (NIZK well-formedness proof) ... paper-level off-chain proof ... n/a"
        # row) that merely mention "off-chain" in prose, or carry an "n/a"/"excl"
        # byte field, are never substring-matched as a numeric payload line.
        pay = _section(comm, "atomic-swap payload")
        d["pl_offchain"] = _last_eq_int(_find(pay, "off-chain"), "off-chain")
        d["pl_settlement"] = _last_eq_int(_find(pay, "= 2*signature"), "settlement")
        d["pl_settle_Y"] = _last_eq_int(_find(pay, "escrowed Y"), "settlement incl Y")

        comp = _section(lines, "--- D. COMPONENT")
        d["c_aprod"] = _mean_sd(_find(comp, "A-product"), "A-product")
        d["c_hash"] = _mean_sd(_find(comp, "challenge hash"), "challenge hash")
        d["c_cr_one"] = _mean_sd(_find(comp, "(one response"), "c*r one poly")
        d["c_cr_all"] = _mean_sd(_find(comp, "(all LAS_M"), "c*r all M")
        d["c_norm"] = _mean_sd(_find(comp, "norm check"), "norm check")
        d["c_wY"] = _mean_sd(_find(comp, "w + t'"), "w + t'")
        d["c_zwit"] = _mean_sd(_find(comp, "z_hat + witness"), "z_hat + witness")
        # verify-side / KeyGen / Ext component attribution (added in bench_levels.c).
        d["c_ct"] = _mean_sd(_find(comp, "all LAS_N"), "c*t all n")
        d["c_keygen_r"] = _mean_sd(_find(comp, "sample r (n+ell"), "KeyGen sample r")
        d["c_ext_sub"] = _mean_sd(_find(comp, "Ext: s = z"), "Ext s=z-zhat")
        d["c_ext_amul"] = _mean_sd(_find(comp, "Ext: A*s"), "Ext A*s")
        d["c_ext_check"] = _mean_sd(_find(comp, "Ext: t' == A*s"), "Ext check")
    except ParseError as e:
        raise ParseError("%s" % e)
    return d


def validate(d):
    name = d["_file"]
    errs = []
    if d["sz_c"] + d["sz_z"] != d["sz_sig"]:
        errs.append("signature != c + z (%d + %d != %d)" % (d["sz_c"], d["sz_z"], d["sz_sig"]))
    if d["sz_presig"] != d["sz_sig"]:
        errs.append("pre-signature (%d) != signature (%d)" % (d["sz_presig"], d["sz_sig"]))
    if d["sz_adapted"] != d["sz_sig"]:
        errs.append("adapted (%d) != signature (%d)" % (d["sz_adapted"], d["sz_sig"]))
    if d["pl_offchain"] != d["sz_Y"] + 2 * d["sz_presig"]:
        errs.append("off-chain (%d) != Y + 2*pre-signature (%d)"
                    % (d["pl_offchain"], d["sz_Y"] + 2 * d["sz_presig"]))
    if d["pl_settlement"] != 2 * d["sz_sig"]:
        errs.append("settlement (%d) != 2*signature (%d)" % (d["pl_settlement"], 2 * d["sz_sig"]))
    if d["M"] != d["n"] + d["ell"]:
        errs.append("M (%d) != n + ell (%d)" % (d["M"], d["n"] + d["ell"]))
    if d["z_pct_printed"] is not None:
        zp = 100.0 * d["sz_z"] / d["sz_sig"]
        if abs(zp - d["z_pct_printed"]) > 0.2:
            errs.append("z%% printed (%.1f) != z/signature (%.1f)" % (d["z_pct_printed"], zp))
    if errs:
        raise ParseError("%s: validation failed:\n  - %s" % (name, "\n  - ".join(errs)))


def parse_app_log(path):
    """Parse application_benchmark.log (bench_app3, L3-like): the atomic-swap
    accounting and the multi-hop AMHL K-series table."""
    text = Path(path).read_text(errors="replace")
    lines = text.splitlines()
    a = {"_file": os.path.basename(path)}

    def fail(msg):
        raise ParseError("%s: %s" % (a["_file"], msg))

    def one(pattern, what, cast=int):
        for ln in lines:
            m = re.search(pattern, ln)
            if m:
                return cast(m.group(1))
        fail("could not find %s" % what)

    m = re.search(r"n=(\d+)\s+ell=(\d+)\s+kappa=(\d+)\s+gamma=(\d+)\s+N=(\d+)\s+Q=(\d+)",
                  _find(lines, "params:"))
    if not m:
        fail("could not parse params header")
    a["n"], a["ell"], a["kappa"], a["gamma"], a["N"], a["Q"] = (int(m.group(i)) for i in range(1, 7))

    a["sw_Y"] = one(r"statement Y\s+(\d+)\s*B", "statement Y")
    a["sw_preA"] = one(r"sigma\^_A\s+(\d+)\s*B", "pre-sig A")
    a["sw_preB"] = one(r"sigma\^_B\s+(\d+)\s*B", "pre-sig B")
    a["sw_offchain"] = one(r"=>\s*3 messages,\s*(\d+)\s*B off-chain", "off-chain total")
    sline = _find(lines, "=> 2 signatures,")
    ms = re.search(r"=>\s*2 signatures,\s*(\d+)\s*B\s*\((\d+)\s*B incl", sline)
    if not ms:
        fail("could not parse settlement line: %r" % sline.strip())
    a["sw_settle"] = int(ms.group(1))
    a["sw_settle_Y"] = int(ms.group(2))
    cline = _find(lines, "chain A: sigma_A")
    a["sw_sigA"] = int(re.search(r"chain A: sigma_A\s+(\d+)\s*B", cline).group(1))
    a["sw_sigB"] = int(re.search(r"chain B: sigma_B\s+(\d+)\s*B", cline).group(1))
    a["sw_time_ms"] = one(r"signing work.*:\s*(%s)\s*ms" % FLOAT, "signing time", float)
    a["sw_attempts"] = one(r"pre-signs \(measured\):\s*(\d+)", "presign attempts")
    a["sw_ok"] = one(r"binding\):\s*(OK|FAIL)", "swap correctness", str)

    # multi-hop AMHL K-series table
    row_rx = re.compile(
        r"^\s*(\d+)\s*\|\s*(\d+)\s*\|\s*(\d+)\s*\|\s*(%s)\s*\|\s*(%s)\s*\|"
        r"\s*(\d+)\s*B\s*\|\s*(\d+)\s*B\s*\|\s*(\d+)\s+(OK|FAIL)" % (FLOAT, FLOAT))
    rows = []
    for ln in lines:
        m = row_rx.match(ln)
        if m:
            rows.append({
                "K": int(m.group(1)), "bound_gkK": int(m.group(2)),
                "num_presig": int(m.group(3)), "attempts_per_presig": float(m.group(4)),
                "presig_time_ms": float(m.group(5)), "settle_sigs_bytes": int(m.group(6)),
                "public_stmts_bytes": int(m.group(7)), "max_norm": int(m.group(8)),
                "ok": m.group(9),
            })
    a["mh"] = rows
    return a


def validate_app(a):
    errs = []
    if a["sw_offchain"] != a["sw_Y"] + a["sw_preA"] + a["sw_preB"]:
        errs.append("off-chain (%d) != Y + preA + preB (%d)"
                    % (a["sw_offchain"], a["sw_Y"] + a["sw_preA"] + a["sw_preB"]))
    if a["sw_settle"] != a["sw_sigA"] + a["sw_sigB"]:
        errs.append("settlement (%d) != sigA + sigB (%d)"
                    % (a["sw_settle"], a["sw_sigA"] + a["sw_sigB"]))
    if a["sw_settle_Y"] != a["sw_settle"] + 2 * a["sw_Y"]:
        errs.append("settlement incl Y (%d) != settlement + 2*Y (%d)"
                    % (a["sw_settle_Y"], a["sw_settle"] + 2 * a["sw_Y"]))
    if errs:
        raise ParseError("%s: application validation failed:\n  - %s"
                         % (a["_file"], "\n  - ".join(errs)))


# ---------------------------------------------------------------------------
# colours / helpers
# ---------------------------------------------------------------------------

COL = {
    "base_sign": "#9ecae1", "las_presign": "#08519c",
    "base_verify": "#fdd0a2", "las_preverify": "#d94701",
    "adapt": "#6a51a3", "witness_add": "#bcbddc", "ext": "#238b45",
    "c": "#d9a7a0", "z": "#99000d", "zhat": "#cb181d",
    "Y": "#1b9e9e", "y_witness": "#238b45", "w": "#969696",
    "hash": "#feb24c", "cr_all": "#3182bd", "norm": "#bdbdbd",
    "pk": "#6baed6", "sk": "#74c476", "sig": "#525252", "presig": "#08519c",
}


def _levels_in_order(data):
    return [lvl for _, lvl in LEVEL_FILES if lvl in data]


def _headline(data):
    return HEADLINE if HEADLINE in data else _levels_in_order(data)[0]


def _save(fig, out_dir, name):
    import matplotlib.pyplot as plt
    for ext in ("png", "pdf"):
        fig.savefig(out_dir / ("%s.%s" % (name, ext)), bbox_inches="tight", dpi=150)
    plt.close(fig)


def _bytes_label(n):
    return "%d B" % n


# ---------------------------------------------------------------------------
# fair-benchmark figures (clean, report-quality set)
# ---------------------------------------------------------------------------

def make_fair_plots(data, out_dir):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Patch

    levels = _levels_in_order(data)
    hl = _headline(data)
    dh = data[hl]

    # 1. timing_timeline_base_vs_las (all levels; horizontal stacked timelines) ----
    base_ops = [("Sign", "t_sign", COL["base_sign"]), ("Verify", "t_verify", COL["base_verify"])]
    las_ops = [("PreSign", "t_presign", COL["las_presign"]),
               ("PreVerify", "t_preverify", COL["las_preverify"]),
               ("Adapt", "t_adapt", COL["adapt"]), ("Ext / Extract", "t_ext", COL["ext"])]
    rows = []   # (label, [(segname,color,value)...])
    for lvl in levels:
        rows.append(("%s · Base" % lvl, [(n, c, data[lvl][k][0]) for n, k, c in base_ops]))
        rows.append(("%s · LAS" % lvl, [(n, c, data[lvl][k][0]) for n, k, c in las_ops]))
    fig, ax = plt.subplots(figsize=(9.5, 0.55 * len(rows) + 2.4))
    ys = list(range(len(rows)))[::-1]
    totals = []
    for y, (label, segs) in zip(ys, rows):
        left = 0.0
        for segname, color, val in segs:
            ax.barh(y, val, left=left, color=color, edgecolor="white", height=0.6)
            left += val
        totals.append(left)
    maxtot = max(totals)
    for y, tot in zip(ys, totals):
        ax.text(tot + maxtot * 0.012, y, "%.0f us" % tot, va="center", fontsize=8)
    ax.set_xlim(0, maxtot * 1.14)                  # headroom so the us labels are not clipped
    ax.set_yticks(ys)
    ax.set_yticklabels([r[0] for r in rows], fontsize=9)
    ax.set_xlabel("cumulative time (us)  --  paths shown separately, NOT summed")
    ax.set_title("Base vs LAS protocol timelines (two separate protocols, compared not summed)")
    handles = [Patch(facecolor=c, label=n) for n, _, c in base_ops] + \
              [Patch(facecolor=c, label=n) for n, _, c in las_ops]
    ax.legend(handles=handles, fontsize=8, ncol=6, loc="upper center",
              bbox_to_anchor=(0.5, -0.10), frameon=False)
    ax.grid(axis="x", alpha=0.3)
    _save(fig, out_dir, "timing_timeline_base_vs_las")

    # 1b. protocol_step_timeline (per level; cumulative line; NAMED protocol steps) -
    #     Finding: the LAS adaptor cycle costs only ~30% more than a base Sign+Verify,
    #     and that cost is dominated by the shared rejection-sampling Sign/PreSign step;
    #     the adaptor-specific steps (PreVerify, Adapt, Ext) are each <= one Verify.
    def _cum(dd, keys):
        out, s = [], 0.0
        for k in keys:
            s += dd[k][0]
            out.append(s)
        return out

    step_labels = ["Sign /\nPreSign", "Verify /\nPreVerify", "Adapt\n(LAS only)",
                   "Ext·Extract\n(LAS only)"]
    fig, axes = plt.subplots(2, 2, figsize=(11, 7.4), squeeze=False)
    flat = [a for row in axes for a in row]
    for ax, lvl in zip(flat, levels):
        dd = data[lvl]
        bc = _cum(dd, ["t_sign", "t_verify"])               # base: 2 named steps
        lc = _cum(dd, ["t_presign", "t_preverify", "t_adapt", "t_ext"])  # LAS: 4 named steps
        ax.plot([1, 2], bc, "o-", color=COL["base_sign"], lw=2, ms=7,
                label="Base path (Sign→Verify)")
        ax.plot([1, 2, 3, 4], lc, "s-", color=COL["las_presign"], lw=2, ms=7,
                label="LAS path (PreSign→PreVerify→Adapt→Ext)")
        ax.text(2.0, bc[-1], "  %.0f us (base done)" % bc[-1], fontsize=8,
                va="bottom", ha="left", color="#2171b5")
        ax.text(4.0, lc[-1], "  %.0f us" % lc[-1], fontsize=8, va="center", ha="left",
                color=COL["las_presign"], fontweight="bold")
        over = 100.0 * (lc[-1] - bc[-1]) / bc[-1]
        ax.set_title("%s (n=%d, ell=%d)  —  full LAS cycle %+.1f%% vs base"
                     % (lvl, dd["n"], dd["ell"], over), fontsize=9)
        ax.set_xticks([1, 2, 3, 4])
        ax.set_xticklabels(step_labels, fontsize=8)
        ax.set_ylabel("cumulative time (us)")
        ax.set_xlim(0.8, 4.9)
        ax.grid(alpha=0.3)
    for ax in flat[len(levels):]:
        ax.axis("off")
    h, lbl = flat[0].get_legend_handles_labels()
    fig.legend(h, lbl, loc="lower center", ncol=2, fontsize=9, frameon=False,
               bbox_to_anchor=(0.5, -0.01))
    fig.suptitle("Protocol-step timeline: LAS adaptor cycle vs base signature "
                 "(named steps; rejection-sampling Sign/PreSign dominates each path)",
                 fontsize=11)
    fig.tight_layout(rect=[0, 0.04, 1, 0.95])
    _save(fig, out_dir, "protocol_step_timeline")

    # 2. timing_overhead_clean (two panels with split y-scales so the fast verify-side
    #    ops are not dwarfed by the slow sign-side; in each pair left=base, right=adaptor)
    fig, (axL, axR) = plt.subplots(
        1, 2, figsize=(10, 4.8), gridspec_kw={"width_ratios": [1.0, 2.4]})

    def _ovpair(ax, xpos, base, adpt, bcol, acol):
        ax.bar(xpos - 0.2, base[0], 0.38, yerr=base[1], capsize=3,
               color=bcol, edgecolor="black", linewidth=0.3)
        ax.bar(xpos + 0.2, adpt[0], 0.38, yerr=adpt[1], capsize=3,
               color=acol, edgecolor="black", linewidth=0.3)
        return max(base[0] + base[1], adpt[0] + adpt[1])

    # left panel: slow sign-side pair, on its own y-scale
    tL = _ovpair(axL, 0, dh["t_sign"], dh["t_presign"], COL["base_sign"], COL["las_presign"])
    axL.text(0, tL * 1.05, "%+.1f%%" % dh["o_presign"][2], ha="center", fontsize=9, fontweight="bold")
    axL.set_xticks([0]); axL.set_xticklabels(["Sign\nvs PreSign"])
    axL.set_xlim(-0.7, 0.7); axL.set_ylim(0, tL * 1.18)
    axL.set_ylabel("time (us)  (error bars = sample SD)")
    axL.set_title("sign-side (slow path)")
    axL.grid(axis="y", alpha=0.3)

    # right panel: fast verify-side ops, on a much smaller y-scale
    t1 = _ovpair(axR, 0, dh["t_verify"], dh["t_preverify"], COL["base_verify"], COL["las_preverify"])
    t2 = _ovpair(axR, 1, dh["t_verify"], dh["t_adapt"], COL["base_verify"], COL["adapt"])
    axR.bar(2, dh["t_ext"][0], 0.38, yerr=dh["t_ext"][1], capsize=3,
            color=COL["ext"], edgecolor="black", linewidth=0.3)
    tR = max(t1, t2, dh["t_ext"][0] + dh["t_ext"][1])
    axR.text(0, t1 * 1.05, "%+.1f%%" % dh["o_preverify"][2], ha="center", fontsize=9, fontweight="bold")
    axR.text(1, t2 * 1.05, "%+.1f%%" % dh["o_adapt"][2], ha="center", fontsize=9, fontweight="bold")
    axR.text(2, dh["t_ext"][0] + dh["t_ext"][1] + tR * 0.03, "no base op", ha="center", fontsize=8)
    axR.set_xticks([0, 1, 2])
    axR.set_xticklabels(["Verify\nvs PreVerify", "Verify\nvs Adapt", "Ext /\nExtract"])
    axR.set_ylim(0, tR * 1.18)
    axR.set_title("verify-side ops (fast path)")
    axR.grid(axis="y", alpha=0.3)

    fig.suptitle("Adaptor overhead vs the mirrored base op  (%s; in each pair left=base, "
                 "right=adaptor; %% above bars)" % hl, fontsize=11)
    fig.tight_layout(rect=[0, 0, 1, 0.94])
    _save(fig, out_dir, "timing_overhead_clean")

    # 3. computation_component_absolute (headline; horizontal, sorted) -------------
    comps = [("A-product / commitment w=A·y_mask", dh["c_aprod"], COL["w"]),
             ("challenge hash", dh["c_hash"], COL["hash"]),
             ("c·r (all LAS_M response polys)", dh["c_cr_all"], COL["cr_all"]),
             ("norm check", dh["c_norm"], COL["norm"]),
             ("w + Y", dh["c_wY"], COL["Y"]),
             ("z_hat + witness", dh["c_zwit"], COL["witness_add"])]
    comps.sort(key=lambda t: t[1][0])
    fig, ax = plt.subplots(figsize=(8.5, 4.2))
    ys = list(range(len(comps)))
    ax.barh(ys, [c[1][0] for c in comps], xerr=[c[1][1] for c in comps], capsize=3,
            color=[c[2] for c in comps], edgecolor="black", linewidth=0.3)
    for y, c in zip(ys, comps):
        ax.text(c[1][0] + c[1][1] + 0.01 * comps[-1][1][0], y, "%.2f us" % c[1][0],
                va="center", fontsize=8)
    ax.set_yticks(ys)
    ax.set_yticklabels([c[0] for c in comps], fontsize=9)
    ax.set_xlabel("time (us)  (error bars = sample SD)")
    ax.set_title("Diagnostic component attribution, NOT full-protocol percentage  (%s)" % hl)
    ax.grid(axis="x", alpha=0.3)
    _save(fig, out_dir, "computation_component_absolute")

    # 4. communication_summary_clean (headline; horizontal; byte labels; legend below)
    fig, ax = plt.subplots(figsize=(9.0, 4.0))
    items = [
        ("signature\n(c + z)", [("c", COL["c"], dh["sz_c"]), ("z", COL["z"], dh["sz_z"])], dh["sz_sig"]),
        ("pre-signature\n(c + z_hat)", [("c", COL["c"], dh["sz_c"]), ("z_hat", COL["zhat"], dh["sz_zhat"])], dh["sz_presig"]),
        ("off-chain\n(Y + 2·pre-sig)", [("payload", COL["las_presign"], dh["pl_offchain"])], dh["pl_offchain"]),
        ("settlement\n(2·signature)", [("payload", COL["sig"], dh["pl_settlement"])], dh["pl_settlement"]),
    ]
    LEG = {"c", "z", "z_hat"}                       # only the c/z segments carry a legend entry
    ys = list(range(len(items)))[::-1]
    seen = set()
    totals = []
    for y, (label, segs, total) in zip(ys, items):
        left = 0.0
        for segname, color, val in segs:
            lab = segname if (segname in LEG and segname not in seen) else None
            ax.barh(y, val, left=left, color=color, edgecolor="white", height=0.6, label=lab)
            if lab:
                seen.add(segname)
            left += val
        totals.append(total)
    maxtot = max(totals)
    for y, total in zip(ys, totals):
        ax.text(total + maxtot * 0.012, y, _bytes_label(total), va="center",
                fontsize=9, fontweight="bold")
    ax.set_xlim(0, maxtot * 1.16)                   # headroom so byte labels are not clipped
    ax.set_yticks(ys)
    ax.set_yticklabels([it[0] for it in items], fontsize=9)
    ax.set_xlabel("packed bytes  (byte-level payload only, NOT EVM gas)")
    ax.set_title("Communication summary (%s) — response z / z_hat is %.1f%% of the signature"
                 % (hl, 100.0 * dh["sz_z"] / dh["sz_sig"]))
    ax.legend(fontsize=8, ncol=3, loc="upper center", bbox_to_anchor=(0.5, -0.14), frameon=False)
    fig.text(0.5, -0.02, "detailed c / z splits per level: communication_components.csv",
             ha="center", fontsize=8, style="italic")
    ax.grid(axis="x", alpha=0.3)
    _save(fig, out_dir, "communication_summary_clean")

    # ======================= CROSS-LEVEL figures =============================
    # The four figures above are the L3 "headline"; these show the trends ACROSS
    # paper/L2/L3/L5 that an L3-only view hides (findings: adaptor overhead small &
    # stable; rejection ~e^-1 stable; c*r one-poly flat while aggregates grow with
    # M=n+ell; and the measured Verify/Ext cost attribution at every level).
    xs = list(range(len(levels)))
    EUL = 100.0 / 2.718281828459045          # e^-1 acceptance, as a percentage

    # 5. adaptor_overhead_vs_level (finding 1) ------------------------------------
    fig, ax = plt.subplots(figsize=(8.6, 4.4))
    ov_series = [("PreSign vs Sign", "o_presign", COL["las_presign"]),
                 ("PreVerify vs Verify", "o_preverify", COL["las_preverify"]),
                 ("Adapt vs Verify", "o_adapt", COL["adapt"])]
    w = 0.26
    for i, (lbl, key, c) in enumerate(ov_series):
        vals = [data[lvl][key][2] for lvl in levels]      # [2] = overhead %
        bars = ax.bar([x + (i - 1) * w for x in xs], vals, w, label=lbl,
                      color=c, edgecolor="black", linewidth=0.3)
        for b, v in zip(bars, vals):
            ax.text(b.get_x() + b.get_width() / 2, v + (0.15 if v >= 0 else -0.35),
                    "%+.1f" % v, ha="center", fontsize=7)
    ax.axhline(0, color="black", linewidth=0.6)
    ax.set_xticks(xs); ax.set_xticklabels(levels, fontsize=9)
    ax.set_ylabel("overhead vs the mirrored base op (%)")
    ax.set_title("Adaptor overhead is small and stable across parameter sets "
                 "(all single-digit %)")
    ax.legend(fontsize=8); ax.grid(axis="y", alpha=0.3)
    _save(fig, out_dir, "adaptor_overhead_vs_level")

    # 6. acceptance_vs_level (finding 7) ------------------------------------------
    fig, ax = plt.subplots(figsize=(8.2, 4.2))
    base_acc = [data[lvl]["rej_base"]["accept_pct"] for lvl in levels]
    las_acc = [data[lvl]["rej_las"]["accept_pct"] for lvl in levels]
    w = 0.36
    ax.bar([x - w / 2 for x in xs], base_acc, w, label="Base Sign",
           color=COL["base_sign"], edgecolor="black", linewidth=0.3)
    ax.bar([x + w / 2 for x in xs], las_acc, w, label="LAS PreSign",
           color=COL["las_presign"], edgecolor="black", linewidth=0.3)
    ax.axhline(EUL, color="#d94701", linestyle="--", linewidth=1.2,
               label="e^-1 = %.1f%%" % EUL)
    ax.set_xticks(xs); ax.set_xticklabels(levels, fontsize=9)
    ax.set_ylabel("acceptance per attempt (%)")
    ax.set_ylim(0, max(base_acc + las_acc) * 1.3)
    ax.set_title("Rejection sampling is stable across parameter sets (~37%, ~ e^-1)")
    ax.legend(fontsize=8); ax.grid(axis="y", alpha=0.3)
    _save(fig, out_dir, "acceptance_vs_level")

    # 7. component_scaling_vs_level (finding 4 + verify-side c*t) -----------------
    fig, ax = plt.subplots(figsize=(8.6, 4.6))
    comp_series = [("c*r one poly (sparse c)", "c_cr_one", "o-", COL["cr_all"]),
                   ("c*r all n+ell polys", "c_cr_all", "s-", COL["las_presign"]),
                   ("c*t all n pk polys", "c_ct", "^-", COL["las_preverify"]),
                   ("A-product (w=A*y)", "c_aprod", "D-", COL["w"]),
                   ("challenge hash", "c_hash", "v-", COL["hash"])]
    for lbl, key, style, c in comp_series:
        ax.plot(xs, [data[lvl][key][0] for lvl in levels], style, color=c,
                label=lbl, lw=1.8, ms=6)
    ax.set_xticks(xs)
    ax.set_xticklabels(["%s\n(M=%d)" % (lvl, data[lvl]["M"]) for lvl in levels], fontsize=8)
    ax.set_xlabel("parameter set  (M = n + ell, the response-vector dimension)")
    ax.set_ylabel("time (us)  (diagnostic component estimate)")
    ax.set_title("Component scaling: c*r one-poly stays flat (~14us); aggregates grow with M=n+ell")
    ax.legend(fontsize=8); ax.grid(alpha=0.3)
    _save(fig, out_dir, "component_scaling_vs_level")

    # 8. verify_ext_attribution_vs_level (the measured decomposition) ------------
    fig, (axV, axE) = plt.subplots(1, 2, figsize=(11, 4.7))

    def _stack(ax, parts, op_key, title):
        bottoms = [0.0] * len(levels)
        for lbl, key, c in parts:
            vals = [data[lvl][key][0] for lvl in levels]
            ax.bar(xs, vals, 0.6, bottom=bottoms, label=lbl, color=c, edgecolor="white")
            bottoms = [b + v for b, v in zip(bottoms, vals)]
        ax.plot(xs, [data[lvl][op_key][0] for lvl in levels], "k_", ms=22, mew=2.5,
                label="protocol op (measured)")
        ax.set_xticks(xs); ax.set_xticklabels(levels, fontsize=8)
        ax.set_ylabel("time (us)"); ax.set_title(title, fontsize=9)
        ax.legend(fontsize=7); ax.grid(axis="y", alpha=0.3)

    _stack(axV, [("A-product (A*z)", "c_aprod", COL["w"]),
                 ("c*t (n pk polys)", "c_ct", COL["las_preverify"]),
                 ("challenge hash", "c_hash", COL["hash"]),
                 ("norm check", "c_norm", COL["norm"])],
           "t_verify", "Verify = A*z + c*t + hash (+ norm)")
    _stack(axE, [("s = z - z_hat", "c_ext_sub", COL["witness_add"]),
                 ("A*s (re-derive Y)", "c_ext_amul", COL["w"]),
                 ("t'==A*s check", "c_ext_check", COL["norm"])],
           "t_ext", "Ext ~ one A*s (re-derive the statement)")
    fig.suptitle("Measured cost attribution across parameter sets: stacked components "
                 "vs the protocol op (black tick)", fontsize=11)
    fig.tight_layout(rect=[0, 0, 1, 0.94])
    _save(fig, out_dir, "verify_ext_attribution_vs_level")


def make_app_plots(a, out_dir):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.patches import Patch

    made = []

    # atomic-swap payload breakdown (horizontal, stacked components; legend below) --
    fig, ax = plt.subplots(figsize=(9.0, 3.9))
    items = [
        ("off-chain", [("Y", COL["Y"], a["sw_Y"]),
                       ("pre-sig A", COL["las_presign"], a["sw_preA"]),
                       ("pre-sig B", COL["las_preverify"], a["sw_preB"])], a["sw_offchain"]),
        ("settlement\n(sigs only)", [("sigma_A", COL["sig"], a["sw_sigA"]),
                                     ("sigma_B", COL["adapt"], a["sw_sigB"])], a["sw_settle"]),
        ("settlement\nincl. escrowed Y", [("2·sig", COL["norm"], a["sw_settle"]),
                                          ("2·Y", COL["Y"], 2 * a["sw_Y"])], a["sw_settle_Y"]),
    ]
    ys = list(range(len(items)))[::-1]
    seen = set()
    totals = []
    for y, (label, segs, total) in zip(ys, items):
        left = 0.0
        for segname, color, val in segs:
            ax.barh(y, val, left=left, color=color, edgecolor="white", height=0.6,
                    label=segname if segname not in seen else None)
            seen.add(segname)
            left += val
        totals.append(total)
    maxtot = max(totals)
    for y, total in zip(ys, totals):
        ax.text(total + maxtot * 0.012, y, _bytes_label(total), va="center",
                fontsize=9, fontweight="bold")
    ax.set_xlim(0, maxtot * 1.14)                   # headroom so '22336 B' is not clipped/covered
    ax.set_yticks(ys)
    ax.set_yticklabels([it[0] for it in items], fontsize=9)
    ax.set_xlabel("packed bytes  (simulated-ledger proxy, NOT EVM gas)")
    ax.set_title("Atomic-swap payload breakdown  (L3-like; bench_app3)")
    ax.legend(fontsize=8, ncol=4, loc="upper center", bbox_to_anchor=(0.5, -0.18), frameon=False)
    ax.grid(axis="x", alpha=0.3)
    _save(fig, out_dir, "application_atomic_swap_payload_breakdown")
    made.append("application_atomic_swap_payload_breakdown")

    # multi-hop K-series figures (only if rows exist) ------------------------------
    mh = a.get("mh", [])
    if mh:
        Ks = [r["K"] for r in mh]
        fig, ax = plt.subplots(figsize=(7.5, 4.2))
        ax.plot(Ks, [r["settle_sigs_bytes"] for r in mh], "o-", color=COL["sig"],
                label="settlement signatures (K·sig)")
        ax.plot(Ks, [r["public_stmts_bytes"] for r in mh], "s--", color=COL["Y"],
                label="public statements (K·Y)")
        ax.set_xlabel("path length K (hops)")
        ax.set_ylabel("packed bytes")
        ax.set_title("Multi-hop AMHL settlement payload vs K  (L3-like)")
        ax.legend(fontsize=8)
        ax.grid(alpha=0.3)
        _save(fig, out_dir, "application_multihop_payload_vs_k")
        made.append("application_multihop_payload_vs_k")

        fig, ax = plt.subplots(figsize=(7.5, 4.2))
        ax.plot(Ks, [r["presig_time_ms"] for r in mh], "o-", color=COL["las_presign"])
        ax.set_xlabel("path length K (hops)")
        ax.set_ylabel("pre-sign time per route (ms)")
        ax.set_title("Multi-hop AMHL pre-sign time vs K  (L3-like)")
        ax.grid(alpha=0.3)
        _save(fig, out_dir, "application_multihop_presign_time_vs_k")
        made.append("application_multihop_presign_time_vs_k")

        # AMHL norm-margin: the achieved cumulative witness norm vs the PreSign
        # bound g-k-K, BOTH on one log axis so the (huge) gap is visible -- the
        # bound is extremely loose (the witness ||s_j||inf is tiny: <= K).
        fig, ax = plt.subplots(figsize=(7.5, 4.2))
        ax.plot(Ks, [r["max_norm"] for r in mh], "o-", color=COL["y_witness"],
                label="achieved max ||s_j||inf  (cumulative witness)")
        ax.plot(Ks, [r["bound_gkK"] for r in mh], "s--", color=COL["las_preverify"],
                label="PreSign bound g-k-K")
        ax.set_yscale("log")
        ax.set_xlabel("path length K (hops)")
        ax.set_ylabel("infinity norm  (log scale)")
        ax.set_title("AMHL witness norm vs PreSign bound g-k-K  (L3-like): the bound is "
                     "extremely loose")
        ax.legend(fontsize=8, loc="center right")
        ax.grid(alpha=0.3, which="both")
        _save(fig, out_dir, "application_multihop_norm_vs_k")
        made.append("application_multihop_norm_vs_k")
    return made


# ---------------------------------------------------------------------------
# CSV writers
# ---------------------------------------------------------------------------

def _writer(out_dir, name, header, rows):
    with open(out_dir / name, "w", newline="") as f:
        wr = csv.writer(f)
        wr.writerow(header)
        wr.writerows(rows)


def write_fair_csvs(data, out_dir):
    levels = _levels_in_order(data)

    ops = [("Setup", "t_setup"), ("KeyGen", "t_keygen"), ("Sign", "t_sign"),
           ("Verify", "t_verify"), ("PreSign", "t_presign"), ("PreVerify", "t_preverify"),
           ("Adapt", "t_adapt"), ("Ext / Extract", "t_ext")]
    rows = [[lvl, data[lvl]["n"], data[lvl]["ell"], data[lvl]["kappa"], lab,
             data[lvl][k][0], data[lvl][k][1]] for lvl in levels for lab, k in ops]
    _writer(out_dir, "primary_timing.csv",
            ["level", "n", "ell", "kappa", "operation", "mean_us", "sd_us"], rows)

    rows = []
    for lvl in levels:
        d = data[lvl]
        for pair, key in (("PreSign vs Sign", "o_presign"), ("PreVerify vs Verify", "o_preverify"),
                          ("Adapt vs Verify", "o_adapt")):
            ad, ba, pct = d[key]
            rows.append([lvl, pair, ad, ba, pct])
        rows.append([lvl, "Ext / Extract (separate)", d["t_ext"][0], "", ""])
    _writer(out_dir, "adaptor_overhead.csv",
            ["level", "pair", "adaptor_us", "base_us", "overhead_pct"], rows)

    rows = []
    for lvl in levels:
        for op, key in (("Base Sign", "rej_base"), ("LAS PreSign", "rej_las")):
            r = data[lvl][key]
            rows.append([lvl, op, r["avg"], r["accept_pct"], r["min"], r["max"], r["p50"], r["p95"]])
    _writer(out_dir, "rejection_sampling.csv",
            ["level", "operation", "avg_attempts", "acceptance_pct", "min", "max", "p50", "p95"], rows)

    comps = [("pk = t", "sz_pk"), ("sk = r", "sz_sk"), ("Y = t'", "sz_Y"),
             ("r' = y_witness", "sz_ywit"), ("c", "sz_c"), ("z", "sz_z"), ("z_hat", "sz_zhat"),
             ("signature (c,z)", "sz_sig"), ("pre-signature (c,z_hat)", "sz_presig"),
             ("final adapted sig (c,z)", "sz_adapted")]
    rows = [[lvl, lab, data[lvl][k], round(100.0 * data[lvl][k] / data[lvl]["sz_sig"], 2)]
            for lvl in levels for lab, k in comps]
    _writer(out_dir, "communication_components.csv",
            ["level", "component", "bytes", "pct_of_signature"], rows)

    ccomps = [("A-product / commitment w", "c_aprod"), ("challenge hash", "c_hash"),
              ("c*r one response poly", "c_cr_one"), ("c*r all LAS_M polys", "c_cr_all"),
              ("norm check", "c_norm"), ("w + Y", "c_wY"), ("z_hat + witness", "c_zwit"),
              ("c*t all LAS_N pk polys", "c_ct"), ("KeyGen sample r", "c_keygen_r"),
              ("Ext s = z - z_hat", "c_ext_sub"), ("Ext A*s", "c_ext_amul"),
              ("Ext t'==A*s check", "c_ext_check")]
    rows = [[lvl, lab, data[lvl][k][0], data[lvl][k][1]] for lvl in levels for lab, k in ccomps]
    _writer(out_dir, "computation_components.csv",
            ["level", "component", "mean_us", "sd_us"], rows)

    cat = [("public key", "pk = t", "sz_pk"), ("secret key", "sk = r", "sz_sk"),
           ("statement", "Y = t'", "sz_Y"), ("witness", "r' = y_witness", "sz_ywit"),
           ("challenge", "c", "sz_c"), ("final response", "z", "sz_z"),
           ("pre-sig response", "z_hat", "sz_zhat"), ("signature", "(c, z)", "sz_sig"),
           ("pre-signature", "(c, z_hat)", "sz_presig"),
           ("final adapted signature", "(c, z)", "sz_adapted")]
    rows = [[lvl, obj, notation, data[lvl][k]] for lvl in levels for obj, notation, k in cat]
    _writer(out_dir, "las_object_catalogue.csv",
            ["level", "object", "paper_notation", "bytes"], rows)


def write_app_csvs(a, out_dir):
    note = "L3-like (bench_app3: n=%d ell=%d kappa=%d)" % (a["n"], a["ell"], a["kappa"])
    rows = [
        ["statement_Y_bytes", a["sw_Y"], "bytes", "off-chain msg 1; " + note],
        ["presig_sigmaA_bytes", a["sw_preA"], "bytes", "off-chain msg 2"],
        ["presig_sigmaB_bytes", a["sw_preB"], "bytes", "off-chain msg 3"],
        ["offchain_total_bytes", a["sw_offchain"], "bytes", "Y + preA + preB"],
        ["settlement_sigs_bytes", a["sw_settle"], "bytes", "sigma_A + sigma_B published"],
        ["settlement_incl_Y_bytes", a["sw_settle_Y"], "bytes", "+ 2 escrowed statements Y"],
        ["signing_time_ms", a["sw_time_ms"], "ms", "2xPreSign + 2xAdapt + Ext"],
        ["presign_attempts", a["sw_attempts"], "count", "measured rejection attempts (2 pre-signs)"],
        ["correctness", a["sw_ok"], "flag", "adapted sigs verify; pre-sigs do not"],
    ]
    _writer(out_dir, "application_atomic_swap.csv", ["metric", "value", "unit", "note"], rows)

    rows = [[r["K"], r["bound_gkK"], r["num_presig"], r["attempts_per_presig"],
             r["presig_time_ms"], r["settle_sigs_bytes"], r["public_stmts_bytes"],
             r["max_norm"], r["ok"]] for r in a.get("mh", [])]
    _writer(out_dir, "application_multihop_amhl.csv",
            ["K", "bound_g_minus_k_minus_K", "num_presig", "attempts_per_presig",
             "presig_time_ms", "settle_sigs_bytes", "public_stmts_bytes",
             "max_witness_norm_inf", "ok"], rows)

    rows = [
        ["off-chain", "Y + preA + preB", a["sw_offchain"],
         "Y", a["sw_Y"], "preA", a["sw_preA"], "preB", a["sw_preB"]],
        ["settlement", "sigma_A + sigma_B", a["sw_settle"],
         "sigma_A", a["sw_sigA"], "sigma_B", a["sw_sigB"], "", ""],
        ["settlement_incl_Y", "settlement + 2*Y", a["sw_settle_Y"],
         "settlement", a["sw_settle"], "2*Y", 2 * a["sw_Y"], "", ""],
    ]
    _writer(out_dir, "application_payload_breakdown.csv",
            ["payload", "formula", "total_bytes",
             "part1_label", "part1_bytes", "part2_label", "part2_bytes",
             "part3_label", "part3_bytes"], rows)


# ---------------------------------------------------------------------------
# report figure manifest
# ---------------------------------------------------------------------------

def write_manifest(out_dir, have_app, have_mh):
    H = ["figure_file", "use_in_report_main_or_appendix", "claim_supported", "caution_note"]
    rows = [
        ["timing_timeline_base_vs_las.png/.pdf", "main",
         "Base and LAS protocol cost timelines across paper/L2/L3/L5",
         "two SEPARATE protocols shown side by side; NOT summed into one pipeline"],
        ["protocol_step_timeline.png/.pdf", "main",
         "Cumulative cost per NAMED protocol step; full LAS cycle ~30% over base, "
         "dominated by the shared rejection-sampling Sign/PreSign step",
         "per-level; base and LAS are two separate protocols (not summed); paths share "
         "step 1-2 names (Sign/PreSign, Verify/PreVerify), LAS adds Adapt + Ext"],
        ["timing_overhead_clean.png/.pdf", "main",
         "Adaptor overhead is small: PreSign~Sign, PreVerify~Verify, Adapt~Verify",
         "headline L3 only; per-level numbers in adaptor_overhead.csv; Ext has no base op"],
        ["computation_component_absolute.png/.pdf", "main",
         "Where LAS compute time goes (A-product/hash/c*r dominate)",
         "DIAGNOSTIC component attribution from local copies, NOT a full-protocol %; L3"],
        ["communication_summary_clean.png/.pdf", "main",
         "Signature/pre-sig/off-chain/settlement byte sizes; response z dominates",
         "byte-level payload only, NOT EVM gas; headline L3"],
        ["primary_timing.csv", "table only",
         "per-op mean+/-SD across paper/L2/L3/L5", ""],
        ["adaptor_overhead.csv", "table only",
         "overhead % per mirrored pair across levels", ""],
        ["rejection_sampling.csv", "table only",
         "avg/accept%/min/max/p50/p95 for Base Sign and LAS PreSign",
         "diagnostic; no figure generated by default"],
        ["communication_components.csv", "table only",
         "per-component packed bytes and % of signature across levels", ""],
        ["computation_components.csv", "table only",
         "diagnostic component timings incl. c*r one-poly",
         "diagnostic attribution, not protocol %"],
        ["las_object_catalogue.csv", "table only",
         "object sizes with paper notation (pk=t, sk=r, Y=t', r'=y_witness, ...)",
         "no figure generated by default (catalogue bar chart was too crowded)"],
        ["adaptor_overhead_vs_level.png/.pdf", "main",
         "Adaptor overhead small and stable across paper/L2/L3/L5 (all single-digit %)",
         "cross-level; from adaptor_overhead.csv (the paper set is the noisiest)"],
        ["acceptance_vs_level.png/.pdf", "appendix",
         "Rejection-sampling acceptance ~37% (~ e^-1) at every parameter set",
         "cross-level; from rejection_sampling.csv"],
        ["component_scaling_vs_level.png/.pdf", "main",
         "c*r one-poly flat ~14us; aggregate c*r/c*t/A-product grow with M=n+ell",
         "cross-level; DIAGNOSTIC component estimates, not protocol %"],
        ["verify_ext_attribution_vs_level.png/.pdf", "main",
         "Measured attribution across levels: Verify=A*z+c*t+hash; Ext~one A*s",
         "cross-level; stacked components vs the protocol op (black tick); estimates"],
    ]
    if have_app:
        rows += [
            ["application_atomic_swap_payload_breakdown.png/.pdf", "main",
             "Atomic-swap payload story: off-chain (Y + 2 pre-sigs) vs settlement (2 sigs)",
             "L3-like (bench_app3 only); simulated-ledger byte proxy, NOT gas"],
            ["application_atomic_swap.csv", "table only",
             "atomic-swap payload / timing / attempts / correctness", "L3-like"],
            ["application_payload_breakdown.csv", "table only",
             "atomic-swap payload composition (off-chain, settlement, settlement incl. Y)", "L3-like"],
            ["application_multihop_amhl.csv", "table only",
             "multi-hop AMHL K-series: bound, #presig, attempts, time, payload, witness norm", "L3-like"],
        ]
    if have_mh:
        rows += [
            ["application_multihop_payload_vs_k.png/.pdf", "main",
             "Multi-hop settlement payload grows linearly in path length K",
             "L3-like; simulated-ledger byte proxy"],
            ["application_multihop_presign_time_vs_k.png/.pdf", "appendix",
             "Multi-hop pre-sign time per route vs K (roughly flat per hop)",
             "L3-like; mean over the bench_app3 routes/K"],
            ["application_multihop_norm_vs_k.png/.pdf", "main",
             "AMHL achieved witness norm ||s_j||inf vs the PreSign bound g-k-K: "
             "the bound is extremely loose (witness <= K, bound ~ gamma)",
             "L3-like; log y-axis shows both series; norm grows linearly (<=K), bound ~flat"],
        ]
    _writer(out_dir, "report_figure_manifest.csv", H, rows)


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
    ap = argparse.ArgumentParser(description="Plot one LAS benchmark-suite run folder.")
    ap.add_argument("--input-dir", default=None,
                    help="run folder with fair_*.log (default: evidence/latest, else evidence/)")
    ap.add_argument("--output-dir", default=None,
                    help="where CSVs+figures are written (default: same as --input-dir)")
    args = ap.parse_args(argv)

    repo_root = Path(__file__).resolve().parents[1]
    in_dir = resolve_input_dir(repo_root, args.input_dir)
    out_dir = Path(args.output_dir).resolve() if args.output_dir else in_dir
    if not in_dir.exists():
        sys.exit("ERROR: input dir does not exist: %s" % in_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    # ---- fair benchmark (required) ----
    data, missing = {}, []
    for fname, lvl in LEVEL_FILES:
        path = in_dir / fname
        if not path.exists():
            missing.append(fname)
            continue
        try:
            d = parse_log(path)
            validate(d)
        except ParseError as e:
            sys.exit("ERROR parsing %s:\n%s\n\nHas bench_levels stdout changed, or do the "
                     "logs need regenerating with scripts/run_benchmark_suite.sh?" % (path, e))
        data[lvl] = d
    if not data:
        sys.exit("ERROR: no fair_*.log found in %s (looked for %s).\nRegenerate with "
                 "scripts/run_benchmark_suite.sh first."
                 % (in_dir, ", ".join(f for f, _ in LEVEL_FILES)))
    if missing:
        print("WARNING: missing fair logs (skipped): %s" % ", ".join(missing), file=sys.stderr)

    write_fair_csvs(data, out_dir)

    # ---- application benchmark (optional) ----
    app = None
    app_path = in_dir / APP_FILE
    if app_path.exists():
        try:
            app = parse_app_log(app_path)
            validate_app(app)
            write_app_csvs(app, out_dir)
        except ParseError as e:
            print("WARNING: %s could not be parsed/validated; skipping application "
                  "CSVs/figures (fair benchmark unaffected):\n  %s" % (APP_FILE, e), file=sys.stderr)
            app = None
    else:
        print("WARNING: %s not found in %s; skipping application CSVs/figures."
              % (APP_FILE, in_dir), file=sys.stderr)

    have_mh = bool(app and app.get("mh"))
    write_manifest(out_dir, have_app=bool(app), have_mh=have_mh)

    # ---- figures (skip cleanly if matplotlib unavailable) ----
    made_app = []
    try:
        make_fair_plots(data, out_dir)
        if app:
            made_app = make_app_plots(app, out_dir)
    except ImportError as e:
        print("WARNING: matplotlib unavailable (%s); CSVs + manifest were written, "
              "figures skipped. Install with: pip install matplotlib" % e, file=sys.stderr)

    print("Run folder    : %s" % out_dir)
    print("Fair levels   : %s" % ", ".join(_levels_in_order(data)))
    print("Application   : %s" % ("parsed (L3-like)" if app else "absent/skipped"))
    print("Wrote CSVs + report_figure_manifest.csv and report-quality figures (PNG+PDF).")


if __name__ == "__main__":
    main()
