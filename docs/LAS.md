# LAS — A Lattice-Based Adaptor Signature on Dilithium Primitives

*Design, implementation, correctness, testing, application and benchmarks.*

This document is the technical reference for the implementation in `ref/las.{c,h}`,
`ref/test/test_las.c`, `ref/test/test_swap.c` and `ref/test/bench_las.c`. It is
written to be the source material for the dissertation chapter; section numbering
maps roughly onto report sections.

---

## 1. Introduction and motivation

Blockchains authorise transactions with digital signatures, today almost always
ECDSA or Schnorr over elliptic curves. Both are broken by Shor's algorithm on a
cryptographically relevant quantum computer. The migration target is
*post-quantum* (PQ) cryptography, built on problems — here, lattice problems —
for which no efficient quantum algorithm is known.

NIST has standardised *basic* PQ signatures: CRYSTALS-Dilithium (ML-DSA), Falcon
and SPHINCS+. These provide existential unforgeability and nothing more. Real
blockchain protocols, however, lean on *exotic* signatures that bundle extra
functionality — multisignatures, threshold, ring, and **adaptor** signatures. In
the PQ setting these exotic schemes are largely *paper-only*: described and proven
secure, but with little or no working code, and to our knowledge none deployed or
demonstrated on a blockchain workflow. This project closes part of that gap for
**adaptor signatures**, the primitive behind *scriptless scripts*, atomic swaps and
payment-channel networks.

An **adaptor signature** ties a signature to the revelation of a secret. Concretely
it augments a base signature scheme with four algorithms:

- `PreSign(sk, Y, M)` → a *pre-signature* `σ̂` on message `M`, bound to a public
  *statement* `Y`;
- `PreVerify(Y, pk, σ̂, M)` → checks a pre-signature is well-formed and bound to `Y`;
- `Adapt((Y, y), σ̂)` → using the *witness* `y` for `Y`, completes `σ̂` into an
  ordinary signature `σ`;
- `Ext(Y, σ, σ̂)` → recovers the witness `y` from a pre-signature and its adapted
  signature.

The magic property: `σ` is an ordinary signature that any verifier accepts with the
*unmodified* verification algorithm, yet publishing it lets anyone holding `σ̂`
extract `y`. In an atomic swap this means "claiming your coin reveals the secret
that lets me claim mine" — atomicity without on-chain scripts.

We implement **LAS** (Esgin, Ersoy, Erkin, *Post-Quantum Adaptor Signatures and
Payment Channel Networks*, eprint 2020/845), the first lattice-based adaptor
signature, reusing the CRYSTALS-Dilithium reference C code for all low-level
arithmetic.

---

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

## 3. The base signature

Parameters: `n = ℓ = 4`, `n+ℓ = 8`, `κ = 60`, `γ = κ·d·(n+ℓ) = 60·256·8 = 122880`.

```
KeyGen():                              # = Gen
    r ← S_1^{n+ℓ}                      # ternary secret
    t = A·r                            # public key
    return (pk, sk) = (t, r)

Sign(sk, M):
    repeat:
        y ← S_γ^{n+ℓ}
        w = A·y
        c = H(pk, w, M)                # SampleInBall, ‖c‖_1 = κ
        z = y + c·r
    until ‖z‖∞ ≤ γ − κ                 # else resample
    return σ = (c, z)

Verify(pk, σ=(c,z), M):
    if ‖z‖∞ > γ − κ: return false
    w' = A·z − c·t
    return c == H(pk, w', M)
```

**Why Verify works.** For an honest signature,
`A·z − c·t = A(y + c·r) − c·(A·r) = A·y = w`, so `w' = w` and the recomputed
challenge equals `c`. The bound `‖c·r‖∞ ≤ κ` (a `±1`-weight-`κ` challenge times a
ternary vector) guarantees `‖z‖∞ ≤ γ + κ` before rejection, and the accepted band
`‖z‖∞ ≤ γ − κ` leaves room for the adaptor offset (Section 4).

---

## 4. The adaptor extension (LAS, variant B)

The single idea that turns the base scheme into an adaptor signature is: **fold the
statement `Y` into the Fiat–Shamir hash during pre-signing.**

```
PreSign(sk, Y, M):
    repeat:
        y ← S_γ^{n+ℓ}
        w = A·y
        c = H(pk, w + Y, M)            # <-- statement folded in
        ẑ = y + c·r
    until ‖ẑ‖∞ ≤ γ − κ − 1            # tighter bound by 1
    return σ̂ = (c, ẑ)

PreVerify(Y, pk, σ̂=(c,ẑ), M):
    if ‖ẑ‖∞ > γ − κ − 1: return false
    w' = A·ẑ − c·t
    return c == H(pk, w' + Y, M)

Adapt((Y, y), σ̂=(c,ẑ)):
    if not PreVerify(...): return ⊥
    return σ = (c, ẑ + y)

Ext(Y, σ=(c,z), σ̂=(c,ẑ)):
    s = z − ẑ
    if A·s ≠ Y: return ⊥
    return s
```

### 4.1 Correctness, line by line
Let `t = A·r` (signer key) and `Y = A·y_w` (statement, witness `y_w`).

- **PreVerify accepts honest pre-signatures.**
  `A·ẑ − c·t = A(y + c·r) − c·(A·r) = A·y = w`. Hence `w' = w` and
  `H(pk, w' + Y, M) = H(pk, w + Y, M) = c`. ✔

- **Adapted signatures verify with the *ordinary* Verify.**
  With `z = ẑ + y_w`,
  `A·z − c·t = A·ẑ + A·y_w − c·t = (w + c·t) + Y − c·t = w + Y`.
  So ordinary Verify recomputes `w'' = w + Y` and checks `H(pk, w'', M)`. But the
  pre-signing challenge was `c = H(pk, w + Y, M)`, so it matches. ✔
  The adapted signature is indistinguishable from one produced directly by `Sign`
  on the *shifted* commitment — no special verifier is needed on-chain.

- **Extraction recovers the witness.**
  `z − ẑ = (ẑ + y_w) − ẑ = y_w`, and `A·(z − ẑ) = A·y_w = Y`, so `Ext` returns
  `y_w` exactly and the `A·s == Y` check passes. ✔

- **The norm budget.** PreSign accepts only `‖ẑ‖∞ ≤ γ − κ − 1`. The witness is
  ternary (`‖y_w‖∞ ≤ 1`), so the adapted response satisfies
  `‖z‖∞ = ‖ẑ + y_w‖∞ ≤ (γ − κ − 1) + 1 = γ − κ`,
  exactly the band ordinary Verify accepts. This one-unit tightening is the whole
  reason Adapt produces in-bounds signatures. ✔

### 4.2 The "tripwire": a pre-signature is **not** a signature
A pre-signature must fail the *ordinary* verifier — otherwise the statement binding
would be meaningless. Feeding `σ̂ = (c, ẑ)` to `Verify`:
- the norm check passes (`‖ẑ‖∞ ≤ γ − κ − 1 < γ − κ`), so rejection is **not** the
  reason;
- ordinary Verify recomputes `w' = A·ẑ − c·t = w` and checks `H(pk, w, M)`. But
  `c = H(pk, w + Y, M)`. Since `Y ≠ 0` with overwhelming probability,
  `H(pk, w, M) ≠ H(pk, w + Y, M)` and Verify returns false.

This is a **cryptographic** failure (a Fiat–Shamir mismatch caused by the missing
`+Y`), not a formatting or length artefact. `test_las.c` asserts it on every
iteration (test step 5).

### 4.3 Security properties (stated, not proven — out of scope)
LAS satisfies the three standard adaptor-signature notions (proven in eprint
2020/845): *pre-signature correctness*, *pre-signature adaptability* (any valid
pre-signature can be adapted with a valid witness), and *witness extractability*
(a valid pre-signature plus its adapted signature yields the witness). The proofs
rely on Module-SIS/LWE hardness and the forking lemma; we do not reproduce them.

### 4.4 Why variant (B) and not "variant (A)"
An earlier internal sketch ("variant A") put the offset on the *response* — pre-sign
emits `z̃ = z + y` and verification *subtracts* `Y` — which requires the pre-signer
to know the witness `y` and produces an inflated response needing a widened
encoding. The paper's Algorithm 2 is variant (B): the pre-signer needs only the
*statement* `Y` (correct adaptor semantics — the signer must *not* know the witness),
the inflated value never needs special packing, and the adapted signature is an
ordinary Dilithium-shaped signature. We switched to (B) to match the paper and to
keep the on-chain object a standard signature.

---

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
`polymul(out, a, b)` performs a full negacyclic product mod `(X^N+1, Q)`:
`ntt(a); ntt(b); pointwise_montgomery; invntt_tomont; reduce`. This is the standard
Dilithium idiom whose `invntt_tomont` reabsorbs the Montgomery factor, leaving the
true product (reduced to a centred representative). Used for `c·r` (small, `≤κ`) and
`c·t` (a full mod-`q` product).

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
`las_keygen`, `las_sign`, `las_verify`, `las_presign`, `las_preverify`, `las_adapt`,
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

## 6. Testing

### 6.1 Functional tests (`ref/test/test_las.c`)
Per iteration (200 iterations, modes 2/3/5, random `pp`, keys, message):
1. `(pk, sk) = KeyGen`
2. `(Y, y)  = KeyGen` — statement/witness is another key pair
3. `σ̂ = PreSign(sk, Y, M)`
4. assert `PreVerify(Y, pk, σ̂, M) == true`
5. **assert `Verify(pk, σ̂, M) == false`** — the tripwire (hash omits `+Y`)
6. `σ = Adapt((Y, y), σ̂)`
7. assert `Verify(pk, σ, M) == true` — adapted sig is ordinary
8. `y' = Ext(Y, σ, σ̂)`; assert `A·y' == Y` **and** `y' == y` (exact)

Plus an ordinary `Sign`/`Verify` round-trip and a forgery check (flip a message
bit, expect Verify to reject). **Result: all assertions pass on all modes, with
zero warnings under `-Wall -Wextra -Wpedantic -Wmissing-prototypes -Wshadow -Wvla`.**

### 6.2 Why the tests are meaningful
Step 4 exercises pre-signature correctness; step 5 proves the statement is genuinely
*bound* (a pre-signature is not a usable signature); steps 6–7 prove adaptability and
that the result is an *ordinary* signature; step 8 proves witness extractability and
*exactness* (no knowledge-gap noise in this parameterisation). Together they
demonstrate the full adaptor-signature contract end to end.

---

## 7. Application: a post-quantum atomic swap (`ref/test/test_swap.c`)

### 7.1 Protocol
Two parties, Alice and Bob, want to swap coins across two ledgers ("chain A" and
"chain B") with no trusted third party and no on-chain scripts.

```
Setup:  public params A; Alice key (pkA, skA); Bob key (pkB, skB).
        Bob draws a fresh (Y, y) = KeyGen  (Bob is the witness holder).

1. Bob → Alice:  Y                                   (statement only; y kept secret)
2. Alice:  σ̂_A = PreSign(skA, Y, tx_A)   (tx_A: Alice→Bob on chain A)
   Alice → Bob;  Bob checks PreVerify(Y, pkA, σ̂_A, tx_A)
3. Bob:    σ̂_B = PreSign(skB, Y, tx_B)   (tx_B: Bob→Alice on chain B)
   Bob → Alice;  Alice checks PreVerify(Y, pkB, σ̂_B, tx_B)
   [Neither pre-signature is spendable: Verify(σ̂_A)=Verify(σ̂_B)=false.]
4. Bob (knows y):  σ_A = Adapt((Y, y), σ̂_A);  PUBLISH σ_A on chain A.
   Verify(pkA, σ_A, tx_A)=true  ⇒  Bob claims Alice's coin.
5. Alice observes σ_A on chain A and extracts:  y' = Ext(Y, σ_A, σ̂_A).
6. Alice (now knows y'=y):  σ_B = Adapt((Y, y'), σ̂_B);  PUBLISH σ_B on chain B.
   Verify(pkB, σ_B, tx_B)=true  ⇒  Alice claims Bob's coin.
```

### 7.2 Why it is atomic
The *same* statement `Y` binds both pre-signatures. Bob can only claim Alice's coin
by adapting `σ̂_A`, which publishes `σ_A`; from `σ_A` and `σ̂_A` Alice extracts `y`,
which is exactly what she needs to adapt `σ̂_B` and claim Bob's coin. Conversely,
**before** `σ_A` is published Alice does not know `y`, so she cannot complete `σ̂_B`
(it fails ordinary Verify — the tripwire). So either both legs settle or neither
does. The demo prints this narrative and asserts every step, including the
counterfactual that pre-adaptation `σ̂_B` is unspendable.

### 7.3 Relation to payment channels
The same mechanism generalises to payment-channel networks (multi-hop payments):
each hop is pre-signed against a statement derived from the same secret, and the
receiver's claim cascades witness revelation back along the path. The "knowledge
gap" caveat (Section 8) is what bounds how long such chains can be in the relaxed
lattice setting; our exact-extraction parameterisation sidesteps it for the demo.

---

## 8. Performance (measured)

Wall-clock microseconds per operation, mode 3, 2000 iterations/op, on the build
container (`-O3`). Absolute numbers are machine-dependent; the *ratios* are the
point.

| Operation | Time (µs) | Note |
|---|---:|---|
| Setup (expand `A`) | ~41 | `n·ℓ = 16` uniform polys |
| KeyGen | ~44 | sample `r`, compute `A·r` |
| Sign | ~480 | dominated by `S_γ` sampling + rejection loop |
| Verify | ~112 | `A·z − c·t` + hash |
| PreSign | ~482 | ≈ Sign; folding `+Y` is negligible |
| PreVerify | ~112 | ≈ Verify; one extra `+Y` add |
| Adapt | ~117 | runs PreVerify + a vector add |
| Ext | ~35 | one `A·s` + compare |

In-memory object sizes (full `int32` coefficients, **not** bit-packed):
`pk = Y = 4096 B`, `sk = witness = 8192 B`, `signature = pre-signature = 9216 B`.

**Takeaways for the report.** (i) The adaptor operations cost essentially the same
as the base operations — `PreSign ≈ Sign`, `PreVerify ≈ Verify` — so the adaptor
functionality is effectively *free* relative to signing, matching the paper's claim
that LAS is "essentially as efficient as an ordinary lattice signature". (ii)
`Adapt`/`Ext` cost on the order of a single verification. (iii) Sizes are large only
because the simplified scheme stores full coefficients and hashes the full
commitment `w`; a bit-packed encoding (à la Dilithium's `polyz`/hint packing) would
shrink them substantially and is the obvious next optimisation. (iv) `Sign`/`PreSign`
dominate, due to wide `S_γ` sampling and rejection; acceptance per attempt is high
because `γ = κ·d·(n+ℓ)`.

## 9. Limitations and future work
- **Knowledge gap.** Extraction here is exact. In the paper's relaxed relation the
  extracted witness can carry bounded noise that accumulates along long channel
  paths; handling that is a known hard point and is out of scope.
- **Security proofs.** Not reproduced; we rely on the paper's analysis.
- **Modulus.** `Q ≈ 2^23` rather than the paper's `2^24` (Section 5.9).
- **Sizes/performance.** The simplified scheme hashes the full `w` and stores full
  responses, so keys/signatures are larger than optimised Dilithium; a hint-based
  optimisation is possible but unnecessary for a feasibility demonstration.
- **Constant-time.** The rejection samplers and norm checks follow the reference
  (non-constant-time) style; side-channel hardening is future work.
- **Second exotic scheme.** The "best" success tier (a second PQ exotic signature,
  e.g. a ring or threshold variant) remains open.

## 10. Build and run
```
cd ref
make test/test_las3   && ./test/test_las3     # functional tests
make test/test_swap3  && ./test/test_swap3    # narrated atomic swap + asserts
make test/bench_las3  && ./test/bench_las3    # per-operation timings
```
All three are mode-independent; `-DDILITHIUM_MODE=2/5` behave identically.

## 11. References
1. M. F. Esgin, O. Ersoy, Z. Erkin. *Post-Quantum Adaptor Signatures and Payment
   Channel Networks*. ESORICS 2020 / IACR eprint 2020/845.
2. L. Ducas et al. *CRYSTALS-Dilithium* (ML-DSA / FIPS 204). Reference C
   implementation reused here.
3. A. Erwig et al. / poqeth. *Integration template for PQ scriptless scripts*. IACR
   eprint 2025/091.
4. M. Ajtai. *Generating hard instances of lattice problems*. STOC 1996 (`f_A`).
