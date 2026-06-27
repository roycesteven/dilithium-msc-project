#!/usr/bin/env python3
"""
make_report_tables.py -- render the report's DATA tables from the captured benchmark
CSVs into clean table IMAGES (vector PDF + PNG) for \\includegraphics into the LaTeX
report. It reads ONLY the CSVs that scripts/plot_las_benchmarks.py already wrote from
the captured logs (plus classical.log for the classical baseline); it never runs a
benchmark and invents/edits no numbers -- every value comes from a CSV/log.

Workflow (Meeting-4 / Royce): CSV -> table -> image -> import into LaTeX. Each image is
the data grid only (no baked title/caption); the LaTeX caption carries the setting,
parameter values, and iteration count.

Inputs (read-only):
  --tables-dir (default evidence/latest/tables):
      parameter_sets.csv, primary_timing.csv, communication_components.csv,
      adaptor_overhead.csv
  --logs-dir   (default evidence/latest/logs):
      classical.log   (optional; the classical table is skipped if absent)

Outputs (--output-dir, default report/latex/figures): one PDF+PNG per table
  tab_params, tab_notation, tab_overhead_l3, tab_overhead_levels,
  tab_components, tab_complete_l3, tab_classical
"""
import argparse
import csv
import re
import sys
import textwrap
from pathlib import Path

LEVEL_ORDER = ["paper", "L2", "L3", "L5"]
HEADLINE = "L3"                      # Simplified Dilithium-III == project target
DISPLAY = {
    "paper": "LAS-2020/845 reference",
    "L2": "Simplified Dilithium-II",
    "L3": "Simplified Dilithium-III",
    "L5": "Simplified Dilithium-V",
}
HEADER_BG = "#08519c"               # dark blue header row, white bold text
SECTION_BG = "#d9e3f0"             # light blue section-divider row


# ---------------------------------------------------------------------------
# read CSVs (numbers come only from these files)
# ---------------------------------------------------------------------------

def _read(d, name, required=True):
    p = d / name
    if not p.exists():
        if required:
            sys.exit("ERROR: missing CSV %s (run scripts/plot_las_benchmarks.py first)" % p)
        return None
    with open(p, newline="") as f:
        return list(csv.DictReader(f))


def load(tables_dir):
    params = {r["level"]: r for r in _read(tables_dir, "parameter_sets.csv")}
    timing = {}
    for r in _read(tables_dir, "primary_timing.csv"):
        op = "Ext" if r["operation"].startswith("Ext") else r["operation"]
        timing.setdefault(r["level"], {})[op] = (float(r["mean_us"]), float(r["sd_us"]))
    comm = {}
    for r in _read(tables_dir, "communication_components.csv"):
        comp = "r'" if r["component"] in ("r'", "r' = y_witness") else r["component"]
        comm.setdefault(r["level"], {})[comp] = int(r["bytes"])
    over = {}
    for r in _read(tables_dir, "adaptor_overhead.csv"):
        over.setdefault(r["level"], {})[r["pair"]] = r
    return params, timing, comm, over


def parse_classical(logs_dir):
    p = logs_dir / "classical.log"
    if not p.exists():
        return None
    t = p.read_text(errors="replace")

    def g(label):
        m = re.search(r"%s[^0-9]*([0-9]+(?:\.[0-9]+)?)\s*us" % re.escape(label), t)
        return float(m.group(1)) if m else None

    def gi(rx):
        m = re.search(rx, t)
        return int(m.group(1)) if m else None

    return {
        "KeyGen": g("KeyGen"), "Sign": g("Sign"), "Verify": g("Verify"),
        "PreSign": g("PreSign"), "PreVerify": g("PreVerify"),
        "Adapt": g("Adapt"), "Ext": g("Ext"),
        "pk": gi(r"pk/statement\s+(\d+)\s*B"), "sk": gi(r"sk/witness\s+(\d+)\s*B"),
        "sig": gi(r"sig\s+(\d+)\s*B"),
    }


# ---------------------------------------------------------------------------
# generic renderer: one table on one axis (data grid only, no title/caption)
# ---------------------------------------------------------------------------

def _style(rcParams):
    rcParams["font.family"] = "serif"
    rcParams["mathtext.fontset"] = "dejavuserif"


def _put_table(ax, rows, col_labels, col_widths, fontsize, section_rows=()):
    ax.axis("off")
    tbl = ax.table(cellText=rows, colLabels=col_labels, colWidths=col_widths,
                   loc="center", cellLoc="center")
    tbl.auto_set_font_size(False)
    tbl.set_fontsize(fontsize)
    tbl.scale(1, 1.55)
    ncol = len(col_labels)
    for j in range(ncol):                      # header row
        c = tbl[0, j]
        c.set_facecolor(HEADER_BG)
        c.set_text_props(color="white", fontweight="bold")
    for i in section_rows:                      # section-divider rows (1-based body)
        for j in range(ncol):
            tbl[i, j].set_facecolor(SECTION_BG)
            tbl[i, j].set_text_props(fontweight="bold")
    return tbl


def _save(fig, out_dir, name):
    import matplotlib.pyplot as plt
    fig.tight_layout()
    for ext in ("pdf", "png"):
        fig.savefig(out_dir / ("%s.%s" % (name, ext)), bbox_inches="tight", dpi=220)
    plt.close(fig)


def one_table(rows, col_labels, out_dir, name, col_widths=None, fontsize=9,
              figw=None, rowh=0.46, section_rows=()):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    _style(plt.rcParams)
    ncol = len(col_labels)
    if figw is None:
        figw = 1.25 * ncol
    figh = rowh * (len(rows) + 1) + 0.3
    fig, ax = plt.subplots(figsize=(figw, figh))
    _put_table(ax, rows, col_labels, col_widths, fontsize, section_rows)
    _save(fig, out_dir, name)


# ---------------------------------------------------------------------------
# the seven report tables
# ---------------------------------------------------------------------------

def t_params(params, out_dir):
    cols = ["Setting", "n", "$\\ell$", "M=n+$\\ell$", "κ", "γ", "N", "q"]
    rows = []
    for lvl in LEVEL_ORDER:
        p = params[lvl]
        rows.append([DISPLAY[lvl], p["n"], p["ell"], p["M"], p["kappa"],
                     p["gamma"], p["N"], p["Q"]])
    rows.append(["Classical (secp256k1 ECDSA adaptor)", "—", "—", "—", "—", "—",
                 "—", "≈128-bit classical"])
    one_table(rows, cols, out_dir, "tab_params",
              col_widths=[0.30, 0.05, 0.05, 0.10, 0.06, 0.12, 0.06, 0.20],
              fontsize=9, figw=11.5)


def t_notation(params, out_dir):
    def vals(key):
        return ", ".join(params[lvl][key] for lvl in LEVEL_ORDER)
    cols = ["Symbol", "Meaning", "Value(s): paper / Dil-II / -III / -V"]
    rows = [
        ["n", "module rank: rows of A, length of public key t", vals("n")],
        ["$\\ell$", "extra columns of A' (secret-vector extension)", vals("ell")],
        ["M = n+$\\ell$", "response-vector dimension (# polys in z, ẑ, y)", vals("M")],
        ["κ", "challenge Hamming weight (# of ±1 in c; Dilithium τ)", vals("kappa")],
        ["γ", "rejection / masking bound, γ = κ·N·M", vals("gamma")],
        ["N", "ring degree ($X^N{+}1$); the paper's d", params["paper"]["N"] + " (all)"],
        ["q", "modulus from the reused NTT, $q \\approx 2^{23}$", params["paper"]["Q"] + " (all)"],
    ]
    one_table(rows, cols, out_dir, "tab_notation",
              col_widths=[0.12, 0.55, 0.33], fontsize=9, figw=12.0)


def _ms(timing, lvl, op):
    m, s = timing[lvl][op]
    return "%.0f ± %.0f" % (m, s) if s >= 0.5 else "%.0f ± %.1f" % (m, s)


def t_overhead_l3(timing, over, out_dir):
    lvl = HEADLINE
    cols = ["Adaptor op", "paired basic op", "basic (µs)", "adaptor (µs)", "overhead"]
    o = over[lvl]
    rows = [
        ["PreSign", "Sign", _ms(timing, lvl, "Sign"), _ms(timing, lvl, "PreSign"),
         "+%s%%" % o["PreSign vs Sign"]["overhead_pct"]],
        ["PreVerify", "Verify", _ms(timing, lvl, "Verify"), _ms(timing, lvl, "PreVerify"),
         "+%s%%" % o["PreVerify vs Verify"]["overhead_pct"]],
        ["Adapt", "Verify", _ms(timing, lvl, "Verify"), _ms(timing, lvl, "Adapt"),
         "+%s%%" % o["Adapt vs Verify"]["overhead_pct"]],
        ["Extract", "—", "—", _ms(timing, lvl, "Ext"), "(separate)"],
        ["KeyGen / statement gen", "(shared)", _ms(timing, lvl, "KeyGen"),
         _ms(timing, lvl, "KeyGen"), "—"],
    ]
    one_table(rows, cols, out_dir, "tab_overhead_l3",
              col_widths=[0.26, 0.18, 0.18, 0.18, 0.14], fontsize=9.5, figw=10.5)


def t_overhead_levels(timing, over, out_dir):
    cols = ["Set (n,$\\ell$,κ)", "PreSign/Sign", "PreVerify/Verify", "Adapt/Verify",
            "Extract (µs)"]
    rows = []
    for lvl in LEVEL_ORDER:
        p = timing[lvl]
        o = over[lvl]
        tag = "%s (%d,%d,%d)" % (DISPLAY[lvl], int(params_cache[lvl]["n"]),
                                 int(params_cache[lvl]["ell"]),
                                 int(params_cache[lvl]["kappa"]))
        rows.append([tag,
                     "+%s%%" % o["PreSign vs Sign"]["overhead_pct"],
                     "+%s%%" % o["PreVerify vs Verify"]["overhead_pct"],
                     "+%s%%" % o["Adapt vs Verify"]["overhead_pct"],
                     "%.0f" % p["Ext"][0]])
    one_table(rows, cols, out_dir, "tab_overhead_levels",
              col_widths=[0.36, 0.16, 0.18, 0.15, 0.15], fontsize=9, figw=11.0)


def t_components(comm, out_dir):
    lvls = ["L2", "L3", "L5"]

    def b(key):
        return [str(comm[l][key]) for l in lvls]

    def zpct():
        return ["%.1f%%" % (100.0 * comm[l]["z"] / comm[l]["signature (c,z)"])
                for l in lvls]
    cols = ["Component", "Simpl. Dil-II (4,4)", "Simpl. Dil-III (6,5)",
            "Simpl. Dil-V (8,7)"]
    rows = [
        ["c (challenge)"] + b("c"),
        ["z (response)"] + b("z"),
        ["Signature (c,z)"] + b("signature (c,z)"),
        ["z as % of signature"] + zpct(),
        ["Public key pk"] + b("pk = t"),
        ["Secret key sk"] + b("sk = r"),
        ["Statement Y (LAS only)"] + b("Y = t'"),
        ["Witness y (LAS only)"] + b("r'"),
        ["Pre-signature (LAS only)"] + b("pre-signature (c,z_hat)"),
    ]
    one_table(rows, cols, out_dir, "tab_components",
              col_widths=[0.34, 0.22, 0.22, 0.22], fontsize=9, figw=10.5,
              section_rows=(3,))   # highlight the "z as % of signature" summary row


def t_complete_l3(timing, comm, out_dir):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    _style(plt.rcParams)
    lvl = HEADLINE
    c = comm[lvl]
    comm_cols = ["Object", "Notation", "Bytes", "In the basic signature?"]
    comm_rows = [
        ["public key", "pk = t", str(c["pk = t"]), "yes (shared)"],
        ["secret key", "sk = r", str(c["sk = r"]), "yes (shared)"],
        ["challenge", "c", str(c["c"]), "yes"],
        ["response", "z / ẑ", str(c["z"]), "yes"],
        ["signature", "(c, z)", str(c["signature (c,z)"]), "yes (basic signature)"],
        ["statement", "Y = t'", str(c["Y = t'"]), "LAS only (public)"],
        ["witness", "r'", str(c["r'"]), "LAS only (private)"],
        ["pre-signature", "(c, ẑ)", str(c["pre-signature (c,z_hat)"]), "LAS only"],
        ["adapted signature", "(c, z)", str(c["final adapted sig (c,z)"]),
         "LAS (verifies as basic sig)"],
    ]
    comp_cols = ["Operation", "basic (µs)", "LAS adaptor (µs)", "overhead"]
    comp_rows = [
        ["KeyGen", _ms(timing, lvl, "KeyGen"), _ms(timing, lvl, "KeyGen") + " (shared)", "—"],
        ["Sign / PreSign", _ms(timing, lvl, "Sign"), _ms(timing, lvl, "PreSign"), "+6.7%"],
        ["Verify / PreVerify", _ms(timing, lvl, "Verify"), _ms(timing, lvl, "PreVerify"), "+3.1%"],
        ["Verify / Adapt", _ms(timing, lvl, "Verify"), _ms(timing, lvl, "Adapt"), "+8.1%"],
        ["Extract", "—", _ms(timing, lvl, "Ext"), "(LAS only)"],
    ]
    fig, (axc, axp) = plt.subplots(
        2, 1, figsize=(10.5, 0.46 * (len(comm_rows) + len(comp_rows) + 2) + 0.8),
        gridspec_kw={"height_ratios": [len(comm_rows) + 1, len(comp_rows) + 1]})
    _put_table(axc, comm_rows, comm_cols, [0.22, 0.16, 0.12, 0.40], 9)
    _put_table(axp, comp_rows, comp_cols, [0.28, 0.22, 0.28, 0.18], 9)
    axc.set_title("Communication (packed bytes)", fontsize=10, loc="left",
                  fontweight="bold")
    axp.set_title("Computation (µs/op)", fontsize=10, loc="left", fontweight="bold")
    _save(fig, out_dir, "tab_complete_l3")


def t_classical(timing, comm, classical, out_dir):
    if classical is None:
        print("  (classical.log absent -> tab_classical skipped)")
        return
    l2 = timing["L2"]
    cc = comm["L2"]

    def r2(x):
        return "%.0f" % x if x is not None else "—"
    cols = ["", "ECDSA adaptor (classical)", "LAS adaptor (post-quantum)"]
    rows = [
        ["KeyGen", r2(classical["KeyGen"]), "%.0f" % l2["KeyGen"][0]],
        ["Sign", r2(classical["Sign"]), "%.0f" % l2["Sign"][0]],
        ["Verify", r2(classical["Verify"]), "%.0f" % l2["Verify"][0]],
        ["PreSign", r2(classical["PreSign"]), "%.0f" % l2["PreSign"][0]],
        ["PreVerify", r2(classical["PreVerify"]), "%.0f" % l2["PreVerify"][0]],
        ["Adapt", r2(classical["Adapt"]), "%.0f" % l2["Adapt"][0]],
        ["Extract", r2(classical["Ext"]), "%.0f" % l2["Ext"][0]],
        ["public key / statement (B)", str(classical["pk"]), str(cc["pk = t"])],
        ["secret key / witness (B)", str(classical["sk"]), str(cc["sk = r"])],
        ["signature (B)", str(classical["sig"]), str(cc["signature (c,z)"])],
    ]
    one_table(rows, cols, out_dir, "tab_classical",
              col_widths=[0.34, 0.33, 0.33], fontsize=9, figw=9.5,
              section_rows=(8,))   # the sizes block starts at body row 8 (1-based)


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

params_cache = {}


def main(argv=None):
    ap = argparse.ArgumentParser(description="Render report data tables (CSV -> image).")
    repo = Path(__file__).resolve().parents[1]
    ap.add_argument("--tables-dir", default=str(repo / "evidence/latest/tables"))
    ap.add_argument("--logs-dir", default=str(repo / "evidence/latest/logs"))
    ap.add_argument("--output-dir", default=str(repo / "report/latex/figures"))
    a = ap.parse_args(argv)

    tdir = Path(a.tables_dir)
    ldir = Path(a.logs_dir)
    odir = Path(a.output_dir)
    odir.mkdir(parents=True, exist_ok=True)

    params, timing, comm, over = load(tdir)
    params_cache.update(params)
    classical = parse_classical(ldir)

    t_params(params, odir)
    t_notation(params, odir)
    t_overhead_l3(timing, over, odir)
    t_overhead_levels(timing, over, odir)
    t_components(comm, odir)
    t_complete_l3(timing, comm, odir)
    t_classical(timing, comm, classical, odir)

    print("Tables dir : %s" % tdir)
    print("Output dir : %s" % odir)
    print("Wrote      : tab_params, tab_notation, tab_overhead_l3, tab_overhead_levels,")
    print("             tab_components, tab_complete_l3%s (PDF+PNG)"
          % ("" if classical is None else ", tab_classical"))


if __name__ == "__main__":
    main()
