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

# --- 0. both halves of the evidence must exist before ANYTHING is rewritten ---
# Step 2's gen_report_data.py parses the C and the Rust logs in ONE pass and dies on a
# missing Rust log.  The three Rust logs are written by run_rust_bench_suite.sh and none
# of them is tracked, so on a clean checkout all three are absent.  Bail out BEFORE step
# 1 rather than part-way through: a run that refreshed the figures and then died on the
# macros would leave report/latex HALF synced -- figures from this run beside macros from
# the last one -- which is worse than not syncing at all.  Exit 0, because the C suite has
# already saved its evidence and this is the documented order, not a failure.
RUST_LOG_DIR="$ROOT/rust/fips204-las"
RUST_MISSING=""
for rlog in bench_levels_rust.log size_report_rust.log bench_las_criterion.log; do
  [ -f "$RUST_LOG_DIR/$rlog" ] || RUST_MISSING="$RUST_MISSING $rlog"
done
if [ -n "$RUST_MISSING" ]; then
  echo "sync_report.sh: DEFERRED -- missing Rust log(s):$RUST_MISSING" >&2
  echo "  report/latex left untouched (figures and generated/*.tex unchanged)." >&2
  echo "  Run scripts/run_rust_bench_suite.sh next; its own sync then does both halves." >&2
  exit 0
fi

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

# Section res-txstruct is measured, not projected (Royce, 2026-08-30): its figures come
# from two MINED transactions rather than from the projection above. This must run, or the
# chapter's btcMeas* macros and tab_btctx_measured.tex go stale while btcmacros.tex moves.
python3 "$ROOT/scripts/gen_btc_measured_tx.py"

# --- 3. optional PDF rebuild -------------------------------------------------
if [ "$BUILD" = yes ]; then
  make -C "$ROOT/report/latex"
  echo "report/latex/report.pdf rebuilt."
else
  echo "Report data synced. Rebuild the PDF with: make -C report/latex"
fi
