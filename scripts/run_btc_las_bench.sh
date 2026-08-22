#!/usr/bin/env bash
# What OP_CHECKLASSIGVERIFY costs a validating node.
#
#   ./scripts/run_btc_las_bench.sh
#
# WHY THIS EXISTS
# ---------------
# `run_btc_las_node.sh` proves the patched rule *works*: a spend authorised by nothing but a
# LAS signature is accepted and mined, and every mutation is refused by the patched node
# while a stock node accepts it. What it never asked is what that costs. Modifying a
# consensus rule buys something and charges something, and the charge falls on every full
# node that ever validates the chain — so "it works" is only half an answer.
#
# WHAT IS MEASURED
# ----------------
# The verification predicate the script interpreter calls, per input, against the same
# signature checks Bitcoin performs (BIP340 Schnorr for a Taproot key path, ECDSA for
# P2WPKH), in one process on one machine. Both sides start from SERIALIZED bytes and parse
# inside the timed call, so the wire codec is charged to both or neither. The reject path is
# a valid signature against a different 32-byte message digest, which fails late and
# therefore costs what a rejection really costs. Details and the standing measurement gates
# are documented in bitcoin/las_consensus/bench_las_consensus.c.
#
# ⚠ THE BINARY IS FORCE-REBUILT (make -B). A flag-only change to the Makefile does not make
# the existing binary out of date, so a plain `make` would happily re-run a stale executable
# and report it as the current source's numbers. Every figure this script emits must come
# from the source and flags present at the moment it ran; -B is what guarantees that.
#
# WHAT IS NOT MEASURED — three caveats travel with every number this produces:
#   (0) the curve baseline is a PINNED libsecp256k1-zkp, a fork of libsecp256k1 — the same
#       verification algorithms, but NOT the library the patched Bitcoin Core links, so
#       these are not Bitcoin Core's numbers;
#   (a) the SECURITY of the consensus modification is not analysed anywhere in this project;
#   (b) the two schemes are NOT at a matched security level — the node runs Simplified
#       Dilithium-III while secp256k1 is an engineering match to Dilithium-II, so the ratios
#       OVERSTATE the rule's cost relative to a level-matched pairing.
#
# Needs the same vendored library as the classical baseline:
#   git clone --depth 1 https://github.com/BlockstreamResearch/secp256k1-zkp \
#       third_party/secp256k1-zkp
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SHIM="$REPO/bitcoin/las_consensus"
SECP="$REPO/third_party/secp256k1-zkp"
RUN_ID="$(date +%Y%m%d_%H%M%S)"
OUT="$REPO/evidence/btc_las_bench/$RUN_ID"

[ -d "$SECP" ] || { echo "ERROR: vendored secp256k1-zkp not found at $SECP" >&2; exit 1; }

mkdir -p "$OUT"

# Force-rebuild both the tool and the benchmark, and the vendored secp objects with them, so
# nothing measured can have come from an earlier flag set. See the note above.
echo "==> clean + force rebuild (make -B)"
make -C "$SHIM" clean                     > "$OUT/build_clean.log" 2>&1
make -C "$SHIM" -B las_btc_tool           > "$OUT/build_tool.log"  2>&1
make -C "$SHIM" -B bench_las_consensus    > "$OUT/build_bench.log" 2>&1

# The crypto must be right before its speed means anything: a verifier that wrongly rejected
# would still produce a beautifully consistent timing.
echo "==> shim self-test (positive + negative controls)"
( cd "$SHIM" && ./las_btc_tool selftest ) > "$OUT/selftest.log" 2>&1
echo "    PASSED"

echo "==> environment record"
{
  echo "run_id=$RUN_ID"
  echo "date_utc=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "host=$(uname -srmo)"
  echo "cpu=$(grep -m1 '^model name' /proc/cpuinfo 2>/dev/null | cut -d: -f2- | sed 's/^ //')"
  echo "cc=$(${CC:-cc} --version | head -1)"
  echo "git_commit=$(cd "$REPO" && git rev-parse --short HEAD 2>/dev/null || echo unknown)"
  echo "git_dirty=$(cd "$REPO" && [ -n "$(git status --porcelain 2>/dev/null)" ] && echo yes || echo no)"
  echo "secp_pin=$(cd "$SECP" && git rev-parse --short HEAD 2>/dev/null || echo 'not a git checkout')"
  echo "build=FORCED (make clean + make -B); no stale binary can survive this"
  echo "--- what is being timed ---"
  echo "predicate=LASConsensusVerify (ref/basesig.c base_verify_packed)"
  echo "opcode=OP_CHECKLASSIGVERIFY (0xbb, a BIP342 OP_SUCCESSx slot)"
  echo "las_parameter_set=Simplified Dilithium-III (n=6, ell=5, kappa=49)"
  echo "baselines=BIP340 Schnorr (Taproot key path) and ECDSA (P2WPKH)"
  echo "baseline_library=PINNED libsecp256k1-zkp — a FORK of libsecp256k1, NOT the copy"
  echo "baseline_library=Bitcoin Core vendors; same algorithms, but not Core's numbers"
  echo "boundary=serialized bytes in, parsing inside the timed call, both sides"
  echo "message=a 32-byte digest; the paths do NOT share a sighash algorithm (BIP341 for"
  echo "message=the Taproot spend the opcode lives in, BIP143 for P2WPKH) — only the width"
  echo "reject_path=valid signature vs a different 32-byte message digest (fails late)"
  echo "build_flags=-O3 -fomit-frame-pointer on BOTH sides (secp objects and LAS verifier)"
  echo "--- caveats that travel with every figure ---"
  echo "caveat_0=curve baseline is libsecp256k1-zkp, not the patched client's libsecp256k1"
  echo "caveat_a=consensus-modification security NOT analysed; timing is not a safety argument"
  echo "caveat_b=security levels NOT matched: node runs D3, secp256k1 matches D2, so the"
  echo "caveat_b=ratios OVERSTATE the rule's cost vs a level-matched pairing"
  echo "scope=per-input cryptographic cost only; no block download, UTXO lookup, script"
  echo "scope=parsing or signature cache included (the stock client pays those too)"
} > "$OUT/environment.txt"

echo "==> benchmark"
( cd "$SHIM" && ./bench_las_consensus ) | tee "$OUT/bench_las_consensus.log"

grep '^SUMMARY ' "$OUT/bench_las_consensus.log" > "$OUT/summary.txt"

ln -sfn "$RUN_ID" "$REPO/evidence/btc_las_bench/latest"

echo
echo "==> evidence: evidence/btc_las_bench/$RUN_ID (latest -> $RUN_ID)"
echo "    bench_las_consensus.log, summary.txt, selftest.log, environment.txt"
