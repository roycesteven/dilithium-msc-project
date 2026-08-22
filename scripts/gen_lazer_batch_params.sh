#!/usr/bin/env bash
# Regenerate the BATCHED LaZer parameter sets ref/relation_zk_params_k{2,4,8}.h.
#
# Those headers are COMMITTED, exactly like the k=1 set ref/relation_zk_params.h,
# so SageMath is needed only to REGENERATE them -- never to build or run the
# experiment. Run this only when the batch sizes or the LAS parameter set change.
#
#   ./scripts/gen_lazer_batch_params.sh [k ...]      (default: 2 4 8)
#
# WHAT IT GENERATES
#   For each k, the spec for k INDEPENDENT copies of the Fig. 1 relation laid out
#   block-diagonally: dim = (6k, 23k), with 22k binary witness polynomials and k
#   dummy l2 polynomials (one per block). Each block is exactly the k=1 spec in
#   scripts/las_pi_params.py, so a batched proof is the conjunction of k copies
#   of the deployed statement.
#
#   k=1 is deliberately NOT generated here: the experiment uses the committed
#   las_pi_params, i.e. the set configuration 3 actually ships, so its baseline
#   row is the deployed prover rather than a re-derived lookalike.
#
# REQUIREMENTS
#   SageMath, and the vendored LaZer checkout with its codegen scripts. On this
#   machine sage lives in a micromamba env; override with SAGE=... if yours does
#   not. Codegen took ~30 s at k=2 and a few minutes at k=8.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SAGE="${SAGE:-$HOME/micromamba/envs/lazer-sage/bin/sage}"
CODEGEN_DIR="$REPO/third_party/lazer/scripts"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

KS=("$@")
[ ${#KS[@]} -eq 0 ] && KS=(2 4 8)

[ -x "$SAGE" ] || { echo "sage not found at $SAGE (set SAGE=...)" >&2; exit 1; }
[ -f "$CODEGEN_DIR/lin-codegen.sage" ] || {
  echo "LaZer codegen not found: $CODEGEN_DIR/lin-codegen.sage" >&2
  echo "clone the vendored library first -- see README 'pi / atomic swap'." >&2
  exit 1
}

for k in "${KS[@]}"; do
  spec="$WORK/k$k.py"
  python3 - "$k" "$spec" <<'PY'
import sys
k, out = int(sys.argv[1]), sys.argv[2]
# Block i occupies columns [23i, 23i+23): 22 binary polys (r_plus || r_minus)
# then one dummy l2 poly e bound to the all-zero 23rd column of that block.
binary, dummy = [], []
for i in range(k):
    b = 23 * i
    binary += list(range(b, b + 22))
    dummy.append(b + 22)
with open(out, "w") as f:
    f.write(f'vname = "las_pi_params_k{k}"\n')
    f.write("deg   = 256\n")
    f.write("mod   = 8380417\n")
    f.write(f"dim   = ({6*k}, {23*k})\n")
    f.write(f"wpart = [ {binary}, {dummy} ]\n")
    # honest dummies are all zero, so any positive l2 bound is satisfied; 16 is
    # the bound the committed k=1 spec uses.
    f.write("wl2   = [ 0, 16 ]\n")
    f.write("wbin  = [ 1,  0 ]\n")
    f.write("wrej  = [ 0,  0 ]\n")
    f.write("wlinf = 1\n")
PY

  echo "=== codegen k=$k (dim $((6*k)) x $((23*k))) ==="
  ( cd "$CODEGEN_DIR" && "$SAGE" lin-codegen.sage "$spec" ) > "$WORK/k$k.h"
  grep -A 1 '^// Proof size' "$WORK/k$k.h" | tail -1
  mv "$WORK/k$k.h" "$REPO/ref/relation_zk_params_k$k.h"
  echo "wrote ref/relation_zk_params_k$k.h"
done

echo
echo "Done. Rebuild with: make -C ref test/bench_lazer_amortise"
echo "Adding a NEW k also needs params_for() extended in ref/relation_zk_lazer_batch.c"
echo "and PI_BATCH_MAX_K raised in ref/relation_zk_lazer_batch.h."
