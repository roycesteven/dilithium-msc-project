#!/usr/bin/env python3
"""Render the 6-8 minute video deck from the report's own generated macros.

report/slides/video_deck.template.html carries the slides and the speaker notes;
every {{placeholder}} in it is filled from report/latex/generated/*.tex -- the
same macro files the LaTeX report reads, produced by scripts/sync_report.sh from
a real evidence run.  The deck therefore never carries a hand-typed number, and a
regenerated benchmark suite updates the slides by re-running this script.

Two report figures are embedded as base64 PNG (rasterised from the committed
PDFs with pdftoppm) so the deck is a single self-contained file: it opens from
disk, over a share link, or inside a recording tool with no asset paths to break.

This script only READS evidence and report artefacts.  It never builds, runs a
benchmark, or estimates a value; an unknown placeholder or an unresolved macro
is a hard error rather than a blank on a slide.

    python3 scripts/gen_slides.py            # -> report/slides/video_deck.html
    python3 scripts/gen_slides.py --check    # non-zero exit if the deck is stale
"""

from __future__ import annotations

import argparse
import base64
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
MACRO_DIR = ROOT / "report" / "latex" / "generated"
FIG_DIR = ROOT / "report" / "latex" / "figures"
SLIDES = ROOT / "report" / "slides"
# (template, output) pairs, both rendered from the same macro set
TARGETS = [
    (SLIDES / "video_deck.template.html", SLIDES / "video_deck.html"),
    (SLIDES / "swap_console.template.html", SLIDES / "swap_console.html"),
]

# placeholder -> figure stem under report/latex/figures/
FIGURES = {
    "FIG_TIMING": "fig_timing",
    "FIG_ONCHAIN": "fig_onchain",
}
FIG_DPI = 150

# placeholder -> committed image under report/slides/assets/, embedded the same
# way as the figures so the deck stays one self-contained file.  uom_logo.png is
# the University of Manchester mark, rasterised from the master's own EMF in
# report/slides/Master_169 presentation(2).pptx (provenance: assets/README.md).
ASSETS = {
    "UOM_LOGO": SLIDES / "assets" / "uom_logo.png",
}

PLACEHOLDER = re.compile(r"\{\{([A-Za-z_][A-Za-z0-9_]*)\}\}")
NEWCOMMAND = re.compile(r"\\newcommand\{\\([A-Za-z]+)\}\{")

# LaTeX spellings that appear in the generated macro values, in the order they
# must be applied.  `\,` is this repository's thousands separator, so it becomes
# a comma rather than a space.
LATEX_FIXES = [
    (r"\ldots", "\u2026"),
    (r"$\sim$", "\u2248"),
    (r"$\approx$", "\u2248"),
    (r"\times", "\u00d7"),
    (r"{,}", ","),
    (r"\,", ","),
    (r"\_", "_"),
    (r"\%", "%"),
    (r"\&", "&"),
    (r"\#", "#"),
    (r"--", "\u2013"),
    (r"~", " "),
    (r"$", ""),
]


def read_macros() -> dict[str, str]:
    """Collect every \\newcommand in the generated macro files, braces balanced."""
    macros: dict[str, str] = {}
    files = sorted(MACRO_DIR.glob("*.tex"))
    if not files:
        sys.exit(f"no macro files under {MACRO_DIR} -- run scripts/sync_report.sh first")
    for path in files:
        text = path.read_text(encoding="utf-8")
        for match in NEWCOMMAND.finditer(text):
            name = match.group(1)
            depth, j = 1, match.end()
            while j < len(text) and depth:
                if text[j] == "{":
                    depth += 1
                elif text[j] == "}":
                    depth -= 1
                j += 1
            if depth:
                sys.exit(f"{path.name}: unbalanced braces in \\{name}")
            macros[name] = clean(text[match.end() : j - 1], f"{path.name}:\\{name}")
    return macros


def clean(value: str, where: str) -> str:
    """Turn a LaTeX macro body into plain text, refusing anything left over."""
    for src, dst in LATEX_FIXES:
        value = value.replace(src, dst)
    value = re.sub(r"\\(?:emph|texttt|textbf|mathit)\{([^{}]*)\}", r"\1", value)
    value = value.strip()
    if "\\" in value:
        sys.exit(f"{where}: unhandled LaTeX in macro value: {value!r}")
    return value


def derive(macros: dict[str, str]) -> dict[str, str]:
    """Chart geometry, computed from the same macros the labels print.

    A bar width typed by hand is a number that silently stops matching its own
    label the next time the evidence changes, so the widths are derived here
    instead.  These are presentation geometry only -- percentages of a bar --
    never a reported quantity.
    """

    def num(name: str) -> float:
        try:
            return float(macros[name].replace(",", "").replace("–", "-").split("-")[0])
        except (KeyError, ValueError):
            sys.exit(f"cannot derive chart geometry: macro \\{name} missing or non-numeric")

    out: dict[str, str] = {}

    # Slide 7: four size bars share ONE byte scale, so the eye compares them.
    scale = max(num("clSigBytes"), num("sigBytesTwo"), num("clPkBytes"), num("pkBytesTwo"))
    for key, macro in (
        ("wClSig", "clSigBytes"),
        ("wLasSig", "sigBytesTwo"),
        ("wClPk", "clPkBytes"),
        ("wLasPk", "pkBytesTwo"),
    ):
        out[key] = f"{100 * num(macro) / scale:.2f}"

    # Swap console: bytes-per-swap comparison, one shared scale.  Where a total is a
    # range (a Huffman-coded proof length varies with the sampled values) the LOW end
    # sets the bar; the label beside it still prints the range verbatim.
    totals = {k: num(k) for k in ("cfgOneBytesTotal", "cfgTwoBytesTotal", "cfgThreeBytesTotal")}
    widest = max(totals.values())
    for key, macro in (
        ("wTotOne", "cfgOneBytesTotal"),
        ("wTotTwo", "cfgTwoBytesTotal"),
        ("wTotThree", "cfgThreeBytesTotal"),
    ):
        out[key] = f"{100 * totals[macro] / widest:.2f}"

    # Part-to-whole remainders, so the smaller segment is never eyeballed.
    for key, macro in (
        ("zRestPct", "zPctTarget"),
        ("cfgTwoRestPct", "cfgTwoProofPct"),
        ("cfgThreeRestPct", "cfgThreeProofPct"),
    ):
        out[key] = f"{100 - num(macro):.1f}"

    # Cost-in-time slide: the adaptor layer's overhead over ITS OWN base, on one
    # shared scale for both schemes.  The classical macro is a ratio (x4.6 of
    # Sign), the LAS macros are percentages; the bars encode the same quantity
    # -- overhead percent -- while each label still prints its macro verbatim,
    # so no number is retyped in another unit.
    cl_over_pct = (num("clOvPreSignX") - 1.0) * 100.0
    out["wClAdLayer"] = "100.00"
    out["wLasAdPacked"] = f"{100 * num('packedOvPreSign') / cl_over_pct:.2f}"
    out["wLasAdCore"] = f"{100 * num('ovPreSign') / cl_over_pct:.2f}"
    return out


def figure_data_uri(stem: str) -> str:
    """Rasterise a committed report figure to a base64 PNG data URI."""
    png = FIG_DIR / f"{stem}.png"
    if png.is_file():
        return "data:image/png;base64," + base64.b64encode(png.read_bytes()).decode()

    pdf = FIG_DIR / f"{stem}.pdf"
    if not pdf.is_file():
        sys.exit(f"missing figure {pdf} -- regenerate it with scripts/plot_las_paper_figures.py")
    if not shutil.which("pdftoppm"):
        sys.exit("pdftoppm not found (install poppler-utils) -- needed to embed the figures")
    with tempfile.TemporaryDirectory() as tmp:
        out = pathlib.Path(tmp) / stem
        subprocess.run(
            ["pdftoppm", "-png", "-r", str(FIG_DPI), "-singlefile", str(pdf), str(out)],
            check=True,
            capture_output=True,
        )
        data = out.with_suffix(".png").read_bytes()
    return "data:image/png;base64," + base64.b64encode(data).decode()


def build_values() -> dict[str, str]:
    values = read_macros()
    values.update(derive(values))
    for key, stem in FIGURES.items():
        values[key] = figure_data_uri(stem)
    for key, path in ASSETS.items():
        if not path.is_file():
            sys.exit(f"missing asset {path} -- see report/slides/assets/README.md")
        values[key] = ("data:image/png;base64,"
                       + base64.b64encode(path.read_bytes()).decode())
    return values


def render(template_path: pathlib.Path, values: dict[str, str]) -> str:
    if not template_path.is_file():
        sys.exit(f"missing template {template_path}")
    template = template_path.read_text(encoding="utf-8")

    missing: list[str] = []

    def substitute(match: re.Match[str]) -> str:
        name = match.group(1)
        if name not in values:
            missing.append(name)
            return match.group(0)
        return values[name]

    rendered = PLACEHOLDER.sub(substitute, template)
    if missing:
        sys.exit(
            "template uses placeholders with no generated macro: "
            + ", ".join(sorted(set(missing)))
            + "\n(a renamed macro, or a stale report/latex/generated/ -- re-run scripts/sync_report.sh)"
        )
    banner = (
        f"<!-- GENERATED by scripts/gen_slides.py from {template_path.name}\n"
        "     + report/latex/generated/*.tex. DO NOT EDIT: edit the template and re-run. -->\n"
    )
    return rendered.replace("<!doctype html>\n", "<!doctype html>\n" + banner, 1)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="exit non-zero if a committed artefact differs from a fresh render",
    )
    args = parser.parse_args()

    values = build_values()
    stale = 0
    for template_path, output in TARGETS:
        rendered = render(template_path, values)
        if args.check:
            current = output.read_text(encoding="utf-8") if output.is_file() else ""
            if current != rendered:
                print(f"STALE: {output.relative_to(ROOT)} -- re-run scripts/gen_slides.py")
                stale = 1
            else:
                print(f"up to date: {output.relative_to(ROOT)}")
            continue
        output.write_text(rendered, encoding="utf-8")
        slides = rendered.count('<section class="slide')
        detail = f"{slides} slides, " if slides else ""
        print(f"wrote {output.relative_to(ROOT)} ({detail}{len(rendered)/1024:.0f} KB)")
    if args.check:
        return stale
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
