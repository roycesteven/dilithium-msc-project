//! `setup` — the SHARED system setup, i.e. the paper's public parameters
//! `pp = (A, H)` (eprint 2020/845, Section 3): the construction parameters
//! (n, ell, kappa, gamma), the `PublicParams` type, and `setup_public_params`,
//! which expands the public matrix `A = [I | A']` once from a public seed
//! (`H` is the fixed SHAKE256-based challenge hash, realised in the scheme
//! modules, not a stored field).  Rust twin of `ref/setup.{c,h}`.
//!
//! The six LAS PROTOCOL object types (PublicKey, SecretKey, Signature,
//! Statement, Witness, PreSignature) live in the sibling module
//! [`crate::las_types`] (C twin `ref/las_types.h`), which sits directly below
//! this one; `PublicParams` stays HERE because it is the system infrastructure
//! the parameters are expanded into, not a protocol object.
//!
//! Dependency layering (each module uses only modules listed above it — a DAG,
//! not a single chain): `setup` (parameters + `PublicParams`) is used by
//! `las_types` (the six object types), which in turn is used by `relation`,
//! `serialize` and both scheme modules.  `serialize` and both schemes each
//! import `las_types` directly; `basesig` additionally uses `serialize`; `las`
//! additionally uses `relation` and `serialize`.  C build headers follow the
//! same order: `setup.h -> las_types.h -> {relation.h, serialize.h} ->
//! {basesig.c, las.c}`.
//!
//! Both scheme files carry [DELETED] notes for `ml_dsa.rs`'s per-call
//! `expand_a` (ml_dsa.rs:181, ml_dsa.rs:406): upstream must re-expand A from
//! rho inside every call because rho travels inside each packed key, whereas
//! here A is fixed public infrastructure set up once.
//!
//! `setup_public_params` itself calls the UNMODIFIED upstream `expand_a`
//! (hashing.rs:225, FIPS 204 Algorithm 32): the absorb order
//! `seed || col || row` and the 23-bit accept window of
//! `rej_ntt_poly`/`coeff_from_three_bytes` match the C
//! `poly_uniform(&pp->a_prime[i][j], seed, (i << 8) + j)` calls in
//! `ref/setup.c` — the pinned KAT digest (tests/las_kat.rs) cross-checks that
//! byte agreement.

// This module is deliberately kept lint-clean under the crate-root deny list
// (no blanket `#![allow(warnings)]` / clippy suppression): it is only the
// construction parameters, the `PublicParams` type, and `setup_public_params`.

use crate::hashing::expand_a;
use crate::types::T;

/* ---- LAS construction parameters (paper Section 3 / Table 1), Simplified
 * Dilithium-III set.  Mirrors ref/setup.h with -DLAS_N=6 -DLAS_ELL=5
 * -DLAS_KAPPA=49 (the parameter set pinned by the C KAT build,
 * `make test/test_kat3`).  The C build makes n/ell/kappa compile-time
 * overridable to sweep parameter sets; the Rust port is fixed at this one set.
 *
 * These are the SHARED construction parameters — every layer (las_types,
 * relation, serialize, basesig, las) consumes them by import, so they live in
 * this setup module.  They are named after the paper's Table-1 identifiers with
 * NO `LAS_` prefix (n -> N, ell -> ELL, kappa -> KAPPA, gamma -> GAMMA, ring
 * degree d -> D); the C twins keep `LAS_N`/`LAS_D`/... only because C's
 * params.h already defines bare `N`=256 and `D`=13 for the reused Dilithium
 * primitives, which the -DLAS_N sweep must not collide with.  The two
 * per-scheme rejection bounds do NOT live here: `BOUND_SIGN` (Algorithm 1)
 * belongs to `basesig`, `BOUND_PRESIGN` (Algorithm 2) belongs to `las`. ---- */

/// paper n: rows of A, dimension of t (and of the statement t' = Y).
pub const N: usize = 6;
/// paper ell: extra columns of A.
pub const ELL: usize = 5;
/// paper n+ell: dimension of r, y, z, z_hat.  NOT the paper's M — in
/// Algorithms 1 and 2, M denotes the MESSAGE; this array length is n + ell.
pub const N_PLUS_ELL: usize = N + ELL;
/// paper d: ring degree, R_q = Z_q[X]/(X^d + 1).  Equals the reused upstream
/// FIPS 204 ring degree (256).  C twin: `LAS_D` (= params.h `N`), the alias
/// that dodges the params.h `N`=256 / `D`=13 macros.
pub const D: usize = 256;
/// paper kappa: challenge weight ||c||_1.
pub const KAPPA: i32 = 49;
/// paper gamma = kappa * d * (n+ell).
pub const GAMMA: i32 = KAPPA * (D as i32) * (N_PLUS_ELL as i32);
/// Seed length in bytes.
pub const LAS_SEEDBYTES: usize = 32;
/// Challenge-hash length in bytes: the stored digest `c_tilde`, the
/// implementation realisation of the paper's `H : {0,1}* -> C` (the paper's
/// challenge `c` IS this hash — eq. 7 counts it as the 32-byte term of `|sigma|`).
/// Twin of upstream `CTILDEBYTES` (params.h) / `LAMBDA_DIV4` (ml_dsa.rs); the
/// same value as `LAS_SEEDBYTES`, but a DISTINCT knob, as upstream keeps the two
/// separate.  Stored in the `Signature`/`PreSignature` types (las_types.rs); the
/// challenge polynomial `c = SampleInBall(c_tilde)` is only ever a local
/// arithmetic value, never serialised.
pub const LAS_CTILDEBYTES: usize = 32;

/* ---- The construction-wide public parameters `pp = (A, H)`.  A' is held in
 * the NTT domain (type T), as the C `public_params.a_prime` holds it.  The six
 * protocol object types live in `las_types.rs`. ---- */

/// CONSTRUCTION-WIDE.  Paper `pp = (A, H)` (Section 3): the public matrix
/// `A = [I | A']` expanded from a public seed (A' in NTT domain).  `H` is the
/// fixed hash implementation, not a field.  One concrete type, passed by
/// parameter into every procedure of every layer.
#[derive(Clone)]
pub struct PublicParams {
    pub(crate) a_prime: [[T; ELL]; N], // paper A': the non-identity block of A = [I | A']
    // Kept for parity with the C `public_params.seed`: the schemes read only
    // `a_prime`, so nothing reads this field in the Rust build (the seed is
    // public and A is re-derivable from it).  A precise, justified suppression
    // rather than a module-wide blanket allow.
    #[allow(dead_code)]
    pub(crate) seed: [u8; LAS_SEEDBYTES],
}

/// `setup_public_params` (construction-wide system setup — consumed by every
/// layer alike; no basesig.rs/ml_dsa.rs slot): the public parameters
/// `pp = (A, H)`, with `A = [I | A']` expanded from a public seed into the NTT
/// domain by the UNMODIFIED upstream `expand_a` (hashing.rs:225).  A is expanded
/// ONCE here and passed BY PARAMETER into every scheme function — that is why
/// both scheme files carry [DELETED] notes for the per-call `expand_a`
/// (ml_dsa.rs:181, ml_dsa.rs:406).
///
/// Byte-identity with `ref/setup.c setup_public_params`: upstream `expand_a`
/// absorbs `seed || IntegerToBytes(s,1) || IntegerToBytes(r,1)` =
/// `seed || col || row` per poly, which equals the C
/// `poly_uniform(seed, (row << 8) + col)` nonce bytes (little-endian 16-bit:
/// low byte = col, high byte = row), and `coeff_from_three_bytes` applies the
/// same 23-bit mask + reject-≥-Q window as the C `rej_uniform`.
pub fn setup_public_params(seed: &[u8; LAS_SEEDBYTES]) -> PublicParams {  // returns paper pp = (A, H); A = [I | A']
    PublicParams {
        // paper A': the [I | ·] identity block is applied by the schemes'
        // matrix-product helpers (b_mat_vec_mul / the A-product twins)
        a_prime: expand_a::<false, N, ELL>(seed),
        seed: *seed,
    }
}
