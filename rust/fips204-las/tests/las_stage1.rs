//! Stage-1 integration tests for the Rust LAS port beyond the KAT:
//! the base signature in isolation, the adaptor interlock (an Adapted
//! pre-signature passes the base verifier; the raw pre-signature does not),
//! and validating serialisation decoders (mirroring `ref/test/test_serde.c`'s
//! round-trip and tamper rejections, in reduced form — the exhaustive
//! all-byte-flip sweep remains a C-side artefact).

use fips204::basesig::{keygen, keygen_seed, sign, sign_det, verify, verify_packed};
use fips204::las::{adapt, ext, presign_det, preverify};
use fips204::relation::gen_seed;
use fips204::serialize::{
    pack_pre_signature, pack_public_key, pack_secret_key, pack_signature, unpack_public_key,
    unpack_secret_key, unpack_signature, SIGNATURE_BYTES,
};
use fips204::setup::setup_public_params;
use rand_chacha::rand_core::SeedableRng;
use rand_chacha::ChaCha8Rng;

const MSG: &[u8] = b"stage-1 interlock test message 33";

#[test]
fn cross_module_interlock() {
    let ppseed: [u8; 32] = core::array::from_fn(|i| i as u8);
    let pp = setup_public_params(&ppseed);
    let mut rng = ChaCha8Rng::seed_from_u64(1);

    let kseed: [u8; 32] = core::array::from_fn(|i| (i + 1) as u8);
    let yseed: [u8; 32] = core::array::from_fn(|i| (i + 100) as u8);
    let (pk, sk) = keygen_seed(&pp, &kseed); // Algorithm-1 signing key
    let (statement, witness) = gen_seed(&pp, &yseed); // hard-relation (Y, y)

    // 1. base sign/verify round-trip + wrong-message rejection (Algorithm 1).
    let (bpk, bsk) = keygen(&pp, &mut rng);
    let bsig = sign(MSG, &bpk, &bsk, &pp, &mut rng);
    assert!(verify(&bsig, MSG, &bpk, &pp), "base round-trip");
    assert!(
        !verify(&bsig, b"wrong message", &bpk, &pp),
        "base must reject wrong message"
    );

    // 2. the interlock headline: an Adapted LAS pre-signature passes the base
    //    verifier; the raw pre-signature must NOT.  The pre-signature is a
    //    distinct type from a signature, so the negative tripwire is checked
    //    at the BYTE level (pack the pre-signature, decode it AS a signature,
    //    then run the base verifier — exactly what an on-chain relayer that
    //    only sees bytes would do).
    let sigma_hat = presign_det(MSG, &statement, &pk, &sk, &pp);
    assert!(preverify(&sigma_hat, MSG, &statement, &pk, &pp), "preverify");
    let adapted = adapt(&sigma_hat, MSG, &statement, &witness, &pk, &pp).expect("adapt");
    assert!(
        verify(&adapted, MSG, &pk, &pp),
        "adapted signature must pass the base verifier"
    );
    let pre_b = pack_pre_signature(&sigma_hat).expect("pack pre-signature");
    let pre_as_sig = unpack_signature(&pre_b).expect("pre-signature bytes decode as a signature");
    assert!(
        !verify(&pre_as_sig, MSG, &pk, &pp),
        "pre-signature (decoded as a signature) must FAIL the base verifier (statement-binding tripwire)"
    );
    let wext = ext(&adapted, &sigma_hat, &statement, &pp).expect("ext");
    assert!(wext == witness, "ext recovers the witness exactly");
}

#[test]
fn serde_round_trip_and_validation() {
    let ppseed: [u8; 32] = core::array::from_fn(|i| i as u8);
    let pp = setup_public_params(&ppseed);

    let kseed: [u8; 32] = core::array::from_fn(|i| (i + 7) as u8);
    let (pk, sk) = keygen_seed(&pp, &kseed);
    let sig = sign_det(MSG, &pk, &sk, &pp);

    // round-trips
    let pk_b = pack_public_key(&pk);
    let sk_b = pack_secret_key(&sk).expect("pack sk");
    let sig_b = pack_signature(&sig).expect("pack sig");
    assert!(unpack_public_key(&pk_b).expect("unpack pk") == pk, "pk round-trip");
    assert!(unpack_secret_key(&sk_b).expect("unpack sk") == sk, "sk round-trip");
    assert!(unpack_signature(&sig_b).expect("unpack sig") == sig, "sig round-trip");

    // validating decoders reject malformed encodings
    let mut bad_pk = pk_b;
    bad_pk[0] = 0xFF; // first 23-bit field -> 0x7FFFFF >= Q
    bad_pk[1] = 0xFF;
    bad_pk[2] |= 0x7F;
    assert!(unpack_public_key(&bad_pk).is_none(), "pk coeff >= Q must be rejected");

    let mut bad_sk = sk_b;
    bad_sk[0] |= 0b11; // first 2-bit code -> 3 (invalid)
    assert!(unpack_secret_key(&bad_sk).is_none(), "sk code 3 must be rejected");

    // The signature's challenge (c_tilde, 32 raw bytes) and response z (FIPS
    // BitUnpack) both decode permissively now, so tampering either is caught at
    // VERIFY, not decode -- upstream-faithful.  Challenge-region tamper:
    let mut bad_sig_c = sig_b;
    bad_sig_c[0] ^= 0x01; // flip a c_tilde byte
    assert!(unpack_signature(&bad_sig_c).is_some(), "any 32 bytes are a valid c_tilde digest");
    assert!(
        !verify_packed(&bad_sig_c, MSG, &pk_b, &pp),
        "a tampered c_tilde must fail verification"
    );
    // (z-region tamper -> verify rejection is exercised by the verify_packed
    // test below, which flips a byte at SIGNATURE_BYTES/2, inside the z region.)

    // byte-interface verifier: accepts the valid pair, rejects tampered bytes
    assert!(verify_packed(&sig_b, MSG, &pk_b, &pp), "verify_packed valid");
    let mut tampered = sig_b;
    tampered[SIGNATURE_BYTES / 2] ^= 0x01; // flip one bit in the z region
    assert!(
        !verify_packed(&tampered, MSG, &pk_b, &pp),
        "verify_packed must reject a tampered signature byte"
    );
    let mut tampered_pk = pk_b;
    tampered_pk[10] ^= 0x01;
    assert!(
        !verify_packed(&sig_b, MSG, &tampered_pk, &pp),
        "verify_packed must reject a tampered public-key byte"
    );
}
