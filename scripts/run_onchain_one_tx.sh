#!/usr/bin/env bash
# Prove — against a real Ethereum client, not against our own arithmetic — that full
# on-chain LAS verification settles in ONE transaction under EIP-7825's cap.
#
#   ./scripts/run_onchain_one_tx.sh
#
# WHY THIS EXISTS ALONGSIDE run_onchain_gas.sh. That script runs `forge test`, whose
# totals are a MODEL: `gasleft()` deltas plus an EIP-7623 formula we wrote ourselves.
# A model can be wrong in the same direction twice. Here the node does the accounting:
#
#   * anvil runs at an EXPLICIT hardfork (osaka) with --enable-tx-gas-limit, so EIP-7825
#     is enforced BY THE CLIENT and the EVM revision is pinned rather than "latest",
#     which drifts between Foundry releases and silently changes gas semantics;
#   * THE INSTRUMENT IS VERIFIED BEFORE IT IS TRUSTED (step 3): we spawn our own node on
#     a private port, refuse to start if that port is already bound, confirm the node we
#     are talking to is the process we spawned, and then run a DIFFERENTIAL CONTROL —
#     the same trivial transfer at gasLimit = cap and at gasLimit = cap+1. Only the pair
#     (accepted at the cap, refused one gas over it) attributes the refusal TO THE CAP;
#     a bare non-zero exit code would equally well mean a bad nonce, no funds or a dead
#     RPC, and would give false confidence that the ceiling was being enforced;
#   * the claim is sent by `cast send --gas-limit`, an explicit cap rather than an
#     estimate, so "it fits" cannot be an artefact of a generous gas estimator — and the
#     limit that actually ended up ON the mined transaction is read back and asserted,
#     rather than assumed from the flag we passed;
#   * fund and claim are SEPARATE, MINED transactions, so the claim's escrow reads are
#     COLD (2,100-gas SLOADs), matching this project's gas methodology; funding inside
#     the measured call would have pre-warmed the very slots it then reads;
#   * the calldata the node received is compared BYTE FOR BYTE against what was built
#     from the freshly exported vectors — equal length is not equal bytes — and that
#     expected calldata is RETAINED as evidence so the run stays auditable afterwards;
#   * the evidence is the RECEIPT: status and gasUsed, as reported by the client;
#   * the payout is asserted EXACTLY, so a partial or misdirected transfer fails here.
#
# A CLIENT REJECTION IS A RESULT. If the node refuses the transaction — for exceeding the
# per-transaction cap, or any other reason — this script does NOT abort silently. It
# keeps the client's own error text, anvil's log, the expected calldata and the
# environment, writes a DETAILED FAIL verdict naming every post-condition that was not
# met, and exits non-zero. The negative outcome is reproducible and citable, exactly like
# the positive one.
#
# The logs and receipt are written BY THE TOOLS. Never hand-edit them; never type a gas
# number from here into the report.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN_ID="$(date +%Y%m%d_%H%M%S)"
OUT="$REPO/evidence/onchain_onetx/$RUN_ID"

# EIP-7825, the ceiling the whole exercise is against.
TX_GAS_CAP=16777216
# Pin the EVM revision. EIP-7825 arrived in Osaka; "latest" is a moving target.
HARDFORK="${HARDFORK:-osaka}"
# A private port, NOT anvil's default 8545: a node left running from another session
# would otherwise be measured instead of ours, and it may enforce neither the cap nor
# this hardfork. Overridable, but the occupancy check below applies either way.
PORT="${PORT:-8557}"
RPC="http://127.0.0.1:$PORT"

# anvil's first default account — a well-known test key, not a secret.
PRIVATE_KEY=0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80
export PRIVATE_KEY
export BENEFICIARY=0x70997970C51812dc3A010C7d01b50e0d17dc79C8   # anvil account #1
CONTROL_SINK=0x3C44CdDdB6a900fa2b585dd299e03d12FA4293BC        # anvil account #2

mkdir -p "$OUT"

command -v anvil >/dev/null || { echo "anvil not found (install Foundry)" >&2; exit 1; }
command -v cast  >/dev/null || { echo "cast not found (install Foundry)" >&2;  exit 1; }

# Record a FAIL verdict with everything needed to understand and reproduce it, then stop.
# APPENDS, so a detailed verdict already written by the post-condition checker survives.
fail_out() {
  {
    echo
    echo "RESULT: FAIL — $1"
    echo
    echo "run_id: $RUN_ID"
    echo "This is a recorded negative result, not a crashed run. Retained alongside it:"
    echo "  claim_send.log      the client's own error text, if the send got that far"
    echo "  anvil.log           the node's log"
    echo "  claim.calldata      the exact calldata this run intended to send"
    echo "  cap_control.log     the differential control: was the cap enforced at all?"
    echo "  environment.txt     toolchain, hardfork, flags, vector digests"
  } | tee -a "$OUT/verdict.txt"
  [ -f "$REPO/evm/onetx/claim.calldata" ] && cp "$REPO/evm/onetx/claim.calldata" "$OUT/claim.calldata" || true
  echo
  echo "evidence written (FAILING run): evidence/onchain_onetx/$RUN_ID/" >&2
  exit 1
}

# --- 1. re-export the GOLDEN VECTORS THIS EXPERIMENT ACTUALLY READS ---------------
# `export_verify_vector` writes evm/test/vectors/{pp_normal,t,msg,sig,w_prime}.bin —
# the files script/OneTxClaim.s.sol loads. (`export_packed` writes evm/test/las_sig.bin,
# a DIFFERENT fixture used by the claimLAS floor path; refreshing that one here would
# leave this experiment verifying a stale committed signature while looking refreshed.)
# The exporter self-checks that w_prime.bin reproduces the challenge digest, so a broken
# export fails before anything is measured.
make -C "$REPO/ref" test/export_verify_vector >/dev/null
(cd "$REPO/ref" && ./test/export_verify_vector ../evm/test/vectors) > "$OUT/export_vectors.log" 2>&1

# --- 2. spawn OUR OWN node, on a port we know is free ------------------------------
cd "$REPO/evm"
rm -rf onetx && mkdir -p onetx

if cast block-number --rpc-url "$RPC" >/dev/null 2>&1; then
  echo "something is already serving $RPC — refusing to measure against a node this" >&2
  echo "script did not start (it may not enforce EIP-7825 or run $HARDFORK)." >&2
  echo "Stop it, or re-run with PORT=<free port>." >&2
  exit 1
fi

anvil --hardfork "$HARDFORK" --port "$PORT" --enable-tx-gas-limit > "$OUT/anvil.log" 2>&1 &
ANVIL_PID=$!
trap 'kill "$ANVIL_PID" 2>/dev/null || true' EXIT

# Wait for OUR process: if it died (bad hardfork name, bad flag, port race), say so with
# its log rather than spinning out the full timeout and blaming the network.
UP=0
for _ in $(seq 1 60); do
  if ! kill -0 "$ANVIL_PID" 2>/dev/null; then
    cat "$OUT/anvil.log" >&2
    fail_out "anvil (pid $ANVIL_PID) exited during startup — see anvil.log (is --hardfork $HARDFORK supported by this build?)"
  fi
  cast block-number --rpc-url "$RPC" >/dev/null 2>&1 && { UP=1; break; }
  sleep 0.2
done
[ "$UP" -eq 1 ] || fail_out "anvil did not answer at $RPC within 12s"
kill -0 "$ANVIL_PID" 2>/dev/null || fail_out "anvil exited immediately after answering"

# --- 3. DIFFERENTIAL CONTROL: is the refusal actually caused by the cap? ------------
# One variable changes between the two sends — the gas limit, by exactly 1, across
# EIP-7825's boundary. Everything else (sender, recipient, value, node) is identical, so:
#   accepted at cap  AND  refused at cap+1  ->  the cap is enforced, and THAT is why
#   accepted at both                        ->  the cap is NOT enforced; abort
#   refused at both                         ->  something unrelated is broken (funds,
#                                               nonce, RPC); the control is inconclusive
#                                               and must NOT be read as "cap enforced"
#
# The exit status is taken STRAIGHT off `cast` via `|| rc=$?`. It must not be read with
# `$?` after a redirected group or with PIPESTATUS: in `{ cast ...; echo $?; } >> log`
# the group's status is the trailing `echo`'s, i.e. always 0, which would silently
# report every send as "accepted" and break the control in exactly the place that is
# meant to catch breakage.
control_send() { # $1 = gas limit, $2 = label; returns cast's own exit status
  local rc=0
  echo "--- control [$2] : cast send at gasLimit=$1 ---" >> "$OUT/cap_control.log"
  cast send "$CONTROL_SINK" --value 1 --gas-limit "$1" \
    --private-key "$PRIVATE_KEY" --rpc-url "$RPC" >> "$OUT/cap_control.log" 2>&1 || rc=$?
  echo "cast exit code: $rc" >> "$OUT/cap_control.log"
  return "$rc"
}

OVER=$((TX_GAS_CAP + 1))
RC_AT=0;   control_send "$TX_GAS_CAP" "at cap"  || RC_AT=$?
RC_OVER=0; control_send "$OVER"       "cap + 1" || RC_OVER=$?
{
  echo
  echo "--- differential control summary ---"
  echo "gasLimit=$TX_GAS_CAP (at cap) : cast exit $RC_AT   (expected 0 = accepted)"
  echo "gasLimit=$OVER (cap + 1)      : cast exit $RC_OVER (expected non-zero = refused)"
} >> "$OUT/cap_control.log"

if [ "$RC_AT" -ne 0 ] && [ "$RC_OVER" -ne 0 ]; then
  fail_out "control INCONCLUSIVE: the node refused the trivial transfer at the cap as well as above it, so the refusal above the cap cannot be attributed to EIP-7825 (see cap_control.log)"
fi
if [ "$RC_AT" -eq 0 ] && [ "$RC_OVER" -eq 0 ]; then
  fail_out "the node ACCEPTED a transaction one gas ABOVE the EIP-7825 cap — the cap is NOT being enforced, so no result from this run would mean anything (see cap_control.log)"
fi
if [ "$RC_AT" -ne 0 ]; then
  fail_out "control INCONCLUSIVE: the trivial transfer failed AT the cap (cast exit $RC_AT) — see cap_control.log"
fi
echo "control OK: accepted at gasLimit=$TX_GAS_CAP, refused at $OVER — the refusal is the cap"

# --- 4. environment record ----------------------------------------------------------
# The vector digests pin WHICH signature was verified, so the receipt below cannot later
# be attributed to a different fixture.
{
  echo "run_id=$RUN_ID"
  echo "git=$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo n/a)"
  echo "host=$(uname -s) $(uname -r) $(uname -m)"
  echo "anvil=$(anvil --version 2>/dev/null | head -1)"
  echo "cast=$(cast --version 2>/dev/null | head -1)"
  echo "forge=$(forge --version 2>/dev/null | head -1)"
  echo "hardfork=$HARDFORK"
  echo "rpc=$RPC (private port; anvil spawned by this script, pid $ANVIL_PID)"
  echo "chain_id=$(cast chain-id --rpc-url "$RPC" 2>/dev/null || echo n/a)"
  echo "anvil_flags=--hardfork $HARDFORK --port $PORT --enable-tx-gas-limit"
  echo "cap_enforcement_control=PASSED (accepted at $TX_GAS_CAP, refused at $OVER)"
  echo "tx_gas_cap=$TX_GAS_CAP"
  echo "vectors_exporter=ref/test/export_verify_vector (re-run for this measurement)"
  for f in sig.bin t.bin pp_normal.bin msg.bin; do
    echo "vector_${f%.bin}=sha256:$(sha256sum "$REPO/evm/test/vectors/$f" | cut -c1-16)... $(stat -c%s "$REPO/evm/test/vectors/$f") B"
  done
  echo "note=gas is deterministic for fixed bytecode, EVM revision, inputs and state"
} > "$OUT/environment.txt"

# --- 5. tx 1 + tx 2: deploy and fund (separate, mined transactions) -----------------
if ! forge script script/OneTxClaim.s.sol:OneTxClaim \
      --rpc-url "$RPC" --broadcast --private-key "$PRIVATE_KEY" \
      > "$OUT/setup.log" 2>&1; then
  cat "$OUT/setup.log" >&2
  fail_out "deploy/fund stage failed before the claim was attempted (see setup.log)"
fi
cat "$OUT/setup.log"

SWAP_ADDR="$(cat onetx/swap.addr)"
BENEF="$(cat onetx/beneficiary.addr)"
CLAIM_LEN="$(cat onetx/claim.calldata.len)"
COIN_WEI="$(cat onetx/coin.wei)"
BAL_BEFORE="$(cast balance "$BENEF" --rpc-url "$RPC")"

# Retain the expected calldata BEFORE sending, on every path: it is what makes the
# byte-equality check below auditable after the fact rather than only inside this run.
cp onetx/claim.calldata "$OUT/claim.calldata"
echo "expected_calldata=sha256:$(sha256sum "$OUT/claim.calldata" | cut -c1-16)... $CLAIM_LEN B" \
  >> "$OUT/environment.txt"

# --- 6. tx 3: THE claim, with an explicitly capped gas limit -------------------------
# If verification does not fit, the failure happens HERE — either the transaction reverts
# or the node refuses it for exceeding the limit. Either way the client's own words are
# preserved rather than lost to `set -e`.
SEND_RC=0
SEND_JSON="$(cast send "$SWAP_ADDR" \
  --data "$(cat onetx/claim.calldata)" \
  --gas-limit "$TX_GAS_CAP" \
  --private-key "$PRIVATE_KEY" --rpc-url "$RPC" --json 2> "$OUT/claim_send.log")" || SEND_RC=$?
printf '%s\n' "$SEND_JSON" >> "$OUT/claim_send.log"
if [ "$SEND_RC" -ne 0 ]; then
  cat "$OUT/claim_send.log" >&2
  fail_out "the client REJECTED the claim transaction at gasLimit=$TX_GAS_CAP (cast exit $SEND_RC). The control above showed a trivial transfer IS accepted at this same limit, so the refusal is about this transaction, not the limit itself."
fi

TX_HASH="$(printf '%s' "$SEND_JSON" | python3 -c 'import json,sys; print(json.load(sys.stdin)["transactionHash"])')"

# BOTH halves of the record: the receipt (status, gasUsed) and the transaction itself
# (the gasLimit actually mined, and the calldata the node actually received).
cast receipt "$TX_HASH" --rpc-url "$RPC" --json > "$OUT/claim_receipt.json"
cast tx      "$TX_HASH" --rpc-url "$RPC" --json > "$OUT/claim_tx.json"
BAL_AFTER="$(cast balance "$BENEF" --rpc-url "$RPC")"

# --- 7. verdict, computed FROM the receipt, the transaction and the retained calldata --
# Every post-condition failure is printed to STDOUT so `tee` captures it into
# verdict.txt: a verdict that says only "FAIL" is not evidence of anything.
# (Here PIPESTATUS is the right tool — this genuinely IS a pipeline, and [0] is python.)
set +o pipefail
python3 - "$OUT/claim_receipt.json" "$OUT/claim_tx.json" "$TX_GAS_CAP" "$CLAIM_LEN" \
         "$BAL_BEFORE" "$BAL_AFTER" "$COIN_WEI" "$OUT/claim.calldata" \
         <<'PY' | tee "$OUT/verdict.txt"
import json, sys

(receipt_p, tx_p, cap, cdlen, bal_before, bal_after, coin, sent_p) = (
    sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]),
    int(sys.argv[5]), int(sys.argv[6]), int(sys.argv[7]), sys.argv[8])

def num(v):
    s = str(v)
    return int(s, 16) if s.startswith("0x") else int(s)

def hexbytes(s):
    s = s.strip().lower()
    return bytes.fromhex(s[2:] if s.startswith("0x") else s)

r  = json.load(open(receipt_p))
tx = json.load(open(tx_p))

status    = num(r["status"])
gas_used  = num(r["gasUsed"])
# The limit that was actually ON the mined transaction -- not the flag we passed.
gas_limit = num(tx.get("gas", tx.get("gasLimit")))
received  = hexbytes(tx["input"])          # calldata the NODE got
sent      = hexbytes(open(sent_p).read())  # calldata we BUILT from the fresh vectors
paid      = bal_after - bal_before

print("claim transaction    :", r["transactionHash"])
print("status               :", "SUCCESS" if status == 1 else "FAILED")
print("gasLimit ON THE TX   :", gas_limit)
print("EIP-7825 cap         :", cap)
print("gasUsed (by client)  :", gas_used)
print("headroom             :", cap - gas_used, "gas  (%.1f%% of the cap used)" % (100.0 * gas_used / cap))
print("calldata bytes       :", len(received), "received /", len(sent), "expected /", cdlen, "declared")
print("calldata byte-equal  :", "YES" if received == sent else "NO")
print("beneficiary paid     :", paid, "wei (escrow was %d)" % coin)

fail = []
if status != 1:
    fail.append("the claim transaction reverted (receipt status 0)")
if gas_limit > cap:
    fail.append("the mined transaction's gasLimit (%d) exceeds the EIP-7825 cap (%d)" % (gas_limit, cap))
if gas_used >= cap:
    fail.append("gasUsed (%d) is not under the EIP-7825 cap (%d)" % (gas_used, cap))
if received != sent:
    # Byte equality, not length equality: this is what shows the node verified the
    # signature and parameters built from THIS run's freshly exported vectors.
    n = min(len(received), len(sent))
    first = next((i for i in range(n) if received[i] != sent[i]), n)
    fail.append("calldata the node received differs from the retained expected calldata "
                "(lengths %d vs %d, first differing byte at offset %d)"
                % (len(received), len(sent), first))
if len(sent) != cdlen:
    fail.append("retained calldata is %d bytes, script declared %d" % (len(sent), cdlen))
if paid != coin:
    fail.append("payout was %d wei, expected exactly %d" % (paid, coin))

if fail:
    print()
    print("RESULT: FAIL — %d post-condition(s) not met:" % len(fail))
    for i, f in enumerate(fail, 1):
        print("  %d. %s" % (i, f))
    sys.exit(1)

print()
print("RESULT: full on-chain LAS verification settled the swap in ONE transaction,")
print("        mined by the client under a gasLimit at or below EIP-7825's cap,")
print("        over byte-identical calldata, paying the exact escrowed amount.")
PY
VERDICT_RC=${PIPESTATUS[0]}
set -o pipefail
[ "$VERDICT_RC" -eq 0 ] || fail_out "post-conditions not met — see the enumerated list in verdict.txt"

ln -sfn "$RUN_ID" "$REPO/evidence/onchain_onetx/latest"
rm -rf "$REPO/evm/onetx"

echo
echo "evidence written: evidence/onchain_onetx/$RUN_ID/"
echo "  verdict.txt         — pass/fail computed FROM the receipt, tx and calldata"
echo "  claim_receipt.json  — the client's own receipt (status, gasUsed)"
echo "  claim_tx.json       — the mined transaction (gasLimit actually used, calldata)"
echo "  claim.calldata      — the expected calldata, retained so the byte-equality"
echo "                        check above can be re-verified independently"
echo "  cap_control.log     — the differential control that attributes refusal to the cap"
echo "  claim_send.log      — the client's own words on the send"
echo "  setup.log           — deploy + fund transactions"
echo "  export_vectors.log  — the fresh golden-vector export this run verified"
echo "  environment.txt     — toolchain, hardfork, flags, vector + calldata digests"
