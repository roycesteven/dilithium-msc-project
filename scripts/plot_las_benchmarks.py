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

Paper notation (eprint 2020/845): pp=(A,H), pk=t, sk=r, statement Y=t', witness r'
(the y of the pair (Y,y); Algorithm 2 writes r':=y), masking randomness y,
commitment w=A*y (hashed into c, NOT transmitted), pre-signature response z_hat
(ASCII fallback for the paper's hat-z), final response z, Ext s=z-z_hat. No invented
aliases are used. The base path and the LAS adaptor path are two separate protocols
and are never summed.
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

# Human-readable meaning of each setting (Meeting-4: "paper / L2 / L3 / L5" are
# NOT self-explanatory). These are LAS PARAMETER SETS derived from simplified
# Dilithium dimensions, NOT formal NIST security-level claims. (LaTeX-safe text.)
LEVEL_DESC = {
    "paper": "LAS-2020/845 reference parameter set; built on the Dilithium modulus "
             "Q=8380417, not the exact paper modulus.",
    "L2": "Simplified Dilithium-II parameter setting (LAS/Base) from the Dilithium "
          "mode-2 dimensions (not a formal NIST security claim).",
    "L3": "Simplified Dilithium-III parameter setting (LAS/Base) from the Dilithium "
          "mode-3 dimensions (target; not a formal NIST security claim).",
    "L5": "Simplified Dilithium-V parameter setting (LAS/Base) from the Dilithium "
          "mode-5 dimensions (not a formal NIST security claim).",
}
# Short display labels for figures/tables (the data keys stay paper/L2/L3/L5).
LEVEL_DISPLAY = {
    "paper": "LAS-2020/845 reference",
    "L2": "Simplified Dilithium-II",
    "L3": "Simplified Dilithium-III (target)",
    "L5": "Simplified Dilithium-V",
}
# Compact labels for the paper-facing package (one short token per setting).
PAPER_DISPLAY = {
    "paper": "LAS-2020/845 reference",
    "L2": "Simplified Dilithium-II",
    "L3": "Simplified Dilithium-III",
    "L5": "Simplified Dilithium-V",
}
# Distinct fill per level for grouped/per-level bars.
LEVEL_COLORS = ["#6baed6", "#fd8d3c", "#74c476", "#9e9ac8"]
# One shared sentence that disambiguates the axes a reader cannot infer.
LEVELS_CAPTION = (
    "Settings are LAS parameter sets, NOT formal NIST levels: the LAS-2020/845 "
    "reference set uses Dilithium's modulus; the Simplified Dilithium-II/III/V sets "
    "reuse the Dilithium mode-2/3/5 dimensions.   Base = simplified Dilithium-style "
    "basic signature (no statement Y);  LAS = adaptor path (statement Y folded into "
    "the hash)."
)


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
        # Tolerant: matches the current "Ext" label and the older "Ext / Extract"
        # label, so existing logs and regenerated logs both parse.
        d["t_ext"] = _anchored_mean_sd(prim, r"Ext(?:\s*/\s*Extract)?", "Ext")

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
        # Tolerant: matches the current "r'" label and the older "r' = y_witness".
        d["sz_ywit"] = grab_int(r"r'(?:\s*=\s*y_witness)?\s+(\d+)", "witness r'")
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


def parse_metadata(path):
    """Best-effort one-line machine descriptor from metadata.txt (CPU / WSL /
    compiler) so the timings carry their hardware context onto the figure itself
    (Meeting-4: state the machine -- numbers differ on a different PC). Returns
    '' if the file is absent or unparseable; never fails the plotting run."""
    try:
        lines = Path(path).read_text(errors="replace").splitlines()
    except OSError:
        return ""
    f = {}
    for ln in lines:
        m = re.match(r"^\s*(cpu|wsl|compiler|os_uname)\s*:\s*(.+?)\s*$", ln)
        if m:
            f[m.group(1)] = m.group(2)
    cpu = f.get("cpu", "")
    plat = "WSL2" if f.get("wsl", "").lower().startswith("yes") else "Linux"
    cm = re.search(r"(\d+\.\d+\.\d+)", f.get("compiler", ""))
    comp = ("gcc " + cm.group(1)) if cm else f.get("compiler", "")
    return " / ".join(p for p in (cpu, plat, comp) if p)


# ---------------------------------------------------------------------------
# colours / helpers
# ---------------------------------------------------------------------------

COL = {
    "base_sign": "#9ecae1", "las_presign": "#08519c",
    "base_verify": "#fdd0a2", "las_preverify": "#d94701",
    "adapt": "#6a51a3", "witness_add": "#bcbddc", "ext": "#238b45",
    "c": "#d9a7a0", "z": "#99000d", "zhat": "#cb181d",
    "Y": "#1b9e9e", "witness": "#238b45", "w": "#969696",
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
# figure helpers (Meeting-4: machine on the figure; params on every setting)
# ---------------------------------------------------------------------------

def _machine_footer_text(machine):
    base = "single thread, -O3, 10 runs x 1000 iters/op; error bars = sample SD"
    return ("Machine: %s   |   %s" % (machine, base)) if machine else base


def _add_footer(fig, machine, caption=True):
    fig.text(0.5, -0.03, _machine_footer_text(machine), ha="center", va="top",
             fontsize=7, style="italic", color="#444444")
    if caption:
        fig.text(0.5, -0.075, LEVELS_CAPTION, ha="center", va="top", fontsize=7,
                 color="#333333", wrap=True)


def _level_tick(d, lvl):
    """Multi-line x-tick that carries the key parameters with the level label."""
    return "%s\nn=%d, ℓ=%d\nκ=%d" % (lvl, d["n"], d["ell"], d["kappa"])


# ---------------------------------------------------------------------------
# NEW headline figures (Meeting-4 main set: parameters table, per-operation
# timing as the PRIMARY timing result, and the component-level comm breakdown)
# ---------------------------------------------------------------------------

def make_param_table(data, out_dir, machine):
    """Standalone parameter-set table -- the artefact Meeting-4 said was missing.
    Numeric-only table (kept readable, no overlap); the per-setting meanings go in
    the wrapped footnote below and in parameter_sets.csv / parameter_sets.tex."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import textwrap

    levels = _levels_in_order(data)
    cols = ["setting", "n", "ℓ", "M=n+ℓ", "κ", "γ", "N", "Q"]
    colw = [0.30, 0.07, 0.07, 0.13, 0.08, 0.15, 0.08, 0.12]
    cell = [[LEVEL_DISPLAY.get(lvl, lvl), data[lvl]["n"], data[lvl]["ell"],
             data[lvl]["M"], data[lvl]["kappa"], data[lvl]["gamma"],
             data[lvl]["N"], data[lvl]["Q"]] for lvl in levels]
    # Two panels: table on top, explanatory note + machine footer below, so the
    # text never collides with the table (matplotlib manages the separation).
    fig, (axT, axN) = plt.subplots(
        2, 1, figsize=(11.5, 2.0 + 0.5 * len(levels)),
        gridspec_kw={"height_ratios": [len(levels) + 1, 1.5]})
    axT.axis("off")
    axN.axis("off")
    tbl = axT.table(cellText=cell, colLabels=cols, colWidths=colw,
                    loc="center", cellLoc="center")
    tbl.auto_set_font_size(False)
    tbl.set_fontsize(9.5)
    tbl.scale(1, 1.7)
    for j in range(len(cols)):
        tbl[0, j].set_facecolor("#08519c")
        tbl[0, j].set_text_props(color="white", fontweight="bold")
    fig.suptitle("LAS benchmark parameter sets  (all share the Dilithium ring: "
                 "N=256, Q=8380417 ≈ 2²³)", fontsize=11, y=0.97)
    note = textwrap.fill(
        "Settings are LAS parameter sets derived from simplified Dilithium "
        "dimensions, NOT formal NIST security levels.  κ = challenge weight, "
        "γ = rejection bound, M = n + ℓ = response-vector dimension, "
        "N = 256 ring degree, Q = 8380417 modulus.  Full per-setting meanings: "
        "parameter_sets.csv / parameter_sets.tex.", 118)
    axN.text(0.5, 0.92, note, ha="center", va="top", transform=axN.transAxes,
             fontsize=8, color="#333333")
    axN.text(0.5, 0.08, _machine_footer_text(machine), ha="center", va="top",
             transform=axN.transAxes, fontsize=7, style="italic", color="#444444")
    _save(fig, out_dir, "parameter_sets")


def make_per_op_timing(data, out_dir, machine):
    """PRIMARY timing figure (Meeting-4): every operation reported INDEPENDENTLY
    (never summed), with BASE vs LAS explicit on the x-axis and one bar per
    parameter set. Error bars = sample SD."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    levels = _levels_in_order(data)
    # BASE = simplified Dilithium-style signature; LAS = adaptor operations.
    ops = [("BASE KeyGen", "t_keygen"), ("BASE Sign", "t_sign"),
           ("BASE Verify", "t_verify"), ("LAS PreSign", "t_presign"),
           ("LAS PreVerify", "t_preverify"), ("LAS Adapt", "t_adapt"),
           ("LAS Ext", "t_ext")]
    n_base = 3
    xs = list(range(len(ops)))
    nlev = len(levels)
    width = 0.8 / max(nlev, 1)
    fig, ax = plt.subplots(figsize=(12.0, 5.6))
    for i, lvl in enumerate(levels):
        d = data[lvl]
        means = [d[k][0] for _, k in ops]
        sds = [d[k][1] for _, k in ops]
        off = (i - (nlev - 1) / 2.0) * width
        ax.bar([x + off for x in xs], means, width, yerr=sds, capsize=2,
               color=LEVEL_COLORS[i % len(LEVEL_COLORS)], edgecolor="black",
               linewidth=0.3,
               label="%s" % LEVEL_DISPLAY.get(lvl, lvl))
    ax.set_xticks(xs)
    ax.set_xticklabels([n.replace(" ", "\n") for n, _ in ops], fontsize=8.5)
    ax.set_ylabel("time per operation (µs)")
    ax.set_title("Per-operation timing (µs): base signature vs LAS adaptor")
    ymax = ax.get_ylim()[1]
    ax.axvline(n_base - 0.5, color="#888888", linestyle="--", linewidth=1.0)
    ax.text((n_base - 1) / 2.0, ymax * 0.97, "BASE", ha="center", va="top",
            fontsize=10, fontweight="bold", color="#333333")
    ax.text((n_base + len(ops) - 1) / 2.0, ymax * 0.97, "LAS adaptor", ha="center",
            va="top", fontsize=10, fontweight="bold", color="#08519c")
    ax.legend(fontsize=8, title="parameter set", title_fontsize=8)
    ax.grid(axis="y", alpha=0.3)
    _add_footer(fig, machine)
    _save(fig, out_dir, "per_operation_timing")


def make_comm_components(data, out_dir, machine):
    """Component-level communication breakdown (Meeting-4): pk, sk, Y, witness,
    c, z, signature, pre-signature and adapted signature -- and the explicit note
    that sig == pre-sig == adapted (they differ only by the ternary witness)."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import textwrap

    hl = _headline(data)
    dh = data[hl]
    rows = [
        ("public key  pk = t", dh["sz_pk"], COL["pk"]),
        ("secret key  sk = r", dh["sz_sk"], COL["sk"]),
        ("statement  Y = t'", dh["sz_Y"], COL["Y"]),
        ("adaptor witness  r'", dh["sz_ywit"], COL["witness"]),
        ("challenge  c", dh["sz_c"], COL["c"]),
        ("response  z / ẑ", dh["sz_z"], COL["z"]),
        ("signature  (c, z)", dh["sz_sig"], COL["sig"]),
        ("pre-signature  (c, ẑ)", dh["sz_presig"], COL["presig"]),
        ("adapted sig  (c, z)", dh["sz_adapted"], COL["adapt"]),
    ]
    ys = list(range(len(rows)))[::-1]
    fig, ax = plt.subplots(figsize=(9.8, 5.2))
    ax.barh(ys, [r[1] for r in rows], color=[r[2] for r in rows],
            edgecolor="black", linewidth=0.3, height=0.66)
    maxv = max(r[1] for r in rows)
    for y, r in zip(ys, rows):
        ax.text(r[1] + maxv * 0.01, y, "%d B" % r[1], va="center",
                fontsize=8.5, fontweight="bold")
    ax.set_xlim(0, maxv * 1.18)
    ax.set_yticks(ys)
    ax.set_yticklabels([r[0] for r in rows], fontsize=9)
    ax.set_xlabel("packed bytes  (byte-level wire size, NOT EVM gas)")
    ax.set_title("Communication component sizes (%s: n=%d, ℓ=%d, κ=%d) — "
                 "response z is %.1f%% of the signature"
                 % (LEVEL_DISPLAY.get(hl, hl), dh["n"], dh["ell"], dh["kappa"],
                    100.0 * dh["sz_z"] / dh["sz_sig"]))
    note = textwrap.fill(
        "signature = (c, z),  pre-signature = (c, ẑ),  "
        "adapted = (c, ẑ + r'):  all %d B — same encoded size because the "
        "adaptor witness r' changes the response value, it does not add a new "
        "serialized field." % dh["sz_sig"], 118)
    ax.grid(axis="x", alpha=0.3)
    fig.text(0.5, -0.02, note, ha="center", va="top", fontsize=8, style="italic",
             color="#222222")
    fig.text(0.5, -0.20, _machine_footer_text(machine), ha="center", va="top",
             fontsize=7, style="italic", color="#444444")
    _save(fig, out_dir, "communication_components_clean")


# ---------------------------------------------------------------------------
# REPORT-CLEAN variants (for LaTeX inclusion): no baked-in long title, no
# machine footer, no LEVELS_CAPTION footer, no explanatory paragraph -- the
# figure explanation lives in the LaTeX caption (notation guide §9). Axes,
# legends, value labels and mean +/- SD error bars are kept.
# ---------------------------------------------------------------------------

def make_param_table_report(data, out_dir):
    """parameter_sets_report: the numeric parameter table only (no title/footnote/
    footer); the meaning of each setting goes in the LaTeX caption."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    levels = _levels_in_order(data)
    # Fully spelled-out headers (no bare single-letter abbreviations): the metric
    # name on top, the paper symbol below it.
    cols = ["parameter\nsetting", "module rank\nn", "module rank\nℓ",
            "response dimension\nM = n + ℓ", "challenge weight\nκ",
            "masking bound\nγ", "ring degree\nN", "modulus\nQ"]
    colw = [0.18, 0.10, 0.10, 0.18, 0.13, 0.12, 0.09, 0.10]
    cell = [[LEVEL_DISPLAY.get(lvl, lvl), data[lvl]["n"], data[lvl]["ell"],
             data[lvl]["M"], data[lvl]["kappa"], data[lvl]["gamma"],
             data[lvl]["N"], data[lvl]["Q"]] for lvl in levels]
    fig, ax = plt.subplots(figsize=(13.5, 1.0 + 0.55 * (len(levels) + 1)))
    ax.axis("off")
    tbl = ax.table(cellText=cell, colLabels=cols, colWidths=colw,
                   loc="center", cellLoc="center")
    tbl.auto_set_font_size(False)
    tbl.set_fontsize(9)
    tbl.scale(1, 2.2)
    for j in range(len(cols)):
        tbl[0, j].set_facecolor("#08519c")
        tbl[0, j].set_text_props(color="white", fontweight="bold")
    _save(fig, out_dir, "parameter_sets_report")


def make_per_op_timing_report(data, out_dir):
    """per_operation_timing_report: per-operation timing, one bar per parameter set,
    mean +/- SD error bars, basic signature vs LAS adaptor. No title/footer."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    levels = _levels_in_order(data)
    ops = [("KeyGen", "t_keygen"), ("Sign", "t_sign"), ("Verify", "t_verify"),
           ("PreSign", "t_presign"), ("PreVerify", "t_preverify"),
           ("Adapt", "t_adapt"), ("Ext", "t_ext")]
    n_base = 3
    xs = list(range(len(ops)))
    nlev = len(levels)
    width = 0.8 / max(nlev, 1)
    fig, ax = plt.subplots(figsize=(12.0, 5.4))
    for i, lvl in enumerate(levels):
        d = data[lvl]
        means = [d[k][0] for _, k in ops]
        sds = [d[k][1] for _, k in ops]
        off = (i - (nlev - 1) / 2.0) * width
        ax.bar([x + off for x in xs], means, width, yerr=sds, capsize=2,
               color=LEVEL_COLORS[i % len(LEVEL_COLORS)], edgecolor="black",
               linewidth=0.3,
               label="%s" % LEVEL_DISPLAY.get(lvl, lvl))
    ax.set_xticks(xs)
    ax.set_xticklabels([n for n, _ in ops])
    ax.set_ylabel("time per operation (microseconds)")
    ymax = ax.get_ylim()[1]
    ax.axvline(n_base - 0.5, color="#888888", linestyle="--", linewidth=1.0)
    ax.text((n_base - 1) / 2.0, ymax * 0.97, "basic signature operations",
            ha="center", va="top", fontsize=10, fontweight="bold", color="#333333")
    ax.text((n_base + len(ops) - 1) / 2.0, ymax * 0.97, "LAS adaptor operations",
            ha="center", va="top", fontsize=10, fontweight="bold", color="#08519c")
    ax.legend(fontsize=8, title="parameter setting (scaling context)",
              title_fontsize=8)
    ax.grid(axis="y", alpha=0.3)
    _save(fig, out_dir, "per_operation_timing_report")


def make_adaptor_overhead_report(data, out_dir):
    """adaptor_overhead_vs_level_report: adaptor op vs the mirrored basic op (%),
    grouped by parameter set, value labels. No title/footer."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    levels = _levels_in_order(data)
    short = {"paper": "LAS-2020/845 reference", "L2": "Simplified Dilithium-II",
             "L3": "Simplified Dilithium-III", "L5": "Simplified Dilithium-V"}
    xs = list(range(len(levels)))
    fig, ax = plt.subplots(figsize=(8.8, 5.2))
    ov_series = [("PreSign versus Sign", "o_presign", COL["las_presign"]),
                 ("PreVerify versus Verify", "o_preverify", COL["las_preverify"]),
                 ("Adapt versus Verify", "o_adapt", COL["adapt"])]
    w = 0.26
    allvals = []
    for i, (lbl, key, c) in enumerate(ov_series):
        vals = [data[lvl][key][2] for lvl in levels]
        allvals += vals
        bars = ax.bar([x + (i - 1) * w for x in xs], vals, w, label=lbl,
                      color=c, edgecolor="black", linewidth=0.3)
        for b, v in zip(bars, vals):
            ax.text(b.get_x() + b.get_width() / 2, v + (0.15 if v >= 0 else -0.35),
                    "%+.1f" % v, ha="center", fontsize=7)
    ax.axhline(0, color="black", linewidth=0.6)
    ax.set_ylim(top=max(allvals) * 1.28)              # headroom so the legend clears the bars
    ax.set_xticks(xs)
    ax.set_xticklabels([short.get(lvl, lvl) for lvl in levels], fontsize=7.5)
    ax.set_xlabel("parameter setting (scaling context)")
    ax.set_ylabel("overhead versus basic operation (percent)", fontsize=9)
    ax.legend(fontsize=8, loc="upper right")
    ax.grid(axis="y", alpha=0.3)
    _save(fig, out_dir, "adaptor_overhead_vs_level_report")


def make_comm_components_report(data, out_dir):
    """communication_components_clean_report: component byte sizes with value labels.
    No title/footer/note; the adapted-sig structure is shown inline in the label."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    hl = _headline(data)
    dh = data[hl]
    rows = [
        ("public key  pk = t", dh["sz_pk"], COL["pk"]),
        ("secret key  sk = r", dh["sz_sk"], COL["sk"]),
        ("statement  Y = t'", dh["sz_Y"], COL["Y"]),
        ("adaptor witness  r'", dh["sz_ywit"], COL["witness"]),
        ("challenge  c", dh["sz_c"], COL["c"]),
        ("response  z / ẑ", dh["sz_z"], COL["z"]),
        ("signature  (c, z)", dh["sz_sig"], COL["sig"]),
        ("pre-signature  (c, ẑ)", dh["sz_presig"], COL["presig"]),
        ("adapted signature  (c, z = ẑ + r')", dh["sz_adapted"], COL["adapt"]),
    ]
    ys = list(range(len(rows)))[::-1]
    fig, ax = plt.subplots(figsize=(10.5, 5.0))
    ax.barh(ys, [r[1] for r in rows], color=[r[2] for r in rows],
            edgecolor="black", linewidth=0.3, height=0.66)
    maxv = max(r[1] for r in rows)
    for y, r in zip(ys, rows):
        ax.text(r[1] + maxv * 0.01, y, "%d bytes" % r[1], va="center",
                fontsize=8.5, fontweight="bold")
    ax.set_xlim(0, maxv * 1.20)
    ax.set_yticks(ys)
    ax.set_yticklabels([r[0] for r in rows], fontsize=9)
    ax.set_xlabel("serialized size in bytes  (wire size, not on-chain gas)")
    ax.grid(axis="x", alpha=0.3)
    _save(fig, out_dir, "communication_components_clean_report")


# ---------------------------------------------------------------------------
# PAPER-FACING package (Meeting-4 final set). Main story = the basic simplified
# Dilithium-style signature vs the LAS adaptor signature. The LAS-2020/845 reference
# and Simplified Dilithium-II/III/V are engineering parameter settings only. Style: no baked-in
# long title, no machine footer, no caption paragraph; explanation lives in the
# LaTeX caption / KEY_FINDINGS_paper.md.
# ---------------------------------------------------------------------------

def write_param_sets_paper_tex(data, out_dir):
    """Table 1 (paper): parameter settings. Standard tabular; \\input into report."""
    levels = _levels_in_order(data)
    out = [
        "% parameter_sets_paper.tex -- auto-generated by plot_las_benchmarks.py; do not hand-edit.",
        "\\begin{tabular}{l r r r r r r r}",
        "\\hline",
        "Setting & $n$ & $\\ell$ & $M=n+\\ell$ & $\\kappa$ & $\\gamma$ & $N$ & $Q$ \\\\",
        "\\hline",
    ]
    for lvl in levels:
        d = data[lvl]
        out.append("%s & %d & %d & %d & %d & %d & %d & %d \\\\" % (
            PAPER_DISPLAY.get(lvl, lvl), d["n"], d["ell"], d["M"], d["kappa"],
            d["gamma"], d["N"], d["Q"]))
    out += [
        "\\hline",
        "\\multicolumn{8}{l}{\\footnotesize the LAS-2020/845 reference and the Simplified Dilithium-II/III/V sets are engineering parameter settings} \\\\",
        "\\multicolumn{8}{l}{\\footnotesize for scaling context, not formal NIST-equivalent security levels.} \\\\",
        "\\hline",
        "\\end{tabular}",
    ]
    (out_dir / "parameter_sets_paper.tex").write_text("\n".join(out) + "\n")


def make_per_op_timing_paper(data, out_dir):
    """Figure 1 (paper): per-operation timing at the headline setting; basic
    signature operations vs LAS adaptor operations, reported independently (not
    cumulatively), mean +/- standard deviation."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    hl = _headline(data)
    d = data[hl]
    ops = [("KeyGen", "t_keygen", "ord"), ("Sign", "t_sign", "ord"),
           ("Verify", "t_verify", "ord"), ("PreSign", "t_presign", "las"),
           ("PreVerify", "t_preverify", "las"), ("Adapt", "t_adapt", "las"),
           ("Ext", "t_ext", "las")]
    n_ord = 3
    c_ord, c_las = "#9ecae1", "#08519c"
    xs = list(range(len(ops)))
    means = [d[k][0] for _, k, _ in ops]
    sds = [d[k][1] for _, k, _ in ops]
    colors = [c_ord if g == "ord" else c_las for _, _, g in ops]
    fig, ax = plt.subplots(figsize=(9.0, 5.0))
    ax.bar(xs, means, 0.62, yerr=sds, capsize=3, color=colors,
           edgecolor="black", linewidth=0.4)
    top = max(m + s for m, s in zip(means, sds))
    for x, m, s in zip(xs, means, sds):
        ax.text(x, m + s + top * 0.015, "%.0f" % m, ha="center", va="bottom",
                fontsize=8)
    ax.set_xticks(xs)
    ax.set_xticklabels([n for n, _, _ in ops])
    ax.set_ylabel("time per operation (microseconds)")
    ax.set_ylim(0, top * 1.30)
    ax.axvline(n_ord - 0.5, color="#888888", linestyle="--", linewidth=1.0)
    ax.text((n_ord - 1) / 2.0, top * 1.24, "basic signature", ha="center",
            va="top", fontsize=10, fontweight="bold", color="#08306b")
    ax.text((n_ord + len(ops) - 1) / 2.0, top * 1.24, "LAS adaptor", ha="center",
            va="top", fontsize=10, fontweight="bold", color="#08519c")
    # Short scientific setting label only; the full parameter list (n, ℓ, M, κ, γ,
    # N, q) lives in the report \caption, not baked into the plot body.
    ax.text(0.0, 1.02, "%s setting" % PAPER_DISPLAY.get(hl, hl),
            transform=ax.transAxes, ha="left", va="bottom", fontsize=9,
            color="#444444")
    # No legend: the dashed divider, the two group labels and the colour grouping
    # already identify basic (light) vs LAS (dark) bars unambiguously.
    ax.grid(axis="y", alpha=0.3)
    _save(fig, out_dir, "per_operation_timing_paper")


def write_per_op_paper_tex(data, out_dir):
    """Companion table for Figure 1: per-operation mean +/- standard deviation
    (microseconds) across every parameter setting."""
    levels = _levels_in_order(data)
    ncol = len(levels) + 1
    head = "Operation & " + " & ".join(PAPER_DISPLAY.get(l, l) for l in levels) + " \\\\"
    out = [
        "% per_operation_timing_paper.tex -- auto-generated; companion table to Figure 1.",
        "\\begin{tabular}{l%s}" % (" r" * len(levels)),
        "\\hline",
        head,
        "\\hline",
        "\\multicolumn{%d}{l}{\\textit{basic signature}} \\\\" % ncol,
    ]

    def row(name, key):
        cells = " & ".join("%.1f $\\pm$ %.1f" % (data[l][key][0], data[l][key][1])
                           for l in levels)
        return "%s & %s \\\\" % (name, cells)

    for name, key in (("KeyGen", "t_keygen"), ("Sign", "t_sign"),
                      ("Verify", "t_verify")):
        out.append(row(name, key))
    out += ["\\hline",
            "\\multicolumn{%d}{l}{\\textit{LAS adaptor}} \\\\" % ncol]
    for name, key in (("PreSign", "t_presign"), ("PreVerify", "t_preverify"),
                      ("Adapt", "t_adapt"), ("Ext", "t_ext")):
        out.append(row(name, key))
    out += [
        "\\hline",
        "\\multicolumn{%d}{l}{\\footnotesize time in microseconds, mean $\\pm$ sample standard deviation.} \\\\" % ncol,
        "\\hline",
        "\\end{tabular}",
    ]
    (out_dir / "per_operation_timing_paper.tex").write_text("\n".join(out) + "\n")


def make_comm_components_paper(data, out_dir):
    """Figure 2 (paper): serialized component sizes at the headline setting. The
    three signature objects share one colour so their equal size is obvious; the
    statement Y is highlighted as the adaptor-lock object. All components are shown
    (pk, sk, c, z/z_hat, Y, witness, signature, pre-signature, adapted signature)."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    hl = _headline(data)
    dh = data[hl]
    sig_c = "#525252"      # the three equal-size signature objects share this colour
    rows = [
        ("public key  (pk = t)", dh["sz_pk"], COL["pk"]),
        ("secret key  (sk = r)", dh["sz_sk"], COL["sk"]),
        ("challenge  c", dh["sz_c"], COL["c"]),
        ("response  z / ẑ", dh["sz_z"], COL["z"]),
        ("statement  Y = t'", dh["sz_Y"], "#1b9e9e"),
        ("adaptor witness  r'", dh["sz_ywit"], COL["witness"]),
        ("signature  (c, z)", dh["sz_sig"], sig_c),
        ("pre-signature  (c, ẑ)", dh["sz_presig"], sig_c),
        ("adapted signature  (c, z = ẑ + r')", dh["sz_adapted"], sig_c),
    ]
    ys = list(range(len(rows)))[::-1]
    fig, ax = plt.subplots(figsize=(10.5, 5.2))
    ax.barh(ys, [r[1] for r in rows], color=[r[2] for r in rows],
            edgecolor="black", linewidth=0.4, height=0.66)
    maxv = max(r[1] for r in rows)
    for y, r in zip(ys, rows):
        ax.text(r[1] + maxv * 0.01, y, "%d bytes" % r[1], va="center",
                fontsize=8.5, fontweight="bold")
    ax.set_xlim(0, maxv * 1.22)
    ax.set_yticks(ys)
    ax.set_yticklabels([r[0] for r in rows], fontsize=9)
    ax.set_xlabel("serialized size in bytes  (wire size, not on-chain gas)")
    ax.text(0.0, 1.02, "%s setting" % PAPER_DISPLAY.get(hl, hl),
            transform=ax.transAxes, ha="left", va="bottom", fontsize=9,
            color="#444444")
    ax.grid(axis="x", alpha=0.3)
    _save(fig, out_dir, "communication_components_paper")


def write_comm_paper_tex(data, out_dir):
    """Table 2 (paper): serialized component sizes (bytes) at the headline setting."""
    hl = _headline(data)
    d = data[hl]
    rows = [
        ("public key", "$\\mathrm{pk}=t$", d["sz_pk"]),
        ("secret key", "$\\mathrm{sk}=r$", d["sz_sk"]),
        ("challenge", "$c$", d["sz_c"]),
        ("response", "$z$ / $\\hat{z}$", d["sz_z"]),
        ("statement", "$Y=t'$", d["sz_Y"]),
        ("adaptor witness", "$r'$", d["sz_ywit"]),
        ("signature", "$(c, z)$", d["sz_sig"]),
        ("pre-signature", "$(c, \\hat{z})$", d["sz_presig"]),
        ("adapted signature", "$(c, z)$", d["sz_adapted"]),
    ]
    out = [
        "%% communication_components_paper.tex -- auto-generated; Table 2 (%s setting)."
        % PAPER_DISPLAY.get(hl, hl),
        "\\begin{tabular}{l l r}",
        "\\hline",
        "Object & Notation & Size (bytes) \\\\",
        "\\hline",
    ]
    for obj, nota, sz in rows:
        out.append("%s & %s & %d \\\\" % (obj, nota, sz))
    out += [
        "\\hline",
        "\\multicolumn{3}{l}{\\footnotesize basic signature, pre-signature and adapted signature have the same size;} \\\\",
        "\\multicolumn{3}{l}{\\footnotesize LAS adds the statement $Y$, it does not enlarge the final signature.} \\\\",
        "\\hline",
        "\\end{tabular}",
    ]
    (out_dir / "communication_components_paper.tex").write_text("\n".join(out) + "\n")


def make_adaptor_overhead_paper(data, out_dir):
    """Figure 3 (paper): adaptor overhead (percent) versus the matching basic
    operation, across the parameter settings. Values are parsed, never hardcoded."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    levels = _levels_in_order(data)
    xs = list(range(len(levels)))
    fig, ax = plt.subplots(figsize=(8.8, 5.2))
    ov_series = [("PreSign versus Sign", "o_presign", COL["las_presign"]),
                 ("PreVerify versus Verify", "o_preverify", COL["las_preverify"]),
                 ("Adapt versus Verify", "o_adapt", COL["adapt"])]
    w = 0.26
    allvals = []
    for i, (lbl, key, c) in enumerate(ov_series):
        vals = [data[lvl][key][2] for lvl in levels]
        allvals += vals
        bars = ax.bar([x + (i - 1) * w for x in xs], vals, w, label=lbl,
                      color=c, edgecolor="black", linewidth=0.3)
        for b, v in zip(bars, vals):
            ax.text(b.get_x() + b.get_width() / 2, v + (0.15 if v >= 0 else -0.35),
                    "%+.1f" % v, ha="center", fontsize=7)
    ax.axhline(0, color="black", linewidth=0.6)
    ax.set_ylim(top=max(allvals) * 1.28)
    ax.set_xticks(xs)
    ax.set_xticklabels([PAPER_DISPLAY.get(lvl, lvl) for lvl in levels],
                       fontsize=7.5)
    ax.set_xlabel("parameter setting (scaling context)")
    ax.set_ylabel("overhead versus basic operation (percent)", fontsize=9)
    ax.legend(fontsize=8, loc="upper right")
    ax.grid(axis="y", alpha=0.3)
    _save(fig, out_dir, "adaptor_overhead_paper")


def make_rejection_sampling_paper(data, out_dir):
    """Appendix (paper, optional): rejection-sampling acceptance per attempt for the
    basic Sign and the LAS PreSign across settings. Explains timing variance and
    shows the adaptor does not change acceptance. Not part of the main claim."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    levels = _levels_in_order(data)
    xs = list(range(len(levels)))
    eul = 100.0 / 2.718281828459045
    base_acc = [data[lvl]["rej_base"]["accept_pct"] for lvl in levels]
    las_acc = [data[lvl]["rej_las"]["accept_pct"] for lvl in levels]
    w = 0.36
    fig, ax = plt.subplots(figsize=(8.4, 4.6))
    ax.bar([x - w / 2 for x in xs], base_acc, w, label="basic Sign",
           color=COL["base_sign"], edgecolor="black", linewidth=0.3)
    ax.bar([x + w / 2 for x in xs], las_acc, w, label="LAS PreSign",
           color=COL["las_presign"], edgecolor="black", linewidth=0.3)
    ax.axhline(eul, color="#d94701", linestyle="--", linewidth=1.2,
               label="1/e = %.1f percent" % eul)
    ax.set_xticks(xs)
    ax.set_xticklabels([PAPER_DISPLAY.get(lvl, lvl) for lvl in levels],
                       fontsize=7.5)
    ax.set_xlabel("parameter setting (scaling context)")
    ax.set_ylabel("acceptance per attempt (percent)")
    ax.set_ylim(0, max(base_acc + las_acc) * 1.35)
    ax.legend(fontsize=8)
    ax.grid(axis="y", alpha=0.3)
    _save(fig, out_dir, "rejection_sampling_paper")


def write_key_findings_paper(data, out_dir):
    """Paper-facing key findings (2-3 concise points) answering: vs basic
    Sign/Verify, what extra computation and communication does LAS add?"""
    hl = _headline(data)
    d = data[hl]
    disp = PAPER_DISPLAY.get(hl, hl)
    pp = d["o_presign"][2]
    pvp = d["o_preverify"][2]
    adp = d["o_adapt"][2]
    ad = d["o_adapt"][0]
    ext = d["t_ext"][0]
    lines = [
        "# Key findings (paper-facing; headline = %s setting)" % disp,
        "",
        "Main comparison: the basic simplified Dilithium-style signature versus "
        "the LAS adaptor signature. The LAS-2020/845 reference and the Simplified "
        "Dilithium-II/III/V sets are engineering parameter settings for scaling "
        "context, not formal NIST-equivalent security levels.",
        "",
        "1. **Extra computation is small.** On top of the basic Sign and Verify, "
        "LAS adds four operations. At the %s setting PreSign costs %+.1f%% versus "
        "Sign, PreVerify %+.1f%% versus Verify, and Adapt %+.1f%% versus Verify "
        "(Adapt %.0f microseconds, Ext %.0f microseconds); Ext extracts the witness "
        "s = z - ẑ. Pre-signing and pre-verification mirror basic signing and "
        "verification, so the adaptor adds roughly one extra signing pass plus a few "
        "verification-scale operations." % (disp, pp, pvp, adp, ad, ext),
        "",
        "2. **The final signature does not grow.** The basic signature, the "
        "pre-signature and the adapted signature are byte-identical (%d bytes), "
        "because Adapt computes z = ẑ + r' (it changes the response value, not "
        "the serialized structure)." % d["sz_sig"],
        "",
        "3. **LAS adds one public communication object: the statement.** Beyond the "
        "basic signature, LAS publishes the statement Y (%d bytes, the same size "
        "as the public key) that locks the signature, plus the adaptor witness "
        "r' (%d bytes) held privately by the signer. The extra "
        "communication is the statement Y, not a larger signature."
        % (d["sz_Y"], d["sz_ywit"]),
    ]
    (out_dir / "KEY_FINDINGS_paper.md").write_text("\n".join(lines) + "\n")


def write_paper_manifest(out_dir, have_app, have_mh):
    """paper_figure_manifest.csv: the small final package is main; everything else is
    explicitly excluded from main."""
    H = ["artefact", "role", "claim", "note"]
    rows = [
        ["parameter_sets_paper.tex", "Table 1 (main)",
         "Parameter settings: n, ell, M=n+ell, kappa, gamma, N, Q",
         "engineering settings for scaling context, not NIST-equivalent security levels"],
        ["per_operation_timing_paper.png/.pdf", "Figure 1 (main)",
         "Per-operation computation: basic signature (KeyGen/Sign/Verify) vs LAS "
         "adaptor (PreSign/PreVerify/Adapt/Ext) at the headline setting; mean +/- SD",
         "operations reported independently, not cumulatively"],
        ["per_operation_timing_paper.tex", "Figure 1 companion table (main)",
         "Per-operation mean +/- SD (microseconds) across all settings", ""],
        ["communication_components_paper.png/.pdf", "Figure 2 (main)",
         "Serialized sizes: pk, sk, c, z/z_hat, statement Y, witness, signature, "
         "pre-signature, adapted signature; the three signatures are equal size",
         "LAS adds the statement Y; it does not enlarge the final signature"],
        ["communication_components_paper.tex", "Table 2 (main)",
         "Same component sizes as Figure 2, as a table", ""],
        ["adaptor_overhead_paper.png/.pdf", "Figure 3 (main)",
         "Adaptor overhead (percent) vs the matching basic operation across "
         "settings (PreSign vs Sign, PreVerify vs Verify, Adapt vs Verify)",
         "x-axis is parameter setting / scaling context, not a security-level comparison"],
        ["KEY_FINDINGS_paper.md", "main (text)",
         "Extra computation; signature size unchanged; statement Y added", ""],
        ["rejection_sampling_paper.png/.pdf", "appendix (optional, supporting)",
         "Rejection-sampling acceptance per attempt (~1/e) for basic Sign and LAS "
         "PreSign; explains timing variance and that the adaptor does not change it",
         "supporting only, not part of the main claim"],
    ]
    excluded = [
        ("per_operation_timing_report.png/.pdf", "duplicate report variant"),
        ("communication_components_clean_report.png/.pdf", "duplicate report variant"),
        ("adaptor_overhead_vs_level_report.png/.pdf", "duplicate report variant"),
        ("parameter_sets_report.png/.pdf", "parameter PNG (use Table 1 instead)"),
        ("parameter_sets.png/.pdf", "parameter PNG (use Table 1 instead)"),
        ("per_operation_timing.png/.pdf", "duplicate variant"),
        ("communication_components_clean.png/.pdf", "duplicate variant"),
        ("adaptor_overhead_vs_level.png/.pdf", "duplicate variant"),
        ("timing_timeline_base_vs_las.png/.pdf", "cumulative timeline (excluded)"),
        ("protocol_step_timeline.png/.pdf", "cumulative protocol-step plot (excluded)"),
        ("timing_overhead_clean.png/.pdf", "superseded by adaptor_overhead_paper"),
        ("computation_component_absolute.png/.pdf", "component micro-attribution (evidence/debug)"),
        ("component_scaling_vs_level.png/.pdf", "component micro-attribution (evidence/debug)"),
        ("verify_ext_attribution_vs_level.png/.pdf", "component micro-attribution (evidence/debug)"),
        ("communication_summary_clean.png/.pdf", "mixes stage-1 sizes with stage-2 payloads (excluded)"),
        ("acceptance_vs_level.png/.pdf", "superseded by rejection_sampling_paper"),
    ]
    if have_app:
        excluded.append(("application_atomic_swap_payload_breakdown.png/.pdf",
                         "atomic-swap figure (stage-2, excluded from main)"))
    if have_mh:
        excluded += [
            ("application_multihop_payload_vs_k.png/.pdf", "AMHL/multi-hop figure (stage-2, excluded)"),
            ("application_multihop_presign_time_vs_k.png/.pdf", "AMHL/multi-hop figure (stage-2, excluded)"),
            ("application_multihop_norm_vs_k.png/.pdf", "AMHL/multi-hop figure (stage-2, excluded)"),
        ]
    for fn, why in excluded:
        rows.append([fn, "excluded from main", "", why])
    _writer(out_dir, "paper_figure_manifest.csv", H, rows)


# ---------------------------------------------------------------------------
# fair-benchmark figures (clean, report-quality set)
# ---------------------------------------------------------------------------

def make_fair_plots(data, out_dir, machine=""):
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
               ("Adapt", "t_adapt", COL["adapt"]), ("Ext", "t_ext", COL["ext"])]
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
                   "Ext\n(LAS only)"]
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
    axL.set_title("sign-side ops")
    axL.grid(axis="y", alpha=0.3)

    # right panel: fast verify-side ops, on a much smaller y-scale
    t1 = _ovpair(axR, 0, dh["t_verify"], dh["t_preverify"], COL["base_verify"], COL["las_preverify"])
    t2 = _ovpair(axR, 1, dh["t_verify"], dh["t_adapt"], COL["base_verify"], COL["adapt"])
    axR.bar(2, dh["t_ext"][0], 0.38, yerr=dh["t_ext"][1], capsize=3,
            color=COL["ext"], edgecolor="black", linewidth=0.3)
    tR = max(t1, t2, dh["t_ext"][0] + dh["t_ext"][1])
    axR.text(0, t1 * 1.05, "%+.1f%%" % dh["o_preverify"][2], ha="center", fontsize=9, fontweight="bold")
    axR.text(1, t2 * 1.05, "%+.1f%%" % dh["o_adapt"][2], ha="center", fontsize=9, fontweight="bold")
    axR.set_xticks([0, 1, 2])
    axR.set_xticklabels(["Verify\nvs PreVerify", "Verify\nvs Adapt", "Ext"])
    axR.set_ylim(0, tR * 1.18)
    axR.set_title("verify-side ops")
    axR.grid(axis="y", alpha=0.3)

    fig.suptitle("Adaptor overhead vs the mirrored base op  (%s; in each pair left=base, "
                 "right=adaptor; %% above bars)" % hl, fontsize=11)
    fig.tight_layout(rect=[0, 0, 1, 0.94])
    _save(fig, out_dir, "timing_overhead_clean")

    # 3. computation_component_absolute (headline; horizontal, sorted) -------------
    comps = [("A-product / commitment w=A·y", dh["c_aprod"], COL["w"]),
             ("challenge hash", dh["c_hash"], COL["hash"]),
             ("c·r (all LAS_M response polys)", dh["c_cr_all"], COL["cr_all"]),
             ("norm check", dh["c_norm"], COL["norm"]),
             ("w + t'", dh["c_wY"], COL["Y"]),
             ("ẑ + r'", dh["c_zwit"], COL["witness_add"])]
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
    ax.set_title("Diagnostic component attribution, NOT full-protocol percentage  (%s)"
                 % LEVEL_DISPLAY.get(hl, hl))
    ax.grid(axis="x", alpha=0.3)
    fig.text(0.5, -0.02, "Caution: diagnostic microbenchmark attribution (timing local "
             "copies of inner steps), NOT a full protocol runtime decomposition.",
             ha="center", va="top", fontsize=8, style="italic", color="#444444")
    _save(fig, out_dir, "computation_component_absolute")

    # 4. communication_summary_clean (headline; horizontal; byte labels; legend below)
    fig, ax = plt.subplots(figsize=(9.0, 4.0))
    items = [
        ("signature\n(c + z)", [("c", COL["c"], dh["sz_c"]), ("z", COL["z"], dh["sz_z"])], dh["sz_sig"]),
        ("pre-signature\n(c + ẑ)", [("c", COL["c"], dh["sz_c"]), ("ẑ", COL["zhat"], dh["sz_zhat"])], dh["sz_presig"]),
        ("off-chain\n(Y + 2·pre-sig)", [("payload", COL["las_presign"], dh["pl_offchain"])], dh["pl_offchain"]),
        ("settlement\n(2·signature)", [("payload", COL["sig"], dh["pl_settlement"])], dh["pl_settlement"]),
    ]
    LEG = {"c", "z", "ẑ"}                           # only the c/z segments carry a legend entry
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
    ax.set_title("Communication summary (%s) — response z / ẑ is %.1f%% of the signature"
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
    ax.set_xticks(xs)
    ax.set_xticklabels([_level_tick(data[lvl], lvl) for lvl in levels], fontsize=7.5)
    ax.set_ylabel("overhead vs the mirrored basic op (%)")
    ax.set_title("Adaptor overhead across parameter sets")
    ax.legend(fontsize=8); ax.grid(axis="y", alpha=0.3)
    _add_footer(fig, machine)
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
    ax.set_xticks(xs)
    ax.set_xticklabels([_level_tick(data[lvl], lvl) for lvl in levels], fontsize=7.5)
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
           "t_verify", "Verify = A*z - c*t + hash (+ norm)")
    _stack(axE, [("s = z - ẑ", "c_ext_sub", COL["witness_add"]),
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
    ax.set_title("Atomic-swap payload breakdown  (Simplified Dilithium-III; bench_app3)")
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
        ax.set_title("Multi-hop AMHL settlement payload vs K  (Simplified Dilithium-III)")
        ax.legend(fontsize=8)
        ax.grid(alpha=0.3)
        _save(fig, out_dir, "application_multihop_payload_vs_k")
        made.append("application_multihop_payload_vs_k")

        fig, ax = plt.subplots(figsize=(7.5, 4.2))
        ax.plot(Ks, [r["presig_time_ms"] for r in mh], "o-", color=COL["las_presign"])
        ax.set_xlabel("path length K (hops)")
        ax.set_ylabel("pre-sign time per route (ms)")
        ax.set_title("Multi-hop AMHL pre-sign time vs K  (Simplified Dilithium-III)")
        ax.grid(alpha=0.3)
        _save(fig, out_dir, "application_multihop_presign_time_vs_k")
        made.append("application_multihop_presign_time_vs_k")

        # AMHL norm-margin: the achieved cumulative witness norm vs the PreSign
        # bound g-k-K, BOTH on one log axis so the (huge) gap is visible -- the
        # bound is extremely loose (the witness ||s_j||inf is tiny: <= K).
        fig, ax = plt.subplots(figsize=(7.5, 4.2))
        ax.plot(Ks, [r["max_norm"] for r in mh], "o-", color=COL["witness"],
                label="achieved max ||s_j||inf  (cumulative witness)")
        ax.plot(Ks, [r["bound_gkK"] for r in mh], "s--", color=COL["las_preverify"],
                label="PreSign bound g-k-K")
        ax.set_yscale("log")
        ax.set_xlabel("path length K (hops)")
        ax.set_ylabel("infinity norm  (log scale)")
        ax.set_title("AMHL witness norm vs PreSign bound g-k-K  (Simplified Dilithium-III): the bound is "
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


def write_param_sets_tex(data, out_dir):
    """LaTeX-ready parameter table (\\input into the report; standard tabular,
    no extra packages). Meanings are LaTeX-safe by construction (no _ ^ ~ & %)."""
    levels = _levels_in_order(data)
    out = [
        "% parameter_sets.tex -- auto-generated by plot_las_benchmarks.py; do not hand-edit.",
        "\\begin{tabular}{l p{5.2cm} r r r r r r r}",
        "\\hline",
        "Setting & Meaning & $n$ & $\\ell$ & $M$ & $\\kappa$ & $\\gamma$ & $N$ & $Q$ \\\\",
        "\\hline",
    ]
    for lvl in levels:
        d = data[lvl]
        out.append("%s & %s & %d & %d & %d & %d & %d & %d & %d \\\\" % (
            LEVEL_DISPLAY.get(lvl, lvl), LEVEL_DESC.get(lvl, ""), d["n"], d["ell"],
            d["M"], d["kappa"], d["gamma"], d["N"], d["Q"]))
    out += ["\\hline", "\\end{tabular}"]
    (out_dir / "parameter_sets.tex").write_text("\n".join(out) + "\n")


def write_fair_csvs(data, out_dir):
    levels = _levels_in_order(data)

    ops = [("Setup", "t_setup"), ("KeyGen", "t_keygen"), ("Sign", "t_sign"),
           ("Verify", "t_verify"), ("PreSign", "t_presign"), ("PreVerify", "t_preverify"),
           ("Adapt", "t_adapt"), ("Ext", "t_ext")]
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
        rows.append([lvl, "Ext (separate)", d["t_ext"][0], "", ""])
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
             ("r'", "sz_ywit"), ("c", "sz_c"), ("z", "sz_z"), ("z_hat", "sz_zhat"),
             ("signature (c,z)", "sz_sig"), ("pre-signature (c,z_hat)", "sz_presig"),
             ("final adapted sig (c,z)", "sz_adapted")]
    rows = [[lvl, lab, data[lvl][k], round(100.0 * data[lvl][k] / data[lvl]["sz_sig"], 2)]
            for lvl in levels for lab, k in comps]
    _writer(out_dir, "communication_components.csv",
            ["level", "component", "bytes", "pct_of_signature"], rows)

    ccomps = [("A-product / commitment w", "c_aprod"), ("challenge hash", "c_hash"),
              ("c*r one response poly", "c_cr_one"), ("c*r all LAS_M polys", "c_cr_all"),
              ("norm check", "c_norm"), ("w + Y", "c_wY"), ("z_hat + r'", "c_zwit"),
              ("c*t all LAS_N pk polys", "c_ct"), ("KeyGen sample r", "c_keygen_r"),
              ("Ext s = z - z_hat", "c_ext_sub"), ("Ext A*s", "c_ext_amul"),
              ("Ext t'==A*s check", "c_ext_check")]
    rows = [[lvl, lab, data[lvl][k][0], data[lvl][k][1]] for lvl in levels for lab, k in ccomps]
    _writer(out_dir, "computation_components.csv",
            ["level", "component", "mean_us", "sd_us"], rows)

    cat = [("public key", "pk = t", "sz_pk"), ("secret key", "sk = r", "sz_sk"),
           ("statement", "Y = t'", "sz_Y"), ("witness", "r'", "sz_ywit"),
           ("challenge", "c", "sz_c"), ("final response", "z", "sz_z"),
           ("pre-sig response", "z_hat", "sz_zhat"), ("signature", "(c, z)", "sz_sig"),
           ("pre-signature", "(c, z_hat)", "sz_presig"),
           ("final adapted signature", "(c, z)", "sz_adapted")]
    rows = [[lvl, obj, notation, data[lvl][k]] for lvl in levels for obj, notation, k in cat]
    _writer(out_dir, "las_object_catalogue.csv",
            ["level", "object", "paper_notation", "bytes"], rows)

    # parameter-set table backing the parameter_sets figure (Meeting-4: make the
    # meaning of paper/L2/L3/L5 explicit and machine-readable) + a LaTeX version.
    rows = [[lvl, LEVEL_DISPLAY.get(lvl, lvl), LEVEL_DESC.get(lvl, ""),
             data[lvl]["n"], data[lvl]["ell"], data[lvl]["M"], data[lvl]["kappa"],
             data[lvl]["gamma"], data[lvl]["N"], data[lvl]["Q"]] for lvl in levels]
    _writer(out_dir, "parameter_sets.csv",
            ["level", "display", "meaning", "n", "ell", "M", "kappa", "gamma", "N", "Q"], rows)
    write_param_sets_tex(data, out_dir)


def write_app_csvs(a, out_dir):
    note = "Simplified Dilithium-III (bench_app3: n=%d ell=%d kappa=%d)" % (a["n"], a["ell"], a["kappa"])
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
# key-findings summary (Meeting-4: 2-3 sentences, don't make the reader infer)
# ---------------------------------------------------------------------------

def write_key_findings(data, out_dir, machine):
    hl = _headline(data)
    d = data[hl]
    disp = LEVEL_DISPLAY.get(hl, hl)
    zpct = 100.0 * d["sz_z"] / d["sz_sig"]
    ps, sg, pp = d["o_presign"]       # PreSign, Sign, percent
    pv, vf, pvp = d["o_preverify"]    # PreVerify, Verify, percent
    ad, av, adp = d["o_adapt"]        # Adapt, Verify, percent
    ext = d["t_ext"][0]
    lines = [
        "# Key findings (auto-generated; headline = %s; machine: %s)"
        % (disp, machine or "n/a"),
        "",
        "**Question answered: compared with the basic simplified Dilithium-style "
        "signature, how much extra computation and communication does the LAS "
        "exotic adaptor signature add?**",
        "",
        "1. **Extra computation.** The basic signature uses Sign and Verify; the "
        "LAS adaptor adds four operations. At the %s setting: PreSign takes %.0f "
        "microseconds versus Sign at %.0f (%+.1f%%); PreVerify takes %.0f versus "
        "Verify at %.0f (%+.1f%%); Adapt takes %.0f microseconds (%+.1f%% versus "
        "Verify); and Ext takes %.0f microseconds. Pre-signing and pre-verification "
        "stay close to basic signing and verification, so the adaptor machinery "
        "costs roughly one extra signing pass plus a few verification-scale operations."
        % (disp, ps, sg, pp, pv, vf, pvp, ad, adp, ext),
        "",
        "2. **Extra communication: the signature does not grow.** The basic "
        "signature, the pre-signature and the adapted signature are all byte-"
        "identical (%d bytes), because Adapt computes z = ẑ + r' (it changes the "
        "response value, not the serialized structure). Inside the signature the "
        "response z is %.1f%% of the bytes and the challenge c is only %d bytes."
        % (d["sz_sig"], zpct, d["sz_c"]),
        "",
        "3. **Extra communication: LAS adds one public object, the statement.** "
        "Beyond the basic signature, LAS publishes the statement Y (%d bytes, the "
        "same size as the public key) that locks the signature, plus the adaptor "
        "witness r' (%d bytes) held privately by the signer. So moving from the "
        "basic signature to the LAS adaptor signature costs essentially one extra "
        "public-key-sized object on the wire, not a larger signature."
        % (d["sz_Y"], d["sz_ywit"]),
        "",
        "_The LAS-2020/845 reference and the Simplified Dilithium-II/III/V sets are "
        "engineering parameter settings used for scaling context, not formal NIST "
        "security levels. "
        "Sources: per-operation timings -> primary_timing.csv / "
        "per_operation_timing_report.*; component sizes -> "
        "communication_components.csv / communication_components_clean_report.*; "
        "parameters -> parameter_sets.csv / parameter_sets_report.*._",
    ]
    (out_dir / "KEY_FINDINGS.md").write_text("\n".join(lines) + "\n")


# ---------------------------------------------------------------------------
# report figure manifest
# ---------------------------------------------------------------------------

def write_manifest(out_dir, have_app, have_mh):
    H = ["figure_file", "use_in_report_main_or_appendix", "claim_supported", "caution_note"]
    rows = [
        # ---- MAIN SET (report-clean _report figures; captions live in LaTeX) ----
        # Main story = the basic simplified Dilithium-style signature vs LAS (exotic) PQ adaptor
        # signature. paper / L2-like / L3-like / L5-like are scaling context only.
        ["per_operation_timing_report.png/.pdf", "main",
         "PRIMARY computation result: basic signature operations (KeyGen, Sign, "
         "Verify) versus LAS adaptor operations (PreSign, PreVerify, Adapt, Ext), per "
         "operation; parameter settings shown only as scaling context",
         "report-clean; error bars = sample standard deviation; story is basic vs LAS, not paper vs L2/L3/L5"],
        ["communication_components_clean_report.png/.pdf", "main",
         "PRIMARY communication result: byte size of every object (public key, secret "
         "key, statement, adaptor witness, challenge, response, signature, "
         "pre-signature, adapted signature); the signature does not grow under "
         "adaptation, LAS adds the statement",
         "report-clean; serialized size in bytes, not on-chain gas; headline setting"],
        ["adaptor_overhead_vs_level_report.png/.pdf", "main",
         "Overhead / scaling support: percent overhead of each LAS adaptor operation "
         "versus the matching basic operation, across the paper, L2-like, L3-like "
         "and L5-like parameter settings",
         "supporting; shows the basic-vs-LAS overhead stays small as parameters scale"],
        ["parameter_sets_report.png/.pdf", "main",
         "Setup / context (NOT a result): the engineering parameter settings paper, "
         "L2-like, L3-like, L5-like used for scaling; defines module ranks n and ell, "
         "response dimension M=n+ell, challenge weight kappa, masking bound gamma, "
         "ring degree N, modulus Q",
         "context only; engineering scaling settings, not formal NIST security levels"],
        # ---- SUPPORTING: standalone variants (title + machine footer) of the 4 mains ----
        ["parameter_sets.png/.pdf", "appendix",
         "Parameter-set table with explanatory footnote + machine footer",
         "standalone variant of parameter_sets_report"],
        ["per_operation_timing.png/.pdf", "appendix",
         "Per-operation timing with title + machine footer",
         "standalone variant of per_operation_timing_report"],
        ["adaptor_overhead_vs_level.png/.pdf", "appendix",
         "Adaptor overhead across paper/L2/L3/L5 with title + machine footer",
         "standalone variant of adaptor_overhead_vs_level_report"],
        ["communication_components_clean.png/.pdf", "appendix",
         "Component sizes with callout note + machine footer",
         "standalone variant of communication_components_clean_report"],
        # ---- APPENDIX / supporting figures ----
        ["timing_timeline_base_vs_las.png/.pdf", "appendix",
         "End-to-end CUMULATIVE cost of the base vs LAS path (context only)",
         "cumulative supporting view; per_operation_timing is the headline (Meeting-4)"],
        ["protocol_step_timeline.png/.pdf", "appendix",
         "CUMULATIVE cost per named protocol step; full LAS cycle ~30% over base",
         "cumulative supporting view; demoted in favour of per_operation_timing"],
        ["timing_overhead_clean.png/.pdf", "appendix",
         "Adaptor overhead per mirrored pair: PreSign~Sign, PreVerify~Verify, Adapt~Verify",
         "headline-level pairs; per-level numbers in adaptor_overhead.csv; Ext has no mirrored basic op"],
        ["computation_component_absolute.png/.pdf", "appendix",
         "Where LAS compute time goes (A-product/hash/c*r dominate)",
         "DIAGNOSTIC component attribution from local copies, NOT a full-protocol %"],
        ["communication_summary_clean.png/.pdf", "appendix",
         "Signature/pre-sig/off-chain/settlement byte totals; response z dominates",
         "byte-level payload only, NOT EVM gas; superseded by communication_components_clean"],
        ["acceptance_vs_level.png/.pdf", "appendix",
         "Rejection-sampling acceptance ~37% (~ e^-1) at every parameter set",
         "cross-level; from rejection_sampling.csv"],
        ["component_scaling_vs_level.png/.pdf", "appendix",
         "c*r one-poly flat ~14us; aggregate c*r/c*t/A-product grow with M=n+ell",
         "cross-level; DIAGNOSTIC component estimates, not protocol %"],
        ["verify_ext_attribution_vs_level.png/.pdf", "appendix",
         "Measured attribution across levels: Verify=A*z-c*t+hash; Ext~one A*s",
         "cross-level; stacked components vs the protocol op (black tick); estimates"],
        # ---- table-only artefacts ----
        ["parameter_sets.csv", "table only",
         "machine-readable parameter sets backing the parameter_sets figure", ""],
        ["parameter_sets.tex", "table only",
         "LaTeX tabular of the parameter sets (\\input into the report)", ""],
        ["KEY_FINDINGS.md", "table only",
         "2-3 sentence summary of the headline findings (auto-generated)", ""],
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
         "object sizes with paper notation (pk=t, sk=r, Y=t', r', ...)",
         "no figure generated by default (catalogue bar chart was too crowded)"],
    ]
    if have_app:
        rows += [
            ["application_atomic_swap_payload_breakdown.png/.pdf", "appendix",
             "Atomic-swap payload story: off-chain (Y + 2 pre-sigs) vs settlement (2 sigs)",
             "Stage-2 (deferred per Meeting-4); L3-like (bench_app3); simulated-ledger proxy, NOT gas"],
            ["application_atomic_swap.csv", "table only",
             "atomic-swap payload / timing / attempts / correctness", "L3-like"],
            ["application_payload_breakdown.csv", "table only",
             "atomic-swap payload composition (off-chain, settlement, settlement incl. Y)", "L3-like"],
            ["application_multihop_amhl.csv", "table only",
             "multi-hop AMHL K-series: bound, #presig, attempts, time, payload, witness norm", "L3-like"],
        ]
    if have_mh:
        rows += [
            ["application_multihop_payload_vs_k.png/.pdf", "appendix",
             "Multi-hop settlement payload grows linearly in path length K",
             "Stage-2 (deferred per Meeting-4); L3-like; simulated-ledger byte proxy"],
            ["application_multihop_presign_time_vs_k.png/.pdf", "appendix",
             "Multi-hop pre-sign time per route vs K (roughly flat per hop)",
             "L3-like; mean over the bench_app3 routes/K"],
            ["application_multihop_norm_vs_k.png/.pdf", "appendix",
             "AMHL achieved witness norm ||s_j||inf vs the PreSign bound g-k-K: "
             "the bound is extremely loose (witness <= K, bound ~ gamma)",
             "Stage-2 (deferred per Meeting-4); L3-like; log y-axis; norm grows <=K, bound ~flat"],
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
    machine = parse_metadata(in_dir / "metadata.txt")

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
    write_key_findings(data, out_dir, machine)
    # ---- paper-facing package (Table 1, companion tables, findings) ----
    write_param_sets_paper_tex(data, out_dir)
    write_per_op_paper_tex(data, out_dir)
    write_comm_paper_tex(data, out_dir)
    write_key_findings_paper(data, out_dir)

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
    write_paper_manifest(out_dir, have_app=bool(app), have_mh=have_mh)

    # ---- figures (skip cleanly if matplotlib unavailable) ----
    made_app = []
    try:
        make_param_table(data, out_dir, machine)
        make_per_op_timing(data, out_dir, machine)
        make_comm_components(data, out_dir, machine)
        make_fair_plots(data, out_dir, machine)
        # report-clean variants (no title/footer/caption; for LaTeX inclusion)
        make_param_table_report(data, out_dir)
        make_per_op_timing_report(data, out_dir)
        make_adaptor_overhead_report(data, out_dir)
        make_comm_components_report(data, out_dir)
        # paper-facing main package (Figure 1, Figure 2, Figure 3, optional appendix)
        make_per_op_timing_paper(data, out_dir)
        make_comm_components_paper(data, out_dir)
        make_adaptor_overhead_paper(data, out_dir)
        make_rejection_sampling_paper(data, out_dir)
        if app:
            made_app = make_app_plots(app, out_dir)
    except ImportError as e:
        print("WARNING: matplotlib unavailable (%s); CSVs + manifest were written, "
              "figures skipped. Install with: pip install matplotlib" % e, file=sys.stderr)

    print("Run folder    : %s" % out_dir)
    print("Machine       : %s" % (machine or "unknown (no metadata.txt)"))
    print("Fair levels   : %s" % ", ".join(_levels_in_order(data)))
    print("Application   : %s" % ("parsed (L3-like)" if app else "absent/skipped"))
    print("Paper package : Table 1 parameter_sets_paper.tex; "
          "Figure 1 per_operation_timing_paper (+.tex); "
          "Figure 2/Table 2 communication_components_paper (+.tex); "
          "Figure 3 adaptor_overhead_paper; appendix rejection_sampling_paper")
    print("Paper manifest: paper_figure_manifest.csv  |  findings: KEY_FINDINGS_paper.md")
    print("Wrote CSVs + KEY_FINDINGS*.md + *figure_manifest.csv and figures (PNG+PDF).")


if __name__ == "__main__":
    main()
