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
# TWO PASSES, and NO cap gate may run in the --gas-report one.
#
# `--gas-report` inflates every in-test gas measurement -- `gasleft()` deltas and
# `vm.lastCallGas()` alike -- because Foundry's inspector is metered inside the measured
# call frame. On the LAS claim that is ~688k gas, MORE than the real headroom under the
# EIP-7825 cap (363,941 gas by the node's own receipt), so a cap assertion measured under
# the flag FAILS FOR A REPORTING REASON while a real client mines the same transaction
# perfectly well. That false red would sit in the evidence log looking like a result.
#
# Foundry's --gas-report TABLE is accurate; only in-test measurement is affected. Untraced,
# the in-test model agrees with the real-node receipt (run_onchain_one_tx.sh) to ~0.2%.
#
# Pass 1  no --gas-report, gate contracts ONLY   -> BOTH cap gates (verifier-level in
#         LASGasBreakdownTest, swap-level in LasVerifiedOptSwapGate) plus the stage
#         attribution, measured clean.
# Pass 2  --gas-report, gate contracts EXCLUDED  -> the per-function table for the figure.
#         LasVerifiedOptSwapGas (report-only twin of the gate) stays in and supplies the
#         claimLASVerifiedOpt row; the gate contracts contribute no rows the figure needs.
# -vv in both so the console.log tables are captured -- they are the only place execution
# and INTRINSIC gas appear together, which --gas-report never shows.
#
# EACH PASS'S EXIT STATUS IS CAPTURED SEPARATELY. Wrapping both in `{ ... } | tee` would
# report only the LAST command's status, so a FAILING CAP GATE IN PASS 1 would be hidden
# behind a green pass 2 and the script would sail on to regenerate the figure and announce
# success — the exact failure mode a gate exists to prevent. `set +e` around each pipeline
# is needed because errexit would otherwise abort before PIPESTATUS could be read, and
# PIPESTATUS must be read IMMEDIATELY (any later command overwrites it).
GATES='LASGasBreakdownTest|LasVerifiedOptSwapGate'
cd "$REPO/evm"
LOG="$OUT/gas_report.log"
: > "$LOG"

echo "=== PASS 1: cap gates + stage attribution (NO --gas-report; see note in this script) ===" | tee -a "$LOG"
set +e
forge test --match-contract "$GATES" -vv 2>&1 | tee -a "$LOG"
RC_GATES=${PIPESTATUS[0]}
set -e

echo | tee -a "$LOG"
echo "=== PASS 2: full suite with --gas-report, cap gates excluded (table for the figure) ===" | tee -a "$LOG"
set +e
forge test --gas-report --no-match-contract "$GATES" -vv 2>&1 | tee -a "$LOG"
RC_TABLE=${PIPESTATUS[0]}
set -e

echo "pass1_gates_exit=$RC_GATES"  >> "$OUT/environment.txt"
echo "pass2_table_exit=$RC_TABLE"  >> "$OUT/environment.txt"

# A failing gate is a RESULT: keep the evidence directory, but do not let it become the
# cited one. `latest` is not moved and the figure is not regenerated, so a failing run can
# never silently replace the numbers the report quotes.
if [ "$RC_GATES" -ne 0 ] || [ "$RC_TABLE" -ne 0 ]; then
  echo
  echo "FAIL: pass 1 (gates) exit $RC_GATES, pass 2 (table) exit $RC_TABLE" >&2
  echo "      evidence kept at evidence/onchain/$RUN_ID/ for inspection;" >&2
  echo "      'latest' was NOT moved and the figure was NOT regenerated." >&2
  exit 1
fi

ln -sfn "$RUN_ID" "$REPO/evidence/onchain/latest"

# --- 4. regenerate the figure FROM the captured log ----------------------------
python3 "$REPO/scripts/plot_onchain_gas.py" \
  --log "$REPO/evidence/onchain/latest/gas_report.log" \
  --out "$REPO/report/latex/figures"

echo
echo "evidence written: evidence/onchain/$RUN_ID/{gas_report.log,environment.txt}"
echo "figure regenerated: report/latex/figures/fig_onchain.pdf"
