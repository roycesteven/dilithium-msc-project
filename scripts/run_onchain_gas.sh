#!/usr/bin/env bash
# Re-measure on-chain settlement gas and capture it as evidence.
#
# Layout mirrors evidence/stage2/ and evidence/stark/: one timestamped directory
# per run holding the tool's own stdout plus an environment record, with `latest`
# pointing at it.
#
#   ./scripts/run_onchain_gas.sh
#
# Order matters. The gas cost of claimLAS/claimLASVerified is dominated by the
# CALLDATA of a real packed signature, so the fixture evm/test/las_sig.bin must be
# re-exported from the C implementation BEFORE forge runs -- otherwise the figure
# reports gas for a signature width the scheme no longer produces. That is exactly
# how this figure went stale: the fixture was exported once by hand, and the FIPS 204
# c_tilde alignment later changed SIGNATURE_BYTES underneath it.
#
# The log is written BY THE TOOL (tee'd verbatim). Never hand-edit it; never type a
# gas number into a plotting script. scripts/plot_onchain_gas.py reads THIS log.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN_ID="$(date +%Y%m%d_%H%M%S)"
OUT="$REPO/evidence/onchain/$RUN_ID"
mkdir -p "$OUT"

# --- 1. re-export the signature fixture from the C implementation of record ----
make -C "$REPO/ref" test/export_packed >/dev/null
"$REPO/ref/test/export_packed" "$REPO/evm/test/las_sig.bin"
SIG_BYTES="$(stat -c%s "$REPO/evm/test/las_sig.bin")"
echo "fixture: evm/test/las_sig.bin = ${SIG_BYTES} B (re-exported from ref/)"

# --- 2. environment record -----------------------------------------------------
{
  echo "run_id=$RUN_ID"
  echo "git=$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo n/a)"
  echo "host=$(uname -s) $(uname -r) $(uname -m)"
  echo "forge=$(forge --version 2>/dev/null | head -1 || echo n/a)"
  echo "solc=$(grep -m1 'solc' "$REPO/evm/foundry.toml" 2>/dev/null | sed 's/.*= *//' || echo default)"
  echo "evm_version=$(grep -m1 'evm_version' "$REPO/evm/foundry.toml" 2>/dev/null | sed 's/.*= *//' || echo default)"
  echo "sig_bytes=$SIG_BYTES"
  echo "note=gas is deterministic for fixed bytecode, EVM revision, inputs and state"
} > "$OUT/environment.txt"

# --- 3. measure ----------------------------------------------------------------
cd "$REPO/evm"
forge test --gas-report 2>&1 | tee "$OUT/gas_report.log"

ln -sfn "$RUN_ID" "$REPO/evidence/onchain/latest"

# --- 4. regenerate the figure FROM the captured log ----------------------------
python3 "$REPO/scripts/plot_onchain_gas.py" \
  --log "$REPO/evidence/onchain/latest/gas_report.log" \
  --out "$REPO/report/latex/figures"

echo
echo "evidence written: evidence/onchain/$RUN_ID/{gas_report.log,environment.txt}"
echo "figure regenerated: report/latex/figures/fig_onchain.pdf"
