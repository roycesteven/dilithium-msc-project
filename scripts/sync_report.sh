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

resolve_latest_path() {
  local path="$1"
  if [ -f "$path" ]; then
    local target
    target="$(tr -d '[:space:]' < "$path")"
    case "$target" in
      /*) printf '%s\n' "$target" ;;
      *)  printf '%s/%s\n' "$(dirname "$path")" "$target" ;;
    esac
  else
    printf '%s\n' "$path"
  fi
}

ROOT="$(git rev-parse --show-toplevel)"
EV="$(resolve_latest_path "$ROOT/evidence/latest")"
# `latest` is a symlink under some runners and a one-line POINTER FILE under others
# (it currently holds "runs/<timestamp>"), so resolve it before joining any path.
FIG="$ROOT/report/latex/figures"

BUILD=no
[ "${1:-}" = "--build" ] && BUILD=yes

# --- 1. figures: evidence/latest -> report names ---------------------------
declare -A FIGMAP=(
  ["paper_package/communication_components_paper.pdf"]="fig_components.pdf"
  ["appendix_package/rejection_attempts_distribution_paper.pdf"]="fig_rejection_dist.pdf"
)
for src in "${!FIGMAP[@]}"; do
  if [ ! -f "$EV/$src" ]; then
    echo "sync_report.sh: ERROR: missing $EV/$src (run scripts/run_benchmark_suite.sh)" >&2
    exit 1
  fi
  cp "$EV/$src" "$FIG/${FIGMAP[$src]}"
  echo "  figure: ${FIGMAP[$src]}  <-  evidence/latest/$src"
done

# The three figures the report actually \includegraphics are REGENERATED here in
# print mode, not copied: no figure may carry type smaller than the 12pt body, and
# the evidence-package copies are drawn on a ~9in canvas that LaTeX then scales
# down to ~7pt on the page (Royce, 2026-08-28).  Print mode draws them at exactly
# \textwidth with 12pt type and saves uncropped, so 1pt in the plot script is 1pt
# on the page -- which is why the report includes all three at width=\linewidth.
# Copying them back from evidence/ would silently undo the floor.
PRINTFIGS="$(mktemp -d)"
python3 "$ROOT/scripts/plot_las_paper_figures.py" \
  --input-dir "$EV/tables" --output-dir "$PRINTFIGS" --appendix-dir "$PRINTFIGS" \
  --print-figures >/dev/null
cp "$PRINTFIGS/per_operation_timing_paper.pdf"     "$FIG/fig_timing.pdf"
cp "$PRINTFIGS/adaptor_overhead_paper.pdf"         "$FIG/fig_overhead.pdf"
cp "$PRINTFIGS/rejection_acceptance_cdf_paper.pdf" "$FIG/fig_rejection_cdf.pdf"
rm -rf "$PRINTFIGS"
echo "  figures: fig_timing, fig_overhead, fig_rejection_cdf  <-  regenerated at 12pt"

# --- 2. macros + data tables ------------------------------------------------
python3 "$ROOT/scripts/gen_report_data.py"

# Bitcoin wire-format projection of the settled swap transaction. Derived from the
# Stage-2 log's measured object sizes, so it must be regenerated whenever those
# change; it self-checks against the published P2WPKH/P2TR reference spends and
# exits non-zero rather than emitting numbers it cannot validate.
STAGE2_LOG="$(resolve_latest_path "$ROOT/evidence/stage2/latest")/bench_swap.log"
if [ -f "$STAGE2_LOG" ]; then
  python3 "$ROOT/scripts/gen_bitcoin_tx_data.py" \
    --log "$STAGE2_LOG" \
    --out "$ROOT/report/latex/generated/btcmacros.tex" \
    --tab "$ROOT/report/latex/generated/tab_btctx.tex"
else
  echo "WARNING: no evidence/stage2/latest/bench_swap.log; btcmacros.tex left as-is." >&2
fi

# --- 3. optional PDF rebuild -------------------------------------------------
if [ "$BUILD" = yes ]; then
  make -C "$ROOT/report/latex"
  echo "report/latex/report.pdf rebuilt."
else
  echo "Report data synced. Rebuild the PDF with: make -C report/latex"
fi
