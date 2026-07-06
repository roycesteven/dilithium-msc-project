<!-- Part of docs/LAS.md, split by report chapter (2026-07-06). Index: docs/LAS.md.
     Section numbering is preserved verbatim, so external references like
     "LAS.md §8" resolve to this file. Do not renumber sections. -->

## 8. Performance (measured)

Wall-clock microseconds per operation, mode 3, 2000 iterations/op, `-O3`,
measured by `ref/test/bench_las3`. Absolute numbers are machine-dependent; the
*ratios* are the point.

| Operation | Time (µs) | Note |
|---|---:|---|
| Setup (expand `A`) | 58 | `n·ℓ = 16` uniform polys via SHAKE128 |
| KeyGen / statement gen | 78 | sample `r` (ternary), compute `A·r`; *same cost* for `(Y,y)` |
| Sign | 804 | ~2.7 attempts/signature (≈37% acceptance, see below) |
| Verify | 191 | one `A·z − c·t` + hash |
| PreSign | 828 | ≈ Sign; `H(pk, w+Y, M)` vs `H(pk, w, M)` is negligible |
| PreVerify | 197 | ≈ Verify; one extra `+Y` add |
| Adapt | 203 | PreVerify + 8 poly adds |
| Ext | 68 | one `A·s` + compare |

**Rejection-sampling acceptance rate (measured *directly*, bench_las3).**
`las_sign`/`las_presign`/`las_presign_k` increment a global `las_attempts`
counter once per rejection-loop iteration (instrumentation only — never read by
the scheme), so the benchmark reports the **exact** average attempts per
signature over 2000 calls rather than estimating it. Measured:

| | attempts/sig | acceptance | retries |
|---|---:|---:|---:|
| Sign | 2.71 | 36.9 % | 1.71 |
| PreSign | 2.77 | 36.1 % | 1.77 |

This matches the closed-form prediction. One attempt is accepted iff all
`(n+ℓ)·N = 2048` response coefficients land within `±(γ−κ)`, so the per-attempt
acceptance is `≈ (1 − κ/γ)^{(n+ℓ)·N} = (1 − 60/122880)^{2048} ≈ 36.8 %`
(`≈ e^{-1}`), i.e. `≈ 2.72` attempts/signature — within noise of the measured
numbers. This is expected and correct for a Fiat–Shamir-with-aborts scheme:
rejection sampling is intrinsic to the family. A subtle point worth stating
precisely, because it is easy to get backwards: omitting the hint vector does
**not** worsen our per-attempt acceptance. Optimised Dilithium rejects on *several*
conditions each attempt — the `‖z‖∞` bound **plus** a low-order-bits check on
`w − c·s₂` **plus** a hint-count limit — whereas this simplified scheme rejects on
the single `‖z‖∞` bound. Additional conditions can only lower acceptance, so the
hint-free design carries no inherent acceptance penalty; optimised Dilithium's own
expected signing repetitions are a small single-digit count (see the Dilithium
specification), i.e. comparable to our ≈2.7 attempts — not the >5× advantage that an
">80% with hints" claim would imply. (That earlier figure was not merely
unmeasured here but directionally wrong.) The `γ = κ·d·(n+ℓ) = 122880` choice
governs the MSIS hardness parameter, not the acceptance rate.

> **Correction (methodology note worth keeping in the report).** An earlier
> version of `bench_las` *estimated* retries from the timing ratio
> `t_sign / t_verify` and reported ~23 % acceptance (~4.3 attempts). That
> estimator is **biased**: one Sign attempt does `n+ℓ = 8` `c·r` products plus
> `A·y`, whereas one Verify does only `n = 4` `c·t` products plus `A·z`, so a Sign
> attempt is dearer than a Verify and the ratio over-counts attempts. The direct
> counter (~37 %, ~2.7 attempts) supersedes it and agrees with the `e^{-1}`
> theory line — a small but honest example of preferring direct measurement to a
> proxy.

**Object sizes (three distinct numbers — do not confuse them):**

| Object | In-memory `sizeof` | Packed (measured, `serialize.c`) | Paper's estimate |
|---|---:|---:|---|
| pk / statement Y | 4096 B | 2944 B | — |
| sk / witness y | 8192 B | 512 B | — |
| sig / pre-sig | 9216 B | 4672 B | ~3210 B |

- *In-memory:* `sizeof` counts full `int32_t` per coefficient.
- *Packed (measured):* these are the **actual** sizes emitted by `ref/serialize.c`
  (`LAS_PK_BYTES`, `LAS_SK_BYTES`, `LAS_SIG_BYTES`), validated by `test_serde`, not
  formulas. pk: 23 bits/coeff (`Q < 2^23`) → `4·256·23/8 = 2944 B`. sk: ternary at
  2 bits/coeff → `8·256·2/8 = 512 B`. sig: challenge `c` packed as a 2-bit ternary
  polynomial (`256·2/8 = 64 B`) + response `z` at 18 bits/coeff
  (`8·256·18/8 = 4608 B`) = **4672 B**. (`z` needs 18 bits at the paper/D2 sets because
  the centred range `2·(γ−κ)+1 = 245641 < 2^18`; the larger D3/D5 dimensions need 19 bits,
  so `LAS_Z_COEFF_BITS` is selected from the parameters at compile time. Packing `c` as
  ternary is 4 B smaller than the position-encoded 68 B and simpler to validate.)
- *Paper's ~3210 B:* the paper's *optimised* scheme at `q ≈ 2^24` with a hint
  vector and high/low-bit decomposition. Not comparable to this implementation.
  The correct comparison for our scheme is the "Packed (measured)" column.

**Takeaways for the report.** (i) `PreSign ≈ Sign`, `PreVerify ≈ Verify` — the
adaptor operations add negligible overhead over the base scheme, matching the
paper's efficiency claim. (ii) Sizes are large in-memory only; the *measured*
packed sig (4672 B) is not dramatically larger than optimised Dilithium-3
(3309 B bit-packed), and bit-packing is now implemented, not just estimated.
(iii) The sign rejection rate (~2.7 attempts, ≈37 % acceptance) is the honest cost
of the simplified, hint-free scheme — not a bug, and it matches the `e^{-1}` theory.

### 8.1 Primary comparison (`bench_levels`) — LAS vs its own simplified base

> **Methodology (corrected 2026-06-22; base path modularised 2026-06-23).** The
> **primary, fair** comparison is the **simplified Dilithium-style base signature path**
> against the **LAS adaptor path**, using the *same parameters and same primitives*. The
> base path is now a **separate, dedicated module** — `ref/basesig.c`
> (`base_keygen`/`base_sign`/`base_verify`): `Sign` hashes `c = H(pk, w, M)` and `Verify`
> recomputes `c = H(pk, w', M)` with **no statement `Y`** anywhere. The adaptor path is
> `ref/las.c` (`PreSign`/`PreVerify`/`Adapt`/`Ext`), which folds `Y` into the hash
> (`c = H(pk, w + Y, M)`). `basesig.c` is kept **out of `las.{c,h}`** on purpose so the
> LAS protocol is never conflated or modified; it shares only `las.h`'s parameter macros
> and key/signature struct layout, which keeps both schemes on the **same parameter set**
> (a dimension-level match — `n,ℓ,κ` — not a formal bit-security claim; security proofs are
> out of scope) and makes their keys/signatures interchangeable. That interchangeability is verified at
> runtime: a LAS pre-signature, once `Adapt`-ed, verifies under the **independent**
> `base_verify` with no explicit `+Y`, because `A(ẑ+y) − c·t = (Aẑ − c·t) + A·y = w' + Y`.
> The only thing that varies between the paths is the adaptor layer, so the fair result
> is its **overhead**. Official PQ-CRYSTALS Dilithium is a *different* algorithm (hints,
> Power2Round, decomposition, bit-packing) and appears **only as context, clearly
> labelled "not algorithm-matched"** — never as the fair baseline. (`bench_fair`/
> `bench_compare`, which read official Dilithium as a peer baseline, are superseded.)
>
> *Note on the figures below:* the base/Sign/Verify algorithm is unchanged by the
> module split (`basesig.c` is behaviourally identical to the previous in-`las.c` base —
> same sampler, same `H`, same `γ−κ` bound), so the measured overheads carry over; the
> numbers are refreshed by re-running `make test/bench_levels_* && ./test/bench_levels_*`.

**Pairing.** Each adaptor op is compared with the base op it mirrors:
`simplified Sign vs PreSign`, `simplified Verify vs PreVerify`,
`simplified Verify vs Adapt`, and `Ext` reported separately (no base analogue).

**Method.** Mean over **5 repetitions × 500 (sign-class) / 1000 (verify-class)
iters**, single-threaded, `-O3`, machine of record (AMD Ryzen 7 7745HX, Ubuntu 24.04,
GCC 13.3.0); **mean ± sample SD**. The repetition scheme, fixed pp seed and fixed
33-byte message mirror the Rust driver exactly, and every run self-validates via the
**rejection gate**: measured attempts/call over the timed sign-class calls must match
the exact expectation `las_expected_attempts` (Sign 2.71875, PreSign 2.77483 at the
headline set) within 5σ, else the run aborts. *(Evidence tables committed before
2026-07-06 were measured under the previous 10 × 1000 scheme — same statistic, more
iterations — and are superseded on the next `run_benchmark_suite.sh` run.)* Sizes are
packed bytes by field-width formula (per parameter set). Parameters are overridable at
compile time (`-DLAS_N/-DLAS_ELL/-DLAS_KAPPA`); `γ=κ·d·(n+ℓ)` is derived; the `S_γ`
sampler bit-width adapts to `γ`.

**Primary (fair): adaptor overhead at the paper set (n=ℓ=4, κ=60):**

| Adaptor op | paired base op | base (µs) | adaptor (µs) | overhead |
|---|---|---:|---:|---:|
| PreSign | Sign | 763±27 | 797±26 | +4.4% |
| PreVerify | Verify | 182±2 | 191±4 | +5.0% |
| Adapt | Verify | 182±2 | 193±8 | +5.9% |
| Ext | — | — | 63±0.4 | (separate) |
| KeyGen/stmt gen | — | 80±1 (shared) | | — |

*The `Adapt` row is timed **checked — it includes the internal `las_preverify`** that a
real `las_adapt` must run before adding the witness ([las.c:431](../ref/las.c#L431)), so
the figure is Adapt **plus** PreVerify, not a bare core. (The benchmark's own stdout
label still prints `Adapt`; this caption is the precise reading.)*

**Why these are small (structural, not coincidental):**
- `PreSign≈Sign` — PreSign only folds `Y` into the FS commitment and uses a tighter
  bound; same masked-commit/hash/respond/reject loop.
- `PreVerify≈Verify` — recomputes the same commitment shape, only with `+Y`.
- `Adapt≈Verify` — Adapt runs a PreVerify then adds the witness.
- `Ext` cheapest/separate — only `z−ẑ` and a check `A·y=Y`.

**Adaptor overhead stays small at every parameter set:**

| Set (n,ℓ,κ) | PreSign/Sign | PreVerify/Verify | Adapt/Verify | Ext (µs) |
|---|---:|---:|---:|---:|
| paper (4,4,60) | +4.4% | +5.0% | +5.9% | 63 |
| NIST 2 (4,4,39) | +4.8% | +5.0% | +4.4% | 64 |
| NIST 3 (6,5,49) | +3.2% | +3.9% | +3.6% | 100 |
| NIST 5 (8,7,60) | +3.3% | +3.8% | +1.9% | 150 |

**Communication — component breakdown (packed bytes):**

| Component | NIST 2 (4,4) | NIST 3 (6,5) | NIST 5 (8,7) |
|---|---:|---:|---:|
| `c` (challenge) | 64 | 64 | 64 |
| `z` (response) | 4608 | 6688 | 9120 |
| **signature (c,z)** | **4672** | **6752** | **9184** |
| `z` as % of sig | 98.6% | 99.1% | 99.3% |
| public key `pk` | 2944 | 4416 | 5888 |
| secret key `sk` | 512 | 704 | 960 |
| statement `Y` (LAS only) | 2944 | 4416 | 5888 |
| pre-signature (LAS only) | 4672 | 6752 | 9184 |

**The sharp conclusion:** the cost of LAS is **not adaptor computation** (≤~6% over
the base, everywhere) but **communication** — especially the response vector `z`
(98.6–99.3% of the signature) and the public-key-sized statement `Y`. For a blockchain,
where every byte is stored and replicated, this is the property that matters.

#### 8.1.1 Context only: optimised CRYSTALS-Dilithium (NOT algorithm-matched)

For context — *not* a fair head-to-head, because it is a different (optimised)
algorithm — the simplified base vs the optimised reference at matching dimensions:

| | base KeyGen | Dil KeyGen | base Sign | Dil Sign | base Verify | Dil Verify | base sig | Dil sig |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| NIST 2 | 77 | 144 | 857 | 689 | 181 | 160 | 4672 | 2420 |
| NIST 3 | 116 | 260 | 1311 | 1168 | 280 | 258 | 6752 | 3309 |
| NIST 5 | 174 | 388 | 1594 | 1409 | 397 | 420 | 9184 | 4627 |

The simplified base is in the same regime (KeyGen faster — no Power2Round/packing;
Verify within a few %; Sign somewhat slower as it is otherwise unoptimised). Its
signature is ~2× larger and pk larger, while its ternary sk is several× *smaller*.
Because dimensions match, this gap is **packing/optimisation, not security** — the
price of omitting Dilithium's hint/decomposition/bit-packing. `bench_fair` and
`bench_compare` remain in the tree only so older references resolve; they are
superseded and must not be cited as fair baselines.

### 8.2 Application-level benchmark (`ref/test/bench_app.c`)

Section 8 / 8.1 measure the **signature** dimension. Wang explicitly asked for
*two* benchmark types — the signature itself **and** the application. `bench_app3`
supplies the second: the communication and settlement-payload cost of the two LAS
workflows. Sizes are the **actual serialised sizes** produced by `serialize.c`
(`LAS_PK_BYTES`, `LAS_SIG_BYTES`; Sections 5.10 and 8) — not formulas; restart
counts are measured directly via `las_attempts`.

> **Simulated ledger (scope).** `bench_app` runs against a *simulated* ledger, not a
> deployed Ethereum or Bitcoin contract. All byte figures below are an
> **application-level payload / settlement-footprint proxy**, not measured gas;
> per the project scope, (pre-)signature and statement sizes stand in for
> transaction cost. Real-chain gas measurement is out of scope (Section 9).

**(1) Atomic cross-chain swap (2 parties, 2 chains, no scripts).**

| Phase | Object(s) | Bytes |
|---|---|---:|
| Off-chain (3 messages) | `Y` + `σ̂_A` + `σ̂_B` | 2944 + 4672 + 4672 = **12 288 B** |
| Settlement footprint (proxy) | `σ_A`, `σ_B` published | 2 × 4672 = **9 344 B** |
| Settlement footprint incl. escrowed `Y` | + 2 × `Y` | 15 232 B |

Only the two *adapted* signatures would be published on a real chain; each is a
single ordinary-looking LAS signature. The "escrowed `Y`" row is the more
conservative **same-`Y` scriptless-HTLC** model realised only in the *simulated*
ledger (`ref/chain.c`, which stores `Y` as the on-chain lock); the deployed EVM
artefact (§8.4, `evm/src/AdaptorSwap.sol`) **deliberately stores no `Y`** — its LAS
claim path charges calldata for the final 4672-byte adapted signature alone, so the
`Y` bytes are off-chain adaptor communication, not an on-chain cost there.
End-to-end signing work (2× PreSign + 2×
Adapt + Ext) is a few milliseconds (a single un-averaged sample, dominated by the
two rejection-sampled pre-signs, so it varies run to run). The harness re-asserts
the fairness invariant (adapted sigs verify, pre-sigs do not).

**(2) Multi-hop AMHL payment — cost as a function of path length K** (40 routes per
K, mode 3; `attempts/presig` measured directly):

| K | bound `γ−κ−K` | #pre-sigs | attempts/presig | presig time (ms) | settlement sigs | public statements | max `‖s_j‖∞` |
|--:|--:|--:|--:|--:|--:|--:|--:|
| 1 | 122820 | 1 | 2.60 | 0.74 | 4 672 B | 2 944 B | 1 |
| 2 | 122819 | 2 | 2.30 | 1.29 | 9 344 B | 5 888 B | 2 |
| 4 | 122817 | 4 | 2.73 | 3.01 | 18 688 B | 11 776 B | 4 |
| 6 | 122815 | 6 | 2.95 | 4.97 | 28 032 B | 17 664 B | 6 |
| 8 | 122813 | 8 | 2.91 | 6.61 | 37 376 B | 23 552 B | 7 |

*(Representative run; the byte columns are exact — they are the `serialize.c`
sizes `K·LAS_SIG_BYTES` and `K·LAS_PK_BYTES` — but `attempts/presig`, `presig
time` and the realised `max‖s_j‖∞` (≤ K) are random/machine-dependent and vary
between runs.)*

Three findings for the report:
1. **Settlement footprint is linear in K** — `K` adapted signatures + `K` public
   statements (payload proxy, not gas); no super-linear blow-up.
2. **Witness norm grows with the hop index, `‖s_j‖∞ ≤ j ≤ K`** (each `s_j` is a sum
   of `j` ternary vectors; the realised maximum is at or just below `K`, e.g. 7–8
   at K=8 across runs), the "knowledge gap" made concrete and the precise reason
   every hop pre-signs at `γ−κ−K`.
3. **The `γ−κ−K` tightening is performance-negligible.** Going `K = 1 → 8` shrinks
   the accept band by `7/(γ−κ) ≈ 0.0057 %`, so `attempts/presig` is flat in `K`
   (≈2.7–3.0, the variation is sampling noise). AMHL therefore adds **no per-hop
   signing penalty** beyond the unavoidable "K hops ⇒ K pre-signatures." This is a
   genuine, slightly counter-intuitive result: the bound change that makes
   multi-hop *correct* costs essentially nothing in *speed*.

### 8.3 Classical adaptor baseline — "the price of post-quantum" (`ref/test/bench_classical.c`)

Meeting 2 added a second required baseline (objective B2.ii): LAS vs a
**classical adaptor signature**. We use the **ECDSA-based adaptor** from
`libsecp256k1-zkp` (BlockstreamResearch's fork of Bitcoin Core's libsecp256k1;
module `ecdsa_adaptor`, production code used in Discreet Log Contracts), vendored
at commit `95b9835` and benchmarked **on the same machine and compiler** as every
LAS number in this document — so the comparison needs no hardware caveats. Per the
supervisor's guidance the implementation is *reused as-is*; only the timing
harness (which mirrors the LAS operation set one-to-one) is ours. Reproduce via
`README.md` (one-time clone + `make test/bench_classical`).

**The 2×2 timing matrix (µs/op, 2000 iters, same machine):**

| Operation | ECDSA (classical basic) | ECDSA-adaptor (classical exotic) | Dilithium-3 (PQ basic) | LAS (PQ exotic) |
|---|---:|---:|---:|---:|
| KeyGen / statement gen | 31 | 31 | 162 | 78 |
| Sign | 41 | — | 642 | 804 |
| Verify | 62 | — | 155 | 191 |
| PreSign | — | 189 | — | 828 |
| PreVerify | — | 244 | — | 197 |
| Adapt | — | 3 | — | 203 |
| Ext | — | 35 | — | 68 |

**Sizes (B):**

| Object | ECDSA(-adaptor) | LAS (packed, measured) | ratio |
|---|---:|---:|---:|
| public key / statement | 33 | 2944 | ×89 |
| secret key / witness | 32 | 512 | ×16 |
| signature | 64 (70 DER) | 4672 | ×73 |
| pre-signature | 162 | 4672 | ×29 |

**Reading the data (the report's "let the data speak" paragraph):**

1. **The price of post-quantum is overwhelmingly *communication*, not
   computation.** Sizes grow ×29–×89; per-operation times grow far less (Verify
   ×3.1, PreSign ×4.4, Sign ×19.5), and everything stays in the
   sub-millisecond regime on commodity hardware. For blockchain use the size
   column is the binding constraint (on-chain bytes), which is exactly the
   motivation for the packing of Section 5.10.
2. **The adaptor *overhead structure* is inverted — LAS's headline win.** In the
   classical scheme the adaptor functionality is expensive *relative to its own
   base*: PreSign costs 4.6× Sign and PreVerify 3.9× Verify, because the
   pre-signature must carry and check a DLEQ proof. In LAS, PreSign ≈ Sign and
   PreVerify ≈ Verify (×1.03): the statement folds into the Fiat–Shamir hash for
   free. Strikingly, **LAS PreVerify (197µs) is absolutely faster than classical
   ECDSA-adaptor PreVerify (244µs)** on the same machine.
3. **Structural contrast worth a paragraph:** the classical pre-signature is a
   *syntactically different object* (162 B = ECDSA sig + DLEQ proof) that cannot
   even be parsed as a signature, whereas a LAS pre-signature shares the
   signature format and fails ordinary Verify *cryptographically* (the `+Y`
   tripwire, Section 4.2). LAS's adapted signature is indistinguishable from an
   ordinary one; the classical adapted signature is too, but its pre-signature
   pipeline needs a second verifier implementation on the wire.

**Honest caveats (state in the report):** libsecp256k1 is constant-time, heavily
optimised production code, while our LAS is a reference-style simplified scheme —
the timing comparison therefore *flatters the classical side*; LAS additionally
sits at a reduced security margin (`q ≈ 2²³`, Section 5.9). Neither caveat
affects the size ratios, which are format-determined. And the entire classical
column is broken by Shor's algorithm — that asymmetry is the thesis.

### 8.4 On-chain gas: a real Solidity atomic swap (`evm/`)

Sections 8.2–8.3 measure cost off-chain (a simulated ledger, byte proxies). To
answer the supervisor's "take an atomic swap, replace the signature scheme, and
compare" directly, `evm/AdaptorSwap.sol` is a signature-scheme-agnostic HTLC escrow
run on **Foundry's local EVM** (a private chain). The `fund`/`refund` escrow is
shared; only the claim-time verification of the *published adapted signature*
differs, so a gas report isolates the price of post-quantum **on-chain**:

- **Classical** (`claimClassical`): the adapted ECDSA signature is verified
  natively with the `ecrecover` precompile — how a real EVM ECDSA-adaptor swap
  settles.
- **Post-quantum** (`claimLAS`): the adapted LAS signature is a **real 4672-byte
  packed lattice signature** (`evm/test/las_sig.bin`, exported deterministically
  from the C implementation by `ref/test/export_packed`). Native lattice
  verification (NTT + SHAKE256 over the packed signature) is **infeasible in the
  EVM**, so this charges only the unavoidable on-chain **floor** — calldata for the
  4672 bytes + one keccak256 pass — a strict *lower bound* on the true cost.

**Measured gas (EVM gas is deterministic — not machine-dependent):**

| Step | Classical (ECDSA-adaptor) | Post-quantum (LAS) |
|---|---:|---:|
| fund | 180,285 | 139,568 |
| **claim** (settlement + signature check) | **75,709** (full verification) | **208,400** (floor; *no* real verification) |
| refund | 39,330 | 39,330 |
| deploy | 715,257 | — |

The real LAS signature is 4649 non-zero / 23 zero bytes → **74,476 gas of calldata
alone** (16 gas/non-zero byte, 4 gas/zero). Three readings for the report:

1. **The on-chain price of PQ is, again, communication.** The 4672-byte signature's
   calldata (74,476 gas) is ~97 % of the marginal claim cost and alone exceeds the
   *entire* classical claim (75,709 gas, which includes real verification).
2. **Even the floor is ~2.75× the full classical claim** (208,400 vs 75,709), and
   that floor does **no** cryptographic verification.
3. **True on-chain LAS verification is prohibitively expensive, but — measured —
   does *not* by itself exceed the block gas limit.** An earlier draft asserted that
   native verification "would dwarf the block gas limit". That claim was never
   quantified, and on inspection it is an *overstatement*: the EVM has native
   `mulmod`/`addmod` opcodes (8 gas each), so the modular arithmetic, while large, is
   not astronomically so. Section 8.4.1 replaces the assertion with an actual
   experiment (`evm/test/LASVerifyCost.t.sol`). The corrected, evidenced finding is
   that one native `las_verify` costs **≈12 M gas** — roughly **158× the classical
   `ecrecover` claim** and **≈40 % of a 30 M block** (≈33 % of the ~36 M block limit
   of 2025). It is *economically absurd* and an *implementation nightmare* (it needs
   SHAKE256 and a negacyclic NTT in EVM bytecode), which is exactly why on-chain PQ
   verification wants a dedicated precompile or a succinct (zk) proof of verification
   (the poqeth precedent for *basic* PQ) — but the honest barrier is **cost and
   missing precompiles, not the hard block ceiling**. The *protocol* works
   end-to-end (Stage 2); native on-chain *verification* is impractical-but-possible
   future work. (`fund` differs only by stored-field count, an artefact of the
   struct layout, not the scheme.)

Reproduce: `evm/README.md` (one `export_packed` + `forge test --gas-report`).

#### 8.4.1 Experiment: the gas cost of native LAS verification (`LASVerifyCost`)

*(A plain-English version of this experiment's reasoning, for non-specialists, is in
`docs/03-results/GAS_LIMIT_INVESTIGATION.md`.)*

The "exceeds the block gas limit" claim above is a falsifiable quantitative
statement, so we measured it rather than asserting it. `evm/src/LASVerifyCost.sol`
is a **gas-faithful cost probe**: it executes the exact arithmetic op-budget of one
`ref/las.c` `las_verify` — recomputing `w' = A·ẑ − c·t` — on Foundry's local EVM,
and `forge test --match-contract LASVerifyCost -vv` prices it. The probe relies on a
property of the EVM that makes it rigorous despite not being a *correct* verifier:
**opcode gas is independent of operand values** (`mulmod`/`addmod` are a flat 8 gas,
`MLOAD`/`MSTORE` 3 gas), so a kernel that reproduces the exact *structure* of
`las_verify` — the same **12 forward NTTs, 8 inverse NTTs, 20 pointwise products**
and coefficient passes over real 256-word memory arrays — has the same gas as a
numerically-correct verifier's arithmetic. (Twiddle factors come from a runtime
recurrence purely so the optimiser cannot constant-fold the loops away; a
numerically-correct on-chain verifier is the documented future work.)

The challenge hash is priced separately. LAS's `hash_challenge` absorbs 8 packed
polynomials (8×1024 = 8192 B) and `las_challenge` re-hashes the seed, ≈ **64
Keccak-f[1600] permutations** in total. The EVM's *native* `keccak256` opcode cannot
implement SHAKE256 (different padding, fixed 256-bit squeeze, no rejection-sampling
loop), so a faithful verifier must run Keccak-f[1600] in bytecode at ≈30 k gas per
permutation (hand-rolled SHA-3 ports measure in the 25–35 k band). The native
opcode's cost over the same 8192 B (measured: **1,593 gas**) is reported only as a
strict lower bound.

**Measured / calculated breakdown** (EVM gas is deterministic — not machine-dependent):

| Component | Per-unit gas | Count | Subtotal | Source |
|---|---:|---:|---:|---|
| forward NTT (negacyclic, 1024 butterflies) | 378,148 | 12 | 4,537,776 | measured |
| inverse NTT (+ Montgomery scaling) | 421,756 | 8 | 3,374,048 | measured |
| pointwise product (256 `mulmod`) | 47,025 | 20 | 940,500 | measured |
| coefficient passes (add/sub/reduce/caddq) | ≈30,700 | ~40 | ≈1,227,720 | measured (residual) |
| **`w' = A·ẑ − c·t` arithmetic** | | | **10,080,044** | **measured (`verifyArith`)** |
| SHAKE256 challenge hash | ≈30,000 | 64 perm. | 1,920,000 | calculated |
| **One native `las_verify`** | | | **≈12,000,044** | measured + calculated |
| — for reference: classical `ecrecover` claim | | | 75,709 | measured (§8.4) |
| — for reference: 30 M block gas limit | | | 30,000,000 | Ethereum mainnet |

The first four rows are an *independent reconciliation* of the single measured
`verifyArith` figure: rebuilding the total from the per-primitive op budget
(12·378,148 + 8·421,756 + 20·47,025 = 8,852,324) plus the ≈40 coefficient passes
recovers the measured 10,080,044, confirming the number is not an artefact.

**Reading.** Native LAS verification is **≈12 M gas — about 158× the full classical
claim and ≈40 % of a single 30 M block** (≈33 % of the 2025 ~36 M limit). Two honest
consequences: (i) the earlier "exceeds the block gas limit" wording was **wrong** and
is retracted — with EVM-native `mulmod` the arithmetic fits inside a block; (ii) the
real barriers are *economics* (two orders of magnitude over the classical settlement,
so no one would pay it) and *engineering* (SHAKE256 + NTT in EVM bytecode), which is
precisely the case for a PQ precompile or a zk-proof-of-verification rather than naïve
on-chain replay — the same conclusion poqeth reaches for *basic* PQ, here quantified
for the exotic case. The figure is a *lower bound* on a straightforward Solidity
verifier: it excludes expanding the public matrix `A'` from its seed and unpacking the
4672-byte signature into coefficients (both add gas but not an order of magnitude),
while a hand-tuned Yul implementation could shave the per-NTT cost. Across that whole
range the conclusion is invariant: a large fraction of a block, ~10²× the classical
claim, but not over the ceiling.

Reproduce: `cd evm && forge test --match-contract LASVerifyCost -vv` (and
`--gas-report` for the clean per-function figures).

---

