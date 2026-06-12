# LAS — A Lattice-Based Adaptor Signature on Dilithium Primitives

*Design, implementation, correctness, testing, application and benchmarks.*

This document is the technical reference for the implementation in `ref/las.{c,h}`,
`ref/amhl.{c,h}`, `ref/chain.{c,h}`, `ref/serialize.{c,h}`, and the
tests/benchmarks under `ref/test/` (`test_las.c`, `test_swap.c`, `test_pcn.c`,
`test_amhl.c`, `test_serde.c`, `test_kat.c`, `bench_las.c`, `bench_compare.c`,
`bench_app.c`). It is written to be the source material for the dissertation
chapter; section numbering maps roughly onto report sections.

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

### 1.1 Related work and scheme selection

**Why LAS and not IAS?** Two post-quantum adaptor signatures were available:

- **LAS** (eprint 2020/845, ESORICS 2020, Esgin–Ersoy–Erkin) — lattice-based,
  built directly on Dilithium. This is what we implement.
- **IAS** (eprint 2020/1345, Tairi–Moreno-Sanchez–Maffei) — isogeny-based, built
  on CSI-FiSh/CSIDH.

LAS was chosen for three reasons, each warranting a sentence in the Methodology:

1. **Implementation leverage.** LAS extends CRYSTALS-Dilithium, whose reference C
   implementation is the starting point of this project. The four LAS functions
   (`PreSign`, `PreVerify`, `Adapt`, `Ext`) are additions to, not replacements of,
   the base Dilithium scheme; all polynomial arithmetic, NTT, SHAKE/Keccak and
   sampling code is reused directly.

2. **Security assumptions.** LAS is based on Module-SIS and Module-LWE, the same
   problems underlying Dilithium and the NIST standard ML-DSA. These assumptions
   are mature and well-studied. IAS is based on CSIDH-512, which offers only
   approximately 60-bit quantum security — below NIST's 128-bit threshold — and
   requires isogeny-group-action arithmetic that is far harder to implement.

3. **Survey recommendation.** A 2022 survey of post-quantum exotic signatures
   (eprint 2022/1151) explicitly recommends LAS over IAS unless on-chain signature
   size is the primary concern, and calls LAS "an acceptable solution for
   post-quantum blockchain." IAS achieves smaller signatures (~50B vs our 4672B
   packed), but at the cost of the weaker security level.

**The "knowledge gap."** LAS (and all lattice adaptor signatures) carry a caveat
not present in classical schemes: the extracted witness `y = z − ẑ` in this
implementation is exact, but in the general lattice setting the extraction can
carry bounded noise that *accumulates* across long payment-channel paths (the
"knowledge gap" identified in eprint 2022/1151). For a K-hop path the extraction
guarantee degrades unless PreSign uses the tighter bound `γ−κ−K` per hop (rather
than `γ−κ−1`). Both the single-hop case (Sections 4, 7) **and** the full K-hop
Adaptor Multi-Hop Lock (AMHL) construction from LAS Fig. 2 are now implemented
(Section 7.5): `las_presign_k` enforces the `γ−κ−K` bound, each hop carries a
distinct cumulative statement, and the per-hop witness-norm growth `‖s_j‖∞ ≤ j`
— the concrete face of the knowledge gap — is exhibited directly in the demo.

**poqeth context.** The integration template eprint 2025/091 (poqeth, Erwig et al.)
put *basic* PQ signatures on Ethereum. Our project extends the same idea to an
*exotic* PQ signature, demonstrating that the gap between "basic PQ on a blockchain"
and "exotic PQ on a blockchain" can be bridged with a modest code addition.

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
  **Generalisation to K hops:** if the adapted witness is a *sum* of up to `K`
  ternary vectors (`‖y_w‖∞ ≤ K`, as in the multi-hop construction of Section 7.5),
  PreSign must instead accept only `‖ẑ‖∞ ≤ γ − κ − K`, giving
  `‖z‖∞ ≤ (γ − κ − K) + K = γ − κ` again. This is exactly the paper's `γ−κ−K`
  bound, implemented as `las_presign_k(…, K)` with the macro `LAS_BOUND_PRESIGN_K`.
  Setting `K = 1` recovers the single-hop case verbatim.

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

### 5.10 Serialisation and the on-chain verifier interface (`ref/serialize.{c,h}`)

The scheme functions operate on in-memory `poly` structs, but any realistic
deployment — and specifically an on-chain verifier in the style of poqeth
(eprint 2025/091) — exchanges objects as **byte strings**. `serialize.{c,h}` adds
the canonical wire/on-chain encoding plus a *validating* decoder, leaving the
scheme code (`las.c`) untouched (clean separation):

- **Encoding** (LSB-first bit packing): pk/statement `Y` at 23 bits/coeff
  (`Q < 2^23`); sk/witness at 2 bits/coeff (ternary); signature `(c, z)` as a
  2-bit ternary `c` plus an 18-bit offset-encoded `z`. Sizes:
  `LAS_PK_BYTES = 2944`, `LAS_SK_BYTES = 512`, `LAS_SIG_BYTES = 4672`.
- **Defensive decoding.** A verifier cannot trust its input, so `unpack`
  *rejects* malformed bytes: a pk coefficient `≥ Q`, the invalid ternary code `3`,
  or a `z` field outside the 18-bit band. `pack` symmetrically rejects
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

- `las_keygen_seed(pk, sk, pp, seed)` derives the secret directly from a 32-byte
  seed (KeyGen from explicit randomness);
- `las_sign_det` and `las_presign_det` derive the per-signature mask seed as
  `SHAKE256(tag ‖ sk ‖ [Y] ‖ M)` instead of drawing fresh randomness, so the
  output is a *pure function* of the inputs.

Internally `las_sign`/`las_sign_det` share one `sign_core`, and
`las_presign`/`las_presign_k`/`las_presign_det` share one `presign_core`, differing
only in (a) where the 64-byte mask seed comes from (fresh `randombytes` vs the
derivation above) and (b) the rejection bound — so the deterministic and randomised
paths are guaranteed identical in distribution and validity. Beyond reproducibility,
deterministic signing also removes the per-signature RNG dependency and the
nonce-reuse failure mode that has repeatedly broken classical (EC)DSA deployments —
a desirable property in a blockchain setting.

## 6. Testing

### 6.1 Functional tests (`ref/test/test_las.c`)
Per iteration (1000 iterations — the objectives' B1 acceptance bar of ≥1000 runs
at 100 % correctness — modes 2/3/5, random `pp`, keys, message):
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

### 6.3 Serialisation tests (`ref/test/test_serde.c`)
A separate suite exercises the byte-level encoding of Section 5.10 over 256 random
instances, hard-asserting: (i) **round-trip** `unpack(pack(x)) == x` for pk, sk, and
the ordinary / pre / adapted signatures; (ii) **verify-from-bytes** — a packed
`(pk, adapted σ)` verifies via `las_verify_packed`, while a packed *pre-signature*
is rejected (the tripwire survives serialisation); (iii) **tamper** — every one of
the `LAS_SIG_BYTES = 4672` single-byte flips of a valid packed signature makes it
fail verification; (iv) **validation** — `pack` and `unpack` both reject
out-of-range inputs (coefficient `≥ Q`, non-ternary code, `z` outside the band).
All pass, zero warnings.

### 6.4 Known-answer tests (`ref/test/test_kat.c`)
Reproducibility (objective C4) is verified by a KAT suite that fixes *all* inputs
(public-parameter seed, key seeds, statement seeds, messages) and uses the
deterministic API of Section 5.11. For `NVEC = 4` vectors it runs the full
deterministic pipeline (`keygen_seed → sign_det / presign_det → adapt → ext`),
hard-asserts the adaptor contract and that re-running the deterministic functions
yields byte-identical output, then folds the packed bytes of every object
(`pk, sk, σ, σ̂, σ_adapted`) into a single SHAKE256 digest and checks it against a
**pinned 32-byte expected value**. That one fingerprint locks down the entire
implementation: any unintended change to keygen, signing, the adaptor algebra, or
the serialisation flips the digest. Because every step is integer/SHAKE arithmetic
over a fixed canonical byte encoding, the digest is stable across machines and
compilers, and an independent verifier (Solidity/circuit) can be checked against
the same vectors. The digest is reproduced on every run; the test passes.

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

### 7.4 A scriptless-script ledger (`ref/chain.{c,h}`, `ref/test/test_pcn.c`)

To show LAS in a setting closer to a real blockchain, `chain.{c,h}` provide a small
ledger abstraction: accounts with balances, a block height, and *adaptor-locked
contracts* — the scriptless analogue of a Hash-Time-Locked Contract (HTLC):

- the **hash lock** is replaced by an adaptor statement `Y` — claiming requires the
  witness `y`;
- the **time lock** is a timeout block height — the funder may refund after it.

The chain stores only public data (public keys, statements, (pre-)signatures); the
secret keys and the `PreSign`/`Adapt` steps live in the parties' wallets, exactly as
on a real chain. `chain_fund_swap` *pre-verifies* the funder's pre-signature before
escrowing funds; `chain_claim_swap` *verifies* the adapted signature, pays the
beneficiary, and records it on-chain (revealing `y` to any watcher); `chain_refund_swap`
is gated on `height ≥ timeout`; `chain_extract_witness` runs `las_ext` on a claimed
contract.

**Important model note:** Scenarios 1–3 use the *same-Y* model — one shared
statement `Y` locks all hops. This is a correct scriptless HTLC but not the paper's
AMHL (Adaptor Multi-Hop Lock). In the same-Y model, observing `y` from any claimed
hop lets a party adapt *all* other hops locked to `Y` — a wormhole-style weakness on
longer paths. The paper's AMHL assigns each hop a *different* cumulative statement
`Y_j = A·(l_1 + … + l_j)` and uses PreSign bound `γ−κ−K` for path length K. **AMHL
is now implemented** (`ref/amhl.{c,h}`, `ref/test/test_amhl.c`) and described in
Section 7.5; the same-Y demo below is retained as the simpler baseline that
isolates the core adaptor mechanism.

`test_pcn.c` runs three hard-asserted scenarios (all pass):

1. **Cross-chain atomic swap (happy path).** Alice locks 10 on chain A to Bob; Bob
   locks 10 on chain B to Alice, both to the same `Y`. Bob claims on A (revealing
   `y`); Alice extracts `y` and claims on B. Both legs settle.
2. **Timeout / refund (unhappy path).** No one claims. Refund *before* timeout is
   rejected; after advancing block height both parties refund — no coins lost. Legs
   use laddered timeouts so the second claimant always has a safety window.
3. **Multi-hop payment (same-Y PCN).** Carol issues invoice `(Y, y)`. Alice pays Bob
   (11, outer hop) and Bob pays Carol (10, inner hop), both locked to `Y`. Carol
   pulls the inner hop with `y` (revealing it); Bob extracts `y` and pulls the outer
   hop, earning his routing fee. Final: Alice=89, Bob=101, Carol=10.

This is a **working** post-quantum scriptless swap and payment-channel
demonstration. The same-Y model is sufficient to prove the adaptor mechanism is
functional end-to-end and is the clearest baseline for isolating it; the
multi-hop-safe AMHL construction that removes its wormhole weakness is the
headline multi-hop artefact and is described next in Section 7.5.

### 7.5 Adaptor Multi-Hop Locks — AMHL (`ref/amhl.{c,h}`, `ref/test/test_amhl.c`)

The same-Y ledger of Section 7.4 proves the adaptor mechanism works, but it is
*not* the paper's payment-channel-network construction. AMHL (LAS Fig. 2 / §5) is,
and it is the headline multi-hop artefact of this project. It removes the
same-Y wormhole weakness by giving every hop on a route a **distinct** statement.

#### 7.5.1 Construction
For a K-hop route `U_0 → U_1 → … → U_K` (sender `U_0`, receiver `U_K`):

```
Setup (run by the sender U_0, who knows the whole route):
    sample increments   l_1, …, l_K  ← S_1          (ternary, ‖l_j‖∞ ≤ 1)
    cumulative witness   s_0 = 0,  s_j = s_{j-1} + l_j = l_1 + … + l_j
    cumulative statement Y_0 = 0,  Y_j = A·s_j       (so Y_j = Y_{j-1} + A·l_j)
    Hop j (payer U_{j-1} → payee U_j) is locked to Y_j and opened with s_j.

Secret distribution (least-privilege):
    receiver U_K      ← s_K                 (the full witness, i.e. the "invoice")
    intermediary U_j  ← l_{j+1}  only        (1 ≤ j < K)

Pre-signing: every hop uses  las_presign_k(…, K)  → bound γ−κ−K.

Cascade (claims flow right→left, receiver first):
    U_K  Adapts hop K with s_K and publishes σ_K           ⇒ s_K becomes public
    U_{j-1} extracts s_j from hop j, computes s_{j-1}=s_j−l_{j+1}, Adapts hop j−1
    … down to hop 1, which the sender's first counterparty U_1 finally pulls.
```

The cumulative statements are built additively from the increment key pairs
(`Y_j = Y_{j-1} + A·l_j`), reusing `las_keygen` to produce each `(A·l_j, l_j)` —
no new lattice arithmetic. Adapt and Ext are **unchanged** from Section 4: Adapt
adds the cumulative witness `s_j`, and Ext returns exactly `s_j` (it satisfies
`A·s_j = Y_j`).

#### 7.5.2 Why each property holds
- **Distinct statements ⇒ no wormhole.** Because `Y_i ≠ Y_j` for `i ≠ j`, learning
  the opener `s_j` of one hop reveals nothing usable about a non-adjacent hop.
  The demo asserts the converse directly: adapting hop 1 with the receiver's secret
  `s_K` yields a signature that ordinary `Verify` **rejects** (it would force
  `A·z−c·t = w + Y_K ≠ w + Y_1`, a Fiat–Shamir mismatch). Only the adjacent
  increment `l_{j+1}` bridges hop `j+1` to hop `j`.
- **Witness-norm growth (the knowledge gap, made concrete).** `s_j` is a sum of `j`
  ternary vectors, so `‖s_j‖∞ ≤ j ≤ K`. The demo prints this growth exactly
  (`‖s_1‖∞ = 1, …, ‖s_4‖∞ = 4`). This is *why* every hop must pre-sign at the
  tighter bound `γ−κ−K`: the adapted response `z = ẑ + s_j` then still satisfies
  `‖z‖∞ ≤ (γ−κ−K) + j ≤ γ−κ` and clears ordinary `Verify`.
- **Exact recovery.** At each hop the extractor recovers `s_j` exactly, and the
  intermediary recomputes `s_{j-1} = s_j − l_{j+1}` exactly (asserted equal to the
  setup value). Extraction is noise-free in this parameterisation; the residual
  *relaxed-relation* knowledge gap is discussed in Section 9.
- **Timeouts/refund.** Hops carry laddered timeouts (outer hops expire last, so
  every puller retains a safety window). The refund scenario advances the block
  height past every timeout and refunds each hop to its funder — no coins lost.

#### 7.5.3 The K-hop bound in practice
The tightening from `γ−κ−1` to `γ−κ−K` is *cryptographically essential* but has
**negligible performance cost**: with `γ = 122880` and `K ≤ 8`, the accepted band
shrinks by at most `K/(γ−κ) ≈ 0.007 %`, so the rejection-sampling acceptance rate
(Section 8) is indistinguishable from the single-hop case. AMHL therefore adds no
per-hop *signing* penalty beyond the obvious linear "K hops ⇒ K pre-signatures."

#### 7.5.4 Demo output (mode 3)
`test_amhl3` runs two hard-asserted scenarios; abridged transcript:

```
== Scenario 1: AMHL 4-hop routed payment (happy path) ==
    hop 1: Y_1 distinct=yes   ‖s_1‖inf = 1  (<= 1)
    hop 2: Y_2 distinct=yes   ‖s_2‖inf = 2  (<= 2)
    hop 3: Y_3 distinct=yes   ‖s_3‖inf = 3  (<= 3)
    hop 4: Y_4 distinct=yes   ‖s_4‖inf = 4  (<= 4)
  funded hops: Alice→Bob(13) Bob→Carol(12) Carol→Dave(11) Dave→Erin(10)
  wormhole check: s_4 cannot open hop 1 (distinct statement)  -> OK
  cascade: Erin→Dave→Carol→Bob pull right-to-left, exact recovery each hop
  balances: Alice=87 Bob=101 Carol=101 Dave=101 Erin=10   (payment + per-hop fee)
== Scenario 2: AMHL route times out (refund path) ==
  refund before timeout rejected; after timeouts every hop refunds  -> safe OK
```

Alice pays 10 to Erin across a 4-hop route; each of the three intermediaries earns
a 1-unit routing fee, and the conservation `87+101+101+101+10 = 400` holds. This is
the genuinely novel part of LAS exercised end-to-end — most adaptor-signature demos
stop at a single swap.

## 8. Performance (measured)

Wall-clock microseconds per operation, mode 3, 2000 iterations/op, `-O3`,
measured by `ref/test/bench_las3`. Absolute numbers are machine-dependent; the
*ratios* are the point.

| Operation | Time (µs) | Note |
|---|---:|---|
| Setup (expand `A`) | 58 | `n·ℓ = 16` uniform polys via SHAKE128 |
| KeyGen / statement gen | 78 | sample `r` (ternary), compute `A·r`; *same cost* for `(Y,y)` |
| Sign | 804 | ~2.7 attempts/signature (≈37% acceptance, see below) |
| Verify | 191 | one `A·z − c·t` + hash |
| PreSign | 828 | ≈ Sign; `H(pk, w+Y, M)` vs `H(pk, w, M)` is negligible |
| PreVerify | 197 | ≈ Verify; one extra `+Y` add |
| Adapt | 203 | PreVerify + 8 poly adds |
| Ext | 68 | one `A·s` + compare |

**Rejection-sampling acceptance rate (measured *directly*, bench_las3).**
`las_sign`/`las_presign`/`las_presign_k` increment a global `las_attempts`
counter once per rejection-loop iteration (instrumentation only — never read by
the scheme), so the benchmark reports the **exact** average attempts per
signature over 2000 calls rather than estimating it. Measured:

| | attempts/sig | acceptance | retries |
|---|---:|---:|---:|
| Sign | 2.71 | 36.9 % | 1.71 |
| PreSign | 2.77 | 36.1 % | 1.77 |

This matches the closed-form prediction. One attempt is accepted iff all
`(n+ℓ)·N = 2048` response coefficients land within `±(γ−κ)`, so the per-attempt
acceptance is `≈ (1 − κ/γ)^{(n+ℓ)·N} = (1 − 60/122880)^{2048} ≈ 36.8 %`
(`≈ e^{-1}`), i.e. `≈ 2.72` attempts/signature — within noise of the measured
numbers. This is expected and correct for a Fiat–Shamir-with-aborts scheme:
rejection sampling is intrinsic to the family. A subtle point worth stating
precisely, because it is easy to get backwards: omitting the hint vector does
**not** worsen our per-attempt acceptance. Optimised Dilithium rejects on *several*
conditions each attempt — the `‖z‖∞` bound **plus** a low-order-bits check on
`w − c·s₂` **plus** a hint-count limit — whereas this simplified scheme rejects on
the single `‖z‖∞` bound. Additional conditions can only lower acceptance, so the
hint-free design carries no inherent acceptance penalty; optimised Dilithium's own
expected signing repetitions are a small single-digit count (see the Dilithium
specification), i.e. comparable to our ≈2.7 attempts — not the >5× advantage that an
">80% with hints" claim would imply. (That earlier figure was not merely
unmeasured here but directionally wrong.) The `γ = κ·d·(n+ℓ) = 122880` choice
governs the MSIS hardness parameter, not the acceptance rate.

> **Correction (methodology note worth keeping in the report).** An earlier
> version of `bench_las` *estimated* retries from the timing ratio
> `t_sign / t_verify` and reported ~23 % acceptance (~4.3 attempts). That
> estimator is **biased**: one Sign attempt does `n+ℓ = 8` `c·r` products plus
> `A·y`, whereas one Verify does only `n = 4` `c·t` products plus `A·z`, so a Sign
> attempt is dearer than a Verify and the ratio over-counts attempts. The direct
> counter (~37 %, ~2.7 attempts) supersedes it and agrees with the `e^{-1}`
> theory line — a small but honest example of preferring direct measurement to a
> proxy.

**Object sizes (three distinct numbers — do not confuse them):**

| Object | In-memory `sizeof` | Packed (measured, `serialize.c`) | Paper's estimate |
|---|---:|---:|---|
| pk / statement Y | 4096 B | 2944 B | — |
| sk / witness y | 8192 B | 512 B | — |
| sig / pre-sig | 9216 B | 4672 B | ~3210 B |

- *In-memory:* `sizeof` counts full `int32_t` per coefficient.
- *Packed (measured):* these are the **actual** sizes emitted by `ref/serialize.c`
  (`LAS_PK_BYTES`, `LAS_SK_BYTES`, `LAS_SIG_BYTES`), validated by `test_serde`, not
  formulas. pk: 23 bits/coeff (`Q < 2^23`) → `4·256·23/8 = 2944 B`. sk: ternary at
  2 bits/coeff → `8·256·2/8 = 512 B`. sig: challenge `c` packed as a 2-bit ternary
  polynomial (`256·2/8 = 64 B`) + response `z` at 18 bits/coeff
  (`8·256·18/8 = 4608 B`) = **4672 B**. (`z` needs 18 bits because the centred range
  `2·(γ−κ)+1 = 245641 < 2^18`; packing `c` as ternary is 4 B smaller than the
  position-encoded 68 B and simpler to validate.)
- *Paper's ~3210 B:* the paper's *optimised* scheme at `q ≈ 2^24` with a hint
  vector and high/low-bit decomposition. Not comparable to this implementation.
  The correct comparison for our scheme is the "Packed (measured)" column.

**Takeaways for the report.** (i) `PreSign ≈ Sign`, `PreVerify ≈ Verify` — the
adaptor operations add negligible overhead over the base scheme, matching the
paper's efficiency claim. (ii) Sizes are large in-memory only; the *measured*
packed sig (4672 B) is not dramatically larger than optimised Dilithium-3
(3309 B bit-packed), and bit-packing is now implemented, not just estimated.
(iii) The sign rejection rate (~2.7 attempts, ≈37 % acceptance) is the honest cost
of the simplified, hint-free scheme — not a bug, and it matches the `e^{-1}` theory.

### 8.1 Head-to-head vs. optimised Dilithium-3

`bench_compare` (`ref/test/bench_compare.c`) times LAS against the NIST-optimised
Dilithium-3 reference on the same machine. **This is not a same-security
comparison**: Dilithium-3 uses larger module dimensions (K=6, L=5), hints,
high/low-bit decomposition and bit-packing, at the standard modulus; LAS here is the
simplified adaptor scheme (n=ℓ=4, no hints, unpacked objects, q≈2^23). It is an
order-of-magnitude feasibility check plus the adaptor-overhead measurement.

| Operation | Dilithium-3 (µs) | LAS (µs) |
|---|---:|---:|
| KeyGen | 162 | 41 |
| Sign | 642 | 440 |
| Verify | 155 | 104 |
| PreSign | — | 458 |
| PreVerify | — | 106 |
| Adapt | — | 108 |
| Ext | — | 33 |

| Object | Dilithium-3 (B) | LAS (B) |
|---|---:|---:|
| public key / statement | 1952 | 4096 |
| secret key / witness | 4032 | 8192 |
| signature / pre-signature | 3309 | 9216 |

Reading this honestly for the report:
- **Speed.** LAS is faster per operation here, but that is *expected, not a win*: it
  has smaller module dimensions, skips hint generation/decomposition, and sits at a
  lower security margin. The takeaway is *feasibility* — LAS operations are in the
  same hundreds-of-microseconds regime as a deployed PQ signature — not superiority.
- **Size.** LAS objects are ~2–3× larger only because they are stored as full
  `int32` coefficients while Dilithium-3 bit-packs. Applying the same packing to LAS
  would close most of the gap and is the obvious next optimisation.
- **Adaptor overhead (the real result).** `PreSign ≈ Sign`, `PreVerify ≈ Verify`,
  `Adapt ≈ Verify`, and `Ext` is the cheapest operation — so adding adaptor
  capability costs essentially nothing over the base scheme, matching eprint
  2020/845's headline claim.

### 8.2 Application-level benchmark (`ref/test/bench_app.c`)

Section 8 / 8.1 measure the **signature** dimension. Wang explicitly asked for
*two* benchmark types — the signature itself **and** the application. `bench_app3`
supplies the second: the communication and settlement-payload cost of the two LAS
workflows. Sizes are the **actual serialised sizes** produced by `serialize.c`
(`LAS_PK_BYTES`, `LAS_SIG_BYTES`; Sections 5.10 and 8) — not formulas; restart
counts are measured directly via `las_attempts`.

> **Simulated ledger (scope).** `bench_app` runs against a *simulated* ledger, not a
> deployed Ethereum or Bitcoin contract. All byte figures below are an
> **application-level payload / settlement-footprint proxy**, not measured gas;
> per the project scope, (pre-)signature and statement sizes stand in for
> transaction cost. Real-chain gas measurement is out of scope (Section 9).

**(1) Atomic cross-chain swap (2 parties, 2 chains, no scripts).**

| Phase | Object(s) | Bytes |
|---|---|---:|
| Off-chain (3 messages) | `Y` + `σ̂_A` + `σ̂_B` | 2944 + 4672 + 4672 = **12 288 B** |
| Settlement footprint (proxy) | `σ_A`, `σ_B` published | 2 × 4672 = **9 344 B** |
| Settlement footprint incl. escrowed `Y` | + 2 × `Y` | 15 232 B |

Only the two *adapted* signatures would be published on a real chain; each is a
single ordinary-looking LAS signature. End-to-end signing work (2× PreSign + 2×
Adapt + Ext) is a few milliseconds (a single un-averaged sample, dominated by the
two rejection-sampled pre-signs, so it varies run to run). The harness re-asserts
the fairness invariant (adapted sigs verify, pre-sigs do not).

**(2) Multi-hop AMHL payment — cost as a function of path length K** (40 routes per
K, mode 3; `attempts/presig` measured directly):

| K | bound `γ−κ−K` | #pre-sigs | attempts/presig | presig time (ms) | settlement sigs | public statements | max `‖s_j‖∞` |
|--:|--:|--:|--:|--:|--:|--:|--:|
| 1 | 122820 | 1 | 2.60 | 0.74 | 4 672 B | 2 944 B | 1 |
| 2 | 122819 | 2 | 2.30 | 1.29 | 9 344 B | 5 888 B | 2 |
| 4 | 122817 | 4 | 2.73 | 3.01 | 18 688 B | 11 776 B | 4 |
| 6 | 122815 | 6 | 2.95 | 4.97 | 28 032 B | 17 664 B | 6 |
| 8 | 122813 | 8 | 2.91 | 6.61 | 37 376 B | 23 552 B | 7 |

*(Representative run; the byte columns are exact — they are the `serialize.c`
sizes `K·LAS_SIG_BYTES` and `K·LAS_PK_BYTES` — but `attempts/presig`, `presig
time` and the realised `max‖s_j‖∞` (≤ K) are random/machine-dependent and vary
between runs.)*

Three findings for the report:
1. **Settlement footprint is linear in K** — `K` adapted signatures + `K` public
   statements (payload proxy, not gas); no super-linear blow-up.
2. **Witness norm grows with the hop index, `‖s_j‖∞ ≤ j ≤ K`** (each `s_j` is a sum
   of `j` ternary vectors; the realised maximum is at or just below `K`, e.g. 7–8
   at K=8 across runs), the "knowledge gap" made concrete and the precise reason
   every hop pre-signs at `γ−κ−K`.
3. **The `γ−κ−K` tightening is performance-negligible.** Going `K = 1 → 8` shrinks
   the accept band by `7/(γ−κ) ≈ 0.0057 %`, so `attempts/presig` is flat in `K`
   (≈2.7–3.0, the variation is sampling noise). AMHL therefore adds **no per-hop
   signing penalty** beyond the unavoidable "K hops ⇒ K pre-signatures." This is a
   genuine, slightly counter-intuitive result: the bound change that makes
   multi-hop *correct* costs essentially nothing in *speed*.

### 8.3 Classical adaptor baseline — "the price of post-quantum" (`ref/test/bench_classical.c`)

Meeting 2 added a second required baseline (objective B2.ii): LAS vs a
**classical adaptor signature**. We use the **ECDSA-based adaptor** from
`libsecp256k1-zkp` (BlockstreamResearch's fork of Bitcoin Core's libsecp256k1;
module `ecdsa_adaptor`, production code used in Discreet Log Contracts), vendored
at commit `95b9835` and benchmarked **on the same machine and compiler** as every
LAS number in this document — so the comparison needs no hardware caveats. Per the
supervisor's guidance the implementation is *reused as-is*; only the timing
harness (which mirrors the LAS operation set one-to-one) is ours. Reproduce via
`README_LAS.md` (one-time clone + `make test/bench_classical`).

**The 2×2 timing matrix (µs/op, 2000 iters, same machine):**

| Operation | ECDSA (classical basic) | ECDSA-adaptor (classical exotic) | Dilithium-3 (PQ basic) | LAS (PQ exotic) |
|---|---:|---:|---:|---:|
| KeyGen / statement gen | 31 | 31 | 162 | 78 |
| Sign | 41 | — | 642 | 804 |
| Verify | 62 | — | 155 | 191 |
| PreSign | — | 189 | — | 828 |
| PreVerify | — | 244 | — | 197 |
| Adapt | — | 3 | — | 203 |
| Ext | — | 35 | — | 68 |

**Sizes (B):**

| Object | ECDSA(-adaptor) | LAS (packed, measured) | ratio |
|---|---:|---:|---:|
| public key / statement | 33 | 2944 | ×89 |
| secret key / witness | 32 | 512 | ×16 |
| signature | 64 (70 DER) | 4672 | ×73 |
| pre-signature | 162 | 4672 | ×29 |

**Reading the data (the report's "let the data speak" paragraph):**

1. **The price of post-quantum is overwhelmingly *communication*, not
   computation.** Sizes grow ×29–×89; per-operation times grow far less (Verify
   ×3.1, PreSign ×4.4, Sign ×19.5), and everything stays in the
   sub-millisecond regime on commodity hardware. For blockchain use the size
   column is the binding constraint (on-chain bytes), which is exactly the
   motivation for the packing of Section 5.10.
2. **The adaptor *overhead structure* is inverted — LAS's headline win.** In the
   classical scheme the adaptor functionality is expensive *relative to its own
   base*: PreSign costs 4.6× Sign and PreVerify 3.9× Verify, because the
   pre-signature must carry and check a DLEQ proof. In LAS, PreSign ≈ Sign and
   PreVerify ≈ Verify (×1.03): the statement folds into the Fiat–Shamir hash for
   free. Strikingly, **LAS PreVerify (197µs) is absolutely faster than classical
   ECDSA-adaptor PreVerify (244µs)** on the same machine.
3. **Structural contrast worth a paragraph:** the classical pre-signature is a
   *syntactically different object* (162 B = ECDSA sig + DLEQ proof) that cannot
   even be parsed as a signature, whereas a LAS pre-signature shares the
   signature format and fails ordinary Verify *cryptographically* (the `+Y`
   tripwire, Section 4.2). LAS's adapted signature is indistinguishable from an
   ordinary one; the classical adapted signature is too, but its pre-signature
   pipeline needs a second verifier implementation on the wire.

**Honest caveats (state in the report):** libsecp256k1 is constant-time, heavily
optimised production code, while our LAS is a reference-style simplified scheme —
the timing comparison therefore *flatters the classical side*; LAS additionally
sits at a reduced security margin (`q ≈ 2²³`, Section 5.9). Neither caveat
affects the size ratios, which are format-determined. And the entire classical
column is broken by Shor's algorithm — that asymmetry is the thesis.

## 9. Limitations and future work

- **AMHL (multi-hop, K-hop bound).** ✅ **Implemented** (Section 7.5,
  `ref/amhl.{c,h}`, `ref/test/test_amhl.c`). Each hop carries a distinct cumulative
  statement `Y_j = A·(l_1+…+l_j)`, pre-signing uses the `γ−κ−K` bound via
  `las_presign_k`, and the demo asserts wormhole resistance, the witness-norm
  growth `‖s_j‖∞ ≤ j`, exact per-hop recovery, and a timeout/refund path. The
  same-Y scriptless HTLC (Section 7.4) is retained as the simpler baseline. A
  remaining nicety is a *privacy*-preserving variant (per the IAS critique,
  Section 1.1) and randomised (non-cumulative-sum) lock setups.

- **Knowledge gap.** The extracted witness norm grows with path length: a K-hop
  intermediate witness has `‖s_j‖∞ ≤ j` (a sum of up to K ternary vectors), now
  exhibited concretely by `test_amhl` (`‖s_1‖∞=1 … ‖s_4‖∞=4`). In *this*
  parameterisation extraction is still **exact** (the cumulative witness is an
  integer vector recovered without error). The deeper limitation is in the paper's
  *relaxed* relation, where extraction may carry bounded noise that accumulates
  across long chains — acknowledged in the survey (eprint 2022/1151) as a
  fundamental gap of lattice adaptor signatures vs. classical ones. Analysing that
  relaxed-relation noise growth is out of scope for this project.

- **Modulus.** `Q ≈ 2^23` rather than the paper's `2^24` (Section 5.9). Correctness
  holds; only the MSIS/MLWE security margin differs.

- **Signature packing.** ✅ **Implemented** (`ref/serialize.{c,h}`, Section 5.10):
  bit-packed wire/on-chain encoding with a validating decoder and the
  `las_verify_packed` byte-level verifier, giving a measured packed signature of
  4672 B (vs 9216 B in-memory). The residual gap to optimised Dilithium-3 (3309 B)
  is the modulus (`2^23` vs `2^24`) and the hint/decomposition compression of the
  optimised scheme — out of scope here.

- **Reproducibility / KATs.** ✅ **Implemented** (`ref/test/test_kat.c`, Sections
  5.11 and 6.4): a deterministic API (`las_keygen_seed`, `las_sign_det`,
  `las_presign_det`) plus a pinned SHAKE256 known-answer digest over fixed vectors.
  This satisfies objective C4's reproducibility requirement and provides the test
  vectors a future on-chain verifier would be cross-checked against. (NIST-style
  DRBG-seeded KAT files, if a marker wants the exact `PQCgenKAT` format, would be a
  cosmetic add-on.)

- **Rejection rate.** ≈37% acceptance per attempt (~2.7 attempts/sig), measured
  directly and matching the `e^{-1}` theory (Section 8). Rejection sampling is
  intrinsic to Fiat–Shamir-with-aborts; optimised Dilithium uses a hint vector that
  we omit deliberately for transparent algebra. Re-introducing hints without
  breaking the adaptor algebra is non-trivial and is future work.

- **Constant-time.** Rejection samplers and norm checks follow the reference
  (non-constant-time) style; side-channel hardening is future work.

- **Second exotic scheme.** The "best" success tier (a second PQ exotic signature,
  e.g. a PQ ring or threshold variant) remains open.

## 10. Build and run
```
cd ref
make test/test_las3   && ./test/test_las3     # functional tests
make test/test_swap3  && ./test/test_swap3    # narrated atomic swap + asserts
make test/test_pcn3   && ./test/test_pcn3     # scriptless HTLCs: swap / refund / same-Y PCN
make test/test_amhl3  && ./test/test_amhl3    # AMHL: K-hop route, wormhole + norm-growth + refund
make test/test_serde3 && ./test/test_serde3   # serialisation: round-trip / verify-from-bytes / tamper
make test/test_kat3   && ./test/test_kat3     # deterministic known-answer test (reproducibility)
make test/bench_las3  && ./test/bench_las3    # per-operation timings + direct rejection rate
make test/bench_compare3 && ./test/bench_compare3  # LAS vs optimised Dilithium-3
make test/bench_app3  && ./test/bench_app3    # application cost: swap + AMHL-vs-K
```
All are mode-independent; `-DDILITHIUM_MODE=2/5` behave identically.

## 11. References
1. M. F. Esgin, O. Ersoy, Z. Erkin. *Post-Quantum Adaptor Signatures and Payment
   Channel Networks*. ESORICS 2020 / IACR eprint 2020/845.
2. L. Ducas et al. *CRYSTALS-Dilithium* (ML-DSA / FIPS 204). Reference C
   implementation reused here.
3. A. Erwig et al. / poqeth. *Integration template for PQ scriptless scripts*. IACR
   eprint 2025/091.
4. M. Ajtai. *Generating hard instances of lattice problems*. STOC 1996 (`f_A`).
