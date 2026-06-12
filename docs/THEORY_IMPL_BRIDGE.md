# Theory → Implementation Bridge

*Mapping every paper equation in eprint 2020/845 (Algorithm 2) to the exact
C function, file, and line in this implementation.*

This document is for your own understanding. Use it when reading the paper
alongside the code, or when explaining design choices in the report.

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
| Ring `R_q = Z_q[X]/(X^N+1)` | `poly` (struct of `int32_t coeffs[N]`) | `ref/poly.h` | degree-255 polynomial mod Q |
| Modulus `q` | `Q` | `ref/params.h` | 8380417 (≈2^23; paper uses 2^24) |
| Ring degree `d` | `N` | `ref/params.h` | 256 |
| Module dimension `n` | `LAS_N` | `ref/las.h:32` | 4 |
| Extra columns `ℓ` | `LAS_ELL` | `ref/las.h:33` | 4 |
| Total vector length `n+ℓ` | `LAS_M` | `ref/las.h:34` | 8 |
| Challenge weight `κ` | `LAS_KAPPA` | `ref/las.h:35` | 60 |
| Mask bound `γ` | `LAS_GAMMA` | `ref/las.h:36` | 122880 (= κ·d·(n+ℓ) = 60·256·8) |
| Matrix `A = [I_n \| A']` | `las_pp.mat[LAS_N][LAS_ELL]` (A' only) | `ref/las.h:52` | A' in NTT domain; identity handled implicitly |
| Public key / statement `t = A·r` | `las_pk.t[LAS_N]` | `ref/las.h:54` | vector of n=4 polynomials in [0,Q) |
| Secret key / witness `r ∈ S_1` | `las_sk.s[LAS_M]` | `ref/las.h:55` | vector of n+ℓ=8 ternary polynomials |
| Signature `σ = (c, z)` | `las_sig` | `ref/las.h:56` | challenge poly + response vector |
| Ternary set `S_1` | `{-1, 0, 1}` | — | sampled by `sample_ternary` in `las.c:169` |
| Mask set `S_γ` | `[-γ, γ]` | — | sampled by `sample_Sgamma` in `las.c:138` |

---

## 3. Public parameters: `las_setup`

### Paper: sample `A' ← R_q^{n×ℓ}` uniformly at random

```c
/* las.c:199 — las_setup */
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
/* las.c:56 — las_Amul */
// identity block: w[i] += v[i]   (v[0..n-1])
// A' block:       w[i] += Σ_j A'[i][j] * v[n+j]  (v[n..n+ℓ-1])
```

Step by step:
1. NTT-transform `v[n..n+ℓ-1]` → `vhat[j]` (line 61–63)
2. For each output row `i`: pointwise-multiply `A'[i][j]` (already NTT) with
   `vhat[j]`, accumulate in `acc` (lines 67–70)
3. Inverse-NTT `acc` back to normal domain (line 72)
4. Add identity contribution `v[i]` (line 73)
5. Canonicalise to `[0, Q)` with `reduce` + `caddq` (lines 74–75)

**Why canonicalise?** The output is used either for hashing (needs a canonical
form) or for subtraction in Verify (`A·z − c·t`). Using `[0, Q)` consistently
means the equality check `poly_equal(Ay, Y->t)` in `las_ext` is byte-exact.

---

## 5. KeyGen

### Paper: `r ← S_1^{n+ℓ}; t = A·r; return (pk=t, sk=r)`

```c
/* las.c:208 — las_keygen */
randombytes(seed, LAS_SEEDBYTES);            // fresh randomness
for(j = 0; j < LAS_M; ++j)
    sample_ternary(&sk->s[j], seed, LAS_SEEDBYTES, (uint16_t)j);  // r ← S_1^8
las_Amul(pk->t, pp, sk->s);                  // t = A·r
```

**`sample_ternary` (las.c:169):** reads 2 bits at a time from a SHAKE256 stream.
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
/* las.c:115 — hash_challenge */
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

**`las_challenge` (las.c:81):** identical to Dilithium's `poly_challenge`
(`ref/poly.c:489`) with `TAU=60`. Places exactly `κ=60` nonzero `±1` entries
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
/* las.c:217 — las_sign */
randombytes(seed, 64);
for(;;) {                                          // rejection loop
    for(j=0; j<LAS_M; ++j)
        sample_Sgamma(&y[j], seed, 64, nonce++);  // y ← S_γ^8
    las_Amul(w, pp, y);                            // w = A·y
    hash_challenge(&c, pk, w, m, mlen);            // c = H(pk, w, M)
    for(j=0; j<LAS_M; ++j) {
        polymul(&cr, &c, &sk->s[j]);               // c·r_j
        poly_add(&sig->z[j], &y[j], &cr);          // z_j = y_j + c·r_j
        poly_reduce(&sig->z[j]);
    }
    if(chknorm_vec(sig->z, LAS_BOUND_SIGN)) continue;  // ‖z‖∞ > γ−κ → retry
    sig->c = c;
    return;
}
```

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

**Deterministic variant (`las_sign_det`, `las.c`).** The randomised `las_sign`
draws the 64-byte mask seed from `randombytes`; `las_sign_det` instead derives it
as `SHAKE256(0x00 ‖ sk ‖ M)` (and `las_presign_det` as `SHAKE256(0x01 ‖ sk ‖ Y ‖ M)`),
making (pre)signing a pure function of its inputs. Both call the same `sign_core`
/ `presign_core` rejection loop, so distribution and validity are unchanged; the
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
/* las.c:242 — las_verify */
if(chknorm_vec(sig->z, LAS_BOUND_SIGN)) return -1;   // ‖z‖∞ > γ−κ
las_Amul(w, pp, sig->z);                              // A·z
for(j=0; j<LAS_N; ++j) {
    polymul(&ct, &sig->c, &pk->t[j]);                 // c·t_j
    poly_sub(&w[j], &w[j], &ct);                      // A·z − c·t
    poly_reduce(&w[j]);
    poly_caddq(&w[j]);                                // → [0, Q)
}
hash_challenge(&c2, pk, w, m, mlen);                  // H(pk, w', M)
return poly_equal(&c2, &sig->c) ? 0 : -1;            // c == c2 ?
```

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
/* las.c:261 — las_presign */
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
/* las.c:291 — las_preverify */
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

**The tripwire (test step 5):** If you feed `σ̂` to standard `las_verify`, it
computes `H(pk, w', M) = H(pk, w, M)`. Since `c = H(pk, w+Y, M) ≠ H(pk, w, M)`
(with overwhelming probability over the random oracle H), standard Verify returns
false. This is asserted in `test_las.c:67–70`.

---

## 11. Adapt

### Paper:
```
if PreVerify(Y, pk, σ̂, M) = false: return ⊥
return σ = (c, ẑ + y_wit)
```

```c
/* las.c:315 — las_adapt */
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
/* las.c:330 — las_ext */
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
always canonicalised to `[0,Q)`) and `Y->t[j]` (set by `las_keygen` via
`las_Amul`, also `[0,Q)`) use the same canonical representation.

---

## 12.5 AMHL — multi-hop locks (`amhl.c`, `las_presign_k`)

### Paper: AMHL (eprint 2020/845, Fig. 2 / Section 5)
For a K-hop route, lock hop `j` with the cumulative statement `Y_j = A·s_j` where
`s_j = l_1 + … + l_j`, and pre-sign every hop with the tightened bound `γ−κ−K`.

#### The K-hop bound
```c
/* las.h:56 */
#define LAS_BOUND_PRESIGN_K(K)  (LAS_GAMMA - LAS_KAPPA - (int32_t)(K) + 1)
```
`poly_chknorm` rejects at `≥ bound`, so this accepts `‖ẑ‖∞ ≤ γ−κ−K`. The adapted
response `z = ẑ + s_j` then has `‖z‖∞ ≤ (γ−κ−K) + ‖s_j‖∞ ≤ (γ−κ−K) + K = γ−κ`,
which clears ordinary `Verify`. `K=1` reproduces `LAS_BOUND_PRESIGN` exactly.

```c
/* las.c:317 — las_presign_k */   // identical to las_presign except:
if(chknorm_vec(presig->z, LAS_BOUND_PRESIGN_K(nhops))) continue;  // las.c:341
/* las.c:348 — las_preverify_k */ // same, with LAS_BOUND_PRESIGN_K(nhops)
```
(The parameter is named `nhops`, not `K`, because Dilithium's `params.h` already
defines the object-like macro `K` for its module dimension — a direct `K`
parameter would be textually replaced by `6` and fail to compile.)

#### Cumulative setup: `amhl_setup_gen`
```c
/* amhl.c:21 — amhl_setup_gen */
las_keygen(&Lj, &st->incr[j-1], pp);      // (L_j, l_j) = (A·l_j, l_j) ← reuse KeyGen
// s_j = s_{j-1} + l_j           (amhl.c:36, kept small/centred)
// Y_j = Y_{j-1} + L_j = A·s_j   (amhl.c:41, canonical [0,Q))
```
The statements are built **additively** from the increment key pairs, so AMHL adds
no new lattice arithmetic — it is pure reuse of `las_keygen` (= `A·(·)`) plus
`poly_add`. `Y_0 = 0`, `s_0 = 0`.

#### Adapt / Ext are unchanged
Hop `j` is adapted with the *cumulative* witness `s_j` (= `cum[j]`), and `las_ext`
returns exactly `s_j` because `A·s_j = Y_j` by construction. No K-specific code
path is needed in Adapt or Ext (§11, §12).

#### Witness recovery along the path: `amhl_recover_prev`
```c
/* amhl.c:62 — amhl_recover_prev:  prev = cur − incr */
poly_sub(&prev->s[i], &cur->s[i], &incr->s[i]);   // s_{j-1} = s_j − l_j
```
After extracting `s_j` from the on-chain claim of hop `j`, intermediary `U_{j-1}`
subtracts the increment `l_j` it was given to obtain `s_{j-1}`, the opener of its
own hop. `test_amhl.c` asserts the recovered `s_{j-1}` equals the setup value
byte-for-byte (exact recovery).

#### Why there is no wormhole
The statements are pairwise distinct (`Y_i ≠ Y_j`), so the opener of one hop does
not open a non-adjacent hop. `test_amhl.c` asserts the attack fails directly:
adapting hop 1 with the receiver's secret `s_K` produces a signature for which
ordinary `Verify` recomputes `A·z−c·t = w + Y_K`, but the challenge was
`c = H(pk, w + Y_1, M)`, so `H(pk, w+Y_K, M) ≠ c` and `Verify` returns false.

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
| Modulus `q` | ≈2^24 | 8380417 ≈ 2^23 | Correctness unaffected (Q > 2γ); reduced MSIS/MLWE security margin |
| Multi-hop PCN | AMHL with `γ−κ−K` per hop | **AMHL implemented** (`amhl.c`, `las_presign_k`, §12.5) + same-Y baseline (`chain.c`) | Functionally matches the paper's multi-hop locks; a *privacy*-preserving variant remains future work |
| Signature packing | Bit-packed, ~3210B | Bit-packed wire/on-chain encoding **implemented** (`serialize.c`, 4672B) + full-int32 9216B in-memory structs | Sizes only; correctness unaffected. Validating decoder + `las_verify_packed` byte-level verifier added for on-chain use |
| Hint vector | Used in paper's optimised scheme | Not used (simplified scheme) | ~2.7 attempts/sign (≈37% acceptance, measured directly); Dilithium's own rate not measured here |

---

## 16. Test assertions as theorems

`ref/test/test_las.c` encodes the formal correctness properties as executable
assertions over 200 randomised iterations:

| Test step | Theorem being checked |
|---|---|
| Step 4: PreVerify accepts | Pre-signature correctness (§4.1 ✓) |
| Step 5: Verify rejects | Statement binding (§4.2 — the "tripwire") |
| Step 7: Verify accepts adapted σ | Pre-signature adaptability (§4.1 ✓) |
| Step 8: Ext recovers y and A·y'==Y | Witness extractability (§4.1 ✓) |
| Sign/Verify round-trip | Base scheme correctness |
| Forgery check (flip bit, expect reject) | Basic unforgeability |

`ref/test/test_amhl.c` then extends these to the multi-hop (AMHL) setting:

| AMHL test step | Theorem being checked |
|---|---|
| `Y_j ≠ Y_{j-1}` for all hops | Distinct per-hop statements (no shared lock) |
| `‖s_j‖∞ ≤ j` for all hops | Cumulative witness-norm growth (the `γ−κ−K` bound's reason) |
| Adapt hop 1 with `s_K` ⇒ `Verify` rejects | Wormhole resistance (non-adjacent hops are unrelated) |
| Right-to-left cascade all `Verify`/claim | K-hop adaptability with the `γ−κ−K` bound |
| `Ext`→`s_j`, then `s_j − l_j == s_{j-1}` (setup) | Exact per-hop witness recovery along the path |
| Refund rejected pre-timeout, accepted post-timeout | Time-lock safety on a route |

All pass on every run. The AMHL test (`test_amhl3`, mode 3 — mode-independent by
construction, like `test_pcn`/`test_swap`) exercises the general `K`-hop case
(`K = 4` happy path, `K = 2` refund path) with the `γ−κ−K` bound; the single-hop
`test_las.c` remains the `K = 1` specialisation, run on modes 2/3/5.
