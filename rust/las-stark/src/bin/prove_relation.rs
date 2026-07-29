//! `prove_relation` -- read the golden verification vectors, sanity-check the
//! native relation oracle against the C golden `w'`, then produce a Blake3/FRI
//! STARK proof of the ARITHMETIC CORE of `base_verify` at the real ring degree
//! `d = 256`: constraints (1) `||z||inf <= B` and (3)
//! `w'_m = z_top[m] + sum_j A'[m][j] (X) z_bot[j] - c (X) t[m]` for every `m < n`,
//! all bound to ONE shared response `z`.
//!
//! Timings are reported as **mean +/- SD over `--runs` repetitions** (default and
//! floor `MIN_RUNS = 5`), matching the statistical convention of the other
//! benchmarks in this project (`rust/las-swap/src/metrics.rs`,
//! `rust/fips204-las/examples/bench_levels.rs`): the sample is collected in
//! microseconds and the SD is the `n-1` sample standard deviation. Single-shot
//! timings are not reportable.
//!
//! Every number printed is measured on the run that printed it -- nothing here is
//! fabricated, and nothing is written to `evidence/`.
//!
//! NOT proven here: constraints (2) `SampleInBall` and (4) the SHAKE256
//! challenge. `c` and `w'` are PUBLIC inputs, so `z` is bound to `(A', t, c, w')`
//! but not to `(c_tilde, M)`. See the crate README.
//!
//! Run from the crate dir:
//!   cargo run --release --bin prove_relation -- [--runs N] [vectors_dir] [out]
//! Defaults: runs = 5, vectors_dir = ../../evm/test/vectors, out = relation_proof.bin

use las_stark::params::{B, D};
use las_stark::relation;
use las_stark::relation_air::{
    build_main_trace, verify_relation, RelationProver, RelationPublicInputs, RelationWitness,
    AUX_WIDTH, EV_LEN, MAIN_WIDTH,
};
use las_stark::vectors::{norm_inf_vec, VerifyVector};
use std::time::{Duration, Instant};
use winterfell::math::ToElements;
use winterfell::Prover;

/// Statistical floor: never report a mean over fewer runs than this. Mirrors
/// `las_swap::metrics::MIN_RUNS` and `bench_levels::REPS`.
const MIN_RUNS: usize = 5;

/// Mean and `n-1` sample standard deviation of a set of durations, in microseconds.
fn mean_sd(runs: &[Duration]) -> (f64, f64) {
    let n = runs.len();
    assert!(n > 0, "statistics over an empty sample");
    let xs: Vec<f64> = runs.iter().map(|d| d.as_secs_f64() * 1e6).collect();
    let mean = xs.iter().sum::<f64>() / n as f64;
    let sd = if n > 1 {
        (xs.iter().map(|x| (x - mean) * (x - mean)).sum::<f64>() / (n - 1) as f64).sqrt()
    } else {
        0.0
    };
    (mean, sd)
}

/// One `phase   mean   SD` row, printed in milliseconds.
fn row(label: &str, runs: &[Duration]) {
    let (m, s) = mean_sd(runs);
    println!("  {label:<26} {:>12.3} {:>11.3}", m / 1000.0, s / 1000.0);
}

fn main() {
    // --- argument parsing: `--runs N` anywhere, then positional dir / out ----
    let mut runs = MIN_RUNS;
    let mut positional: Vec<String> = Vec::new();
    let mut it = std::env::args().skip(1);
    while let Some(a) = it.next() {
        if a == "--runs" {
            let v = it.next().unwrap_or_else(|| panic!("--runs needs a value"));
            runs = v.parse().unwrap_or_else(|_| panic!("--runs: not a number: {v}"));
        } else {
            positional.push(a);
        }
    }
    assert!(
        runs >= MIN_RUNS,
        "benchmark validity requires >= {MIN_RUNS} repetitions for a meaningful mean/SD, got {runs}"
    );
    let dir = positional.first().cloned().unwrap_or_else(|| "../../evm/test/vectors".to_string());
    let out = positional.get(1).cloned().unwrap_or_else(|| "relation_proof.bin".to_string());

    let vv =
        VerifyVector::load(&dir).unwrap_or_else(|e| panic!("load golden vectors from {dir}: {e}"));

    // Sanity: the native oracle must reproduce the C golden w' before we prove.
    relation::check_relation(&vv).expect("native relation oracle vs golden w'");
    let nz = norm_inf_vec(&vv.z);
    assert!(nz <= B, "golden z violates the norm bound: {nz} > {B}");

    let pub_inputs = RelationPublicInputs::from_vector(&vv);
    let n_pub = pub_inputs.to_elements().len();

    let mut t_witness = Vec::with_capacity(runs);
    let mut t_trace = Vec::with_capacity(runs);
    let mut t_prove = Vec::with_capacity(runs);
    let mut t_verify = Vec::with_capacity(runs);
    let mut sizes = Vec::with_capacity(runs);
    let mut last_bytes = Vec::new();

    for _ in 0..runs {
        // Phase 1: the integer quotient witness (h_m by X^d+1, g_m by q). Its very
        // existence -- the q-division being exact -- is an independent check that
        // the relation holds.
        let t0 = Instant::now();
        let witness = RelationWitness::build(&vv, &pub_inputs)
            .unwrap_or_else(|e| panic!("quotient witness: {e}"));
        t_witness.push(t0.elapsed());

        // Phase 2: the main execution trace (z + h + g + range decompositions).
        let t1 = Instant::now();
        let trace = build_main_trace(&witness);
        t_trace.push(t1.elapsed());

        // Phase 3: the STARK itself. This also builds the auxiliary segment, whose
        // random challenge point is drawn after the main trace is committed.
        let prover = RelationProver::new(las_stark::proof_options(), pub_inputs.clone());
        let t2 = Instant::now();
        let proof = prover.prove(trace).expect("prove");
        t_prove.push(t2.elapsed());

        let bytes = proof.to_bytes();
        sizes.push(bytes.len());
        last_bytes = bytes;

        // Phase 4: verification of the proof just produced. The public-input clone
        // is taken OUTSIDE the timer -- verify_relation consumes them by value, but
        // a 176 KB copy is not part of verification.
        let pi = pub_inputs.clone();
        let t3 = Instant::now();
        verify_relation(proof, pi).expect("self-check: proof must verify");
        t_verify.push(t3.elapsed());
    }

    std::fs::write(&out, &last_bytes).unwrap_or_else(|e| panic!("write {out}: {e}"));

    let smin = *sizes.iter().min().expect("non-empty");
    let smax = *sizes.iter().max().expect("non-empty");
    let size_str =
        if smin == smax { format!("{smin} bytes") } else { format!("{smin}..{smax} bytes") };

    println!("las-stark: proved base_verify constraints (1) + (3) at d = {D}");
    println!("  relation              : w'_m = z_top[m] + sum_j A'[m][j] (X) z_bot[j] - c (X) t[m]");
    println!("  outputs               : all m < n, bound to ONE shared z");
    println!("  oracle w' vs C golden : OK");
    println!("  witness z             : {} coefficients", vv.z.len() * D);
    println!("  measured ||z||inf     : {nz}  (bound B = {B})");
    println!("  trace                 : {EV_LEN} rows x {MAIN_WIDTH} main + {AUX_WIDTH} aux columns");
    println!("  public inputs         : {n_pub} field elements (A', t, c, w')");
    println!("  proof size            : {size_str}");
    println!("  written to            : {out}");
    println!();
    println!("  {:<26} {:>12} {:>11}   (n = {runs})", "phase", "mean (ms)", "SD (ms)");
    row("quotient witness build", &t_witness);
    row("main trace build", &t_trace);
    row("STARK prove", &t_prove);
    row("STARK verify", &t_verify);
    println!();
    println!("  NOT proven            : (2) SampleInBall, (4) SHAKE256 challenge -- c and w' are public");
}
