#!/usr/bin/env bash
# The statement-compression experiment: can the statement Y be made smaller on
# the wire, and if not, exactly which adaptor function does each candidate break?
#
# WHY THIS EXISTS
#   The ML-DSA head-to-head found that building on unmodified ML-DSA halves the
#   signature and the public key but leaves the statement Y BYTE-IDENTICAL, so Y
#   -- not z -- is what now limits the swap payload. The claim that Y is
#   incompressible was still an ASSERTION (test_mldsa_hint.c prints it as
#   structural finding (d)). This runner turns it into a measurement, the same
#   way the hint experiment did for its own claim.
#
#   ./scripts/run_statement_compress.sh [--set N]
#
#     (default)  all three LAS sets: D2-aligned, D3-aligned (target), D5-aligned
#     --set N    only one set (2, 3 or 5)
#
# HOW TO READ THE OUTPUT
#   Diagnostic, NOT pass/fail. A "FAILS" row is a result: it localises which
#   function a candidate compression breaks. The one hard gate is the CONTROL
#   variant -- the binary exits non-zero if the uncompressed statement does not
#   hold the adaptor contract, because then nothing below it is attributable to
#   compression.
#
#   Decisive rows: "P3 base Verify ACCEPTS the adapted signature" (the chain)
#                  "P4 Ext recovers the exact witness"            (atomicity)
#
# Layout mirrors evidence/mldsa_hint/: one timestamped directory per run holding
# the tool's own output plus an environment record, with `latest` pointing at it.
# Never hand-edit a log: to change a number, change the code and re-run.
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REF="$REPO/ref"

SETS="2 3 5"
while [ $# -gt 0 ]; do
  case "$1" in
    --set) SETS="${2:?--set needs a value (2, 3 or 5)}"; shift 2 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done

RUN_ID="$(date +%Y%m%d_%H%M%S)"
OUT="$REPO/evidence/statement_compress/$RUN_ID"
mkdir -p "$OUT"

cd "$REF"
FAILED=""
for S in $SETS; do
  echo "=== LAS set $S: statement-compression candidates ==="
  make "test/test_statement_compress$S" >"$OUT/build_set$S.log" 2>&1
  if ! "./test/test_statement_compress$S" 2>&1 | tee "$OUT/statement_compress_set$S.log"; then
    FAILED="$FAILED control-set$S"
  fi
  echo
done

{
  echo "run_id=$RUN_ID"
  echo "git=$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null || echo n/a)"
  echo "branch=$(git -C "$REPO" rev-parse --abbrev-ref HEAD 2>/dev/null || echo n/a)"
  echo "host=$(uname -s) $(uname -r) $(uname -m)"
  echo "cpu=$({ grep -m1 'model name' /proc/cpuinfo 2>/dev/null || true; } | sed 's/.*: //')"
  echo "cc=$(${CC:-cc} --version 2>/dev/null | head -1 || echo n/a)"
  echo "sets=$SETS"
  echo "sources=ref/test/test_statement_compress.c"
  echo "scheme=UNMODIFIED: the candidates change only which statement is put on the wire"
  echo "note=functional admissibility only; no security analysis of any candidate"
} > "$OUT/environment.txt"

ln -sfn "$RUN_ID" "$REPO/evidence/statement_compress/latest"

echo "evidence written to evidence/statement_compress/$RUN_ID (latest -> $RUN_ID)"
if [ -n "$FAILED" ]; then
  echo "FAIL:$FAILED -- the control did not hold, so no row is attributable." >&2
  exit 1
fi
echo "control held at every set; the candidate rows are attributable to compression."
