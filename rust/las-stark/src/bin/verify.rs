//! `verify` -- read a STARK proof from disk and verify it against the canonical
//! bound `2B`. Prints verify time (real measured; nothing fabricated). Exits
//! non-zero on failure.
//!
//! Run from the crate dir:  cargo run --release --bin verify [proof_path]
//! Default: proof_path = proof.bin

use las_stark::prover::verify_norm;
use std::time::Instant;
use winterfell::Proof;

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let path = args.get(1).cloned().unwrap_or_else(|| "proof.bin".to_string());

    let bytes = std::fs::read(&path).unwrap_or_else(|e| panic!("read {path}: {e}"));
    let proof = Proof::from_bytes(&bytes).expect("parse proof");

    let start = Instant::now();
    let res = verify_norm(proof);
    let elapsed = start.elapsed();

    match res {
        Ok(()) => println!("las-stark: VERIFY OK  ({} proof bytes, {elapsed:?})", bytes.len()),
        Err(e) => {
            eprintln!("las-stark: VERIFY FAILED: {e}");
            std::process::exit(1);
        }
    }
}
