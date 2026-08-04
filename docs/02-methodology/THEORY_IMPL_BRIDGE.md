# Theory → Implementation Bridge

*Mapping every paper equation in eprint 2020/845 (Algorithm 2) to the exact
C function and file in this implementation. (Function names, not line numbers, so
the references stay correct as the code evolves.)*

This document is for the reader who wants the equation-by-equation correspondence.
For a plain-English, non-cryptographer walkthrough of the whole project, start
with **`docs/01-introduction/LAS_WALKTHROUGH.md`**; for the full design/evaluation write-up see
`docs/LAS.md`. Use this bridge when reading the paper alongside the code.

---

## 1. How to read this document

The paper's algorithm boxes use mathematical notation. This document shows
the C translation of each step, why it works, and what could go wrong if
you changed it. Every claim is grounded in the code as it stands today.

**Key files:**
- `ref/las.h` — types and constants
- `ref/las.c` — all seven scheme functions
- `ref/test/test_las.c` — test assertions (the "proof" each property holds)

---

## 2. Types and parameters

### Paper notation → C type

| Paper notation | C identifier | Defined in | Value / meaning |
|---|---|---|---|
| Ring `R_q = Z_q[X]/(X^d+1)` | `poly` (struct of `int32_t coeffs[N]`) | `ref/poly.h` | degree-255 polynomial mod Q |
| Modulus `q` | `Q` | `ref/params.h` | 8380417 (≈2^23; paper uses 2^24) |
| Ring degree `d` | `N` | `ref/params.h` | 256 |
| Module dimension `n` | `LAS_N` | `ref/las.h` | 4 |
| Extra columns `ℓ` | `LAS_ELL` | `ref/las.h` | 4 |
| Total vector length `n+ℓ` | `LAS_M` | `ref/las.h` | 8 |
| Challenge weight `κ` | `LAS_KAPPA` | `ref/las.h` | 60 |
| Mask bound `γ` | `LAS_GAMMA` | `ref/las.h` | 122880 (= κ·d·(n+ℓ) = 60·256·8) |
| Matrix `A = [I_n \| A']` | `las_pp.mat[LAS_N][LAS_ELL]` (A' only) | `ref/las.h` | A' in NTT domain; identity handled implicitly |
| Public key / statement `t = A·r` | `las_pk.t[LAS_N]` | `ref/las.h` | vector of n=4 polynomials in [0,Q) |
| Secret key / witness `r ∈ S_1` | `las_sk.s[LAS_M]` | `ref/las.h` | vector of n+ℓ=8 ternary polynomials |
| Signature `σ = (c, z)` | `las_sig` | `ref/las.h` | challenge poly + response vector |
| Ternary set `S_1` | `{-1, 0, 1}` | — | sampled by `sample_ternary` in `las.c` |
| Mask set `S_γ` | `[-γ, γ]` | — | sampled by `sample_Sgamma` in `las.c` |

---

## 3. Public parameters: `setup_public_params`

### Paper: sample `A' ← R_q^{n×ℓ}` uniformly at random

```c
/* las.c — setup_public_params */
poly_uniform(&pp->mat[i][j], seed, (uint16_t)((i << 8) + j));
```

- `poly_uniform` (from `ref/poly.c`) uses SHAKE128 to rejection-sample
  coefficients uniform in `[0, Q)`. The nonce `(i<<8)+j` separates the
  `n·ℓ = 16` independent matrix entries.
- **A key detail:** `poly_uniform` leaves coefficients in the *NTT domain*
  (Montgomery representation). This is Dilithium's convention — the matrix
  is never inverse-transformed. Multiplying `A'·v` therefore means:
  NTT-transform `v`, do pointwise multiply, inverse-NTT. The identity block
  `I_n` is handled by directly adding `v[0..n-1]` without any NTT (see
  `las_Amul` below).

---

## 4. Matrix-vector product: `las_Amul`

### Paper: `w = A·v` where `A = [I_n | A']`, `v ∈ R_q^{n+ℓ}`

```c
/* las.c — las_Amul */
// identity block: w[i] += v[i]   (v[0..n-1])
// A' block:       w[i] += Σ_j A'[i][j] * v[n+j]  (v[n..n+ℓ-1])
```

Step by step:
1. NTT-transform `v[n..n+ℓ-1]` → `vhat[j]`
2. For each output row `i`: pointwise-multiply `A'[i][j]` (already NTT) with
   `vhat[j]`, accumulate in `acc`
3. Inverse-NTT `acc` back to normal domain
4. Add identity contribution `v[i]`
5. Canonicalise to `[0, Q)` with `reduce` + `caddq`

**Why canonicalise?** The output is used either for hashing (needs a canonical
form) or for subtraction in Verify (`A·z − c·t`). Using `[0, Q)` consistently
means the equality check `poly_equal(Ay, Y->t)` in `las_ext` is byte-exact.

---

## 5. KeyGen

### Paper: `r ← S_1^{n+ℓ}; t = A·r; return (pk=t, sk=r)`

```c
/* las.c — base_keygen */
randombytes(seed, LAS_SEEDBYTES);            // fresh randomness
for(j = 0; j < LAS_M; ++j)
    sample_ternary(&sk->s[j], seed, LAS_SEEDBYTES, (uint16_t)j);  // r ← S_1^8
las_Amul(pk->t, pp, sk->s);                  // t = A·r
```

**`sample_ternary` (las.c):** reads 2 bits at a time from a SHAKE256 stream.
Values `{0,1,2}` map to `{-1,0,1}`; value `3` is rejected. This gives a
uniform distribution over `{-1,0,1}` with no bias. The nonce `j` separates the
8 component polynomials.

**Why ternary?** The adaptor norm budget `γ−κ−1` requires the witness to have
`‖y_wit‖∞ ≤ 1`. Using a ternary witness makes the statement/witness pair
identical to a key pair, simplifying the interface (no separate Gen algorithm).

---

## 6. Challenge hash: `hash_challenge`

### Paper: `c = H(pk, commit, M)` where `commit = w` (Sign) or `w+Y` (PreSign)

```c
/* las.c — hash_challenge */
// 1. Absorb pk = t  (n=4 polynomials, 4 bytes/coeff, canonical [0,Q))
// 2. Absorb commit  (n=4 polynomials, same encoding)
// 3. Absorb message M
// 4. Squeeze 32-byte seed
// 5. Feed seed to las_challenge → sample c with ||c||_1 = κ, ||c||_∞ = 1
```

**Why 4 bytes per coefficient?** `Q < 2^24`, so 3 bytes would suffice for the
value range, but `uint32_t` packing (4 bytes little-endian) is simpler and
standard in Dilithium. The important invariant is that the SAME encoding is
used at sign time and verify time — if you changed the encoding you would need
to change it in both places consistently.

**`las_challenge` (las.c):** identical to Dilithium's `poly_challenge`
(`ref/poly.c`) with `TAU=60`. Places exactly `κ=60` nonzero `±1` entries
at distinct positions in a 256-element polynomial via Fisher-Yates-style
rejection sampling, using the first 8 squeezed bytes as sign bits.

---

## 7. Sign

### Paper:
```
y ← S_γ^{n+ℓ};  w = A·y;  c = H(pk, w, M);  z = y + c·r
reject if ‖z‖∞ > γ−κ
return σ = (c, z)
```

```c
/* las.c — base_sign_internal (shared by base_sign / base_sign_det) */
for(j=0; j<LAS_M; ++j) { shat[j] = sk->s[j]; poly_ntt(&shat[j]); }  // NTT(s) once per CALL
for(;;) {                                          // rejection loop
    for(j=0; j<LAS_M; ++j)
        sample_Sgamma(&y[j], seed, 64, nonce++);  // y ← S_γ^8
    las_Amul(w, pp, y);                            // w = A·y
    hash_challenge(&c, pk, w, m, mlen);            // c = H(pk, w, M)
    chat = c; poly_ntt(&chat);                     // NTT(c) once per ATTEMPT
    for(j=0; j<LAS_M; ++j) {
        polymul_prehat(&cr, &chat, &shat[j]);      // c·r_j (operands pre-NTT'd)
        poly_add(&sig->z[j], &y[j], &cr);          // z_j = y_j + c·r_j
        poly_reduce(&sig->z[j]);
    }
    if(chknorm_vec(sig->z, LAS_BOUND_SIGN)) continue;  // ‖z‖∞ > γ−κ → retry
    sig->c = c;
    return;
}
```

**NTT hoisting (mirrors upstream `ref/sign.c`).** The secret `r` is invariant
across rejection attempts, so `NTT(s_j)` is computed **once per call** —
exactly as `crypto_sign_signature_internal` does `polyvecl_ntt(&s1)` *before*
its `rej:` loop ([sign.c:128](../../ref/sign.c#L128)). The challenge `c` is
shared by all `n+ℓ` products `c·r_j`, so `NTT(c)` is computed **once per
attempt** — as `sign.c:154` does `poly_ntt(&cp)`. `polymul_prehat` is then only
the pointwise-multiply + inverse-NTT half of the product. The earlier
`polymul(&cr, &c, &sk->s[j])` re-transformed *both* operands on every product
(so `NTT(c)` ran `n+ℓ`× per attempt and `NTT(s_j)` `n+ℓ`× per attempt); the
hoisted form is algebraically identical and is the same amortisation the
reference Dilithium uses.

**Rejection condition:** `LAS_BOUND_SIGN = γ−κ+1 = 122821`. `poly_chknorm(v, B)`
rejects when `‖v‖∞ ≥ B`. So `chknorm_vec(z, 122821)` rejects when `‖z‖∞ ≥ 122821`,
i.e., `‖z‖∞ > γ−κ = 122820`. This exactly encodes "`‖z‖∞ > γ−κ` → reject."

**Why the loop is secure:** A fresh `y` is drawn on each iteration (via `nonce++`),
so the distribution of `z` given the accepted output is independent of `r`. This
is the zero-knowledge property of Fiat–Shamir-with-aborts.

**Acceptance rate:** Measured **directly** at ~37% per attempt (≈2.7 attempts/sig)
via the `las_attempts` counter in `bench_las3`, matching the closed form
`(1 − κ/γ)^{(n+ℓ)·N} ≈ e^{-1} ≈ 36.8%`. (An earlier indirect estimate from the
`t_sign/t_verify` timing ratio reported ~23%; it over-counts because a Sign attempt
does `n+ℓ` `c·r` products vs a Verify's `n` `c·t` products — superseded by the
direct counter; see `docs/LAS.md §8`.) The bound `γ = κ·d·(n+ℓ)` is chosen to make
the MSIS hardness parameter adequate, not to maximise the acceptance rate.
Rejection sampling is intrinsic to Fiat–Shamir-with-aborts. Omitting the hint
vector does not *worsen* acceptance: optimised Dilithium rejects on the `‖z‖∞`
bound **plus** a low-order-bits check and a hint-count limit, while this scheme
rejects on `‖z‖∞` alone, and more conditions can only lower acceptance. So the
hint-free design carries no acceptance penalty (the old ">80% with hints" framing
was directionally wrong); Dilithium's own expected repetitions are a small
single-digit count per its specification.

**Deterministic variant (`base_sign_det`, `las.c`).** The randomised `base_sign`
draws the 64-byte mask seed from `randombytes`; `base_sign_det` instead derives it
as `SHAKE256(0x00 ‖ sk ‖ M)` (and `las_presign_det` as `SHAKE256(0x01 ‖ sk ‖ Y ‖ M)`),
making (pre)signing a pure function of its inputs. Both call the same `base_sign_internal`
/ `las_presign_internal` rejection loop, so distribution and validity are unchanged; the
deterministic variants exist only for reproducible KATs (`test_kat.c`) and to
remove the nonce-reuse failure mode. This is the standard Fiat–Shamir
"derandomisation" (as in deterministic Dilithium), not a change to the scheme.

---

## 8. Verify

### Paper:
```
if ‖z‖∞ > γ−κ: return false
w' = A·z − c·t
return c == H(pk, w', M)
```

```c
/* las.c — base_verify */
if(chknorm_vec(sig->z, LAS_BOUND_SIGN)) return -1;   // ‖z‖∞ > γ−κ
las_Amul(w, pp, sig->z);                              // A·z
chat = sig->c; poly_ntt(&chat);                       // NTT(c) once (cf. sign.c:333)
for(j=0; j<LAS_N; ++j) {
    that = pk->t[j]; poly_ntt(&that);                 // NTT(t_j)
    polymul_prehat(&ct, &chat, &that);                // c·t_j (operands pre-NTT'd)
    poly_sub(&w[j], &w[j], &ct);                      // A·z − c·t
    poly_reduce(&w[j]);
    poly_caddq(&w[j]);                                // → [0, Q)
}
hash_challenge(&c2, pk, w, m, mlen);                  // H(pk, w', M)
return poly_equal(&c2, &sig->c) ? 0 : -1;            // c == c2 ?
```

The same hoisting applies on the verify side: `NTT(c)` once per call (upstream
`crypto_sign_verify_internal` does `poly_ntt(&cp)` once at
[sign.c:333](../../ref/sign.c#L333)), then one `NTT(t_j)` per public-key
polynomial. `PreVerify` is identical with the `w' + Y` offset.

**Why Verify works (the algebra):**
```
A·z − c·t = A·(y + c·r) − c·(A·r) = A·y + c·(A·r) − c·(A·r) = A·y = w
```
So `w' = w`, and `H(pk, w', M) = H(pk, w, M) = c`. ✓

**`poly_equal` is safe here** because both `w` (freshly computed, canonicalised
with `caddq`) and the original `w` stored in the challenge (also canonicalised
before hashing) are in `[0, Q)`. The equality check is coefficient-by-coefficient
on integers — no modular reduction needed.

---

## 9. PreSign

### Paper:
```
y ← S_γ^{n+ℓ};  w = A·y;  c = H(pk, w + Y, M);  ẑ = y + c·r
reject if ‖ẑ‖∞ > γ−κ−1
return σ̂ = (c, ẑ)
```

```c
/* las.c — las_presign */
las_Amul(w, pp, y);                                  // w = A·y
for(j=0; j<LAS_N; ++j) {
    poly_add(&wY[j], &w[j], &Y->t[j]);               // w + Y  ← KEY DIFFERENCE
    poly_reduce(&wY[j]);  poly_caddq(&wY[j]);
}
hash_challenge(&c, pk, wY, m, mlen);                 // c = H(pk, w+Y, M)
// ... ẑ = y + c·r as in Sign ...
if(chknorm_vec(presig->z, LAS_BOUND_PRESIGN)) continue; // tighter bound
```

**The single algorithmic difference from Sign:** The commitment hashed is
`w + Y` instead of `w`. This is the entire adaptor mechanism. The verifier
will compute `A·ẑ − c·t = w`, then check `c == H(pk, w+Y, M)` — and the
`+Y` term makes this different from what standard Verify checks.

**Tighter rejection bound:** `LAS_BOUND_PRESIGN = γ−κ = 122820`, which encodes
"reject when `‖ẑ‖∞ ≥ γ−κ`", i.e., "accept only when `‖ẑ‖∞ ≤ γ−κ−1`."
This leaves a norm budget of 1 for the witness (see Adapt below).

---

## 10. PreVerify

### Paper:
```
if ‖ẑ‖∞ > γ−κ−1: return false
w' = A·ẑ − c·t
return c == H(pk, w'+Y, M)
```

```c
/* las.c — las_preverify */
if(chknorm_vec(presig->z, LAS_BOUND_PRESIGN)) return -1;
las_Amul(w, pp, presig->z);                          // A·ẑ
for(j=0; j<LAS_N; ++j) {
    polymul(&ct, &presig->c, &pk->t[j]);
    poly_sub(&w[j], &w[j], &ct);                     // w' = A·ẑ − c·t (= A·y = w)
    poly_reduce(&w[j]);  poly_caddq(&w[j]);
}
for(j=0; j<LAS_N; ++j) {
    poly_add(&wY[j], &w[j], &Y->t[j]);               // w' + Y
    poly_reduce(&wY[j]);  poly_caddq(&wY[j]);
}
hash_challenge(&c2, pk, wY, m, mlen);                // H(pk, w'+Y, M)
return poly_equal(&c2, &presig->c) ? 0 : -1;        // c == c2 ?
```

**Why it works:** `w' = A·ẑ − c·t = w` (same algebra as Verify), so
`w' + Y = w + Y`, and `H(pk, w+Y, M) = c` (the pre-signing challenge). ✓

**The tripwire (test step 5):** If you feed `σ̂` to standard `base_verify`, it
computes `H(pk, w', M) = H(pk, w, M)`. Since `c = H(pk, w+Y, M) ≠ H(pk, w, M)`
(with overwhelming probability over the random oracle H), standard Verify returns
false. This is asserted in `test_las.c` (the tripwire, test step 5).

---

## 11. Adapt

### Paper:
```
if PreVerify(Y, pk, σ̂, M) = false: return ⊥
return σ = (c, ẑ + y_wit)
```

```c
/* las.c — las_adapt */
if(las_preverify(presig, m, mlen, Y, pk, pp)) return -1;
sig->c = presig->c;
for(j=0; j<LAS_M; ++j) {
    poly_add(&sig->z[j], &presig->z[j], &y->s[j]);  // z = ẑ + y_wit
    poly_reduce(&sig->z[j]);
}
```

**Why the adapted σ passes standard Verify:**
```
A·z − c·t = A·(ẑ + y_wit) − c·t
           = (A·ẑ − c·t) + A·y_wit
           = w + Y                     (since A·ẑ−c·t = w  and  A·y_wit = Y)
```
Standard Verify hashes `H(pk, w+Y, M)` and gets `c`. ✓

**The norm budget:**
```
‖z‖∞ = ‖ẑ + y_wit‖∞ ≤ ‖ẑ‖∞ + ‖y_wit‖∞ ≤ (γ−κ−1) + 1 = γ−κ
```
So the adapted signature passes the `‖z‖∞ ≤ γ−κ` norm check in standard Verify. ✓
This is precisely why PreSign uses the tighter bound `γ−κ−1` rather than `γ−κ`.

---

## 12. Ext

### Paper:
```
s = z − ẑ
if A·s ≠ Y: return ⊥
return s
```

```c
/* las.c — las_ext */
for(j=0; j<LAS_M; ++j) {
    poly_sub(&y->s[j], &sig->z[j], &presig->z[j]);  // s = z − ẑ
    poly_reduce(&y->s[j]);
}
las_Amul(Ay, pp, y->s);                              // A·s
for(j=0; j<LAS_N; ++j)
    if(!poly_equal(&Ay[j], &Y->t[j])) return -1;    // A·s == Y ?
return 0;
```

**Why it recovers the witness:**
```
z − ẑ = (ẑ + y_wit) − ẑ = y_wit
```
Then `A·y_wit = Y` by construction (Y is the statement). ✓

**`poly_equal` is safe here** because both `Ay[j]` (output of `las_Amul`,
always canonicalised to `[0,Q)`) and `Y->t[j]` (set by `base_keygen` via
`las_Amul`, also `[0,Q)`) use the same canonical representation.

---

## 12.5 AMHL — multi-hop locks — **out of scope**

> **Dropped from the project (2026-08-03).** Anonymous multi-hop locks (AMHL,
> eprint 2020/845 Fig. 2 / §5) are **out of scope** — not a deliverable, not a
> bonus, and not future work. The exploratory C files `ref/amhl.{c,h}` remain in
> the tree on the pre-restructure API, do not compile, and must not be repaired.
> Nothing in this project's results, report or evaluation rests on them.

The paper's `γ−κ−K` bound therefore has no counterpart in this build: only the
single-hop `K = 1` case (`BOUND_PRESIGN` = `γ−κ`, §9) is implemented and measured.

---

## 12.6 The proof of knowledge π — paper §4.1 / Fig. 1 (`relation_zk.c`, `relation_zk_lazer.c`)

**Paper.** Fig. 1, message 1: `π ← P((t′; r′), {∃ r′ : A·r′ = t′ ∧ ‖r′‖∞ ≤ 1})`,
suggested realisation Esgin–Nguyen–Seiler; §4.1's fairness analysis needs the
**exact** ternary bound (`s = r′` by M-SIS uniqueness, hence `‖s‖∞ ≤ 1`, hence
u₂'s Adapt clears `γ − κ`). π is verified by u₂ before pre-signing ("If verif.
of π or σ̂₁ fails, Abort") and travels **off-chain only**.

**Code.** Two halves, split because ref headers and the vendored LaZer's
`lazer.h` cannot share a translation unit:

| Paper object / step | C function (file) |
|---|---|
| `π ← P((t′; r′), …)` | `relation_prove` (`relation_zk.c`) |
| verify π against `(A, t′)` | `relation_proof_verify` (`relation_zk.c`) |
| the LNP prover/verifier | `relation_zk_lin_prove` / `relation_zk_lin_verify` (`relation_zk_lazer.c` → LaZer `lin_prover_*` / `lin_verifier_*`) |
| proof-system parameters | `ref/relation_zk_params.h` (committed; generated by LaZer's `sage lin-codegen.sage` from `scripts/las_pi_params.py`) |

**Statement encoding.** LaZer's linear frontend proves per-partition *binary*
coefficients or *exact ℓ₂* bounds (its `wlinf` input is a size hint, **not**
proven), so the ternary statement is the binary decomposition

```
[A | −A | 0] · (r₊ ‖ r₋ ‖ e) = t′ ,   r₊, r₋ binary (proven),  ℓ₂(e) ≤ 16
```

with `r′ = r₊ − r₋`; the all-zero 23rd column carries a dummy witness poly
`e = 0` only because the parameter generator requires one ℓ₂-bounded partition.
`relation_prove` refuses non-ternary witnesses (an Ext output in `R′_A` is not
a π witness). The normal-domain `A′` is recovered from the NTT-domain
`pp->a_prime` by pushing `ntt(1)` through the same pipeline `relation.c` uses
for `A·r′` (validated once against schoolbook negacyclic multiplication), so
the proven matrix is exactly the build's matrix.

**Guarantees (generated parameter set).** Knowledge error ≤ 2⁻¹²⁷ under M-SIS,
zero-knowledge under M-LWE; measured proof ≈ 30.7 KB. Tests: `test_zkp.c`
(completeness / tamper / wrong statement / non-ternary refusal), `test_swap.c`
(π inside the Fig. 1 gate). Rust twin `relation_zk.rs` (feature `relation-zk`)
FFIs into the *same* bridge and parameter set.

---

## 13. Norm-bound encoding convention

The paper writes "reject if `‖z‖∞ > B`" (strict). The C code uses:

```c
poly_chknorm(v, B_code)  // returns 1 (reject) if ‖v‖∞ >= B_code
```

So `B_code = B + 1` to encode "reject if ≥ B+1" = "reject if > B":

| Algorithm | Paper condition | `B_code` in C | Defined as |
|---|---|---|---|
| Sign/Verify | reject if `‖z‖∞ > γ−κ` | `γ−κ+1 = 122821` | `LAS_BOUND_SIGN` |
| PreSign/PreVerify | reject if `‖ẑ‖∞ > γ−κ−1` | `γ−κ = 122820` | `LAS_BOUND_PRESIGN` |

`poly_chknorm` internally assumes inputs have been reduced by `reduce32()`.
All our norm checks are called after `poly_reduce()`, so this precondition is met.

---

## 14. Fiat–Shamir security intuition

The security argument rests on two properties:

1. **Unforgeability (EUF-CMA):** Breaking LAS requires finding a valid `(c,z)` for
   a new message. By the forking lemma applied to the random oracle H, any forger
   can be turned into a Module-SIS solver (short vector `y_1 − y_2 = c_1·r − c_2·r`
   for two queries with the same commitment but different challenges). Module-SIS
   is believed hard classically and quantumly.

2. **Witness extractability:** Given a valid pre-sig `σ̂ = (c, ẑ)` and adapted sig
   `σ = (c, z)`, the extractor computes `s = z − ẑ` and checks `A·s = Y`. Soundness
   follows from the fact that any party who can produce both `σ̂` and `σ` has
   "committed" to `y_wit = z − ẑ` at the time of pre-signing (bound into the
   challenge via `c = H(pk, w+Y, M)`). The binding property holds under Module-LWE.

Neither proof is reproduced here — see eprint 2020/845 §4 for the formal treatment.

---

## 15. Known deviations from the paper

| Property | Paper | This implementation | Impact |
|---|---|---|---|
| Modulus `q` | ≈2^24 | 8380417 ≈ 2^23 — **NIST FIPS 204's modulus**, and FIPS 204 is this project's parameter authority | Correctness unaffected (Q > 2γ); the concrete MSIS/MLWE margin differs from the paper's. Not a shortfall and not a migration target |
| Multi-hop PCN | AMHL with `γ−κ−K` per hop | **out of scope** (dropped 2026-08-03) — single-hop `K = 1` only | The paper's §5 construction is neither built nor claimed; nothing in the results depends on it |
| Signature packing | Bit-packed, ~3210B | Wire/on-chain encoding **implemented** (`serialize.c`, `c_tilde ‖ BitPack(z)`, 4640B) + in-memory structs (8224B) | Sizes only; correctness unaffected. Validating pk/sk decoder + `base_verify_packed` byte-level verifier for on-chain use |
| Hint vector | Used in paper's optimised scheme | Not used (simplified scheme) | ~2.7 attempts/sign (≈37% acceptance, measured directly); Dilithium's own rate not measured here |

---

## 16. Test assertions as theorems

`ref/test/test_las.c` encodes the formal correctness properties as executable
assertions over 1000 randomised iterations (modes 2/3/5):

| Test step | Theorem being checked |
|---|---|
| Step 4: PreVerify accepts | Pre-signature correctness (§4.1 ✓) |
| Step 5: Verify rejects | Statement binding (§4.2 — the "tripwire") |
| Step 7: Verify accepts adapted σ | Pre-signature adaptability (§4.1 ✓) |
| Step 8: Ext recovers y and A·y'==Y | Witness extractability (§4.1 ✓) |
| Sign/Verify round-trip | Base scheme correctness |
| Forgery check (flip bit, expect reject) | Basic unforgeability |

All pass on every run, on modes 2/3/5.
