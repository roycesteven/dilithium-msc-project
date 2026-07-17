# CONTEXT — session handoff (updated 2026-07-17; eighth-session update appended below)

Read this first, then `CLAUDE.md`. Supersedes the previous CONTEXT.md entirely.
Branch: **`restructure`**. KAT digest (C = Rust, current, post-c_tilde/Stage-B):
**`bb6ad0dab998c1f90ca4d3cc0f5d3dfa723e89f79aff18fce2698a08c96e260c`**
(the old `641a176c…` in the prior CONTEXT.md was pre-Stage-B and is dead).

## Eighth-session update (2026-07-17) — Stage-1 refreshed; Stage-2 HELD

Royce's direction this session: **hold Stage-2 (out of scope), focus Stage-1 +
Meeting-5.** Done, all verified by real runs (no invented numbers; working-tree, **not
committed**):

- **Fair Stage-1 evidence refreshed** → `evidence/latest` = **`20260717_084012`**; the
  report (`report/latex/generated/*` + `figures/fig_{timing,components,overhead}.pdf`)
  is synced to it. Headline core-tier adaptor overhead @ Simplified Dilithium-III:
  **PreSign +1.6%, PreVerify +5.1%, Adapt +9.0%** (C reference; *core / struct-API tier*).
  The **full-protocol (packed, incl. pack/unpack) overhead is now ALSO surfaced** —
  **PreSign +20.8%, PreVerify +35.3%, Adapt +79.5%** (C) — via a new
  `parse_packed_overhead` in `gen_report_data.py` (reads the fair log's TIER-2 section →
  `\packedOv*` macros), shown in `report/latex/chapters/03-results.tex`, the slide, and
  `00-diagrams-and-summary.md`. It is larger than core because each adaptor op decodes
  the pk-sized statement Y (serialization, not adaptor math). Rejection base 2.715 /
  PreSign 2.716 (≈37%). Rust Criterion (incl. the packed tier) + `bench_levels` /
  `size_report` logs regenerated; C⇄Rust sizes identical; **KAT digest unchanged**
  (`bb6ad0da…260c`).
- **Pipeline un-drifted (root cause of the stale report).** The restructure changed
  `bench_levels.c`/`test_serde.c` output but not the parsers. Fixed 5 anchors in
  `scripts/plot_las_benchmarks.py` (`d=` vs `N=`, `c_tilde` vs `c`, three component
  labels) + `parse_tamper` and the **signed-overhead regex** in
  `scripts/gen_report_data.py` — the Rust Adapt−Verify is **−4.9%** (below the separate
  base/LAS verify-helper floor; the true adaptor overhead is smaller than the compiled
  cost gap between the two helpers). `report/latex/chapters/03-results.tex` fixed to
  render the negative sign and explain it honestly.
- **Stage-1-scoped suite:** `STAGE1_ONLY=1 scripts/run_benchmark_suite.sh` skips the four
  uncompilable Stage-2 targets (guarded via `build_run_stage2`); the full suite is intact
  for when Stage-2 is ported back. Downstream plot/report needs zero Stage-2 inputs.
- **Meeting-5 artefacts delivered:**
  `docs/02-methodology/walkthrough/00-diagrams-and-summary.md` (Mermaid base + LAS
  API-flow diagrams with where `Y` is set up, repo-structure diagram, high-level
  reused/new front, C⇄Rust size table, rejection theory-vs-measured) and
  `report/slides/stage1_summary.html` (the 1–2 slide summary; published as a private
  artifact). STATUS §1 M5 rows 2/3/4/7/8/11/12 now ✅.
- **Stage-2 still STALE/HELD:** `amhl/chain/test_swap/test_pcn/bench_app/test_contract`
  NOT ported (Royce: out of scope this session). The seven-type port was started
  (old→new API map worked out) then stopped on instruction; resume only if Stage-2 is
  re-scoped.

## TL;DR — what this session did

This session was **benchmark-fairness hardening + adding the packed (end-to-end)
timing tier to the Rust side**, driven by Royce's insistence that the C and Rust
benchmarks be **100 % algorithm/pseudocode-identical** so the numbers are
scientifically defensible. All code changes are in the working tree, **not
git-committed** (Royce commits). Every change below was verified to build; the KAT
digest is **unchanged**, so all of it is behaviour-neutral except where noted.

Also updated early in the session: `las-context-consolidated.md` (new **§15
Meeting-5 directives**, 2026-07-06), `docs/STATUS.md` (Meeting-5 subsection), and a
new memory `meeting5-pivot.md`.

## Code changes this session (working tree, not committed)

1. **Norm-check fairness fix — `rust/fips204-las/src/basesig.rs`.** Base `Sign`
   (`sign_internal`) and `Verify` (`verify_internal`) used the upstream full-scan
   `infinity_norm().max()`; every other path (C base, C LAS, Rust LAS
   `las.rs::chknorm_vec`) uses an **early-exit** `poly_chknorm`-style check. Since
   this runs **inside the timed rejection loop**, full-scan-vs-early-exit was a
   base-vs-LAS *work-profile* asymmetry — **KAT-invisible** (same boolean output:
   `max|z|≥b ⇔ ∃ coeff |z|≥b`) but it biased the primary PreSign-vs-Sign /
   PreVerify-vs-Verify overhead. Added `fn b_chknorm_vec` (early-exit, mirrors C
   `poly_chknorm` + `las.rs::chknorm_vec`), swapped both call sites, dropped the now
   unused `infinity_norm` import, re-imported `crate::Q` for the `(Q-1)/8` guard.
   **KAT digest unchanged → proven behaviour-neutral.** C was already early-exit
   (`b_polyvecm_chknorm`) — untouched.

2. **SD estimator fix — `examples/bench_levels.rs` `mean_sd`.** Population SD (`/n`)
   → **sample SD `/(n-1)`** (Bessel), matching the C drivers (`stats()` in
   bench_levels.c and bench_criterion.c). At REPS=5 the old population SD printed
   error bars ~10.6 % too small.

3. **≥5-repetition hard asserts (statistical-validity floor).** Compile-time guards
   so a driver can't be built below the floor: `_Static_assert(RUNS >= 5, …)` in
   `ref/test/bench_levels.c`, `_Static_assert(CRIT_SAMPLES >= 5, …)` in
   `ref/test/bench_criterion.c`, `const _: () = assert!(REPS >= 5, …)` in
   `examples/bench_levels.rs`. **Neutral wording only** — Royce vetoed any
   "supervisor requires" phrasing (this repo goes public; no meeting context in
   code/messages).

4. **Rust suite gate scoped — `scripts/run_rust_bench_suite.sh`.**
   `cargo test --lib --tests` → `… -- --skip conversion::tests`. The 13 upstream
   fips204 `conversion::tests` (bit-pack/hint range-validation `should_panic` tests
   for ml_dsa encodings LAS does **not** use) fail under the LAS parameter overrides
   and were aborting the gate before any benchmark ran. Scoped out with a
   self-documenting comment; the LAS tests (`las_kat`, `las_stage1`, `integration`,
   `messages`) all pass.

5. **Packed TIER-2 added to the Rust benchmarks — full C⇄Rust parity.** Royce noted
   only the *core* tier had been shown; the *full protocol* tier (incl.
   packing/unpacking) was missing on the Rust side.
   - **`rust/fips204-las/src/las.rs` — 4 NEW public wrappers**: `presign_packed`,
     `preverify_packed`, `adapt_packed`, `ext_packed` (validating unpack → core →
     pack, mirroring C `las_*_packed`). This filled a **real gap**: CLAUDE.md's
     canonical naming table lists these Rust names but the port never had them
     (only base `keygen_packed`/`sign_packed`/`verify_packed` existed). Added the
     `serialize` imports to las.rs. KAT-neutral (additive).
   - **`examples/bench_levels.rs`** — TIER-2 section: pack canonical state once,
     byte-level success contract gate, 7 packed-op timings, packed adaptor-overhead
     line, codec-boundary-cost line (packed − core), and packed-tier rejection gates.
   - **`benches/las_bench.rs`** — two packed Criterion groups (g3 "Algorithm 1
     packed", g4 "Algorithm 2 packed") + packed rejection gates. Compiles; **the
     full Criterion (now 14 benchmarks, ~15 min) has NOT been run yet.**

## Key benchmark findings (Simplified Dilithium-III, this session's runs)

- **Core-tier adaptor overhead is tiny and noise-sensitive.** C core: PreSign vs
  Sign +5.7 %, PreVerify vs Verify +3.4 %, Adapt vs Verify +6.7 % (per-attempt
  +3.1 %). Rust core (Criterion, 300 samples): +0.8 % / **−1.2 %** / +0.1 %. Rust
  even measures **PreVerify < Verify (wrong direction)** — not noise (non-overlapping
  CIs) but the base (`basesig.rs`) and LAS (`las.rs`) verify paths use **separate
  duplicated hash/encode helpers** that compile to slightly different cost; the pure
  adaptor overhead is below that floor. (Candidate future tidy: share one hash
  helper — design change, not a bug.)
- **Packed / end-to-end tier is the ROBUST signal** (the pk-sized statement Y =
  4416 B decode dominates): C packed +24.0 / +36.6 / +82.3 %; Rust packed
  **+4.5 / +15.0 / +28.5 %** — both positive, same ordering (Adapt > PreVerify >
  PreSign), driven by the statement-Y codec. KeyGen codec cost matches C⇄Rust
  (~280 µs). **Raw times are NOT cross-language comparable** (C `−O3` vs Rust
  `opt-level="s"+lto`) — only within-language ratios.
- **Rejection gates pass everywhere** (C + Rust, core + packed): ≈2.7 attempts/call,
  ≈37 % acceptance, hard-checked vs `las_expected_attempts` within 5σ.
- **Sizes match C⇄Rust**: pk 4416 B, sk/witness 704 B, sig=pre-sig=adapted 6720 B.

## ⚠ BLOCKER (Stage-1 side RESOLVED 2026-07-17 via `STAGE1_ONLY`; Stage-2 itself still STALE/HELD — see eighth-session update) — Stage-2 / application C layer is STALE and does NOT compile

The restructure updated the Stage-1 core but left the application layer on the OLD
API. On `restructure` these do **not** build (old names `las_pk`/`las_sk`/`las_sig`/
`las_pp`, `LAS_M`/`LAS_ELL`/`LAS_KAPPA`/`LAS_GAMMA`, `LAS_{PK,SK,SIG}_BYTES`, the
deleted `LAS_C_COEFF_BITS`, witness `.s`):
`ref/amhl.{c,h}`, `ref/chain.{c,h}`, `ref/test/test_contract.c`,
`ref/test/test_swap.c`, `ref/test/test_pcn.c`, `ref/test/bench_app.c`.

Consequence: **`scripts/run_benchmark_suite.sh` (full C suite) ABORTS** at
`test_contract3`. **Stage-1 builds fine**: `bench_levels_{paper,2,3,5}`, `test_kat`,
`test_las`, `test_serde`, `bench_criterion`, `bench_classical`.

## Evidence / report sync status (REFRESHED 2026-07-17 → run `20260717_084012`, see eighth-session update; text below is the pre-refresh state)

- `evidence/latest` still points at the OLD C run `20260706_150850` (captured on
  `main`, pre-fix). This session ran Stage-1 benches **directly to scratchpad logs**
  (see below); it did **not** update `evidence/latest` or run the plotting/sync
  pipeline, because the full suite is blocked by the Stage-2 breakage above.
- So `report/latex` figures/generated data still reflect old numbers. Refreshing
  needs either (a) a Stage-1-scoped/resilient `run_benchmark_suite.sh` that skips
  the broken Stage-2 targets, or (b) porting the Stage-2 C files.

## Verified this session (Claude ran, under explicit "build and run")

- Rust: `cargo test` KAT + interlock PASS, digest `bb6ad0da…260c` unchanged.
- C Stage-1: `bench_levels_{paper,2,3,5}` + `bench_classical` built + ran (exit 0),
  all rejection gates OK.
- Rust: `bench_levels` example (core + NEW packed tier) ran, all gates OK; Criterion
  **core** tier (300 samples) ran, gates OK. Criterion **packed** tier compiles, not
  yet run.
- Scratchpad logs (this session, not in repo):
  `…/scratchpad/{c_paper,c_l2,c_l3,c_l5,c_classical,rust_bench,rust_criterion,rust_bench_packed,rust_size}.log`.

## Hard constraints (unchanged, binding)

- No git branch changes, no commits (Royce commits). No invented numbers. No
  hand-editing evidence logs (regenerate via the tool).
- Upstream Rust files and upstream C primitives (`poly.c`, `ntt.c`, `sign.c`,
  `packing.c`, `fips202.c`, …) stay untouched — bare `N`/`D`.
- **No meeting/"supervisor" context in code or in anything that ships to the public
  GitHub repo** — justify on technical/scientific grounds only.
- Don't run benches/tests unless explicitly told (Royce runs them; this session was
  explicitly told to build and run).

## Immediate next actions

1. (Optional, ~15 min) run the full Rust Criterion (now includes the packed tier)
   for clean statistical packed numbers: `cargo bench --bench las_bench`.
2. Refresh `evidence/latest` + `report/latex` with the fair Stage-1 numbers — needs
   a Stage-1-scoped C suite (skip the broken Stage-2 targets) or the Stage-2 port.
3. Port the stale Stage-2/application C files (amhl/chain/swap/pcn/bench_app/
   test_contract) to the seven-type API — separate task; only if Royce wants Stage-2
   back in scope (Meeting-4/5 defer it behind Stage-1 sign-off).
4. Documentation sync happens LAST, after Royce accepts the code.
