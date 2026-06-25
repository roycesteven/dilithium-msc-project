#!/usr/bin/env bash
#
# run_benchmark_suite.sh -- the ONE supported LAS evidence pipeline.
#
# Every invocation creates a single self-contained, timestamped run folder:
#
#     evidence/runs/YYYYMMDD_HHMMSS/
#
# and writes EVERYTHING for that run directly inside it (no logs/, figures/ or
# tables/ subfolders, and nothing under figures/benchmark, tables/benchmark or
# evidence/final-runs):
#
#     metadata.txt MANIFEST.md
#     functional_tests.log contract.log serialization_tests.log kat.log
#     atomic_swap.log pcn.log
#     fair_paper.log fair_l2.log fair_l3.log fair_l5.log
#     application_benchmark.log
#     classical.log            (ECDSA-adaptor baseline; only if third_party present)
#     *.csv  *.png  *.pdf      (written by scripts/plot_las_benchmarks.py)
#
# Older runs are never overwritten or deleted; evidence/latest is repointed at the
# newest run.  Build the benchmarks and run them yourself when you want fresh
# numbers -- this script does the building and running; do NOT edit any log by hand.
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel)"
STAMP="$(date +%Y%m%d_%H%M%S)"
RUN_DIR="$ROOT/evidence/runs/$STAMP"

if [ -e "$RUN_DIR" ]; then
  echo "Refusing to overwrite existing run directory: $RUN_DIR" >&2
  exit 1
fi
mkdir -p "$RUN_DIR"

# --- reproducibility provenance ------------------------------------------------
COMMIT="$(git rev-parse --short HEAD 2>/dev/null || echo n/a)"
BRANCH="$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo n/a)"
# Injected into the bench_levels binaries so the git stamp appears in fair_*.log.
REPRO="-DLAS_GIT_COMMIT=\\\"$COMMIT\\\" -DLAS_GIT_BRANCH=\\\"$BRANCH\\\""

# The exact command list this run uses (recorded verbatim in metadata.txt).
read -r -d '' CMD_LIST <<'EOF' || true
make test/test_las3        && ./test/test_las3        > functional_tests.log
make test/test_contract3   && ./test/test_contract3   > contract.log
make test/test_serde3      && ./test/test_serde3      > serialization_tests.log
make test/test_kat3        && ./test/test_kat3        > kat.log
make test/test_swap3       && ./test/test_swap3       > atomic_swap.log
make test/test_pcn3        && ./test/test_pcn3        > pcn.log
make REPRO_FLAGS=... test/bench_levels_paper && ./test/bench_levels_paper > fair_paper.log
make REPRO_FLAGS=... test/bench_levels2      && ./test/bench_levels2      > fair_l2.log
make REPRO_FLAGS=... test/bench_levels3      && ./test/bench_levels3      > fair_l3.log
make REPRO_FLAGS=... test/bench_levels5      && ./test/bench_levels5      > fair_l5.log
make test/bench_app3       && ./test/bench_app3       > application_benchmark.log
make test/bench_classical  && ./test/bench_classical  > classical.log   # only if third_party/secp256k1-zkp present
python3 scripts/plot_las_benchmarks.py --input-dir <run> --output-dir <run>
EOF

{
  echo "# LAS benchmark-suite run metadata"
  echo
  echo "run_id     : $STAMP"
  echo "timestamp  : $(date)"
  echo "git_commit : $(git rev-parse HEAD 2>/dev/null || echo n/a)"
  echo "git_short  : $COMMIT"
  echo "git_branch : $BRANCH"
  echo "git_status :"
  git status -sb 2>/dev/null || true
  echo
  echo "compiler   : $({ ${CC:-cc} --version 2>/dev/null || true; } | head -n 1)"
  echo "os_uname   : $(uname -a 2>/dev/null || true)"
  if grep -qiE 'microsoft|wsl' /proc/version 2>/dev/null; then
    echo "wsl        : yes  ($(cat /proc/version 2>/dev/null))"
  else
    echo "wsl        : no"
  fi
  echo "cpu        : $({ grep -m1 'model name' /proc/cpuinfo 2>/dev/null || true; } | sed 's/.*: //')"
  echo "cpu_cores  : $(nproc 2>/dev/null || echo '?')"
  echo
  echo "command_list :"
  echo "$CMD_LIST" | sed 's/^/  /'
} > "$RUN_DIR/metadata.txt"

cd "$ROOT/ref"

# build_run <make-target> <binary> <logfile> [extra make vars...]
build_run() {
  target="$1"; binary="$2"; logfile="$3"; shift 3
  make "$@" "$target"
  "./$binary" > "$RUN_DIR/$logfile"
  echo "  wrote $logfile"
}

echo "Correctness / test evidence:"
build_run test/test_las3       test/test_las3       functional_tests.log
build_run test/test_contract3  test/test_contract3  contract.log
build_run test/test_serde3     test/test_serde3     serialization_tests.log
build_run test/test_kat3       test/test_kat3       kat.log
build_run test/test_swap3      test/test_swap3      atomic_swap.log
build_run test/test_pcn3       test/test_pcn3       pcn.log

echo "Fair benchmark (primary evidence):"
build_run test/bench_levels_paper test/bench_levels_paper fair_paper.log REPRO_FLAGS="$REPRO"
build_run test/bench_levels2      test/bench_levels2      fair_l2.log    REPRO_FLAGS="$REPRO"
build_run test/bench_levels3      test/bench_levels3      fair_l3.log    REPRO_FLAGS="$REPRO"
build_run test/bench_levels5      test/bench_levels5      fair_l5.log    REPRO_FLAGS="$REPRO"

echo "Application benchmark (L3-like):"
build_run test/bench_app3      test/bench_app3      application_benchmark.log

# Classical adaptor baseline (B2.ii). Needs the one-time vendored clone
# third_party/secp256k1-zkp (git-ignored; see README_LAS.md). Skipped gracefully
# under `set -e` when that clone is absent, so the rest of the suite still runs.
echo "Classical adaptor baseline (secp256k1-zkp ecdsa_adaptor):"
if [ -d "$ROOT/third_party/secp256k1-zkp/src" ]; then
  build_run test/bench_classical test/bench_classical classical.log
else
  echo "  SKIP classical.log -- third_party/secp256k1-zkp not present"
  echo "       (clone it per README_LAS.md, then re-run to capture this baseline)"
fi

# --- CSVs + figures, written directly into the SAME run folder -----------------
cd "$ROOT"
python3 scripts/plot_las_benchmarks.py --input-dir "$RUN_DIR" --output-dir "$RUN_DIR"

# --- MANIFEST.md (lists what the run folder actually contains) -----------------
{
  echo "# Run $STAMP -- contents"
  echo
  echo "Self-contained evidence for one LAS benchmark run. Generated by"
  echo "scripts/run_benchmark_suite.sh; older runs are never overwritten."
  echo
  echo "## Logs (raw benchmark/test stdout -- do not edit by hand)"
  for f in functional_tests.log contract.log serialization_tests.log kat.log \
           atomic_swap.log pcn.log fair_paper.log fair_l2.log fair_l3.log \
           fair_l5.log application_benchmark.log classical.log; do
    [ -f "$RUN_DIR/$f" ] && echo "- $f"
  done
  echo
  echo "## CSV tables"
  for f in "$RUN_DIR"/*.csv; do [ -e "$f" ] && echo "- $(basename "$f")"; done
  echo
  echo "## Figures (PNG + PDF)"
  for f in "$RUN_DIR"/*.png; do [ -e "$f" ] && echo "- $(basename "$f") (+ .pdf)"; done
  echo
  echo "See report_figure_manifest.csv for which figures are main-report vs appendix,"
  echo "the claim each supports, and any caution notes."
} > "$RUN_DIR/MANIFEST.md"

# --- repoint evidence/latest at the newest run (keep older runs) ---------------
# Always remove any existing pointer FIRST -- whether it is a stale symlink or a
# real directory left by a previous copy-fallback -- otherwise `ln` would create the
# link *inside* an existing directory and evidence/latest would point at an old run.
rm -rf "$ROOT/evidence/latest"
if ln -s "runs/$STAMP" "$ROOT/evidence/latest" 2>/dev/null; then
  echo "evidence/latest -> runs/$STAMP (symlink)"
else
  mkdir -p "$ROOT/evidence/latest"
  cp -r "$RUN_DIR"/. "$ROOT/evidence/latest/"
  echo "evidence/latest (copy of runs/$STAMP; symlinks unavailable)"
fi

echo
echo "Run complete. Everything is in: $RUN_DIR"
