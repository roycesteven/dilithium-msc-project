//! Criterion benchmark — ordinary lattice-based signature (Algorithm 1) vs
//! LAS adaptor signature (Algorithm 2), Simplified Dilithium-III setting.
//!
//! This is the STANDARD statistical benchmark for the Rust port, using
//! Criterion.rs — the same library and style as the upstream fips204 crate's
//! own `benches/benchmark.rs` and the wider Rust cryptography ecosystem
//! (RustCrypto et al.). Criterion provides warm-up, automatic iteration
//! tuning, outlier classification and bootstrap confidence intervals, and can
//! diff runs against saved baselines (`--save-baseline` / `--baseline`).
//!
//! It complements (does not replace) the protocol-level driver
//! `examples/bench_levels.rs`, which additionally reports the rejection-restart
//! counters and the adaptor-overhead summary — see BENCHMARKING.md for when to
//! use which and how to read the numbers.
//!
//! Methodology notes (details in BENCHMARKING.md):
//!  * ONE consistent state, asserted against the full success-path contract
//!    before any measurement (the pre-signature must FAIL the ordinary
//!    verifier), so no failure path is ever timed;
//!  * Sign and PreSign have inherently multimodal timing (Fiat–Shamir with
//!    aborts: 1, 2, 3, … rejection attempts per call, acceptance ≈ 1/e).
//!    Criterion will therefore flag many "outliers" for these two benchmarks —
//!    that is the geometric attempt distribution, not measurement noise.
//!    Use the MEAN as the point estimate for sign-class operations (it includes
//!    restarts, matching the C protocol) and the median for verify-class ones;
//!  * the RNG is a fixed-seed ChaCha8, so the workload is reproducible;
//!  * REJECTION GATE (run validity): for Sign and PreSign the run counts calls
//!    (one Cell increment inside the timed closure — sub-ns against a ~450 us
//!    call, ~0.0001%) and reads the attempt counters outside the timed region,
//!    then HARD-ASSERTS that the measured attempts/call matches the EXACT
//!    theoretical expectation (`las_expected_attempts`: Sign 2.7188, PreSign
//!    2.7748 at the D3 set) within a 5-sigma statistical tolerance for the
//!    number of calls made. A run whose restart rate drifts from theory
//!    (changed bound, broken sampler, biased workload) FAILS LOUDLY instead of
//!    silently producing an invalid log;
//!  * custom Criterion config (`criterion_config()` below): 300 samples per
//!    benchmark over a 60 s measurement window — sized so the sign-class
//!    attempt distribution converges and the PreSign-vs-Sign difference
//!    resolves above measurement noise (full run ≈ 8 minutes);
//!  * raw times are NOT comparable with the C numbers (different compiler and
//!    optimisation profile); compare Algorithm 1 vs Algorithm 2 ratios only.
//!
//! Run:  cargo bench --bench las_bench
//! HTML report: target/criterion/report/index.html

use criterion::{criterion_group, criterion_main, Criterion};
use std::hint::black_box;

use fips204::basesig::{keygen, sign, verify, BASE_ATTEMPTS, BOUND_SIGN};
use fips204::las::{
    adapt, ext, las_expected_attempts, presign, preverify, LAS_ATTEMPTS, BOUND_PRESIGN,
};
use fips204::relation::gen;
use fips204::serialize::{pack_pre_signature, unpack_signature};
use fips204::setup::setup_public_params;
use rand_chacha::rand_core::SeedableRng;
use rand_chacha::ChaCha8Rng;
use std::cell::Cell;
use std::sync::atomic::Ordering;
use std::time::Duration;

const MSG: &[u8] = b"bench message, thirty-three bytes";

/// Rejection gate: assert the measured restart rate of this very run matches
/// the exact theoretical expectation, so every saved log is self-validating.
/// Tolerance: attempts/call is a mean of `calls` i.i.d. geometric draws with
/// SD = E*sqrt(1-1/E), so a 5-sigma band is ~+-1% at criterion's full call
/// counts (>=100k) — far above chance deviation, far below the ~2% shift the
/// nearest real defect (PreSign bound loosened by 1) would cause.
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

fn bench_stage1(c: &mut Criterion) {
    // Fixed public parameters and fixed-seed RNG => reproducible workload.
    let ppseed: [u8; 32] = core::array::from_fn(|i| i as u8);
    let pp = setup_public_params(&ppseed);
    let mut rng = ChaCha8Rng::seed_from_u64(0x4c41_5342_454e_4348); // "LASBENCH"

    // One consistent state, gated on the full success-path contract.
    let (pk, sk) = keygen(&pp, &mut rng);
    let (statement, witness) = gen(&pp, &mut rng);
    let sig_base = sign(MSG, &pk, &sk, &pp, &mut rng);
    let presig = presign(MSG, &statement, &pk, &sk, &pp, &mut rng);
    let adapted = adapt(&presig, MSG, &statement, &witness, &pk, &pp).expect("adapt");

    assert!(verify(&sig_base, MSG, &pk, &pp), "gate: ordinary signature verifies");
    assert!(preverify(&presig, MSG, &statement, &pk, &pp), "gate: pre-signature pre-verifies");
    // Pre-signature is a distinct type from a signature; the "must FAIL ordinary
    // Verify" tripwire runs at the BYTE level (decode its bytes AS a signature).
    let pre_b_gate = pack_pre_signature(&presig).expect("gate: presig packs");
    let pre_as_sig = unpack_signature(&pre_b_gate).expect("gate: presig bytes decode as a signature");
    assert!(!verify(&pre_as_sig, MSG, &pk, &pp), "gate: pre-signature must FAIL ordinary Verify");
    assert!(verify(&adapted, MSG, &pk, &pp), "gate: adapted passes the independent base verifier");
    let wext = ext(&adapted, &presig, &statement, &pp).expect("gate: ext");
    assert!(wext == witness, "gate: ext recovers the witness exactly");

    // ---- Algorithm 1: the ordinary signature (independent module basesig.rs) ----
    let mut g1 = c.benchmark_group("Algorithm 1 - ordinary lattice-based signature");
    g1.bench_function("KeyGen", |b| {
        b.iter(|| black_box(keygen(&pp, &mut rng)));
    });
    let sign_calls = Cell::new(0u64);
    let sign_att0 = BASE_ATTEMPTS.load(Ordering::Relaxed);
    g1.bench_function("Sign", |b| {
        b.iter(|| {
            sign_calls.set(sign_calls.get() + 1);
            black_box(sign(black_box(MSG), &pk, &sk, &pp, &mut rng))
        });
    });
    let sign_attempts = BASE_ATTEMPTS.load(Ordering::Relaxed) - sign_att0;
    g1.bench_function("Verify", |b| {
        b.iter(|| black_box(verify(black_box(&sig_base), MSG, &pk, &pp)));
    });
    g1.finish();
    rejection_gate(
        "Algorithm 1 Sign",
        sign_attempts,
        sign_calls.get(),
        las_expected_attempts(BOUND_SIGN),
    );

    // ---- Algorithm 2: the LAS adaptor signature (las.rs) ----
    let mut g2 = c.benchmark_group("Algorithm 2 - LAS adaptor signature");
    let presign_calls = Cell::new(0u64);
    let presign_att0 = LAS_ATTEMPTS.load(Ordering::Relaxed);
    g2.bench_function("PreSign", |b| {
        b.iter(|| {
            presign_calls.set(presign_calls.get() + 1);
            black_box(presign(black_box(MSG), &statement, &pk, &sk, &pp, &mut rng))
        });
    });
    let presign_attempts = LAS_ATTEMPTS.load(Ordering::Relaxed) - presign_att0;
    g2.bench_function("PreVerify", |b| {
        b.iter(|| black_box(preverify(black_box(&presig), MSG, &statement, &pk, &pp)));
    });
    g2.bench_function("Adapt (including its internal PreVerify)", |b| {
        b.iter(|| black_box(adapt(black_box(&presig), MSG, &statement, &witness, &pk, &pp).is_some()));
    });
    g2.bench_function("Extract", |b| {
        b.iter(|| black_box(ext(black_box(&adapted), &presig, &statement, &pp).is_some()));
    });
    g2.finish();
    rejection_gate(
        "Algorithm 2 PreSign",
        presign_attempts,
        presign_calls.get(),
        las_expected_attempts(BOUND_PRESIGN),
    );
}

criterion_group!{name=benches; config=criterion_config();targets=bench_stage1}
criterion_main!(benches);

fn criterion_config() -> Criterion {
    Criterion::default()
        .measurement_time(Duration::from_secs(60))
        .sample_size(300)
}
