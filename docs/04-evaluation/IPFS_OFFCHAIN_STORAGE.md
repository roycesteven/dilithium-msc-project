# Off-chain storage (IPFS) as a fallback — what it buys, and what it costs

**Status:** documentation only, as Meeting 8 (2026-07-31) directed. Nothing here is
implemented, measured, or claimed as a project artefact. It is written so the option is
*assessed* rather than left as an unexamined suggestion, and so the reasons it is **not**
the answer for this project are on the record.

**Supervisor framing.** Meeting 8 listed "IPFS-style off-chain storage, documented as a
fallback with its bridge/maintenance caveats" alongside the hint experiment. It is a
fallback for the *on-chain verification* route, not for the swap.

---

## 1. First, the part that is already off-chain

The single most important thing to state, because it is easy to get backwards:

> **The atomic swap does not need off-chain storage.** Its proof of knowledge π
> (≈31 kB) is exchanged **directly between the two parties** as a protocol message, as
> eprint 2020/845 §4.1 specifies. The statement `Y`, the witness `y` and π never touch a
> chain in either venue (see `docs/02-methodology/BITCOIN_TX_STRUCTURE.md` §5 and
> `EVM_TX_STRUCTURE.md` §3). There is no availability requirement to solve, because
> there is no third party who needs to read them.

So IPFS has nothing to offer the deliverable of record. It becomes relevant only for a
design where a large object must be **publicly** available:

| candidate object | size | who needs to read it | needs public availability? |
|---|---|---|---|
| π (swap) | ≈31 kB | the counterparty only | **no** — sent directly |
| adapted signature σ | 6,736 B | consensus / the verifier contract | **yes**, but it *is* the payment authorisation; it cannot be a pointer |
| public parameters `A′`, key `t` | large | the verifier contract, on the verified path | **yes** — a genuine candidate |
| a fraud-proof witness (Naysayer) | large | any potential challenger | **yes** — the hard case, see §4 |

Only the last two are real candidates, and both belong to the EVM route that Meeting 7
already demoted.

## 2. How the fallback would work

Standard content-addressed pattern:

1. The large object is published to IPFS; its CID is the multihash of the content.
2. The chain stores **only the CID** (32–36 bytes) or an equivalent `keccak256`
   commitment, at ~20,000 gas for a cold `SSTORE` plus calldata.
3. Anyone who needs the object fetches it by CID and re-hashes to check integrity.

The saving is genuine and arithmetic: replacing a multi-kilobyte calldata argument with
a 32-byte commitment removes ~16 gas per non-zero byte (EIP-2028). Against the measured
`claimLAS` calldata floor of 290,640 gas, moving the *signature* off-chain would recover
most of it — which is exactly why the idea is tempting and exactly why it is wrong for
the signature (see §3).

## 3. Why it does not apply to the signature itself

A signature is not reference data; it is the authorisation. Replacing σ with a CID means
the chain no longer holds the thing that authorises the spend, so:

- the contract cannot verify what it cannot see, and
- a spend becomes valid only *conditionally* on data the chain does not have.

The chain would be accepting a promise. That is not a storage optimisation, it is a
change to the trust model.

## 4. The caveat that decides it: availability is relocated, not solved

This is the substantive objection, and it is sharpest exactly where the saving is
largest — the **optimistic (Naysayer) verifier**.

An optimistic scheme is sound only if a challenger *can construct the fraud proof during
the challenge window*. If the data needed to construct it lives on IPFS and is not
retrievable in time — because nobody pinned it, because the pinning service expired,
because the publisher withheld it deliberately — then no valid challenge can be made, and
a fraudulent claim finalises. **Withholding the data becomes the attack.** This is the
classical data-availability problem, and IPFS does not solve it:

- **IPFS is not storage; it is addressing.** A CID proves *what* the content is if you
  already have it. It provides no guarantee that any node still holds it. Unpinned
  content is garbage-collected.
- **Pinning is a paid service with an operator.** Someone must be paid to keep the data
  alive for at least the challenge window, in practice much longer. That reintroduces
  the "who pays to keep this available" question the chain's own fee market answers by
  construction.
- **Retrieval latency is unbounded** and interacts badly with a fixed challenge period.
- **A bridge or oracle is a second trust assumption.** If the contract must *react* to
  off-chain content, something has to attest to it on-chain. That attester becomes part
  of the security argument, and it is not post-quantum-relevant, it is simply
  additional trust — in a project whose entire point is removing trust assumptions
  (LaZer over Groth16 was chosen partly to avoid a trusted setup).
- **A second platform is a second maintenance burden**: another dependency, another
  failure mode, another thing a reviewer must audit.

Purpose-built data-availability layers (erasure-coded DA with sampling) exist precisely
because content addressing alone is insufficient here. Assessing them is well outside
this project's scope.

## 5. Verdict

- **For the swap (the deliverable of record): not applicable.** π is already a direct
  protocol message; there is nothing to relocate.
- **For the on-chain verification route: a real saving, bought with a trust assumption
  the project would otherwise not have** — and, for the optimistic variant specifically,
  one that can convert an availability failure into a soundness failure.
- **Therefore it is recorded as a fallback**, not adopted, and the report's future-work
  entry states the saving *and* the relocation together. Presenting the saving alone
  would be the overclaim.

---

## 6. Sources

- eprint 2020/845 §4.1 — π is exchanged between the parties, not published.
- EIP-2028 (calldata gas), EIP-7825 (per-transaction cap) — the arithmetic in §2.
- Measured gas: `evidence/onchain/latest/gas_report.log`.
- IPFS content addressing and pinning semantics — <https://docs.ipfs.tech/>.
