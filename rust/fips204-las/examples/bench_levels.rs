//! Stage-1 benchmark (Rust) — ordinary lattice-based signature (Algorithm 1)
//! vs LAS adaptor signature (Algorithm 2), mirroring the PRIMARY section of the
//! C driver `ref/test/bench_levels.c`.
//!
//! The two paths live in two SEPARATE modules so neither contaminates the other:
//!     BASE path    -> las_basesig.rs (base_keygen / base_sign / base_verify; no Y)
//!     ADAPTOR path -> las.rs         (las_presign / las_preverify / las_adapt / las_ext)
//!
//! Protocol (mirrors the C driver):
//!  * ONE consistent state per run (key pair, statement/witness, and the
//!    signature / pre-signature / adapted signature derived from that key);
//!  * all timing gated on the full success-path contract — the ordinary
//!    signature verifies, the pre-signature pre-verifies but FAILS the ordinary
//!    verifier (statement-binding tripwire), the adapted signature passes the
//!    INDEPENDENT base verifier, and Ext recovers the witness exactly — so no
//!    failure or early-return path is ever timed;
//!  * per-operation timings are the PRIMARY result; REPS repetitions, mean ± SD;
//!  * rejection restarts are read DIRECTLY off the per-module attempt counters
//!    (BASE_ATTEMPTS / LAS_ATTEMPTS), never estimated from a timing ratio.
//!
//! Run:  cargo run --release --example bench_levels
//!
//! NOTE: raw microseconds are NOT comparable with the C numbers (different
//! compiler and optimisation profile — upstream release profile is
//! opt-level="s" + lto). The comparable quantity is the Algorithm 1 vs
//! Algorithm 2 overhead ratio WITHIN this run.

use std::hint::black_box;
use std::sync::atomic::Ordering;
use std::time::Instant;

use fips204::las::{
    las_adapt, las_ext, las_keygen, las_presign, las_preverify, las_setup, LAS_ATTEMPTS,
    LAS_ELL, LAS_GAMMA, LAS_KAPPA, LAS_N,
};
use fips204::las_basesig::{base_keygen, base_sign, base_verify, BASE_ATTEMPTS};
use fips204::las_serialize::{LAS_PK_BYTES, LAS_SIG_BYTES, LAS_SK_BYTES};
use rand_chacha::rand_core::SeedableRng;
use rand_chacha::ChaCha8Rng;

const REPS: usize = 5; // outer repetitions -> mean +/- SD
// Sign-class ops include the rejection restarts, so the attempt count must be
// large enough for attempts/op to converge to the ~e design target on BOTH
// paths; otherwise the PreSign-vs-Sign ratio is dominated by sampling luck.
// The per-attempt (rejection-normalised) diagnostic below removes what remains.
const NITER_SIGN: usize = 500;
const NITER_FAST: usize = 1000; // verify-class ops
const MSG: &[u8] = b"bench message, thirty-three bytes";

fn time_us<F: FnMut()>(iters: usize, mut f: F) -> f64 {
    let t0 = Instant::now();
    for _ in 0..iters {
        f();
    }
    t0.elapsed().as_secs_f64() * 1e6 / iters as f64
}

fn mean_sd(xs: &[f64]) -> (f64, f64) {
    let n = xs.len() as f64;
    let mean = xs.iter().sum::<f64>() / n;
    let var = xs.iter().map(|x| (x - mean) * (x - mean)).sum::<f64>() / n;
    (mean, var.sqrt())
}

fn main() {
    // Fixed public parameters; fixed-seed RNG => identical workload every run
    // (repetition-to-repetition SD then measures timing noise only).
    let ppseed: [u8; 32] = core::array::from_fn(|i| i as u8);
    let pp = las_setup(&ppseed);
    let mut rng = ChaCha8Rng::seed_from_u64(0x4c41_5342_454e_4348); // "LASBENCH"

    // ---- one consistent state, contract-gated before any timing ----
    let (pk, sk) = las_keygen(&pp, &mut rng);
    let (y_stmt, y_wit) = las_keygen(&pp, &mut rng); // statement/witness = another key pair
    let sig_base = base_sign(MSG, &pk, &sk, &pp, &mut rng);
    let presig = las_presign(MSG, &y_stmt, &pk, &sk, &pp, &mut rng);
    let adapted = las_adapt(&presig, MSG, &y_stmt, &y_wit, &pk, &pp).expect("adapt");

    assert!(base_verify(&sig_base, MSG, &pk, &pp), "gate: ordinary signature verifies");
    assert!(las_preverify(&presig, MSG, &y_stmt, &pk, &pp), "gate: pre-signature pre-verifies");
    assert!(
        !base_verify(&presig, MSG, &pk, &pp),
        "gate: pre-signature must FAIL the ordinary verifier"
    );
    assert!(
        base_verify(&adapted, MSG, &pk, &pp),
        "gate: adapted signature passes the INDEPENDENT base verifier"
    );
    let yext = las_ext(&adapted, &presig, &y_stmt, &pp).expect("gate: ext");
    assert!(yext == y_wit, "gate: ext recovers the witness exactly");

    println!("=== LAS Stage-1 benchmark (Rust port) ===");
    println!("ordinary lattice-based signature (Algorithm 1, las_basesig.rs)");
    println!("vs LAS adaptor signature (Algorithm 2, las.rs)");
    println!(
        "parameter set: Simplified Dilithium-III engineering setting \
         (n={LAS_N}, ell={LAS_ELL}, kappa={LAS_KAPPA}, gamma={LAS_GAMMA})"
    );
    println!(
        "packed sizes: public key {LAS_PK_BYTES} B | secret key / witness {LAS_SK_BYTES} B | \
         signature {LAS_SIG_BYTES} B (ordinary = pre-signature = adapted)"
    );
    println!(
        "protocol: {REPS} repetitions, {NITER_SIGN} iterations/sign-class op, \
         {NITER_FAST} iterations/verify-class op, mean +/- SD; success-path contract gated"
    );
    println!("build: cargo --release (upstream profile: opt-level=\"s\", lto)");
    println!();

    let mut t_keygen = vec![];
    let mut t_sign = vec![];
    let mut t_verify = vec![];
    let mut t_presign = vec![];
    let mut t_preverify = vec![];
    let mut t_adapt = vec![];
    let mut t_ext = vec![];
    let mut t_sign_att = vec![]; // per-attempt (rejection-normalised) diagnostics
    let mut t_presign_att = vec![];
    let mut base_ops = 0u64;
    let mut base_att = 0u64;
    let mut las_ops = 0u64;
    let mut las_att = 0u64;

    for _rep in 0..REPS {
        // ---- Algorithm 1: the base path (independent module) ----
        t_keygen.push(time_us(NITER_FAST, || {
            black_box(base_keygen(&pp, &mut rng));
        }));

        BASE_ATTEMPTS.store(0, Ordering::Relaxed);
        let per_op = time_us(NITER_SIGN, || {
            black_box(base_sign(black_box(MSG), &pk, &sk, &pp, &mut rng));
        });
        let att = BASE_ATTEMPTS.load(Ordering::Relaxed);
        t_sign.push(per_op);
        t_sign_att.push(per_op * NITER_SIGN as f64 / att as f64);
        base_att += att;
        base_ops += NITER_SIGN as u64;

        t_verify.push(time_us(NITER_FAST, || {
            black_box(base_verify(black_box(&sig_base), MSG, &pk, &pp));
        }));

        // ---- Algorithm 2: the adaptor path ----
        LAS_ATTEMPTS.store(0, Ordering::Relaxed);
        let per_op = time_us(NITER_SIGN, || {
            black_box(las_presign(black_box(MSG), &y_stmt, &pk, &sk, &pp, &mut rng));
        });
        let att = LAS_ATTEMPTS.load(Ordering::Relaxed);
        t_presign.push(per_op);
        t_presign_att.push(per_op * NITER_SIGN as f64 / att as f64);
        las_att += att;
        las_ops += NITER_SIGN as u64;

        t_preverify.push(time_us(NITER_FAST, || {
            black_box(las_preverify(black_box(&presig), MSG, &y_stmt, &pk, &pp));
        }));
        // Adapt checked total: the protocol operation (incl. its internal PreVerify).
        t_adapt.push(time_us(NITER_FAST, || {
            black_box(las_adapt(black_box(&presig), MSG, &y_stmt, &y_wit, &pk, &pp).is_some());
        }));
        t_ext.push(time_us(NITER_FAST, || {
            black_box(las_ext(black_box(&adapted), &presig, &y_stmt, &pp).is_some());
        }));
    }

    let rows: [(&str, &Vec<f64>); 7] = [
        ("Algorithm 1  KeyGen", &t_keygen),
        ("Algorithm 1  Sign", &t_sign),
        ("Algorithm 1  Verify", &t_verify),
        ("Algorithm 2  PreSign", &t_presign),
        ("Algorithm 2  PreVerify", &t_preverify),
        ("Algorithm 2  Adapt (incl. PreVerify)", &t_adapt),
        ("Algorithm 2  Extract", &t_ext),
    ];
    println!("{:<38} {:>12} {:>10}", "operation", "mean (us)", "SD (us)");
    for (name, xs) in rows {
        let (m, s) = mean_sd(xs);
        println!("{name:<38} {m:>12.1} {s:>10.1}");
    }
    println!();

    let (sign_m, _) = mean_sd(&t_sign);
    let (verify_m, _) = mean_sd(&t_verify);
    let (presign_m, _) = mean_sd(&t_presign);
    let (preverify_m, _) = mean_sd(&t_preverify);
    let (adapt_m, _) = mean_sd(&t_adapt);
    println!(
        "adaptor overhead (per operation): PreSign vs Sign {:+.1}% | \
         PreVerify vs Verify {:+.1}% | Adapt vs Verify {:+.1}%",
        (presign_m - sign_m) / sign_m * 100.0,
        (preverify_m - verify_m) / verify_m * 100.0,
        (adapt_m - verify_m) / verify_m * 100.0,
    );
    println!(
        "rejection sampling (measured via attempt counters): \
         base {:.2} attempts/signature (acceptance {:.1}%) | \
         adaptor {:.2} attempts/pre-signature (acceptance {:.1}%)",
        base_att as f64 / base_ops as f64,
        base_ops as f64 / base_att as f64 * 100.0,
        las_att as f64 / las_ops as f64,
        las_ops as f64 / las_att as f64 * 100.0,
    );
    // Rejection-normalised diagnostic: per-ATTEMPT cost removes the residual
    // restart-count luck between the two paths, isolating the pure adaptor
    // cost of one Fiat-Shamir pass (the w+Y addition and the extra Y hashing).
    let (sign_att_m, sign_att_s) = mean_sd(&t_sign_att);
    let (presign_att_m, presign_att_s) = mean_sd(&t_presign_att);
    println!(
        "per-attempt diagnostic (rejection-normalised): \
         Sign {sign_att_m:.1} +/- {sign_att_s:.1} us | \
         PreSign {presign_att_m:.1} +/- {presign_att_s:.1} us | overhead {:+.1}%",
        (presign_att_m - sign_att_m) / sign_att_m * 100.0,
    );
    println!(
        "contract (asserted before timing): ordinary verifies; pre-signature FAILS the \
         ordinary verifier; adapted passes the independent base verifier; Ext recovers \
         the witness exactly"
    );
}
