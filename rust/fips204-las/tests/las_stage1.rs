//! Stage-1 integration tests for the Rust LAS port beyond the KAT:
//! cross-module interlock (independent base verifier vs the adaptor path,
//! mirroring `ref/test/test_basesig.c`'s equivalence checks) and validating
//! serialisation decoders (mirroring `ref/test/test_serde.c`'s round-trip and
//! tamper rejections, in reduced form — the exhaustive all-byte-flip sweep
//! remains a C-side artefact).

use fips204::las::{
    las_adapt, las_ext, las_keygen_seed, las_presign_det, las_preverify, las_setup, las_sign_det,
    las_verify,
};
use fips204::las_basesig::{base_keygen, base_sign, base_verify};
use fips204::las_serialize::{
    las_pack_pk, las_pack_sig, las_pack_sk, las_unpack_pk, las_unpack_sig, las_unpack_sk,
    las_verify_packed, LAS_SIG_BYTES,
};
use rand_chacha::rand_core::SeedableRng;
use rand_chacha::ChaCha8Rng;

const MSG: &[u8] = b"stage-1 interlock test message 33";

#[test]
fn cross_module_interlock() {
    let ppseed: [u8; 32] = core::array::from_fn(|i| i as u8);
    let pp = las_setup(&ppseed);
    let mut rng = ChaCha8Rng::seed_from_u64(1);

    let kseed: [u8; 32] = core::array::from_fn(|i| (i + 1) as u8);
    let yseed: [u8; 32] = core::array::from_fn(|i| (i + 100) as u8);
    let (pk, sk) = las_keygen_seed(&pp, &kseed);
    let (y_stmt, y_wit) = las_keygen_seed(&pp, &yseed);

    // 1. base sign/verify round-trip + wrong-message rejection (Algorithm 1).
    let (bpk, bsk) = base_keygen(&pp, &mut rng);
    let bsig = base_sign(MSG, &bpk, &bsk, &pp, &mut rng);
    assert!(base_verify(&bsig, MSG, &bpk, &pp), "base round-trip");
    assert!(
        !base_verify(&bsig, b"wrong message", &bpk, &pp),
        "base must reject wrong message"
    );

    // 2. cross-module equivalence: the two Algorithm-1 implementations accept
    //    each other's signatures (identical challenge hash, bit-for-bit).
    let lsig = las_sign_det(MSG, &pk, &sk, &pp);
    assert!(
        base_verify(&lsig, MSG, &pk, &pp),
        "independent base verifier must accept a las.rs ordinary signature"
    );
    assert!(
        las_verify(&bsig, MSG, &bpk, &pp),
        "las verifier must accept a basesig.rs signature"
    );

    // 3. the interlock headline: an Adapted LAS pre-signature passes the
    //    INDEPENDENT base verifier; the raw pre-signature must NOT.
    let presig = las_presign_det(MSG, &y_stmt, &pk, &sk, &pp);
    assert!(las_preverify(&presig, MSG, &y_stmt, &pk, &pp), "preverify");
    let adapted = las_adapt(&presig, MSG, &y_stmt, &y_wit, &pk, &pp).expect("adapt");
    assert!(
        base_verify(&adapted, MSG, &pk, &pp),
        "adapted signature must pass the independent base verifier"
    );
    assert!(
        !base_verify(&presig, MSG, &pk, &pp),
        "pre-signature must FAIL the base verifier (statement-binding tripwire)"
    );
    let yext = las_ext(&adapted, &presig, &y_stmt, &pp).expect("ext");
    assert!(yext == y_wit, "ext recovers the witness exactly");
}

#[test]
fn serde_round_trip_and_validation() {
    let ppseed: [u8; 32] = core::array::from_fn(|i| i as u8);
    let pp = las_setup(&ppseed);

    let kseed: [u8; 32] = core::array::from_fn(|i| (i + 7) as u8);
    let (pk, sk) = las_keygen_seed(&pp, &kseed);
    let sig = las_sign_det(MSG, &pk, &sk, &pp);

    // round-trips
    let pk_b = las_pack_pk(&pk);
    let sk_b = las_pack_sk(&sk).expect("pack sk");
    let sig_b = las_pack_sig(&sig).expect("pack sig");
    assert!(las_unpack_pk(&pk_b).expect("unpack pk") == pk, "pk round-trip");
    assert!(las_unpack_sk(&sk_b).expect("unpack sk") == sk, "sk round-trip");
    assert!(las_unpack_sig(&sig_b).expect("unpack sig") == sig, "sig round-trip");

    // validating decoders reject malformed encodings
    let mut bad_pk = pk_b;
    bad_pk[0] = 0xFF; // first 23-bit field -> 0x7FFFFF >= Q
    bad_pk[1] = 0xFF;
    bad_pk[2] |= 0x7F;
    assert!(las_unpack_pk(&bad_pk).is_none(), "pk coeff >= Q must be rejected");

    let mut bad_sk = sk_b;
    bad_sk[0] |= 0b11; // first 2-bit code -> 3 (invalid)
    assert!(las_unpack_sk(&bad_sk).is_none(), "sk code 3 must be rejected");

    let mut bad_sig_c = sig_b;
    bad_sig_c[0] |= 0b11; // first challenge code -> 3 (invalid)
    assert!(las_unpack_sig(&bad_sig_c).is_none(), "sig c code 3 must be rejected");

    let mut bad_sig_z = sig_b;
    bad_sig_z[64] = 0xFF; // first z field (19 bits, starts at bit 512) -> 0x7FFFF > LAS_Z_MAX
    bad_sig_z[65] = 0xFF;
    bad_sig_z[66] |= 0x07;
    assert!(las_unpack_sig(&bad_sig_z).is_none(), "out-of-band z must be rejected");

    // byte-interface verifier: accepts the valid pair, rejects tampered bytes
    assert!(las_verify_packed(&pk_b, &sig_b, MSG, &pp), "verify_packed valid");
    let mut tampered = sig_b;
    tampered[LAS_SIG_BYTES / 2] ^= 0x01; // flip one bit in the z region
    assert!(
        !las_verify_packed(&pk_b, &tampered, MSG, &pp),
        "verify_packed must reject a tampered signature byte"
    );
    let mut tampered_pk = pk_b;
    tampered_pk[10] ^= 0x01;
    assert!(
        !las_verify_packed(&tampered_pk, &sig_b, MSG, &pp),
        "verify_packed must reject a tampered public-key byte"
    );
}
