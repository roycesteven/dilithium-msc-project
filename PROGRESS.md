

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
