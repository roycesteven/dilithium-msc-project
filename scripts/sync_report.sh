#!/usr/bin/env bash
#
# sync_report.sh -- make the LaTeX report reflect the LATEST benchmark evidence.
#
# Runs automatically at the end of scripts/run_benchmark_suite.sh (C evidence)
# and scripts/run_rust_bench_suite.sh (Rust evidence); safe to run by hand too.
# It only READS evidence -- it never runs a benchmark and never edits a log.
#
#   1. copy the Stage-1 paper-package figures from evidence/latest into
#      report/latex/figures/ under the report's figure names
#      (fig_timing, fig_components, fig_overhead) -- no more hand-copying;
#   2. regenerate report/latex/generated/*.tex (all inline numbers as macros
#      + all data tables) via scripts/gen_report_data.py;
#   3. with --build, also rebuild report/latex/report.pdf.
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel)"
EV="$ROOT/evidence/latest"
FIG="$ROOT/report/latex/figures"

BUILD=no
[ "${1:-}" = "--build" ] && BUILD=yes

# --- 1. figures: evidence/latest -> report names ---------------------------
declare -A FIGMAP=(
  ["paper_package/per_operation_timing_paper.pdf"]="fig_timing.pdf"
  ["paper_package/communication_components_paper.pdf"]="fig_components.pdf"
  ["appendix_package/adaptor_overhead_paper.pdf"]="fig_overhead.pdf"
)
for src in "${!FIGMAP[@]}"; do
  if [ ! -f "$EV/$src" ]; then
    echo "sync_report.sh: ERROR: missing $EV/$src (run scripts/run_benchmark_suite.sh)" >&2
    exit 1
  fi
  cp "$EV/$src" "$FIG/${FIGMAP[$src]}"
  echo "  figure: ${FIGMAP[$src]}  <-  evidence/latest/$src"
done

# --- 2. macros + data tables ------------------------------------------------
python3 "$ROOT/scripts/gen_report_data.py"

# --- 3. optional PDF rebuild -------------------------------------------------
if [ "$BUILD" = yes ]; then
  make -C "$ROOT/report/latex"
  echo "report/latex/report.pdf rebuilt."
else
  echo "Report data synced. Rebuild the PDF with: make -C report/latex"
fi
