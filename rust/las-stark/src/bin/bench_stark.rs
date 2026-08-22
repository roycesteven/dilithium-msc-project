//! `bench_stark` -- the benchmark of record for this crate.
//!
//! It measures the TWO AIRs under one identical protocol, in one process, back to
//! back, so the comparison between them is controlled:
//!
//!   * `NormAir`      -- constraint (1) alone, `||z||inf <= B` (the Stage-A gadget);
//!   * `RelationAir`  -- constraints (1) AND (3) at the real degree `d = 256`,
//!                       all `n` outputs bound to one shared `z`.
//!
//! ## Like-for-like: what the headline ratio compares
//!
//! `prove_norm` is a whole pipeline: it builds the trace, constructs the prover,
//! and proves. Timing it against `RelationProver::prove()` ALONE would flatter the
//! relation AIR by hiding its witness and trace construction. The headline ratio
//! therefore compares `prove_norm` against a **total-pipeline timer** covering the
//! relation AIR's quotient-witness build, trace construction, prover construction
//! and `prove()`. The per-phase breakdown is reported as well, so the total stays
//! auditable -- but the ratio is taken on the totals.
//!
//! ## Protocol (aligned with the project's benchmarking methodology)
//!
//! The established protocol (`ref/test/bench_levels.c`,
//! `rust/fips204-las/examples/bench_levels.rs`) is `REPS = 5` outer repetitions
//! reported as mean +/- sample SD, each repetition averaging several hundred inner
//! iterations. The inner loop exists there to amortise timer overhead over
//! microsecond-scale operations; a STARK proof takes ~10^5 times longer, so one
//! proof per repetition already dwarfs the timer resolution and an inner loop
//! would only multiply the runtime.
//!
//! What the inner loop ALSO provided -- a warm cache, a warm allocator and a
//! settled CPU frequency before the timed work -- is supplied here by an explicit
//! **discarded warm-up phase of `WARMUP_SECS` seconds**, matching the ~3 s
//! Criterion warm-up that the supervisor accepted for the Rust measurements
//! (`las-context-consolidated.md` Sec. 15.6). Warm-up results are thrown away.
//!
//! So: warm up >= 3 s (discarded), then `REPS = 5` timed repetitions, reported as
//! mean +/- sample (`n-1`) standard deviation -- the same statistic, the same
//! repetition floor, and the same single machine as every other benchmark here.
//!
//! Allocation that is NOT part of the measured operation is kept out of the timed
//! region: the public inputs are cloned before the verification timer starts, so
//! the verify figure is verification, not a 176 KB copy.
//!
//! Every number printed is measured on the run that printed it.
//!
//! Run from the crate dir:
//!   cargo run --release --bin bench_stark -- [--runs N] [vectors_dir]

use las_stark::air::{TRACE_LEN, TRACE_WIDTH};
use las_stark::params::{B, D};
use las_stark::prover::{prove_norm, verify_norm};
use las_stark::relation;
use las_stark::relation_air::{
    build_main_trace, verify_relation, RelationProver, RelationPublicInputs, RelationWitness,
    AUX_WIDTH, EV_LEN, MAIN_WIDTH,
};
use las_stark::vectors::{norm_inf_vec, VerifyVector};
use std::hint::black_box;
use std::time::{Duration, Instant};
use winterfell::math::ToElements;
use winterfell::Prover;

/// Outer repetitions -> mean +/- SD. Mirrors `REPS` in the C and Rust drivers.
const REPS: usize = 5;
/// Statistical-validity floor, enforced at COMPILE time exactly as the twins do.
const _: () =
    assert!(REPS >= 5, "benchmark validity requires >= 5 repetitions for a meaningful mean/SD");
/// Discarded warm-up, in seconds. Mirrors the ~3 s Criterion warm-up accepted in
/// `las-context-consolidated.md` Sec. 15.6; replaces the inner iteration loop that
/// the microsecond-scale benchmarks use to reach steady state.
const WARMUP_SECS: f64 = 3.0;

/// Mean and `n-1` sample standard deviation, in milliseconds.
fn mean_sd_ms(runs: &[Duration]) -> (f64, f64) {
    let n = runs.len();
    assert!(n > 0, "statistics over an empty sample");
    let xs: Vec<f64> = runs.iter().map(|d| d.as_secs_f64() * 1e3).collect();
    let mean = xs.iter().sum::<f64>() / n as f64;
    let sd = if n > 1 {
        (xs.iter().map(|x| (x - mean) * (x - mean)).sum::<f64>() / (n - 1) as f64).sqrt()
    } else {
        0.0
    };
    (mean, sd)
}

fn row(label: &str, runs: &[Duration]) {
    let (m, s) = mean_sd_ms(runs);
    println!("  {label:<34} {:>11.3} {:>10.3}", m, s);
}

/// Run `f` repeatedly, discarding results, until `WARMUP_SECS` have elapsed.
/// Returns the number of discarded iterations so the log records the effort.
fn warm_up(mut f: impl FnMut()) -> usize {
    let t0 = Instant::now();
    let mut n = 0usize;
    while t0.elapsed().as_secs_f64() < WARMUP_SECS {
        f();
        n += 1;
    }
    n
}

fn main() {
    let mut runs = REPS;
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
        runs >= REPS,
        "benchmark validity requires >= {REPS} repetitions for a meaningful mean/SD, got {runs}"
    );
    let dir = positional.first().cloned().unwrap_or_else(|| "../../evm/test/vectors".to_string());

    let vv =
        VerifyVector::load(&dir).unwrap_or_else(|e| panic!("load golden vectors from {dir}: {e}"));

    // Correctness gate before any timing: the native oracle must reproduce the C
    // golden w', and the response must satisfy the base-verify norm bound. This
    // mirrors the "assert the contract on the very objects about to be timed"
    // rule of the other drivers -- no failure path is ever measured.
    relation::check_relation(&vv).expect("native relation oracle vs golden w'");
    let nz = norm_inf_vec(&vv.z);
    assert!(nz <= B, "golden z violates the norm bound: {nz} > {B}");

    let pub_inputs = RelationPublicInputs::from_vector(&vv);
    let n_pub = pub_inputs.to_elements().len();

    println!("las-stark benchmark of record");
    println!("  protocol              : {WARMUP_SECS:.0} s discarded warm-up, then {runs} timed repetitions,");
    println!("                          mean +/- sample (n-1) SD, one machine, one process");
    println!("  golden vector         : {dir}");
    println!("  measured ||z||inf     : {nz}  (bound B = {B})");
    println!("  witness z             : {} coefficients", vv.z.len() * D);
    println!();

    // ---------------------------------------------------------------------
    // A. NormAir -- constraint (1) alone.
    //    prove_norm = trace build + prover construction + prove.
    // ---------------------------------------------------------------------
    let w_norm = warm_up(|| {
        let p = prove_norm(&vv.z).expect("prove");
        black_box(verify_norm(p).is_ok());
    });

    let mut n_total = Vec::with_capacity(runs);
    let mut n_verify = Vec::with_capacity(runs);
    let mut n_sizes = Vec::with_capacity(runs);
    for _ in 0..runs {
        let t = Instant::now();
        let proof = prove_norm(&vv.z).expect("prove");
        n_total.push(t.elapsed());

        n_sizes.push(proof.to_bytes().len());

        let t = Instant::now();
        black_box(verify_norm(proof).expect("self-check"));
        n_verify.push(t.elapsed());
    }

    // ---------------------------------------------------------------------
    // B. RelationAir -- constraints (1) + (3) at d = 256.
    //    The TOTAL timer spans the same span prove_norm covers: witness build,
    //    trace construction, prover construction and prove().
    // ---------------------------------------------------------------------
    let w_rel = warm_up(|| {
        let wit = RelationWitness::build(&vv, &pub_inputs).expect("witness");
        let tr = build_main_trace(&wit);
        let pr = RelationProver::new(las_stark::proof_options(), pub_inputs.clone());
        let p = pr.prove(tr).expect("prove");
        let pi = pub_inputs.clone();
        black_box(verify_relation(p, pi).is_ok());
    });

    let mut r_total = Vec::with_capacity(runs);
    let mut r_witness = Vec::with_capacity(runs);
    let mut r_trace = Vec::with_capacity(runs);
    let mut r_setup = Vec::with_capacity(runs);
    let mut r_prove = Vec::with_capacity(runs);
    let mut r_verify = Vec::with_capacity(runs);
    let mut r_sizes = Vec::with_capacity(runs);
    for _ in 0..runs {
        let t_total = Instant::now();

        let t = Instant::now();
        let witness = RelationWitness::build(&vv, &pub_inputs).expect("quotient witness");
        r_witness.push(t.elapsed());

        let t = Instant::now();
        let trace = build_main_trace(&witness);
        r_trace.push(t.elapsed());

        let t = Instant::now();
        let prover = RelationProver::new(las_stark::proof_options(), pub_inputs.clone());
        r_setup.push(t.elapsed());

        let t = Instant::now();
        let proof = prover.prove(trace).expect("prove");
        r_prove.push(t.elapsed());

        r_total.push(t_total.elapsed());

        r_sizes.push(proof.to_bytes().len());

        // Clone the public inputs BEFORE the timer: verify_relation consumes them,
        // but a 176 KB copy is not part of verification.
        let pi = pub_inputs.clone();
        let t = Instant::now();
        black_box(verify_relation(proof, pi).expect("self-check"));
        r_verify.push(t.elapsed());
    }

    // ---------------------------------------------------------------------
    // report
    // ---------------------------------------------------------------------
    let fmt_size = |s: &[usize]| {
        let (a, b) = (*s.iter().min().unwrap(), *s.iter().max().unwrap());
        if a == b {
            format!("{a} bytes")
        } else {
            format!("{a}..{b} bytes")
        }
    };

    println!("A. NormAir -- constraint (1) only: ||z||inf <= B");
    println!("   trace   : {TRACE_LEN} rows x {TRACE_WIDTH} columns (single segment)");
    println!("   proof   : {}", fmt_size(&n_sizes));
    println!("   warm-up : {w_norm} discarded iterations");
    println!("  {:<34} {:>11} {:>10}   (n = {runs})", "phase", "mean (ms)", "SD (ms)");
    row("prove TOTAL (trace+setup+prove)", &n_total);
    row("verify", &n_verify);
    println!();

    println!("B. RelationAir -- constraints (1) + (3) at d = {D}, all n outputs, shared z");
    println!("   trace   : {EV_LEN} rows x {MAIN_WIDTH} main + {AUX_WIDTH} aux columns");
    println!("   public  : {n_pub} field elements (A', t, c, w')");
    println!("   proof   : {}", fmt_size(&r_sizes));
    println!("   warm-up : {w_rel} discarded iterations");
    println!("  {:<34} {:>11} {:>10}   (n = {runs})", "phase", "mean (ms)", "SD (ms)");
    row("prove TOTAL (the like-for-like span)", &r_total);
    row("  .. quotient witness build", &r_witness);
    row("  .. main trace build", &r_trace);
    row("  .. prover construction", &r_setup);
    row("  .. Winterfell prove()", &r_prove);
    row("verify", &r_verify);
    println!();

    // Controlled comparison: both measured above, same protocol, same process,
    // and both totals span the same set of steps.
    let (np, _) = mean_sd_ms(&n_total);
    let (rp, _) = mean_sd_ms(&r_total);
    let (nvf, _) = mean_sd_ms(&n_verify);
    let (rvf, _) = mean_sd_ms(&r_verify);
    let ns = n_sizes[0] as f64;
    let rs = r_sizes[0] as f64;
    println!("C. Cost of proving (1)+(3) rather than (1) alone  [same protocol, same process]");
    println!("   prove TOTAL : {:.2}x   ({:.1} ms -> {:.1} ms)", rp / np, np, rp);
    println!("   verify      : {:.2}x   ({:.2} ms -> {:.2} ms)", rvf / nvf, nvf, rvf);
    println!("   proof size  : {:.2}x   ({} B -> {} B)", rs / ns, n_sizes[0], r_sizes[0]);
    println!();
    println!("SCOPE: this proves the ARITHMETIC of base_verify only. Constraints (2)");
    println!("  SampleInBall and (4) the SHAKE256 challenge are NOT in the AIR: c and w'");
    println!("  are public inputs, so z is bound to (A', t, c, w') and NOT to (c_tilde, M).");
    println!("  'Winterfell prove()' is the whole prover -- trace low-degree extension,");
    println!("  Merkle commitments, constraint evaluation, DEEP composition AND FRI --");
    println!("  not FRI alone. No gas figure is claimed: none has been measured.");
}
