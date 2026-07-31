#!/usr/bin/env bash
# Run the Stage-2 UTXO atomic-swap benchmark of record and capture it as evidence.
#
# Layout mirrors evidence/stark/: one timestamped directory per run holding the
# tool's own stdout plus an environment record, with `latest` pointing at it.
#
# The log is written BY THE TOOL (tee'd verbatim). Never hand-edit it: to change a
# number, change the code and re-run. The report macros are then regenerated with
# scripts/gen_stage2_data.py, which reads this log -- so a report figure can never
# be typed by hand.
#
#   ./scripts/run_swap_bench.sh
#
# This benchmark was previously run ad hoc, which is how its evidence drifted out
# of step with the Stage-1 evidence run after the FIPS 204 c_tilde alignment
# changed the signature width. Having a runner keeps the two in step.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CRATE="$REPO/rust/las-swap"
FEATURES="secp256k1,groth16,relation-zk"

RUN_ID="$(date +%Y%m%d_%H%M%S)"
OUT="$REPO/evidence/stage2/$RUN_ID"
mkdir -p "$OUT"

{
  echo "run_id=$RUN_ID"
  echo "git=$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo n/a)"
  echo "host=$(uname -s) $(uname -r) $(uname -m)"
  echo "cpu=$({ grep -m1 'model name' /proc/cpuinfo 2>/dev/null || true; } | sed 's/.*: //')"
  echo "rustc=$(rustc --version 2>/dev/null || echo n/a)"
  echo "features=$FEATURES"
} > "$OUT/environment.txt"

cd "$CRATE"
cargo run --release --quiet --bin bench_swap --features "$FEATURES" \
  2>&1 | tee "$OUT/bench_swap.log"

ln -sfn "$RUN_ID" "$REPO/evidence/stage2/latest"
echo
echo "evidence written: evidence/stage2/$RUN_ID/{bench_swap.log,environment.txt}"
echo "regenerate report macros with:"
echo "  python3 scripts/gen_stage2_data.py \\"
echo "    --log evidence/stage2/latest/bench_swap.log \\"
echo "    --env evidence/stage2/latest/environment.txt \\"
echo "    --out report/latex/generated/stage2macros.tex"
echo
echo "and the Bitcoin wire-format projection of the settled transaction (it reads the"
echo "same log, so it goes stale with the object sizes if it is not re-run):"
echo "  python3 scripts/gen_bitcoin_tx_data.py \\"
echo "    --log evidence/stage2/latest/bench_swap.log \\"
echo "    --out report/latex/generated/btcmacros.tex \\"
echo "    --tab report/latex/generated/tab_btctx.tex"
