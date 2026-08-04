# Reproducing the Rust implementation — from fips204 to the Algorithm 1 vs Algorithm 2 benchmark

Step-by-step guide to reproduce the **Rust** port of LAS (Esgin–Ersoy–Erkin, IACR
eprint 2020/845) on top of the `fips204` crate, cross-checked **byte-for-byte**
against the C implementation via the pinned KAT digest, and ending at the Stage-1
benchmark: **ordinary lattice-based signature (Algorithm 1) vs LAS adaptor
signature (Algorithm 2)**.

**Scope — deliberately excluded.** No application layer: no atomic swap, no
payment-channel / AMHL (out of project scope), no classical ECDSA-adaptor
baseline, no EVM/gas. The guide stops at the Algorithm 1 vs Algorithm 2
comparison; nothing application-level starts until the signature implementation
and benchmark are airtight.

**Status (2026-07-03).** All steps 1–8 are implemented and verified: the KAT
digest matches the C pinned value byte-for-byte, and the Rust Algorithm 1 vs
Algorithm 2 benchmark has been run (raw output:
`rust/fips204-las/bench_levels_rust.log`).

Companion documents: `docs/A-appendix/REPRODUCE_LAS_C.md` (the C guide — read it first; the
Rust port mirrors it function-for-function), `rust/fips204-las/LAS_PROVENANCE.md`
(upstream pin + reuse classification), `docs/paper/LAS_2020_845_NOTATION.md`
(notation source of truth).

---

## 0. Environment

- Rust toolchain: any `rustc ≥ 1.70` (the crate's MSRV); the digest match was
  produced with `cargo 1.96.0` on Ubuntu 24.04 (WSL2).
- Network access once, to fetch crates.io dependencies (`sha3` etc.).

## Step 1 — Choose and vendor the base crate

Base = **`fips204`** (Eric Schorn / integritychain), a pure-Rust FIPS 204 ML-DSA
implementation. Chosen over alternatives against the supervisor's criteria
(reliable base, simple to layer on, comparable per-algorithm to the spec): plain
`const` parameters instead of heavy const-generics, an explicit
Algorithm-N → file map in `src/lib.rs`, dudect constant-time testing, NIST-KAT
validation, and the same `Q = 8380417`, `d = 256` as the C build — so the
`Q ≈ 2²³ vs 2²⁴` caveat carries over unchanged.

```sh
cd rust
git clone https://github.com/integritychain/fips204.git fips204-las
cd fips204-las && git checkout c948882 && rm -rf .git   # pin v0.4.6, commit the tree
```

The nested `.git` is removed and the tree committed into this repository —
the same pattern as the vendored pq-crystals `ref/` tree on the C side.
Provenance is recorded in `rust/fips204-las/LAS_PROVENANCE.md`.

## Step 2 — Establish the equivalence argument (before porting anything)

The port targets **byte equality** with C, so first verify how the two primitive
pipelines relate:

1. `helpers::mont_reduce` is **bit-identical** to C `montgomery_reduce`
   (same `QINV = 58728449`, same wrapping-truncation semantics).
2. `helpers::ZETA_TABLE_MONT` is **congruent mod Q** to C `zetas[]` (positive vs
   signed representatives, e.g. `5771523 ≡ −2608894 + Q`); both NTTs implement
   the standard FIPS 204 transform with the same indexing. Therefore sampling
   `A′` *directly in the NTT domain* yields the **same effective matrix**
   `A′ = InvNTT(U)` in both implementations.
3. Pipeline difference: fips204's `inv_ntt` returns the true inverse NTT already
   canonicalised to `[0,Q)`, while C's `invntt_tomont` leaves a Montgomery
   factor that its callers cancel. This does not matter because **every value
   that is hashed, packed or compared is either canonicalised to `[0,Q)` first,
   or is an exact small centred value (‖·‖∞ ≪ Q/2) whose representative in the
   reduction range is unique** — so representative differences cancel.
4. Where fips204's pipeline needs a Montgomery factor injected, use
   `helpers::to_mont` on one pointwise operand (net product then has no stray
   factor). Insert `partial_reduce32` after `ntt()` and after matrix-row
   accumulation, mirroring C's `poly_reduce(&acc)` and keeping `i32` bounds.

## Step 3 — Register the LAS modules additively

Mirror the C methodology exactly: **zero upstream functions modified.** The only
edit to an existing file is two registration lines in `src/lib.rs` (the analogue
of the C Makefile's additive targets):

```rust
pub mod las;             // port of ref/las.c (deterministic path)
pub mod las_serialize;   // port of ref/serialize.c (packing side)
```

> **Lint gotcha:** the crate root `#![deny(...)]`s many lints **by name**
> (`dead_code`, `missing_docs`, `unused_results`, …). A module-level
> `#![allow(warnings)]` does *not* override a named deny — named lints outrank
> the `warnings` group regardless of scope. The LAS modules therefore carry an
> explicit named allow-list at the top of each file.

Because the modules live inside the crate, they can use the `pub(crate)`
internals directly: `ntt::{ntt, inv_ntt}`, `helpers::{mont_reduce,
partial_reduce32, full_reduce32, center_mod, to_mont}`, `types::{R, T}`, and
SHAKE via the crate's existing `sha3` dependency (the same Keccak family as the
C `fips202.c`).

## Step 4 — Port the samplers byte-exactly

The KAT digest is decided here. Each sampler must consume the XOF stream with
**exactly** the C semantics:

| Sampler | Stream | Consumption semantics (must match C exactly) |
| --- | --- | --- |
| `poly_uniform_ntt` (expand `A′`) | SHAKE128(seed ‖ nonce_le16), nonce `(i<<8)+j` | contiguous 3-byte groups, mask `0x7FFFFF`, accept `< Q`. Contiguity holds because the C buffer sizes 840 and 168 are both divisible by 3 — no byte is ever dropped. |
| `sample_sgamma` (`S_γ`) | SHAKE256(seed64 ‖ nonce_le16) | 3 bytes/attempt, mask = smallest `2^k−1 ≥ 2γ`, accept `≤ 2γ`, shift by `−γ`. Buffered per 136-byte block with refill on `pos+3 > 136`: **the last byte of every block is discarded.** Reproduce the discard or the digest diverges. |
| `sample_ternary` (`S₁`) | SHAKE256(seed32 ‖ nonce_le16) | contiguous bytes, 2-bit codes `{0,1,2} → {−1,0,1}`, code 3 rejected; unused codes of the final byte dropped when 256 coefficients are reached. |
| `las_challenge` | SHAKE256(seed32) | first 8 bytes = sign bits (LE u64); then Fisher–Yates-style positions with rejection `b > i`; weight `κ` per parameter set. |
| `det_seed` | SHAKE256(tag ‖ sk ‖ [Y] ‖ M) → 64 B | `sk` absorbed 1 byte/coeff with `(uint8_t)(int8_t)` semantics (`−1 → 0xFF`); `Y` absorbed canonically (4 LE bytes/coeff in `[0,Q)`). |

## Step 5 — Port the scheme functions

One-to-one map, module by module (deterministic path — the KAT scope). Algorithm 1
lives in `basesig`, Algorithm 2 in `las`; the C keeps a `las_`/`base_` prefix where
Rust uses module paths:

| C | Rust |
| --- | --- |
| `setup_public_params` (`setup.c`) | `setup_public_params` (`setup.rs`) |
| `relation_gen` / `relation_gen_seed` (`relation.c`) | `gen` (RNG-injected) / `gen_seed` (`relation.rs`) |
| `base_keygen` / `base_keygen_seed` (`basesig.c`) | `keygen` (RNG-injected) / `keygen_seed` (`basesig.rs`) |
| `base_sign_internal` / `base_sign` / `base_sign_det` | `sign_internal` / `sign` (RNG-injected) / `sign_det` |
| `base_verify` | `verify` |
| `las_presign_internal` / `las_presign` / `las_presign_det` (`las.c`) | `presign_internal` / `presign` (RNG-injected) / `presign_det` (`las.rs`) |
| `las_preverify` | `preverify` |
| `las_adapt` | `adapt` (returns `Option`) |
| `las_ext` | `ext` (returns `Option`) |
| `las_attempts` counter | `LAS_ATTEMPTS: AtomicU64` (measurement only) |
| `pack_poly_canon`, `hash_challenge`, `chknorm_vec` | same names |

The randomised wrappers take `&mut impl CryptoRngCore` (Rust idiom: RNG
injected by the caller) where C calls `randombytes` internally; the sampling and
rejection logic is the shared core in both cases.

**The independent Algorithm-1 baseline** is ported as `src/basesig.rs`
(mirror of `ref/basesig.c`): `keygen(_seed)` / `sign` / `sign_det` / `verify`
(C twins `base_keygen(_seed)` / `base_sign` / `base_sign_det` / `base_verify`)
plus `BASE_ATTEMPTS`, with **local copies** of all helpers (`b_*`)
so the base path never calls `las.rs` code — the benchmark cannot time the
adaptor module against itself, and keys/signatures stay interchangeable
(an Adapted pre-signature passes this independent verifier).

Parameters are hard-coded to the **Simplified Dilithium-III set
(n=6, ℓ=5, κ=49, γ = κ·d·(n+ℓ) = 137 984)** — see the KAT gotcha in Step 7.
Bounds as in C: Sign rejects `‖z‖∞ > γ−κ`; PreSign rejects at the tighter
`‖ẑ‖∞ > γ−κ−1` (the −1 budget that keeps adapted signatures inside the ordinary
Verify bound — the failure mode to watch).

Not ported (out of project scope by design): AMHL/multi-hop locks and
everything application-level — see the scope statement at the top.

## Step 6 — Port the serialization (`src/serialize.rs`)

Identical field layout to `ref/serialize.c`: pk 23 bits/coeff canonical
(LSB-first); sk 2-bit ternary (validating); signature = a 32-byte challenge
digest `c_tilde` followed by `BitPack(z)` — the response `z` packed with the
reused upstream FIPS BitPack (`conversion::bit_pack`, Alg. 17) at width
`⌈log₂(2(γ−κ)+1)⌉` = 19 bits, and the C side is a byte-identical equivalent. The
validating decoders (`unpack_public_key` rejects coeff ≥ Q; `unpack_secret_key`
rejects the invalid 2-bit code 3) still guard the key/witness fields, while
`c_tilde` and `z` decode permissively (upstream-faithful: a tampered signature is
caught at Verify, which byte-compares the recomputed challenge digest). The
byte-interface verifier `base_verify_packed` (decode-with-validation, then
ordinary Verify) is ported too. Compile-time anchors pin the sizes:

```rust
const _: () = assert!(LAS_Z_COEFF_BITS == 19);
const _: () = assert!(PUBLIC_KEY_BYTES == 4416 && SECRET_KEY_BYTES == 704 && SIGNATURE_BYTES == 6720);
```

## Step 7 — The KAT gate (the proof of an accurate port)

`tests/las_kat.rs` is a line-for-line port of `ref/test/test_kat.c`: same fixed
seeds/messages, same adaptor-contract assertions (ordinary signature verifies;
adapted signature verifies; **pre-signature fails ordinary Verify**; PreVerify
passes; Ext recovers the witness exactly; determinism on re-run), same packing
order, same pinned digest.

> **Gotcha (cost a real debugging detour):** the C pinned digest belongs to the
> **Simplified Dilithium-III set** — `make test/test_kat3` builds with
> `-DLAS_N=6 -DLAS_ELL=5 -DLAS_KAPPA=49` — *not* the paper set (4,4,60) that
> `las.h` defaults to. A port that hard-codes the paper set produces a valid
> scheme but the wrong digest.

```sh
cd rust/fips204-las
cargo test --test las_kat -- --nocapture
```

Verified result (2026-07-03, commit `4aeb3dd`):

```text
KAT digest (Rust): bb6ad0dab998c1f90ca4d3cc0f5d3dfa723e89f79aff18fce2698a08c96e260c
=== KAT digest matches the C pinned expected value. ===
```

Also re-run the upstream suite to prove the additive layering broke nothing:

```sh
cargo test --lib     # 34/34 upstream fips204 unit tests pass
```

A digest match means keygen, both Fiat–Shamir hash paths (`H(pk, w, M)` and
`H(pk, w+Y, M)`), rejection sampling, Adapt/Ext and the bit-packing are all
byte-identical to C — the strongest cross-implementation evidence available
without a formal proof.

## Step 8 — The Algorithm 1 vs Algorithm 2 benchmark in Rust

Two complementary benchmark targets, one methodology. The full guide —
interpretation rules, baseline diffing, troubleshooting — is
**`rust/fips204-las/BENCHMARKING.md`**; this step summarises it.

### Step 8a — Criterion.rs micro-benchmark (the standard statistical tool)

`benches/las_bench.rs`, registered with an additive `[[bench]]` block in
`Cargo.toml`. Criterion.rs is the standard statistical benchmark harness of
the Rust cryptography ecosystem — **the upstream fips204 crate benchmarks
with the same harness** (the shared dev-dependency, bumped `0.4.0 → 0.8.2`
here — the one upstream manifest line changed, dev-only, documented in
`LAS_PROVENANCE.md`) — and provides warm-up, **300 samples per operation over
a 60 s measurement window** (`criterion_config()`), Tukey outlier
classification, bootstrap 95% confidence intervals, and significance-tested
baseline diffing (`--save-baseline` / `--baseline`; the committed run saved
baseline `criterion082`). Each of the seven operations is an independent
benchmark (per-operation = the primary result, Meeting-4 §14.3), on one state
gated by the same success-path contract as the driver below.

```sh
cd rust/fips204-las
cargo bench --bench las_bench 2>&1 | tee bench_las_criterion.log
```

**Measured (2026-07-03, WSL2 / AMD Ryzen 7 7745HX, `rustc 1.96.0`,
`[profile.bench]` = `opt-level 3` + `lto`; raw log:
`rust/fips204-las/bench_las_criterion.log`).** Values are the bootstrap 95%
confidence interval of the mean, `[low · point estimate · high]`:

| Operation | Time (µs) [low · point · high] |
| --- | --- |
| Algorithm 1 — KeyGen | 111.01 · 111.57 · 112.20 |
| Algorithm 1 — Sign | 1 088.5 · 1 113.5 · 1 137.3 |
| Algorithm 1 — Verify | 201.95 · 202.33 · 202.80 |
| Algorithm 2 — PreSign | 1 097.3 · 1 127.6 · 1 160.9 |
| Algorithm 2 — PreVerify | 203.19 · 204.22 · 205.30 |
| Algorithm 2 — Adapt (including its internal PreVerify) | 210.29 · 212.67 · 215.43 |
| Algorithm 2 — Extract | 69.22 · 70.25 · 71.48 |

Point-estimate overheads: PreSign vs Sign **+1.3%** (confidence intervals
overlap — statistically indistinguishable), PreVerify vs Verify **+0.9%**,
Adapt vs Verify **+5.1%**; Extract has no Algorithm-1 analogue. Note:
criterion 0.4 generates no HTML report by default (`html_reports` feature is
off and the upstream manifest is left untouched) — the terminal output, the
saved log and `target/criterion/*/new/estimates.json` are the artefacts.

### Step 8b — Protocol driver (overhead summary + rejection counters)

What Criterion by design does not report — the Algorithm-1-vs-Algorithm-2
overhead percentages and the directly measured rejection restarts — comes
from `examples/bench_levels.rs` (auto-discovered example target — no
manifest edit needed), mirroring the PRIMARY section of the C driver
`ref/test/bench_levels.c`:

- the two paths come from the two **separate** modules (`basesig.rs` vs
  `las.rs`), matched parameters, same primitives;
- ONE consistent state per run; timing gated on the full success-path contract
  (ordinary verifies; pre-signature pre-verifies but **fails** the ordinary
  verifier; adapted passes the *independent* base verifier; Ext recovers the
  witness exactly) — no failure path is ever timed;
- 5 repetitions, mean ± SD; sign-class ops run 500 iterations so the restart
  count converges to the ≈ e design target on both paths (with fewer
  iterations, the PreSign-vs-Sign ratio is dominated by restart-count luck —
  a 100-iteration pilot showed a spurious +28% for exactly this reason);
- rejection restarts read directly from `BASE_ATTEMPTS` / `LAS_ATTEMPTS`;
- a **per-attempt (rejection-normalised) diagnostic** isolates the pure cost of
  one PreSign Fiat–Shamir pass (the `w+Y` addition and the extra `Y` hashing);
- workload is fixed-seed (`ChaCha8Rng`), so repetition-to-repetition SD
  measures timing noise, and the run is reproducible.

```sh
cd rust/fips204-las
cargo run --release --example bench_levels | tee bench_levels_rust.log
```

**Measured (2026-07-03, WSL2 / AMD Ryzen 7 7745HX, upstream release profile
`opt-level="s"` + `lto`; raw log: `rust/fips204-las/bench_levels_rust.log`):**

| Operation | Mean ± SD (µs) |
| --- | --- |
| Algorithm 1 — KeyGen | 123.6 ± 7.4 |
| Algorithm 1 — Sign | 1292.0 ± 53.1 |
| Algorithm 1 — Verify | 258.2 ± 15.4 |
| Algorithm 2 — PreSign | 1342.7 ± 61.9 |
| Algorithm 2 — PreVerify | 255.0 ± 6.7 |
| Algorithm 2 — Adapt (incl. its internal PreVerify) | 259.2 ± 5.8 |
| Algorithm 2 — Extract | 88.7 ± 1.9 |

- **Adaptor overhead (per operation):** PreSign vs Sign **+3.9%**, PreVerify vs
  Verify **−1.2%**, Adapt vs Verify **+0.4%** — the verify-class differences are
  within the run-to-run noise (SD ≈ 2–6%), so the honest statement is
  *"single-digit percent at most, indistinguishable from zero for the
  verify-class operations"*.
- **Rejection sampling (measured, not inferred):** base 2.68 attempts/signature
  (acceptance 37.4%), adaptor 2.77 attempts/pre-signature (acceptance 36.2%) —
  both ≈ 1/e, matching the `γ = κ·d·(n+ℓ)` design target and the C measurement.
- **Per-attempt diagnostic:** Sign 482.6 ± 18.0 µs vs PreSign 485.5 ± 20.4 µs →
  the pure per-pass adaptor cost is **+0.6%**.
- **Consistency with C:** same qualitative conclusion as the C headline
  (PreSign +6.7%, PreVerify +3.1%, Adapt +8.1% — `docs/A-appendix/REPRODUCE_LAS_C.md`
  Step 11): the adaptor layer costs at most a few percent per operation.
  Compare **overhead ratios** across the two languages, never raw microseconds —
  the compilers and optimisation profiles differ (`gcc -O3` vs rustc
  `opt-level="s"` + `lto`).

### Step 8c — Communication cost: the component-size report

Timing is only one of the two supervisor-mandated cost axes (§13.2c); the
communication axis is covered by `examples/size_report.rs` — a deterministic
measurement program (sizes have zero variance, so Criterion is deliberately
not involved). It packs live, contract-gated objects with `serialize.rs`,
prints the component-level table, and **hard-asserts every value equal to the
C evidence row** (`evidence/latest/tables/communication_components.csv`,
level L3):

```sh
cargo run --release --example size_report | tee size_report_rust.log
```

**Measured (2026-07-05; raw log: `rust/fips204-las/size_report_rust.log`):**
pk = t 4416 B · sk = r 704 B · statement Y = t′ 4416 B · witness r′ 704 B ·
challenge c_tilde 32 B (0.48%) · response z 6688 B (**99.52% — the size driver**) ·
signature = pre-signature = adapted signature = 6720 B each.

Key statements for the report (§13.2, §14.4): z drives the size; signature,
pre-signature and adapted signature are byte-identical in size because Adapt
only adds the ternary witness (`‖y‖∞ ≤ 1`) to ẑ, which stays inside the same
19-bit packed field; the only extra communicated object is the statement `Y`
(4416 B = one public key). The four-set size sweep remains C-side evidence
(the Rust port hard-codes the Simplified Dilithium-III setting).

## Reproduction checklist

```sh
cd rust/fips204-las
cargo test --test las_kat -- --nocapture      # digest must be bb6ad0da…260c
cargo test --lib --tests                       # upstream 34/34 + interlock + serde
cargo bench --bench las_bench                  # Criterion micro-benchmark (BENCHMARKING.md)
cargo run --release --example bench_levels     # overhead summary + rejection counters
cargo run --release --example size_report      # component sizes == C evidence row
```

Stop here. Atomic swap, the classical-adaptor baseline and
EVM gas are all later-stage work, out of scope until the PQ signature
implementation and its benchmark are airtight in both languages.
