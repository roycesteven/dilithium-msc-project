//! The three Stage-2 configurations, behind two traits.
//!
//! | # | Signature | Role-A proof of knowledge `pi` |
//! |---|---|---|
//! | 1 | classical adaptor (ECDSA) | **not required** — see below |
//! | 2 | LAS (post-quantum) | Groth16 over `∃r : A r = t ∧ ‖r‖∞ ≤ 1` |
//! | 3 | LAS (post-quantum) | LaZer over `∃r : A r = t ∧ ‖r‖∞ ≤ 1` |
//!
//! # What "role-A `pi`" is, precisely
//!
//! It is the proof eprint 2020/845 Fig. 1 puts on the wire immediately after the
//! statement `Y`:
//!
//! * **author:** `u_1`, the party that ran `Gen()` and holds the witness;
//! * **claim:** "I know a witness to `Y`" — for LAS, `∃r : A r = t ∧ ‖r‖∞ ≤ 1`;
//! * **when:** before `u_2` is willing to pre-sign anything.
//!
//! Section 4.1 explains why LAS needs it. LAS has a *knowledge gap*: `Ext`
//! returns a witness of the **extended** relation `R'_A`, which need not satisfy
//! `‖s‖∞ ≤ 1`. If `u_1` could choose a `Y` whose only witnesses are large, then
//! after `u_1` claims, the `s` that `u_2` extracts would be unusable and `u_2`'s
//! `Adapt` would fail — `u_1` gets paid, `u_2` does not. `pi` closes that gap:
//! it forces a *ternary* witness to exist, and M-SIS then pins the extracted `s`
//! to equal it. In the paper's words, "`pi` is essential to make sure that `u_2`
//! receives the coins `c_1`".
//!
//! # Why configuration 1 has no role-A proof — and what its DLEQ actually is
//!
//! **A classical ECDSA adaptor has no knowledge gap.** Extraction recovers the
//! discrete log exactly, so pre-signature adaptability is unconditional and
//! there is nothing for a proof of witness knowledge to rescue. Real classical
//! swaps therefore do not carry Fig. 1's `pi`, and adding one would measure a
//! construction nobody deploys.
//!
//! secp256k1-zkp's adaptor pre-signature *does* contain a DLEQ proof (97 of its
//! 162 bytes), and it must **not** be reported as role-A `pi`. They differ in
//! every respect that matters:
//!
//! | | Fig. 1's `pi` (role A) | secp256k1-zkp's embedded DLEQ |
//! |---|---|---|
//! | author | `u_1`, the statement generator | the **pre-signer** (`u_2` for `tx_2`) |
//! | claim | "I know a witness to `Y`" | "this pre-signature is well formed w.r.t. `Y`" — the nonce is consistent across the two group elements |
//! | protects | `u_2` against an unopenable statement | the verifier against a malformed pre-signature |
//! | when | once, right after `Gen()` | once per pre-signature |
//! | counted as | a separate protocol message | part of the pre-signature's bytes |
//!
//! The DLEQ is a component of the *adaptor signature construction*, checked
//! inside `PreVerify`. Its bytes are reported under the signature's
//! communication cost via [`AdaptorScheme::internal_proof_bytes`], never as
//! `pi`. Conflating the two would credit the classical baseline with satisfying
//! a requirement it never had to meet.
//!
//! This is also **not** a succinct proof that an on-chain verification was
//! performed correctly — that is the deferred EVM path, and `rust/las-stark/`.
//! See `docs/02-methodology/STAGE2_UTXO_SWAP_PLAN.md` §2.
//!
//! # What the configurations can and cannot attribute
//!
//! * **2 → 3 is a controlled comparison.** Same signature scheme, same public
//!   parameters `A`, same relation, and — because every input is derived
//!   deterministically from one pinned master seed ([`SwapInputs`]) —
//!   byte-identical keys, statement, witness and transactions. *Only the prover
//!   changes*, so the difference is attributable to the proof system. This is
//!   the comparison to lead with.
//! * **1 → 2/3 is a whole-stack comparison.** The signature scheme, the
//!   relation, and whether a role-A proof exists at all change together. Report
//!   it as "classical baseline versus post-quantum stack", and state that part
//!   of the gap is a requirement LAS has and ECDSA does not. The
//!   signature-only comparison is the Stage-1 result (`ref/test/bench_classical.c`,
//!   `docs/LAS.md` §8.3).
//!
//! # Why the interface is bytes
//!
//! Every operation takes and returns wire-format buffers rather than scheme
//! structs: communication cost is *observed* rather than computed from a
//! formula that could drift, and it is the only way one driver can run schemes
//! whose native types share nothing. The cost is that these timings sit at the
//! **packed tier** of the Stage-1 two-tier convention (pack/unpack included),
//! not the core tier — the driver prints this and the report must state it.
//!
//! # One-time setup is never inside a timed region
//!
//! Expanding `A`, and (for a pairing-based backend) generating a structured
//! reference string, are one-time costs amortised over every swap that uses
//! them. [`configurations`] does all of it up front and reports it separately.

use std::rc::Rc;

use crate::las_backend::LasParams;

/// Default master seed. Every parameter, key, statement and mask in a run is
/// derived from this by domain-separated SHAKE256, so a run is byte-for-byte
/// reproducible and the seed is the only thing that must be recorded alongside
/// the numbers. Overridable with `LAS_SWAP_SEED=<64 hex chars>`.
pub const DEFAULT_MASTER_SEED: [u8; 32] = *b"las-swap/eprint-2020-845/fig1/v1";

/// Read the master seed from `LAS_SWAP_SEED` (64 hex chars) or fall back to
/// [`DEFAULT_MASTER_SEED`]. Returns the seed and whether it was overridden, so
/// the driver records which one produced the numbers.
///
/// A malformed `LAS_SWAP_SEED` is a hard error rather than a silent fallback:
/// the operator asked for a specific seed, and quietly substituting a different
/// one would produce numbers that do not correspond to the seed they recorded.
pub fn master_seed_from_env() -> ([u8; 32], bool) {
    let Ok(hex) = std::env::var("LAS_SWAP_SEED") else {
        return (DEFAULT_MASTER_SEED, false);
    };
    assert_eq!(
        hex.len(),
        64,
        "LAS_SWAP_SEED must be exactly 64 hex characters (32 bytes), got {}",
        hex.len()
    );
    let mut seed = [0u8; 32];
    for (i, byte) in seed.iter_mut().enumerate() {
        *byte = u8::from_str_radix(&hex[2 * i..2 * i + 2], 16)
            .unwrap_or_else(|_| panic!("LAS_SWAP_SEED is not valid hex at byte {i}"));
    }
    (seed, true)
}

/// Domain-separated sub-seed derivation: `SHAKE256(tag ‖ master ‖ iteration)`.
///
/// Used by every backend so two scheme instances over the same parameters derive
/// byte-identical material for the same `(master, iteration)`.
pub fn subseed(master: &[u8; 32], tag: &str, iteration: u32) -> [u8; 32] {
    use sha3::digest::{ExtendableOutput, Update, XofReader};
    let mut h = sha3::Shake256::default();
    h.update(b"las-swap/subseed/v1");
    h.update(tag.as_bytes());
    h.update(master);
    h.update(&iteration.to_le_bytes());
    let mut out = [0u8; 32];
    h.finalize_xof().read(&mut out);
    out
}

/// One swap's key material, in wire format.
///
/// Produced by [`AdaptorScheme::derive_inputs`] deterministically, so
/// configurations sharing a scheme and parameters receive **identical bytes**.
/// This is what makes the 2 → 3 comparison controlled rather than merely similar.
#[derive(Debug, Clone)]
pub struct SwapInputs {
    /// `u_1` (Alice) — the witness holder, who commits first in Fig. 1.
    pub pk1: Vec<u8>,
    pub sk1: Vec<u8>,
    /// `u_2` (Bob).
    pub pk2: Vec<u8>,
    pub sk2: Vec<u8>,
    /// `(Y, y)` — the statement/witness pair from Fig. 1's `Gen()`.
    pub statement: Vec<u8>,
    pub witness: Vec<u8>,
}

/// The hard relation a statement/witness pair belongs to.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Relation {
    /// secp256k1 discrete log: know `y` with `Y = y·G`.
    Secp256k1DiscreteLog,
    /// LAS module-lattice preimage: `∃r : A r = t ∧ ‖r‖∞ ≤ 1`, **with respect
    /// to one specific public matrix `A`**. Two instances with different `A` are
    /// different relations — hence [`RelationBinding`].
    LasTernaryPreimage,
}

impl Relation {
    pub fn describe(self) -> &'static str {
        match self {
            Relation::Secp256k1DiscreteLog => "∃y : Y = y·G  (secp256k1 discrete log)",
            Relation::LasTernaryPreimage => "∃r : A r = t ∧ ‖r‖∞ ≤ 1  (module-lattice preimage)",
        }
    }
}

/// Identifies the *instance* of a relation — the concrete public parameters it
/// is stated over.
///
/// This is a digest of the parameter instance **as the scheme actually computes
/// with it** (see [`LasParams::binding`]), not of a seed held alongside it. A
/// seed is an input to parameter generation; hashing it would certify that two
/// backends were *asked* for the same parameters, not that they are *using* the
/// same ones. Deriving the binding through the scheme's own code closes that gap.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct RelationBinding(pub [u8; 32]);

impl RelationBinding {
    /// The binding for a relation with no per-instance parameters (secp256k1:
    /// curve and generator are fixed by the standard).
    pub fn fixed(tag: &[u8]) -> Self {
        use sha3::digest::{ExtendableOutput, Update, XofReader};
        let mut h = sha3::Shake256::default();
        h.update(b"las-swap/relation-binding/v1/fixed");
        h.update(tag);
        let mut out = [0u8; 32];
        h.finalize_xof().read(&mut out);
        Self(out)
    }

    /// Short hex prefix, for printing beside the results.
    pub fn short_hex(&self) -> String {
        self.0[..8].iter().map(|b| format!("{b:02x}")).collect()
    }
}

/// Whether a backend is actually compiled into this build.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Availability {
    Ready,
    /// Not linked; the payload explains what is missing and how to supply it.
    Missing(&'static str),
}

impl Availability {
    pub fn is_ready(self) -> bool {
        matches!(self, Availability::Ready)
    }
}

/// A signature scheme with the four adaptor operations (paper Definition 3: a
/// base signature plus `PreSign`/`PreVerify`/`Adapt`/`Ext`).
///
/// **All operations take the deterministic path** where the scheme offers one
/// (`keygen_seed`, `gen_seed`, `presign_det`). Randomised paths would make a run
/// unreproducible and would break the byte-identity configurations 2 and 3
/// depend on.
pub trait AdaptorScheme {
    fn name(&self) -> &'static str;
    /// Parameter set, for the results table (fairness rule: state the parameters
    /// of every scheme compared).
    fn param_note(&self) -> &'static str;
    fn availability(&self) -> Availability;
    /// Whether the signature itself is post-quantum secure.
    fn post_quantum(&self) -> bool;

    /// The relation this scheme's statements `Y` belong to.
    fn relation(&self) -> Relation;
    /// The concrete instance of that relation (for LAS, the `A` in use).
    fn binding(&self) -> RelationBinding;

    // Wire sizes. Fixed per scheme, so reported once without dispersion.
    fn pk_bytes(&self) -> usize;
    fn sk_bytes(&self) -> usize;
    fn statement_bytes(&self) -> usize;
    fn witness_bytes(&self) -> usize;
    fn signature_bytes(&self) -> usize;
    fn pre_signature_bytes(&self) -> usize;

    /// How many bytes larger a pre-signature is than an ordinary signature.
    ///
    /// For ECDSA this excess carries the adaptor's nonce commitments **and** its
    /// DLEQ proof together. The harness deliberately does **not** split it into
    /// "proof bytes" versus "structure bytes": the vendored header documents the
    /// pre-signature only as an opaque 162-byte object, so any split would be an
    /// invented number. It is reported as what it is — pre-signature overhead.
    ///
    /// Whatever proof material sits inside it is *not* Fig. 1's role-A `pi`
    /// (different author, different claim, different purpose — see the module
    /// docs), and belongs to the signature's communication cost.
    fn presignature_overhead_bytes(&self) -> usize {
        self.pre_signature_bytes().saturating_sub(self.signature_bytes())
    }

    /// Wall-clock cost of this scheme's **one-time** setup — expanding `A`,
    /// creating a library context, and any other per-parameter-set work.
    ///
    /// Backends do that work in their constructor, which [`configurations`] runs
    /// before any timing begins, so it never lands inside a per-operation
    /// measurement. Reported separately because it is amortised over every swap.
    ///
    /// Deliberately has no default: a backend must state its setup cost — even
    /// if zero — rather than inherit a silent zero it never considered.
    fn setup_duration(&self) -> std::time::Duration;

    /// Exact expected rejection-loop attempts per `PreSign` call, for schemes
    /// that use Fiat–Shamir with aborts; `None` for schemes that do not (ECDSA
    /// signs in one shot).
    ///
    /// Together with [`Self::presign_attempts_counter`] this is the project's
    /// standing **run-validity gate**: if the measured attempt rate matches the
    /// closed form, the sampler is healthy and the run was long enough to be
    /// representative. Never weaken or rename it.
    fn expected_presign_attempts(&self) -> Option<f64>;

    /// Monotonic count of rejection-loop iterations performed by `PreSign` since
    /// the process started, or `None` when the scheme has no rejection loop.
    /// The driver samples it either side of a batch and divides by the number of
    /// calls.
    fn presign_attempts_counter(&self) -> Option<u64>;

    /// Derive both key pairs and the statement/witness pair for swap
    /// `iteration`, deterministically from `master_seed`.
    ///
    /// Contract: two instances of the same scheme over the same parameters
    /// **must** return byte-identical output for the same arguments. The driver
    /// relies on this to give configurations 2 and 3 the same inputs.
    fn derive_inputs(&self, master_seed: &[u8; 32], iteration: u32) -> Option<SwapInputs>;

    /// `PreSign((pk, sk), Y, M)` → `sigma_hat`, deterministic path.
    fn presign(&self, msg: &[u8], statement: &[u8], pk: &[u8], sk: &[u8]) -> Option<Vec<u8>>;

    /// `PreVerify(Y, pk, sigma_hat, M)`. For ECDSA this also checks the
    /// construction's embedded DLEQ.
    fn preverify(&self, presig: &[u8], msg: &[u8], statement: &[u8], pk: &[u8]) -> bool;

    /// `Adapt((Y, y), pk, sigma_hat, M)` → `sigma`, or `None` for `⊥`.
    fn adapt(
        &self,
        presig: &[u8],
        msg: &[u8],
        statement: &[u8],
        witness: &[u8],
        pk: &[u8],
    ) -> Option<Vec<u8>>;

    /// `Ext(Y, sigma, sigma_hat)` → witness, or `None` for `⊥`.
    fn ext(&self, sig: &[u8], presig: &[u8], statement: &[u8]) -> Option<Vec<u8>>;

    /// Ordinary `Verify` — the chain's spending rule (paper §4's "the signature
    /// algorithm"). The ledger calls exactly this and nothing else.
    fn verify(&self, sig: &[u8], msg: &[u8], pk: &[u8]) -> bool;
}

/// A prover for Fig. 1's role-A proof of knowledge `pi`.
pub trait ZkpBackend {
    fn name(&self) -> &'static str;
    /// What it proves and how it is realised — printed beside the numbers.
    fn note(&self) -> &'static str;
    fn availability(&self) -> Availability;
    /// Whether the proof system itself is post-quantum secure.
    fn post_quantum(&self) -> bool;

    /// The relation this backend proves statements of.
    fn relation(&self) -> Relation;
    /// The concrete instance of that relation this backend is bound to.
    fn binding(&self) -> RelationBinding;

    /// What kind of one-time setup this proof system requires. Reported beside
    /// the numbers: it is a security property, not only a cost.
    fn setup_kind(&self) -> SetupKind;

    /// Wall-clock cost of this backend's **one-time** setup.
    ///
    /// Backends perform that work in their constructor, which [`configurations`]
    /// runs before any timing begins. It is therefore never inside a
    /// [`Self::prove`] or [`Self::proof_verify`] measurement; the driver reports
    /// it as a separate one-time figure, because it is amortised over every swap
    /// that uses the parameter set rather than paid per swap.
    ///
    /// Deliberately has no default: a backend must state its setup cost — even
    /// if zero — rather than inherit a silent zero it never considered.
    fn setup_duration(&self) -> std::time::Duration;

    /// `pi ← P((Y; y), R)` for this backend's relation `R`. Proving only — no
    /// setup work happens here.
    fn prove(&self, statement: &[u8], witness: &[u8]) -> Option<Vec<u8>>;

    /// Verify `pi` against `Y` under this backend's relation and binding.
    /// Verification only — no setup work happens here.
    fn proof_verify(&self, proof: &[u8], statement: &[u8]) -> bool;
}

/// What kind of one-time setup a proof system requires.
///
/// This distinction is reportable in its own right. Configuration 2's Groth16
/// needs a **trusted** per-circuit ceremony whose secret must be destroyed for
/// soundness to hold; configuration 3's LaZer needs none. So 2 → 3 removes a
/// trust assumption as well as a quantum vulnerability, and the evaluation
/// should say so rather than comparing only proof sizes and times.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SetupKind {
    /// No setup at all.
    None,
    /// Public-coin / transparent: no secret is involved, so there is nothing to
    /// trust and nothing to destroy.
    Transparent,
    /// A structured reference string generated per circuit. Soundness depends on
    /// the setup secret ("toxic waste") having been destroyed.
    TrustedPerCircuit,
}

impl SetupKind {
    pub fn describe(self) -> &'static str {
        match self {
            SetupKind::None => "none",
            SetupKind::Transparent => "transparent (public-coin; no trusted party)",
            SetupKind::TrustedPerCircuit => {
                "trusted, per circuit (soundness requires the setup secret to be destroyed)"
            }
        }
    }
}

/// Whether a configuration carries Fig. 1's role-A proof, and if not, why not.
///
/// Modelled as a sum type rather than an "always present, sometimes trivial"
/// backend, because "this construction does not need `pi`" is a *result* about
/// the construction, not a missing feature — and the report has to say so.
pub enum RoleAProof {
    Required(Box<dyn ZkpBackend>),
    /// The construction needs no proof of witness knowledge. The payload is the
    /// justification, printed in the results table.
    NotRequired { reason: &'static str },
}

impl RoleAProof {
    pub fn name(&self) -> &'static str {
        match self {
            RoleAProof::Required(z) => z.name(),
            RoleAProof::NotRequired { .. } => "none (not required by the construction)",
        }
    }

    pub fn post_quantum(&self) -> bool {
        match self {
            RoleAProof::Required(z) => z.post_quantum(),
            // Nothing to break: there is no proof system in the stack.
            RoleAProof::NotRequired { .. } => true,
        }
    }

    pub fn backend(&self) -> Option<&dyn ZkpBackend> {
        match self {
            RoleAProof::Required(z) => Some(z.as_ref()),
            RoleAProof::NotRequired { .. } => None,
        }
    }
}

/// Why a configuration can or cannot be measured.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ConfigStatus {
    Ready,
    /// One half is not compiled in.
    BackendMissing(&'static str),
    /// The proof system does not speak about this scheme's statements.
    RelationMismatch { scheme: Relation, zkp: Relation },
    /// Same relation, but stated over different public parameters — the proof
    /// would be about a different `A` than the signatures use.
    BindingMismatch,
}

impl ConfigStatus {
    pub fn is_ready(self) -> bool {
        matches!(self, ConfigStatus::Ready)
    }
}

/// One configuration = one signature scheme × its role-A proof situation.
pub struct Configuration {
    pub number: u8,
    pub label: &'static str,
    pub scheme: Box<dyn AdaptorScheme>,
    pub role_a: RoleAProof,
}

impl Configuration {
    /// Ready only when every linked half is present **and**, where a proof is
    /// required, it agrees with the scheme about the statement: same relation,
    /// same instance of it.
    pub fn status(&self) -> ConfigStatus {
        if let Availability::Missing(m) = self.scheme.availability() {
            return ConfigStatus::BackendMissing(m);
        }
        let Some(zkp) = self.role_a.backend() else {
            return ConfigStatus::Ready;
        };
        if let Availability::Missing(m) = zkp.availability() {
            return ConfigStatus::BackendMissing(m);
        }
        if self.scheme.relation() != zkp.relation() {
            return ConfigStatus::RelationMismatch {
                scheme: self.scheme.relation(),
                zkp: zkp.relation(),
            };
        }
        if self.scheme.binding() != zkp.binding() {
            return ConfigStatus::BindingMismatch;
        }
        ConfigStatus::Ready
    }

    /// True when the whole stack (signature *and* any proof) is post-quantum.
    pub fn fully_post_quantum(&self) -> bool {
        self.scheme.post_quantum() && self.role_a.post_quantum()
    }
}

/// The configuration table plus the one-time setup cost that produced it.
pub struct ConfigurationSet {
    pub configs: Vec<Configuration>,
    /// The seed every parameter, key and statement was derived from.
    pub master_seed: [u8; 32],
    /// Whether it came from `LAS_SWAP_SEED` rather than the pinned default.
    pub seed_overridden: bool,
    /// Wall-clock cost of the one-time parameter setup shared by
    /// configurations 2 and 3 (expanding `A` from its seed). Reported
    /// separately; never folded into a per-operation timing.
    pub las_setup: std::time::Duration,
}

/// Build the configuration table, in the order Meeting-7 fixed.
///
/// **Configurations 2 and 3 share one [`LasParams`]**, hence one `A`, one
/// parameter set and one relation binding; and because both derive key material
/// deterministically from `master_seed`, they also receive byte-identical keys,
/// statements and witnesses. Those two facts are what make the 2 → 3 delta
/// attributable to the proof system alone.
///
/// Every backend is constructed **from the actual parameter instance**, not from
/// a seed or a digest, so a backend cannot be bound to parameters it is not
/// computing with. All of this runs before any timing begins.
pub fn configurations() -> ConfigurationSet {
    use crate::ecdsa_backend::EcdsaAdaptor;
    use crate::las_backend::{Groth16, Las, Lazer};

    let (master_seed, seed_overridden) = master_seed_from_env();

    // One-time setup, outside every timed region.
    let t0 = std::time::Instant::now();
    let las_params = Rc::new(LasParams::setup(&subseed(&master_seed, "public-params", 0)));
    let las_setup = t0.elapsed();

    let configs = vec![
        Configuration {
            number: 1,
            label: "Classical adaptor signature (ECDSA)",
            scheme: Box::new(EcdsaAdaptor::new()),
            role_a: RoleAProof::NotRequired {
                reason: "ECDSA adaptors have no knowledge gap: Ext recovers the discrete log \
                         exactly, so pre-signature adaptability is unconditional and Fig. 1's \
                         proof of witness knowledge is unnecessary. (The construction's own \
                         DLEQ proves pre-signature well-formedness by the pre-signer — a \
                         different claim — and is counted under the signature's communication \
                         cost, inside the pre-signature overhead.)",
            },
        },
        Configuration {
            number: 2,
            label: "LAS (post-quantum) + Groth16 proof of ∃r : A r = t ∧ ‖r‖∞ ≤ 1",
            scheme: Box::new(Las::new(Rc::clone(&las_params))),
            role_a: RoleAProof::Required(Box::new(Groth16::new(Rc::clone(&las_params)))),
        },
        Configuration {
            number: 3,
            label: "LAS (post-quantum) + LaZer proof of ∃r : A r = t ∧ ‖r‖∞ ≤ 1",
            scheme: Box::new(Las::new(Rc::clone(&las_params))),
            role_a: RoleAProof::Required(Box::new(Lazer::new(Rc::clone(&las_params)))),
        },
    ];

    ConfigurationSet { configs, master_seed, seed_overridden, las_setup }
}
