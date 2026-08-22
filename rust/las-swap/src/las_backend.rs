//! The post-quantum half of the Stage-2 configurations: the LAS adaptor
//! signature (eprint 2020/845 Algorithm 2), and the two role-A provers for its
//! relation `∃r : A r = t ∧ ‖r‖∞ ≤ 1`.
//!
//! * [`Las`] — the signature scheme, configurations 2 and 3.
//! * [`Groth16`] — configuration 2's prover.
//! * [`Lazer`] — configuration 3's prover, over the vendored LaZer library.
//!
//! Both provers must establish the **same** claim, including the norm bound
//! `‖r‖∞ ≤ 1`. Dropping the bound and proving only `A r = t` would be a
//! different, weaker statement that does not close the knowledge gap Section 4.1
//! relies on, and the two configurations would no longer be comparable.
//!
//! Everything routes through `fips204` (`rust/fips204-las`), the KAT-locked LAS
//! port. Nothing here reimplements scheme logic — this module is an adapter from
//! that crate's typed API onto the byte-oriented [`AdaptorScheme`] interface.

use std::rc::Rc;
use std::sync::atomic::Ordering;
use std::time::Duration;

use fips204::basesig;
use fips204::las;
use fips204::relation;
use fips204::serialize::{
    pack_pre_signature, pack_public_key, pack_secret_key, pack_signature, pack_statement,
    pack_witness, unpack_pre_signature, unpack_public_key, unpack_secret_key, unpack_signature,
    unpack_statement, unpack_witness, PRE_SIGNATURE_BYTES, PUBLIC_KEY_BYTES, SECRET_KEY_BYTES,
    SIGNATURE_BYTES, STATEMENT_BYTES, WITNESS_BYTES,
};
use fips204::setup::{setup_public_params, PublicParams, ELL, GAMMA, KAPPA, N, N_PLUS_ELL};

use crate::backend::{
    subseed, AdaptorScheme, Availability, Relation, RelationBinding, SetupKind, SwapInputs,
    ZkpBackend,
};

/// How many independent probe statements enter the relation binding. Each probe
/// contributes `n·d` coefficients mod `q` that are a function of `A`; several of
/// them together pin the matrix down far beyond any accidental agreement.
const BINDING_PROBES: u32 = 4;

/// 32-byte SHAKE256 helper.
fn shake256_32(parts: &[&[u8]]) -> [u8; 32] {
    use sha3::digest::{ExtendableOutput, Update, XofReader};
    let mut h = sha3::Shake256::default();
    for p in parts {
        h.update(p);
    }
    let mut out = [0u8; 32];
    h.finalize_xof().read(&mut out);
    out
}

/// The LAS public parameters `A = [I | A']`, expanded once and shared by every
/// backend that speaks about the same relation instance.
///
/// Constructed once by `configurations()` **before any timing begins**:
/// expanding `A` is a one-time cost amortised over every swap, and must not land
/// inside a per-operation measurement.
pub struct LasParams {
    pp: PublicParams,
    binding: RelationBinding,
}

impl LasParams {
    /// Expand `A` from `seed` and compute the relation binding.
    ///
    /// # What the binding covers, and why it has two halves
    ///
    /// `binding = SHAKE256(domain ‖ seed ‖ t'_0 ‖ … ‖ t'_{k-1})` where each
    /// `t'_i = A r'_i` is produced by `relation::gen_seed` under *this* `A`.
    ///
    /// * The **seed** is the identity of the parameter set: `A` is expanded from
    ///   it deterministically, so equal seeds mean equal `A` by construction.
    ///   On its own, though, it would only certify that two backends were
    ///   *asked* for the same parameters — not that either is computing with
    ///   what it was given.
    /// * The **probe statements** are computed by running the scheme's own code
    ///   against the expanded matrix, so they certify the instance in use. A
    ///   single probe would leave `A` under-determined (one probe fixes `n·d`
    ///   coefficients while `A'` has `n·ℓ·d`), so [`BINDING_PROBES`] independent
    ///   probes are used.
    ///
    /// Together they answer both questions: *which* parameter set, and *is this
    /// instance really using it*.
    pub fn setup(seed: &[u8; 32]) -> Self {
        let pp = setup_public_params(seed);

        let mut probes: Vec<u8> = Vec::new();
        for i in 0..BINDING_PROBES {
            let probe_seed = shake256_32(&[b"las-swap/binding-probe/v1", &i.to_le_bytes()]);
            let (t_prime, _r_prime) = relation::gen_seed(&pp, &probe_seed);
            probes.extend_from_slice(&pack_statement(&t_prime));
        }

        let binding =
            RelationBinding(shake256_32(&[b"las-swap/relation-binding/v1/las", seed, &probes]));

        Self { pp, binding }
    }

    pub fn public_params(&self) -> &PublicParams {
        &self.pp
    }

    pub fn binding(&self) -> RelationBinding {
        self.binding
    }
}

/// Human-readable parameter set, for the results table.
fn las_param_note() -> &'static str {
    use std::sync::OnceLock;
    static NOTE: OnceLock<String> = OnceLock::new();
    NOTE.get_or_init(|| {
        format!(
            "n={N}, ell={ELL}, n+ell={N_PLUS_ELL}, d=256, kappa={KAPPA}, gamma={GAMMA}, q=8380417"
        )
    })
    .as_str()
}

/// Copy a slice into a fixed-size array, or `None` on a length mismatch.
///
/// Every decode below is length-checked: the driver hands these functions bytes
/// that arrived over a wire, so a wrong length is a protocol error to be
/// rejected, never a panic.
fn fixed<const LEN: usize>(bytes: &[u8]) -> Option<[u8; LEN]> {
    bytes.try_into().ok()
}

/// The LAS adaptor signature, configurations 2 and 3.
pub struct Las {
    params: Rc<LasParams>,
}

impl Las {
    pub fn new(params: Rc<LasParams>) -> Self {
        Self { params }
    }

    fn pp(&self) -> &PublicParams {
        self.params.public_params()
    }
}

impl AdaptorScheme for Las {
    fn name(&self) -> &'static str {
        "LAS (simplified Dilithium-III)"
    }

    fn param_note(&self) -> &'static str {
        las_param_note()
    }

    fn availability(&self) -> Availability {
        Availability::Ready
    }

    fn post_quantum(&self) -> bool {
        true
    }

    fn relation(&self) -> Relation {
        Relation::LasTernaryPreimage
    }

    fn binding(&self) -> RelationBinding {
        self.params.binding()
    }

    fn pk_bytes(&self) -> usize {
        PUBLIC_KEY_BYTES
    }
    fn sk_bytes(&self) -> usize {
        SECRET_KEY_BYTES
    }
    fn statement_bytes(&self) -> usize {
        STATEMENT_BYTES
    }
    fn witness_bytes(&self) -> usize {
        WITNESS_BYTES
    }
    fn signature_bytes(&self) -> usize {
        SIGNATURE_BYTES
    }
    fn pre_signature_bytes(&self) -> usize {
        PRE_SIGNATURE_BYTES
    }

    /// Zero, and that is a result worth stating: a LAS pre-signature and a LAS
    /// signature have the **identical** wire layout `(c_tilde, z)`, so the
    /// adaptor layer costs no extra bytes at all. The tripwire that stops a raw
    /// pre-signature being spent is cryptographic — the `+Y` Fiat–Shamir
    /// mismatch — rather than structural. The classical construction pays a real
    /// per-pre-signature overhead here; LAS does not.
    fn presignature_overhead_bytes(&self) -> usize {
        0
    }

    /// `A` is expanded once in [`LasParams::setup`], before any timing begins.
    /// The cost is attributed to the shared parameter set (reported by
    /// `configurations()` as `las_setup`) rather than to either scheme instance,
    /// because configurations 2 and 3 share one expansion between them.
    fn setup_duration(&self) -> Duration {
        Duration::ZERO
    }

    /// The closed form for this build's parameters: at the D3 set (6, 5, 49) the
    /// `PreSign` loop runs 2.77483 attempts/call on average
    /// (`BOUND_PRESIGN = gamma - kappa`).
    fn expected_presign_attempts(&self) -> Option<f64> {
        Some(las::las_expected_attempts(las::BOUND_PRESIGN))
    }

    fn presign_attempts_counter(&self) -> Option<u64> {
        Some(las::LAS_ATTEMPTS.load(Ordering::Relaxed))
    }

    fn derive_inputs(&self, master_seed: &[u8; 32], iteration: u32) -> Option<SwapInputs> {
        let pp = self.pp();

        // Both key pairs and the statement/witness pair come from domain-separated
        // sub-seeds of the one pinned master seed, so configurations 2 and 3
        // receive byte-identical inputs.
        let (pk1, sk1) = basesig::keygen_seed(pp, &subseed(master_seed, "key-u1", iteration));
        let (pk2, sk2) = basesig::keygen_seed(pp, &subseed(master_seed, "key-u2", iteration));
        let (statement, witness) =
            relation::gen_seed(pp, &subseed(master_seed, "statement", iteration));

        Some(SwapInputs {
            pk1: pack_public_key(&pk1).to_vec(),
            sk1: pack_secret_key(&sk1)?.to_vec(),
            pk2: pack_public_key(&pk2).to_vec(),
            sk2: pack_secret_key(&sk2)?.to_vec(),
            statement: pack_statement(&statement).to_vec(),
            witness: pack_witness(&witness)?.to_vec(),
        })
    }

    fn presign(&self, msg: &[u8], statement: &[u8], pk: &[u8], sk: &[u8]) -> Option<Vec<u8>> {
        let statement = unpack_statement(&fixed::<STATEMENT_BYTES>(statement)?)?;
        let pk = unpack_public_key(&fixed::<PUBLIC_KEY_BYTES>(pk)?)?;
        let sk = unpack_secret_key(&fixed::<SECRET_KEY_BYTES>(sk)?)?;
        // Deterministic path: the mask seed is derived from (sk, Y, M), so a run
        // is reproducible and configurations 2 and 3 produce identical bytes.
        let sigma_hat = las::presign_det(msg, &statement, &pk, &sk, self.pp());
        Some(pack_pre_signature(&sigma_hat)?.to_vec())
    }

    fn preverify(&self, presig: &[u8], msg: &[u8], statement: &[u8], pk: &[u8]) -> bool {
        let Some(presig) =
            fixed::<PRE_SIGNATURE_BYTES>(presig).and_then(|b| unpack_pre_signature(&b))
        else {
            return false;
        };
        let Some(statement) = fixed::<STATEMENT_BYTES>(statement).and_then(|b| unpack_statement(&b))
        else {
            return false;
        };
        let Some(pk) = fixed::<PUBLIC_KEY_BYTES>(pk).and_then(|b| unpack_public_key(&b)) else {
            return false;
        };
        las::preverify(&presig, msg, &statement, &pk, self.pp())
    }

    fn adapt(
        &self,
        presig: &[u8],
        msg: &[u8],
        statement: &[u8],
        witness: &[u8],
        pk: &[u8],
    ) -> Option<Vec<u8>> {
        let presig = unpack_pre_signature(&fixed::<PRE_SIGNATURE_BYTES>(presig)?)?;
        let statement = unpack_statement(&fixed::<STATEMENT_BYTES>(statement)?)?;
        let witness = unpack_witness(&fixed::<WITNESS_BYTES>(witness)?)?;
        let pk = unpack_public_key(&fixed::<PUBLIC_KEY_BYTES>(pk)?)?;
        let sigma = las::adapt(&presig, msg, &statement, &witness, &pk, self.pp())?;
        Some(pack_signature(&sigma)?.to_vec())
    }

    fn ext(&self, sig: &[u8], presig: &[u8], statement: &[u8]) -> Option<Vec<u8>> {
        let sig = unpack_signature(&fixed::<SIGNATURE_BYTES>(sig)?)?;
        let presig = unpack_pre_signature(&fixed::<PRE_SIGNATURE_BYTES>(presig)?)?;
        let statement = unpack_statement(&fixed::<STATEMENT_BYTES>(statement)?)?;
        let s = las::ext(&sig, &presig, &statement, self.pp())?;
        // `pack_witness` accepts only a ternary witness. In the single-hop swap
        // the extracted `s` equals the honest `r` — the M-SIS argument of §4.1,
        // which is exactly what `pi` buys — so it packs. A non-ternary result
        // here would mean that argument had failed, and `None` is the correct
        // answer rather than a silently widened encoding.
        Some(pack_witness(&s)?.to_vec())
    }

    fn verify(&self, sig: &[u8], msg: &[u8], pk: &[u8]) -> bool {
        let Some(sig) = fixed::<SIGNATURE_BYTES>(sig).and_then(|b| unpack_signature(&b)) else {
            return false;
        };
        let Some(pk) = fixed::<PUBLIC_KEY_BYTES>(pk).and_then(|b| unpack_public_key(&b)) else {
            return false;
        };
        basesig::verify(&sig, msg, &pk, self.pp())
    }
}

/// Configuration 3's role-A prover: the Fig. 1 proof of knowledge over the
/// vendored LaZer library.
///
/// Reuses the same C bridge and the same generated parameter set as the C build
/// (`ref/relation_zk_lazer.c`, `ref/relation_zk_params.h`), so the two languages
/// run the identical proof system. The ternary bound is encoded by binary
/// decomposition `r = r₊ − r₋`: LaZer proves per-partition *binary*
/// coefficients, which yields exactly the `‖r‖∞ ≤ 1` that Section 4.1 requires —
/// the norm bound is proven, not assumed.
///
/// Requires the `relation-zk` feature and a built LaZer (see the repo README,
/// "π + atomic swap"); otherwise it reports as unavailable.
pub struct Lazer {
    params: Rc<LasParams>,
}

impl Lazer {
    pub fn new(params: Rc<LasParams>) -> Self {
        Self { params }
    }
}

impl ZkpBackend for Lazer {
    fn name(&self) -> &'static str {
        "LaZer (LNP22 linear relation with norms)"
    }

    fn note(&self) -> &'static str {
        "post-quantum proof of ∃r : A r = t ∧ ‖r‖∞ ≤ 1, via binary decomposition \
         r = r₊ − r₋; knowledge error ≤ 2⁻¹²⁷ under M-SIS, zero-knowledge under M-LWE; \
         exchanged off-chain only (paper §4.1)"
    }

    fn availability(&self) -> Availability {
        #[cfg(feature = "relation-zk")]
        {
            Availability::Ready
        }
        #[cfg(not(feature = "relation-zk"))]
        {
            Availability::Missing(
                "LaZer backend not linked: rebuild with `--features relation-zk` after building \
                 third_party/lazer (see README, \"π + atomic swap\")",
            )
        }
    }

    fn post_quantum(&self) -> bool {
        true
    }

    fn relation(&self) -> Relation {
        Relation::LasTernaryPreimage
    }

    fn binding(&self) -> RelationBinding {
        self.params.binding()
    }

    /// Transparent: the parameter set is public and generated by codegen
    /// (`scripts/las_pi_params.py`, committed as `ref/relation_zk_params.h`).
    /// No secret exists, so there is no trusted party and nothing to destroy.
    fn setup_kind(&self) -> SetupKind {
        SetupKind::Transparent
    }

    /// The proof-system parameters are compiled in, so there is no runtime setup
    /// step to charge for. Stated explicitly rather than inherited by default.
    fn setup_duration(&self) -> Duration {
        Duration::ZERO
    }

    #[allow(unused_variables)]
    fn prove(&self, statement: &[u8], witness: &[u8]) -> Option<Vec<u8>> {
        #[cfg(feature = "relation-zk")]
        {
            use fips204::relation_zk;
            let statement = unpack_statement(&fixed::<STATEMENT_BYTES>(statement)?)?;
            let witness = unpack_witness(&fixed::<WITNESS_BYTES>(witness)?)?;
            let mut proof = [0u8; relation_zk::PI_PROOF_MAX_BYTES];
            let len =
                relation_zk::prove(&mut proof, &statement, &witness, self.params.public_params())?;
            Some(proof[..len].to_vec())
        }
        #[cfg(not(feature = "relation-zk"))]
        {
            None
        }
    }

    #[allow(unused_variables)]
    fn proof_verify(&self, proof: &[u8], statement: &[u8]) -> bool {
        #[cfg(feature = "relation-zk")]
        {
            use fips204::relation_zk;
            let Some(statement) =
                fixed::<STATEMENT_BYTES>(statement).and_then(|b| unpack_statement(&b))
            else {
                return false;
            };
            relation_zk::proof_verify(proof, &statement, self.params.public_params())
        }
        #[cfg(not(feature = "relation-zk"))]
        {
            false
        }
    }
}

/// Configuration 2's role-A prover: Groth16 over BN254, proving the **same**
/// relation `∃r : A r = t ∧ ‖r‖∞ ≤ 1` that [`Lazer`] proves — which is what
/// makes 2 → 3 isolate the proof system.
///
/// The circuit is in [`crate::groth16_circuit`]; see its module docs for how a
/// lattice statement becomes tractable R1CS (the map `r ↦ A r` is linear with
/// public coefficients, so it costs no multiplication constraints; what is
/// actually constrained is the ternary bound and the reduction mod `q`).
///
/// # Randomness: the one place this harness must NOT be reproducible
///
/// Everything else in this crate derives from a pinned seed so runs reproduce.
/// Groth16 is the deliberate exception, in two places, and both are security
/// requirements rather than preferences:
///
/// * **The trusted setup consumes OS entropy.** `circuit_specific_setup`
///   internally samples the toxic waste (`τ, α, β, γ, δ`). If it were driven by
///   a reproducible RNG, anyone re-running the harness would recover those
///   values and could forge a proof for a statement with no witness — Groth16's
///   soundness would be void. The entropy is taken from the OS and the sampled
///   secrets are dropped when setup returns; nothing persists them.
/// * **Each proof draws fresh randomness.** A Groth16 proof is randomised
///   (`r, s`) and that randomness is what makes it zero-knowledge. Reusing it
///   across two proofs for different statements leaks witness information. Every
///   `prove` call therefore takes fresh entropy.
///
/// What this does and does not cost the measurement:
///
/// * **Proof size is unaffected** — a Groth16 proof is three group elements
///   whatever randomness produced it, so the communication figure is exact and
///   still needs no dispersion.
/// * **Proving time is affected, slightly.** The OS entropy draw happens
///   *inside* `prove`, so it is inside the timed region and the `Prove` phase
///   will show a little more run-to-run variance than a seeded RNG would give.
///   That is the honest cost of a sound proof, and it is why the driver reports
///   mean ± SD rather than a single number. It is not removed by pretending the
///   randomness is free.
/// * Only the exact proof *bytes* differ between runs, and those are not a
///   reported quantity.
///
/// # Two properties to report alongside the numbers
///
/// * **Not post-quantum.** Groth16's soundness rests on pairings over elliptic
///   curves, which Shor breaks. Configuration 2 is a *hybrid*: a post-quantum
///   signature whose accompanying proof is not. That is the gap configuration 3
///   closes.
/// * **Trusted setup.** The keys come from a per-circuit ceremony whose secret
///   must be destroyed. LaZer needs no such assumption, so 2 → 3 removes a trust
///   assumption as well as a quantum vulnerability.
///
/// Setup (matrix extraction + key generation) happens in [`Groth16::new`], which
/// `configurations()` runs before any timing begins.
pub struct Groth16 {
    params: Rc<LasParams>,
    setup: Duration,
    #[cfg(feature = "groth16")]
    keys: Option<Groth16Keys>,
}

#[cfg(feature = "groth16")]
struct Groth16Keys {
    matrix: crate::groth16_circuit::CompositeMatrix,
    pk: ark_groth16::ProvingKey<ark_bn254::Bn254>,
    vk: ark_groth16::PreparedVerifyingKey<ark_bn254::Bn254>,
}

impl Groth16 {
    pub fn new(params: Rc<LasParams>) -> Self {
        #[cfg(feature = "groth16")]
        {
            use crate::groth16_circuit::{CompositeMatrix, RelationCircuit, ROWS};
            use ark_groth16::Groth16 as ArkGroth16;
            use ark_snark::SNARK;

            let t0 = std::time::Instant::now();

            // Recover A column by column, then generate the circuit-specific
            // keys. Both are one-time and never inside prove/verify.
            let matrix = CompositeMatrix::extract(params.public_params());

            let setup_circuit = RelationCircuit {
                matrix: &matrix,
                t: vec![ark_bn254::Fr::from(0u64); ROWS],
                r: None,
            };

            // OS entropy: see the type docs. The toxic waste sampled inside is
            // dropped when this call returns.
            let keys = ArkGroth16::<ark_bn254::Bn254>::circuit_specific_setup(
                setup_circuit,
                &mut rand_core::OsRng,
            )
            .ok()
            .map(|(pk, vk)| Groth16Keys {
                matrix,
                vk: ArkGroth16::<ark_bn254::Bn254>::process_vk(&vk)
                    .expect("processing a freshly generated vk cannot fail"),
                pk,
            });

            Self { params, setup: t0.elapsed(), keys }
        }
        #[cfg(not(feature = "groth16"))]
        {
            Self { params, setup: Duration::ZERO }
        }
    }
}

impl ZkpBackend for Groth16 {
    fn name(&self) -> &'static str {
        "Groth16 over BN254 (pairing-based zk-SNARK)"
    }

    fn note(&self) -> &'static str {
        "classical (NOT post-quantum) proof of ∃r : A r = t ∧ ‖r‖∞ ≤ 1; the ternary \
         bound is enforced by r³ = r per coefficient and the modular reduction by a \
         range-checked quotient per row; requires a trusted per-circuit setup"
    }

    fn availability(&self) -> Availability {
        #[cfg(feature = "groth16")]
        {
            if self.keys.is_some() {
                Availability::Ready
            } else {
                Availability::Missing("Groth16 circuit-specific setup failed")
            }
        }
        #[cfg(not(feature = "groth16"))]
        {
            Availability::Missing("Groth16 backend not linked: rebuild with `--features groth16`")
        }
    }

    fn post_quantum(&self) -> bool {
        // Soundness rests on pairings over elliptic curves — broken by Shor.
        // This is the whole reason configuration 3 exists.
        false
    }

    fn relation(&self) -> Relation {
        Relation::LasTernaryPreimage
    }

    fn binding(&self) -> RelationBinding {
        self.params.binding()
    }

    /// A structured reference string per circuit, whose secret must be destroyed
    /// for soundness.
    fn setup_kind(&self) -> SetupKind {
        SetupKind::TrustedPerCircuit
    }

    fn setup_duration(&self) -> Duration {
        self.setup
    }

    #[allow(unused_variables)]
    fn prove(&self, statement: &[u8], witness: &[u8]) -> Option<Vec<u8>> {
        #[cfg(feature = "groth16")]
        {
            use crate::groth16_circuit::{
                public_input_from_bytes, witness_coefficients, RelationCircuit,
            };
            use ark_groth16::Groth16 as ArkGroth16;
            use ark_serialize::CanonicalSerialize;
            use ark_snark::SNARK;

            let keys = self.keys.as_ref()?;
            let t = public_input_from_bytes(statement)?;
            let packed: [u8; WITNESS_BYTES] = witness.try_into().ok()?;
            let r = witness_coefficients(&packed)?;

            let circuit = RelationCircuit { matrix: &keys.matrix, t, r: Some(r) };
            // Fresh entropy per proof: Groth16's proof randomness is what makes
            // it zero-knowledge, and reusing it across statements leaks.
            let proof =
                ArkGroth16::<ark_bn254::Bn254>::prove(&keys.pk, circuit, &mut rand_core::OsRng)
                    .ok()?;

            let mut out = Vec::new();
            proof.serialize_compressed(&mut out).ok()?;
            Some(out)
        }
        #[cfg(not(feature = "groth16"))]
        {
            None
        }
    }

    #[allow(unused_variables)]
    fn proof_verify(&self, proof: &[u8], statement: &[u8]) -> bool {
        #[cfg(feature = "groth16")]
        {
            use crate::groth16_circuit::public_input_from_bytes;
            use ark_groth16::Groth16 as ArkGroth16;
            use ark_serialize::CanonicalDeserialize;
            use ark_snark::SNARK;

            let Some(keys) = self.keys.as_ref() else {
                return false;
            };
            let Some(t) = public_input_from_bytes(statement) else {
                return false;
            };
            let Ok(proof) = ark_groth16::Proof::deserialize_compressed(proof) else {
                return false;
            };
            ArkGroth16::<ark_bn254::Bn254>::verify_with_processed_vk(&keys.vk, &t, &proof)
                .unwrap_or(false)
        }
        #[cfg(not(feature = "groth16"))]
        {
            false
        }
    }
}
