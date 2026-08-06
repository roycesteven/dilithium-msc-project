#!/usr/bin/env bash
# Put a real post-quantum-sized transaction to a real, STOCK Bitcoin Core regtest node:
# construct it, offer it to policy, broadcast it, mine it, and read back what the client
# says it weighs.
#
#   BTC_TAG=v28.0 BTC_BIN=/path/to/bitcoind BTC_SRC=/path/to/bitcoin-git-checkout \
#     ./scripts/run_btc_regtest_carriage.sh
#
# WHAT THIS RUN CLAIMS, AND WHAT IT DOES NOT
# ------------------------------------------
# CLAIMS, and only these: transaction construction, policy acceptance (or refusal),
# broadcast, mining, serialisation, and ledger carriage of LAS-sized objects — plus the
# real `vsize`/`weight` a client reports for them.
#
# DOES NOT CLAIM: anything about LAS verification. Stock Bitcoin Script has no lattice
# opcode. The carriage transaction is authorised by an ORDINARY BIP340 Schnorr signature
# and the LAS bytes are dropped from the stack. No line of this run's output may be
# described as Bitcoin verifying a LAS signature; that is Stage 3's question.
#
# THE NODE IS STOCK AND THE PIN IS EXACT
# --------------------------------------
# BTC_SRC must be a CLEAN GIT CHECKOUT AT EXACTLY THE PINNED TAG. Not "a tree that came
# from an archive with the right hash" — an archive digest pins the bytes you downloaded,
# not the tree you are running against, and it cannot show the tree is unmodified now. Not
# `git describe` either: describe also matches commits made AFTER the tag. The gate is
# HEAD == the tag's commit, and `git status --porcelain` empty. The binary must report the
# same tag. Recorded every run: tag, HEAD, the tag's commit, the binary's SHA-256, and the
# source archive's SHA-256 when one was supplied.
#
# ONE CHAIN, TWO POLICIES
# -----------------------
# `-acceptnonstdtxn=1` is a startup option, so the two regimes are two datadirs. But they
# are PEERED ONTO ONE CHAIN, and every funding block is synced before anything is offered
# to the mempool: a node that lacks the funding transaction answers `missing-inputs`, which
# would be mistaken for a policy difference. With both nodes synced, policy is the only
# variable between the two answers — which is the whole point of asking twice. A
# `missing-inputs` answer therefore fails the run rather than being reported.
#
# WHAT IS A GATE AND WHAT IS A RESULT
# -----------------------------------
# RESULT, recorded whatever it says: the DEFAULT-POLICY verdict for each transaction. Both
# acceptance and refusal are legitimate outcomes and neither is predicted here.
# GATE, failing the run: (a) the carriage transaction must actually MINE — without it there
# is no measured vsize/weight and the stage yields nothing; (b) this project's size model
# must equal the client's `vsize`/`weight` for every mined transaction. Note (b) is NOT
# "A1 must equal 110 vB": a DER signature's length varies, which is exactly why the oracle
# is the client and not a constant.
#
# Logs are written BY THE TOOLS. Never hand-edit them; never type a byte count into the
# report from here.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUN_ID="$(date +%Y%m%d_%H%M%S)"
OUT="$REPO/evidence/btc_regtest/$RUN_ID"
TOOLS="$REPO/bitcoin/tools"

RPC_STD="${RPC_STD:-18555}"     # default-policy node
RPC_PERM="${RPC_PERM:-18556}"   # -acceptnonstdtxn=1 node
P2P_STD="${P2P_STD:-18565}"
P2P_PERM="${P2P_PERM:-18566}"
FEE_SAT="${FEE_SAT:-100000}"    # generous; regtest, and fee policy is not the subject

# The wire sizes this experiment exists to carry (ref/serialize.h static asserts, D3 set).
# Asserted below: a run that silently carried differently-sized objects would produce a
# vsize that no longer describes a LAS settlement.
EXPECT_SIG_BYTES=6736
EXPECT_PK_BYTES=4416

mkdir -p "$OUT"

: "${BTC_TAG:?set BTC_TAG to the pinned Bitcoin Core release tag, e.g. v28.0}"
: "${BTC_BIN:?set BTC_BIN to the pinned bitcoind binary}"
: "${BTC_SRC:?set BTC_SRC to a clean git checkout of Bitcoin Core at exactly \$BTC_TAG}"
BTC_CLI="${BTC_CLI:-$(dirname "$BTC_BIN")/bitcoin-cli}"

DATA_STD="$OUT/datadir_std"
DATA_PERM="$OUT/datadir_perm"
cleanup() {
  "$BTC_CLI" -regtest -datadir="$DATA_STD"  -rpcport="$RPC_STD"  stop >/dev/null 2>&1 || true
  "$BTC_CLI" -regtest -datadir="$DATA_PERM" -rpcport="$RPC_PERM" stop >/dev/null 2>&1 || true
}
trap cleanup EXIT

fail_out() {
  {
    echo
    echo "RESULT: FAIL — $1"
    echo
    echo "run_id: $RUN_ID"
    echo "This is a recorded negative result, not a crashed run. Retained alongside it:"
    echo "  datadir_*/regtest/debug.log     both nodes' own logs"
    echo "  A*_decoded.json                 what the client made of each transaction"
    echo "  A*_testmempoolaccept_*.json     each policy regime's verdict, verbatim"
    echo "  A3_build.json                   the carriage tx, its shape and the helpers used"
    echo "  environment.txt                 tag, commit, binary and archive digests"
  } | tee -a "$OUT/verdict.txt"
  echo
  echo "evidence written (FAILING run): evidence/btc_regtest/$RUN_ID/" >&2
  exit 1
}

cli_std()  { "$BTC_CLI" -regtest -datadir="$DATA_STD"  -rpcport="$RPC_STD"  "$@"; }
cli_perm() { "$BTC_CLI" -regtest -datadir="$DATA_PERM" -rpcport="$RPC_PERM" "$@"; }
jqpy() { python3 -c "$1" "${@:2}"; }

# --- 1. PIN GATES: same release, exact commit, clean tree --------------------------------
[ -x "$BTC_BIN" ] || fail_out "BTC_BIN ($BTC_BIN) is not executable"
[ -x "$BTC_CLI" ] || fail_out "bitcoin-cli not found at $BTC_CLI (set BTC_CLI)"
[ -d "$BTC_SRC" ] || fail_out "BTC_SRC ($BTC_SRC) is not a directory"

BIN_VERSION="$("$BTC_BIN" --version | head -1)"
tag_bare="${BTC_TAG#v}"
case "$BIN_VERSION" in
  *"$tag_bare"*) : ;;
  *) fail_out "the bitcoind binary reports '$BIN_VERSION', which does not contain the pinned tag '$BTC_TAG'. Binary and source tree MUST be the same release." ;;
esac

# A git checkout is REQUIRED. An extracted archive cannot be shown to be unmodified, and
# its digest pins what was downloaded rather than what is being run against.
[ -d "$BTC_SRC/.git" ] || fail_out "BTC_SRC ($BTC_SRC) is not a git checkout. Stage 2 requires a clean git checkout at exactly '$BTC_TAG':
    git clone https://github.com/bitcoin/bitcoin \"$BTC_SRC\"
    git -C \"$BTC_SRC\" checkout $BTC_TAG
  An extracted tarball is not accepted: its digest pins the download, not the tree, and it
  cannot demonstrate the tree is unmodified now."
command -v git >/dev/null || fail_out "git not found, but BTC_SRC must be verified as a clean checkout"

SRC_COMMIT="$(git -C "$BTC_SRC" rev-parse HEAD)"
SRC_TAG_COMMIT="$(git -C "$BTC_SRC" rev-list -n 1 "$BTC_TAG" 2>/dev/null || true)"
[ -n "$SRC_TAG_COMMIT" ] || fail_out "tag '$BTC_TAG' does not exist in the source tree at $BTC_SRC (fetch tags?)"
# EXACT: `git describe` would also match commits made AFTER the tag, which is a different tree.
[ "$SRC_COMMIT" = "$SRC_TAG_COMMIT" ] || fail_out "the source tree's HEAD ($SRC_COMMIT) is not the commit tagged '$BTC_TAG' ($SRC_TAG_COMMIT). Check the tag out exactly."
if [ -n "$(git -C "$BTC_SRC" status --porcelain)" ]; then
  git -C "$BTC_SRC" status --porcelain > "$OUT/src_dirty.txt"
  fail_out "the Bitcoin Core source tree has local modifications (see src_dirty.txt). Stage 2 must run against STOCK, UNMODIFIED Core; a patched tree is Stage 3."
fi

BIN_SHA="$(sha256sum "$BTC_BIN" | cut -d' ' -f1)"
ARCHIVE_SHA="(none supplied)"
if [ -n "${BTC_SRC_ARCHIVE:-}" ]; then
  [ -f "$BTC_SRC_ARCHIVE" ] || fail_out "BTC_SRC_ARCHIVE ($BTC_SRC_ARCHIVE) does not exist"
  ARCHIVE_SHA="$(sha256sum "$BTC_SRC_ARCHIVE" | cut -d' ' -f1)"
fi

# --- 2. the LAS objects the transaction will carry --------------------------------------
# Real packed LAS wire objects at the D3 set. They are NOT required to be a matching
# key/signature pair: stock Bitcoin does not verify them, and calling them a verified pair
# would claim more than this stage establishes. What matters is that they are genuine
# objects of the true wire sizes — which is asserted, not assumed.
VEC="$OUT/las_objects"
mkdir -p "$VEC"
make -C "$REPO/ref" test/export_verify_vector > "$OUT/build.log" 2>&1 \
  || { cat "$OUT/build.log" >&2; fail_out "could not build ref/test/export_verify_vector"; }
make -C "$REPO/ref" test/export_swap_vectors >> "$OUT/build.log" 2>&1 \
  || { cat "$OUT/build.log" >&2; fail_out "could not build ref/test/export_swap_vectors"; }
(cd "$REPO/ref" && ./test/export_verify_vector "$VEC") >> "$OUT/build.log" 2>&1 \
  || { cat "$OUT/build.log" >&2; fail_out "export_verify_vector failed"; }
(cd "$REPO/ref" && ./test/export_swap_vectors setup "$VEC" --no-pi) >> "$OUT/build.log" 2>&1 \
  || { cat "$OUT/build.log" >&2; fail_out "export_swap_vectors setup failed"; }
LAS_SIG="$VEC/sig.bin"   # packed adapted signature
LAS_PK="$VEC/pk1.bin"    # packed public key
[ -s "$LAS_SIG" ] || fail_out "missing LAS signature $LAS_SIG"
[ -s "$LAS_PK" ]  || fail_out "missing LAS public key $LAS_PK"
GOT_SIG=$(stat -c%s "$LAS_SIG"); GOT_PK=$(stat -c%s "$LAS_PK")
[ "$GOT_SIG" -eq "$EXPECT_SIG_BYTES" ] || fail_out "LAS signature is $GOT_SIG B, expected $EXPECT_SIG_BYTES (D3). A different parameter set would make every size in this run describe something other than the headline configuration."
[ "$GOT_PK" -eq "$EXPECT_PK_BYTES" ]   || fail_out "LAS public key is $GOT_PK B, expected $EXPECT_PK_BYTES (D3)"

# --- 3. start both nodes, PEERED ONTO ONE CHAIN -----------------------------------------
mkdir -p "$DATA_STD" "$DATA_PERM"
"$BTC_BIN" -regtest -datadir="$DATA_STD" -rpcport="$RPC_STD" -port="$P2P_STD" \
  -bind=127.0.0.1 -fallbackfee=0.0002 -debug=mempool -daemonwait \
  > "$OUT/node_std_start.log" 2>&1 \
  || { cat "$OUT/node_std_start.log" >&2; fail_out "default-policy node failed to start"; }

# -connect points the permissive node at the first, so both follow the same chain and a
# policy answer is never confounded with a missing UTXO.
"$BTC_BIN" -regtest -datadir="$DATA_PERM" -rpcport="$RPC_PERM" -port="$P2P_PERM" \
  -bind=127.0.0.1 -fallbackfee=0.0002 -debug=mempool -daemonwait \
  -acceptnonstdtxn=1 -connect=127.0.0.1:"$P2P_STD" \
  > "$OUT/node_perm_start.log" 2>&1 \
  || { cat "$OUT/node_perm_start.log" >&2; fail_out "permissive node failed to start. Note -acceptnonstdtxn is regtest-only in stock Core."; }

cli_std  -rpcwait getblockchaininfo > /dev/null || fail_out "default-policy node did not answer"
cli_perm -rpcwait getblockchaininfo > /dev/null || fail_out "permissive node did not answer"

sync_nodes() {
  local a b
  for _ in $(seq 1 150); do
    a="$(cli_std getbestblockhash 2>/dev/null || true)"
    b="$(cli_perm getbestblockhash 2>/dev/null || true)"
    [ -n "$a" ] && [ "$a" = "$b" ] && return 0
    sleep 0.2
  done
  fail_out "the two nodes did not converge on the same best block — a policy verdict from an unsynced node would really be a missing-inputs verdict"
}

cli_std  createwallet carriage > /dev/null 2>&1 || fail_out "could not create a wallet on the default-policy node (is this a no-wallet build?)"
cli_perm createwallet carriage > /dev/null 2>&1 || fail_out "could not create a wallet on the permissive node"

# Mine one block and echo its hash. Captured so `getrawtransaction` can be given it
# explicitly: without -txindex a mined transaction is otherwise not retrievable.
mine_on_std()  { cli_std  generatetoaddress 1 "$(cli_std getnewaddress)"  | jqpy 'import json,sys;print(json.load(sys.stdin)[0])'; }
mine_on_perm() { cli_perm generatetoaddress 1 "$(cli_perm getnewaddress)" | jqpy 'import json,sys;print(json.load(sys.stdin)[0])'; }

cli_std generatetoaddress 101 "$(cli_std getnewaddress)" > /dev/null
sync_nodes

# --- 4. environment record ----------------------------------------------------------------
{
  echo "run_id=$RUN_ID"
  echo "git=$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo n/a)"
  echo "branch=$(git -C "$REPO" rev-parse --abbrev-ref HEAD 2>/dev/null || echo n/a)"
  echo "host=$(uname -s) $(uname -r) $(uname -m)"
  echo "python=$(python3 --version 2>&1)"
  echo "cc=$(${CC:-cc} --version 2>/dev/null | head -1)"
  echo "--- pinned Bitcoin Core (STOCK, UNMODIFIED) ---"
  echo "btc_tag=$BTC_TAG"
  echo "btc_bin=$BTC_BIN"
  echo "btc_bin_version=$BIN_VERSION"
  echo "btc_bin_sha256=$BIN_SHA"
  echo "btc_src=$BTC_SRC"
  echo "btc_src_head=$SRC_COMMIT"
  echo "btc_src_tag_commit=$SRC_TAG_COMMIT"
  echo "btc_src_head_equals_tag_commit=VERIFIED"
  echo "btc_src_unmodified=VERIFIED (git status --porcelain empty)"
  echo "btc_src_archive=${BTC_SRC_ARCHIVE:-(none supplied)}"
  echo "btc_src_archive_sha256=$ARCHIVE_SHA"
  echo "--- nodes (peered: one chain, two policies) ---"
  echo "node_std=regtest rpc=$RPC_STD p2p=$P2P_STD policy=DEFAULT"
  echo "node_perm=regtest rpc=$RPC_PERM p2p=$P2P_PERM policy=-acceptnonstdtxn=1 -connect=127.0.0.1:$P2P_STD"
  echo "--- LAS objects carried (NOT verified by any node in this stage) ---"
  echo "las_sig=sha256:$(sha256sum "$LAS_SIG" | cut -c1-16)... $GOT_SIG B (asserted == $EXPECT_SIG_BYTES)"
  echo "las_pk=sha256:$(sha256sum "$LAS_PK" | cut -c1-16)... $GOT_PK B (asserted == $EXPECT_PK_BYTES)"
  echo "note=this stage measures CARRIAGE ONLY: construction, policy, broadcast, mining,"
  echo "note=serialisation and size. No LAS verification takes place anywhere in it."
} > "$OUT/environment.txt"

: > "$OUT/policy_verdicts.txt"
: > "$OUT/size_model.txt"

# Offer a raw transaction to BOTH policy regimes and record each verdict verbatim. Both
# nodes hold the same chain, so the only difference between the answers is policy.
mempool_verdicts() { # $1 = label, $2 = raw hex
  cli_std  testmempoolaccept "[\"$2\"]" > "$OUT/$1_testmempoolaccept_default.json"
  cli_perm testmempoolaccept "[\"$2\"]" > "$OUT/$1_testmempoolaccept_permissive.json"
  local d p
  d="$(jqpy 'import json,sys;r=json.load(open(sys.argv[1]))[0];print("ACCEPTED" if r["allowed"] else "REJECTED: "+r.get("reject-reason","?"))' "$OUT/$1_testmempoolaccept_default.json")"
  p="$(jqpy 'import json,sys;r=json.load(open(sys.argv[1]))[0];print("ACCEPTED" if r["allowed"] else "REJECTED: "+r.get("reject-reason","?"))' "$OUT/$1_testmempoolaccept_permissive.json")"
  case "$d$p" in
    *missing-inputs*) fail_out "$1: a node answered 'missing-inputs', which means it does not hold the funding transaction — the nodes are out of sync and this verdict is not about policy" ;;
  esac
  printf '%-3s  default-policy: %-42s permissive: %s\n' "$1" "$d" "$p" | tee -a "$OUT/policy_verdicts.txt"
}

decode_and_check() { # $1 = label, $2 = raw hex
  cli_perm decoderawtransaction "$2" > "$OUT/$1_decoded.json"
  python3 "$TOOLS/btc_model_check.py" --decoded "$OUT/$1_decoded.json" --label "$1" \
    --json-out "$OUT/$1_sizecheck.json" | tee -a "$OUT/size_model.txt" \
    || fail_out "$1: this project's size model disagrees with the client (see ${1}_sizecheck.json)"
}

# Broadcast to the PERMISSIVE node and mine there, then sync: the default-policy node must
# still accept the BLOCK even where its relay policy refused the transaction. That
# distinction — policy refuses to relay, consensus accepts — is itself a result.
broadcast_and_mine() { # $1 = label, $2 = raw hex; echoes the txid
  local txid bh
  txid="$(cli_perm sendrawtransaction "$2")" \
    || fail_out "$1: the permissive node refused the transaction into its mempool"
  bh="$(mine_on_perm)"
  sync_nodes
  cli_perm getrawtransaction "$txid" true "$bh" > "$OUT/$1_mined.json"
  jqpy 'import json,sys
t=json.load(open(sys.argv[1]))
if not t.get("blockhash"): sys.exit("FATAL: %s was not mined" % sys.argv[2])
print("%s mined in %s: vsize=%s weight=%s" % (sys.argv[2], t["blockhash"][:16]+"...", t["vsize"], t["weight"]))' \
    "$OUT/$1_mined.json" "$1" || fail_out "$1 was not mined"
  cli_std getblock "$bh" > "$OUT/$1_block_seen_by_default_node.json" \
    || fail_out "$1: the default-policy node does not have the block containing it — the transaction is not consensus-valid there"
  echo "$txid"
}

# --- 5. A1 / A2: reference spends, signed BY THE NODE --------------------------------------
# The node's own signer produces the DER signature, so the classical baseline is a real
# witness rather than a chosen constant. This is what corrects the projection's 64-byte
# raw-ECDSA assumption.
build_simple_spend() { # $1 = address type; echoes signed raw hex
  local addr dest fund_txid bh vout amount raw signed
  addr="$(cli_std getnewaddress "" "$1")"
  fund_txid="$(cli_std sendtoaddress "$addr" 1.0)"
  bh="$(mine_on_std)"
  # The permissive node must hold this funding block before the spend is offered to it,
  # or its verdict would be `missing-inputs` rather than a policy answer.
  sync_nodes
  vout="$(cli_std getrawtransaction "$fund_txid" true "$bh" \
    | jqpy 'import json,sys;t=json.load(sys.stdin);a=sys.argv[1];print(next(o["n"] for o in t["vout"] if o["scriptPubKey"].get("address")==a))' "$addr")"
  amount="$(cli_std getrawtransaction "$fund_txid" true "$bh" \
    | jqpy 'import json,sys;t=json.load(sys.stdin);n=int(sys.argv[1]);print("%.8f" % next(o["value"] for o in t["vout"] if o["n"]==n))' "$vout")"
  dest="$(cli_std getnewaddress "" "$1")"
  raw="$(cli_std createrawtransaction \
    "[{\"txid\":\"$fund_txid\",\"vout\":$vout}]" \
    "[{\"$dest\":$(jqpy 'import sys;print("%.8f" % (float(sys.argv[1]) - 0.001))' "$amount")}]")"
  signed="$(cli_std signrawtransactionwithwallet "$raw" \
    | jqpy 'import json,sys
r=json.load(sys.stdin)
if not r.get("complete"): sys.exit("signing failed: %s" % r.get("errors"))
print(r["hex"])')"
  echo "$signed"
}

A1_HEX="$(build_simple_spend bech32)"  || fail_out "could not build A1 (P2WPKH spend)"
decode_and_check A1 "$A1_HEX"
mempool_verdicts A1 "$A1_HEX"
A1_TXID="$(broadcast_and_mine A1 "$A1_HEX")"

A2_HEX="$(build_simple_spend bech32m)" || fail_out "could not build A2 (P2TR key-path spend)"
decode_and_check A2 "$A2_HEX"
mempool_verdicts A2 "$A2_HEX"
A2_TXID="$(broadcast_and_mine A2 "$A2_HEX")"

# --- 6. A3: the carriage transaction --------------------------------------------------------
python3 "$TOOLS/btc_carriage.py" --core-src "$BTC_SRC" --sig "$LAS_SIG" --pk "$LAS_PK" \
  --address-only --out "$OUT/A3_address.json" > "$OUT/A3_address.log" 2>&1 \
  || { cat "$OUT/A3_address.log" >&2; fail_out "could not derive the carriage address (Core helper mismatch?)"; }
A3_ADDR="$(jqpy 'import json,sys;print(json.load(open(sys.argv[1]))["address"])' "$OUT/A3_address.json")"

A3_FUND_TXID="$(cli_std sendtoaddress "$A3_ADDR" 1.0)"
A3_FUND_BH="$(mine_on_std)"
sync_nodes
A3_VOUT="$(cli_std getrawtransaction "$A3_FUND_TXID" true "$A3_FUND_BH" \
  | jqpy 'import json,sys;t=json.load(sys.stdin);a=sys.argv[1];print(next(o["n"] for o in t["vout"] if o["scriptPubKey"].get("address")==a))' "$A3_ADDR")"
A3_VALUE_SAT="$(cli_std getrawtransaction "$A3_FUND_TXID" true "$A3_FUND_BH" \
  | jqpy 'import json,sys;t=json.load(sys.stdin);n=int(sys.argv[1]);print(int(round(next(o["value"] for o in t["vout"] if o["n"]==n)*1e8)))' "$A3_VOUT")"

python3 "$TOOLS/btc_carriage.py" --core-src "$BTC_SRC" --sig "$LAS_SIG" --pk "$LAS_PK" \
  --txid "$A3_FUND_TXID" --vout "$A3_VOUT" --value-sat "$A3_VALUE_SAT" --fee-sat "$FEE_SAT" \
  --out "$OUT/A3_build.json" > "$OUT/A3_build.log" 2>&1 \
  || { cat "$OUT/A3_build.log" >&2; fail_out "could not build the carriage transaction"; }
cat "$OUT/A3_build.log"
A3_HEX="$(jqpy 'import json,sys;print(json.load(open(sys.argv[1]))["raw_hex"])' "$OUT/A3_build.json")"

decode_and_check A3 "$A3_HEX"
mempool_verdicts A3 "$A3_HEX"
# GATE: without a mined A3 there is no measured vsize/weight and the stage yields nothing.
A3_TXID="$(broadcast_and_mine A3 "$A3_HEX")" \
  || fail_out "A3 (the carriage transaction) did NOT mine. Without it there is no measured vsize/weight — debug the client's reason rather than reporting this as a finding."

# --- 7. verdict ------------------------------------------------------------------------------
{
  echo "run_id: $RUN_ID"
  echo
  echo "SCOPE. Carriage only: construction, policy, broadcast, mining, serialisation, size."
  echo "       No LAS verification takes place in this stage; A3's spend is authorised by an"
  echo "       ordinary BIP340 Schnorr signature and the LAS bytes are dropped."
  echo
  echo "Bitcoin Core $BTC_TAG (STOCK, unmodified): binary sha256 ${BIN_SHA:0:16}..."
  echo "source HEAD $SRC_COMMIT == tag commit $SRC_TAG_COMMIT, working tree clean"
  echo
  echo "--- policy verdicts (RESULTS, recorded either way; both nodes on one chain) ---"
  cat "$OUT/policy_verdicts.txt"
  echo
  echo "--- size model vs the client (GATE) ---"
  cat "$OUT/size_model.txt"
  echo
  echo "--- mined, and the containing block accepted by the DEFAULT-policy node too ---"
  for L in A1 A2 A3; do
    jqpy 'import json,sys;t=json.load(open(sys.argv[1]));print("%s txid=%s vsize=%s weight=%s"%(sys.argv[2],t["txid"],t["vsize"],t["weight"]))' \
      "$OUT/${L}_mined.json" "$L"
  done
  echo
  echo "RESULT: a transaction carrying a real packed LAS signature and public key was"
  echo "        constructed with stock Bitcoin Core's own helpers, offered to both policy"
  echo "        regimes on one chain, broadcast and MINED, and this project's size model"
  echo "        reproduced the client's vsize and weight for every mined transaction."
} | tee -a "$OUT/verdict.txt"

ln -sfn "$RUN_ID" "$REPO/evidence/btc_regtest/latest"

echo
echo "evidence written: evidence/btc_regtest/$RUN_ID/"
echo "  verdict.txt                     — scope, policy verdicts, size gate, mined txs"
echo "  policy_verdicts.txt             — default vs permissive, per transaction"
echo "  A*_testmempoolaccept_*.json     — each regime's answer, verbatim from the node"
echo "  A*_decoded.json / A*_mined.json — the client's own reading of each transaction"
echo "  A*_sizecheck.json               — our model beside the client's vsize/weight"
echo "  A*_block_seen_by_default_node.json — consensus acceptance, independent of policy"
echo "  A3_build.json                   — the carriage tx, witness shape, helpers used"
echo "  environment.txt                 — tag, exact commit, digests, stock proof"
echo
echo "optional cross-check against the report's projection:"
echo "  python3 scripts/gen_bitcoin_tx_data.py --oracle evidence/btc_regtest/latest/"
