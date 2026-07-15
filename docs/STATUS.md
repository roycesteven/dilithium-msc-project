# Project STATUS & Test Checklist — single source of truth for "what's done / tested"

*Living tracker. Updated 2026-06-27. Read this first to see, at a glance, every
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
| D3 | Atomic swap (2-party, 2-chain), narrated + asserted | ✅ | ✅ | ✅ | `make test/test_swap3 && ./test/test_swap3` |
| D4 | Scriptless HTLC ledger: swap / timeout-refund / same-Y PCN | ✅ | ✅ | ✅ | `make test/test_pcn3 && ./test/test_pcn3` |
| D5 | AMHL multi-hop (γ−κ−K, distinct Y_j, wormhole-resistant, refund) — *bonus* | ✅ | ✅ | ✅ | `make test/test_amhl3 && ./test/test_amhl3` |
| D6 | Byte serialisation + validating decoder + `base_verify_packed`; swept across parameter sets (paper + D2/D3/D5, parameter-derived `z` width) | ✅ | ✅ | ✅ | `make test/test_serde3 test/test_serde_l2 test/test_serde_l3 test/test_serde_l5 && ./test/test_serde3 && ./test/test_serde_l2 && ./test/test_serde_l3 && ./test/test_serde_l5` |
| D7 | Deterministic API + pinned KAT (reproducibility, C4) | ✅ | ✅ | ✅ | `make test/test_kat3 && ./test/test_kat3` |
| D8 | Benchmark 1 — per-op timings + **direct** rejection rate | ✅ | ✅ | ✅ | `make test/bench_las3 && ./test/bench_las3` |
| D9 | Benchmark 2 — LAS vs optimised Dilithium-3 (context; superseded as headline by D20) | ✅ | ✅ | ✅ | `make test/bench_compare3 && ./test/bench_compare3` |
| D10 | Benchmark 3 — application cost (swap payload + AMHL-vs-K) | ✅ | ✅ | ✅ | `make test/bench_app3 && ./test/bench_app3` |
| D11 | Benchmark 4 — **classical adaptor baseline** (ECDSA, same machine) | ✅ | ✅ | ✅ | clone secp256k1-zkp (README.md §4.1), `make test/bench_classical && ./test/bench_classical` |
| D12 | Function map (reused/modified/added; 0 upstream modified) | ✅ | n/a | ✅ | `docs/02-methodology/FUNCTION_MAP.md` |
| D13 | Reproducibility README + recorded provenance/toolchain | ✅ | n/a | ✅ | `README.md` |
| D14 | Report draft (~8k words, B4 skeleton) | 🟡 | n/a | 🟡 | `report/REPORT_DRAFT.md` (v0.1 — superseded by LaTeX scaffold D22) |
| D15 | On-chain gas: real Solidity swap (classical vs LAS sig) **+ measured native-verify cost-probe** | ✅ | ✅ | ✅ | `cd ref && make test/export_packed && ./test/export_packed ../evm/test/las_sig.bin; cd ../evm && forge test --gas-report && forge test --match-contract LASVerifyCost -vv` |
| D20 | **Primary fair benchmark** (corrected 2026-06-22; base path modularised to `basesig.c` 2026-06-23): separate base path (`basesig.c`) vs LAS adaptor path (`las.c`) — adaptor overhead (PreSign/Sign, PreVerify/Verify, Adapt/Verify, Ext separate) + cross-verify contract; official Dilithium = CONTEXT only ("not algorithm-matched"); ≥5 runs mean±SD; component sizes | ✅ | ✅ | ✅ | `make test/bench_levels_paper test/bench_levels2 test/bench_levels3 test/bench_levels5 && ./test/bench_levels_paper …`; `docs/LAS.md §8.1` |
| D23 | **Correctness-contract harness** (itemised 8-point PASS): PreSign→PreVerify, tripwire, Adapt→Verify, Ext exact, tampered msg/sig, malformed bytes, deterministic | ✅ | ✅ | ✅ | `make test/test_contract3 && ./test/test_contract3` |
| D24 | **Base-signature correctness test** (`basesig.c`, **CHECK**-gated, 1000 iters × paper/2/3/5): honest verify, tamper/wrong-key rejection, cross-module equivalence with `las.c`, cross-path interlock (tripwire + adapted-verifies-under-base + exact Ext), + 4 negative tests (wrong statement, wrong witness, tampered pre-signature, tampered adapted signature) | 🟡 ready (build via `make`) | ⬜ run by Royce | ✅ | `make test/test_basesig_paper test/test_basesig2 test/test_basesig3 test/test_basesig5 && ./test/test_basesig_paper` |
| D21 | **Two-branch code-diff view** (Meeting-3): `dilithium-baseline` (pristine) vs `main`; 0 upstream sources changed | ✅ | n/a | ✅ | `git diff --name-status dilithium-baseline main -- ref/`; `docs/02-methodology/CODE_DIFF_VIEW.md` |
| D22 | **LaTeX report scaffold** (Meeting-3): muthesis.cls, by chapter, official title, real benchmark tables, builds to PDF | 🟡 | n/a | ✅ | `cd report/latex && make` (TODOs: student id, figure, machine-of-record) |
| D16 | Parameter migration to paper's q≈2²⁴ | ⬜ | ⬜ | documented as future work | — |
| D17 | On-chain LAS *verification* (precompile or zk proof) | ⬜ | ⬜ | n/a (future work) | — *swap + gas floor already done (D15)* |
| D18 | Second LAS-family scheme (application-layer) | ⬜ | ⬜ | n/a | — *optional stretch* |
| D19 | Video (6–8 min) | ⬜ | ⬜ | n/a | see §5 storyboard |

**Headline:** every *required* Meeting-2 deliverable (Stage 1, Stage 2, both
benchmark baselines, function map, reproducibility) is ✅ done & tested. AMHL,
serialisation, KATs, **and the on-chain Solidity gas benchmark (D15)** are ✅ done.
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

---

## 2. Test inventory — what each test actually proves

All run with zero compiler warnings under
`-Wall -Wextra -Wpedantic -Wmissing-prototypes -Wredundant-decls -Wshadow -Wvla -Wpointer-arith`.

| Test binary | Iters / scope | Properties hard-asserted |
|---|---|---|
| `test_las{2,3,5}` | 1000 each, modes 2/3/5 | PreVerify accepts; **Verify rejects pre-sig (tripwire)**; adapted σ verifies; Ext recovers y **exactly**; Sign/Verify round-trip; bit-flip forgery rejected |
| `test_swap3` | 1 narrated run | two-leg atomicity; pre-adaptation σ̂_B unspendable |
| `test_pcn3` | 3 scenarios | cross-chain swap; timeout-refund (no coins lost); same-Y multi-hop PCN |
| `test_amhl3` | K=4 happy + K=2 refund | distinct Y_j; **wormhole resistance** (s_K can't open hop 1); ‖s_j‖∞≤j; exact cascade recovery; refund |
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

**Fair primary (bench_levels, evidence run `20260627_135247`) — Simplified
Dilithium-III (target), µs mean±SD, 10×1000:** KeyGen 114±5, Sign 1270±28,
Verify 280±7, PreSign 1355±62 (+6.7% vs Sign), PreVerify 288±2 (+3.1% vs Verify),
Adapt 302±5 (+8.1% vs Verify), Ext 101±3. Acceptance ≈37%/attempt at every setting.
Headline adaptor overhead ≤ ~8% across all four settings (paper/L2/L3/L5). These are
the numbers now in `report/latex` (`tab:overhead`, `tab:overhead-levels`,
`tab:complete-l3`).

**Classical baseline (bench_classical, evidence run `20260627_135247`):** KeyGen 34,
Sign 45, Verify 69, PreSign 207, PreVerify 268, Adapt 4, Ext 36; sizes pk 33, sk 32,
sig 64 (70 DER), pre-sig 162.

**Sizes (bytes):** LAS packed pk/Y **2944**, sk/witness **512**, sig=pre-sig
**4640** (32-byte `c_tilde` + `BitPack(z)`; in-memory `sizeof` 4096/8192/8224).
Classical: pk 33, sk 32, sig 64 (70 DER), pre-sig 162. Dilithium-3: pk 1952,
sk 4032, sig 3309.

**On-chain gas (EVM, deterministic — `evm/`, `forge test --gas-report`):** classical
claim (ecrecover) 75,709; LAS claim *floor* 208,400 (calldata 74,476 for the real
4672-B sig + keccak, **no** verification); fund 180k/140k; refund 39,330. LAS
settlement floor = 2.75× the full classical claim. **Native verification cost —
measured experiment (`evm/test/LASVerifyCost.t.sol`, `make`-free
`forge test --match-contract LASVerifyCost -vv`):** the full `base_verify` arithmetic
(12 fwd NTT + 8 inv NTT + 20 pointwise) is **10.08 M gas measured** on the EVM; the
SHAKE256 challenge (~64 Keccak-f permutations) adds **~1.92 M calculated** ⇒ **≈12 M
gas total ≈ 158× the classical claim ≈ 40 % of a 30 M block**. Correction: native
verification is *prohibitively expensive but does **not** exceed the block gas limit*
(EVM-native `mulmod` is 8 gas) — the earlier "exceeds the block gas limit" wording was
an overstatement and is retracted; the real barrier is cost + missing PQ precompiles.
**⚠ Stage-B caveat (re-measure):** the *calldata*-derived figures above (74,476 /
208,400, based on the 4672-B sig) predate the `c_tilde ‖ BitPack(z)` change; the packed
sig is now 4640 B (paper) / 6720 B (D3 export), so re-run `forge test` to refresh them.
The native-verify arithmetic figures (~12 M gas) are unaffected — Verify does the same
NTTs and one `SampleInBall(c_tilde)` as before.

**Three headline findings** (the report's centrepieces): (i) price of PQ is
*communication* (×29–89 sizes; on-chain calldata 74k gas > whole classical claim)
not *computation* (sub-ms, ≤×20 time); (ii) LAS adaptor overhead ≈0 (PreSign≈Sign,
PreVerify≈Verify) vs the classical adaptor's ~4× DLEQ overhead — LAS PreVerify even
absolutely faster; (iii) on-chain, the swap protocol runs end-to-end, and native LAS
*verification* is **measured at ≈12 M gas (≈158× the classical claim, ≈40 % of a 30 M
block)** — prohibitively expensive and needing a SHAKE/NTT precompile or zk proof, but
**not** over the block gas limit (the earlier "exceeds the limit" claim was retracted
after the `LASVerifyCost` experiment — the poqeth boundary is cost, not the ceiling).

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
| Evaluation (20%) | report §4 + `docs/LAS.md §8–8.3` | **two baselines**, 2×2 matrix, direct rejection measurement, AMHL-vs-K, correctness 100% |
| Conclusion (10%) | report §6 | conclusions vs objectives + ordered future work |
| Format/structure (5%) | report headings, numbered tables/figs, refs | — (needs figure redraw + reference formatting pass) |
| Project achievement (20%) | whole artefact | first public LAS; first exotic-PQ-on-chain; 0 upstream fns modified; zero-warning build; tamper/KAT robustness |

---

## 5. Video storyboard hook (6–8 min — D19, not started)

Suggested arc that *complements* (not repeats) the report: (1) the 2×2 quadrant
and why the exotic-PQ cell is empty [30s]; (2) live `./test/test_amhl3` showing the
wormhole-resistance assertion + norm growth scrolling [90s]; (3) animate the swap
cascade (Fig. 3) with the "publishing σ reveals y" reveal [120s]; (4) the
"price of PQ" bar chart — sizes ×29–89 vs near-flat adaptor overhead, the
counter-intuitive inverted-overhead result [120s]; (5) `test_serde3` tamper test
(all 4640 flips rejected) + KAT digest match as the "it's real, reproducible"
beat [60s]; (6) limitations + future work [30s]. Talking-head in corner.

---

## 6. Immediate next actions (in order)

1. **Open the clean-Dilithium → LAS PR and invite Wang** (Meeting-4 named
   deliverable): artefacts ready (`dilithium-baseline` branch, `docs/02-methodology/CODE_DIFF_VIEW.md`,
   `docs/02-methodology/FUNCTION_MAP.md`).
2. **Report polish** (`report/latex/`): the Stage-1 presentation tables, parameter
   notation, and basic-vs-LAS comparisons are in (Meeting-4); next is a word-count pass
   to 7–9k, embedding the regenerated `evidence/latest/paper_package` figures with the
   parameter-rich captions, and a references pass. (`report/REPORT_DRAFT.md` is the
   superseded v0.1.)
3. **Video** per §5 storyboard.
4. *(optional tier, only after draft is supervisor-approved)* D16 param migration to
   q≈2²⁴ / on-chain LAS *verification* (precompile or zk — the swap + gas floor are
   already done in D15) / a second LAS-family scheme.
