

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
