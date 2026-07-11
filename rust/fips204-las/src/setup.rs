//! `setup` — the SHARED system setup, i.e. the paper's `Setup() -> pp`
//! (eprint 2020/845): the public parameters (n, ell, kappa, gamma), the shared
//! object types, and the public matrix `A = [I | A']`, expanded once from a
//! public seed.  Rust twin of `ref/setup.{c,h}` (the C build's shared setup
//! layer).
//!
//! This layer is deliberately a SEPARATE module because it is NOT scheme
//! specific: BOTH `basesig.rs` (Algorithm 1, the base signature) and
//! `las.rs` (Algorithm 2, the adaptor scheme) consume the same `LasPp` by
//! parameter, and `serialize.rs` (the wire codec) encodes the same object
//! layouts.  The module layering mirrors the C build exactly
//! (`setup.h -> serialize.h -> basesig.c/las.c`, itself mirroring upstream's
//! `params/polyvec -> packing -> sign.c`):
//!
//! ```text
//!     setup.rs  ->  serialize.rs  ->  basesig.rs / las.rs
//! ```
//!
//! That is exactly why both scheme files carry [DELETED] notes for
//! `ml_dsa.rs`'s per-call `expand_a` (ml_dsa.rs:181, ml_dsa.rs:406): upstream
//! must re-expand A from rho inside every call because rho travels inside each
//! packed key, whereas here A is fixed public infrastructure set up once.
//!
//! `las_setup` itself calls the UNMODIFIED upstream `expand_a` (hashing.rs:225,
//! FIPS 204 Algorithm 32): the absorb order `seed || col || row` and the
//! 23-bit accept window of `rej_ntt_poly`/`coeff_from_three_bytes` are
//! byte-identical to the C `poly_uniform(&pp->mat[i][j], seed, (i << 8) + j)`
//! calls in `ref/setup.c` — the KAT digest (tests/las_kat.rs) proves it.

#![allow(warnings)]
#![allow(clippy::all, clippy::pedantic, clippy::nursery)]
// Named allows: crate root denies these BY NAME, and named lints outrank the
// `warnings` group regardless of scope depth.
#![allow(
    absolute_paths_not_starting_with_crate,
    dead_code,
    elided_lifetimes_in_paths,
    explicit_outlives_requirements,
    let_underscore_drop,
    macro_use_extern_crate,
    meta_variable_misuse,
    missing_abi,
    missing_docs,
    non_ascii_idents,
    single_use_lifetimes,
    trivial_casts,
    trivial_numeric_casts,
    unreachable_pub,
    unused_extern_crates,
    unused_import_braces,
    unused_lifetimes,
    unused_macro_rules,
    unused_qualifications,
    unused_results,
    variant_size_differences
)]

use crate::hashing::expand_a;
use crate::types::{R, T};
use zeroize::{Zeroize, ZeroizeOnDrop};

/* ---- LAS parameters (paper Section 3 / Table 1), Simplified Dilithium-III set.
 * Mirrors ref/setup.h with -DLAS_N=6 -DLAS_ELL=5 -DLAS_KAPPA=49 (the parameter
 * set pinned by the C KAT build, `make test/test_kat3`).  The C build makes
 * n/ell/kappa compile-time overridable to sweep parameter sets; the Rust port
 * is fixed at this one set. ---- */

/// n: rows of A, dimension of t (=Y).
pub const LAS_N: usize = 6;
/// ell: extra columns of A.
pub const LAS_ELL: usize = 5;
/// n+ell: dimension of r, y, z.
pub const LAS_M: usize = LAS_N + LAS_ELL;
/// kappa: challenge weight ||c||_1.
pub const LAS_KAPPA: i32 = 49;
/// gamma = kappa * d * (n+ell), d = 256.
pub const LAS_GAMMA: i32 = LAS_KAPPA * 256 * (LAS_M as i32);
/// Seed length in bytes.
pub const LAS_SEEDBYTES: usize = 32;

/// Sign/Verify rejection bound, SHARED by both schemes (chknorm-style: reject
/// `>= bound`, so the strict `>` test is encoded as bound = limit+1):
/// basesig.rs's Algorithm-1 Sign/Verify and las.rs's Sign/Verify reject at
/// this same `|z|inf > gamma-kappa`, and an Adapted pre-signature must clear
/// exactly this bound — so it lives HERE, below both schemes (the
/// adaptor-only PreSign bound stays in las.rs).  Mirrors `ref/setup.h`.
pub const LAS_BOUND_SIGN: i32 = LAS_GAMMA - LAS_KAPPA + 1;

/* ---- Shared object types (vectors are plain arrays of the crate's degree-256
 * polys).  These live HERE, below both schemes, for the same reason upstream
 * keeps its vector types in types.rs rather than ml_dsa.rs: the key/signature
 * LAYOUT is common infrastructure — basesig.rs, las.rs and
 * serialize.rs all operate on the same structs, which is what makes a
 * LAS-adapted signature verifiable by the independent base verifier and one
 * codec serve both schemes.  A' is stored in the NTT domain (type T), exactly
 * like the C `las_pp.mat`. ---- */

/// Public parameters pp = A = [I | A'] expanded from a public seed (A' in NTT domain).
#[derive(Clone)]
pub struct LasPp {
    pub(crate) mat: [[T; LAS_ELL]; LAS_N],
    pub(crate) seed: [u8; LAS_SEEDBYTES],
}

/// Public key / statement: t = A r, canonical [0,Q).
#[derive(Clone, PartialEq)]
pub struct LasPk {
    pub(crate) t: [R; LAS_N], // paper t (public key t = A r), or t′ = Y when used as a statement
}

/// Secret key / witness: r in S_1 (ternary, stored as exact -1/0/1).
/// Zeroized on drop, mirroring the upstream crate's secret-material policy
/// (types.rs `PrivateKey` derives `Zeroize`/`ZeroizeOnDrop`); the same type
/// carries the adaptor witness, so extracted witnesses are wiped too.
#[derive(Clone, PartialEq, Zeroize, ZeroizeOnDrop)]
pub struct LasSk {
    pub(crate) s: [R; LAS_M], // paper r (secret key / witness); also the extracted witness s in Ext
}

/// (Pre-)signature (c, z): c ternary challenge, z exact centred response.
#[derive(Clone, PartialEq)]
pub struct LasSig {
    pub(crate) c: R,          // paper c: the challenge
    pub(crate) z: [R; LAS_M], // paper z (signature) / ẑ (pre-signature): the response
}

/// `las_setup` (shared system setup — consumed by `basesig.rs` AND
/// `las.rs`; no basesig.rs/ml_dsa.rs slot): public parameters
/// pp = A = [I | A'], with A' expanded from a public seed into the NTT domain
/// by the UNMODIFIED upstream `expand_a` (hashing.rs:225).  A is expanded ONCE
/// here and passed BY PARAMETER into every basesig.rs and las.rs scheme
/// function alike — that is why both scheme files carry [DELETED] notes for
/// the per-call `expand_a` (ml_dsa.rs:181, ml_dsa.rs:406).
///
/// Byte-identity with `ref/setup.c las_setup`: upstream `expand_a` absorbs
/// `seed || IntegerToBytes(s,1) || IntegerToBytes(r,1)` = `seed || col || row`
/// per poly, which equals the C `poly_uniform(seed, (row << 8) + col)` nonce
/// bytes (little-endian 16-bit: low byte = col, high byte = row), and
/// `coeff_from_three_bytes` applies the same 23-bit mask + reject-≥-Q window
/// as the C `rej_uniform`.
pub fn las_setup(seed: &[u8; LAS_SEEDBYTES]) -> LasPp {  // returns paper A = [I | A']
    LasPp {
        // paper A: mat = A' (NTT domain); the [I | ·] identity block is applied
        // by the schemes' matrix-product helpers (b_amul / las_amul)
        mat: expand_a::<false, LAS_N, LAS_ELL>(seed),
        seed: *seed,
    }
}
