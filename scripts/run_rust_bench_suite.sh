#!/usr/bin/env bash
#
# run_rust_bench_suite.sh -- the ONE supported Rust evidence pipeline
# (the Rust counterpart of scripts/run_benchmark_suite.sh).
#
# Runs the four steps of rust/fips204-las/BENCHMARKING.md in order, saves each
# tool's stdout to the crate's committed log files (the canonical Rust
# evidence locations, parsed by scripts/gen_report_data.py), then syncs the
# report so it reflects this run. Logs are captured, never hand-edited.
#
#   1. cargo test            gate: never benchmark unverified code (incl. the
#                            pinned cross-language KAT digest)
#   2. cargo bench           Criterion.rs statistical harness -> bench_las_criterion.log
#   3. bench_levels example  protocol driver (mirrors the C driver) -> bench_levels_rust.log
#   4. size_report example   packed component sizes -> size_report_rust.log
set -euo pipefail

ROOT="$(git rev-parse --show-toplevel)"
CRATE="$ROOT/rust/fips204-las"
cd "$CRATE"

# Gate: verify the LAS scheme before benchmarking -- cross-language KAT
# (tests/las_kat.rs), cross-module interlock (tests/las_stage1.rs), and the LAS
# library + integration tests.  The vendored upstream fips204 encoding tests
# (conversion::tests -- bit-pack/hint helpers for ml_dsa's compressed encodings)
# exercise code paths the LAS port does not use (LAS has its own serialize.rs and
# no hint/Power2Round), so they are excluded from the gate: they are not part of
# the LAS scheme under test.
echo "Gate: cargo test (LAS library + KAT + interlock) -- benchmarks refuse to run on unverified code"
cargo test --lib --tests -- --skip conversion::tests

echo "Criterion.rs micro-benchmark (-> bench_las_criterion.log)"
cargo bench --bench las_bench 2>&1 | tee bench_las_criterion.log

echo "Protocol driver (-> bench_levels_rust.log)"
cargo run --release --example bench_levels 2>&1 | tee bench_levels_rust.log

echo "Size report (-> size_report_rust.log)"
cargo run --release --example size_report 2>&1 | tee size_report_rust.log

# Report sync: regenerates report/latex/generated/*.tex from evidence/latest
# (C) + the three logs above (Rust), and refreshes the report figures.
"$ROOT/scripts/sync_report.sh"

echo
echo "Rust run complete. Committed logs updated in rust/fips204-las/."
echo "  -> report/latex synced; rebuild the PDF with: make -C report/latex"
