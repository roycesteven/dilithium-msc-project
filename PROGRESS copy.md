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


## Checkpoint — 2026-07-13 (Stage-A C mirror DONE + KAT gate GREEN)

Branch: restructure

Current goal:
- Stage A: seven-type/Algorithm-split C mirror of the proven Rust. Gate = pinned KAT digest unchanged.

Done (this session):
- C mirror COMPLETE for the in-scope core: ref/relation.{h,c} (new), serialize.{h,c} (six typed pairs), basesig.{h,c} (base_keygen/base_sign/base_verify + base_keygen_seed/base_sign_det/det_seed/base_verify_packed; paper-faithful locals), las.{h,c} (Algorithm-2 only; Alg-1 + S1 sampler deleted; packed tier uses typed codecs; BOUND_PRESIGN[_K]).
- Core tests retyped + BUILT + PASS at D3: test_kat3 (KAT digest 641a176c…5a19 MATCHES — zero bytes changed), test_las3 (1000/1000), test_basesig3 (all checks), test_serde3 (all serde). All under full CFLAGS -Wall -Wextra -Wpedantic -Wshadow…, zero warnings.
- Benches retyped + syntax-clean (-Wall -Wextra -Wpedantic): bench_levels.c, bench_criterion.c, export_packed.c. NOT run (8-min benches; not sanctioned). Rejection-gate lines preserved (LAS_BOUND_*→BOUND_*, las_expected_attempts kept).
- Makefile patched: relation.c added to core link lines; -DLAS_ELL/-DLAS_KAPPA → -DELL/-DKAPPA (LAS_N kept).
- Royce scope decision (this session): AMHL + chain OUT of Stage-A scope. amhl.{c,h}, chain.{c,h}, test_amhl.c, test_pcn.c, test_swap.c, test_contract.c, bench_app.c NOT retyped (still old API; their `make` targets will fail — intentional).
- CLAUDE.md gained a canonical "Rust ⇄ C ⇄ paper" naming-convention section (source of truth for the mirror).

Evidence used:
- In-session build+run of test_kat3/las3/serde3/basesig3 (compiled + executed by Claude this session, sanctioned by Royce). Digest 641a176c…5a19 reproduced.

Open risks:
- Benches compiled but NOT run (Royce should run bench_levels3/bench_criterion3 to confirm the rejection gate passes at runtime, and test_las2/las5, test_serde_l2/l3/l5, test_basesig_paper/2/5 across param sets).
- Out-of-scope amhl/chain tier left on old API — `make all` fails on those targets; build only the in-scope targets.
- Stage B (c_tilde) NOT started; will change wire size (D3 6752→6720) → new digest both langs.

Next action:
- Royce: `make test/test_kat3 && ./test/test_kat3` to reconfirm digest; optionally run benches. Then decide amhl/chain retype + Stage B.

## Checkpoint — 2026-07-12 (c_tilde correction + seven-type refactor)

Branch: restructure

Current goal:
- Stage A: finish seven-type/Algorithm-split refactor + LAS_-prefix param rename (both langs). Stage B: c_tilde challenge-lifecycle correction. Two digest checkpoints.

Done:
- Rust Stage A COMPLETE + PROVEN: basesig.rs (det_seed, packed tier), las.rs (Alg-2 only), lib.rs (pub mod relation), all tests/examples/benches; byte-level presig tripwires. KAT digest 641a176c… UNCHANGED, cargo build/check --all-targets/las_stage1 all pass.
- Applied Royce's naming decision (this session): Rust drops LAS_ → N/ELL/GAMMA/KAPPA/N_PLUS_ELL, ring degree D; BOUND_SIGN→basesig, BOUND_PRESIGN→las. C keeps LAS_N + adds LAS_D alias (params.h N=256/D=13 collision).
- C mirror STARTED: setup.{h,c} done (7 typedefs, setup_public_params, params rename, LAS_D, BOUND_SIGN removed).

Files touched/inspected:
- rust/fips204-las/src/{setup,relation,serialize,basesig,las,lib}.rs
- ref/setup.{h,c}
- plus Rust tests/examples/benches + plan file, not listed to keep checkpoint short.

Evidence used:
- none (KAT run in-session: digest 641a176c…, matched)

Open risks:
- C mirror is large + unbuildable by Claude (Royce runs make); semantic split las_pk→{public_key|statement} etc. per-site.
- Stage B c_tilde changes wire size (D3 6752→6720) → new digest, both langs.

Next action:
- Write ref/relation.{h,c} (new), then serialize.{h,c}, basesig, las, amhl, chain, tests, Makefile.



## Checkpoint — 2026-07-23 (On-chain LAS verifier: Stages 1–3 DONE + validated)

Branch: restructure

Current goal:
- Build a REAL on-chain (Solidity) LAS verifier (poqeth on-chain mode; poqeth 2025/091
  itself excludes Dilithium + has no zk — see 2025-091.md). Staged, each stage validated
  vs C golden before the next. No protocol simplification.

Done (this session):
- 2025-091.pdf -> 2025-091.md (faithful working guide; key correction up top: poqeth =
  native on-chain + Naysayer optimistic, NOT zk; excludes lattice/Dilithium).
- Stage 1: ref/test/export_verify_vector.c (+Makefile target) — golden vectors
  (pp_normal A', t, M, packed sig, negacyclic-conv golden). BUILT+RAN; C base_verify
  ACCEPTS the golden adapted sig. Vectors in evm/test/vectors/*.bin.
- Stage 2: vendored ZKNox SHAKE256 -> evm/lib/zknox/ZKNOX_shake.sol (+NOTICE, MIT,
  upstream fc09dff). evm/test/ZKNoxShake.t.sol 4/4 PASS (NIST KAT + multi-absorb +
  streaming-squeeze).
- Stage 3: reuse ZKNox normal-domain NTT (nttFw/nttInv/vecMulMod) -> vendored
  ZKNOX_NTT_dilithium.sol + ZKNOX_dilithium_utils.sol. evm/test/LASNtt.t.sol 2/2 PASS
  (round-trip + conv == C schoolbook negacyclic-conv golden).

Key decisions:
- Reuse ZKNox primitives (SHAKE/NTT/SampleInBall), assemble OUR OWN hint-less LAS
  base_verify (A=[I|A'], c=H(pk,w,M), BitPack19 z). ETHDILITHIUM's ML-DSA top verifier
  NOT reused (hints, 4x4).
- NTT is normal-domain (not ref/ntt.c Montgomery); feed A'/t normal-domain; w' identical
  after canonicalisation => equivalent, not a simplification.
- A' registered as public param (paper Verify takes pp as given) -> only SHAKE256 needed
  on-chain, no SHAKE128 A'-expansion.

Files touched: 2025-091.md; ref/test/export_verify_vector.c; ref/Makefile;
evm/lib/zknox/{ZKNOX_shake,ZKNOX_NTT_dilithium,ZKNOX_dilithium_utils}.sol + NOTICE.md;
evm/test/{ZKNoxShake,LASNtt}.t.sol; evm/test/vectors/*.bin. third_party/ETHDILITHIUM
cloned (git-ignored).

Evidence: forge test ZKNoxShakeTest 4/4; LASNttTest 2/2; C exporter self-check ACCEPT.

Open risks / notes:
- Gas: 1 SHAKE256 ~200k; 1 conv ~1.2M. Full verify likely several M (consistent w/ ~16.7M est).
- SampleInBall validation needs a C challenge golden (b_poly_challenge is static in
  basesig.c) — export via a local copy in the exporter, or rely on Stage-5 end-to-end.

Next action:
- Stage 4: reuse ZKNOX_SampleInBall (tau=kappa=49) validated vs C challenge golden;
  BitPack19 z-decode validated vs exported z; norm check ||z||inf<=gamma-kappa.
- Then Stage 5 (assemble base_verify, end-to-end vs sig.bin), Stage 6 (wire claimLAS +
  gas report), and the paper two-timeout refund fix.

## Checkpoint — 2026-07-23b (On-chain LAS verifier WORKS end-to-end — Stage 5 DONE)

Branch: restructure

MILESTONE: a numerically-complete, validated on-chain (Solidity) LAS base verifier exists.
evm/src/LASVerifier.sol (library LASVerify) reproduces ref/basesig.c base_verify_internal;
evm/test/LASVerifier.t.sol 6/6 PASS — test_verify_accepts_golden ACCEPTS the real adapted
signature (matches C), and rejects tampered sig / c_tilde / message.

Stages (all validated vs C ground truth):
- 2 SHAKE256 (ZKNox vendored): 4/4  (incl. large block-crossing multi-absorb)
- 3 NTT (ZKNox nttFw/nttInv/vecMulMod): 2/2 vs C schoolbook conv
- 4 SampleInBall(tau=kappa=49) + BitPack19 z-decode + norm: 3/3 vs c.bin/z.bin
- 5 full verify: 6/6 (accept golden + reject tamper + w' vs w_prime.bin + oracle vs c_tilde)

KEY BUG FOUND+FIXED (Dilithium NTT-domain gotcha): poly_uniform samples A' DIRECTLY in
NTT domain (Â'); the verifier's ZKNox NTT needs NORMAL A'. export_verify_vector.c now
recovers normal A' via the "multiply-by-1" idiom (pointwise_montgomery(Â',ntt(1)) +
invntt_tomont). Decisive C self-check added: SHAKE256(pack(t)||pack(w')||M)==c_tilde -> OK.

Vendored (evm/lib/zknox, MIT, upstream fc09dff): ZKNOX_shake, ZKNOX_NTT_dilithium,
ZKNOX_dilithium_utils, ZKNOX_SampleInBall, ZKNOX_keccak_prng (+NOTICE.md). Full clone in
git-ignored third_party/ETHDILITHIUM. foundry.toml: via_ir=true (verify has many locals).

Design decisions locked: A' registered in NTT domain (LASVerify.toNttDomain, once) — NEVER
NTT'd per verify; per-verify op budget = 12 fwd NTT (5 z_bot + 1 c + 6 t) + 36 pointwise +
12 inv NTT, matching base_verify_internal.

GAS (measured, honest): verify ~77M — EXCEEDS a 30M block. Unoptimized; the bit-by-bit
BitPack19 z-decode (_readBits, ~17M) + Solidity NTT/pack overhead dominate. Optimization
(byte-aligned z-decode, etc.) is Stage 6.

Next action:
- Stage 6: wire LASVerify.verify into AdaptorSwap.claimLAS (replace floor stub); optimize
  z-decode toward the block limit; forge --gas-report.
- Then paper two-timeout (t2<t1) refund fix in chain.c + AdaptorSwap.sol.

## Checkpoint — 2026-07-23c (Stage 6 DONE — verified on-chain swap + gas report)

Branch: restructure

Stage 6 complete. AdaptorSwap.sol now has claimLASVerified (FULL LASVerify.verify) beside
claimLAS (floor). SECURE: fundLASVerified commits keccak256(abi.encode(A',t,M)); claim
re-derives + checks it (rejects substituted pk — test_LASVerifiedSwap_rejects_wrong_context).
Title fixed HTLC->SCRIPTLESS (no hash-preimage; chain never checks Y; timeout=refund only).

GAS REPORT (via_ir, deterministic):
  claimClassical      75,751   (full ecrecover verify)
  claimLAS (floor)   289,930   (calldata+keccak, NO verify)
  claimLASVerified 56,538,682  (FULL native LAS base_verify)  <- headline
Apples-to-apples full verify: 75,751 -> 56,538,682 = ~746x. ECDSA=precompile, LAS=Solidity.
56.5M EXCEEDS EIP-7825 per-TX cap 16,777,216 (~3.4x) => not one mainnet tx. (Block now 30M
target/60M max, which 56.5M would fit — binding limit is the per-tx cap, NOT the block.)
Old 16.7M was an ESTIMATE (incl ~2.76M calculated SHAKE); real adds Solidity SHAKE + decode
+ packing + ABI/memory + settlement overhead.

Full suite: 22/22 pass (7 suites). z-decode optimized bit-by-bit->byte-window (77M->68M verify;
claim 56.5M). foundry.toml via_ir=true.

Docs updated: evm/README.md (claimLASVerified bullet + result table + apples-to-apples),
AdaptorSwap.sol header+claimLASVerified comment. STILL TODO doc-sync: docs/LAS.md §8.4 +
docs/03-results/GAS_LIMIT_INVESTIGATION.md still cite the 16.7M estimate as headline.

Corrections applied from Royce review: function name base_verify (not base_sign_verify_internal);
scope claim to "evaluated D3 Solidity verifier" not "all PQ"; EIP-7825 per-tx cap framing;
~746x precise; SCRIPTLESS not HTLC; chain doesn't check Y; secure settlement via (A',t,M) commit.

Next action:
- Paper two-timeout (t2<t1) refund fix: chain.c/chain.h (single timeout -> t1>t2 asymmetric)
  + AdaptorSwap.sol refund. Then doc-sync LAS.md §8.4 / GAS_LIMIT_INVESTIGATION.md.

## Checkpoint — 2026-07-23 (Two-timeout refund fix — EVM only)

Branch: restructure

Current goal:
- Apply paper §4.1 two-timeout (t2<t1) refund rule to the EVM adaptor swap.

Done:
- AdaptorSwap.sol: added TWO-TIMEOUT RULE NatSpec (first-claimed leg = shorter t2, second = longer t1, gap = u2 safety window); clarified refund enforces only this leg's own timeout.
- AdaptorSwap.t.sol: new test_TwoTimeoutSafetyWindow (2 classical legs, asserts t2<t1, u1 claims 1st at ~t2, second leg still un-refundable => u2 keeps window, then u2 claims).
- evm/README.md: added "Two-timeout refund rule (§4.1)" subsection.

Files touched/inspected:
- evm/src/AdaptorSwap.sol, evm/test/AdaptorSwap.t.sol, evm/README.md

Evidence used:
- none (forge NOT run per guardrails)

Open risks:
- pcn C ledger (test_pcn.c scen 1&2) has same reversed-timeout bug but is OUT OF SCOPE (Royce), left unfixed.
- New test not yet run (should be 23rd EVM test).

Next action:
- Royce: cd evm && forge test --match-test test_TwoTimeoutSafetyWindow -vv (then full suite).