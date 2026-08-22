//! `prove` -- read the golden verification vectors, sanity-check the native
//! relation oracle against the C golden `w'`, then produce a Blake3/FRI STARK
//! proof of the response norm bound and write it to disk. Prints proof size and
//! prove time (real measured numbers; nothing is fabricated).
//!
//! Run from the crate dir:  cargo run --release --bin prove [vectors_dir] [out]
//! Defaults: vectors_dir = ../../evm/test/vectors, out = proof.bin

use las_stark::params::B;
use las_stark::prover::prove_norm;
use las_stark::relation;
use las_stark::vectors::{norm_inf_vec, VerifyVector};
use std::time::Instant;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let dir = args.get(1).cloned().unwrap_or_else(|| "../../evm/test/vectors".to_string());
    let out = args.get(2).cloned().unwrap_or_else(|| "proof.bin".to_string());

    let vv = VerifyVector::load(&dir)
        .unwrap_or_else(|e| panic!("load golden vectors from {dir}: {e}"));

    // Sanity: the native oracle must reproduce the C golden w' before we prove.
    relation::check_relation(&vv).expect("native relation oracle vs golden w'");
    let nz = norm_inf_vec(&vv.z);
    assert!(nz <= B, "golden z violates the norm bound: {nz} > {B}");

    let start = Instant::now();
    let proof = prove_norm(&vv.z).expect("prove");
    let elapsed = start.elapsed();
    let bytes = proof.to_bytes();
    std::fs::write(&out, &bytes).unwrap_or_else(|e| panic!("write {out}: {e}"));

    println!("las-stark: proved ||z||inf <= B  (range gadget over {} coeffs)", vv.z.len() * 256);
    println!("  oracle w' vs C golden : OK");
    println!("  measured ||z||inf     : {nz}  (bound B = {B})");
    println!("  proof size            : {} bytes", bytes.len());
    println!("  prove time            : {elapsed:?}");
    println!("  written to            : {out}");
}
