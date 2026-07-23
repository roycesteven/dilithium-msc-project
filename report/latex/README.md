# Dissertation (LaTeX, muthesis.cls)

Official title: **Implementing Post-Quantum Secure Exotic Signature Schemes in
Blockchains**. Built with the University of Manchester CS `muthesis` class.

## Build

```bash
cd report/latex
make            # -> report.pdf  (uses latexmk if present, else pdflatex+bibtex+pdflatex x2)
make clean      # remove aux/log/bbl ...
make distclean  # also remove report.pdf
```

Requires a TeX Live install with `pdflatex` and `bibtex` (both present on the dev
host). No `siunitx` dependency (kept minimal so it builds anywhere).

## Layout

- `report.tex` — master; front matter + `\input`s each chapter.
- `chapters/00-abstract.tex` — informative abstract (no citations).
- `chapters/01-introduction.tex` — context, brief lit review, aim + objectives.
  (No separate Background chapter — folded in per the rubric.)
- `chapters/02-methodology.tex` — construction, reuse strategy, testing, benchmarking.
- `chapters/03-results.tex` — correctness, computation, communication (component
  breakdown), classical baseline, application, threats to validity.
- `chapters/04-evaluation.tex` — per-objective evaluation + reflection + limitations.
- `chapters/05-conclusion.tex` — conclusions + future work.
- `chapters/A-appendix.tex` — reproduction commands + selected code listings.
- `refs.bib` — bibliography (style `alpha`).

## Numbers in the tables

The benchmark tables are populated from real runs of `ref/test/bench_fair2`
(computation + component sizes) and the classical baseline / rejection-rate figures
recorded in `docs/LAS.md`. Re-run on the final machine of record before submission
and refresh.

## Remaining author TODOs (rendered red in the PDF)

- Student id (`\stuid` in `report.tex`).
- A four-operation data-flow figure (methodology).
- Machine of record: CPU / RAM / OS (methodology + results captions).
- Optional bonus-tier content: on-chain gas figure, multi-hop AMHL (results).
- Confirm external author lists in `refs.bib` (survey 2022/1151, poqeth 2025/091).

## Word count (for submission)

`muthesis` supports a `wordcount` class option that reads a `word.count` file:

```bash
make wordcount   # needs texcount; then enable the wordcount option in report.tex
```
