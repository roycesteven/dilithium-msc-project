

## Checkpoint — 2026-08-17 15:40

Branch: report

Current goal:
- Discharge Meeting-10 items (deck rework + report fixes). Secondary, Royce-directed: settle
  on-chain fit at D2/D5.

Done:
- Meeting-10 transcript consolidated from two ASR sources (Samsung diarised + Teams
  undiarised); Meeting 6 merged into the spec as §15A, Meeting 10 as §19; spec retitled 1-10.
- Deck 13 -> 10 slides, 6:30 planned: application-first opener posing 3 questions, closing
  slide answering them, conclusion made blockchain-specific, swap board relabelled Alice/Bob.
- Deck "The method" slide rebuilt as an SVG diagram (3 reused/added layers + the one
  substitution Sign c=H(pk,w,M) vs PreSign c=H(pk,w+Y,M)); prose 116 -> 71 words.
- Report: fig:swapidea added before fig:swapflow (net-zero words); fig:lasfuncs panel (a) now
  shows c = H(...) = SampleInBall(c-tilde); exotic-implementation claim softened to "remain
  unevenly served by practical implementations"; fig:lasfuncs caption -39 words.
- D5: LASShakeGrowth.t.sol (fixed arena) + derive_onchain_d5_bound.py -> exceeds one tx.
- D2: LASVerifierOptD2.sol (copy, not parameterised) + LASGasBreakdownD2.t.sol + Makefile
  target test/export_verify_vector2 -> golden instance fits at ~65% of cap.

Files touched:
- meeting10_cleaned_transcript.md, las-context-consolidated.md, CLAUDE.md
- report/slides/video_deck.template.html, report/slides/VIDEO_PLAN.md
- report/latex/chapters/{01-introduction,02-methodology,03-results}.tex
- evm/src/LASVerifierOptD2.sol, evm/test/{LASShakeGrowth,LASGasBreakdownD2}.t.sol
- scripts/derive_onchain_d5_bound.py, scripts/update_claude_context.py
- ref/Makefile, ref/test/export_verify_vector.c

Evidence used:
- evidence/onchain_d5bound/latest (deltaAbsorb 3383185 vs threshold 963322)
- evidence/onchain_d2/latest (golden instance total 10956784, ~65% of cap)
- evidence/onchain_onetx/latest + evidence/onchain/latest (D3 anchor + stage attribution)

Open risks:
- Deck 6:30 is PLANNED, never spoken. Four slides have rewritten notes. Unvalidated.
- SVG method diagram verified structurally only (XML valid, macros resolved) - NOT rendered;
  no browser on this machine. Layout unconfirmed.
- 3 slides still have no visual: "Why this matters", "What an adaptor signature does",
  "Answering the questions" (100/108/121 words).
- Word count at 8999/9000 - one word of headroom, any addition needs an offsetting cut.
- derive_onchain_d5_bound.py comment calls the whole packed region "4-byte big-endian";
  aHat/tHat are BE but tPacked is LE. Counts unaffected, comment wrong.
- Instance variation unquantified at every parameter set; D3 headroom ~364k vs ~732k spent in
  the two data-dependent stages. Do not assert negligible or material.

Next action:
- Open report/slides/video_deck.html and check the method-diagram layout renders; then run the
  deck aloud against a clock to validate 6-8 min.

## Checkpoint — 2026-08-17 17:55

Branch: report

Current goal:
- Make the Meeting-10 deck rework real: Wang's critique was "pictures, not text", and the
  13->10 pass had only reordered text.

Done:
- Rendered every slide headless (Windows Chrome via /mnt/c, #N deep links, both themes) and
  confirmed the defects: 200-260 px voids on slides 2/3/6/9/10, slide 9's headline promising
  "three things" while showing two, no "where are we" marker, help overlay in shot.
- Slides 2, 3, 4 rebuilt as drawn SVG scenes on the deck tokens, four timed stages each:
  2 = the swap scenario (Alice/Bob, two chains, one shared secret, asymmetric caption),
  3 = the four functions as one flow (PreVerify named; Ext checked against Y),
  4 = landscape architecture + the one substitution + c = SampleInBall(c-tilde).
- Slide 9: headline now "Two beliefs measurement overturned - and three suggestions I closed
  instead"; ML-DSA mini table (macro-backed) = Wang's slide-table ask; caveat re-scoped to
  on-chain gas only; columns rebalanced after a render showed the table clipped.
- Chrome: section stepper (Why/Method/Results/Takeaway, data-part per slide), help overlay
  default off, .mid utility to centre short cards in stretched columns.
- Claim fixes, repo-wide: uncited "mostly paper-only" deleted from 01-introduction.tex:36 and
  00-abstract.tex:8 (word-neutral) and from CLAUDE.md's "Why"; deck notes' hand-typed
  "98 percent" -> {{cfgThreeProofPct}}, "Dilithium-2 near 65 percent" -> no number (no macro
  exists); "size work must target Y, not the signature" -> "Y is the dominant remaining
  target" (payload still falls to 0.69x).
- VIDEO_PLAN.md: 13-slide table -> the real 10 with parts and 6:35; demos are slides 5 and 8;
  cut order rewritten; rubric row numbers fixed; keys + stepper documented.

Files touched:
- report/slides/video_deck.template.html (+ regenerated video_deck.html, swap_console.html)
- report/slides/VIDEO_PLAN.md
- report/latex/chapters/{00-abstract,01-introduction}.tex
- CLAUDE.md, PROGRESS.md

Evidence used:
- report/latex/generated/*.tex macros only (gen_slides.py --check passes)
- ref/las.c, ref/basesig.c:556, ref/test/test_serde.c:155,182 (PreVerify accepts / Verify
  rejects), rust/fips204-las/src/basesig.rs:756 (SampleInBall twin)

Open risks:
- 6:35 planned is 35 s over the 6:00 floor: under-running is now the risk, and the deck has
  still never been spoken against a clock.
- Slide 10's three cards centre independently, so their headings sit up to ~13 px apart.
- Report word count not regenerated after the two chapter edits (both word-neutral by
  construction, but unverified): run make -C report/latex wordcount.
- Screenshots live in /mnt/c/Users/Royce/AppData/Local/Temp/deckshot, outside the repo.

Next action:
- Run the deck aloud against a clock (N for notes, T to start) and adjust per-slide data-time
  from what it actually takes; then regenerate the word count.
