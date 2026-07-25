# las-swap — Stage 2: post-quantum atomic swap on UTXO chains

Implements the atomic-swap protocol of **eprint 2020/845 §4.1, Fig. 1** over two UTXO
ledgers, and benchmarks three configurations of it on **execution time** and
**communication cost**.

Plan and interpretation: [`docs/02-methodology/STAGE2_UTXO_SWAP_PLAN.md`](../../docs/02-methodology/STAGE2_UTXO_SWAP_PLAN.md).
Read `src/backend.rs`'s module docs before interpreting any number.

## The three configurations

| # | Signature | Role-A proof `pi` | Post-quantum |
|---|---|---|---|
| 1 | classical adaptor (ECDSA, `secp256k1-zkp`) | **none required** | no |
| 2 | LAS | Groth16 over `∃r : A r = t ∧ ‖r‖∞ ≤ 1` | signature only |
| 3 | LAS | LaZer over `∃r : A r = t ∧ ‖r‖∞ ≤ 1` | fully |

Configuration 1 carries no `pi` because ECDSA adaptors have **no knowledge gap** — `Ext`
recovers the discrete log exactly, so pre-signature adaptability is unconditional. The
DLEQ inside its pre-signature is a *different* proof (author: the pre-signer; claim:
pre-signature well-formedness) and is counted under the signature's bytes, never as `pi`.

## Build and run

```bash
cargo run --release --bin bench_swap --features secp256k1,groth16,relation-zk
```

Per configuration: `secp256k1` → 1, `groth16` → 2, `relation-zk` → 3.

* `relation-zk` needs the vendored LaZer library built once — see the repo README,
  "π + atomic swap". It enables `fips204/relation-zk`, reusing the **same** C bridge and
  parameter set as the C build.
* `groth16` pulls arkworks, pinned to the **0.4** line. (0.6 renamed
  `ark_relations::r1cs` to `gr1cs` and reworked the gadget traits, so the circuit does
  not build against it unmodified.) The circuit itself is ours:
  `src/groth16_circuit.rs`.

## The Groth16 circuit, briefly

Proving `∃r : A r = t ∧ ‖r‖∞ ≤ 1` in R1CS looks hostile but is not, because
`r ↦ A r` is **linear with public coefficients** — a linear combination costs no
multiplication constraint. Only two things need constraining:

| What | How | Constraints |
|---|---|---|
| ternary bound `‖r‖∞ ≤ 1` | `r³ = r` per coefficient | 2 × 2816 |
| reduction mod `q` | range-checked quotient per row, 14 bits | 15 × 1536 |

≈ 28.7 k total. It would *not* have been tractable if the relation involved products of
two secret polynomials.

`A` is recovered from the KAT-locked port by evaluating the map on unit vectors, via an
additive `relation::apply_a` accessor — so the circuit uses the map the scheme actually
computes with. The KAT digest was re-verified unchanged after that addition.

**Randomness is the one deliberate exception to reproducibility here.** The trusted setup
and every proof draw from the OS RNG: a reproducible CRS would leave Groth16's toxic
waste recoverable (soundness void), and reused proof randomness breaks zero-knowledge.
Consequences for the measurement: proof **size** is unaffected (three group elements
regardless), but proving **time** is — the entropy draw happens inside `prove`, so the
`Prove` phase carries a little extra run-to-run variance. That is the real cost of a
sound proof, and part of why timings are reported as mean ± SD.

Configurations whose backends are absent are reported and skipped. The driver never
estimates a number it did not measure.

## What can be attributed

* **2 → 3 is controlled.** Same signature, same `A`, same relation, and byte-identical
  keys/statement/witness/transactions (everything derives from one pinned master seed).
  Only the prover differs, so the delta is the proof system's. Lead with this.
* **1 → 2/3 is a whole-stack comparison** — signature, relation, and whether `pi` exists
  all change. It is *not* the cost of the post-quantum signature alone; that is the
  Stage-1 result (`docs/LAS.md` §8.3).

## Reproducibility

Every parameter, key, statement and mask derives from a pinned 32-byte master seed,
printed in the output and overridable:

```bash
LAS_SWAP_SEED=$(python3 -c "print('ab'*32)") cargo run --release --bin bench_swap --features secp256k1
```

A malformed `LAS_SWAP_SEED` is a hard error, never a silent fallback. All operations take
the schemes' deterministic paths (`keygen_seed`, `gen_seed`, `presign_det`,
`encrypt_no_aux_rand`).

## Measurement conventions

* **Packed tier** — every operation includes wire encode/decode. **Not** comparable with
  Stage-1 core-tier numbers.
* Per-operation timing is primary; end-to-end is context only.
* ≥5 runs (10 configured), mean ± **sample** SD, matching the C drivers.
* A phase must be timed in *every* run or in none — never averaged over a subset.
* Communication subtotals are computed per run, then reduced; sizes are observed from
  real buffers, never from a formula.
* One-time setup (expanding `A`, building the secp256k1 context, any CRS) happens in
  constructors before timing and is reported separately.

## Relation to the rest of the repo

* Depends on `rust/fips204-las` as a **path dependency**; that package's pinned KAT
  digest is untouched by anything here.
* Does **not** reuse `ref/chain.{c,h}` or `ref/test/test_pcn.c` — they remain on the
  pre-restructure C API and do not compile. The UTXO ledger here replaces that model.
* `third_party/eigenwallet-core` @ `26b9a4477` informed the design (its `encsign` /
  `verify_encsig` / `decrypt_signature` / `recover` quartet confirms a maintained
  classical swap does implement `Ext`); its code is not reused, since it is a
  Bitcoin↔Monero codebase whose cross-group machinery is the part to avoid.

## Scope

An in-process ledger model: no p2p, no mempool policy, no fee market, no reorgs, no
script interpreter. It provides real transaction objects, real serialised sizes, and the
real leak channel (`Chain::spending_signature`) — but not real fees or confirmation
latency. Paper §4 assumes a UTXO chain "where the signature algorithm is replaced with a
lattice-based signature scheme given in Algorithm 1", which is exactly what
`utxo::Chain` models by taking the signature algorithm as a parameter.
