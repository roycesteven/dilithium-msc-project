#!/usr/bin/env bash
# The full ML-DSA adaptor experiment: does LAS compose with NIST FIPS 204 as
# specified, is the resulting scheme correct, and what does it cost against the
# implementation of record?
#
# Three binaries per parameter set, in the order they must be read:
#
#   1. test_mldsa_hint     WHICH ML-DSA feature breaks a naive adaptor port?
#                          Diagnostic. A "FAILS ALWAYS" row is a RESULT.
#   2. test_mldsa_las      Is the repaired scheme a CORRECT adaptor signature?
#                          Itemised contract, pass/fail, non-zero exit on failure.
#   3. bench_mldsa_compare What does it COST vs the simplified-Dilithium scheme?
#                          Both constructions in ONE binary, same machine, one run.
#
# Layout mirrors evidence/onchain/, evidence/stage2/, evidence/criterion/ and
# evidence/stark/: one timestamped directory per run holding the tools' own
# output plus an environment record, with `latest` pointing at it.
#
#   ./scripts/run_mldsa_hint_experiment.sh [--mode N] [--skip-bench]
#
#     (default)     all three sets: ML-DSA-44, -65, -87
#     --mode N      only DILITHIUM_MODE=N (2, 3 or 5)
#     --skip-bench  correctness only (the benchmark is the slow part)
#
# WHY THIS EXISTS
#   Everywhere else the project builds LAS on the LAS paper's SIMPLIFIED
#   Dilithium -- no hint vector, no Power2Round, no high/low-bit split -- and
#   ASSERTED that NIST's construction has to be modified before an adaptor layer
#   can sit on it.  This experiment builds the adaptor on ML-DSA with all three
#   features ENABLED and reports what actually survives, so the report's claim is
#   a demonstration rather than an assertion.
#
# GATES
#   test_mldsa_las exits non-zero if any contract item fails.
#   bench_mldsa_compare exits non-zero if a rejection gate, an attempt-counter
#   check or a success-path assertion fails -- a run that drifts off theory must
#   never produce a publishable-looking number.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REF="$REPO/ref"

MODES="2 3 5"
SKIP_BENCH=no
while [ $# -gt 0 ]; do
  case "$1" in
    --mode)       MODES="${2:?--mode needs a value (2, 3 or 5)}"; shift 2 ;;
    --skip-bench) SKIP_BENCH=yes; shift ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

RUN_ID="$(date +%Y%m%d_%H%M%S)"
OUT="$REPO/evidence/mldsa_hint/$RUN_ID"
mkdir -p "$OUT"

GIT_SHORT="$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo n/a)"
GIT_BRANCH="$(git -C "$REPO" rev-parse --abbrev-ref HEAD 2>/dev/null || echo n/a)"
REPRO="-DLAS_GIT_COMMIT=\\\"$GIT_SHORT\\\" -DLAS_GIT_BRANCH=\\\"$GIT_BRANCH\\\""

cd "$REF"
FAILED=""
for M in $MODES; do
  echo "=== ML-DSA mode $M: 1/3 hint diagnostic ==="
  make "test/test_mldsa_hint$M" >"$OUT/build_hint_mode$M.log" 2>&1
  "./test/test_mldsa_hint$M" 2>&1 | tee "$OUT/mldsa_hint_mode$M.log"

  echo
  echo "=== ML-DSA mode $M: 2/3 correctness contract ==="
  make "test/test_mldsa_las$M" >"$OUT/build_contract_mode$M.log" 2>&1
  if ! "./test/test_mldsa_las$M" 2>&1 | tee "$OUT/mldsa_contract_mode$M.log"; then
    FAILED="$FAILED contract-mode$M"
  fi

  if [ "$SKIP_BENCH" = no ]; then
    echo
    echo "=== ML-DSA mode $M: 3/3 head-to-head benchmark vs simplified Dilithium ==="
    make "test/bench_mldsa_compare$M" REPRO_FLAGS="$REPRO" \
      >"$OUT/build_bench_mode$M.log" 2>&1
    if ! "./test/bench_mldsa_compare$M" 2>&1 | tee "$OUT/mldsa_compare_mode$M.log"; then
      FAILED="$FAILED bench-mode$M"
    fi
  fi
  echo
done

{
  echo "run_id=$RUN_ID"
  echo "git=$GIT_SHORT"
  echo "branch=$GIT_BRANCH"
  echo "host=$(uname -s) $(uname -r) $(uname -m)"
  echo "cpu=$({ grep -m1 'model name' /proc/cpuinfo 2>/dev/null || true; } | sed 's/.*: //')"
  echo "cc=$(${CC:-cc} --version 2>/dev/null | head -1 || echo n/a)"
  echo "modes=$MODES"
  echo "skip_bench=$SKIP_BENCH"
  echo "sources=ref/mldsa_las.c ref/test/test_mldsa_hint.c ref/test/test_mldsa_las.c ref/test/bench_mldsa_compare.c"
  echo "upstream=ref/sign.c and all ML-DSA primitives UNMODIFIED; control verifier is the stock crypto_sign_verify"
  echo "note=timings are wall-clock on a loaded desktop; the two constructions are"
  echo "note=measured sequentially in one process, so intra-run drift is not controlled for"
} > "$OUT/environment.txt"

ln -sfn "$RUN_ID" "$REPO/evidence/mldsa_hint/latest"

echo "evidence written to evidence/mldsa_hint/$RUN_ID (latest -> $RUN_ID)"
if [ -n "$FAILED" ]; then
  echo "FAIL:$FAILED -- see the logs above; these results must not be published." >&2
  exit 1
fi
echo "all contracts and run-validity gates passed."
echo "decisive rows: 'P4 stock Verify ACCEPTS adapted signature' (hint diagnostic),"
echo "               'PreSign vs Sign (per attempt)' (head-to-head benchmark)."
