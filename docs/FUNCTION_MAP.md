# Function Map — Dilithium reference vs. LAS

*Supervisor deliverable (Meeting 2, B5.4): for every function in the Dilithium
reference repository, classify it as **call-as-is / modify / new**. Also serves as
the report's "reused vs modified vs added" table (report skeleton B4).*

**Headline:** **zero upstream Dilithium source functions were modified.** LAS is
implemented as a set of **new, self-contained modules** (`las`, `serialize`,
`amhl`, `chain`) that *call* a small subset of Dilithium's mode-independent
arithmetic/hash primitives as-is. The only edit to an existing file is the
`Makefile`, which gains additive build targets for the new modules. This is the
"clean diff = visible contribution" design choice (see §4).

**Provenance.** Dilithium reference C code vendored at repo commit
`2374d22` ("Initial commit: add Dilithium reference code", 2026-06-02); it is the
CRYSTALS-Dilithium / ML-DSA (FIPS 204) reference implementation, `ref/` tree.
Toolchain: `cc (Ubuntu 13.3.0)`, GNU Make 4.3, built `-O3` under
`-Wall -Wextra -Wpedantic -Wmissing-prototypes -Wredundant-decls -Wshadow -Wvla
-Wpointer-arith` (zero warnings). Build flag `-DDILITHIUM_MODE=3` (LAS is
mode-independent; see §3).

---

## 1. Dilithium primitives CALLED AS-IS by LAS

These are invoked directly by LAS code and used unchanged. (Their internal
callees — `ntt`/`invntt_tomont` in `ntt.c`, `reduce32`/`montgomery_reduce`/`caddq`
in `reduce.c`, `stream128`/Keccak in `symmetric-shake.c` — are therefore reused
transitively, also unmodified.)

| Function | File | Classification | Role in LAS |
|---|---|---|---|
| `poly_add`, `poly_sub` | `poly.c` | **call-as-is** | ring add/sub for `z=y+c·r`, `w'=Az−ct`, `w+Y` |
| `poly_reduce`, `poly_caddq` | `poly.c` | **call-as-is** | canonicalise to centred / `[0,Q)` before hashing & equality |
| `poly_chknorm` | `poly.c` | **call-as-is** | infinity-norm rejection (`‖z‖∞` bound checks) |
| `poly_ntt`, `poly_invntt_tomont` | `poly.c` | **call-as-is** | forward/inverse NTT for polynomial multiplication |
| `poly_pointwise_montgomery` | `poly.c` | **call-as-is** | pointwise product in NTT domain (`A'·v`, `c·r`, `c·t`) |
| `poly_uniform` | `poly.c` | **call-as-is** | expand the public matrix `A'` (pulls in SHAKE128) |
| `shake256_init/absorb/finalize/squeeze/squeezeblocks` | `fips202.c` | **call-as-is** | the random oracle `H`, all samplers, deterministic-seed derivation |
| `randombytes` | `randombytes.c` | **call-as-is** | seeds for `las_setup`, randomised KeyGen/Sign/PreSign |
| `params.h` constants `N`, `Q` | `params.h` | **call-as-is** | ring degree 256, modulus `8380417` (≈2²³) |

**Count:** 9 Dilithium API functions + their transitive callees, all unmodified.

---

## 2. Dilithium machinery PRESENT but DELIBERATELY NOT USED by LAS

Compiled and linked (they are part of the reference tree), but LAS does **not**
call them. Documenting this is important: it is exactly what makes LAS the
paper's *simplified* scheme, and what keeps the diff clean.

| Function(s) | File | Why LAS bypasses it |
|---|---|---|
| `crypto_sign_keypair`, `crypto_sign_signature`, `crypto_sign_verify`, `crypto_sign[_open]` | `sign.c` | LAS provides its **own** KeyGen/Sign/Verify (different relation, `[I\|A']`, ternary keys) |
| `pack_pk`/`unpack_pk`, `pack_sk`/`unpack_sk`, `pack_sig`/`unpack_sig` | `packing.c` | LAS has its own canonical encoding in `serialize.c` (different object shapes) |
| all `polyvecl_*` / `polyveck_*` | `polyvec.c` | LAS uses flat `poly[]` arrays of its own dimensions (`n=ℓ=4`), not Dilithium's `K×L` vectors |
| `power2round`, `decompose`, `make_hint`, `use_hint` | `rounding.c` | **The simplified scheme omits hints / hi-lo decomposition** — the clean identity `Az−ct = w+Y` requires hashing the *full* `w`, not its high bits |
| `poly_power2round`, `poly_decompose`, `poly_make_hint`, `poly_use_hint` | `poly.c` | same reason (hint/decomposition wrappers) |
| `poly_challenge` | `poly.c` | LAS re-implements it as `las_challenge` with a fixed weight `κ=60` (self-contained, no `params.h` `TAU` dependency) |
| `poly_uniform_eta`, `poly_uniform_gamma1`, `polyt0/t1/w1` packers | `poly.c` | LAS has its own `S₁` (ternary) and `S_γ` samplers and encoding |

**Modify count across the entire Dilithium tree: 0 functions, 0 files.**

---

## 3. NEW code added for LAS (all "new")

Everything below is new and self-contained; none of it modifies upstream files.
The module is mode-independent — it uses only `N` and `Q`, never the
mode-specific `K`, `L`, `TAU`, `GAMMA1`, … — so it builds identically under
`-DDILITHIUM_MODE=2/3/5`.

### 3.1 `las.{c,h}` — the scheme
| Function | Kind | Role |
|---|---|---|
| `las_setup` | public | expand public parameters `A=[I\|A']` from a seed |
| `las_keygen`, `las_keygen_seed` | public | `r←S₁`, `t=Ar`; seeded variant for KATs |
| `las_sign`, `las_sign_det` | public | Fiat–Shamir-with-aborts signature; deterministic variant |
| `las_verify` | public | ordinary verification (`Az−ct`, hash compare) |
| `las_presign`, `las_presign_k`, `las_presign_det` | public | adaptor PreSign (`H(pk,w+Y,M)`, bound `γ−κ−1`; K-hop bound `γ−κ−K`; deterministic) |
| `las_preverify`, `las_preverify_k` | public | adaptor PreVerify (single-hop / K-hop bound) |
| `las_adapt` | public | `σ=(c, ẑ+y)` — completes a pre-signature |
| `las_ext` | public | `y=z−ẑ`; return iff `A·y==Y` |
| `las_Amul`, `polymul`, `las_challenge`, `hash_challenge`, `sample_Sgamma`, `sample_ternary`, `pack_poly_canon`, `poly_equal`, `chknorm_vec`, `det_seed`, `sign_core`, `presign_core` | internal (static) | the `[I\|A']` product, NTT mult, `κ`-weight challenge, `H`, samplers, helpers, shared cores |

### 3.2 `serialize.{c,h}` — byte-level encoding (on-chain interface)
`las_pack_pk`/`las_unpack_pk`, `las_pack_sk`/`las_unpack_sk`,
`las_pack_sig`/`las_unpack_sig`, `las_verify_packed` (validating decoder + verify
from bytes). Sizes: pk 2944 B, sk 512 B, sig 4672 B.

### 3.3 `amhl.{c,h}` — multi-hop locks (optional/bonus tier)
`amhl_setup_gen` (cumulative statements `Y_j=A·(l₁+…+l_j)`), `amhl_norm`,
`amhl_recover_prev`.

### 3.4 `chain.{c,h}` — toy ledger for the swap/PCN demos
`chain_init`, `chain_account_add`, `chain_advance`, `chain_balance`,
`chain_fund_swap`, `chain_fund_swap_k`, `chain_claim_swap`,
`chain_extract_witness`, `chain_refund_swap`.

### 3.5 Tests / benchmarks (`ref/test/`)
`test_las`, `test_swap`, `test_pcn`, `test_amhl`, `test_serde`, `test_kat`
(functional/KAT); `bench_las`, `bench_compare`, `bench_app` (benchmarks).

---

## 4. Design note: a self-contained module, not edits to `sign.c`

Wang's framing in Meeting 1/2 was "implement LAS by *modifying* the Dilithium
reference (signature part only)." We instead built LAS as a **parallel,
self-contained module** that reuses Dilithium's low-level primitives (§1) without
editing any upstream function. Both routes deliver "LAS on Dilithium primitives";
we chose the module approach because:

1. **Clean, auditable diff.** Upstream Dilithium remains byte-for-byte intact, so
   the contribution is exactly the new files — easy for an examiner to review and
   for us to keep in sync with upstream.
2. **The relation differs from Dilithium's.** LAS uses a different matrix shape
   (`[I_n\|A']`, `n=ℓ=4`), ternary keys, and the *full*-`w` Fiat–Shamir hash; in-
   place edits to `crypto_sign_*` would have meant gutting most of `sign.c` anyway.
3. **Mode-independence.** The module avoids all mode-specific constants, so a
   single source builds and tests under modes 2/3/5.

This is a discussion point for Meeting 3; if Wang prefers an in-place edit of
`sign.c`, the primitives reused (§1) and the bypassed machinery (§2) already pin
down exactly which lines would change. (The parameter choice `q=2²³` vs the
paper's `2²⁴` is tracked separately as a documented reconciliation step.)
