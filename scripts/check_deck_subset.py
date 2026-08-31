#!/usr/bin/env python3
"""deck ⊆ report: every macro-backed claim the deck renders must be one the report makes.

Royce, 2026-08-30: the invariant is kept "ketat dan akurat" -- strictly, and accurately.
The obvious check (macro used in deck, not in report) is a PROXY, and successive drafts of
this gate each had a way to pass something falsely. All are closed here; do not reintroduce
any of them.

  1. VALUE MATCHING IS NOT COVERAGE. Searching the tables for a macro's value passes on
     coincidence: \\clSigBytes is 64, and "64" also occurs in tab_components, which has no
     ECDSA column. Nothing is matched on value alone.
  2. ONE GEOMETRY USE DOES NOT MAKE A MACRO GEOMETRY. A macro used once as a bar width and
     once as a label would be skipped entirely, so geometry requires EVERY occurrence to be
     a `--w:` width.
  3. AN EXEMPTION THAT DOES NOT RE-VERIFY IS A HARDCODED PASS. The three macros the report
     states without citing by name are re-checked at run time against the row that states
     them, in a table that must still be \\input by report.tex.
  4. AN EMPTY VALUE MUST NOT MATCH. If a macro's definition disappears, its value is "" and
     an unguarded regex matches the empty position in any line -- passing an exemption whose
     evidence no longer exists. Absence of a value is itself a failure.
  5. ONLY COMPILED, UNCOMMENTED TeX COUNTS. The search follows report.tex's include graph
     with TeX comments stripped first, so neither a stale draft nor `% \\input{old}` nor a
     macro named only in a `%` design note can satisfy the invariant.

⚠ SCOPE: this audits macro placeholders only. Literal figures typed into slide text (2035,
  520), qualitative claims and citations are NOT covered and still need reading.
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
DECK = ROOT / "report/slides/video_deck.template.html"
LATEX = ROOT / "report/latex"
MAIN = LATEX / "report.tex"

# macro -> (generated table stem, the row label whose line must carry its value).
# Each was established by READING that row, not by searching for the number.
STATED_IN_TABLE = {
    "clSigBytes":  ("tab_classical", "Signature"),
    "sigBytesTwo": ("tab_classical", "Signature"),
    "pkBytesTwo":  ("tab_classical", "Public key / statement"),
}


def uncomment(s: str) -> str:
    """Drop TeX comments; \\% is an escaped percent and stays."""
    return re.sub(r"(?<!\\)%.*", "", s)


def included_files(main: pathlib.Path) -> set[pathlib.Path]:
    """Every .tex actually pulled in, following \\input/\\include from report.tex."""
    seen, queue = set(), [main]
    while queue:
        f = queue.pop()
        if f in seen or not f.exists():
            continue
        seen.add(f)
        for rel in re.findall(r"\\(?:input|include)\{([^}]+)\}",
                              uncomment(f.read_text(encoding="utf-8"))):
            queue.append(LATEX / (rel if rel.endswith(".tex") else rel + ".tex"))
    return seen


def main() -> int:
    deck = DECK.read_text(encoding="utf-8")
    used = set(re.findall(r"\{\{([A-Za-z][A-Za-z0-9]*)\}\}", deck)) - {"UOM_LOGO"}

    geometry = {m for m in used
                if len(re.findall(r"\{\{" + m + r"\}\}", deck))
                == len(re.findall(r"--w:\{\{" + m + r"\}\}", deck))}
    claims = used - geometry

    files = included_files(MAIN)
    prose = "".join(uncomment(f.read_text(encoding="utf-8")) for f in files
                    if "generated" not in f.parts)
    stems = {f.stem for f in files}

    values = {}
    for g in (LATEX / "generated").glob("*.tex"):
        values.update(re.findall(r"\\newcommand\{\\([A-Za-z]+)\}\{([^}]*(?:\{,\}[^}]*)*)\}",
                                 g.read_text(encoding="utf-8")))

    gaps, exempt = [], []
    for m in sorted(claims):
        if re.search(r"\\" + m + r"(?![A-Za-z])", prose):
            continue
        if m in STATED_IN_TABLE:
            stem, row = STATED_IN_TABLE[m]
            value = values.get(m, "").replace("{,}", ",")
            table = LATEX / "generated" / f"{stem}.tex"
            if not value:
                why = f"{m} has no generated value, so its exemption has no evidence"
            elif stem not in stems:
                why = f"{stem} is no longer \\input by report.tex"
            elif not table.exists():
                why = f"{stem}.tex is missing"
            elif not any(row in ln
                         and re.search(r"(?<![0-9.])" + re.escape(value) + r"(?![0-9])", ln)
                         for ln in table.read_text(encoding="utf-8").splitlines()):
                why = f"no row '{row}' in {stem} carries {value} any more"
            else:
                why = None
            if why:
                gaps.append((m, f"EXEMPTION BROKEN: {why}"))
            else:
                exempt.append(f"{m} -- {stem} row '{row}' states {value}")
            continue
        visible = "{{%s}}" % m in re.sub(r'data-notes="[^"]*"', "", deck)
        gaps.append((m, "VISIBLE on a slide" if visible else "speaker notes only"))

    print(f"deck renders {len(used)} macros: {len(geometry)} bar geometry, {len(claims)} claims")
    print(f"report include graph: {len(files)} .tex files")
    for e in exempt:
        print(f"  stated without being cited: {e}")
    if not gaps:
        print("HOLDS: every macro-backed deck claim is stated by the report")
        return 0
    print(f"\n{len(gaps)} claim(s) the report does not make:")
    for m, where in gaps:
        print(f"  {m:<20} {where}")
    print("\nFix by stating it in the report (the appendix is word-count free) or by removing "
          "it from the deck. Only add an entry above after READING the row that states it.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
