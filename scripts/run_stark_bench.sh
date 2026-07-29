#!/usr/bin/env bash
# Run the las-stark benchmark of record and capture it as evidence.
#
# Layout mirrors evidence/stage2/: one timestamped directory per run holding the
# tool's own stdout plus an environment record, with `latest` pointing at it.
#
# The log is written BY THE TOOL (tee'd verbatim). Never hand-edit it: to change a
# number, change the code and re-run.
#
#   ./scripts/run_stark_bench.sh [runs]
#
# Protocol (see rust/las-stark/src/bin/bench_stark.rs): 3 s discarded warm-up,
# then >= 5 timed repetitions, mean +/- sample (n-1) SD, one machine, one process.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CRATE="$REPO/rust/las-stark"
RUNS="${1:-5}"

RUN_ID="$(date +%Y%m%d_%H%M%S)"
OUT="$REPO/evidence/stark/$RUN_ID"
mkdir -p "$OUT"

{
  echo "run_id=$RUN_ID"
  echo "git=$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo n/a)"
  echo "branch=$(git -C "$REPO" rev-parse --abbrev-ref HEAD 2>/dev/null || echo n/a)"
  echo "host=$(uname -s) $(uname -r) $(uname -m)"
  echo "cpu=$({ grep -m1 'model name' /proc/cpuinfo 2>/dev/null || true; } | sed 's/.*: //')"
  echo "cpu_cores=$(nproc 2>/dev/null || echo n/a)"
  echo "rustc=$(rustc --version 2>/dev/null || echo n/a)"
  echo "profile=release (opt-level 3, debug-assertions off)"
  echo "crate=las-stark $(grep -m1 '^version' "$CRATE/Cargo.toml" | sed 's/.*= *//')"
  echo "winterfell=$(grep -m1 '^winterfell' "$CRATE/Cargo.toml" | sed 's/.*= *//')"
  echo "protocol=3 s discarded warm-up; $RUNS timed repetitions; mean +/- sample (n-1) SD"
  echo "vectors=evm/test/vectors (deterministic export from the C implementation)"
} > "$OUT/environment.txt"

cd "$CRATE"
cargo run --release --quiet --bin bench_stark -- --runs "$RUNS" \
  2>&1 | tee "$OUT/bench_stark.log"

ln -sfn "$RUN_ID" "$REPO/evidence/stark/latest"
echo
echo "evidence written: evidence/stark/$RUN_ID/{bench_stark.log,environment.txt}"
echo "evidence/stark/latest -> $RUN_ID"
