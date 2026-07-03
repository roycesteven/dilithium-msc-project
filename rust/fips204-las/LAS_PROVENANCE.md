# Provenance — fips204-las (vendored fork + LAS additive modules)

## Upstream

- **Crate:** `fips204` (Eric Schorn / integritychain) — pure-Rust FIPS 204
  ML-DSA, <https://github.com/integritychain/fips204>
- **Vendored at commit:** `c948882` ("Update package.json", 2025-09-01),
  version `0.4.6`. The nested `.git` was removed; the tree is committed into
  this repository (same pattern as the pq-crystals Dilithium `ref/` tree,
  vendored at repo commit `2374d22`).
- **License:** MIT OR Apache-2.0 (unchanged).

## What was added (and the ONLY upstream edit)

Mirrors the C methodology exactly (see `docs/FUNCTION_MAP.md`): **zero upstream
functions modified**; LAS is layered as additive modules that *call* the
crate's primitives as-is.

| Change | Kind |
| --- | --- |
| `src/las.rs` | **new** — LAS scheme (port of `ref/las.c`: deterministic path + randomised `las_keygen`/`las_sign`/`las_presign` wrappers + `LAS_ATTEMPTS` counter) |
| `src/las_basesig.rs` | **new** — independent Algorithm-1 base signature (port of `ref/basesig.c`; local `b_*` helper copies + `BASE_ATTEMPTS`) |
| `src/las_serialize.rs` | **new** — bit-packing + validating decoders + `las_verify_packed` (port of `ref/serialize.c`) |
| `tests/las_kat.rs` | **new** — KAT (port of `ref/test/test_kat.c`, same pinned digest) |
| `tests/las_stage1.rs` | **new** — cross-module interlock + serde round-trip/tamper tests |
| `examples/bench_levels.rs` | **new** — Algorithm 1 vs Algorithm 2 benchmark (mirror of `ref/test/bench_levels.c` primary section) |
| `bench_levels_rust.log` | **generated** — raw output of the benchmark run (2026-07-03) |
| `src/lib.rs` | **additive edit only** — three `pub mod` registration lines (analogue of the C Makefile's additive targets) |

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
cargo run --release --example bench_levels     # Algorithm 1 vs Algorithm 2 timings
```

C side of the KAT: `make test/test_kat3 && ./test/test_kat3`. Benchmark results
and interpretation: `docs/REPRODUCE_LAS_RUST.md` Step 8.
