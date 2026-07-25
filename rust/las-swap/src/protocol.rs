//! The atomic-swap protocol of eprint 2020/845 §4.1, Fig. 1, implemented
//! **verbatim** over two UTXO chains.
//!
//! ```text
//! u1((pk1, sk1), pk2, c1)
//!     (Y, y) = (t, r) <- Gen()
//!     pi <- P((t; r), {exists r : A r = t and ||r||inf <= 1})
//!     Generate tx1 for spending c1 to u2
//!     sigma_hat_1 <- PreSign((pk1, sk1), Y, tx1)
//!                                          --> Y, pi, sigma_hat_1, tx1
//! u2((pk2, sk2), pk1, c2)
//!     If verif. of pi or sigma_hat_1 fails, Abort
//!     Generate tx2 for spending c2 to u1
//!     sigma_hat_2 <- PreSign((pk2, sk2), Y, tx2)
//!                                          --> sigma_hat_2, tx2
//! u1
//!     sigma_2 <- Adapt((Y, y), pk2, sigma_hat_2, tx2)
//!     If sigma_2 = bottom, Abort
//!     Publish sigma_2 on blockchain        --> sigma_2
//! u2
//!     y' <- Ext(Y, sigma_2, sigma_hat_2)
//!     sigma_1 <- Adapt((Y, y'), pk1, sigma_hat_1, tx1)
//!     Publish sigma_1 on blockchain if sigma_1 != bottom
//! ```
//!
//! # Points where fidelity to the figure matters
//!
//! * **`u_1` commits first.** The witness holder pre-signs her *own* coin before
//!   `u_2` has committed anything. Reversing this would change who bears the
//!   risk, which is the whole content of the fairness argument.
//! * **The abort gate is `pi` *and* `sigma_hat_1`,** checked before `u_2`
//!   pre-signs. Both are load-bearing: `pi` rules out an unopenable statement,
//!   `PreVerify` rules out a malformed pre-signature.
//! * **`u_2` re-reads `sigma_2` from the chain** rather than being handed it.
//!   That is the actual leak channel — `Ext` runs on public data, which is why
//!   the swap is atomic and needs no cooperation at the end.
//! * **`Ext`'s output is used, not the original witness.** `u_2` adapts with
//!   `y'` as extracted; substituting `y` would hide an extraction failure behind
//!   a passing run.
//!
//! Fig. 1 shows the honest path only; timeout/refund is an edge case
//! (`las-context-consolidated.md` §16.4) and lives in [`run_refund`].

use crate::backend::{AdaptorScheme, Configuration, RoleAProof, SwapInputs};
use crate::metrics::{Direction, Phase, SwapTiming, Transcript};
use crate::utxo::{Chain, Condition, OutPoint, Tx, TxError};

/// Why a swap did not complete. Each corresponds to an explicit `Abort` in
/// Fig. 1, or to a chain rejecting a settlement transaction.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum SwapError {
    /// `Gen()`/`KeyGen` could not produce inputs.
    KeyMaterial,
    /// `u_1` could not produce `pi`.
    ProveFailed,
    /// Fig. 1: "If verif. of pi ... fails, Abort".
    AbortProofRejected,
    /// Fig. 1: "If verif. of ... sigma_hat_1 fails, Abort".
    AbortPreVerifyRejected,
    /// A `PreSign` did not return a pre-signature.
    PreSignFailed,
    /// Fig. 1: "If sigma_2 = bottom, Abort".
    AbortAdaptFailed,
    /// `Ext` returned `⊥` — the extracted value is not a witness for `Y`.
    ExtFailed,
    /// A chain rejected a transaction.
    Settlement(&'static str, TxError),
    /// A settled signature verified, but funds did not move as intended.
    Atomicity(&'static str),
}

/// Everything one swap produced.
pub struct SwapOutcome {
    pub transcript: Transcript,
    /// Whether the witness `u_2` extracted from the chain equals the one `u_1`
    /// generated — the paper's §4.1 M-SIS conclusion, made observable.
    pub extracted_matches: bool,
}

/// Value moved by each leg, in whatever unit the model chain counts.
const COIN: u64 = 100_000_000;

/// Run one complete Fig. 1 swap across `chain1` and `chain2`.
///
/// `chain1` holds `u_1`'s coin `c_1`; `chain2` holds `u_2`'s coin `c_2`. Both
/// chains must have been constructed over this configuration's scheme, so their
/// spending rule is that scheme's `Verify` (paper §4: "the signature algorithm
/// is replaced with ...").
pub fn run_swap(
    config: &Configuration,
    inputs: &SwapInputs,
    chain1: &mut Chain<'_>,
    chain2: &mut Chain<'_>,
    timing: &mut SwapTiming,
) -> Result<SwapOutcome, SwapError> {
    let scheme: &dyn AdaptorScheme = config.scheme.as_ref();
    let mut transcript = Transcript::default();

    // ---- Funding. Not part of Fig. 1: the coins exist before the protocol
    // starts. c_1 is spendable by u_1's signature and c_2 by u_2's, which is
    // what lets sigma_hat_1 (made under sk_1) authorise spending c_1.
    let c1: OutPoint = chain1.coinbase(COIN, Condition::pay_to(&inputs.pk1));
    let c2: OutPoint = chain2.coinbase(COIN, Condition::pay_to(&inputs.pk2));

    // ---- u_1: generate tx_1 (spending c_1 to u_2), prove, pre-sign ----------
    let mut tx1 = Tx::new(0);
    tx1.add_input(c1.clone(), false).add_output(COIN, Condition::pay_to(&inputs.pk2));
    let tx1_sighash = tx1.sighash();

    // pi <- P((t; r), {exists r : A r = t and ||r||inf <= 1})
    let pi = match &config.role_a {
        RoleAProof::Required(zkp) => Some(
            timing
                .measure(Phase::Prove, || zkp.prove(&inputs.statement, &inputs.witness))
                .ok_or(SwapError::ProveFailed)?,
        ),
        // The construction needs no proof of witness knowledge, so no Prove
        // phase is recorded — which is why `Aggregate` permits a phase to be
        // absent from *every* run of a configuration.
        RoleAProof::NotRequired { .. } => None,
    };

    let sigma_hat_1 = timing
        .measure(Phase::PreSignU1, || {
            scheme.presign(&tx1_sighash, &inputs.statement, &inputs.pk1, &inputs.sk1)
        })
        .ok_or(SwapError::PreSignFailed)?;

    // --> Y, pi, sigma_hat_1, tx1
    // tx1 travels as a serialised (still unsigned) transaction, not as its
    // sighash: the sighash is only the signing message.
    transcript.record("Y (statement)", Direction::U1ToU2, inputs.statement.len());
    if let Some(p) = &pi {
        transcript.record("pi", Direction::U1ToU2, p.len());
    }
    transcript.record("sigma_hat_1", Direction::U1ToU2, sigma_hat_1.len());
    transcript.record("tx1", Direction::U1ToU2, tx1.serialize().len());

    // ---- u_2: "If verif. of pi or sigma_hat_1 fails, Abort" -----------------
    if let (RoleAProof::Required(zkp), Some(p)) = (&config.role_a, &pi) {
        let ok = timing.measure(Phase::ProofVerify, || zkp.proof_verify(p, &inputs.statement));
        if !ok {
            return Err(SwapError::AbortProofRejected);
        }
    }

    let ok = timing.measure(Phase::PreVerifyU1, || {
        scheme.preverify(&sigma_hat_1, &tx1_sighash, &inputs.statement, &inputs.pk1)
    });
    if !ok {
        return Err(SwapError::AbortPreVerifyRejected);
    }

    // ---- u_2: generate tx_2 (spending c_2 to u_1), pre-sign under the SAME Y -
    let mut tx2 = Tx::new(0);
    tx2.add_input(c2.clone(), false).add_output(COIN, Condition::pay_to(&inputs.pk1));
    let tx2_sighash = tx2.sighash();

    let sigma_hat_2 = timing
        .measure(Phase::PreSignU2, || {
            scheme.presign(&tx2_sighash, &inputs.statement, &inputs.pk2, &inputs.sk2)
        })
        .ok_or(SwapError::PreSignFailed)?;

    // --> sigma_hat_2, tx2
    transcript.record("sigma_hat_2", Direction::U2ToU1, sigma_hat_2.len());
    transcript.record("tx2", Direction::U2ToU1, tx2.serialize().len());

    // ---- u_1: sigma_2 <- Adapt(...); abort on bottom; publish on chain 2 ----
    let sigma_2 = timing
        .measure(Phase::AdaptU1, || {
            scheme.adapt(&sigma_hat_2, &tx2_sighash, &inputs.statement, &inputs.witness, &inputs.pk2)
        })
        .ok_or(SwapError::AbortAdaptFailed)?;

    tx2.inputs[0].sig = sigma_2;
    timing
        .measure(Phase::SettleChain2, || chain2.submit(&tx2))
        .map_err(|e| SwapError::Settlement("chain 2", e))?;
    transcript.record("tx2 + sigma_2", Direction::OnChain("chain 2"), tx2.onchain_bytes());

    // ---- u_2: read sigma_2 back OFF THE CHAIN, then Ext ---------------------
    // The leak. u_2 is not handed sigma_2; it observes the settled transaction
    // exactly as any other chain watcher would.
    let published = chain2
        .spending_signature(&c2)
        .ok_or(SwapError::Atomicity("sigma_2 not observable on chain 2"))?;

    let y_ext = timing
        .measure(Phase::Ext, || scheme.ext(&published, &sigma_hat_2, &inputs.statement))
        .ok_or(SwapError::ExtFailed)?;
    let extracted_matches = y_ext == inputs.witness;

    // ---- u_2: sigma_1 <- Adapt((Y, y'), ...); publish on chain 1 ------------
    // Adapting with the EXTRACTED y', not the original witness: substituting the
    // original would hide an extraction failure behind a passing run.
    let sigma_1 = timing
        .measure(Phase::AdaptU2, || {
            scheme.adapt(&sigma_hat_1, &tx1_sighash, &inputs.statement, &y_ext, &inputs.pk1)
        })
        .ok_or(SwapError::AbortAdaptFailed)?;

    tx1.inputs[0].sig = sigma_1;
    timing
        .measure(Phase::SettleChain1, || chain1.submit(&tx1))
        .map_err(|e| SwapError::Settlement("chain 1", e))?;
    transcript.record("tx1 + sigma_1", Direction::OnChain("chain 1"), tx1.onchain_bytes());

    // ---- atomicity: both legs moved, each to the correct party --------------
    if chain2.balance(&inputs.pk1) != COIN {
        return Err(SwapError::Atomicity("u1 did not receive c2 on chain 2"));
    }
    if chain1.balance(&inputs.pk2) != COIN {
        return Err(SwapError::Atomicity("u2 did not receive c1 on chain 1"));
    }

    Ok(SwapOutcome { transcript, extracted_matches })
}

/// The timeout/refund edge case (`las-context-consolidated.md` §16.4).
///
/// Not part of Fig. 1, which shows the honest path only. The invariant checked
/// is the one that matters: **a counterparty who walks away must not cost the
/// honest party their coin.** `u_1` locks a coin claimable by `u_2`, `u_2` never
/// responds, and after the timeout `u_1` reclaims it.
///
/// It also asserts the refund is *not* available early — otherwise the timelock
/// would be decorative and `u_2` could be denied the chance to complete.
pub fn run_refund(
    config: &Configuration,
    inputs: &SwapInputs,
    chain: &mut Chain<'_>,
    timeout: u64,
) -> Result<(), SwapError> {
    let scheme: &dyn AdaptorScheme = config.scheme.as_ref();

    let funded = chain.coinbase(COIN, Condition::swap(&inputs.pk2, &inputs.pk1, timeout));

    let mut refund = Tx::new(0);
    refund.add_input(funded, true).add_output(COIN, Condition::pay_to(&inputs.pk1));
    let sighash = refund.sighash();

    // A genuine signature under u_1's key, so the early attempt fails for the
    // *timing* reason rather than because the signature was invalid.
    refund.inputs[0].sig = sign_ordinary(scheme, &sighash, inputs).ok_or(SwapError::PreSignFailed)?;

    match chain.submit(&refund) {
        Err(TxError::Timeout) => {}
        Err(e) => return Err(SwapError::Settlement("refund (early)", e)),
        Ok(()) => return Err(SwapError::Atomicity("refund succeeded before its timeout")),
    }

    chain.mine(timeout);
    chain.submit(&refund).map_err(|e| SwapError::Settlement("refund (after timeout)", e))?;
    if chain.balance(&inputs.pk1) != COIN {
        return Err(SwapError::Atomicity("refund did not return the coin to u1"));
    }
    Ok(())
}

/// Produce an ordinary (non-adaptor) signature under `u_1`'s key.
///
/// The refund branch is a plain spend, so it needs a plain signature. The
/// adaptor interface exposes no `Sign`, so this pre-signs and immediately
/// adapts with a witness `u_1` already holds. The result is an ordinary
/// signature *by construction* — that is exactly `Adapt`'s contract in the
/// paper, and it holds for the ECDSA adaptor too.
fn sign_ordinary(scheme: &dyn AdaptorScheme, msg: &[u8], inputs: &SwapInputs) -> Option<Vec<u8>> {
    let presig = scheme.presign(msg, &inputs.statement, &inputs.pk1, &inputs.sk1)?;
    scheme.adapt(&presig, msg, &inputs.statement, &inputs.witness, &inputs.pk1)
}
