#!/usr/bin/env bash
#
# run_benchmark_suite.sh -- the ONE supported LAS evidence pipeline.
#
# Every invocation creates a single self-contained, timestamped run folder:
#
#     evidence/runs/YYYYMMDD_HHMMSS/
#
# whose generated outputs are ORGANISED INTO SUBFOLDERS so the Meeting-4 Stage-1
# paper figures are never mixed with debug/report/application output:
#
#     metadata.txt MANIFEST.md
#     logs/                 raw benchmark/test stdout (*.log)
#     tables/               CSV evidence parsed from the logs (+ report_figure_manifest.csv)
#     paper_package/        Stage-1 MAIN package for Wang/report (Table 1 + 3 figures + KEY_FINDINGS.md)
#     appendix_package/     optional supporting figure (rejection_sampling_paper.*)
#     debug_figures/        cumulative timing, internal attribution, report/duplicate, parameter PNGs
#     application_package/   Stage-2 atomic-swap / AMHL multi-hop figures
#
# Pipeline:
#   1. build + run the C benchmarks; stdout goes into logs/.
#   2. scripts/plot_las_benchmarks.py parses the logs into a transient .stage/ and
#      produces ALL evidence CSVs + debug/report/application figures, which are then
#      distributed into tables/, debug_figures/ and application_package/.
#   3. scripts/plot_las_paper_figures.py reads tables/ and writes ONLY the Stage-1
#      paper package into paper_package/ (+ rejection_sampling_paper.* into
#      appendix_package/).
#
# Older runs are never overwritten or deleted; only generated OUTPUT locations are
# organised, never the raw evidence. evidence/latest is repointed at the newest run.
# Build the benchmarks and run them yourself when you want fresh numbers -- this
# script does the building and running; do NOT edit any log by hand.
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel)"
STAMP="$(date +%Y%m%d_%H%M%S)"
RUN_DIR="$ROOT/evidence/runs/$STAMP"

if [ -e "$RUN_DIR" ]; then
  echo "Refusing to overwrite existing run directory: $RUN_DIR" >&2
  exit 1
fi
mkdir -p "$RUN_DIR"/logs "$RUN_DIR"/tables "$RUN_DIR"/paper_package \
         "$RUN_DIR"/appendix_package "$RUN_DIR"/debug_figures "$RUN_DIR"/application_package

# --- reproducibility provenance ------------------------------------------------
COMMIT="$(git rev-parse --short HEAD 2>/dev/null || echo n/a)"
BRANCH="$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo n/a)"
# Injected into the bench_levels binaries so the git stamp appears in fair_*.log.
REPRO="-DLAS_GIT_COMMIT=\\\"$COMMIT\\\" -DLAS_GIT_BRANCH=\\\"$BRANCH\\\""

# The exact command list this run uses (recorded verbatim in metadata.txt).
read -r -d '' CMD_LIST <<'EOF' || true
make test/test_las3        && ./test/test_las3        > logs/functional_tests.log
make test/test_contract3   && ./test/test_contract3   > logs/contract.log
make test/test_serde_l3    && ./test/test_serde_l3    > logs/serialization_tests.log
make test/test_kat3        && ./test/test_kat3        > logs/kat.log
make test/test_swap3       && ./test/test_swap3       > logs/atomic_swap.log
make test/test_pcn3        && ./test/test_pcn3        > logs/pcn.log
make REPRO_FLAGS=... test/bench_levels_paper && ./test/bench_levels_paper > logs/fair_paper.log
make REPRO_FLAGS=... test/bench_levels2      && ./test/bench_levels2      > logs/fair_l2.log
make REPRO_FLAGS=... test/bench_levels3      && ./test/bench_levels3      > logs/fair_l3.log
make REPRO_FLAGS=... test/bench_levels5      && ./test/bench_levels5      > logs/fair_l5.log
make test/bench_app3       && ./test/bench_app3       > logs/application_benchmark.log
make test/bench_classical  && ./test/bench_classical  > logs/classical.log   # only if third_party/secp256k1-zkp present
# generated outputs, organised into subfolders (NOT written flat into the run root):
python3 scripts/plot_las_benchmarks.py --input-dir <stage> --output-dir <stage>
#   -> tables/*.csv (+ report_figure_manifest.csv), debug_figures/*, application_package/*
python3 scripts/plot_las_paper_figures.py --input-dir tables --output-dir paper_package --appendix-dir appendix_package
#   -> paper_package/* (Table 1 + 3 figures + KEY_FINDINGS.md + paper_figure_manifest.csv)
#   -> appendix_package/rejection_sampling_paper.*
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
# stdout is captured into the run's logs/ subfolder.
build_run() {
  target="$1"; binary="$2"; logfile="$3"; shift 3
  make "$@" "$target"
  "./$binary" > "$RUN_DIR/logs/$logfile"
  echo "  wrote logs/$logfile"
}

# Stage-2 application targets (test_contract/test_swap/test_pcn/bench_app +
# amhl/chain) are TEMPORARILY excluded when STAGE1_ONLY=1: they still use the
# pre-seven-type API and do not compile, which would abort the whole suite under
# `set -e`.  `STAGE1_ONLY=1 scripts/run_benchmark_suite.sh` regenerates the full
# Stage-1 evidence (fair benchmark + las/serde/kat correctness + classical) --
# everything the report actually consumes -- without them.  The downstream
# plot/report pipeline already treats the application log as optional, so the
# Stage-1 figures/tables/macros are produced unchanged.  Drop STAGE1_ONLY once
# the Stage-2 files are ported back to the seven-type API.
STAGE1_ONLY="${STAGE1_ONLY:-0}"
build_run_stage2() {   # like build_run, but a no-op under STAGE1_ONLY
  if [ "$STAGE1_ONLY" = 1 ]; then echo "  SKIP (STAGE1_ONLY): $1"; return 0; fi
  build_run "$@"
}

echo "Correctness / test evidence:"
build_run        test/test_las3       test/test_las3       functional_tests.log
build_run_stage2 test/test_contract3  test/test_contract3  contract.log
# Serde evidence is captured at the TARGET set (6,5,49), not the paper dims that
# test_serde3 builds: the report quotes the tamper-flip count alongside the
# target-setting signature size, so both must come from the same parameter set.
build_run        test/test_serde_l3   test/test_serde_l3   serialization_tests.log
build_run        test/test_kat3       test/test_kat3       kat.log
build_run_stage2 test/test_swap3      test/test_swap3      atomic_swap.log
build_run_stage2 test/test_pcn3       test/test_pcn3       pcn.log

echo "Fair benchmark (primary evidence):"
build_run test/bench_levels_paper test/bench_levels_paper fair_paper.log REPRO_FLAGS="$REPRO"
build_run test/bench_levels2      test/bench_levels2      fair_l2.log    REPRO_FLAGS="$REPRO"
build_run test/bench_levels3      test/bench_levels3      fair_l3.log    REPRO_FLAGS="$REPRO"
build_run test/bench_levels5      test/bench_levels5      fair_l5.log    REPRO_FLAGS="$REPRO"

echo "Application benchmark (L3-like):"
build_run_stage2 test/bench_app3      test/bench_app3      application_benchmark.log

# Classical adaptor baseline (B2.ii). Needs the one-time vendored clone
# third_party/secp256k1-zkp (git-ignored; see README.md). Skipped gracefully
# under `set -e` when that clone is absent, so the rest of the suite still runs.
echo "Classical adaptor baseline (secp256k1-zkp ecdsa_adaptor):"
if [ -d "$ROOT/third_party/secp256k1-zkp/src" ]; then
  build_run test/bench_classical test/bench_classical classical.log
else
  echo "  SKIP classical.log -- third_party/secp256k1-zkp not present"
  echo "       (clone it per README.md, then re-run to capture this baseline)"
fi

cd "$ROOT"

# --- stage 2: parse logs -> CSVs + debug/application figures, then distribute ---
# plot_las_benchmarks.py reads logs (and metadata.txt) from ONE directory and writes
# everything flat into one directory, so we give it a transient staging dir and then
# MOVE its outputs into the organised subfolders by explicit allowlist. The script
# itself is unchanged (its parsers stay compatible); only output LOCATIONS change.
STAGE="$RUN_DIR/.stage"
mkdir -p "$STAGE"
cp "$RUN_DIR"/logs/*.log "$STAGE"/
cp "$RUN_DIR/metadata.txt" "$STAGE/"
python3 scripts/plot_las_benchmarks.py --input-dir "$STAGE" --output-dir "$STAGE"

# CSV evidence (+ the full report figure manifest) -> tables/
TABLE_CSVS="parameter_sets.csv primary_timing.csv adaptor_overhead.csv \
rejection_sampling.csv communication_components.csv computation_components.csv \
las_object_catalogue.csv report_figure_manifest.csv application_atomic_swap.csv \
application_payload_breakdown.csv application_multihop_amhl.csv"

# Stage-2 application figures -> application_package/
APP_FIGS="application_atomic_swap_payload_breakdown application_multihop_payload_vs_k \
application_multihop_presign_time_vs_k application_multihop_norm_vs_k"

# cumulative timing, internal attribution, report/duplicate variants, parameter PNGs
DEBUG_FIGS="parameter_sets per_operation_timing communication_components_clean \
timing_timeline_base_vs_las protocol_step_timeline timing_overhead_clean \
computation_component_absolute communication_summary_clean adaptor_overhead_vs_level \
acceptance_vs_level component_scaling_vs_level verify_ext_attribution_vs_level \
parameter_sets_report per_operation_timing_report adaptor_overhead_vs_level_report \
communication_components_clean_report"

stage_move() {   # $1=dest-subfolder ; rest=exact filenames in the stage
  d="$1"; shift
  for f in "$@"; do [ -f "$STAGE/$f" ] && mv "$STAGE/$f" "$RUN_DIR/$d/"; done
  return 0
}
stage_move_fig() {   # $1=dest-subfolder ; rest=figure basenames (.png + .pdf)
  d="$1"; shift
  for b in "$@"; do
    for e in png pdf; do [ -f "$STAGE/$b.$e" ] && mv "$STAGE/$b.$e" "$RUN_DIR/$d/"; done
  done
  return 0
}

stage_move tables $TABLE_CSVS
stage_move_fig application_package $APP_FIGS
stage_move_fig debug_figures $DEBUG_FIGS

# Anything still in the stage (e.g. plot_las_benchmarks' own *_paper duplicates,
# its KEY_FINDINGS*.md / paper_figure_manifest.csv, the copied logs/metadata) is
# discarded: the canonical Stage-1 paper package is produced below by
# plot_las_paper_figures.py, and the raw logs already live in logs/.
rm -rf "$STAGE"

# --- stage 3: the Meeting-4 Stage-1 paper package (from the CSV tables) ---------
python3 scripts/plot_las_paper_figures.py \
  --input-dir "$RUN_DIR/tables" \
  --output-dir "$RUN_DIR/paper_package" \
  --appendix-dir "$RUN_DIR/appendix_package"

# --- paper_package/README.md (what to show Wang) -------------------------------
{
  echo "# Stage-1 LAS paper package (Meeting-4)"
  echo
  echo "Show THESE files to Wang for the Meeting-4 Stage-1 discussion. Everything else"
  echo "in this run (../debug_figures, ../application_package, ../tables, ../logs) is"
  echo "appendix / debug / evidence only."
  echo
  echo "- parameter_sets_paper.tex                  Table 1 -- parameter settings"
  echo "- per_operation_timing_paper.pdf/.png       Figure 1 -- per-operation computation timing"
  echo "- communication_components_paper.pdf/.png   Figure 2 -- communication / serialized sizes"
  echo "- KEY_FINDINGS.md                           2-3 sentence summary"
  echo "- paper_figure_manifest.csv                 provenance of each output"
  echo
  echo "Appendix (../appendix_package/):"
  echo "- adaptor_overhead_paper.pdf/.png           multi-setting overhead sweep (scaling context, NOT main)"
  echo "- rejection_sampling_paper.pdf/.png         rejection acceptance (explains timing variance)"
} > "$RUN_DIR/paper_package/README.md"

# --- MANIFEST.md (organised; states the main package explicitly) ---------------
{
  echo "# Run $STAMP -- contents (organised)"
  echo
  echo "Self-contained evidence for one LAS benchmark run, organised into subfolders so"
  echo "the Meeting-4 Stage-1 paper figures are not mixed with debug/application output."
  echo "Generated by scripts/run_benchmark_suite.sh; older runs are never overwritten."
  echo
  echo "## MAIN PACKAGE for Wang / report Stage 1  ->  paper_package/"
  echo "Show these for the Meeting-4 Stage-1 discussion; everything else is appendix/debug/evidence."
  echo "- paper_package/parameter_sets_paper.tex                  (Table 1: parameter settings)"
  echo "- paper_package/per_operation_timing_paper.pdf/.png       (Figure 1: per-operation computation)"
  echo "- paper_package/communication_components_paper.pdf/.png   (Figure 2: communication / storage sizes)"
  echo "- paper_package/KEY_FINDINGS.md, paper_package/paper_figure_manifest.csv"
  echo "Appendix (appendix_package/):"
  echo "- appendix_package/adaptor_overhead_paper.pdf/.png        (multi-setting overhead sweep; scaling context, NOT main)"
  echo "- appendix_package/rejection_sampling_paper.pdf/.png      (explains timing variance; NOT main)"
  echo
  echo "## logs/ (raw benchmark/test stdout -- do not edit by hand)"
  for f in "$RUN_DIR"/logs/*.log; do [ -e "$f" ] && echo "- logs/$(basename "$f")"; done
  echo
  echo "## tables/ (CSV evidence parsed from the logs)"
  for f in "$RUN_DIR"/tables/*.csv; do [ -e "$f" ] && echo "- tables/$(basename "$f")"; done
  echo
  echo "## paper_package/ (Meeting-4 Stage-1 MAIN package)"
  for f in "$RUN_DIR"/paper_package/*; do [ -e "$f" ] && echo "- paper_package/$(basename "$f")"; done
  echo
  echo "## appendix_package/ (optional supporting figures)"
  for f in "$RUN_DIR"/appendix_package/*; do [ -e "$f" ] && echo "- appendix_package/$(basename "$f")"; done
  echo
  echo "## debug_figures/ (cumulative timing, internal attribution, report/duplicate, parameter PNGs)"
  for f in "$RUN_DIR"/debug_figures/*.png; do [ -e "$f" ] && echo "- debug_figures/$(basename "$f") (+ .pdf)"; done
  echo
  echo "## application_package/ (Stage-2: atomic swap, AMHL/multi-hop -- NOT Stage-1)"
  for f in "$RUN_DIR"/application_package/*.png; do [ -e "$f" ] && echo "- application_package/$(basename "$f") (+ .pdf)"; done
  echo
  echo "Provenance: metadata.txt (CPU/OS/compiler/git). Full per-figure classification:"
  echo "tables/report_figure_manifest.csv and paper_package/paper_figure_manifest.csv."
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

# --- stage 4: sync the report to this run --------------------------------------
# Figures + generated macros/tables in report/latex/ now reflect THIS run
# (evidence is already saved above, so a sync failure loses nothing).
"$ROOT/scripts/sync_report.sh"

echo
echo "Run complete. Organised evidence in: $RUN_DIR"
echo "  -> show paper_package/ to Wang (Stage-1); see MANIFEST.md for the full layout."
echo "  -> report/latex synced to this run; rebuild the PDF with: make -C report/latex"
