#!/usr/bin/env bash
# Settle a two-leg LAS swap ACROSS TWO DIFFERENT KINDS OF BLOCKCHAIN — leg A pays ETH on a
# real Ethereum client, leg B pays BTC on a patched Bitcoin Core — and separately exercise
# the Bitcoin leg's TIMEOUT REFUND branch.
#
#   BTC_TAG=v31.1 BTC_SRC=/path/to/patched-src BTC_BIN_PATCHED=.../build/bin/bitcoind \
#   BTC_BIN_STOCK=.../bitcoin-31.1/bin/bitcoind ./scripts/run_btc_eth_swap.sh
#
# ⚠️ WHAT THIS IS NOT — READ BEFORE QUOTING ANYTHING FROM IT
# -----------------------------------------------------------
# This is NOT a demonstration of a fair atomic swap, and no output of it may be described as
# one. The Bitcoin claim leaf is a SINGLE-KEY check under the FUNDER's own public key —
# that being the key the pre-signature was made under — so the funder can spend the coin
# without waiting for the timeout, and the refund branch is therefore not an EXCLUSIVE
# recovery path. Worse, the mempool exposes the ADAPTED SIGNATURE before confirmation: the
# funder, which holds the matching pre-signature, can run Ext on that unconfirmed signature
# and attempt a conflicting spend. If its conflicting spend confirms instead, it keeps this
# coin while using the extracted witness to claim the other leg. (Whether the conflicting
# spend wins depends on relay/replacement policy and miners, so the race is the claim, not a
# guaranteed theft — but a race is already enough to deny fairness.) eprint 2020/845 Sec 4.1 reasons
# from a signature "published on a blockchain" and abstracts that race away; a concrete
# Bitcoin implementation does not get to. Closing it needs a claim condition the funder
# cannot satisfy alone — a second LAS check under the claimant's key, which the consensus
# opcode does support, since it pops a FIXED 1 + n_pk + n_sig elements and therefore
# composes. That is NOT done here and NOT claimed.
#
# Supported wording: a cross-venue LAS settlement, plus an explicit timeout refund branch
# shown to be enforced on the Bitcoin side.
#
# WHAT IS NEW HERE, AND WHY IT IS A DIFFERENT CLAIM FROM THE SAME-VENUE RUNNERS
# -----------------------------------------------------------------------------
#   * ONE LAS INSTANCE, TWO INDEPENDENT VENUE-SPECIFIC VERIFICATION MECHANISMS. A' is
#     expanded from the pp seed alone and every operation consumes it, so BOTH venues must
#     verify against the SAME instance — same A', same keys, same relation. What differs is
#     the mechanism: the EVM verifier receives A' as CALLDATA, while this patched Bitcoin
#     verifier instead fixes one seed in bitcoin/las_consensus/. Bitcoin is therefore the
#     binding constraint here, and the WHOLE swap is generated under `las_btc_tool seed`,
#     which the Ethereum leg then registers as its A'.
#   * THE WITNESS CROSSES VENUES. u1 settles leg B on BITCOIN; the adapted signature is read
#     back out of the MINED WITNESS, its boundary located by the commitment the funding
#     output's tapleaf made rather than by a known length (bitcoin/tools/btc_recover_sig.py).
#     The ADAPTED-SIGNATURE INPUT to Ext comes exclusively from that mined witness, and what
#     Ext recovers is what settles leg A on ETHEREUM.
#   * TIMEOUT REFUND ON BITCOIN, WITH THE TWO REJECTION CAUSES SEPARATED. See section 17.
#
# THE RECOVERY LAYER — AN ADDITION, NOT A FIG. 1 REPAIR. Read this before quoting the paper.
# eprint 2020/845 Sec 4.1 opens by RECALLING the classical atomic-swap protocol of its [23], in
# which both transactions carry timeouts and u2's transaction carries the shorter one
# (t2 < t1), to give u2 enough time to react. ⚠️ That is not the same as publishing order: in
# that recap u1 publishes her transaction first, so the leg published FIRST is the one with the
# LONGER t1. It is the leg SETTLED first that carries t2. Sec 4.1 then turns to LAS, and the
# choreography it gives — Fig. 1 — restates no timeout at all. So the settlement path here
# follows the Fig. 1 LAS choreography, as it already did, and everything below about deadlines
# is an EXTRA recovery experiment inspired by the classical timeout setup that Sec 4.1 recalls.
# Nothing here may be reported as making Fig. 1 compliant.
#
# WHAT THE RECOVERY EXPERIMENT COVERS, AND ON WHAT:
#   * Both venues' refund mechanisms are exercised on DEDICATED RECOVERY-TEST OBJECTS, one of
#     each venue's own kind — a control UTXO on Bitcoin (coin B3, section 17) and a second
#     escrow on Ethereum (section 18b). NEITHER settled leg has its refund path taken; both
#     are claimed, which is what the honest path means.
#   * The two CONFIGURED deadlines are expressed in ONE NUMERIC DOMAIN (absolute UNIX seconds)
#     and their ordering t2 < t1 is checked by this script, leg B — settled first, and whose
#     settlement publishes the adapted signature the witness is extracted from — carrying the
#     shorter one.
#
# ⚠️ A SHARED NUMERIC DOMAIN IS NOT A SHARED CLOCK, and the difference is not cosmetic. Leg A's
# deadline is enforced against `block.timestamp` by the contract. Leg B's is a timestamp-mode
# `nLockTime`: BIP65's OP_CHECKLOCKTIMEVERIFY compares its operand with `nLockTime`, requires
# the same locktime TYPE and a non-final input, while whether the transaction may be included
# is decided by consensus finality against the block's MEDIAN TIME PAST (BIP113). Two ledgers,
# two enforcement quantities. What this run supports is the CONFIGURED numeric inequality and
# the fact that each side enforces its own deadline. It does not show the two instants
# coincide, the difference t1 - t2 is a configured gap and not a guaranteed reaction window,
# and neither venue checks the other's deadline — pairing them stays a deployment decision.
#
# WHAT DOES NOT CARRY OVER FROM run_btc_two_leg.sh. That runner proves its two ledgers are
# independent by OFFERING leg B's settled transaction to chain 1 and having it refused for a
# coin that does not exist there. That control HAS NO CROSS-VENUE ANALOGUE and is not
# attempted: a Bitcoin transaction cannot be offered to an EVM node. Ledger separation here
# rests on the two venues being different clients with different consensus rules and
# different genesis states — a weaker and more obvious fact, recorded as such.
#
# WHAT EACH LEG'S MESSAGE BINDS — THEY ARE NOT THE SAME LIST, AND NEITHER IS A SUPERSET.
# Leg A binds chain id, contract, escrow id, payer, beneficiary and amount
# (`LASRegister.claimMessage`). Leg B's BIP341 sighash binds its Bitcoin transaction inputs
# and prevout amounts and its outputs, but has NO chain id. There is no "transaction input"
# on the EVM side and no chain identifier on the Bitcoin side; say the two lists, never
# "both bind their inputs".
#
# CONSENSUS, NOT POLICY, ON THE BITCOIN SIDE. Every Bitcoin verdict comes from
# `generateblock ... submit=false`: block validation without submission. `testmempoolaccept`
# would mix in relay policy under which these 520-byte witness chunks are non-standard for
# reasons unrelated to LAS.
#
# SCOPE. Honest settlement path plus the Bitcoin refund branch. A patched node is not
# Bitcoin; the consensus rule's SECURITY is unanalysed; regtest and anvil are not mainnets;
# the two sides' security levels are unmatched.
#
# PI IS NOT OPTIONAL. Fig. 1 has u2 verify a proof of knowledge before it pre-signs. PI=0 is
# allowed for diagnosis but is recorded as INCOMPLETE: it cannot print the success verdict
# and does not move `latest`.
#
# Logs and receipts are written BY THE TOOLS. Never hand-edit them; never type a number from
# here into the report.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN_ID="$(date +%Y%m%d_%H%M%S)"
OUT="$REPO/evidence/btc_eth_swap/$RUN_ID"
TOOLS="$REPO/bitcoin/tools"
SHIM="$REPO/bitcoin/las_consensus"
PATCH="$REPO/bitcoin/patches/0001-op-checklassigverify-v31.1.patch"

# Must live under evm/ because forge resolves VEC_DIR relative to its own cwd, while the C
# tools take it absolute. One directory, addressed both ways.
VEC_REL="test/vectors/swap_xchain"
VEC="$REPO/evm/$VEC_REL"

# Private ports, and NOT the ones the same-venue runners use.
RPCP="${RPCP:-18610}"; RPCS="${RPCS:-18611}"
P2PP="${P2PP:-18612}"; P2PS="${P2PS:-18613}"
PORT_ETH="${PORT_ETH:-8562}"
RPC_ETH="http://127.0.0.1:$PORT_ETH"
CHAINID_ETH="${CHAINID_ETH:-31337}"

TX_GAS_CAP=16777216
HARDFORK="${HARDFORK:-osaka}"
FEE_SAT="${FEE_SAT:-200000}"
COIN_BTC="${COIN_BTC:-1.0}"
COIN_WEI=1000000000000000000
SEED_PREIMAGE="LAS-CONSENSUS-PARAMS-v1"
PI="${PI:-1}"

# THE TWO DEADLINES, AS OFFSETS FROM ONE HORIZON COMPUTED AT RUN TIME (section 4b).
# Leg B is settled FIRST — settling it publishes the ADAPTED SIGNATURE, from which a party
# holding the matching pre-signature extracts the witness — so it carries the SHORTER
# deadline, preserving the configured reaction margin before leg A's later deadline. Both are
# absolute UNIX seconds so the configured ordering is a numeric fact this script can check
# rather than estimate; that is a shared numeric domain and NOT a shared consensus clock
# (section 4b says exactly what each side enforces).
DELTA_T2="${DELTA_T2:-3600}"    # leg B, BITCOIN, claimed first  -> t2
DELTA_T1="${DELTA_T1:-86400}"   # leg A, ETHEREUM, claimed second -> t1
# BIP65: an operand below this is a BLOCK HEIGHT, at or above it a UNIX TIME, and CLTV will
# not compare one kind against the other. Mirrors LOCKTIME_THRESHOLD in btc_las_spend.py.
LOCKTIME_THRESHOLD=500000000

U1_KEY=0xac0974bec39a17e36ba4a6b4d238ff944bacb478cbed5efcae784d7bf4f2ff80
U1_ADDR=0xf39Fd6e51aad88F6F4ce6aB8827279cffFb92266
U2_KEY=0x59c6995e998f97a5a0044966f0945389dc9e86dae88c7a8412f4603b6b78690d
U2_ADDR=0x70997970C51812dc3A010C7d01b50e0d17dc79C8
CONTROL_SINK=0x3C44CdDdB6a900fa2b585dd299e03d12FA4293BC

: "${BTC_TAG:?set BTC_TAG, e.g. v31.1}"
: "${BTC_SRC:?set BTC_SRC to the PATCHED Bitcoin Core source tree}"
: "${BTC_BIN_PATCHED:?set BTC_BIN_PATCHED to the bitcoind built from BTC_SRC}"
: "${BTC_BIN_STOCK:?set BTC_BIN_STOCK to the stock release bitcoind of the same tag}"
CLI="${CLI:-$(dirname "$BTC_BIN_PATCHED")/bitcoin-cli}"

mkdir -p "$OUT"
DP="$OUT/btc_patched"; DS="$OUT/btc_stock"
ANVIL_PID=""

cleanup() {
  "$CLI" -regtest -datadir="$DP" -rpcport="$RPCP" stop >/dev/null 2>&1 || true
  "$CLI" -regtest -datadir="$DS" -rpcport="$RPCS" stop >/dev/null 2>&1 || true
  [ -n "$ANVIL_PID" ] && kill "$ANVIL_PID" 2>/dev/null || true
}
trap cleanup EXIT

fail_out() {
  {
    echo; echo "RESULT: FAIL — $1"; echo
    echo "run_id: $RUN_ID"
    echo "This is a recorded negative result, not a crashed run. Retained alongside it:"
    echo "  btc_{patched,stock}/regtest/debug.log   both Bitcoin nodes' logs"
    echo "  anvil.log                               the Ethereum node's log"
    echo "  *_consensus_*.{json,err}  each Bitcoin node's block-validation answer, verbatim"
    echo "  legB_spend.json refund_*_spend.json     exactly what this run intended to mine"
    echo "  legA.calldata legA_send.log             what it intended to send, and the reply"
    echo "  replay_call.log replay_send.log         the EVM cross-venue binding control"
    echo "  cap_control.log                         the EIP-7825 differential control"
    echo "  recovered_sigB.json                     the chain-byte recovery, if reached"
    echo "  setup_vectors.log presign.log adapt.log extract_adapt.log"
    echo "  applied.patch environment.txt selftest.log"
  } | tee -a "$OUT/verdict.txt"
  echo "evidence written (FAILING run): evidence/btc_eth_swap/$RUN_ID/" >&2
  exit 1
}

cli_p() { "$CLI" -regtest -datadir="$DP" -rpcport="$RPCP" "$@"; }
cli_s() { "$CLI" -regtest -datadir="$DS" -rpcport="$RPCS" "$@"; }
jqpy()  { python3 -c "$1" "${@:2}"; }
hex_to_bin() {
  python3 -c 'import sys;h=sys.argv[1].strip();h=h[2:] if h.startswith("0x") else h;open(sys.argv[2],"wb").write(bytes.fromhex(h))' "$1" "$2"
}

# --- 1. pin gates (Bitcoin) --------------------------------------------------------------
# Three separate gates, not one equality: a version STRING is not a commit, and `git
# describe` would also match commits made after the tag.
[ -x "$BTC_BIN_PATCHED" ] || fail_out "BTC_BIN_PATCHED is not executable"
[ -x "$BTC_BIN_STOCK" ]   || fail_out "BTC_BIN_STOCK is not executable"
[ -f "$PATCH" ]           || fail_out "the committed patch $PATCH is missing"
[ -d "$BTC_SRC/.git" ]    || fail_out "BTC_SRC must be a git checkout so the patch can be verified"

tag_bare="${BTC_TAG#v}"
for b in "$BTC_BIN_PATCHED" "$BTC_BIN_STOCK"; do
  v="$("$b" --version | head -1)"
  case "$v" in *"$tag_bare"*) : ;; *) fail_out "$b reports '$v', not the pinned tag $BTC_TAG" ;; esac
done

SRC_TAG_COMMIT="$(git -C "$BTC_SRC" rev-list -n 1 "$BTC_TAG" 2>/dev/null || true)"
[ -n "$SRC_TAG_COMMIT" ] || fail_out "tag $BTC_TAG not found in $BTC_SRC"
[ "$(git -C "$BTC_SRC" rev-parse HEAD)" = "$SRC_TAG_COMMIT" ] \
  || fail_out "the patched tree's HEAD is not the $BTC_TAG commit; the diff would then include upstream changes as well as ours"

git -C "$BTC_SRC" diff HEAD > "$OUT/applied.patch"
APPLIED_SHA="$(sha256sum "$OUT/applied.patch" | cut -d' ' -f1)"
COMMITTED_SHA="$(sha256sum "$PATCH" | cut -d' ' -f1)"
[ "$APPLIED_SHA" = "$COMMITTED_SHA" ] \
  || fail_out "the patch applied to $BTC_SRC (sha256 $APPLIED_SHA) is not the committed one ($COMMITTED_SHA)"

UNTRACKED="$(git -C "$BTC_SRC" status --porcelain --untracked-files=all \
             | grep '^??' | grep -v '^?? build/' || true)"
[ -z "$UNTRACKED" ] || { printf '%s\n' "$UNTRACKED" > "$OUT/untracked.txt"; \
  fail_out "the patched tree has untracked files the committed patch does not contain (see untracked.txt)"; }

command -v anvil >/dev/null || fail_out "anvil not found (install Foundry)"
command -v cast  >/dev/null || fail_out "cast not found (install Foundry)"
command -v forge >/dev/null || fail_out "forge not found (install Foundry)"

# --- 2. the shim and the consensus seed, before any node exists ---------------------------
make -C "$SHIM" las_btc_tool > "$OUT/build_shim.log" 2>&1 \
  || { cat "$OUT/build_shim.log" >&2; fail_out "could not build the consensus shim"; }
"$SHIM/las_btc_tool" selftest > "$OUT/selftest.log" 2>&1 \
  || { cat "$OUT/selftest.log" >&2; fail_out "the shim's selftest failed — the crypto is wrong before any node is involved"; }
cat "$OUT/selftest.log"

SEED="$("$SHIM/las_btc_tool" seed)"
SEED_EXPECT="$(jqpy 'import hashlib,sys;print(hashlib.sha256(sys.argv[1].encode()).hexdigest())' "$SEED_PREIMAGE")"
[ "$SEED" = "$SEED_EXPECT" ] \
  || fail_out "the consensus seed compiled in ($SEED) is not SHA-256(\"$SEED_PREIMAGE\") = $SEED_EXPECT"
echo "consensus seed OK: SHA-256(\"$SEED_PREIMAGE\") == the compiled-in value"

# --- 3. the off-chain tooling, and Gen + pi, under the CONSENSUS seed ---------------------
# THE SEED IS THE JOIN. Both venues must verify against the SAME LAS instance; the EVM
# verifier receives A' as calldata, while this patched Bitcoin verifier fixes one seed in
# bitcoin/las_consensus/. Generating the swap under the node's seed is what puts them on one
# instance — arranged here, not discovered from a rejected transaction.
rm -rf "$VEC" && mkdir -p "$VEC"
if [ "$PI" = "1" ]; then
  SWAP_TOOL=test/export_swap_vectors_pi
  SETUP_ARGS=""
  if ! make -C "$REPO/ref" "$SWAP_TOOL" > "$OUT/build.log" 2>&1; then
    cat "$OUT/build.log" >&2
    fail_out "could not build $SWAP_TOOL — the vendored LaZer library is required for Fig. 1's proof of knowledge (see README). Re-run with PI=0 to record deliberately that this run omits pi, but such a run is INCOMPLETE and cannot pass."
  fi
else
  SWAP_TOOL=test/export_swap_vectors
  SETUP_ARGS="--no-pi"
  make -C "$REPO/ref" "$SWAP_TOOL" > "$OUT/build.log" 2>&1 || {
    cat "$OUT/build.log" >&2; fail_out "could not build $SWAP_TOOL"; }
  echo "WARNING: PI=0 — Fig. 1's proof-of-knowledge step is OMITTED. This run is recorded" >&2
  echo "         as INCOMPLETE and will not move evidence/btc_eth_swap/latest." >&2
fi
make -C "$REPO/ref" test/extract_and_adapt >> "$OUT/build.log" 2>&1 || {
  cat "$OUT/build.log" >&2; fail_out "could not build test/extract_and_adapt"; }

if ! (cd "$REPO/ref" && "./$SWAP_TOOL" setup "$VEC" $SETUP_ARGS --pp-seed "$SEED") \
      > "$OUT/setup_vectors.log" 2>&1; then
  cat "$OUT/setup_vectors.log" >&2
  fail_out "swap setup failed (Gen or pi) — see setup_vectors.log"
fi
cat "$OUT/setup_vectors.log"

# --- 4. the nodes: two Bitcoin (patched + stock, one chain), one Ethereum ------------------
mkdir -p "$DP" "$DS"
start_node() { # $1 bin, $2 datadir, $3 rpcport, $4 p2pport, $5 log, $6 optional -connect peer
  local extra=()
  if [ -n "${6:-}" ]; then extra=(-connect=127.0.0.1:"$6"); fi
  "$1" -regtest -datadir="$2" -rpcport="$3" -port="$4" -bind=127.0.0.1 \
    -fallbackfee=0.0002 -daemonwait "${extra[@]}" > "$5" 2>&1 \
    || { cat "$5" >&2; fail_out "node failed to start: $2"; }
}
# The stock node sees the SAME chain: its answer must differ from the patched node's only
# because of the rule, never because it is looking at a different ledger.
start_node "$BTC_BIN_PATCHED" "$DP" "$RPCP" "$P2PP" "$OUT/node_patched_start.log"
start_node "$BTC_BIN_STOCK"   "$DS" "$RPCS" "$P2PS" "$OUT/node_stock_start.log" "$P2PP"
for f in cli_p cli_s; do
  $f -rpcwait getblockchaininfo > /dev/null || fail_out "$f did not answer"
done

sync_chain() {
  local a b
  for _ in $(seq 1 200); do
    a="$(cli_p getbestblockhash 2>/dev/null || true)"
    b="$(cli_s getbestblockhash 2>/dev/null || true)"
    [ -n "$a" ] && [ "$a" = "$b" ] && return 0
    sleep 0.2
  done
  fail_out "the two Bitcoin nodes did not converge — a verdict from an unsynced node would be about missing inputs, not about the rule"
}

cli_p createwallet lasx > /dev/null 2>&1 || fail_out "no wallet support in the patched build"
cli_p generatetoaddress 101 "$(cli_p getnewaddress)" > /dev/null
sync_chain
mine_b() { cli_p generatetoaddress "${1:-1}" "$(cli_p getnewaddress)" > /dev/null; sync_chain; }

if cast block-number --rpc-url "$RPC_ETH" >/dev/null 2>&1; then
  fail_out "something is already serving $RPC_ETH — refusing to measure against a node this script did not start. Stop it, or re-run with PORT_ETH=<free port>."
fi
anvil --hardfork "$HARDFORK" --port "$PORT_ETH" --chain-id "$CHAINID_ETH" --enable-tx-gas-limit \
  > "$OUT/anvil.log" 2>&1 &
ANVIL_PID=$!
for _ in $(seq 1 60); do
  if ! kill -0 "$ANVIL_PID" 2>/dev/null; then
    cat "$OUT/anvil.log" >&2
    fail_out "anvil exited during startup — see anvil.log (is --hardfork $HARDFORK supported by this build?)"
  fi
  cast block-number --rpc-url "$RPC_ETH" >/dev/null 2>&1 && break
  sleep 0.2
done
cast block-number --rpc-url "$RPC_ETH" >/dev/null 2>&1 \
  || fail_out "anvil did not answer at $RPC_ETH within 12s"
GOT_ETH="$(cast chain-id --rpc-url "$RPC_ETH")"
[ "$GOT_ETH" = "$CHAINID_ETH" ] || fail_out "the Ethereum node reports chain id $GOT_ETH, expected $CHAINID_ETH"

# --- 4b. the two deadlines, in one numeric domain, ordered t2 < t1 --------------------------
# WHAT EACH SIDE ACTUALLY ENFORCES — they are different quantities, and the run never claims
# otherwise. Leg A: the contract compares `block.timestamp` against the stored deadline. Leg
# B: a timestamp-mode `nLockTime`, where BIP65's OP_CHECKLOCKTIMEVERIFY compares its operand
# with `nLockTime`, requires the same locktime TYPE (both height or both time) and a non-final
# input, while whether the transaction may be included at all is decided by consensus finality
# against the block's MEDIAN TIME PAST under BIP113. Expressing both as absolute UNIX seconds
# makes the CONFIGURED ordering checkable; it does not make the two enforcement instants one
# clock, and no artefact may say it does.
#
# The horizon is taken from BOTH chains' own records, not from this machine's wall clock, so a
# node whose time has drifted cannot leave a deadline already in the past on its side.
eth_now() { cast block latest --field timestamp --rpc-url "$RPC_ETH"; }
btc_mtp() { cli_p getblockchaininfo | jqpy 'import json,sys;print(json.load(sys.stdin)["mediantime"])'; }

BTC_MTP0="$(btc_mtp)"; ETH_T0="$(eth_now)"
[ -n "$BTC_MTP0" ] && [ -n "$ETH_T0" ] || fail_out "could not read both chains' current time"
T_HORIZON=$(( BTC_MTP0 > ETH_T0 ? BTC_MTP0 : ETH_T0 ))
T2=$(( T_HORIZON + DELTA_T2 ))
T1=$(( T_HORIZON + DELTA_T1 ))

# Each of these has teeth. Without the ordering the recovery layer is not the [23] setup Sec
# 4.1 recalls; without the threshold the Bitcoin operand is a different kind of deadline
# altogether and the numeric comparison against leg A's would be meaningless.
[ "$T2" -lt "$T1" ] \
  || fail_out "the deadlines are not ordered t2 < t1 (t2=$T2, t1=$T1): the leg settled FIRST must carry the SHORTER deadline so the configured reaction margin described by the [23] setup recalled in Sec 4.1 is preserved. Check DELTA_T2 < DELTA_T1."
[ "$T2" -ge "$LOCKTIME_THRESHOLD" ] \
  || fail_out "leg B's deadline $T2 is below BIP65's LOCKTIME_THRESHOLD ($LOCKTIME_THRESHOLD), so CLTV would interpret it as a BLOCK HEIGHT — a different kind of deadline from leg A's, which would void both the intended timestamp-mode leaf and the numeric comparison with t1"
[ "$BTC_MTP0" -lt "$T2" ] && [ "$ETH_T0" -lt "$T1" ] \
  || fail_out "a deadline is not strictly ahead of its own chain at funding time (btc mtp=$BTC_MTP0 vs t2=$T2, eth=$ETH_T0 vs t1=$T1) — the intended before-deadline controls would no longer exercise a pre-deadline state"
{
  echo "horizon (max of both chains' own clocks) : $T_HORIZON"
  echo "  bitcoin median time past at horizon    : $BTC_MTP0"
  echo "  ethereum latest block timestamp        : $ETH_T0"
  echo "t2 = leg B (BITCOIN, settled first)      : $T2  (+${DELTA_T2}s) — enforced as a timestamp nLockTime against MEDIAN TIME PAST"
  echo "t1 = leg A (ETHEREUM, settled second)    : $T1  (+${DELTA_T1}s) — enforced as block.timestamp by AdaptorSwapBound"
  echo "ordering t2 < t1                         : $((T1 - T2))s CONFIGURED DEADLINE GAP, checked by this script"
  echo "⚠ one numeric domain (UNIX seconds), NOT one consensus clock: the two enforcement"
  echo "  quantities are median time past and block.timestamp on separate ledgers, so the gap"
  echo "  above is a configured difference and NOT a guaranteed real reaction window."
} | tee "$OUT/deadlines.txt"

# --- 5. DIFFERENTIAL CONTROL on the Ethereum node: is the cap actually enforced? -----------
# The exit status is taken STRAIGHT off `cast` via `|| rc=$?`; read with `$?` after a
# redirected group it would be the trailing command's, and every send would look accepted.
control_send() { # $1 = gas limit, $2 = label
  local rc=0
  echo "--- control [$2] : cast send at gasLimit=$1 on $RPC_ETH ---" >> "$OUT/cap_control.log"
  cast send "$CONTROL_SINK" --value 1 --gas-limit "$1" \
    --private-key "$U1_KEY" --rpc-url "$RPC_ETH" >> "$OUT/cap_control.log" 2>&1 || rc=$?
  echo "cast exit code: $rc" >> "$OUT/cap_control.log"
  return "$rc"
}
OVER=$((TX_GAS_CAP + 1))
RC_AT=0; RC_OVER=0
control_send "$TX_GAS_CAP" "at cap"  || RC_AT=$?
control_send "$OVER"       "cap + 1" || RC_OVER=$?
{
  echo; echo "--- differential control summary ---"
  echo "gasLimit=$TX_GAS_CAP (at cap) : cast exit $RC_AT   (expected 0 = accepted)"
  echo "gasLimit=$OVER (cap + 1)      : cast exit $RC_OVER (expected non-zero = refused)"
} >> "$OUT/cap_control.log"
[ "$RC_AT" -eq 0 ] || fail_out "control INCONCLUSIVE: the trivial transfer failed AT the cap (cast exit $RC_AT) — see cap_control.log"
[ "$RC_OVER" -ne 0 ] || fail_out "the node ACCEPTED a transaction one gas ABOVE the EIP-7825 cap — the cap is NOT being enforced, so no result from this run would mean anything"
echo "cap control OK: accepted at gasLimit=$TX_GAS_CAP, refused at $OVER"

# --- 6. fund leg A on ETHEREUM, and read its message FROM THE NODE -------------------------
# Leg A: u1 escrows 1 ether for u2, pre-signed under u1's key (t1). Claimed SECOND.
# VEC_DIR points at the SAME directory the Bitcoin side uses, so the A' registered here is
# expanded from the consensus seed — this is where the two verifiers meet on one instance.
cd "$REPO/evm"
rm -rf xchain && mkdir -p xchain/eth
# TIMEOUT_ABS, not TIMEOUT_SECS: an absolute deadline is what makes leg A's stored value
# numerically comparable with leg B's CLTV operand. `TwoLegFund` reads the deadline back out
# of the escrow and asserts it round-tripped, so `deadline.unix` is the contract's value.
PRIVATE_KEY="$U1_KEY" BENEFICIARY="$U2_ADDR" VEC_DIR="$VEC_REL" T_FILE=t1.bin \
OUT_DIR="xchain/eth" COIN_WEI="$COIN_WEI" TIMEOUT_ABS="$T1" \
forge script script/TwoLeg.s.sol:TwoLegFund --rpc-url "$RPC_ETH" --broadcast \
  --private-key "$U1_KEY" > "$OUT/fund_legA.log" 2>&1 \
  || { cat "$OUT/fund_legA.log" >&2; fail_out "funding leg A on Ethereum failed"; }

SWAP_A="$(cat xchain/eth/swap.addr)"; ID_A="$(cat xchain/eth/escrow.id)"
# Read the escrow back from the NODE and check the deadline it will enforce is t1. The script
# already asserted its own round-trip; this asserts it against the chain the runner queries.
# NOT `mapfile < <(...)`: a process substitution's failure is not mapfile's exit status, so a
# dead RPC would reach the field reads below and die as an unbound array, with no verdict.
eth_call() { cast call "$@" --rpc-url "$RPC_ETH" | sed 's/[[:space:]]*\[.*$//'; }
SWAPS_SIG="swaps(uint256)(address,address,uint256,uint64,uint8,bytes32)"
SW_A_RAW="$(eth_call "$SWAP_A" "$SWAPS_SIG" "$ID_A")" \
  || fail_out "could not read leg A's escrow back from the node"
mapfile -t SW_A <<< "$SW_A_RAW"
[ "${#SW_A[@]}" -eq 6 ] \
  || fail_out "leg A's escrow decoded to ${#SW_A[@]} fields, expected 6 — the getter's shape is not what this script reads"
DEADLINE_A="${SW_A[3]}"; CTX_A="${SW_A[5]}"
[ "$DEADLINE_A" = "$T1" ] \
  || fail_out "leg A's escrow stores deadline $DEADLINE_A, not the intended t1=$T1 — the ordering check in section 4b would then be about a value the contract does not enforce"
[ "${SW_A[4]}" = "1" ] || fail_out "leg A's escrow is not OPEN after funding (state ${SW_A[4]})"
MSG_A="$(cast call "$SWAP_A" "legMessage(uint256)(bytes32)" "$ID_A" --rpc-url "$RPC_ETH")"
EXP_A="$(cat xchain/eth/legmsg.expected)"
[ "${MSG_A,,}" = "${EXP_A,,}" ] \
  || fail_out "leg A: the node's legMessage ($MSG_A) differs from what the funding script derived ($EXP_A)"
hex_to_bin "$MSG_A" "$VEC/legA_msg.bin"

# --- 7. leg B's REFUNDABLE address on BITCOIN, and its three coins -------------------------
# Leg B: u2 escrows 1 BTC for u1, pre-signed under u2's key (pk2). Claimed FIRST — settling it
# publishes the ADAPTED SIGNATURE, from which a holder of the matching pre-signature extracts
# the witness. The witness itself never goes on chain.
#
# THE ADDRESS IS THE TWO-LEAF ONE. `--refund-pk`/`--refund-locktime` select
# `taproot_for_refundable`, so this address is NOT the single-leaf address the same-venue
# runners use, and their pinned evidence is untouched by this run existing.
#
# THE OPERAND IS t2, A UNIX TIMESTAMP, NOT A BLOCK HEIGHT. Section 4b forced it above BIP65's
# LOCKTIME_THRESHOLD so CLTV reads it as a time; that is what makes it the same KIND of
# quantity as leg A's stored deadline and the ordering check meaningful. It also changes what
# has to happen to mature it: the refund battery advances MEDIAN TIME PAST, not the height.
REFUND_T="$T2"
REFUND_ARGS=(--refund-pk "$VEC/pk2.bin" --refund-locktime "$REFUND_T")

las_address() { # $1 = out json, $2 = log
  python3 "$TOOLS/btc_las_spend.py" sighash --core-src "$BTC_SRC" --pk "$VEC/pk2.bin" \
    "${REFUND_ARGS[@]}" --address-only --out "$1" > "$2" 2>&1 \
    || { cat "$2" >&2; fail_out "could not derive the refundable LAS taproot address"; }
  jqpy 'import json,sys;print(json.load(open(sys.argv[1]))["address"])' "$1"
}
ADDR_B="$(las_address "$OUT/legB_address.json" "$OUT/legB_address.log")"
DEST_B="$(cli_p getnewaddress u1_payout)"
DEST_B_SPK="$(cli_p getaddressinfo "$DEST_B" | jqpy 'import json,sys;print(json.load(sys.stdin)["scriptPubKey"])')"
REFUND_DEST="$(cli_p getnewaddress u2_refund)"
REFUND_DEST_SPK="$(cli_p getaddressinfo "$REFUND_DEST" | jqpy 'import json,sys;print(json.load(sys.stdin)["scriptPubKey"])')"

# THREE coins at the same address. B1 settles the swap. B2 is a REAL alternative outpoint so
# the input-binding control can point at a coin that exists — a nonexistent one would be
# refused for missing inputs and would prove nothing about the sighash covering inputs. B3 is
# never claimed, and exists so the refund battery runs on a LIVE coin without disturbing the
# settlement path: a refund test on B1 would have nothing left to spend.
fund() { # -> "txid vout value_sat"
  local txid bh vout val
  txid="$(cli_p sendtoaddress "$ADDR_B" "$COIN_BTC")"
  cli_p generatetoaddress 1 "$(cli_p getnewaddress)" > /dev/null
  bh="$(cli_p getbestblockhash)"
  vout="$(cli_p getrawtransaction "$txid" true "$bh" \
    | jqpy 'import json,sys;t=json.load(sys.stdin);a=sys.argv[1];print(next(o["n"] for o in t["vout"] if o["scriptPubKey"].get("address")==a))' "$ADDR_B")"
  val="$(cli_p getrawtransaction "$txid" true "$bh" \
    | jqpy 'import json,sys;t=json.load(sys.stdin);n=int(sys.argv[1]);print(int(round(next(o["value"] for o in t["vout"] if o["n"]==n)*1e8)))' "$vout")"
  echo "$txid $vout $val"
}
read -r FUND_B1 VOUT_B1 VAL_B1 <<< "$(fund)"
read -r FUND_B2 VOUT_B2 _      <<< "$(fund)"
read -r FUND_B3 VOUT_B3 VAL_B3 <<< "$(fund)"
sync_chain

# The refund deadline must still be strictly ahead of MEDIAN TIME PAST after funding, or the
# intended immature-refund control no longer exercises a pre-deadline state. MTP, not the wall
# clock and not the height: MTP is the quantity BIP113 finality is judged against. The three
# cases, exactly — a timestamp `nLockTime` is final only when it is STRICTLY below MTP:
#   MTP <  t2  before the deadline        MTP == t2  still NOT final        MTP >  t2  final
NOW_MTP="$(btc_mtp)"
[ "$NOW_MTP" -lt "$REFUND_T" ] \
  || fail_out "leg B's median time past ($NOW_MTP) is not strictly below its deadline $REFUND_T, so the intended before-deadline control would no longer exercise a strictly pre-deadline state. Raise DELTA_T2."

# --- 8. leg B's signed message is its own BIP341 sighash -----------------------------------
sighash_leg() { # $1 label, $2 txid, $3 vout, $4 value, $5 alt_txid, $6 alt_vout, $7 dest_spk, $8 mutate, then extra args
  local alt=()
  if [ -n "$5" ]; then alt=(--alt-txid "$5" --alt-vout "$6"); fi
  python3 "$TOOLS/btc_las_spend.py" sighash --core-src "$BTC_SRC" --pk "$VEC/pk2.bin" \
    "${REFUND_ARGS[@]}" \
    --txid "$2" --vout "$3" --value-sat "$4" --fee-sat "$FEE_SAT" \
    "${alt[@]}" --dest-spk "$7" --mutate "$8" "${@:9}" \
    --out-sighash "$OUT/$1_sighash.bin" --state "$OUT/$1_state.json" \
    --out "$OUT/$1_sighash.json" > "$OUT/$1_sighash.log" 2>&1
}
sighash_leg legB "$FUND_B1" "$VOUT_B1" "$VAL_B1" "$FUND_B2" "$VOUT_B2" "$DEST_B_SPK" none \
  || { cat "$OUT/legB_sighash.log" >&2; fail_out "leg B sighash failed"; }
cp "$OUT/legB_sighash.bin" "$VEC/legB_msg.bin"
MSG_B="$(jqpy 'import sys;print(open(sys.argv[1],"rb").read().hex())' "$VEC/legB_msg.bin")"
[ "${MSG_A#0x}" != "$MSG_B" ] \
  || fail_out "both legs derive the SAME message — one signature would settle both, so this is not a binding swap"
{
  echo "legA (ETHEREUM): chain_id=$GOT_ETH contract=$SWAP_A id=$ID_A message=$MSG_A"
  echo "  binds  : chain id, contract, escrow id, payer, beneficiary, amount"
  echo "  deadline: t1 = $T1 (absolute UNIX seconds), enforced against block.timestamp"
  echo "legB (BITCOIN) : coin=$FUND_B1:$VOUT_B1 -> $DEST_B  sighash=$MSG_B"
  echo "  binds  : this transaction's inputs and prevout amounts, and its outputs."
  echo "           NO chain id — BIP341's sighash has none."
  echo "  deadline: t2 = $REFUND_T (absolute UNIX seconds), enforced as a timestamp nLockTime"
  echo "            against MEDIAN TIME PAST"
  echo "  ⚠ t2 < t1 holds as a CONFIGURED numeric ordering, checked in section 4b. The two are"
  echo "    one numeric domain, NOT one consensus clock: median time past and block.timestamp"
  echo "    are different quantities on separate ledgers."
} | tee "$OUT/leg_messages.txt"

# --- 9. environment record -----------------------------------------------------------------
{
  echo "run_id=$RUN_ID"
  echo "git=$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo n/a)"
  echo "branch=$(git -C "$REPO" rev-parse --abbrev-ref HEAD 2>/dev/null || echo n/a)"
  echo "host=$(uname -s) $(uname -r) $(uname -m)"
  echo "cc=$(${CC:-cc} --version 2>/dev/null | head -1)"
  echo "--- venues ---"
  echo "legA=ETHEREUM anvil chain_id=$GOT_ETH rpc=$RPC_ETH hardfork=$HARDFORK (u1 -> u2, $COIN_WEI wei)"
  echo "legB=BITCOIN  regtest rpc=$RPCP/$RPCS tag=$BTC_TAG (u2 -> u1, $COIN_BTC BTC)"
  echo "ledger_separation=different clients, consensus rules and genesis states. NOT the"
  echo "ledger_separation=cross-offer control run_btc_two_leg.sh uses; that has no analogue."
  echo "legA_binds=chain id, contract, escrow id, payer, beneficiary, amount"
  echo "legB_binds=its transaction inputs and prevout amounts and its outputs; NO chain id"
  echo "deadlines=ONE NUMERIC DOMAIN: legA t1=$T1 and legB t2=$T2, both absolute UNIX seconds,"
  echo "deadlines=ordered t2 < t1 — CONFIGURED and checked by this run, NOT enforced by either"
  echo "deadlines=venue. NOT one clock: legB is judged against median time past (BIP113),"
  echo "deadlines=legA against block.timestamp, so t1-t2 is a configured gap and not a"
  echo "deadlines=guaranteed reaction window."
  echo "deadline_exercised=BOTH VENUES, each on a dedicated recovery-test object of its own"
  echo "deadline_exercised=kind — a control UTXO on Bitcoin, a second escrow on Ethereum."
  echo "deadline_exercised=NEITHER settled leg takes its refund path; both are claimed."
  echo "--- Ethereum ---"
  echo "anvil=$(anvil --version 2>/dev/null | head -1)"
  echo "cast=$(cast --version 2>/dev/null | head -1)"
  echo "forge=$(forge --version 2>/dev/null | head -1)"
  echo "tx_gas_cap=$TX_GAS_CAP  cap_control=PASSED (accepted at cap, refused at cap+1)"
  echo "settlement_contract=AdaptorSwapBound (message DERIVED from escrow state)"
  echo "--- Bitcoin Core ---"
  echo "btc_src=$BTC_SRC (HEAD == $BTC_TAG commit $SRC_TAG_COMMIT, plus the committed patch)"
  echo "patch_sha256=$COMMITTED_SHA"
  echo "applied_patch_sha256=$APPLIED_SHA (VERIFIED EQUAL; no untracked files)"
  echo "bitcoind_patched_sha256=$(sha256sum "$BTC_BIN_PATCHED" | cut -d' ' -f1)"
  echo "bitcoind_stock_sha256=$(sha256sum "$BTC_BIN_STOCK" | cut -d' ' -f1)"
  echo "opcode=OP_CHECKLASSIGVERIFY (0xbb, a BIP342 OP_SUCCESSx slot)"
  echo "instrument=generateblock submit=false — CONSENSUS validation, not mempool policy"
  echo "tapleaf=TWO leaves: claim <sha256(pk2)> OP_CHECKLASSIGVERIFY OP_1, and refund"
  echo "tapleaf=<$REFUND_T> OP_CHECKLOCKTIMEVERIFY OP_DROP <sha256(pk2)> OP_CHECKLASSIGVERIFY OP_1"
  echo "legB_address=$ADDR_B (the TWO-leaf address; NOT the single-leaf one the same-venue runners pin)"
  echo "refund_deadline_t2=$REFUND_T (a UNIX TIMESTAMP, not a height; median time past after"
  echo "  funding was $NOW_MTP). Timestamp finality is evaluated against MTP under BIP113;"
  echo "  the refund leaf itself is constrained by CLTV/BIP65."
  echo "legA_deadline_t1=$T1 (a UNIX TIMESTAMP, enforced against block.timestamp)"
  echo "deadline_ordering=t2 < t1, CONFIGURED and checked; one numeric domain, NOT one clock"
  echo "shim_selftest=PASSED"
  echo "--- LAS: ONE instance, TWO venue-specific verification mechanisms ---"
  echo "consensus_seed=$SEED (SHA-256 of \"$SEED_PREIMAGE\", VERIFIED)"
  echo "pp_seed=$SEED — the WHOLE swap was generated under the Bitcoin node's compiled-in"
  echo "pp_seed=seed, and the Ethereum leg registered that same A' as calldata. Both venues"
  echo "pp_seed=verify the SAME LAS instance; only the mechanism differs."
  echo "swap_tool=ref/$SWAP_TOOL"
  echo "pi=$([ "$PI" = "1" ] && echo "proved and verified (Fig. 1 steps 2-3)" || echo "OMITTED by PI=0 — INCOMPLETE, cannot pass, latest not moved")"
  echo "--- ⚠ FAIRNESS IS NOT ESTABLISHED ---"
  echo "caveat=The claim leaf is single-key under the FUNDER's key, so the funder can spend"
  echo "caveat=without waiting: the refund branch is NOT an exclusive recovery path."
  echo "caveat=The mempool exposes the ADAPTED SIGNATURE before confirmation — the funder,"
  echo "caveat=holding the matching pre-signature, can Ext from it and attempt a conflicting"
  echo "caveat=spend; if that confirms instead it keeps this coin and claims the other leg."
  echo "caveat=A timeout closes neither hole."
  echo "caveat=Supported wording: a cross-venue LAS settlement, plus both venues' timeout"
  echo "caveat=refund mechanisms exercised on dedicated recovery-test objects and the deadline"
  echo "caveat=ordering t2 < t1 configured and checked. NEVER 'a fair/full atomic swap'."
  for f in pp_normal.bin t1.bin t2.bin Y.bin pk1.bin pk2.bin; do
    [ -f "$VEC/$f" ] && echo "vector_${f%.bin}=sha256:$(sha256sum "$VEC/$f" | cut -c1-16)... $(stat -c%s "$VEC/$f") B"
  done
  [ -f "$VEC/pi.bin" ] && echo "vector_pi=sha256:$(sha256sum "$VEC/pi.bin" | cut -c1-16)... $(stat -c%s "$VEC/pi.bin") B"
} > "$OUT/environment.txt"

# --- 10. PreSign both legs, PreVerify both (Fig. 1 steps 4-5) -------------------------------
# One PreSign is over an EVM digest and the other over a BIP341 sighash; the function does
# not care, which is exactly the point — only the VERIFICATION differs between venues.
if ! (cd "$REPO/ref" && "./$SWAP_TOOL" presign "$VEC") > "$OUT/presign.log" 2>&1; then
  cat "$OUT/presign.log" >&2
  fail_out "PreSign/PreVerify failed — see presign.log (a PreVerify failure aborts Fig. 1 before any Adapt)"
fi
cat "$OUT/presign.log"

# --- 11. u1 adapts sigma_hat_2, and the node's own predicate is asked first -------------------
if ! (cd "$REPO/ref" && "./$SWAP_TOOL" adapt "$VEC") > "$OUT/adapt.log" 2>&1; then
  cat "$OUT/adapt.log" >&2
  fail_out "Adapt (u1: sigma_hat_2 -> sigma_2) failed — see adapt.log"
fi
cat "$OUT/adapt.log"

# The first place a cross-venue pp-seed error could show, so it is asked explicitly rather
# than left to surface as a rejected transaction whose cause has to be guessed at.
"$SHIM/las_btc_tool" verify "$VEC/pk2.bin" "$VEC/sigma2.bin" "$VEC/legB_msg.bin" \
  > "$OUT/shim_verify_sigma2.log" 2>&1 \
  || { cat "$OUT/shim_verify_sigma2.log" >&2; fail_out "the adapted sigma_2 is refused by LASConsensusVerify — the node would reject it too. If the vectors were generated under a different pp seed than the shim compiles in, this is where that shows."; }
cat "$OUT/shim_verify_sigma2.log"

# --- 12. the consensus instrument, and the differential control ------------------------------
: > "$OUT/controls.txt"
FAILED_CONTROLS=""

consensus_check() { # $1 = node fn, $2 = label, $3 = tag, $4 = raw hex
  if "$1" generateblock "$ADDR_B" "[\"$4\"]" false > "$OUT/$2_consensus_$3.json" 2> "$OUT/$2_consensus_$3.err"; then
    echo "ACCEPTED"; return 0
  fi
  if grep -q "TestBlockValidity failed:" "$OUT/$2_consensus_$3.err"; then
    echo "REJECTED: $(tr -d '\r' < "$OUT/$2_consensus_$3.err" | tr '\n' ' ' | sed 's/  */ /g' | cut -c1-160)"
    return 0
  fi
  return 1
}

ask() { # $1 = label, $2 = raw hex -> echoes "patched|stock"
  local p s
  p="$(consensus_check cli_p "$1" patched "$2")" \
    || fail_out "$1: the PATCHED node failed for a reason that is NOT a consensus rejection — see $1_consensus_patched.err. A non-consensus error must never be recorded as a refusal."
  s="$(consensus_check cli_s "$1" stock "$2")" \
    || fail_out "$1: the STOCK node failed for a reason that is NOT a consensus rejection — see $1_consensus_stock.err."
  case "$p$s" in
    *missing*|*"bad-txns-inputs"*)
      fail_out "$1: a node reported missing inputs — the nodes are out of sync and this verdict is not about the rule" ;;
  esac
  printf '%-34s patched: %-58s stock: %s\n' "$1" "$p" "$s" | tee -a "$OUT/controls.txt" >&2
  echo "$p|$s"
}

# THREE expectations, not two. `negative` is the usual differential: the patched node refuses
# and the STOCK node still accepts, which is what attributes the refusal to the new rule.
# `negative_both` exists for one case only — a transaction that is not yet FINAL. Finality is
# checked by block validation BEFORE any script runs, so EVERY node rejects it, patched or
# not. That case therefore evidences the deadline being enforced by consensus, and says
# NOTHING about the refund leaf; demanding stock ACCEPTED there would fail a correct run.
expect() { # $1 = label, $2 = "patched|stock", $3 = valid|negative|negative_both
  local v="$2" p s
  case "$v" in *"|"*) : ;; *) FAILED_CONTROLS="$FAILED_CONTROLS $1(unparseable-verdict)"; return ;; esac
  p="${v%%|*}"; s="${v##*|}"
  if [ -z "$p" ] || [ -z "$s" ]; then
    FAILED_CONTROLS="$FAILED_CONTROLS $1(empty-verdict)"; return
  fi
  case "$3" in
    valid)
      [ "$s" = "ACCEPTED" ] || FAILED_CONTROLS="$FAILED_CONTROLS $1(stock-not-ACCEPTED:not-attributable)"
      [ "$p" = "ACCEPTED" ] || FAILED_CONTROLS="$FAILED_CONTROLS $1(patched-did-not-accept-valid)" ;;
    negative)
      [ "$s" = "ACCEPTED" ] || FAILED_CONTROLS="$FAILED_CONTROLS $1(stock-not-ACCEPTED:not-attributable)"
      case "$p" in "REJECTED: "*) : ;; *) FAILED_CONTROLS="$FAILED_CONTROLS $1(patched-did-not-reject)" ;; esac ;;
    negative_both)
      case "$p" in "REJECTED: "*) : ;; *) FAILED_CONTROLS="$FAILED_CONTROLS $1(patched-did-not-reject)" ;; esac
      case "$s" in "REJECTED: "*) : ;; *) FAILED_CONTROLS="$FAILED_CONTROLS $1(stock-accepted-a-nonfinal-tx)" ;; esac ;;
  esac
}

judge() { # $1 = label, $2 = raw hex, $3 = valid|negative|negative_both
  local verdict
  verdict="$(ask "$1" "$2")" || fail_out "$1: could not obtain a verdict from both nodes"
  expect "$1" "$verdict" "$3"
}

assemble() { # $1 = state label, $2 = sig file, $3 = mutate, $4 = out label -> raw hex
  python3 "$TOOLS/btc_las_spend.py" assemble --core-src "$BTC_SRC" \
    --state "$OUT/$1_state.json" --sig "$2" --mutate "$3" \
    --out "$OUT/$4_spend.json" > "$OUT/$4_assemble.log" 2>&1 || return 1
  jqpy 'import json,sys;print(json.load(open(sys.argv[1]))["raw_hex"])' "$OUT/$4_spend.json"
}

CTLDIR="$OUT/control_keys"; mkdir -p "$CTLDIR"
control_sign() { # $1 = pk file, $2 = sk file, $3 = message file, $4 = out sig file
  cp "$1" "$CTLDIR/pk.bin"; cp "$2" "$CTLDIR/sk.bin"
  "$SHIM/las_btc_tool" sign "$CTLDIR" "$3" >> "$OUT/control_sign.log" 2>&1 || return 1
  cp "$CTLDIR/sig.bin" "$4"
}

# --- 13. leg B: the valid case, then the control battery --------------------------------------
LEGB_HEX="$(assemble legB "$VEC/sigma2.bin" none legB)" \
  || { cat "$OUT/legB_assemble.log" >&2; fail_out "assembling leg B failed"; }
[ -n "$LEGB_HEX" ] || fail_out "leg B assembled to an empty transaction"
judge legB_valid "$LEGB_HEX" valid
[ -z "$FAILED_CONTROLS" ] || fail_out "leg B's valid case did not behave as required:$FAILED_CONTROLS (patched must ACCEPT and stock must ACCEPT; without both, nothing below is attributable)"

for mut in output_amount output_recipient input_outpoint chunk_truncated chunk_reordered wrong_pubkey; do
  label="legB_${mut}"
  sighash_leg "$label" "$FUND_B1" "$VOUT_B1" "$VAL_B1" "$FUND_B2" "$VOUT_B2" "$DEST_B_SPK" "$mut" \
    || { cat "$OUT/${label}_sighash.log" >&2; fail_out "$label: sighash phase failed"; }
  hex="$(assemble "$label" "$VEC/sigma2.bin" "$mut" "$label")" \
    || { cat "$OUT/${label}_assemble.log" >&2; fail_out "$label: could not assemble"; }
  [ -n "$hex" ] || fail_out "$label: built an empty transaction"
  judge "$label" "$hex" negative
done

# A signature over a FALSE sighash: the transaction is honest, but the signer was told the
# input was worth more than it is. Reusing the honest signature would leave the transaction
# unchanged and test nothing, so this one signs what it mutated.
label="legB_wrong_prevout_amt"
sighash_leg "$label" "$FUND_B1" "$VOUT_B1" "$VAL_B1" "$FUND_B2" "$VOUT_B2" "$DEST_B_SPK" wrong_prevout_amt \
  || { cat "$OUT/${label}_sighash.log" >&2; fail_out "$label: sighash phase failed"; }
control_sign "$VEC/pk2.bin" "$VEC/sk2.bin" "$OUT/${label}_sighash.bin" "$OUT/sig_${label}.bin" \
  || fail_out "$label: control signing failed"
hex="$(assemble "$label" "$OUT/sig_${label}.bin" wrong_prevout_amt "$label")" || fail_out "$label: could not assemble"
judge "$label" "$hex" negative

# A genuine signature under the right key over a DIFFERENT real sighash.
label="legB_foreign_signature"
sighash_leg "${label}_src" "$FUND_B2" "$VOUT_B2" "$VAL_B1" "" "" "$DEST_B_SPK" none \
  || { cat "$OUT/${label}_src_sighash.log" >&2; fail_out "$label: foreign sighash failed"; }
control_sign "$VEC/pk2.bin" "$VEC/sk2.bin" "$OUT/${label}_src_sighash.bin" "$OUT/sig_${label}.bin" \
  || fail_out "$label: control signing failed"
hex="$(assemble legB "$OUT/sig_${label}.bin" none "$label")" || fail_out "$label: could not assemble"
judge "$label" "$hex" negative

# A signature over something that is not a sighash at all.
label="legB_non_sighash_msg"
head -c 32 /dev/zero | tr '\0' 'A' > "$OUT/${label}_msg.bin"
control_sign "$VEC/pk2.bin" "$VEC/sk2.bin" "$OUT/${label}_msg.bin" "$OUT/sig_${label}.bin" \
  || fail_out "$label: control signing failed"
hex="$(assemble legB "$OUT/sig_${label}.bin" none "$label")" || fail_out "$label: could not assemble"
judge "$label" "$hex" negative

# THE CROSS-VENUE BINDING CONTROL, BITCOIN SIDE. A GENUINE LAS signature, under leg B's OWN
# key, over LEG A's ETHEREUM digest. Only one variable differs from the settlement signature
# — which venue's message was signed — so a refusal is attributable to the message and not to
# the key, the format or the transaction.
label="legB_evm_message_signature"
control_sign "$VEC/pk2.bin" "$VEC/sk2.bin" "$VEC/legA_msg.bin" "$OUT/sig_${label}.bin" \
  || fail_out "$label: control signing failed"
hex="$(assemble legB "$OUT/sig_${label}.bin" none "$label")" || fail_out "$label: could not assemble"
judge "$label" "$hex" negative

[ -z "$FAILED_CONTROLS" ] || fail_out "leg B negative controls behaved wrongly:$FAILED_CONTROLS"

# --- 14. SETTLE leg B on Bitcoin ---------------------------------------------------------------
cli_p generateblock "$(cli_p getnewaddress)" "[\"$LEGB_HEX\"]" true > "$OUT/legB_settle.json" 2> "$OUT/legB_settle.err" \
  || { cat "$OUT/legB_settle.err" >&2; fail_out "the patched node refused to MINE leg B"; }
BH_B="$(jqpy 'import json,sys;print(json.load(open(sys.argv[1]))["hash"])' "$OUT/legB_settle.json")"
sync_chain
TXID_B="$(cli_p getblock "$BH_B" | jqpy 'import json,sys;print(json.load(sys.stdin)["tx"][1])')"
cli_p getrawtransaction "$TXID_B" true "$BH_B" > "$OUT/legB_mined.json"
cli_s getblock "$BH_B" > "$OUT/legB_block_seen_by_stock.json" \
  || fail_out "the stock node does not have the block containing leg B"

# --- 15. recover sigma_2 FROM THE MINED WITNESS -------------------------------------------------
python3 "$TOOLS/btc_recover_sig.py" --tx-json "$OUT/legB_mined.json" \
  --out-sig "$VEC/sigma2_from_chain.bin" --out-pk "$OUT/pk2_from_chain.bin" \
  --out "$OUT/recovered_sigB.json" > "$OUT/recover_sigB.log" 2>&1 \
  || { cat "$OUT/recover_sigB.log" >&2; fail_out "could not recover sigma_2 from the mined witness"; }
cat "$OUT/recover_sigB.log"

# --- 16. u2: Ext from BITCOIN's bytes, then Adapt leg A -------------------------------------------
# THE CROSS-VENUE STEP. `extract_and_adapt` legitimately reads the protocol state u2 already
# holds — pp, Y, both public keys, both leg messages, and its own sigma_hat_2. What it must NOT
# have is a local copy of the adapted signature, and it does not: that input is a path argument
# pointing at the bytes recovered from the mined Bitcoin witness, and the program never opens
# sigma2.bin. So the ADAPTED-SIGNATURE INPUT to Ext comes exclusively from the ledger, and what
# Ext recovers is what settles the ETHEREUM leg.
if ! (cd "$REPO/ref" && ./test/extract_and_adapt "$VEC" "$VEC/sigma2_from_chain.bin") \
      > "$OUT/extract_adapt.log" 2>&1; then
  cat "$OUT/extract_adapt.log" >&2
  fail_out "Ext/Adapt from the chain-observed signature failed — see extract_adapt.log"
fi
cat "$OUT/extract_adapt.log"

# --- 17. THE REFUND BATTERY, on coin B3 -----------------------------------------------------------
# The BITCOIN half of the recovery layer, on a coin the swap never touches; Ethereum's half is
# section 18b. Sec 4.1 states the two-timeout setup while RECALLING the classical protocol of
# its [23] — Fig. 1 restates none — so this is an addition around Fig. 1, not a repair to it.
#
# "REFUSED BEFORE THE DEADLINE" HAS TWO DIFFERENT CAUSES AND ONLY ONE IS ABOUT CLTV. Both are
# run, separately, because a test that conflated them would report the wrong reason:
#
#   A  nLockTime = the deadline, chain not yet there. The transaction is NOT FINAL, and block
#      validation refuses it BEFORE any script runs. EVERY node refuses, patched or stock —
#      hence `negative_both`. Evidences the deadline; evidences NOTHING about the refund leaf.
#   B  nLockTime set to a TIMESTAMP strictly below median time past — so the transaction is
#      final and the script DOES run — while the leaf's CLTV operand is still the deadline.
#      CLTV then refuses it. The patched node rejects; the STOCK node accepts, because 0xbb is
#      an OP_SUCCESSx and BIP342 makes the whole script succeed without executing a single
#      opcode. THIS is the case that attributes the refusal to the refund leaf.
#      ⚠ NOT nLockTime = 0, which this case used while the operand was a block height. BIP65
#      requires both to be the SAME KIND, so against a timestamp operand a zero nLockTime
#      would be refused for a TYPE MISMATCH — still a CLTV refusal, but a different rule from
#      the one this case exists to isolate. A past timestamp keeps the failing rule the
#      magnitude comparison.
#   C  median time past advanced past the deadline, nLockTime = the deadline. Accepted, then
#      actually MINED, because a validity verdict is not a payout — the coin must be seen to
#      return.
refund_case() { # $1 label, $2... extra sighash args
  sighash_leg "$1" "$FUND_B3" "$VOUT_B3" "$VAL_B3" "" "" "$REFUND_DEST_SPK" none \
    --path refund "${@:2}" \
    || { cat "$OUT/$1_sighash.log" >&2; fail_out "$1: refund sighash phase failed"; }
  control_sign "$VEC/pk2.bin" "$VEC/sk2.bin" "$OUT/$1_sighash.bin" "$OUT/sig_$1.bin" \
    || fail_out "$1: refund signing failed"
  assemble "$1" "$OUT/sig_$1.bin" none "$1" \
    || { cat "$OUT/$1_assemble.log" >&2; fail_out "$1: could not assemble the refund"; }
}

# A — immature, and refused for NON-FINALITY.
HEX_A="$(refund_case refund_immature)"
[ -n "$HEX_A" ] || fail_out "refund_immature assembled to an empty transaction"
judge refund_immature "$HEX_A" negative_both
grep -qi "non-final\|nonfinal" "$OUT/refund_immature_consensus_patched.err" \
  || fail_out "refund_immature was refused, but NOT for non-finality — the case is meant to isolate the finality rule, and its reason must say so (see refund_immature_consensus_patched.err)"

# B — final, but below the deadline: refused by CLTV specifically. The nLockTime is a
# timestamp STRICTLY below median time past, so it is final (a timestamp nLockTime is final
# only when strictly below MTP) and it is the same KIND as the operand, leaving the magnitude
# comparison as the only rule that can refuse it.
MTP_B="$(btc_mtp)"
LOCK_FINAL=$(( MTP_B - 1 ))
[ "$LOCK_FINAL" -ge "$LOCKTIME_THRESHOLD" ] \
  || fail_out "the control nLockTime $LOCK_FINAL is below BIP65's LOCKTIME_THRESHOLD, so CLTV would refuse it for a TYPE MISMATCH against the timestamp operand rather than for being below the deadline — the case would no longer isolate the leaf"
[ "$LOCK_FINAL" -lt "$REFUND_T" ] \
  || fail_out "the control nLockTime $LOCK_FINAL is not below leg B's deadline $REFUND_T, so CLTV's magnitude check would be satisfied and this case would test nothing"
HEX_B="$(refund_case refund_cltv --tx-locktime "$LOCK_FINAL")"
[ -n "$HEX_B" ] || fail_out "refund_cltv assembled to an empty transaction"
judge refund_cltv "$HEX_B" negative
grep -qi "locktime" "$OUT/refund_cltv_consensus_patched.err" \
  || fail_out "refund_cltv was refused by the patched node, but the reason does not mention locktime — the refusal is not attributable to OP_CHECKLOCKTIMEVERIFY rather than to the signature check (see refund_cltv_consensus_patched.err)"

[ -z "$FAILED_CONTROLS" ] || fail_out "the premature-refund controls behaved wrongly:$FAILED_CONTROLS"

# C — advance MEDIAN TIME PAST past the deadline, then refund for real.
#
# The deadline is a timestamp now, so height does not mature it: what has to move is MTP, the
# median of the last 11 blocks' times. `setmocktime` moves the node's clock, and 12 newly
# mined blocks ensure the current 11-block MTP window consists entirely of blocks produced
# after that jump, driving MTP past the deadline. Individual later block timestamps may exceed
# MATURE_AT, because Bitcoin requires each new block time to be greater than the previous MTP.
# BOTH nodes are set, not just the generating one: a block more than two hours ahead of a
# node's own clock is refused as too far in the future, so leaving the stock node behind would
# break the differential for any DELTA_T2 near that bound rather than for a reason about the
# rule.
MATURE_AT=$(( REFUND_T + 600 ))
cli_p setmocktime "$MATURE_AT" > /dev/null || fail_out "could not set mocktime on the patched node"
cli_s setmocktime "$MATURE_AT" > /dev/null || fail_out "could not set mocktime on the stock node"
mine_b 12
MATURE_MTP="$(btc_mtp)"
# STRICTLY greater: at MTP == t2 a timestamp nLockTime is still not final, so equality here
# would leave case C testing the non-finality rule instead of the matured refund.
[ "$MATURE_MTP" -gt "$REFUND_T" ] \
  || fail_out "median time past is $MATURE_MTP, not strictly past leg B's deadline $REFUND_T — the refund is not yet mature and case C would not exercise the matured branch"
HEX_C="$(refund_case refund_mature)"
[ -n "$HEX_C" ] || fail_out "refund_mature assembled to an empty transaction"
judge refund_mature "$HEX_C" valid
[ -z "$FAILED_CONTROLS" ] || fail_out "the matured refund did not behave as required:$FAILED_CONTROLS"

cli_p generateblock "$(cli_p getnewaddress)" "[\"$HEX_C\"]" true > "$OUT/refund_settle.json" 2> "$OUT/refund_settle.err" \
  || { cat "$OUT/refund_settle.err" >&2; fail_out "the patched node validated the matured refund but refused to MINE it"; }
BH_R="$(jqpy 'import json,sys;print(json.load(open(sys.argv[1]))["hash"])' "$OUT/refund_settle.json")"
sync_chain
TXID_R="$(cli_p getblock "$BH_R" | jqpy 'import json,sys;print(json.load(sys.stdin)["tx"][1])')"
cli_p getrawtransaction "$TXID_R" true "$BH_R" > "$OUT/refund_mined.json"
cli_p gettxout "$FUND_B3" "$VOUT_B3" > "$OUT/refund_funding_utxo_after.json" 2>/dev/null || true
cli_p gettxout "$TXID_R" 0 > "$OUT/refund_payout_utxo.json" 2>/dev/null || true

# --- 18. leg A: the cross-venue binding control, then SETTLE on Ethereum ----------------------------
cd "$REPO/evm"
build_claim() { # $1 outdir, $2 sig path, $3 escrow id, $4 leg message, $5 log
  OUT_DIR="$1" SIG_PATH="$2" ESCROW_ID="$3" LEG_MESSAGE="$4" \
  forge script script/TwoLeg.s.sol:TwoLegClaim > "$5" 2>&1
}

# THE CROSS-VENUE BINDING CONTROL, ETHEREUM SIDE — the mirror of legB_evm_message_signature,
# run BEFORE the real claim so a success here cannot be masked by the escrow already being
# closed. Established two ways, because a non-zero `cast` exit alone would also be produced by
# a bad nonce, a dead RPC or a tool error: (a) `eth_call` must revert with the CONTRACT'S OWN
# reason, `message not bound`; (b) a REAL broadcast must leave the escrow's balance unchanged,
# which holds however the send failed.
build_claim xchain/eth "$VEC_REL/sigma1.bin" "$ID_A" "0x$MSG_B" "$OUT/build_replay.log" \
  || { cat "$OUT/build_replay.log" >&2; fail_out "building the cross-venue replay calldata failed"; }
cp xchain/eth/claim.calldata "$OUT/replay.calldata"

ESCROW_A_BEFORE_REPLAY="$(cast balance "$SWAP_A" --rpc-url "$RPC_ETH")"
REPLAY_RC=0
cast call "$SWAP_A" --data "$(cat xchain/eth/claim.calldata)" --from "$U2_ADDR" \
  --rpc-url "$RPC_ETH" > "$OUT/replay_call.log" 2>&1 || REPLAY_RC=$?
echo "cast exit code: $REPLAY_RC" >> "$OUT/replay_call.log"
REPLAY_REASON_OK=0
grep -qi "message not bound" "$OUT/replay_call.log" && REPLAY_REASON_OK=1
[ "$REPLAY_RC" -ne 0 ] || fail_out "CROSS-VENUE REPLAY ACCEPTED: leg A's claim carrying leg B's BITCOIN sighash did not revert under eth_call. The signatures do not bind their legs across venues (see replay_call.log)"
[ "$REPLAY_REASON_OK" -eq 1 ] || fail_out "the cross-venue replay call failed, but NOT with the contract's 'message not bound' reason — the refusal is not attributable to message binding (see replay_call.log)"

REPLAY_SEND_RC=0
cast send "$SWAP_A" --data "$(cat xchain/eth/claim.calldata)" \
  --gas-limit "$TX_GAS_CAP" --private-key "$U2_KEY" --rpc-url "$RPC_ETH" \
  > "$OUT/replay_send.log" 2>&1 || REPLAY_SEND_RC=$?
echo "cast exit code: $REPLAY_SEND_RC" >> "$OUT/replay_send.log"
ESCROW_A_AFTER_REPLAY="$(cast balance "$SWAP_A" --rpc-url "$RPC_ETH")"
[ "$ESCROW_A_BEFORE_REPLAY" = "$ESCROW_A_AFTER_REPLAY" ] \
  || fail_out "THE CROSS-VENUE REPLAY SETTLED THE ESCROW: leg A's balance changed across a claim carrying leg B's Bitcoin sighash ($ESCROW_A_BEFORE_REPLAY -> $ESCROW_A_AFTER_REPLAY)"
echo "cross-venue replay control OK: refused with the contract's own reason; a real send left the escrow untouched"

build_claim xchain/eth "$VEC_REL/sigma1.bin" "$ID_A" "$MSG_A" "$OUT/build_legA.log" \
  || { cat "$OUT/build_legA.log" >&2; fail_out "building leg A claim calldata failed"; }
cp xchain/eth/claim.calldata "$OUT/legA.calldata"

# u2 is leg A's beneficiary AND the sender, so it pays the fee for its own payout; the balance
# is snapshotted before the send and the fee added back from the receipt.
BAL_U2_BEFORE="$(cast balance "$U2_ADDR" --rpc-url "$RPC_ETH")"
SEND_RC=0
SEND_A="$(cast send "$SWAP_A" --data "$(cat xchain/eth/claim.calldata)" \
  --gas-limit "$TX_GAS_CAP" --private-key "$U2_KEY" --rpc-url "$RPC_ETH" --json \
  2> "$OUT/legA_send.log")" || SEND_RC=$?
printf '%s\n' "$SEND_A" >> "$OUT/legA_send.log"
[ "$SEND_RC" -eq 0 ] || { cat "$OUT/legA_send.log" >&2; fail_out "the client REJECTED leg A's claim (cast exit $SEND_RC). The control showed a trivial transfer IS accepted at this same limit, so the refusal is about this transaction."; }

TX_A="$(printf '%s' "$SEND_A" | python3 -c 'import json,sys; print(json.load(sys.stdin)["transactionHash"])')"
cast receipt "$TX_A" --rpc-url "$RPC_ETH" --json > "$OUT/legA_receipt.json"
cast tx      "$TX_A" --rpc-url "$RPC_ETH" --json > "$OUT/legA_tx.json"
BAL_U2_AFTER="$(cast balance "$U2_ADDR" --rpc-url "$RPC_ETH")"
ESCROW_A_BAL="$(cast balance "$SWAP_A" --rpc-url "$RPC_ETH")"

# --- 18b. THE ETHEREUM REFUND BATTERY -----------------------------------------------------------------
# The other half of the recovery layer. Bitcoin's deadline was exercised on coin B3; this
# exercises leg A's, on a SECOND ESCROW funded on the same contract with the same deadline t1
# and the same registered context. A second escrow, not leg A itself, for the reason coin B3
# exists: leg A has settled, and a refund test on a settled escrow could only ever observe the
# state check. What each case shows is different, and they are kept apart:
#
#   E1  before t1, by the payer          -> refused, "before timeout"   (the deadline itself)
#   E2  after t1, by someone else        -> refused, "not payer"        (who may recover)
#   E3  after t1, by the payer           -> ACCEPTED, and the coin returns
#   E4  after t1, the SETTLED leg A      -> refused, "not open"
#
# E4 speaks to LOCAL STATE EXCLUSIVITY, not to deadline recovery and not to cross-chain
# atomicity: once leg A is CLAIMED, passing the deadline cannot make that same escrow
# refundable. It is a property of one contract's state machine, nothing wider.
ETH_STATE_OPEN=1; ETH_STATE_CLAIMED=2; ETH_STATE_REFUNDED=3

eth_refund_refused() { # $1 label, $2 escrow id, $3 from, $4 expected contract reason
  local rc=0
  cast call "$SWAP_A" "refund(uint256)" "$2" --from "$3" --rpc-url "$RPC_ETH" \
    > "$OUT/ethrefund_$1.log" 2>&1 || rc=$?
  echo "cast exit code: $rc" >> "$OUT/ethrefund_$1.log"
  [ "$rc" -ne 0 ] \
    || fail_out "ETH refund control $1: refund($2) from $3 did NOT revert — see ethrefund_$1.log"
  grep -qi "$4" "$OUT/ethrefund_$1.log" \
    || fail_out "ETH refund control $1: refund($2) reverted, but NOT with the contract's own '$4' reason, so the refusal is not attributable to that rule — see ethrefund_$1.log"
}

eth_escrow_field() { # $1 escrow id, $2 field index
  local raw
  raw="$(eth_call "$SWAP_A" "$SWAPS_SIG" "$1")" || return 1
  printf '%s\n' "$raw" | sed -n "$(( $2 + 1 ))p"
}
eth_tx_hash() { printf '%s' "$1" | python3 -c 'import json,sys
try:
    print(json.load(sys.stdin)["transactionHash"])
except Exception:
    pass'; }
eth_receipt_status() { # $1 receipt json path
  jqpy 'import json,sys;s=str(json.load(open(sys.argv[1]))["status"]);print(int(s,16) if s.startswith("0x") else int(s))' "$1"
}

# Leg A must be CLAIMED before E4 means anything.
STATE_A_AFTER="$(eth_escrow_field "$ID_A" 4)" || fail_out "could not re-read leg A's escrow"
[ "$STATE_A_AFTER" = "$ETH_STATE_CLAIMED" ] \
  || fail_out "leg A's escrow is in state $STATE_A_AFTER, not CLAIMED ($ETH_STATE_CLAIMED) — E4 would then not be testing a settled leg"

# The refund escrow. Same deadline, same context, funded by u1 exactly as leg A was; `nextId`
# is read first because `fundLASBound` returns the id to the caller, not to a receipt field.
ID_R="$(eth_call "$SWAP_A" "nextId()(uint256)")" || fail_out "could not read nextId"
cast send "$SWAP_A" "fundLASBound(address,uint64,bytes32)" "$U2_ADDR" "$T1" "$CTX_A" \
  --value "$COIN_WEI" --private-key "$U1_KEY" --rpc-url "$RPC_ETH" \
  > "$OUT/ethrefund_fund.log" 2>&1 \
  || { cat "$OUT/ethrefund_fund.log" >&2; fail_out "could not fund the Ethereum refund escrow"; }

SW_R_RAW="$(eth_call "$SWAP_A" "$SWAPS_SIG" "$ID_R")" || fail_out "could not read the refund escrow"
mapfile -t SW_R <<< "$SW_R_RAW"
[ "${#SW_R[@]}" -eq 6 ] || fail_out "the refund escrow decoded to ${#SW_R[@]} fields, expected 6"
[ "${SW_R[0],,}" = "${U1_ADDR,,}" ] \
  || fail_out "the refund escrow's payer is ${SW_R[0]}, not u1 ($U1_ADDR) — E2/E3 would not be testing the payer check"
[ "${SW_R[2]}" = "$COIN_WEI" ] || fail_out "the refund escrow holds ${SW_R[2]} wei, expected $COIN_WEI"
[ "${SW_R[3]}" = "$T1" ] \
  || fail_out "the refund escrow's deadline is ${SW_R[3]}, not t1=$T1 — it would not be exercising leg A's own deadline"
[ "${SW_R[4]}" = "$ETH_STATE_OPEN" ] || fail_out "the refund escrow is not OPEN (state ${SW_R[4]})"

# E1 — before the deadline, two ways, as the cross-venue replay control does it: the call must
# revert with the contract's OWN reason, and a broadcast must leave the escrow untouched.
#
# TWO THINGS MAKE THE SECOND HALF REAL, and it is worth nothing without both. The EXPLICIT GAS
# LIMIT stops `cast send` from halting at gas estimation, which sees the same revert and
# broadcasts nothing. The RECEIPT then proves a transaction was actually mined and reverted —
# without it, an RPC or nonce failure would leave the balance unchanged for a reason that says
# nothing about the contract. `cast`'s exit code is recorded but deliberately NOT gated: a
# mined revert may or may not set it, so the receipt is the evidence.
ETH_NOW_E1="$(eth_now)"
[ "$ETH_NOW_E1" -lt "$T1" ] \
  || fail_out "the chain is already at $ETH_NOW_E1, not before t1=$T1 — E1 would not be a before-deadline case"
eth_refund_refused e1_before_timeout "$ID_R" "$U1_ADDR" "before timeout"
BAL_C_BEFORE_E1="$(cast balance "$SWAP_A" --rpc-url "$RPC_ETH")"
E1_SEND_RC=0
E1_SEND="$(cast send "$SWAP_A" "refund(uint256)" "$ID_R" --gas-limit "$TX_GAS_CAP" \
  --private-key "$U1_KEY" --rpc-url "$RPC_ETH" --json 2> "$OUT/ethrefund_e1_send.log")" || E1_SEND_RC=$?
printf '%s\n' "$E1_SEND" >> "$OUT/ethrefund_e1_send.log"
echo "cast exit code: $E1_SEND_RC" >> "$OUT/ethrefund_e1_send.log"
TX_E1="$(eth_tx_hash "$E1_SEND")"
[ -n "$TX_E1" ] \
  || fail_out "the premature refund produced no transaction hash, so it was never mined — the unchanged balance below would then evidence nothing about the contract. See ethrefund_e1_send.log"
cast receipt "$TX_E1" --rpc-url "$RPC_ETH" --json > "$OUT/ethrefund_e1_receipt.json" \
  || fail_out "could not fetch the receipt for the premature refund $TX_E1"
STATUS_E1="$(eth_receipt_status "$OUT/ethrefund_e1_receipt.json")"
[ "$STATUS_E1" = "0" ] \
  || fail_out "THE PREMATURE REFUND SUCCEEDED: refund($ID_R) was mined with receipt status $STATUS_E1 before t1=$T1, so the deadline is not enforced"
BAL_C_AFTER_E1="$(cast balance "$SWAP_A" --rpc-url "$RPC_ETH")"
STATE_R_AFTER_E1="$(eth_escrow_field "$ID_R" 4)" || fail_out "could not re-read the refund escrow after E1"
[ "$BAL_C_BEFORE_E1" = "$BAL_C_AFTER_E1" ] \
  || fail_out "A PREMATURE REFUND MOVED FUNDS: the contract's balance changed across a mined pre-deadline refund ($BAL_C_BEFORE_E1 -> $BAL_C_AFTER_E1)"
[ "$STATE_R_AFTER_E1" = "$ETH_STATE_OPEN" ] \
  || fail_out "A PREMATURE REFUND CHANGED THE ESCROW: state went OPEN -> $STATE_R_AFTER_E1 across a mined pre-deadline refund"

# Past the deadline. anvil's clock is moved, then the result is READ BACK: the assertion is on
# the chain's own timestamp, not on the RPC having returned successfully.
eth_travel_past() { # $1 = target
  local now target
  now="$(eth_now)"; target=$(( $1 + 60 ))
  [ "$now" -ge "$target" ] && return 0
  echo "--- advancing from $now to $target ---" >> "$OUT/eth_timetravel.log"
  cast rpc evm_setNextBlockTimestamp "$target" --rpc-url "$RPC_ETH" >> "$OUT/eth_timetravel.log" 2>&1 \
    || cast rpc evm_increaseTime "$(( target - now ))" --rpc-url "$RPC_ETH" >> "$OUT/eth_timetravel.log" 2>&1 \
    || fail_out "the Ethereum node accepted neither evm_setNextBlockTimestamp nor evm_increaseTime — see eth_timetravel.log"
  cast rpc evm_mine --rpc-url "$RPC_ETH" >> "$OUT/eth_timetravel.log" 2>&1 \
    || fail_out "evm_mine failed after advancing the clock — see eth_timetravel.log"
}
eth_travel_past "$T1"
ETH_NOW_MATURE="$(eth_now)"
[ "$ETH_NOW_MATURE" -ge "$T1" ] \
  || fail_out "the Ethereum chain is at $ETH_NOW_MATURE, still short of t1=$T1 — the matured cases would not be matured"

# E2 — matured, but the caller is not the payer.
eth_refund_refused e2_not_payer "$ID_R" "$U2_ADDR" "not payer"

# E4 — matured, but the escrow is the SETTLED leg A. Run before E3 so a success here could not
# be mistaken for the refund escrow's own payout.
eth_refund_refused e4_settled_leg "$ID_A" "$U1_ADDR" "not open"

# E3 — matured, by the payer: the coin must actually come back. GATED, not merely recorded —
# `cast send` can exit zero on a transaction that reverted, so the receipt status, the escrow's
# resulting state and the money actually leaving the contract are all asserted. The CONTRACT's
# balance is the payout evidence rather than u1's, because u1 also pays this transaction's gas.
BAL_C_BEFORE_E3="$(cast balance "$SWAP_A" --rpc-url "$RPC_ETH")"
REFUND_SEND="$(cast send "$SWAP_A" "refund(uint256)" "$ID_R" \
  --private-key "$U1_KEY" --rpc-url "$RPC_ETH" --json 2> "$OUT/ethrefund_e3_send.log")" \
  || { cat "$OUT/ethrefund_e3_send.log" >&2; fail_out "the matured Ethereum refund was REJECTED by the client — see ethrefund_e3_send.log"; }
printf '%s\n' "$REFUND_SEND" >> "$OUT/ethrefund_e3_send.log"
TX_R_ETH="$(eth_tx_hash "$REFUND_SEND")"
[ -n "$TX_R_ETH" ] || fail_out "the matured Ethereum refund produced no transaction hash — see ethrefund_e3_send.log"
cast receipt "$TX_R_ETH" --rpc-url "$RPC_ETH" --json > "$OUT/ethrefund_receipt.json" \
  || fail_out "could not fetch the receipt for the matured refund $TX_R_ETH"
STATUS_R="$(eth_receipt_status "$OUT/ethrefund_receipt.json")"
[ "$STATUS_R" = "1" ] \
  || fail_out "the matured Ethereum refund was mined but REVERTED (receipt status $STATUS_R) — see ethrefund_receipt.json"
STATE_R_AFTER="$(eth_escrow_field "$ID_R" 4)" || fail_out "could not re-read the refund escrow"
[ "$STATE_R_AFTER" = "$ETH_STATE_REFUNDED" ] \
  || fail_out "the Ethereum refund escrow ended in state $STATE_R_AFTER, expected REFUNDED ($ETH_STATE_REFUNDED) — the transaction succeeded without the escrow recording a refund"
ESCROW_R_BAL="$(cast balance "$SWAP_A" --rpc-url "$RPC_ETH")"
[ "$(( BAL_C_BEFORE_E3 - ESCROW_R_BAL ))" = "$COIN_WEI" ] \
  || fail_out "the matured refund did not move the escrowed coin out of the contract: balance went $BAL_C_BEFORE_E3 -> $ESCROW_R_BAL, expected a fall of exactly $COIN_WEI"
[ "$ESCROW_R_BAL" = "0" ] \
  || fail_out "the contract still holds $ESCROW_R_BAL wei after leg A was claimed and the refund-test escrow was refunded — something remains escrowed that this run did not account for"
{
  echo "eth refund escrow id     : $ID_R (payer u1, beneficiary u2, deadline t1=$T1)"
  echo "chain timestamp at E1    : $ETH_NOW_E1  (strictly before t1)"
  echo "chain timestamp at E2-E4 : $ETH_NOW_MATURE  (at or past t1)"
  echo "E1 before t1, payer      : eth_call REVERTED 'before timeout'; a broadcast forced past"
  echo "                           gas estimation was MINED with status $STATUS_E1, left the"
  echo "                           balance at $BAL_C_AFTER_E1 and the escrow OPEN"
  echo "E2 after t1, not payer   : REVERTED 'not payer'"
  echo "E4 after t1, settled legA: REVERTED 'not open' (local state exclusivity)"
  echo "E3 after t1, payer       : receipt status $STATUS_R, escrow state $STATE_R_AFTER (REFUNDED=$ETH_STATE_REFUNDED)"
  echo "contract balance         : $BAL_C_BEFORE_E3 -> $ESCROW_R_BAL wei (fell by exactly $COIN_WEI)"
} | tee "$OUT/eth_refund.txt"

# --- 19. did the Bitcoin coins actually move? --------------------------------------------------------
cli_p gettxout "$FUND_B1" "$VOUT_B1" > "$OUT/legB_funding_utxo_after.json" 2>/dev/null || true
cli_p gettxout "$TXID_B" 0 > "$OUT/legB_payout_utxo.json" 2>/dev/null || true
python3 "$TOOLS/btc_model_check.py" --decoded "$OUT/legB_mined.json" --label "LAS legB" \
  --json-out "$OUT/legB_sizecheck.json" | tee "$OUT/legB_size_model.txt" \
  || fail_out "the size model disagrees with the client on leg B"

# --- 20. verdict, computed FROM both clients' records and the retained bytes ---------------------------
set +o pipefail
python3 - "$OUT/legB_mined.json" "$OUT/legA_receipt.json" "$OUT/legA_tx.json" \
         "$VEC/sigma2.bin" "$VEC/sigma2_from_chain.bin" "$VEC/sigma1.bin" \
         "$VEC/witness.bin" "$VEC/witness_extracted.bin" \
         "$OUT/legB_funding_utxo_after.json" "$OUT/legB_payout_utxo.json" \
         "$DEST_B" "$VAL_B1" "$FEE_SAT" "$OUT/recovered_sigB.json" \
         "$TX_GAS_CAP" "$OUT/legA.calldata" \
         "$BAL_U2_BEFORE" "$BAL_U2_AFTER" "$COIN_WEI" "$ESCROW_A_BAL" \
         "$REPLAY_RC" "$REPLAY_REASON_OK" "$PI" \
         "$OUT/refund_funding_utxo_after.json" "$OUT/refund_payout_utxo.json" \
         "$REFUND_DEST" "$VAL_B3" "$REFUND_T" "$MATURE_MTP" \
         "$T2" "$T1" "$ETH_NOW_MATURE" "$STATE_R_AFTER" "$ESCROW_R_BAL" "$STATUS_R" "$STATUS_E1" \
         <<'PY' | tee -a "$OUT/verdict.txt"
import hashlib, json, os, sys

(minedB, rcptA, txAj, sig2_local, sig2_chain, sig1_local, wit_honest, wit_ext,
 fundB_after, payB, destB, valB, fee, recjson, cap, cdA,
 u2_before, u2_after, coin, escrowA, replay_rc, replay_reason_ok, pi,
 rfund_after, rpay, rdest, valB3, refund_t, mature_mtp,
 t2, t1, eth_mature, state_r, escrow_r, status_r, status_e1) = sys.argv[1:37]
valB, fee, cap, coin, valB3 = int(valB), int(fee), int(cap), int(coin), int(valB3)
t2, t1 = int(t2), int(t1)

def blob(p):
    return open(p, "rb").read() if os.path.exists(p) else b""

def js(p):
    if not os.path.exists(p) or os.path.getsize(p) == 0:
        return None
    try:
        return json.load(open(p))
    except json.JSONDecodeError:
        return None

def num(v):
    s = str(v)
    return int(s, 16) if s.startswith("0x") else int(s)

def hexbytes(s):
    s = s.strip().lower()
    return bytes.fromhex(s[2:] if s.startswith("0x") else s)

def sha(b):
    return hashlib.sha256(b).hexdigest()[:32] if b else "(absent)"

def addr_of(u):
    if not u:
        return None
    spk = u.get("scriptPubKey", {})
    if spk.get("address"):
        return spk["address"]
    a = spk.get("addresses") or []
    return a[0] if a else None

def sat(u):
    return int(round(u["value"] * 1e8)) if u else None

tb = json.load(open(minedB))
ra, ta = json.load(open(rcptA)), json.load(open(txAj))
rec = json.load(open(recjson))
s2l, s2c, s1l = blob(sig2_local), blob(sig2_chain), blob(sig1_local)
wh, we = blob(wit_honest), blob(wit_ext)
sent_a, recv_a = hexbytes(open(cdA).read()), hexbytes(ta["input"])

print("=== leg B — BITCOIN (settled first; publishes the adapted signature Ext runs on) ===")
print("settlement tx       : %s" % tb.get("txid"))
print("block               : %s (confirmations %s)" % (tb.get("blockhash"), tb.get("confirmations")))
print()
print("=== leg A — ETHEREUM (settled second, from the witness Bitcoin published) ===")
print("claim tx            : %s" % ra["transactionHash"])
print("status              : %s" % ("SUCCESS" if num(ra["status"]) == 1 else "FAILED"))
print("gasLimit on tx      : %d  (EIP-7825 cap %d)" % (num(ta.get("gas", ta.get("gasLimit"))), cap))
print("gasUsed             : %d" % num(ra["gasUsed"]))
print("calldata            : %d received / %d expected, byte-equal %s"
      % (len(recv_a), len(sent_a), "YES" if recv_a == sent_a else "NO"))
print()
print("=== the witness crossing from Bitcoin to Ethereum ===")
print("sigma_2 recovered from the MINED BITCOIN witness : %d bytes, split at offset %d"
      % (rec["sig_bytes"], rec["split_offset"]))
print("  boundary located by                           : %s" % rec["boundary_source"])
print("  identical to the Adapt output                 : %s   <- Adapt provenance"
      % ("YES" if s2c and s2c == s2l else "NO"))
print("extracted witness == Gen's witness              : %s" % ("YES" if we and we == wh else "NO"))
print("  sha256(witness extracted)                     : %s..." % sha(we))
print("sigma_1 (adapted under the EXTRACTED witness)   : %d bytes, sha256 %s..." % (len(s1l), sha(s1l)))
print("  (Ext also reads the protocol state u2 already holds; what came from the ledger, and")
print("   only from it, is the ADAPTED-SIGNATURE input.)")
print()

fb, pb = js(fundB_after), js(payB)
rf, rp = js(rfund_after), js(rpay)
fee_paid = num(ra["gasUsed"]) * num(ra["effectiveGasPrice"])
u2_paid = (int(u2_after) - int(u2_before)) + fee_paid

print("=== did the coins move? ===")
print("legB escrowed coin spent : %s" % ("YES" if fb is None else "NO — still unspent"))
print("legB payout              : %s sat to %s (expected %d to %s)"
      % (sat(pb), addr_of(pb), valB - fee, destB))
print("legA payout to u2        : %d wei, fee-adjusted (escrow was %d)" % (u2_paid, coin))
print("legA escrow balance after: %s wei (must be 0)" % escrowA)
print("cross-venue replay ctrl  : cast exit %s, contract reason matched: %s"
      % (replay_rc, "YES" if replay_reason_ok == "1" else "NO"))
print()
print("=== the two deadlines (the recovery layer Sec 4.1 recalls from [23]) ===")
print("t2  leg B, BITCOIN, settled first  : %d — enforced as a timestamp nLockTime against" % t2)
print("                                     MEDIAN TIME PAST")
print("t1  leg A, ETHEREUM, settled second: %d — enforced against block.timestamp" % t1)
print("ordering t2 < t1                   : %s (%ds configured deadline gap)"
      % ("YES" if t2 < t1 else "NO", t1 - t2))
print("⚠ one numeric domain (UNIX seconds), NOT one consensus clock: median time past and")
print("  block.timestamp are different quantities on separate ledgers, so the gap above is a")
print("  CONFIGURED difference and not a guaranteed real reaction window.")
print()
print("=== the BITCOIN timeout refund branch (coin B3, never part of the swap) ===")
print("deadline t2                   : %s, median time past at maturity %s" % (refund_t, mature_mtp))
print("before deadline, NOT final    : refused by BOTH nodes (finality, not the leaf)")
print("before deadline, final        : refused by the PATCHED node only, for locktime")
print("                                (stock accepts: 0xbb is an OP_SUCCESSx, so BIP342")
print("                                 wins the whole script without executing it)")
print("after deadline                : accepted and MINED")
print("refund escrowed coin spent    : %s" % ("YES" if rf is None else "NO — still unspent"))
print("refund payout                 : %s sat to %s (expected %d to %s)"
      % (sat(rp), addr_of(rp), valB3 - fee, rdest))
print()
print("=== the ETHEREUM timeout refund branch (a second escrow, never part of the swap) ===")
print("deadline t1                   : %s, chain timestamp at maturity %s" % (t1, eth_mature))
print("before t1, by the payer       : reverted 'before timeout'; a broadcast forced past gas")
print("                                estimation was MINED with status %s and moved nothing" % status_e1)
print("after t1, not the payer       : reverted 'not payer'")
print("after t1, the SETTLED leg A   : reverted 'not open' — local state exclusivity only,")
print("                                NOT evidence of cross-chain atomicity")
print("after t1, by the payer        : receipt status %s, escrow state %s (REFUNDED=3)"
      % (status_r, state_r))
print("contract balance afterwards   : %s wei" % escrow_r)

fail = []
if not tb.get("blockhash"):
    fail.append("leg B was not mined")
if int(tb.get("confirmations", 0)) < 1:
    fail.append("leg B reports %s confirmations" % tb.get("confirmations"))
if num(ra["status"]) != 1:
    fail.append("leg A's claim reverted (receipt status 0)")
if num(ta.get("gas", ta.get("gasLimit"))) > cap:
    fail.append("leg A's mined gasLimit exceeds the EIP-7825 cap")
if num(ra["gasUsed"]) >= cap:
    fail.append("leg A's gasUsed is not under the EIP-7825 cap")
if sent_a != recv_a:
    n = min(len(sent_a), len(recv_a))
    first = next((i for i in range(n) if sent_a[i] != recv_a[i]), n)
    fail.append("leg A: the calldata the node received differs from the retained expected "
                "calldata (lengths %d vs %d, first differing byte at offset %d)"
                % (len(recv_a), len(sent_a), first))

# THE PROVENANCE CHECK, AND THE WHOLE POINT OF THIS RUN.
if not s2c:
    fail.append("no signature was recovered from the mined Bitcoin witness")
elif s2c != s2l:
    fail.append("the sigma_2 recovered from the mined Bitcoin witness differs from the Adapt "
                "output that was broadcast — leg A's witness cannot be attributed to the ledger")
if not we:
    fail.append("no extracted witness was produced")
elif we != wh:
    fail.append("the extracted witness differs from the one Gen produced")
if not s1l:
    fail.append("sigma_1 was not produced")

if fb is not None:
    fail.append("leg B's escrowed coin is still unspent — the leg did not settle")
if pb is None:
    fail.append("leg B produced no spendable payout output")
else:
    if sat(pb) != valB - fee:
        fail.append("leg B paid %s sat, expected %d" % (sat(pb), valB - fee))
    if addr_of(pb) != destB:
        fail.append("leg B paid %s, not the intended beneficiary %s" % (addr_of(pb), destB))
if u2_paid != coin:
    fail.append("u2's fee-adjusted payout on Ethereum was %d wei, expected exactly %d" % (u2_paid, coin))
if int(escrowA) != 0:
    fail.append("leg A's escrow still holds %s wei after settlement" % escrowA)
if replay_rc == "0":
    fail.append("the cross-venue replay control was ACCEPTED: signatures do not bind their legs")
if replay_reason_ok != "1":
    fail.append("the cross-venue replay was refused, but not with the contract's 'message not "
                "bound' reason, so the refusal is not attributable to message binding")

# The refund branch must have actually returned the coin, not merely validated.
if rf is not None:
    fail.append("the refund's escrowed coin B3 is still unspent — the refund did not settle")
if rp is None:
    fail.append("the refund produced no spendable payout output")
else:
    if sat(rp) != valB3 - fee:
        fail.append("the refund paid %s sat, expected %d" % (sat(rp), valB3 - fee))
    if addr_of(rp) != rdest:
        fail.append("the refund paid %s, not the funder's address %s" % (addr_of(rp), rdest))

# The Ethereum half of the recovery layer, and the configured ordering. Each of these is also
# gated in the shell as it happens; re-checked here so verdict.txt stands on its own.
if t2 >= t1:
    fail.append("the deadlines are not ordered t2 < t1 (t2=%d, t1=%d): the leg settled first "
                "does not carry the shorter deadline" % (t2, t1))
if status_e1 != "0":
    fail.append("the premature Ethereum refund was mined with receipt status %s, not 0 — the "
                "deadline did not refuse it" % status_e1)
if status_r != "1":
    fail.append("the matured Ethereum refund was mined with receipt status %s, not 1" % status_r)
if state_r != "3":
    fail.append("the Ethereum refund escrow ended in state %s, not REFUNDED (3)" % state_r)
if num(escrow_r) != 0:
    fail.append("the Ethereum contract still holds %s wei after leg A and the refund-test "
                "escrow closed" % escrow_r)

if fail:
    print()
    print("RESULT: FAIL — %d post-condition(s) not met:" % len(fail))
    for i, f in enumerate(fail, 1):
        print("  %d. %s" % (i, f))
    sys.exit(1)

if pi != "1":
    print()
    print("RESULT: INCOMPLETE — every post-condition above was met, but PI=0 omitted Fig. 1's")
    print("        proof-of-knowledge step, so this run is NOT a full Fig. 1 execution and")
    print("        must not be reported as one.")
    sys.exit(2)

print()
print("RESULT: a CROSS-VENUE LAS SETTLEMENT. Leg B paid BTC on a patched Bitcoin Core,")
print("        authorised solely by a LAS signature verified IN CONSENSUS under")
print("        OP_CHECKLASSIGVERIFY. Its adapted signature was read back out of the MINED")
print("        WITNESS, its boundary located by the funding output's own commitment; that")
print("        signature — and only it — was the adapted-signature input to Ext; and leg A")
print("        paid ETH on a real Ethereum client, verified by a DEPLOYED CONTRACT inside one")
print("        EIP-7825-capped transaction, under a signature adapted with the recovered")
print("        witness. ONE LAS instance, two venue-specific verification mechanisms.")
print("        Separately, BOTH VENUES' TIMEOUT REFUND mechanisms were exercised on DEDICATED")
print("        RECOVERY-TEST OBJECTS, one of each venue's own kind — Bitcoin's CLTV leaf at")
print("        t2, on a control UTXO funded to the same address as leg B and spent through the")
print("        same refundable tree, built from the same refund key and locktime; and the")
print("        contract's `refund` at t1, on a second escrow carrying leg A's own deadline and")
print("        registered context. Each was")
print("        refused before its configured deadline and, once matured, returned the test")
print("        coin. The configured deadlines are ordered t2 < t1, the venue settled first")
print("        carrying the shorter one. NEITHER SETTLED LEG had its own refund path taken —")
print("        both were claimed, which is what the honest path means.")
print()
print("⚠ THIS IS NOT A FAIR ATOMIC SWAP AND MUST NOT BE CALLED ONE, and the deadlines above do")
print("  NOT change that. The claim leaf is single-key under the FUNDER's key, so the funder")
print("  can spend without waiting and the refund is NOT an exclusive recovery path; and the")
print("  mempool exposes the ADAPTED SIGNATURE before confirmation, so the funder — holding the")
print("  matching pre-signature — can Ext from it and attempt a conflicting spend; if that")
print("  confirms instead it keeps this coin and claims the other leg. Whether it wins depends")
print("  on relay/replacement policy and miners, so the race is the claim and not a guaranteed")
print("  theft, but a race is already enough to deny fairness. A timeout closes neither hole.")
print("  eprint 2020/845 Sec 4.1 reasons from a signature 'published on a blockchain' and")
print("  abstracts that race away; this does not.")
print("⚠ THE ORDERING IS CONFIGURED, NOT ENFORCED. Each side enforces only its own deadline:")
print("  nothing in the contract or the consensus rule checks the other leg's, and the two are")
print("  enforced against different quantities (median time past vs block.timestamp). Pairing")
print("  them remains a deployment decision.")
print("⚠ Sec 4.1 states the two-timeout setup while RECALLING the classical protocol of its")
print("  [23]; the LAS choreography it then gives — Fig. 1 — restates no timeout. This")
print("  recovery layer is therefore an ADDITION around Fig. 1, never a repair to it.")
PY
VERDICT_RC=${PIPESTATUS[0]}
set -o pipefail
[ "$VERDICT_RC" -eq 2 ] && [ "$PI" != "1" ] && INCOMPLETE=1 || INCOMPLETE=0
[ "$VERDICT_RC" -eq 0 ] || [ "$INCOMPLETE" -eq 1 ] \
  || fail_out "post-conditions not met — see the enumerated list in verdict.txt"

# --- 21. the control tables, appended to the same verdict ----------------------------------------------
{
  echo
  echo "--- BITCOIN: every case put to BOTH nodes as a CONSENSUS question ---"
  echo "    (generateblock submit=false: block validation, not mempool policy)"
  echo
  cat "$OUT/controls.txt"
  echo
  echo "  Required and checked exactly: valid = ACCEPTED/ACCEPTED; every negative ="
  echo "  REJECTED/ACCEPTED — EXCEPT refund_immature, which is REJECTED/REJECTED because a"
  echo "  non-final transaction is refused by block validation before any script runs, by"
  echo "  every node. That case evidences the deadline, NOT the refund leaf; refund_cltv is"
  echo "  the one that isolates the leaf, and its patched-node reason names locktime."
  echo
  echo "--- THE TWO CROSS-VENUE BINDING CONTROLS (the mirrored pair) ---"
  echo "  legB_evm_message_signature : a GENUINE LAS signature under leg B's OWN key over"
  echo "                               LEG A's Ethereum digest, offered to the Bitcoin"
  echo "                               consensus rule. Refused. Only the message differs"
  echo "                               from the settlement signature."
  echo "  legA cross-venue replay    : leg A's Ethereum claim rebuilt carrying LEG B's"
  echo "                               BIP341 sighash. Reverted with the contract's own"
  echo "                               'message not bound', and a real broadcast left the"
  echo "                               escrow untouched."
  echo
  cat "$OUT/leg_messages.txt"
  echo
  cat "$OUT/deadlines.txt"
  echo
  echo "--- THE ETHEREUM REFUND BATTERY (a second escrow; leg A itself was CLAIMED) ---"
  cat "$OUT/eth_refund.txt"
  echo
  cat "$OUT/legB_size_model.txt"
  echo
  echo "LEDGER SEPARATION. The two venues are different clients with different consensus"
  echo "rules and different genesis states. This is NOT the cross-offer control the"
  echo "same-venue Bitcoin runner uses — a Bitcoin transaction cannot be offered to an EVM"
  echo "node at all — and must not be reported as its equivalent."
  echo
  echo "WHAT EACH MESSAGE BINDS — TWO DIFFERENT LISTS, NEITHER A SUPERSET OF THE OTHER."
  echo "Leg A binds chain id, contract, escrow id, payer, beneficiary and amount. Leg B's"
  echo "BIP341 sighash binds its transaction inputs and prevout amounts and its outputs, and"
  echo "has NO chain id. There is no transaction input on the EVM side and no chain"
  echo "identifier on the Bitcoin side."
  echo
  echo "Both settlement signatures are Adapt's output. base_sign_det is used by the shim's"
  echo "selftest, for the control signatures above and for the refund signatures, but neither"
  echo "settlement signature comes from it. Their Adapt provenance rests on the byte-for-byte"
  echo "equality of the chain-recovered signature with the retained Adapt output. Ext"
  echo "succeeding on the mined sigma_2 is additional, weaker evidence: any (c, z_hat + y')"
  echo "with A*y' = Y would also pass it."
  echo
  echo "SCOPE: the settlement path, plus BOTH venues' refund mechanisms exercised on dedicated"
  echo "recovery-test objects — a control UTXO on Bitcoin and a second escrow on Ethereum —"
  echo "rather than by taking the refund path of either settled leg. The deadline ordering"
  echo "t2 < t1 is CONFIGURED and checked, not enforced by either venue. Full fairness is NOT"
  echo "established — see the warnings in the verdict. A patched node is not Bitcoin; the"
  echo "consensus rule's security is unanalysed; regtest and anvil are not mainnets; the two"
  echo "sides' security levels are unmatched."
} | tee -a "$OUT/verdict.txt"

rm -rf "$REPO/evm/xchain"

if [ "$INCOMPLETE" -eq 1 ]; then
  echo
  echo "evidence written (INCOMPLETE run, pi omitted): evidence/btc_eth_swap/$RUN_ID/" >&2
  echo "latest was NOT moved — only a PI=1 run is a full Fig. 1 execution." >&2
  exit 1
fi

mkdir -p "$REPO/evidence/btc_eth_swap"
ln -sfn "$RUN_ID" "$REPO/evidence/btc_eth_swap/latest"

echo
echo "evidence written: evidence/btc_eth_swap/$RUN_ID/"
echo "  verdict.txt              — pass/fail computed FROM both clients' records and the bytes"
echo "  legB_mined.json          — Bitcoin's record of the leg it settled"
echo "  legA_receipt.json / legA_tx.json — Ethereum's record of the leg it settled"
echo "  refund_mined.json        — the matured BITCOIN refund, as mined"
echo "  refund_*_consensus_*.err — per-node output for the premature-refund controls, verbatim"
echo "                             (patched/stock differential: for refund_cltv the STOCK node"
echo "                              ACCEPTS, so these are not all refusals)"
echo "  deadlines.txt            — t2, t1, the horizon they came from, and what enforces each"
echo "  eth_refund.txt           — the Ethereum refund battery, case by case"
echo "  ethrefund_*.log          — each Ethereum refund control's reply, verbatim"
echo "  ethrefund_e1_receipt.json / ethrefund_receipt.json — the premature and matured refunds"
echo "  eth_timetravel.log       — how the Ethereum clock was advanced past t1"
echo "  recovered_sigB.json      — the chain-byte recovery, with the commitment used"
echo "  controls.txt             — patched vs stock, per case"
echo "  replay_call.log / replay_send.log — the Ethereum cross-venue binding control"
echo "  cap_control.log          — the EIP-7825 differential control"
echo "  leg_messages.txt         — each leg's digest, what it binds, and its deadline's units"
echo "  setup_vectors.log presign.log extract_adapt.log applied.patch environment.txt"
echo
echo "decisive rows in verdict.txt:"
echo "  'identical to the Adapt output ... YES'          (Adapt provenance)"
echo "  'extracted witness == Gen's witness ... YES'     (Ext recovered y from Bitcoin's bytes)"
echo "  'refund escrowed coin spent ... YES'             (Bitcoin refund control UTXO was spent)"
echo "  'refund payout ... expected'                     (and paid the funder the right amount)"
echo "  'after t1, by the payer ... state 3'             (Ethereum refund escrow reached REFUNDED)"
echo "  'contract balance ... fell by exactly'           (and the coin left the contract)"
echo "  'ordering t2 < t1 ... YES'                       (the configured deadline ordering)"
echo "  'cross-venue replay ctrl ... reason matched: YES'(the legs bind across venues)"
