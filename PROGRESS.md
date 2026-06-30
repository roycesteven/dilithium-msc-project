---
## Checkpoint — 2026-06-30 00:30

Branch: main

Current goal:
- Reorganise benchmark evidence output into clean subfolders so Stage-1 paper figures aren't mixed with debug/application output.

Done:
- Reworked scripts/run_benchmark_suite.sh: logs/, tables/, paper_package/, appendix_package/, debug_figures/, application_package/ via staging + allowlist distribution.
- Added --appendix-dir to scripts/plot_las_paper_figures.py (rejection figure -> appendix_package).
- Generated organised MANIFEST.md + paper_package/README.md ("show these to Wang").

Files touched/inspected:
- scripts/run_benchmark_suite.sh
- scripts/plot_las_paper_figures.py
- scripts/plot_las_benchmarks.py (read only; unchanged)

Evidence used:
- none

Open risks:
- Suite not yet run; new tree only validated via bash -n + py_compile + scratch run of paper script.

Next action:
- Run scripts/run_benchmark_suite.sh and eyeball paper_package/ before showing Wang.
- Make Stage-1 results/methodology presentation defensible (Meeting-4) + paper-faithful notation.

Done:
- New supervisor-review skill (.claude/skills/supervisor-review); used it to judge Fig 3.1 + methodology.
- Fig 3.1 reworked to paired basic(blue)-vs-LAS(orange) overhead chart at D3 (overhead % labels); moved tab:overhead-l3 to appendix (chart-in-body/table-in-appendix); Table 3.2 caveat+param strip; fixed methodology kappa=60 + polynomial-count inconsistencies.
- Report notation N->d (paper) everywhere + figures regenerated (d=256); CLAUDE.md source-of-truth rule strengthened; las.h:18 paper<->code bridge comment; THEORY_IMPL_BRIDGE.md X^N->X^d cell.

Files touched/inspected:
- report/latex/chapters/{02-methodology,03-results,A-appendix}.tex
- scripts/plot_las_paper_figures.py; report/latex/figures/{fig_timing,fig_components}.pdf
- CLAUDE.md; ref/las.h; docs/THEORY_IMPL_BRIDGE.md

Evidence used:
- evidence/latest/tables/{primary_timing,adaptor_overhead}.csv

Open risks:
- Report PDF not rebuilt (no make per guardrail); Royce to run make in report/latex.
- Table 3.2 still lacks +/- SD (needs measured classical run, not invented).

Next action:
- Rebuild report.pdf (make in report/latex) and eyeball Fig 3.1 + tab:notation render.
---