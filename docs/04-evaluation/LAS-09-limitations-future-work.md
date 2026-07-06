<!-- Part of docs/LAS.md, split by report chapter (2026-07-06). Index: docs/LAS.md.
     Section numbering is preserved verbatim, so external references like
     "LAS.md §9" resolve to this file. Do not renumber sections. -->

## 9. Limitations and future work

- **AMHL (multi-hop, K-hop bound).** ✅ **Implemented** (Section 7.5,
  `ref/amhl.{c,h}`, `ref/test/test_amhl.c`). Each hop carries a distinct cumulative
  statement `Y_j = A·(l_1+…+l_j)`, pre-signing uses the `γ−κ−K` bound via
  `las_presign_k`, and the demo asserts wormhole resistance, the witness-norm
  growth `‖s_j‖∞ ≤ j`, exact per-hop recovery, and a timeout/refund path. The
  same-Y scriptless HTLC (Section 7.4) is retained as the simpler baseline. A
  remaining nicety is a *privacy*-preserving variant (statements are public
  on-chain) and randomised (non-cumulative-sum) lock setups.

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

