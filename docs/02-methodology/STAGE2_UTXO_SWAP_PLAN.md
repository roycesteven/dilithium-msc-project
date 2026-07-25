# Stage 2 — atomic swap on a UTXO chain: plan and implementation

**Authority:** `las-context-consolidated.md` §16 (Meeting-7 directives, 2026-07-24) ·
transcript `meeting7_cleaned_transcript.md` · survey
`docs/04-evaluation/CLASSICAL_ADAPTOR_ONCHAIN_SURVEY.md` · protocol
`2020-845.md` §4.1 Fig. 1.

**Status (2026-07-25):** **built, run, and measured.** All three configurations
completed end-to-end. Evidence: `evidence/stage2/20260725_202359/bench_swap.log`
(+ `environment.txt`). Report macros are generated from that log by
`scripts/gen_stage2_data.py`; the results are written up in
`report/latex/chapters/03-results.tex` §3.6. No number is hand-typed anywhere.

**Headline results** (10 swaps per configuration, packed tier, AMD Ryzen 7 7745HX):

| | classical (ECDSA) | LAS + Groth16 | LAS + LaZer |
|---|---|---|---|
| end-to-end | 1 754 µs | 1 102 691 µs | 517 800 µs |
| share spent on π | — | 99.2 % | 98.3 % |
| end-to-end excluding π | 1 754 µs | 8 976 µs | 9 011 µs |
| bytes / swap | 941 | 49 412 | 80 001–80 050 |
| π size | — | 128 B | 30 717–30 766 B |
| setup | 22 µs | 2.75 s, **trusted** | 0, transparent |
| fully post-quantum | no | no | **yes** |

Three findings:

1. **The proof, not the signature, is the cost.** π is 98–99 % of end-to-end. With
   π removed, the whole post-quantum signature workload is ≈5.1× the classical
   swap — the Stage-1 "adaptor layer is nearly free" result reproduced at the
   application level.
2. **The controlled 2→3 comparison inverts the expected trade-off.** LaZer is
   **53.0 % faster** end-to-end than Groth16 yet **61.9 % larger**, with a proof
   ≈240× bigger. Groth16 is succinct but pays a multi-scalar multiplication over the
   whole circuit; LaZer is not succinct but avoids encoding 23-bit modular
   arithmetic into a 254-bit pairing field. Verification runs the other way
   (29 ms vs 164 ms). Configuration 3 is therefore faster, needs no trusted party,
   and is post-quantum — paying only in bytes.
3. **Communication is again the real price** — ≈85× total, ≈63× on-chain.

Validation that the 2→3 comparison really is controlled: both configurations report
the identical relation binding (`33c1947f2ba13f98`) and spend 8 976 µs / 9 011 µs
outside the proof — within 0.4 %, as byte-identical inputs on one `A` predict.

---

## 1. What changed

Stage 2's application target moves from a smart-contract chain to a **UTXO-based
chain**. The EVM results already obtained — native LAS verification **56,538,682 gas
measured**, against the EIP-7825 per-transaction cap of 16,777,216 (≈3.4× over), plus
the Naysayer optimistic variant — are **retained as the evidence for why**, not
discarded. The EVM route is deferred to "if we have time".

> Figure provenance: `docs/03-results/GAS_LIMIT_INVESTIGATION.md` §5. An earlier
> ≈16.7 M figure was an op-budget *lower-bound estimate* and is superseded by the
> measured 56.5 M. Anywhere the ≈16.7 M number still appears, it is stale.

The deliverable is **three configurations of the same swap, benchmarked against each
other** on time and communication cost.

---

## 2. Critical clarification: there are TWO distinct ZKP roles

Routinely conflated, and must not be. Wang's "replace Groth16 with a post-quantum ZKP"
concerns **role A only**.

| | **Role A — proof of knowledge in the swap protocol** | **Role B — succinct proof of on-chain verification** |
|---|---|---|
| What it proves | that a party knows the witness behind the statement `Y` (Fig. 1's π) | that an LAS verification *was performed correctly*, so a chain need not re-run it |
| Where it runs | **off-chain**, between the two parties | on-chain verifier checks it |
| This project's PQ instantiation | **LaZer** — implemented (`ref/relation_zk*.{c,h}`, `rust/fips204-las/src/relation_zk.rs`) | **`rust/las-stark/`** — Winterfell FRI-STARK scaffold |
| Meeting-7 status | **in scope** — what the configurations vary | **deferred** with the EVM path |

`rust/las-stark/` is *not* on the Meeting-7 critical path.

---

## 3. The three configurations

| # | Signature | Role-A proof π | Post-quantum? |
|---|---|---|---|
| 1 | classical adaptor (ECDSA) | **none required** — see below | no |
| 2 | **LAS** | Groth16 over `∃r : A r = t ∧ ‖r‖∞ ≤ 1` | signature only |
| 3 | **LAS** | LaZer over `∃r : A r = t ∧ ‖r‖∞ ≤ 1` | fully |

### 3.1 Why configuration 1 carries no role-A proof

A classical ECDSA adaptor has **no knowledge gap**: `Ext` recovers the discrete log
exactly, so pre-signature adaptability is unconditional and Fig. 1's proof of witness
knowledge has nothing to rescue. Deployed classical swaps do not carry π, and adding
one would benchmark a construction nobody runs.

secp256k1-zkp's adaptor pre-signature *does* contain a DLEQ proof, and it must **not**
be reported as role-A π — different author, different claim:

| | Fig. 1's π (role A) | the construction's embedded DLEQ |
|---|---|---|
| author | `u₁`, the statement generator | the **pre-signer** |
| claim | "I know a witness to `Y`" | "this pre-signature is well formed w.r.t. `Y`" |
| protects | `u₂` against an unopenable statement | the verifier against a malformed pre-signature |
| counted as | a separate protocol message | part of the pre-signature's bytes |

The harness therefore reports **pre-signature overhead** (162 − 64 = 98 B for ECDSA;
**0 B for LAS**, whose pre-signature and signature share one wire layout) rather than
inventing a byte split the upstream header does not document.

### 3.2 What the configurations can and cannot attribute

- **2 → 3 is a controlled comparison.** Same signature, same `A`, same relation, and —
  because all inputs derive deterministically from one pinned seed — byte-identical
  keys, statement, witness and transactions. Only the prover changes, so the delta is
  attributable to the proof system. **This is the comparison to lead with.**
- **1 → 2/3 is a whole-stack comparison.** Signature, relation, and *whether π exists
  at all* change together. Report it as "classical baseline versus post-quantum
  stack" — **not** as the cost of the post-quantum signature alone. That quantity is
  the Stage-1 result (`ref/test/bench_classical.c`, `docs/LAS.md` §8.3).

---

## 4. What exists, and what was actually reusable

| Asset | Where | Status |
|---|---|---|
| LAS scheme, Rust (KAT-locked) | `rust/fips204-las/` | ✅ reused as a path dependency, untouched |
| Classical ECDSA adaptor | `secp256k1-zkp` crate 0.11.0 | ✅ used (official Blockstream Rust bindings) |
| π over LaZer | `fips204::relation_zk`, feature `relation-zk` | ✅ reused via the same C bridge as the C build |
| Fig. 1 protocol, verbatim | `ref/test/test_swap.c` | ✅ transferred — message order and abort gates |
| Simulated ledger + HTLC | `ref/chain.{c,h}`, `ref/test/test_pcn.c` | ❌ **stale, does not compile** (old `las_pk`/`las_sig`/`las_pp` API); replaced rather than ported |
| Wire encoding | `fips204::serialize` | ✅ the basis of the communication measurement |

**Correction to the earlier draft of this plan:** it claimed `ref/chain.{c,h}` and
`test_pcn.c` carried over. They do not — along with `amhl.{c,h}`, `bench_app.c`,
`test_contract.c` and `test_amhl.c` they were left on the pre-restructure API and do
not build. Stage 2 therefore got a **new UTXO ledger** rather than a port of the
account-model one.

**Provenance note.** The C classical baseline links the vendored
`third_party/secp256k1-zkp` @ `95b9835`; the Rust configuration 1 uses the
`secp256k1-zkp` crate, which builds its own bundled copy. Same library, independently
pinned — say which implementation produced which number.

---

## 5. Chain choice

Wang's steer: **avoid Monero**, prefer **two similar UTXO chains**. Implemented as
**two independent in-repo UTXO ledgers** (`rust/las-swap/src/utxo.rs`) with different
nominal block times (600 s / 150 s, Bitcoin-like and Litecoin-like).

The venue follows the paper directly. §4 assumes "a UTXO-based blockchain like Bitcoin
**where the signature algorithm is replaced with a lattice-based signature scheme given
in Algorithm 1**", with spending conditions built from "signature ... verifications, and
timing conditions". The ledger models exactly that, and takes the signature algorithm as
a **parameter** — so one ledger serves all three configurations and any measured
difference is the cryptography, not the chain.

**Limitation to state in the report.** This is an in-process model: no p2p, no mempool
policy, no fee market, no reorgs, no script interpreter. It gives real transaction
objects, real serialised sizes, and the real leak channel (`spending_signature`), but
not real fees or confirmation latency. Running configurations 2 and 3 against a *real*
bitcoind regtest is not possible without a consensus change — Bitcoin Script has no
ML-DSA opcode — which is itself the finding that motivates the paper's assumption.

---

## 6. Measurement plan

Gas does not exist here, so per §16.3 the axes are **time** and **communication cost**.

- **Time:** per protocol phase, reported per operation (`metrics::Phase`), consistent
  with the Stage-1 rule that per-operation timing is primary; end-to-end is printed as
  context only.
- **Communication:** every off-chain message counted, not just what reaches the chain.
  Sizes are *observed* from the actual buffers, never computed from a formula — this is
  why the backend interface is byte-oriented.
- **Tier:** these timings are **packed tier** (every call includes encode/decode). They
  are **not** comparable with Stage-1 core-tier numbers; the driver prints this.
- **Statistics:** ≥5 runs (10 configured), mean ± **sample** SD, matching the C drivers.
  A phase must appear in every run or none — a phase timed in only some runs is
  rejected rather than averaged over a silently different sample.
- **Setup is excluded:** expanding `A` and building the secp256k1 context happen in
  constructors, before timing, and are reported separately. Groth16's **trusted
  per-circuit** setup is recorded as a *kind* as well as a cost, since it is a security
  assumption configuration 3 does not carry.
- **Reproducibility:** all parameters, keys, statements and masks derive from one pinned
  master seed (printed in the output; overridable with `LAS_SWAP_SEED`). All operations
  take the schemes' deterministic paths.

- **Rejection gate (wired):** the driver asserts the measured `PreSign` attempt rate
  against the closed form (`las_expected_attempts(BOUND_PRESIGN)` = 2.775 at D3) before
  reporting anything, and fails the configuration if it drifts. It runs on its **own
  batch of 2000 calls**, not on the swap loop: attempts/call are geometric, so at the
  swap loop's `2 × RUNS = 20` calls the 5σ band is ±2.48 — wider than the gap between
  the expected 2.775 and a completely broken loop that never rejects (1.0), which would
  therefore pass. At 2000 calls the band is ±0.248 and such a loop misses by ≈36σ.
  Schemes with no rejection loop (ECDSA) are skipped rather than assigned a rate of 1.0.

---

## 7. Sequencing and progress

1. ~~Select and vendor the host repo~~ — `third_party/eigenwallet-core` @ `26b9a4477`
   vendored. **Confirmed offline:** it implements the full quartet, including `Ext`
   (`swap-core/src/bitcoin.rs:239` `recover` → `recover_decryption_key`), answering the
   Meeting-7 item-11 question. Its architecture informed the design; its code was not
   reused, because it is a Bitcoin↔Monero codebase whose cross-group machinery is
   exactly the part to avoid.
2. ✅ UTXO ledger, both chains (`utxo.rs`).
3. ✅ Fig. 1 protocol driver, verbatim (`protocol.rs`), including the honest path and
   the timeout/refund edge case.
4. ✅ Configuration 1 (classical) — compiles under `--features secp256k1`.
5. ✅ Configuration 3 (LAS + LaZer) — compiles under `--features relation-zk`.
6. ✅ Configuration 2 (LAS + Groth16) — circuit built, compiles under
   `--features groth16`. All three build together.
7. ✅ Rejection gate wired (§6).
8. ⬜ **Run the benchmark** and capture evidence. Not yet done — this is the
   remaining step, and the first run is also the first execution of the protocol
   code, so expect to iterate.

---

## 8. Risks and open decisions

| Risk | Assessment |
|---|---|
| **Bitcoin Script cannot verify an LAS signature.** | Not required by the paper, which *assumes* a UTXO chain whose signature algorithm is Algorithm 1 (§4). It does mean a real bitcoind regtest cannot settle configurations 2–3 without a consensus change — recorded as the limitation in §5, not worked around. |
| **Configuration 2 needs a Groth16 circuit over a lattice statement.** | **Resolved — built** (`rust/las-swap/src/groth16_circuit.rs`, arkworks 0.4 / BN254). The feared blow-up did not materialise, and the reason is worth reporting: `r ↦ A r` is **linear with public coefficients**, so it costs *no* multiplication constraints in R1CS. Only two things need constraining — the ternary bound (`r³ = r`, 2 constraints/coefficient) and the reduction mod `q` (a range-checked quotient per row). Total ≈ 28.7 k constraints. It would have been intractable had the relation involved products of two *secret* polynomials. |
| Groth16's trusted setup and proof randomness. | Both must use real entropy: a reproducible CRS would leave the toxic waste recoverable (soundness void), and reused proof randomness breaks zero-knowledge. Setup and every `prove` call therefore draw from the OS RNG — the **one** deliberate exception to this harness's pinned-seed reproducibility. Proof size and timing are unaffected; only the exact proof bytes differ between runs, and those are not a reported quantity. |
| Reading `A` out of the KAT-locked port. | The circuit needs `A` as public constants, but `PublicParams` keeps it `pub(crate)` in the NTT domain. Added `relation::apply_a` — an additive, read-only wrapper over the existing private `amul`; the matrix is then recovered column-by-column by evaluating the map on unit vectors, so the circuit uses *the map the scheme actually computes with* rather than a re-derivation that could drift. **KAT digest re-verified unchanged** (`cargo test --test las_kat` passes). No C twin was added: `ref/` has no caller. |
| Both provers must establish the *same* claim. | Dropping the norm bound and proving only `A r = t` would be a weaker statement that does not close the knowledge gap §4.1 depends on, and 2 → 3 would no longer be a comparison. Enforced by `Relation` + `RelationBinding`. |
| Two halves of a configuration could disagree about `A`. | `Configuration::status()` refuses a pairing whose relation or parameter binding differs. The binding is derived *through the scheme's own code* (seed identity + several probe statements), not from a stored seed. |
| Host repo is Rust; the C implementation is the one of record. | Stage 2 is Rust, on the KAT-locked port. State which implementation produced which number. |

---

## 9. Explicitly out of scope for this stage

Live testnet/mainnet deployment · EVM/Solidity work (deferred) · real network sockets
(messages passed directly, §16.4) · on-chain π · AMHL multi-hop (bonus, already done) ·
parameter migration to q≈2²⁴.

---

## 10. How to build and run

```bash
cd rust/las-swap
cargo run --release --bin bench_swap --features secp256k1,groth16,relation-zk
```

Per configuration: `secp256k1` → 1, `groth16` → 2, `relation-zk` → 3 (the last also
needs the vendored LaZer built once; see the repo README, "π + atomic swap").

Configurations whose backends are absent are reported as unavailable and skipped; the
driver never estimates a number it did not measure.
