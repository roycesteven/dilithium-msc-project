# Reproducing the C implementation — from CRYSTALS-Dilithium to the Algorithm 1 vs Algorithm 2 benchmark

Step-by-step guide to reproduce the **C** implementation of LAS (Esgin–Ersoy–Erkin,
IACR eprint 2020/845) from the CRYSTALS-Dilithium reference code, ending at the
Stage-1 benchmark: **ordinary lattice-based signature (Algorithm 1) vs LAS adaptor
signature (Algorithm 2)** at matched parameters.

**Scope — deliberately excluded from this guide.** No application layer: no atomic
swap, no payment-channel / AMHL, no `las_presign_k` (K-hop bound), no classical
ECDSA-adaptor baseline, no EVM/gas measurement. The guide stops at the Algorithm 1
vs Algorithm 2 comparison; the application tier is only started once the signature
implementation and its benchmark are airtight.

Companion documents: `docs/FUNCTION_MAP.md` (reuse classification),
`docs/THEORY_IMPL_BRIDGE.md` (paper equation → C line), `docs/LAS.md` (full design),
`docs/paper/LAS_2020_845_NOTATION.md` (notation source of truth). The Rust
counterpart of this guide is `docs/REPRODUCE_LAS_RUST.md`.

---

## 0. Environment

Toolchain used for the reported numbers (README §1): `gcc` (Ubuntu 13.3.0),
GNU Make 4.3, Ubuntu 24.04 (WSL2), AMD Ryzen 7 7745HX. Compiled `-O3` under
`-Wall -Wextra -Wpedantic -Wmissing-prototypes -Wredundant-decls -Wshadow -Wvla
-Wpointer-arith`, zero warnings. Nothing beyond a C compiler and `make` is needed
for everything in this guide.

---

## Step 1 — Vendor the Dilithium reference tree (unchanged)

Vendor the CRYSTALS-Dilithium / ML-DSA (FIPS 204) reference C implementation as the
`ref/` tree. In this repository that is the initial commit `2374d22`
("Initial commit: add Dilithium reference code"); the upstream files are **never
edited afterwards** (verifiable: `git log --follow ref/poly.c ref/ntt.c ref/reduce.c
ref/fips202.c` each show exactly one commit).

Only the *digital-signature* part is relevant; there is no KEM code in this tree.
All LAS targets build with `-DDILITHIUM_MODE=3` (the LAS layer itself is
mode-independent — it uses only `N=256` and `Q=8380417`).

## Step 2 — Fix the reuse boundary (before writing any LAS code)

The whole methodology rests on one decision, recorded in `docs/FUNCTION_MAP.md`:
**zero upstream functions are modified.** LAS is a parallel, self-contained module
that *calls* a small set of mode-independent primitives as-is:

| Reused as-is | File | Used for |
| --- | --- | --- |
| `poly_add`, `poly_sub` | `ref/poly.c` | ring add/sub (`z = y + c·r`, `w′ = Az − ct`, `w + Y`) |
| `poly_reduce`, `poly_caddq` | `ref/poly.c` | canonicalise to centred / `[0,Q)` before hashing |
| `poly_chknorm` | `ref/poly.c` | infinity-norm rejection checks |
| `poly_ntt`, `poly_invntt_tomont` | `ref/poly.c` | NTT multiplication |
| `poly_pointwise_montgomery` | `ref/poly.c` | pointwise product in NTT domain |
| `poly_uniform` | `ref/poly.c` | expand the public matrix `A′` (SHAKE128) |
| `shake256_*` | `ref/fips202.c` | the random oracle `H`, all samplers, seed derivation |
| `randombytes` | `ref/randombytes.c` | randomised KeyGen/Sign/PreSign seeds |
| `N`, `Q` | `ref/params.h` | ring degree 256, modulus 8380417 |

Deliberately **bypassed** (this is exactly what makes it the paper's *simplified*
scheme): `sign.c` (`crypto_sign_*`), `packing.c`, `polyvec.c`, and all of
`rounding.c` (`power2round` / `decompose` / hints). The clean identity
`Az − ct = w + Y` requires hashing the **full** commitment `w`, not its high bits,
so the hint machinery must not be used.

> **Gotcha:** Dilithium's `params.h` defines `#define K 6` (mode 3). Nothing in the
> LAS layer may be named `K` — use `nhops`/`HOPS`-style names.

## Step 3 — Define the LAS parameter layer (`ref/las.h`)

LAS is self-contained: its own dimensions layered on the reused primitives.

```c
#define LAS_N      4   /* n   : rows of A, dim of t (=Y)  — overridable -DLAS_N   */
#define LAS_ELL    4   /* ℓ   : extra columns of A        — overridable -DLAS_ELL */
#define LAS_KAPPA 60   /* κ   : challenge weight ‖c‖₁     — overridable -DLAS_KAPPA */
#define LAS_M      (LAS_N + LAS_ELL)
#define LAS_GAMMA  ((int32_t)LAS_KAPPA * 256 * LAS_M)   /* γ = κ·d·(n+ℓ), d = 256 */
```

The three primitive parameters are compile-time overridable so one source builds
every parameter set (defaults = the paper set):

| Setting | (n, ℓ, κ) | γ = κ·d·(n+ℓ) | Matches Dilithium (K,L,τ) |
| --- | --- | --- | --- |
| LAS-2020/845 reference (paper set) | (4, 4, 60) | 122 880 | — |
| Simplified Dilithium-II | (4, 4, 39) | 79 872 | (4,4,39) |
| Simplified Dilithium-III | (6, 5, 49) | 137 984 | (6,5,49) |
| Simplified Dilithium-V | (8, 7, 60) | 230 400* | (8,7,60) |

*computed from the formula; these are **dimension-matched engineering settings**,
not formal NIST security levels (`docs/paper/LAS_2020_845_NOTATION.md` §8).

**Modulus note:** the construction states only the *size* of `q` matters and the
concrete value may be chosen for NTT speed (2020/845 §3.2). This build reuses
Dilithium's NTT, so `Q = 8380417 ≈ 2²³` rather than `≈ 2²⁴`; `Q > 2γ` keeps
correctness intact. Call this the *paper-derived / default LAS setting*, never
"the paper's exact parameters".

**The two rejection bounds — the single most important correctness detail:**

```c
#define LAS_BOUND_SIGN     (LAS_GAMMA - LAS_KAPPA + 1)  /* reject ‖z‖∞  > γ−κ   */
#define LAS_BOUND_PRESIGN  (LAS_GAMMA - LAS_KAPPA)      /* reject ‖ẑ‖∞ > γ−κ−1 */
```

PreSign must reject at the **tighter** `γ−κ−1`: the ternary witness has `‖y‖∞ ≤ 1`,
so the adapted response `z = ẑ + y` still satisfies `‖z‖∞ ≤ γ−κ` and passes the
ordinary Verify. If PreSign is loosened to `γ−κ`, adapted signatures can exceed the
Verify bound and *everything* breaks — this is the failure mode to watch.

Types are plain arrays of the repo's degree-256 `poly`:

```c
typedef struct { poly mat[LAS_N][LAS_ELL]; uint8_t seed[32]; } las_pp;  /* A′, NTT domain */
typedef struct { poly t[LAS_N]; } las_pk;                /* pk / statement, t = A·r */
typedef struct { poly s[LAS_M]; } las_sk;                /* sk / witness, ternary   */
typedef struct { poly c; poly z[LAS_M]; } las_sig;       /* (pre-)signature (c, z)  */
```

## Step 4 — Implement the shared helpers (`ref/las.c`)

1. **`pack_poly_canon`** — canonicalise a poly to `[0,Q)` (`poly_reduce` +
   `poly_caddq`) and serialise 4 LE bytes/coeff. *Everything hashed goes through
   this*, which is what makes the scheme (and any later port) byte-reproducible.
2. **`las_Amul`** — `w = A·v = v_top + A′·v_bot` with `A = [I_n | A′]`.
   `A′` is sampled **directly in the NTT domain** (Dilithium's trick): `las_setup`
   fills `pp->mat[i][j] = poly_uniform(seed, (i<<8)+j)` and `las_Amul` uses it
   straight in `poly_pointwise_montgomery`. Output is canonical `[0,Q)`.
3. **`polymul`** — full NTT product for `c·r` and `c·t`
   (`ntt → pointwise → invntt_tomont → reduce`).
4. **`chknorm_vec`** — `poly_chknorm` across the `LAS_M`-vector.

## Step 5 — Implement the samplers and the challenge

All samplers are **new functions** built on SHAKE (the reused Dilithium samplers
target different distributions — `η`, `γ₁` bit-packed — and are not applicable):

- **`sample_ternary`** — coefficients uniform in `{−1,0,1}`: 2-bit codes from
  SHAKE256(seed‖nonce), value 3 rejected. Used by KeyGen (`r ← S₁^{n+ℓ}`).
- **`sample_Sgamma`** — coefficients uniform in `[−γ, γ]`: 3 bytes per attempt
  masked to the smallest `2^k − 1 ≥ 2γ`, accepted iff `≤ 2γ`, shifted by `−γ`.
  Buffered per 136-byte SHAKE256 block; refill condition `pos + 3 > RATE` means
  **the last byte of every block is discarded** — a port must replicate this
  exactly or its output diverges.
- **`las_challenge`** — challenge with `‖c‖₁ = κ`, `‖c‖∞ = 1`; same construction
  as Dilithium's `poly_challenge` but with the weight fixed to `LAS_KAPPA`
  (per-set: 60/39/49/60 — never hard-code 60).
- **`hash_challenge`** — the random oracle `H`:
  `SHAKE256(pk_canon ‖ commit_canon ‖ M) → 32-byte seed → las_challenge`.

## Step 6 — Algorithm 1: the ordinary lattice-based signature

Two implementations exist, on purpose:

- **In `las.c`:** `las_keygen(_seed)`, `las_sign` (`sign_core`), `las_verify` —
  the scheme's own Sign/Verify, `c = H(pk, w, M)`:

  ```text
  Sign:   y ← S_γ^{n+ℓ}; w = A·y; c = H(pk, w, M); z = y + c·r;
          restart if ‖z‖∞ > γ−κ
  Verify: reject if ‖z‖∞ > γ−κ; w′ = A·z − c·t; accept iff c == H(pk, w′, M)
  ```

- **In `basesig.c` (`base_keygen`/`base_sign`/`base_verify`):** an **independent
  copy** of Algorithm 1 used as the fair-benchmark baseline. It shares only
  `las.h`'s parameter macros and struct layout; all logic is its own local copy,
  so the benchmark never times the adaptor module against itself and `las.c`
  stays byte-for-byte untouched. Interchangeability is the point: an Adapted LAS
  pre-signature passes this *independent* `base_verify`.

Both paths keep a rejection-attempt counter (`las_attempts` / `base_attempts`) —
measurement-only instrumentation so the restart rate is **measured directly**,
never estimated from timing ratios.

## Step 7 — Algorithm 2: the LAS adaptor signature

The whole adaptor layer is one change plus three small functions
(`presign_core`, `las_preverify`, `las_adapt`, `las_ext`):

```text
PreSign  : y ← S_γ^{n+ℓ}; w = A·y; c = H(pk, w + Y, M); ẑ = y + c·r;
           restart if ‖ẑ‖∞ > γ−κ−1                  ← statement folded into H; tighter bound
PreVerify: reject if ‖ẑ‖∞ > γ−κ−1; w′ = A·ẑ − c·t; accept iff c == H(pk, w′ + Y, M)
Adapt    : PreVerify first; then σ = (c, ẑ + y_wit)
Ext      : s = z − ẑ; return s iff A·s == Y
```

Why the adapted signature passes the **ordinary** Verify with no `+Y` anywhere:

```text
A·(ẑ + y) − c·t = (A·ẑ − c·t) + A·y = w′ + Y        (since Y = A·y)
```

The statement/witness pair is literally another key pair (`Gen` = `KeyGen`).

## Step 8 — Deterministic API + pinned KAT

To make the implementation reproducible (and portable to other languages),
add seeded/deterministic variants sharing the same cores:

- `las_keygen_seed(pk, sk, pp, seed32)`
- `las_sign_det` / `las_presign_det` — mask seed = `SHAKE256(tag ‖ sk ‖ [Y] ‖ M)`
  (tag 0 = sign, 1 = presign; `sk` absorbed 1 byte/coeff with
  `(uint8_t)(int8_t)` semantics; `Y` absorbed canonically).

`ref/test/test_kat.c` fixes every input (pp seed = 0..31; per-vector key/statement
seeds and 33-byte messages), asserts the full adaptor contract + determinism, packs
`(pk, sk, sig, pre-sig, adapted-sig)` for 4 vectors and folds all bytes into one
SHAKE256 digest, pinned in the source:

```text
641a176c 3eb21250 98fdbb7a d16bfa38 fb5744b5 2dd9696b eeb7d07b e1445a19
```

> **Important:** the KAT binary builds at the **Simplified Dilithium-III set**
> (`make test/test_kat3` passes `-DLAS_N=6 -DLAS_ELL=5 -DLAS_KAPPA=49`), *not*
> the paper set. Any cross-implementation check must use (6, 5, 49).

## Step 9 — Serialization (`ref/serialize.{c,h}`)

LSB-first bit packing with **validating** decoders (reject coeff ≥ Q, non-ternary
codes, out-of-band z):

| Object | Encoding | Paper set / Simplified Dilithium-II | Simplified Dilithium-III |
| --- | --- | --- | --- |
| public key / statement Y | 23 bits/coeff, canonical | 2944 B | 4416 B |
| secret key / witness | 2 bits/coeff, ternary | 512 B | 704 B |
| signature (c, z) | c 2-bit ternary + z offset-encoded, width `⌈log₂(2(γ−κ)+1)⌉` (18 or 19 bits) | 4672 B | 6752 B |

The z width is derived from the actual parameters at compile time, so every set
packs losslessly. Ordinary, pre- and adapted signatures all share one size —
Adapt computes `z = ẑ + r′`, it does not append any field.

## Step 10 — Correctness tests (run before any benchmark)

```sh
cd ref
make test/test_las3        && ./test/test_las3        # 1000-iter adaptor contract (also _las2/_las5)
make test/test_basesig3    && ./test/test_basesig3    # Algorithm-1 baseline + cross-module interlock
make test/test_serde3      && ./test/test_serde3      # pack/unpack round-trip + tamper rejection
make test/test_kat3        && ./test/test_kat3        # pinned-digest KAT (Simplified Dilithium-III)
```

All must pass with zero compiler warnings before timing anything.

## Step 11 — The Algorithm 1 vs Algorithm 2 benchmark

One driver, `ref/test/bench_levels.c`, produces the primary comparison: the
**separate** base-signature path (`basesig.c`) vs the LAS adaptor path (`las.c`),
at matched parameters, on the same primitives, from one shared benchmark state.
Timing is gated on the full success-path contract (ordinary signature verifies;
pre-signature pre-verifies but **fails** ordinary Verify; adapted signature
verifies; Ext recovers the witness exactly), so no failure path is ever timed.

```sh
cd ref
make test/bench_levels_paper && ./test/bench_levels_paper   # (4,4,60) paper set
make test/bench_levels2      && ./test/bench_levels2        # (4,4,39)
make test/bench_levels3      && ./test/bench_levels3        # (6,5,49)  ← headline setting
make test/bench_levels5      && ./test/bench_levels5        # (8,7,60)
```

Or the one supported evidence pipeline, which runs everything, saves raw logs and
regenerates the tables/figures into a self-contained timestamped folder:

```sh
bash scripts/run_benchmark_suite.sh     # → evidence/runs/YYYYMMDD_HHMMSS/, evidence/latest
```

**How to read the results (rules, in order of priority):**

1. **Per-operation timing is the primary result.** Lead with
   Sign vs PreSign, Verify vs PreVerify, Verify vs Adapt — never cumulative time.
2. **The headline is the adaptor overhead at Simplified Dilithium-III.** Measured
   (evidence/latest/paper_package/KEY_FINDINGS.md, run `20260627_135247`):
   PreSign **+6.7%** over Sign, PreVerify **+3.1%** over Verify, Adapt **+8.1%**
   over Verify (Adapt includes its mandatory internal PreVerify), Ext ≈ **101 µs**
   (no Algorithm-1 analogue). The two paths share algorithm, parameters and
   primitives, so the difference is purely the adaptor layer.
3. **Sizes:** the adapted signature is byte-identical in size to the ordinary one
   (6752 B at the headline setting); the only extra communicated object is the
   statement `Y` (4416 B = one public key).
4. **Rejection rate** is read directly off `base_attempts`/`las_attempts`:
   acceptance ≈ 1/e ≈ 37% per attempt (≈ 2.7 attempts/signature), matching the
   `γ = κ·d·(n+ℓ)` design target of *e* restarts (2020/845 §3.2) — expected for
   the simplified scheme, and the PreSign bound being tighter by 1 changes it
   only negligibly.
5. **The four-set sweep is a secondary fairness/scaling axis only** — supporting
   or appendix material, never the headline framing.

**Methodology parity with the Rust port:** the Rust benchmarks measure the same
operations with the same contract-gated, per-operation methodology (plus a
Criterion.rs statistical layer); the element-by-element mapping between
`bench_levels.c`, the Rust protocol driver and the Rust Criterion benchmark is
the table in `rust/fips204-las/BENCHMARKING.md` ("Methodology parity with the
C benchmark"). Cross-language claims compare overhead *ratios* only, never raw
microseconds.

## Reproduction checklist

```sh
git clone <repo> && cd dilithium-msc-project/ref
make test/test_las3     && ./test/test_las3        # contract
make test/test_basesig3 && ./test/test_basesig3    # Algorithm-1 baseline
make test/test_serde3   && ./test/test_serde3      # bytes
make test/test_kat3     && ./test/test_kat3        # pinned digest 641a176c…5a19
cd .. && bash scripts/run_benchmark_suite.sh       # Algorithm 1 vs 2 benchmark + evidence
```

Stop here. Atomic swap, PCN/AMHL, `las_presign_k`, the classical-adaptor baseline
and EVM gas are all later-stage work, out of scope for this document.
