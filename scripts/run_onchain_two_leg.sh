#!/usr/bin/env bash
# Settle a WHOLE two-leg atomic swap (eprint 2020/845 Fig. 1) against real Ethereum
# clients — two of them, on two chains.
#
#   ./scripts/run_onchain_two_leg.sh
#
# WHY THIS EXISTS ALONGSIDE run_onchain_one_tx.sh. That script proves one claim fits in one
# transaction. It does not prove a SWAP. Three things separate the two claims:
#
#   * TWO CHAINS, TWO NODES. Fig. 1 exchanges two different coins on two different chains.
#     Two escrows on one chain would settle without ever exercising the property the
#     protocol exists for — that u2 learns the witness only from what u1 published on the
#     OTHER chain. So this runs two anvil instances with DIFFERENT CHAIN IDS.
#   * THE WITNESS MUST COME FROM THE LEDGER. After u1 settles leg B, this script pulls the
#     adapted signature back out of the MINED transaction, slicing it by DECODING the ABI
#     head (never a constant offset), and hands those bytes — and only those — to
#     `extract_and_adapt`, a separate program with no access to the local copy.
#   * THE SIGNATURES MUST BIND THEIR LEGS. `AdaptorSwap`'s verified paths sign a blob the
#     funder chose, naming neither chain, contract, escrow, beneficiary nor amount: two
#     legs settled that way evidence two valid LAS signatures, not a swap. This run uses
#     `AdaptorSwapBound`, whose message is DERIVED from escrow state and read here FROM THE
#     NODE, plus a live replay control (below).
#
# HOW THE REPLAY CONTROL AVOIDS PASSING FOR THE WRONG REASON. A non-zero `cast` exit proves
# nothing on its own — a bad nonce, a dead RPC or a tool error would produce one too. So it
# is established twice over: an `eth_call` must revert with the CONTRACT'S OWN reason string
# (`message not bound`), which attributes the refusal to message binding rather than to
# transport; and a REAL BROADCAST is then attempted, after which the escrow's balance must
# be unchanged. That second check holds however the send failed — refused before broadcast,
# or mined with status 0 — because had the replay worked the escrow would be empty.
#
# HOW THE PAYOUT CHECK AVOIDS BEING WRONG BY THE GAS FEE. In Fig. 1 each claimant is also
# the beneficiary of the leg it claims, so it pays the fee for its own payout. A raw
# balance delta therefore understates the payout by exactly the fee, and the assertion is
# made on `delta + gasUsed × effectiveGasPrice`, both read from the receipt.
#
# A CLIENT REJECTION IS A RESULT. Any refusal keeps the client's own error text, both node
# logs, the retained calldata and the environment, writes a detailed FAIL verdict naming
# every unmet post-condition, and exits non-zero. `latest` is not moved on a failing run.
#
# Logs and receipts are written BY THE TOOLS. Never hand-edit them; never type a number
# from here into the report.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN_ID="$(date +%Y%m%d_%H%M%S)"
OUT="$REPO/evidence/onchain_twoleg/$RUN_ID"

TX_GAS_CAP=16777216
HARDFORK="${HARDFORK:-osaka}"
# Private ports, and NOT the one run_onchain_one_tx.sh uses, so the two experiments can
# never end up measuring each other's node.
PORT1="${PORT1:-8560}"
PORT2="${PORT2:-8561}"
RPC1="http://127.0.0.1:$PORT1"
RPC2="http://127.0.0.1:$PORT2"
# DIFFERENT chain ids: this is what makes the cross-chain replay control meaningful.
CHAINID1="${CHAINID1:-31337}"
CHAINID2="${CHAINID2:-31338}"

# anvil's first default accounts — well-known test keys, not secrets.
# u1 owns coin c1 on chain 1; u2 owns coin c2 on chain 2.
U1_KEY=0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80
U1_ADDR=0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266
U2_KEY=0x59c6995e998f97a5a0044966f0945389dc9e86dae88c7a8412f4603b6b78690d
U2_ADDR=0x70997970C51812dc3A010C7d01b50e0d17dc79C8
CONTROL_SINK=0x3C44CdDdB6a900fa2b585dd299e03d12FA4293BC

COIN_WEI=1000000000000000000   # 1 ether per leg
# 2020/845 Sec 4.1: the leg claimed FIRST (leg B, which reveals y) carries the SHORTER
# timeout, so the reacting party still has a window to extract and claim leg A.
TIMEOUT_A=172800   # 48 h  (t1, claimed second)
TIMEOUT_B=86400    # 24 h  (t2, claimed first)

VEC="$REPO/evm/test/vectors/swap"
PI="${PI:-1}"

mkdir -p "$OUT"
command -v anvil >/dev/null || { echo "anvil not found (install Foundry)" >&2; exit 1; }
command -v cast  >/dev/null || { echo "cast not found (install Foundry)" >&2;  exit 1; }

ANVIL1_PID=""; ANVIL2_PID=""
cleanup() {
  [ -n "$ANVIL1_PID" ] && kill "$ANVIL1_PID" 2>/dev/null || true
  [ -n "$ANVIL2_PID" ] && kill "$ANVIL2_PID" 2>/dev/null || true
}
trap cleanup EXIT

# APPENDS, so a detailed verdict already written by the post-condition checker survives.
fail_out() {
  {
    echo
    echo "RESULT: FAIL — $1"
    echo
    echo "run_id: $RUN_ID"
    echo "This is a recorded negative result, not a crashed run. Retained alongside it:"
    echo "  anvil1.log / anvil2.log      both nodes' logs"
    echo "  leg{A,B}_send.log            the client's own error text, where a send was reached"
    echo "  leg{A,B}.calldata            exactly what this run intended to send"
    echo "  replay_call.log              the replay control's revert reason, if it got that far"
    echo "  replay_send.log              the replay control's real broadcast attempt"
    echo "  cap_control.log              the differential control: was the cap enforced at all?"
    echo "  environment.txt              toolchain, hardfork, chain ids, flags, vector digests"
  } | tee -a "$OUT/verdict.txt"
  echo
  echo "evidence written (FAILING run): evidence/onchain_twoleg/$RUN_ID/" >&2
  exit 1
}

hex_to_bin() { # $1 = 0x-prefixed hex, $2 = destination file
  python3 -c 'import sys;h=sys.argv[1].strip();h=h[2:] if h.startswith("0x") else h;open(sys.argv[2],"wb").write(bytes.fromhex(h))' "$1" "$2"
}

# --- 1. build the off-chain tooling, and the swap objects ---------------------------
# The pi-enabled binary is the default: Fig. 1's proof of knowledge is a step of the
# protocol, not an optional extra, so running without it must be deliberate and recorded.
rm -rf "$VEC" && mkdir -p "$VEC"
if [ "$PI" = "1" ]; then
  SWAP_TOOL=test/export_swap_vectors_pi
  SETUP_ARGS=""
  if ! make -C "$REPO/ref" "$SWAP_TOOL" > "$OUT/build.log" 2>&1; then
    cat "$OUT/build.log" >&2
    fail_out "could not build $SWAP_TOOL — the vendored LaZer library is required for Fig. 1's proof of knowledge (see README). Re-run with PI=0 to record deliberately that this run omits pi."
  fi
else
  SWAP_TOOL=test/export_swap_vectors
  SETUP_ARGS="--no-pi"
  make -C "$REPO/ref" "$SWAP_TOOL" > "$OUT/build.log" 2>&1 || {
    cat "$OUT/build.log" >&2; fail_out "could not build $SWAP_TOOL"; }
fi
make -C "$REPO/ref" test/extract_and_adapt >> "$OUT/build.log" 2>&1 || {
  cat "$OUT/build.log" >&2; fail_out "could not build test/extract_and_adapt"; }

# Gen + pi, and u2's verification of pi. A pi that does not verify aborts Fig. 1 here.
if ! (cd "$REPO/ref" && "./$SWAP_TOOL" setup "$VEC" $SETUP_ARGS) > "$OUT/setup_vectors.log" 2>&1; then
  cat "$OUT/setup_vectors.log" >&2
  fail_out "swap setup failed (Gen or pi) — see setup_vectors.log"
fi
cat "$OUT/setup_vectors.log"

# --- 2. spawn OUR OWN nodes, on ports we know are free -------------------------------
cd "$REPO/evm"
rm -rf twoleg && mkdir -p twoleg/chain1 twoleg/chain2

for rpc in "$RPC1" "$RPC2"; do
  if cast block-number --rpc-url "$rpc" >/dev/null 2>&1; then
    echo "something is already serving $rpc — refusing to measure against a node this" >&2
    echo "script did not start. Stop it, or re-run with PORT1=/PORT2=<free ports>." >&2
    exit 1
  fi
done

anvil --hardfork "$HARDFORK" --port "$PORT1" --chain-id "$CHAINID1" --enable-tx-gas-limit \
  > "$OUT/anvil1.log" 2>&1 &
ANVIL1_PID=$!
anvil --hardfork "$HARDFORK" --port "$PORT2" --chain-id "$CHAINID2" --enable-tx-gas-limit \
  > "$OUT/anvil2.log" 2>&1 &
ANVIL2_PID=$!

wait_up() { # $1 = rpc, $2 = pid, $3 = log
  for _ in $(seq 1 60); do
    if ! kill -0 "$2" 2>/dev/null; then
      cat "$3" >&2
      fail_out "anvil (pid $2) exited during startup — see $(basename "$3") (is --hardfork $HARDFORK supported by this build?)"
    fi
    cast block-number --rpc-url "$1" >/dev/null 2>&1 && return 0
    sleep 0.2
  done
  fail_out "anvil did not answer at $1 within 12s"
}
wait_up "$RPC1" "$ANVIL1_PID" "$OUT/anvil1.log"
wait_up "$RPC2" "$ANVIL2_PID" "$OUT/anvil2.log"

# The chain ids must be what we asked for AND must differ, or the replay control below
# proves nothing.
GOT1="$(cast chain-id --rpc-url "$RPC1")"
GOT2="$(cast chain-id --rpc-url "$RPC2")"
[ "$GOT1" = "$CHAINID1" ] || fail_out "chain 1 reports chain id $GOT1, expected $CHAINID1"
[ "$GOT2" = "$CHAINID2" ] || fail_out "chain 2 reports chain id $GOT2, expected $CHAINID2"
[ "$GOT1" != "$GOT2" ] || fail_out "both nodes report chain id $GOT1 — this is not a cross-chain swap"

# --- 3. DIFFERENTIAL CONTROL on both nodes: is the cap actually enforced? -------------
# One variable changes between the two sends — the gas limit, by exactly 1, across
# EIP-7825's boundary. The exit status is taken STRAIGHT off `cast` via `|| rc=$?`; read
# with `$?` after a redirected group it would be the trailing command's, and every send
# would look accepted.
control_send() { # $1 = gas limit, $2 = label, $3 = rpc, $4 = key
  local rc=0
  echo "--- control [$2] : cast send at gasLimit=$1 on $3 ---" >> "$OUT/cap_control.log"
  cast send "$CONTROL_SINK" --value 1 --gas-limit "$1" \
    --private-key "$4" --rpc-url "$3" >> "$OUT/cap_control.log" 2>&1 || rc=$?
  echo "cast exit code: $rc" >> "$OUT/cap_control.log"
  return "$rc"
}
OVER=$((TX_GAS_CAP + 1))
control_pair() { # $1 = rpc, $2 = key, $3 = label
  local rc_at=0 rc_over=0
  control_send "$TX_GAS_CAP" "$3 at cap"  "$1" "$2" || rc_at=$?
  control_send "$OVER"       "$3 cap + 1" "$1" "$2" || rc_over=$?
  {
    echo
    echo "--- differential control summary [$3] ---"
    echo "gasLimit=$TX_GAS_CAP (at cap) : cast exit $rc_at   (expected 0 = accepted)"
    echo "gasLimit=$OVER (cap + 1)      : cast exit $rc_over (expected non-zero = refused)"
  } >> "$OUT/cap_control.log"
  if [ "$rc_at" -ne 0 ] && [ "$rc_over" -ne 0 ]; then
    fail_out "control INCONCLUSIVE on $3: the node refused the trivial transfer at the cap as well as above it, so a refusal above the cap cannot be attributed to EIP-7825 (see cap_control.log)"
  fi
  if [ "$rc_at" -eq 0 ] && [ "$rc_over" -eq 0 ]; then
    fail_out "$3 ACCEPTED a transaction one gas ABOVE the EIP-7825 cap — the cap is NOT being enforced, so no result from this run would mean anything (see cap_control.log)"
  fi
  if [ "$rc_at" -ne 0 ]; then
    fail_out "control INCONCLUSIVE on $3: the trivial transfer failed AT the cap (cast exit $rc_at) — see cap_control.log"
  fi
}
control_pair "$RPC1" "$U1_KEY" "chain1"
control_pair "$RPC2" "$U2_KEY" "chain2"
echo "control OK on both nodes: accepted at gasLimit=$TX_GAS_CAP, refused at $OVER"

# --- 4. environment record ------------------------------------------------------------
{
  echo "run_id=$RUN_ID"
  echo "git=$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo n/a)"
  echo "branch=$(git -C "$REPO" rev-parse --abbrev-ref HEAD 2>/dev/null || echo n/a)"
  echo "host=$(uname -s) $(uname -r) $(uname -m)"
  echo "anvil=$(anvil --version 2>/dev/null | head -1)"
  echo "cast=$(cast --version 2>/dev/null | head -1)"
  echo "forge=$(forge --version 2>/dev/null | head -1)"
  echo "cc=$(${CC:-cc} --version 2>/dev/null | head -1)"
  echo "hardfork=$HARDFORK"
  echo "chain1=$RPC1 chain_id=$GOT1 (anvil pid $ANVIL1_PID)"
  echo "chain2=$RPC2 chain_id=$GOT2 (anvil pid $ANVIL2_PID)"
  echo "anvil_flags=--hardfork $HARDFORK --chain-id <id> --enable-tx-gas-limit"
  echo "cap_enforcement_control=PASSED on both nodes (accepted at $TX_GAS_CAP, refused at $OVER)"
  echo "tx_gas_cap=$TX_GAS_CAP"
  echo "settlement_contract=AdaptorSwapBound (single claim path; message DERIVED from escrow state)"
  echo "swap_tool=ref/$SWAP_TOOL"
  echo "pi=$([ "$PI" = "1" ] && echo "proved and verified (Fig. 1 steps 2-3)" || echo "OMITTED by PI=0 — NOT a full Fig. 1 execution")"
  echo "coin_wei_per_leg=$COIN_WEI"
  echo "timeout_legA_secs=$TIMEOUT_A (t1, claimed second)"
  echo "timeout_legB_secs=$TIMEOUT_B (t2, claimed first — the shorter one, per 2020/845 Sec 4.1)"
  for f in pp_normal.bin t1.bin t2.bin Y.bin; do
    echo "vector_${f%.bin}=sha256:$(sha256sum "$VEC/$f" | cut -c1-16)... $(stat -c%s "$VEC/$f") B"
  done
  [ -f "$VEC/pi.bin" ] && echo "vector_pi=sha256:$(sha256sum "$VEC/pi.bin" | cut -c1-16)... $(stat -c%s "$VEC/pi.bin") B"
  echo "note=gas is deterministic for fixed bytecode, EVM revision, inputs and state"
} > "$OUT/environment.txt"

# --- 5. fund both legs, each on its own chain -----------------------------------------
# Leg A: u1 escrows c1 on chain 1 for u2, pre-signed under u1's key (t1).
# Leg B: u2 escrows c2 on chain 2 for u1, pre-signed under u2's key (t2).
fund_leg() { # $1 rpc, $2 payer key, $3 beneficiary, $4 t file, $5 outdir, $6 timeout, $7 log
  PRIVATE_KEY="$2" BENEFICIARY="$3" VEC_DIR="test/vectors/swap" T_FILE="$4" \
  OUT_DIR="$5" COIN_WEI="$COIN_WEI" TIMEOUT_SECS="$6" \
  forge script script/TwoLeg.s.sol:TwoLegFund --rpc-url "$1" --broadcast --private-key "$2" \
    > "$7" 2>&1
}
fund_leg "$RPC1" "$U1_KEY" "$U2_ADDR" t1.bin twoleg/chain1 "$TIMEOUT_A" "$OUT/fund_legA.log" \
  || { cat "$OUT/fund_legA.log" >&2; fail_out "funding leg A on chain 1 failed"; }
fund_leg "$RPC2" "$U2_KEY" "$U1_ADDR" t2.bin twoleg/chain2 "$TIMEOUT_B" "$OUT/fund_legB.log" \
  || { cat "$OUT/fund_legB.log" >&2; fail_out "funding leg B on chain 2 failed"; }

SWAP_A="$(cat twoleg/chain1/swap.addr)";  ID_A="$(cat twoleg/chain1/escrow.id)"
SWAP_B="$(cat twoleg/chain2/swap.addr)";  ID_B="$(cat twoleg/chain2/escrow.id)"

# --- 6. read each leg's message FROM ITS NODE ------------------------------------------
# The digest the parties sign comes from the chain, not from this script: `legMessage` is
# what `claimBound` recomputes and compares against. The value the funding script wrote is
# used only as a cross-check that the two agree.
MSG_A="$(cast call "$SWAP_A" "legMessage(uint256)(bytes32)" "$ID_A" --rpc-url "$RPC1")"
MSG_B="$(cast call "$SWAP_B" "legMessage(uint256)(bytes32)" "$ID_B" --rpc-url "$RPC2")"
EXP_A="$(cat twoleg/chain1/legmsg.expected)"
EXP_B="$(cat twoleg/chain2/legmsg.expected)"
[ "${MSG_A,,}" = "${EXP_A,,}" ] || fail_out "leg A: the node's legMessage ($MSG_A) differs from what the funding script derived ($EXP_A)"
[ "${MSG_B,,}" = "${EXP_B,,}" ] || fail_out "leg B: the node's legMessage ($MSG_B) differs from what the funding script derived ($EXP_B)"
[ "${MSG_A,,}" != "${MSG_B,,}" ] || fail_out "both legs derive the SAME message — one signature would settle both, so this is not a binding swap"

hex_to_bin "$MSG_A" "$VEC/legA_msg.bin"
hex_to_bin "$MSG_B" "$VEC/legB_msg.bin"
{
  echo "legA: chain_id=$GOT1 contract=$SWAP_A id=$ID_A message=$MSG_A"
  echo "legB: chain_id=$GOT2 contract=$SWAP_B id=$ID_B message=$MSG_B"
} | tee "$OUT/leg_messages.txt"

# --- 7. PreSign both legs, PreVerify both (Fig. 1 steps 4-5) ---------------------------
if ! (cd "$REPO/ref" && "./$SWAP_TOOL" presign "$VEC") > "$OUT/presign.log" 2>&1; then
  cat "$OUT/presign.log" >&2
  fail_out "PreSign/PreVerify failed — see presign.log (a PreVerify failure aborts Fig. 1 before any Adapt)"
fi
cat "$OUT/presign.log"

# --- 8. u1 adapts sigma_hat_2 and settles leg B on chain 2 -----------------------------
if ! (cd "$REPO/ref" && "./$SWAP_TOOL" adapt "$VEC") > "$OUT/adapt.log" 2>&1; then
  cat "$OUT/adapt.log" >&2
  fail_out "Adapt (u1: sigma_hat_2 -> sigma_2) failed — see adapt.log"
fi
cat "$OUT/adapt.log"

# Building calldata is purely local — no chain is read and nothing is broadcast — so this
# deliberately runs WITHOUT --rpc-url. Pointing it at one node while assembling the other
# node's claim would only invite the reader to think chain state was involved.
build_claim() { # $1 outdir, $2 sig path, $3 escrow id, $4 leg message, $5 log
  OUT_DIR="$1" SIG_PATH="$2" ESCROW_ID="$3" LEG_MESSAGE="$4" \
  forge script script/TwoLeg.s.sol:TwoLegClaim > "$5" 2>&1
}
build_claim twoleg/chain2 test/vectors/swap/sigma2.bin "$ID_B" "$MSG_B" "$OUT/build_legB.log" \
  || { cat "$OUT/build_legB.log" >&2; fail_out "building leg B claim calldata failed"; }
cp twoleg/chain2/claim.calldata "$OUT/legB.calldata"

# u1 is leg B's beneficiary AND the sender, so it pays the fee for its own payout; the
# balance is snapshotted before the send and the fee added back from the receipt.
BAL_U1_BEFORE="$(cast balance "$U1_ADDR" --rpc-url "$RPC2")"
SEND_RC=0
SEND_B="$(cast send "$SWAP_B" --data "$(cat twoleg/chain2/claim.calldata)" \
  --gas-limit "$TX_GAS_CAP" --private-key "$U1_KEY" --rpc-url "$RPC2" --json \
  2> "$OUT/legB_send.log")" || SEND_RC=$?
printf '%s\n' "$SEND_B" >> "$OUT/legB_send.log"
[ "$SEND_RC" -eq 0 ] || { cat "$OUT/legB_send.log" >&2; fail_out "the client REJECTED leg B's claim (cast exit $SEND_RC). The control showed a trivial transfer IS accepted at this same limit, so the refusal is about this transaction."; }

TX_B="$(printf '%s' "$SEND_B" | python3 -c 'import json,sys; print(json.load(sys.stdin)["transactionHash"])')"
cast receipt "$TX_B" --rpc-url "$RPC2" --json > "$OUT/legB_receipt.json"
cast tx      "$TX_B" --rpc-url "$RPC2" --json > "$OUT/legB_tx.json"
BAL_U1_AFTER="$(cast balance "$U1_ADDR" --rpc-url "$RPC2")"

# --- 9. recover sigma_2 FROM THE MINED TRANSACTION -------------------------------------
# The offset is DECODED from the ABI head (argument 1's offset word, then its length word),
# never a constant: a hardcoded offset would keep pointing somewhere if the argument order
# changed, and would slice the wrong field in silence.
python3 - "$OUT/legB_tx.json" "$VEC/sigma2_from_chain.bin" <<'PY'
import json, sys
tx = json.load(open(sys.argv[1]))
raw = tx["input"]
b = bytes.fromhex(raw[2:] if raw.startswith("0x") else raw)
off = int.from_bytes(b[4 + 32:4 + 64], "big")      # head slot 1 = offset of `sigPacked`
start = 4 + off
ln = int.from_bytes(b[start:start + 32], "big")
open(sys.argv[2], "wb").write(b[start + 32:start + 32 + ln])
print("sliced %d signature bytes out of the mined transaction" % ln)
PY

# --- 10. u2: Ext from the CHAIN's bytes, then Adapt leg A -------------------------------
# extract_and_adapt takes the observed signature as a path argument and cannot reach
# sigma2.bin — u2's knowledge really is limited to what the ledger published.
if ! (cd "$REPO/ref" && ./test/extract_and_adapt "$VEC" "$VEC/sigma2_from_chain.bin") \
      > "$OUT/extract_adapt.log" 2>&1; then
  cat "$OUT/extract_adapt.log" >&2
  fail_out "Ext/Adapt from the chain-observed signature failed — see extract_adapt.log"
fi
cat "$OUT/extract_adapt.log"

# --- 11. LIVE replay control, before the legitimate claim -------------------------------
# Leg A's own claim, rebuilt with LEG B's message. Run BEFORE the real claim so a success
# here cannot be masked by the escrow already being closed.
#
# Established two ways, because a non-zero `cast` exit alone would also be produced by a
# bad nonce, a dead RPC or a tool error, and would give false confidence:
#   (a) WHY it is refused — `eth_call` must revert with the CONTRACT'S OWN reason string,
#       `message not bound`. That attributes the refusal to message binding specifically,
#       not to transport or tooling;
#   (b) THAT a real transaction cannot settle it — an actual broadcast is attempted, and
#       the escrow's balance must be UNCHANGED afterwards. This one holds however the send
#       failed (refused pre-broadcast, or mined with status 0): had the replay worked, the
#       escrow would be empty.
build_claim twoleg/chain1 test/vectors/swap/sigma1.bin "$ID_A" "$MSG_B" "$OUT/build_replay.log" \
  || { cat "$OUT/build_replay.log" >&2; fail_out "building the replay-control calldata failed"; }
cp twoleg/chain1/claim.calldata "$OUT/replay.calldata"

ESCROW_A_BEFORE_REPLAY="$(cast balance "$SWAP_A" --rpc-url "$RPC1")"

# (a) the contract's own reason, via a state-free call
REPLAY_RC=0
cast call "$SWAP_A" --data "$(cat twoleg/chain1/claim.calldata)" --from "$U2_ADDR" \
  --rpc-url "$RPC1" > "$OUT/replay_call.log" 2>&1 || REPLAY_RC=$?
echo "cast exit code: $REPLAY_RC" >> "$OUT/replay_call.log"
REPLAY_REASON_OK=0
grep -qi "message not bound" "$OUT/replay_call.log" && REPLAY_REASON_OK=1
[ "$REPLAY_RC" -ne 0 ] || fail_out "REPLAY ACCEPTED: leg A's claim carrying leg B's message did not revert under eth_call. The signatures do not bind their legs, so two payouts would not evidence a swap (see replay_call.log)"
[ "$REPLAY_REASON_OK" -eq 1 ] || fail_out "the replay call failed, but NOT with the contract's 'message not bound' reason — the refusal cannot be attributed to message binding rather than to a transport or tooling error (see replay_call.log)"

# (b) a real broadcast, which must leave the escrow untouched however it fails
REPLAY_SEND_RC=0
cast send "$SWAP_A" --data "$(cat twoleg/chain1/claim.calldata)" \
  --gas-limit "$TX_GAS_CAP" --private-key "$U2_KEY" --rpc-url "$RPC1" \
  > "$OUT/replay_send.log" 2>&1 || REPLAY_SEND_RC=$?
echo "cast exit code: $REPLAY_SEND_RC" >> "$OUT/replay_send.log"
ESCROW_A_AFTER_REPLAY="$(cast balance "$SWAP_A" --rpc-url "$RPC1")"
[ "$ESCROW_A_BEFORE_REPLAY" = "$ESCROW_A_AFTER_REPLAY" ] || fail_out "THE REPLAY SETTLED THE ESCROW: leg A's balance changed across a claim carrying leg B's message ($ESCROW_A_BEFORE_REPLAY -> $ESCROW_A_AFTER_REPLAY)"
echo "replay control OK: refused with the contract's own reason; a real send left the escrow untouched"

# --- 12. u2 settles leg A on chain 1 ----------------------------------------------------
build_claim twoleg/chain1 test/vectors/swap/sigma1.bin "$ID_A" "$MSG_A" "$OUT/build_legA.log" \
  || { cat "$OUT/build_legA.log" >&2; fail_out "building leg A claim calldata failed"; }
cp twoleg/chain1/claim.calldata "$OUT/legA.calldata"

BAL_U2_BEFORE="$(cast balance "$U2_ADDR" --rpc-url "$RPC1")"
SEND_RC=0
SEND_A="$(cast send "$SWAP_A" --data "$(cat twoleg/chain1/claim.calldata)" \
  --gas-limit "$TX_GAS_CAP" --private-key "$U2_KEY" --rpc-url "$RPC1" --json \
  2> "$OUT/legA_send.log")" || SEND_RC=$?
printf '%s\n' "$SEND_A" >> "$OUT/legA_send.log"
[ "$SEND_RC" -eq 0 ] || { cat "$OUT/legA_send.log" >&2; fail_out "the client REJECTED leg A's claim (cast exit $SEND_RC)"; }

TX_A="$(printf '%s' "$SEND_A" | python3 -c 'import json,sys; print(json.load(sys.stdin)["transactionHash"])')"
cast receipt "$TX_A" --rpc-url "$RPC1" --json > "$OUT/legA_receipt.json"
cast tx      "$TX_A" --rpc-url "$RPC1" --json > "$OUT/legA_tx.json"
BAL_U2_AFTER="$(cast balance "$U2_ADDR" --rpc-url "$RPC1")"

ESCROW_A_BAL="$(cast balance "$SWAP_A" --rpc-url "$RPC1")"
ESCROW_B_BAL="$(cast balance "$SWAP_B" --rpc-url "$RPC2")"

# --- 13. verdict, computed FROM the receipts, transactions and retained bytes -----------
set +o pipefail
python3 - "$OUT/legB_receipt.json" "$OUT/legB_tx.json" "$OUT/legA_receipt.json" "$OUT/legA_tx.json" \
         "$TX_GAS_CAP" "$OUT/legB.calldata" "$OUT/legA.calldata" \
         "$VEC/sigma2.bin" "$VEC/sigma2_from_chain.bin" \
         "$BAL_U1_BEFORE" "$BAL_U1_AFTER" "$BAL_U2_BEFORE" "$BAL_U2_AFTER" "$COIN_WEI" \
         "$ESCROW_A_BAL" "$ESCROW_B_BAL" "$REPLAY_RC" "$REPLAY_REASON_OK" \
         <<'PY' | tee -a "$OUT/verdict.txt"
import json, sys

(rB, txB, rA, txA, cap, cdB, cdA, sig_local, sig_chain,
 u1_before, u1_after, u2_before, u2_after, coin,
 escrowA, escrowB, replay_rc, replay_reason_ok) = sys.argv[1:19]
cap, coin = int(cap), int(coin)

def num(v):
    s = str(v)
    return int(s, 16) if s.startswith("0x") else int(s)

def hexbytes(s):
    s = s.strip().lower()
    return bytes.fromhex(s[2:] if s.startswith("0x") else s)

rb, tb = json.load(open(rB)), json.load(open(txB))
ra, ta = json.load(open(rA)), json.load(open(txA))
sent_b, sent_a = hexbytes(open(cdB).read()), hexbytes(open(cdA).read())
recv_b, recv_a = hexbytes(tb["input"]), hexbytes(ta["input"])
local = open(sig_local, "rb").read()
chain = open(sig_chain, "rb").read()

# Each claimant is also the beneficiary of the leg it claims, so it pays the fee for its
# own payout: a raw balance delta understates the payout by exactly that fee.
def payout(before, after, receipt):
    fee = num(receipt["gasUsed"]) * num(receipt["effectiveGasPrice"])
    return (int(after) - int(before)) + fee

u1_paid = payout(u1_before, u1_after, rb)   # leg B, on chain 2
u2_paid = payout(u2_before, u2_after, ra)   # leg A, on chain 1

def leg(name, r, t, sent, recv):
    print("%s claim tx        : %s" % (name, r["transactionHash"]))
    print("%s status          : %s" % (name, "SUCCESS" if num(r["status"]) == 1 else "FAILED"))
    print("%s gasLimit on tx  : %d  (cap %d)" % (name, num(t.get("gas", t.get("gasLimit"))), cap))
    print("%s gasUsed         : %d" % (name, num(r["gasUsed"])))
    print("%s calldata        : %d received / %d expected, byte-equal %s"
          % (name, len(recv), len(sent), "YES" if recv == sent else "NO"))

leg("legB", rb, tb, sent_b, recv_b)
leg("legA", ra, ta, sent_a, recv_a)
print("sigma_2 recovered from chain : %d bytes, identical to local copy: %s"
      % (len(chain), "YES" if chain == local else "NO"))
print("replay control               : cast exit %s, contract reason matched: %s"
      % (replay_rc, "YES" if replay_reason_ok == "1" else "NO"))
print("u1 payout on chain 2 (leg B) : %d wei, fee-adjusted (escrow was %d)" % (u1_paid, coin))
print("u2 payout on chain 1 (leg A) : %d wei, fee-adjusted (escrow was %d)" % (u2_paid, coin))
print("escrow balances after        : legA %s wei, legB %s wei (both must be 0)"
      % (escrowA, escrowB))

fail = []
for name, r, t in (("legB", rb, tb), ("legA", ra, ta)):
    if num(r["status"]) != 1:
        fail.append("%s claim reverted (receipt status 0)" % name)
    if num(t.get("gas", t.get("gasLimit"))) > cap:
        fail.append("%s mined gasLimit exceeds the EIP-7825 cap" % name)
    if num(r["gasUsed"]) >= cap:
        fail.append("%s gasUsed is not under the EIP-7825 cap" % name)
for name, sent, recv in (("legB", sent_b, recv_b), ("legA", sent_a, recv_a)):
    if sent != recv:
        n = min(len(sent), len(recv))
        first = next((i for i in range(n) if sent[i] != recv[i]), n)
        fail.append("%s: calldata the node received differs from the retained expected "
                    "calldata (lengths %d vs %d, first differing byte at offset %d)"
                    % (name, len(recv), len(sent), first))
# THE PROVENANCE CHECK. Leg A was settled with a signature derived from the witness that
# `extract_and_adapt` recovered from these bytes; if they are not the bytes the chain
# carried, the run demonstrated an adaptor signature rather than an atomic swap.
if chain != local:
    fail.append("the sigma_2 sliced out of the mined transaction differs from the copy "
                "that was broadcast — leg A's witness cannot be attributed to the ledger")
if replay_rc == "0":
    fail.append("the replay control was ACCEPTED: signatures do not bind their legs")
if replay_reason_ok != "1":
    fail.append("the replay was refused, but not with the contract's 'message not bound' "
                "reason, so the refusal is not attributable to message binding")
if u1_paid != coin:
    fail.append("u1's fee-adjusted payout on chain 2 was %d wei, expected exactly %d" % (u1_paid, coin))
if u2_paid != coin:
    fail.append("u2's fee-adjusted payout on chain 1 was %d wei, expected exactly %d" % (u2_paid, coin))
if int(escrowA) != 0 or int(escrowB) != 0:
    fail.append("an escrow still holds funds after settlement (legA %s, legB %s)" % (escrowA, escrowB))

if fail:
    print()
    print("RESULT: FAIL — %d post-condition(s) not met:" % len(fail))
    for i, f in enumerate(fail, 1):
        print("  %d. %s" % (i, f))
    sys.exit(1)

print()
print("RESULT: a two-leg atomic swap settled across TWO real Ethereum clients on two")
print("        chain ids, each leg verified on-chain inside one capped transaction, with")
print("        leg A's witness recovered from the bytes chain 2 actually carried, and a")
print("        replay of leg B's message onto leg A refused by the contract.")
PY
VERDICT_RC=${PIPESTATUS[0]}
set -o pipefail
[ "$VERDICT_RC" -eq 0 ] || fail_out "post-conditions not met — see the enumerated list in verdict.txt"

ln -sfn "$RUN_ID" "$REPO/evidence/onchain_twoleg/latest"
rm -rf "$REPO/evm/twoleg"

echo
echo "evidence written: evidence/onchain_twoleg/$RUN_ID/"
echo "  verdict.txt          — pass/fail computed FROM both receipts, txs and the bytes"
echo "  leg{A,B}_receipt.json / leg{A,B}_tx.json — the clients' own records"
echo "  leg{A,B}.calldata    — expected calldata, retained for independent re-checking"
echo "  replay.calldata      — the live replay control's calldata"
echo "  replay_call.log      — its revert reason (WHY it is refused)"
echo "  replay_send.log      — its real broadcast attempt (THAT it cannot settle)"
echo "  leg_messages.txt     — each leg's digest, as read from its node"
echo "  setup_vectors.log    — Gen and pi (proved AND verified)"
echo "  presign.log          — PreSign + both PreVerify gates"
echo "  extract_adapt.log    — Ext from the chain's bytes, then Adapt"
echo "  cap_control.log      — the differential control on BOTH nodes"
echo "  anvil{1,2}.log       — both nodes' logs"
echo "  environment.txt      — toolchain, hardfork, chain ids, flags, vector digests"
echo
echo "decisive rows in verdict.txt:"
echo "  'sigma_2 recovered from chain ... identical to local copy: YES'  (provenance)"
echo "  'replay control ... contract reason matched: YES'                (binding)"
echo "  'u1/u2 payout ... fee-adjusted'                                  (both legs paid)"
