#!/usr/bin/env bash
# A succinct POST-QUANTUM proof for the role-A relation, measured against the
# two deployed provers.
#
# WHY THIS EXISTS
#   The amortisation experiment (evidence/amortise/, evidence/lazer_amortise/)
#   closed the "make the existing proof cheaper" direction for BOTH deployed
#   provers: Groth16's proof is a constant 128 B but was never the bottleneck,
#   and LaZer's ~30 KiB does shrink under batching -- only by making per-swap
#   proving and verification 3.3x worse. What that left was not a cheaper
#   instance of either system but a different KIND: post-quantum (Groth16 is
#   not) AND succinct (LaZer is not).
#
#   This runner measures a Winterfell FRI-STARK proving the SAME statement,
#   `exists r : A r = t' and ||r||inf <= 1`, so the three provers can be compared
#   on one relation.
#
#   ./scripts/run_role_a_stark.sh
#
# GATES (the binary exits non-zero if any fails)
#   * the instance must satisfy the relation BEFORE anything is timed, so no
#     proof can be of a vacuous statement;
#   * a success-path assertion after every timed proof;
#   * a tampered-statement check per repetition -- a proof that verified against
#     the wrong t' would mean the measurement is of nothing;
#   * untimed warm-up, >= 5 repetitions.
#
# The correctness tests live in rust/las-stark/tests/las_stark.rs
# (`role_a_*`): witness refusal for a non-ternary r and for a wrong statement,
# proof round-trip, tampered-statement rejection, tampered-proof rejection.
#
# SCOPE
#   No concrete-security analysis of this AIR has been done, and Winterfell's
#   prover adds no zero-knowledge randomisation -- this is a succinct ARGUMENT of
#   knowledge, not a zk one. The relation proven is exactly the deployed one
#   (same ternary bound, same real A'), so SIZES and TIMES are comparable; the
#   security levels are not claimed to be. Nothing here is wired into the swap.
#
# Layout mirrors evidence/stark/. Never hand-edit a log: to change a number,
# change the code and re-run.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CRATE="$REPO/rust/las-stark"

RUN_ID="$(date +%Y%m%d_%H%M%S)"
OUT="$REPO/evidence/role_a_stark/$RUN_ID"
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
  echo "winterfell=$(grep -m1 '^winterfell' "$CRATE/Cargo.toml" | sed 's/.*= *//')"
  echo "sources=rust/las-stark/src/role_a_air.rs rust/las-stark/src/bin/bench_role_a.rs"
  echo "vectors=evm/test/vectors (A' is the REAL matrix of the C build)"
  echo "baselines=evidence/amortise/latest (Groth16 k=1), evidence/lazer_amortise/latest (LaZer k=1)"
  echo "note=succinct ARGUMENT of knowledge; zero-knowledge NOT added by Winterfell's prover"
  echo "note=no concrete-security analysis of this AIR; engineering data point only"
} > "$OUT/environment.txt"

cd "$CRATE"
cargo test --release --quiet 2>&1 | tee "$OUT/tests.log"
cargo run --release --quiet --bin bench_role_a 2>&1 | tee "$OUT/bench_role_a.log"

ln -sfn "$RUN_ID" "$REPO/evidence/role_a_stark/latest"
echo
echo "evidence written: evidence/role_a_stark/$RUN_ID/{bench_role_a.log,tests.log,environment.txt}"
echo "decisive rows: the three-prover table -- read proof size against verify time."
