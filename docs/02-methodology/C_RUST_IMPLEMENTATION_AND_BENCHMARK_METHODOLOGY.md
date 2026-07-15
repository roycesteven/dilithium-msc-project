# C & Rust implementations on Dilithium / ML-DSA primitives — and the Algorithm 1 vs Algorithm 2 benchmark methodology

*One consolidated entry point, written as report source material for
**Chapter 2 (Methodology)** and feeding **Chapter 3 (Results)**: what exactly is
implemented in each language, what is reused from the CRYSTALS-Dilithium /
ML-DSA codebases, and precisely how the **ordinary lattice-based signature
(Algorithm 1) vs LAS adaptor signature (Algorithm 2)** comparison is collected,
validated and read. This file consolidates by reference — the authoritative
deep documents stay where they are:*

| Topic | Authoritative document |
| --- | --- |
| Full design + maths + results narrative | `docs/LAS.md` |
| Paper-equation → C-function mapping | `docs/02-methodology/THEORY_IMPL_BRIDGE.md` |
| Per-function reuse classification (C) | `docs/02-methodology/FUNCTION_MAP.md` |
| Rust provenance (vendored crate + additive modules) | `rust/fips204-las/LAS_PROVENANCE.md` |
| Benchmark how-to-run / how-to-read (Rust + parity table) | `rust/fips204-las/BENCHMARKING.md` |
| Step-by-step reproduction, C / Rust | `docs/A-appendix/REPRODUCE_LAS_C.md` / `docs/A-appendix/REPRODUCE_LAS_RUST.md` |
| Paper notation source of truth | `docs/paper/LAS_2020_845_NOTATION.md` |

---

## 1. What is implemented (and what "from ML-DSA" means precisely)

The scheme is **LAS** (Esgin–Ersoy–Erkin, IACR eprint 2020/845): a lattice-based
adaptor signature whose underlying ordinary signature is the paper's
**Algorithm 1 — a *simplified* Dilithium** (the paper's own framing, §2.2/§3.2).
It is **not** full CRYSTALS-Dilithium and not the FIPS 204 ML-DSA standard;
what *is* taken from those codebases is the **lattice arithmetic layer**
(NTT, polynomial ops, SHAKE, reductions), reused as-is:

- **C:** vendored **pq-crystals/dilithium reference code** (upstream commit
  `2374d22`) — CRYSTALS-Dilithium is the scheme NIST standardised as ML-DSA.
- **Rust:** vendored **`fips204` crate** (integritychain, commit `c948882`,
  v0.4.6) — a pure-Rust **FIPS 204 ML-DSA** implementation.

Core mechanism (Algorithm 2): the statement is folded into the Fiat–Shamir
hash — Sign uses `c = H(pk, w, M)`, **PreSign uses `c = H(pk, w + t′, M)`**
(statement `Y = t′`). PreSign rejects at the tighter bound `‖ẑ‖∞ > γ−κ−1`
(Sign: `‖z‖∞ > γ−κ`), so the adapted response `z = ẑ + r′` (witness
`‖r′‖∞ ≤ 1`) still clears ordinary Verify; `Ext` recovers `s = z − ẑ`.
Parameters: `d = 256`, `γ = κ·d·(n+ℓ)`, modulus `Q = 8380417 ≈ 2²³`
(paper-sanctioned substitution for `q ≈ 2²⁴`: "only the size of the modulus is
important", §3.2). Headline setting: **Simplified Dilithium-III engineering
set `(n, ℓ, κ) = (6, 5, 49)`** — an L3-like scaling setting, not a formal
NIST-level claim.

## 2. The C implementation (primary)

**Reuse discipline: zero upstream Dilithium functions modified** (the
per-function audit is `docs/02-methodology/FUNCTION_MAP.md`; the diff-level view is
`docs/02-methodology/CODE_DIFF_VIEW.md`). LAS is layered as additive modules that *call*
`poly.c`/`ntt.c`/`reduce.c`/`fips202.c` primitives:

| Module | Role |
| --- | --- |
| `ref/setup.{c,h}` | shared construction parameters + `public_params` (`pp = (A,H)`) + `setup_public_params` (A expanded once from a public seed) |
| `ref/las_types.h` | the six protocol object types (`public_key`, `secret_key`, `signature`, `statement`, `witness`, `pre_signature`), each owned by one layer |
| `ref/relation.{c,h}` | the hard relation: `relation_gen`/`relation_gen_seed` → `(statement Y = t′, witness r′)` |
| `ref/basesig.{c,h}` | **Algorithm 1 only**, and the ONE canonical ordinary signature of the build (Definition 3: the adaptor *inherits* KeyGen/Sign/Verify from it): `base_keygen`/`base_sign`/`base_verify` + seeded-KAT variants (`base_keygen_seed`, `base_sign_det`), `base_attempts`. No statement anywhere — also the fair comparison partner, kept out of `las.c` so neither path can contaminate the other |
| `ref/las.{c,h}` | **Algorithm 2 only**: `las_presign/preverify/adapt/ext` + `las_presign_det`, `las_attempts` counter, `las_expected_attempts` (exact restart-rate theory for the benchmark gate) |
| `ref/serialize.{c,h}` | the wire codec: six typed pack/unpack pairs, wire `c_tilde ‖ BitPack(z)`, validating pk/sk decoders (`base_verify_packed`, the byte-level verifier, lives in `basesig.c`) |
| `ref/amhl.{c,h}`, `ref/chain.{c,h}` | multi-hop locks (optional tier) and the simulated ledger for the atomic-swap demo — Stage 2, not part of the Stage-1 comparison |
| `ref/test/…` | correctness tests (1000-iteration contract, KATs, serde/tamper) and the benchmark drivers (primary: `bench_levels.c`; secondary: `bench_compare.c`, `bench_app.c`, `bench_classical.c` — see `docs/02-methodology/FUNCTION_MAP.md` §3.5) |

## 3. The Rust implementation (independent cross-language confirmation)

Same methodology, second language: the vendored `fips204` crate is untouched
except the `pub mod` registration lines and one dev-dependency bump (criterion
0.4.0 → 0.8.2); LAS is additive modules `src/setup.rs`, `src/las_types.rs`,
`src/relation.rs`, `src/serialize.rs`, `src/basesig.rs`, `src/las.rs` — the same
layering as the C build — calling the crate's `ntt`, `mont_reduce`, SHAKE and
`conversion::bit_pack` as-is (full table: `rust/fips204-las/LAS_PROVENANCE.md`).

**The two implementations are locked together by a KAT:** the Rust port
reproduces the C pinned SHAKE256 digest (`bb6ad0da…260c`, 4 deterministic
vectors, D3 set) **byte-for-byte** — `cargo test --test las_kat` vs
`make test/test_kat3`. This works because the crate's Montgomery reduction is
bit-identical to C's, every hashed value is canonicalised, and the C
`sample_Sgamma` SHAKE block-discard behaviour is replicated exactly. The KAT
lock is what makes the cross-language benchmark a comparison of *the same
algorithm*, not two look-alikes.

## 4. Benchmark methodology — ordinary (Algorithm 1) vs adaptor (Algorithm 2)

### 4.1 What is compared (the primary Stage-1 comparison)

Per-operation pairing, at matched parameters, on shared primitives, from one
consistent benchmark state:

| Adaptor operation | Compared against | Why |
| --- | --- | --- |
| PreSign | Sign | same FSwA loop, + `w + t′` fold and tighter bound |
| PreVerify | Verify | same recompute-and-hash, + `w′ + t′` fold |
| Adapt (incl. its mandatory internal PreVerify) | Verify | the completing step a settling party pays |
| Ext | — (no Algorithm-1 analogue) | reported separately |

**Per-operation timing is the primary result — never cumulative/end-to-end
time.** The four-parameter-set sweep (paper/D2/D3/D5) is a secondary
fairness/scaling axis only.

### 4.2 Validity safeguards (identical in C and Rust)

1. **Module isolation:** the two paths live in separate modules
   (`basesig.c`/`las.c`; `basesig.rs`/`las.rs`).
2. **Contract gate before any timing** — the run refuses to measure unless:
   the ordinary signature verifies; the pre-signature pre-verifies but
   **fails** ordinary Verify (statement-binding tripwire); the adapted
   signature passes the *independent* base verifier; Ext recovers the witness
   exactly. No failure path is ever timed.
3. **Run-validity rejection gate (2026-07-06):** every run counts the timed
   Sign/PreSign calls and their attempt-counter deltas, and **hard-asserts**
   the measured attempts/call against the exact expectation
   `E[attempts] = ((2·bound−1)/(2γ+1))^{−(n+ℓ)d}`
   (derived and verified against 2020/845 Table 1, Fact 1, Alg. 1 step 11 /
   Alg. 2 step 6; at D3: **Sign 2.71875, PreSign 2.77483**) within a 5σ
   statistical tolerance. A run whose restart rate deviates aborts with
   `FAIL` instead of producing plausible-looking but invalid evidence. The
   two theory values differing (the `−1` bound) means the gate also
   re-confirms both rejection bounds on every run.

### 4.3 Collection scheme (mirrored between the languages, 2026-07-06)

| Element | C driver `ref/test/bench_levels.c` | Rust driver `examples/bench_levels.rs` | Rust Criterion `benches/las_bench.rs` |
| --- | --- | --- | --- |
| Repetitions | 5 × 500 (sign-class) / 1000 (verify-class), mean ± sample SD | identical | 300 samples / 60 s per op, warm-up, outliers, bootstrap 95% CI |
| Workload | fixed pp seed `00..1f`, fixed 33-byte message (same bytes both languages → identical public parameters) | identical | identical |
| Rejection counters | `base_attempts`/`las_attempts` + gate | `BASE_ATTEMPTS`/`LAS_ATTEMPTS` + gate | counters + gate |
| Per-attempt (rejection-normalised) diagnostic | printed | printed | — |
| Extra diagnostics | attempt distribution (min/max/p50/p95), component microbenchmarks, packed sizes, norm margins | — | HTML report, saved-baseline significance diffing |

Residual documented difference: keygen/mask randomness (C: system RNG inside
the scheme code; Rust: fixed-seed ChaCha8) — statistically equivalent
workloads, checked per run by the gate.

### 4.4 Reading rules (defensibility)

- **Sign-class statistics are multimodal by design** (FSwA restarts,
  acceptance ≈ 1/e ≈ 37 %/attempt): quote the **mean** (it includes restarts —
  what a protocol pays). Verify-class: quote the **median** (Criterion).
- **Never compare raw C µs with raw Rust µs** (different compilers/profiles) —
  the cross-language claim is that the **overhead ratios agree**. Same-machine
  runs on different days can differ ~2.5× uniformly (WSL2 power state);
  ratios survive, absolute numbers do not travel without the §14.5 machine
  metadata.
- **Restart-luck pitfall:** below ~500 sign iterations the PreSign-vs-Sign
  ratio is dominated by attempt-count luck (a 100-iteration pilot showed a
  spurious +28 %); the per-attempt diagnostic divides the restarts out.
- Variance provenance is proven, not assumed: from saved Criterion sample
  data, sign-class variance shrinks ∝ 1/iterations with ≈ 0 lag-1
  autocorrelation (i.i.d. restarts), while verify-class variance is flat and
  autocorrelated (machine drift) — so "PreSign ≈ Sign" is not an artefact of
  a noisy floor.

### 4.5 Communication methodology

Sizes are **measured packed bytes** (`ref/serialize.c` bit-packing; per-set
field widths), never formula-only claims, split per component (`pk = t`,
`sk = r`, `Y = t′`, witness `r′`, challenge `c`, response `z`). The Rust
`examples/size_report.rs` **hard-asserts** its packed sizes equal the C
evidence row (`communication_components.csv`, L3), making the
communication-cost claim cross-language too.

## 5. Measured snapshot (provenance-cited; regenerate before quoting in the report)

- **C evidence run `20260627_135247`** (pre-mirror scheme 10 × 1000; superseded
  by the next `run_benchmark_suite.sh` run under §4.3): at Simplified
  Dilithium-III — PreSign **+6.7 %** vs Sign, PreVerify **+3.1 %** vs Verify,
  Adapt **+8.1 %** vs Verify, Ext ≈ 101 µs. Sizes: pk/Y 4416 B, sk/witness
  704 B, c_tilde 32 B, z 6688 B (99.52 % of the 6720 B signature);
  signature = pre-signature = adapted signature.
- **Rust Criterion run 2026-07-05** (0.8.2, 300/60, baseline `criterion082`,
  `bench_las_criterion.log`): PreSign vs Sign **statistically
  indistinguishable** (−0.2 % means / +0.4 % medians, CIs almost coincide);
  PreVerify **+0.4 %**, Adapt **+3.3 %** (medians, disjoint CIs); Extract
  ≈ 27 µs; ordering Verify < PreVerify < Adapt resolved.
- **Attempt counters** (both languages): ≈ 2.7 attempts/call at ≈ 37 %
  acceptance, matching the exact theory above and the paper's "average number
  of restarts ≈ e" design target (§3.2).
- Joint conclusion for the report: **the adaptor layer costs at most a few
  percent per operation and zero bytes on-chain** — the paper's "essentially
  as efficient as an ordinary lattice-based signature" claim, reproduced
  independently in two languages.

## 6. Where this feeds the report

| Report chapter | Content from this document |
| --- | --- |
| Ch. 2 Methodology | §§1–4 (scheme, reuse discipline, two implementations, comparison design, validity safeguards, reading rules) |
| Ch. 3 Results | §5 pattern + regenerated evidence (`evidence/latest/`, `bench_las_criterion.log`) |
| Ch. 4 Evaluation | §4.4 defensibility arguments (variance provenance, restart-luck, ratios-not-µs) |
| Appendix | reproduction guides, code snippets, exact-number tables |
