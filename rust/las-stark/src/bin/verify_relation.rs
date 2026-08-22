//! `verify_relation` -- read a relation STARK proof from disk and verify it
//! against the PUBLIC part of the golden verification vector (`A'`, `t`, `c`,
//! `w'`). Exits non-zero on failure.
//!
//! Verify time is reported as **mean +/- SD over `--runs` repetitions** (default
//! and floor `MIN_RUNS = 5`), matching the convention of the other benchmarks in
//! this project. Deserialization is done OUTSIDE the timed region so the figure
//! is the cost of verification itself. Real measured; nothing fabricated, and
//! nothing is written to `evidence/`.
//!
//! The public inputs are rebuilt from the same golden files the prover read, so
//! a proof only verifies against the statement it was made for -- pointing this
//! at a different vector directory is expected to FAIL.
//!
//! Run from the crate dir:
//!   cargo run --release --bin verify_relation -- [--runs N] [proof_path] [vectors_dir]
//! Defaults: runs = 5, proof_path = relation_proof.bin, vectors_dir = ../../evm/test/vectors

use las_stark::relation_air::{verify_relation, RelationPublicInputs};
use las_stark::vectors::VerifyVector;
use std::time::{Duration, Instant};
use winterfell::Proof;

/// Statistical floor: never report a mean over fewer runs than this.
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

fn main() {
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
    let path = positional.first().cloned().unwrap_or_else(|| "relation_proof.bin".to_string());
    let dir = positional.get(1).cloned().unwrap_or_else(|| "../../evm/test/vectors".to_string());

    let bytes = std::fs::read(&path).unwrap_or_else(|e| panic!("read {path}: {e}"));
    let vv =
        VerifyVector::load(&dir).unwrap_or_else(|e| panic!("load golden vectors from {dir}: {e}"));
    let pub_inputs = RelationPublicInputs::from_vector(&vv);

    let mut t_verify = Vec::with_capacity(runs);
    for _ in 0..runs {
        // Parse AND clone outside the timed region: this measures verification,
        // not decoding and not a 176 KB copy of the public inputs (which
        // verify_relation consumes by value).
        let proof = Proof::from_bytes(&bytes).expect("parse proof");
        let pi = pub_inputs.clone();
        let start = Instant::now();
        let res = verify_relation(proof, pi);
        t_verify.push(start.elapsed());
        if let Err(e) = res {
            eprintln!("las-stark: RELATION VERIFY FAILED: {e}");
            std::process::exit(1);
        }
    }

    let (m, s) = mean_sd(&t_verify);
    println!("las-stark: RELATION VERIFY OK  ({} proof bytes)", bytes.len());
    println!("  verify time : {:.3} ms +/- {:.3} ms   (n = {runs})", m / 1000.0, s / 1000.0);
    println!("  checked     : ||z||inf <= B  AND  w' = z_top + A'*z_bot - c*t  (mod q), all m < n");
    println!("  NOT checked : SampleInBall / SHAKE256 challenge -- c and w' are public inputs");
}
