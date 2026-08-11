#!/usr/bin/env bash
# Capture the Criterion.rs statistical output as evidence and rebuild the report's
# PreSign distribution figure from it.
#
# Layout mirrors evidence/onchain/, evidence/stage2/ and evidence/stark/: one
# timestamped directory per run holding the tool's own output plus an environment
# record, with `latest` pointing at it.
#
#   ./scripts/run_criterion_fig.sh [--reuse]
#
#     (default)  re-run `cargo bench` so the figure and the log share one run
#     --reuse    skip the bench and capture whatever is already in target/criterion
#                (use only when the existing run is known to be current)
#
# Why this exists: the report figure was previously produced by converting a
# Criterion SVG by hand, so it carried no provenance and silently went stale while
# the underlying Criterion run moved on. The figure now records the run that
# produced it, alongside the raw estimates.json and sample.json it was drawn from.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CRATE="$REPO/rust/fips204-las"
BENCH="Algorithm 2 - LAS adaptor signature/PreSign"

REUSE=no
[ "${1:-}" = "--reuse" ] && REUSE=yes

RUN_ID="$(date +%Y%m%d_%H%M%S)"
OUT="$REPO/evidence/criterion/$RUN_ID"
mkdir -p "$OUT"

cd "$CRATE"
if [ "$REUSE" = no ]; then
  echo "running Criterion (this takes ~15 min for the full 14-benchmark suite)"
  cargo bench --bench las_bench 2>&1 | tee "$OUT/bench_las_criterion.log"
  cp "$OUT/bench_las_criterion.log" "$CRATE/bench_las_criterion.log"
else
  echo "--reuse: capturing the existing target/criterion run"
  [ -f "$CRATE/bench_las_criterion.log" ] && \
    cp "$CRATE/bench_las_criterion.log" "$OUT/bench_las_criterion.log"
fi

SRC="$CRATE/target/criterion/$BENCH"
[ -d "$SRC" ] || { echo "ERROR: no Criterion output at $SRC" >&2; exit 1; }

# raw statistics the figure is drawn from -- kept so the figure is auditable
cp "$SRC/new/estimates.json" "$OUT/presign_estimates.json"
cp "$SRC/new/sample.json"    "$OUT/presign_sample.json"
cp "$SRC/report/pdf.svg"     "$OUT/presign_pdf.svg"

{
  echo "run_id=$RUN_ID"
  echo "git=$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo n/a)"
  echo "host=$(uname -s) $(uname -r) $(uname -m)"
  echo "cpu=$({ grep -m1 'model name' /proc/cpuinfo 2>/dev/null || true; } | sed 's/.*: //')"
  echo "rustc=$(rustc --version 2>/dev/null || echo n/a)"
  echo "benchmark=$BENCH"
  echo "criterion_run_mtime=$(date -r "$SRC/new/estimates.json" '+%Y-%m-%d %H:%M:%S')"
  echo "reused_existing_run=$REUSE"
} > "$OUT/environment.txt"

ln -sfn "$RUN_ID" "$REPO/evidence/criterion/latest"

# SVG -> PDF, so the report ships Criterion's OWN plot (not a redrawing of it).
# gen_criterion_figure.py enlarges only the type and re-flows the margins the
# larger type needs -- at Criterion's native 12-unit type the labels render at
# about 4.2 pt when included at \linewidth.  No plotted value is touched.
python3 "$REPO/scripts/gen_criterion_figure.py" \
        "$OUT/presign_pdf.svg" \
        "$REPO/report/latex/figures/fig_criterion_presign.pdf"

echo
echo "evidence written: evidence/criterion/$RUN_ID/"
echo "  bench_las_criterion.log, presign_{estimates,sample}.json, presign_pdf.svg, environment.txt"
echo "figure regenerated: report/latex/figures/fig_criterion_presign.pdf"
