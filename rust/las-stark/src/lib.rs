//! # las-stark
//!
//! A transparent, hash-based **FRI-STARK** toolkit (built on Winterfell)
//! *targeting* the LAS on-chain verification relation (eprint 2020/845).
//!
//! **Work in progress: NOT yet a succinct proof of on-chain verification.**
//! Motivation: native LAS verify in Solidity is ~56.5M gas (> EIP-7825's 16.77M
//! per-tx cap), so it needs a succinct proof, and that proof must stay PQ -- a
//! Groth16/pairing wrap would recover cheap on-chain verification but break
//! post-quantum security. A FRI-STARK is the PQ route: its soundness rests on the
//! collision-resistance of the hash (Blake3) AND the conjectured soundness of
//! low-degree testing (FRI) in the random-oracle model -- no elliptic-curve or
//! pairing assumptions (concrete security is also bounded by field size / queries).
//!
//! ## Status (staged; see each module and `README.md`)
//! * `vectors` / `relation` / `hashing` -- load the C golden verification vectors
//!   and a NATIVE (non-STARK) reference oracle that reproduces the FULL
//!   `base_verify` relation byte-exactly: `SampleInBall(c_tilde)`,
//!   `w' = z_top + A'*z_bot - c*t` (mod q, negacyclic), and the
//!   `SHAKE256(pack(t)||pack(w')||M)` challenge -- each checked against the C
//!   goldens. This is the spec/trace-generator for the AIR, not a proof itself.
//! * `air` / `prover` -- **Stage A**: a sound STARK range-check gadget proving
//!   the response norm bound `||z||inf <= B` (constraint (1) of `base_verify`)
//!   over the LAS-sized `z`, with the bound pinned to `2B` and the trace length
//!   pinned to the LAS `z`-size. This is a real end-to-end prover+verifier but
//!   **not** a complete signature-verification proof: it does not yet bind `z`
//!   to the public statement `(A', t, c_tilde, M)`.
//! * `conv_air` -- **Stage A.2 (superseded)**: a sound STARK for ONE negacyclic
//!   convolution, but only at a reduced ring degree `CONV_D = 64` (its rotated
//!   window costs `d` columns, and Winterfell caps a trace at 255), and with no
//!   binding between separate convolutions. Kept as the schoolbook reference.
//! * `relation_air` -- **Stage A.2**: the arithmetic core of `base_verify` at
//!   the REAL degree `d = 256`, all `n` output polynomials at once and bound to
//!   ONE shared response `z`: constraints (1) `||z||inf <= B` and (3)
//!   `w' = z_top + A'*z_bot - c*t`. It is narrow (a random-evaluation argument
//!   over the auxiliary trace segment) instead of wide, which is what gets past
//!   the column cap. `c` and `w'` are still taken as PUBLIC inputs, so the
//!   Fiat-Shamir hashes -- (2) `SampleInBall` and (4) `SHAKE256` -- remain out.
//! * Later stages fold `SampleInBall` and the `SHAKE256` challenge
//!   re-derivation into the AIR (binding `z` to `c_tilde` and `M`), then add a
//!   native EVM FRI verifier for the resulting proof.

pub mod air;
pub mod conv_air;
pub mod hashing;
pub mod params;
pub mod prover;
pub mod relation;
pub mod relation_air;
pub mod vectors;

use winterfell::{BatchingMethod, FieldExtension, ProofOptions};

/// STARK parameters shared by the prover and verifier.
///
/// Blake3 hash + FRI => transparent and post-quantum. The base field is 64-bit
/// Goldilocks; a quadratic extension is used so FRI/DEEP soundness is set by a
/// ~128-bit field rather than the 64-bit base. 32 queries at blowup 8 give
/// ~96-bit conjectured security for this configuration.
pub fn proof_options() -> ProofOptions {
    ProofOptions::new(
        32,                        // number of FRI queries
        8,                         // blowup factor
        0,                         // grinding factor
        FieldExtension::Quadratic, // 64-bit base -> 128-bit extension for soundness
        8,                         // FRI folding factor
        31,                        // FRI max remainder polynomial degree
        BatchingMethod::Linear,    // constraint batching
        BatchingMethod::Linear,    // DEEP batching
    )
}
