//! The SEPARATE simplified Dilithium-style BASE signature = Algorithm 1 of
//! eprint 2020/845, written as a STRUCTURAL MIRROR of the upstream ML-DSA
//! reference `src/ml_dsa.rs` so the diff shows exactly what Algorithm 1 removes.
//!
//!   `base_sign_keypair`    <->  `key_gen` / `key_gen_internal` (ml_dsa.rs:26/57)
//!   `base_sign_signature`  <->  `sign_internal`                (ml_dsa.rs:153)
//!   `base_sign_verify`     <->  `verify_internal`              (ml_dsa.rs:351)
//!
//! The names are base-tagged (not the literal `key_gen`/`sign_internal`) so a
//! `grep` never confuses the simplified base with the real ML-DSA; each function
//! keeps ml_dsa.rs's step-numbered comment structure (`// 1:`, `// 11:`, ...) and
//! every block is annotated REUSED / CHANGED / DELETED against the corresponding
//! ml_dsa.rs line.  This is the Rust twin of `ref/basesig.c`.
//!
//! Algorithm 1 (paper p.7-8):
//!     KeyGen : r <- S_1^{n+l};  t = A r;  (pk, sk) = (t, r)
//!     Sign   : y <- S_g; w = A y; c = H(pk, w, M); z = y + c r; reject |z|inf > g-k
//!     Verify : w' = A z - c t;   accept iff  c == H(pk, w', M)
//!
//! What Algorithm 1 DELETES vs ML-DSA (all "for ease of presentation", paper
//! s2.2/s3.2): Power2Round key compression, the high/low-bit split, the hint
//! vector (MakeHint/UseHint, the omega bound), the second (low-bits) rejection,
//! and hashing only the high bits.  What it CHANGES: ExpandS(eta) -> ternary
//! S_1; ExpandMask(gamma1) -> uniform S_gamma; SampleInBall(tau) -> kappa-weight
//! challenge; and it hashes the FULL commitment w.
//!
//! Kept SEPARATE from `las.rs` (depends on it ONLY for the shared parameters and
//! the `LasPp`/`LasPk`/`LasSk`/`LasSig` struct layout) and behaviour-identical to
//! it, so `A*r` and the challenge hash match `las.rs` bit-for-bit and an Adapted
//! LAS pre-signature verifies under `base_sign_verify` with no explicit `+Y`:
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
use crate::types::{R, R0, T, T0};
use crate::Q;
use zeroize::Zeroize;

const N: usize = 256;
const SHAKE256_RATE: usize = 136;

/// Rejection-sampling attempt counter for the BASE path (measurement only;
/// mirrors the C `base_attempts`; no ml_dsa.rs analogue).  Never read by the
/// scheme itself; benchmarks reset and read it to report the restart rate.
pub static BASE_ATTEMPTS: AtomicU64 = AtomicU64::new(0);

/* ---- local helpers (behaviour-identical to las.rs's).  Correspondence to
 * ml_dsa.rs primitives:
 *   b_sample_ternary   <->  expand_s      (ml_dsa.rs:79)  -- CHANGED (S_1)
 *   b_sample_sgamma    <->  expand_mask   (ml_dsa.rs:215) -- CHANGED (S_gamma)
 *   b_challenge        <->  sample_in_ball(ml_dsa.rs:237) -- CHANGED (kappa)
 *   b_amul             <->  expand_a + mat_vec_mul (+ identity block)
 *   b_ntt_a/b_ntt_b_mont/b_polymul_prehat <-> the c_hat/s_1_hat_mont pre-computes
 *                                              + inv_ntt(c_hat o s_hat) products
 *   b_hash_challenge   <->  the mu||w1Encode SHAKE (ml_dsa.rs:230-234) -- FULL w. */

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

/* NTT-hoisted product helpers, behaviour-identical to las.rs's ntt_a /
 * ntt_b_mont / polymul_prehat: the invariant operand (secret r, public t) is
 * transformed once per call and the challenge once per attempt / per verify,
 * exactly as ml_dsa.rs pre-computes s_1_hat_mont and takes c_hat = NTT(c) once. */
fn b_ntt_a(a: &R) -> T {
    let mut ah = ntt(&[a.clone()]);
    for x in ah[0].0.iter_mut() {
        *x = partial_reduce32(*x);
    }
    let [ah] = ah;
    ah
}

fn b_ntt_b_mont(b: &R) -> T {
    let mut bh = ntt(&[b.clone()]);
    for x in bh[0].0.iter_mut() {
        *x = partial_reduce32(*x);
    }
    let bm = to_mont(&bh);
    let [bm] = bm;
    bm
}

fn b_polymul_prehat(ah: &T, bm: &T) -> R {
    let mut ch = T0;
    for n in 0..N {
        ch.0[n] = mont_reduce(i64::from(ah.0[n]) * i64::from(bm.0[n]));
    }
    let prod = inv_ntt(&[ch]);
    R(from_fn(|n| center_mod(prod[0].0[n])))
}

/// w = A*v = v_top + A'*v_bot, A=[I|A'], A' already in NTT domain.  The A'*v_bot
/// part is ml_dsa.rs's mat_vec_mul; the identity block (+ v_top) is HNF (no s2).
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

/// SampleInBall with kappa (not tau) 1-coefficients.  <-> ml_dsa.rs sample_in_ball.
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

/// c = H(pk, w, M) — Algorithm 1 hashes the FULL commitment w.  This is
/// ml_dsa.rs's `H(mu || w1Encode(w_1))` with HighBits DELETED (the whole w bound).
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

/// Mask y <- S_gamma (uniform [-gamma, gamma]).  <-> ml_dsa.rs expand_mask, whose
/// gamma1 is a fixed power of two; here gamma = kappa*d*(n+l).
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

/// Secret r <- S_1 (ternary).  <-> ml_dsa.rs expand_s, with eta -> 1.
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

/// `base_sign_keypair` <-> `key_gen` (ml_dsa.rs:26): draw a seed, expand a key.
/// The RNG is injected (Rust idiom), as ml_dsa.rs's `key_gen(rng)`.
pub fn base_sign_keypair(pp: &LasPp, rng: &mut impl CryptoRngCore) -> (LasPk, LasSk) {
    // 1: xi <- B^32                      (ml_dsa.rs:40)  [REUSED] random seed
    let mut seed = [0u8; LAS_SEEDBYTES];
    rng.fill_bytes(&mut seed);
    // 5: return KeyGen_internal(xi)      (ml_dsa.rs:44)
    let out = base_sign_keypair_seed(pp, &seed);
    seed.zeroize(); // sk is derivable from this seed: wipe
    out
}

/// `base_sign_keypair_seed` <-> `key_gen_internal` (ml_dsa.rs:57), Algorithm 1 KeyGen.
/// Block-by-block against ml_dsa.rs:
///   [CHANGED] (rho,rho',K) <- H(xi..)  -> the seed samples r directly (:67-74)
///   [CHANGED] (s1,s2) <- ExpandS(rho') -> r <- S_1^{n+l} ternary        (:79)
///   [CHANGED] t = NTT-1(A o NTT(s1))+s2 -> t = A r (identity block = s2) (:84-92)
///   [DELETED] (t1,t0) <- Power2Round(t)                                 (:92)
///   [DELETED] pkEncode / tr=H(pk) / skEncode + precomputes           (:100-130)
pub fn base_sign_keypair_seed(pp: &LasPp, seed: &[u8; LAS_SEEDBYTES]) -> (LasPk, LasSk) {
    let sk = LasSk {
        s: from_fn(|j| b_sample_ternary(seed, j as u16)), // [CHANGED] ternary r
    };
    let pk = LasPk {
        t: b_amul(pp, &sk.s), // [CHANGED] t = A r ; [DELETED] Power2Round
    };
    (pk, sk)
}

/// `base_sign_signature` <-> `sign_internal` (ml_dsa.rs:153), Algorithm 1 Sign.
/// Block-by-block against ml_dsa.rs:
///   [DELETED] skDecode                                        (:166)
///   [CHANGED] mu=H(tr||M); rho'=H(K||rnd||mu) -> fresh mask seed (:183-201)
///   [REUSED ] kappa counter / the `loop`                       (:204-212)
///   [CHANGED] y <- ExpandMask -> y <- S_gamma                  (:215)
///   [CHANGED] w = NTT-1(A o NTT(y)) -> w = A y                 (:217-222)
///   [DELETED] w_1 <- HighBits(w)                               (:224-226)
///   [CHANGED] c_tilde <- H(mu||w1Encode) -> c = H(pk,w,M) full (:230-234)
///   [CHANGED] c <- SampleInBall (tau) -> kappa challenge       (:237)
///   [REUSED ] c_hat <- NTT(c) once per attempt                 (:240)
///   [CHANGED] cs1 <- NTT-1(c_hat o s1_hat); z = y + cs1        (:243-265)
///   [CHANGED] reject ||z||>=g1-b OR ||r0||>=g2-b -> reject |z|inf>g-k (:276-285)
///   [DELETED] c_t0, MakeHint, ||ct0||, hint weight             (:287-319)
///   [DELETED] sigEncode                                        (:334-336)
pub fn base_sign_signature(
    m: &[u8],
    pk: &LasPk,
    sk: &LasSk,
    pp: &LasPp,
    rng: &mut impl CryptoRngCore,
) -> LasSig {
    let mut seed = [0u8; 64]; // [CHANGED] fresh mask seed (no mu/rho' chain)
    rng.fill_bytes(&mut seed);
    let mut nonce: u16 = 0; // [REUSED] the kappa counter
    let s_hat_mont: [T; LAS_M] = from_fn(|j| b_ntt_b_mont(&sk.s[j])); // NTT(r) once per call
    let sig = loop {
        BASE_ATTEMPTS.fetch_add(1, Ordering::Relaxed); // instrumentation only
        // 11: y <- ExpandMask -> S_gamma
        let mut y: [R; LAS_M] = [R0; LAS_M];
        for j in 0..LAS_M {
            y[j] = b_sample_sgamma(&seed, nonce);
            nonce = nonce.wrapping_add(1);
        }
        let w = b_amul(pp, &y); // 12: w = A y   ([DELETED] 13: HighBits)
        let c = b_hash_challenge(pk, &w, m); // 15/16: c = H(pk, w, M) full w
        let c_hat = b_ntt_a(&c); // 17: c_hat <- NTT(c), once per attempt

        // 18/20: z = y + c r
        let mut z: [R; LAS_M] = [R0; LAS_M];
        for j in 0..LAS_M {
            let cr = b_polymul_prehat(&c_hat, &s_hat_mont[j]);
            for n in 0..N {
                z[j].0[n] = y[j].0[n] + cr.0[n];
            }
        }
        // 23: reject |z|inf > g-k   ([DELETED] low-bits + hint checks)
        if b_chknorm_vec(&z, LAS_BOUND_SIGN) {
            continue;
        }
        break LasSig { c, z }; // [DELETED] sigEncode: struct output
    };
    seed.zeroize(); // mask seed: wipe
    sig
}

/// `base_sign_verify` <-> `verify_internal` (ml_dsa.rs:351), Algorithm 1 Verify.
/// Block-by-block against ml_dsa.rs:
///   [DELETED] sigDecode / pkDecode                            (:365-368) struct
///   [CHANGED] reject |z|inf > g-k                              (:378/434)
///   [CHANGED] mu = H(tr||M) -> hash pk directly               (:386-397)
///   [REUSED ] c_hat <- NTT(c) once; NTT(z) via b_amul         (:404-410)
///   [CHANGED] w'approx = Az - c*t1*2^d -> w' = A z - c t exact (:404-417)
///   [DELETED] w'_1 <- UseHint(h, w'approx)                     (:420-422)
///   [CHANGED] c_tilde' = H(mu||w1Encode) -> c2 = H(pk,w',M)    (:427-431)
///   [CHANGED] return ||z||<g1-b AND c_tilde==c_tilde' -> c==c2 (:433-436)
pub fn base_sign_verify(sig: &LasSig, m: &[u8], pk: &LasPk, pp: &LasPp) -> bool {
    if b_chknorm_vec(&sig.z, LAS_BOUND_SIGN) {
        // [CHANGED] |z|inf > g-k
        return false;
    }
    let mut w = b_amul(pp, &sig.z); // A z
    let c_hat = b_ntt_a(&sig.c); // NTT(c) once per call
    for j in 0..LAS_N {
        // w' = A z - c t  (exact; [DELETED] c*t1*2^d + UseHint)
        let ct = b_polymul_prehat(&c_hat, &b_ntt_b_mont(&pk.t[j]));
        for n in 0..N {
            w[j].0[n] = full_reduce32(w[j].0[n] - ct.0[n]);
        }
    }
    let c2 = b_hash_challenge(pk, &w, m); // c2 = H(pk, w', M)
    c2 == sig.c // accept iff c == c2
}
