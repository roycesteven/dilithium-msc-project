#!/usr/bin/env python3
"""check_figure_type.py -- enforce the report's figure type floor.

No figure may carry type smaller than the paragraph around it (Royce, 2026-08-28),
and the report body is 12pt.  This reads the type sizes back out of the figure PDFs
themselves rather than trusting the plotting scripts, because the size on the PAGE
is the source size times the include scale: a figure drawn on a 9in canvas and
included at \\linewidth lands near 7pt however large its rcParams say it is.  The
figures the report includes are therefore drawn at exactly \\textwidth and saved
uncropped, so 1pt in the plot script is 1pt on the page -- and this script checks
both halves of that: the canvas width AND every embedded font size.

    python3 scripts/check_figure_type.py            # all report figures
    python3 scripts/check_figure_type.py a.pdf b.pdf

Exits non-zero if any figure is under the floor, so it can gate a build.

Figures whose text is stored as OUTLINES carry no font-size operators at all and
report "no embedded text" -- fig_criterion_presign.pdf is one (its type is enlarged
by scripts/gen_criterion_figure.py and is deliberately left alone).  Those cannot be
checked here; check them by measuring the rendered page.
"""
import re
import subprocess
import sys
import zlib
from pathlib import Path

FLOOR_PT = 12.0          # the report's body size (muthesis, 12pt option)
TEXT_WIDTH_PT = 411.12   # \textwidth = 145mm, the width the report includes at
WIDTH_TOL_PT = 1.5

DEFAULT = ["fig_timing.pdf", "fig_overhead.pdf", "fig_rejection_cdf.pdf",
           "fig_onchain.pdf"]


def font_sizes(pdf):
    """Every font size set by a Tf operator anywhere in the file."""
    raw = Path(pdf).read_bytes()
    sizes = set()
    for m in re.finditer(rb"stream\r?\n(.*?)endstream", raw, re.S):
        blob = m.group(1)
        try:
            blob = zlib.decompress(blob)
        except Exception:
            pass
        for t in re.finditer(rb"/[A-Za-z0-9]+\s+([0-9.]+)\s+Tf", blob):
            sizes.add(round(float(t.group(1)), 2))
    return sizes


def page_width(pdf):
    out = subprocess.run(["pdfinfo", str(pdf)], capture_output=True, text=True).stdout
    m = re.search(r"Page size:\s+([0-9.]+) x", out)
    return float(m.group(1)) if m else None


def main(argv):
    paths = [Path(a) for a in argv[1:]]
    if not paths:
        figs = Path(__file__).resolve().parents[1] / "report" / "latex" / "figures"
        paths = [figs / n for n in DEFAULT]

    bad = 0
    for p in paths:
        if not p.exists():
            print("MISSING  %s" % p)
            bad += 1
            continue
        sizes = font_sizes(p)
        w = page_width(p)
        notes = []
        if not sizes:
            notes.append("no embedded text (outlined?) -- not checkable here")
        else:
            under = sorted(s for s in sizes if s < FLOOR_PT - 0.01)
            if under:
                notes.append("TYPE UNDER %.0fpt: %s" % (FLOOR_PT, under))
        # The canvas check only means something alongside a font size: for an
        # outlined figure there is no type size to rescale, and its generator may
        # crop to its own width by design (fig_criterion_presign does).
        if sizes and w is not None and abs(w - TEXT_WIDTH_PT) > WIDTH_TOL_PT:
            notes.append("canvas %.1fpt != %.1fpt, so \\includegraphics rescales "
                         "the type" % (w, TEXT_WIDTH_PT))
        fail = any(n.startswith(("TYPE", "canvas")) for n in notes)
        bad += fail
        print("%-28s %-22s %s" % (p.name,
                                  "sizes=%s" % sorted(sizes) if sizes else "sizes=-",
                                  "FAIL: " + "; ".join(notes) if fail
                                  else ("OK" + (" (%s)" % notes[0] if notes else ""))))
    if bad:
        print("\n%d figure(s) under the floor." % bad)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
