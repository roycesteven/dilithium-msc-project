# Function Map — Dilithium reference vs. LAS

*Supervisor deliverable (Meeting 2, B5.4): for every function in the Dilithium
reference repository, classify it as **call-as-is / modify / new**. Also serves as
the report's "reused vs modified vs added" table (report skeleton B4).*

**Headline:** **zero upstream Dilithium source functions were modified.** LAS is
implemented as a set of **new, self-contained modules** (`las`, `basesig`,
`setup`, `relation`, `serialize`) that *call* a small subset of Dilithium's mode-independent
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
| `randombytes` | `randombytes.c` | **call-as-is** | seeds for `setup_public_params`, randomised KeyGen/Sign/PreSign |
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
| `setup_public_params` | public | expand public parameters `A=[I\|A']` from a seed |
| `base_keygen`, `base_keygen_seed` | public | `r←S₁`, `t=Ar`; seeded variant for KATs |
| `base_sign`, `base_sign_det` | public | Fiat–Shamir-with-aborts signature; deterministic variant |
| `base_verify` | public | ordinary verification (`Az−ct`, hash compare) |
| `base_sign_internal`, `las_verify_internal`, `las_presign_internal`, `las_preverify_internal` | public (internal API) | seed-/bound-parameterised bodies shared by the random, deterministic and K-hop entry points — declared in `las.h` exactly as `sign.h` declares `crypto_sign_signature_internal`/`crypto_sign_verify_internal`; provenance chain `crypto_sign_*_internal → base_sign_*_internal → las_*_internal` |
| `las_presign`, `las_presign_k`, `las_presign_det` | public | adaptor PreSign (`H(pk,w+Y,M)`, bound `γ−κ−1`; K-hop bound `γ−κ−K`; deterministic) |
| `las_preverify`, `las_preverify_k` | public | adaptor PreVerify (single-hop / K-hop bound) |
| `las_adapt` | public | `σ=(c, ẑ+y)` — completes a pre-signature |
| `las_ext` | public | `y=z−ẑ`; return iff `A·y==Y` |
| `las_expected_attempts` | public (instrumentation only) | exact expected attempts/call of the rejection loop at a given bound (`((2·bound−1)/(2γ+1))^{−(n+ℓ)d}`, verified against 2020/845 Table 1 / Fact 1 / §3.2); consumed by the benchmarks' run-validity rejection gate, never by the scheme — mirrored in the Rust port |
| `las_Amul`, `polymul_prehat`, `las_challenge`, `hash_challenge`, `sample_Sgamma`, `sample_ternary`, `pack_poly_canon`, `poly_equal`, `chknorm_vec`, `det_seed` | internal (static, defined at the bottom of `las.c` in the same order as `basesig.c`'s local copies) | the `[I\|A']` product, NTT-domain pointwise mult (forward NTTs hoisted per-call/per-attempt as in `ref/sign.c`), `κ`-weight challenge, `H`, samplers, helpers |

**`basesig.{c,h}` — the separate simplified-base signature (new; the fair-benchmark baseline).**
A standalone simplified Dilithium-style signature kept deliberately **out of `las.{c,h}`**
so the LAS protocol is never conflated or modified. Its API mirrors `sign.h` one-to-one
(same slots, same order, same `int` returns) by the uniform prefix swap
`crypto_sign* → base_*`: `base_keygen` (+ the seeded KAT slot `base_keygen_seed`),
`base_sign_internal`, `base_sign` (`c = H(pk, w, M)`, no statement `Y`), `base_sign_det`,
`base_verify_internal`, `base_verify` (`c == H(pk, w', M)`); the two zero-caller
sm-wrappers (upstream `crypto_sign`/`crypto_sign_open`) are **deleted**, and the packed
tier adds `base_keygen_packed`/`base_sign_packed`/`base_verify_packed`. It depends on
`setup.h` (the shared parameter macros + `public_params`) and `las_types.h` (the object
types `public_key`/`secret_key`/`signature`) — **not** on `las.h` — so both schemes use
the same parameter set (a dimension-level match — `n,ℓ,κ` —
not a formal bit-security claim; proofs are out of scope) and their keys/signatures are
interchangeable — verified by `bench_levels`, where a LAS-`Adapt`-ed signature passes the
independent `base_verify`. Its static helpers (`b_Amul`, `b_polymul_prehat`, `b_challenge`,
`b_hash_challenge`, `b_sample_Sgamma`, `b_sample_ternary`, `b_pack_poly_canon`,
`b_poly_equal`, `b_chknorm_vec`) are behaviour-identical local copies of LAS's so the
challenge hash matches bit-for-bit (including the identical NTT hoisting). **No upstream files modified.**

### 3.2 `serialize.{c,h}` — byte-level encoding (on-chain interface)
`pack_public_key`/`unpack_public_key`, `pack_secret_key`/`unpack_secret_key`,
`pack_signature`/`unpack_signature`, `base_verify_packed` (validating decoder + verify
from bytes). The `z` field width is parameter-derived (`LAS_Z_COEFF_BITS` = 18 bits for
the paper/D2 sets, 19 for D3/D5), so every parameter set packs losslessly. Sizes
(paper/D2): pk 2944 B, sk 512 B, sig 4640 B.

### 3.5 Tests / benchmarks (`ref/test/`)
Functional / KAT: `test_las` (1000-iter 8-point contract, modes 2/3/5),
`test_basesig` (1000-iter **CHECK**-gated base-signature correctness: honest verify +
tamper/wrong-key rejection + cross-module equivalence with `las.c` + cross-path interlock
+ four negative tests — wrong statement, wrong witness, tampered pre-signature, tampered
adapted signature; paper/2/3/5),
`test_swap` (paper §4.1 Fig. 1 verbatim incl. π; opt-in, needs LaZer), `test_zkp`
(π completeness / tamper / wrong-statement / non-ternary refusal; opt-in),
`test_serde` (round-trip /
verify-from-bytes / tamper, swept across parameter sets — `test_serde3` paper dims plus
`test_serde_l2/l3/l5`), `test_kat` (pinned SHAKE256 digest).
Benchmarks: `bench_levels` (**primary fair** benchmark — base path `basesig.c` vs LAS
adaptor path `las.c` at matched parameters, ≥5 runs mean±SD, component sizes),
`bench_las` (per-op + direct rejection rate), `bench_compare` (LAS vs optimised
Dilithium-3), `bench_app` (application payload, simulated), `bench_classical`
(LAS vs classical ECDSA-adaptor from libsecp256k1-zkp).
Helper: `export_packed` (writes one real packed adapted signature for the EVM test).

### 3.6 `evm/` — on-chain gas benchmark (Solidity, new; no C edits)
`AdaptorSwap.sol` — a signature-agnostic HTLC escrow run on Foundry's local EVM,
settling an atomic swap with either a classical ECDSA adapted signature
(`claimClassical`, native `ecrecover`) or a real packed LAS adapted signature
(`claimLAS`, the on-chain calldata+keccak floor). It consumes only the *bytes*
produced by `serialize.c` (via `export_packed`), so it touches no C source.
Measured gas: classical claim 75,751 vs LAS `claimLAS` floor (settle only, no lattice
verify) 289,930. The **complete** native verifier now also exists — `evm/src/LASVerifier.sol`
(Solidity, reusing vendored ZKNox primitives; not C, so outside this map) wired into
`AdaptorSwap.claimLASVerified`, measured **56,538,682 gas** — see `docs/LAS.md §8.4` and
`evm/README.md`.

### 3.7 `relation_zk.{c,h}` + `relation_zk_lazer.{c,h}` — Fig. 1 proof π (new; vendored LaZer reused as-is)
The paper-§4.1 proof of knowledge π. New code: `relation_prove` /
`relation_proof_verify` (statement building + witness decomposition, relation
layer) and the bridge `relation_zk_lin_prove` / `relation_zk_lin_verify` (the
single TU that includes `lazer.h`). The proof system itself is the vendored
**LaZer** library (`third_party/lazer`, git-ignored) **called as-is, zero
files modified** — the same posture as `secp256k1-zkp` in the classical
baseline. `ref/relation_zk_params.h` is generated (LaZer `sage
lin-codegen.sage` from `scripts/las_pi_params.py`) and committed. Rust twin:
`relation_zk.rs` + `build.rs` (feature `relation-zk`), FFI onto the same
bridge. See `docs/LAS.md §7.6` and `THEORY_IMPL_BRIDGE.md §12.6`.

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
