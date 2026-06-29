---

## Checkpoint — 2026-06-26 18:45

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
