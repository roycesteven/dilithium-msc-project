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
//!  * raw times are NOT comparable with the C numbers (different compiler and
//!    optimisation profile); compare Algorithm 1 vs Algorithm 2 ratios only.
//!
//! Run:  cargo bench --bench las_bench
//! HTML report: target/criterion/report/index.html

use criterion::{criterion_group, criterion_main, Criterion};
use std::hint::black_box;

use fips204::las::{las_adapt, las_ext, las_keygen, las_presign, las_preverify, las_setup};
use fips204::las_basesig::{base_keygen, base_sign, base_verify};
use rand_chacha::rand_core::SeedableRng;
use rand_chacha::ChaCha8Rng;

const MSG: &[u8] = b"bench message, thirty-three bytes";

fn bench_stage1(c: &mut Criterion) {
    // Fixed public parameters and fixed-seed RNG => reproducible workload.
    let ppseed: [u8; 32] = core::array::from_fn(|i| i as u8);
    let pp = las_setup(&ppseed);
    let mut rng = ChaCha8Rng::seed_from_u64(0x4c41_5342_454e_4348); // "LASBENCH"

    // One consistent state, gated on the full success-path contract.
    let (pk, sk) = las_keygen(&pp, &mut rng);
    let (y_stmt, y_wit) = las_keygen(&pp, &mut rng);
    let sig_base = base_sign(MSG, &pk, &sk, &pp, &mut rng);
    let presig = las_presign(MSG, &y_stmt, &pk, &sk, &pp, &mut rng);
    let adapted = las_adapt(&presig, MSG, &y_stmt, &y_wit, &pk, &pp).expect("adapt");

    assert!(base_verify(&sig_base, MSG, &pk, &pp), "gate: ordinary signature verifies");
    assert!(las_preverify(&presig, MSG, &y_stmt, &pk, &pp), "gate: pre-signature pre-verifies");
    assert!(!base_verify(&presig, MSG, &pk, &pp), "gate: pre-signature must FAIL ordinary Verify");
    assert!(base_verify(&adapted, MSG, &pk, &pp), "gate: adapted passes the independent base verifier");
    let yext = las_ext(&adapted, &presig, &y_stmt, &pp).expect("gate: ext");
    assert!(yext == y_wit, "gate: ext recovers the witness exactly");

    // ---- Algorithm 1: the ordinary signature (independent module las_basesig.rs) ----
    let mut g1 = c.benchmark_group("Algorithm 1 - ordinary lattice-based signature");
    g1.bench_function("KeyGen", |b| {
        b.iter(|| black_box(base_keygen(&pp, &mut rng)));
    });
    g1.bench_function("Sign", |b| {
        b.iter(|| black_box(base_sign(black_box(MSG), &pk, &sk, &pp, &mut rng)));
    });
    g1.bench_function("Verify", |b| {
        b.iter(|| black_box(base_verify(black_box(&sig_base), MSG, &pk, &pp)));
    });
    g1.finish();

    // ---- Algorithm 2: the LAS adaptor signature (las.rs) ----
    let mut g2 = c.benchmark_group("Algorithm 2 - LAS adaptor signature");
    g2.bench_function("PreSign", |b| {
        b.iter(|| black_box(las_presign(black_box(MSG), &y_stmt, &pk, &sk, &pp, &mut rng)));
    });
    g2.bench_function("PreVerify", |b| {
        b.iter(|| black_box(las_preverify(black_box(&presig), MSG, &y_stmt, &pk, &pp)));
    });
    g2.bench_function("Adapt (including its internal PreVerify)", |b| {
        b.iter(|| black_box(las_adapt(black_box(&presig), MSG, &y_stmt, &y_wit, &pk, &pp).is_some()));
    });
    g2.bench_function("Extract", |b| {
        b.iter(|| black_box(las_ext(black_box(&adapted), &presig, &y_stmt, &pp).is_some()));
    });
    g2.finish();
}

criterion_group!(benches, bench_stage1);
criterion_main!(benches);
