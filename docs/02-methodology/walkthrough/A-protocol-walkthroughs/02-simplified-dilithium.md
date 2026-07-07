# A/02 · How OUR simplified Dilithium base signature works

> **Our code:** `ref/basesig.c` (C) and
> `rust/fips204-las/src/las_basesig.rs` (Rust). This is the paper's
> **Algorithm 1** ("ordinary lattice-based signature"). It is a **new, small,
> self-contained module** — *not* an edited copy of `sign.c`. It calls the
> reused primitives (NTT, SHAKE, sampling, reduction) and nothing else.

Read [A/01](01-upstream-ml-dsa.md) first. This page describes the *same three
operations*, but as **our** code implements them — with the compression layer
gone. Compare each step directly against A/01.

The scheme parameters are self-contained: `n` rows, `ℓ` extra columns,
`M = n+ℓ`, challenge weight `κ`, mask bound **`γ = κ·d·(n+ℓ)`**, ring degree
`d = 256`, modulus `Q = 8380417`. The D3 engineering set is `(n,ℓ,κ) = (6,5,49)`.

---

## Setup — the public matrix A

Before any key exists, the public parameters fix one matrix
**A = [ I | A′ ]** (identity block next to a random block A′). Only A′ is random,
and it is expanded from a 32-byte seed straight into the NTT domain (the same
trick ML-DSA uses for ExpandA). In LAS this is `las_setup`; the base signature
shares the same A via the shared parameter struct.

Why `[ I | A′ ]` and not a full random matrix: the identity block makes
`t = A·r = r_top + A′·r_bot` — the top half of the one secret plays the role
ML-DSA gives to a *separate* error vector s₂. So there is only **one** secret
vector, not two.

---

## KeyGen — make a key pair

> C: `base_sign_keypair` ([basesig.c:250](../../../../ref/basesig.c#L250)) ·
> Rust: `base_sign_keypair_seed` ([las_basesig.rs:316](../../../../rust/fips204-las/src/las_basesig.rs#L316))

```
   1.  r ← ternary secret in S_1^{n+ℓ}     (every coefficient is −1, 0, or +1)
   2.  t = A · r
   3.  public key = t          secret key = r
```

- **Step 1** uses our own sampler `b_sample_ternary`: it reads 2 bits at a time
  from SHAKE256 and maps {0,1,2}→{−1,0,1} (rejecting the 4th code). The secret is
  **ternary** (‖r‖∞ ≤ 1), not ML-DSA's S_η — because in LAS this very same secret
  type must later serve as the adaptor *witness*, which has to be provably tiny.
- **Step 2** is `b_Amul`: NTT-multiply r_bot by A′, inverse-transform, then add
  r_top (the free identity block). Output is canonicalised to [0, Q).
- **Step 3:** unlike ML-DSA there is **no Power2Round** — we publish the **full**
  `t`. That is the whole difference at KeyGen, and it is deliberate ([B/01](../B-modifications/01-ml-dsa-to-simplified.md), and it is what makes LAS's `Ext`/`Adapt` exact).

---

## Sign — produce a signature σ = (c, z) on message M

> C: `base_sign_signature` ([basesig.c:276](../../../../ref/basesig.c#L276)) ·
> Rust: `base_sign_signature` ([las_basesig.rs:341](../../../../rust/fips204-las/src/las_basesig.rs#L341))

```
   once per call:   ŝ = NTT(r)                          ← the secret is fixed, transform it once
   repeat (rejection loop):
     1.  y ← mask in S_γ^{n+ℓ}                           (each coeff uniform in [−γ, γ])
     2.  w = A · y                                       (the commitment)
     3.  c = H( pk , w , M )                             (the challenge — hashes the FULL w)
     4.  ĉ = NTT(c)                                      ← once per attempt
     5.  z = y + c · r                                   (the response)
     6.  if ‖z‖∞ > γ − κ :  restart from step 1          (rejection sampling)
   return σ = (c, z)
```

Step by step, in our code:

- **The NTT hoist (before the loop and step 4).** The secret `r` never changes
  between attempts, so we transform it **once per call** (`ŝ = NTT(r)`), exactly
  as `sign.c` transforms `s1` before its loop. The challenge `c` is shared by all
  `n+ℓ` products in step 5, so we transform it **once per attempt**. The actual
  multiply `b_polymul_prehat` then only does the pointwise-multiply + inverse
  transform on the already-transformed operands. (This is the redundancy fix from
  this session — see [F/02](../F-project-tracking/02-session-fixes.md).)
- **Step 1** `b_sample_Sgamma`: a uniform mask on the wide interval [−γ, γ]. Not
  ML-DSA's `ExpandMask` (which is locked to power-of-two ranges) — we need the
  interval width to be exactly `2γ+1` so the acceptance rate lands on the paper's
  target (see [D/01](../D-rejection-sampling/01-theory-and-measured.md)).
- **Step 2** `b_Amul`: same matrix product as KeyGen.
- **Step 3** `b_hash_challenge`: SHAKE256 over `pk ‖ w ‖ M`, with every
  polynomial canonicalised to [0, Q) at 4 bytes/coefficient first. We hash the
  **full commitment w** — there is no high/low-bit split to hash a piece of.
- **Step 5** `z = y + c·r` via the hoisted multiply.
- **Step 6** the **only** rejection test: `‖z‖∞ > γ − κ` (using the reused
  `poly_chknorm`). ML-DSA has three rejection tests; two of them exist only
  because of decomposition/hints, which we do not have.

---

## Verify — check a signature σ = (c, z)

> C: `base_sign_verify` ([basesig.c:318](../../../../ref/basesig.c#L318)) ·
> Rust: `base_sign_verify` ([las_basesig.rs:392](../../../../rust/fips204-las/src/las_basesig.rs#L392))

```
   1.  if ‖z‖∞ > γ − κ :  reject
   2.  w′ = A · z − c · t                       (recompute the commitment EXACTLY)
   3.  accept  iff  c == H( pk , w′ , M )
```

- **Step 2** recomputes the commitment **exactly** (`b_Amul` for A·z, then
  subtract c·t via the hoisted multiply, canonicalise). There is **no
  approximation and no hint** — because we published the full `t`, the verifier
  can reproduce `w` to the last bit.
- **Why it works (the algebra):** `A·z − c·t = A·(y + c·r) − c·(A·r) = A·y = w`.
  So `w′ = w`, the hash matches, and the signature verifies.

---

## Side-by-side with upstream (the whole point of this page)

| Operation | Upstream ML-DSA ([A/01](01-upstream-ml-dsa.md)) | Our simplified Dilithium (this page) |
|---|---|---|
| KeyGen output | pk = (ρ, **t₁ only**) after Power2Round | pk = **full t** |
| Sign commitment hash | `H(µ ‖ w1Encode(w₁))` — **high bits only** | `H(pk ‖ w ‖ M)` — **full w** |
| Sign rejection tests | **three** (‖z‖, low-bits, hint/ω) | **one** (‖z‖∞ > γ−κ) |
| Sign output | σ = (c̃, z, **h**) — includes a hint | σ = (c, z) — **no hint** |
| Verify | approximate `w′` + `UseHint` to repair | recompute `w′` **exactly** |

Everything we removed is compression. What remains is the clean Σ-protocol. The
next step — folding a statement `Y` into this scheme to get the adaptor — is
[A/03](03-las-adaptor.md).

→ Next: [A/03 · LAS adaptor (our adaptor scheme)](03-las-adaptor.md)
