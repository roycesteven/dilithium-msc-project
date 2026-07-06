<!-- Part of docs/LAS.md, split by report chapter (2026-07-06). Index: docs/LAS.md.
     Section numbering is preserved verbatim, so external references like
     "LAS.md §1" resolve to this file. Do not renumber sections. -->

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

**Why LAS.** **LAS** (eprint 2020/845, ESORICS 2020, Esgin–Ersoy–Erkin) is the
first lattice-based adaptor signature, built directly on Dilithium; it is what we
implement. Three reasons, each a Methodology sentence:

1. **Implementation leverage.** LAS extends CRYSTALS-Dilithium, whose reference C
   implementation is the starting point of this project. The four LAS functions
   (`PreSign`, `PreVerify`, `Adapt`, `Ext`) are additions to, not replacements of,
   the base Dilithium scheme; all polynomial arithmetic, NTT, SHAKE/Keccak and
   sampling code is reused directly.

2. **Security assumptions.** LAS is based on Module-SIS and Module-LWE — the same
   problems underlying Dilithium and the NIST standard ML-DSA. These assumptions are
   mature and well-studied; in their standardised Dilithium instantiations they target
   the 128-bit category. This build's concrete bit-security is not formally analysed and
   uses a reduced modulus `q ≈ 2²³` (see §5.9).

3. **Survey recommendation.** A 2022 systematisation of post-quantum exotic
   signatures (eprint 2022/1151) calls LAS "an acceptable solution for post-quantum
   blockchain"; its direct Dilithium reuse and well-studied assumption base are exactly
   what make it the practical choice for a working implementation.

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

