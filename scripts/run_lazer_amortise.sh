#!/usr/bin/env bash
# Does the role-A proof amortise across swaps when the prover is LaZer?
#
# WHY THIS EXISTS
#   scripts/run_amortise_bench.sh answered the question for Groth16 and got a
#   NEGATIVE result for a Groth16-specific reason: batching shrank a 128 B proof
#   that was never the bottleneck, while proving stayed flat. It left one thing
#   open -- the same 1/k would land on a cost that MATTERS for a proof system
#   with a large proof, and LaZer is exactly that case (~30 KiB proof, ~216 ms
#   generation in evidence/stage2/latest). This runner settles it.
#
#   ./scripts/run_lazer_amortise.sh
#
# WHAT IT MEASURES
#   For k = 1, 2, 4, 8: ONE LaZer proof over k independent instances of the
#   Fig. 1 relation, laid out block-diagonally, reported as totals and per swap.
#   Each k uses its own generated parameter set; k=1 uses the COMMITTED set that
#   configuration 3 ships, so the baseline is the deployed prover itself.
#
# GATES (the binary exits non-zero if any fails)
#   * success-path assertion after every timed proof;
#   * a tamper check per batch size on the LAST instance's statement -- a batch
#     that did not bind all k could look cheap by proving less;
#   * untimed warm-up, >= 5 repetitions.
#
# REQUIREMENTS
#   The vendored LaZer library built once (see README "pi / atomic swap").
#   SageMath is NOT needed: ref/relation_zk_params_k{2,4,8}.h are committed.
#
# SCOPE
#   Time and communication only. The batched parameter sets come from the same
#   LaZer codegen as the committed k=1 set and target the same knowledge error,
#   but have NOT been independently reviewed; no security claim is made about
#   batching. Nothing here is wired into the swap -- configuration 3 still uses
#   the k=1 module.
#
# Layout mirrors evidence/amortise/. Never hand-edit a log: to change a number,
# change the code and re-run.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REF="$REPO/ref"

RUN_ID="$(date +%Y%m%d_%H%M%S)"
OUT="$REPO/evidence/lazer_amortise/$RUN_ID"
mkdir -p "$OUT"

cd "$REF"
make test/bench_lazer_amortise >"$OUT/build.log" 2>&1

{
  echo "run_id=$RUN_ID"
  echo "git=$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo n/a)"
  echo "branch=$(git -C "$REPO" rev-parse --abbrev-ref HEAD 2>/dev/null || echo n/a)"
  echo "host=$(uname -s) $(uname -r) $(uname -m)"
  echo "cpu=$({ grep -m1 'model name' /proc/cpuinfo 2>/dev/null || true; } | sed 's/.*: //')"
  echo "cc=$(${CC:-cc} --version 2>/dev/null | head -1 || echo n/a)"
  echo "sources=ref/test/bench_lazer_amortise.c ref/relation_zk_batch.c ref/relation_zk_lazer_batch.c"
  echo "params=ref/relation_zk_params.h (k=1, committed, as shipped) + relation_zk_params_k{2,4,8}.h"
  echo "params_regen=scripts/gen_lazer_batch_params.sh (needs SageMath; not needed to build)"
  echo "note=wall-clock on a loaded desktop; k=1 is the parameter set configuration 3 ships"
  echo "note=not wired into the swap; configuration 3 continues to use the k=1 module"
} > "$OUT/environment.txt"

./test/bench_lazer_amortise 2>&1 | tee "$OUT/bench_lazer_amortise.log"

ln -sfn "$RUN_ID" "$REPO/evidence/lazer_amortise/latest"
echo
echo "evidence written: evidence/lazer_amortise/$RUN_ID/{bench_lazer_amortise.log,environment.txt}"
echo "decisive rows: the 'Amortised PER SWAP' table -- proof bytes fall sublinearly,"
echo "               but prove and verify per swap RISE. Read them together."
