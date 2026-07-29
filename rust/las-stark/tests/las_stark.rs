//! End-to-end tests over the C golden verification vectors:
//!   * the native relation oracle reproduces the C golden `w'`;
//!   * the golden response `z` satisfies the base-verify norm bound;
//!   * a valid STARK proof round-trips (prove -> verify);
//!   * a tampered proof is rejected;
//!   * a bound other than `2B` is rejected by the AIR guard.
//!
//! Requires the golden vectors in `evm/test/vectors/` (regenerate with
//! `cd ref && make test/export_verify_vector && ./test/export_verify_vector ../evm/test/vectors`).

use las_stark::air::PublicInputs;
use las_stark::params::{B, TWO_B};
use las_stark::prover::{prove_norm, verify_norm, verify_with_pub_inputs};
use las_stark::relation;
use las_stark::relation_air::{
    prove_relation, verify_relation, RelationPublicInputs, RelationWitness,
};
use las_stark::vectors::{norm_inf_vec, VerifyVector};
use winterfell::math::fields::f64::BaseElement;
use winterfell::Proof;

const DIR: &str = "../../evm/test/vectors";

fn load() -> VerifyVector {
    VerifyVector::load(DIR).expect("golden vectors present (run export_verify_vector first)")
}

#[test]
fn oracle_matches_golden() {
    let vv = load();
    relation::check_relation(&vv).expect("native oracle must reproduce golden w'");
    assert!(norm_inf_vec(&vv.z) <= B, "||z||inf must be <= B for the adapted response");
}

#[test]
fn full_relation_native() {
    // The complete base_verify relation, byte-exact against the C golden vectors:
    // (2) SampleInBall(c_tilde) == c, (3) w' == z_top + A'*z_bot - c*t,
    // (4) SHAKE256(pack(t)||pack(w')||M) == c_tilde. This is the authoritative
    // native spec the Stage-A.2 AIR must match.
    let vv = load();
    relation::check_full_relation(&vv)
        .expect("full base_verify relation must match golden (SampleInBall + w' + SHAKE256 challenge)");
}

#[test]
fn stark_roundtrip() {
    let vv = load();
    let proof = prove_norm(&vv.z).expect("prove");
    assert!(!proof.to_bytes().is_empty(), "proof must be non-empty");
    verify_norm(proof).expect("a valid proof must verify");
}

#[test]
fn tampered_proof_rejected() {
    let vv = load();
    let proof = prove_norm(&vv.z).expect("prove");
    let mut bytes = proof.to_bytes();
    let mid = bytes.len() / 2;
    bytes[mid] ^= 0xFF; // flip a byte in the middle of the proof
    match Proof::from_bytes(&bytes) {
        Err(_) => {} // corruption caught at deserialization -> rejected
        Ok(p) => assert!(verify_norm(p).is_err(), "a tampered proof must not verify"),
    }
}

#[test]
#[should_panic] // guard #3: NormAir::new rejects any bound other than 2B
fn wrong_bound_rejected() {
    let vv = load();
    let proof = prove_norm(&vv.z).expect("prove");
    let bad = PublicInputs { two_b: BaseElement::new((TWO_B - 2) as u64) };
    let _ = verify_with_pub_inputs(proof, bad);
}

#[test]
fn conv_gadget_roundtrip() {
    // one negacyclic convolution p = a (X) b at the reduced degree CONV_D (see the
    // 255-column-cap note in conv_air): a public, b witness, p pinned to the output.
    let (a, b, out) = las_stark::conv_air::demo_instance();
    // the AIR fill must reproduce the native oracle output
    assert_eq!(las_stark::conv_air::negacyclic_conv(&a, &b), out, "native conv self-check");
    let proof = las_stark::conv_air::prove_conv(&a, &b, &out).expect("prove conv");
    assert!(!proof.to_bytes().is_empty());
    las_stark::conv_air::verify_conv(proof, &a, &out).expect("valid conv proof must verify");
}

#[test]
fn conv_gadget_wrong_output_rejected() {
    // tampering the claimed output must break verification
    let (a, b, mut out) = las_stark::conv_air::demo_instance();
    let good = las_stark::conv_air::prove_conv(&a, &b, &out).expect("prove conv");
    out[0] = (out[0] + 1) % las_stark::params::Q; // flip one output coefficient
    assert!(
        las_stark::conv_air::verify_conv(good, &a, &out).is_err(),
        "a proof must not verify against a tampered output"
    );
}

// --- the full arithmetic relation at the real degree d = 256 ---------------

#[test]
fn relation_witness_matches_golden() {
    // The quotient witness (h_m, g_m) exists and the q-division is EXACT only if
    // w' = z_top + A'*z_bot - c*t really holds over the golden vector, so this is
    // an independent check of the relation -- and it validates the bounds the
    // range checks enforce (|h| < 2^51, |g| < 2^29, ||z||inf <= B).
    let vv = load();
    let pi = RelationPublicInputs::from_vector(&vv);
    RelationWitness::build(&vv, &pi).expect("quotient witness must exist for a valid signature");
}

#[test]
fn relation_witness_rejects_tampered_commitment() {
    // Flipping one coefficient of w' breaks the divisibility by q, so no integer
    // quotient witness exists -- the prover cannot even build a trace.
    let mut vv = load();
    vv.w_prime[0][0] = (vv.w_prime[0][0] + 1) % las_stark::params::Q;
    let pi = RelationPublicInputs::from_vector(&vv);
    assert!(
        RelationWitness::build(&vv, &pi).is_err(),
        "a tampered w' must not admit a quotient witness"
    );
}

#[test]
fn relation_stark_roundtrip() {
    // A real end-to-end STARK for constraints (1) and (3) of base_verify at
    // d = 256, over all n output polynomials with ONE shared z.
    let vv = load();
    let (proof, pub_inputs) = prove_relation(&vv).expect("prove relation");
    assert!(!proof.to_bytes().is_empty(), "proof must be non-empty");
    verify_relation(proof, pub_inputs).expect("a valid relation proof must verify");
}

#[test]
fn relation_proof_rejects_tampered_public_inputs() {
    // The proof is bound to the public statement: verifying it against a w'
    // that differs in one coefficient must fail.
    let vv = load();
    let (proof, mut pub_inputs) = prove_relation(&vv).expect("prove relation");
    pub_inputs.w_prime[0][0] += 1;
    assert!(
        verify_relation(proof, pub_inputs).is_err(),
        "a relation proof must not verify against a tampered w'"
    );
}

#[test]
fn relation_tampered_proof_rejected() {
    let vv = load();
    let (proof, pub_inputs) = prove_relation(&vv).expect("prove relation");
    let mut bytes = proof.to_bytes();
    let mid = bytes.len() / 2;
    bytes[mid] ^= 0xFF;
    match Proof::from_bytes(&bytes) {
        Err(_) => {} // corruption caught at deserialization -> rejected
        Ok(p) => assert!(
            verify_relation(p, pub_inputs).is_err(),
            "a tampered relation proof must not verify"
        ),
    }
}
