//! Post-quantum atomic swap via LAS — eprint 2020/845 Section 4.1, Fig. 1,
//! implemented VERBATIM in the Rust port; twin of `ref/test/test_swap.c`
//! (message order, witness-holder role, the proof of knowledge pi, and the
//! abort conditions).
//!
//!   u1 = Alice: holds coin c1 on chain 1, generates (Y, y) and pi
//!   u2 = Bob  : holds coin c2 on chain 2
//!
//!   u1 -> u2 : { Y, pi, sigma_hat_1, tx1 }   (pre-signs her OWN coin first)
//!   u2 -> u1 : { sigma_hat_2, tx2 }          (only after pi and sigma_hat_1 verify)
//!   u1       : sigma_2 = Adapt((Y,y), sigma_hat_2); publish on chain 2 -> claims c2
//!   u2       : y' = Ext(Y, sigma_2, sigma_hat_2);
//!              sigma_1 = Adapt((Y,y'), sigma_hat_1); publish on chain 1 -> claims c1
//!
//! pi is load-bearing (Section 4.1): it proves the witness is TERNARY, so by
//! the M-SIS uniqueness argument the extracted y' equals y and u2's Adapt is
//! GUARANTEED to clear the Verify bound.  The test asserts y' == y — which,
//! y being an honest Gen witness, IS the `||y'||inf <= 1` guarantee.
//! Fig. 1 shows the happy path; timeout/refund is chain-level (C test_pcn).
//!
//! Opt-in: `cargo test --features relation-zk`.

#![cfg(feature = "relation-zk")]

use fips204::basesig::{keygen, verify};
use fips204::las::{adapt, ext, presign, preverify};
use fips204::relation::gen;
use fips204::relation_zk::{proof_verify, prove, PI_PROOF_MAX_BYTES};
use fips204::serialize::{pack_pre_signature, unpack_signature};
use fips204::setup::setup_public_params;
use rand_chacha::rand_core::SeedableRng;
use rand_chacha::ChaCha8Rng;

const TX1: &[u8] = b"tx1: Alice -> Bob, 10 coins on chain 1";
const TX2: &[u8] = b"tx2: Bob -> Alice, 10 coins on chain 2";

#[test]
fn fig1_atomic_swap_with_pi() {
    let ppseed: [u8; 32] = core::array::from_fn(|i| (i + 3) as u8);
    let pp = setup_public_params(&ppseed);
    let mut rng = ChaCha8Rng::seed_from_u64(42);

    // toy ledgers
    let (mut c1_alice, mut c1_bob) = (10, 0);
    let (mut c2_alice, mut c2_bob) = (0, 10);

    // Setup: both key pairs (Algorithm 1 KeyGen)
    let (pk1, sk1) = keygen(&pp, &mut rng);
    let (pk2, sk2) = keygen(&pp, &mut rng);

    // ---- u1 (Alice): (Y, y) <- Gen(); pi <- P((t'; r'), ...); PreSign tx1 ----
    let (y_stmt, y_wit) = gen(&pp, &mut rng);
    let mut pi = [0u8; PI_PROOF_MAX_BYTES];
    let pilen = prove(&mut pi, &y_stmt, &y_wit, &pp).expect("Alice's relation_zk::prove");
    let presig1 = presign(TX1, &y_stmt, &pk1, &sk1, &pp, &mut rng);
    // message 1: u1 -> u2 : { Y, pi, sigma_hat_1, tx1 }   (off-chain)

    // ---- u2 (Bob): "If verif. of pi or sigma_hat_1 fails, Abort" (Fig. 1) ----
    assert!(proof_verify(&pi[..pilen], &y_stmt, &pp), "Bob's verification of pi");
    assert!(preverify(&presig1, TX1, &y_stmt, &pk1, &pp), "Bob's PreVerify of sigma_hat_1");

    // a proof must not transfer to a statement it was not made for
    let (y_evil, _) = gen(&pp, &mut rng);
    assert!(
        !proof_verify(&pi[..pilen], &y_evil, &pp),
        "pi for Y wrongly accepted for a different statement"
    );

    // ---- u2 (Bob): PreSign tx2 under the SAME Y ----
    let presig2 = presign(TX2, &y_stmt, &pk2, &sk2, &pp, &mut rng);
    assert!(preverify(&presig2, TX2, &y_stmt, &pk2, &pp), "Alice's PreVerify of sigma_hat_2");
    // message 2: u2 -> u1 : { sigma_hat_2, tx2 }   (off-chain)

    // Neither pre-signature is spendable on its own: an adversary submitting
    // the raw pre-signature bytes as a signature is rejected by the ordinary
    // verifier (the byte route is the only one — the type system already
    // forbids passing a PreSignature to basesig::verify).
    let bytes1 = pack_pre_signature(&presig1).expect("pack sigma_hat_1");
    let forged1 = unpack_signature(&bytes1).expect("presig bytes decode as sig-shaped");
    assert!(!verify(&forged1, TX1, &pk1, &pp), "sigma_hat_1 bytes wrongly spendable");
    let bytes2 = pack_pre_signature(&presig2).expect("pack sigma_hat_2");
    let forged2 = unpack_signature(&bytes2).expect("presig bytes decode as sig-shaped");
    assert!(!verify(&forged2, TX2, &pk2, &pp), "sigma_hat_2 bytes wrongly spendable");

    // ---- u1 (Alice): sigma_2 = Adapt((Y,y), sigma_hat_2); abort on bottom;
    // publish on chain 2 => Alice claims c2 ----
    let sigma2 = adapt(&presig2, TX2, &y_stmt, &y_wit, &pk2, &pp).expect("Alice's Adapt");
    assert!(verify(&sigma2, TX2, &pk2, &pp), "published sigma_2 must pass ordinary Verify");
    c2_bob -= 10;
    c2_alice += 10;

    // ---- u2 (Bob): y' = Ext(Y, sigma_2, sigma_hat_2) from PUBLIC chain-2
    // data; Adapt sigma_hat_1; publish on chain 1 => Bob claims c1 ----
    let y_ext = ext(&sigma2, &presig2, &y_stmt, &pp).expect("Bob's Ext (A*y' != Y)");
    assert!(
        y_ext == y_wit,
        "extracted y' != y (the M-SIS uniqueness guarantee pi bought, §4.1)"
    );
    let sigma1 = adapt(&presig1, TX1, &y_stmt, &y_ext, &pk1, &pp)
        .expect("Bob's Adapt with the extracted witness");
    assert!(verify(&sigma1, TX1, &pk1, &pp), "published sigma_1 must pass ordinary Verify");
    c1_alice -= 10;
    c1_bob += 10;

    // ---- atomicity ----
    assert_eq!((c1_alice, c1_bob, c2_alice, c2_bob), (0, 10, 10, 0));
}
