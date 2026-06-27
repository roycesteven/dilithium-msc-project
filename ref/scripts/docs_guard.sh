#!/usr/bin/env bash
set -euo pipefail

BASE="${1:-origin/main}"

echo "== Documentation guard =="
echo "Base: $BASE"
echo

changed="$(git diff --name-only "$BASE"...HEAD || true)"

if [ -z "$changed" ]; then
  echo "No changed files against $BASE."
  exit 0
fi

echo "Changed files:"
echo "$changed"
echo

need_status=0
need_readme=0
need_history=0
need_progress=0
need_evidence=0
need_report=0

# New/changed tests or Makefile usually affect reproduce commands/status.
if echo "$changed" | grep -Eq '(^ref/test/|^ref/Makefile|^Makefile|CMakeLists|\.github/workflows/)'; then
  need_status=1
  need_readme=1
fi

# Benchmark source changes need status/report/evidence attention.
if echo "$changed" | grep -Eq 'bench|benchmark'; then
  need_status=1
  need_evidence=1
  need_report=1
fi

# New major implementation modules need project history.
if echo "$changed" | grep -Eq '(^ref/.*\.(c|h)$|^evm/src/.*\.sol$)'; then
  need_history=1
  need_progress=1
fi

# Build/run docs or commands changed.
if echo "$changed" | grep -Eq '(^README\.md$|^ref/Makefile|^Makefile|^evm/README\.md|foundry\.toml|package\.json)'; then
  need_readme=1
fi

# Raw evidence changed.
if echo "$changed" | grep -Eq '^evidence/'; then
  need_status=1
  need_report=1
fi

echo "Required documentation review:"
[ "$need_progress" -eq 1 ] && echo "- PROGRESS.md: add/update checkpoint summary."
[ "$need_status" -eq 1 ] && echo "- docs/STATUS.md: update deliverable/test/benchmark status if results changed."
[ "$need_readme" -eq 1 ] && echo "- README.md: update build/run/reproduce commands if commands changed."
[ "$need_history" -eq 1 ] && echo "- docs/PROJECT_HISTORY_EXPLAINED.md: update only if a major file/module/function was added."
[ "$need_evidence" -eq 1 ] && echo "- evidence/: save raw benchmark/test output; do not overwrite old logs."
[ "$need_report" -eq 1 ] && echo "- report/: update benchmark/result tables when values are final."

if [ "$need_progress$need_status$need_readme$need_history$need_evidence$need_report" = "000000" ]; then
  echo "- No special documentation update detected."
fi

echo
echo "Reminder:"
echo "- GitHub PR is the canonical code diff."
echo "- Do not merge to main before supervisor verification."
