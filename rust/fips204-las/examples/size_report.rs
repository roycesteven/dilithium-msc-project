//! Communication-cost evidence (Rust side): component-level packed sizes.
//!
//! Supervisor requirements covered (las-context-consolidated.md):
//!  * §13.2 / §14.4 — do not just say "larger": break every object into its
//!    components (public key, secret key, statement Y, witness, challenge c,
//!    response z, signature, pre-signature, adapted signature), give the bytes
//!    of each, and name WHICH component drives the size (z, ~99%).
//!  * Sizes are MEASURED from live objects via the packers in
//!    `las_serialize.rs` (`.len()` of the produced byte arrays), not quoted
//!    from formulas. The c/z split inside the signature follows the packer's
//!    field layout (2-bit challenge region + 19-bit response region) and is
//!    asserted to sum to the measured total.
//!  * Cross-language check: every value is hard-asserted equal to the C
//!    evidence row (evidence/latest/tables/communication_components.csv,
//!    level L3 = Simplified Dilithium-III) — the wire format itself is
//!    byte-for-byte identical to C (covered by the pinned KAT digest).
//!
//! This is a deterministic measurement/report program, NOT a statistical
//! benchmark: packed sizes have zero variance, so Criterion is deliberately
//! not involved (see BENCHMARKING.md, "Communication cost").
//!
//! Run:  cargo run --release --example size_report

use fips204::las::{
    las_adapt, las_ext, las_keygen, las_presign, las_preverify, las_setup, LAS_M,
};
use fips204::las_basesig::{base_sign_signature, base_sign_verify};
use fips204::las_serialize::{
    las_pack_pk, las_pack_sig, las_pack_sk, LAS_C_COEFF_BITS, LAS_PK_BYTES, LAS_SIG_BYTES,
    LAS_SK_BYTES, LAS_Z_COEFF_BITS,
};
use rand_chacha::rand_core::SeedableRng;
use rand_chacha::ChaCha8Rng;

const MSG: &[u8] = b"bench message, thirty-three bytes";
const RING_DEGREE: usize = 256; // the paper's d (reused Dilithium NTT ring degree)

fn pct(part: usize, whole: usize) -> f64 {
    part as f64 * 100.0 / whole as f64
}

fn main() {
    // Same fixed-seed setup as benches/las_bench.rs (sizes are seed-invariant;
    // the fixed seed just keeps the whole evidence suite reproducible).
    let ppseed: [u8; 32] = core::array::from_fn(|i| i as u8);
    let pp = las_setup(&ppseed);
    let mut rng = ChaCha8Rng::seed_from_u64(0x4c41_5342_454e_4348);

    let (pk, sk) = las_keygen(&pp, &mut rng);
    let (y_stmt, y_wit) = las_keygen(&pp, &mut rng);
    let sig = base_sign_signature(MSG, &pk, &sk, &pp, &mut rng);
    let presig = las_presign(MSG, &y_stmt, &pk, &sk, &pp, &mut rng);
    let adapted = las_adapt(&presig, MSG, &y_stmt, &y_wit, &pk, &pp).expect("adapt");

    // Gate: only measure objects from a verified success-path state.
    assert!(base_sign_verify(&sig, MSG, &pk, &pp), "gate: ordinary signature verifies");
    assert!(las_preverify(&presig, MSG, &y_stmt, &pk, &pp), "gate: pre-signature pre-verifies");
    assert!(!base_sign_verify(&presig, MSG, &pk, &pp), "gate: pre-signature must FAIL ordinary Verify");
    assert!(base_sign_verify(&adapted, MSG, &pk, &pp), "gate: adapted signature verifies");
    assert!(
        las_ext(&adapted, &presig, &y_stmt, &pp).expect("gate: ext") == y_wit,
        "gate: ext recovers the witness exactly"
    );

    // MEASURED packed sizes (length of the actual byte arrays produced).
    let pk_len = las_pack_pk(&pk).len();
    let sk_len = las_pack_sk(&sk).expect("sk ternary").len();
    let y_len = las_pack_pk(&y_stmt).len(); // statement Y is pk-shaped (Y = A y)
    let wit_len = las_pack_sk(&y_wit).expect("witness ternary").len();
    let sig_len = las_pack_sig(&sig).expect("sig in range").len();
    let presig_len = las_pack_sig(&presig).expect("presig in range").len();
    let adapted_len = las_pack_sig(&adapted).expect("adapted in range").len();

    // Component split of the signature, from the packer's field layout.
    let c_len = RING_DEGREE * LAS_C_COEFF_BITS / 8;
    let z_len = LAS_M * RING_DEGREE * LAS_Z_COEFF_BITS / 8;
    assert!(c_len + z_len == sig_len, "field layout must sum to the measured signature size");

    // Measured values must equal the compile-time constants ...
    assert!(pk_len == LAS_PK_BYTES && sk_len == LAS_SK_BYTES && sig_len == LAS_SIG_BYTES);
    // ... and the adapted/pre-signature encodings must be size-identical.
    assert!(sig_len == presig_len && sig_len == adapted_len, "sig == pre-sig == adapted size");
    // ... and every value must match the C evidence row
    // (evidence/latest/tables/communication_components.csv, level L3).
    assert!(pk_len == 4416, "C row: pk = t = 4416 B");
    assert!(sk_len == 704, "C row: sk = r = 704 B");
    assert!(y_len == 4416, "C row: Y = t' = 4416 B");
    assert!(wit_len == 704, "C row: r' = 704 B");
    assert!(c_len == 64, "C row: c = 64 B");
    assert!(z_len == 6688, "C row: z = z_hat = 6688 B");
    assert!(sig_len == 6752, "C row: signature = pre-signature = adapted = 6752 B");

    println!("Communication cost — component-level packed sizes (measured)");
    println!("Setting: Simplified Dilithium-III (n=6, ell=5, kappa=49), ring degree d=256");
    println!("Wire format: las_serialize.rs (byte-for-byte identical to C ref/serialize.c;");
    println!("             cross-checked against the C pinned KAT digest 641a176c…5a19)");
    println!();
    println!("{:<42} {:>7}   {:>14}", "component", "bytes", "% of signature");
    let rows: [(&str, usize); 10] = [
        ("public key pk = t", pk_len),
        ("secret key sk = r", sk_len),
        ("statement Y = t' (adaptor lock)", y_len),
        ("witness r'", wit_len),
        ("challenge c", c_len),
        ("response z", z_len),
        ("response z_hat (in pre-signature)", z_len),
        ("signature (c, z)", sig_len),
        ("pre-signature (c, z_hat)", presig_len),
        ("adapted signature (c, z)", adapted_len),
    ];
    for (name, bytes) in rows {
        println!("{:<42} {:>7}   {:>13.2}%", name, bytes, pct(bytes, sig_len));
    }
    println!();
    println!("Finding: the response z drives the size ({z_len} of {sig_len} B = {:.2}%);", pct(z_len, sig_len));
    println!("the challenge c is negligible ({c_len} B = {:.2}%).", pct(c_len, sig_len));
    println!("Signature, pre-signature and adapted signature are byte-identical in size:");
    println!("Adapt only adds the ternary witness (|y|_inf <= 1) to z_hat, so |z|_inf grows");
    println!("by at most 1 and stays inside the same {LAS_Z_COEFF_BITS}-bit packed field.");
    println!("The only extra object the adaptor protocol communicates is the statement Y");
    println!("({y_len} B = one public key).");
    println!();
    println!("ALL SIZE CHECKS PASSED (constants, C evidence row L3, sig==presig==adapted)");
}
