# Project history, step by step — what was built, in what order, and why

*A sequential, file-by-file and function-by-function account of how this project was
built, written so you can explain to your supervisor **what** was coded, **in what
order**, and **why that order**. It is beginner-friendly but precise: every new C
file and every key function is named, in the sequence it was actually written
(reconstructed from the git history). For the plain-English "what is this project"
story see `docs/01-introduction/LAS_WALKTHROUGH.md`; for the gas experiment specifically see
`docs/03-results/GAS_LIMIT_INVESTIGATION.md`.*

> How to read this: Section 1 is the one-paragraph goal. Section 2 is a single
> timeline table (the whole story at a glance). Section 3 walks each step with its
> new files + functions + the decision behind it. Section 4 is the "why this order"
> dependency flow. Section 5 is the current, uncommitted work. Keep this open next to
> `git log` and it will all line up.

---

## 1. The goal everything serves

Turn **LAS** — a "fancy" post-quantum *adaptor* signature that existed only as maths
in an academic paper (eprint 2020/845) — into **real, tested, measured code**, built
on top of the trusted NIST-standard **Dilithium** primitives, and demonstrate it in a
**blockchain atomic swap**. The build order below is not arbitrary: each step is a
foundation the next step stands on.

---

## 2. The whole timeline at a glance

| # | Commit | Date | New code files (in order) | What it gives us | Why it had to come here |
|---|---|---|---|---|---|
| 0 | `2374d22` | 06-02 | *(all of upstream Dilithium)* | the trusted lattice primitives (poly/NTT/SHAKE/sampling) | you can't build on Dilithium without Dilithium |
| 1 | `701f97a` | 06-03 | `ref/las.c`, `ref/las.h`, `ref/test/test_las.c` | **the LAS scheme itself** (KeyGen/Sign/Verify + PreSign/PreVerify/Adapt/Ext) | the core deliverable — nothing else matters until this works |
| 2 | `6746331` | 06-03 | `ref/chain.c`+`.h`, `test_swap.c`, `test_pcn.c`, `bench_las.c`, `bench_compare.c` | a **simulated ledger** + atomic-swap & payment-channel demos + first benchmarks | a signature is only interesting once it *does something* on a chain |
| 3 | `bac594b` | 06-03 | *(docs: `THEORY_IMPL_BRIDGE.md`)* | paper-equation → code-line mapping | report marks: prove the code *is* the paper |
| 4 | `3a4c357` | 06-03 | `ref/amhl.c`+`.h`, `test_amhl.c` (+ new fns in `las.c`/`chain.c`) | **multi-hop** payment locks (bonus) | multi-hop is an *extension* of the single-hop swap, so it comes after it |
| 5 | `5dc1b63` | 06-12 | `ref/serialize.c`+`.h`, `bench_classical.c`, `bench_app.c`, `test_serde.c`, `test_kat.c` (+ deterministic fns in `las.c`) | **byte format** + byte-level verifier + classical baseline + reproducible vectors | to measure *size* and to have the interface a blockchain consumes |
| 6 | `2ffcca4` | 06-15 | `evm/src/AdaptorSwap.sol`, `evm/test/AdaptorSwap.t.sol`, `ref/test/export_packed.c` | a **real Solidity swap on a local EVM** + gas measurement | graduate from a simulated ledger to a real blockchain |
| 7 | *(this session, uncommitted)* | 06-18 | `evm/src/LASVerifyCost.sol`, `evm/test/LASVerifyCost.t.sol` | **measured gas of native LAS verification** | replace a hand-waved "exceeds the block limit" claim with a number |

---

## 3. Step by step — files, functions, and the decision behind each

### Step 0 — Foundation: the Dilithium reference (`2374d22`)
**Files:** the entire upstream CRYSTALS-Dilithium reference (`ref/poly.c`, `ntt.c`,
`reduce.c`, `symmetric-shake.c`, `packing.c`, …) — **unmodified**.
**Decision/why:** LAS is "Dilithium + four extra functions." Reinventing lattice
arithmetic (polynomials, the NTT, SHAKE hashing, samplers) would be slow and
error-prone. So the very first move is to import the trusted, standardised code and
**change none of it** — later proven in `docs/02-methodology/FUNCTION_MAP.md` (zero upstream
functions modified), which is itself a credibility argument for the thesis.

### Step 1 — The LAS scheme (`701f97a`) — *the heart of everything*
**New files:** `ref/las.c`, `ref/las.h`, `ref/test/test_las.c`.

Inside `las.c` the functions were written **bottom-up by dependency** — helpers first,
then the scheme, then the adaptor layer:

1. **Low-level helpers (built first because everything calls them):**
   - `pack_poly_canon`, `poly_equal`, `chknorm_vec` — encode a polynomial for hashing,
     compare two challenges, check a vector's size bound.
   - `polymul`, `las_Amul` — multiply polynomials (via the NTT) and compute the
     matrix-vector product `A·v`. **`las_Amul` is the workhorse** later measured in the
     gas experiment.
   - `las_challenge`, `hash_challenge` — the **Fiat–Shamir hash** `c = H(pk, commit, M)`
     that turns the commitment into a challenge. This is the one spot where Sign and
     PreSign differ (PreSign folds the statement `Y` into the hash).
   - `sample_Sgamma`, `sample_ternary` — draw the random masks and the ternary
     secret/witness.
2. **The ordinary signature (built and tested *before* the adaptor part):**
   - `setup_public_params`, `base_keygen` — public parameters and a key pair.
   - `base_sign_internal` → `base_sign`, and `base_verify`.
3. **The adaptor layer (built last, *on top of* a working Verify):**
   - `las_presign_internal` → `las_presign`, `las_preverify`.
   - `las_adapt` (turns a pre-signature into an ordinary signature), `las_ext`
     (recovers the secret witness — the trick that makes swaps atomic).

**Decision/why this internal order:** `las_adapt`'s whole job is to produce a
signature that **ordinary `base_verify` accepts**. So `base_verify` must exist and be
trusted *first*; only then can you build and test `las_adapt`/`las_ext` against it.
`test_las.c` then hammers all eight functions **1000×** and asserts the key safety
property (the pre-signature must *not* pass ordinary Verify — the "tripwire").

### Step 2 — A (simulated) blockchain and the swap (`6746331`)
**New files:** `ref/chain.c`+`.h` (the ledger), `test_swap.c`, `test_pcn.c` (demos),
`bench_las.c`, `bench_compare.c` (first benchmarks).

`chain.h` adds the ledger API, in the order a swap actually happens:
`chain_init` → `chain_account_add` → `chain_advance` (mine blocks) →
`chain_fund_swap` (lock coins behind a statement `Y`) → `chain_claim_swap` (spend by
publishing the adapted signature) → `chain_extract_witness` (the counterparty pulls
the secret out of the on-chain signature) → `chain_refund_swap` (timeout path).

**Decision/why:** Only *after* the scheme verifiably works do you build something that
uses it. A "simulated ledger" (accounts + block height + locked contracts, all in C)
is the right amount of blockchain to prove the swap logic **before** taking on the
complexity of a real EVM. `bench_las.c` (per-operation timings) and `bench_compare.c`
(LAS vs plain Dilithium) start the measurement story.

### Step 3 — Tie the code back to the paper (`bac594b`)
**New file:** `docs/02-methodology/THEORY_IMPL_BRIDGE.md` (plus a literature section in `docs/LAS.md`).
**Decision/why:** Pure assessment value — examiners want every equation in the paper
mapped to the exact C function/line. No new scheme code; this is the evidence layer.

### Step 4 — Multi-hop locks: AMHL (`3a4c357`) — *the bonus*
**New files:** `ref/amhl.c`+`.h`, `ref/test/test_amhl.c`.
**Also extended existing files:** `las.c`/`las.h` gained `las_presign_k` /
`las_preverify_k` (a tighter `γ−κ−K` bound for K hops); `chain.c` gained
`chain_fund_swap_k`.
New AMHL functions: `amhl_setup_gen` (distinct per-hop statements
`Y_j = A·(l_1+…+l_j)`), `amhl_norm`, `amhl_recover_prev` (the cascade recovery).
**Decision/why:** Paying through a *chain* of intermediaries (A→B→C→D) is a
generalisation of the single-hop swap, so it is built **after** single-hop works and
**reuses** PreSign with only a tighter size bound. It was re-classified as
optional/bonus at Meeting 2 — done, but it must not displace the core.

### Step 5 — Bytes, baselines, reproducibility (`5dc1b63`)
**New files:** `ref/serialize.c`+`.h`, `ref/test/bench_classical.c`,
`ref/test/bench_app.c`, `ref/test/test_serde.c`, `ref/test/test_kat.c`.
**Also extended `las.c`:** a **deterministic** API — `base_keygen_seed`,
`base_sign_det`, `las_presign_det`, and the shared `det_seed`/`base_sign_internal`/`las_presign_internal`
refactor — so runs are byte-for-byte reproducible.
`serialize.h` adds `pack_*`/`unpack_*` and, crucially, **`base_verify_packed`**
— verification straight from a byte string (the exact interface an on-chain verifier
would consume).
**Decision/why:** Two things become possible only once signatures exist as **bytes**:
(a) measuring the real **size** (the headline "price of post-quantum is bytes"), and
(b) a *validating* byte-level verifier. `bench_classical.c` adds the required
**classical** comparison (ECDSA adaptor), and `test_kat.c` pins fixed test vectors so
anyone can reproduce identical output.

### Step 6 — A real blockchain: the EVM swap (`2ffcca4`)
**New files:** `evm/src/AdaptorSwap.sol` (a Solidity HTLC escrow),
`evm/test/AdaptorSwap.t.sol` (the gas benchmark), and `ref/test/export_packed.c`
(exports a real packed LAS signature from C so Solidity can load the genuine bytes).
**Decision/why:** Now that there is a byte format and a working swap, graduate from
the simulated ledger to a **real local Ethereum**. The contract settles a swap with
either a classical or a LAS signature, so a gas report isolates the on-chain "price of
post-quantum." `export_packed.c` exists so the on-chain test uses a *real* 6720-byte
signature, not a fake.

### Step 7 — The gas experiment (this session, uncommitted)
**New files:** `evm/src/LASVerifyCost.sol`, `evm/test/LASVerifyCost.t.sol`.
**Decision/why:** Step 6 left a claim it never proved ("native verification exceeds
the block gas limit"). Supervisor feedback item 8 demanded an experiment or a
calculation. The cost probe reproduces the operation count of `base_verify` at the
D3 parameter set (12 fwd NTT + 12 inv NTT + 36 pointwise + 54 coefficient passes) on
the EVM, prices it (**13.93 M gas measured**, +2.76 M calculated SHAKE256 ⇒ **≈16.7 M
total**), and **corrects** the claim — see `docs/03-results/GAS_LIMIT_INVESTIGATION.md`.
*(Later superseded, 2026-07-23: a complete native Solidity verifier `evm/src/LASVerifier.sol`
was built and validated end-to-end vs C, wired into `AdaptorSwap.claimLASVerified`, and
**measured at 56,538,682 gas** — larger than the op-count estimate because it also runs the
real SHAKE256, z-decode and packing, and it exceeds the EIP-7825 per-transaction gas cap.)*

---

## 4. The decision flow, as a picture

```
 Dilithium primitives (import, never modify)        ← must exist first
        │
        ▼
 LAS scheme  las.c : helpers → Sign/Verify → PreSign/PreVerify/Adapt/Ext
        │            (ordinary signature proven before the adaptor layer)
        ▼
 Simulated ledger  chain.c  +  swap / PCN demos      ← scheme must work first
        │
        ├── docs: theory↔code bridge                 (evidence, parallel)
        │
        ├── AMHL multi-hop  (extends PreSign w/ tighter bound)   ← after single-hop
        │
        ▼
 Serialization  serialize.c  →  bytes + base_verify_packed
        │   (unlocks SIZE measurement + classical baseline + KATs)
        ▼
 Real EVM  AdaptorSwap.sol  (real gas of a real swap)
        │
        ▼
 Gas of native verification  LASVerifyCost.sol  (measure the last open claim)
```

The single rule behind the whole order: **you can only measure or apply something
after it exists and is proven.** Primitives → scheme → application → measurement.

---

## 5. Status of this session's work

The Step-7 files and the documentation corrections are currently **uncommitted** in
the working tree (nothing pushed, nothing lost). They are listed in `docs/STATUS.md`
(deliverable D15) and explained in `docs/03-results/GAS_LIMIT_INVESTIGATION.md`.

---

## 6. One-line "explain it to the supervisor" summary

> "I imported the standard Dilithium primitives unchanged, built the LAS adaptor
> scheme on top (ordinary Sign/Verify first, then the four adaptor functions), proved
> it with a 1000-iteration test, then demonstrated it on a simulated ledger (atomic
> swap, payment channels), added the bonus multi-hop construction, gave signatures a
> real byte format so I could measure size and verify from bytes, ran it as a real
> Solidity swap on a local EVM for gas, and finally measured the gas cost of native
> on-chain verification — which corrected an earlier over-claim. Each step is a
> foundation for the next, and none of the upstream Dilithium code was modified."
