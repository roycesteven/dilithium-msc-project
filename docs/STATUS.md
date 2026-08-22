# Project STATUS & Test Checklist — single source of truth for "what's done / tested"

*Living tracker. Updated 2026-07-25 (**Meeting-7 pivot: Stage 2 retargets from the EVM
to Bitcoin/UTXO — see §6a and `las-context-consolidated.md` §16**; previous update
2026-07-16, Meeting-5 explainability/reproducibility package, see §1). Read this first
to see, at a glance, every
deliverable, whether it is **built**, **tested**, and **documented**, plus the one
command that reproduces each claim. Maps every item to the Meeting-2 objectives
(`las-context-consolidated.md`) and to the report's assessment criteria (`CLAUDE.md`).*

Legend: ✅ done & verified · 🟡 partial / proxy · ⬜ not done (future work).

**New to the project / explaining it to someone?** Read, in order:
`docs/01-introduction/LAS_WALKTHROUGH.md` (what & why, no maths) →
`docs/04-evaluation/PROJECT_HISTORY_EXPLAINED.md` (the step-by-step build order: which C files and
functions were written first, and why) →
`docs/03-results/GAS_LIMIT_INVESTIGATION.md` (the on-chain gas experiment in plain English).

---

## 1. Deliverable × status matrix

| # | Deliverable | Built | Tested | Documented | Reproduce (`cd ref`) |
|---|---|:--:|:--:|:--:|---|
| D1 | LAS scheme: KeyGen/Sign/Verify + PreSign/PreVerify/Adapt/Ext (variant B, simplified) | ✅ | ✅ | ✅ | `make test/test_las3 && ./test/test_las3` |
| D2 | 8-point adaptor contract, 1000 iters, modes 2/3/5, 100% correct | ✅ | ✅ | ✅ | `./test/test_las2 ./test/test_las3 ./test/test_las5` |
| D3 | Atomic swap (2-party, 2-chain), narrated + asserted — **rewritten 2026-07-19 to paper §4.1 Fig. 1 verbatim** (witness holder commits first, π gate, byte-level tripwire) | ✅ | ✅ | ✅ | needs LaZer (README "π + atomic swap"); `make test/test_swap3 && ./test/test_swap3` |
| D4 | Scriptless HTLC ledger: swap / timeout-refund / same-Y PCN | ✅ | ✅ | ✅ | `make test/test_pcn3 && ./test/test_pcn3` |
| D6 | Byte serialisation + validating decoder + `base_verify_packed`; swept across parameter sets (paper + D2/D3/D5, parameter-derived `z` width) | ✅ | ✅ | ✅ | `make test/test_serde3 test/test_serde_l2 test/test_serde_l3 test/test_serde_l5 && ./test/test_serde3 && ./test/test_serde_l2 && ./test/test_serde_l3 && ./test/test_serde_l5` |
| D7 | Deterministic API + pinned KAT (reproducibility, C4) | ✅ | ✅ | ✅ | `make test/test_kat3 && ./test/test_kat3` |
| D8 | Benchmark 1 — per-op timings + **direct** rejection rate | ✅ | ✅ | ✅ | `make test/bench_las3 && ./test/bench_las3` |
| D9 | Benchmark 2 — LAS vs optimised Dilithium-3 (context; superseded as headline by D20) | ✅ | ✅ | ✅ | `make test/bench_compare3 && ./test/bench_compare3` |
| D11 | Benchmark 4 — **classical adaptor baseline** (ECDSA, same machine) | ✅ | ✅ | ✅ | clone secp256k1-zkp (README.md §4.1), `make test/bench_classical && ./test/bench_classical` |
| D12 | Function map (reused/modified/added; 0 upstream modified) | ✅ | n/a | ✅ | `docs/02-methodology/FUNCTION_MAP.md` |
| D13 | Reproducibility README + recorded provenance/toolchain | ✅ | n/a | ✅ | `README.md` |
| D14 | Report draft (~8k words, B4 skeleton) | 🟡 | n/a | 🟡 | `report/REPORT_DRAFT.md` (v0.1 — superseded by LaTeX scaffold D22) |
| D15 | On-chain gas: real Solidity swap (classical vs LAS sig) **+ measured native-verify cost-probe** | ✅ | ✅ | ✅ | `cd ref && make test/export_packed && ./test/export_packed ../evm/test/las_sig.bin; cd ../evm && forge test --gas-report && forge test --match-contract LASVerifyCost -vv` |
| D20 | **Primary fair benchmark** (corrected 2026-06-22; base path modularised to `basesig.c` 2026-06-23): separate base path (`basesig.c`) vs LAS adaptor path (`las.c`) — adaptor overhead (PreSign/Sign, PreVerify/Verify, Adapt/Verify, Ext separate) + cross-verify contract; official Dilithium = CONTEXT only ("not algorithm-matched"); ≥5 runs mean±SD; component sizes | ✅ | ✅ | ✅ | `make test/bench_levels_paper test/bench_levels2 test/bench_levels3 test/bench_levels5 && ./test/bench_levels_paper …`; `docs/LAS.md §8.1` |
| D23 | **Correctness-contract harness** (itemised 8-point PASS): PreSign→PreVerify, tripwire, Adapt→Verify, Ext exact, tampered msg/sig, malformed bytes, deterministic | ✅ | ✅ | ✅ | `make test/test_contract3 && ./test/test_contract3` |
| D24 | **Base-signature correctness test** (`basesig.c`, **CHECK**-gated, 1000 iters × paper/2/3/5): honest verify, tamper/wrong-key rejection, cross-module equivalence with `las.c`, cross-path interlock (tripwire + adapted-verifies-under-base + exact Ext), + 4 negative tests (wrong statement, wrong witness, tampered pre-signature, tampered adapted signature) | 🟡 ready (build via `make`) | ⬜ run by Royce | ✅ | `make test/test_basesig_paper test/test_basesig2 test/test_basesig3 test/test_basesig5 && ./test/test_basesig_paper` |
| D25 | **Fig. 1 proof of knowledge π** (2026-07-19; Royce-directed scope extension): `relation_zk.{c,h}` + `relation_zk_lazer.{c,h}` over vendored LaZer; binary decomposition `[A\|−A]`; knowledge error ≤ 2⁻¹²⁷; measured proof ≈ 30.7 KB off-chain; committed params `relation_zk_params.h`; Rust twin `relation_zk.rs` (`--features relation-zk`, same C bridge) | ✅ | ✅ | ✅ | needs LaZer (README "π + atomic swap"); `make test/test_zkp3 && ./test/test_zkp3`; Rust: `cargo test --offline --features relation-zk --test las_zkp --test las_swap` |
| D26 | **Stage-2 UTXO atomic swap, three configurations** (Meeting-7): `rust/las-swap/` — UTXO ledger per paper §4 (signature algorithm as a parameter), Fig. 1 driver verbatim, honest path + timeout/refund; (1) classical ECDSA adaptor, (2) LAS + Groth16 over `∃r: Ar=t ∧ ‖r‖∞≤1` (own R1CS circuit, arkworks 0.4/BN254), (3) LAS + LaZer. Measured on time + communication incl. off-chain messages; rejection gate wired | ✅ | ✅ | ✅ | `cd rust/las-swap && cargo run --release --bin bench_swap --features secp256k1,groth16,relation-zk`; evidence `evidence/stage2/20260725_202359/`; write-up `report/latex/chapters/03-results.tex` §3.6 |
| D27 | **ML-DSA adaptor experiment, complete** (Meeting-8, Wang: *"just try it"*; 2026-08-03): LAS built on NIST FIPS 204 **as specified** — hint vector, Power2Round and the high/low-bit split all **enabled** — as a full scheme with its own wire format, in three binaries: (a) `test_mldsa_hint` diagnostic (which feature breaks a naive port), (b) `test_mldsa_las` itemised contract **13/13 at ML-DSA-44/65/87** incl. 4 tamper rejections + malformed-input rejection + determinism, (c) `bench_mldsa_compare` head-to-head against the simplified scheme with **both constructions in one binary**. Converts the project's *assertion* into a *demonstration* and **corrects it**: the signer's side must be modified (naive port fails PreVerify 0/200), the **verifier's must not** (unmodified `crypto_sign_verify` accepts the adapted signature 200/200). Cost: adaptor overhead single-digit % on both constructions; ML-DSA halves the signature (3309 vs 6736 B) at equal compute, but the statement `Y` is byte-identical (4416 B), so the swap payload only reaches 0.69×. Functional demonstration only — security of committing to `HighBits(w+Y)` is **not** analysed | ✅ | ✅ | ✅ | `./scripts/run_mldsa_hint_experiment.sh` → `evidence/mldsa_hint/<ts>/`; write-up `docs/03-results/MLDSA_HINT_EXPERIMENT.md` |
| D21 | **Two-branch code-diff view** (Meeting-3): `dilithium-baseline` (pristine) vs `main`; 0 upstream sources changed | ✅ | n/a | ✅ | `git diff --name-status dilithium-baseline main -- ref/`; `docs/02-methodology/CODE_DIFF_VIEW.md` |
| D22 | **LaTeX report scaffold** (Meeting-3): muthesis.cls, by chapter, official title, real benchmark tables, builds to PDF | 🟡 | n/a | ✅ | `cd report/latex && make` (TODOs: student id, figure, machine-of-record) |
| D16 | ~~Parameter migration to paper's q≈2²⁴~~ — **DROPPED** (2026-08-03): NIST FIPS 204 is the parameter authority, so `q = 8380417` is *correct*, not a shortfall. Superseded by the ML-DSA hint experiment (D27) | n/a | n/a | n/a | — |
| D17 | On-chain LAS *verification* — **complete native Solidity verifier** implemented (`evm/src/LASVerifier.sol`, `library LASVerify`; reuses vendored ZKNox ETHDILITHIUM primitives SHAKE256/NTT/SampleInBall; validated end-to-end vs C — **accepts** golden adapted sig, **rejects** tamper; wired into `AdaptorSwap.claimLASVerified`, securely bound by a fund-time `keccak256(A',t,M)` commitment) | ✅ | ✅ | ✅ | `cd ref && make test/export_verify_vector && ./test/export_verify_vector ../evm/test/vectors; cd ../evm && forge test` — **measured ≈56.5M gas**; > EIP-7825 per-tx cap (16,777,216) ⇒ a *deployed* precompile / zk proof / Naysayer scheme remains future work |
| D18 | Second LAS-family scheme (application-layer) | ⬜ | ⬜ | n/a | — *optional stretch* |
| D19 | Video (6–8 min) | ⬜ | ⬜ | n/a | see §5 storyboard |

**Headline:** every *required* Meeting-2 deliverable (Stage 1, Stage 2, both
benchmark baselines, function map, reproducibility) is ✅ done & tested.
Serialisation, KATs, **and the on-chain Solidity gas benchmark (D15)** are ✅ done.
What remains is the **report polish** (D14), the **video** (D19), and the
explicitly-optional tier (D16–D18).

### Meeting-4 Stage-1 *presentation* (done 2026-06-27 · evidence run `20260627_135247` · commit `aba9003`)

Meeting 4: the numbers were already right; the *presentation* had to become
defensible (self-explanatory labels, parameters on the page, per-operation timing,
captions). Now done:

- **Self-explanatory figure labels.** `paper-derived` / `L2-like` / `L3-like` /
  `L5-like` replaced by **LAS-2020/845 reference** and **Simplified Dilithium-II/III/V**
  in `scripts/plot_las_benchmarks.py` + `scripts/plot_las_paper_figures.py`; **all**
  figure artefacts in `evidence/latest/{paper_package,tables,debug_figures,application_package,appendix_package}`
  regenerated from the captured logs — **numbers unchanged, labels only** (numeric CSVs
  verified byte-identical).
- **`ordinary signature` → `basic signature`** everywhere (figures, sidecar `.tex`,
  `KEY_FINDINGS*`); the `statement Y = t' (LAS-added)` label is now just `statement Y = t'`.
- **Full parameters go in the report caption, not the plot body** (supervisor steer):
  in-figure setting annotations carry only the short scientific name; `n, ℓ, M, κ, γ,
  N, q` and their values live in `report/latex` captions that point at the new
  parameter tables.
- **Report sync (`report/latex/`):** added a complete parameter **notation+values**
  table (`tab:notation`) and a fuller parameter table (`tab:params`); a **key overhead
  summary at the Simplified Dilithium-III target** (`tab:overhead`); a **complete
  communication+computation vs basic-signature table** (`tab:complete-l3`); KeyGen now
  documented as **shared** between the basic signature and LAS; per-op/levels/components/
  classical tables + abstract/conclusion synced to evidence run `20260627_135247`
  (headline overhead now ≤ ~8 %). Report passes a clean `pdflatex` error-check (final
  PDF build is Royce's).
- **Still open (Meeting-4 named deliverable):** open the clean-Dilithium → LAS **PR**
  and invite Wang — supporting artefacts already exist (`dilithium-baseline` branch,
  `docs/02-methodology/CODE_DIFF_VIEW.md`, `docs/02-methodology/FUNCTION_MAP.md`).

### Meeting-5 Stage-1 *explainability + reproducibility* package (2026-07-06 · core artefacts delivered 2026-07-17)

Meeting 5 did not sign off Stage 1 either. The numbers/implementation are accepted;
what's missing is a package that lets *another person understand, verify, and
reproduce* the work (`las-context-consolidated.md §15`). Now built: the diagrams,
the 1–2 slide summary, the C⇄Rust size/rejection tables, and the short-README split
(rows below). `evidence/latest` + the report were refreshed on the fair Stage-1 run
`20260717_084012`. Still open: classical-overhead columns (M5.9), figure-set trim
(M5.10), and the machine statement pinned into every caption (M5.1).

| M5 item | Status | What exists / what's missing |
|---|:--:|---|
| M5.1 Machine/env statement travelling *with* the figures (WSL OK) | 🟡 | CPU/OS/compiler/flags are in the methodology (§14.5 / `docs/LAS.md`); must be pinned into figure captions + the short README |
| M5.2 High-level **API-flow diagram** (basic `KeyGen/Sign/Verify` → LAS `+PreSign/PreVerify/Adapt/Ext`, where `Y` is set up) | ✅ | **drawn** (Mermaid) in `docs/02-methodology/walkthrough/00-diagrams-and-summary.md` — base-sig flow + LAS flow with `Y` set up in `relation_gen` |
| M5.3 **Repo-structure diagram** (base C / LAS C / **LAS Rust**; reused primitives marked) | ✅ | **drawn** (Mermaid, Diagram 3) in `00-diagrams-and-summary.md`; reused primitives boxed, C⇄Rust modules mirrored |
| M5.4 Reused/modified/new components table, **high-level front** | ✅ | Table A (one-glance reused/new front) in `00-diagrams-and-summary.md`, over `FUNCTION_MAP.md`'s per-function detail |
| M5.5 High-to-low written "what changed (simplified Dilithium → LAS)" summary | 🟡 | pieces in `UPSTREAM_TO_LAS_WALKTHROUGH.md` + `THEORY_IMPL_BRIDGE.md`; consolidate top-down |
| M5.6 Benchmark methodology stated (Criterion samples/warm-up/SD; C repeated-run mean±SD) | ✅ | Rust criterion @300 + C driver mirror (see memory `benchmark-rejection-gate`); ensure it's written up in report §methodology |
| M5.7 **C ⇄ Rust size cross-check** (pk/sig sizes match across implementations) | ✅ | Table B (size-equality, D3+paper sets) in `00-diagrams-and-summary.md`; `size_report.rs` hard-asserts C=Rust; KAT `bb6ad0da…260c` |
| M5.8 Rejection sampling **theory + measured**, in own words | ✅ | Table C (theory vs measured) + plain-language justification in `00-diagrams-and-summary.md`; fresh L3 measured base 2.715 / PreSign 2.716 (run `20260717_084012`) |
| M5.9 Classical-adaptor comparison as **overhead columns** in the compute + comms tables (refines M4.8 — pulled into Stage-1) | 🟡 | `bench_classical` numbers captured (D11); need the overhead/increase columns + security-level (≈ Dilithium L2) caveat |
| M5.10 Reduce to the important figures (compute / comms / rejection / C-vs-Rust); rest → appendix | 🟡 | overlaps M4.6's 3–4-figure trim; add the C-vs-Rust figure to the keep-set |
| M5.11 **Short README** (key commands) + extended README | ✅ | `README.md` = short quick-start; `README_EXTENDED.md` = full detail; per-language reproduce guides in `docs/A-appendix/` |
| M5.12 **1–2 slide summary** with a diagram for the next meeting | ✅ | `report/slides/stage1_summary.html` (self-contained; published artifact) — slide 1 base→LAS API flow, slide 2 evidence tiles |
| M5.13 Keep the **simplified** LAS (full ML-DSA LAS = future work only) | ✅ | already the settled scope; note in report as a deliberate decision |
| M5.14 Stage-2 preview (Foundry local chain: classical adaptor workflow first, then swap in LAS) | ⬜ | deferred until Stage 1 is signed off; captured in `las-context-consolidated.md §15.14` |

---

## 2. Test inventory — what each test actually proves

All run with zero compiler warnings under
`-Wall -Wextra -Wpedantic -Wmissing-prototypes -Wredundant-decls -Wshadow -Wvla -Wpointer-arith`.

| Test binary | Iters / scope | Properties hard-asserted |
|---|---|---|
| `test_las{2,3,5}` | 1000 each, modes 2/3/5 | PreVerify accepts; **Verify rejects pre-sig (tripwire)**; adapted σ verifies; Ext recovers y **exactly**; Sign/Verify round-trip; bit-flip forgery rejected |
| `test_swap3` | 1 narrated run | **Fig. 1 verbatim incl. π**: Bob's abort gate (π + PreVerify); π rejected against any other statement; raw pre-signature *bytes* unspendable (both legs); two-leg atomicity; post-Ext `‖y′‖∞ ≤ 1 ∧ y′ = y` |
| `test_zkp3` | 1 statement + sweeps | π completeness; single-byte tamper sweep rejected; wrong-statement rejected; non-ternary (R′_A) witness refused by the prover. Rust twins: `las_zkp.rs`, `las_swap.rs` (same C bridge) |
| `test_serde3` | 256 random + exhaustive | round-trip pk/sk/sig; verify-from-bytes; tripwire survives packing; **all 4640 byte-flips rejected at Verify** (wire = `c_tilde ‖ BitPack(z)`); malformed pk/sk input rejected |
| `test_kat3` | 4 fixed vectors | full deterministic pipeline; byte-identical re-runs; **pinned SHAKE256 digest match** |

KAT digest (pinned, C = Rust): `bb6ad0dab998c1f90ca4d3cc0f5d3dfa723e89f79aff18fce2698a08c96e260c`.

---

## 3. Measured numbers (this machine — re-run to refresh for the report's Appendix B)

*Timings are wall-clock µs/op, `-O3`, mode 3, ≥2000 iters; absolute values are
machine-dependent (and vary a few % run-to-run) — the **ratios** are the result.*

**Per-op (bench_las3, representative):** Setup ≈49, KeyGen ≈61, Sign ≈684,
Verify ≈163, PreSign ≈703, PreVerify ≈180, Adapt ≈186, Ext ≈58.
Rejection sampling **measured directly**: Sign 2.72 attempts (36.8%), PreSign 2.81
(35.6%) — matches `(1−κ/γ)^{(n+ℓ)N} ≈ e⁻¹ = 36.8%`.

**Fair primary (bench_levels, evidence run `20260717_084012`) — Simplified
Dilithium-III (target), core tier, µs mean±SD, 5×(500 sign / 1000 verify):**
Setup 94±11, KeyGen 92±2, Sign 852±25, Verify 199±6, PreSign 866±41 (+1.6% vs Sign),
PreVerify 210±9 (+5.1% vs Verify), Adapt 217±5 (+9.0% vs Verify), Ext 84±6.
Acceptance ≈37%/attempt at every setting. Headline adaptor overhead ≤ ~9% across all
four settings (paper/L2/L3/L5). These are the numbers now in `report/latex`
(`tab:overhead`, `tab:overhead-levels`, `tab:complete-l3`), refreshed by
`STAGE1_ONLY=1 scripts/run_benchmark_suite.sh` (Stage-2 targets skipped while stale).

**Classical baseline (bench_classical, evidence run `20260627_135247`):** KeyGen 34,
Sign 45, Verify 69, PreSign 207, PreVerify 268, Adapt 4, Ext 36; sizes pk 33, sk 32,
sig 64 (70 DER), pre-sig 162.

**Sizes (bytes):** LAS packed pk/Y **2944**, sk/witness **512**, sig=pre-sig
**4640** (32-byte `c_tilde` + `BitPack(z)`; in-memory `sizeof` 4096/8192/8224).
Classical: pk 33, sk 32, sig 64 (70 DER), pre-sig 162. Dilithium-3: pk 1952,
sk 4032, sig 3309.

**On-chain gas (EVM, deterministic — `evm/`, `forge test --gas-report`; re-measured
2026-07-22 at the correct D3 set, n=6/ℓ=5, 6720-B sig):** classical claim (settle +
ecrecover) 75,709; LAS claim (settle only, **no** lattice verify) 289,930 (calldata
107,088 for the real 6720-B sig + keccak); fund 180k/140k; refund 39,330. LAS
settlement = 3.8× the full classical claim. **Native verification cost — MEASURED
end-to-end:** one full on-chain LAS verified settlement (`claimLASVerified`, running
the complete `LASVerify.verify`) is **56,538,682 gas measured** — **≈746× a complete
classical claim** and **≈3.4× the EIP-7825 per-transaction cap of 16,777,216**, so it
cannot execute as a single mainnet transaction. It stays *below* the 60 M block gas
limit, so the binding ceiling is the **per-transaction cap**, not the block.
Authority: `docs/03-results/GAS_LIMIT_INVESTIGATION.md` §5.

> **Superseded figure — do not reuse.** An earlier ≈16.7 M total (13.93 M measured
> arithmetic + 2.76 M calculated SHAKE256, quoted as "≈55.6 % of a 30 M threshold, does
> not exceed it") was an op-budget **lower-bound estimate**, not a measurement of the
> transaction. The real cost is larger because it also pays for the Solidity SHAKE256,
> bit-unpacking `z`, canonical packing, and ABI/memory overhead. The arithmetic-only
> sweep (D2 9.33 M / D3 13.93 M / D5 20.04 M, `verifyArithLevel2/3/5`) remains valid
> **as an arithmetic-only figure** and is supporting material.

**Three headline findings** (the report's centrepieces): (i) price of PQ is
*communication* (×29–89 sizes; on-chain calldata 107k gas > whole classical claim)
not *computation* (sub-ms, ≤×20 time); (ii) LAS adaptor overhead ≈0 (PreSign≈Sign,
PreVerify≈Verify) vs the classical adaptor's ~4× DLEQ overhead — LAS PreVerify even
absolutely faster; (iii) on-chain, the swap protocol runs end-to-end, but native LAS
*verification* costs **56,538,682 gas measured (≈746× a complete classical claim)** and
is **≈3.4× the EIP-7825 per-transaction cap (16,777,216)**, so it cannot run as
one mainnet transaction — it stays under the 60 M *block* limit, so the binding ceiling
is the per-transaction cap. It needs a SHAKE/NTT precompile or a zk proof. This is a
*missing-precompile* gap, not an algorithmic defect of LAS: ECDSA is cheap on-chain
precisely because Ethereum ships a subsidised native implementation of it. **This
finding is what motivated the Meeting-7 pivot of Stage 2 to a UTXO chain**
(`docs/02-methodology/STAGE2_UTXO_SWAP_PLAN.md`).

> ⚠️ For the final report, re-run all four benchmarks **in one session** on the
> submission machine and paste the verbatim output into Appendix B with CPU/OS/date.
> The numbers above and in `docs/LAS.md §8` are representative, not a fixed run.

---

## 4. Assessment-criteria coverage (where each mark is earned)

| Criterion (weight) | Where it is satisfied | Evidence |
|---|---|---|
| Abstract (5%) | `report/REPORT_DRAFT.md` Abstract | executive summary w/ key results |
| Introductory material (20%) | report §1 + `docs/LAS.md §1, 1.1` | quantum threat, 2×2 framing, related work (LAS / survey / poqeth), objectives O1–O5 |
| Methodology (20%) | report §2–3 + `docs/LAS.md §2–5`, `docs/02-methodology/THEORY_IMPL_BRIDGE.md`, `docs/02-methodology/FUNCTION_MAP.md` | variant-B design, simplified-scheme & param justification, alternatives rejected, reused-vs-added table |
| Evaluation (20%) | report §4 + `docs/LAS.md §8–8.3` | **two baselines**, 2×2 matrix, direct rejection measurement, parameter-sensitivity sweep, correctness 100% |
| Conclusion (10%) | report §6 | conclusions vs objectives + ordered future work |
| Format/structure (5%) | report headings, numbered tables/figs, refs | — (needs figure redraw + reference formatting pass) |
| Project achievement (20%) | whole artefact | first public LAS; first exotic-PQ-on-chain; 0 upstream fns modified; zero-warning build; tamper/KAT robustness |

---

## 5. Video storyboard hook (6–8 min — D19, not started)

Suggested arc that *complements* (not repeats) the report: (1) the 2×2 quadrant
and why the exotic-PQ cell is empty [30s]; (2) live `./test/test_contract3` showing the
itemised 8-point adaptor contract scrolling to PASS [90s]; (3) animate the swap
cascade (Fig. 3) with the "publishing σ reveals y" reveal [120s]; (4) the
"price of PQ" bar chart — sizes ×29–89 vs near-flat adaptor overhead, the
counter-intuitive inverted-overhead result [120s]; (5) `test_serde3` tamper test
(all 4640 flips rejected) + KAT digest match as the "it's real, reproducible"
beat [60s]; (6) limitations + future work [30s]. Talking-head in corner.

---

## 6. Immediate next actions (in order)

**Meeting-7 (2026-07-24) is the latest word, and it SUPERSEDES the Meeting-5 ordering
below: the active priority is now the Stage-2 application on Bitcoin/UTXO — three
configurations — plus the report restructure.** Authority:
`las-context-consolidated.md` §16 · `meeting7_cleaned_transcript.md`. Stage-2 code is
no longer gated behind Stage-1 sign-off; Wang set it as the plan for the coming week.

### 6a. Meeting-7 priorities (do these first)

1. **Choose the base repo** — a maintained classical atomic-swap implementation whose
   architecture can be reused with the cryptography replaced. Avoid Monero's
   privacy-preserving complexity; prefer two similar UTXO chains. Candidates and
   maintenance status surveyed in
   `docs/04-evaluation/CLASSICAL_ADAPTOR_ONCHAIN_SURVEY.md` §2 (note:
   `comit-network/xmr-btc-swap` is **deprecated**, succeeded by `eigenwallet/core`).
2. **Configuration 1 — classical adaptor (ECDSA) + Groth16:** build, then benchmark.
   Note that some demos stop at Adapt and never implement Ext.
3. **Configuration 2 — LAS + Groth16:** replace the signature only. This isolates the
   post-quantum *signature* cost from the proof system's.
4. **Configuration 3 — LAS + LaZer:** the fully post-quantum stack.
5. **Comparison on time + communication cost** (gas is not available on Bitcoin),
   counting off-chain protocol messages, plus the usability discussion.
6. ✅ **Cumulative-acceptance figure regenerated and installed** as
   `report/latex/figures/fig_rejection_cdf.pdf`, from
   `evidence/runs/20260717_084012/tables` (no benchmark re-run; the plot script reads
   the existing CSVs). Reproduce with:
   `python3 scripts/plot_las_paper_figures.py --input-dir evidence/runs/20260717_084012/tables --output-dir <out> --appendix-dir <out-appendix>`
   then copy `rejection_acceptance_cdf_paper.pdf` → `report/latex/figures/fig_rejection_cdf.pdf`.
   The superseded per-attempt mass function is emitted to the appendix dir as
   `rejection_attempts_distribution_paper.pdf` (not yet included in the appendix `.tex`).
   **The report still needs a LaTeX rebuild** to pick the figure up.
7. ✅ Preparatory Solidity/classical cost check —
   `docs/04-evaluation/CLASSICAL_ADAPTOR_ONCHAIN_SURVEY.md`.
8. ✅ Report restructure — evaluation as its own chapter; critical reflection in
   Chapter 5 (achieved / fell short / would do differently).
9. ⬜ **Share the Overleaf project with Wang's Manchester address** (Royce's account
   action; cannot be done from the repo).

### 6b. Carried-over Meeting-5 Stage-1 items (still owed, now lower priority)

1. **Open the clean-Dilithium → LAS PR and invite Wang** (Meeting-4 + Meeting-5 named
   deliverable): artefacts ready (`dilithium-baseline` branch, `docs/02-methodology/CODE_DIFF_VIEW.md`,
   `docs/02-methodology/FUNCTION_MAP.md`).
2. **Draw the two diagrams** (M5.2, M5.3): high-level API flow (basic → LAS) and the
   repo-structure diagram covering base C / LAS C / LAS Rust with reused primitives
   marked.
3. **Build the 1–2 slide summary** (M5.12) around those diagrams + a 2–3 sentence
   findings statement.
4. **C ⇄ Rust size-equality table** (M5.7) and a high-level reused/modified/new front on
   `FUNCTION_MAP.md` (M5.4).
5. **Split the README** (M5.11): short quick-start (key commands) + keep the extended one.
6. **Report polish** (`report/latex/`): fold in the classical-adaptor overhead columns
   (M5.9), the rejection theory-vs-measured own-words write-up (M5.8), the C/Rust
   cross-check, and the machine/env statement in captions (M5.1); word-count pass to
   7–9k, embed the regenerated `evidence/latest/paper_package` figures, references pass.
   (`report/REPORT_DRAFT.md` is the superseded v0.1.)
7. **Video** per §5 storyboard.
8. *(Stage 2 — only after Stage 1 is supervisor-signed-off)* Foundry local chain:
   classical adaptor workflow first, then swap in LAS (M5.14).
9. *(optional tier, only after draft is supervisor-approved)* D16 param migration to
   q≈2²⁴ / on-chain LAS *verification* (precompile or zk — the swap + gas floor are
   already done in D15) / a second LAS-family scheme.
