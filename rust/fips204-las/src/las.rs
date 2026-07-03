//! LAS — Lattice-based Adaptor Signature (Esgin–Ersoy–Erkin, eprint 2020/845,
//! Algorithm 2), layered ADDITIVELY on this crate's FIPS 204 primitives.
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
/// `sign_core`/`presign_core`; never read by the scheme itself.  Benchmarks
/// reset it and read it to report the restart rate DIRECTLY rather than
/// estimating it from a timing ratio.  Relaxed ordering: single-threaded
/// benchmark instrumentation, not synchronisation.
pub static LAS_ATTEMPTS: AtomicU64 = AtomicU64::new(0);

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
    pub(crate) t: [R; LAS_N],
}

/// Secret key / witness: r in S_1 (ternary, stored as exact -1/0/1).
#[derive(Clone, PartialEq)]
pub struct LasSk {
    pub(crate) s: [R; LAS_M],
}

/// (Pre-)signature (c, z): c ternary challenge, z exact centred response.
#[derive(Clone, PartialEq)]
pub struct LasSig {
    pub(crate) c: R,
    pub(crate) z: [R; LAS_M],
}

/* ============================ helpers ============================ */

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

/// out = a*b mod (X^256+1, Q), exact CENTRED representative.
/// Mirrors ref/las.c polymul (NTT -> pointwise -> invNTT -> reduce); the final
/// center_mod pins the unique centred representative, which for the uses below
/// (c*r with |c*r|inf <= kappa; c*t later canonicalised) matches the C values.
fn polymul_centered(a: &R, b: &R) -> R {
    let mut ah = ntt(&[a.clone()]);
    let mut bh = ntt(&[b.clone()]);
    // Keep values small before to_mont / pointwise (representative-neutral).
    for x in ah[0].0.iter_mut() {
        *x = partial_reduce32(*x);
    }
    for x in bh[0].0.iter_mut() {
        *x = partial_reduce32(*x);
    }
    let bm = to_mont(&bh);
    let mut ch = T0;
    for n in 0..N {
        ch.0[n] = mont_reduce(i64::from(ah[0].0[n]) * i64::from(bm[0].0[n]));
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
/// leftover-carry never discards bytes).
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

    let mut out = [0u8; 64];
    h.finalize_xof().read(&mut out);
    out
}

/* ============================ scheme ============================ */

/// Public parameters pp = A (A' expanded from a public seed, NTT domain).
pub fn las_setup(seed: &[u8; LAS_SEEDBYTES]) -> LasPp {
    LasPp {
        mat: from_fn(|i| from_fn(|j| poly_uniform_ntt(seed, ((i as u16) << 8) + j as u16))),
        seed: *seed,
    }
}

/// Deterministic KeyGen from an explicit 32-byte seed (reproducible KATs).
/// KeyGen = Gen: r <- S_1^(n+l); t = A r; (pk,sk) = (t,r).
pub fn las_keygen_seed(pp: &LasPp, seed: &[u8; LAS_SEEDBYTES]) -> (LasPk, LasSk) {
    let sk = LasSk {
        s: from_fn(|j| sample_ternary(seed, j as u16)),
    };
    let pk = LasPk {
        t: amul(pp, &sk.s),
    };
    (pk, sk)
}

/// Shared Sign body, parameterised by the 64-byte mask seed. Mirrors sign_core.
fn sign_core(m: &[u8], pk: &LasPk, sk: &LasSk, pp: &LasPp, seed: &[u8; 64]) -> LasSig {
    let mut nonce: u16 = 0;
    loop {
        LAS_ATTEMPTS.fetch_add(1, Ordering::Relaxed); // instrumentation only
        let mut y: [R; LAS_M] = [R0; LAS_M];
        for j in 0..LAS_M {
            y[j] = sample_sgamma(seed, nonce);
            nonce = nonce.wrapping_add(1);
        }
        let w = amul(pp, &y); //  w = A y
        let c = hash_challenge(pk, &w, m); //  c = H(pk, w, M)

        let mut z: [R; LAS_M] = [R0; LAS_M];
        for j in 0..LAS_M {
            let cr = polymul_centered(&c, &sk.s[j]); // exact, |.|inf <= kappa
            for n in 0..N {
                z[j].0[n] = y[j].0[n] + cr.0[n]; // z = y + c r (exact, C reduce = identity)
            }
        }
        if chknorm_vec(&z, LAS_BOUND_SIGN) {
            continue;
        }
        return LasSig { c, z };
    }
}

/// Deterministic Sign: mask randomness derived from (sk, M). Mirrors las_sign_det.
pub fn las_sign_det(m: &[u8], pk: &LasPk, sk: &LasSk, pp: &LasPp) -> LasSig {
    let seed = det_seed(0, sk, None, m); // tag 0 = sign (no statement)
    sign_core(m, pk, sk, pp, &seed)
}

/// Randomised KeyGen: fresh 32-byte seed from the caller's RNG (mirrors
/// C `las_keygen` = `randombytes` + `las_keygen_seed`; RNG injected, Rust idiom).
pub fn las_keygen(pp: &LasPp, rng: &mut impl CryptoRngCore) -> (LasPk, LasSk) {
    let mut seed = [0u8; LAS_SEEDBYTES];
    rng.fill_bytes(&mut seed);
    las_keygen_seed(pp, &seed)
}

/// Randomised Sign: fresh 64-byte mask seed per call (mirrors C `las_sign`).
pub fn las_sign(
    m: &[u8],
    pk: &LasPk,
    sk: &LasSk,
    pp: &LasPp,
    rng: &mut impl CryptoRngCore,
) -> LasSig {
    let mut seed = [0u8; 64];
    rng.fill_bytes(&mut seed);
    sign_core(m, pk, sk, pp, &seed)
}

/// Ordinary Verify. Returns true iff the signature is valid. Mirrors las_verify.
pub fn las_verify(sig: &LasSig, m: &[u8], pk: &LasPk, pp: &LasPp) -> bool {
    if chknorm_vec(&sig.z, LAS_BOUND_SIGN) {
        return false;
    }
    let mut w = amul(pp, &sig.z); // A z
    for j in 0..LAS_N {
        let ct = polymul_centered(&sig.c, &pk.t[j]);
        for n in 0..N {
            // w' = A z - c t, canonicalised (C: poly_sub; poly_reduce; poly_caddq)
            w[j].0[n] = full_reduce32(w[j].0[n] - ct.0[n]);
        }
    }
    let c2 = hash_challenge(pk, &w, m);
    c2 == sig.c
}

/// Shared PreSign body: like sign_core but hashes (w+Y) and rejects at `bound`.
fn presign_core(
    m: &[u8],
    y_stmt: &LasPk,
    pk: &LasPk,
    sk: &LasSk,
    pp: &LasPp,
    bound: i32,
    seed: &[u8; 64],
) -> LasSig {
    let mut nonce: u16 = 0;
    loop {
        LAS_ATTEMPTS.fetch_add(1, Ordering::Relaxed); // instrumentation only
        let mut y: [R; LAS_M] = [R0; LAS_M];
        for j in 0..LAS_M {
            y[j] = sample_sgamma(seed, nonce);
            nonce = nonce.wrapping_add(1);
        }
        let w = amul(pp, &y); // w = A y
        let mut w_y: [R; LAS_N] = [R0; LAS_N];
        for j in 0..LAS_N {
            for n in 0..N {
                // commit = w + Y, canonical (C: poly_add; poly_reduce; poly_caddq)
                w_y[j].0[n] = full_reduce32(w[j].0[n] + y_stmt.t[j].0[n]);
            }
        }
        let c = hash_challenge(pk, &w_y, m); // c = H(pk, w+Y, M)

        let mut z: [R; LAS_M] = [R0; LAS_M];
        for j in 0..LAS_M {
            let cr = polymul_centered(&c, &sk.s[j]);
            for n in 0..N {
                z[j].0[n] = y[j].0[n] + cr.0[n]; // z^ = y + c r
            }
        }
        if chknorm_vec(&z, bound) {
            continue;
        }
        return LasSig { c, z };
    }
}

/// Deterministic PreSign: mask randomness derived from (sk, Y, M); single-hop
/// bound gamma-kappa-1. Mirrors las_presign_det.
pub fn las_presign_det(
    m: &[u8],
    y_stmt: &LasPk,
    pk: &LasPk,
    sk: &LasSk,
    pp: &LasPp,
) -> LasSig {
    let seed = det_seed(1, sk, Some(y_stmt), m); // tag 1 = presign (binds Y)
    presign_core(m, y_stmt, pk, sk, pp, LAS_BOUND_PRESIGN, &seed)
}

/// Randomised PreSign: fresh 64-byte mask seed per call, single-hop bound
/// gamma-kappa-1 (mirrors C `las_presign`).
pub fn las_presign(
    m: &[u8],
    y_stmt: &LasPk,
    pk: &LasPk,
    sk: &LasSk,
    pp: &LasPp,
    rng: &mut impl CryptoRngCore,
) -> LasSig {
    let mut seed = [0u8; 64];
    rng.fill_bytes(&mut seed);
    presign_core(m, y_stmt, pk, sk, pp, LAS_BOUND_PRESIGN, &seed)
}

/// PreVerify(Y, pk, sigma^, M). Returns true iff the pre-signature is valid.
pub fn las_preverify(
    presig: &LasSig,
    m: &[u8],
    y_stmt: &LasPk,
    pk: &LasPk,
    pp: &LasPp,
) -> bool {
    if chknorm_vec(&presig.z, LAS_BOUND_PRESIGN) {
        return false;
    }
    let mut w = amul(pp, &presig.z); // A z^
    for j in 0..LAS_N {
        let ct = polymul_centered(&presig.c, &pk.t[j]);
        for n in 0..N {
            w[j].0[n] = full_reduce32(w[j].0[n] - ct.0[n]); // w' = A z^ - c t
        }
    }
    let mut w_y: [R; LAS_N] = [R0; LAS_N];
    for j in 0..LAS_N {
        for n in 0..N {
            w_y[j].0[n] = full_reduce32(w[j].0[n] + y_stmt.t[j].0[n]); // w' + Y
        }
    }
    let c2 = hash_challenge(pk, &w_y, m); // check c == H(pk, w'+Y, M)
    c2 == presig.c
}

/// Adapt((Y,y), sigma^): PreVerify, then sigma = (c, z^ + y). None on failure.
pub fn las_adapt(
    presig: &LasSig,
    m: &[u8],
    y_stmt: &LasPk,
    y_wit: &LasSk,
    pk: &LasPk,
    pp: &LasPp,
) -> Option<LasSig> {
    if !las_preverify(presig, m, y_stmt, pk, pp) {
        return None;
    }
    let mut z: [R; LAS_M] = [R0; LAS_M];
    for j in 0..LAS_M {
        for n in 0..N {
            z[j].0[n] = presig.z[j].0[n] + y_wit.s[j].0[n]; // z = z^ + y_wit (exact)
        }
    }
    Some(LasSig {
        c: presig.c.clone(),
        z,
    })
}

/// Ext(Y, sigma, sigma^): s = z - z^; Some(s) iff A*s == Y, else None.
pub fn las_ext(
    sig: &LasSig,
    presig: &LasSig,
    y_stmt: &LasPk,
    pp: &LasPp,
) -> Option<LasSk> {
    let mut s: [R; LAS_M] = [R0; LAS_M];
    for j in 0..LAS_M {
        for n in 0..N {
            s[j].0[n] = sig.z[j].0[n] - presig.z[j].0[n]; // s = z - z^ (exact)
        }
    }
    let ay = amul(pp, &s); // check A s == Y (both canonical)
    if ay != y_stmt.t {
        return None;
    }
    Some(LasSk { s })
}
