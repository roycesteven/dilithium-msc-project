---
## Checkpoint — 2026-06-30 00:30

Branch: main

Current goal:
- Reorganise benchmark evidence output into clean subfolders so Stage-1 paper figures aren't mixed with debug/application output.

Done:
- Reworked scripts/run_benchmark_suite.sh: logs/, tables/, paper_package/, appendix_package/, debug_figures/, application_package/ via staging + allowlist distribution.
- Added --appendix-dir to scripts/plot_las_paper_figures.py (rejection figure -> appendix_package).
- Generated organised MANIFEST.md + paper_package/README.md ("show these to Wang").

Files touched/inspected:
- scripts/run_benchmark_suite.sh
- scripts/plot_las_paper_figures.py
- scripts/plot_las_benchmarks.py (read only; unchanged)

Evidence used:
- none

Open risks:
- Suite not yet run; new tree only validated via bash -n + py_compile + scratch run of paper script.

Next action:
- Run scripts/run_benchmark_suite.sh and eyeball paper_package/ before showing Wang.
- Make Stage-1 results/methodology presentation defensible (Meeting-4) + paper-faithful notation.

Done:
- New supervisor-review skill (.claude/skills/supervisor-review); used it to judge Fig 3.1 + methodology.
- Fig 3.1 reworked to paired basic(blue)-vs-LAS(orange) overhead chart at D3 (overhead % labels); moved tab:overhead-l3 to appendix (chart-in-body/table-in-appendix); Table 3.2 caveat+param strip; fixed methodology kappa=60 + polynomial-count inconsistencies.
- Report notation N->d (paper) everywhere + figures regenerated (d=256); CLAUDE.md source-of-truth rule strengthened; las.h:18 paper<->code bridge comment; THEORY_IMPL_BRIDGE.md X^N->X^d cell.

Files touched/inspected:
- report/latex/chapters/{02-methodology,03-results,A-appendix}.tex
- scripts/plot_las_paper_figures.py; report/latex/figures/{fig_timing,fig_components}.pdf
- CLAUDE.md; ref/las.h; docs/THEORY_IMPL_BRIDGE.md

Evidence used:
- evidence/latest/tables/{primary_timing,adaptor_overhead}.csv

Open risks:
- Report PDF not rebuilt (no make per guardrail); Royce to run make in report/latex.
- Table 3.2 still lacks +/- SD (needs measured classical run, not invented).

Next action:
- Rebuild report.pdf (make in report/latex) and eyeball Fig 3.1 + tab:notation render.
---
Checkpoint 2026-07-06 — run-validity rejection gates + C↔Rust methodology mirror

Branch: main (working tree UNCOMMITTED — commit only when Royce asks)

Objective: every benchmark run must prove its acceptance rate matches theory
(else invalid per Royce/Wang), and bench_levels.c must mirror the Rust driver 100%.

Done:
- `las_expected_attempts(bound)` added to ref/las.{c,h} AND rust src/las.rs —
  exact E[attempts] = ((2·bound−1)/(2γ+1))^(−(n+ℓ)d), verified against the
  RENDERED 2020-845.pdf (Table 1 S_c, Alg.1 s11, Alg.2 s6, Fact 1, §3.2 ≈e).
  D3: Sign 2.71875, PreSign 2.77483 (differ by design, −1 bound).
- 5σ rejection gate (prints "rejection gate [...] => OK", aborts on FAIL) in
  benches/las_bench.rs, examples/bench_levels.rs, ref/test/bench_levels.c
  (new variadic MEASURE(niter,...) + MEASURE_SIGN(counter,...); per-attempt
  diagnostic now printed in C too).
- bench_levels.c mirrors the Rust driver: 5 reps × 500 sign / 1000 verify
  (was 10×1000), fixed ppseed 00..1f + fixed 33-byte MSG (same bytes as Rust
  → identical pp); randombytes include removed. Parser anchors of
  scripts/plot_las_benchmarks.py all preserved (gate labels chosen to avoid
  "Base Sign"/"LAS PreSign" substrings).
- Checks: gcc -fsyntax-only clean ×4 param sets (-Wall -Wextra); cargo check clean.
- Docs synced: BENCHMARKING.md (Run-validity section, parity table, RNG-source
  note), REPRODUCE_LAS_C.md Step 11, FUNCTION_MAP.md §3.1, LAS.md §8 Method,
  LAS_PROVENANCE.md. Memory: benchmark-rejection-gate.md added; working
  agreement + rust-port memory refreshed.
- Earlier in session: criterion 0.8.2 run (300/60, baseline criterion082,
  2026-07-05) analysed; examples/size_report.rs + size_report_rust.log;
  variance-provenance test (sign-class variance = i.i.d. restarts, slope≈−0.8
  autocorr≈0; verify-class = drift).

Evidence used: target/criterion estimates.json+sample.json (Jul-4 base /
Jul-5 criterion082), bench_levels_rust.log (Jul-3), communication_components.csv L3.

Open risks:
- ALL committed logs predate the gates: bench_las_criterion.log,
  bench_levels_rust.log, evidence/latest C tables need regeneration by Royce
  (guardrail: Claude never runs benches, reads outputs only).
- docs/REPRODUCE_LAS_RUST.md Step 8a still shows the old Jul-3 criterion-0.4
  numbers (replacement edit was rejected) — redo after the next criterion run.
- report/latex methodology wording may still assume the old 10×1000 scheme —
  check after evidence regeneration.

Next action (new chat):
1. Royce runs: cd rust/fips204-las && cargo bench --bench las_bench --
   --baseline criterion082 2>&1 | tee bench_las_criterion.log; then
   cargo run --release --example bench_levels 2>&1 | tee bench_levels_rust.log;
   then bash scripts/run_benchmark_suite.sh (C evidence).
2. Claude reads the logs: all gates "=> OK"; criterion diff vs criterion082
   should say "No change in performance detected" (instrumentation is inert);
   then sync numbers into REPRODUCE_LAS_RUST.md Step 8a + BENCHMARKING.md
   measured section (+ LAS.md/report if C numbers moved), update memory, and
   make ONE commit when Royce says so.
---

Checkpoint 2026-07-06 (b) — docs/ restructured by report chapter

Objective: (1) new consolidated C+Rust implementation & benchmark-methodology
doc; (2) docs/ physically organised per report.tex chapter, big files split.

Done:
- NEW docs/02-methodology/C_RUST_IMPLEMENTATION_AND_BENCHMARK_METHODOLOGY.md
  (chapter-2 entry point: both implementations, KAT lock, Alg1-vs-Alg2
  methodology incl. rejection gate; measured snapshot provenance-cited).
- Chapter folders docs/{01-introduction,02-methodology,03-results,
  04-evaluation,A-appendix}; single-topic docs moved via git mv (history kept).
- LAS.md (1164 lines) split VERBATIM at ## boundaries into 10 part files
  (diff-verified lossless; § numbering preserved); docs/LAS.md is now the
  hub/index (path + "LAS.md §N" convention preserved; §11 refs kept there).
- Repo-wide reference sweep: CLAUDE.md, README.md, las-context-consolidated.md,
  docs cross-refs, rust/fips204-las docs, .claude agents/skills,
  ref/scripts/docs_guard.sh, defense/build_defense.py, session memory —
  all old docs/ paths rewritten; verified zero stale references.
  PROGRESS.md history + evidence/ captures intentionally untouched.
- docs/DOCS_BY_CHAPTER.md: per-chapter map + topic-ownership (anti-redundancy)
  rules + split/merge decisions. STATUS.md, paper/, references/ stay put as
  cross-cutting authorities.

Open risks:
- Nothing committed yet (this restructure + the earlier gate work are one
  working tree). Untracked: LAS-* part files, DOCS_BY_CHAPTER.md, C_RUST_* doc.
- Benchmark rerun by Royce still pending (see previous checkpoint).

Next action: Royce reruns benches (previous checkpoint's commands), Claude
reads logs, syncs numbers, then ONE commit of gates + restructure when asked.
---

## Checkpoint — 2026-07-09 10:54

Branch: restructure

Current goal:
- Re-lay-out the 4 scheme files for side-by-side comparison and rename LAS API for uniform provenance chain crypto_sign*→base_sign*→las*.

Done:
- Mirrored basesig.c↔sign.c and las.c↔basesig.c (same slots/order/int returns, helpers at bottom); Rust twins las_basesig.rs/las.rs likewise.
- Renamed las_keygen→las_keypair, las_sign→las_signature, sign_core→las_signature_internal (+ _internal splits) across all C/Rust callers, examples, and project docs.
- Built + ran full suites: 16 C targets PASS (KAT digest matches), Rust cargo test PASS (las_kat parity, doctests); fixed pre-existing rustdoc failure.

Files touched/inspected:
- ref/basesig.{c,h}, ref/las.{c,h}
- rust/fips204-las/src/las.rs, las_basesig.rs
- plus other related files (tests, examples, docs), not listed to keep checkpoint short.

Evidence used:
- none

Open risks:
- report/REPORT_DRAFT.md (~L376/433/787) still uses old names — left for Royce.
- las_verbose_comment.{c,h,rs} renamed but still old layout (annotated copies, unbuilt).

Next action:
- Decide whether to regenerate las_verbose_comment.* to the new layout or drop them.

## Checkpoint 2026-07-09 (mirror-rigor rewrite + two-tier architecture)

Branch: restructure

Current goal:
- Upstream-twin-helper rewrite of las.c (quote basesig.c lines verbatim + WHY), then two-tier architecture: core-crypto (struct) vs end-to-end (packed) API.

Done:
- ref/basesig.c (accepted earlier) built+tested: 4 param sets x 1000 iters PASS.
- ref/las.c rewritten to the same standard: helpers = verbatim copies of basesig.c's b_* twins (las_* prefix), inline SHAKE/matrix/rej composition, every line annotated [REUSED]/[CHANGED]/[DELETED] quoting basesig.c:<line> (Alg-1) or its own Alg-1 twin las.c:<line> (Alg-2); all 207 line citations machine-verified.
- Adopted basesig's z-pipeline order (proved KAT-safe: divergent reduce32 representatives differ by Q and both always fail chknorm).
- Provenance chain completed per Royce: las_sign<->base_sign, las_open<->base_sign_open added.
- Shared setup split out: ref/setup.{c,h} = params + shared types (las_pp/pk/sk/sig) + las_setup (paper Setup()->pp); las.h/basesig.h/serialize.h re-layered (setup.h -> serialize.h -> schemes, mirroring params/polyvec -> packing -> sign).
- End-to-end PACKED-API tier added per Royce (packing inside the call, like sign.c): base_sign_{keypair,signature,verify}_packed in basesig.c; las_{keypair,signature,verify,presign,preverify,adapt,ext}_packed in las.c; serialize.c now pure codec (las_verify_packed moved to las.c, arg order unified with struct tier; callers updated).
- test_serde.c: packed-tier roundtrips + byte-level interlock (base verifier accepts adapted LAS sig through bytes).
- Build clean (zero warnings), ALL 16 C tests PASS, KAT digest 641a176c... matches pinned value.

Files touched:
- ref/las.{c,h}, ref/basesig.{c,h}, ref/setup.{c,h} (new), ref/serialize.{c,h}, ref/Makefile, ref/test/test_serde.c, ref/test/test_contract.c.

Evidence used:
- test run output this session (16/16 PASS incl. test_kat3 pinned digest).

Open risks:
- docs/walkthrough/FUNCTION_MAP/bridge line-number links now stale (doc sync deferred until Royce accepts code).
- basesig.h header still says "las.{c,h} are byte-for-byte untouched" (stale claim).
- las_verbose_comment.* still old layout.

Next action:
- Cycles/op + packed-tier timings in bench drivers (bench_levels.c; bench_criterion.c must stay in lockstep with the Rust driver), then Rust mirror rewrite (las_basesig.rs quotes ml_dsa.rs, las.rs quotes las_basesig.rs, + setup/packed-tier parity), then cargo test KAT parity, then doc sync.
