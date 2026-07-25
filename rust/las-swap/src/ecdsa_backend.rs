//! Configuration 1: the classical ECDSA adaptor signature, over Blockstream's
//! `secp256k1-zkp` crate — the official Rust bindings for `libsecp256k1-zkp`,
//! including its `ecdsa_adaptor` module.
//!
//! The crate is used rather than hand-written FFI so the classical baseline runs
//! maintained, upstream-audited binding code. It is the same primitive and the
//! same C implementation as the Stage-1 classical baseline
//! (`ref/test/bench_classical.c`), which links the vendored
//! `third_party/secp256k1-zkp` directly from C.
//!
//! **Provenance note for the report.** The C baseline links the repo's vendored
//! clone (commit `95b9835`); this crate builds its own bundled copy of
//! `libsecp256k1-zkp` via `secp256k1-zkp-sys`. Same library, independently
//! pinned versions — so state which implementation produced which number rather
//! than presenting the C and Rust classical figures as one series.
//!
//! # Operation correspondence
//!
//! | LAS | secp256k1-zkp |
//! |---|---|
//! | `KeyGen` | `PublicKey::from_secret_key` |
//! | `Gen()` | a second key pair — the statement *is* an encryption key |
//! | `PreSign` | `EcdsaAdaptorSignature::encrypt_no_aux_rand` |
//! | `PreVerify` | `EcdsaAdaptorSignature::verify` |
//! | `Adapt` | `EcdsaAdaptorSignature::decrypt` |
//! | `Ext` | `EcdsaAdaptorSignature::recover` |
//! | `Verify` | `Secp256k1::verify_ecdsa` |
//!
//! # Two honest caveats for the report
//!
//! * libsecp256k1 is constant-time, heavily optimised production code; the LAS
//!   side is a reference-style simplified scheme. The comparison **flatters the
//!   classical side**, and must be reported that way.
//! * ECDSA signs a 32-byte digest by construction, so this backend hashes the
//!   transaction sighash to 32 bytes with SHAKE256 before signing. LAS absorbs
//!   the message directly. Both commit to the same transaction; the extra
//!   compression is intrinsic to ECDSA, not an artefact of the harness.

use std::time::Duration;
#[cfg(feature = "secp256k1")]
use std::time::Instant;

#[cfg(feature = "secp256k1")]
use crate::backend::subseed;
use crate::backend::{AdaptorScheme, Availability, Relation, RelationBinding, SwapInputs};

/// Serialised sizes of the classical objects.
const ECDSA_PK_BYTES: usize = 33; // compressed point
const ECDSA_SK_BYTES: usize = 32;
const ECDSA_SIG_BYTES: usize = 64; // compact (r, s)
const ECDSA_PRESIG_BYTES: usize = 162; // adaptor pre-signature (opaque)

/// ECDSA commits to a 32-byte digest; compress the transaction sighash first.
#[cfg(feature = "secp256k1")]
fn msg32(msg: &[u8]) -> [u8; 32] {
    use sha3::digest::{ExtendableOutput, Update, XofReader};
    let mut h = sha3::Shake256::default();
    h.update(b"las-swap/ecdsa-sighash/v1");
    h.update(msg);
    let mut out = [0u8; 32];
    h.finalize_xof().read(&mut out);
    out
}

/// The classical ECDSA adaptor signature.
pub struct EcdsaAdaptor {
    #[cfg(feature = "secp256k1")]
    secp: secp256k1_zkp::Secp256k1<secp256k1_zkp::All>,
    /// Measured cost of the one-time context construction (precomputed tables).
    setup: Duration,
}

impl EcdsaAdaptor {
    pub fn new() -> Self {
        #[cfg(feature = "secp256k1")]
        {
            // Building the context computes the library's precomputation tables.
            // A genuine one-time cost, so it is measured rather than assumed
            // zero — and it happens here, in the constructor that
            // `configurations()` runs before any timing begins.
            let t0 = Instant::now();
            let secp = secp256k1_zkp::Secp256k1::new();
            let setup = t0.elapsed();
            Self { secp, setup }
        }
        #[cfg(not(feature = "secp256k1"))]
        {
            Self { setup: Duration::ZERO }
        }
    }
}

impl Default for EcdsaAdaptor {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(feature = "secp256k1")]
impl EcdsaAdaptor {
    /// Derive a valid secret key from a seed, re-deriving on the negligible
    /// chance a candidate lies outside `[1, n)`.
    fn seckey_from(&self, seed: &[u8; 32]) -> Option<secp256k1_zkp::SecretKey> {
        let mut candidate = *seed;
        for attempt in 0u32..64 {
            if let Ok(sk) = secp256k1_zkp::SecretKey::from_slice(&candidate) {
                return Some(sk);
            }
            candidate = subseed(seed, "seckey-retry", attempt);
        }
        None
    }
}

impl AdaptorScheme for EcdsaAdaptor {
    fn name(&self) -> &'static str {
        "ECDSA adaptor signature (libsecp256k1-zkp)"
    }

    fn param_note(&self) -> &'static str {
        "secp256k1, 256-bit group, ~128-bit classical security (no post-quantum security)"
    }

    fn availability(&self) -> Availability {
        #[cfg(feature = "secp256k1")]
        {
            Availability::Ready
        }
        #[cfg(not(feature = "secp256k1"))]
        {
            Availability::Missing(
                "ECDSA backend not linked: rebuild with `--features secp256k1` \
                 (pulls the secp256k1-zkp crate, which builds libsecp256k1-zkp itself)",
            )
        }
    }

    fn post_quantum(&self) -> bool {
        false
    }

    fn relation(&self) -> Relation {
        Relation::Secp256k1DiscreteLog
    }

    fn binding(&self) -> RelationBinding {
        // Curve and generator are fixed by the standard, so the relation has no
        // per-instance parameters to bind.
        RelationBinding::fixed(b"secp256k1")
    }

    fn pk_bytes(&self) -> usize {
        ECDSA_PK_BYTES
    }
    fn sk_bytes(&self) -> usize {
        ECDSA_SK_BYTES
    }
    fn statement_bytes(&self) -> usize {
        // The statement is an encryption *public key* — same shape as a pk.
        ECDSA_PK_BYTES
    }
    fn witness_bytes(&self) -> usize {
        ECDSA_SK_BYTES
    }
    fn signature_bytes(&self) -> usize {
        ECDSA_SIG_BYTES
    }
    fn pre_signature_bytes(&self) -> usize {
        ECDSA_PRESIG_BYTES
    }

    // `presignature_overhead_bytes` uses the trait default: 162 − 64 = 98 bytes.
    // That excess carries the adaptor's nonce commitments together with its DLEQ
    // proof. The upstream header documents the pre-signature only as an opaque
    // 162-byte object, so the harness reports the overhead and does not invent a
    // split between "proof" and "structure".

    fn setup_duration(&self) -> Duration {
        self.setup
    }

    /// ECDSA has no Fiat–Shamir-with-aborts loop: signing succeeds on the first
    /// attempt, so there is no rejection rate to gate. Stated as `None` rather
    /// than a rate of 1.0, which would imply a loop that does not exist.
    fn expected_presign_attempts(&self) -> Option<f64> {
        None
    }

    fn presign_attempts_counter(&self) -> Option<u64> {
        None
    }

    #[allow(unused_variables)]
    fn derive_inputs(&self, master_seed: &[u8; 32], iteration: u32) -> Option<SwapInputs> {
        #[cfg(feature = "secp256k1")]
        {
            use secp256k1_zkp::PublicKey;

            let sk1 = self.seckey_from(&subseed(master_seed, "ecdsa-key-u1", iteration))?;
            let sk2 = self.seckey_from(&subseed(master_seed, "ecdsa-key-u2", iteration))?;
            let witness = self.seckey_from(&subseed(master_seed, "ecdsa-statement", iteration))?;

            Some(SwapInputs {
                pk1: PublicKey::from_secret_key(&self.secp, &sk1).serialize().to_vec(),
                sk1: sk1.secret_bytes().to_vec(),
                pk2: PublicKey::from_secret_key(&self.secp, &sk2).serialize().to_vec(),
                sk2: sk2.secret_bytes().to_vec(),
                // The statement Y is the public key of the witness — exactly the
                // "statement is another key pair" structure LAS also has.
                statement: PublicKey::from_secret_key(&self.secp, &witness).serialize().to_vec(),
                witness: witness.secret_bytes().to_vec(),
            })
        }
        #[cfg(not(feature = "secp256k1"))]
        {
            None
        }
    }

    #[allow(unused_variables)]
    fn presign(&self, msg: &[u8], statement: &[u8], pk: &[u8], sk: &[u8]) -> Option<Vec<u8>> {
        #[cfg(feature = "secp256k1")]
        {
            use secp256k1_zkp::{EcdsaAdaptorSignature, Message, PublicKey, SecretKey};

            let enckey = PublicKey::from_slice(statement).ok()?;
            let sk = SecretKey::from_slice(sk).ok()?;
            let m = Message::from_digest(msg32(msg));
            // `encrypt_no_aux_rand` is the deterministic variant, so a run is
            // reproducible from the pinned master seed.
            let presig = EcdsaAdaptorSignature::encrypt_no_aux_rand(&self.secp, &m, &sk, &enckey);
            Some(presig.as_ref().to_vec())
        }
        #[cfg(not(feature = "secp256k1"))]
        {
            None
        }
    }

    #[allow(unused_variables)]
    fn preverify(&self, presig: &[u8], msg: &[u8], statement: &[u8], pk: &[u8]) -> bool {
        #[cfg(feature = "secp256k1")]
        {
            use secp256k1_zkp::{EcdsaAdaptorSignature, Message, PublicKey};

            let (Ok(presig), Ok(enckey), Ok(pubkey)) = (
                EcdsaAdaptorSignature::from_slice(presig),
                PublicKey::from_slice(statement),
                PublicKey::from_slice(pk),
            ) else {
                return false;
            };
            let m = Message::from_digest(msg32(msg));
            // Also checks the construction's embedded DLEQ.
            presig.verify(&self.secp, &m, &pubkey, &enckey).is_ok()
        }
        #[cfg(not(feature = "secp256k1"))]
        {
            false
        }
    }

    #[allow(unused_variables)]
    fn adapt(
        &self,
        presig: &[u8],
        msg: &[u8],
        statement: &[u8],
        witness: &[u8],
        pk: &[u8],
    ) -> Option<Vec<u8>> {
        #[cfg(feature = "secp256k1")]
        {
            use secp256k1_zkp::{EcdsaAdaptorSignature, SecretKey};

            // Fig. 1 has Adapt return ⊥ on a bad pre-signature. `decrypt` would
            // happily transform one, so check first and keep the paper's
            // contract identical across configurations.
            if !self.preverify(presig, msg, statement, pk) {
                return None;
            }
            let presig = EcdsaAdaptorSignature::from_slice(presig).ok()?;
            let witness = SecretKey::from_slice(witness).ok()?;
            let sig = presig.decrypt(&witness).ok()?;
            Some(sig.serialize_compact().to_vec())
        }
        #[cfg(not(feature = "secp256k1"))]
        {
            None
        }
    }

    #[allow(unused_variables)]
    fn ext(&self, sig: &[u8], presig: &[u8], statement: &[u8]) -> Option<Vec<u8>> {
        #[cfg(feature = "secp256k1")]
        {
            use secp256k1_zkp::{ecdsa::Signature, EcdsaAdaptorSignature, PublicKey};

            let presig = EcdsaAdaptorSignature::from_slice(presig).ok()?;
            let sig = Signature::from_compact(sig).ok()?;
            let enckey = PublicKey::from_slice(statement).ok()?;
            let deckey = presig.recover(&self.secp, &sig, &enckey).ok()?;
            Some(deckey.secret_bytes().to_vec())
        }
        #[cfg(not(feature = "secp256k1"))]
        {
            None
        }
    }

    #[allow(unused_variables)]
    fn verify(&self, sig: &[u8], msg: &[u8], pk: &[u8]) -> bool {
        #[cfg(feature = "secp256k1")]
        {
            use secp256k1_zkp::{ecdsa::Signature, Message, PublicKey};

            let (Ok(sig), Ok(pubkey)) = (Signature::from_compact(sig), PublicKey::from_slice(pk))
            else {
                return false;
            };
            let m = Message::from_digest(msg32(msg));
            self.secp.verify_ecdsa(&m, &sig, &pubkey).is_ok()
        }
        #[cfg(not(feature = "secp256k1"))]
        {
            false
        }
    }
}
