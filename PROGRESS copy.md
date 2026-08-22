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



## Checkpoint — 2026-07-27 12:48

Branch: report

Current goal:
- Build a fully-PQ succinct proof (Winterfell FRI-STARK, no Groth16) for on-chain LAS verification, in new standalone crate rust/las-stark.

Done:
- Stage A: sound STARK norm gadget ||z||inf<=B (real proof, tested).
- Stage A.2 groundwork: native byte-exact full-relation spec (SampleInBall + w' + SHAKE256 challenge) vs C goldens.
- Stage A.2 convolution: sound single negacyclic-conv STARK (b range-checked) at reduced CONV_D=64; cargo test 7/7 green.

Files touched/inspected:
- rust/las-stark/src/conv_air.rs
- rust/las-stark/src/{air,prover}.rs
- rust/las-stark/src/{relation,hashing,vectors,params,lib}.rs
- rust/las-stark/tests/las_stark.rs
- rust/las-stark/{Cargo.toml,README.md}

Evidence used:
- none

Open risks:
- d=256 conv blocked by Winterfell 255-column cap; needs narrow layout (streaming MAC + LogUp lookup, or NTT-transform) + shared z_bot across 30 A'.z_bot convs.
- Hashes (SampleInBall/SHAKE256) not yet in-AIR, so z not yet bound to c_tilde; still a gadget, not a succinct proof of verification.

Next action:
- Implement narrow d=256 convolution (streaming MAC + LogUp lookup) sharing one z_bot to assemble w'.

## Checkpoint — 2026-07-27 (later)

Branch: report

Current goal:
- Narrow d=256 arithmetic proof for on-chain LAS verification (rust/las-stark).

Done:
- rust/las-stark/src/relation_air.rs: NEW STARK proving base_verify constraints (1) ||z||inf<=B
  AND (3) w'_m = z_top[m] + sum_j A'[m][j](x)z_bot[j] - c(x)t[m] at the REAL d=256, all n=6 outputs
  bound to ONE shared z. Narrow layout (4096 rows x 135 main + 15 aux cols) instead of the
  d-wide window that hit Winterfell's 255-column cap.
- Method: random-evaluation (Schwartz-Zippel) argument on Winterfell's AUX trace segment
  (randomness drawn after main commitment). Prover commits z + integer quotients h_m (by X^d+1)
  and g_m (by q); aux segment Horner-evaluates at random x and checks
  sum_m rho_m [P_m(x) - (x^d+1)h_m(x) - q g_m(x)] = 0. Range checks |z|<=B, |h|<2^51, |g|<2^29
  are load-bearing (they lift the F_p identity to Z; unbounded g would make it vacuous).
- 5 new tests in tests/las_stark.rs (witness exists / rejects tampered w', proof round-trip,
  rejects tampered public inputs, rejects tampered proof).
- conv_air.rs (CONV_D=64) demoted to "schoolbook reference, superseded"; README rewritten;
  lib.rs module docs updated; STAGE2_UTXO_SWAP_PLAN.md role-B cell updated.

Files touched:
- rust/las-stark/src/relation_air.rs (new), src/lib.rs, tests/las_stark.rs, README.md
- docs/02-methodology/STAGE2_UTXO_SWAP_PLAN.md

Evidence used:
- none (no benchmarks run)

Verification status:
- `cargo check --tests` clean, no warnings.
- `cargo test --release` = 12/12 PASS (was 7/7 before this change), incl. all 5 new
  relation-AIR tests. No fixes were needed. No benchmark numbers taken.

Open risks:
- Plain `cargo test` (debug) is NOT the supported command: Winterfell's debug-assertion
  "declared degree == measured degree" self-check would trip, since the declared aux
  degrees are deliberately upper bounds. Use --release (README says so).
- Hashes still out of the AIR: c and w' are PUBLIC inputs, so z is bound to (A',t,c,w') but
  NOT to (c_tilde, M). Still not a stand-alone signature-verification proof.
- Public inputs enter as 44 periodic columns (cycle d) => verifier work linear in |public|;
  fine in Rust, would want A' behind a commitment for an on-chain verifier.

Also done (same session, Royce chose option 2):
- rust/las-stark/src/bin/prove_relation.rs + verify_relation.rs (registered in Cargo.toml).
  prove_relation prints trace shape (rows x main+aux cols), public-input size in field
  elements, proof size, and a PHASE SPLIT of prove time (quotient witness / trace build /
  STARK). verify_relation rebuilds the public inputs from the same goldens and prints
  verify time. Both print the "NOT proven: (2) SampleInBall, (4) SHAKE256" caveat.
  `cargo check --release --bins` clean.
- src/bin/bench_stark.rs = THE BENCHMARK OF RECORD + scripts/run_stark_bench.sh writes
  evidence/stark/<ts>/{bench_stark.log,environment.txt} + latest symlink (stage2 layout).
  Protocol aligned with the project (las-context §13.3, §15.6): 3 s DISCARDED WARM-UP,
  then 5 timed reps, mean +- sample (n-1) SD, one machine, one process. Warm-up replaces
  the inner 500/1000-iteration loop the us-scale drivers use (a 0.4 s op needs no timer
  amortisation, it needs a warm cache/allocator).
- MEASURED (evidence/stark/20260729_142953), both AIRs in ONE process, same protocol:
    NormAir     (1) only : prove TOTAL 110.821 +- 0.212 ms, verify 0.470 +- 0.020 ms, 52,876 B
    RelationAir (1)+(3)  : prove TOTAL 442.157 +- 4.501 ms, verify 1.319 +- 0.042 ms, 98,419 B
      phases: witness 1.993, trace 1.306, prover ctor 0.007, Winterfell prove() 438.851
    RATIO: prove 3.99x, verify 2.81x, size 1.86x.
- TWO EARLIER CLAIMS WERE WRONG AND ARE RETRACTED:
    (a) "~1.5-1.6x prove time" came from cold single shots + non-like-for-like spans.
        prove_norm internally does trace build + prover ctor + prove, so it must be
        compared against a RelationAir TOTAL covering the same steps. Correct: 3.99x.
    (b) "FRI is the entire cost" -- Winterfell prove() is trace LDE + Merkle commits +
        constraint evaluation + DEEP + FRI. Nothing here attributes cost to FRI alone.
    Survives: witness+trace+ctor = 3.3 ms = 0.75% of total (arithmetising is ~free).
- Also fixed: pub_inputs.clone() (176 KB) was INSIDE the verify timer in prove_relation
  and verify_relation; hoisted out of the timed region in all three binaries.
- On-chain: NO gas figure claimed. The 98,419 B proof is not the whole payload -- the
  11,008 public field elements also reach the verifier, of which A' (7680, fixed params)
  and t (1536, signer public key) are reusable/committable; only c (256) and w' (1536)
  are signature-specific.
- README Build & run section reordered: the relation CLIs are now step 2/3 (the main
  result), the norm gadget demoted to step 4.

Next action:
- Royce runs: cd rust/las-stark && cargo run --release --bin prove_relation
                                 && cargo run --release --bin verify_relation
  -> gives the quotable d=256 proof size + prove/verify time for the report.
- Then: in-AIR Keccak-f for SampleInBall + SHAKE256 challenge (binds z to c_tilde, M). Large.

## Checkpoint — 2026-07-29 — FIPS 204 c_tilde alignment (EMERGENCY CORRECTION, done)

Branch: report

Current goal:
- Correct the implementation so c_tilde follows FIPS 204 lambda/4 per parameter set,
  then regenerate every artefact and report macro from real runs.

Done (Tiers 1-4 applied atomically; see CONTEXT.md ninth-session section for detail):
- ROOT CAUSE: ref/params.h already scaled upstream CTILDEBYTES 32/48/64, but the LAS
  layer overrode it with a flat 32. FIPS 204 sec 7.3 Alg 29 takes a seed in B^{lambda/4}.
- LAS_CTILDEBYTES now keyed on (LAS_N, ELL, KAPPA) TOGETHER, #error/panic! on unknown:
  (4,4,39)->32  (6,5,49)->48 TARGET  (8,7,60)->64  (4,4,60)->32 paper reproduction.
  NOT keyed on DILITHIUM_MODE (the Makefile picks the mode only to satisfy params.h).
- SAFETY: c_tilde buffers were [LAS_SEEDBYTES]=32 but memcpy'd at LAS_CTILDEBYTES; at 48
  that is a 16-byte OVERREAD. Widened buffers + squeeze + SampleInBall absorb at 7 sites
  in basesig.c and 7 in las.c. PRG-seed sites deliberately left at LAS_SEEDBYTES.
- Conditional wire-size anchors for ALL FOUR sets in both languages (_Static_assert
  ladder in serialize.h; expected_wire_sizes() in serialize.rs). All four sets compile.
- Tier 4: CTILDE_BYTES=48 threaded through LASVerifier.sol + LASNaysayer.sol (7 hardcoded
  32s); LAS_CTILDEBYTES added to las-stark params/vectors/hashing.

Regenerated values (only c_tilde changed structurally):
- c_tilde 32 -> 48 ; signature = pre-sig = adapted 6720 -> 6736 ; L5 9152 -> 9184
- L2 unchanged at 4640 ; pk 4416, sk 704, z 6688 (99.29%), Y 4416 all unchanged
- KAT digest bb6ad0da...260c -> b4a10ffb6e645e5076d1ff5993faa72909232fc71e554b93544141d6590503be

Evidence used / regenerated:
- evidence/latest -> runs/20260729_180517 (STAGE1_ONLY=1 run_benchmark_suite.sh + sync_report.sh)
- golden vectors + naysayer fixtures re-exported (sig.bin 6736, C base_verify ACCEPT)
- Criterion bench_las_criterion.log CONFIRMED post-change (raw estimates.json 18:40-18:44
  vs setup.h 17:20), 14 benchmarks, 4 rejection gates OK; tab_rust Criterion column matches
- serde evidence repointed from the paper-dims test_serde3 to the TARGET test_serde_l3,
  so \tamperFlips is 6736 (was 4640) and matches \sigBytesTarget

Tests passed:
- C+Rust reached the new KAT digest INDEPENDENTLY (cross-language gate held)
- C: test_kat3, test_las3 1000/1000, test_serde_l2/l3/l5 (tamper 4640/6736/9184), test_basesig3
- Rust fips204-las: all pass in debug; las-stark 12/12 incl. full_relation_native
- All rejection gates pass (Sign 2.704-2.735 vs 2.7188; PreSign 2.765-2.784 vs 2.7748)

Unresolved risks:
- Legacy C Stage-2 (amhl/chain/test_contract/test_swap/test_pcn/bench_app) still does not
  compile. Royce ruled it superseded by the Rust Stage-2 evaluation -- DO NOT repair.
  STAGE1_ONLY=1 skips it and regenerates everything the report consumes.
- Report PROSE not yet updated; word count still 9658 (must end within 7000-9000).

Next action (NEW SESSION -- report only, no more code; full brief in CONTEXT.md):
1. Rewrite 02-methodology.tex sec 2.3: c_tilde is 48 B at the target and the old flat 32
   did NOT match FIPS 204 at ML-DSA-65. Construction is the standard's; width follows
   lambda/4 for the aligned set.
2. Audit every ML-DSA alignment claim. Required framing: "aligns the reusable ML-DSA
   primitives and challenge-digest strength with ML-DSA-65, while retaining LAS-specific
   distributions, bounds, exact relations, and unoptimised serialization." Do NOT say LAS
   IS ML-DSA-65 or inherits its security category; do NOT say 2020/845 specifies a
   FIPS-style 256-bit c_tilde (it defines H:{0,1}*->C only). fair_paper = separate
   historical reproduction set, appendix only; main benchmarks L2/L3/L5, L3 headline.
3. Trim 9658 -> ~8000 words, HARD BOUND 7000-9000, using COVERAGE-BASED rebalancing
   (map each rubric question to paragraphs; delete the weaker duplicate; cut detail
   already in appendix/repo; do NOT pad the Introduction just because it is 20%).
   Already trimmed: Ch5 sec 5.1, sec 5.2.2, sec 5.3 item 1.
   Still to do: Ch4 sec 4.4 (repeats sec 3.7), Ch2 sec 2.6 (duplicates Ch4 sec 4.1),
   Ch2 sec 2.3/2.5 (appendix-level detail).
   Per-chapter texcount: Abstract 477, Intro 1156, Method 2682, Results 2639,
   Evaluation 992, Conclusion 1695 (appendix 2886 excluded).
   NOTE: Ch4 is only 992 words but its rubric COVERAGE is complete (two big tables are
   uncounted) -- do not pad it mechanically.

## Checkpoint — 2026-07-30 — report prose pass, tasks 1-2 (c_tilde width + ML-DSA alignment audit)

Branch: report

Current goal:
- Prose-only pass over report/latex. No code, no pipeline changes, no benchmark runs.

Task 1 DONE — stale 32-byte c_tilde claims corrected (3 sites):
- chapters/02-methodology.tex sec 2.3: construction stated as FIPS 204's (transmit c_tilde,
  re-derive c = SampleInBall(c_tilde)); width = 48 B at the target set via the lambda/4 rule;
  per-set widths deferred to appendix. Body word delta approx +12.
- chapters/A-appendix.tex app:serialize: new paragraph — width is an implementation choice,
  NOT a requirement of 2020/845 (which specifies H only as a random oracle onto C); lambda/4
  with lambda=128/192/256 -> 32/48/64; width tracks the LAS set not the build mode; alignment
  is of reusable primitives + challenge-digest strength only, not a claim that LAS is
  ML-DSA-65 or inherits its security category. Royce corrections applied: "historical
  paper-parameter reference set"; caching paragraph deleted; sets named as ML-DSA-44/-87-
  aligned LAS sets rather than Simplified Dilithium-II/-V.
- chapters/03-results.tex: "the fixed 32-byte hash c_tilde" -> "only its short digest c_tilde"
  (that sentence spans L2..L5, where the width is 32 and 64, so any single literal was wrong).
- NOT changed: 02-methodology.tex "pinned 32-byte value" = the KAT SHAKE256 digest, correct.

Task 2 DONE — ML-DSA alignment audit:
- Verdict: no over-strong claim existed to retract. The report nowhere says LAS IS ML-DSA-65
  or inherits a security category, and disclaims formal equivalence in three places
  (tab:params caption, tab:classical caption, sec 3 threats-to-validity). The only offending
  sentence was the c_tilde one fixed in task 1. Ch1/4/5 make no parameter-set-specific claims.
- What was missing was the POSITIVE half of the required framing. Added (captions/tables are
  word-free under texcount -sum=1,1,0,0,0):
  * tab:params caption — Simplified Dilithium-II/III/V "align the reusable ML-DSA primitives
    and the challenge-digest strength |c_tilde| with ML-DSA-44/65/87 while retaining LAS's own
    ternary secret distribution, rejection bounds, exact hint-free relation and unoptimised
    serialization"; explicit "a matched dimension is not a claim that LAS IS the corresponding
    ML-DSA parameter set or inherits its security category"; LAS-2020/845 reference set marked
    as a historical paper-parameter reference point corresponding to no ML-DSA set.
  * tab:notation — new row |c_tilde| = 32 (project choice), 32, 48, 64, with the meaning column
    stating lambda/4 applies only to the three ML-DSA-aligned sets. Royce rejected a first
    draft that labelled the row "FIPS 204's lambda/4 rule" flat, since that misattributes the
    paper-reference set's 32 to a rule that does not cover it (ref/setup.h says the same).
  * tab:notation caption — 2020/845 does not fix the digest width; it is an implementation
    choice taken from FIPS 204 wherever a set aligns with an ML-DSA one.

Decisions:
- Literal 48 kept in sec 2.3 prose; NO \ctildeBytesTarget macro and NO gen_report_data.py
  change during the prose-only pass (Royce). Report already carries the evidence-backed value
  via generated/tab_complete_target.tex.

DEFERRED — later pipeline fix (do NOT do in a prose pass):
- generated/tab_complete_target.tex labels the 48 bytes "Challenge (c)". The object on the wire
  is the digest c_tilde, not the challenge polynomial c — the same conflation that produced the
  32-byte bug. Rename to "Challenge digest (c_tilde)". Source is ref/test/bench_levels.c ->
  evidence tables (las_object_catalogue.csv / communication_components.csv, column
  paper_notation) -> scripts/gen_report_data.py; needs a sync_report.sh regeneration.

Evidence used:
- evidence/latest (20260729_180517) read-only: las_object_catalogue.csv (L3 challenge = 48),
  logs/fair_l{2,3,5}.log and fair_paper.log (c_tilde 32/48/64/32). No runs, no edits.

Verification status:
- No LaTeX build run (guardrail: Royce runs builds). Macros used are all pre-existing;
  \lvert..\rvert is safe (amsmath loaded, report.tex:27).

Unresolved risks:
- Word count not re-measured this pass; task 3 (trim 9658 -> 7000-9000) NOT started.

Next action:
- Task 3: coverage-based trim. Still-to-do duplication from CONTEXT.md: Ch4 sec 4.4 (repeats
  sec 3.7), Ch2 sec 2.6 (duplicates Ch4 sec 4.1), Ch2 sec 2.3/2.5 (appendix-level detail).

## Checkpoint — 2026-07-30 (cont.) — task 3 word trim, partial

Branch: report

Authoritative sources added this session:
- MSc_Report_and_Video_Rubric.md read. Line 28: ~8,000 words, "significantly outside
  7,000-9,000 will be penalised"; references/appendices/CAPTIONS excluded -- which is
  exactly what report/latex/Makefile's texcount -sum=1,1,0,0,0 + %TC:ignore implement,
  so the measured number is the right one. Line 50: the criteria are NOT section-heading
  requirements (only Abstract and Conclusion must be separate sections).
- Lecture5-ResearchWriting-2026-Lin.pdf converted to
  docs/references/Lecture5_ResearchWriting_2026_Lin.md (pdftotext -layout, 43 slides).

Word count: 9,563 -> 9,193 (-370). Target agreed with Royce: ~8,700 (not 8,000).

Why the target moved off CONTEXT's ~8,000 (evidence, not preference):
- filler scan: only 3 stock-phrase hits in the whole report.
- cross-chapter 7-gram scan: 35 shared n-grams total, ~150 words of genuine redundancy,
  the only real clusters being Ch1<->Ch5 aim restatement and the ledger-model caveat.
- therefore ~1,300 of CONTEXT's 1,450-word gap would have to be UNIQUE ANALYSIS deleted,
  from Methodology/Results, which feed three 20%-weighted rubric criteria.
- the rubric penalises only "significantly outside 7,000-9,000"; 8,700 is fully compliant
  and there is no extra mark for hitting 8,000. Royce chose ~8,700 via AskUserQuestion.

Rubric/Lin compliance checks run (all PASS): abstract has 0 \cite; no \subsubsection;
no equation* (every display equation numbered); zero first-person we/our/us.

Chapter balance (Lin slide 37: same-level sections similar in length; Intro/Conclusion
may be shorter). Ch4 Evaluation is the outlier at 933 vs Ch2 2,645 / Ch3 2,605.
DECIDED: leave Ch4 alone. Rubric line 50 means "Evaluation and/or Reflection (20%)" is
not measured by Ch4's length -- it is covered across Ch3 (testing, threats to validity),
Ch4 (objectives, challenges, limitations) and Ch5 sec 5.2 (critical reflection). Moving
analysis into Ch4 for balance would risk 20%-weighted criteria to buy part of the 5%
Format-and-Structure mark. Trim comes from Ch2/Ch3 instead, which improves the ratio.

Edits applied this pass:
- Ch4 sec 4.4 Limitations: dropped the four items already in Ch3 sec 3.7; kept the two
  genuine artefact limitations. Royce correction: config 2 trades POST-QUANTUM ASSURANCE
  for a controlled comparison, NOT cryptographic optimisation -- and it maps to no
  sec:future item (configuration 3 answers it inside the study), so the original
  "each maps to an item of sec:future" could not be carried over.
- Ch2 sec 2.6: removed the justification half that Ch4 sec 4.1 owns (method stays in Ch2).
- Ch2 sec 2.3: removed the decoder enumeration duplicated verbatim in app:serialize.
- Ch2 sec 2.1: cut the pairing clause duplicating sec 2.6.
- Ch2 sec 2.7: ledger boundary KEPT in body per Royce ("pertahankan batas model secara
  ringkas") -- compressed and split, not deferred to the appendix.
- Ch2: Groth16-circuit and 2->3-controlled-step paragraphs compressed/split.
- Ch3: core-tier structural paragraph (68-word embedded list, Lin slide 16), Rust
  cross-check paragraph, classical two-findings paragraph.
- Ch5 sec 5.1: aim restatement no longer duplicates Ch1 sec:aims verbatim; numbers
  paragraph now concludes rather than re-derives.
- Ch5 sec 5.2: compressed; Royce correction -- restored "exposed every timing conclusion
  to an independent harness", a methodological benefit DISTINCT from correctness
  cross-validation that a first draft had collapsed away.
- Abstract 477 -> 350 (biggest single win): dropped methodology-level detail and the
  future-work clause, kept every result. Rubric asks for a concise executive summary.

Verification status:
- make wordcount run repeatedly (explicitly requested). No LaTeX PDF build run.
- No evidence regenerated, no numbers retyped; all figures still come from macros.

Unresolved:
- 9,193 vs ~8,700 target = 493 words still to cut. 44 sentences over 40 words remain
  (Ch2 12, Ch3 12, Ch5 10, Ch4 5, Ch1 3, Abstract 2). Compression yields ~8-12 words
  each, so the remainder is a mechanical grind of roughly 40-50 more edits, NOT a
  quick pass. No further duplication or filler is available to harvest.

DEFERRED pipeline fix (unchanged): generated/tab_complete_target.tex labels the 48 bytes
"Challenge (c)"; the wire object is the digest c_tilde. Fix in ref/test/bench_levels.c ->
evidence CSVs -> scripts/gen_report_data.py, needs sync_report.sh.

Next action:
- Continue sentence-level compression through the remaining 44 long sentences,
  concentrating on Ch2 and Ch3, re-measuring every few edits.

## Checkpoint — 2026-07-30 (cont.) — figure/table regeneration, runners, EVM fixes

Branch: report. Word count 8,955 (limit 9,000).

RUNNERS ADDED (all follow one pattern: timestamped evidence dir, raw tool output,
environment/commit record, `latest` symlink, automatic macro/figure regeneration):
- scripts/run_swap_bench.sh   -> evidence/stage2/<ts>/
- scripts/run_onchain_gas.sh  -> evidence/onchain/<ts>/  (re-exports the signature
  fixture from ref/ FIRST, so gas is always measured at the current SIGNATURE_BYTES)
- scripts/run_criterion_fig.sh -> evidence/criterion/<ts>/ (+ --reuse flag)
- sync_report.sh FIGMAP extended with fig_rejection_cdf + fig_rejection_dist.

REAL BUGS FOUND AND FIXED (not cosmetic):
- LASNaysayer.sol decoded z from `bit = 256` (= 32 B x 8, the OLD c_tilde offset in
  BITS). The earlier CTILDE_BYTES sweep only caught byte-valued 32s, so the Naysayer
  read the response vector from the wrong position. Now CTILDE_BYTES * 8.
- evm/test/las_sig.bin was 6720 B (Jul 22) and evm/test/vectors/{sig,msg,z,pp_normal,
  t,w_prime,sig_digestfault}.bin were MISSING -> 6 EVM suites failing. Re-exported from
  ref/ (sig 6736, C base_verify = ACCEPT). forge test now 45/45 PASS (was 39/45).
- AdaptorSwap.sol + 2 .t.sol hardcoded `length == 6720`; LASSampleZ.t.sol sliced a
  32-byte c_tilde and decoded z from bit 32*8. All now derive from LASVerify.SIG_BYTES
  / LASVerify.CTILDE_BYTES so they cannot go stale again.
- plot_onchain_gas.py had gas values HARDCODED in the script. Rewritten to parse a
  captured `forge --gas-report` log (--log/--out); ratios derived, never typed.
- Report ran an unfilled \TODO placeholder and a chapter title too long for the running
  header; long \texttt paths overflowed. Fixed (\allowbreak, \chapter[short]{long}).
  Overfull boxes: 90.6/49.6/33.5/30.1pt all gone; worst remaining 14.4pt is the
  frontmatter TODO only Royce can fill.

RE-MEASURED (evidence/onchain/20260730_164836, sig_bytes=6736):
  claimClassical 75,763 | claimLAS 290,640 | claimLASVerified 56,647,378 (748x)
  Naysayer claim 1.2 M | naysayDigest fraud proof 28.2 M
  (were 75,751 / 289,930 / 56,538,682 / 746x / 1.1 M / 29.4 M)
  New macros \gasClassical..\gasNaysayDigestM emitted by gen_report_data.py from the
  captured log; every hardcoded gas figure in Ch3/Ch4/appendix replaced by a macro.

TABLE FIXES (Royce-directed):
- 3.6 tab:classical: overhead column is TIER-MATCHED per operation -- core tier for
  KeyGen/Sign/Verify, packed tier for PreSign/PreVerify/Adapt/Extract, because the
  classical library's single hybrid boundary is core-like for the first three and
  packed-like for the last four. \clRatioTimeMax moved to the same basis so prose and
  table cannot disagree (Adapt 47x -> 270x; PreVerify 0.58x -> 2.4x).
- 3.9 tab:stage2-comm: on-chain sub-components now shown (tx1+sigma1, tx2+sigma2), each
  group with its own total, then total per swap. Arithmetic verified against the log.
- 4.1 tab:objectives: Verdict column removed (caption now carries "all five were met").
- 4.2 tab:challenges: no overflow remained after the \texttt fixes.

STAGE-2 RE-RUN (evidence/stage2/20260730_162109): signature 6736 everywhere; report now
has 14x 6736 and 0x 6720. Also fixed a degenerate "99.3-99.3%" range and a "rising to"
between two identical numbers (all sets round to 99.3%).

Next action:
- Trim 8,955 -> 8,000 (955 words). Duplication/filler exhausted; remaining source is the
  ~40 sentences over 40 words plus further high-level-ing of sec 2.6 (848 w) and 2.7.
- Fill the frontmatter \TODO (student id, prior degrees, acknowledgements) -- Royce only.

## Checkpoint — 2026-08-03 — remaining-work sweep: AMHL cleanup, Adapt gap, ML-DSA hint experiment, EVM/IPFS write-ups

Branch: report. No commits made. Royce mid-session: "skip the report for the moment,
focus on the work being done first" -> report edits paused after the Adapt fix; Ch4/Ch5
report follow-ups are recorded as owed in CLAUDE.md.

DONE
1. AMHL cleanup (was owed in CLAUDE.md). Report: tab:reuse row, 03-results multi-hop
   result claim, A-appendix "three scenarios" -> two. Docs: 51 edits across 22 files via
   two assertion-guarded scripts (every anchor asserted to match exactly once, no file
   written unless all matched). Literature mentions kept per the (a)/(b) test; dead
   sections replaced by a DROPPED banner rather than erased. Also removed the dead
   bench_app3/test_pcn3/test_amhl3 targets from ref/Makefile `all`.
2. Adapt ~270x explanation (Wang, owed). Root cause found by reading BOTH algorithms:
   eprint 2020/845 Alg. 2 line 21 obliges LAS Adapt to run PreVerify; secp256k1's
   ecdsa_adaptor_decrypt does NOT verify (deserialise + one scalar inverse + one mul;
   verification is a separate exported call). So the ratio compares different work.
   Added 6 DERIVED macros to scripts/gen_report_data.py (nothing typed): 69% of LAS
   Adapt is the mandated PreVerify; like-for-like (charging classical its verify) is
   3x, not 270x; codec 15.3 kB. Regenerated generated/*.tex; rewrote 03-results.tex
   §3.6. Note: the old prose said "some 23 kilobytes" - that was the D3 figure in a D2
   table; now a macro at the table's own parameter set.
   The earlier answer "the pre-signature is huge" was wrong: size drives only the
   smaller (codec) term.
3. ML-DSA HINT EXPERIMENT (highest-novelty item) - built, run, evidence captured.
   New: ref/mldsa_las.{c,h} (structural mirror of ref/sign.c, [REUSED]/[CHANGED]/[NEW]
   annotations, zero upstream functions modified), ref/test/test_mldsa_hint.c,
   Makefile targets test/test_mldsa_hint{2,3,5}, scripts/run_mldsa_hint_experiment.sh,
   write-up docs/03-results/MLDSA_HINT_EXPERIMENT.md, STATUS D27.
   Evidence: evidence/mldsa_hint/20260803_124058 (all three ML-DSA sets, 200 iters).
   THE RESULT CORRECTS THE PROJECT'S OWN CLAIM:
     - signer side MUST be modified: naive port (statement in the hash, nothing else)
       fails PreVerify 0/200. The whole commitment path - committed high bits,
       low-bits rejection test AND MakeHint - has to move onto w+Y, and PreSign must
       tighten to GAMMA1-BETA-ETA.
     - verifier side must NOT: the UNMODIFIED FIPS 204 crypto_sign_verify accepts the
       adapted signature 200/200 at ML-DSA-44/65/87, with hint + Power2Round +
       high/low-bit split all ENABLED.
     - cost ~0 in rejection sampling (+7.2% at ML-DSA-65, within the tightened-bound
       prediction); statement Y is LARGER than the signature at every set (4416 vs
       3309 B at -65) because Power2Round cannot compress Y.
   METHOD NOTE worth keeping: the first run showed P4 failing 0/200 in BOTH variants -
   which looked like a headline finding and was actually a harness bug. PreSign/
   PreVerify omitted FIPS 204's 2-byte context prefix {0,0} that crypto_sign_verify
   absorbs into mu. Fixed, and a matched no-statement baseline (VBASE) was added as a
   FIDELITY GATE: it must verify under the stock verifier, and it reproduces FIPS 204's
   own repetition rates (4.255/5.105/3.865 vs ~4.25/5.1/3.85). Without that gate the
   bug would have shipped as a result.
   CAVEAT that must travel with the claim: functional demonstration only - the security
   of committing to HighBits(w+Y) is NOT analysed (security analysis stays out of scope).
4. EVM/Naysayer write-up: docs/02-methodology/EVM_TX_STRUCTURE.md - the EVM counterpart
   of BITCOIN_TX_STRUCTURE.md, per Meeting 8's "same question applies to the EVM".
   No new measurement; quotes evidence/onchain/latest. Key contrast recorded: Ethereum
   needs NO consensus change (LAS is application payload in `data`, never the tx's own
   secp256k1 signature) but the only verifying path is 3.4x over the EIP-7825 cap;
   Bitcoin needs a consensus change but then meters by size. Naysayer stated as a
   negative result (naysayDigest 1.7x over the cap).
5. IPFS fallback: docs/04-evaluation/IPFS_OFFCHAIN_STORAGE.md. Documentation only.
   Headline: the swap needs none of it (pi is a direct party-to-party message); for the
   optimistic verifier, data-availability failure becomes a SOUNDNESS failure.
6. Verified Royce's ChatGPT future-work list against Wang's rulings - see the reply.
   Two items rejected: multi-hop/payment channels (DROPPED by standing ruling, removed
   from the docs in item 1 today) and "port to ML-DSA" as future work (it is item 3,
   already done, and its purpose is evidential not production).

WHAT I RAN (deviates from the usual "Royce runs everything"): the hint experiment
(a correctness diagnostic whose OUTPUT is the deliverable - writing it up unrun would
have meant guessing), plus scripts/gen_report_data.py. No benchmark was run; no timing
number was produced or changed.

NEXT
- Report (deferred by Royce): fold the hint result into Ch. 4 - it corrects the claim at
  04-evaluation.tex:169 - and rewrite Ch. 5 future work (drop the q~2^24 bullet).
- 6-8 minute slides.
- Word count re-check after the above; frontmatter TODO is Royce-only.

## Checkpoint — 2026-08-03 (cont.) — full ML-DSA adaptor experiment + head-to-head benchmark

Branch: report. No commits. Royce: "lakukan full ML-DSA experiment secara utuh dan
komprehensif, lalu lakukan comparison benchmark dengan implementasi yang ada sekarang."

BUILT (all under evidence/mldsa_hint/latest, runner run for modes 2/3/5):
- ref/mldsa_las.{c,h} completed into a FULL scheme: added the wire format --
  statement = 23-bit-per-coefficient codec (Power2Round cannot compress Y, it enters
  the identity before rounding), witness = ML-DSA's own polyeta codec, BOTH decoders
  validating. Also fixed mldsa_las_gen to leave Y canonical in [0,Q) (polyveck_reduce
  leaves centred reps, which broke the codec round-trip).
- ref/test/test_mldsa_las.c: itemised contract, 13 items -- 7 positive, 5 negative
  (tampered msg / tampered pre-sig / wrong statement / wrong witness / malformed
  statement bytes), 1 determinism. PASS 13/13 at ML-DSA-44/65/87.
- ref/test/bench_mldsa_compare.c: head-to-head, BOTH constructions in ONE binary
  (name spaces don't collide: LAS_N/ELL/KAPPA vs K/L/GAMMA1/GAMMA2). Same protocol as
  bench_levels (5 reps x 500/1000, mean+-SD, direct attempt counting).
- Makefile: test_mldsa_las{2,3,5} + bench_mldsa_compare{2,3,5}, in `all` and `clean`.
- scripts/run_mldsa_hint_experiment.sh: now runs all three binaries per mode,
  --skip-bench flag, exits non-zero if any contract/gate fails.

RESULTS
- Fidelity gate passes; matched baseline reproduces FIPS 204 repetition rates
  (4.246/5.217/3.996 vs ~4.25/5.1/3.85) and the matched partner lands on stock
  crypto_sign_signature (514.73 vs 522.41 us at level 3).
- Adaptor overhead PAIRED per attempt: simplified +1.3/+2.2/+2.4%, ML-DSA
  +3.8/+2.8/+3.5%. Single-digit on both -> ML-DSA costs no extra adaptor overhead.
- Per-signature cost nearly equal FOR OPPOSITE REASONS: ML-DSA ~2x the restarts
  (5.22 vs 2.71) but ~half the price per attempt (gamma1 bit-unpack over L=5 vs
  rejection sampling over n+ell=11).
- SIZES are the real story: ML-DSA halves sig (3309 vs 6736) and pk (1952 vs 4416),
  but Y is BYTE-IDENTICAL (4416 both) -> swap payload only 0.69x, not 0.49x.
  Y > signature at every set. Future size work must target Y, not the signature.

FIVE MEASUREMENT FAULTS CAUGHT BY GATES (all would have shipped wrong numbers; the
rules they forced are now recorded in CLAUDE.md and must not be weakened):
1. Missing FIPS 204 2-byte empty-context prefix {0,0} in mu -> P4 failed 0/200 in BOTH
   variants and looked like the headline finding. Harness bug, not a result.
2. Deterministic rejection loop: benchmark re-signed one fixed instance -> ML-DSA
   restarted identically every call (exactly 4.0000 / 2.0000 attempts). Gate caught it.
3. Stale key: the KeyGen benchmark replaced the keypair, so Verify was timed on an
   invalid signature = the REJECTION path. Now every timed block has a success-path
   assertion (EXPECT_OK).
4. Drift-inverted overheads: Sign and PreSign measured in separate blocks let clock
   drift land on one -> overheads swung -3%..+8%, sometimes negative. Fixed by PAIRED
   INTERLEAVED ratios (alternate within each repetition, ratio per rep, mean+-SD) plus
   an untimed warm-up of both constructions before anything is timed.
5. Mislabelled column: simplified column hardcoded "Dilithium-III" at all three modes.
   Now keyed on (LAS_N, ELL, KAPPA) with #error on an unrecognised set.
Also added: attempt-counter sanity check (delta >= number of timed calls).

Royce's mid-session review (via ChatGPT) asked for 5 verifications; all addressed --
fresh rnd per ML-DSA signing call (confirmed, plus config.h already randomizes stock
signing), m_sig regenerated after the keypair changes (was a REAL bug, fixed),
da_ >= niter validated, g_att_mean/sd stored and used (4 sites each), and the
"interleaved" claim in the header corrected (the two CONSTRUCTIONS are sequential;
only the overhead PAIRS are interleaved).

NEXT
- Report (still deferred): fold the hint + comparison results into Ch. 4, rewrite Ch. 5
  future work (drop q~2^24; add "target Y, not the signature").
- 6-8 minute slides.

## Checkpoint — 2026-08-04 — all chapters updated for the ML-DSA result; word count 10,361 -> 8,990

Branch: report. No commits. Royce: "update all chapters ... only novelty / most important
/ 'gold' content, absolute max 9000, always refer to the rubric."

CRITICAL FINDING FIRST: the stored report/latex/word.count said 8,955; the REAL body
count was 10,361. Several past checkpoints planned trims against the stale number. The
fencing is correct (%TC:ignore excludes frontmatter/appendix/bibliography, -sum weights
exclude captions), so 10,361 was genuinely 1,361 OVER the hard limit. Always run
`make -C report/latex wordcount` before reasoning about budget. Also learned: tabular
BODIES count, but TikZ picture content and generated/*.tex tables count ZERO.

THE CORRECTION THE EXPERIMENT FORCED (3 places, all applied):
- 02-methodology "Simplifications": dropped "which the adaptor mechanism requires" --
  refuted by the experiment. Now says the question was TESTED, not assumed, and forward-
  references sec:res-mldsa. Modulus reframed as FIPS 204's (the parameter authority),
  and the "migrating is future work" clause deleted.
- 04-evaluation challenge 2: was "the simplified scheme is a design requirement, not a
  convenience". Now: the belief was tested and proved too strong -- a SUFFICIENT route,
  not a necessary one.
- 05-conclusion future work: q~2^24 bullet DELETED (dropped, not future work). New lead
  bullet "Shrink the statement, not the signature" + a new bullet on analysing the
  security of the ML-DSA variant.

NEW CONTENT (the novelty, ~410 words in Ch3):
- New section sec:res-mldsa "Testing the simplification: the adaptor on unmodified
  ML-DSA": naive port fails (0/200), repaired version 13/13 + stock FIPS 204 verifier
  accepts 200/200, adaptor overhead single-digit on both constructions, signature halves
  but Y is byte-identical so the payload only reaches 0.69x.
- Every number is a MACRO. Extended scripts/gen_report_data.py with parse_mldsa()
  reading evidence/mldsa_hint/latest -> 14 new macros (\mldsaContract, \mldsaRepairedP,
  \mldsaSigBytes, \mldsaStmtRatio, \mldsaPayloadRatio, ...). It dies loudly if the
  repaired variant did not hold P4 on every iteration, so the report cannot claim a
  result the evidence does not support. Nothing typed by hand.
- Abstract gained a 5-line paragraph on the ML-DSA correction.

WORD BUDGET (10,361 -> 8,990), per chapter:
  abstract   354 -> 408   (+54, ML-DSA para)
  intro     1132 -> 953   (-179, background trimmed per Meeting 8)
  method    2922 -> 2328  (-594)
  results   3073 -> 2814  (-259 NET, after +411 of new ML-DSA content)
  eval      1381 -> 1227  (-154)
  conclusion 1482 -> 1243 (-239)
Method's biggest saving was moving tab:tests, tab:notation and tab:tiers into the
appendix as a new section app:tests -- reference tables, uncounted there, and the
appendix is excluded by the rubric.

VERIFIED: `make` builds report.pdf clean, 0 undefined references, 0 undefined citations,
5 overfull boxes (pre-existing). make wordcount = 8990.

NEXT
- 6-8 minute slides (the last Meeting-8 deliverable).
- Frontmatter \TODO (student id, prior degrees, acknowledgements) -- Royce only.
- Optional: re-run scripts/run_benchmark_suite.sh, since ref/ sources are newer than
  evidence/latest (the freshness tripwire in CLAUDE.md).

## Checkpoint — 2026-08-04 (later) — two future-work items promoted to implemented experiments

Branch: report. No commits.

Royce asked for four Ch. 5 future-work directions. Asked which reading he wanted; he chose
"attempt the feasible ones", having been shown that this BREAKS the Meeting-8 feature
freeze and competes with report polish + slides. His call; proceeded.

BUILT (neither has been RUN -- no numbers exist yet):
- ref/test/test_statement_compress.c + Makefile targets test/test_statement_compress{2,3,5}
  (added to `all`). Diagnostic in the test_mldsa_hint shape. Candidates: C0 control (full
  Y), C1 Power2Round-style truncation swept b=1,2,4,8,13, C2 seed-instead-of-Y. Every
  adaptor call gets the COMPRESSED statement. Properties P1..P5; decisive rows P3 (base
  Verify accepts adapted sig -- the chain) and P4 (Ext recovers the witness -- atomicity).
  Hard gate: control must hold or nothing is attributable. gcc -fsyntax-only -Wall -Wextra
  clean at set 3.
- rust/las-swap: BatchedRelationCircuit in groth16_circuit.rs + new bin bench_amortise.rs
  (registered in Cargo.toml, builds without the groth16 feature and says it needs it).
  Refactor: the relation body was factored into `emit_instance`, now shared by the single
  and batched circuits, so a batch cannot prove something weaker than k single proofs.
  Per-batch tamper check on the LAST instance's public input enforces that at runtime.
  k = 1,2,4,8; warm-up + 5 reps + success-path assertions per the ML-DSA measurement rules.
- scripts/run_statement_compress.sh -> evidence/statement_compress/<ts>/
  scripts/run_amortise_bench.sh     -> evidence/amortise/<ts>/   (both bash -n clean, +x)
- docs/03-results/STATEMENT_COMPRESSION_EXPERIMENT.md
  docs/03-results/PROOF_AMORTISATION_EXPERIMENT.md
  Both state "implemented, not yet run" and contain ZERO numbers.
- CLAUDE.md: new block recording the freeze override, both capabilities, and the two items
  deliberately NOT attempted.

NOT ATTEMPTED (stated to Royce, stays in Ch. 5 future work):
- "Analyse the ML-DSA variant's security" -- barred by the standing supervisor ruling that
  security analysis is out of scope; it is a reduction, not code.
- "Solve one-transaction verification" -- needs a SHAKE256 precompile / Merkle-opened
  dispute / succinct proof of verification; a new artefact far beyond the freeze.

UNRESOLVED RISKS:
- The Rust side is NOT compile-checked (cargo check would build arkworks = a real build,
  which Royce runs). Only the C file was syntax-checked. First `cargo run --features
  groth16` may surface type errors in bench_amortise.rs / groth16_circuit.rs.
- bench_amortise regenerates a fresh SRS per repetition per batch size; k=8 is ~230k
  constraints. Expect several minutes and significant RAM. Reduce REPS or BATCHES if it
  is too slow.

NEXT
- Royce: `./scripts/run_statement_compress.sh` then `./scripts/run_amortise_bench.sh`.
- Then rewrite 05-conclusion.tex:118-129 (the two bullets these answer) against the
  evidence, and decide whether either earns space in Ch. 3 within the 9,000-word ceiling.
- Still owed: 6-8 min slides; frontmatter \TODO (Royce only).

## Checkpoint — 2026-08-04 (later still) — both new experiments RUN; results in

Branch: report. No commits. Royce said "run", which overrides the standing don't-build rule.

STATEMENT COMPRESSION -- evidence/statement_compress/20260804_112718 (all 3 sets, 100 iters)
Verdict: the assertion is CONFIRMED and now has a mechanism.
- Truncation: P1 PreVerify 100/100 and P2 Adapt 100/100 hold, while P3 base Verify and
  P4 Ext are 0/100 -- at EVERY depth b=1,2,4,8,13, at all three sets. b=1 is only a 4%
  saving and already fails 100/100. Not a marginal failure.
- Seed candidate: 4416 -> 32 B (138x), all 5 functional rows 100/100 (Y exact), and the
  receiver recovered the witness 100/100. Largest compression = total break, demonstrated.
- Hint repair: net +0 B at every depth, by construction. Lossless baseline already applied:
  23-bit packing 4416 vs 6144 B naive (-28%).
- Control held at every set, so all rows are attributable.

PROOF AMORTISATION -- evidence/amortise/20260804_114004 (k=1,2,4,8 x 5 reps, exit 0)
Verdict: NEGATIVE result for configuration 2, and worth reporting as one.
- Proof is 128 B at EVERY k; per-swap 128/64/32/16 = exactly 1/k.
- BUT 128 B was never the bottleneck. Recomputed from evidence/stage2/latest: Prove(pi) is
  645,621 us = 97.3% of the swap and 47.1x the next phase. So batching amortises the cost
  that was already negligible and leaves the dominant one alone.
- Per-swap proving 494 -> 653 ms (+32% at k=8) -- flat to WORSE, never better. Across the
  three runs that day the trend varied, so do not claim a monotone law.
- Per-swap verify flat (~12 ms). Setup 667 -> 5687 ms, per circuit.
- Framing: this is about GROTH16, not batching. The 1/k would matter for a large-proof /
  cheap-generation system (LaZer's profile) -- NOT measured.

CORRECTION MADE MID-RUN (why evidence/amortise has 3 dirs):
The binary's own closing prose asserted the swap was proof-dominant in BOTH time and
communication. Checked against evidence/stage2/latest: false -- dominant in time only
(128 B proof). Fixed the prose + a `%%` Rust format bug (Rust does not escape %), re-ran.
latest = 20260804_114004 is the one to quote. Earlier two are superseded, not deleted.

FILES CHANGED SINCE LAST CHECKPOINT
- rust/las-swap/src/bin/bench_amortise.rs (interpretation corrected, %% fixed)
- docs/03-results/{STATEMENT_COMPRESSION_EXPERIMENT.md,PROOF_AMORTISATION_EXPERIMENT.md}
  -- both now "run", with measured tables; compression doc section numbering fixed
- CLAUDE.md -- both results recorded under the promoted-future-work block

NEXT
- Rewrite 05-conclusion.tex:118-129. Both bullets are now ANSWERED, not open:
  "Shrink the statement" -> settled negative within this construction; the live question is
  a different hard relation whose statement is small by design.
  "Reduce the proof" -> amortisation measured and it targets the wrong cost for Groth16;
  the live question is a succinct PQ system, or batching a large-proof system.
- Decide whether either earns body space inside the 9,000-word ceiling (currently 8,990),
  or stays an appendix/doc-only result.
- Still owed: 6-8 min slides; frontmatter \TODO (Royce only).

## Checkpoint — 2026-08-04 — LaZer amortisation measured; the question is now CLOSED

Branch: report. No commits. Royce: "uji LaZer sekarang" (test LaZer now) -- the open item
the Groth16 run left behind.

KEY CORRECTION TO AN EARLIER BELIEF: SageMath IS available on this machine
(~/micromamba/envs/lazer-sage/bin/sage). The earlier note that batched LaZer was blocked on
regenerating relation_zk_params.h was wrong -- codegen took ~30 s at k=2, minutes at k=8.

NEW CAPABILITY (batched pi, NOT wired into the swap):
- ref/relation_zk_batch.{c,h} -- block-diagonal statement, k copies of [I|A'|-I|-A'|0] on
  the diagonal, off-block zero => the batch is the CONJUNCTION of k copies of the deployed
  statement. Per-instance block is byte-identical to relation_zk.c's.
- ref/relation_zk_lazer_batch.{c,h} -- the second (and only other) TU that includes lazer.h.
  k=1 dispatches to the COMMITTED las_pi_params, so the baseline IS the shipped prover.
  New gate names PI_BATCH_* -- they do NOT rename/alias PI_ROWS/PI_COLS/PI_DEG/
  PI_PROOF_MAX_BYTES, which remain the k=1 module's.
- ref/relation_zk_params_k{2,4,8}.h -- COMMITTED generated sets (sage not needed to build).
- scripts/gen_lazer_batch_params.sh -- regenerates them (needs sage).
- ref/test/bench_lazer_amortise.c + scripts/run_lazer_amortise.sh ->
  evidence/lazer_amortise/20260804_122156. Built clean, no warnings. Exit 0, all gates
  passed (success-path assertions + per-batch tamper check on the LAST instance).

RESULT -- batching fails for BOTH provers, for OPPOSITE reasons:
- Groth16: proof/swap 128 -> 16 B (perfect 1/k) but 128 B was never the bottleneck;
  per-swap compute flat.
- LaZer:   proof/swap 30723 -> 17645 B (0.57x, -43%) -- a REAL saving on config 3's
  dominant communication object, and the first real saving in this direction.
  BUT per-swap prove+verify is 3.33x WORSE (prove 159->445 ms, verify 75->335 ms):
  LaZer's work grows SUPERLINEARLY in the batch.
  Since role-A pi is already 98.6% of config 3's end-to-end time, the binding constraint is
  COMPUTE -- batching buys bytes by spending exactly that. Wrong way round.
Measured LaZer sizes track the codegen predictions (31.3/47.7/78.1/144.7 KiB) closely.

ALSO CORRECTED THIS SESSION: bench_lazer_amortise.c's closing prose originally asserted the
sublinear size win was "a saving on something that mattered" and stopped there. After seeing
the compute column that was incomplete, so the binary now computes and prints the trade-off
(size ratio vs compute ratio) rather than a pre-written conclusion. Same class of mistake as
the Groth16 %% / "dominant in BOTH" fix earlier today: do not pre-write conclusions.

FILES CHANGED
- ref/{relation_zk_batch.c,relation_zk_batch.h,relation_zk_lazer_batch.c,
       relation_zk_lazer_batch.h,relation_zk_params_k{2,4,8}.h,Makefile}
- ref/test/bench_lazer_amortise.c
- scripts/{gen_lazer_batch_params.sh,run_lazer_amortise.sh}
- docs/03-results/PROOF_AMORTISATION_EXPERIMENT.md (now covers BOTH provers)
- CLAUDE.md

NEXT
- Rewrite 05-conclusion.tex:126-129 ("Reduce the proof"): amortisation is now ANSWERED
  negatively for both provers, with the LaZer trade-off quantified. The live question is a
  succinct PQ proof system, not batching.
- Also still owed: 05-conclusion.tex:119-125 (statement compression, answered negatively).
- 6-8 min slides; frontmatter \TODO (Royce only).

## Checkpoint — 2026-08-04 — ZK correction to the STARK result; LaBRADOR groundwork

Branch: report. No commits. Royce: "baca 2020-845.md 4.1 ! PROOF NYA HARUS ZERO KNOWLEDGE !
Jalankan relation yang sama menggunakan backend LaBRADOR di LaZer !"

THE CORRECTION (this is the important part):
Read eprint 2020/845 4.1. The swap's security needs pi to HIDE the witness -- if u2 learned
r from pi it could adapt sigma-hat_1 itself and take BOTH sides. So pi must be ZERO-KNOWLEDGE,
not merely a proof of knowledge. The FRI-STARK built earlier today is NOT zk (Winterfell adds
no zk randomisation), so it is NOT a valid pi. I had written that as a scope caveat; that was
too weak -- it is a DISQUALIFICATION, and it also means the three-prover comparison flattered
the STARK on an axis where it does not qualify (LaZer and Groth16 both pay for zk).
Applied to: docs/03-results/SUCCINCT_PQ_PROOF_EXPERIMENT.md (banner + scope + new section 6),
CLAUDE.md (merged into the las-stark block, not appended).

LABRADOR GROUNDWORK (library builds; the RUN IS NOT DONE):
- third_party/lazer/src/labrados was an UNINITIALISED git submodule (lazer-crypto/labrador).
  Fetched at pinned commit 3f95485. Built liblabrador{32,36,38}.so (needs -lmvec -lm only).
- TRAP FOUND, recorded in CLAUDE.md: LaZer's shipped src/labradosNN_py.h declares internal
  ring degree N=64, but the submodule the Makefile builds defines N=256. Struct layouts
  disagree -> a C driver against the shipped _py.h would corrupt memory. Must use
  src/labrados/labrados_python.h with -DLOGQ=NN -DNDEBUG -Isrc/labrados.
- Python/cffi route is BLOCKED: liblazer.a is not -fPIC, so _lazer_cffi fails to link. Would
  need liblazer.so (lib-shared-all). Also needs LIBRARY_PATH=~/micromamba/envs/lazer-build/lib
  for -lmpfr. C driver is the better route anyway (matches project convention).
- Encoding worked out (NOT yet coded): witness w0 = (r_plus || r_minus), 22 polys, normty BIN
  (native binary norm type -- matches the deployed LNP22 decomposition); witness w1 = g, 6
  polys, L2EXACT bound; 6 constraints [A|-A]*w0 - q*g = t'. g is needed because LaBRADOR
  works over ITS prime p, not our q; the l2 bound on g is load-bearing (unbounded g satisfies
  the equation for any t'). LOGQ=38 REQUIRED: binary decomposition doubles columns -> coeffs
  ~2^35 vs LOGQ=36's p/2 ~ 2^35.
- API: labrador38_py_{init_witness,set_witness_vector,init_statement,append_constraint,
  gen_params(zk flag!),simple_verify,prove,verify}. Layout: witness element i = deg*N int64
  coeffs; phi = per vector, n[i] polys of deg*N coeffs consecutively; b = deg*N.
  Dev strategy: use py_simple_verify(st,wt) to check the ENCODING before proving.
- MEASUREMENT CAVEAT: proof size is only available via py_gen_params' printed "Estimated
  proof size" (dch_pack_params_gen returns it but is hidden by -fvisibility=hidden). That is
  NOT byte-exact like LNP22's prooflen; any comparison must say so.

NEXT
- Write ref/relation_zk_labrador.{c,h} (third lazer-adjacent TU, labrados headers ONLY) +
  ref/test/bench_labrador_role_a.c, encode as above, check with py_simple_verify first.
- Then measure zk=1 vs the LNP22 baseline and fold into SUCCINCT_PQ_PROOF_EXPERIMENT.md.

## Checkpoint — 2026-08-04 — LaBRADOR run: succinct+PQ+zk measured; direction CLOSED

Branch: report. No commits. Royce: "run" (the LaBRADOR driver laid out last checkpoint).

RESULT: it works, and it closes the succinct-PQ-proof direction.
- LaBRADOR is the ONLY prover tested that is succinct AND post-quantum AND zero-knowledge,
  i.e. the only succinct candidate that actually satisfies 2020/845 Sec 4.1.
- At this statement size it LOSES to the deployed LNP22 on every axis (~3.7x proof, ~10x
  prove, ~10x verify). Same lesson as the STARK from the other side: succinctness is
  asymptotic and one role-A relation (28 polys) sits deep in the fixed-overhead regime.
- Encoding gate ACCEPT, every proof verified, 5 reps + warm-up, zk ON.
Evidence evidence/labrador_role_a/20260804_173459. Numbers live there and in the write-up
section 6; do not retype them elsewhere.

BUILT
- ref/relation_zk_labrador.{c,h} -- THIRD (and last) vendored-proof-library TU. Encoding
  [A|-A]w - q*g = t', w binary via LaBRADOR's native BIN norm type, g an l2-bounded quotient.
- ref/test/bench_labrador_role_a.c (+ Makefile target, LABRADOR_LOGQ=38 default).
- scripts/run_labrador_role_a.sh -> evidence/labrador_role_a/<ts>/.

THREE TRAPS HIT, ALL NOW IN CLAUDE.md + the runner header:
1. src/labrados is a git submodule the README's LaZer clone does NOT fetch. That is why
   liblazer.a had zero labrador symbols. `git submodule update --init src/labrados`.
2. LaZer's shipped src/labradosNN_py.h declares internal N=64; the submodule it actually
   builds defines N=256. Struct layouts disagree -> building against the shipped header
   silently corrupts memory. Use src/labrados/labrados_python.h, -DLOGQ=NN -DNDEBUG.
3. labrados' simple_verify/verify return 1 on SUCCESS, opposite to the setters in the same
   header. I got this backwards first and it looked like a broken encoding. The encoding
   gate is what localised it -- worth keeping in any future bridge.

TWO THINGS I GOT WRONG AND FIXED MID-RUN (same class as the earlier %% / "dominant in BOTH"):
- The g bound: I derived an aligned-worst-case |g|inf<=641, which LaBRADOR REFUSED as too
  large to prove exactly. The negacyclic sum has cancellation; measured |g|inf is ~25-31.
  Replaced with a stated-margin bound (||g||^2 <= 1e8, ~1000x honest) and documented WHY it
  is a margin rather than a worst case.
- LOGQ: 36 was insufficient once the quotient is in the lifting sum. 38 uses ~78% of p/2.

CAVEATS THAT MUST TRAVEL (in the write-up, do not drop them)
- The encoding is OURS and may not be LaBRADOR's best: LaZer's python/labrados.py ships its
  own mod-q lifting helpers (num_pols_in_r, "As=t mod p -> As+qGr=t mod q"), suggesting a
  more economical decomposition exists. So this measures LaBRADOR THROUGH THIS ENCODING.
- Proof size is LaBRADOR's printed "Estimated proof size", NOT byte-exact like LNP22's
  prooflen (dch_pack_params_gen is hidden by -fvisibility=hidden). Never compare silently.
- Prove time is noisy (~18% SD). Not wired into the swap.

NEXT
- Ch5 "Reduce the proof" bullet can now be written against THREE measured results
  (amortisation both provers, STARK, LaBRADOR) -- the direction is closed, not open.
- Still owed: 6-8 min slides; frontmatter \TODO (Royce only).
- Optional: measure zk=0 vs zk=1 to price zero-knowledge (one-flag change).

## Checkpoint — 2026-08-05 — report updated for the three new results; STARK excluded

Branch: report. No commits. Royce: "report update and polish! jangan masukkan STARK karena
belum ZKP!" then, mid-trim: "jangan sampai pengurangan kata reduce quality ya" and
"you may tighten Context and motivation as well".

WHAT CHANGED IN THE REPORT
- 05-conclusion.tex future work: the two bullets the new experiments ANSWERED were rewritten
  from open questions into measured verdicts.
  * "Shrink the statement" -> "A statement that is small by design": compression tested and
    fails; truncation invisible to the adaptor's own functions, fatal at base Verify and Ext;
    seed derivation compresses completely and hands over the witness. Open question restated
    as a different hard relation, not a better encoding.
  * "Reduce the proof": both proposed routes measured, both fail at this scale. Batching
    amortises the wrong cost (Groth16 constant proof / LaZer pays several times the compute);
    succinct+PQ+zk run under LaBRADOR loses to the deployed prover on every axis.
- STARK deliberately ABSENT from the report (Royce's instruction): it is not zk, so not a
  valid pi per 4.1, and including a disqualified prover would overclaim. Recorded as a rule
  in CLAUDE.md so a later session cannot reintroduce it.
- No numbers were added to the report for these results -- the bullets are qualitative, which
  avoids hardcoding figures that have no macros yet. Evidence lives in docs/03-results/.

WORD COUNT: 9030 at session start (already OVER the 9,000 ceiling, drifted from the 8,990 in
the live block) -> 9131 after the bullets -> trimmed to 8999. Builds clean: 74 pages, 0
undefined references, 0 undefined citations.
Trims were prose-efficiency only, per Royce's constraint that trimming must not cost quality:
Ch5 (strategic-choice, target-selection, demonstrators, live-network bullet), Ch2 (modulus,
Rust caveat, three-configurations, parameter-sensitivity, venue, settled-transaction --
the last de-duplicated against fig:txstruct's caption, which is free), Ch1 (dropped a
throat-clearing framing sentence and the "this section reviews only..." signpost), plus
single filler words. Royce REJECTED one Ch2 cut that removed real signal ("where Y enters
and where y is revealed") -- correct call; the rule is cut filler, never signal.

NOTATION FIX: my first draft of the statement bullet used $\mathbf{Az}-c\mathbf{t}$ and
\textsc{Ext}; the report uses $A\vecz - c\,t$ and \textsc{Extract}. Corrected.

NEXT
- 6-8 min slides (last Meeting-8 deliverable).
- Frontmatter \TODO (Royce only).
- If these results ever need numbers in the report, add macros via scripts/gen_report_data.py
  rather than typing them -- there are no macros for the three new experiments yet.

## Checkpoint — 2026-08-05 (later) — audit of the word-trim: three substantive cuts restored

Royce asked whether, BEFORE he stopped me, I had cut load-bearing content. Audited via
git diff rather than memory. Answer: YES, three cuts took real substance and one created
an overclaim. All restored.

RESTORED:
1. "methodological" in "If one METHODOLOGICAL result deserves to outlive this
   dissertation". Dropping it broadened the claim to the single most durable result of the
   whole dissertation, competing with the technical findings. This was an OVERCLAIM I
   introduced -- the worst of the three.
2. "No module depends on a mode-specific constant" (Ch2 modulus) -- the REASON identical
   code runs at every parameter set. A concrete implementation fact, replaced by nothing.
3. "and exposed every timing conclusion to an independent harness" (Ch5 reflection) --
   the third distinct benefit of the Rust port.
Also restored: "a deployment figure for" (my version changed the claim from the FIGURE
being blocked to the SWAP being blocked), "implementation-level" (feasibility estimate),
"confirmation" (latency).

DELIBERATELY KEPT CUT, flagged to Royce: the enumeration of off-chain objects in sec 2.7
("statement Y, proof pi, both pre-signatures" / "and its own sigma-hat_2"). Both appear
verbatim in fig:txstruct's caption and figure box, and captions are word-count FREE, so
this is de-duplication rather than loss -- but the body now depends on the figure for it.
Royce can overrule.

HOW THE RESTORATION WAS PAID FOR (the rule is: trimming must not reduce quality):
Not from existing prose -- results and evaluation are tightly written and further cuts
would have cost content. Paid from MY OWN new future-work bullets instead, where I know
what is load-bearing, including two genuine duplications with the Conclusions section
("the role-A proof dominates a swap" and the ML-DSA signature/statement restatement).

FINAL: 8,999 words, under the 9,000 ceiling. Builds clean, 74 pages, 0 undefined
references, 0 undefined citations.

LESSON FOR FUTURE TRIMMING (do not repeat): compressing a sentence can silently BROADEN a
claim. "If one methodological result" -> "If one result" cost one word and turned a
scoped claim into an overclaim. When trimming, check whether a dropped qualifier was
narrowing a claim -- those words are load-bearing even when they look like filler.

## Checkpoint — 2026-08-05 — CLAUDE.md: standing "DO NOT REPEAT" rules for all sessions

Royce: "perintahkan semua sesi untuk tidak melakukan kesalahan yang sama lagi dan lagi."
Earlier in the same exchange he specified the target state for CLAUDE.md (no contradictory
status, no chronology/duplicated results, detail via pointers, scannable) and then said not
to restructure if the current shape is already effective. Assessed it: the structure already
follows that shape, so NO restructure -- only targeted fixes.

ADDED: a "🚫 DO NOT REPEAT" section near the top, 9 numbered rules, each one line with an
arrow to the section holding the full rule. Contents are the mistakes that have each cost
work more than once: pre-writing conclusions; retyping numbers; trusting stale figures;
trimming that cuts signal; recording unverified facts; appending instead of merging; claiming
more than a gate proves; self-starting measurements/builds; reading the parameter set off
setup.h defaults.

FIXED (defects against Royce's own rules):
- Two blocks on one subject (rule 4): the STARK had a Status block AND a separate
  "must not enter the report" block. Merged into the block that owns it.
- Duplicated experiment results (rule 2): statement-compression (14 lines) and amortisation
  (20 lines) blocks compressed to gate + mechanism + framings + pointer.
- Stale facts: `01-introduction.tex:79` -> :75 (my own intro trim moved it); "overridden
  explicitly twice" -> rule form (LaBRADOR and the report work came after, so the count was
  wrong); "both new experiments" -> "every experiment listed in Status" (there are five now);
  two "verified" dates refreshed.

PAID FOR IT (rule 1, compress in the same edit) by cutting narrative Royce's spec says not to
keep: the "On record" anecdote, Meeting-7 items already discharged, the promoted-future-work
"he was told and chose it anyway" story, and the STARK's size/time framings (now only in the
write-up, since it is barred from the report anyway).

NET: 720 -> 732 lines. Over the ~700 budget by design -- Royce's latest instruction is to
prioritise consistency and startup usefulness over line reduction. All 9 arrow targets in the
new section verified to resolve to real sections.