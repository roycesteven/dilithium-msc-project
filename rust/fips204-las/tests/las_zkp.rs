//! Integration tests for the pi module (`relation_zk`, eprint 2020/845 §4.1),
//! mirroring `ref/test/test_zkp.c`:
//!
//!   1. completeness   : an honest Gen witness proves and verifies,
//!   2. tamper         : the proof is rejected after any sampled byte flip,
//!   3. wrong statement: the proof is rejected against a different Y.
//!
//! (The C test's fourth check — the prover refusing a non-ternary witness —
//! lives as a unit test inside `src/relation_zk.rs`: outside the crate a
//! non-ternary `Witness` is unconstructible by design, since the public codec
//! validates ternary.)  Opt-in: `cargo test --features relation-zk`.

#![cfg(feature = "relation-zk")]

use fips204::relation::gen_seed;
use fips204::relation_zk::{proof_verify, prove, PI_PROOF_MAX_BYTES};
use fips204::setup::setup_public_params;

#[test]
fn pi_prove_verify_tamper_wrong_statement() {
    let ppseed: [u8; 32] = core::array::from_fn(|i| i as u8);
    let pp = setup_public_params(&ppseed);
    let yseed: [u8; 32] = core::array::from_fn(|i| (i + 100) as u8);
    let y2seed: [u8; 32] = core::array::from_fn(|i| (i + 200) as u8);
    let (y, r_prime) = gen_seed(&pp, &yseed);
    let (y2, _) = gen_seed(&pp, &y2seed); // an unrelated second statement

    // 1. completeness
    let mut proof = [0u8; PI_PROOF_MAX_BYTES];
    let len = prove(&mut proof, &y, &r_prime, &pp).expect("prove on an honest Gen witness");
    assert!(proof_verify(&proof[..len], &y, &pp), "honest proof must verify");

    // 2. single-byte tamper across the proof (sampled stride to keep it fast)
    let mut pos = 0;
    while pos < len {
        proof[pos] ^= 1;
        assert!(
            !proof_verify(&proof[..len], &y, &pp),
            "tampered proof accepted (byte {pos})"
        );
        proof[pos] ^= 1;
        pos += 997;
    }

    // 3. the proof does not transfer to a different statement
    assert!(
        !proof_verify(&proof[..len], &y2, &pp),
        "proof for Y accepted against Y2"
    );

    // untampered proof still verifies after the negative tests
    assert!(proof_verify(&proof[..len], &y, &pp));
}
