//! The SEPARATE simplified Dilithium-style BASE signature — Rust port of
//! `ref/basesig.{c,h}` (Algorithm 1 of eprint 2020/845: KeyGen / Sign / Verify,
//! with NO adaptor statement `Y` anywhere in the Fiat–Shamir hash).
//!
//!     KeyGen : r <- S_1^{n+l};  t = A r;  (pk, sk) = (t, r)
//!     Sign   : y <- S_g; w = A y; c = H(pk, w, M); z = y + c r; |z|inf <= g-k
//!     Verify : w' = A z - c t;   accept iff  c == H(pk, w', M)
//!
//! It is kept deliberately SEPARATE from `las.rs` so the LAS adaptor protocol is
//! never touched or conflated: this module depends on `las.rs` ONLY for the
//! shared parameter constants and the key/signature struct layout
//! (`LasPp`/`LasPk`/`LasSk`/`LasSig`); all of its signing and verification logic
//! is its own local copy (`b_*` helpers), exactly as the C `basesig.c` duplicates
//! `las.c`'s static helpers.  Sharing the parameters keeps the two schemes at the
//! same setting (a fair comparison); sharing the struct layout makes their keys
//! and signatures interchangeable — an Adapted LAS pre-signature passes THIS
//! independent `base_verify` with no explicit `+Y`, because
//!
//!     A(z_hat + y) - c t = (A z_hat - c t) + A y = w' + Y      (since Y = A y).

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

use crate::helpers::{center_mod, full_reduce32, mont_reduce, partial_reduce32, to_mont};
use crate::las::{
    LasPk, LasPp, LasSig, LasSk, LAS_BOUND_SIGN, LAS_ELL, LAS_GAMMA, LAS_KAPPA, LAS_M, LAS_N,
    LAS_SEEDBYTES,
};
use crate::ntt::{inv_ntt, ntt};
use crate::types::{R, R0, T0};
use crate::Q;

const N: usize = 256;
const SHAKE256_RATE: usize = 136;

/// Rejection-sampling attempt counter for the BASE path (measurement only;
/// mirrors the C `base_attempts`), so base and adaptor restart counts can be
/// compared directly.  Never read by the scheme itself.
pub static BASE_ATTEMPTS: AtomicU64 = AtomicU64::new(0);

/* ---- local copies of the helpers (behaviour-identical to las.rs's) ---- */

fn b_pack_poly_canon(a: &R) -> [u8; N * 4] {
    let mut out = [0u8; N * 4];
    for (i, &c) in a.0.iter().enumerate() {
        let x = full_reduce32(c) as u32;
        out[4 * i..4 * i + 4].copy_from_slice(&x.to_le_bytes());
    }
    out
}

fn b_chknorm_vec(z: &[R; LAS_M], bound: i32) -> bool {
    if bound > (Q - 1) / 8 {
        return true;
    }
    z.iter()
        .flat_map(|p| p.0.iter())
        .any(|&x| x.abs() >= bound)
}

fn b_polymul_centered(a: &R, b: &R) -> R {
    let mut ah = ntt(&[a.clone()]);
    let mut bh = ntt(&[b.clone()]);
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

fn b_amul(pp: &LasPp, v: &[R; LAS_M]) -> [R; LAS_N] {
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
        for x in acc.0.iter_mut() {
            *x = partial_reduce32(*x);
        }
        let lo = inv_ntt(&[acc]);
        for n in 0..N {
            w[i].0[n] = full_reduce32(lo[0].0[n] + v[i].0[n]);
        }
    }
    w
}

fn b_challenge(seed: &[u8; LAS_SEEDBYTES]) -> R {
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

/// c = H(pk, w, M) — the BASE hash: the commitment is hashed as-is, NO statement.
fn b_hash_challenge(pk: &LasPk, commit: &[R; LAS_N], m: &[u8]) -> R {
    let mut h = Shake256::default();
    for i in 0..LAS_N {
        h.update(&b_pack_poly_canon(&pk.t[i]));
    }
    for i in 0..LAS_N {
        h.update(&b_pack_poly_canon(&commit[i]));
    }
    h.update(m);
    let mut seed = [0u8; LAS_SEEDBYTES];
    h.finalize_xof().read(&mut seed);
    b_challenge(&seed)
}

fn b_sample_sgamma(seed: &[u8; 64], nonce: u16) -> R {
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

fn b_sample_ternary(seed: &[u8; LAS_SEEDBYTES], nonce: u16) -> R {
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

/* ============================ scheme (Algorithm 1) ============================ */

/// KeyGen = Gen: r <- S_1^(n+l); t = A r; (pk,sk) = (t,r).
pub fn base_keygen(pp: &LasPp, rng: &mut impl CryptoRngCore) -> (LasPk, LasSk) {
    let mut seed = [0u8; LAS_SEEDBYTES];
    rng.fill_bytes(&mut seed);
    base_keygen_seed(pp, &seed)
}

/// Deterministic KeyGen from an explicit 32-byte seed.
pub fn base_keygen_seed(pp: &LasPp, seed: &[u8; LAS_SEEDBYTES]) -> (LasPk, LasSk) {
    let sk = LasSk {
        s: from_fn(|j| b_sample_ternary(seed, j as u16)),
    };
    let pk = LasPk {
        t: b_amul(pp, &sk.s),
    };
    (pk, sk)
}

/// Sign: ordinary simplified Dilithium-style signature; c = H(pk, w, M), no Y.
pub fn base_sign(
    m: &[u8],
    pk: &LasPk,
    sk: &LasSk,
    pp: &LasPp,
    rng: &mut impl CryptoRngCore,
) -> LasSig {
    let mut seed = [0u8; 64];
    rng.fill_bytes(&mut seed);
    let mut nonce: u16 = 0;
    loop {
        BASE_ATTEMPTS.fetch_add(1, Ordering::Relaxed); // instrumentation only
        let mut y: [R; LAS_M] = [R0; LAS_M];
        for j in 0..LAS_M {
            y[j] = b_sample_sgamma(&seed, nonce);
            nonce = nonce.wrapping_add(1);
        }
        let w = b_amul(pp, &y); //  w = A y
        let c = b_hash_challenge(pk, &w, m); //  c = H(pk, w, M)

        let mut z: [R; LAS_M] = [R0; LAS_M];
        for j in 0..LAS_M {
            let cr = b_polymul_centered(&c, &sk.s[j]);
            for n in 0..N {
                z[j].0[n] = y[j].0[n] + cr.0[n]; // z = y + c r
            }
        }
        if b_chknorm_vec(&z, LAS_BOUND_SIGN) {
            continue;
        }
        return LasSig { c, z };
    }
}

/// Verify: recompute w' = A z - c t, accept iff c == H(pk, w', M).
pub fn base_verify(sig: &LasSig, m: &[u8], pk: &LasPk, pp: &LasPp) -> bool {
    if b_chknorm_vec(&sig.z, LAS_BOUND_SIGN) {
        return false;
    }
    let mut w = b_amul(pp, &sig.z); // A z
    for j in 0..LAS_N {
        let ct = b_polymul_centered(&sig.c, &pk.t[j]);
        for n in 0..N {
            w[j].0[n] = full_reduce32(w[j].0[n] - ct.0[n]); // w' = A z - c t
        }
    }
    let c2 = b_hash_challenge(pk, &w, m);
    c2 == sig.c
}
