# A Post-Quantum Adaptor Signature on CRYSTALS-Dilithium: Implementation, Blockchain Integration, and Evaluation

*MSc dissertation — working draft v0.1 (2026-06-12). Word budget: main body ≈8,000
words (references, appendices and captions excluded). Figures are ASCII stubs to be
redrawn for submission. All measurements were produced by the artefact in this
repository on one machine; reproduction commands are in Appendix D.*

---

## Abstract

Blockchains authorise transactions almost exclusively with ECDSA and Schnorr
signatures, both of which are broken by a cryptographically relevant quantum
computer running Shor's algorithm. NIST has standardised replacement *basic*
post-quantum signatures, but the *exotic* signature functionality that modern
blockchain protocols depend on — and in particular **adaptor signatures**, the
primitive behind scriptless atomic swaps and payment-channel networks — has
remained essentially paper-only in the post-quantum setting. This project
presents, to our knowledge, the first public implementation of LAS
(Esgin–Ersoy–Erkin, ESORICS 2020), the first lattice-based adaptor signature,
built directly on the CRYSTALS-Dilithium reference implementation with **zero
upstream functions modified**. The implementation covers the full adaptor
contract (PreSign/PreVerify/Adapt/Ext), byte-level serialisation with a
validating verifier, deterministic known-answer tests, a scriptless atomic-swap
and timeout-refund ledger demonstration, and a bonus multi-hop lock (AMHL)
construction with the paper's `γ−κ−K` bound. Evaluation against **two
same-machine baselines** — basic Dilithium-3 and a production classical
ECDSA-adaptor (libsecp256k1-zkp) — shows that the price of post-quantum adaptor
signatures is overwhelmingly *communication* (×29–89 larger objects; 4,672-byte
packed signatures) rather than *computation* (all operations remain sub-millisecond).
Notably, the adaptor overhead structure inverts: classical pre-signing costs ≈4.6×
its own base signing due to a DLEQ proof, whereas LAS pre-signing costs ≈1.03× —
and LAS pre-verification is absolutely faster than its classical counterpart on the
same machine. Correctness holds in 100% of 1,000 randomised end-to-end trials. We
conclude that exotic post-quantum signatures are deployable at the application
layer today, with on-chain bytes — not CPU time — as the binding constraint.

---

## 1. Introduction

### 1.1 Context: the quantum threat to blockchain signatures

Every transaction on Bitcoin, Ethereum, and essentially every deployed blockchain
is authorised by a digital signature over an elliptic-curve group — ECDSA or
Schnorr over secp256k1, or BLS over BLS12-381. The security of all of these rests
on the hardness of the discrete-logarithm problem, which Shor's algorithm [7]
solves in polynomial time on a sufficiently large quantum computer. For
confidentiality, the quantum threat is often framed as "harvest now, decrypt
later"; for signatures the framing is sharper: the moment discrete logarithms
become feasible, an attacker can forge spending authorisations, and any output
whose public key is visible on-chain — which on Bitcoin includes every address
that has ever been spent from — becomes stealable. Migration to *post-quantum*
(PQ) signatures, built on problems with no known efficient quantum algorithm, is
therefore not optional for blockchains; it is a question of timing.

The standardisation of basic PQ signatures is essentially complete: NIST has
published FIPS 204 (ML-DSA, derived from CRYSTALS-Dilithium [2]), FIPS 205
(SLH-DSA/SPHINCS+), and FN-DSA (Falcon) is in progress. These schemes provide
existential unforgeability — the ability to sign and verify — and nothing more.

### 1.2 The gap: exotic signatures are paper-only in the PQ setting

Real blockchain protocols lean on signatures that do *more* than sign.
Multisignatures aggregate signers; threshold signatures split keys; ring
signatures hide signers; and **adaptor signatures** tie the validity of a
signature to the revelation of a secret, enabling *scriptless scripts* [8]:
atomic swaps, payment channels, and payment-channel networks (PCNs) such as the
Lightning Network, implemented without any on-chain script support. A 2022
systematisation of exotic signatures for post-quantum blockchains [4] found that
in the PQ setting these schemes are almost entirely *paper-only*: proven secure,
but with little or no working code, and none demonstrated in a blockchain
workflow. The nearest precedent, poqeth [5], integrated *basic* PQ signatures
into Ethereum and measured their gas cost; no *exotic* PQ signature had a public
implementation, let alone an application demonstration.

This project closes part of that gap for adaptor signatures. The claim defended
throughout is twofold: **(i)** the first public implementation of LAS, the first
lattice-based adaptor signature; **(ii)** the first integration of an exotic
post-quantum signature into a blockchain setting.

### 1.3 Adaptor signatures

An adaptor signature scheme augments an ordinary signature scheme
(KeyGen/Sign/Verify) with four algorithms bound to a hard relation
`R = {(Y, y)}` of statements `Y` and witnesses `y`:

- `PreSign(sk, Y, M) → σ̂` — a *pre-signature* on message `M`, bound to statement `Y`;
- `PreVerify(Y, pk, σ̂, M) → {0,1}` — checks the pre-signature is well-formed and bound to `Y`;
- `Adapt((Y, y), σ̂) → σ` — completes the pre-signature into an ordinary signature using the witness;
- `Ext(Y, σ, σ̂) → y` — recovers the witness from a pre-signature and its adapted signature.

Three properties make the primitive useful: a pre-signature is *not* a valid
signature (so it can be exchanged safely); any valid pre-signature can be
completed by anyone holding the witness; and publishing the completed signature
*reveals the witness* to anyone holding the pre-signature. In an atomic swap this
yields atomicity by construction: claiming your coin publishes exactly the secret
that lets me claim mine (Section 2.5).

### 1.4 Related work

**LAS** [1] (Esgin–Ersoy–Erkin, ESORICS 2020) is the first lattice-based adaptor
signature. It is constructed directly on the Dilithium design: a
Fiat–Shamir-with-aborts signature over module lattices, with the adaptor
statement folded into the Fiat–Shamir hash. Its security reduces to Module-SIS
and Module-LWE — the same assumptions as NIST's ML-DSA — and the paper proves
pre-signature correctness, adaptability, and witness extractability. The paper
presents both a *simplified* scheme (no low/high-bit decomposition, no hint
vector) and an *optimised* scheme with Dilithium-style compression; it also
constructs adaptor multi-hop locks (AMHL) for PCNs, requiring a tightened
rejection bound `γ−κ−K` for path length `K`. We implement the simplified scheme
(the choice is justified in Section 3.3) and the AMHL construction.

**IAS** [3] (Tairi–Moreno-Sanchez–Maffei, FC 2021) is the isogeny-based
alternative, built on CSI-FiSh over CSIDH-512. Its signatures are dramatically
smaller (tens of bytes), but CSIDH-512 is now estimated to offer only ≈60-bit
quantum security — below NIST's floor — its group-action arithmetic is orders of
magnitude slower, and the public artefact is six years old. The survey [4]
recommends LAS over IAS unless on-chain size dominates all other concerns; we
adopt that recommendation and treat IAS as the considered-and-rejected
alternative (an attempted run of its artefact remains future work, Section 6).

**poqeth** [5] (eprint 2025/091) is the integration precedent: it implemented and
gas-measured *basic* PQ verification (including Dilithium) on Ethereum. Our
project is complementary — it supplies the exotic layer poqeth lacks, and
poqeth's gas figures are the natural anchor for a future on-chain port of our
byte-level verifier.

**Classical adaptor signatures.** Adaptor signatures originate in Poelstra's
scriptless scripts [8]; Aumayr et al. [9] formalised the ECDSA-based
construction, and Malavolta et al. [10] introduced anonymous multi-hop locks for
PCNs. A production implementation of the ECDSA adaptor exists in Blockstream's
libsecp256k1-zkp [11] and is used in Discreet Log Contracts; it serves as our
classical baseline (Section 4).

**Out of scope.** Consensus-level multisignatures (e.g. Squirrel, Chipmunk),
blind and group signatures, and heavy ZKP/MPC constructions are explicitly out of
scope per the project's supervision: they require modifying consensus clients or
introduce machinery orthogonal to the adaptor question. They are not discussed
further.

### 1.5 Objectives

- **O1 (Stage 1 — scheme).** Implement LAS (simplified scheme, the paper's
  Algorithm 2) in C on the CRYSTALS-Dilithium reference implementation, reusing
  its arithmetic primitives with zero modifications to upstream functions, and
  validate the full adaptor contract with 100% success over ≥1,000 randomised
  end-to-end trials.
- **O2 (Stage 2 — application).** Demonstrate the scheme in a blockchain
  setting: a two-party, two-chain atomic swap; a scriptless hash-time-locked
  ledger with claim and timeout-refund paths; and, as a bonus tier, the paper's
  AMHL multi-hop construction with the `γ−κ−K` bound.
- **O3 (evaluation).** Benchmark at both the signature and the application level
  against **two** baselines measured on the same machine: basic Dilithium-3 (the
  PQ basic row) and a production classical ECDSA-adaptor (the classical exotic
  row) — quantifying "the price of post-quantum".
- **O4 (engineering quality).** Provide byte-level serialisation with a
  validating decoder (the interface an on-chain verifier consumes), deterministic
  signing, pinned known-answer tests, and a reproducible build with recorded
  provenance.
- **O5 (critical analysis).** Identify and quantify the limitations honestly:
  the knowledge gap, the simplified-scheme trade-offs, the parameter deviation,
  and the fairness caveats of the comparison itself.

### 1.6 Contributions and roadmap

The artefact comprises ≈2,300 lines of new C: four modules (`las`, `serialize`,
`amhl`, `chain`), six hard-asserted test programs, and four benchmark programs.
Section 2 presents the design; Section 3 the implementation methodology and key
decisions; Section 4 the evaluation against both baselines; Section 5 the
critical analysis; Section 6 concludes.

---

## 2. Design

### 2.1 Preliminaries

Let `d = 256` and `q` prime, and let `R_q = Z_q[X]/(X^d + 1)` be the cyclotomic
ring; elements are polynomials of degree < 256 with coefficients in `Z_q`. The
infinity norm `‖a‖∞` is the largest absolute *centred* coefficient. We work with
the matrix

```
A = [ I_n | A' ] ∈ R_q^{n×(n+ℓ)},
```

an identity block beside a uniformly random block (Hermite normal form). The map
`x ↦ A·x` is Ajtai's hash [12]: finding short non-zero preimages of `0` is the
Module-SIS problem, and distinguishing `(A, A·s+e)` from uniform for short `s, e`
is Module-LWE. Per the project scope, both are treated as black-box hardness
assumptions; no security proofs are reproduced (they are in [1]).

LAS, like Dilithium, is a **Fiat–Shamir-with-aborts** [13] signature: the signer
commits `w = A·y` for a random mask `y`, derives a challenge `c` by hashing,
responds `z = y + c·r`, and *rejects and retries* if `‖z‖∞` leaves a safe band —
both to keep `z` independent of the secret `r` and to keep verification sound.

**Table 1 — parameters (the paper's simplified scheme).**

| Symbol | Value | Meaning |
|---|---|---|
| `d (=N)` | 256 | ring degree |
| `n = ℓ` | 4 | module dimensions; `A ∈ R_q^{4×8}` |
| `κ` | 60 | challenge weight (`‖c‖₁ = κ`, coefficients ±1) |
| `γ` | 122,880 | mask bound, `γ = κ·d·(n+ℓ)` |
| `q` | 8,380,417 ≈ 2²³ | modulus (deviation from the paper's ≈2²⁴; Section 3.3) |
| Sign bound | `γ−κ` | accept `‖z‖∞ ≤ 122,820` |
| PreSign bound | `γ−κ−1` | accept `‖ẑ‖∞ ≤ 122,819` |
| AMHL bound | `γ−κ−K` | for path length `K` |

### 2.2 The base signature

```
KeyGen():  r ← S₁^{n+ℓ} (ternary);  t = A·r;  return (pk, sk) = (t, r)

Sign(sk, M):    repeat:  y ← S_γ^{n+ℓ};  w = A·y;  c = H(pk, w, M);  z = y + c·r
                until ‖z‖∞ ≤ γ−κ;        return σ = (c, z)

Verify(pk, σ, M):  accept iff ‖z‖∞ ≤ γ−κ  and  c = H(pk, A·z − c·t, M)
```

Correctness is the one-line identity `A·z − c·t = A(y + c·r) − c·(A·r) = w`. The
bound `‖c·r‖∞ ≤ κ` (a weight-`κ`, ±1 challenge times a ternary secret) makes the
accepted band feasible, and — crucially for what follows — the band `γ−κ` leaves
exactly one unit of headroom below the information-theoretic maximum the adaptor
needs.

### 2.3 The adaptor extension (the paper's variant, "B")

The entire adaptor mechanism is one idea: **fold the statement into the
Fiat–Shamir hash.** The statement/witness pair is literally another key pair,
`y_w ← S₁^{n+ℓ}`, `Y = A·y_w`; recovering `y_w` from `Y` is exactly the
Module-LWE key-recovery problem, so the relation is hard.

```
PreSign(sk, Y, M):   repeat:  y ← S_γ;  w = A·y;  c = H(pk, w + Y, M);  ẑ = y + c·r
                     until ‖ẑ‖∞ ≤ γ−κ−1;          return σ̂ = (c, ẑ)

PreVerify(Y, pk, σ̂, M):  accept iff ‖ẑ‖∞ ≤ γ−κ−1  and  c = H(pk, (A·ẑ − c·t) + Y, M)

Adapt((Y, y_w), σ̂):  if PreVerify fails return ⊥;  return σ = (c, ẑ + y_w)

Ext(Y, σ, σ̂):        s = z − ẑ;  return s iff A·s = Y
```

```
[Figure 2 — the adaptor flow]

 signer (sk)                       witness holder (y_w)            any verifier
     │  σ̂ = PreSign(sk, Y, M)          │                                │
     ├────────────────────────────────►│  PreVerify ✓                   │
     │                                 │  σ = Adapt((Y,y_w), σ̂)         │
     │                                 ├───────────── publish ─────────►│ Verify ✓ (ordinary!)
     │      y_w = Ext(Y, σ, σ̂)  ◄──────┴── σ is public ──               │
```

Four facts, each verified line-by-line in the artefact's documentation and
asserted by the test suite:

1. **PreVerify accepts honest pre-signatures** — same identity as Verify, plus
   `+Y` on both sides of the hash.
2. **Adapted signatures pass the *unmodified* Verify.** With `z = ẑ + y_w`:
   `A·z − c·t = (A·ẑ − c·t) + A·y_w = w + Y`, and the pre-signing challenge was
   `c = H(pk, w+Y, M)` — they match. The on-chain object is an ordinary
   signature; no special verifier is needed.
3. **A pre-signature is *not* a signature (the tripwire).** Ordinary Verify
   recomputes `H(pk, w, M)`, but `c` was formed over `w + Y`; since `Y ≠ 0`, the
   hashes mismatch. This failure is *cryptographic* (a Fiat–Shamir mismatch),
   not a formatting artefact — a distinction that matters in Section 4.2.
4. **Extraction is exact:** `z − ẑ = y_w`, checked against `A·s = Y`.

**The norm budget is the failure mode to respect.** PreSign accepts only
`‖ẑ‖∞ ≤ γ−κ−1`; the ternary witness adds at most 1, so the adapted response
satisfies `‖z‖∞ ≤ γ−κ` — exactly ordinary Verify's band. Loosening PreSign to
`γ−κ` silently breaks *every* adapted signature. This one-unit budget
generalises: a witness of norm ≤ `K` requires pre-signing at `γ−κ−K`, which is
precisely the AMHL bound below.

**Why this variant and not the response-offset alternative.** An earlier design
sketch ("variant A") had the pre-signer emit `z̃ = z + y` with verification
subtracting `Y`. That requires the pre-signer to *know the witness* — wrong
adaptor semantics — and produces an out-of-band response needing widened
encodings. The paper's Algorithm 2 ("variant B") needs only the public statement
at pre-sign time and keeps every object Dilithium-shaped; it is what we
implement.

### 2.4 Applications: atomic swap and scriptless HTLCs

```
[Figure 3 — two-chain atomic swap]

  Bob draws (Y, y).            chain A                    chain B
  1. Bob → Alice:  Y
  2. Alice: σ̂_A = PreSign(sk_A, Y, tx_A)   ──►  escrow (PreVerify on-chain)
  3. Bob:   σ̂_B = PreSign(sk_B, Y, tx_B)                        ──►  escrow
  4. Bob publishes σ_A = Adapt((Y,y), σ̂_A)  ──►  Bob paid; σ_A public
  5. Alice: y = Ext(Y, σ_A, σ̂_A)
  6. Alice publishes σ_B = Adapt((Y,y), σ̂_B)                    ──►  Alice paid
```

One statement `Y` binds both legs. Bob can only claim by *publishing* `σ_A`, and
`σ_A` together with `σ̂_A` is exactly what Alice needs to extract `y` and claim
her side; before step 4 Alice cannot complete `σ̂_B` (the tripwire). Either both
legs settle or neither does. Our ledger abstraction generalises this into a
scriptless HTLC: the hash-lock is replaced by the adaptor statement, and a
block-height timeout provides the refund path (laddered timeouts give the second
claimant a safety window).

### 2.5 Multi-hop locks (AMHL) — the bonus tier

A naive PCN locks every hop of a route `U₀ → U₁ → … → U_K` to the *same* `Y`,
which is vulnerable to the wormhole attack: one revealed witness opens every hop.
The paper's AMHL gives each hop a **distinct cumulative statement**:

```
increments  l₁ … l_K ← S₁          (ternary)
cumulative  s_j = l₁ + … + l_j ;   Y_j = A·s_j         (Y_j = Y_{j-1} + A·l_j)
hop j locked to Y_j, opened by s_j;  every hop pre-signs at bound γ−κ−K
receiver gets s_K; intermediary U_j gets only l_{j+1}
```

```
[Figure 4 — AMHL cascade (K = 4)]

 U0 ──Y₁── U1 ──Y₂── U2 ──Y₃── U3 ──Y₄── U4
            ◄─ s₁     ◄─ s₂=s₃−l₃  ◄─ s₃=s₄−l₄  ◄─ s₄ (receiver opens first)
```

Claims cascade right-to-left: the receiver opens hop `K` with `s_K`, publishing
it; each intermediary extracts `s_j` on-chain and computes `s_{j-1} = s_j −
l_j` to open its own hop. Because `‖s_j‖∞ ≤ j ≤ K` (a sum of `j` ternary
vectors), the adapted response satisfies `‖z‖∞ ≤ (γ−κ−K) + K = γ−κ` — the
`γ−κ−K` bound is exactly the price of multi-hop witnesses, and the witness-norm
growth it absorbs is the concrete face of the *knowledge gap* discussed in
Section 5.

---

## 3. Implementation

### 3.1 Method and justification

**Language and base.** The implementation is C, extending the CRYSTALS-Dilithium
reference implementation — both supervisor-confirmed choices. Three alternatives
were considered and rejected. A high-level prototype (Python) would have produced
unrepresentative timings and required reimplementing the NTT and samplers anyway.
Building on a packaged library (liboqs) was rejected because its APIs
deliberately hide the internals the adaptor needs — the `+Y` fold happens *inside*
the Fiat–Shamir loop, which packaged sign/verify functions do not expose. Writing
the lattice arithmetic from scratch was rejected as pure risk: the reference
NTT, SHAKE, and samplers are the most-audited lattice code in existence, and the
thesis claim ("an exotic scheme is a basic scheme plus four functions") is best
demonstrated by literal reuse.

**The clean-diff principle.** The headline engineering fact, recorded
function-by-function in the repository's function map (Appendix D): **zero
upstream Dilithium source functions were modified.** LAS is four new,
self-contained modules that *call* a small set of Dilithium's mode-independent
primitives as-is; the only edit to any existing file is additive `Makefile`
targets. The diff against upstream *is* the contribution, and it is auditable at
a glance.

**Table 2 — what is reused vs added (condensed function map).**

| Layer | Functions | Status |
|---|---|---|
| NTT multiplication | `poly_ntt`, `poly_invntt_tomont`, `poly_pointwise_montgomery` | reused as-is |
| Ring arithmetic | `poly_add/sub/reduce/caddq`, `poly_chknorm` | reused as-is |
| Sampling/hashing | `poly_uniform`, SHAKE-128/256 (`fips202.c`), `randombytes` | reused as-is |
| Mode-specific Dilithium (`Power2Round`, hints, packing, `sign.c`) | — | **not used** (Section 3.3) |
| LAS scheme | `las_keygen/sign/verify/presign/preverify/adapt/ext` (+ `_k`, `_det`, `_seed` variants) | **new** (`las.c`, ~560 lines) |
| Serialisation | `las_pack/unpack_{pk,sk,sig}`, `las_verify_packed` | **new** (`serialize.c`) |
| Multi-hop | `amhl_setup_gen`, `amhl_recover_prev`, `amhl_norm` | **new** (`amhl.c`) |
| Ledger | `chain_fund_swap(_k)/claim/refund/extract_witness` | **new** (`chain.c`) |

### 3.2 Key decisions

**The simplified scheme, deliberately.** Optimised Dilithium hashes only the
*high bits* of the commitment and transmits a hint vector so the verifier can
reconstruct them. The adaptor identity `A·z − c·t = w + Y`, however, must hold
*exactly* as hashed — the `+Y` fold and the hint/decomposition machinery
interact, and reconciling them is an open engineering problem (the paper's
optimised variant redesigns this; we did not attempt it). The simplified scheme
keeps the algebra transparent: the full commitment is hashed, correctness is a
three-line identity, and every test failure during development was attributable.
The costs — larger objects and a lower per-attempt acceptance rate — are
quantified honestly in Section 4.

**Parameters: `q ≈ 2²³` rather than the paper's `2²⁴`.** The reference NTT's
root-of-unity table is fixed to `Q = 8,380,417`. Re-deriving the bounds for this
`q` shows correctness is unaffected — every intermediate value (responses ≤ γ,
products mod `q`) is represented faithfully since `Q > 2γ` — and only the
concrete MSIS/MLWE security margin shifts, which is out of scope per the project
brief. Starting from the implementation's parameters rather than the paper's was
the supervisor-sanctioned approach; migrating to `2²⁴` (a new NTT table) is
documented future work.

**Bound handling after Adapt.** All rejection bounds are encoded once as macros
(`LAS_BOUND_SIGN`, `LAS_BOUND_PRESIGN`, `LAS_BOUND_PRESIGN_K(K)`), with the
`+1`-encoding subtlety (the reused `poly_chknorm(v, B)` rejects at `≥ B`)
documented at the definition site, because this is the scheme's single
failure mode: a one-unit mistake breaks all adapted signatures while every other
test still passes.

**Canonical hashing.** Commitments are canonicalised to `[0, q)` before hashing
so that the verifier's recomputed `w′` is byte-identical to the signer's `w`
whenever they are equal mod `q` — the kind of detail that costs a day of
debugging exactly once.

### 3.3 Serialisation and the on-chain interface

The scheme functions operate on in-memory structs, but a real deployment — and
specifically an on-chain verifier in the style of poqeth — consumes *bytes*.
`serialize.c` provides the canonical encoding (LSB-first bit-packing: 23
bits/coefficient for public keys, 2 for ternary secrets, 2+18 for signatures)
and, critically, a **validating decoder**: a verifier must reject malformed
input, so unpacking rejects any coefficient `≥ q`, the invalid ternary code, and
any response outside the 18-bit band. `las_verify_packed(pk_bytes, sig_bytes, M)`
— decode-with-validation, then ordinary Verify — is the exact interface a
Solidity contract, precompile, or circuit would expose. Measured sizes: public
key/statement **2,944 B**, secret/witness **512 B**, signature = pre-signature
**4,672 B**.

### 3.4 Determinism and reproducibility

Per-signature randomness is a deployment hazard (nonce reuse has repeatedly
broken classical ECDSA in the field) and an obstacle to testing. We therefore
added deterministic siblings — `las_keygen_seed`, `las_sign_det`,
`las_presign_det` — which derive the mask seed as `SHAKE256(tag ‖ sk ‖ [Y] ‖ M)`,
the standard Fiat–Shamir derandomisation. The randomised and deterministic paths
share one core (they differ only in seed origin), so their output distributions
are identical by construction. On top of this sits a known-answer test: four
fully-deterministic vectors exercising the whole pipeline
(keygen → presign → adapt → ext → serialise), folded into a single pinned
SHAKE256 digest. Any unintended change to keygen, signing, the adaptor algebra,
or the encoding flips the digest; an independent (e.g. on-chain) verifier can be
cross-checked against the same vectors. Provenance is recorded in the repository:
upstream Dilithium at commit `2374d22`, classical baseline library at `95b9835`,
toolchain `cc 13.3.0 / GNU Make 4.3 / -O3`, zero warnings under
`-Wall -Wextra -Wpedantic -Wmissing-prototypes -Wshadow -Wvla`.

### 3.5 Testing methodology

Testing is structured so that each formal property of the adaptor contract is an
executable assertion:

1–2. `(pk,sk) = KeyGen()`, `(Y,y) = KeyGen()` — the statement *is* a key pair.
3–4. `σ̂ = PreSign`; assert `PreVerify(σ̂) = true` — *pre-signature correctness*.
5. assert `Verify(σ̂) = false` — *statement binding* (the tripwire).
6–7. `σ = Adapt(σ̂)`; assert `Verify(σ) = true` — *adaptability*, ordinary verifier.
8. `y′ = Ext(σ, σ̂)`; assert `A·y′ = Y` **and** `y′ = y` — *extractability, exact*.

This 8-point contract runs **1,000 randomised iterations** (fresh parameters,
keys, statements, messages each time) under three Dilithium build modes, plus a
sign/verify round-trip and a bit-flip forgery rejection. The serialisation suite
adds round-trip identity, verification-from-bytes (including the tripwire
surviving serialisation), **exhaustive single-byte tamper rejection (all 4,672
positions)**, and malformed-input rejection. The AMHL suite asserts distinct
per-hop statements, wormhole resistance (the receiver's `s_K` *fails* to open a
non-adjacent hop), the norm growth `‖s_j‖∞ ≤ j`, exact cascade recovery, and the
timeout-refund path. All suites pass with zero compiler warnings; 100% of the
1,000 contract iterations succeed (objective O1's acceptance bar).

---

## 4. Evaluation

### 4.1 Evaluation methodology and its justification

**Two baselines, one machine.** The supervisor's brief asks what adaptor
functionality costs (i) relative to the basic PQ signature it extends, and (ii)
relative to the classical adaptor it replaces. Published numbers from other
papers would import hardware differences larger than some of the effects being
measured, so both baselines were *run locally*: optimised **Dilithium-3** (the
reference implementation in this repository), and the **ECDSA adaptor** from
libsecp256k1-zkp [11] — Blockstream's production library (used in Discreet Log
Contracts), reused as-is with only a thin timing harness of ours, exactly as the
brief permits. All numbers below: same machine, same compiler, `-O3`, ≥1,000
(typically 2,000) iterations per operation.

**Direct measurement over proxies — a correction worth reporting.** An early
version of the benchmark *estimated* rejection-sampling retries from the timing
ratio `t_sign/t_verify`, reporting ≈23% acceptance. That estimator is biased — a
Sign attempt computes `n+ℓ = 8` products `c·rⱼ` against Verify's `n = 4` — and
direct instrumentation (an attempt counter incremented inside the rejection
loop) gives ≈37%, matching theory: an attempt is accepted iff all
`(n+ℓ)·d = 2,048` response coefficients land in `±(γ−κ)`, i.e.
`(1 − κ/γ)^2048 ≈ e^{−1} = 36.8%`. The corrected pipeline (measure directly;
corroborate with a closed form) is applied throughout, and the episode is kept
in the report deliberately: it is a concrete instance of the "let the data
speak" principle.

**Fairness caveats, stated up front.** libsecp256k1 is constant-time, heavily
optimised production code; our LAS is a reference-style simplified scheme at a
reduced security margin (`q ≈ 2²³`). Timing ratios therefore *flatter the
classical side*. Size ratios are format-determined and carry no such caveat.
The ledger is simulated, so application-level "on-chain" figures are a
settlement-payload proxy, not measured gas.

### 4.2 Signature-level results

**Table 3 — the 2×2 timing matrix (µs/op, same machine).**

| Operation | ECDSA (classical basic) | ECDSA-adaptor (classical exotic) | Dilithium-3 (PQ basic) | LAS (PQ exotic) |
|---|---:|---:|---:|---:|
| KeyGen / statement gen | 31 | 31 | 162 | 78 |
| Sign | 41 | — | 642 | 804 |
| Verify | 62 | — | 155 | 191 |
| PreSign | — | 189 | — | 828 |
| PreVerify | — | 244 | — | 197 |
| Adapt | — | 3 | — | 203 |
| Ext | — | 35 | — | 68 |

**Table 4 — object sizes (bytes).**

| Object | ECDSA(-adaptor) | Dilithium-3 | LAS packed (measured) | LAS / classical |
|---|---:|---:|---:|---:|
| public key / statement | 33 | 1,952 | 2,944 | ×89 |
| secret key / witness | 32 | 4,032 | 512 | ×16 |
| signature | 64 (70 DER) | 3,309 | 4,672 | ×73 |
| pre-signature | 162 | — | 4,672 | ×29 |

**Table 5 — rejection sampling (measured directly, 2,000 signatures).**

| | attempts/sig | acceptance | theory |
|---|---:|---:|---:|
| Sign | 2.71 | 36.9% | `(1−κ/γ)^2048` = 36.8% |
| PreSign | 2.77 | 36.1% | (the `−1` tightening is negligible) |

Four findings:

**(1) The price of post-quantum is communication, not computation.** Sizes grow
×29–89 against the classical adaptor; per-operation times grow far less (Verify
×3.1, PreSign ×4.4, Sign ×19.5 — and KeyGen is ×2.5) and *everything stays
sub-millisecond* on commodity hardware. For blockchain deployment the size
column is the binding constraint, since on-chain bytes are the metered resource.

**(2) The adaptor overhead structure inverts — the headline result.** Measured
*within each family*: classical PreSign costs **4.6×** its own Sign and
PreVerify **3.9×** its own Verify, because the ECDSA pre-signature must carry
and check a DLEQ proof. LAS PreSign costs **1.03×** Sign and PreVerify **1.03×**
Verify: folding `+Y` into a hash is free. Strikingly, LAS PreVerify (197 µs) is
**absolutely faster** than classical ECDSA-adaptor PreVerify (244 µs) on the
same machine, despite operating on objects ×29 larger. The marginal cost of
adaptor functionality is essentially zero in the lattice setting — the paper's
efficiency claim, now measured.

**(3) Against its own base, LAS is feasible.** LAS operations sit in the same
hundreds-of-microseconds regime as Dilithium-3. LAS Sign is 1.25× Dilithium-3
Sign — the hint-free scheme's ≈2.7 attempts (Table 5) instead of Dilithium's
small repetition count, partially offset by smaller module dimensions. We
emphasise this is a feasibility statement, not a superiority claim: the two
schemes sit at different security margins (Section 4.1).

**(4) A structural contrast.** The classical pre-signature is a *syntactically
different object* (162 B: signature plus DLEQ proof) that cannot be parsed as a
signature at all; the LAS pre-signature shares the signature format and fails
ordinary verification *cryptographically* (the `+Y` mismatch). Consequently a
LAS-based system needs exactly one wire format and one verifier; the adapted
signature is indistinguishable from an ordinary one in both families.

### 4.3 Application-level results

**Atomic swap (Table 7).** Three off-chain messages
(`Y, σ̂_A, σ̂_B` = 2,944 + 2×4,672 = **12,288 B**); settlement publishes only the
two adapted signatures (**9,344 B** across both chains; 15,232 B counting the
escrowed statements). End-to-end signing work (2×PreSign + 2×Adapt + Ext) is a
few milliseconds, dominated by rejection sampling. The harness re-asserts
fairness on every run: adapted signatures verify, pre-signatures do not.

**Multi-hop cost as a function of path length (Table 6).**

| K | bound `γ−κ−K` | attempts/presig | presign total (ms) | settlement sigs | statements | max ‖s_j‖∞ |
|--:|--:|--:|--:|--:|--:|--:|
| 1 | 122,820 | 2.70 | 0.6 | 4,672 B | 2,944 B | 1 |
| 2 | 122,819 | 2.79 | 1.4 | 9,344 B | 5,888 B | 2 |
| 4 | 122,817 | 2.74 | 2.5 | 18,688 B | 11,776 B | 4 |
| 6 | 122,815 | 3.07 | 4.1 | 28,032 B | 17,664 B | 6 |
| 8 | 122,813 | 2.84 | 4.9 | 37,376 B | 23,552 B | ≤8 |

Three observations. *(i)* Cost is linear in `K` — `K` pre-signatures, `K`
statements, no super-linear blow-up. *(ii)* The witness norm grows exactly as
the theory predicts (`max‖s_j‖∞ = j`, realised at or just below `K`) — the
knowledge gap made measurable. *(iii)* The `γ−κ−K` tightening, while
cryptographically essential, is performance-free: moving `K` from 1 to 8 shrinks
the acceptance band by `7/(γ−κ) ≈ 0.006%`, and the measured attempts/presign
column is statistically flat. The bound change that makes multi-hop *correct*
costs nothing in speed — a counter-intuitive and, to our knowledge, previously
unmeasured property.

### 4.4 Correctness, robustness, reproducibility

100% of 1,000 randomised end-to-end contract trials pass (modes 2/3/5). All
4,672 single-byte corruptions of a packed signature are rejected by the
byte-level verifier, as are structurally malformed inputs (coefficients ≥ q,
invalid ternary codes, out-of-band responses). The deterministic KAT digest is
bit-stable across runs and re-pinned in the test source; the artefact rebuilds
from a clean tree with zero warnings (Appendix D).

### 4.5 Positioning against the literature

Our measured 4,672 B packed signature compares against the paper's ≈3,210 B
*estimate* for its **optimised** scheme — the gap is precisely the hint-vector
compression and `2²⁴` parameter set we deliberately did not implement, plus our
challenge encoding. IAS [3] achieves ~50 B signatures but at ≈60-bit quantum
security and group-action timings orders of magnitude slower; on the survey's
own criteria [4], LAS remains the recommended construction. poqeth [5] reports
gas for basic Dilithium verification on Ethereum; our `las_verify_packed`
consumes byte strings precisely so that porting it into poqeth's harness is
future work with measured baselines on both ends.

---

## 5. Critical analysis and limitations

**The knowledge gap.** In our parameterisation extraction is *exact* — the test
suite asserts `y′ = y` bitwise, and the AMHL cascade recovers every intermediate
witness exactly. In the paper's general (relaxed) relation, extraction can carry
bounded noise that accumulates along payment paths; the measured witness-norm
growth `‖s_j‖∞ ≤ j` (Table 6) is the concrete, benign face of the same
phenomenon. We quantify it; we do not solve it — the survey [4] flags it as
fundamental to lattice adaptor signatures, and it bounds realistic path lengths.

**Privacy.** LAS-based payment channels leak more than classical AMHL designs:
statements are public on-chain, and our same-Y HTLC baseline is wormhole-
vulnerable by construction (which is exactly why the AMHL tier exists — its
wormhole resistance is hard-asserted). Anonymity properties in the sense of
Malavolta et al. [10] are not claimed and were not evaluated.

**Simplified-scheme trade-offs, quantified.** Omitting the hint machinery costs
≈2.7 rejection attempts per signature (the `e^{−1}` law) and roughly 40% in
signature size versus the paper's optimised estimate. It does *not* cost
acceptance relative to optimised Dilithium — a point we initially got wrong in
the classical direction (Section 4.1) — and it bought a transparent,
fully-auditable adaptor core. We consider the trade favourable for a research
artefact and indefensible for a production deployment; the report says both.

**Security margin and proofs.** `q ≈ 2²³` reduces the concrete MSIS/MLWE margin
relative to the paper's parameter set; per the project scope we state, cite,
and do not re-derive the security argument. No constant-time guarantees are
claimed: samplers and norm checks follow the reference style, and side-channel
hardening is future work.

**Fairness of the comparison.** The classical column is production-grade,
constant-time code; the LAS column is a reference-style implementation. The
honest reading is that the *time* ratios are upper bounds on the true gap, the
*size* ratios are exact, and the inverted-overhead finding (Section 4.2, (2)) is
robust because it compares each scheme against *its own base*, cancelling
implementation quality to first order.

**Threats to validity.** Single machine (WSL2), wall-clock timing, and
run-to-run variance of a few percent on the small classical timings; medians
over ≥1,000 iterations and reported variability mitigate but do not eliminate
this. The ledger is simulated; gas figures remain unmeasured (Section 6).

---

## 6. Conclusions and future work

This project set out to move post-quantum *exotic* signatures from paper to
artefact. Against its objectives: **O1** — LAS implemented on the unmodified
Dilithium reference, the full adaptor contract passing 1,000/1,000 randomised
trials; **O2** — a working scriptless atomic swap, a timeout-refund HTLC ledger,
and the bonus AMHL multi-hop construction with the `γ−κ−K` bound and
hard-asserted wormhole resistance; **O3** — two same-machine baselines, yielding
the 2×2 evaluation: the price of post-quantum is ×29–89 in bytes, not in time,
and LAS's adaptor operations cost almost nothing over its base scheme — the
overhead structure actually *inverts* relative to classical ECDSA adaptors;
**O4** — byte-level serialisation with a validating verifier, deterministic
signing, pinned KATs, recorded provenance; **O5** — the limitations above,
quantified where possible rather than asserted.

The broader conclusion is that the gap between "basic PQ signatures on a
blockchain" (poqeth) and "exotic PQ signatures on a blockchain" is smaller than
the paper-only literature suggested: it closed here with ≈2,300 lines of C and
zero changes to the underlying NIST-track primitive. What stands between this
artefact and deployment is not cryptography but bytes: 4,672 B signatures are
the real cost, and they are already within ×1.4 of optimised Dilithium-3's.

**Future work**, in order of value per effort: (1) port `las_verify_packed`
into a poqeth-style Ethereum harness and measure gas against their basic-
Dilithium figures — both endpoints now exist; (2) migrate parameters to the
paper's `q ≈ 2²⁴` (new NTT table) and report the before/after benchmark diff;
(3) attempt to run the six-year-old IAS artefact, where even a documented
failure is data; (4) reconcile the hint/compression machinery with the adaptor
algebra to recover the remaining ~40% of signature size; (5) a second exotic
scheme (a lattice ring signature such as Falafl) for an exotic-vs-exotic
comparison; (6) constant-time hardening.

---

## References

1. M. F. Esgin, O. Ersoy, Z. Erkin. *Post-Quantum Adaptor Signatures and Payment Channel Networks.* ESORICS 2020; IACR ePrint 2020/845.
2. L. Ducas, E. Kiltz, T. Lepoint, V. Lyubashevsky, P. Schwabe, G. Seiler, D. Stehlé. *CRYSTALS-Dilithium: Algorithm Specifications.* NIST FIPS 204 (ML-DSA), 2024; reference implementation github.com/pq-crystals/dilithium.
3. E. Tairi, P. Moreno-Sanchez, M. Maffei. *Post-Quantum Adaptor Signature for Privacy-Preserving Off-Chain Payments.* FC 2021; IACR ePrint 2020/1345.
4. M. Buser et al. *A Survey on Exotic Signatures for Post-Quantum Blockchain.* IACR ePrint 2022/1151.
5. R. Ilesik et al. *poqeth: Efficient, post-quantum signature verification on Ethereum.* IACR ePrint 2025/091.
6. M. Ajtai. *Generating Hard Instances of Lattice Problems.* STOC 1996.
7. P. W. Shor. *Polynomial-Time Algorithms for Prime Factorization and Discrete Logarithms on a Quantum Computer.* SIAM J. Comput. 26(5), 1997.
8. A. Poelstra. *Scriptless Scripts.* Presentation/notes, 2017.
9. L. Aumayr et al. *Generalized Channels from Limited Blockchain Scripts and Adaptor Signatures.* ASIACRYPT 2021; IACR ePrint 2020/476.
10. G. Malavolta, P. Moreno-Sanchez, C. Schneidewind, A. Kate, M. Maffei. *Anonymous Multi-Hop Locks for Blockchain Scalability and Interoperability.* NDSS 2019.
11. Blockstream Research. *libsecp256k1-zkp*, module `ecdsa_adaptor`. github.com/BlockstreamResearch/secp256k1-zkp (commit 95b9835).
12. V. Lyubashevsky. *Fiat-Shamir with Aborts: Applications to Lattice and Factoring-Based Signatures.* ASIACRYPT 2009.
13. NIST. *FIPS 204: Module-Lattice-Based Digital Signature Standard.* 2024.

---

## Appendix A — Core algorithm listings (excerpts)

*(Final document: include the `presign_core` rejection loop, `las_adapt`, and
`las_ext` from `ref/las.c` — ≈60 lines total — plus `las_verify_packed` from
`ref/serialize.c`. Full source remains in the repository, not the report.)*

## Appendix B — Full benchmark tables

*(Paste the complete outputs of `bench_las3`, `bench_compare3`, `bench_app3`,
`bench_classical` for the submission machine, with date, CPU model, and OS.)*

## Appendix C — Parameters, bounds, and KAT

Parameter table as Table 1; bound-encoding convention (`poly_chknorm` rejects at
`≥ B`, so code bounds are paper-bounds `+1`); KAT digest
`f7fc40f0b7752cafc083fcddd6a13759fbde9b2a2d538045cd0d62f87747e6b1` over the four
pinned vectors.

## Appendix D — Reproducibility

Upstream Dilithium vendored at commit `2374d22`; classical baseline
`secp256k1-zkp` at `95b9835` (one-time clone, git-ignored). Toolchain:
`cc (Ubuntu 13.3.0)`, GNU Make 4.3, Linux/WSL2, `-O3`, zero warnings under the
project's full warning set. Build/run: see `README_LAS.md`; every test and
benchmark in this report is a single `make` target.
