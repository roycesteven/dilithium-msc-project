#!/usr/bin/env bash
# Does the role-A proof amortise across swaps?
#
# WHY THIS EXISTS
#   The Stage-2 swap study found the role-A proof of knowledge to be the dominant
#   end-to-end cost by an order of magnitude, and left "reduce or amortise it" as
#   an open direction. A party opens each swap with a FRESH statement, so today
#   each swap carries its own proof -- but a party running many swaps could prove
#   a batch at once. This runner measures whether that helps, and which of the
#   three costs (proving, verification, proof size) actually amortises.
#
#   ./scripts/run_amortise_bench.sh
#
# WHAT IT MEASURES
#   For k = 1, 2, 4, 8: one Groth16 proof covering k instances of the SAME
#   relation configuration 2 already proves, reported as totals and per swap.
#   The batched circuit emits every instance through the same `emit_instance` the
#   single-instance circuit uses, so a batch proves exactly the conjunction of k
#   copies of the claim -- nothing is relaxed to make batching look better.
#
# GATES (the binary exits non-zero if any fails)
#   * success-path assertion after every timed proof: a timed block that measured
#     a rejection would publish a fast, meaningless number;
#   * a tamper check per batch size, corrupting the LAST instance's public input
#     and requiring rejection -- otherwise a batch could look cheap merely by
#     proving less than k single proofs do;
#   * an untimed warm-up before the first measurement, and >= 5 repetitions.
#
# SCOPE
#   Groth16 only. LaZer's proof grows with the relation it proves, so batching
#   there is a different measurement and would need the committed parameter
#   header regenerated; it is NOT measured here and no claim is made about it.
#   Groth16 is not post-quantum -- it is configuration 2's prover, kept as the
#   controlled comparison against LaZer.
#
# Layout mirrors evidence/stage2/. Never hand-edit a log: to change a number,
# change the code and re-run.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CRATE="$REPO/rust/las-swap"
FEATURES="groth16"

RUN_ID="$(date +%Y%m%d_%H%M%S)"
OUT="$REPO/evidence/amortise/$RUN_ID"
mkdir -p "$OUT"

{
  echo "run_id=$RUN_ID"
  echo "git=$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo n/a)"
  echo "branch=$(git -C "$REPO" rev-parse --abbrev-ref HEAD 2>/dev/null || echo n/a)"
  echo "host=$(uname -s) $(uname -r) $(uname -m)"
  echo "cpu=$({ grep -m1 'model name' /proc/cpuinfo 2>/dev/null || true; } | sed 's/.*: //')"
  echo "rustc=$(rustc --version 2>/dev/null || echo n/a)"
  echo "features=$FEATURES"
  echo "sources=rust/las-swap/src/bin/bench_amortise.rs rust/las-swap/src/groth16_circuit.rs"
  echo "seed=same master seed and subseed tags as bench_swap, so the statements are the same"
  echo "note=wall-clock on a loaded desktop; a fresh SRS is generated per repetition"
} > "$OUT/environment.txt"

cd "$CRATE"
cargo run --release --quiet --bin bench_amortise --features "$FEATURES" \
  2>&1 | tee "$OUT/bench_amortise.log"

ln -sfn "$RUN_ID" "$REPO/evidence/amortise/latest"
echo
echo "evidence written: evidence/amortise/$RUN_ID/{bench_amortise.log,environment.txt}"
echo "decisive rows: the 'Amortised PER SWAP' table -- proof bytes fall as 1/k,"
echo "               proving and verification per swap do not."
