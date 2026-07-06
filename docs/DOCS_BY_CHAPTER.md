# docs/ — organised by report chapter

*Physical layout of `docs/` mirrors the dissertation structure in
`report/latex/report.tex` (muthesis, rubric-aligned). Reorganised 2026-07-06:
single-topic documents were **moved** into chapter folders (`git mv`, history
preserved); the one multi-chapter document (`LAS.md`, 1164 lines) was **split
verbatim** into per-section part files — `docs/LAS.md` remains as the
index/hub, so every existing "`LAS.md §N`" reference still resolves. There is
deliberately no Background chapter — the literature review folds into the
Introduction.*

---

## 00 — Abstract (`chapters/00-abstract.tex`)

No dedicated file. Compress from: the framing paragraphs of
`01-introduction/LAS_WALKTHROUGH.md` + the headline claims of
`03-results/LAS-08-performance-measured.md`.

## 01 — Introduction, incl. literature review (`docs/01-introduction/`)

| File | Role |
| --- | --- |
| `LAS-01-introduction-and-motivation.md` | LAS.md §1 — motivation, problem statement, contribution (report-source register) |
| `LAS_WALKTHROUGH.md` | plain-English end-to-end explainer ("start here"); reader-roadmap and video-script source |

## 02 — Methodology (`docs/02-methodology/`)

| File | Role |
| --- | --- |
| `C_RUST_IMPLEMENTATION_AND_BENCHMARK_METHODOLOGY.md` | **chapter entry point**: C + Rust implementations on Dilithium / ML-DSA primitives, KAT lock, and the full Algorithm 1 vs Algorithm 2 benchmark methodology (pairing, contract gate, 5σ rejection gate, mirrored collection scheme, reading rules) |
| `LAS-02-mathematical-background.md` | LAS.md §2 — rings, norms, M-SIS/M-LWE, FSwA |
| `LAS-03-base-signature.md` | LAS.md §3 — Algorithm 1 (ordinary lattice-based signature) |
| `LAS-04-adaptor-extension.md` | LAS.md §4 — Algorithm 2 (variant B), bound budget γ−κ−1 |
| `LAS-05-implementation.md` | LAS.md §5 — module-by-module C implementation incl. serialization + deterministic API |
| `LAS-06-testing.md` | LAS.md §6 — correctness/KAT/tamper test suite |
| `LAS-07-application-atomic-swap.md` | LAS.md §7 — Stage-2 design: swap, chain model, AMHL |
| `THEORY_IMPL_BRIDGE.md` | every 2020/845 equation → exact C function |
| `FUNCTION_MAP.md` | per-function call-as-is / modify / new classification (zero upstream functions modified) — the rubric's reuse table |
| `CODE_DIFF_VIEW.md` | Meeting-3 diff-level view: original Dilithium vs this work |

## 03 — Results and discussion (`docs/03-results/`)

| File | Role |
| --- | --- |
| `LAS-08-performance-measured.md` | LAS.md §8 — ALL measured Stage-1 results: per-operation timings (primary: basic signature vs LAS adaptor), rejection statistics, component sizes, classical ECDSA-adaptor 2×2 (§8.3), parameter sweep (secondary axis) |
| `GAS_LIMIT_INVESTIGATION.md` | the on-chain axis: measured ≈12 M gas native LAS verify (~40 % of a 30 M block; "exceeds limit" claim retracted) |

*Data, not prose:* `evidence/latest/tables/*.csv`, `rust/fips204-las/*.log` —
regenerate before quoting (both must show `rejection gate … => OK`).

## 04 — Evaluation and reflection (`docs/04-evaluation/`)

| File | Role |
| --- | --- |
| `LAS-09-limitations-future-work.md` | LAS.md §9 — knowledge gap, modulus substitution, constant-time caveat, production gaps (its future-work half also feeds ch. 5) |
| `PROJECT_HISTORY_EXPLAINED.md` | sequential build narrative — process reflection + video preparation |

## 05 — Conclusion and future work (`chapters/05-conclusion.tex`)

No dedicated folder: source = the future-work half of
`04-evaluation/LAS-09-limitations-future-work.md` + the open/optional items in
`docs/STATUS.md`.

## A — Appendix (`docs/A-appendix/`)

| File | Role |
| --- | --- |
| `REPRODUCE_LAS_C.md` | full C reproduction guide (toolchain → KATs → gated benchmark → evidence suite) |
| `REPRODUCE_LAS_RUST.md` | full Rust reproduction guide (vendored crate → byte-for-byte KAT → Criterion → size report) |
| `LAS-10-build-and-run.md` | LAS.md §10 — quick build/run reference |

Code snippets appear **only** in the appendix (rubric rule).

## Cross-cutting authorities (deliberately NOT in a chapter folder)

| File | Why it stays put |
| --- | --- |
| `docs/LAS.md` | the hub/index of the split — preserves the repo's most-referenced path and the "`LAS.md §N`" convention; also holds §11 References |
| `docs/STATUS.md` | living deliverable/test tracker (project management, not report prose) |
| `docs/paper/LAS_2020_845_NOTATION.md` | **notation source of truth** — governs every chapter's maths; pinned path in CLAUDE.md's source-of-truth rule |
| `docs/references/` | collected papers + index → `refs.bib` |

Related report sources outside `docs/`: root `README.md` (build/toolchain,
ch. 2/A), root `las-context-consolidated.md` (supervisor objectives → scope
statements), `rust/fips204-las/BENCHMARKING.md` + `LAS_PROVENANCE.md`
(ch. 2/3/A), `MSc_Report_and_Video_Rubric.md` + `research_writing_guide.md`
(assessment criteria — shape, not content).

---

## Anti-redundancy rules (topic ownership)

Every topic has exactly **one owning file**; everything else links to it
instead of restating it. When numbers or claims change, edit the owner and the
pointers stay valid.

| Topic | Owner |
| --- | --- |
| Measured Stage-1 numbers (timings, %, sizes) | `03-results/LAS-08-performance-measured.md` (from `evidence/latest/` + the Rust logs) |
| Benchmark methodology & validity (gates, statistics rules) | `02-methodology/C_RUST_IMPLEMENTATION_AND_BENCHMARK_METHODOLOGY.md` (deep detail: `rust/fips204-las/BENCHMARKING.md`) |
| Paper equations ↔ code mapping | `02-methodology/THEORY_IMPL_BRIDGE.md` |
| Reuse classification (what is upstream vs ours) | `02-methodology/FUNCTION_MAP.md` (C) / `rust/fips204-las/LAS_PROVENANCE.md` (Rust) |
| Notation and symbols | `docs/paper/LAS_2020_845_NOTATION.md` (the PDF wins on conflict) |
| Reproduction commands | the two `A-appendix/REPRODUCE_*.md` guides |
| What is done / open | `docs/STATUS.md` |

## Split & merge decisions (2026-07-06)

- **Split:** only `LAS.md` (1164 lines, the one file spanning four chapters) —
  split **verbatim** at its `## §` boundaries (diff-verified lossless), section
  numbering preserved, hub left at `docs/LAS.md`. The other files are ≤ 516
  lines and single-topic: splitting a reproduction guide or the bridge table
  would fragment one coherent sub-chapter for no gain.
- **No content merges:** each remaining pair that looks mergeable serves a
  different reader (walkthrough vs technical source; per-function table vs
  per-file diff; per-language reproduction guides). The consolidation that was
  genuinely missing — one chapter-2 narrative across both languages — was
  added as `C_RUST_IMPLEMENTATION_AND_BENCHMARK_METHODOLOGY.md` (merge by
  reference, owners stay authoritative).
- **All live references updated** repo-wide (CLAUDE.md, README, docs
  cross-refs, rust docs, `.claude` agents/skills, `ref/scripts/docs_guard.sh`,
  `defense/build_defense.py`, session memory). Historical artefacts
  (`PROGRESS.md` past checkpoints, `evidence/` captures) intentionally left
  untouched.
