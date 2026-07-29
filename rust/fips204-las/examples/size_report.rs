//! Communication-cost evidence (Rust side): component-level packed sizes.
//!
//! Supervisor requirements covered (las-context-consolidated.md):
//!  * §13.2 / §14.4 — do not just say "larger": break every object into its
//!    components (public key, secret key, statement Y, witness, challenge c,
//!    response z, signature, pre-signature, adapted signature), give the bytes
//!    of each, and name WHICH component drives the size (z, ~99%).
//!  * Sizes are MEASURED from live objects via the packers in
//!    `serialize.rs` (`.len()` of the produced byte arrays), not quoted
//!    from formulas. The c/z split inside the signature follows the packer's
//!    field layout (32-byte c_tilde digest + BitPack(z) response region) and is
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

use fips204::basesig::{keygen, sign, verify};
use fips204::las::{adapt, ext, presign, preverify};
use fips204::relation::gen;
use fips204::serialize::{
    pack_pre_signature, pack_public_key, pack_secret_key, pack_signature, pack_statement,
    pack_witness, unpack_signature, LAS_Z_COEFF_BITS, PUBLIC_KEY_BYTES, SECRET_KEY_BYTES,
    SIGNATURE_BYTES,
};
use fips204::setup::{setup_public_params, LAS_CTILDEBYTES, N_PLUS_ELL};
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
    let pp = setup_public_params(&ppseed);
    let mut rng = ChaCha8Rng::seed_from_u64(0x4c41_5342_454e_4348);

    let (pk, sk) = keygen(&pp, &mut rng);
    let (statement, witness) = gen(&pp, &mut rng);
    let sig = sign(MSG, &pk, &sk, &pp, &mut rng);
    let presig = presign(MSG, &statement, &pk, &sk, &pp, &mut rng);
    let adapted = adapt(&presig, MSG, &statement, &witness, &pk, &pp).expect("adapt");

    // Gate: only measure objects from a verified success-path state.
    assert!(verify(&sig, MSG, &pk, &pp), "gate: ordinary signature verifies");
    assert!(preverify(&presig, MSG, &statement, &pk, &pp), "gate: pre-signature pre-verifies");
    // Pre-signature is a distinct type from a signature, so the "must FAIL
    // ordinary Verify" tripwire runs at the BYTE level (decode its bytes AS a
    // signature, then verify — the on-chain relayer's view).
    let pre_b_gate = pack_pre_signature(&presig).expect("gate: presig packs");
    let pre_as_sig = unpack_signature(&pre_b_gate).expect("gate: presig bytes decode as a signature");
    assert!(!verify(&pre_as_sig, MSG, &pk, &pp), "gate: pre-signature must FAIL ordinary Verify");
    assert!(verify(&adapted, MSG, &pk, &pp), "gate: adapted signature verifies");
    assert!(
        ext(&adapted, &presig, &statement, &pp).expect("gate: ext") == witness,
        "gate: ext recovers the witness exactly"
    );

    // MEASURED packed sizes (length of the actual byte arrays produced).
    let pk_len = pack_public_key(&pk).len();
    let sk_len = pack_secret_key(&sk).expect("sk ternary").len();
    let y_len = pack_statement(&statement).len(); // statement Y is pk-shaped (Y = A y)
    let wit_len = pack_witness(&witness).expect("witness ternary").len();
    let sig_len = pack_signature(&sig).expect("sig in range").len();
    let presig_len = pack_pre_signature(&presig).expect("presig in range").len();
    let adapted_len = pack_signature(&adapted).expect("adapted in range").len();

    // Component split of the signature, from the packer's field layout.
    let c_len = LAS_CTILDEBYTES; // 32-byte challenge digest c_tilde (raw, unpacked)
    let z_len = N_PLUS_ELL * RING_DEGREE * LAS_Z_COEFF_BITS / 8;
    assert!(c_len + z_len == sig_len, "field layout must sum to the measured signature size");

    // Measured values must equal the compile-time constants ...
    assert!(pk_len == PUBLIC_KEY_BYTES && sk_len == SECRET_KEY_BYTES && sig_len == SIGNATURE_BYTES);
    // ... and the adapted/pre-signature encodings must be size-identical.
    assert!(sig_len == presig_len && sig_len == adapted_len, "sig == pre-sig == adapted size");
    // ... and every value must match the C evidence row
    // (evidence/latest/tables/communication_components.csv, level L3).
    assert!(pk_len == 4416, "C row: pk = t = 4416 B");
    assert!(sk_len == 704, "C row: sk = r = 704 B");
    assert!(y_len == 4416, "C row: Y = t' = 4416 B");
    assert!(wit_len == 704, "C row: r' = 704 B");
    assert!(c_len == 48, "C row: c_tilde = 48 B (FIPS 204 lambda/4, ML-DSA-65-aligned)");
    assert!(z_len == 6688, "C row: z = z_hat = 6688 B");
    assert!(sig_len == 6736, "C row: signature = pre-signature = adapted = 6736 B");

    println!("Communication cost — component-level packed sizes (measured)");
    println!("Setting: Simplified Dilithium-III (n=6, ell=5, kappa=49), ring degree d=256");
    println!("Wire format: serialize.rs (byte-for-byte identical to C ref/serialize.c;");
    println!("             cross-checked against the C pinned KAT digest b4a10ffb…03be)");
    println!();
    println!("{:<42} {:>7}   {:>14}", "component", "bytes", "% of signature");
    let rows: [(&str, usize); 10] = [
        ("public key pk = t", pk_len),
        ("secret key sk = r", sk_len),
        ("statement Y = t' (adaptor lock)", y_len),
        ("witness r'", wit_len),
        ("challenge c_tilde (H digest)", c_len),
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
    println!("the challenge c_tilde is negligible ({c_len} B = {:.2}%).", pct(c_len, sig_len));
    println!("Signature, pre-signature and adapted signature are byte-identical in size:");
    println!("Adapt only adds the ternary witness (|y|_inf <= 1) to z_hat, so |z|_inf grows");
    println!("by at most 1 and stays inside the same {LAS_Z_COEFF_BITS}-bit packed field.");
    println!("The only extra object the adaptor protocol communicates is the statement Y");
    println!("({y_len} B = one public key).");
    println!();
    println!("ALL SIZE CHECKS PASSED (constants, C evidence row L3, sig==presig==adapted)");
}
