#!/usr/bin/env bash
# The role-A proof of knowledge under LaBRADOR: succinct + post-quantum + ZERO-KNOWLEDGE.
#
# WHY THIS EXISTS
#   eprint 2020/845 Section 4.1 requires pi to be ZERO-KNOWLEDGE -- the swap's
#   security rests on pi hiding the witness, since a counterparty who learned r
#   could adapt the other pre-signature and take both sides. The FRI-STARK
#   (scripts/run_role_a_stark.sh) is succinct and PQ but NOT zk, so it was never
#   a valid pi. LaBRADOR is succinct, PQ and zk at once, and LaZer ships it.
#
#   ./scripts/run_labrador_role_a.sh
#
# ONE-TIME SETUP (LaBRADOR is a git submodule of the vendored LaZer, and is NOT
# fetched by the LaZer clone in the README):
#   cd third_party/lazer
#   git submodule update --init src/labrados
#   make liblabrador38.so
#
# ⚠️ HEADER TRAP: LaZer ships src/labradosNN_py.h declaring the internal ring
# degree N=64, but the submodule it actually builds defines N=256. The struct
# layouts disagree, so a driver built against the shipped header silently
# corrupts memory. ref/relation_zk_labrador.c includes the SUBMODULE's own
# labrados_python.h, and must be compiled with the same -DLOGQ and -DNDEBUG as
# the library (labrados structs have #ifndef NDEBUG members).
#
# ⚠️ RETURN-VALUE TRAP: labrados' simple_verify/verify return 1 on SUCCESS, the
# opposite of the 0-on-success used by the setter functions in the same header.
#
# WHY LOGQ=38: the encoding carries a mod-q quotient g (LaBRADOR works over its
# own prime, not our q). Soundness needs |[A|-A]w| + |q g| + |t'| < p/2; with the
# declared l2 bound on g that sum uses ~78% of LOGQ=38's budget and overflows
# LOGQ=36's. Override with LABRADOR_LOGQ=NN make ... only if you redo that sum.
#
# GATES (the binary exits non-zero if any fails)
#   * the honest instance is checked against the relation in integer arithmetic
#     before anything is encoded;
#   * LaBRADOR's own simple_verify must ACCEPT the encoded statement before any
#     proof is produced -- an encoding check, re-run every repetition;
#   * every produced proof must verify;
#   * untimed warm-up, then >= 5 repetitions.
#
# SCOPE: an experiment. Nothing is wired into the swap; configuration 3 still
# uses the k=1 LNP22 module. Proof size is LaBRADOR's own printed "Estimated
# proof size", NOT a byte-exact packed length like LNP22's prooflen -- the
# function returning that is hidden by -fvisibility=hidden. Never compare the
# two silently.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REF="$REPO/ref"
LOGQ="${LABRADOR_LOGQ:-38}"

if [ ! -f "$REPO/third_party/lazer/liblabrador$LOGQ.so" ]; then
  echo "liblabrador$LOGQ.so not built -- see the one-time setup in this script's header." >&2
  exit 1
fi

RUN_ID="$(date +%Y%m%d_%H%M%S)"
OUT="$REPO/evidence/labrador_role_a/$RUN_ID"
mkdir -p "$OUT"

cd "$REF"
make LABRADOR_LOGQ="$LOGQ" test/bench_labrador_role_a >"$OUT/build.log" 2>&1

{
  echo "run_id=$RUN_ID"
  echo "git=$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo n/a)"
  echo "branch=$(git -C "$REPO" rev-parse --abbrev-ref HEAD 2>/dev/null || echo n/a)"
  echo "host=$(uname -s) $(uname -r) $(uname -m)"
  echo "cpu=$({ grep -m1 'model name' /proc/cpuinfo 2>/dev/null || true; } | sed 's/.*: //')"
  echo "cc=$(${CC:-cc} --version 2>/dev/null | head -1 || echo n/a)"
  echo "labrador_logq=$LOGQ"
  echo "labrador_submodule=$(git -C "$REPO/third_party/lazer" submodule status src/labrados 2>/dev/null | tr -s ' ')"
  echo "sources=ref/relation_zk_labrador.c ref/test/bench_labrador_role_a.c"
  echo "zero_knowledge=ON (LaBRADOR gen_params zk flag = 1)"
  echo "note=proof size is LaBRADOR's own printed estimate, not a byte-exact packed length"
  echo "note=not wired into the swap; configuration 3 still uses the k=1 LNP22 module"
} > "$OUT/environment.txt"

./test/bench_labrador_role_a 2>&1 | tee "$OUT/bench_labrador_role_a.log"

ln -sfn "$RUN_ID" "$REPO/evidence/labrador_role_a/latest"
echo
echo "evidence written: evidence/labrador_role_a/$RUN_ID/{bench_labrador_role_a.log,environment.txt}"
echo "decisive rows: the encoding gate, the proof-verifies gate, and the size/time block."
