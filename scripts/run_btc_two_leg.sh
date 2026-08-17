#!/usr/bin/env bash
# Settle a WHOLE two-leg atomic swap (eprint 2020/845 Fig. 1) on Bitcoin, across TWO
# independent regtest chains, with both legs authorised by LAS signatures that a patched
# Bitcoin Core verifies under OP_CHECKLASSIGVERIFY.
#
#   BTC_TAG=v31.1 BTC_SRC=/path/to/patched-src BTC_BIN_PATCHED=.../build/bin/bitcoind \
#   BTC_BIN_STOCK=.../bitcoin-31.1/bin/bitcoind ./scripts/run_btc_two_leg.sh
#
# WHAT THIS ADDS TO run_btc_las_node.sh, AND WHY IT IS A DIFFERENT CLAIM
# ----------------------------------------------------------------------
# That script settles ONE transaction whose only authorisation is a LAS signature. It does
# not exercise the adaptor at all: the signature it mines comes from `base_sign_det`, so it
# evidences consensus verification and nothing about a swap. This script is the Bitcoin
# counterpart of scripts/run_onchain_two_leg.sh, and the same three things separate it:
#
#   * TWO CHAINS. Fig. 1 exchanges two coins on two ledgers. Two legs on one chain would
#     settle without ever exercising the property the protocol exists for. So this runs two
#     regtest chains that are never connected and diverge from height 1, and each leg's
#     coin exists on ONE of them.
#   * THE WITNESS MUST COME FROM THE LEDGER. After u1 settles leg B, this pulls the adapted
#     signature back out of the MINED transaction's witness — locating the signature/key
#     boundary from the COMMITMENT THE FUNDING OUTPUT MADE (bitcoin/tools/btc_recover_sig.py),
#     never from a known signature length — and hands those bytes, and only those, to
#     `extract_and_adapt`, a separate program with no access to the local copy.
#   * THE SIGNATURES MUST BIND THEIR LEGS. Each leg's signed message is its own BIP341
#     sighash, which commits to that leg's input coin, its output value and its beneficiary.
#     A cross-leg control puts leg B's settlement signature on leg A and requires refusal.
#
# WHAT BITCOIN DOES NOT BIND, STATED PLAINLY. BIP341's sighash covers the transaction, NOT
# the chain: there is no chain id in it, unlike the EVM leg. The chain-isolation control
# below is therefore evidence that the two LEDGERS are separate — leg B's coin does not
# exist on chain 1 — and is NOT evidence that a signature is bound to a chain. It must
# never be reported as the latter.
#
# CONSENSUS, NOT POLICY. Every verdict comes from `generateblock ... submit=false`, which
# builds a block containing the transaction and runs block validation without submitting.
# `testmempoolaccept` would answer a different question, mixing in relay policy under which
# these 520-byte witness chunks are non-standard for reasons unrelated to LAS.
#
# THE DIFFERENTIAL CONTROL IS PRESERVED, PER LEG. Every case is put to BOTH a patched and a
# STOCK node of the same release, sharing that leg's chain. On the stock node 0xbb is still
# an OP_SUCCESSx, so tapscript succeeds UNCONDITIONALLY and the stock node cannot be
# reacting to the signature. Required pattern, checked exactly:
#     valid case      patched ACCEPTED   and stock ACCEPTED
#     every negative  patched REJECTED:* and stock ACCEPTED
#
# THE SETTLEMENT SIGNATURES ARE ADAPT'S OUTPUT, AND HOW THAT IS ESTABLISHED. The two mined
# signatures are sigma2 (u1's Adapt of sigma_hat_2) and sigma1 (u2's Adapt of sigma_hat_1
# under the witness extracted from chain 2). `base_sign_det` is used elsewhere in this stack
# — the shim's selftest and the control signatures below both go through it — but NEITHER
# settlement signature does, and no transaction it produced is ever mined.
#
# Their Adapt provenance rests on the BYTE-FOR-BYTE comparison of what came off the chain
# against the retained Adapt output. Ext succeeding on the mined sigma2 is ADDITIONAL and
# WEAKER evidence: it shows the chain-observed signature is compatible with the
# pre-signature and yields a witness satisfying A*y = Y. It is not proof that Adapt produced
# those bytes — any (c, z_hat + y') with A*y' = Y would also pass — so it must never be
# reported as provenance on its own.
#
# SCOPE, so nothing here is read as more than it is. The honest path only: the tapleaf is
# `<sha256(pk)> OP_CHECKLASSIGVERIFY OP_1` with NO refund branch, so timeouts and the
# refund edge case are not implemented and no claim is made about them (Meeting 7 permits
# the honest path first). A patched node is not Bitcoin.
#
# PI IS NOT OPTIONAL. Fig. 1 has u2 verify a proof of knowledge before it pre-signs, so a
# run without it is not a Fig. 1 execution. PI=0 is allowed for diagnosis but is recorded
# as INCOMPLETE: it cannot print the success verdict and does not move `latest`.
#
# A CLIENT REJECTION IS A RESULT. Any refusal keeps the client's own error text, all four
# node logs, the retained transactions and the environment, writes a FAIL verdict naming
# every unmet post-condition, and exits non-zero. `latest` is not moved on a failing run.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN_ID="$(date +%Y%m%d_%H%M%S)"
OUT="$REPO/evidence/btc_twoleg/$RUN_ID"
TOOLS="$REPO/bitcoin/tools"
SHIM="$REPO/bitcoin/las_consensus"
PATCH="$REPO/bitcoin/patches/0001-op-checklassigverify-v31.1.patch"
VEC="$REPO/evm/test/vectors/swap_btc"

# Private ports, and NOT the ones run_btc_las_node.sh uses, so the two experiments can
# never end up measuring each other's node.
RPC1P="${RPC1P:-18590}"; RPC1S="${RPC1S:-18591}"
RPC2P="${RPC2P:-18592}"; RPC2S="${RPC2S:-18593}"
P2P1P="${P2P1P:-18600}"; P2P1S="${P2P1S:-18601}"
P2P2P="${P2P2P:-18602}"; P2P2S="${P2P2S:-18603}"

FEE_SAT="${FEE_SAT:-200000}"
COIN_BTC="${COIN_BTC:-1.0}"
SEED_PREIMAGE="LAS-CONSENSUS-PARAMS-v1"
PI="${PI:-1}"

: "${BTC_TAG:?set BTC_TAG, e.g. v31.1}"
: "${BTC_SRC:?set BTC_SRC to the PATCHED Bitcoin Core source tree}"
: "${BTC_BIN_PATCHED:?set BTC_BIN_PATCHED to the bitcoind built from BTC_SRC}"
: "${BTC_BIN_STOCK:?set BTC_BIN_STOCK to the stock release bitcoind of the same tag}"
CLI="${CLI:-$(dirname "$BTC_BIN_PATCHED")/bitcoin-cli}"

mkdir -p "$OUT"
D1P="$OUT/chain1_patched"; D1S="$OUT/chain1_stock"
D2P="$OUT/chain2_patched"; D2S="$OUT/chain2_stock"

cleanup() {
  "$CLI" -regtest -datadir="$D1P" -rpcport="$RPC1P" stop >/dev/null 2>&1 || true
  "$CLI" -regtest -datadir="$D1S" -rpcport="$RPC1S" stop >/dev/null 2>&1 || true
  "$CLI" -regtest -datadir="$D2P" -rpcport="$RPC2P" stop >/dev/null 2>&1 || true
  "$CLI" -regtest -datadir="$D2S" -rpcport="$RPC2S" stop >/dev/null 2>&1 || true
}
trap cleanup EXIT

# APPENDS, so a detailed verdict already written by the post-condition checker survives.
fail_out() {
  {
    echo; echo "RESULT: FAIL — $1"; echo
    echo "run_id: $RUN_ID"
    echo "This is a recorded negative result, not a crashed run. Retained alongside it:"
    echo "  chain{1,2}_{patched,stock}/regtest/debug.log   all four nodes' logs"
    echo "  *_consensus_*.{json,err}   each node's block-validation answer, verbatim"
    echo "  leg{A,B}_spend.json        exactly what this run intended to mine"
    echo "  recovered_sigB.json        the chain-byte recovery, where it got that far"
    echo "  setup_vectors.log presign.log adapt.log extract_adapt.log"
    echo "  applied.patch environment.txt selftest.log"
  } | tee -a "$OUT/verdict.txt"
  echo "evidence written (FAILING run): evidence/btc_twoleg/$RUN_ID/" >&2
  exit 1
}

cli_1p() { "$CLI" -regtest -datadir="$D1P" -rpcport="$RPC1P" "$@"; }
cli_1s() { "$CLI" -regtest -datadir="$D1S" -rpcport="$RPC1S" "$@"; }
cli_2p() { "$CLI" -regtest -datadir="$D2P" -rpcport="$RPC2P" "$@"; }
cli_2s() { "$CLI" -regtest -datadir="$D2S" -rpcport="$RPC2S" "$@"; }
jqpy()  { python3 -c "$1" "${@:2}"; }

# --- 1. pin gates ---------------------------------------------------------------------
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

# --- 2. the shim and the consensus seed, before any node exists -------------------------
make -C "$SHIM" las_btc_tool > "$OUT/build_shim.log" 2>&1 \
  || { cat "$OUT/build_shim.log" >&2; fail_out "could not build the consensus shim"; }
"$SHIM/las_btc_tool" selftest > "$OUT/selftest.log" 2>&1 \
  || { cat "$OUT/selftest.log" >&2; fail_out "the shim's selftest failed — the crypto is wrong before the node is even involved"; }
cat "$OUT/selftest.log"

SEED="$("$SHIM/las_btc_tool" seed)"
SEED_EXPECT="$(jqpy 'import hashlib,sys;print(hashlib.sha256(sys.argv[1].encode()).hexdigest())' "$SEED_PREIMAGE")"
[ "$SEED" = "$SEED_EXPECT" ] \
  || fail_out "the consensus seed compiled in ($SEED) is not SHA-256(\"$SEED_PREIMAGE\") = $SEED_EXPECT"
echo "consensus seed OK: SHA-256(\"$SEED_PREIMAGE\") == the compiled-in value"

# --- 3. the off-chain tooling, and Gen + pi --------------------------------------------
# THE SEED IS THE JOIN. A' is expanded from the pp seed alone, and every operation consumes
# it — KeyGen's t = A'r_1, Gen's Y = A'r'_1, PreSign's w = A'y_1, PreVerify's w' = A'z_1,
# Ext's A*s test — while the node verifies under the seed compiled into the shim. Generating
# the swap under any other seed puts signer and verifier on different LAS instances. Passing
# `las_btc_tool seed` straight into `setup` makes that agreement something arranged here,
# not something discovered from a rejected transaction.
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
  echo "         as INCOMPLETE: it cannot print the success verdict and will not move" >&2
  echo "         evidence/btc_twoleg/latest." >&2
fi
make -C "$REPO/ref" test/extract_and_adapt >> "$OUT/build.log" 2>&1 || {
  cat "$OUT/build.log" >&2; fail_out "could not build test/extract_and_adapt"; }

if ! (cd "$REPO/ref" && "./$SWAP_TOOL" setup "$VEC" $SETUP_ARGS --pp-seed "$SEED") \
      > "$OUT/setup_vectors.log" 2>&1; then
  cat "$OUT/setup_vectors.log" >&2
  fail_out "swap setup failed (Gen or pi) — see setup_vectors.log"
fi
cat "$OUT/setup_vectors.log"

# --- 4. four nodes, two chains ----------------------------------------------------------
mkdir -p "$D1P" "$D1S" "$D2P" "$D2S"
start_node() { # $1 bin, $2 datadir, $3 rpcport, $4 p2pport, $5 log, $6 optional -connect peer
  local extra=()
  if [ -n "${6:-}" ]; then extra=(-connect=127.0.0.1:"$6"); fi
  "$1" -regtest -datadir="$2" -rpcport="$3" -port="$4" -bind=127.0.0.1 \
    -fallbackfee=0.0002 -daemonwait "${extra[@]}" > "$5" 2>&1 \
    || { cat "$5" >&2; fail_out "node failed to start: $2"; }
}
# Each chain's stock node connects to ITS OWN patched node and to nothing else. Regtest has
# no DNS or fixed seeds, so a node only ever meets peers it is told about: the two chains
# have no path to each other, which is what keeps them independent ledgers.
start_node "$BTC_BIN_PATCHED" "$D1P" "$RPC1P" "$P2P1P" "$OUT/node_c1_patched_start.log"
start_node "$BTC_BIN_STOCK"   "$D1S" "$RPC1S" "$P2P1S" "$OUT/node_c1_stock_start.log" "$P2P1P"
start_node "$BTC_BIN_PATCHED" "$D2P" "$RPC2P" "$P2P2P" "$OUT/node_c2_patched_start.log"
start_node "$BTC_BIN_STOCK"   "$D2S" "$RPC2S" "$P2P2S" "$OUT/node_c2_stock_start.log" "$P2P2P"

for f in cli_1p cli_1s cli_2p cli_2s; do
  $f -rpcwait getblockchaininfo > /dev/null || fail_out "$f did not answer"
done

sync_chain() { # $1 = patched cli fn, $2 = stock cli fn, $3 = label
  local a b
  for _ in $(seq 1 200); do
    a="$($1 getbestblockhash 2>/dev/null || true)"
    b="$($2 getbestblockhash 2>/dev/null || true)"
    [ -n "$a" ] && [ "$a" = "$b" ] && return 0
    sleep 0.2
  done
  fail_out "$3: the two nodes did not converge on one chain — a verdict from an unsynced node would be about missing inputs, not about the rule"
}

cli_1p createwallet las1 > /dev/null 2>&1 || fail_out "no wallet support in the patched build (chain 1)"
cli_2p createwallet las2 > /dev/null 2>&1 || fail_out "no wallet support in the patched build (chain 2)"
cli_1p generatetoaddress 101 "$(cli_1p getnewaddress)" > /dev/null
cli_2p generatetoaddress 101 "$(cli_2p getnewaddress)" > /dev/null
sync_chain cli_1p cli_1s "chain 1"
sync_chain cli_2p cli_2s "chain 2"

mine_1() { cli_1p generatetoaddress 1 "$(cli_1p getnewaddress)" > /dev/null; }
mine_2() { cli_2p generatetoaddress 1 "$(cli_2p getnewaddress)" > /dev/null; }

# --- 5. the two chains must actually be two --------------------------------------------
# Divergence is asserted at height 1, not merely at the tip: two chains that agreed on
# early blocks and differed only at the tip would be one chain in a temporary fork.
H1_1="$(cli_1p getblockhash 1)"; H1_2="$(cli_2p getblockhash 1)"
BEST_1="$(cli_1p getbestblockhash)"; BEST_2="$(cli_2p getbestblockhash)"
[ "$H1_1" != "$H1_2" ] || fail_out "both chains report the same block 1 ($H1_1) — this is one ledger, not two, so nothing below would be a cross-chain swap"
[ "$BEST_1" != "$BEST_2" ] || fail_out "both chains report the same tip — this is not a cross-chain swap"
{
  echo "chain 1: block1=$H1_1 tip=$BEST_1"
  echo "chain 2: block1=$H1_2 tip=$BEST_2"
  echo "the two chains diverge from height 1 and are never connected to each other"
} | tee "$OUT/chain_independence.txt"

# --- 6. environment record ---------------------------------------------------------------
{
  echo "run_id=$RUN_ID"
  echo "git=$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo n/a)"
  echo "branch=$(git -C "$REPO" rev-parse --abbrev-ref HEAD 2>/dev/null || echo n/a)"
  echo "host=$(uname -s) $(uname -r) $(uname -m)"
  echo "cc=$(${CC:-cc} --version 2>/dev/null | head -1)"
  echo "--- Bitcoin Core ---"
  echo "btc_tag=$BTC_TAG"
  echo "btc_src=$BTC_SRC (HEAD == $BTC_TAG commit $SRC_TAG_COMMIT, plus the committed patch)"
  echo "patch=bitcoin/patches/$(basename "$PATCH")"
  echo "patch_sha256=$COMMITTED_SHA"
  echo "applied_patch_sha256=$APPLIED_SHA (VERIFIED EQUAL; no untracked files)"
  echo "bitcoind_patched_sha256=$(sha256sum "$BTC_BIN_PATCHED" | cut -d' ' -f1)"
  echo "bitcoind_stock_sha256=$(sha256sum "$BTC_BIN_STOCK" | cut -d' ' -f1)"
  echo "--- chains ---"
  echo "chain1=regtest rpc=$RPC1P/$RPC1S block1=$H1_1 (leg A: u1 -> u2)"
  echo "chain2=regtest rpc=$RPC2P/$RPC2S block1=$H1_2 (leg B: u2 -> u1)"
  echo "chain_independence=block 1 differs; the two chains are never connected"
  echo "note=BIP341's sighash covers the TRANSACTION, not the chain — there is no chain id"
  echo "note=in it, unlike the EVM leg. Chain isolation here is a property of the LEDGERS."
  echo "--- LAS ---"
  echo "consensus_seed=$SEED"
  echo "consensus_seed_preimage=$SEED_PREIMAGE (SHA-256 VERIFIED)"
  echo "pp_seed=$SEED (the swap was generated under the NODE's seed; see step 3)"
  echo "opcode=OP_CHECKLASSIGVERIFY (0xbb, a BIP342 OP_SUCCESSx slot)"
  echo "sighash=BIP341 SIGHASH_DEFAULT only; it IS each leg's signed message"
  echo "swap_tool=ref/$SWAP_TOOL"
  echo "pi=$([ "$PI" = "1" ] && echo "proved and verified (Fig. 1 steps 2-3)" || echo "OMITTED by PI=0 — run is INCOMPLETE, cannot pass, latest not moved")"
  echo "settlement_signatures=Adapt output ONLY (sigma2, then sigma1 from the extracted witness)"
  echo "settlement_provenance=byte-for-byte equality of the chain-recovered signature with"
  echo "settlement_provenance=the retained Adapt output; Ext success is weaker corroboration"
  echo "base_sign_det=used by the shim selftest and for control signatures; NEITHER"
  echo "base_sign_det=settlement signature comes from it, and no transaction it signed is mined"
  echo "instrument=generateblock submit=false — CONSENSUS validation, not mempool policy"
  echo "shim_selftest=PASSED (positive + 7 negative controls)"
  echo "coin_per_leg=$COIN_BTC BTC   fee_sat=$FEE_SAT"
  echo "scope=honest path only; the tapleaf has NO refund branch, so timeouts/refund are"
  echo "scope=not implemented and no claim is made about them"
  for f in pp_normal.bin t1.bin t2.bin Y.bin pk1.bin pk2.bin; do
    [ -f "$VEC/$f" ] && echo "vector_${f%.bin}=sha256:$(sha256sum "$VEC/$f" | cut -c1-16)... $(stat -c%s "$VEC/$f") B"
  done
  [ -f "$VEC/pi.bin" ] && echo "vector_pi=sha256:$(sha256sum "$VEC/pi.bin" | cut -c1-16)... $(stat -c%s "$VEC/pi.bin") B"
} > "$OUT/environment.txt"

# --- 7. each leg's LAS address, and its beneficiary --------------------------------------
# Leg A is locked to u1's key: u1 pre-signs it and u2 completes it, so the coin moves only
# once u2 holds an adapted signature under pk1. Leg B likewise under pk2.
las_address() { # $1 = pk file, $2 = out json, $3 = log
  python3 "$TOOLS/btc_las_spend.py" sighash --core-src "$BTC_SRC" --pk "$1" \
    --address-only --out "$2" > "$3" 2>&1 || { cat "$3" >&2; fail_out "could not derive a LAS taproot address"; }
  jqpy 'import json,sys;print(json.load(open(sys.argv[1]))["address"])' "$2"
}
ADDR_A="$(las_address "$VEC/pk1.bin" "$OUT/legA_address.json" "$OUT/legA_address.log")"
ADDR_B="$(las_address "$VEC/pk2.bin" "$OUT/legB_address.json" "$OUT/legB_address.log")"
[ "$ADDR_A" != "$ADDR_B" ] || fail_out "both legs derived the SAME LAS address — the two keys are not distinct"

# The beneficiaries: u2 is paid on chain 1, u1 on chain 2.
DEST_A="$(cli_1p getnewaddress u2_payout)"
DEST_B="$(cli_2p getnewaddress u1_payout)"
spk_of() { $1 getaddressinfo "$2" | jqpy 'import json,sys;print(json.load(sys.stdin)["scriptPubKey"])'; }
DEST_A_SPK="$(spk_of cli_1p "$DEST_A")"
DEST_B_SPK="$(spk_of cli_2p "$DEST_B")"

# --- 8. fund both legs, each on its own chain --------------------------------------------
# Two outputs per leg: the second is a REAL alternative outpoint, so the input-binding
# control can point at a coin that exists. Pointing at a nonexistent one would be rejected
# for missing inputs and would prove nothing about the sighash covering inputs.
fund() { # $1 = cli fn, $2 = address, $3 = mine fn -> "txid vout value_sat"
  local txid bh vout val
  txid="$($1 sendtoaddress "$2" "$COIN_BTC")"
  $3
  bh="$($1 getbestblockhash)"
  vout="$($1 getrawtransaction "$txid" true "$bh" \
    | jqpy 'import json,sys;t=json.load(sys.stdin);a=sys.argv[1];print(next(o["n"] for o in t["vout"] if o["scriptPubKey"].get("address")==a))' "$2")"
  val="$($1 getrawtransaction "$txid" true "$bh" \
    | jqpy 'import json,sys;t=json.load(sys.stdin);n=int(sys.argv[1]);print(int(round(next(o["value"] for o in t["vout"] if o["n"]==n)*1e8)))' "$vout")"
  echo "$txid $vout $val"
}
read -r FUND_A1 VOUT_A1 VAL_A1 <<< "$(fund cli_1p "$ADDR_A" mine_1)"
read -r FUND_A2 VOUT_A2 _      <<< "$(fund cli_1p "$ADDR_A" mine_1)"
read -r FUND_B1 VOUT_B1 VAL_B1 <<< "$(fund cli_2p "$ADDR_B" mine_2)"
read -r FUND_B2 VOUT_B2 _      <<< "$(fund cli_2p "$ADDR_B" mine_2)"
sync_chain cli_1p cli_1s "chain 1"
sync_chain cli_2p cli_2s "chain 2"

# Each leg's coin exists on ITS chain and nowhere else.
#
# ASKED OF THE UTXO SET, NOT `getrawtransaction`. Without -txindex that call fails for ANY
# confirmed transaction the wallet does not own, so its failure would say nothing about
# whether the coin exists — it would be a passing check for the wrong reason. `gettxout`
# reads the UTXO set, which every node maintains regardless. And it is asked BOTH ways: the
# coin must be PRESENT on its own chain and ABSENT on the other, so the negative is paired
# with a positive that shows the query works at all.
utxo_present() { # $1 = cli fn, $2 = txid, $3 = vout
  local o
  o="$($1 gettxout "$2" "$3" 2>/dev/null || true)"
  [ -n "$o" ] && [ "$o" != "null" ]
}
utxo_present cli_1p "$FUND_A1" "$VOUT_A1" || fail_out "leg A's coin is not in chain 1's UTXO set — the funding did not take effect, so the absence check below would pass vacuously"
utxo_present cli_2p "$FUND_B1" "$VOUT_B1" || fail_out "leg B's coin is not in chain 2's UTXO set"
! utxo_present cli_2p "$FUND_A1" "$VOUT_A1" || fail_out "chain 2 holds leg A's coin — the chains are not independent"
! utxo_present cli_1p "$FUND_B1" "$VOUT_B1" || fail_out "chain 1 holds leg B's coin — the chains are not independent"
{
  echo "leg A coin $FUND_A1:$VOUT_A1 — present in chain 1's UTXO set, absent from chain 2's"
  echo "leg B coin $FUND_B1:$VOUT_B1 — present in chain 2's UTXO set, absent from chain 1's"
  echo "asked via gettxout (the UTXO set), so the answer does not depend on -txindex"
} | tee -a "$OUT/chain_independence.txt"

# --- 9. each leg's signed message is its own BIP341 sighash -------------------------------
# The alt outpoint is OMITTED when empty rather than passed as "": --alt-vout is an int, so
# an empty string is an argparse error, and the control that needs it would die instead of
# running.
sighash_leg() { # $1 label, $2 pk, $3 txid, $4 vout, $5 value, $6 alt_txid, $7 alt_vout, $8 dest_spk, $9 mutate
  local alt=()
  if [ -n "$6" ]; then alt=(--alt-txid "$6" --alt-vout "$7"); fi
  python3 "$TOOLS/btc_las_spend.py" sighash --core-src "$BTC_SRC" --pk "$2" \
    --txid "$3" --vout "$4" --value-sat "$5" --fee-sat "$FEE_SAT" \
    "${alt[@]}" --dest-spk "$8" --mutate "$9" \
    --out-sighash "$OUT/$1_sighash.bin" --state "$OUT/$1_state.json" \
    --out "$OUT/$1_sighash.json" > "$OUT/$1_sighash.log" 2>&1
}
sighash_leg legA "$VEC/pk1.bin" "$FUND_A1" "$VOUT_A1" "$VAL_A1" "$FUND_A2" "$VOUT_A2" "$DEST_A_SPK" none \
  || { cat "$OUT/legA_sighash.log" >&2; fail_out "leg A sighash failed"; }
sighash_leg legB "$VEC/pk2.bin" "$FUND_B1" "$VOUT_B1" "$VAL_B1" "$FUND_B2" "$VOUT_B2" "$DEST_B_SPK" none \
  || { cat "$OUT/legB_sighash.log" >&2; fail_out "leg B sighash failed"; }

cp "$OUT/legA_sighash.bin" "$VEC/legA_msg.bin"
cp "$OUT/legB_sighash.bin" "$VEC/legB_msg.bin"
MSG_A="$(jqpy 'import sys;print(open(sys.argv[1],"rb").read().hex())' "$VEC/legA_msg.bin")"
MSG_B="$(jqpy 'import sys;print(open(sys.argv[1],"rb").read().hex())' "$VEC/legB_msg.bin")"
[ "$MSG_A" != "$MSG_B" ] || fail_out "both legs derive the SAME sighash — one signature would settle both, so this is not a binding swap"
{
  echo "legA: chain 1 coin=$FUND_A1:$VOUT_A1 -> $DEST_A  sighash=$MSG_A"
  echo "legB: chain 2 coin=$FUND_B1:$VOUT_B1 -> $DEST_B  sighash=$MSG_B"
} | tee "$OUT/leg_messages.txt"

# --- 10. PreSign both legs, PreVerify both (Fig. 1 steps 4-5) -----------------------------
if ! (cd "$REPO/ref" && "./$SWAP_TOOL" presign "$VEC") > "$OUT/presign.log" 2>&1; then
  cat "$OUT/presign.log" >&2
  fail_out "PreSign/PreVerify failed — see presign.log (a PreVerify failure aborts Fig. 1 before any Adapt)"
fi
cat "$OUT/presign.log"

# --- 11. u1 adapts sigma_hat_2 -------------------------------------------------------------
if ! (cd "$REPO/ref" && "./$SWAP_TOOL" adapt "$VEC") > "$OUT/adapt.log" 2>&1; then
  cat "$OUT/adapt.log" >&2
  fail_out "Adapt (u1: sigma_hat_2 -> sigma_2) failed — see adapt.log"
fi
cat "$OUT/adapt.log"

# The node's own predicate, before broadcasting. A pp-seed mismatch would otherwise surface
# as a rejected transaction whose cause has to be guessed at.
"$SHIM/las_btc_tool" verify "$VEC/pk2.bin" "$VEC/sigma2.bin" "$VEC/legB_msg.bin" \
  > "$OUT/shim_verify_sigma2.log" 2>&1 \
  || { cat "$OUT/shim_verify_sigma2.log" >&2; fail_out "the adapted sigma_2 is refused by LASConsensusVerify — the node would reject it too"; }
cat "$OUT/shim_verify_sigma2.log"

# --- 12. the consensus instrument, and the differential control ---------------------------
: > "$OUT/controls.txt"
FAILED_CONTROLS=""

consensus_check() { # $1 = node fn, $2 = label, $3 = tag, $4 = address, $5 = raw hex
  if "$1" generateblock "$4" "[\"$5\"]" false > "$OUT/$2_consensus_$3.json" 2> "$OUT/$2_consensus_$3.err"; then
    echo "ACCEPTED"; return 0
  fi
  if grep -q "TestBlockValidity failed:" "$OUT/$2_consensus_$3.err"; then
    echo "REJECTED: $(tr -d '\r' < "$OUT/$2_consensus_$3.err" | tr '\n' ' ' | sed 's/  */ /g' | cut -c1-160)"
    return 0
  fi
  return 1
}

ask() { # $1 = chain (1|2), $2 = label, $3 = raw hex -> echoes "patched|stock"
  local pfn sfn addr p s
  if [ "$1" = "1" ]; then pfn=cli_1p; sfn=cli_1s; addr="$ADDR_A"; else pfn=cli_2p; sfn=cli_2s; addr="$ADDR_B"; fi
  p="$(consensus_check "$pfn" "$2" patched "$addr" "$3")" \
    || fail_out "$2: the PATCHED node failed for a reason that is NOT a consensus rejection — see ${2}_consensus_patched.err. A non-consensus error must never be recorded as a refusal."
  s="$(consensus_check "$sfn" "$2" stock "$addr" "$3")" \
    || fail_out "$2: the STOCK node failed for a reason that is NOT a consensus rejection — see ${2}_consensus_stock.err."
  case "$p$s" in
    *missing*|*"bad-txns-inputs"*)
      fail_out "$2: a node reported missing inputs — the nodes are out of sync and this verdict is not about the rule" ;;
  esac
  printf '%-30s patched: %-58s stock: %s\n' "$2" "$p" "$s" | tee -a "$OUT/controls.txt" >&2
  echo "$p|$s"
}

expect() { # $1 = label, $2 = "patched|stock", $3 = valid|negative
  local v="$2" p s
  case "$v" in *"|"*) : ;; *) FAILED_CONTROLS="$FAILED_CONTROLS $1(unparseable-verdict)"; return ;; esac
  p="${v%%|*}"; s="${v##*|}"
  if [ -z "$p" ] || [ -z "$s" ]; then
    FAILED_CONTROLS="$FAILED_CONTROLS $1(empty-verdict)"; return
  fi
  # The stock node must ACCEPT in every case: it treats 0xbb as OP_SUCCESS, so if it ever
  # rejects, the transaction is defective for an unrelated reason and the patched node's
  # answer is not attributable to the new rule.
  [ "$s" = "ACCEPTED" ] || FAILED_CONTROLS="$FAILED_CONTROLS $1(stock-not-ACCEPTED:not-attributable)"
  case "$3" in
    valid)    [ "$p" = "ACCEPTED" ] || FAILED_CONTROLS="$FAILED_CONTROLS $1(patched-did-not-accept-valid)" ;;
    negative) case "$p" in "REJECTED: "*) : ;; *) FAILED_CONTROLS="$FAILED_CONTROLS $1(patched-did-not-reject)" ;; esac ;;
  esac
}

# One step per line, and `ask`'s exit status is CHECKED: nesting it inside `expect` would
# let a node or tooling error be swallowed and resurface as "unparseable-verdict", which
# names the symptom instead of the cause.
judge() { # $1 = chain, $2 = label, $3 = raw hex, $4 = valid|negative
  local verdict
  verdict="$(ask "$1" "$2" "$3")" || fail_out "$2: could not obtain a verdict from both nodes"
  expect "$2" "$verdict" "$4"
}

assemble() { # $1 = state label, $2 = sig file, $3 = mutate, $4 = out label -> raw hex
  python3 "$TOOLS/btc_las_spend.py" assemble --core-src "$BTC_SRC" \
    --state "$OUT/$1_state.json" --sig "$2" --mutate "$3" \
    --out "$OUT/$4_spend.json" > "$OUT/$4_assemble.log" 2>&1 || return 1
  jqpy 'import json,sys;print(json.load(open(sys.argv[1]))["raw_hex"])' "$OUT/$4_spend.json"
}

# A control signature under a leg's own key, via base_sign_det. These are never mined: a
# control needs a signature that is genuine but WRONG for this transaction.
CTLDIR="$OUT/control_keys"; mkdir -p "$CTLDIR"
control_sign() { # $1 = pk file, $2 = sk file, $3 = message file, $4 = out sig file
  cp "$1" "$CTLDIR/pk.bin"; cp "$2" "$CTLDIR/sk.bin"
  "$SHIM/las_btc_tool" sign "$CTLDIR" "$3" >> "$OUT/control_sign.log" 2>&1 || return 1
  cp "$CTLDIR/sig.bin" "$4"
}

# Runs one leg's full control battery against the settlement transaction itself, so the
# negatives are mutations OF THE REAL SWAP LEG rather than of a stand-in.
run_leg_controls() { # $1 chain, $2 leg, $3 pk, $4 sk, $5 adapted sig, $6 fundtx, $7 vout,
                     # $8 value, $9 alttx, $10 altvout, $11 dest_spk, $12 other-leg sig
  local ch="$1" leg="$2" pk="$3" sk="$4" sig="$5" ftx="$6" fvout="$7" fval="$8"
  local atx="$9" avout="${10}" dspk="${11}" othersig="${12}"
  local mut hex label

  for mut in output_amount output_recipient input_outpoint chunk_truncated chunk_reordered wrong_pubkey; do
    label="leg${leg}_${mut}"
    sighash_leg "$label" "$pk" "$ftx" "$fvout" "$fval" "$atx" "$avout" "$dspk" "$mut" \
      || { cat "$OUT/${label}_sighash.log" >&2; fail_out "$label: sighash phase failed"; }
    hex="$(assemble "$label" "$sig" "$mut" "$label")" || { cat "$OUT/${label}_assemble.log" >&2; fail_out "$label: could not assemble"; }
    [ -n "$hex" ] || fail_out "$label: built an empty transaction"
    judge "$ch" "$label" "$hex" negative
  done

  # A signature over a FALSE sighash: the transaction is honest, but the signer was told
  # the input was worth more than it is. Reusing the honest signature would leave the
  # transaction unchanged and test nothing, so this one signs what it mutated.
  label="leg${leg}_wrong_prevout_amt"
  sighash_leg "$label" "$pk" "$ftx" "$fvout" "$fval" "$atx" "$avout" "$dspk" wrong_prevout_amt \
    || { cat "$OUT/${label}_sighash.log" >&2; fail_out "$label: sighash phase failed"; }
  control_sign "$pk" "$sk" "$OUT/${label}_sighash.bin" "$OUT/sig_${label}.bin" \
    || fail_out "$label: control signing failed"
  hex="$(assemble "$label" "$OUT/sig_${label}.bin" wrong_prevout_amt "$label")" \
    || fail_out "$label: could not assemble"
  judge "$ch" "$label" "$hex" negative

  # A genuine signature under the right key over a DIFFERENT real sighash. Its source
  # transaction spends the ALTERNATIVE coin, so it is a real sighash, not a contrivance.
  label="leg${leg}_foreign_signature"
  sighash_leg "${label}_src" "$pk" "$atx" "$avout" "$fval" "" "" "$dspk" none \
    || { cat "$OUT/${label}_src_sighash.log" >&2; fail_out "$label: foreign sighash failed"; }
  control_sign "$pk" "$sk" "$OUT/${label}_src_sighash.bin" "$OUT/sig_${label}.bin" \
    || fail_out "$label: control signing failed"
  hex="$(assemble "leg${leg}" "$OUT/sig_${label}.bin" none "$label")" || fail_out "$label: could not assemble"
  judge "$ch" "$label" "$hex" negative

  # A signature over something that is not a sighash at all.
  label="leg${leg}_non_sighash_msg"
  head -c 32 /dev/zero | tr '\0' 'A' > "$OUT/${label}_msg.bin"
  control_sign "$pk" "$sk" "$OUT/${label}_msg.bin" "$OUT/sig_${label}.bin" \
    || fail_out "$label: control signing failed"
  hex="$(assemble "leg${leg}" "$OUT/sig_${label}.bin" none "$label")" || fail_out "$label: could not assemble"
  judge "$ch" "$label" "$hex" negative

  # THE CROSS-LEG CONTROL: the OTHER leg's settlement signature, on this leg. This is the
  # Bitcoin counterpart of the EVM runner's replay control. If it were accepted, one
  # signature would settle both legs and two payouts would not evidence a swap.
  if [ -n "$othersig" ]; then
    label="leg${leg}_cross_leg_signature"
    hex="$(assemble "leg${leg}" "$othersig" none "$label")" || fail_out "$label: could not assemble"
    judge "$ch" "$label" "$hex" negative
  fi
}

# --- 13. leg B: controls, then SETTLE on chain 2 -------------------------------------------
LEGB_HEX="$(assemble legB "$VEC/sigma2.bin" none legB)" || { cat "$OUT/legB_assemble.log" >&2; fail_out "assembling leg B failed"; }
[ -n "$LEGB_HEX" ] || fail_out "leg B assembled to an empty transaction"
judge 2 legB_valid "$LEGB_HEX" valid
[ -z "$FAILED_CONTROLS" ] || fail_out "leg B's valid case did not behave as required:$FAILED_CONTROLS (patched must ACCEPT and stock must ACCEPT; without both, nothing below is attributable)"

run_leg_controls 2 B "$VEC/pk2.bin" "$VEC/sk2.bin" "$VEC/sigma2.bin" \
  "$FUND_B1" "$VOUT_B1" "$VAL_B1" "$FUND_B2" "$VOUT_B2" "$DEST_B_SPK" ""
[ -z "$FAILED_CONTROLS" ] || fail_out "leg B negative controls behaved wrongly:$FAILED_CONTROLS"

cli_2p generateblock "$(cli_2p getnewaddress)" "[\"$LEGB_HEX\"]" true > "$OUT/legB_settle.json" 2> "$OUT/legB_settle.err" \
  || { cat "$OUT/legB_settle.err" >&2; fail_out "the patched node refused to MINE leg B"; }
BH_B="$(jqpy 'import json,sys;print(json.load(open(sys.argv[1]))["hash"])' "$OUT/legB_settle.json")"
sync_chain cli_2p cli_2s "chain 2"
TXID_B="$(cli_2p getblock "$BH_B" | jqpy 'import json,sys;print(json.load(sys.stdin)["tx"][1])')"
cli_2p getrawtransaction "$TXID_B" true "$BH_B" > "$OUT/legB_mined.json"
cli_2s getblock "$BH_B" > "$OUT/legB_block_seen_by_stock.json" \
  || fail_out "the stock node does not have the block containing leg B"

# --- 14. recover sigma_2 FROM THE MINED WITNESS --------------------------------------------
# The boundary between signature and key is located by the commitment the FUNDING OUTPUT
# made, never by a known signature length — see bitcoin/tools/btc_recover_sig.py.
python3 "$TOOLS/btc_recover_sig.py" --tx-json "$OUT/legB_mined.json" \
  --out-sig "$VEC/sigma2_from_chain.bin" --out-pk "$OUT/pk2_from_chain.bin" \
  --out "$OUT/recovered_sigB.json" > "$OUT/recover_sigB.log" 2>&1 \
  || { cat "$OUT/recover_sigB.log" >&2; fail_out "could not recover sigma_2 from the mined witness"; }
cat "$OUT/recover_sigB.log"

# --- 15. u2: Ext from the CHAIN's bytes, then Adapt leg A ------------------------------------
# extract_and_adapt takes the observed signature as a path argument and cannot reach
# sigma2.bin — u2's knowledge really is limited to what the ledger published.
if ! (cd "$REPO/ref" && ./test/extract_and_adapt "$VEC" "$VEC/sigma2_from_chain.bin") \
      > "$OUT/extract_adapt.log" 2>&1; then
  cat "$OUT/extract_adapt.log" >&2
  fail_out "Ext/Adapt from the chain-observed signature failed — see extract_adapt.log"
fi
cat "$OUT/extract_adapt.log"

"$SHIM/las_btc_tool" verify "$VEC/pk1.bin" "$VEC/sigma1.bin" "$VEC/legA_msg.bin" \
  > "$OUT/shim_verify_sigma1.log" 2>&1 \
  || { cat "$OUT/shim_verify_sigma1.log" >&2; fail_out "sigma_1 (adapted under the EXTRACTED witness) is refused by LASConsensusVerify"; }
cat "$OUT/shim_verify_sigma1.log"

# --- 16. leg A: controls, then SETTLE on chain 1 ---------------------------------------------
LEGA_HEX="$(assemble legA "$VEC/sigma1.bin" none legA)" || { cat "$OUT/legA_assemble.log" >&2; fail_out "assembling leg A failed"; }
[ -n "$LEGA_HEX" ] || fail_out "leg A assembled to an empty transaction"

# CHAIN ISOLATION, recorded before leg A settles. Leg B's settled transaction is offered to
# chain 1. It must be refused — but the reason is that the COIN DOES NOT EXIST on chain 1,
# which evidences that the two ledgers are separate. It is NOT evidence that a signature is
# bound to a chain: BIP341's sighash contains no chain id.
ISO_RC=0
cli_1p generateblock "$ADDR_A" "[\"$LEGB_HEX\"]" false > "$OUT/chain_isolation.json" 2> "$OUT/chain_isolation.err" || ISO_RC=$?
ISO_NOTE="$(tr -d '\r' < "$OUT/chain_isolation.err" | tr '\n' ' ' | sed 's/  */ /g' | cut -c1-160)"
[ "$ISO_RC" -ne 0 ] || fail_out "chain 1 ACCEPTED leg B's transaction — the two chains share coins and this is not a cross-chain swap"
{
  echo "leg B's settled transaction offered to CHAIN 1: refused (exit $ISO_RC)"
  echo "  reason: $ISO_NOTE"
  echo "  reading: the two LEDGERS are separate — leg B's coin does not exist on chain 1."
  echo "  NOT a claim that the signature is bound to a chain: BIP341 has no chain id."
} | tee "$OUT/chain_isolation.txt"

judge 1 legA_valid "$LEGA_HEX" valid
[ -z "$FAILED_CONTROLS" ] || fail_out "leg A's valid case did not behave as required:$FAILED_CONTROLS"

run_leg_controls 1 A "$VEC/pk1.bin" "$VEC/sk1.bin" "$VEC/sigma1.bin" \
  "$FUND_A1" "$VOUT_A1" "$VAL_A1" "$FUND_A2" "$VOUT_A2" "$DEST_A_SPK" "$VEC/sigma2.bin"
[ -z "$FAILED_CONTROLS" ] || fail_out "leg A negative controls behaved wrongly:$FAILED_CONTROLS"

cli_1p generateblock "$(cli_1p getnewaddress)" "[\"$LEGA_HEX\"]" true > "$OUT/legA_settle.json" 2> "$OUT/legA_settle.err" \
  || { cat "$OUT/legA_settle.err" >&2; fail_out "the patched node refused to MINE leg A"; }
BH_A="$(jqpy 'import json,sys;print(json.load(open(sys.argv[1]))["hash"])' "$OUT/legA_settle.json")"
sync_chain cli_1p cli_1s "chain 1"
TXID_A="$(cli_1p getblock "$BH_A" | jqpy 'import json,sys;print(json.load(sys.stdin)["tx"][1])')"
cli_1p getrawtransaction "$TXID_A" true "$BH_A" > "$OUT/legA_mined.json"
cli_1s getblock "$BH_A" > "$OUT/legA_block_seen_by_stock.json" \
  || fail_out "the stock node does not have the block containing leg A"

# --- 17. did the coins actually move? ---------------------------------------------------------
# A mined transaction is not yet a payout. For each leg: the escrowed coin must be GONE from
# the UTXO set, and the beneficiary must hold a new one of the expected value. Both are read
# from `gettxout`, i.e. the UTXO set, for the same reason as the independence check above.
cli_1p gettxout "$FUND_A1" "$VOUT_A1" > "$OUT/legA_funding_utxo_after.json" 2>/dev/null || true
cli_2p gettxout "$FUND_B1" "$VOUT_B1" > "$OUT/legB_funding_utxo_after.json" 2>/dev/null || true
cli_1p gettxout "$TXID_A" 0 > "$OUT/legA_payout_utxo.json" 2>/dev/null || true
cli_2p gettxout "$TXID_B" 0 > "$OUT/legB_payout_utxo.json" 2>/dev/null || true

python3 "$TOOLS/btc_model_check.py" --decoded "$OUT/legA_mined.json" --label "LAS legA" \
  --json-out "$OUT/legA_sizecheck.json" | tee "$OUT/legA_size_model.txt" \
  || fail_out "the size model disagrees with the client on leg A"
python3 "$TOOLS/btc_model_check.py" --decoded "$OUT/legB_mined.json" --label "LAS legB" \
  --json-out "$OUT/legB_sizecheck.json" | tee "$OUT/legB_size_model.txt" \
  || fail_out "the size model disagrees with the client on leg B"

# --- 18. verdict, computed FROM the clients' records and the retained bytes ---------------------
set +o pipefail
python3 - "$OUT/legB_mined.json" "$OUT/legA_mined.json" \
         "$VEC/sigma2.bin" "$VEC/sigma2_from_chain.bin" "$VEC/sigma1.bin" \
         "$VEC/witness.bin" "$VEC/witness_extracted.bin" \
         "$OUT/legA_funding_utxo_after.json" "$OUT/legB_funding_utxo_after.json" \
         "$OUT/legA_payout_utxo.json" "$OUT/legB_payout_utxo.json" \
         "$DEST_A" "$DEST_B" "$VAL_A1" "$VAL_B1" "$FEE_SAT" \
         "$OUT/recovered_sigB.json" "$PI" \
         <<'PY' | tee -a "$OUT/verdict.txt"
import hashlib, json, os, sys

(minedB, minedA, sig2_local, sig2_chain, sig1_local, wit_honest, wit_ext,
 fundA_after, fundB_after, payA, payB, destA, destB, valA, valB, fee, recjson, pi) = sys.argv[1:19]
valA, valB, fee = int(valA), int(valB), int(fee)

def blob(p):
    return open(p, "rb").read() if os.path.exists(p) else b""

def js(p):
    if not os.path.exists(p) or os.path.getsize(p) == 0:
        return None
    try:
        return json.load(open(p))
    except json.JSONDecodeError:
        return None

tb, ta = json.load(open(minedB)), json.load(open(minedA))
rec = json.load(open(recjson))
s2l, s2c, s1l = blob(sig2_local), blob(sig2_chain), blob(sig1_local)
wh, we = blob(wit_honest), blob(wit_ext)

def sha(b):
    return hashlib.sha256(b).hexdigest()[:32] if b else "(absent)"

print("legB settlement tx  : %s" % tb.get("txid"))
print("legB block          : %s (confirmations %s)" % (tb.get("blockhash"), tb.get("confirmations")))
print("legA settlement tx  : %s" % ta.get("txid"))
print("legA block          : %s (confirmations %s)" % (ta.get("blockhash"), ta.get("confirmations")))
print()
print("sigma_2 recovered from the MINED witness : %d bytes, split found at offset %d"
      % (rec["sig_bytes"], rec["split_offset"]))
print("  boundary located by                    : %s" % rec["boundary_source"])
print("  identical to the Adapt output          : %s   <- Adapt provenance"
      % ("YES" if s2c and s2c == s2l else "NO"))
print("  sha256(sigma_2 from chain)             : %s..." % sha(s2c))
print("extracted witness == Gen's witness       : %s" % ("YES" if we and we == wh else "NO"))
print("  sha256(witness extracted)              : %s..." % sha(we))
print("sigma_1 (adapted under EXTRACTED y)      : %d bytes, sha256 %s..." % (len(s1l), sha(s1l)))

fa, fb = js(fundA_after), js(fundB_after)
pa, pb = js(payA), js(payB)

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

print()
print("legA escrowed coin spent : %s" % ("YES" if fa is None else "NO — still unspent"))
print("legB escrowed coin spent : %s" % ("YES" if fb is None else "NO — still unspent"))
print("legA payout              : %s sat to %s (expected %d to %s)"
      % (sat(pa), addr_of(pa), valA - fee, destA))
print("legB payout              : %s sat to %s (expected %d to %s)"
      % (sat(pb), addr_of(pb), valB - fee, destB))

fail = []
for name, t in (("legB", tb), ("legA", ta)):
    if not t.get("blockhash"):
        fail.append("%s was not mined" % name)
    if int(t.get("confirmations", 0)) < 1:
        fail.append("%s reports %s confirmations" % (name, t.get("confirmations")))

# THE PROVENANCE CHECK. Leg A was settled with a signature derived from the witness that
# extract_and_adapt recovered from these bytes; if they are not the bytes the chain
# carried, the run demonstrated an adaptor signature rather than an atomic swap.
if not s2c:
    fail.append("no signature was recovered from the mined witness")
elif s2c != s2l:
    fail.append("the sigma_2 recovered from the mined witness differs from the Adapt "
                "output that was broadcast — leg A's witness cannot be attributed to the ledger")
if not we:
    fail.append("no extracted witness was produced")
elif we != wh:
    fail.append("the extracted witness differs from the one Gen produced")
if not s1l:
    fail.append("sigma_1 was not produced")

if fa is not None:
    fail.append("leg A's escrowed coin is still unspent — the leg did not settle")
if fb is not None:
    fail.append("leg B's escrowed coin is still unspent — the leg did not settle")
for name, u, want, dest in (("legA", pa, valA - fee, destA), ("legB", pb, valB - fee, destB)):
    if u is None:
        fail.append("%s produced no spendable payout output" % name)
        continue
    if sat(u) != want:
        fail.append("%s paid %s sat, expected %d" % (name, sat(u), want))
    if addr_of(u) != dest:
        fail.append("%s paid %s, not the intended beneficiary %s" % (name, addr_of(u), dest))

if fail:
    print()
    print("RESULT: FAIL — %d post-condition(s) not met:" % len(fail))
    for i, f in enumerate(fail, 1):
        print("  %d. %s" % (i, f))
    sys.exit(1)

# Fig. 1 has u2 verify pi BEFORE it pre-signs. A run that skipped it satisfied every
# post-condition above but did not execute the protocol, so it is reported as incomplete
# rather than as a success. The caller also declines to move `latest` in that case.
if pi != "1":
    print()
    print("RESULT: INCOMPLETE — every post-condition above was met, but PI=0 omitted Fig. 1's")
    print("        proof-of-knowledge step, so this run is NOT a full Fig. 1 execution and")
    print("        must not be reported as one.")
    sys.exit(2)

print()
print("RESULT: a two-leg atomic swap settled on TWO independent Bitcoin regtest chains.")
print("        Both legs were authorised solely by LAS signatures verified in consensus")
print("        under OP_CHECKLASSIGVERIFY by a patched Bitcoin Core. Leg B's adapted")
print("        signature was read back out of the MINED witness, its boundary located by")
print("        the funding output's own commitment; Ext recovered the witness from those")
print("        bytes alone, and leg A was adapted with it and settled on the other chain.")
PY
VERDICT_RC=${PIPESTATUS[0]}
set -o pipefail
[ "$VERDICT_RC" -eq 2 ] && [ "$PI" != "1" ] && INCOMPLETE=1 || INCOMPLETE=0
[ "$VERDICT_RC" -eq 0 ] || [ "$INCOMPLETE" -eq 1 ] \
  || fail_out "post-conditions not met — see the enumerated list in verdict.txt"

# --- 19. the control table, appended to the same verdict ----------------------------------------
{
  echo
  echo "--- every case, put to BOTH nodes of its chain as a CONSENSUS question ---"
  echo "    (generateblock submit=false: block validation, not mempool policy)"
  echo
  cat "$OUT/controls.txt"
  echo
  echo "  Required and checked exactly: valid = ACCEPTED/ACCEPTED; every negative ="
  echo "  REJECTED/ACCEPTED. The stock node still treats 0xbb as OP_SUCCESS, so it accepts"
  echo "  unconditionally and cannot be reacting to the signature. The difference between"
  echo "  the two columns is the new consensus rule, and nothing else."
  echo
  cat "$OUT/chain_independence.txt"
  echo
  cat "$OUT/chain_isolation.txt"
  echo
  cat "$OUT/legA_size_model.txt"
  cat "$OUT/legB_size_model.txt"
  echo
  echo "Both settlement signatures are Adapt's output. base_sign_det is used by the shim's"
  echo "selftest and to make the control signatures above, but neither settlement signature"
  echo "comes from it and no transaction it signed was mined. Their Adapt provenance rests"
  echo "on the byte-for-byte equality of the chain-recovered signature with the retained"
  echo "Adapt output. Ext succeeding on the mined sigma_2 is additional, weaker evidence:"
  echo "it shows the chain-observed signature is compatible with the pre-signature and"
  echo "yields a witness satisfying A*y = Y — not that Adapt produced those bytes."
  echo
  echo "SCOPE: honest path only. The tapleaf carries no refund branch, so timeouts and the"
  echo "refund case are not implemented and nothing here speaks to them. A patched node is"
  echo "not Bitcoin."
} | tee -a "$OUT/verdict.txt"

if [ "$INCOMPLETE" -eq 1 ]; then
  echo
  echo "evidence written (INCOMPLETE run, pi omitted): evidence/btc_twoleg/$RUN_ID/" >&2
  echo "latest was NOT moved — only a PI=1 run is a full Fig. 1 execution." >&2
  exit 1
fi

ln -sfn "$RUN_ID" "$REPO/evidence/btc_twoleg/latest"

echo
echo "evidence written: evidence/btc_twoleg/$RUN_ID/"
echo "  verdict.txt              — pass/fail computed FROM both clients' records and the bytes"
echo "  leg{A,B}_mined.json      — each client's record of its settled leg"
echo "  recovered_sigB.json      — the chain-byte recovery, with the commitment used"
echo "  controls.txt             — patched vs stock, per case, per leg"
echo "  chain_independence.txt / chain_isolation.txt — that the two ledgers are two"
echo "  setup_vectors.log        — Gen and pi (proved AND verified)"
echo "  presign.log              — PreSign + both PreVerify gates"
echo "  extract_adapt.log        — Ext from the chain's bytes, then Adapt"
echo "  shim_verify_sigma{1,2}.log — the node's own predicate, run before broadcasting"
echo "  applied.patch selftest.log environment.txt"
echo
echo "decisive rows in verdict.txt:"
echo "  'identical to the Adapt output          : YES'   (Adapt provenance)"
echo "  'extracted witness == Gen's witness      : YES'   (Ext really recovered y)"
echo "  'legA/legB payout ... to <beneficiary>'          (both legs settled)"
