# Project context — LAS on Dilithium for blockchain

## One-line goal
Implement LAS (Lattice-based Adaptor Signatures) by extending this Dilithium
reference repo with PreSign / Verify_pre / Adapt / Extract, then demo it in a
blockchain atomic-swap scenario.

## Why this project exists
- Blockchains sign with ECDSA/Schnorr. Shor's algorithm on a quantum computer
  breaks both. "Post-quantum" = built on lattice/hash problems Shor can't solve.
- NIST standardised BASIC post-quantum signatures (Dilithium, Falcon, SPHINCS+).
- EXOTIC signatures (multisig, ring, group, adaptor) add features. In the
  post-quantum setting they are mostly PAPER-ONLY — little working code, and
  none deployed on a blockchain. That gap is the thesis.
- Adaptor signatures (this project) enable atomic swaps / payment channels.

## Key design fact
An exotic scheme = a basic scheme + extra functions. LAS is Dilithium plus
PreSign/Verify_pre/Adapt/Extract. We REUSE Dilithium's KeyGen/Sign/Verify and
its poly/NTT/packing internals — we do NOT reinvent lattice arithmetic.

## The LAS mechanism (variant A — matches our equations)
- Secret witness y, public statement Y = A·y mod q. Knowing Y doesn't reveal y.
- PreSign: like Sign but response shifted: z̃ = z + y. Not a valid Dilithium sig.
- Verify_pre: checks σ̃ is well-formed and bound to Y.
- Adapt(σ̃, y): z = z̃ − y → a valid Dilithium signature.
- Extract(σ̃, σ): y = z̃ − z. Recovers the witness from both signatures.
  This is what makes swaps atomic: publishing the adapted sig leaks y on-chain.

## THE failure mode to watch
Dilithium requires ‖z‖_∞ < γ₁ − β. Since z̃ = z + y carries the offset, y must
be sampled SMALL and PreSign must reject-and-resample (reuse the existing loop)
so z̃ stays under the bound. This is the #1 source of "verify rejects everything"
bugs. Agree on γ₁, β for dilithium3 before coding.

## Known caveat (note in thesis, do NOT need to solve)
"Knowledge gap": extracted y is exact here, but in the paper's relaxed setting the
witness can carry noise that grows across long payment-channel chains.

## Scope discipline (from supervisor)
- Target dilithium3 (NIST level ~2/3, 128-bit PQ) — matches LAS paper parameters.
- Do NOT implement or analyse security proofs. Implement + benchmark only.
- Success ladder: (min) working LAS + a basic blockchain demo;
  (better) benchmark vs plain Dilithium; (best) a second exotic scheme.

## Reference
LAS paper: eprint 2020/845. poqeth (integration template): eprint 2025/091.
