//! A minimal but faithful UTXO ledger — the venue eprint 2020/845 Section 4
//! assumes for its applications:
//!
//! > "we assume an Unspent Transaction Output (UTXO)-based blockchain like
//! > Bitcoin **where the signature algorithm is replaced with a lattice-based
//! > signature scheme given in Algorithm 1**. In the UTXO model, coins are kept
//! > in addresses where each address consists of the amount and the spending
//! > condition. The spending condition is defined by the scripting language and
//! > the most common ones are signature and hash preimage verifications, and
//! > timing conditions."
//!
//! This module models exactly that and nothing more: coins live in outputs, an
//! output carries an amount plus a spending condition, and a condition is a
//! signature check with an optional timelocked refund branch. Hash-preimage
//! conditions are deliberately absent — the point of the scriptless
//! construction is that the hash lock is replaced by the adaptor statement `Y`,
//! which never appears on chain.
//!
//! # Why the signature algorithm is a parameter
//!
//! The emphasised clause above is load-bearing for this project. A chain
//! configured with ECDSA is the classical baseline; the same chain configured
//! with Algorithm 1 (`fips204::basesig`) is the post-quantum setting. Holding
//! the ledger fixed and varying only [`AdaptorScheme::verify`] is what makes any
//! measured difference between the three configurations attributable to the
//! cryptography rather than to the ledger.
//!
//! # Scope
//!
//! An in-process model, not a consensus implementation: no p2p, no mempool
//! policy, no fee market, no reorgs, no script interpreter. It exists so the
//! protocol runs against real transaction objects, and to expose the one
//! mechanism the swap depends on — that a published spending signature is
//! readable by anyone ([`Chain::spending_signature`]), which is how the witness
//! leaks in Fig. 1.

use crate::backend::AdaptorScheme;

/// Errors a node can reject a transaction with. The driver distinguishes these:
/// a refund attempted early must fail as [`TxError::Timeout`] (a timing
/// failure), never as [`TxError::BadSignature`] (a cryptographic one).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TxError {
    /// An input is missing, already spent, or names a nonexistent branch.
    BadInput,
    /// Outputs exceed inputs.
    ValueOverflow,
    /// Transaction locktime has not been reached.
    Locktime,
    /// The timelocked refund branch was taken before its timeout.
    Timeout,
    /// The chain's signature algorithm rejected an input's signature.
    BadSignature,
}

/// A reference to a previous output: `(txid, index)`.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct OutPoint {
    pub txid: [u8; 32],
    pub vout: u32,
}

/// The spending condition of an output (paper §4: signature + timing).
///
/// * before `timeout` — spendable by a signature under `pk`
/// * at/after `timeout` — *also* spendable by a signature under `refund_pk`
///
/// In the swap, `pk` is the counterparty's key (the claim branch, pre-authorised
/// by the adaptor pre-signature) and `refund_pk` is the funder's own key.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct Condition {
    pub pk: Vec<u8>,
    /// Empty means there is no refund branch.
    pub refund_pk: Vec<u8>,
    pub timeout: u64,
}

impl Condition {
    /// A plain "pay to this key" output with no refund branch.
    pub fn pay_to(pk: &[u8]) -> Self {
        Self { pk: pk.to_vec(), refund_pk: Vec::new(), timeout: 0 }
    }

    /// The swap output: claimable by `pk`, refundable by `refund_pk` from
    /// `timeout` onward.
    pub fn swap(pk: &[u8], refund_pk: &[u8], timeout: u64) -> Self {
        Self { pk: pk.to_vec(), refund_pk: refund_pk.to_vec(), timeout }
    }
}

#[derive(Debug, Clone)]
pub struct TxOut {
    pub value: u64,
    pub cond: Condition,
    pub spent: bool,
}

/// An input together with the signature authorising it. The signature is stored
/// on chain and is what [`Chain::spending_signature`] returns — the public leak
/// that makes the swap atomic.
#[derive(Debug, Clone)]
pub struct TxIn {
    pub prev: OutPoint,
    pub sig: Vec<u8>,
    /// `true` when spending via the timelocked refund branch.
    pub refund_branch: bool,
}

#[derive(Debug, Clone, Default)]
pub struct Tx {
    pub txid: [u8; 32],
    pub inputs: Vec<TxIn>,
    pub outputs: Vec<TxOut>,
    /// Not valid before this height.
    pub locktime: u64,
    /// Height at which it was accepted (set by the chain).
    pub height: u64,
}

impl Tx {
    pub fn new(locktime: u64) -> Self {
        Self { locktime, ..Default::default() }
    }

    pub fn add_input(&mut self, prev: OutPoint, refund_branch: bool) -> &mut Self {
        self.inputs.push(TxIn { prev, sig: Vec::new(), refund_branch });
        self
    }

    pub fn add_output(&mut self, value: u64, cond: Condition) -> &mut Self {
        self.outputs.push(TxOut { value, cond, spent: false });
        self
    }

    /// The bytes a signature commits to: a canonical serialisation of the
    /// transaction **with the input signatures excluded** (Bitcoin's sighash
    /// idea, reduced to essentials). This is Fig. 1's `tx_1` / `tx_2` — the
    /// message passed to `PreSign`.
    ///
    /// Fixed-width little-endian fields keep the encoding canonical and
    /// host-independent. The spending condition is committed to in full: a
    /// signature authorises specific outputs with specific keys and timeouts,
    /// never merely an amount.
    pub fn sighash(&self) -> Vec<u8> {
        let mut b = Vec::new();
        b.extend_from_slice(&self.locktime.to_le_bytes());
        b.extend_from_slice(&(self.inputs.len() as u32).to_le_bytes());
        for i in &self.inputs {
            b.extend_from_slice(&i.prev.txid);
            b.extend_from_slice(&i.prev.vout.to_le_bytes());
            b.push(u8::from(i.refund_branch));
        }
        b.extend_from_slice(&(self.outputs.len() as u32).to_le_bytes());
        for o in &self.outputs {
            b.extend_from_slice(&o.value.to_le_bytes());
            b.extend_from_slice(&o.cond.timeout.to_le_bytes());
            b.extend_from_slice(&(o.cond.pk.len() as u32).to_le_bytes());
            b.extend_from_slice(&o.cond.pk);
            b.extend_from_slice(&(o.cond.refund_pk.len() as u32).to_le_bytes());
            b.extend_from_slice(&o.cond.refund_pk);
        }
        b
    }

    /// The transaction's full wire encoding: everything [`Tx::sighash`] commits
    /// to, followed by a length-prefixed signature per input.
    ///
    /// This — not the sighash — is what a party actually transmits and what a
    /// chain actually stores. An unsigned template serialises with zero-length
    /// signatures, so the same function serves both the off-chain exchange of
    /// `tx_1`/`tx_2` and the settled transaction.
    ///
    /// Keeping it distinct from `sighash()` matters for the measurement: the
    /// sighash is a *signing message* (for ECDSA, ultimately a 32-byte digest),
    /// and reporting its length as the size of a transmitted transaction would
    /// understate communication cost and mean different things per scheme.
    pub fn serialize(&self) -> Vec<u8> {
        let mut b = self.sighash();
        for i in &self.inputs {
            b.extend_from_slice(&(i.sig.len() as u32).to_le_bytes());
            b.extend_from_slice(&i.sig);
        }
        b
    }

    /// Byte length of the transaction as stored on chain.
    pub fn onchain_bytes(&self) -> usize {
        self.serialize().len()
    }

    /// `txid = SHAKE256(serialised transaction)`, 32 bytes. Signatures are
    /// included, so this identifies the *settled* transaction rather than the
    /// unsigned template — two different adapted signatures over one template
    /// are two different transactions.
    fn compute_txid(&self) -> [u8; 32] {
        shake256_32(&self.serialize())
    }
}

/// 32-byte SHAKE256. Uses the same `sha3` 0.10 crate the LAS port already
/// depends on, so the ledger introduces no new hash implementation.
fn shake256_32(data: &[u8]) -> [u8; 32] {
    use sha3::digest::{ExtendableOutput, Update, XofReader};
    let mut hasher = sha3::Shake256::default();
    hasher.update(data);
    let mut out = [0u8; 32];
    hasher.finalize_xof().read(&mut out);
    out
}

/// A single UTXO chain.
///
/// `block_seconds` is reporting metadata only — it lets two chains differ the
/// way Bitcoin and Litecoin do (600 s vs 150 s) so timeouts can be quoted in
/// wall-clock terms. Nothing in validation depends on it.
pub struct Chain<'a> {
    pub name: String,
    pub height: u64,
    pub txs: Vec<Tx>,
    pub block_seconds: u32,
    /// Total bytes of transaction data this chain has accepted.
    pub onchain_bytes: usize,
    /// The chain's signature algorithm (paper §4).
    scheme: &'a dyn AdaptorScheme,
}

impl<'a> Chain<'a> {
    pub fn new(name: &str, scheme: &'a dyn AdaptorScheme, block_seconds: u32) -> Self {
        Self {
            name: name.to_string(),
            height: 1,
            txs: Vec::new(),
            block_seconds,
            onchain_bytes: 0,
            scheme,
        }
    }

    pub fn mine(&mut self, blocks: u64) {
        self.height += blocks;
    }

    /// Create coins out of thin air (a coinbase) so a party has something to
    /// swap. Returns the new outpoint.
    pub fn coinbase(&mut self, value: u64, cond: Condition) -> OutPoint {
        let mut tx = Tx::new(0);
        tx.add_output(value, cond);
        tx.height = self.height;
        tx.txid = tx.compute_txid();
        let op = OutPoint { txid: tx.txid, vout: 0 };
        self.txs.push(tx);
        op
    }

    fn find_output(&self, op: &OutPoint) -> Option<(usize, usize)> {
        self.txs.iter().position(|t| t.txid == op.txid).and_then(|ti| {
            if (op.vout as usize) < self.txs[ti].outputs.len() {
                Some((ti, op.vout as usize))
            } else {
                None
            }
        })
    }

    /// Validate and apply `tx`.
    ///
    /// Checks in node order: locktime, inputs exist and are unspent, the refund
    /// branch is not taken early, value conservation, and finally the chain's
    /// signature verifier over [`Tx::sighash`]. On success the inputs are marked
    /// spent and the transaction's outputs become spendable.
    pub fn submit(&mut self, tx: &Tx) -> Result<(), TxError> {
        if tx.locktime > self.height {
            return Err(TxError::Locktime);
        }

        let sighash = tx.sighash();
        let mut located = Vec::with_capacity(tx.inputs.len());
        let mut in_value: u64 = 0;

        for txin in &tx.inputs {
            let (ti, oi) = self.find_output(&txin.prev).ok_or(TxError::BadInput)?;
            let out = &self.txs[ti].outputs[oi];
            if out.spent {
                return Err(TxError::BadInput);
            }
            if txin.refund_branch {
                if out.cond.refund_pk.is_empty() {
                    return Err(TxError::BadInput);
                }
                if self.height < out.cond.timeout {
                    return Err(TxError::Timeout);
                }
            }
            in_value = in_value.checked_add(out.value).ok_or(TxError::ValueOverflow)?;
            located.push((ti, oi));
        }

        let out_value: u64 = tx.outputs.iter().map(|o| o.value).sum();
        if out_value > in_value {
            return Err(TxError::ValueOverflow);
        }

        // The chain's signature algorithm — the only scheme-dependent step.
        for (txin, &(ti, oi)) in tx.inputs.iter().zip(located.iter()) {
            let cond = &self.txs[ti].outputs[oi].cond;
            let pk = if txin.refund_branch { &cond.refund_pk } else { &cond.pk };
            if txin.sig.is_empty() || !self.scheme.verify(&txin.sig, &sighash, pk) {
                return Err(TxError::BadSignature);
            }
        }

        for &(ti, oi) in &located {
            self.txs[ti].outputs[oi].spent = true;
        }
        let mut accepted = tx.clone();
        accepted.height = self.height;
        accepted.txid = accepted.compute_txid();
        self.onchain_bytes += accepted.onchain_bytes();
        self.txs.push(accepted);
        Ok(())
    }

    /// Total unspent value payable to `pk` via the primary (non-refund) branch.
    pub fn balance(&self, pk: &[u8]) -> u64 {
        self.txs
            .iter()
            .flat_map(|t| t.outputs.iter())
            .filter(|o| !o.spent && o.cond.pk == pk)
            .map(|o| o.value)
            .sum()
    }

    /// Recover the signature that spent `prev`, if any.
    ///
    /// **This is the leak Fig. 1 relies on.** Once `u_1` publishes `sigma_2` to
    /// claim `c_2`, `u_2` reads it back off the chain and runs `Ext`.
    pub fn spending_signature(&self, prev: &OutPoint) -> Option<Vec<u8>> {
        self.txs
            .iter()
            .flat_map(|t| t.inputs.iter())
            .find(|i| &i.prev == prev)
            .map(|i| i.sig.clone())
    }
}
