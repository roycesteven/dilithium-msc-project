//! `las_types` — the six protocol object types of the LAS construction
//! (eprint 2020/845), split out of `setup.rs` so the shared setup module holds
//! only the construction parameters, `PublicParams` and `setup_public_params`.
//! Rust twin of `ref/las_types.h`.
//!
//! ## Type ownership (paper model — Definition 3 + Algorithms 1 and 2)
//!
//! This module is the PHYSICAL home of all six protocol object types (the codec
//! in `serialize.rs` must see them, and `basesig` must never depend on `las`),
//! but the types are NOT all "shared"; each belongs to exactly one layer, and
//! each owner module re-exports its own:
//!
//! * Algorithm 1, the underlying signature scheme Σ (owner: `basesig`,
//!   `pub use crate::las_types::{PublicKey, SecretKey, Signature}`): [`PublicKey`],
//!   [`SecretKey`], [`Signature`].  Definition 3: the adaptor scheme *inherits*
//!   KeyGen/Sign/Verify from Σ; Adapt's output is this same `Signature` type,
//!   checked by Algorithm-1 Verify.
//! * hard relation R_A / R'_A (owner: `relation`,
//!   `pub use crate::las_types::{Statement, Witness}`): [`Statement`] (paper
//!   `Y = t'`), [`Witness`] (paper `y` in the pair `(Y, y)`).
//! * Algorithm 2, the adaptor operations (owner: `las`,
//!   `pub use crate::las_types::PreSignature`): [`PreSignature`] (paper
//!   `σ̂ = (c, ẑ)`).
//!
//! Dependency layering (a DAG, not a single chain): `setup` is used by this
//! module, which is in turn used by `relation`, `serialize` and both scheme
//! modules; `serialize` and both schemes import `las_types` directly, `basesig`
//! additionally uses `serialize`, and `las` additionally uses `relation` and
//! `serialize`.  C build headers follow the same order: `setup.h ->
//! las_types.h -> {relation.h, serialize.h} -> {basesig.c, las.c}`.
//!
//! `PublicParams` (the paper's `pp = (A, H)`) stays in `setup.rs` with the
//! construction parameters it is expanded from: it is the shared public setup
//! `pp`, not one of the six protocol objects.

// This module is deliberately kept lint-clean under the crate-root deny list
// (no blanket `#![allow(warnings)]`): it is only type definitions, derives and
// two trivial accessors.

use crate::setup::{LAS_CTILDEBYTES, N, N_PLUS_ELL};
use crate::types::R;
use zeroize::{Zeroize, ZeroizeOnDrop};

/* ---- The six protocol object types (vectors are plain arrays of the crate's
 * degree-256 polys).  Physically HERE so one codec (serialize.rs) serves all of
 * them and `basesig` never includes `las`; ownership is per the module-header
 * table.  `PublicParams` is deliberately NOT here — it is the shared public
 * setup `pp`, not a protocol object, and stays in setup.rs. ---- */

/// ALGORITHM 1 (Σ).  Paper `pk = t` where `t = A r` (Alg. 1 step 3),
/// canonical [0,Q).  Owner: `basesig`.
#[derive(Clone, PartialEq)]
pub struct PublicKey {
    pub(crate) t: [R; N], // paper t: the public key t = A r
}

/// ALGORITHM 1 (Σ).  Paper `sk = r` where `r <-$ S_1^(n+ell)` (Alg. 1
/// step 2; ternary, stored as exact -1/0/1).  Owner: `basesig`.
/// Zeroized on drop, mirroring the upstream crate's secret-material policy
/// (types.rs `PrivateKey` derives `Zeroize`/`ZeroizeOnDrop`).
#[derive(Clone, PartialEq, Zeroize, ZeroizeOnDrop)]
pub struct SecretKey {
    pub(crate) r: [R; N_PLUS_ELL], // paper r: the secret key
}

/// ALGORITHM 1 (Σ).  Paper `σ = (c, z)`: ordinary signature (Sign, Alg. 1
/// step 12) — and Adapt's output (Alg. 2 step 25, `σ = (c, ẑ + r')`), which
/// is this SAME type because an adapted pre-signature is an ordinary
/// signature checked by Algorithm-1 Verify.  Bound: `||z||inf <= gamma-kappa`.
/// Owner: `basesig`.  Deliberately NOT interchangeable with [`PreSignature`].
#[derive(Clone, PartialEq)]
pub struct Signature {
    /// Stored 32-byte representation of the paper challenge
    /// `c = H(pk, w, M)`.
    ///
    /// `Sign` derives the local arithmetic polynomial as
    /// `c = SampleInBall(c_tilde)` to compute `z = y + c r`.
    /// `Verify` derives the same polynomial to reconstruct the commitment.
    /// The polynomial `c` is temporary and is never stored or serialised.
    pub(crate) c_tilde: [u8; LAS_CTILDEBYTES],
    pub(crate) z: [R; N_PLUS_ELL], // paper z: response, z = y + c r
}

/// ALGORITHM 2.  Paper `σ̂ = (c, ẑ)`: pre-signature (PreSign, Alg. 2 step 7).
/// Owner: `las`.  A SEPARATE type from [`Signature`] because the two are
/// different protocol objects: produced by different algorithms (Sign vs
/// PreSign), verified by different algorithms (Verify vs PreVerify), bounded
/// differently (`gamma-kappa` vs the TIGHTER `gamma-kappa-1`, Alg. 2 steps
/// 6/11), only a pre-signature may enter Adapt, only a signature may enter
/// ordinary Verify, and they occupy different positions in
/// `Ext(Y, σ, σ̂)` (Alg. 2 step 27) — the type system enforces all of that.
#[derive(Clone, PartialEq)]
pub struct PreSignature {
    /// Stored 32-byte representation of the paper challenge
    /// `c = H(pk, w + t', M)` (the statement `Y = t'` folded into the hash).
    ///
    /// `PreSign` derives the local arithmetic polynomial as
    /// `c = SampleInBall(c_tilde)` to compute `ẑ = y + c r`.
    /// `PreVerify` derives the same polynomial to reconstruct the commitment.
    /// The polynomial `c` is temporary and is never stored or serialised; `Ext`
    /// (Alg. 2 step 28) never touches the challenge.
    pub(crate) c_tilde: [u8; LAS_CTILDEBYTES],
    pub(crate) z_hat: [R; N_PLUS_ELL], // paper ẑ: masked response, ẑ = y + c r
}

/// HARD RELATION R_A.  Paper statement `Y = t'` (Table 1; Alg. 2 parses
/// `t' := Y`).  Owner: `relation`.  A statement is pk-SHAPED (Gen runs
/// exactly as KeyGen, Section 3) but is NOT a [`PublicKey`] — the two are
/// deliberately non-interchangeable types.
#[derive(Clone, PartialEq)]
pub struct Statement(pub(crate) [R; N]);

impl Statement {
    /// The paper's parse `t' := Y` (Alg. 2 steps 4, 10, 29).
    pub(crate) fn as_t_prime(&self) -> &[R; N] {
        &self.0
    }
}

/// HARD RELATION R_A / R'_A.  Paper witness `y` in the pair `(Y, y)`
/// (Definition 3).  Owner: `relation`.  Storage is NEUTRAL — the same type
/// carries Gen's honest witness (`||.||inf <= 1`, relation R_A; Adapt locally
/// parses it as `r' := y`, Alg. 2 step 24) AND Ext's extracted witness
/// (paper-local `s = z - ẑ`, Alg. 2 step 30, only guaranteed in the extended
/// relation R'_A with `||.||inf <= 2(gamma-kappa)` — the knowledge gap, p.9).
/// Therefore NOT assumed ternary, and NOT a [`SecretKey`].
/// Zeroized on drop (witness secrecy is what makes the swap atomic).
#[derive(Clone, PartialEq, Zeroize, ZeroizeOnDrop)]
pub struct Witness(pub(crate) [R; N_PLUS_ELL]);

impl Witness {
    /// The paper's parse `r' := y` (Alg. 2 step 24) — used inside Adapt ONLY;
    /// Ext's output is the paper's `s`, never renamed to r'.
    pub(crate) fn as_relation_vector(&self) -> &[R; N_PLUS_ELL] {
        &self.0
    }

    /// Wrap a relation vector (Gen's sampled `r'`, or Ext's `s = z - ẑ`).
    pub(crate) fn from_relation_vector(v: [R; N_PLUS_ELL]) -> Self {
        Witness(v)
    }
}
