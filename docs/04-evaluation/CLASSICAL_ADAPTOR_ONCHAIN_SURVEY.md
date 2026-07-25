# Classical adaptor signatures on smart-contract chains — preparatory survey

**Status:** literature/desk survey, 2026-07-25. **No new measurement.** Every number
below is either (a) cited to an external source, or (b) a figure already measured in
this project and recorded in [`docs/03-results/GAS_LIMIT_INVESTIGATION.md`](../03-results/GAS_LIMIT_INVESTIGATION.md).
The two are labelled distinctly throughout.

**Why this document exists.** Meeting-7 action item 6 (Wang, 2026-07-24): before doing
any further EVM work, check *how classical adaptor signatures / atomic swaps are
implemented in Solidity, and at what cost*, so that a future post-quantum-on-EVM
measurement has a baseline to be compared against. Wang's words: "I'm quite curious to
see their performance… then we have a benchmark for the future."

---

## 1. Headline answer

**There is essentially no classical adaptor-signature *verifier* in Solidity to
benchmark against, and that absence is structural rather than a gap in the
literature.** In a scriptless atomic swap the adaptor machinery is deliberately
confined to the off-chain protocol; what finally reaches the chain is an *ordinary*
signature that the chain validates with its ordinary rules. The chain never learns a
statement `Y`, never runs PreVerify, and never runs Adapt.

This is the whole point of the design. "Scriptless scripts" moves contract logic off
the chain and into a conversation between the participants, so the settlement
transaction is indistinguishable from a plain payment
([Blockstream Research, scriptless-scripts](https://github.com/BlockstreamResearch/scriptless-scripts/blob/master/md/atomic-swap.md);
[Tari Labs, Introduction to Scriptless Scripts](https://tlu.tarilabs.com/cryptography/introduction-to-scriptless-scripts)).
Adaptor signatures tie the publication of a signature to the leakage of a secret
([Komodo Academy](https://komodoplatform.com/en/academy/adaptor-signatures-and-scriptless-atomic-swaps/)),
and it is precisely that leakage — not any on-chain verification — that makes the swap
atomic.

**Consequence for this project.** The comparison "classical adaptor verification on
chain vs LAS adaptor verification on chain" is not a meaningful benchmark, because the
classical side of it does not exist and does not need to. The *meaningful* baseline is
narrower and much more damaging, and it is given in §3: what it costs the EVM to verify
the one ordinary signature that settlement does require.

---

## 2. Where the classical implementations actually live

Consistently, in **off-chain daemons over UTXO chains**, in Rust or C, not in smart
contracts:

| Artefact | What it is | Notes |
|---|---|---|
| [`BlockstreamResearch/secp256k1-zkp`](https://github.com/BlockstreamResearch/secp256k1-zkp) | C; the `ecdsa_adaptor` module | **Already vendored in this project** at commit `95b9835` and used for the classical baseline in `bench_classical.c`. This is the de-facto reference implementation of the primitive. |
| [`rust-bitcoin/rust-secp256k1`](https://github.com/rust-bitcoin/rust-secp256k1) | Rust bindings; ECDSA adaptor discussed/implemented via [issue #292](https://github.com/rust-bitcoin/rust-secp256k1/issues/292), following *Generalized Bitcoin-Compatible Channels* | Library, off-chain. |
| [`eigenwallet/core`](https://github.com/eigenwallet/core) | Rust monorepo; "battle-tested Monero–Bitcoin DEX based on Atomic Swaps" | **The maintained successor to `comit-network/xmr-btc-swap`, which is deprecated.** Active (~3.9k commits). Crates: `swap`, `swap-core`, `swap-p2p`, `bitcoin-wallet`, `monero-wallet`. |
| [`farcaster-project`](https://github.com/farcaster-project) | Rust; BTC–XMR peer-to-peer swap protocol | Grant-funded ([Monero CCS](https://ccs.getmonero.org/proposals/h4sh3d-atomic-swap-implementation.html)); check current activity before relying on it. |
| [`h4sh3d/xmr-btc-atomic-swap`](https://github.com/h4sh3d/xmr-btc-atomic-swap) | Protocol spec + draft | Requires cross-group discrete-log-equality proofs (ed25519 ↔ secp256k1) and ECDSA one-time VES. Useful as protocol documentation. |

On the research side, ECDSA-based adaptor signatures are an active topic — e.g.
[*Efficient ECDSA-based Adaptor Signature for Batched Atomic Swaps*](https://eprint.iacr.org/2024/140),
which notes that a provably secure ECDSA adaptor needs a zero-knowledge proof in
pre-signing and optimises by batching those proofs offline; and
[*Universal Atomic Swaps*](https://eprint.iacr.org/2021/1612.pdf). Neither puts a
verifier on chain.

**Cross-check against this project's own finding.** This matches what Royce reported in
Meeting 7 and what Wang stated from his own knowledge: adaptor signatures are used for
swaps on Bitcoin and other UTXO chains rather than on smart-contract chains. The
survey therefore *corroborates* the Meeting-7 decision to retarget Stage 2 to
Bitcoin/UTXO; it is not merely consistent with it.

---

## 3. The cost baseline that does matter

Since the chain only ever verifies an ordinary signature, the honest baseline is the
cost of that verification on the EVM:

| What | Gas | Source |
|---|---|---|
| Classical ECDSA signature verification via the **`ecRecover` precompile** | **3,000** | EVM precompile pricing (`ECRECOVER`, address `0x01`) |
| Classical ECDSA verification implemented **in Solidity instead of the precompile** | ≈827,766 (reported mean) | [On the Practicality of Smart Contract PKI](https://arxiv.org/pdf/1902.00878) — external figure, not measured here |
| **This project, measured:** one full on-chain LAS verified settlement (`claimLASVerified`) | **56,538,682** | `GAS_LIMIT_INVESTIGATION.md` — measured on a local EVM |
| **This project, measured:** optimistic (Naysayer) honest claim | ≈1.1 M | ditto |
| **This project, measured:** Naysayer worst case — SHAKE256 hash dispute | ≈29.4 M | ditto |
| **This project, measured:** Naysayer worst case — matrix commitment dispute | ≈13.9 M execution (+ ≈246 kB calldata) | ditto |
| EIP-7825 (Pectra) **per-transaction** gas cap | **16,777,216** (2²⁴) | Ethereum protocol |

**The gap, stated precisely.** Native LAS verification costs roughly **18,800×** the
`ecRecover` precompile (56,538,682 / 3,000), and exceeds the per-transaction cap by
about **3.4×**. Even the Solidity-implemented classical verifier — itself considered
impractically expensive, which is *why* `ecRecover` is a precompile — is about **68×
cheaper** than the LAS verifier.

**The correct interpretation, and the one to put in the report.** This is a
*missing-precompile* gap, not an algorithmic defect of LAS. Classical ECDSA is cheap
on-chain for exactly one reason: Ethereum ships a subsidised native implementation of
it. The comparison is therefore between an algorithm with protocol-level support and
one without, and the ~18,800× figure measures the value of that support rather than
the intrinsic cost of lattice verification. The structural blocker is the same one
already identified in this project: SHAKE256 is not an EVM-native hash. Any honest
report framing must say this, otherwise the number reads as an indictment of LAS when
it is really an argument for a precompile (or for choosing a chain that does not
meter execution at all).

---

## 4. What this settles, and what it leaves open

**Settled.**
- There is no classical Solidity adaptor-signature verifier to benchmark against; the
  classical comparison at the *application* level therefore belongs off-chain, where
  this project has already measured it (`bench_classical`, `docs/LAS.md §8.3`).
- The EVM baseline for on-chain settlement is `ecRecover` at 3,000 gas, and it is the
  right anchor for the future PQ-on-EVM comparison Wang asked for.
- The survey independently supports retargeting Stage 2 to Bitcoin/UTXO.

**Open — and each is a genuine measurement, not a literature question.**
1. What a *classical* scriptless swap costs end-to-end on an EVM chain in practice
   (two ordinary transfers plus signature checks), as the denominator for a PQ swap.
2. Whether a Merkle-opening variant of the matrix dispute brings the Naysayer worst
   case under the 2²⁴ cap — the most promising remedy identified so far.
3. Whether any chain in scope offers, or plans, a lattice-signature precompile; if one
   does, the ~18,800× gap collapses and the deployability conclusion changes.

**Deliberately not pursued.** Building a Solidity classical adaptor verifier purely to
have something to compare against would be inventing a strawman: no deployed system
does this, so its gas figure would describe nothing real.

---

## 5. Sources

- [Blockstream Research — scriptless-scripts, atomic-swap](https://github.com/BlockstreamResearch/scriptless-scripts/blob/master/md/atomic-swap.md)
- [Tari Labs — Introduction to Scriptless Scripts](https://tlu.tarilabs.com/cryptography/introduction-to-scriptless-scripts)
- [Komodo Academy — Adaptor Signatures and Scriptless Atomic Swaps](https://komodoplatform.com/en/academy/adaptor-signatures-and-scriptless-atomic-swaps/)
- [Efficient ECDSA-based Adaptor Signature for Batched Atomic Swaps (eprint 2024/140)](https://eprint.iacr.org/2024/140)
- [Universal Atomic Swaps (eprint 2021/1612)](https://eprint.iacr.org/2021/1612.pdf)
- [JugglingSwap: Scriptless Atomic Cross-Chain Swaps (arXiv 2007.14423)](https://arxiv.org/pdf/2007.14423)
- [On the Practicality of Smart Contract PKI (arXiv 1902.00878)](https://arxiv.org/pdf/1902.00878)
- [eigenwallet/core](https://github.com/eigenwallet/core) · [farcaster-project](https://github.com/farcaster-project) · [rust-secp256k1 #292](https://github.com/rust-bitcoin/rust-secp256k1/issues/292) · [h4sh3d/xmr-btc-atomic-swap](https://github.com/h4sh3d/xmr-btc-atomic-swap)
- [Monero CCS — atomic swap implementation funding](https://ccs.getmonero.org/proposals/h4sh3d-atomic-swap-implementation.html)
