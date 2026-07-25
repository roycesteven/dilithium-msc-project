# Stage 2 — atomic swap on Bitcoin/UTXO: implementation plan

**Authority:** `las-context-consolidated.md` §16 (Meeting-7 directives, 2026-07-24) ·
transcript `meeting7_cleaned_transcript.md` · survey
`docs/04-evaluation/CLASSICAL_ADAPTOR_ONCHAIN_SURVEY.md`.
**Status:** plan only. Nothing here has been built or measured. No numbers in this
document are measurements.

---

## 1. What changed

Stage 2's application target moves from a smart-contract chain to **Bitcoin / a
UTXO-based chain**. The EVM results already obtained (native LAS verification
≈56.5 M gas, above the EIP-7825 per-transaction cap of 16,777,216; the Naysayer
optimistic variant) are **retained as the evidence for why** — not discarded. The EVM
route is deferred to "if we have time".

The deliverable is **three configurations of the same swap, benchmarked against each
other** on time and communication cost.

---

## 2. Critical clarification: there are TWO distinct ZKP roles

These are routinely conflated and must not be. Wang's "replace Groth16 with a
post-quantum ZKP" concerns **role A only**.

| | **Role A — proof of knowledge in the swap protocol** | **Role B — succinct proof of on-chain verification** |
|---|---|---|
| What it proves | that a party knows the witness behind the statement `Y` (LAS paper §4.1, Fig. 1's π) | that an LAS verification *was performed correctly*, so a chain need not re-run it |
| Where it runs | **off-chain**, between the two parties | on-chain verifier checks it |
| Classical instantiation | Groth16 (as used by existing swap implementations, e.g. for cross-group discrete-log-equality) | Groth16/SNARK |
| This project's PQ instantiation | **LaZer** — already implemented (`ref/relation_zk*.{c,h}`, `rust/fips204-las/src/relation_zk.rs`, ≈30.7 kB measured) | **`rust/las-stark/`** — Winterfell FRI-STARK scaffold (hash-based, no pairings) |
| Meeting-7 status | **in scope** — this is what the three configurations vary | **deferred** with the EVM path |

Consequence: `rust/las-stark/` is *not* on the Meeting-7 critical path. It remains the
documented fix for the EVM deployability problem should that path resume. It is
correctly built as a standalone crate so it cannot disturb the KAT-locked
`rust/fips204-las` package.

---

## 3. The three configurations

Reuse an existing maintained swap implementation's *architecture* and replace its
cryptography. Wang's ordering is mandatory: **signatures first, then the ZKP.**

| # | Signature | Role-A ZKP | What it isolates |
|---|---|---|---|
| 1 | classical adaptor (ECDSA) | Groth16 | the classical reference point |
| 2 | **LAS** | Groth16 | the cost of the post-quantum **signature** alone |
| 3 | **LAS** | **LaZer** | the fully post-quantum stack |

Configuration 2 is the load-bearing one methodologically: without it, any difference
between 1 and 3 cannot be attributed between the signature and the proof system.

---

## 4. What already exists and carries over

Substantially more than a from-scratch estimate suggests:

| Asset | Where | Reuse in Stage 2 |
|---|---|---|
| LAS scheme, C | `ref/las.{c,h}`, `ref/basesig.{c,h}` | configurations 2 and 3 |
| LAS scheme, Rust (KAT-locked) | `rust/fips204-las/` | configurations 2 and 3 if the host repo is Rust |
| Classical ECDSA adaptor | vendored `third_party/secp256k1-zkp` @ `95b9835` (`ecdsa_adaptor`) | configuration 1 — the same primitive the real implementations use |
| π over LaZer | `ref/relation_zk*.{c,h}` + Rust twin, cargo feature `relation-zk` | configuration 3's role-A proof |
| Fig. 1 swap protocol, verbatim | `ref/test/test_swap.c` | the message schedule and abort gates transfer directly |
| Simulated ledger + HTLC | `ref/chain.{c,h}`, `ref/test/test_pcn.c` | the *model* to be replaced by a real UTXO backend |
| Wire encoding | `ref/serialize.{c,h}` | communication-cost measurement |
| Off-chain classical baseline | `bench_classical.c`, `docs/LAS.md §8.3` | already-measured per-op classical costs |

**The genuinely new work** is therefore: a real UTXO chain backend (in place of
`chain.c`'s simulated ledger), the host repo's protocol driver rewired to LAS, and a
Groth16 role-A proof for configuration 2.

---

## 5. Chain choice

Wang's steer: **avoid Monero** — its privacy features add complexity irrelevant to this
project — and prefer **two similar UTXO chains**. Recommended: **Bitcoin regtest on
both legs** (two independent regtest nodes standing in for two chains). Rationale:

- It satisfies "two similar chains" maximally — they are identical, so no cross-group
  discrete-log-equality proof is needed, which removes a confound that has nothing to
  do with post-quantum signatures.
- Regtest is free, instant, and fully local, so no testnet coin acquisition and no
  deployment to a live network (Wang explicitly ruled out mainnet: "it's very expensive
  to deploy things there").
- It keeps the measured quantity clean: the difference between configurations is the
  cryptography, not the two chains' differing consensus rules.

If a genuinely heterogeneous pair is wanted later, Litecoin regtest is the
closest-to-Bitcoin option. Note that the leading real implementations
(`eigenwallet/core`, `farcaster-project`) are Bitcoin↔**Monero**, so their cross-group
proof machinery is exactly the part to *not* reuse.

---

## 6. Measurement plan

Gas does not exist here, so per §16.3 the axes are **time** and **communication cost**.

- **Time:** per protocol phase (setup, pre-sign, pre-verify, adapt, extract, settle),
  reported per operation, consistent with the Stage-1 rule that per-operation timing is
  primary and cumulative timing is never the headline.
- **Communication:** every off-chain message counted, not just what reaches the chain —
  this is where the post-quantum cost concentrates, and where π's ≈30.7 kB dominates.
  Report per-message and per-swap totals.
- **Two tiers, as in Stage 1:** core and packed. Packing in the swap path is optional
  per §16.4; if omitted, say so and record it as a limitation rather than leaving the
  boundary undeclared.
- **Usability finding:** Wang asked for the consequence to be discussed — whether the
  pre-transaction computation is feasible on a phone or needs a dedicated PC.
- **Keep the existing gate:** any run that exercises sign/pre-sign must still assert
  attempts-per-call against the closed-form prediction (2.719 Sign / 2.775 PreSign at
  the target setting). Do not weaken or rename that gate.

---

## 7. Sequencing

1. Select and vendor the host repo; record its commit hash (reproducibility rule).
2. Stand up two Bitcoin regtest nodes and get configuration 1 settling end-to-end
   *unmodified*, as the control. Confirm whether its Ext is implemented — some demos
   stop at Adapt (Meeting-7 item 11).
3. Benchmark configuration 1. This is the baseline every later number is read against.
4. Replace the signature with LAS → configuration 2. Keep Groth16 untouched.
5. Benchmark configuration 2; attribute the delta to the signature.
6. Replace the role-A proof with LaZer → configuration 3.
7. Benchmark configuration 3; attribute the delta to the proof system.
8. Honest path only. Refund/timeout are edge cases (§16.4) — add them only if time
   allows, with the invariant that a dishonest counterparty must not cause the honest
   party to lose funds.

---

## 8. Risks and open decisions

| Risk | Assessment |
|---|---|
| **Bitcoin Script cannot verify an LAS signature.** | Not required, and this is the whole reason the venue was chosen: in a scriptless swap the chain verifies an *ordinary* signature. But it means the **settlement signature on-chain must remain simplified ML-DSA** even in configurations 2 and 3 — LAS secures the *adaptor* layer off-chain. |
| Configuration 2 needs a Groth16 circuit for role A over a *lattice* statement. | Potentially expensive — proving a lattice relation in a pairing-based SNARK is not a natural fit. If it proves impractical, report that as a finding (it is a real result about mixing classical proofs with PQ signatures) rather than forcing it. |
| Host repo is Rust; the implementation of record is C. | The Rust port is KAT-locked and byte-identical, so either is defensible. Prefer Rust for a Rust host to avoid an FFI layer, and state which implementation produced the numbers. |

---

## 9. Explicitly out of scope for this stage

Live testnet/mainnet deployment · EVM/Solidity work (deferred) · real network sockets
(pass messages directly, §16.4) · on-chain π · AMHL multi-hop (already done, bonus) ·
parameter migration to q≈2²⁴.
