# A/01 · How upstream ML-DSA does KeyGen / Sign / Verify

> **Where:** `ref/sign.c` (C reference) and `rust/fips204-las/src/ml_dsa.rs`
> (Rust `fips204` crate). Both are **vendored unmodified**. This page is the
> baseline the next two pages contrast against.

Both upstreams implement the *identical* FIPS 204 algorithm; only the engineering
differs. One description covers both; the engineering split is at the bottom.

---

## KeyGen — make a key pair

> C: `crypto_sign_keypair` ([sign.c:23](../../../../ref/sign.c#L23)) ·
> Rust: `key_gen_internal` ([ml_dsa.rs:57](../../../../rust/fips204-las/src/ml_dsa.rs#L57))

1. Draw a 32-byte seed ξ; expand it with SHAKE256 into three seeds
   **(ρ, ρ′, K)** — one for the matrix, one for the secret, one for signing.
2. **ExpandA(ρ):** build the public matrix **A** from ρ, directly in the NTT
   domain (SHAKE128).
3. **ExpandS(ρ′):** sample two short secret vectors **s₁ ∈ S_η^ℓ** and
   **s₂ ∈ S_η^k** (η = 2 or 4).
4. Compute **t = A·s₁ + s₂** using the NTT.
5. **Power2Round(t) → (t₁, t₀):** split t into high bits t₁ and low bits t₀.
   - **public key = (ρ, t₁)** — only the *high* half is published. This is the
     size optimisation.
   - **secret key = (ρ, K, tr = H(pk), s₁, s₂, t₀)**.

---

## Sign — produce a signature on message M

> C: `crypto_sign_signature_internal` ([sign.c:85](../../../../ref/sign.c#L85)) ·
> Rust: `sign_internal` ([ml_dsa.rs:153](../../../../rust/fips204-las/src/ml_dsa.rs#L153))

1. Compute the message digest **µ = H(tr ‖ ctx ‖ M)** and a per-signature seed
   **ρ″ = H(K ‖ rnd ‖ µ)** (rnd = 0 gives deterministic signing).
2. **Rejection loop** (repeat until a candidate passes every check):
   - mask **y = ExpandMask(ρ″, counter)** — coefficients in the power-of-two
     range (−γ₁, γ₁];
   - commitment **w = A·y**;
   - **decompose** w into (w₁, w₀) and hash **only the high bits**:
     **c̃ = H(µ ‖ w1Encode(w₁))**;
   - challenge **c = SampleInBall(c̃)** — a polynomial with τ coefficients of ±1;
   - response **z = y + c·s₁**;
   - **reject** if ‖z‖∞ ≥ γ₁−β **or** ‖LowBits(w − c·s₂)‖∞ ≥ γ₂−β;
   - build the **hint vector h** from c·t₀, with two more rejection tests
     (‖c·t₀‖∞ ≥ γ₂, or more than ω hint bits).
3. Signature **σ = (c̃, z, h)**.

---

## Verify — check a signature

> C: `crypto_sign_verify_internal` ([sign.c:289](../../../../ref/sign.c#L289)) ·
> Rust: `verify_internal` ([ml_dsa.rs:351](../../../../rust/fips204-las/src/ml_dsa.rs#L351))

1. Check ‖z‖∞ < γ₁−β.
2. Compute the **approximate** commitment **w′ = A·z − c·t₁·2^d** (approximate
   because t₀ was not published).
3. **Repair the high bits** with the hint: **w₁′ = UseHint(h, w′)**.
4. Accept iff **c̃ = H(µ ‖ w1Encode(w₁′))**.

---

## The one thing to remember from this page

Everything in **bold-with-a-purpose** above — Power2Round, Decompose,
MakeHint/UseHint, the ω bound, the 2nd and 3rd rejection tests, hashing only the
high bits — exists **only to make the public key and signature smaller**. Strip
all of it away and you are left with a plain lattice Σ-protocol:

```
   KeyGen:  t = A·s                 (publish t)
   Sign:    w = A·y; c = H(pk,w,M); z = y + c·s;  reject if z too big
   Verify:  recompute w = A·z − c·t; check c = H(pk,w,M)
```

That stripped-down scheme is exactly our **Simplified Dilithium** — the subject of
[A/02](02-simplified-dilithium.md).

---

## Engineering differences between the two upstreams (not algorithmic)

Same algorithm, different code style. This matters for *reading* the code and for
*benchmark interpretation*, not for correctness.

| Aspect | C reference (`ref/`) | Rust `fips204` crate |
|---|---|---|
| Parameters | `DILITHIUM_MODE` compile-time macros | const generics `<K, L, …>` per level |
| Key handling | byte arrays; sk unpacked and A re-expanded **inside every call** | typed structs holding **precomputed** NTT/Montgomery values (ŝ₁, ŝ₂, t̂₀, and t₁·2^d) |
| Rejection loop | `goto rej` | `loop { … continue }` |
| Secret handling | secrets in stack buffers | secrets **zeroized on drop** (`Zeroize` / `ZeroizeOnDrop`, `types.rs`) |
| Extras | context-string prefix; randomized/deterministic build switch | + HashML-DSA (OID) paths, `CTEST` constant-time flag, RNG injected via a trait |

→ Next: [A/02 · Simplified Dilithium (our base signature)](02-simplified-dilithium.md)
