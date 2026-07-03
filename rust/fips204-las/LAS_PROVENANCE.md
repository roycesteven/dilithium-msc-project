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
|---|---|
| `src/las.rs` | **new** — LAS deterministic path (port of `ref/las.c`) |
| `src/las_serialize.rs` | **new** — bit-packing (port of `ref/serialize.c`, pack side) |
| `tests/las_kat.rs` | **new** — KAT (port of `ref/test/test_kat.c`, same pinned digest) |
| `src/lib.rs` | **additive edit only** — two `pub mod` registration lines (analogue of the C Makefile's additive targets) |

Primitives reused as-is from upstream: `ntt::ntt`, `ntt::inv_ntt`,
`helpers::mont_reduce`, `helpers::partial_reduce32`, `helpers::full_reduce32`,
`helpers::center_mod`, `helpers::to_mont`, types `R`/`T`; SHAKE128/256 via the
crate's existing `sha3` dependency.

## Parameter set

Simplified Dilithium-III engineering set `n=6, ell=5, kappa=49` — the set the
C KAT binary pins (`make test/test_kat3`, `-DLAS_N=6 -DLAS_ELL=5
-DLAS_KAPPA=49`). Packed sizes: pk 4416 B, sk 704 B, sig 6752 B.

## Reproduce the cross-check

```sh
cd rust/fips204-las
cargo test --test las_kat -- --nocapture
```

Expected: digest `641a176c…5a19` — byte-identical to the C
`ref/test/test_kat.c` pinned value (C side: `make test/test_kat3 && ./test/test_kat3`).
