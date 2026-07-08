<!-- Part of docs/LAS.md, split by report chapter (2026-07-06). Index: docs/LAS.md.
     Section numbering is preserved verbatim, so external references like
     "LAS.md §5" resolve to this file. Do not renumber sections. -->

## 5. Implementation

### 5.1 What we reuse vs. what we add
LAS is implemented as a **self-contained module** (`ref/las.c`, `ref/las.h`) that
reuses only Dilithium's *mode-independent* primitives:

| Reused from the repo (`poly.c`, `ntt.c`, `reduce.c`, `fips202.c`) | Purpose |
|---|---|
| `poly_ntt`, `poly_invntt_tomont`, `poly_pointwise_montgomery` | NTT polynomial multiplication mod `Q` |
| `poly_add`, `poly_sub`, `poly_reduce`, `poly_caddq` | ring arithmetic / canonicalisation |
| `poly_chknorm` | infinity-norm rejection check |
| `poly_uniform` | expand the public matrix `A'` (NTT domain) |
| SHAKE128/256 (`shake256_*`, `keccak_state`) | the random oracle `H` and all sampling |
| `randombytes` | seeds |

Everything specific to LAS is **new and self-contained**: the dimensions/parameters,
the ternary and `S_γ` samplers, the `κ = 60` challenge sampler, the `[I | A']`
matrix–vector product, the hash-to-challenge `H`, and the seven scheme functions.
Crucially, LAS **does not** use Dilithium's `Power2Round`, hint vector `h`, or
high/low-bit `Decompose` — this is the paper's *simplified* scheme, where the full
commitment `w` (not just its high bits) is hashed. That makes the code small and the
algebra transparent, at the cost of larger keys/signatures than optimised Dilithium.

Because it touches no mode-specific constant (`K`, `L`, `TAU`, `GAMMA1`, …), the
module compiles and behaves identically under `-DDILITHIUM_MODE=2/3/5`; only `N` and
`Q` (global) are used.

### 5.2 Data types (`las.h`)
Vectors are plain C arrays of the repo's degree-`N` `poly`:
```c
typedef struct { poly mat[LAS_N][LAS_ELL]; uint8_t seed[32]; } las_pp;  // A' in NTT domain
typedef struct { poly t[LAS_N]; }              las_pk;   // public key / statement  t = A r
typedef struct { poly s[LAS_M]; }              las_sk;   // secret key / witness     r ∈ S_1
typedef struct { poly c; poly z[LAS_M]; }      las_sig;  // (pre-)signature (c, z)
```
`LAS_N = 4`, `LAS_ELL = 4`, `LAS_M = 8`, `LAS_KAPPA = 60`, `LAS_GAMMA = 122880`.

### 5.3 Public parameters and the `[I | A']` product
`las_setup` expands `A'` from a public seed with `poly_uniform`, which yields
coefficients already in the NTT domain (Dilithium's convention — the uniform samples
*are* the NTT representation). The matrix–vector product exploits the identity block:
```
w = A·v = v_top + A'·v_bot,   v_top = (v_0..v_{n-1}),  v_bot = (v_n..v_{n+ℓ-1})
```
so `las_Amul` NTT-transforms `v_bot`, accumulates `Σ_j Â'_{ij} ∘ v̂_j` pointwise in
Montgomery form, inverse-transforms, then adds the identity part `v_i` directly. This
avoids representing the identity polynomial in the NTT/Montgomery domain and matches
exactly how `crypto_sign_keypair` computes `A·s1`. Output is canonicalised to `[0,q)`
because it is hashed.

### 5.4 Polynomial multiplication
A negacyclic product mod `(X^N+1, Q)` is `ntt(a); ntt(b); pointwise_montgomery;
invntt_tomont; reduce` — the standard Dilithium idiom whose `invntt_tomont`
reabsorbs the Montgomery factor, leaving the true product (a centred
representative). Used for `c·r` (small, `≤κ`) and `c·t` (a full mod-`q` product).

The two forward NTTs are **hoisted** out of the hot paths exactly as upstream
`ref/sign.c` does, so `polymul_prehat(out, ahat, bhat)` is only the
`pointwise_montgomery; invntt_tomont; reduce` tail on operands that are already
in the NTT domain. In `las_signature_internal`/`las_presign_internal` the secret `NTT(s_j)` is taken
**once per call** (invariant across the rejection loop, as `sign.c:128`
transforms `s1` before its `rej:` loop) and the challenge `NTT(c)` **once per
attempt** (shared by all `n+ℓ` products, as `sign.c:154`); in
`las_verify`/`las_preverify`, `NTT(c)` is once per call (`sign.c:333`) and
`NTT(t_j)` once per public-key polynomial. This is a pure micro-optimisation:
the accepted `(c, z)` is bit-identical to the un-hoisted form (the KAT digest is
unchanged), it only removes redundant transforms.

### 5.5 The hash `H` and challenge
`hash_challenge` absorbs a canonical 4-bytes/coefficient encoding of `pk = t`
(`n` polys) then the commitment (`n` polys — `w` for Sign, `w+Y` for PreSign) then
the message `M`, squeezes a 32-byte seed, and calls `las_challenge`. The challenge
sampler is Dilithium's `SampleInBall` with `κ = 60`: the first 8 squeezed bytes give
`κ` sign bits, then a rejection-sampled inside-the-ball permutation places `κ`
`±1`s. This guarantees `‖c‖_1 = κ`, `‖c‖∞ = 1` exactly.

### 5.6 Samplers
- `sample_ternary` (`S_1`): two bits per attempt, value `3` rejected, `{0,1,2} → {−1,0,1}`.
- `sample_Sgamma` (`S_γ`): 18-bit field per attempt (`2γ+1 = 245761 < 2^18`),
  rejection sampling, accepted value mapped to `[−γ, γ]` (acceptance ≈ 93.7 %).

Both seed a SHAKE256 stream from `seed‖nonce`; the nonce separates the `n+ℓ`
components and successive rejection-loop attempts.

### 5.7 Norm checks
`chknorm_vec` calls the repo's `poly_chknorm` per component. `poly_chknorm(·, B)`
rejects when `‖·‖∞ ≥ B` and requires `B ≤ (Q−1)/8 = 1047552`. Our bounds
(`γ−κ+1 = 122821` for Sign/Verify, `γ−κ = 122820` for PreSign/PreVerify) are
comfortably below that, so the primitive is reused directly. We encode the strict
"`> limit`" tests of the spec as `bound = limit + 1`.

### 5.8 The seven functions
`las_keypair`, `las_signature`, `las_verify`, `las_presign`, `las_preverify`, `las_adapt`,
`las_ext` follow Section 4 verbatim. Subtraction of `c·t` happens in the normal
domain; commitments are canonicalised with `reduce`+`caddq` before hashing so that
`w'` at verify time is byte-identical to `w`/`w+Y` at sign time whenever they are
equal mod `q`.

### 5.9 Modulus choice (a deliberate deviation)
The paper specifies `q ≈ 2^24`. Reusing Dilithium's NTT fixes the root-of-unity
table to `Q = 8380417 ≈ 2^23`, so this build uses that `Q`. Since `Q > 2γ`
(`8380417 > 245760`), every intermediate (responses `≤ γ`, products mod `q`) is
represented faithfully and **correctness is unaffected**; only the concrete
Module-SIS/LWE security margin differs from the paper's parameter set. Targeting
exactly `2^24` would require either a new NTT root table or schoolbook
multiplication — noted as future work, out of the project's scope.

---

### 5.10 Serialisation and the on-chain verifier interface (`ref/serialize.{c,h}`)

The scheme functions operate on in-memory `poly` structs, but any realistic
deployment — and specifically an on-chain verifier in the style of poqeth
(eprint 2025/091) — exchanges objects as **byte strings**. `serialize.{c,h}` adds
the canonical wire/on-chain encoding plus a *validating* decoder, leaving the
scheme code (`las.c`) untouched (clean separation):

- **Encoding** (LSB-first bit packing): pk/statement `Y` at 23 bits/coeff
  (`Q < 2^23`); sk/witness at 2 bits/coeff (ternary); signature `(c, z)` as a
  2-bit ternary `c` plus an offset-encoded `z` whose width is parameter-derived
  (`LAS_Z_COEFF_BITS` = 18 bits for the paper/D2 sets, 19 for D3/D5). Sizes
  (paper/D2): `LAS_PK_BYTES = 2944`, `LAS_SK_BYTES = 512`, `LAS_SIG_BYTES = 4672`.
- **Defensive decoding.** A verifier cannot trust its input, so `unpack`
  *rejects* malformed bytes: a pk coefficient `≥ Q`, the invalid ternary code `3`,
  or a `z` field outside the valid band. `pack` symmetrically rejects
  out-of-range inputs (e.g. a non-ternary secret, or a `z` exceeding `γ−κ`).
- **`las_verify_packed(pk_bytes, sig_bytes, M, pp)`** is the byte-level verifier an
  integration would call: it decodes-with-validation and runs ordinary `Verify`,
  returning `0` only if the bytes are well-formed *and* the signature verifies.
  This is exactly the interface a Solidity/precompile/circuit verifier consumes.

`test_serde` (Section 6.3) hard-asserts round-trip identity, that a packed adapted
signature verifies through `las_verify_packed` while a packed *pre-signature* does
not (the statement-binding tripwire survives serialisation), that **every**
single-byte flip of a packed signature breaks verification, and that the
validation paths reject malformed bytes. This realises the "packed" sizes of
Section 8 as concrete, tested code rather than formulas, and is the prerequisite
for the planned on-chain integration.

### 5.11 Deterministic API and reproducibility

To make the implementation *reproducible* — a distinction-level engineering
property, and a prerequisite for cross-checking an independent on-chain verifier —
the randomness-consuming algorithms gain deterministic siblings:

- `las_keypair_seed(pk, sk, pp, seed)` derives the secret directly from a 32-byte
  seed (KeyGen from explicit randomness);
- `las_signature_det` and `las_presign_det` derive the per-signature mask seed as
  `SHAKE256(tag ‖ sk ‖ [Y] ‖ M)` instead of drawing fresh randomness, so the
  output is a *pure function* of the inputs.

Internally `las_signature`/`las_signature_det` share one `las_signature_internal`, and
`las_presign`/`las_presign_k`/`las_presign_det` share one `las_presign_internal`, differing
only in (a) where the 64-byte mask seed comes from (fresh `randombytes` vs the
derivation above) and (b) the rejection bound — so the deterministic and randomised
paths are guaranteed identical in distribution and validity. Beyond reproducibility,
deterministic signing also removes the per-signature RNG dependency and the
nonce-reuse failure mode that has repeatedly broken classical (EC)DSA deployments —
a desirable property in a blockchain setting.

