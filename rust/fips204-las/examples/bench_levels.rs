//! Stage-1 benchmark (Rust) — ordinary lattice-based signature (Algorithm 1)
//! vs LAS adaptor signature (Algorithm 2), mirroring the PRIMARY section of the
//! C driver `ref/test/bench_levels.c`.
//!
//! The two paths live in two SEPARATE modules so neither contaminates the other:
//!     BASE path    -> basesig.rs (keygen / sign / verify; no Y)
//!     ADAPTOR path -> las.rs         (presign / preverify / adapt / ext)
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

use fips204::basesig::{
    keygen, keygen_packed, sign, sign_packed, verify, verify_packed, BASE_ATTEMPTS, BOUND_SIGN,
};
use fips204::las::{
    adapt, adapt_packed, ext, ext_packed, las_expected_attempts, presign, presign_packed,
    preverify, preverify_packed, LAS_ATTEMPTS, BOUND_PRESIGN,
};
use fips204::relation::gen;
use fips204::serialize::{
    pack_pre_signature, pack_public_key, pack_secret_key, pack_signature, pack_statement,
    pack_witness, unpack_signature, PUBLIC_KEY_BYTES, SECRET_KEY_BYTES, SIGNATURE_BYTES,
};
use fips204::setup::{setup_public_params, ELL, GAMMA, KAPPA, N};
use rand_chacha::rand_core::SeedableRng;
use rand_chacha::ChaCha8Rng;

const REPS: usize = 5; // outer repetitions -> mean +/- SD
// Statistical-validity floor: >= 5 repetitions are required for a meaningful
// mean +/- SD -- single shots are unreliable under machine-load and
// rejection-sampling variance.  Enforced at COMPILE time (mirrors the C twin's
// _Static_assert in bench_levels.c).
const _: () = assert!(REPS >= 5, "benchmark validity requires >= 5 repetitions for a meaningful mean/SD");
// Sign-class ops include the rejection restarts, so the attempt count must be
// large enough for attempts/op to converge to the ~e design target on BOTH
// paths; otherwise the PreSign-vs-Sign ratio is dominated by sampling luck.
// The per-attempt (rejection-normalised) diagnostic below removes what remains.
const NITER_SIGN: usize = 1000;
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
    // Sample SD (Bessel, n-1), matching the C driver's stats() (bench_levels.c)
    // and bench_criterion.c, so C and Rust report dispersion by the identical
    // estimator -- a population SD (/n) would print error bars ~10.6% smaller
    // than C for the same data at n = REPS = 5.
    let var = if n > 1.0 {
        xs.iter().map(|x| (x - mean) * (x - mean)).sum::<f64>() / (n - 1.0)
    } else {
        0.0
    };
    (mean, var.sqrt())
}

/// Run-validity gate — same 5-sigma check as benches/las_bench.rs: the restart
/// rate measured in THIS run must match the exact expectation derived from the
/// paper's rejection bounds (eprint 2020/845 Alg. 1 step 11 / Alg. 2 step 6;
/// see `las_expected_attempts`). Attempts/call over `calls` i.i.d. geometric
/// draws has SD = E*sqrt(1-1/E), so the band is 5*SD/sqrt(calls) — at this
/// driver's 5000 calls that is ~+-6%: a coarse gate against gross breakage
/// (the Criterion run's >=100k calls give the tight ~+-1% version).
fn rejection_gate(label: &str, attempts: u64, calls: u64, theory: f64) {
    assert!(calls > 0, "rejection gate: no calls counted for {label}");
    let measured = attempts as f64 / calls as f64;
    let sigma = theory * (1.0 - 1.0 / theory).sqrt();
    let tol = 5.0 * sigma / (calls as f64).sqrt();
    println!(
        "rejection gate [{label}]: {calls} calls, measured {measured:.4} attempts/call \
         (acceptance {:.2}%) vs theory {theory:.4} ({:.2}%), 5-sigma tolerance +-{tol:.4} => {}",
        100.0 / measured,
        100.0 / theory,
        if (measured - theory).abs() <= tol { "OK" } else { "FAIL" },
    );
    assert!(
        (measured - theory).abs() <= tol,
        "rejection gate [{label}]: measured {measured:.4} attempts/call deviates from the \
         theoretical {theory:.4} by more than the 5-sigma tolerance +-{tol:.4}; \
         the rejection loop is not behaving as designed — this run is NOT valid evidence"
    );
}

fn main() {
    // Fixed public parameters; fixed-seed RNG => identical workload every run
    // (repetition-to-repetition SD then measures timing noise only).
    let ppseed: [u8; 32] = core::array::from_fn(|i| i as u8);
    let pp = setup_public_params(&ppseed);
    let mut rng = ChaCha8Rng::seed_from_u64(0x4c41_5342_454e_4348); // "LASBENCH"

    // ---- one consistent state, contract-gated before any timing ----
    let (pk, sk) = keygen(&pp, &mut rng);
    let (statement, witness) = gen(&pp, &mut rng); // statement/witness = another key pair
    let sig_base = sign(MSG, &pk, &sk, &pp, &mut rng);
    let presig = presign(MSG, &statement, &pk, &sk, &pp, &mut rng);
    let adapted = adapt(&presig, MSG, &statement, &witness, &pk, &pp).expect("adapt");

    assert!(verify(&sig_base, MSG, &pk, &pp), "gate: ordinary signature verifies");
    assert!(preverify(&presig, MSG, &statement, &pk, &pp), "gate: pre-signature pre-verifies");
    // Pre-signature is a distinct type from a signature; the "must FAIL ordinary
    // Verify" tripwire runs at the BYTE level (decode its bytes AS a signature).
    let pre_b_gate = pack_pre_signature(&presig).expect("gate: presig packs");
    let pre_as_sig = unpack_signature(&pre_b_gate).expect("gate: presig bytes decode as a signature");
    assert!(
        !verify(&pre_as_sig, MSG, &pk, &pp),
        "gate: pre-signature must FAIL the ordinary verifier"
    );
    assert!(
        verify(&adapted, MSG, &pk, &pp),
        "gate: adapted signature passes the INDEPENDENT base verifier"
    );
    let wext = ext(&adapted, &presig, &statement, &pp).expect("gate: ext");
    assert!(wext == witness, "gate: ext recovers the witness exactly");

    println!("=== LAS Stage-1 benchmark (Rust port) ===");
    println!("ordinary lattice-based signature (Algorithm 1, basesig.rs)");
    println!("vs LAS adaptor signature (Algorithm 2, las.rs)");
    println!(
        "parameter set: Simplified Dilithium-III engineering setting \
         (n={N}, ell={ELL}, kappa={KAPPA}, gamma={GAMMA})"
    );
    println!(
        "packed sizes: public key {PUBLIC_KEY_BYTES} B | secret key / witness {SECRET_KEY_BYTES} B | \
         signature {SIGNATURE_BYTES} B (ordinary = pre-signature = adapted)"
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
            black_box(keygen(&pp, &mut rng));
        }));

        BASE_ATTEMPTS.store(0, Ordering::Relaxed);
        let per_op = time_us(NITER_SIGN, || {
            black_box(sign(black_box(MSG), &pk, &sk, &pp, &mut rng));
        });
        let att = BASE_ATTEMPTS.load(Ordering::Relaxed);
        t_sign.push(per_op);
        t_sign_att.push(per_op * NITER_SIGN as f64 / att as f64);
        base_att += att;
        base_ops += NITER_SIGN as u64;

        t_verify.push(time_us(NITER_FAST, || {
            black_box(verify(black_box(&sig_base), MSG, &pk, &pp));
        }));

        // ---- Algorithm 2: the adaptor path ----
        LAS_ATTEMPTS.store(0, Ordering::Relaxed);
        let per_op = time_us(NITER_SIGN, || {
            black_box(presign(black_box(MSG), &statement, &pk, &sk, &pp, &mut rng));
        });
        let att = LAS_ATTEMPTS.load(Ordering::Relaxed);
        t_presign.push(per_op);
        t_presign_att.push(per_op * NITER_SIGN as f64 / att as f64);
        las_att += att;
        las_ops += NITER_SIGN as u64;

        t_preverify.push(time_us(NITER_FAST, || {
            black_box(preverify(black_box(&presig), MSG, &statement, &pk, &pp));
        }));
        // Adapt checked total: the protocol operation (incl. its internal PreVerify).
        t_adapt.push(time_us(NITER_FAST, || {
            black_box(adapt(black_box(&presig), MSG, &statement, &witness, &pk, &pp).is_some());
        }));
        t_ext.push(time_us(NITER_FAST, || {
            black_box(ext(black_box(&adapted), &presig, &statement, &pp).is_some());
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
    rejection_gate("Algorithm 1 Sign", base_att, base_ops, las_expected_attempts(BOUND_SIGN));
    rejection_gate(
        "Algorithm 2 PreSign",
        las_att,
        las_ops,
        las_expected_attempts(BOUND_PRESIGN),
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
    // ================= TIER 2: end-to-end packed (byte API) =================
    // Full protocol cost INCLUDING packing/unpacking: each *_packed call does
    // validating unpack -> core -> pack INSIDE the call -- the boundary a
    // wire/on-chain consumer pays (mirrors C bench_levels.c TIER 2).  Pack the
    // canonical state ONCE; producing ops write to fresh scratch so the canonical
    // bytes are never overwritten.
    let pk_b = pack_public_key(&pk);
    let sk_b = pack_secret_key(&sk).expect("sk packs");
    let y_b = pack_statement(&statement);
    let w_b = pack_witness(&witness).expect("witness packs");
    let sig_b = pack_signature(&sig_base).expect("sig packs");
    let presig_b = pack_pre_signature(&presig).expect("presig packs");
    let adapted_b = pack_signature(&adapted).expect("adapted packs");

    // byte-level success contract, re-enforced before timing (mirrors the
    // struct-level gate: the pre-signature must FAIL the ordinary verifier).
    assert!(verify_packed(&sig_b, MSG, &pk_b, &pp), "gate: base signature verifies (bytes)");
    assert!(preverify_packed(&presig_b, MSG, &y_b, &pk_b, &pp), "gate: pre-signature pre-verifies (bytes)");
    assert!(!verify_packed(&presig_b, MSG, &pk_b, &pp), "gate: pre-signature FAILS the ordinary verifier (bytes)");
    assert!(verify_packed(&adapted_b, MSG, &pk_b, &pp), "gate: adapted signature verifies (bytes)");
    assert!(
        ext_packed(&adapted_b, &presig_b, &y_b, &pp).as_ref() == Some(&w_b),
        "gate: Ext recovers the witness bytes exactly"
    );

    let mut tp_keygen = vec![];
    let mut tp_sign = vec![];
    let mut tp_verify = vec![];
    let mut tp_presign = vec![];
    let mut tp_preverify = vec![];
    let mut tp_adapt = vec![];
    let mut tp_ext = vec![];
    let mut base_ops_p = 0u64;
    let mut base_att_p = 0u64;
    let mut las_ops_p = 0u64;
    let mut las_att_p = 0u64;

    for _rep in 0..REPS {
        tp_keygen.push(time_us(NITER_FAST, || {
            black_box(keygen_packed(&pp, &mut rng));
        }));

        BASE_ATTEMPTS.store(0, Ordering::Relaxed);
        tp_sign.push(time_us(NITER_SIGN, || {
            black_box(sign_packed(black_box(MSG), &pk_b, &sk_b, &pp, &mut rng));
        }));
        base_att_p += BASE_ATTEMPTS.load(Ordering::Relaxed);
        base_ops_p += NITER_SIGN as u64;

        tp_verify.push(time_us(NITER_FAST, || {
            black_box(verify_packed(black_box(&sig_b), MSG, &pk_b, &pp));
        }));

        LAS_ATTEMPTS.store(0, Ordering::Relaxed);
        tp_presign.push(time_us(NITER_SIGN, || {
            black_box(presign_packed(black_box(MSG), &y_b, &pk_b, &sk_b, &pp, &mut rng));
        }));
        las_att_p += LAS_ATTEMPTS.load(Ordering::Relaxed);
        las_ops_p += NITER_SIGN as u64;

        tp_preverify.push(time_us(NITER_FAST, || {
            black_box(preverify_packed(black_box(&presig_b), MSG, &y_b, &pk_b, &pp));
        }));
        tp_adapt.push(time_us(NITER_FAST, || {
            black_box(adapt_packed(black_box(&presig_b), MSG, &y_b, &w_b, &pk_b, &pp).is_some());
        }));
        tp_ext.push(time_us(NITER_FAST, || {
            black_box(ext_packed(black_box(&adapted_b), &presig_b, &y_b, &pp).is_some());
        }));
    }

    println!();
    println!("=== TIER 2: end-to-end packed (validating unpack -> core -> pack inside each call) ===");
    let rows_packed: [(&str, &Vec<f64>); 7] = [
        ("Algorithm 1  KeyGen_packed", &tp_keygen),
        ("Algorithm 1  Sign_packed", &tp_sign),
        ("Algorithm 1  Verify_packed", &tp_verify),
        ("Algorithm 2  PreSign_packed", &tp_presign),
        ("Algorithm 2  PreVerify_packed", &tp_preverify),
        ("Algorithm 2  Adapt_packed (incl. PreVerify)", &tp_adapt),
        ("Algorithm 2  Extract_packed", &tp_ext),
    ];
    println!("{:<44} {:>12} {:>10}", "operation", "mean (us)", "SD (us)");
    for (name, xs) in rows_packed {
        let (m, s) = mean_sd(xs);
        println!("{name:<44} {m:>12.1} {s:>10.1}");
    }
    println!();

    let (psign_m, _) = mean_sd(&tp_sign);
    let (pverify_m, _) = mean_sd(&tp_verify);
    let (ppresign_m, _) = mean_sd(&tp_presign);
    let (ppreverify_m, _) = mean_sd(&tp_preverify);
    let (padapt_m, _) = mean_sd(&tp_adapt);
    println!(
        "packed adaptor overhead (per operation): PreSign vs Sign {:+.1}% | \
         PreVerify vs Verify {:+.1}% | Adapt vs Verify {:+.1}%",
        (ppresign_m - psign_m) / psign_m * 100.0,
        (ppreverify_m - pverify_m) / pverify_m * 100.0,
        (padapt_m - pverify_m) / pverify_m * 100.0,
    );
    // Codec boundary cost = packed - core (per op): the price of the byte
    // boundary (validating unpack of the inputs + pack of the output).
    let (keygen_m, _) = mean_sd(&t_keygen);
    let (ext_m, _) = mean_sd(&t_ext);
    let (pkeygen_m, _) = mean_sd(&tp_keygen);
    let (pext_m, _) = mean_sd(&tp_ext);
    println!(
        "codec boundary cost (packed - core, us): KeyGen {:+.1} | Sign {:+.1} | Verify {:+.1} | \
         PreSign {:+.1} | PreVerify {:+.1} | Adapt {:+.1} | Ext {:+.1}",
        pkeygen_m - keygen_m,
        psign_m - sign_m,
        pverify_m - verify_m,
        ppresign_m - presign_m,
        ppreverify_m - preverify_m,
        padapt_m - adapt_m,
        pext_m - ext_m,
    );
    rejection_gate("Algorithm 1 Sign (packed tier)", base_att_p, base_ops_p, las_expected_attempts(BOUND_SIGN));
    rejection_gate("Algorithm 2 PreSign (packed tier)", las_att_p, las_ops_p, las_expected_attempts(BOUND_PRESIGN));

    println!(
        "contract (asserted before timing): ordinary verifies; pre-signature FAILS the \
         ordinary verifier; adapted passes the independent base verifier; Ext recovers \
         the witness exactly"
    );
}
