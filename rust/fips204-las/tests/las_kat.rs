//! Known-Answer Test for the Rust LAS port — mirrors `ref/test/test_kat.c`.
//!
//! Same fixed inputs (public-parameter seed, key/statement seeds, messages),
//! same deterministic functions, same serialisation, and the SAME pinned
//! SHAKE256 digest as the C build (`make test/test_kat3`, Simplified
//! Dilithium-III set n=6 ell=5 kappa=49).  A digest match proves the Rust
//! port reproduces the C implementation byte-for-byte across keygen, sign,
//! presign, adapt and packing.

use fips204::las::{
    las_adapt, las_ext, las_keygen_seed, las_presign_det, las_preverify, las_setup, las_sign_det,
    las_verify,
};
use fips204::las_serialize::{las_pack_pk, las_pack_sig, las_pack_sk};
use sha3::digest::{ExtendableOutput, Update, XofReader};
use sha3::Shake256;

const NVEC: u32 = 4;
const MLEN: usize = 33;

/// Pinned expected digest — copied verbatim from ref/test/test_kat.c EXPECTED.
const EXPECTED: [u8; 32] = [
    0x64, 0x1a, 0x17, 0x6c, 0x3e, 0xb2, 0x12, 0x50, 0x98, 0xfd, 0xbb, 0x7a, 0xd1, 0x6b, 0xfa,
    0x38, 0xfb, 0x57, 0x44, 0xb5, 0x2d, 0xd9, 0x69, 0x6b, 0xee, 0xb7, 0xd0, 0x7b, 0xe1, 0x44,
    0x5a, 0x19,
];

#[test]
fn las_kat_matches_c_pinned_digest() {
    let ppseed: [u8; 32] = core::array::from_fn(|i| i as u8); // fixed A
    let pp = las_setup(&ppseed);
    let mut acc = Shake256::default();

    for v in 0..NVEC {
        let kseed: [u8; 32] = core::array::from_fn(|i| (7 * v + i as u32 + 1) as u8);
        let yseed: [u8; 32] = core::array::from_fn(|i| (7 * v + i as u32 + 100) as u8);
        let msg: [u8; MLEN] = core::array::from_fn(|i| (37 * v + i as u32) as u8);

        // deterministic keygen / statement / sign / presign / adapt
        let (pk, sk) = las_keygen_seed(&pp, &kseed);
        let (y_stmt, y_wit) = las_keygen_seed(&pp, &yseed);
        let sig = las_sign_det(&msg, &pk, &sk, &pp);
        let presig = las_presign_det(&msg, &y_stmt, &pk, &sk, &pp);
        let adapted = las_adapt(&presig, &msg, &y_stmt, &y_wit, &pk, &pp).expect("adapt");

        // adaptor contract
        assert!(las_verify(&sig, &msg, &pk, &pp), "verify sig");
        assert!(las_verify(&adapted, &msg, &pk, &pp), "verify adapted");
        assert!(!las_verify(&presig, &msg, &pk, &pp), "presig must not verify");
        assert!(las_preverify(&presig, &msg, &y_stmt, &pk, &pp), "preverify");
        let yext = las_ext(&adapted, &presig, &y_stmt, &pp).expect("ext");
        assert!(yext == y_wit, "ext recovers witness");

        // determinism: re-running the seeded/deterministic functions is identical
        let (pk2, sk2) = las_keygen_seed(&pp, &kseed);
        assert!(pk == pk2 && sk == sk2, "keygen_seed deterministic");
        let sig2 = las_sign_det(&msg, &pk, &sk, &pp);
        assert!(sig == sig2, "sign_det deterministic");

        // serialise and fold into the running KAT digest (same order as C)
        let pk_b = las_pack_pk(&pk);
        let sk_b = las_pack_sk(&sk).expect("pack sk");
        let sig_b = las_pack_sig(&sig).expect("pack sig");
        let pre_b = las_pack_sig(&presig).expect("pack presig");
        let adp_b = las_pack_sig(&adapted).expect("pack adapted");
        acc.update(&pk_b);
        acc.update(&sk_b);
        acc.update(&sig_b);
        acc.update(&pre_b);
        acc.update(&adp_b);
        println!("  vector {v}: contract OK, deterministic, serialised");
    }

    let mut digest = [0u8; 32];
    acc.finalize_xof().read(&mut digest);

    let hex: String = digest.iter().map(|b| format!("{b:02x}")).collect();
    println!("  KAT digest (Rust): {hex}");

    assert_eq!(
        digest, EXPECTED,
        "KAT digest mismatch — Rust port does not reproduce the C implementation"
    );
    println!("=== KAT digest matches the C pinned expected value. ===");
}
