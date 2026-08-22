#!/usr/bin/env bash
# Settle a Bitcoin transaction whose ONLY authorisation is a LAS signature verified by a
# patched consensus rule.
#
#   BTC_TAG=v31.1 BTC_SRC=/path/to/patched-src BTC_BIN_PATCHED=.../build/bin/bitcoind \
#   BTC_BIN_STOCK=.../bitcoin-31.1/bin/bitcoind ./scripts/run_btc_las_node.sh
#
# WHAT STAGE 3 CLAIMS THAT STAGE 2 DID NOT
# ----------------------------------------
# Stage 2 carried LAS bytes past a stock node: real transaction, real mining, but an
# ordinary Schnorr signature authorised the spend and the LAS bytes were dropped. Here the
# leaf is `<sha256(pk)> OP_CHECKLASSIGVERIFY OP_1` and there is no Schnorr signature
# anywhere. If the patched rule does not verify the LAS signature, the transaction does not
# settle. Accepted and mined therefore means verified.
#
# CONSENSUS, NOT POLICY. Every verdict comes from `generateblock ... submit=false`, which
# builds a block containing the transaction and runs it through block validation without
# submitting. That asks the consensus question directly. `testmempoolaccept` would answer a
# different one — it mixes in relay policy, under which these witnesses are non-standard for
# a reason Stage 2 already measured and which has nothing to do with LAS. The settlement
# likewise goes in through `generateblock ... submit=true`.
#
# THE DIFFERENTIAL CONTROL, AND WHY IT IS THE HEART OF THIS RUN
# -------------------------------------------------------------
# "The patched node rejected it" is weak on its own — nodes reject transactions for dozens
# of reasons. So every case is put to BOTH a patched and a STOCK node of the same release,
# sharing one chain:
#
#   * On the stock node 0xbb is still an OP_SUCCESSx, so tapscript succeeds
#     UNCONDITIONALLY: it accepts the spend whatever the signature says. That is the
#     soft-fork property — old nodes stay compatible — and it makes the stock node a
#     perfect control, because it cannot be reacting to the signature.
#   * The patched node accepts ONLY with a valid signature.
#
# The required pattern is exact, and checked as such:
#   valid case      patched ACCEPTED     and stock ACCEPTED
#   every negative  patched REJECTED:*   and stock ACCEPTED
# A stock rejection would mean the transaction was defective for an unrelated reason, so
# the patched node's refusal could not be attributed to the new rule; an empty or
# unrecognised verdict is a failure, never a quiet pass.
#
# WHAT IS PINNED. The patched tree must differ from the tag by EXACTLY the committed patch,
# compared by digest, including staged changes and with no untracked files — a patch that
# omitted the vendored sources would not reproduce this build. The shim's selftest must pass
# before a node is started, and the consensus seed compiled into the tooling must equal
# SHA-256 of its documented preimage.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN_ID="$(date +%Y%m%d_%H%M%S)"
OUT="$REPO/evidence/btc_las_node/$RUN_ID"
TOOLS="$REPO/bitcoin/tools"
SHIM="$REPO/bitcoin/las_consensus"
PATCH="$REPO/bitcoin/patches/0001-op-checklassigverify-v31.1.patch"

RPC_PATCHED="${RPC_PATCHED:-18575}"
RPC_STOCK="${RPC_STOCK:-18576}"
P2P_PATCHED="${P2P_PATCHED:-18585}"
P2P_STOCK="${P2P_STOCK:-18586}"
FEE_SAT="${FEE_SAT:-200000}"
SEED_PREIMAGE="LAS-CONSENSUS-PARAMS-v1"

: "${BTC_TAG:?set BTC_TAG, e.g. v31.1}"
: "${BTC_SRC:?set BTC_SRC to the PATCHED Bitcoin Core source tree}"
: "${BTC_BIN_PATCHED:?set BTC_BIN_PATCHED to the bitcoind built from BTC_SRC}"
: "${BTC_BIN_STOCK:?set BTC_BIN_STOCK to the stock release bitcoind of the same tag}"
CLI="${CLI:-$(dirname "$BTC_BIN_PATCHED")/bitcoin-cli}"

mkdir -p "$OUT"
DATA_P="$OUT/datadir_patched"
DATA_S="$OUT/datadir_stock"
cleanup() {
  "$CLI" -regtest -datadir="$DATA_P" -rpcport="$RPC_PATCHED" stop >/dev/null 2>&1 || true
  "$CLI" -regtest -datadir="$DATA_S" -rpcport="$RPC_STOCK"  stop >/dev/null 2>&1 || true
}
trap cleanup EXIT

fail_out() {
  {
    echo; echo "RESULT: FAIL — $1"; echo
    echo "run_id: $RUN_ID"
    echo "Retained: datadir_*/regtest/debug.log, *_consensus_*.{json,err}, *_spend.json,"
    echo "          selftest.log, applied.patch, environment.txt"
  } | tee -a "$OUT/verdict.txt"
  echo "evidence written (FAILING run): evidence/btc_las_node/$RUN_ID/" >&2
  exit 1
}

cli_p() { "$CLI" -regtest -datadir="$DATA_P" -rpcport="$RPC_PATCHED" "$@"; }
cli_s() { "$CLI" -regtest -datadir="$DATA_S" -rpcport="$RPC_STOCK"  "$@"; }
jqpy()  { python3 -c "$1" "${@:2}"; }

# --- 1. pin gates -------------------------------------------------------------------
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

# `git diff HEAD`, not `git diff`: it includes STAGED changes too. The vendored LAS sources
# are intent-to-add, so a plain worktree diff could miss them and the patch would then not
# reproduce this build.
git -C "$BTC_SRC" diff HEAD > "$OUT/applied.patch"
APPLIED_SHA="$(sha256sum "$OUT/applied.patch" | cut -d' ' -f1)"
COMMITTED_SHA="$(sha256sum "$PATCH" | cut -d' ' -f1)"
[ "$APPLIED_SHA" = "$COMMITTED_SHA" ] \
  || fail_out "the patch applied to $BTC_SRC (sha256 $APPLIED_SHA) is not the committed one ($COMMITTED_SHA). Diff applied.patch against bitcoin/patches/ to see what else changed."

UNTRACKED="$(git -C "$BTC_SRC" status --porcelain --untracked-files=all \
             | grep '^??' | grep -v '^?? build/' || true)"
[ -z "$UNTRACKED" ] || { printf '%s\n' "$UNTRACKED" > "$OUT/untracked.txt"; \
  fail_out "the patched tree has untracked files the committed patch does not contain (see untracked.txt) — the patch would not reproduce this build"; }

# --- 2. the shim, before any node exists ---------------------------------------------
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

# --- 3. two nodes, one chain ------------------------------------------------------------
mkdir -p "$DATA_P" "$DATA_S"
"$BTC_BIN_PATCHED" -regtest -datadir="$DATA_P" -rpcport="$RPC_PATCHED" -port="$P2P_PATCHED" \
  -bind=127.0.0.1 -fallbackfee=0.0002 -daemonwait \
  > "$OUT/node_patched_start.log" 2>&1 || { cat "$OUT/node_patched_start.log" >&2; fail_out "patched node failed to start"; }
"$BTC_BIN_STOCK" -regtest -datadir="$DATA_S" -rpcport="$RPC_STOCK" -port="$P2P_STOCK" \
  -bind=127.0.0.1 -fallbackfee=0.0002 -daemonwait -connect=127.0.0.1:"$P2P_PATCHED" \
  > "$OUT/node_stock_start.log" 2>&1 || { cat "$OUT/node_stock_start.log" >&2; fail_out "stock node failed to start"; }

cli_p -rpcwait getblockchaininfo > /dev/null || fail_out "patched node did not answer"
cli_s -rpcwait getblockchaininfo > /dev/null || fail_out "stock node did not answer"

sync_nodes() {
  local a b
  for _ in $(seq 1 200); do
    a="$(cli_p getbestblockhash 2>/dev/null || true)"
    b="$(cli_s getbestblockhash 2>/dev/null || true)"
    [ -n "$a" ] && [ "$a" = "$b" ] && return 0
    sleep 0.2
  done
  fail_out "the two nodes did not converge on one chain — a verdict from an unsynced node would be about missing inputs, not about the rule"
}

cli_p createwallet las > /dev/null 2>&1 || fail_out "no wallet support in the patched build"
cli_p generatetoaddress 101 "$(cli_p getnewaddress)" > /dev/null
sync_nodes
mine_p() { cli_p generatetoaddress 1 "$(cli_p getnewaddress)" | jqpy 'import json,sys;print(json.load(sys.stdin)[0])'; }

# --- 4. environment record ----------------------------------------------------------------
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
  echo "bitcoind_patched=$BTC_BIN_PATCHED"
  echo "bitcoind_patched_sha256=$(sha256sum "$BTC_BIN_PATCHED" | cut -d' ' -f1)"
  echo "bitcoind_stock=$BTC_BIN_STOCK"
  echo "bitcoind_stock_sha256=$(sha256sum "$BTC_BIN_STOCK" | cut -d' ' -f1)"
  echo "--- LAS consensus parameters ---"
  echo "consensus_seed=$SEED"
  echo "consensus_seed_preimage=$SEED_PREIMAGE (SHA-256 VERIFIED)"
  echo "opcode=OP_CHECKLASSIGVERIFY (0xbb, a BIP342 OP_SUCCESSx slot)"
  echo "sighash=BIP341 SIGHASH_DEFAULT only"
  echo "shim_selftest=PASSED (positive + 7 negative controls)"
  echo "instrument=generateblock submit=false — CONSENSUS validation, not mempool policy"
  echo "note=on the STOCK node 0xbb remains OP_SUCCESS, so it accepts these spends"
  echo "note=unconditionally; that is the soft-fork property and the differential control."
} > "$OUT/environment.txt"

# --- 5. key, address, funding -------------------------------------------------------------
KEYDIR="$OUT/las_key"; mkdir -p "$KEYDIR"
"$SHIM/las_btc_tool" keygen "$KEYDIR" "$(printf '%064d' 7)" > "$OUT/keygen.log" 2>&1 \
  || { cat "$OUT/keygen.log" >&2; fail_out "LAS keygen failed"; }

python3 "$TOOLS/btc_las_spend.py" sighash --core-src "$BTC_SRC" --pk "$KEYDIR/pk.bin" \
  --address-only --out "$OUT/address.json" > "$OUT/address.log" 2>&1 \
  || { cat "$OUT/address.log" >&2; fail_out "could not derive the LAS taproot address"; }
ADDR="$(jqpy 'import json,sys;print(json.load(open(sys.argv[1]))["address"])' "$OUT/address.json")"
COINBASE_ADDR="$(cli_p getnewaddress)"

# Two funding outputs: the second is a REAL alternative outpoint, so the input-binding
# control can point at a coin that exists. Pointing at a nonexistent one would be rejected
# for missing inputs and would prove nothing about the sighash covering inputs.
FUND1="$(cli_p sendtoaddress "$ADDR" 1.0)"; BH1="$(mine_p)"
FUND2="$(cli_p sendtoaddress "$ADDR" 1.0)"; BH2="$(mine_p)"
sync_nodes

vout_of() { cli_p getrawtransaction "$1" true "$2" \
  | jqpy 'import json,sys;t=json.load(sys.stdin);a=sys.argv[1];print(next(o["n"] for o in t["vout"] if o["scriptPubKey"].get("address")==a))' "$ADDR"; }
VOUT1="$(vout_of "$FUND1" "$BH1")"
VOUT2="$(vout_of "$FUND2" "$BH2")"
VAL1="$(cli_p getrawtransaction "$FUND1" true "$BH1" \
  | jqpy 'import json,sys;t=json.load(sys.stdin);n=int(sys.argv[1]);print(int(round(next(o["value"] for o in t["vout"] if o["n"]==n)*1e8)))' "$VOUT1")"

: > "$OUT/controls.txt"

# $3 = signature file, or the literal SIGN_THIS to sign the sighash this case itself
# produces — needed by wrong_prevout_amt, whose whole point is a signature over a FALSE
# sighash: reusing the honest signature would leave the transaction unchanged and test
# nothing.
build_case() {
  local label="$1" mut="$2" sigfile="$3"
  python3 "$TOOLS/btc_las_spend.py" sighash --core-src "$BTC_SRC" --pk "$KEYDIR/pk.bin" \
    --txid "$FUND1" --vout "$VOUT1" --value-sat "$VAL1" --fee-sat "$FEE_SAT" \
    --alt-txid "$FUND2" --alt-vout "$VOUT2" --mutate "$mut" \
    --out-sighash "$OUT/${label}_sighash.bin" --state "$OUT/${label}_state.json" \
    --out "$OUT/${label}_sighash.json" > "$OUT/${label}_sighash.log" 2>&1 || return 1
  if [ "$sigfile" = "SIGN_THIS" ]; then
    cp "$OUT/${label}_sighash.bin" "$KEYDIR/msg.bin"
    "$SHIM/las_btc_tool" sign "$KEYDIR" "$KEYDIR/msg.bin" > "$OUT/${label}_sign.log" 2>&1 || return 1
    cp "$KEYDIR/sig.bin" "$OUT/sig_${label}.bin"
    sigfile="$OUT/sig_${label}.bin"
  fi
  python3 "$TOOLS/btc_las_spend.py" assemble --core-src "$BTC_SRC" \
    --state "$OUT/${label}_state.json" --sig "$sigfile" --mutate "$mut" \
    --out "$OUT/${label}_spend.json" > "$OUT/${label}_assemble.log" 2>&1 || return 1
  jqpy 'import json,sys;print(json.load(open(sys.argv[1]))["raw_hex"])' "$OUT/${label}_spend.json"
}

# The honest signature, over the honest transaction's own sighash.
python3 "$TOOLS/btc_las_spend.py" sighash --core-src "$BTC_SRC" --pk "$KEYDIR/pk.bin" \
  --txid "$FUND1" --vout "$VOUT1" --value-sat "$VAL1" --fee-sat "$FEE_SAT" \
  --alt-txid "$FUND2" --alt-vout "$VOUT2" \
  --out-sighash "$KEYDIR/msg.bin" --state "$OUT/valid_state.json" \
  --out "$OUT/valid_sighash.json" > "$OUT/valid_sighash.log" 2>&1 \
  || { cat "$OUT/valid_sighash.log" >&2; fail_out "sighash phase failed"; }
"$SHIM/las_btc_tool" sign "$KEYDIR" "$KEYDIR/msg.bin" > "$OUT/sign.log" 2>&1 \
  || { cat "$OUT/sign.log" >&2; fail_out "LAS signing failed"; }
cat "$OUT/sign.log"
cp "$KEYDIR/sig.bin" "$OUT/sig_valid.bin"

# Two signature-substitution controls, produced HERE because they need a different
# signature rather than a different transaction — btc_las_spend.py deliberately refuses to
# list mutations it cannot itself perform.
python3 "$TOOLS/btc_las_spend.py" sighash --core-src "$BTC_SRC" --pk "$KEYDIR/pk.bin" \
  --txid "$FUND2" --vout "$VOUT2" --value-sat "$VAL1" --fee-sat "$FEE_SAT" \
  --out-sighash "$KEYDIR/msg.bin" --state "$OUT/foreign_state.json" \
  --out "$OUT/foreign_sighash.json" > /dev/null 2>&1 || fail_out "foreign sighash failed"
"$SHIM/las_btc_tool" sign "$KEYDIR" "$KEYDIR/msg.bin" > /dev/null 2>&1 || fail_out "foreign sign failed"
cp "$KEYDIR/sig.bin" "$OUT/sig_foreign.bin"

head -c 32 /dev/zero | tr '\0' 'A' > "$KEYDIR/msg.bin"
"$SHIM/las_btc_tool" sign "$KEYDIR" "$KEYDIR/msg.bin" > /dev/null 2>&1 || fail_out "non-sighash sign failed"
cp "$KEYDIR/sig.bin" "$OUT/sig_nonsighash.bin"

python3 "$TOOLS/btc_las_spend.py" assemble --core-src "$BTC_SRC" \
  --state "$OUT/valid_state.json" --sig "$OUT/sig_valid.bin" --mutate none \
  --out "$OUT/valid_spend.json" > "$OUT/valid_assemble.log" 2>&1 \
  || { cat "$OUT/valid_assemble.log" >&2; fail_out "assembling the valid spend failed"; }
cat "$OUT/valid_assemble.log"
VALID_HEX="$(jqpy 'import json,sys;print(json.load(open(sys.argv[1]))["raw_hex"])' "$OUT/valid_spend.json")"

# --- 6. ask BOTH nodes the CONSENSUS question ------------------------------------------------
# `generateblock ... submit=false` assembles a block containing the transaction and runs
# block validation on it without submitting. That is consensus, not policy.
#
# ONLY a `TestBlockValidity failed:` error counts as a rejection. Any other failure — bad
# parameters, a dead RPC, a wallet problem — is NOT the node judging the transaction, and
# recording it as "REJECTED" would manufacture a passing negative control out of a broken
# run. Those return 1 and are propagated as hard failures instead.
consensus_check() { # $1 = node fn, $2 = label, $3 = tag, $4 = raw hex
  if "$1" generateblock "$ADDR" "[\"$4\"]" false > "$OUT/$2_consensus_$3.json" 2> "$OUT/$2_consensus_$3.err"; then
    echo "ACCEPTED"
    return 0
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
    || fail_out "$1: the PATCHED node failed for a reason that is NOT a consensus rejection — see ${1}_consensus_patched.err. A non-consensus error must never be recorded as a refusal."
  s="$(consensus_check cli_s "$1" stock "$2")" \
    || fail_out "$1: the STOCK node failed for a reason that is NOT a consensus rejection — see ${1}_consensus_stock.err."
  case "$p$s" in
    *missing*|*"bad-txns-inputs"*)
      fail_out "$1: a node reported missing inputs — the nodes are out of sync and this verdict is not about the rule" ;;
  esac
  printf '%-20s patched: %-58s stock: %s\n' "$1" "$p" "$s" | tee -a "$OUT/controls.txt" >&2
  echo "$p|$s"
}

# Exact verdict matching. Anything that is not precisely the required pair — including an
# empty string, or a reject reason in an unrecognised shape — counts as a failure.
FAILED_CONTROLS=""
expect() { # $1 = label, $2 = "patched|stock", $3 = valid|negative
  local v="$2" p s
  case "$v" in *"|"*) : ;; *) FAILED_CONTROLS="$FAILED_CONTROLS $1(unparseable-verdict)"; return ;; esac
  p="${v%%|*}"; s="${v##*|}"
  if [ -z "$p" ] || [ -z "$s" ]; then
    FAILED_CONTROLS="$FAILED_CONTROLS $1(empty-verdict)"; return
  fi
  # The stock node must ACCEPT in every case: it is the control, and it treats 0xbb as
  # OP_SUCCESS. If it ever rejects, the transaction is defective for an unrelated reason and
  # the patched node's answer is not attributable to the new rule.
  [ "$s" = "ACCEPTED" ] || FAILED_CONTROLS="$FAILED_CONTROLS $1(stock-not-ACCEPTED:not-attributable)"
  case "$3" in
    valid)
      [ "$p" = "ACCEPTED" ] || FAILED_CONTROLS="$FAILED_CONTROLS $1(patched-did-not-accept-valid)" ;;
    negative)
      case "$p" in
        "REJECTED: "*) : ;;
        *) FAILED_CONTROLS="$FAILED_CONTROLS $1(patched-did-not-reject)" ;;
      esac ;;
  esac
}

# One step per line: nesting these would let a build or RPC failure be swallowed by the
# outer command substitution and surface later as a confusing verdict.
run_case() { # $1 = label, $2 = mutation, $3 = signature file or SIGN_THIS, $4 = valid|negative
  local hex verdict
  hex="$(build_case "$1" "$2" "$3")" || fail_out "$1: could not build the case"
  [ -n "$hex" ] || fail_out "$1: built an empty transaction"
  verdict="$(ask "$1" "$hex")" || fail_out "$1: could not obtain a verdict from both nodes"
  expect "$1" "$verdict" "$4"
}

VALID_VERDICT="$(ask valid "$VALID_HEX")" || fail_out "could not obtain a verdict for the valid spend"
expect valid "$VALID_VERDICT" valid
[ -z "$FAILED_CONTROLS" ] || fail_out "the valid case did not behave as required:$FAILED_CONTROLS (patched must ACCEPT and stock must ACCEPT; without both, nothing below is attributable)"

for mut in output_amount output_recipient input_outpoint chunk_truncated chunk_reordered wrong_pubkey; do
  run_case "$mut" "$mut" "$OUT/sig_valid.bin" negative
done
run_case wrong_prevout_amt wrong_prevout_amt SIGN_THIS    negative
run_case foreign_signature none "$OUT/sig_foreign.bin"    negative
run_case non_sighash_msg   none "$OUT/sig_nonsighash.bin" negative

[ -z "$FAILED_CONTROLS" ] || fail_out "negative controls behaved wrongly:$FAILED_CONTROLS"

# --- 7. settle the valid spend ---------------------------------------------------------------
# submit=true this time: the block is built, validated and CONNECTED. Going in through
# generateblock rather than the mempool keeps the whole stage a consensus claim.
cli_p generateblock "$COINBASE_ADDR" "[\"$VALID_HEX\"]" true > "$OUT/settle.json" 2> "$OUT/settle.err" \
  || { cat "$OUT/settle.err" >&2; fail_out "the patched node refused to mine the valid LAS spend"; }
BH="$(jqpy 'import json,sys;print(json.load(open(sys.argv[1]))["hash"])' "$OUT/settle.json")"
sync_nodes

# The spend is the block's only non-coinbase transaction.
TXID="$(cli_p getblock "$BH" | jqpy 'import json,sys;print(json.load(sys.stdin)["tx"][1])')"
cli_p getrawtransaction "$TXID" true "$BH" > "$OUT/mined.json"
jqpy 'import json,sys
t=json.load(open(sys.argv[1]))
if not t.get("blockhash"): sys.exit("FATAL: the LAS spend was not mined")
print("mined in %s: vsize=%s weight=%s" % (t["blockhash"][:16]+"...", t["vsize"], t["weight"]))' \
  "$OUT/mined.json" || fail_out "the LAS spend was not mined"

# The stock node followed the chain containing it — the block is valid to old nodes too,
# which is what "soft fork" means.
cli_s getblock "$BH" > "$OUT/block_seen_by_stock.json" \
  || fail_out "the stock node does not have the block containing the LAS spend"

python3 "$TOOLS/btc_model_check.py" --decoded "$OUT/mined.json" --label LAS \
  --json-out "$OUT/sizecheck.json" | tee "$OUT/size_model.txt" \
  || fail_out "the size model disagrees with the client on the LAS spend"

# --- 8. verdict --------------------------------------------------------------------------------
{
  echo "run_id: $RUN_ID"
  echo
  echo "Bitcoin Core $BTC_TAG + bitcoin/patches/$(basename "$PATCH")"
  echo "  patch sha256      ${COMMITTED_SHA:0:32}...  (applied tree VERIFIED identical, no untracked files)"
  echo "  patched bitcoind  $(sha256sum "$BTC_BIN_PATCHED" | cut -c1-32)..."
  echo "  stock bitcoind    $(sha256sum "$BTC_BIN_STOCK" | cut -c1-32)..."
  echo "  consensus seed    $SEED"
  echo "                    (= SHA-256 of its documented preimage, verified this run)"
  echo
  echo "--- shim selftest, before any node existed ---"
  tail -3 "$OUT/selftest.log"
  echo
  echo "--- every case, put to BOTH nodes as a CONSENSUS question ---"
  echo "    (generateblock submit=false: block validation, not mempool policy)"
  echo
  cat "$OUT/controls.txt"
  echo
  echo "  Required and checked exactly: valid = ACCEPTED/ACCEPTED; every negative ="
  echo "  REJECTED/ACCEPTED. The stock node still treats 0xbb as OP_SUCCESS, so it accepts"
  echo "  unconditionally and cannot be reacting to the signature. The difference between"
  echo "  the two columns is the new consensus rule, and nothing else."
  echo
  echo "--- the valid spend, settled ---"
  jqpy 'import json,sys;t=json.load(open(sys.argv[1]));print("  txid   %s\n  block  %s\n  vsize  %s\n  weight %s"%(t["txid"],t["blockhash"],t["vsize"],t["weight"]))' "$OUT/mined.json"
  echo "  the stock node accepted the same block (soft-fork compatible)"
  echo
  cat "$OUT/size_model.txt"
  echo
  echo "RESULT: a Bitcoin transaction whose only authorisation is a LAS signature was"
  echo "        accepted and MINED by a patched Bitcoin Core node, because that node"
  echo "        verified the signature under OP_CHECKLASSIGVERIFY. Nine negative controls"
  echo "        were refused by the patched node and accepted by a stock node of the same"
  echo "        release, which attributes the refusals to the new rule."
} | tee -a "$OUT/verdict.txt"

ln -sfn "$RUN_ID" "$REPO/evidence/btc_las_node/latest"
echo
echo "evidence written: evidence/btc_las_node/$RUN_ID/"
echo "  verdict.txt              — pins, selftest, the two-node control table, the mined spend"
echo "  controls.txt             — patched vs stock, per case"
echo "  *_consensus_*.{json,err} — each node's block-validation answer, verbatim"
echo "  applied.patch            — the tree's diff, digest-checked against the committed patch"
echo "  selftest.log             — the shim's positive + 7 negative controls"
echo "  mined.json               — the client's record of the settled LAS spend"
