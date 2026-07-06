# Provenance — fips204-las (vendored fork + LAS additive modules)

## Upstream

- **Crate:** `fips204` (Eric Schorn / integritychain) — pure-Rust FIPS 204
  ML-DSA, <https://github.com/integritychain/fips204>
- **Vendored at commit:** `c948882` ("Update package.json", 2025-09-01),
  version `0.4.6`. The nested `.git` was removed; the tree is committed into
  this repository (same pattern as the pq-crystals Dilithium `ref/` tree,
  vendored at repo commit `2374d22`).
- **License:** MIT OR Apache-2.0 (unchanged).

## What was added (and the only upstream edits)

Mirrors the C methodology exactly (see `docs/02-methodology/FUNCTION_MAP.md`): **zero upstream
functions modified**; LAS is layered as additive modules that *call* the
crate's primitives as-is. The only upstream lines touched are the two
registration edits and one dev-dependency version bump listed at the bottom of
the table.

| Change | Kind |
| --- | --- |
| `src/las.rs` | **new** — LAS scheme (port of `ref/las.c`: deterministic path + randomised `las_keygen`/`las_sign`/`las_presign` wrappers + `LAS_ATTEMPTS` counter + `las_expected_attempts`, the exact restart-rate expectation used by the benchmark rejection gates — instrumentation only, not part of the C port) |
| `src/las_basesig.rs` | **new** — independent Algorithm-1 base signature (port of `ref/basesig.c`; local `b_*` helper copies + `BASE_ATTEMPTS`) |
| `src/las_serialize.rs` | **new** — bit-packing + validating decoders + `las_verify_packed` (port of `ref/serialize.c`) |
| `tests/las_kat.rs` | **new** — KAT (port of `ref/test/test_kat.c`, same pinned digest) |
| `tests/las_stage1.rs` | **new** — cross-module interlock + serde round-trip/tamper tests |
| `benches/las_bench.rs` | **new** — Criterion.rs statistical micro-benchmark (same harness as upstream's own `benches/benchmark.rs`; custom config 300 samples / 60 s per operation; per-run rejection gate hard-asserting measured attempts/call against the exact theory, see BENCHMARKING.md "Run validity") |
| `examples/bench_levels.rs` | **new** — protocol driver: overhead summary + rejection counters (mirror of `ref/test/bench_levels.c` primary section) + the same rejection gate (coarse tolerance) |
| `examples/size_report.rs` | **new** — communication-cost evidence: measured component-level packed sizes, hard-asserted equal to the C evidence row (`communication_components.csv`, level L3) |
| `BENCHMARKING.md` | **new** — how to run/read/reproduce all benchmark artefacts; methodology + parity table with the C driver |
| `bench_las_criterion.log` | **generated** — raw output of the Criterion 0.8.2 run (2026-07-05; baseline `criterion082` saved) |
| `bench_levels_rust.log` | **generated** — raw output of the protocol-driver run (2026-07-03) |
| `size_report_rust.log` | **generated** — raw output of the component-size report (2026-07-05) |
| `src/lib.rs` | **additive edit only** — three `pub mod` registration lines (analogue of the C Makefile's additive targets) |
| `Cargo.toml` | **edit** — additive `[[bench]] name = "las_bench"` registration block, plus ONE dev-dependency version bump `criterion 0.4.0 → 0.8.2` (dev-only: building the benches needs a newer toolchain than upstream's MSRV 1.70; the library and its dependencies are untouched) |

Primitives reused as-is from upstream: `ntt::ntt`, `ntt::inv_ntt`,
`helpers::mont_reduce`, `helpers::partial_reduce32`, `helpers::full_reduce32`,
`helpers::center_mod`, `helpers::to_mont`, types `R`/`T`; SHAKE128/256 via the
crate's existing `sha3` dependency.

## Parameter set

Simplified Dilithium-III engineering set `n=6, ell=5, kappa=49` — the set the
C KAT binary pins (`make test/test_kat3`, `-DLAS_N=6 -DLAS_ELL=5
-DLAS_KAPPA=49`). Packed sizes: pk 4416 B, sk 704 B, sig 6752 B.

## Reproduce the cross-check and the Stage-1 benchmark

```sh
cd rust/fips204-las
cargo test --test las_kat -- --nocapture      # digest 641a176c…5a19 == C pinned value
cargo test --lib --tests                       # upstream 34/34 + interlock + serde
cargo bench --bench las_bench                  # Criterion micro-benchmark (BENCHMARKING.md)
cargo run --release --example bench_levels     # overhead summary + rejection counters
cargo run --release --example size_report      # component sizes == C evidence row
```

C side of the KAT: `make test/test_kat3 && ./test/test_kat3`. Benchmark results
and interpretation: `BENCHMARKING.md` (this directory) and
`docs/A-appendix/REPRODUCE_LAS_RUST.md` Step 8.
