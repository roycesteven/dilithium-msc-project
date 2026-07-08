//! LAS — Lattice-based Adaptor Signature (Esgin–Ersoy–Erkin, eprint 2020/845,
//! Algorithm 2), layered ADDITIVELY on this crate's FIPS 204 primitives.
//!
//! STRUCTURED AS A MIRROR of `las_basesig.rs` (which itself mirrors the
//! vendored upstream `ml_dsa.rs`), so provenance tracks to the uppermost
//! upstream by a uniform name chain (same convention as the C build
//! ref/sign.c -> ref/basesig.c -> ref/las.c):
//!
//!   -- Algorithm 1 (base path) --
//!   key_gen (ml_dsa.rs:26)          -> base_sign_keypair            -> las_keypair
//!   key_gen_internal (ml_dsa.rs:57) -> base_sign_keypair_seed       -> las_keypair_seed
//!   sign_internal (ml_dsa.rs:153)   -> base_sign_signature_internal -> las_signature_internal
//!   (lib.rs try_sign_with_rng)      -> base_sign_signature          -> las_signature
//!   verify_internal (ml_dsa.rs:351) -> base_sign_verify_internal    -> las_verify_internal
//!   (lib.rs verify)                 -> base_sign_verify             -> las_verify
//!   plus a LAS-only deterministic KAT slot: las_signature_det.
//!
//!   -- Algorithm 2 (adaptor layer; upstream = the PAPER, names kept) --
//!   las_presign_internal / las_presign / las_presign_det /
//!   las_preverify_internal / las_preverify / las_adapt / las_ext.
//!
//! The local helpers are defined at the BOTTOM of the file, in the same order
//! as `las_basesig.rs`'s (ml_dsa.rs keeps its equivalents in hashing.rs /
//! helpers.rs / ntt.rs), so the scheme functions read side by side.
//!
//! This module is a Rust port of the C implementation `ref/las.c` from the
//! dilithium-msc-project repository (deterministic path only: seeded KeyGen,
//! deterministic Sign/PreSign, Verify/PreVerify/Adapt/Ext).  It is
//! byte-for-byte cross-checked against the C known-answer test
//! (`ref/test/test_kat.c`, pinned SHAKE256 digest) — see `tests/las_kat.rs`.
//!
//! Methodology mirrors the C build exactly: ZERO upstream functions are
//! modified; LAS only *calls* the crate's mode-independent primitives
//! (`ntt`, `inv_ntt`, `mont_reduce`, reductions) plus SHAKE via the `sha3`
//! dependency, and adds its own self-contained samplers / challenge / scheme,
//! just as the C version reuses the pq-crystals reference primitives as-is.
//!
//! Parameter set: the "Simplified Dilithium-III" engineering set
//! (n=6, ell=5, kappa=49) — the set pinned by the C KAT build
//! (`make test/test_kat3`, flags `-DLAS_N=6 -DLAS_ELL=5 -DLAS_KAPPA=49`).
//!
//! Equivalence-to-C notes (why byte equality holds despite pipeline
//! differences):
//!  * `helpers::mont_reduce` is bit-identical to the C `montgomery_reduce`.
//!  * `ZETA_TABLE_MONT` is congruent mod Q to the C `zetas[]` table (positive
//!    vs signed representatives), and both NTTs implement the standard
//!    FIPS 204 transform, so sampling A' "in the NTT domain" yields the SAME
//!    effective matrix A' = InvNTT(U).
//!  * every value that is hashed, packed or compared is either canonicalised
//!    to [0,Q) first, or is an exact small centred value (|.| << Q/2) whose
//!    representative is unique — so representative differences between the
//!    two pipelines cancel.

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

use core::array::from_fn;
use core::sync::atomic::{AtomicU64, Ordering};

use rand_core::CryptoRngCore;
use sha3::digest::{ExtendableOutput, Update, XofReader};
use sha3::{Shake128, Shake256};
use zeroize::{Zeroize, ZeroizeOnDrop};

use crate::helpers::{center_mod, full_reduce32, mont_reduce, partial_reduce32, to_mont};
use crate::ntt::{inv_ntt, ntt};
use crate::types::{R, R0, T, T0};
use crate::Q;

/* ---- LAS parameters (paper Section 3 / Table 1), Simplified Dilithium-III set.
 * Mirrors ref/las.h with -DLAS_N=6 -DLAS_ELL=5 -DLAS_KAPPA=49. ---- */

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

/// Sign/Verify rejection bound: reject |z|inf  > gamma-kappa   (chknorm-style: >= bound).
pub const LAS_BOUND_SIGN: i32 = LAS_GAMMA - LAS_KAPPA + 1;
/// PreSign/PreVerify rejection bound: reject |z^|inf > gamma-kappa-1 (the tighter -1 budget).
pub const LAS_BOUND_PRESIGN: i32 = LAS_GAMMA - LAS_KAPPA;

const N: usize = 256;
const SHAKE256_RATE: usize = 136;

/// Rejection-sampling attempt counter (measurement only; mirrors the C
/// `las_attempts`).  Incremented once per rejection-loop iteration in
/// `las_signature_internal`/`las_presign_internal`; never read by the scheme
/// itself.  Benchmarks reset it and read it to report the restart rate
/// DIRECTLY rather than estimating it from a timing ratio.  Relaxed ordering:
/// single-threaded benchmark instrumentation, not synchronisation.
pub static LAS_ATTEMPTS: AtomicU64 = AtomicU64::new(0);

/// EXACT expected attempts/call of the rejection loop running at `bound`
/// (`LAS_BOUND_SIGN` or `LAS_BOUND_PRESIGN`), for validating a measured
/// attempt counter against theory. Derivation, verified against eprint
/// 2020/845 (Esgin-Ersoy-Erkin):
///
/// The mask y is drawn from S_gamma^(n+ell), i.e. every coefficient uniform
/// on [-GAMMA, GAMMA] — 2*GAMMA+1 values (Table 1: S_c = {f : |f|inf <= c}).
/// The secret-dependent shift obeys |c*r|inf <= KAPPA (the paper's Fact 1),
/// and the chknorm acceptance window |z_i| <= bound-1 (2*bound-1 values,
/// matching Alg. 1 step 11 "reject |z|inf > gamma-kappa" resp. Alg. 2 step 6
/// "reject |z^|inf > gamma-kappa-1") therefore always lies inside the shifted
/// mask support whenever bound <= GAMMA-KAPPA+1: each coefficient accepts
/// independently with probability exactly (2*bound-1)/(2*GAMMA+1), regardless
/// of the secret. One attempt accepts iff all (n+ell)*d coefficients do, and
/// the attempt count is geometric, so
///   E[attempts] = ((2*bound-1)/(2*GAMMA+1))^-((n+ell)*d).
/// With gamma = kappa*d*(n+ell) this is ~ e — the exact form of the paper's
/// design target "the average number of restarts in Sign and PreSign is
/// about e < 3" (Section 3.2). At this build's D3 engineering set it gives
/// Sign 2.7188 and PreSign 2.7748 attempts/call.
pub fn las_expected_attempts(bound: i32) -> f64 {
    // p^((n+ell)*d) via square-and-multiply, then reciprocal — mirrors the C
    // `las_expected_attempts` exactly (no libm `powf`/`powi`, so this stays
    // usable in the crate's `#![no_std]` build; `f64::powi` is std-only).
    let mut p = f64::from(2 * bound - 1) / f64::from(2 * LAS_GAMMA + 1);
    let mut acc = 1.0_f64;
    let mut e = (LAS_M * N) as u32; // (n+ell)*d coefficients
    while e != 0 {
        if e & 1 == 1 {
            acc *= p;
        }
        p *= p;
        e >>= 1;
    }
    1.0 / acc
}

/* ---- Types (vectors are plain arrays of the crate's degree-256 polys).
 * A' is stored in the NTT domain (type T), exactly like the C `las_pp.mat`. ---- */

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

/* ==================== scheme, Algorithm 1 (base path) ==================== */

/// `las_setup` (no las_basesig.rs/ml_dsa.rs analogue): public parameters
/// pp = A (A' expanded from a public seed, NTT domain).  las_basesig.rs
/// consumes this same `LasPp` (A is shared public infrastructure).
pub fn las_setup(seed: &[u8; LAS_SEEDBYTES]) -> LasPp {  // returns paper A = [I | A']
    LasPp {
        // paper A: mat = A' (NTT domain); the [I | ·] identity block is applied in `amul`
        mat: from_fn(|i| from_fn(|j| poly_uniform_ntt(seed, ((i as u16) << 8) + j as u16))),
        seed: *seed,
    }
}

/// `las_keypair` <-> `base_sign_keypair` (las_basesig.rs) <-> `key_gen`
/// (ml_dsa.rs:26): draw a fresh seed, then the deterministic KeyGen body.
/// The RNG is injected (Rust idiom).  Also used to make the statement/witness
/// pair (Y, y) -- it is literally a key pair.
pub fn las_keypair(
    pp: &LasPp,                    // paper A: pp = A = [I | A']
    rng: &mut impl CryptoRngCore,  // CSPRNG for the seed (no paper symbol)
) -> (LasPk, LasSk) {              // returns (paper t, paper r) = (pk, sk)
    let mut seed = [0u8; LAS_SEEDBYTES]; // PRG seed to sample r (no paper symbol)
    rng.fill_bytes(&mut seed);
    let out = las_keypair_seed(pp, &seed);
    seed.zeroize(); // sk is derivable from this seed: wipe
    out
}

/// `las_keypair_seed` <-> `base_sign_keypair_seed` (las_basesig.rs) <->
/// `key_gen_internal` (ml_dsa.rs:57): deterministic KeyGen from an explicit
/// 32-byte seed (reproducible KATs).
/// KeyGen = Gen: r <- S_1^(n+l); t = A r; (pk,sk) = (t,r).
pub fn las_keypair_seed(
    pp: &LasPp,                  // paper A: pp = A = [I | A']
    seed: &[u8; LAS_SEEDBYTES],  // PRG seed to sample r (no paper symbol)
) -> (LasPk, LasSk) {
    // [PAPER Alg.1] 1:  procedure KeyGen():    // same as Gen
    // [PAPER Alg.1] 2:      r ←$ S₁^(n+ℓ)
    let sk = LasSk {
        s: from_fn(|j| sample_ternary(seed, j as u16)), // paper r: sk.s = r
    };
    // [PAPER Alg.1] 3:      t = A r
    let pk = LasPk {
        t: amul(pp, &sk.s), // paper t: pk.t = t = A r
    };
    // [PAPER Alg.1] 4:      return (pk, sk) = (t, r)
    // [PAPER Alg.1] 5:  end procedure
    (pk, sk)
}

/// `las_signature_internal` <-> `base_sign_signature_internal` (las_basesig.rs)
/// <-> `sign_internal` (ml_dsa.rs:153): Algorithm 1 Sign body, parameterised
/// by the 64-byte mask seed (random or derived).
/// NTT hoisting mirrors ref/las.c (which mirrors ref/sign.c / ml_dsa.rs):
/// NTT(s_j) once per call (invariant across rejection attempts), NTT(c) once
/// per attempt (shared by all n+ell products).
pub(crate) fn las_signature_internal(
    m: &[u8],          // paper M: message
    pk: &LasPk,        // paper t: pk.t = t (public key)
    sk: &LasSk,        // paper r: sk.s = r (secret key)
    pp: &LasPp,        // paper A: pp = A = [I | A']
    seed: &[u8; 64],   // PRG mask seed (no paper symbol)
) -> LasSig {          // paper σ: returns σ = (c, z)
    // [PAPER Alg.1] 6:  procedure Sign((pk, sk), M):
    let mut nonce: u16 = 0; // PRG counter (no paper symbol)
    let s_hat_mont: [T; LAS_M] = from_fn(|j| ntt_b_mont(&sk.s[j])); // paper r in NTT domain: NTT(r) once per call
    loop {
        LAS_ATTEMPTS.fetch_add(1, Ordering::Relaxed); // instrumentation only
        // [PAPER Alg.1] 7:      y ←$ Sγ^(n+ℓ)
        let mut y: [R; LAS_M] = [R0; LAS_M]; // paper y: mask, y <-$ Sγ^(n+ℓ)
        for j in 0..LAS_M {
            y[j] = sample_sgamma(seed, nonce);
            nonce = nonce.wrapping_add(1);
        }
        // [PAPER Alg.1] 8:      w = A y
        let w = amul(pp, &y); // paper w: commitment w = A y
        // [PAPER Alg.1] 9:      c = H(pk, w, M)
        let c = hash_challenge(pk, &w, m); // paper c: challenge c = H(pk, w, M)
        let c_hat = ntt_a(&c); // paper c in NTT domain: NTT(c) once per attempt

        // [PAPER Alg.1] 10:     z = y + c r, where r := sk
        let mut z: [R; LAS_M] = [R0; LAS_M]; // paper z: response z = y + c r
        for j in 0..LAS_M {
            let cr = polymul_prehat(&c_hat, &s_hat_mont[j]); // paper c·r: exact, |.|inf <= kappa
            for n in 0..N {
                z[j].0[n] = y[j].0[n] + cr.0[n]; // z = y + c r (exact, C reduce = identity)
            }
        }
        // [PAPER Alg.1] 11:     if ||z||∞ > γ − κ, then Restart
        if chknorm_vec(&z, LAS_BOUND_SIGN) {
            continue;
        }
        // [PAPER Alg.1] 12:     return σ = (c, z)
        return LasSig { c, z };
    }
    // [PAPER Alg.1] 13: end procedure
}

/// `las_signature` <-> `base_sign_signature` (las_basesig.rs): Algorithm 1
/// Sign, random path — fresh 64-byte mask seed per call, then the internal
/// (mirrors C `las_signature`; upstream's RNG wrapper is lib.rs
/// `try_sign_with_rng`).
pub fn las_signature(
    m: &[u8],                      // paper M: message
    pk: &LasPk,                    // paper t: pk.t = t (public key)
    sk: &LasSk,                    // paper r: sk.s = r (secret key)
    pp: &LasPp,                    // paper A: pp = A = [I | A']
    rng: &mut impl CryptoRngCore,  // CSPRNG for the mask seed (no paper symbol)
) -> LasSig {                      // paper σ: returns σ = (c, z)
    let mut seed = [0u8; 64]; // PRG mask seed (no paper symbol)
    rng.fill_bytes(&mut seed);
    let sig = las_signature_internal(m, pk, sk, pp, &seed);
    seed.zeroize(); // mask seed: knowing it + sig reveals c*r, hence r
    sig
}

/// `las_signature_det` (no las_basesig.rs/ml_dsa.rs slot; deterministic KAT
/// path): mask randomness derived from (sk, M).  Mirrors C `las_signature_det`.
pub fn las_signature_det(
    m: &[u8],    // paper M: message
    pk: &LasPk,  // paper t: pk.t = t (public key)
    sk: &LasSk,  // paper r: sk.s = r (secret key)
    pp: &LasPp,  // paper A: pp = A = [I | A']
) -> LasSig {    // paper σ: returns σ = (c, z)
    let mut seed = det_seed(0, sk, None, m); // PRG mask seed from (sk, M); tag 0 = sign (no statement)
    let sig = las_signature_internal(m, pk, sk, pp, &seed);
    seed.zeroize(); // sk-derived mask seed: wipe
    sig
}

/// `las_verify_internal` <-> `base_sign_verify_internal` (las_basesig.rs)
/// <-> `verify_internal` (ml_dsa.rs:351): Algorithm 1 Verify body.
pub(crate) fn las_verify_internal(
    sig: &LasSig,  // paper σ: sig = (c, z), signature to verify
    m: &[u8],      // paper M: message
    pk: &LasPk,    // paper t: pk.t = t (public key)
    pp: &LasPp,    // paper A: pp = A = [I | A']
) -> bool {
    // [PAPER Alg.1] 14: procedure Verify(pk, σ, M):
    // [PAPER Alg.1] 15:     Parse (c, z) := σ
    // [PAPER Alg.1] 16:     if ||z||∞ > γ − κ, then return 0
    if chknorm_vec(&sig.z, LAS_BOUND_SIGN) {
        return false;
    }
    // [PAPER Alg.1] 17:     w′ = A z − c t, where t := pk
    let mut w = amul(pp, &sig.z); // paper w′: w′ = A z − c t (starts as A z)
    let c_hat = ntt_a(&sig.c); // paper c in NTT domain: NTT(c) once per call
    for j in 0..LAS_N {
        let ct = polymul_prehat(&c_hat, &ntt_b_mont(&pk.t[j])); // paper c·t: the product c t
        for n in 0..N {
            // w' = A z - c t, canonicalised (C: poly_sub; poly_reduce; poly_caddq)
            w[j].0[n] = full_reduce32(w[j].0[n] - ct.0[n]);
        }
    }
    // [PAPER Alg.1] 18:     if c ≠ H(pk, w′, M), then return 0
    // [PAPER Alg.1] 19:     return 1
    let c2 = hash_challenge(pk, &w, m); // paper H(pk, w′, M): recomputed challenge
    c2 == sig.c
    // [PAPER Alg.1] 20: end procedure
}

/// `las_verify` <-> `base_sign_verify` (las_basesig.rs): Algorithm 1 Verify,
/// public entry point (delegates to the internal, exactly like the C build).
/// Returns true iff the signature is valid.
pub fn las_verify(
    sig: &LasSig,  // paper σ: sig = (c, z), signature to verify
    m: &[u8],      // paper M: message
    pk: &LasPk,    // paper t: pk.t = t (public key)
    pp: &LasPp,    // paper A: pp = A = [I | A']
) -> bool {
    las_verify_internal(sig, m, pk, pp)
}

/* =============== scheme, Algorithm 2 (adaptor layer) ===============
 * No las_basesig.rs/ml_dsa.rs analogue from here on: these are the adaptor
 * operations LAS adds on top of the base signature (upstream = the PAPER). */

/// `las_presign_internal` (adaptor twin of `las_signature_internal`; was C
/// `las_presign_internal`): like the Sign body but hashes (w+Y) and rejects
/// at `bound` (gamma-kappa-1 single-hop).
pub(crate) fn las_presign_internal(
    m: &[u8],          // paper M: message
    y_stmt: &LasPk,    // paper t′ := Y: statement, y_stmt.t = Y = A y_wit
    pk: &LasPk,        // paper t: pk.t = t (public key)
    sk: &LasSk,        // paper r: sk.s = r (secret key)
    pp: &LasPp,        // paper A: pp = A = [I | A']
    bound: i32,        // paper γ−κ−1 (single-hop) / γ−κ−K (AMHL)
    seed: &[u8; 64],   // PRG mask seed (no paper symbol)
) -> LasSig {          // paper σ̂: returns σ̂ = (c, ẑ)
    // [PAPER Alg.2] 1:  procedure PreSign((pk, sk), Y, M):
    let mut nonce: u16 = 0; // PRG counter (no paper symbol)
    let s_hat_mont: [T; LAS_M] = from_fn(|j| ntt_b_mont(&sk.s[j])); // paper r in NTT domain: NTT(r) once per call
    loop {
        LAS_ATTEMPTS.fetch_add(1, Ordering::Relaxed); // instrumentation only
        // [PAPER Alg.2] 2:      y ←$ Sγ^(n+ℓ)
        let mut y: [R; LAS_M] = [R0; LAS_M]; // paper y: mask, y <-$ Sγ^(n+ℓ)
        for j in 0..LAS_M {
            y[j] = sample_sgamma(seed, nonce);
            nonce = nonce.wrapping_add(1);
        }
        // [PAPER Alg.2] 3:      w = A y
        let w = amul(pp, &y); // paper w: commitment w = A y
        // [PAPER Alg.2] 4:      c = H(pk, w + t′, M), where t′ := Y
        let mut w_y: [R; LAS_N] = [R0; LAS_N]; // paper w + t′: the hashed commitment w + Y
        for j in 0..LAS_N {
            for n in 0..N {
                // commit = w + Y, canonical (C: poly_add; poly_reduce; poly_caddq)
                w_y[j].0[n] = full_reduce32(w[j].0[n] + y_stmt.t[j].0[n]);
            }
        }
        let c = hash_challenge(pk, &w_y, m); // paper c: challenge c = H(pk, w+Y, M)
        let c_hat = ntt_a(&c); // paper c in NTT domain: NTT(c) once per attempt

        // [PAPER Alg.2] 5:      ẑ = y + c r, where r := sk
        let mut z: [R; LAS_M] = [R0; LAS_M]; // paper ẑ: pre-sig response ẑ = y + c r
        for j in 0..LAS_M {
            let cr = polymul_prehat(&c_hat, &s_hat_mont[j]); // paper c·r: the product c r
            for n in 0..N {
                z[j].0[n] = y[j].0[n] + cr.0[n]; // z^ = y + c r
            }
        }
        // [PAPER Alg.2] 6:      if ||ẑ||∞ > γ − κ − 1, then Restart
        if chknorm_vec(&z, bound) {
            continue;
        }
        // [PAPER Alg.2] 7:      return σ̂ = (c, ẑ)
        return LasSig { c, z };
    }
    // [PAPER Alg.2] 8:  end procedure
}

/// `las_presign` (adaptor twin of `las_signature`): PreSign(sk, Y, M),
/// random path — fresh 64-byte mask seed per call, single-hop bound
/// gamma-kappa-1 (mirrors C `las_presign`).
pub fn las_presign(
    m: &[u8],                      // paper M: message
    y_stmt: &LasPk,                // paper t′ := Y: statement
    pk: &LasPk,                    // paper t: pk.t = t (public key)
    sk: &LasSk,                    // paper r: sk.s = r (secret key)
    pp: &LasPp,                    // paper A: pp = A = [I | A']
    rng: &mut impl CryptoRngCore,  // CSPRNG for the mask seed (no paper symbol)
) -> LasSig {                      // paper σ̂: returns σ̂ = (c, ẑ)
    let mut seed = [0u8; 64]; // PRG mask seed (no paper symbol)
    rng.fill_bytes(&mut seed);
    let presig = las_presign_internal(m, y_stmt, pk, sk, pp, LAS_BOUND_PRESIGN, &seed);
    seed.zeroize(); // mask seed: wipe
    presig
}

/// `las_presign_det` (adaptor twin of `las_signature_det`; KAT path):
/// mask randomness derived from (sk, Y, M); single-hop bound gamma-kappa-1.
/// Mirrors C `las_presign_det`.
pub fn las_presign_det(
    m: &[u8],          // paper M: message
    y_stmt: &LasPk,    // paper t′ := Y: statement
    pk: &LasPk,        // paper t: pk.t = t (public key)
    sk: &LasSk,        // paper r: sk.s = r (secret key)
    pp: &LasPp,        // paper A: pp = A = [I | A']
) -> LasSig {          // paper σ̂: returns σ̂ = (c, ẑ)
    let mut seed = det_seed(1, sk, Some(y_stmt), m); // PRG mask seed from (sk, Y, M); tag 1 = presign (binds Y)
    let presig = las_presign_internal(m, y_stmt, pk, sk, pp, LAS_BOUND_PRESIGN, &seed);
    seed.zeroize(); // sk-derived mask seed: wipe
    presig
}

/// `las_preverify_internal` (adaptor twin of `las_verify_internal`):
/// PreVerify body, parameterised by the rejection bound.
pub(crate) fn las_preverify_internal(
    presig: &LasSig,   // paper σ̂: presig = (c, ẑ), pre-sig to verify
    m: &[u8],          // paper M: message
    y_stmt: &LasPk,    // paper t′ := Y: statement, y_stmt.t = Y
    pk: &LasPk,        // paper t: pk.t = t (public key)
    pp: &LasPp,        // paper A: pp = A = [I | A']
    bound: i32,        // paper γ−κ−1 (single-hop) / γ−κ−K (AMHL)
) -> bool {
    // [PAPER Alg.2] 9:  procedure PreVerify(Y, pk, σ̂, M):
    // [PAPER Alg.2] 10:     Parse (c, ẑ) := σ̂ and t′ := Y
    // [PAPER Alg.2] 11:     if ||ẑ||∞ > γ − κ − 1 then
    // [PAPER Alg.2] 12:         return 0
    // [PAPER Alg.2] 13:     end if
    if chknorm_vec(&presig.z, bound) {
        return false;
    }
    // [PAPER Alg.2] 14:     w′ = A ẑ − c t, where t := pk
    let mut w = amul(pp, &presig.z); // paper w′: w′ = A ẑ − c t (starts as A ẑ)
    let c_hat = ntt_a(&presig.c); // paper c in NTT domain: NTT(c) once per call
    for j in 0..LAS_N {
        let ct = polymul_prehat(&c_hat, &ntt_b_mont(&pk.t[j])); // paper c·t: the product c t
        for n in 0..N {
            w[j].0[n] = full_reduce32(w[j].0[n] - ct.0[n]); // w' = A z^ - c t
        }
    }
    let mut w_y: [R; LAS_N] = [R0; LAS_N]; // paper w′ + t′: the hashed commitment w′ + Y
    for j in 0..LAS_N {
        for n in 0..N {
            w_y[j].0[n] = full_reduce32(w[j].0[n] + y_stmt.t[j].0[n]); // w' + Y
        }
    }
    // [PAPER Alg.2] 15:     if c ≠ H(pk, w′ + t′, M) then
    // [PAPER Alg.2] 16:         return 0
    // [PAPER Alg.2] 17:     end if
    // [PAPER Alg.2] 18:     return 1
    let c2 = hash_challenge(pk, &w_y, m); // paper H(pk, w′ + t′, M): recomputed challenge vs c
    c2 == presig.c
    // [PAPER Alg.2] 19: end procedure
}

/// `las_preverify` (adaptor twin of `las_verify`): PreVerify(Y, pk, sigma^, M),
/// public entry point at the single-hop bound.  Returns true iff valid.
pub fn las_preverify(
    presig: &LasSig,   // paper σ̂: presig = (c, ẑ), pre-sig to verify
    m: &[u8],          // paper M: message
    y_stmt: &LasPk,    // paper t′ := Y: statement, y_stmt.t = Y
    pk: &LasPk,        // paper t: pk.t = t (public key)
    pp: &LasPp,        // paper A: pp = A = [I | A']
) -> bool {
    las_preverify_internal(presig, m, y_stmt, pk, pp, LAS_BOUND_PRESIGN)
}

/// Adapt((Y,y), sigma^): PreVerify, then sigma = (c, z^ + y). None on failure.
pub fn las_adapt(
    presig: &LasSig,   // paper σ̂: presig = (c, ẑ)
    m: &[u8],          // paper M: message
    y_stmt: &LasPk,    // paper t′ := Y: statement
    y_wit: &LasSk,     // paper (Y,y) witness, r′ := y: y_wit.s = y (A y = Y)
    pk: &LasPk,        // paper t: pk.t = t (public key)
    pp: &LasPp,        // paper A: pp = A = [I | A']
) -> Option<LasSig> {  // paper σ: returns σ = (c, ẑ + r′), or None for ⊥
    // [PAPER Alg.2] 20: procedure Adapt((Y, y), pk, σ̂, M):
    // [PAPER Alg.2] 21:     if PreVerify(Y, pk, σ̂, M) = 0 then
    // [PAPER Alg.2] 22:         return ⊥
    // [PAPER Alg.2] 23:     end if
    if !las_preverify(presig, m, y_stmt, pk, pp) {
        return None;
    }
    // [PAPER Alg.2] 24:     Parse (c, ẑ) := σ̂ and r′ := y
    // [PAPER Alg.2] 25:     return σ = (c, ẑ + r′)
    let mut z: [R; LAS_M] = [R0; LAS_M]; // paper ẑ + r′: adapted response
    for j in 0..LAS_M {
        for n in 0..N {
            z[j].0[n] = presig.z[j].0[n] + y_wit.s[j].0[n]; // z = z^ + y_wit (exact)
        }
    }
    Some(LasSig {
        c: presig.c.clone(),
        z,
    })
    // [PAPER Alg.2] 26: end procedure
}

/// Ext(Y, sigma, sigma^): s = z - z^; Some(s) iff A*s == Y, else None.
pub fn las_ext(
    sig: &LasSig,      // paper σ: sig = (c, z)
    presig: &LasSig,   // paper σ̂: presig = (ĉ, ẑ)
    y_stmt: &LasPk,    // paper t′ := Y: statement
    pp: &LasPp,        // paper A: pp = A = [I | A']
) -> Option<LasSk> {   // paper s: returns witness s, or None for ⊥
    // [PAPER Alg.2] 27: procedure Ext(Y, σ, σ̂):
    // [PAPER Alg.2] 28:     Parse (c, z) := σ and (ĉ, ẑ) := σ̂
    // [PAPER Alg.2] 29:     Parse t′ := Y
    // [PAPER Alg.2] 30:     s = z − ẑ
    let mut s: [R; LAS_M] = [R0; LAS_M]; // paper s: extracted witness s = z − ẑ
    for j in 0..LAS_M {
        for n in 0..N {
            s[j].0[n] = sig.z[j].0[n] - presig.z[j].0[n]; // s = z - z^ (exact)
        }
    }
    // [PAPER Alg.2] 31:     if t′ ≠ A s, then return ⊥
    let ay = amul(pp, &s); // paper A s: the product A s, checked against t′ = Y
    if ay != y_stmt.t {
        return None;
    }
    // [PAPER Alg.2] 32:     return s
    Some(LasSk { s })
    // [PAPER Alg.2] 33: end procedure
}

/* ============================ helpers ============================
 * Defined at the BOTTOM of the file, in the same order as las_basesig.rs's
 * local copies (b_ prefix there), plus two LAS-only helpers at the end
 * (poly_uniform_ntt for las_setup, det_seed for the _det KAT variants). */

/// Pack one polynomial into 4 bytes/coeff (canonical [0,Q)) for hashing.
/// Mirrors ref/las.c pack_poly_canon (poly_reduce + poly_caddq == full_reduce32).
fn pack_poly_canon(a: &R) -> [u8; N * 4] {
    let mut out = [0u8; N * 4];
    for (i, &c) in a.0.iter().enumerate() {
        let x = full_reduce32(c) as u32;
        out[4 * i..4 * i + 4].copy_from_slice(&x.to_le_bytes());
    }
    out
}

/// Reject if any component has ||.||inf >= bound (values are exact/small here,
/// so plain abs() equals the C bit-trick absolute value). Mirrors chknorm_vec.
fn chknorm_vec(z: &[R; LAS_M], bound: i32) -> bool {
    // C poly_chknorm guard: bounds above (Q-1)/8 are rejected outright.
    if bound > (Q - 1) / 8 {
        return true;
    }
    z.iter()
        .flat_map(|p| p.0.iter())
        .any(|&x| x.abs() >= bound)
}

/// Challenge-side ("a") NTT operand: NTT + partial reduce, computed ONCE and
/// reused across all products that share the challenge — mirrors ml_dsa.rs
/// sign_internal step 17 (`c_hat ← NTT(c)`, then reused for c*s_1, c*s_2,
/// c*t_0) and ref/las.c's `chat`.  Representative-neutral: identical values to
/// the a-side half of the former per-call `polymul_centered`.
fn ntt_a(a: &R) -> T {
    let mut ah = ntt(&[a.clone()]);
    for x in ah[0].0.iter_mut() {
        *x = partial_reduce32(*x);
    }
    let [ah] = ah;
    ah
}

/// Invariant-side ("b") NTT operand in Montgomery form: NTT + partial reduce +
/// to_mont, computed ONCE per call for operands that do not change across
/// rejection attempts (secret r) or products (public t) — mirrors the
/// `s_1_hat_mont`/`t1_d2_hat_mont` pre-computes in ml_dsa.rs/types.rs and
/// ref/las.c's `shat`/`that`.  Identical values to the b-side half of the
/// former per-call `polymul_centered`.
fn ntt_b_mont(b: &R) -> T {
    let mut bh = ntt(&[b.clone()]);
    for x in bh[0].0.iter_mut() {
        *x = partial_reduce32(*x);
    }
    let bm = to_mont(&bh);
    let [bm] = bm;
    bm
}

/// Second half of the exact centred product out = a*b mod (X^256+1, Q): both
/// operands already transformed (`ntt_a` / `ntt_b_mont`).  The final
/// center_mod pins the unique centred representative, which for the uses above
/// (c*r with |c*r|inf <= kappa; c*t later canonicalised) matches the C values.
fn polymul_prehat(ah: &T, bm: &T) -> R {
    let mut ch = T0;
    for n in 0..N {
        ch.0[n] = mont_reduce(i64::from(ah.0[n]) * i64::from(bm.0[n]));
    }
    let prod = inv_ntt(&[ch]);
    R(from_fn(|n| center_mod(prod[0].0[n])))
}

/// w = A*v = v_top + A'*v_bot, with A=[I|A'], A' (pp.mat) already in NTT domain.
/// Output is canonical [0,Q). Mirrors ref/las.c las_Amul.
fn amul(pp: &LasPp, v: &[R; LAS_M]) -> [R; LAS_N] {
    // vhat = NTT(v_bot), reduced to (-Q,Q) before use (representative-neutral).
    let vbot: [R; LAS_ELL] = from_fn(|j| v[LAS_N + j].clone());
    let mut vhat = ntt(&vbot);
    for p in vhat.iter_mut() {
        for x in p.0.iter_mut() {
            *x = partial_reduce32(*x);
        }
    }
    let vm = to_mont(&vhat);

    let mut w: [R; LAS_N] = [R0; LAS_N];
    for i in 0..LAS_N {
        let mut acc = T0;
        for j in 0..LAS_ELL {
            for n in 0..N {
                acc.0[n] += mont_reduce(i64::from(pp.mat[i][j].0[n]) * i64::from(vm[j].0[n]));
            }
        }
        // Mirrors C poly_reduce(&acc) before the inverse NTT (keeps i32 bounds).
        for x in acc.0.iter_mut() {
            *x = partial_reduce32(*x);
        }
        let lo = inv_ntt(&[acc]); // canonical [0,Q)
        for n in 0..N {
            // identity block + canonicalise (C: poly_add; poly_reduce; poly_caddq)
            w[i].0[n] = full_reduce32(lo[0].0[n] + v[i].0[n]);
        }
    }
    w
}

/// H: challenge poly with ||c||_1 = LAS_KAPPA, ||c||inf = 1.
/// Same construction as Dilithium's poly_challenge, kappa fixed here.
/// Mirrors ref/las.c las_challenge, including SHAKE256 block buffering.
fn las_challenge(seed: &[u8; LAS_SEEDBYTES]) -> R {
    let mut h = Shake256::default();
    h.update(seed);
    let mut rd = h.finalize_xof();
    let mut buf = [0u8; SHAKE256_RATE];
    rd.read(&mut buf);

    let mut signs = u64::from_le_bytes(buf[0..8].try_into().unwrap());
    let mut pos = 8usize;

    let mut c = R0;
    for i in (N - LAS_KAPPA as usize)..N {
        let b = loop {
            if pos >= SHAKE256_RATE {
                rd.read(&mut buf);
                pos = 0;
            }
            let b = buf[pos] as usize;
            pos += 1;
            if b <= i {
                break b;
            }
        };
        c.0[i] = c.0[b];
        c.0[b] = 1 - 2 * ((signs & 1) as i32);
        signs >>= 1;
    }
    c
}

/// c = H(pk, commit, M) where commit is the (already w or w+Y) commitment.
/// Mirrors ref/las.c hash_challenge.
fn hash_challenge(pk: &LasPk, commit: &[R; LAS_N], m: &[u8]) -> R {
    let mut h = Shake256::default();
    for i in 0..LAS_N {
        h.update(&pack_poly_canon(&pk.t[i]));
    }
    for i in 0..LAS_N {
        h.update(&pack_poly_canon(&commit[i]));
    }
    h.update(m);
    let mut seed = [0u8; LAS_SEEDBYTES];
    h.finalize_xof().read(&mut seed);
    las_challenge(&seed)
}

/// Sample one poly with coefficients uniform in [-GAMMA, GAMMA] (set S_g).
/// Mirrors ref/las.c sample_Sgamma EXACTLY, including the SHAKE256 block
/// semantics: 3 bytes per attempt, and the last byte of each 136-byte block
/// is DISCARDED (C refills when pos+3 > RATE).
fn sample_sgamma(seed: &[u8; 64], nonce: u16) -> R {
    let two_gamma = 2u32 * (LAS_GAMMA as u32);
    let mut gmask: u32 = 1;
    while gmask < two_gamma {
        gmask <<= 1;
    }
    gmask -= 1;

    let mut h = Shake256::default();
    h.update(seed);
    h.update(&nonce.to_le_bytes());
    let mut rd = h.finalize_xof();
    let mut buf = [0u8; SHAKE256_RATE];
    rd.read(&mut buf);
    let mut pos = 0usize;

    let mut y = R0;
    let mut ctr = 0usize;
    while ctr < N {
        if pos + 3 > SHAKE256_RATE {
            rd.read(&mut buf);
            pos = 0;
        }
        let t = (u32::from(buf[pos])
            | (u32::from(buf[pos + 1]) << 8)
            | (u32::from(buf[pos + 2]) << 16))
            & gmask;
        pos += 3;
        if t < two_gamma + 1 {
            y.0[ctr] = (t as i32) - LAS_GAMMA;
            ctr += 1;
        }
    }
    y
}

/// Sample one poly with coefficients uniform in {-1,0,1} (set S_1, ternary).
/// Mirrors ref/las.c sample_ternary (2-bit codes, reject 3; contiguous stream).
fn sample_ternary(seed: &[u8; LAS_SEEDBYTES], nonce: u16) -> R {
    let mut h = Shake256::default();
    h.update(seed);
    h.update(&nonce.to_le_bytes());
    let mut rd = h.finalize_xof();
    let mut buf = [0u8; SHAKE256_RATE];
    rd.read(&mut buf);
    let mut pos = 0usize;

    let mut r = R0;
    let mut ctr = 0usize;
    while ctr < N {
        if pos >= SHAKE256_RATE {
            rd.read(&mut buf);
            pos = 0;
        }
        let byte = buf[pos];
        pos += 1;
        let mut s = 0u8;
        while s < 4 && ctr < N {
            let v = (byte >> (2 * s)) & 3;
            if v < 3 {
                r.0[ctr] = i32::from(v) - 1;
                ctr += 1;
            }
            s += 1;
        }
    }
    r
}

/// Expand one uniform poly in [0,Q) DIRECTLY IN THE NTT DOMAIN from
/// SHAKE128(seed || nonce_le16). Byte-stream-identical to the C poly_uniform
/// (contiguous 3-byte groups; 840 and 168 are both divisible by 3, so the C
/// leftover-carry never discards bytes).  LAS-only helper (for las_setup).
fn poly_uniform_ntt(seed: &[u8; LAS_SEEDBYTES], nonce: u16) -> T {
    let mut h = Shake128::default();
    h.update(seed);
    h.update(&nonce.to_le_bytes());
    let mut rd = h.finalize_xof();

    let mut t = T0;
    let mut ctr = 0usize;
    let mut b = [0u8; 3];
    while ctr < N {
        rd.read(&mut b);
        let x = (u32::from(b[0]) | (u32::from(b[1]) << 8) | (u32::from(b[2]) << 16)) & 0x7F_FFFF;
        if x < Q as u32 {
            t.0[ctr] = x as i32;
            ctr += 1;
        }
    }
    t
}

/// Deterministic per-(pre)signature mask randomness:
/// seed = SHAKE256(tag || sk || [Y] || M), 64 bytes. Mirrors ref/las.c det_seed.
/// LAS-only helper (the _det KAT path); no las_basesig.rs analogue.
fn det_seed(tag: u8, sk: &LasSk, y_stmt: Option<&LasPk>, m: &[u8]) -> [u8; 64] {
    let mut h = Shake256::default();
    h.update(&[tag]); // domain: 0=sign, 1=presign

    // ternary sk -> 1 byte/coeff, (uint8_t)(int8_t) semantics: -1 -> 0xFF
    let mut skb = [0u8; LAS_M * N];
    for i in 0..LAS_M {
        for k in 0..N {
            skb[i * N + k] = sk.s[i].0[k] as i8 as u8;
        }
    }
    h.update(&skb);

    if let Some(y) = y_stmt {
        for i in 0..LAS_N {
            h.update(&pack_poly_canon(&y.t[i])); // bind the statement Y
        }
    }
    h.update(m);
    skb.zeroize(); // raw sk bytes: wipe (upstream secret-material policy)

    let mut out = [0u8; 64];
    h.finalize_xof().read(&mut out);
    out
}
