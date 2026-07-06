<!-- Part of docs/LAS.md, split by report chapter (2026-07-06). Index: docs/LAS.md.
     Section numbering is preserved verbatim, so external references like
     "LAS.md §2" resolve to this file. Do not renumber sections. -->

## 2. Mathematical background

### 2.1 Ring and module setting
Let `d = N = 256`, `q = Q` prime, and `R_q = Z_q[X]/(X^N + 1)` the cyclotomic ring.
Elements are degree-`<N` polynomials with coefficients in `Z_q`; we use the centred
representative range `(−q/2, q/2]` for norms and `[0, q)` for hashing. The infinity
norm `‖a‖∞` of a polynomial is the max absolute centred coefficient; for a vector of
polynomials it is the max over components.

We use the matrix
```
A = [ I_n | A' ] ∈ R_q^{ n × (n+ℓ) }
```
in *Hermite normal form*: an `n×n` identity block concatenated with a uniformly
random `n×ℓ` block `A'`. The map `f_A(x) = A·x` is **Ajtai's hash**; finding a short
non-zero `x` with `A·x = 0` is the **Module-SIS** problem, and distinguishing
`(A, A·s + e)` from uniform (short `s, e`) is **Module-LWE**. Both are believed
hard for classical and quantum adversaries, and underpin Dilithium.

### 2.2 Sets used
- `S_1` — polynomials with ternary coefficients in `{−1, 0, 1}` (`‖·‖∞ ≤ 1`).
  Secret keys and witnesses are drawn from `S_1^{n+ℓ}`.
- `S_γ` — polynomials with coefficients uniform in `[−γ, γ]`. Signing masks are
  drawn from `S_γ^{n+ℓ}`.
- The challenge `c` is a polynomial with exactly `κ` non-zero coefficients, each
  `±1`: `‖c‖_1 = κ`, `‖c‖∞ = 1`. This is Dilithium's `SampleInBall` distribution.

### 2.3 The hard relation
The statement/witness relation is
```
R = { (Y, y) : Y = A·y,  y ∈ S_1^{n+ℓ} }.
```
A statement/witness pair is therefore **just another key pair** `(t, r)` of the base
scheme. Recovering `y` from `Y` is exactly the Module-LWE/SIS key-recovery problem,
so the relation is hard — this is what makes the witness extraction meaningful.

### 2.4 Fiat–Shamir with aborts
LAS, like Dilithium, is a Fiat–Shamir signature with *rejection sampling*. The
signer commits `w = A·y` for a random mask `y`, derives a challenge `c` from a hash,
computes a response `z = y + c·r`, and **rejects and retries** if `z` falls outside
a safe norm band. Rejection both (a) makes `z`'s distribution independent of the
secret `r` (zero-knowledge), and (b) keeps `‖z‖∞` bounded so verification's
soundness holds. `γ = κ·d·(n+ℓ)` is chosen so the acceptance probability per attempt
is high (rejection is rare).

---

