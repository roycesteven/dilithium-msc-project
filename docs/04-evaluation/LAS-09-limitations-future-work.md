<!-- Part of docs/LAS.md, split by report chapter (2026-07-06). Index: docs/LAS.md.
     Section numbering is preserved verbatim, so external references like
     "LAS.md §9" resolve to this file. Do not renumber sections. -->

## 9. Limitations and future work

- **Knowledge gap.** In the general lattice setting the extracted witness norm grows
  with path length: a K-hop intermediate witness would have `‖s_j‖∞ ≤ j` (a sum of up
  to K ternary vectors). In *this* single-hop parameterisation extraction is **exact**
  (the witness is an integer vector recovered without error). The deeper limitation is in the paper's
  *relaxed* relation, where extraction may carry bounded noise that accumulates
  across long chains — acknowledged in the survey (eprint 2022/1151) as a
  fundamental gap of lattice adaptor signatures vs. classical ones. Analysing that
  relaxed-relation noise growth is out of scope for this project.

- **Modulus.** `Q = 8380417 ≈ 2^23`, which is **NIST FIPS 204's modulus**, rather than
  the `≈2^24` of eprint 2020/845 (Section 5.9). Since FIPS 204 is this project's
  parameter authority, that is the correct choice rather than a shortfall; `Q > 2γ`,
  so correctness holds, and only the concrete MSIS/MLWE margin differs from the
  paper's. Migrating to `2^24` is **not** a goal of this project.

- **Signature packing.** ✅ **Implemented** (`ref/serialize.{c,h}`, Section 5.10):
  bit-packed wire/on-chain encoding with a validating decoder and the
  `base_verify_packed` byte-level verifier, giving a measured packed signature of
  4640 B (vs 8224 B in-memory). The residual gap to optimised Dilithium-3 (3309 B)
  is the modulus (`2^23` vs `2^24`) and the hint/decomposition compression of the
  optimised scheme — out of scope here.

- **Reproducibility / KATs.** ✅ **Implemented** (`ref/test/test_kat.c`, Sections
  5.11 and 6.4): a deterministic API (`base_keygen_seed`, `base_sign_det`,
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

