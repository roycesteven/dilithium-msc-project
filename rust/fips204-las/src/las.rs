//! LAS — Lattice-based Adaptor Signature (Esgin–Ersoy–Erkin, eprint 2020/845,
//! Algorithm 2), layered ADDITIVELY on this crate's FIPS 204 primitives.
//!
//! This module holds ONLY the four Algorithm-2 adaptor operations
//! (PreSign / PreVerify / Adapt / Ext) plus their deterministic-KAT variants.
//! Definition 3 (paper §2.3) says the adaptor scheme Π_{R,R',Σ} *inherits*
//! KeyGen / Sign / Verify from the underlying signature scheme Σ — so those
//! live in `basesig.rs` (Algorithm 1) and NOT here, and the hard-relation
//! generator `Gen` lives in `relation.rs`.  The `las` surface is reserved for
//! the operations the paper adds on top of the base signature; an adapted
//! pre-signature is checked by `basesig::verify`.
//!
//!   -- Algorithm 2 (adaptor layer; upstream = the PAPER, names kept) --
//!   presign_internal / presign / presign_det /
//!   preverify_internal / preverify / adapt / ext.
//!
//! Object types: [`PreSignature`] is OWNED here (physical home `setup.rs`,
//! re-exported below); [`Statement`]/[`Witness`] come from `relation`
//! (consumed as parameters); [`PublicKey`]/[`SecretKey`]/[`Signature`]/
//! [`PublicParams`] come from `basesig`/`setup` (Adapt produces a
//! `Signature`, Ext produces a `Witness`).
//!
//! The local helpers are defined at the BOTTOM of the file, in the same order
//! as `basesig.rs`'s (ml_dsa.rs keeps its equivalents in hashing.rs /
//! helpers.rs / ntt.rs), so the scheme functions read side by side.
//!
//! This module is a Rust port of the C implementation `ref/las.c` from the
//! dilithium-msc-project repository (deterministic path only: deterministic
//! PreSign, PreVerify/Adapt/Ext).  It is byte-for-byte cross-checked against
//! the C known-answer test (`ref/test/test_kat.c`, pinned SHAKE256 digest) —
//! see `tests/las_kat.rs`.
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
use sha3::Shake256;
use zeroize::Zeroize;

use crate::helpers::{center_mod, full_reduce32, mont_reduce, partial_reduce32, to_mont};
use crate::ntt::{inv_ntt, ntt};
use crate::types::{R, R0, T, T0};
use crate::Q;

/* ---- Shared system setup: the construction parameters and object types live
 * in setup.rs (PublicParams / PublicKey / SecretKey / Signature / PreSignature)
 * because BOTH basesig.rs and this file consume the same setup; the hard
 * relation types (Statement / Witness) are owned by relation.rs.  This file
 * consumes them as parameters and OWNS PreSignature (re-exported below).
 * Mirrors the C `las.h` including `setup.h`. ---- */
use crate::relation::{Statement, Witness};
use crate::las_types::{PublicKey, SecretKey, Signature};
use crate::setup::{PublicParams, D, ELL, GAMMA, KAPPA, N, N_PLUS_ELL, LAS_CTILDEBYTES, LAS_SEEDBYTES};
// Validating byte codecs for the end-to-end packed tier (mirrors basesig.rs's
// serialize imports for its *_packed twins).
use crate::serialize::{
    pack_pre_signature, pack_signature, pack_witness, unpack_pre_signature, unpack_public_key,
    unpack_secret_key, unpack_signature, unpack_statement, unpack_witness, PRE_SIGNATURE_BYTES,
    PUBLIC_KEY_BYTES, SECRET_KEY_BYTES, SIGNATURE_BYTES, STATEMENT_BYTES, WITNESS_BYTES,
};
// Owner re-export: the Algorithm-2 pre-signature type belongs to THIS module
// (physical home is las_types.rs) — callers import it from its owner:
// `use fips204::las::PreSignature`.
pub use crate::las_types::PreSignature;

/// PreSign/PreVerify rejection bound: reject |z^|inf > gamma-kappa-1 (the
/// tighter -1 budget).  Adaptor-only (Algorithm 2) — no basesig analogue — so
/// it OWNS this bound (the Algorithm-1 `BOUND_SIGN` lives in `basesig`).
pub const BOUND_PRESIGN: i32 = GAMMA - KAPPA;

const SHAKE256_RATE: usize = 136;

/// Rejection-sampling attempt counter (measurement only; mirrors the C
/// `las_attempts`).  Incremented once per rejection-loop iteration in
/// `presign_internal`; never read by the scheme itself.  Benchmarks reset it
/// and read it to report the restart rate DIRECTLY rather than estimating it
/// from a timing ratio.  Relaxed ordering: single-threaded benchmark
/// instrumentation, not synchronisation.
pub static LAS_ATTEMPTS: AtomicU64 = AtomicU64::new(0);

/// EXACT expected attempts/call of the rejection loop running at `bound`
/// (`BOUND_PRESIGN` or the base `BOUND_SIGN`), for validating a
/// measured attempt counter against theory. Derivation, verified against
/// eprint 2020/845 (Esgin-Ersoy-Erkin):
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
    let mut p = f64::from(2 * bound - 1) / f64::from(2 * GAMMA + 1);
    let mut acc = 1.0_f64;
    let mut e = (N_PLUS_ELL * D) as u32; // (n+ell)*d coefficients
    while e != 0 {
        if e & 1 == 1 {
            acc *= p;
        }
        p *= p;
        e >>= 1;
    }
    1.0 / acc
}

/* =============== scheme, Algorithm 2 (adaptor layer) ===============
 * No basesig.rs/ml_dsa.rs analogue: these are the adaptor operations LAS adds
 * on top of the base signature (upstream = the PAPER).  Algorithm 1
 * (KeyGen/Sign/Verify) lives in basesig.rs; Gen lives in relation.rs. */

/// `presign_internal` (adaptor twin of `basesig::sign_internal`; C twin
/// `las_presign_internal`): like the Sign body but hashes (w+Y) and rejects
/// at `bound` (gamma-kappa-1 single-hop).
pub(crate) fn presign_internal(
    m: &[u8],             // paper M: message
    statement: &Statement, // paper t′ := Y: statement, as_t_prime() = Y = A y
    pk: &PublicKey,       // paper t: pk.t = t (public key)
    sk: &SecretKey,       // paper r: sk.r = r (secret key)
    pp: &PublicParams,    // paper A: pp = A = [I | A']
    bound: i32,           // paper γ−κ−1 (single-hop) / γ−κ−K (AMHL)
    seed: &[u8; 64],      // PRG mask seed (no paper symbol)
) -> PreSignature {       // paper σ̂: returns σ̂ = (c, ẑ)
    // [PAPER Alg.2] 1:  procedure PreSign((pk, sk), Y, M):
    let t_prime = statement.as_t_prime(); // paper t′ := Y (Alg. 2 step 4)
    let mut mask_nonce: u16 = 0; // PRG counter (no paper symbol)
    let r_hat_mont: [T; N_PLUS_ELL] = from_fn(|j| ntt_b_mont(&sk.r[j])); // paper r in NTT domain: NTT(r) once per call
    loop {
        LAS_ATTEMPTS.fetch_add(1, Ordering::Relaxed); // instrumentation only
        // [PAPER Alg.2] 2:      y ←$ Sγ^(n+ℓ)
        let mut y: [R; N_PLUS_ELL] = [R0; N_PLUS_ELL]; // paper y: mask, y <-$ Sγ^(n+ℓ)
        for j in 0..N_PLUS_ELL {
            y[j] = sample_sgamma(seed, mask_nonce);
            mask_nonce = mask_nonce.wrapping_add(1);
        }
        // [PAPER Alg.2] 3:      w = A y
        let w = amul(pp, &y); // paper w: commitment w = A y
        // [PAPER Alg.2] 4:      c = H(pk, w + t′, M), where t′ := Y
        let mut w_plus_t_prime: [R; N] = [R0; N]; // paper w + t′: the hashed commitment w + Y
        for j in 0..N {
            for n in 0..D {
                // commit = w + Y, canonical (C: poly_add; poly_reduce; poly_caddq)
                w_plus_t_prime[j].0[n] = full_reduce32(w[j].0[n] + t_prime[j].0[n]);
            }
        }
        let c_tilde = hash_challenge(pk, &w_plus_t_prime, m); // paper c: challenge DIGEST c_tilde = H(pk, w+Y, M)
        let c = sample_in_ball(&c_tilde); // paper c: challenge polynomial c = SampleInBall(c_tilde) (local only)
        let c_hat = ntt_a(&c); // paper c in NTT domain: NTT(c) once per attempt

        // [PAPER Alg.2] 5:      ẑ = y + c r, where r := sk
        let mut z_hat: [R; N_PLUS_ELL] = [R0; N_PLUS_ELL]; // paper ẑ: pre-sig response ẑ = y + c r
        for j in 0..N_PLUS_ELL {
            let c_r = polymul_prehat(&c_hat, &r_hat_mont[j]); // paper c·r: the product c r
            for n in 0..D {
                z_hat[j].0[n] = y[j].0[n] + c_r.0[n]; // ẑ = y + c r
            }
        }
        // [PAPER Alg.2] 6:      if ||ẑ||∞ > γ − κ − 1, then Restart
        if chknorm_vec(&z_hat, bound) {
            continue;
        }
        // [PAPER Alg.2] 7:      return σ̂ = (c, ẑ)
        return PreSignature { c_tilde, z_hat };
    }
    // [PAPER Alg.2] 8:  end procedure
}

/// `presign` (adaptor twin of `basesig::sign`; C twin `las_presign`):
/// PreSign(sk, Y, M), random path — fresh 64-byte mask seed per call,
/// single-hop bound gamma-kappa-1.
pub fn presign(
    m: &[u8],                      // paper M: message
    statement: &Statement,         // paper t′ := Y: statement
    pk: &PublicKey,                // paper t: pk.t = t (public key)
    sk: &SecretKey,                // paper r: sk.r = r (secret key)
    pp: &PublicParams,             // paper A: pp = A = [I | A']
    rng: &mut impl CryptoRngCore,  // CSPRNG for the mask seed (no paper symbol)
) -> PreSignature {                // paper σ̂: returns σ̂ = (c, ẑ)
    let mut seed = [0u8; 64]; // PRG mask seed (no paper symbol)
    rng.fill_bytes(&mut seed);
    let sigma_hat = presign_internal(m, statement, pk, sk, pp, BOUND_PRESIGN, &seed);
    seed.zeroize(); // mask seed: wipe
    sigma_hat
}

/// `presign_det` (adaptor twin of `basesig::sign_det`; KAT path):
/// mask randomness derived from (sk, Y, M); single-hop bound gamma-kappa-1.
/// C twin `las_presign_det`.
pub fn presign_det(
    m: &[u8],              // paper M: message
    statement: &Statement, // paper t′ := Y: statement
    pk: &PublicKey,        // paper t: pk.t = t (public key)
    sk: &SecretKey,        // paper r: sk.r = r (secret key)
    pp: &PublicParams,     // paper A: pp = A = [I | A']
) -> PreSignature {        // paper σ̂: returns σ̂ = (c, ẑ)
    let mut seed = det_seed(1, sk, Some(statement), m); // PRG mask seed from (sk, Y, M); tag 1 = presign (binds Y)
    let sigma_hat = presign_internal(m, statement, pk, sk, pp, BOUND_PRESIGN, &seed);
    seed.zeroize(); // sk-derived mask seed: wipe
    sigma_hat
}

/// `preverify_internal` (adaptor twin of `basesig::verify_internal`; C twin
/// `las_preverify_internal`): PreVerify body, parameterised by the rejection
/// bound.
pub(crate) fn preverify_internal(
    sigma_hat: &PreSignature, // paper σ̂: sigma_hat = (c, ẑ), pre-sig to verify
    m: &[u8],                 // paper M: message
    statement: &Statement,    // paper t′ := Y: statement, as_t_prime() = Y
    pk: &PublicKey,           // paper t: pk.t = t (public key)
    pp: &PublicParams,        // paper A: pp = A = [I | A']
    bound: i32,               // paper γ−κ−1 (single-hop) / γ−κ−K (AMHL)
) -> bool {
    // [PAPER Alg.2] 9:  procedure PreVerify(Y, pk, σ̂, M):
    // [PAPER Alg.2] 10:     Parse (c, ẑ) := σ̂ and t′ := Y
    let t_prime = statement.as_t_prime();
    // [PAPER Alg.2] 11:     if ||ẑ||∞ > γ − κ − 1 then
    // [PAPER Alg.2] 12:         return 0
    // [PAPER Alg.2] 13:     end if
    if chknorm_vec(&sigma_hat.z_hat, bound) {
        return false;
    }
    // [PAPER Alg.2] 14:     w′ = A ẑ − c t, where t := pk
    let mut w_prime = amul(pp, &sigma_hat.z_hat); // paper w′: w′ = A ẑ − c t (starts as A ẑ)
    let c = sample_in_ball(&sigma_hat.c_tilde); // paper c: challenge polynomial from the stored digest
    let c_hat = ntt_a(&c); // paper c in NTT domain: NTT(c) once per call
    for j in 0..N {
        let c_t = polymul_prehat(&c_hat, &ntt_b_mont(&pk.t[j])); // paper c·t: the product c t
        for n in 0..D {
            w_prime[j].0[n] = full_reduce32(w_prime[j].0[n] - c_t.0[n]); // w' = A ẑ - c t
        }
    }
    let mut w_prime_plus_t_prime: [R; N] = [R0; N]; // paper w′ + t′: the hashed commitment w′ + Y
    for j in 0..N {
        for n in 0..D {
            w_prime_plus_t_prime[j].0[n] = full_reduce32(w_prime[j].0[n] + t_prime[j].0[n]); // w' + Y
        }
    }
    // [PAPER Alg.2] 15:     if c ≠ H(pk, w′ + t′, M) then
    // [PAPER Alg.2] 16:         return 0
    // [PAPER Alg.2] 17:     end if
    // [PAPER Alg.2] 18:     return 1
    let c_tilde_check = hash_challenge(pk, &w_prime_plus_t_prime, m); // paper H(pk, w′+t′, M): recomputed digest
    c_tilde_check == sigma_hat.c_tilde // accept iff the recomputed digest equals the stored one (byte compare)
    // [PAPER Alg.2] 19: end procedure
}

/// `preverify` (adaptor twin of `basesig::verify`; C twin `las_preverify`):
/// PreVerify(Y, pk, sigma^, M), public entry point at the single-hop bound.
/// Returns true iff valid.
pub fn preverify(
    sigma_hat: &PreSignature, // paper σ̂: sigma_hat = (c, ẑ), pre-sig to verify
    m: &[u8],                 // paper M: message
    statement: &Statement,    // paper t′ := Y: statement, as_t_prime() = Y
    pk: &PublicKey,           // paper t: pk.t = t (public key)
    pp: &PublicParams,        // paper A: pp = A = [I | A']
) -> bool {
    preverify_internal(sigma_hat, m, statement, pk, pp, BOUND_PRESIGN)
}

/// `adapt` (C twin `las_adapt`): Adapt((Y,y), sigma^): PreVerify, then
/// sigma = (c, z^ + y).  None on failure.
pub fn adapt(
    sigma_hat: &PreSignature, // paper σ̂: sigma_hat = (c, ẑ)
    m: &[u8],                 // paper M: message
    statement: &Statement,    // paper t′ := Y: statement
    witness: &Witness,        // paper (Y,y) witness, r′ := y: as_relation_vector() = y (A y = Y)
    pk: &PublicKey,           // paper t: pk.t = t (public key)
    pp: &PublicParams,        // paper A: pp = A = [I | A']
) -> Option<Signature> {      // paper σ: returns σ = (c, ẑ + r′), or None for ⊥
    // [PAPER Alg.2] 20: procedure Adapt((Y, y), pk, σ̂, M):
    // [PAPER Alg.2] 21:     if PreVerify(Y, pk, σ̂, M) = 0 then
    // [PAPER Alg.2] 22:         return ⊥
    // [PAPER Alg.2] 23:     end if
    if !preverify(sigma_hat, m, statement, pk, pp) {
        return None;
    }
    // [PAPER Alg.2] 24:     Parse (c, ẑ) := σ̂ and r′ := y
    let r_prime = witness.as_relation_vector(); // paper r′ := y (Alg. 2 step 24)
    // [PAPER Alg.2] 25:     return σ = (c, ẑ + r′)
    let mut z: [R; N_PLUS_ELL] = [R0; N_PLUS_ELL]; // paper ẑ + r′: adapted response
    for j in 0..N_PLUS_ELL {
        for n in 0..D {
            z[j].0[n] = sigma_hat.z_hat[j].0[n] + r_prime[j].0[n]; // z = ẑ + r′ (exact)
        }
    }
    Some(Signature {
        c_tilde: sigma_hat.c_tilde, // Adapt preserves the challenge digest unchanged (array copy)
        z,
    })
    // [PAPER Alg.2] 26: end procedure
}

/// `ext` (C twin `las_ext`): Ext(Y, sigma, sigma^): s = z - z^;
/// Some(witness) iff A*s == Y, else None.
pub fn ext(
    sigma: &Signature,        // paper σ: sigma = (c, z)
    sigma_hat: &PreSignature, // paper σ̂: sigma_hat = (ĉ, ẑ)
    statement: &Statement,    // paper t′ := Y: statement
    pp: &PublicParams,        // paper A: pp = A = [I | A']
) -> Option<Witness> {        // paper s: returns witness s, or None for ⊥
    // [PAPER Alg.2] 27: procedure Ext(Y, σ, σ̂):
    // [PAPER Alg.2] 28:     Parse (c, z) := σ and (ĉ, ẑ) := σ̂
    // [PAPER Alg.2] 29:     Parse t′ := Y
    let t_prime = statement.as_t_prime();
    // [PAPER Alg.2] 30:     s = z − ẑ
    let mut s: [R; N_PLUS_ELL] = [R0; N_PLUS_ELL]; // paper s: extracted witness s = z − ẑ
    for j in 0..N_PLUS_ELL {
        for n in 0..D {
            s[j].0[n] = sigma.z[j].0[n] - sigma_hat.z_hat[j].0[n]; // s = z - ẑ (exact)
        }
    }
    // [PAPER Alg.2] 31:     if t′ ≠ A s, then return ⊥
    let a_s = amul(pp, &s); // paper A s: the product A s, checked against t′ = Y
    if &a_s != t_prime {
        return None;
    }
    // [PAPER Alg.2] 32:     return s
    Some(Witness::from_relation_vector(s))
    // [PAPER Alg.2] 33: end procedure
}

/* ==================== end-to-end packed tier (byte API) ====================
 * Validating byte-boundary wrappers around the four core Algorithm-2 ops:
 * decode every input (statement Y, keys, (pre-)signatures, witness) with the
 * validating serialize.rs codecs, run the core op, pack the output.  These are
 * the *_packed twins the benchmark's TIER-2 measures and what a wire/on-chain
 * consumer pays.  C twins: las_presign_packed / las_preverify_packed /
 * las_adapt_packed / las_ext_packed (las.c). */

/// `presign_packed` (end-to-end tier of [`presign`]); C twin `las_presign_packed`
/// (las.c).  Validating decode of the statement Y, public and secret keys, then
/// core PreSign (c = H(pk, w + Y, M)), then pack the pre-signature.  `None` if
/// any input fails validating decode.
pub fn presign_packed(
    m: &[u8],                              // paper M: message
    y_b: &[u8; STATEMENT_BYTES],           // packed statement Y (bytes)
    pk_b: &[u8; PUBLIC_KEY_BYTES],         // packed public key (bytes)
    sk_b: &[u8; SECRET_KEY_BYTES],         // packed secret key (bytes)
    pp: &PublicParams,                     // paper A: A = [I | A']
    rng: &mut impl CryptoRngCore,          // CSPRNG for the mask seed
) -> Option<[u8; PRE_SIGNATURE_BYTES]> {
    let statement = unpack_statement(y_b)?;
    let pk = unpack_public_key(pk_b)?;
    let sk = unpack_secret_key(sk_b)?;
    let presig = presign(m, &statement, &pk, &sk, pp, rng);
    pack_pre_signature(&presig) // in-band by the norm gate: always Some
}

/// `preverify_packed` (end-to-end tier of [`preverify`]); C twin
/// `las_preverify_packed` (las.c).  Validating decode of Y, pk and the
/// pre-signature, then core PreVerify (c == H(pk, w' + Y, M)).  Returns `true`
/// iff the bytes decode AND the pre-signature pre-verifies.
pub fn preverify_packed(
    presig_b: &[u8; PRE_SIGNATURE_BYTES],  // packed pre-signature (bytes)
    m: &[u8],                              // paper M: message
    y_b: &[u8; STATEMENT_BYTES],           // packed statement Y (bytes)
    pk_b: &[u8; PUBLIC_KEY_BYTES],         // packed public key (bytes)
    pp: &PublicParams,                     // paper A: A = [I | A']
) -> bool {
    let Some(statement) = unpack_statement(y_b) else {
        return false; // malformed statement
    };
    let Some(pk) = unpack_public_key(pk_b) else {
        return false; // malformed pk
    };
    let Some(presig) = unpack_pre_signature(presig_b) else {
        return false; // malformed pre-signature
    };
    preverify(&presig, m, &statement, &pk, pp)
}

/// `adapt_packed` (end-to-end tier of [`adapt`]); C twin `las_adapt_packed`
/// (las.c).  Validating decode of the pre-signature, statement, honest witness
/// r' and public key; core Adapt (which pre-verifies first); pack the adapted
/// (fully ordinary) signature.  `None` on any decode failure or invalid
/// pre-signature.
pub fn adapt_packed(
    presig_b: &[u8; PRE_SIGNATURE_BYTES],  // packed pre-signature (bytes)
    m: &[u8],                              // paper M: message
    y_b: &[u8; STATEMENT_BYTES],           // packed statement Y (bytes)
    r_prime_b: &[u8; WITNESS_BYTES],       // packed honest witness r' (bytes)
    pk_b: &[u8; PUBLIC_KEY_BYTES],         // packed public key (bytes)
    pp: &PublicParams,                     // paper A: A = [I | A']
) -> Option<[u8; SIGNATURE_BYTES]> {
    let statement = unpack_statement(y_b)?;
    let witness = unpack_witness(r_prime_b)?;
    let pk = unpack_public_key(pk_b)?;
    let presig = unpack_pre_signature(presig_b)?;
    let sigma = adapt(&presig, m, &statement, &witness, &pk, pp)?;
    pack_signature(&sigma)
}

/// `ext_packed` (end-to-end tier of [`ext`]); C twin `las_ext_packed` (las.c).
/// Validating decode of both signatures and the statement; core Ext
/// (s = z − ẑ, checked against A s == Y); pack the recovered witness s.  This is
/// the on-chain leak made byte-real: the two byte strings anyone can fetch from
/// the chain yield the witness.  `None` on any decode failure or invalid input.
pub fn ext_packed(
    sig_b: &[u8; SIGNATURE_BYTES],         // packed adapted signature (bytes)
    presig_b: &[u8; PRE_SIGNATURE_BYTES],  // packed pre-signature (bytes)
    y_b: &[u8; STATEMENT_BYTES],           // packed statement Y (bytes)
    pp: &PublicParams,                     // paper A: A = [I | A']
) -> Option<[u8; WITNESS_BYTES]> {
    let statement = unpack_statement(y_b)?;
    let sigma = unpack_signature(sig_b)?;
    let presig = unpack_pre_signature(presig_b)?;
    let s = ext(&sigma, &presig, &statement, pp)?;
    pack_witness(&s)
}

/* ============================ helpers ============================
 * Defined at the BOTTOM of the file, in the same order as basesig.rs's
 * local copies (b_ prefix there), plus one LAS-only helper at the end
 * (det_seed for the _det KAT variants; the setup expansion lives in
 * setup.rs, Gen's samplers live in relation.rs). */

/// Pack one polynomial into 4 bytes/coeff (canonical [0,Q)) for hashing.
/// Mirrors ref/las.c pack_poly_canon (poly_reduce + poly_caddq == full_reduce32).
fn pack_poly_canon(a: &R) -> [u8; D * 4] {
    let mut out = [0u8; D * 4];
    for (i, &c) in a.0.iter().enumerate() {
        let x = full_reduce32(c) as u32;
        out[4 * i..4 * i + 4].copy_from_slice(&x.to_le_bytes());
    }
    out
}

/// Reject if any component has ||.||inf >= bound (values are exact/small here,
/// so plain abs() equals the C bit-trick absolute value). Mirrors chknorm_vec.
fn chknorm_vec(z: &[R; N_PLUS_ELL], bound: i32) -> bool {
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
    for n in 0..D {
        ch.0[n] = mont_reduce(i64::from(ah.0[n]) * i64::from(bm.0[n]));
    }
    let prod = inv_ntt(&[ch]);
    R(from_fn(|n| center_mod(prod[0].0[n])))
}

/// w = A*v = v_top + A'*v_bot, with A=[I|A'], A' (pp.a_prime) already in NTT
/// domain.  Output is canonical [0,Q). Mirrors ref/las.c las_Amul.
fn amul(pp: &PublicParams, v: &[R; N_PLUS_ELL]) -> [R; N] {
    // vhat = NTT(v_bot), reduced to (-Q,Q) before use (representative-neutral).
    let vbot: [R; ELL] = from_fn(|j| v[N + j].clone());
    let mut vhat = ntt(&vbot);
    for p in vhat.iter_mut() {
        for x in p.0.iter_mut() {
            *x = partial_reduce32(*x);
        }
    }
    let vm = to_mont(&vhat);

    let mut w: [R; N] = [R0; N];
    for i in 0..N {
        let mut acc = T0;
        for j in 0..ELL {
            for n in 0..D {
                acc.0[n] += mont_reduce(i64::from(pp.a_prime[i][j].0[n]) * i64::from(vm[j].0[n]));
            }
        }
        // Mirrors C poly_reduce(&acc) before the inverse NTT (keeps i32 bounds).
        for x in acc.0.iter_mut() {
            *x = partial_reduce32(*x);
        }
        let lo = inv_ntt(&[acc]); // canonical [0,Q)
        for n in 0..D {
            // identity block + canonicalise (C: poly_add; poly_reduce; poly_caddq)
            w[i].0[n] = full_reduce32(lo[0].0[n] + v[i].0[n]);
        }
    }
    w
}

/// H: challenge poly with ||c||_1 = KAPPA, ||c||inf = 1.
/// Same construction as Dilithium's poly_challenge, kappa fixed here.
/// Mirrors ref/las.c las_poly_challenge, including SHAKE256 block buffering.
/// Renamed from the former `las_challenge` to its upstream twin's name
/// (`sample_in_ball`); the `las_` prefix is reserved for the four Algorithm-2
/// public operations, not private helpers.
fn sample_in_ball(seed: &[u8; LAS_SEEDBYTES]) -> R {
    let mut h = Shake256::default();
    h.update(seed);
    let mut rd = h.finalize_xof();
    let mut buf = [0u8; SHAKE256_RATE];
    rd.read(&mut buf);

    let mut signs = u64::from_le_bytes(buf[0..8].try_into().unwrap());
    let mut pos = 8usize;

    let mut c = R0;
    for i in (D - KAPPA as usize)..D {
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

/// c_tilde = H(pk, commit, M) where commit is the (already w or w+Y) commitment:
/// the paper challenge as its 32-byte hash DIGEST.  The caller derives the local
/// challenge polynomial separately via `sample_in_ball(&c_tilde)`, and stores
/// the digest in the (pre-)signature.  Mirrors ref/las.c hash_challenge.
fn hash_challenge(pk: &PublicKey, commit: &[R; N], m: &[u8]) -> [u8; LAS_CTILDEBYTES] {
    let mut h = Shake256::default();
    for i in 0..N {
        h.update(&pack_poly_canon(&pk.t[i]));
    }
    for i in 0..N {
        h.update(&pack_poly_canon(&commit[i]));
    }
    h.update(m);
    let mut c_tilde = [0u8; LAS_CTILDEBYTES];
    h.finalize_xof().read(&mut c_tilde);
    c_tilde
}

/// Sample one poly with coefficients uniform in [-GAMMA, GAMMA] (set S_g).
/// Mirrors ref/las.c sample_Sgamma EXACTLY, including the SHAKE256 block
/// semantics: 3 bytes per attempt, and the last byte of each 136-byte block
/// is DISCARDED (C refills when pos+3 > RATE).
fn sample_sgamma(seed: &[u8; 64], nonce: u16) -> R {
    let two_gamma = 2u32 * (GAMMA as u32);
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
    while ctr < D {
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
            y.0[ctr] = (t as i32) - GAMMA;
            ctr += 1;
        }
    }
    y
}

/// Deterministic per-(pre)signature mask randomness:
/// seed = SHAKE256(tag || sk || [Y] || M), 64 bytes. Mirrors ref/las.c det_seed.
/// LAS-only helper (the _det KAT path); no basesig.rs analogue (basesig has
/// its own tag-0-only twin because it never binds a statement).
fn det_seed(tag: u8, sk: &SecretKey, statement: Option<&Statement>, m: &[u8]) -> [u8; 64] {
    let mut h = Shake256::default();
    h.update(&[tag]); // domain: 0=sign, 1=presign

    // ternary sk -> 1 byte/coeff, (uint8_t)(int8_t) semantics: -1 -> 0xFF
    let mut skb = [0u8; N_PLUS_ELL * D];
    for i in 0..N_PLUS_ELL {
        for k in 0..D {
            skb[i * D + k] = sk.r[i].0[k] as i8 as u8;
        }
    }
    h.update(&skb);

    if let Some(y) = statement {
        let t_prime = y.as_t_prime();
        for i in 0..N {
            h.update(&pack_poly_canon(&t_prime[i])); // bind the statement Y
        }
    }
    h.update(m);
    skb.zeroize(); // raw sk bytes: wipe (upstream secret-material policy)

    let mut out = [0u8; 64];
    h.finalize_xof().read(&mut out);
    out
}
