//! Known-Answer Test for the Rust LAS port — mirrors `ref/test/test_kat.c`.
//!
//! Same fixed inputs (public-parameter seed, key/statement seeds, messages),
//! same deterministic functions, same serialisation, and the SAME pinned
//! SHAKE256 digest as the C build (`make test/test_kat3`, Simplified
//! Dilithium-III set n=6 ell=5 kappa=49).  A digest match proves the Rust
//! port reproduces the C implementation byte-for-byte across keygen, sign,
//! presign, adapt and packing.

use fips204::basesig::{keygen_seed, sign_det, verify};
use fips204::las::{adapt, ext, presign_det, preverify};
use fips204::relation::gen_seed;
use fips204::serialize::{
    pack_pre_signature, pack_public_key, pack_secret_key, pack_signature, unpack_signature,
};
use fips204::setup::setup_public_params;
use sha3::digest::{ExtendableOutput, Update, XofReader};
use sha3::Shake256;

const NVEC: u32 = 4;
const MLEN: usize = 33;

/// Pinned expected digest — copied verbatim from ref/test/test_kat.c EXPECTED.
///
/// Regenerated 2026-07-29 when `c_tilde` moved from a flat 32 bytes to the
/// FIPS 204 `lambda/4` width (48 B for this ML-DSA-65-aligned set), which grew
/// the signature from 6720 to 6736 B. The previous value was `bb6ad0da…260c`;
/// it is deliberately NOT preserved. C and Rust reached this value
/// independently, which is what makes it a cross-language gate.
const EXPECTED: [u8; 32] = [
    0xb4, 0xa1, 0x0f, 0xfb, 0x6e, 0x64, 0x5e, 0x50, 0x76, 0xd1, 0xff, 0x59, 0x93, 0xfa, 0xa7,
    0x29, 0x09, 0x23, 0x2f, 0xc7, 0x1e, 0x55, 0x4b, 0x93, 0x54, 0x41, 0x41, 0xd6, 0x59, 0x05,
    0x03, 0xbe,
];

#[test]
fn las_kat_matches_c_pinned_digest() {
    let ppseed: [u8; 32] = core::array::from_fn(|i| i as u8); // fixed A
    let pp = setup_public_params(&ppseed);
    let mut acc = Shake256::default();

    for v in 0..NVEC {
        let kseed: [u8; 32] = core::array::from_fn(|i| (7 * v + i as u32 + 1) as u8);
        let yseed: [u8; 32] = core::array::from_fn(|i| (7 * v + i as u32 + 100) as u8);
        let msg: [u8; MLEN] = core::array::from_fn(|i| (37 * v + i as u32) as u8);

        // deterministic keygen (Algorithm 1) / statement-witness (Gen) / sign /
        // presign / adapt
        let (pk, sk) = keygen_seed(&pp, &kseed);
        let (statement, witness) = gen_seed(&pp, &yseed);
        let sig = sign_det(&msg, &pk, &sk, &pp);
        let presig = presign_det(&msg, &statement, &pk, &sk, &pp);
        let adapted = adapt(&presig, &msg, &statement, &witness, &pk, &pp).expect("adapt");

        // adaptor contract
        assert!(verify(&sig, &msg, &pk, &pp), "verify sig");
        assert!(verify(&adapted, &msg, &pk, &pp), "verify adapted");
        assert!(preverify(&presig, &msg, &statement, &pk, &pp), "preverify");
        let wext = ext(&adapted, &presig, &statement, &pp).expect("ext");
        assert!(wext == witness, "ext recovers witness");

        // determinism: re-running the seeded/deterministic functions is identical
        let (pk2, sk2) = keygen_seed(&pp, &kseed);
        assert!(pk == pk2 && sk == sk2, "keygen_seed deterministic");
        let sig2 = sign_det(&msg, &pk, &sk, &pp);
        assert!(sig == sig2, "sign_det deterministic");

        // serialise and fold into the running KAT digest (same order as C)
        let pk_b = pack_public_key(&pk);
        let sk_b = pack_secret_key(&sk).expect("pack sk");
        let sig_b = pack_signature(&sig).expect("pack sig");
        let pre_b = pack_pre_signature(&presig).expect("pack presig");
        let adp_b = pack_signature(&adapted).expect("pack adapted");

        // negative tripwire (byte level): the pre-signature is a distinct type
        // from a signature, so decode its bytes AS an ordinary signature and
        // confirm the base verifier rejects it — the statement Y is folded into
        // the pre-signature's challenge, so it cannot pass ordinary Verify.
        let pre_as_sig = unpack_signature(&pre_b).expect("presig bytes decode as a signature");
        assert!(
            !verify(&pre_as_sig, &msg, &pk, &pp),
            "presig must not verify as an ordinary signature"
        );

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
