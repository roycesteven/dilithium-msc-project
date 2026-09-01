# Project context — LAS on Dilithium for blockchain

**The single end-to-end context file.** Everything here is either a *durable
rule/decision* (hand-written) or *live state* (the auto-generated block — never hand-edit
it). Read this file; add the latest `PROGRESS.md` checkpoint only when continuing prior
repository work. Long detail lives in the documents pointed to from each section.

## 🔁 How this file stays current

- Live facts — branch, wire sizes, KAT digest, evidence run ids, report word count, last
  checkpoint, meeting coverage — are regenerated into the **🔄 Live project state** block
  by `scripts/update_claude_context.py`. It only *reads* files and git metadata: never
  builds, tests, benchmarks, or estimates; anything unparseable prints `(not found)`.
- Hooks in `.claude/settings.json` run it at `SessionStart` (regenerate + inject into
  context) and at `Stop` (after every turn). Manual:
  `python3 scripts/update_claude_context.py` (`--check` exits non-zero when stale,
  `--print` writes to stdout only).
- **Never hand-edit between the AUTO-CONTEXT markers** — those edits are overwritten.
  Hand-edit everything else; the script deliberately does not touch it.
- **Handoff protocol:** durable rule / decision / capability / supervisor ruling → edit
  this file *in the session it happens*. Work-in-progress narrative (what was tried, what
  broke, next exact step) → `PROGRESS.md` via `/checkpoint`. Live numbers → do nothing.

### 🔔 STANDING DUTY — update this file mid-session, unprompted (Royce, 2026-08-03)

Royce must never have to re-explain the same thing in a new session; a session that ends
without recording what it learned has wasted the learning. **Stop and edit this file at
the moment** any of these appears: a supervisor ruling or meeting outcome; a decision by
Royce that constrains future work (including one that overrides what is written here); a
correction to a number, name, claim or status recorded here; a new capability/artefact, or
a component becoming deliberately dead / must-not-repair; a rule with teeth (naming, a
gate, a framing that must not drift, a trap already fallen into once); an owed explanation
or open question. Put it in the section it belongs to, in the smallest edit that carries
the **reason** — a rule without its why gets argued away later — and say in the reply that
CLAUDE.md was updated and why. A meeting ruling is also merged into
`las-context-consolidated.md` (the spec) so the two agree. **Does NOT belong here:** live
numbers, session narrative, or anything the repository already records.

## ⚠️ HOW TO WRITE IN THIS FILE — it is NOT a junk drawer (Royce, 2026-08-04, ALL sessions)

Royce has had to say this twice; sessions kept appending. **The value of this file is that it
can be read in full at the start of every session — an unbounded file destroys the thing it
exists for.** Rules, enforced in every session:

1. **Budget: keep the whole file under ~700 lines.** If your addition pushes past it, compress
   something else *in the same edit*. Growth is not free; you are spending every future
   session's context.
2. **Detail belongs in the write-up; this file gets the rule.** A new experiment/capability is
   worth **≤ ~10 lines here**: what it is, where the runner / evidence / write-up live, and only
   the framings, caveats and gates that must not drift. Methodology, derivations, per-run
   results and the argument go to `docs/…`, and are linked, not copied.
3. **Never paste numbers here** — percentages, timings, byte counts, tables. They belong to the
   evidence run and the report macros (see FOCUS); a number in this file is a number that will
   be wrong later and retyped into the report.
4. **Merge, do not append.** A new fact about an existing component is edited *into the section
   that owns it*. Never bolt a second block onto the same topic; two blocks on one subject is
   the defect, not the fix.
5. **Supersede explicitly.** Before adding, delete or rewrite what your addition makes obsolete;
   leaving both is how contradictions get in.
6. **Narrative is `PROGRESS.md`, not here** — what was tried, what broke, what you decided
   mid-session. This file carries only durable rules and current status.

Recording context and dumping text are not the same act.

## 🚫 DO NOT REPEAT — each of these has cost real work MORE THAN ONCE (Royce, 2026-08-05)

**Read this list before acting.** The full rule lives in the section named after each arrow. A
session that repeats one of these has failed even when its output looks right.

1. **Never pre-write a conclusion.** Write the prose that interprets a result only *after*
   reading the result — including a benchmark's own printed "what this shows".
2. **Never retype a number.** Report figures come only from regenerated macros; quote evidence
   by path. A number typed from prose (this file included) will be wrong later. → FOCUS
3. **Regenerate before trusting any figure** — word count, evidence, wire sizes. → WORD COUNT
4. **Trim filler, never signal.** A dropped qualifier can silently widen a claim into an
   overclaim; pay word debts out of new prose, not existing argument. → WORD COUNT
5. **Verify before recording — including review feedback Royce relays.** Line references, dates
   and "both"/"twice" quantifiers go stale silently. Critiques passed on from another model are
   *claims*, not rulings — **and they go BOTH ways, so verify, never presume either**: one on
   2026-08-10 was materially wrong (the `g` bound called witness-dependent when it is a
   constant — the defect is completeness, not dependence) and complying would have written a
   false statement into the repo; one on 2026-08-15 was **right on all three points** and
   caught a would-be overclaim mid-draft (below). Check each against the code, say which way it
   went, then act. → OUTPUT
   **2026-08-17 (largest batch yet, and the pattern is now clear):** of seven relayed points, six
   held — the strongest ones caught wording that **collided with our own artefacts** ("no escrow"
   when `AdaptorSwap.sol` escrows; "adaptor signatures are elliptic-curve" when LAS is a lattice
   one; a spoken "a bitcoin for an ether" against a picture showing 1 BTC / 10 ETH) — while one was
   **flatly forbidden here**: it proposed `c̃ = H(…)`, the exact form the c/c̃ rule bans. Two useful
   habits fell out: a relayed critique may be reviewing a **stale diff** (one rejected an edit for
   not doing what that very edit did) or **belong to another thread entirely**, so check that the
   feedback is about the change in front of you before acting on it. → OUTPUT
   **2026-08-21, standing instruction from Royce: relayed ChatGPT feedback is VERIFIED against
   the paper/code before it is acted on — every time, in the same turn.** That day's was RIGHT
   and blocking: an edit that split `r'` from `s` left "The witness is ternary" unscoped, which
   after the split reads as covering the extracted `s` too (Remark 1 bounds it only by
   `2(γ−κ)`). Pattern worth naming: **introducing a distinction retroactively widens every
   earlier sentence that used the old, merged word** — re-read the neighbours after any symbol
   split, not just the line being edited.
   **2026-08-22 — both points RIGHT, and one IMPROVED the finding** (Royce reaffirmed the standing
   verify-first instruction): (a) **"only in the narration" is NOT a failure** — the deck's
   `data-notes` IS the spoken script, hidden on screen but *delivered* and marked, and Wang named
   speech as the channel for the base-naming (*"you should **say** something like this"*); the
   split is must-be-VISUAL vs claim-where-narration-suffices, not slide-vs-notes. (b) a stale
   price is wrong only when **presented as current** — the slide stamps its own retrieval instant,
   so the exposure is the narration's "this morning". → spec §19A.4.
   **2026-08-23 — three RIGHT, one WRONG, one BARRED, and the VERIFIER was wrong once.**
   Standing, from Royce: **a relayed ChatGPT GRADE ESTIMATE is an OPTIMISTIC UPPER BOUND, never
   a prediction** — past ones always overestimated; quote it as a ceiling or not at all.
   ⚠️ **Check "which branch did it read" against `origin/<branch>`, NEVER the local branch of
   the same name** — local `main` was many commits behind while `origin/main` already carried the
   merge of `report` (identical blob SHAs), so a false provenance accusation was raised and had to
   be retracted. A local ref is not the remote; nor is a stale build artefact the source —
   `word.count`/`report.log` were 2026-08-19 against 2026-08-21 chapters, making both its
   "19 words of headroom" and its overfull-hbox figure stale (→ WORD COUNT rule 3).
   ⚠️ **A proposal can be BARRED by a ruling, and "tidy the appendix" is void as a word-budget
   move**: moving the two negative experiments into Results contradicts M9 (compression belongs
   in the **critical reflection**, spec §18) and risks M9's ban on LaBRADOR figures; and appendix
   words are **not counted** while Wang repeatedly sent detail *into* the appendix, so trimming it
   saves nothing. Signal-to-noise is a **body** edit.
   ⚠️ **A "worst case" is only a bound if EVERY free variable is pushed the adverse way**
   (2026-08-15, cost a retracted draft). A D5-vs-gas-cap derivation was written as *proved*
   under "execution can only grow"— but calldata **byte content** was a second free variable,
   and pushing it the other way (all added bytes zero) flips D5 from 74,331 over the cap to
   231,333 under it. Naming one assumption does not make the others disappear. → EVIDENCE-OR-SILENCE
   **2026-08-25 — NINE points relayed, ALL NINE RIGHT, and they are ONE defect: the ABSOLUTE
   WORD.** Not eight lessons — one, worth naming because it recurs: a small intensifier
   ("verbatim", "no", "nothing", "not", "stops", "years") silently converts a true scoped claim
   into a false universal, and it survives review because the sentence around it is correct.
   Settled replacements, in report AND deck, **do not drift back**: the swap follows the paper
   **step for step, realised over a model ledger** (never *verbatim* — the ledger, the proof
   instantiation and the sighash mapping are ours); **no *swap* script** (Bitcoin outputs always
   have a scriptPubKey — *scriptless* = no swap-specific script); **no shared hash or
   adaptor-specific on-chain marker** (never "nothing links the legs" — the report's own §1.1
   already says timing and amounts still correlate them); **a coordinated consensus-rule change,
   not merely a local software update** (never "not an upgrade" — a consensus change *ships* as
   one — and never "years", which is uncited); **how systems prepare for that transition** (never
   "the question is not whether they get replaced" — NIST's schedule does not bind a chain);
   **standardised basic layer / uneven exotic layer** (never "standardisation stops" — the cited
   warrant is about *implementations*, and M10 ordered that claim softened); and adaptor cost is
   **less overhead relative to each scheme's own base** (never "the cheaper one to add" — LAS is
   dearer in absolute time and bytes); and the closing verdict may exempt only **the adaptor
   layer** — *"what stops deployment is not the adaptor layer"*, **never "not performance"**,
   which contradicted `sec:reflection-shortfalls`' own *"what remains unsolved is cost"*.
   ⚠️ Two of the nine were defects *this file* had licensed and one was encoded as a
   design comment in the deck; two were live report↔deck contradictions — so grep the DECK too
   whenever a report wording is scoped, and vice versa. ⚠️ **And fix the SLIDE, not only the
   NOTES**: a first pass scoped four wordings in `data-notes` and left the same claims standing
   in the visible headline, card and divider — purge-is-not-done-until-re-grepped applies
   *within* a slide. → EVIDENCE-OR-SILENCE, FOCUS
   ⚠️ **2026-08-26 — SELF-INFLICTED, so the grep is now owed on OWN drafting, not only on relayed
   critique.** Six absolutes in one session's new prose, mine: "the words were **never** the
   defect" (this file's own 3,558-word case refutes it), "**every** trim", "the **single biggest**
   waste", "drift **cannot** flatter either side" (interleaving *reduces* drift), "**every**
   payment with one signature scheme" (Bitcoin has ECDSA **and** Schnorr over one *curve*) and
   "accepts **every** variant" (only those tried). Two were plain factual errors: "PreSign is the
   dearest" (`\ovAdapt` 6.9 > `\ovPreSign` 6.6 — an **absolute-time** ordering carried across to
   an **overhead ratio**) and "none of the **four**" overhead figures (Extract has no basic
   analogue, so there are **three** pairs). ⚠️ A relayed critique can be **right about the hazard
   and wrong about the cause**: "the simplified-base question is a strawman" was wrong —
   `03-results.tex:356` records it as *this project's own assertion, tested and overturned* — yet
   the bare "No" it objected to did need scoping to **functionally** (the ML-DSA variant's
   security is unanalysed). Fix the hazard; do not adopt the diagnosis.
   ⚠️ **2026-08-27 — Royce reaffirmed the standing verify-first instruction ("strict verify
   ChatGPT feedback"), and it again went BOTH ways in one batch.** RIGHT, and blocking: a draft
   M12 block had turned three **hedged** supervisor suggestions ("*maybe* lattice-based is too
   specific", the big-companies line, "be faster **or** remove details") into bans and
   requirements — **the new named failure mode is RECORDING STRENGTH: an imperative is a ruling,
   a "maybe you could" is a recommendation, and promoting one to the other invents a
   constraint.** WRONG, and it would have closed a live requirement: a claim that the report's
   adaptor motivation already satisfies M12 — refuted by git, since those paragraphs predate the
   meeting (`9032d1d` 2026-08-25, PDF built 2026-08-26) so the supervisor read them and still
   asked. **Provenance beats prose: check WHEN the artefact was written against WHEN it was
   reviewed before marking any feedback item discharged.**
   ⚠️ **AND THE HEDGE RULE HAS A LIMIT: REPETITION RAISES PRIORITY, NOT MODALITY OR SCOPE.**
   When the same hedged suggestion recurs — M12's "big companies are migrating" was also asked
   in M11, and came back as "*Have you mentioned this?*" — that is evidence he still does not
   see it as addressed, so treat it as a **high-priority recommendation and go check the
   artefact**. It does **not** become a requirement, and **deck feedback does not become a
   report requirement**: both instances of that one were spoken over the *deck*, so the report
   edit followed from this project's own deck ⊆ report rule, not from him. Read wording and
   artefact context separately, every time. Before filing any "already applied / suggestion
   only" verdict, check the EARLIER transcripts and **grep the artefacts for the claim**
   instead of trusting the previous entry.
   ⚠️ **THE NOTES ARE THE SPOKEN SCRIPT, SO THEY HAVE A WORD BUDGET (2026-08-25).** Fused script
   and backup detail had reached **3,558 words — 471 wpm, unspeakable**. Every `data-notes` is now
   **`SPOKEN: … || BACKUP: …`**; only SPOKEN is delivered, `data-time` is **derived from it at
   150 wpm**, keep it ≈ **1,100–1,200 words total**, and re-derive `data-time` after any notes edit.
   ⚠️ **The VIEWER honours the split (2026-08-26):** `N` shows SPOKEN alone at reading size, `B`
   reveals BACKUP — it dumped both at 15 px until then. **Diagnose the panel before cutting the
   script:** that time the count was inside budget and the viewer was the defect, while the
   3,558-word case above is the opposite — check both, assume neither.
   ⚠️ **SPOKEN interprets; the SLIDE carries the figures**, and a term already on screen is not
   spoken unless the conclusion needs it — reciting visible text is what made the script read as a
   report aloud. **Methodology is the exception (M10):** say how it was measured, in plain words.
   Craft rules in `VIDEO_PLAN.md` §1; the five past decks are in `past_report/`.
   ⚠️ **`data-time` is a PLANNING BUDGET, never duration evidence**: it is a derivation from a
   word count at an *assumed* pace, so pauses, demo beats and transitions can still push the
   recording past 8:00. The gate is a **timed full rehearsal inside 6–8 minutes** — quoting the
   derived total as the video's length is the derived-as-measured error, and the first full
   rehearsal (M12, 2026-08-27) ran **≈10:40** against a derived budget under 8:00, so the gap is
   real and large. ⚠️ **The assumed 150 wpm was the defect: measured delivery is ≈112 wpm**
   (1,190 spoken words in ≈640 s), and per-slide it ranges ≈95–125, slowest on the demo slides
   where beats add time speech does not. **`data-time` is therefore derived at 112 wpm now, not
   150** — re-derive at the *measured* pace after any notes edit, never the assumed one. The
   script was cut **1,190 → 887** on 2026-08-27, had drifted back to **941** by 2026-08-30, and is
   now **868 words = 7:45**; **every `data-time` is derived from its own slide's SPOKEN half at
   112 wpm**, though small differences between the summed per-slide times and the whole-script
   estimate can arise from per-slide rounding. ⚠ **A recorded word count goes stale silently —
   recount it, never quote one from prose (this file included).** Margin is ~15 s, so the timed
   rehearsal still decides. A caveat may move to
   BACKUP **only if it is VISIBLE on the slide** (the 2026-08-22 must-be-visual vs
   narration-suffices split); the base-naming and the UTXO-with-UTXO scope note stay SPOKEN
   because nothing on screen carries them.
6. **Merge into the section that owns the subject** — never a second block on one topic.
   → HOW TO WRITE IN THIS FILE
7. **Never claim more than a gate proves** — no gadget described as a complete proof, no
   non-zero-knowledge argument offered as π. → Measurement gates, Status
8. **Never self-start a measurement, build or benchmark.** → Guardrails, WORK-PRIORITY RULE
9. **Read the parameter set from the Makefile**, never from `setup.h` defaults. → naming section
10. **Never state a claim that has not been run, calculated or cited.** → EVIDENCE-OR-SILENCE

## ⚠️ EVIDENCE-OR-SILENCE — no claim without a test, a calculation, or a citation (Royce, 2026-08-05)

**If it has not been measured, derived from measured quantities, or cited, it does not get
stated — in the report, in `docs/`, in code comments, in commit messages, or in this file.**
Caught live: a claim that on-chain verification "breaks at D5" was written from plausibility
alone — D5 was never built or measured, and `LASVerifyOpt`'s parameters are compile-time
D3-only, so the library cannot even run it. A sentence that *sounds* like a result and is not
one is worse than no sentence, because the reader cannot tell the two apart, and an examiner
who finds one stops trusting the rest.

Every claim carries exactly one of three warrants, and the prose must make clear which:

1. **MEASURED** — a real run. Name the evidence path; never retype the number (→ FOCUS).
   ⚠️ **Evidence paths, `app:repro`'s commands and `app:diff`'s file list STAY PRINTED** (Royce,
   2026-08-28, rejecting a relayed "remove all repository paths from the PDF"): the path is part
   of the warrant, and "the captured evidence run" must never replace a concrete checkable one.
   Those listings implement M5's reproducibility package and M3's two-branch diff, but **printing
   paths is THIS PROJECT'S reporting decision, never a Wang ruling** — he never ruled on it and
   M10 §19 cuts the other way (*"the marker does not have access to the repository"*), so do not
   cite him for it. Volatile **run metadata** is the opposite call: the Stage-1/Stage-2 run ids
   and the KAT digest stay in LaTeX `%` comments, unrendered.
2. **DERIVED** — arithmetic on measured quantities. Say *derived*, show the inputs, and never
   let it read as measured. A derivation is not a measurement and must not be reported as one.
3. **CITED** — someone else's result, with the reference that supports *that specific* claim.

Anything else is an **open question** and is written as one — "not evaluated", "not measured
at …" — never as a finding. Further, standing:

- **Scope is part of the claim.** Parameter set, message/input length, configuration, and the
  machine or EVM revision travel WITH it. "It works" with none of them attached is an overclaim.
- **Negative claims need the same warrant** — "X does not fit", "does not scale", "breaks",
  "is infeasible" are claims. This project has already had to retract one ("exceeds the block
  gas limit") for exactly this reason; that retraction is the precedent, not the exception.
  A second, 2026-08-12: **"zero compiler warnings"** stood in the report unevidenced —
  `ref/Makefile` sets `-Wall -Wextra …` but **no `-Werror`**, and `run_benchmark_suite.sh`
  captures only each binary's stdout, never `make`'s. A clean exit is not a clean build.
- **Never claim more than the gate proves** — no gadget described as a complete proof, no
  non-zero-knowledge argument offered as π, no model reported as a client result.
- **Self-contained references**: never a bare "Fig. 1" — say which paper's figure. No
  misattribution to cited work; label ceilings as upper bounds; check that each citation
  actually supports the claim it is attached to.

When an unevidenced claim is found, the fix is to **delete or downgrade it in the same edit**,
not to plan an experiment that would justify it later.

## ⚠️ FOCUS — the primary comparison (READ FIRST; gotten wrong repeatedly)

**Stage-1 focus = BASIC SIGNATURE vs LAS ADAPTOR — the cost of adding the adaptor layer to
the base signature.** Results/evaluation must lead with this
(`las-context-consolidated.md` §13.5, §7(e), §14.3):

- **Base path:** Sign / Verify with `c = H(pk, w, M)` (no statement `Y`).
- **LAS path:** PreSign / PreVerify / Adapt / Extract with `c = H(pk, w+Y, M)`.
- Headline = **adaptor overhead, per operation, at Simplified Dilithium-III**. Both paths
  share algorithm/parameters/primitives, so the difference is purely the adaptor cost.
  Extract has no basic analogue. The settled *shape* is **single-digit per-operation
  overhead**; individual figures move ~1–2 points between runs on the same machine.
  **The only authoritative figures are the regenerated report macros `\ovPreSign` /
  `\ovPreVerify` / `\ovAdapt` from the evidence run named in the live block — never retype
  a percentage from any prose (this file included) into the report.**
  ⚠️ **NAME THE BASE, EVERY TIME — "nearly free" alone is an unbaselined claim (Wang,
  2026-08-21).** He read the deck's "the adaptor layer is nearly free" as *against Bitcoin's
  ECDSA*, where it is emphatically false, and the fix he asked for is now the required framing
  in deck and report alike: **two steps.** Step 1 = classical → post-quantum **basic** (the
  expensive one — paid in bytes, and the step organisations are already migrating for); step 2
  = basic → **adaptor**, which is what `\ovPreSign` & co. measure, against the *same* base at
  identical parameters. The comparator that makes step 2 meaningful is **the same step charged
  the same way on the classical side**: `\clOvPreSignX` / `\clOvPreVerifyX` (new 2026-08-21,
  `gen_report_data.py`) = the ECDSA adaptor's PreSign ÷ **its own Sign** and PreVerify ÷
  **its own Verify** — ⚠️ **each over ITS OWN base, never both over Sign**: this line said
  "both ÷ Sign" until 2026-08-25 and was wrong; `gen_report_data.py:531-532` is the authority.
  ⚠️ Those two are **DERIVED from the classical harness's single mixed native-API tier**, not
  the paired interleaved measurement — never call them a paired overhead, and quote the
  **packed** LAS figures beside them so the comparison survives the conservative pairing —
  **both** of them (`\packedOvPreSign`, `\packedOvPreVerify`): two classical operations need
  two LAS ones. Report home = `tab:classical`'s caption + `sec:res-classical` (2026-08-25).
  ⚠️ Never let it imply LAS is *faster* than ECDSA in absolute time (it is not — that is step 1),
  and never state step 1's `\clRatioSig` flatly as "the cost of post-quantum": it is **this
  build's simplified base**, and the FIPS 204 ML-DSA route measures a smaller signature.
- **NOT the focus:** the four parameter sets (paper / D2 / D3 / D5) are only a *secondary
  fairness / parameter-sensitivity* axis (§13.4). Never frame results around "across
  security parameter" or "as the scheme scales"; the across-parameter overhead chart is
  supporting material, not a primary body figure.
  ⚠️ **The sweep CANNOT isolate lattice-dimension scaling, and here is the one-line proof**
  (§2.6 said "whether the adaptor overhead scales with the lattice dimensions" until
  2026-09-01): the paper set and D2 **share `(n,ℓ) = (4,4)`** and differ in `κ` (60 vs 39),
  `γ` and digest width — four sets, three moving quantities, so no dimension effect is
  separable. Settled wording: **"sensitivity to the parameter set"**.
- **Timing rule:** per-operation timing is the PRIMARY timing result (§14.3) — never lead
  with cumulative / end-to-end time.
  ⚠️ **FOUR SCOPES THAT LOOK LIKE UNIVERSALS AND ARE NOT** (each was a live absolute until
  2026-09-01; relayed critique RIGHT every time). (a) **Only the TARGET-SETTING comparison
  reports both boundaries** — the parameter sweep is **core-tier only** (`fig:overhead`'s own
  caption, and `tab:timing` is "every CORE-TIER per-operation timing"), so never write "every
  base-versus-LAS comparison shows both". (b) **Criterion reports a bootstrap 95% CI of the
  mean, NOT a sample SD** (`tab:rust`'s caption is the authority) — "every timing is a mean ± SD"
  contradicted our own table. Every *other* harness does report mean ± sample SD: C driver,
  `fig:timing` error bars, classical baseline, Stage-2 swap, patched-node bench.
  (c) **NEVER "the same code"** — `las.c` holds **verbatim local copies** of `basesig.c`'s
  helpers rather than sharing them (deliberately: it preserves independent linkability at
  bit-for-bit identical behaviour), so the paths run *identical*, not *shared*, code.
  Settled form is `sec:eval-strategy`'s own: **"identical algorithm,
  parameters and primitives"** — and scope it to the **base-versus-LAS** comparison, since
  the optimised-Dilithium and classical baselines deliberately fix none of the three.
  (d) **The packed-tier gap is NOT "each adaptor operation additionally decodes `Y`"** —
  `tab:overhead-l3`'s caption *and* §3.2's body both said so. Only **PreSign and PreVerify**
  add `Y`'s decode alone; **`las_adapt_packed` also decodes the witness and re-packs the
  signature**, and `bench_levels.c`'s own printed NOTE states that split — read the
  benchmark's note before attributing any gap (→ DO NOT REPEAT #1). Two claims died with
  it: the premium is **several times** the core percentages, never "an order of magnitude"
  (**a derived multiple is a claim — compute it, and never retype it into prose**), and
  codec is **not** "the dominant computation cost": for packed **PreSign** the arithmetic
  still is. ⚠️ **Before retargeting a `\cref`, check it is not that float's ONLY body
  reference** — `fig:overhead` had exactly one, the very sentence the critique wanted
  repointed, so the proposed fix would have orphaned it; the sweep claim now carries it.
- **Presentation rule:** no table↔chart redundancy — the *chart* carries the body, the
  exact-number *table* goes to the appendix; **figures are embedded between paragraphs in
  the body**, never collected at a chapter end or in the appendix (Meeting 8, overrides the
  older figures-to-appendix habit); group the four LAS function figures side by side; avoid
  single-figure pages; no abbreviations of scheme/level names in tables or figures.
- **⚠️ FIGURE TYPE FLOOR = THE BODY'S 12pt, IN THE BODY'S FACE (Royce, 2026-08-28).** Nothing
  inside a figure may be smaller than the paragraph around it — and *nominally* equal is not
  enough: 12pt DejaVu Sans reads as bigger than 12pt Latin Modern, which is what "too large"
  meant when the first pass was rejected. Plotted figures are drawn in **Latin Modern Roman at
  12pt on a canvas exactly `\textwidth` wide and saved UNCROPPED** (`--print-figures`;
  `bbox_inches="tight"` re-crops, so LaTeX rescales the type and the guarantee is void) and
  **included at `width=\linewidth`, never a fraction of it**; ℓ/κ/γ go through mathtext (the
  text face carries no Greek). Gate: `scripts/check_figure_type.py` reads the sizes back out
  of the PDFs — **sizes only, so it passes a figure that is the wrong face or bold.**
  ⚠️ **The family name does NOT pin the face, and BOLD is a size defect too** (2026-08-28,
  `fig_onchain` rejected as "bigger than the caption and paragraph" while measuring 12pt):
  *Latin Modern Roman* spans the optical sizes, and matplotlib took **lmroman10 + lmroman9-bold**
  there (**lmroman17** in `plot_las_paper_figures.py`) against the body's **lmroman12** — x-heights
  are equal but advances and stems are not, and bold LM is ~15% wider per character. So name the
  optical size (`"LM Roman 12"` ahead of the family) and set **no figure text bold**.
  TikZ figures use `\normalsize` throughout — **widen a box or break a line, never
  shrink the font**. ⚠️ **A MARK IS ARTWORK, NOT A LABEL, and the floor is a FLOOR** (Royce,
  2026-09-01: *"harus pakai logo BTC dan ETH sungguhan"*): `fig:swapidea` carries the real
  currency marks — Bitcoin as the asset `figures/bitcoin-symbol.{svg,pdf}` (self-contained:
  its own disc, mark and colour), Ethereum as inline TikZ facets. Neither is rescaled to
  satisfy a type sweep. ⚠️ **Do not go back to a font's capital `B`**: that version needed
  hand-placed strokes, and drawn full height they filled the letter's counters white —
  invisible in the source and at page scale, obvious only at 600 dpi, the render-never-reason
  rule catching what no gate can. `#F7931A`/`#627EEA` are the marks' own brand colours, and
  they arrive **differently**: Bitcoin's is inside the asset, Ethereum's is `\definecolor`d
  locally in the figure (a `btcmark` definition was left dead by the asset switch and is
  gone). Neither belongs to the report's semantic palette, which is never recoloured to a
  brand.
  ⚠️ `fig_criterion_presign` is **exempt and must not be touched** (Royce);
  its text is outlined, so the gate cannot judge it. Cost, paid knowingly: +4 pages, and
  §res-evm's two figures now take a text-free page each.
- **⚠️ DECK ⊆ REPORT — the difference is PRESENTATION, not content (Royce, 2026-08-25).** No
  slide may assert what the report does not; a claim added to the deck is added to the report
  in the same edit, or it does not go on the slide.
  ⚠ **IT IS NOW A GATE, because Royce asked it be kept "ketat dan akurat" (2026-08-30):
  `python3 scripts/check_deck_subset.py`** (exit 1 on a gap). Run it after any deck or report
  edit. **Its header is the real documentation** — five false-pass paths are documented and
  closed, and negative controls confirm the gate fails when its evidence is broken: value
  matching is coincidence (`clSigBytes`=64 also occurs in `tab_components`, which has no
  ECDSA column); a macro is geometry only if **every** occurrence is a `--w:` bar width; an
  exemption that does not re-verify is a hardcoded pass; an empty value must not match; and
  only **compiled, uncommented** TeX counts, so a stale draft or a `%`-commented mention can
  never satisfy it.
  ⚠ **A macro-name diff is a PROXY, not the invariant.** The first run flagged 10; 5 were bar
  geometry, 3 were facts the report prints in `tab:classical` without ever citing the macro,
  and only 2 were real. Never delete a slide claim on the strength of a name search alone.
  ⚠ **Scope: macros only.** Literal figures typed into slide text (2035, 520), qualitative
  claims and citations are outside it and still need reading. (Rubric 3.2.2's "additional aspects" is
  satisfied by the **demos** — work the project did that text cannot show — never by extra
  claims.) Two report figures exist because of this rule: **`fig:whynow`** (§1.1 — the quantum
  estimates + the basic/exotic layers; the rubric 3.1.2 figure the Introduction had none of,
  and the home of the four deck citations) and **`fig:evmtx`** (§res-txstruct — the EVM claim
  transaction field by field, the Ethereum half of Wang's M11 both-venues ask; `fig:txstruct`
  is Bitcoin-only). ⚠️ **`fig:txstruct` lives in RESULTS (§res-txstruct), not §2.7** — moved
  2026-08-28 because it prints measured sizes, which belong beside `tab:btctx`; §2.7 keeps the
  *method* (which fields change, what is on- vs off-chain, how measured objects map to the
  BIP-141/144/341 fields, and that the mapping is accepted only once it reproduces a published
  reference spend). Per-field bytes were dropped from the figure when it moved — the table
  beside it gives every field — leaving only the two rows that DIFFER, both now shaded.
  ⚠️ Captions are excluded from the count by the `-sum` weights; the TikZ
  *bodies* measured zero for these two, but confirm any new one against a regenerated count
  rather than assuming. Placement is global: **re-check the whole PDF** — neither figure has
  been rendered yet.
- **⚠️ NO PAGE MAY CONTAIN ONLY FLOATS (Royce, 2026-08-12).** LaTeX gives queued floats a page
  of their own as soon as they fill `\floatpagefraction`; at the old 0.75 that stranded five
  of Ch. 3's floats on text-free pages. `report.tex` now sets it **0.95** under `topfraction`
  0.92. Two knock-on rules: an over-long caption is what makes a float page-sized (Tab. 3.7's
  was cut to fit — caveats kept, mechanism moved to `app:methoddetail`), and a float placed
  beside its own discussion rather than at the section head does not queue. **Re-check the
  whole PDF after any float edit** — placement is global, so a fix here creates one there.
  ⚠️ **`tab:configs` was the same defect, found 2026-09-01** (p.30 of that build held the table
  and nothing else). Cut the same way: the **duplicated** controlled-comparison mechanics went
  (one matrix / one seed / identical relation, and the LAS knowledge-gap mechanism — all three
  are in the two paragraphs above it), every **caveat** stayed. ⚠️ A caption cut is a
  DUPLICATION cut; deleting a caveat to shorten one is Royce's call. ⚠️ The check needs no
  visual inspection but it does need a CURRENT pdf — rebuild, then `pdftotext -f <p> -l <p>
  report.pdf -` shows whether body text survives on that page. A stale pdf answers only for the
  build it came from, which is all this instance proved: the defect predates the day's edits.
  ⚠️ **Do NOT try to automate it by font size — captions here are set at BODY size (11.8pt),
  not `\small`** (three detectors in a row gave wrong answers on 2026-09-01: size cannot split
  caption from prose, and plotted figures carry 12pt interior text by the type-floor rule, so
  axis labels read as body too). The reliable test is the recipe above: **head and tail of the
  page** — a float-only page opens on figure/table content and ends inside its own caption.
- **Criterion figure is NOT "reproduced unmodified"** — Criterion's 12-unit type renders at
  ~4 pt at `\linewidth`, and its key column spends a fifth of the width on five strings.
  `scripts/gen_criterion_figure.py` enlarges the type, **moves the legend from the right column
  into one row below the plot** (Royce, 2026-08-12), re-flows the margins, crops to the result,
  and folds gnuplot's `10^3` tspans into the glyph `10³`. **The plot interior is the identity
  map** — gated by a coordinate-for-coordinate check (1358 interior coords unchanged); layout is
  derived from the file, so it asserts rather than silently mis-draw. Runs off a captured
  `evidence/criterion/*/presign_pdf.svg`, so the figure rebuilds without re-running the bench.
  ⚠️ The caption must list every change **that actually fired on the shipped SVG**, and the
  count is part of that claim. ⚠️ **The superscript fold is INPUT-DEPENDENT** — it matched the
  2026-07-30 plot's `Iterations (x 10³)`, but the 2026-08-28 run is slow enough that gnuplot
  labels the axes plain `Iterations` / `Average time (ms)` with no exponent at all, so the
  caption went from four changes to **three** (2026-09-01). Re-check the regenerated SVG rather
  than trusting the previous caption; "nothing else was touched" stays an overclaim while the
  fold is in the file.

### ⚠️ WORD COUNT — regenerate with `make -C report/latex wordcount`, never trust a stale file

A stale `word.count` once drove several sessions of trimming against the wrong number.
Always regenerate before reasoning about budget. Mechanics that matter:
- Body only: `%TC:ignore` fences in `report.tex` exclude the frontmatter TODO, the
  **appendix** and the bibliography; `-sum=1,1,0,0,0` excludes **captions**. Appendix and
  captions are FREE, and the rubric agrees.
- **Tabular bodies and prose DO count; TikZ picture content and `generated/*.tex` tables do
  NOT.** Moving a reference table into the appendix is the cheapest real saving — already
  done for `tab:tests`, `tab:notation`, `tab:tiers` (now `app:tests`).
- **A whole `figure` environment (TikZ body + `\caption`) costs ZERO** — so figures are the
  one way to add value for free. What is *not* free: **each label inside `\cref{...}` counts
  1 word**, and the wrapping parentheses another, so a new body figure really costs ~2 words
  for its mandatory body cross-reference. Headroom against the ceiling is currently nil (the
  live block prints the count), so every added word needs an offsetting cut of genuine
  filler **in the same edit**.
- **Hard ceiling 9,000** (Royce; rubric band 7,000–9,000). Current value: live block.
  ⚠️ `PROGRESS.md` still carries a "trim to 8,000" next-action written against the old
  stale count — that is not a separate agreed target; confirm with Royce before spending
  effort below the 9,000 ceiling.
- **⚠️ TRIMMING MUST NOT COST QUALITY (Royce, 2026-08-05).** Cut filler, never signal. Two
  traps, both sprung once: (1) **a dropped qualifier can silently BROADEN a claim** — deleting
  "methodological" from "if one *methodological* result deserves to outlive this dissertation"
  cost one word and turned a scoped claim into an overclaim; (2) dropping a subordinate clause
  can delete the *reason* for the claim it supports. Before cutting, ask whether the words were
  narrowing or justifying something. When the budget is tight, pay out of **newly added** prose
  or genuine duplication (body text restating a caption — captions are free), not existing
  argument. Ch1/2/5 have already been squeezed; results and evaluation are the tight ones.

<!-- BEGIN AUTO-CONTEXT — regenerated by scripts/update_claude_context.py. DO NOT EDIT BY HAND: edits here are overwritten on the next session start. -->

## 🔄 Live project state (auto-generated)

*Regenerated 2026-09-01 22:09 by `scripts/update_claude_context.py`, which only reads files and git metadata — it never builds, tests, or benchmarks, and never estimates a number. Anything it could not parse says (not found).*

### Repository right now

- Branch **`main`** · HEAD cec9519 · 2026-08-31 · report deck 31_08 6:51 pm
- Working tree: 33 modified tracked file(s), 53 untracked path(s) · vs `origin/main`: 0 ahead, 0 behind
- Recent commits:
  - `cec9519 2026-08-31 report deck 31_08 6:51 pm`
  - `e8d2481 2026-08-28 evidence 28_08 18:37`
  - `c80dacb 2026-08-28 28_08 18:36`
  - `c41bbb5 2026-08-27 deck report 27_08 19:05`
  - `9032d1d 2026-08-25 introductory material`

### Target parameter set — anchors parsed from source

- Simplified Dilithium-III / ML-DSA-65-aligned (n=6, ℓ=5, κ=49): `c_tilde` 48 B · public key 4416 B · secret key 704 B · signature = pre-signature 6736 B (`ref/serialize.h` static assert)
- Other sets (signature bytes): D2-aligned 4640 B · D5-aligned 9184 B · paper reproduction 4640 B
- Pinned KAT digest `b4a10ffb…03be` — C and Rust agree ✅
  - full: `b4a10ffb6e645e5076d1ff5993faa72909232fc71e554b93544141d6590503be`

### Latest measured evidence (pointers only — never retype a number)

- Stage-1 benchmark suite: `evidence/latest` → `runs/20260828_144608` (dir mtime 2026-08-28)
- Stage-2 UTXO swap: `evidence/stage2/latest` → `latest` (dir mtime 2026-08-25)
- On-chain gas (EVM): `evidence/onchain/latest` → `latest` (dir mtime 2026-08-25)
- Criterion micro-bench: `evidence/criterion/latest` → `20260901_215744` (dir mtime 2026-09-01)
- las-stark: `evidence/stark/latest` → `latest` (dir mtime 2026-08-25)
- Report word count: **8989** (`report/latex/word.count`, rubric bound 7,000–9,000; `make -C report/latex wordcount`)

### Where the last session stopped

- Last checkpoint in `PROGRESS.md`: Checkpoint — 2026-08-28 15:35
- Next action recorded there:
  - Ask Royce which way to settle sec:res-rust (re-run vs rewrite), then re-run
  - `python3 scripts/gen_slides.py --check` if any Stage-1 macro changes again.
- `CONTEXT.md` (long-form handoff): CONTEXT — session handoff (updated 2026-07-29; ninth-session update first)

### Supervisor meetings on record

- Cleaned transcripts present: 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 (`meetingN_cleaned_transcript.md`)
- Merged into `las-context-consolidated.md` (the objectives spec): meetings 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12

### Freshness tripwires

- Stage-1 evidence (2026-08-28) is newer than the newest scheme source (2026-08-25) — measurements match the code.
- `CLAUDE.md` hand-written sections last touched 2026-09-01.

<!-- END AUTO-CONTEXT -->

## The project in brief

**Goal:** implement LAS (Lattice-based Adaptor Signatures, eprint 2020/845) by reusing the
CRYSTALS-Dilithium reference primitives, then demonstrate it in a post-quantum blockchain
**atomic-swap** scenario, benchmarked and documented.

**Why:** blockchains sign with ECDSA/Schnorr, which Shor breaks. NIST standardised *basic* PQ
signatures (Dilithium, Falcon, SPHINCS+), but *exotic* ones (multisig, ring, group, **adaptor**)
**remain unevenly implemented** in the PQ setting — the cited form (`buser2023survey`), and the only
one licensed: "mostly paper-only / none on a blockchain" was uncited **and is now deleted repo-wide**
(Royce, 2026-08-17 — the third repetition of stating a claim without a warrant). Multi-signatures
*are* being implemented (M10). Adaptor signatures enable atomic swaps / payment channels; closing
that implementation gap is the thesis.

**Key design fact:** an exotic scheme = a basic scheme + extra functions. LAS = a Dilithium-style
Fiat–Shamir-with-aborts signature + PreSign / PreVerify / Adapt / Ext. We **reuse Dilithium's
poly/NTT/SHAKE/sampling internals** and do not reinvent lattice arithmetic; LAS itself is a small
self-contained scheme (own dimensions/parameters) layered on those primitives.

### The LAS mechanism (variant B — the paper, Algorithm 2)

An older "variant A" (`z̃ = z + y`, statement subtracted at verify) is **superseded — never
reintroduce it**; the paper specifies variant B, which is what is implemented.
- Statement/witness `(Y, y)` is **literally another key pair**: `y ← S_1^{n+ℓ}` (ternary),
  `Y = A·y`; knowing `Y` does not reveal `y` (Module-SIS/LWE hard).
- **Core mechanism: the statement is folded into the Fiat–Shamir hash.** Sign uses
  `c = H(pk, w, M)`; **PreSign uses `c = H(pk, w + Y, M)`**.
- `PreSign(sk,Y,M)`: `ẑ = y + c·r`, reject if `‖ẑ‖∞ > γ−κ−1`. Pre-signature `σ̂ = (c, ẑ)`.
- `PreVerify(Y,pk,σ̂,M)`: recompute `w' = Aẑ − c·t`, check `c == H(pk, w'+Y, M)`.
- `Adapt((Y,y),σ̂)`: `σ = (c, ẑ + y)`. Ordinary `Verify` then sees `Az − ct = w + Y`, which
  matches `c` — the adapted signature is a **fully ordinary** signature.
- `Ext(Y,σ,σ̂)`: `y = z − ẑ`, returned iff `A·y == Y`.
- **On-chain leak (why swaps are atomic):** publishing the adapted `σ` lets anyone holding
  `σ̂` recover `y = z − ẑ` and complete the matching half of the swap.
- **Wire form:** what travels is the challenge *digest* `c_tilde`; `c = SampleInBall(c_tilde)`
  is re-derived locally (FIPS 204 construction). Digest width follows FIPS 204's `λ/4` rule
  per aligned set — see the naming section.

### ⚠️ THE failure mode to watch — the bound budget, not packing

PreSign rejects at the **tighter** `γ−κ−1`; the ternary witness has `‖y‖∞ ≤ 1`, so the
adapted `z = ẑ + y` satisfies `‖z‖∞ ≤ γ−κ` and clears ordinary Verify. **Loosen PreSign to
`γ−κ` and adapted signatures can exceed the bound, so Verify rejects everything** —
silently, probabilistically, two operations from the cause. `γ = κ·d·(n+ℓ)` governs the
MSIS hardness parameter; **Sign's** acceptance is ≈37% per attempt (`≈ e^{−1}`) for the
simplified scheme without hint vector, measured directly via the attempt counters —
`base_attempts` (Sign) / `las_attempts` (PreSign) — **never inferred from timing ratios**.
⚠️ **THAT CLOSED FORM IS SIGN'S, NOT THE SIGN CLASS'S** (unattributed in §2.6 *and in this
line* until 2026-09-01; relayed critique RIGHT both times): per-coefficient acceptance
`(2(γ−κ)+1)/(2γ+1)` uses **Sign's** bound, so `e^{−1}` and the **2.71875** gate constant are
Sign's; **PreSign is the same derivation at `γ−κ−1`** → `(2(γ−κ)−1)/(2γ+1)`, ≈36% and
**2.77483**. Scoping this line exposed a paired defect it had hidden — it named
`las_attempts`, **PreSign's** counter, beside a **Sign** figure. Attribute the formula *and*
the counter wherever either is written; `sec:res-rejection` quotes the two predictions apart.
✅ **`fig:rejcdf`'s SOLID curves are the measured empirical CDF** — the dashed pair stays the
closed-form model, drawn as a reference — **as of the 2026-08-28 Stage-1 run, the first to
carry `tables/rejection_histogram.csv`**. ⚠️ Check that file exists for the headline level
before repeating the claim of any later run: `plot_las_paper_figures.py` **falls back to the
model with no error when it does not**, and the only visible tell is the legend (measured =
`basic Sign` / `LAS PreSign` / one `geometric model` entry; model-only = `…: model` twice).
Per-call counts come from `bench_levels.c`'s print-only `histogram k=…` block; the summary
statistics alone cannot reconstruct a distribution, so never rebuild one from them.

**Rejection gate (never weaken or rename):** every C and Rust benchmark driver hard-asserts
measured attempts/call against exact theory — **Sign 2.71875**, **PreSign 2.77483** at the
target set, 5σ tolerance. A drifting run fails loudly instead of quietly producing a
publishable-looking number.

**Known caveat (note in the thesis; do NOT solve):** the "knowledge gap" — here the
extracted `y` is exact, whereas in the paper's relaxed setting the witness can carry noise
that grows across long payment-channel chains.

## ⚠️ Measurement gates — established the hard way, do NOT weaken

Four faults each shipped a plausible-looking wrong number before a gate caught it: a missing FIPS
204 empty-context prefix in `mu`; re-signing one fixed instance (deterministic rejection loop); a
KeyGen benchmark replacing the keypair so `Verify` timed the rejection path; and Sign/PreSign
timed in separate blocks, letting clock drift invert the overhead's sign. Therefore, in every
benchmark: (1) overhead ratios are **PAIRED and INTERLEAVED within each repetition**; (2)
sign-class pairs are compared **per attempt**, never per call; (3) every timed block is followed
by a **success-path assertion**; (4) attempt counters are checked to actually track their loop;
(5) an untimed **warm-up** precedes the first measurement. `bench_mldsa_compare` exits non-zero if
any of these fails; the rejection gate above is the same discipline for the acceptance rate.

## Status — what exists today

Live sizes, digests, evidence ids and word counts are in the auto block; this section says
**what is built and what it means**, never the numbers.

*Audited against source 2026-08-10 (ten claims were wrong; superseded wording is in git history,
not here). Statements below are the corrected state — trust them over any older copy of this
file, and re-verify against the code before extending one.*

**Stage 1 — scheme + benchmark (complete).** LAS variant B in C (`ref/las.c`, `basesig.c`,
`setup.c`, `relation.c`) and Rust (`rust/fips204-las/src/`); **Rust is the naming authority**,
KAT-locked to C byte-for-byte.
- Correctness: `test_las` (1000 iters), `test_basesig`, `test_serde` (round-trip; **low-bit flip
  of every packed-signature byte** rejected by `base_verify_packed` — *not* every possible
  single-byte mutation, do not widen it; plus a packed pre-signature must fail), `test_kat`
  (pinned cross-language digest). All current seven-type code.
- ⚠️ **`test_contract` is NOT a live test, and `make all` is broken on it.** `test_contract.c`
  still uses `las_pp`/`las_pk`/`las_sk`/`las_sig`/`las_signature_det`, none of which exist in the
  headers its target compiles against — yet `ref/Makefile`'s `all` includes `test/test_contract3`,
  so keeping it in `all` breaks a clean build. (Source-level, established by reading; no build
  run.) **Dropped from `all` 2026-08-10**; the rule is kept so the breakage stays visible. The
  itemised contract that *does* run is `test_mldsa_las{2,3,5}`. **Do not repair
  `test_contract.c`** — dead by design. ⚠️ `tab:contract`'s caption used to credit it; fixed
  2026-08-12 to the tests the evidence run actually contains (`test_las` 1–4/5a, `test_serde`
  5b/6, `test_kat` 7) — `evidence/latest/logs/` has no `contract.log`, which is the proof.
  ⚠️ **SECOND INSTANCE 2026-08-23, so the rule now has teeth: before describing any artefact,
  check the target is actually BUILT.** `app:swapdemo` credited a happy-path/refund demo to
  `ref/chain.c` + `test_pcn.c` (both dead); rewritten to the live Rust `run_refund` — a
  **separate one-chain** test: `run_swap` funds with `pay_to`, `run_refund` with `swap`, and
  the two output forms must never be conflated.
- Serialization `ref/serialize.{c,h}` — **codec ONLY**, six typed pack/unpack pairs. Validation
  is asymmetric and deliberately so: *packing* rejects out-of-range input (e.g. a non-ternary
  secret key), while **`unpack_signature` is PERMISSIVE** (`c_tilde` raw, `z` via FIPS
  BitUnpack) — **the norm rejection belongs to Verify**, so never call the decoder "validating"
  flatly. **`base_verify_packed` is in `ref/basesig.{c,h}`, NOT serialize** (it lives there so
  the codec stays pure); it is the byte interface an on-chain verifier consumes, and it uses the
  codec internally. Wire form `c_tilde ‖ BitPack(z)`;
  **`z` dominates the signature** (share = macro, → rule 3).
- Deterministic API + pinned KATs: `base_keygen_seed` / `base_sign_det` / `las_presign_det`, mask
  seed `SHAKE256(tag‖sk‖[Y]‖M)`; reproducible across machines. ⚠️ **The digest absorbs FIVE
  objects over four fixed vectors — packed pk, sk, σ, σ̂, adapted σ — and nothing else** (C and
  Rust identical). PreVerify and Ext are *asserted*, never hashed, so never write that it
  "covers the four adaptor operations", nor that "any divergence would flip it": a hash
  equality witnesses OUTPUT agreement only. Report wording fixed 2026-08-12.
- Benchmarks: `bench_levels` (primary fair base-vs-LAS, ≥5 runs, mean±SD), `bench_las`,
  `bench_compare` (context only — optimised Dilithium is *not* algorithm-matched),
  `bench_classical` (ECDSA adaptor via vendored `secp256k1-zkp`), Rust Criterion. Two baselines,
  per Meeting 2. Headline: the price of post-quantum here is **communication, not computation**;
  LAS's adaptor overhead is small, where the classical adaptor pays ~4× for its DLEQ proof.
- ⚠️ **Absolute C-vs-Rust timing agreement is NOT a safe report claim** — it has already swung
  far outside "close" between evidence runs, and **no cause is established: never explain a
  divergence without evidence** (compiler, machine load and build profile are all guesses until
  measured). What survives independently of it, and is what the claims must rest on: the KAT
  byte-agreement and the *relative* adaptor overheads. So re-read `\rustCMaxDev` against
  §`sec:res-rust`'s own wording after every Stage-1 run. Live status → `PROGRESS.md`.

**Stage 2 — the application (complete, Bitcoin/UTXO).** `rust/las-swap/` = eprint 2020/845 §4.1
Fig. 1 atomic swap over a UTXO ledger model (ledger takes the signature algorithm as a parameter,
as §4 assumes), benchmarking **three configurations**: (1) classical ECDSA adaptor, (2) LAS +
Groth16, (3) LAS + LaZer, from one pinned master seed.
- ⚠️ **What is measured is the HONEST path only** — time + communication, including off-chain
  messages. `bench_swap` runs the timeout/**refund** edge case once per configuration
  **outside the timed set**, and its transcript does **not** enter `CommSummary` — so never
  present refund as a measured phase.
- **Attribution rule:** **2→3 is the controlled comparison** (same signature, same relation, only
  the prover differs — lead with it); 1→2/3 is a whole-stack comparison, *not* the cost of the PQ
  signature alone.
- ⚠️ **`Tx::sighash()` HASHES NOTHING — the name is not a warrant** (`utxo.rs:144`; the report
  called its output "the signature hash" until 2026-09-01, relayed critique RIGHT). It returns
  the template's **canonical bytes**, and `serialize()` is exactly those bytes plus a
  length-prefixed signature per input — so the signed message is **the template minus its
  signature fields**. On a real **Bitcoin** ledger the message is instead the digest of that
  spend's **own** sighash algorithm — BIP143 for SegWit v0, BIP341 for Taproot, never "BIP341"
  as a blanket term (the rule the patched-client block already states) — and on the **EVM** leg
  it is none of these but `AdaptorSwapBound.legMessage`. Keep the three wordings apart. Meeting
  8's transaction-is-not-the-message ruling is this same point.
- ⚠️ **Every claim about π is CONFIGURATION-SCOPED**: config 1 is `RoleAProof::NotRequired`
  (`backend.rs:529`) and `protocol.rs` proves/verifies only under `Required`, while **PreVerify
  runs in all three** — so "Bob pre-signs after π and the pre-signature verify" is false for the
  classical config and contradicts `tab:configs`, which makes that absence a *finding*. Order in
  code: π verify → PreVerify → u₂ PreSign.
- `scripts/gen_bitcoin_tx_data.py` projects measured object sizes onto Bitcoin's real wire format
  (BIP141/144/341), self-checking against two published vB figures before emitting →
  `generated/btcmacros.tex` + `tab:btctx`; prose `docs/02-methodology/BITCOIN_TX_STRUCTURE.md`.
  ⚠️ **Config 1 must be projected from a DER witness item, never the 64-byte compact ECDSA
  signature** — that error understated the classical baseline and inflated every PQ ratio.
  ⚠️ **§res-txstruct IS NO LONGER PROJECTED (Royce, 2026-08-30: "tidak boleh ada satu pun angka
  yang projected").** It now reads two **mined** transactions — carriage `A1_mined.json` (an
  ordinary payment) and `legA_mined.json` (a settled swap leg) — via
  `scripts/gen_btc_measured_tx.py` → `btcmeasmacros.tex` + `tab_btctx_measured.tex`. **No
  `btcOne*`/`btcThree*` macro survives in any chapter** and the projected `tab_btctx` is input
  nowhere; `gen_bitcoin_tx_data.py` still supplies the protocol constants and measured-object
  ratios the chapter keeps (`btcMaxStdWeight`, `btcSigOverElement`), so do **not** delete it.
  ⚠️ **The CLASSICAL projection was validated by the mined A1 spend (191 B, 110 vB, 31 B
  output), but the projected LAS settlement was NOT the transaction later mined:** it assumed a
  Taproot output (94 B base, 2,885 vB) whereas the measured swap leg pays to P2WPKH (82 B base,
  2,905 vB). §res-txstruct replaces the projection with measured evidence — **do not describe
  this as a mere relabelling.**
  ⚠️ **What changed is a CLAIM, by Royce's ruling: the output does NOT grow.** Both mined
  transactions pay to P2WPKH, so the base is byte-identical at 82 B and the whole measured
  increase is witness. "The output script grows 31→43 B" belonged to that assumed Taproot
  output; it is deleted and must never be reinstated from the old figure.
  ⚠️ **Two things must not return with it.** (a) **No per-block capacity in this chapter** —
  `block_weight // tx_weight` is a bound, not a measurement; no block was mined holding 344
  legs, so "N settlements per block" is gone. (b) **The "model ledger agrees to within 0.2%"
  cross-check was tied to the projected total**; against the measured 11,373 B the gap is
  wider, so it was dropped rather than restated.
  ⚠️ A ratio over two mined transactions is **derived from measured**, and the prose says so.
  ⚠️ **Knock-on edits, checked chapter by chapter — only TWO were needed, do not churn the rest.**
  `02-methodology.tex` §"What a settled transaction contains" described the *projection*
  ("mapping the measured objects onto the fields… the committed key hash into the output
  script") and so carried the retired output claim while pointing at `sec:res-txstruct`;
  rewritten to the mined-transaction provenance. `05-conclusion.tex`'s "**Both** are measured
  over ledger models" was falsified by the change — Stage 2 is a model, the Bitcoin evidence is
  now mined — and now separates the two. **Abstract, Introduction and Evaluation need nothing**
  (verified: no projection prose, no `btcOne*`/`btcThree*`, no "43 B" / "per block"), and the
  appendix already reports the mined leg. ⚠️ `sync_report.sh` now also runs
  `gen_btc_measured_tx.py` — without it the chapter's `btcMeas*` go stale while `btcmacros.tex`
  moves underneath them.
- **Deliberately dead, do NOT repair:** `ref/amhl.{c,h}`, `ref/chain.{c,h}`, and
  `ref/test/{test_contract,test_pcn,bench_app}.c` — pre-seven-type (`las_pp`/`las_pk`/`las_sk`/
  `las_sig`), superseded by the Rust evaluation. `STAGE1_ONLY=1` skips them and still regenerates
  the **Stage-1** artefacts — NOT "everything the report consumes": Stage-2, on-chain, Criterion
  and Bitcoin figures come from their own runners.
  ⚠️ **`sync_report.sh` DOES NOT CLOSE THAT GAP EITHER, and the gap has already shipped a
  self-contradicting page (2026-09-01).** It runs only `plot_las_paper_figures.py`,
  `gen_report_data.py`, `gen_bitcoin_tx_data.py` and `gen_btc_measured_tx.py` — so of the six
  macro files `report.tex` inputs, **`stage2macros`, `btcnodemacros` and `btclasbenchmacros` are
  never regenerated by it**, nor is `fig_criterion_presign`. Consequence found live: `tab:rust`'s
  Criterion column moved to the 2026-08-28 log while the figure beside it still came from the
  captured 2026-07-30 run, so a caption reading *"this is the evidence behind the Criterion
  column"* sat next to a plot disagreeing with that column by ~2.5×. **After any Stage-1 re-run,
  regenerate the Criterion figure too** — `scripts/run_criterion_fig.sh --reuse` rebuilds it from
  whatever is already in `target/criterion` **without** re-running the 15-minute bench, but only
  after checking `target/criterion/<bench>/new/estimates.json` matches the table.
  ⚠️ **A run id is not a code pin.** `evidence/stage2/latest` → `20260730_162109` (git `4aef1f7`),
  and `RelationCircuit::generate_constraints` — the circuit configuration 2 actually proves — was
  **rewritten** after it by the (since-dropped) amortisation work, so those macros were measured
  against a different implementation than HEAD carries. Same class: the Stage-1 run's
  `metadata.txt` records `git_commit c41bbb5` while its own `git_status` shows `bench_levels.c`
  dirty, and that driver was only committed later in `c80dacb` — so the named commit does **not**
  contain the code that produced the numbers. Read `git_status`, never just `git_commit`.
  **`STAGE1_ONLY=1 scripts/run_benchmark_suite.sh`
  is the command that made `evidence/latest`, and the only one `app:repro` may print** — the bare
  runner builds the dead targets and aborts under `set -e` (fixed in the report 2026-08-12; the
  appendix also now names `test_serde_l3`, the target set, not the paper-dims `test_serde3`).
  ⚠️ **`test_swap.c` is NOT dead**: it is current seven-type code, built by `test/test_swap3`,
  kept out of `all` only because it needs the vendored proof library (its skip is the library,
  not the API — `run_benchmark_suite.sh`'s own comment lumping it with the dead files is wrong).

**⚠️ REAL CLIENTS, FOUR STAGES.** Write-up + all numbers,
scope and caveats: `docs/03-results/TWO_LEG_REAL_CLIENT_EXPERIMENT.md`. Runners
`run_onchain_two_leg.sh` / `run_btc_regtest_carriage.sh` / `run_btc_las_node.sh` /
`run_btc_two_leg.sh` → `evidence/{onchain_twoleg,btc_regtest,btc_las_node,btc_twoleg}/latest`.
- **Two-leg EVM swap**, two anvils on two chain ids, whole of Fig. 1 incl. π verify + both
  PreVerify gates. **⚠️ What comes off the chain is the adapted signature σ₂, NOT the witness**
  (runner steps 9→10): σ₂ is recovered from leg B's
  MINED transaction, then `test/extract_and_adapt` runs **Ext** on those chain bytes to get the
  witness and Adapts leg A. It is a **separate binary with no access to the local copy**, which
  is what makes the extraction real rather than assumed. Needed a new
  `evm/src/AdaptorSwapBound.sol` — `AdaptorSwap`'s message is a funder-chosen blob binding
  neither chain/contract/escrow/beneficiary/amount, and its `claimLAS` floor path drains any
  escrow without verifying. **`AdaptorSwap` is untouched** (a mode flag would move the
  measured baselines' gas); binding controls in `evm/test/AdaptorSwapBound.t.sol`.
- **Bitcoin carriage on STOCK Core v31.1** (Core's own `test_framework`). **Three separate pin
  gates — not one equality, since a version string is not a commit:** the binary's reported
  version string must *contain* the pinned tag; the source tree's `HEAD` must *be* the tag's
  commit
  (**not** `git describe`, which also matches commits made after the tag); `git status
  --porcelain` must be empty. A3 is `bad-witness-nonstandard` under default policy yet
  consensus-valid and mined. **Carriage only — verifies nothing.**
- **`OP_CHECKLASSIGVERIFY` (0xbb, a BIP342 OP_SUCCESSx) patched into v31.1**, over
  `base_verify_packed` via `bitcoin/las_consensus/` (C, not C++: ref headers use C11
  `_Static_assert`; aborting `randombytes`; consensus seed = SHA-256 of a documented preimage);
  patch applies to a pristine v31.1. **Differential control is load-bearing:** a STOCK node
  still sees 0xbb as OP_SUCCESS and accepts everything, so patched-REJECTS/stock-ACCEPTS is what
  attributes a refusal to the new rule. Verdicts come from `generateblock submit=false`
  (**consensus**, not `testmempoolaccept`'s policy); only `TestBlockValidity failed:` counts.
- **TWO-LEG SWAP ON THE PATCHED NODE (2026-08-18)** — `run_btc_two_leg.sh`, the Bitcoin twin
  of the EVM two-leg run: Gen+π → PreSign/PreVerify both legs → Adapt B → mine B → recover
  σ_B → Ext → Adapt A → mine A, over **two regtest chains** (never connected, diverge at
  block 1). **Three rules with teeth.** (1) The swap MUST be generated under the node's pp
  seed — `export_swap_vectors setup --pp-seed "$(las_btc_tool seed)"`, inherited via
  `pp_seed.bin`; `A'` comes from the seed alone, so a mismatch puts signer and verifier on
  different LAS instances. (2) σ_B is recovered by `bitcoin/tools/btc_recover_sig.py`, which
  locates the sig/key split from the **tapleaf's `sha256(pk)` commitment**, never a known
  length. (3) **Two provenance facts, never merged:** byte-equality with the Adapt output
  shows the chain carried that signature; the *witness*'s ledger provenance comes from `Ext`
  running as a separate program fed only the recovered bytes. Ext succeeding is **weaker**
  than byte-equality — any `(c, ẑ+y')` with `A·y'=Y` passes it. Macros `btcSwap*` (never mix
  with `btcLas*`/`btcMeas*`); PI=0 is recorded INCOMPLETE and does not move `latest`.
  ⚠ **THE MINED LEG IS A MEASUREMENT, AND THE DECK QUOTES IT INSTEAD OF A PROJECTION**
  (Royce, 2026-08-30 — "saya tidak mau ada projected"). `leg{A,B}_mined.json` carry the
  client's own `vsize`/`weight`/`size`, so deck slide 10's Bitcoin column reads
  `btcSwapLeg{Base,Witness,Vsize,Weight}` + `btcSwapItems`, **not** the `btcThree*`
  projections — both venues now show `measured`, which also retires the
  derived-vs-measured asymmetry a reader would otherwise query. New in
  `gen_btc_regtest_data.py`: `btcSwapLegBase`, `btcSwapLegWitness`.
  ⚠ **`size - base` is NOT the witness** — BIP141's total also carries BIP144's marker and
  flag, one byte each, so witness = `size - base - 2`; without it the figure is 2 B too
  large. Asserted to close (`base + 2 + witness == size`), and the generator now also dies
  unless `3*base + size == weight`.
  ⚠ **The chunked stack elements carry the PUBLIC KEY as well as the signature** — measured
  11,152 B = 6,736 + 4,416 exactly — and the two trailing items are the **tapscript** and
  the **control block**. Never write "the signature chunked", never "tapleaf" for that
  element, and say **stack-element limit**, not "push limit": {{btcChunkWidth}} bounds an
  element, and no script push opcode is involved.
  ⚠ **Scope travels with it: that leg settled on a REGTEST node carrying the experimental
  rule, so it is not a mainnet spend** — the slide says so, and "a patched node is not
  Bitcoin" is untouched. `btcThree*` remain correct and stay in the REPORT: they model all
  three configurations including the classical baseline, which was never mined and so
  cannot be measured.
  ✅ **deck ⊆ report closed for it 2026-08-30**: `app:btcnode`'s existing per-leg sentence
  now also gives the base / marker-and-flag / witness split and the chunking — appendix, so
  **zero words**.
  ⚠️ **Never write "read σ₂ off chain"** (deck label, fixed 2026-08-26): it reads as *off-chain*,
  inverting the claim. σ₂ is read **from** the mined transaction on chain B — `Ext` and the second
  `Adapt` are what run locally. Say **"from chain B"**.
  Honest path only — the tapleaf has **no refund branch**, so timeouts are not implemented.
  ⚠️ **REFUND/TIMEOUT ACROSS VENUES — verified against source 2026-08-19; a relayed
  "EVM ✅ / Bitcoin ❌" table overstated BOTH sides.** **Neither real-client two-leg run
  exercises the timeout/refund recovery path** — say *exercises*, never "implements":
  neither of THOSE two wires one in (`run_btc_eth_swap.sh` does — see below), but
  `AdaptorSwapBound.refund` **is**
  implemented (timeout + payer check + `State.REFUNDED` + transfer) and
  `run_onchain_two_leg.sh` simply never calls it. Writing "the EVM does not implement
  refund" would seed a false report claim. ⚠️ "Honest path only" (the runners' own scope
  line) means the **settlement** path, NOT untested-against-adversarial-input — both runs
  carry cross-leg negative controls (EVM §11 replay: leg B's message on leg A must revert
  with the contract's own `message not bound`, then a real send must leave the escrow
  untouched; Bitcoin adds chain-isolation and patched-vs-stock). **Never call either runner
  honest-path-only flatly.** Both `refund`s enforce only **their own** leg's timeout —
  pairing t2<t1 is the funders' job, not the contract's — and `test_Refund` /
  `test_TwoTimeoutSafetyWindow` cover **`AdaptorSwap` only; `AdaptorSwapBound`, the contract
  the run uses, has NO refund test.** `rust/las-swap`'s UTXO model *does* exercise refund
  (`run_refund`): both parties exist, but only **u₁'s leg is funded** — u₂ the counterparty
  never responds, u₁ reclaims, premature refund rejected. That corrected a report overclaim
  ("both parties recover their funds" → "the funder recovers its coin", `app:methoddetail`,
  word-neutral). A Bitcoin refund branch is NEW work under the M10 freeze — Royce's call,
  not a correctness repair; he directed it 2026-08-27 for a BTC↔ETH swap.
  ⚠️ **BTC↔ETH CROSS-VENUE RUN — PASSED 2026-08-27**, `scripts/run_btc_eth_swap.sh` →
  `evidence/btc_eth_swap/latest`. BTC = leg B (settled first, patched node, consensus rule),
  ETH = leg A (`AdaptorSwapBound`, one capped tx); σ₂ recovered from the MINED witness, and
  Ext's **adapted-signature input** came only from it. `btc_las_spend.py` gained an
  **opt-in** CLTV refund branch (`--refund-pk`+`--refund-locktime`, all-or-nothing; separate
  `taproot_for_refundable`, so the legacy **tapleaf, scriptPubKey and address** are
  unchanged — ⚠️ but the JSON outputs DO gain fields, so never widen that to "the legacy
  path is byte-identical"; gate: pk1 ⇒ `bcrt1pcdmx5g…rstwpg`). Needed `./xchain` in
  `evm/foundry.toml` `fs_permissions`. Traps, each cost a rejected edit: CLTV needs **both**
  `nLockTime` and a non-final `nSequence`, set **before** signing; the witness carries the
  **refund** key on the refund leaf. ⚠️ **THE REFUND TEST NEEDS THREE CASES, AND CASE A ALONE
  DOES NOT ISOLATE CLTV:** A (`nLockTime`=deadline, immature) is refused `bad-txns-nonfinal`
  by **BOTH** nodes — that evidences the deadline via transaction finality, but finality is
  checked before any script runs, so the differential says nothing about the leaf; **B**
  (final, deadline still ahead) is refused `Locktime requirement not
  satisfied` by the patched node while **stock ACCEPTS** (0xbb is an OP_SUCCESSx, so BIP342
  wins the whole script unexecuted) — B is the only case that isolates the leaf; C (matured)
  is accepted **and actually mined**, coin returned. ⚠️ **Full fairness is NOT established** — the claim
  leaf stays single-key under the *funder's* key, so the funder can spend without waiting,
  and **the mempool exposes the adapted signature before confirmation**: the funder, which
  holds the matching pre-signature, can run Ext on that unconfirmed signature and attempt a
  conflicting spend. If its conflicting spend confirms instead, it keeps this coin while
  using the extracted witness to claim the other leg. ("abort, not theft" is FALSE —
  retracted.) **A timeout closes neither hole**, so the recovery work below does not buy
  fairness wording.
- ⚠️ **RECOVERY LAYER EXTENDED 2026-08-27 — supersedes "only Bitcoin's deadline is exercised"
  and "no t₂<t₁ ordering is established"; do not quote either back.** Both venues' refund
  mechanisms now run, each on a **dedicated recovery-test object of that venue's own kind** —
  a control UTXO on Bitcoin, a second escrow on Ethereum (E1 before t₁ / E2 not-payer / E3
  matured pays out / E4 the settled leg stays `not open`). **Neither settled leg takes its
  refund path**; both are claimed. Leg B's CLTV operand is now a **UNIX timestamp** (was a
  block height), so t₂ and t₁ share one numeric domain and `t2 < t1` is asserted by the
  runner; `TwoLegFund` gained `TIMEOUT_ABS` (`TIMEOUT_SECS` still works, `run_onchain_two_leg.sh`
  untouched — and that runner **already configured** t₂<t₁, it just never exercised either refund).
  ⚠️ **One numeric domain is NOT one clock**: Bitcoin enforces timestamp finality against
  **median time past** (BIP113) while the contract uses `block.timestamp`, so `t1 − t2` is a
  **configured deadline gap, never a guaranteed reaction window**, and neither venue checks the
  other's deadline. Maturing Bitcoin needs `setmocktime` + blocks on **both** nodes, not height.
  **Supported wording: "a cross-venue LAS settlement, plus both venues' timeout refund
  mechanisms exercised and the deadline ordering t₂<t₁ configured and checked" — never "a
  fair/full atomic swap".** ⚠️ **This run feeds NO report macros** — `evidence/btc_eth_swap/` is read
  only by its own runner, so moving its `latest` breaks nothing, and **`btcSwap*` belong to
  `btc_twoleg`, NOT to this run** (the similar name already caused one false "the report is
  stale" claim). **It IS now in the report**: `app:btcnode` gained "One swap across two kinds
  of ledger" (2026-08-28) — appendix, so **zero body words** — quoting no figures, only the
  evidence path. ⚠️ **M11's "no artefact may imply it was run" is SPENT, not violated**: it
  barred claiming an unrun thing, and the run is real; it does not license recharacterising
  the Stage-2 measured swap, which stays **UTXO-with-UTXO** per Wang. The deck still says
  nothing about it: **deck ⊆ report is now satisfied for the cross-venue claims already
  stated in `app:btcnode`, and any stronger slide claim would still need report support** —
  rubric 3.2.2 is met by the DEMOS, never by extra claims, so adding a slide is Royce's call.
- ⚠️ **§4.1's two-timeout setup is its RECAP of [23], NOT a Fig. 1 requirement** (corrected
  2026-08-27; the old flat "§4.1 requires timeouts on both transactions and t₂<t₁" over-attributed).
  Fig. 1 restates **no** timeout. Its fairness argument instead uses the LAS security properties
  together with the proof-of-knowledge π and the M-SIS argument needed to bridge the extracted
  witness back into the small relation Adapt requires — the paper's own *"π is essential to make
  sure that u₂ receives the coins c₁"*. So the recovery layer is an **addition around Fig. 1,
  never a repair to it** — Fig. 1 was already being followed. ⚠️ In that recap **u₁ publishes
  first**, so the leg published first carries the **longer** t₁; it is the leg **settled** first
  that carries t₂ — publishing order and settling order are opposite. 2-of-2 the paper requires
  only for PCNs (§4.2).
  ⚠️ **Settlement publishes the ADAPTED SIGNATURE, not the witness** — `Ext` needs σ̂ too, so
  "settling it publishes/reveals the witness" is false and was purged from five places.
- **Framings that must not drift:** a patched node is **not** Bitcoin — "cannot settle on
  Bitcoin as it stands" stays true; implementing one of `BITCOIN_TX_STRUCTURE.md` §5.4's
  three routes is **not** a position on which should be adopted; **the rule's security is
  still unanalysed** — that caveat never lapses. Bitcoin binds the transaction, **not the
  chain** (BIP341 sighash has no chain id) — the EVM leg does (`AdaptorSwapBound.legMessage`
  hashes `block.chainid`); state the asymmetry.
  ⚠️ **THE MOMENT A FIGURE NAMES ETHEREUM IT OWES THE ESCROW (2026-09-01).** A generic
  "second ledger" picture may draw payer→payee; naming the venue makes it a venue claim,
  and here the ETH leg settles through `AdaptorSwapBound.claimBound` (adapted signature as
  **calldata**, then `transfer` to the beneficiary) — so "an ordinary account-to-account
  payment" contradicts our own `fig:evmtx`. Caught in `fig:swapidea`; both the drawn box and
  the caption had it, so fix the **picture and its caption together**. Free to fix: figure
  bodies and captions are outside the word count.
  ⚠️ **NEVER frame the two venues as a timeline** (Royce, 2026-08-21): "Ethereum yes /
  Bitcoin not yet" is misleading — it reads as Bitcoin lagging and catching up. The
  difference is **structural**: the EVM is programmable so verification is a contract you
  *deploy*, while Script is fixed so the same work needs a **consensus rule**. The report's
  own phrase is the safe one — *"the blocker is a consensus rule, not engineering"*
  (`03-results.tex`, `05-conclusion.tex`). Deck slide 10 now reads "Ethereum: deploy /
  Bitcoin: new rule". Naming a *specific* route (soft fork, OP_SUCCESS redefinition) on a
  slide is still barred — that would be a position on adoption.
  ⚠️ **WHAT §4 OF THE PAPER ASSUMES — read `2020-845.md:420`, never paraphrase from memory**
  (2026-09-01; two live report sentences were wrong). It assumes a UTXO chain *"where the
  signature algorithm is replaced with a lattice-based signature scheme"* **and, explicitly,
  that the chain supports the spending scripts the applications need** (signature and
  hash-preimage verification, timing conditions) — so **"assumes only the venue" is FALSE**
  and was deleted. It assumes that **end state**, never a **consensus change**: a consensus
  change is one route to it, and attributing it to the paper contradicts this project's own
  no-position-on-the-route rule above. Supported form: *"a consensus change to realise the
  setting assumed in \cite{esgin2020post}"*.
  ⚠️ **SUPERSEDED 2026-08-18, do not reinstate:** "no opcode costing, not wired into the
  swap" — both now **FALSE** (cost measured by the patched-client benchmark below;
  `run_btc_two_leg.sh` settles a whole Fig. 1 swap). Purged from `app:btcnode` and the deck.
- **It corrected a report number:** `gen_bitcoin_tx_data.py` projected config 1 from the
  64-byte *compact* ECDSA signature, not a DER witness item, understating the classical
  baseline and inflating every PQ ratio. Fixed; macros regenerated.

**Proof of knowledge π (Fig. 1)** — in scope by the exception recorded under Scope discipline.
`ref/relation_zk.{c,h}` (`relation_prove`/`relation_proof_verify`; **non-ternary witnesses
refused**, return −1 and no proof) over vendored **LaZer**, bridged by `relation_zk_lazer.c`.
Ternary via binary decomposition `[A|−A|0]·(r₊‖r₋‖e)=t′` with `r₊,r₋` proven binary. Committed
params `ref/relation_zk_params.h` ⇐ LaZer codegen from `scripts/las_pi_params.py` — so the
deployed statement is **witness-independent**, unlike the LaBRADOR bridge's. Rust twin
`relation_zk.rs` FFIs the same C bridge behind cargo feature `relation-zk` (default off, KAT gate
intact). π is **off-chain only**; opt-in targets. ⚠️ **Exactly TWO TUs may include `lazer.h`** —
`relation_zk_lazer.c` and `relation_zk_lazer_batch.c`; never call either "the only" one.

**⚠️ ON-CHAIN VERIFICATION FITS IN ONE TRANSACTION AT D3.**
`evm/src/LASVerifierOpt.sol` + `LASShake.sol` + `LASRegister.sol`, entrypoint
`AdaptorSwap.claimLASVerifiedOpt`; mechanism, numbers and caveats in §7 of
`docs/03-results/GAS_LIMIT_INVESTIGATION.md`. The baseline measured the *expression*, not LAS.
Gated twice: modelled charge (`test/LASGasBreakdown.t.sol`) **and** a real client receipt
(`scripts/run_onchain_one_tx.sh` → `evidence/onchain_onetx/<ts>/`).
- **⚠️ "SAME PREDICATE" IS CONDITIONAL ON A REGISTRATION INVARIANT NOTHING CHECKS** (2026-08-10).
  Equality needs `aHatPacked=NTT(A')` (both verifiers) plus, in the Opt path,
  `tHatPacked=NTT(t)` **and** `tPacked=pack(t)` for the *same* `t` (only `pack(t)` is hashed —
  `A'` never is). `lasContext` binds the values **supplied**, blocking substitution by the
  claimer; `claimLASVerifiedOpt` then hands all three to the verifier **separately**, and the
  verifier derives none from another — so nothing anywhere checks consistency. Break it and the
  predicate is **different** — *not necessarily weaker*; **some** are (`aHat=tHat=0` ⇒
  satisfiable with **no key**, so the escrow pays out without the adapted signature being
  published, the leak atomicity needs). Beneficiary is fixed at funding, so the loss is the
  **registrant's**: *atomicity, not custody*. Never write "full `base_verify`" without the
  condition. The fix is a **derive-and-compare, not another commitment**: recompute `NTT` from
  the submitted `tPacked` at fund time and *require equality* with the submitted `tHatPacked` —
  that discharges the `t` conjuncts only, **never** `A'`. → §7 caveat 3, `tab:onchain` appendix.
- **Never present it as faster-because-weaker** — pinned to `LASVerify` and C ground truth
  (`test/LASVerifierOpt.t.sol`, `test/LASShakeEquiv.t.sol`); do not weaken those pins.
  **`claimLASVerified` stays as the measured baseline — do not delete it.**
- **⚠️ SCOPE IS PART OF THE CLAIM (→ EVIDENCE-OR-SILENCE).** Measured **at D3, 32-byte signed
  message**, on the EVM revision in the evidence. Headroom is effectively a *message-length
  budget* (preimage `pack(t)‖pack(w')‖M`; the sponge dominates) and is a **derivation, not a
  measurement**.
- **⚠️ THREE-WAY STATE across parameter sets (2026-08-15) — do not restate any row.**
  **D3 = MEASURED fits, ONE instance · D2 = MEASURED fits, ONE instance (~65% of cap) ·
  D5 = DERIVED, one transaction is exceeded — a lower bound COMPUTED FROM measured
  quantities; the bound is arithmetic, never itself "a measured lower bound" (2026-08-18).**
  - ⚠️ **THE TWO MEASURED ROWS ARE DIFFERENT BOUNDARIES — never put them side by side
    without saying so** (2026-08-18; three drafts failed this in one session). **D3 is a real
    client's receipt for a WHOLE CLAIM** (`cast send` to the escrow: verification + context
    binding + state transition + event + payout). **D2 is a harness charge for VERIFICATION
    ONLY** — `LASGasBreakdownD2.t.sol` targets `D2VerifyHarness.run`, not
    `claimLASVerifiedOpt`. D2's ~65% is **not** D3's 97.8% one set down. That test also reads
    `gasleft()` *before* `require(ok)`, so the ACCEPT assertion gates the **reported result**,
    not the measurement — never "asserted to accept before any gas was read".
  - ⚠️ **"One instance" is part of BOTH measured rows, symmetrically.** `SampleInBall`
    (rejection loop) and `_decodeZ` (branches on coefficient value) are **data-dependent**, so
    each figure is one signature's cost, not a bound over inputs. Say "the measured golden
    instance fits", never "D3 fits" / "D2 fits" flatly. **Instance-to-instance variation is
    UNQUANTIFIED at every set** — never assert it is negligible, never assert it is material,
    and never assume one set's variation resembles another's. Settling it needs several
    instances measured. Context, not a verdict: D3's headroom is ~364 k gas against ~732 k
    spent in those two stages there; D2's headroom is ~5.8 M.
  - **D2** — `evm/src/LASVerifierOptD2.sol` (a **copy** of `LASVerifierOpt` with the set
    re-instantiated; never parameterise the D3 file, a mode flag would move its pinned
    baselines) + `evm/test/LASGasBreakdownD2.t.sol`, vectors via `test/export_verify_vector2`
    → `evm/test/vectors/d2/`. Evidence `evidence/onchain_d2/latest`.
  - **D5** — gate `evm/test/LASShakeGrowth.t.sol` (fixed arena, **no `--gas-report`**) →
    `scripts/derive_onchain_d5_bound.py --growth-log …`, UNRESOLVED unless measured
    `deltaAbsorb ≥ slack + SampleInBall + decodeZ`. Evidence `evidence/onchain_d5bound/latest`.
    **⚠️ Supported wording is narrow:** "derived from measured quantities, one transaction is
    exceeded at D5". **NOT** — "measured at D5", any **D5 gas total** (a lower bound exists,
    never a value), or "needs more optimisation" (that optimisation could close the gap is its
    own unevidenced claim).
  ⚠️ **"No other parameter set was evaluated" is FALSE** (true only until 2026-08-15).
  **Purged 2026-08-18** from `tab:onchain`, `05-conclusion.tex`, the deck and
  `GAS_LIMIT_INVESTIGATION.md` §7; report macros `\gasDTwoTotal/CapPct/Headroom` now come
  from `evidence/onchain_d2/` via `gen_report_data.py`. If it reappears, it came from an
  older draft.
- **⚠️ Five derivations of D5 failed review before one held — do not re-derive, cite the gate.**
  (a) "Execution can only grow" proves nothing alone: push calldata the D5-favourable way (every
  added byte zero) with execution frozen and D5 lands *under* the cap. **A worst case binds only
  when EVERY free variable is pushed the adverse way at once.** (b) `shake_gas / blocks` is
  **not** a per-block lower bound — the stage total carries `init` and the pad tail, so dividing
  *over*states. (c) **Two** stages are data-dependent, not one — `SampleInBall` **and**
  `_decodeZ` (`if gt(f,137935)`) — so both are subtracted whole; every other stage is
  loop-counter-only (checked in `ZKNOX_NTT_dilithium.sol`/`_mulInto`) and is counted at zero
  growth. (d) An `absorbPad` difference is contaminated unless both calls run against a
  **fixed-size arena**: `absorbPad` allocates its 168 B pad scratch *inside* the timed frame.
  (e) The two lengths take **different tail paths** (rem 80 → two word-copies + the `if tb`
  branch; rem 96 → three, no branch), so the tail is neither droppable nor provably ≥ 0 — the
  fixed arena *measures* it instead. ⚠️ `LASGasBreakdown`'s named SHAKE stage is
  `init() + absorbPad()` and stops there — `_digestMatches` is **not** in it.
- **⚠️ Porting to D2/D5 is NOT a constant-block edit** (checked 2026-08-15; a note here saying
  "constants plus two literals" was wrong). The **assembly unpacker hard-codes D3 at seven
  sites** — `lt(i,11)`, `add(48,mul(19,…))`, `shr(mul(19,u),…)` with mask `0x7FFFF`,
  `gt(f,275870)`, `137935`, `8518352`, `calldatacopy(…,48)` — and its group-of-8 byte-alignment
  argument is keyed to `Z_BITS`. `LASGasBreakdown.t.sol` is D3-shaped too.
- **⚠️ NEVER MEASURE A CAP GATE UNDER `--gas-report`** (cost a false FAIL once): the inspector is
  metered inside the measured frame and inflates `gasleft()` deltas **and** `vm.lastCallGas()` by
  more than the whole headroom. `run_onchain_gas.sh` runs gates **without** the flag, the table
  with gate contracts **excluded**, capturing **each pass's exit status separately** (one
  `{...} | tee` hides a failed gate behind a green table). `LasVerifiedOptSwapGas` (report-only)
  and `LasVerifiedOptSwapGate` (asserts) are twins for this reason — never merge them.
- **⚠️ GAS ACCOUNTING IS EIP-7623, NEVER EIP-2028** (cost real work once). Charge is
  `21000 + max(4·tokens + execution, 10·tokens)`, `tokens = zero + 4·nonzero`. **The
  `+ execution` is load-bearing** — without it the floor always binds and "which branch bound"
  is meaningless. Authority = `LASTxGas`, mirrored in §7. The
  floor binds for calldata-heavy/compute-light txs, so the old 16-per-nonzero-byte model
  **understates** them; every total must say which branch bound. (`TwoLegSwapGas.t.sol`'s helper
  models only the standard branch — never copy it for a gate.)
- **⚠️ EIP-7825 IS A *PER-TRANSACTION* CAP — for THIS PROJECT the block gas limit is NEVER a
  feasibility basis** (2026-08-19; every artefact measured here is a single transaction, so the
  per-transaction cap is what binds — this is a scoped rule, not a universal claim about the
  EVM). The block-limit comparison is the claim this project already RETRACTED, yet it was still
  live in `BITCOIN_TX_STRUCTURE.md`; fixed, and the `GAS_LIMIT_INVESTIGATION.md` glossary now
  says so. From the cap derive **only** percentage of cap, whether one transaction fits, and
  per-transaction headroom — **never claims-per-block**. ⚠️ **Write the ratio "*x* × the cap",
  never "exceeds it by *x*×"** — the latter reads as the *excess* being *x* times the cap. Four
  docs carried the ambiguous form and were fixed the same day; take the ratio from the macros.
- **⚠️ EIP-8051 (ML-DSA precompile) is a CITATION, NOT A ROUTE**: **Draft**, **Declined for
  Inclusion** in Glamsterdam (EIP-7773), **ML-DSA-44 only** — not D3. ⚠️ **Attach no date**:
  pq.ethereum.org places PQ sig precompiles at milestone `J*` in a *relative* order with **no
  fixed date**, so no year may be cited to it. Its ETH variant
  replaces SHAKE256, so "stock FIPS-204 verifier accepts it" and "use the ETH variant" cannot
  both be claimed. Any figure from it is a **conditional model from the EIP's own constant,
  never a measurement**. §8 of the same write-up.

**On-chain verification (EVM) — the baseline, retained as evidence.** `evm/src/LASVerifier.sol`,
a complete native verifier over vendored ZKNox ETHDILITHIUM primitives, validated against C and
bound by a fund-time `keccak256(A′,t,M)` commitment; its cost is far above EIP-7825's cap.
⚠️ **NAYSAYER IS OUT OF THE DISSERTATION (Royce, 2026-08-19) — "tidak ada value nya".** Removed
from Ch.3/4/5, `app:naysayer` deleted, all `gasNaysay*` macros dropped — ⚠️ **except one Ch.5
clause that survived to 2026-08-28** ("the optimistic variant … over-cap fraud proof"), a dangling
reference once `app:naysayer` went: **third** missed purge, so re-grep the ARTEFACT, not the diff.
**SUPERSEDES** the
2026-08-19 rule "retain the measured negative result" written earlier the same day — do not quote
it back. Contract, tests and `evidence/onchain/` **stay**; a Naysayer figure reaching the report
does not. Context, so the decision is not re-litigated: Wang suggested adapting it (M7 08:59) and
M8 §12 filed it as *discussion of a more advanced solution*; Royce overrode. Also settled while
removing it — keep for the source, which still holds it: binding fraud-proof path is
**`naysayDigest`, NOT `naysayWprime`** (true for the tested vectors only, execution gas being
data-dependent); both source comments guessed the reverse and the test header used EIP-2028's
16/4 calldata model instead of EIP-7623. Gas macros come from `gen_report_data.py`, whose parser
is **contract-aware** — a bare `claim` silently bound the test mock `Rejector.claim`.
**⚠️ THE EVM IS A COMPARATIVE STUDY, NOT A FAILED FIRST VENUE (Royce, 2026-08-19).**
eprint 2020/845 §4 *assumes* UTXO ("we assume an Unspent Transaction Output (UTXO)-based
blockchain like Bitcoin"), so UTXO is the paper's own setting and the EVM is what a gas-metered
venue costs by comparison. "Change of venue" / "first venue failed" / "why the venue was changed"
are **purged report-wide** — but the 2026-08-19 purge MISSED two, found 2026-08-20 by grep
(`03-results.tex` "forced the change of venue", `EVM_TX_STRUCTURE.md` §4 heading): a purge is
not done until it is re-grepped. Do not reintroduce them. Meeting 7 still stands on fees-not-gas-limits,
heavy work off-chain, and adaptor swaps being used on UTXO chains in practice; on-chain LAS stays
orders of magnitude above a classical `ecrecover` claim. Do not re-open Stage 2's venue. Detail: `docs/03-results/GAS_LIMIT_INVESTIGATION.md`,
`docs/02-methodology/EVM_TX_STRUCTURE.md`. **IPFS off-chain storage** = **fallback, NOT adopted**
(`docs/04-evaluation/IPFS_OFFCHAIN_STORAGE.md`): the swap needs none of it (π is a direct
party-to-party message), and for the optimistic verifier a data-availability failure becomes a
*soundness* failure.

**Succinct PQ proving — both directions run; numbers, derivations and framings in
`docs/03-results/SUCCINCT_PQ_PROOF_EXPERIMENT.md`.**
- **`rust/las-stark` — TWO modules, never conflate.** `relation_air` (WIP gadget): FRI-STARK
  over the *arithmetic core* of `base_verify`; the Fiat–Shamir hashes are **not** in the AIR,
  so `z` is bound to `(A′,t,c,w')` but **not** `(c̃,M)` — **never** call it a complete proof of
  on-chain verification. `role_a_air` + `bench_role_a.rs` proves the WHOLE role-A statement, so
  **that caveat does not apply there**; runner `run_role_a_stark.sh`.
  **⚠️ DISQUALIFIED AS π AND BARRED FROM THE REPORT (Royce, 2026-08-04/05):** eprint 2020/845
  §4.1 needs π to HIDE the witness (if u₂ learned `r` it could adapt σ̂₁ and take both sides),
  so π must be ZERO-KNOWLEDGE and this STARK is not. Internal evidence only, never a result.
  No Groth16 wrap anywhere in `las-stark` — it would defeat post-quantum security.
- **π under LaBRADOR.** `ref/relation_zk_labrador.{c,h}` (THIRD and last vendored-proof-library
  TU) + `bench_labrador_role_a.c`; runner `run_labrador_role_a.sh`. Encoding
  `[A|−A]w − q·g = t'`; **the g bound is load-bearing** (unbounded g satisfies it for any `t'`).
  **CLOSED ON COST, and cost only:** in the encoding and run tested it loses to the deployed
  LNP22 on **the three measured axes** (proof size, prove, verify) — succinctness is asymptotic
  and one role-A relation far too small. **That verdict stands** (Royce, 2026-08-10).
  **⚠️ NOT closed as a working π — never claim it is** (2026-08-10). Obey the header's
  MAY/MAY-NOT contract everywhere. **MAY:** LaBRADOR proves the encoded statement, and a witness
  for it yields one for the target relation (*soundness direction only*). **MAY NOT:** that the
  encoding is faithful to the target relation, or that this is a zk proof of exactly it. Two
  independent blockers — fixing one leaves the other:
  **(1) PRIVACY (not a ZK failure)** — `zk=1` IS passed and the proof *is* zk **for the encoded
  statement**; the defect is that the driver declares the honest witness's **exact** ‖w‖²
  (= Hamming weight of ternary `r'`) as part of that statement, so the statement itself leaks a
  witness statistic. zk bounds what the *proof* adds beyond the statement and cannot repair one.
  Fix would be witness-independent `‖w‖² ≤ (n+ℓ)·d`. **⚠️ RULED: do NOT apply it, do NOT re-run**
  (Royce, 2026-08-10) — keep `evidence/labrador_role_a/latest` reproducible; it clears privacy
  only, blocker 2 survives, so no project decision moves.
  **(2) FAITHFULNESS (nothing to do with ZK)** — `‖g‖² ≤ G_NORMSQ_BOUND` is an *extra*
  constraint, justified only by an assert that the *sampled* instance fits. The bound **is**
  witness-independent (never say otherwise — the defect is completeness, not dependence), and
  honest-prover failure is **unquantified: not shown zero, not shown positive** — never write
  "not zero", the same unwarranted-negative trap EVIDENCE-OR-SILENCE owns.
  **⚠️ NEVER collapse the three properties into one verdict word** (this conflation has been
  reintroduced three times): PQ survives the encoding; succinctness is intact but its size
  advantage is *unrealised at this statement size*; zk holds for the encoded statement, and what
  fails is the **composite** claim — a zk proof of exactly the *target* relation.
  **Caveats:** the encoding is ours; proof size is the library's printed *estimate*, not
  byte-exact like LNP22's `prooflen` — never compare silently. **Gate names `PI_LAB_*`** — never
  rename/alias `PI_ROWS`/`PI_COLS`/`PI_DEG` or `PI_BATCH_*`. **THREE TRAPS:** `src/labrados` is a
  submodule the README's LaZer clone does NOT fetch (`git submodule update --init src/labrados`,
  then `make liblabrador38.so`); LaZer's `src/labradosNN_py.h` declares `N 64` while the
  submodule uses `N 256`, so **always** use `src/labrados/labrados_python.h` with
  `-DLOGQ=NN -DNDEBUG -Isrc/labrados`; labrados' `simple_verify`/`verify` return **1 on
  SUCCESS**. **LOGQ=38 forced** (36 overflows; soundness budget at the declared g bound).

**ML-DSA adaptor experiment.** LAS on FIPS 204 **as specified** (hint, Power2Round, high/low-bit
split all enabled), zero upstream functions modified, control verifier = stock
`crypto_sign_verify`. `ref/mldsa_las.{c,h}` + `test_mldsa_hint{2,3,5}` (diagnostic — a FAILS row
*is* a result), `test_mldsa_las{2,3,5}`, `bench_mldsa_compare{2,3,5}` (both constructions in ONE
binary); runner `run_mldsa_hint_experiment.sh` → `evidence/mldsa_hint/<ts>/`, write-up
`docs/03-results/MLDSA_HINT_EXPERIMENT.md`. **Never mix its numbers with `evidence/latest/`.**
- **The claim, in its corrected form** (softened again 2026-08-28 — "necessarily new" claimed
  more than the naive port's 0/200 shows, which is that THAT reuse fails, not that none can
  work; same correction `tab:challenges` row 2 already records for the disabled optimisations,
  and the report now says **require adaptor-specific algorithms** at all four sites):
  *PreSign and PreVerify require adaptor-specific algorithms;
  `Verify` is not.* With the whole commitment path (committed high bits, low-bits rejection test
  **and** `MakeHint`) on `w+Y` and PreSign tightened to `GAMMA1−BETA−ETA`, all adaptor properties
  hold at ML-DSA-44/65/87, **including unmodified FIPS 204 `crypto_sign_verify` accepting the
  adapted signature**. ⚠️ Never restate the superseded "reference optimisations must be disabled"
  version.
- **Caveat that travels with it:** functional demonstration only — the security of committing to
  `HighBits(w+Y)` is NOT analysed.
- **`Y` is byte-identical in both** constructions (`K` full-width polynomials either way) while
  ML-DSA halves signature and public key — so **`Y` becomes the dominant remaining size target**
  (corrected 2026-08-17: "must target Y, *not the signature*" overshot the data — the payload
  still falls to `\mldsaPayloadRatio`, so the signature is smaller, not irrelevant).

**Statement compression (CLOSED) and proof amortisation (RUN; verdict NARROWED 2026-08-19).**
Quote the write-ups for numbers, never this file.
- **Statement compression** — `ref/test/test_statement_compress.c` (+`{2,3,5}`, in `all`),
  `run_statement_compress.sh` → `evidence/statement_compress/latest`, write-up
  `docs/03-results/STATEMENT_COMPRESSION_EXPERIMENT.md`. Hard gate: the full-statement control
  must hold the contract or nothing is attributable. **Verdict CONFIRMED with a mechanism:**
  truncation is invisible to the adaptor's own functions and fatal at both boundaries
  (`base_verify` never sees a statement; Ext's acceptance test IS the exact relation), and fails
  outright rather than marginally; the seed candidate compresses totally **and hands the receiver
  the witness**. ⚠️ **Scope: the sweep is `b = 1, 2, 4, 8, 13` — say "every truncation depth
  TESTED"**, never "at every depth". **Do not reopen `Y` compression
  inside this construction; the open question is a different hard relation whose statement is
  smaller by design.**
- **Proof amortisation — BOTH provers measured. ⚠️ "batching fails on both" is SUPERSEDED
  (2026-08-19) and must not be restated:** Groth16's `1/k` lands on a cost that was already
  negligible, but for LaZer only the **compute** penalty is measured — the proof-size result is
  withdrawn (below), so the trade-off cannot be weighed and no overall failure verdict is
  warranted for it. Groth16 harness: `bench_amortise.rs` + `BatchedRelationCircuit` (one relation
  shared by single and batched circuits + a per-batch tamper check, so a batch cannot prove
  something weaker) → `evidence/amortise/latest` (⚠️ three runs that day — **`latest` is the one
  to quote**). LaZer: `ref/relation_zk_batch.{c,h}` (block-diagonal = the conjunction of k copies
  of the *deployed* statement) + `ref/relation_zk_lazer_batch.{c,h}` (the **second** TU that may
  include `lazer.h`) + committed `relation_zk_params_k{2,4,8}.h` ⇐ `gen_lazer_batch_params.sh`
  (SageMath at `~/micromamba/envs/lazer-sage/bin/sage`); **k=1 dispatches to the COMMITTED
  `las_pi_params`**. Write-up `PROOF_AMORTISATION_EXPERIMENT.md`. **Framings:** Groth16's proof
  is constant in k so per-swap bytes fall `1/k` **but bytes were never the bottleneck** — a
  statement about *Groth16*, not about batching; for LaZer, at the **k ≤ 8 actually tested**,
  per-swap prove+verify gets several times worse — never call that *superlinear*, no scaling law
  was established. ⚠️ **The LaZer proof-SIZE direction is NOT a usable finding**:
  `bench_lazer_amortise.c` declares `prooflen` once (l.116), overwrites it every repetition
  (l.123) and records the **last** one (l.163) while timings get mean±SD — and LaZer Huffman-codes
  the Gaussian responses, so length follows the sampled values. One sample per k, not a statistic.
  ⚠️ **NOT wired into the swap**; batched param sets **not independently reviewed**, no security
  claim about batching. ⚠️ **OUT OF THE DISSERTATION (Royce, 2026-08-19) — "tidak ada value nya".**
  Code, params and `evidence/amortise/` **stay**; batching reaches neither report nor deck. Ch. 5
  now names **two** optimisations, not three — and so does the deck's evaluation slide, whose headline read
  "three suggestions" until 2026-08-20 while its own card said "Two". Second missed purge found
  that day: **re-grep counts, not only phrases.**
- **Not attempted, by ruling not by shortfall:** *analysing the ML-DSA variant's security* is
  out of scope (a reduction, not code). It stays in Ch. 5. ⚠️ Never restore the superseded
  "needs a SHAKE256 precompile / Merkle dispute / succinct proof" prediction — unevidenced, and
  falsified by the ON-CHAIN block.

**⚠️ AMHL is DROPPED (Royce, 2026-08-03).** Multi-hop locks are **out of the project** — not a
bonus, not future work, not a deliverable. Do not build on `ref/amhl.{c,h}`, do not revive it,
do not re-add it to any status list or work queue.
*Cleanup state, verified 2026-08-05:* `report/latex/` is **clean**; `docs/` still mentions it
(`grep -rli "amhl\|multi-hop" docs/`). Per occurrence, not wholesale: **(a) a claim about
*this project's* artefacts or results → must go; (b) literature/background about *other
people's* work → may stay.**

**Reproducibility spine:** `README.md` = build/run entry point, delegating to
`docs/A-appendix/REPRODUCE_LAS_{C,RUST}.md`. ⚠️ The upstream pin `2374d22` is **not in README**
(it says only "pinned commit") — cite `FUNCTION_MAP.md`, `CODE_DIFF_VIEW.md` or
`REPRODUCE_LAS_C.md` for it. `docs/02-methodology/FUNCTION_MAP.md` classifies every Dilithium
function call-as-is / modify / new — headline: **zero upstream Dilithium source functions
modified** (the `Makefile` does gain additive targets; keep that qualifier). Two-branch diff =
`dilithium-baseline` (pristine) vs `main`, mapped in `CODE_DIFF_VIEW.md`. Each runner under
`scripts/` writes a timestamped run directory with raw tool output plus a provenance record, and
updates a `latest` pointer. ⚠️ **That is the SHAPE, not a uniform contract:** the provenance
filename (`environment.txt`, but `metadata.txt` in the Stage-1 `run_benchmark_suite.sh`), whether
branch as well as commit is recorded, whether `latest` is a symlink or a copy-fallback, and
whether macros/figures regenerate all **vary by runner** — read the one you mean before asserting
any of it.

## What remains (re-derived from Meeting 10)

1. **6–8 minute presentation — MOCK DELIVERED AND REVIEWED at Meeting 10.** Verdict:
   *"content-wise, it's okay… presentation-wise, you can improve it a bit."* **The rework is
   the deck's next job, not a rebuild** — see the Meeting-10 block for the full ruling: cut
   **13 slides → ~10**, lead with the application motivation, add a high-level **picture of the
   method**, close by answering the questions the opening posed, and **delete the unevidenced
   "D5 needs more optimisation" line**. ⚠ **THE MOTIVATION SLIDE MUST ARGUE THE PQ-EXOTIC CASE,
   VISUALLY (Wang, 2026-08-21)** — explaining what an atomic swap is is NOT motivation. The
   argument is a three-step chain and belongs in the picture, never in a footnote: what runs
   today rests on **elliptic-curve** adaptor signatures → **Shor** breaks the problem they rest
   on → NIST standardised only **basic** replacements, so the exotic layer is uneven (keep
   "multi-signatures are being built" — that clause IS the softening M10 #2 ordered) → the
   adaptor case is this project; slides 2–4 now draw it. **⚠ SECOND BATCH, SAME DAY (Wang via
   Royce, 2026-08-21) — five instructions, all applied, and they OVERRIDE M10's slide count:**
   (1) motivation **first and starting with quantum-computer background + the urgency** — why an
   *expensive* exotic PQ signature is worth paying for; (2) close with **"is LAS good or not"**
   and the **implications for Bitcoin developers / blockchain communities**; (3) show the
   **transaction structure of BOTH Bitcoin and Ethereum** — "people want to see how it works in
   practice, not in theory"; (4) the swap must be followable by someone who does **not** read the
   maths; (5) **real BTC/ETH logos and real prices**. Plus Royce's framing rule: **the video must
   not be the report read aloud** — it exploits the medium (motivation, results, evaluation,
   conclusion, implications), or there was no point asking for a video.
   **REWORK DONE 2026-08-17, EXTENDED 2026-08-21** — `video_deck.html` is now
   **13 slides**; planned runtime = the deck's own `data-time` sum, **never retyped here** (three
   copies had drifted apart by 2026-08-22): motivation over slides 2–4 (quantum clock → the application →
   the exotic gap), the two demos, **"What actually goes on chain"** (both venues field by field),
   and a closing **verdict + one card per audience**. Slides 2–6 are **drawn scenes** (inline
   SVG on the deck tokens, built in four timed `.st1–.st4` stages, no keypress); the stepper's
   fourth part is **Implications** (was Takeaway); the key-map overlay defaults **off** (it was in
   shot); the ML-DSA attempt got the slide **table** Wang suggested. Demos are slides **7 and 11**
   (13-slide deck, 2026-08-21) — fix that pair of numbers wherever it is cited. ⚠ **13 slides
   exceeds M10's "~10" deliberately** — each addition is a later Wang instruction; cutting back is
   *his* call, and `VIDEO_PLAN.md` §1 names the cut order. ⚠ **Timing risk is now BOTH ends**
   (7:23 leaves ~35 s each way), not under-running.
   ⚠ **PRICES AND QUANTUM ESTIMATES ARE *CITED* CLAIMS, and carry a source AND an instant.** The
   swap picture is **value-matched at spot** (so the *ratio* is what is asserted: BTC/USD and
   ETH/USD from the CoinGecko API at a printed UTC minute, corroborated by Coinbase to 0.04%) —
   **re-read both before recording on another day; a stale price is a wrong number.** The urgency
   slide quotes three dated resource estimates and NIST IR 8547; two traps, both live: the fall
   (20 M → <1 M qubits) may only be quoted **at ONE fixed target** (RSA-2048) — dividing the 2019
   RSA figure by the 2026 secp256k1 figure is a ratio across targets, not a trend — and the NIST
   row is the **≥128-bit** one, *disallowed after 2035*, **never** the 112-bit "deprecated after
   2030" row that second-hand sources quote, which is not where secp256k1 sits. ✅ **SETTLED
   2026-08-25 (Royce): the deck may carry NO content the report lacks** — "the difference is
   presentation, not content". All four citations are now **in the report** (`refs.bib`:
   `gidney2021factoring`, `gidney2025factoring`, `babbush2026securing`, `nistir8547`; §1.1 +
   `fig:whynow`), so the divergence is closed — supersedes the old "deck-only, Royce's call"
   note. Two traps that survive the move: the 2019 estimate is **published 2021** (Quantum
   5:433) so the marker year and the citation year differ *by design*, and a body sentence must
   scope the fall (**"for comparable targets"**) — "estimates keep falling" unscoped re-opens
   the across-targets trap the caption exists to prevent.
   ⚠ **RENDER, NEVER REASON, ABOUT LAYOUT (2026-08-17)** — the previous session shipped voids and a
   clipped table that were invisible from the markup. Deck: Windows Chrome from WSL, deep-linked
   (command in `VIDEO_PLAN.md` §3). ⚠ **Report TikZ figures the same way — occlusion is what the
   source cannot be trusted on**: render the page (`pdftoppm -f <page> -r 400 report.pdf`) after a
   geometry change. `fig:swapflow`'s white payload labels sat at `text width=21mm` in an 18 mm gap
   and painted over the neighbouring boxes' corners (2026-08-31, fixed by dropping `text width`
   **there** — not a rule for figures that need controlled wrapping).
   After any **chrome** edit (stepper, eyebrow, logo band)
   re-shoot EVERY slide, not a sample: a 2026-08-20 stepper edit collided with exactly one
   eyebrow, on the one slide not sampled. Three further defects were invisible from the markup on
   2026-08-21 and only a screenshot found them: an SVG label wider than the rect drawn behind it,
   a `.tx` box **silently clipping** its last line under `overflow:hidden` (now `flex:none`, so an
   overrun shows instead of truncating), and a two-line source note printing straight through the
   footer.
   ⚠ **SCREENSHOTS ARE NOT ENOUGH EITHER — MEASURE (2026-08-26).** Four rounds of eyeballing still
   left overruns; `report/slides/audit_overflow.js` + `--dump-dom` walks **every slide and every
   beat** and reports real geometry (recipe and its two gotchas in `VIDEO_PLAN.md` §3). What it
   caught: a 67 px clipped line on slide 5, tx boxes overrun on slide 3, and — the root cause worth
   remembering — **a hidden beat block must leave the FLOW, not just fade**: `.wsay` toggled
   `opacity` only, so Bob's two blocks both occupied space and the visible one painted over the rail
   at beats 2 and 4. Toggle `display` for beat blocks that stack; keep opacity only where the strip
   must hold its height (`.wrail`, `.pipe`). ⚠ A shot taken mid-transition is not a defect — kill
   `transition`/`animation` before capturing a beat.
   ⚠ **THE AUDIT LIED FOR TWO SESSIONS — `audit_overflow.js` HAD A SCALE BUG (fixed 2026-08-30).**
   It compared a **screen-space** `sb.top` against the **unscaled** `slide.clientHeight`, so at the
   headless 0.86 stage scale its bottom threshold sat ~46 px BELOW the slide's real padded edge and
   it returned **CLEAN for slides painting straight through the footer** (DEMO B, caught only by
   screenshot). Both thresholds now derive the scale from the rect itself. **Re-run it before
   trusting any earlier "clean" verdict**, and keep shooting the slide as well: the two find
   different things, and a clean audit is still not a clean slide.
   ⚠ **THE AUDIT CANNOT SEE SIBLING OVERLAP AT ALL — three slides shipped broken behind a green
   run (2026-08-30).** It tests escapes from the slide's padding box and from a framing card, so
   content that paints *over a neighbour* while both stay inside the slide is invisible to it:
   slide 8's column spilled upward through the headline (a `justify-content:center` column that no
   longer fits spills BOTH ways, and upward is never checked), slide 10's summary line painted over
   the LAS bar, and slide 11's Ethereum measured line painted over the conclusion. **Every one was
   found only by looking at the render.** Anchor columns to the top when their height is not
   guaranteed, and treat "overflow clean" as necessary, never sufficient — the screenshot decides.
   ⚠ **IN-SHOT TYPE FLOOR IS NOW 22 px — slide 13's scale, deck-wide (Royce, 2026-08-30:
   "tidak boleh ada tulisan kecil-kecil lagi ... hanya daging").** Superseded the old 13 px floor.
   Gate: `report/slides/audit_type.js` (same `--dump-dom` recipe) reports every visible run below a
   floor, in **canvas px**. ⚠ Its whole point is SVG: a scene's `font-size="22"` paints at
   `22 × viewBox scale`, so a scene that shrinks when neighbouring content stacks silently drops
   below the floor — **measure the painted size, never assume a scene renders 1:1**, and re-measure
   after any change to what sits beside it. Buying the size means CUTTING: what went was byte
   figures repeated from the slide that owns them, arrow captions restating the box beside them,
   and a duplicate opcode badge — never a caveat, a scope note or a warrant.
   ⚠ **ONE ORIENTATION PER SLIDE — columns are allowed, MIXING is not (Royce, 2026-08-30:
   "boleh 2 kolom, tapi pilih salah satu antara vertikal atau horizontal saja").** Two vertical
   stacks side by side is the banned shape, and the fix is to **split it into two slides**, not to
   cram it into one column: that is how "Cost in time" and "Settled by measurement" became four
   slides and **the deck is now 15, not 13** — so "the demos are slides 7 and 11" is stale, they
   are **7 and 12**. ⚠ Slides **7, 10 and 13 are exempt by name** (Royce, "jangan rusak"): the swap
   board, the two-venue comparison and the three-audience verdict are comparisons whose side-by-side
   arrangement IS the content. A blanket single-column rule broke all three and had to be reverted.
   ⚠ **Attribute beliefs to NO ONE on the evaluation slide** (2026-08-21): quotation marks
   manufactured a source (read as citing eprint 2020/845, which drops Dilithium's optimizations
   only *"in order to simplify the presentation"*), and the replacement `I assumed` was ALSO
   wrong — Royce never held those assumptions and the report says *measured*. The slide is
   question-form ("Does X? → measured answer"); reintroduce neither framing. ⚠ **SLIDE TEXT STYLE — visible AND spoken, all 13 slides (Royce, 2026-08-30).** No **em-dash**
   anywhere a viewer can see: colon, semicolon, `·` or a full stop instead (the minus in `z − ẑ`
   is `&#8722;`, not one — never sweep it). No `CITED` badge: sources are **author-year in text**
   (`Babbush et al. (2026)`, `(Valenta and Guthrie, 2026)`), each bound to its **own** claim, never
   one trailing parenthesis after two. **"exotic" / "multi-signature" / "threshold" appear nowhere
   visible or spoken** now the title names the adaptor signature — HTML design comments may keep
   them. ⚠ **That ban must not delete M10 #2's softening**, which now reads *"other advanced types
   are being built, the adaptor case much less so"*; deleting it reinstates the overclaim. Slide 10
   keeps **both** warrants and now says *why* they differ (a marker would otherwise ask): Bitcoin
   **projected** from measured LAS objects onto the wire format, Ethereum **measured** from a real
   client's receipt; the two-published-spends cross-check stays in BACKUP.
   ⚠ **In-shot type floor 13px** (Royce, 2026-08-21, "tulisan harus
   besar dan jelas" per the UoM template, whose own floor is 20pt): rehearsal-only chrome exempt;
   the gas chart left the evaluation slide because its axis type cannot meet the floor at 720p. Plan + beat sheet +
   on-camera claim discipline in `report/slides/VIDEO_PLAN.md`. It is **generated**:
   `scripts/gen_slides.py` fills `video_deck.template.html` from
   `report/latex/generated/*.tex` and embeds the report's own figures, so slides and report
   quote one evidence run (`--check` fails when stale; **edit the template, never the
   output**; re-run after every `sync_report.sh`). Bar geometry is derived from the same
   macros as the labels, so a width cannot drift from its number.
   ⚠ **UoM FORMAT LIVES IN THE HTML DECK — a .pptx conversion was built and REJECTED (Royce,
   2026-08-19).** `video_deck_uom.pptx` + `scripts/gen_slides_pptx.py` survive **only** as a
   fallback if a submission demands PowerPoint: never recorded, never edited, stale the moment
   the template changes. The chrome was skinned onto the deck instead (Arial, `--uom` #7800A2,
   `--uom-rule` #660066, the mark **reproduced** from `Master_169 presentation(2).pptx` via
   `assets/uom_logo.png` — recipe in that directory's `README.md`). ⚠ **Brand colour is CHROME
   ONLY** — base/adaptor/reused/warn are *semantic*, shared with the report's figures, and are
   never recoloured to a brand. ⚠ Overflow is fixed with the reusable `.tight` column knob —
   **scale a column, never cut a claim.**
   ⚠ **NO CAPTURES — NOT A TERMINAL, NOT A WINDOW SWITCH (Royce, 2026-08-13; a terminal dump
   was already rejected 2026-08-12 — it shows that a run happened, not what happened).**
   **Both demos step INSIDE the deck**: `data-sub="n"` makes a slide consume the forward key
   n times, driven by four attribute rules (`data-w`/`data-only`/`data-until`/`data-settle`)
   — a beat is markup, never per-slide script — and each slide **opens on beat 1**. Slide 7 =
   the swap board (abort gate → tripwire → publish σ₂ → Ext y′ **then** Adapt σ̂₁ and publish;
   never collapse those last two, Fig. 1 has both, and u₂ *does* publish — it needs nothing
   further **from u₁**); slide 11 = the node differential, drawn. `swap_console.html` stays as
   the standalone artefact **out of the recording**, banner kept, and its narration is
   **configuration-aware**: config 1 has no role-A π and its Ext is a secp256k1 recovery, so
   `classical:{}` overrides every LAS-specific line, and no byte figure may be attributed to
   its DLEQ (the pre-signature's excess covers nonce commitments *and* DLEQ, unsplit).
   ⚠ **Quote π at its MEASURED wire size** (`\cfgThreeMsgPi`), never `\piProofBytes` — the
   parameter set's own stated figure, a different quantity; both on one screen read as a
   contradiction. ⚠ `run_btc_las_node.sh` needs four env vars (`BTC_TAG`/`BTC_SRC`/
   `BTC_BIN_PATCHED`/`BTC_BIN_STOCK`) or it exits at once; working line in `VIDEO_PLAN.md`
   (build `~/btc-stage2`; pin gates re-verified 2026-08-12).
2. **Frontmatter `\TODO`s** in `report/latex/report.tex` (student id, prior degrees,
   acknowledgements) — **Royce only**.
3. **AMHL doc cleanup** in `docs/` under the (a)/(b) test above.

**⚠ PATCHED-CLIENT BENCHMARK — RUN 2026-08-08; Wang's Meeting-9 measurement ask is DONE.**
Write-up `docs/03-results/BTC_LAS_CONSENSUS_BENCHMARK.md`; bench
`bitcoin/las_consensus/bench_las_consensus.c`, runner `scripts/run_btc_las_bench.sh` →
`evidence/btc_las_bench/latest`, macros via `scripts/gen_btc_las_bench_data.py`. Times
`LASConsensusVerify` against BIP340 Schnorr and ECDSA per input. Corroborated: it lands
within a few percent of the Stage-1 harness's independent D3 packed `Verify`.
- **Four design rules, each cost a rejected draft — do not undo:** (1) **both sides start
  from serialized bytes**, parsing *inside* the timed call, or the wire codec is charged to
  LAS only; (2) **reject = valid signature + wrong 32-byte digest**, never a byte flip, which
  short-circuits; (3) **-O3 on both sides** — the secp objects are -O3 and the shim Makefile
  defaulted to -O2; (4) the runner **force-rebuilds** (`clean` + `-B`), since a flag-only
  change leaves a stale binary looking current.
- **⚠ Never gate on the reject/accept ratio.** Late-failing is established by the input
  *construction*, not by timing; a ratio near 1 is a consequence, not evidence for it.
- **⚠ Claim scope — four overclaims caught in review, do not reintroduce:** the baseline is a
  pinned **libsecp256k1-zkp** (a *fork*, NOT Core's vendored copy — never "Bitcoin Core's
  numbers"); say **"32-byte digest"**, never "BIP341 sighash" across baselines (P2WPKH is
  BIP143); Schnorr is the **comparison baseline**, not something the opcode is shown to
  *displace*; only **`\btcLasControls` mutations tried** were refused, not "every mutation".
  On rejection the *only* supported reading is that a late-failing signature costs ~an
  acceptance — **not** that invalid input buys an attacker no amplification.
- **Two security caveats stay SEPARATE:** (a) the consensus modification's security is
  unanalysed — a timing figure is not a safety argument; (b) the levels are unmatched —
  secp256k1 ≈ D2, the node runs D3, so the ratio **overstates** the rule's cost.
- **⚠ THROUGHPUT / SCALABILITY — the brief's deliverable, CLOSED BY DERIVATION, NO *NEW*
  EXPERIMENT** (Royce, 2026-08-19; the freeze held). The derivation still sources from this
  measured benchmark, so the MEASURED→DERIVED chain stays intact — never say "no experiment".
  **This adaptor-signature application has no signer-count axis** (a scoped claim, not one about
  adaptor constructions generally), so do not look for one. It exposes exactly **two** scaling
  dimensions. (1) **Validation work per input**, holding two *distinct* quantities: the measured
  latency and its reciprocal, the derived serial rate `\btcLasSerialRateK` ⇐
  `gen_btc_las_bench_data.py` — one quantity in two units, never counted as two results — and
  separately the curve ratio, which is a **comparison against the Schnorr/ECDSA baseline**, not
  another unit of that quantity. (2) **Ledger capacity**, the block-weight ceiling already in the
  body. The serial rate **is** a verification rate; it is **not** a network or transaction
  throughput, and **not** whole-node capacity in *either* direction (parallel script checking
  pushes up, per-input node work pushes down; net unmeasured). Never combine it with the
  block-weight ceiling to manufacture a tx/s prediction — they constrain different things. Quote
  no ±: a reciprocal of a mean is not a mean of rates. Read the generator header first.
- **It corrected the report twice:** body and `app:btcnode` both said the validation cost
  "is not measured". Fixed; only the *security* remains unmeasured.

**The seven Meeting-9 report fixes are DONE (2026-08-08)** — §1.4 Contributions; **the
succinct-proof conclusion made consistent with the numbers (M9 items 3–4)**: the future-work
bullet states the *numbers'* verdict — LNP22 wins on **size and time** — with the reason
(succinctness is asymptotic, this statement far too small), and **reports no LaBRADOR figures at
all**, because Wang ruled the un-refined direction stays *discussion without actual numbers*.
⚠️ Never "helpfully" add those figures back — **the appendix is part of the report**, so an
"appendix-only, fully caveated" LaBRADOR table is the same violation, and the appendix being
word-count-free is not a reason (a reviewing model proposed exactly this on 2026-08-11). Never
write "loses on every axis" either (only three axes were measured). ⚠️ §reflection-achieved names
**TWO** optimisations since 2026-08-19 (amortisation removed) and must NOT say both closed their
direction: statement **truncation** did — say *truncation*, not "compression", since the seed
candidate succeeds and leaks the witness — while the succinct-PQ one is closed **on cost, at this
statement size, through our encoding** — the same section's own bullet says what is still open,
and the two must agree. ⚠️ eprint 2020/845 asks of π **knowledge of a short witness to Y** (§4.1)
and notes π is costly in communication but adds no *on-chain storage* cost; it does **not** require
succinctness (that was this project's own goal), and "NIZK" is its wording in the **PCN** section,
not §4.1 — never attribute succinctness or the §4.1 ZK label to the paper. LaBRADOR size is labelled the library's estimate;
proof-size range explained in `tab:stage2-comm`'s caption (LaZer Huffman-codes the Gaussian
responses, so length follows the sampled values); `tab:classical`/`tab:onchain` captions cut
with the mechanism moved to `app:methoddetail` (each fell from a **full page to ~68%**, not
Wang's ≤half — the floor is the mandatory EIP-7623 / equivalence / scope / measured-vs-derived
caveats plus the table bodies; going lower means deleting a caveat, which needs Royce's call); SegWit/Taproot cited as the original-vs-improved
contrast (SegWit fixes the *fee* treatment, **not** the 520 B push limit — do not re-widen that);
the node experiment's result + conclusion now in §res-txstruct with detail left in `app:btcnode`;
negative results in §reflection-achieved. **Paid for by moving the two-page `tab:challenges`
longtable to `app:challenges`** — hand-written tabular *bodies* count toward the word budget,
so that move both satisfied Wang's oversized-table ruling and recovered 449 words; back to
**8,999**. Do not move that table back into Ch. 4.

**Done — do not re-queue** (and **Meeting 9 accepted all of it**): the two Chapter 5 future-work
bullets those experiments answered (statement compression, proof reduction) are **rewritten as
measured verdicts** in `05-conclusion.tex` — do not restore them as open questions; the ML-DSA
hint result is folded into Ch. 4 (`04-evaluation.tex` §"Reference optimisations appear to fight
the adaptor identity" records the corrected "sufficient route, not a necessary one" claim); the
`q≈2^24` bullet is gone from Ch. 5; the `Adapt`-vs-ECDSA gap is explained (`03-results.tex:322`,
detail in `docs/03-results/LAS-08-performance-measured.md`); every experiment listed in Status is
RUN with evidence and a write-up. **Meeting 8 is fully satisfied (2026-08-07):** transaction
breakdown + diagrams, terminology split, Chapter 5 title, functions-not-protocols, and the figure
rulings — `fig:lasfuncs` (four functions side by side, Ch. 2), `fig:swapflow` (arrows = the rows
of `tab:stage2-comm`, Ch. 3), and **no figure in the appendix** (`fig:overhead` moved into
§res-compute). Never move a chart back to the appendix.

### ⚠️ WORK-PRIORITY RULE — nothing here is "optional" (Royce, 2026-08-03)

**Every remaining item is mandatory; they differ only in ORDER.** Never label anything
"optional", "if time permits", "stretch" or "bonus" — in this file, the docs, or the report's
future-work section. Rank by: (1) what Wang instructed or prioritised; (2) low-hanging fruit,
so the queue keeps shrinking; (3) novelty value to the report; (4) feasibility in the time
left. Current derived order = the numbered list above (re-derive it whenever Wang speaks
again). **Never start a new measurement on your own initiative** — Meeting 8's feature freeze
is the default, and every experiment recorded in Status past that freeze exists because Royce
overrode it explicitly. Those stand; a new override must also come from him. When in doubt
about order, ask Royce rather than silently reordering.

## Supervisor rulings in force

Spec: `las-context-consolidated.md` (**§15A = M6**, §16 = M7, §17 = M8, §18 = M9, §19 = M10, **§19A = M11**, **§19B = M12**).
Transcripts: `meetingN_cleaned_transcript.md` (+ `meeting8_summary.md`). **Read §16 before
planning application work.** ⚠ **M6 was merged into the spec 2026-08-15** (Royce) — it is
*lettered* 15A, not numbered, so §16–§19 keep the numbers cited across CLAUDE.md and `docs/`;
its per-item delivery **status matrix** stays in
`docs/04-evaluation/SUPERVISOR_DELIVERABLES_GAP.md` §1. Supersedes the old "meeting 6 is not in
that file" warning — the spec now covers every meeting.

**Meeting 12 (2026-08-27, date inferred from file metadata + the meeting's own date arithmetic
— FULL video rehearsal, then the report's introduction) — LATEST WORD.** Transcript
`meeting12_cleaned_transcript.md`. ✅ Source is a **diarised Teams export**, so unlike M11 these
are attributable; the ASR still garbles every domain term (`lattice`→"latest",
`PreSign`→"Resign") — check its §A before quoting. **Video CONTENT ACCEPTED, LENGTH REJECTED**;
earlier rounds discharged ("you have addressed most of the questions I mentioned before").
⚠ **RECORD THE STRENGTH, NOT JUST THE WORDS: an imperative ("just remove X") is a ruling; a
hedged "maybe you could…" is a recommendation.** A first draft of this block promoted three
hedges into bans and had to be corrected — that is the M12 lesson, and it applies to every
meeting block here.
- ⚠ **THE VIDEO MUST FINISH UNDER 8:00 — blocking.** First end-to-end delivery ran **≈10 min
  40 s** (00:31→≈11:10). He licensed **either** route — "be faster, **or** maybe remove some
  details" — so the route is Royce's; only the finished length is mandated. Cut order in
  `VIDEO_PLAN.md` §1. ⚠ This is the measured confirmation of the `data-time` rule above.
- 💡 **Visualisation is a trade** — "if you would like to add more visualisations, maybe you
  should remove some content"; his test is a non-specialist facing the three dated quantum
  estimates: *"which one is the most important one?"*
- ⚠ **TITLE: drop "exotic signature schemes" and name the adaptor signature** ("just remove…
  by referring to the adaptor signature") — this **supersedes the "confirmed by supervisor"
  comment at `report/latex/report.tex:115`**, which he reopened ("now you have the freedom").
  Avoiding **"lattice-based"** is a *recommendation*, not a bar ("maybe… too specific, I don't
  know"). Agreed aloud: *"…Post-Quantum Secure Adaptor Signature Scheme[s] in Blockchains"*.
- ⚠ **`fig:whynow` (a) must argue from the CHAINS, not RSA-2048** — "not that straightforward,
  not that direct"; make it "much closer to Bitcoin, or to blockchain itself", from cited work
  on when quantum computers become realistic vs the security of *current* blockchains. He
  invited **replacement**, so the RSA rows are not protected. ⚠ What still binds is **this
  project's own rule, not his**: each estimate stays at its own target and the 2019 RSA figure
  is never divided by the 2026 secp256k1 one.
- ✅ **ADAPTOR MOTIVATION IN §1 — BOTH HALVES NOW DONE (b closed 2026-08-28).** It was "one or
  three sentences… very abstract"; owed were (a) why adaptor signatures matter to blockchains,
  concretely, and (b) **why the adaptor among the exotic families**. (a) was expanded 2026-08-27
  (atomic swap + `fig:swapidea` + the HTLC contrast). (b) was still one sentence arguing from
  *applications + gap* and never from what makes an adaptor **functionally different**, which is
  the question Wang actually asked ("a lot of types of exotic signatures… why did you choose
  adaptor signature?"); §1.1 now contrasts it against multi-/threshold/aggregate and names the
  distinguishing function — it **links two separate settlements**. ⚠️ **Two traps hit while
  drafting it, both caught in review:** aggregate signatures **compress** signatures, they do not
  change *who authorises* (do not lump the three together); and "publishing one signature releases
  the secret" is the **publishes-the-witness error again** — `Ext` needs the pre-signature, so say
  *a party holding the matching pre-signature extracts it*. Also restore hedges when rewriting:
  "**Common** classical adaptor constructions", not all. **No Background section ⇒ background goes
  IN the introduction** (rubric §2.1, endorsed; depth over breadth stands) — M12 item 6, *still
  open*, and headroom is now thin.
- ✅ **"PQ migration is not only blockchains" — ASKED IN M11 *AND* M12, never done, DONE
  2026-08-27.** ⚠️ **A suggestion repeated across two meetings is not a hedge: the repetition is
  the evidence it was never applied** ("*Have you mentioned this?*"). Both artefacts carried only
  an unattributed, **uncited** "being adopted now" — Wang's ask and an EVIDENCE-OR-SILENCE defect
  at once. Fixed inside `fig:whynow` (TikZ body + caption ⇒ **zero words**) and deck slide 4,
  with `westerbaan2025pqinternet` / `cloudflare2026pqroadmap` / `valenta2026pqauth`.
  ⚠️ **Never widen it:** big platforms migrated **key agreement** first; the cited signature
  deployment is **ML-DSA authentication on Cloudflare-to-origin connections (July 2026)**, NOT
  the public web PKI — no public PQ certificate was in use as of October 2025.
  ⚠️ **ALL THREE REFS ARE CLOUDFLARE PUBLICATIONS, so the warrant supports ONE NAMED migrator,
  never a plural** — "major internet platforms / many big companies are migrating" asserts
  independent companies none of them evidences. Say **"Cloudflare is already migrating its
  infrastructure"**. Caught 2026-08-30 in *both* halves of one drafting pass (slide text and
  narration), which is the grep-the-deck-AND-the-notes lesson again.
  ⚠️ **The 2026-08-27 pass was slide furniture only — corrected 2026-08-30.** It put the claim
  in two `.ts`/`.stkarrow` captions and left the SPOKEN script silent, while Wang's channel was
  speech both times ("you should **say**", "**Have you mentioned** this?"). Slide 4 now carries
  it aloud. The old caption also asserted "**governments** and major networks" with no source at
  all — deleted, EVIDENCE-OR-SILENCE. **Deck-only by decision** (ChatGPT review 2026-08-30,
  verified): both askings were over the deck, and at the report review Wang graded the PQ
  motivation *"you have already done a good job"* — so the report is NOT edited for this, and
  `westerbaan2025pqinternet` / `cloudflare2026pqroadmap` stay **cited nowhere**, i.e. absent from
  the bibliography, until Royce says otherwise.
- ✅ Accepted as they stand: structure, ≥5 objectives, contributions, critical reflection, 28
  citations, the Ch.3/Ch.4 split; only subsection *titles* may need a tweak. **13 slides counted
  by Wang himself** — closes M11's "you have 10 slides" question.
- ⚠ **WRITTEN Overleaf comments have started arriving (first: 2026-08-31, "also need to cite some
  isogeny-based adaptor signatures").** DONE in §1.2 — `tairi2021post` (Tairi, Moreno-Sanchez,
  Maffei, FC 2021: IAS on CSI-FiSh, proved secure in QROM, **implemented and evaluated**, verified
  in `2020-1345.pdf`). ⚠ It sits **after** "Their work is primarily theoretical", which judges
  Esgin et al.\ alone — IAS is not theory-only, so never let that sentence drift onto it.
- 📅 Report+slides+video to Wang **Fri 2026-08-28 17:00**; he reads Sun/Mon; optional meeting
  **Thu 2026-09-03**; submission **Fri 2026-09-04**.
**Nothing in M12 authorises a new experiment; the M10 freeze stands.**

**Meeting 11 (2026-08-21, date inferred from file metadata — deck review, second pass).**
⚠ **Single ASR source, NO
diarisation** — every speaker label is inferred from content; §D forbids promoting anything to
a ruling on attribution alone, and Royce confirmed the items by instructing they be fulfilled.
Most of it confirms the second-batch instructions already recorded above (quantum-first
motivation, verdict + per-audience implications, both venues' transaction structures, maths-free
swap, real logos/prices — all applied). Genuinely new, all applied 2026-08-21:
- ⚠ **THE COINS DO NOT MOVE BETWEEN CHAINS** (laboured twice): every swap picture must draw
  each payment settling ON its own chain — tx boxes anchored to their ledgers, the shared
  secret the only element crossing. Deck slide 3 redrawn; `fig:swapidea` caption now says it.
  Never reintroduce a card-to-card coin arrow.
- ⚠ **Venue vocabulary is Wang's**: contracts **"more flexible"**, Bitcoin **"more restricted,
  because [its fields] cannot be modified"** — he called the prior framing "a bit misleading".
  Complements (does not replace) "consensus rule, not engineering" and the no-timeline rule.
- ⚠ **Scope wording: the measured swap is UTXO-with-UTXO** — *"safer to just talk about
  UTXO"*; the EVM work may still be reported. **ETH↔USDC is never an example**. BTC↔ETH is
  *doable* ("the only different thing is the verification") and stays the motivating scenario
  only — no artefact may imply it was run. DEMO A now says the scope aloud.
- ✅ **TPS/THROUGHPUT — RESOLVED by Royce 2026-08-21 as "satisfy the intent"** (spec §19A.3): the
  deck quotes **Bitcoin's block-WEIGHT capacity bound only**, labelled a size bound, **never ÷ the
  block interval** and **never an EVM per-block figure** (that is the retracted EIP-7825 reading).
  **No TPS number exists anywhere in the deck, and none may be added.**
**Nothing in Meeting 11 authorises a new experiment; the M10 freeze stands.** Detail: §19A.

**Meeting 10 (date NOT confirmed — 2026-08-14 or -15; never cite a firm one).
The mock presentation was delivered and reviewed: CONTENT ACCEPTED, PRESENTATION NOT, and the
feature freeze reaffirmed in the strongest terms yet.** Transcript
`meeting10_cleaned_transcript.md`, consolidated from two ASR sources — Samsung (diarised, whole
meeting) and Teams (better words but **no diarisation at all**: every line, Wang's included, is
stamped "Royce Steven"). Rulings, each mandatory:
- ⚠ **"You don't need to do new things … now you should make sure all of them are correct, they
  are precise."** Implementation FROZEN with Wang's agreement; remaining time goes to
  **verifying existing results**, the writing and the video. "More than two weeks" stated.
- ⚠ **Deck: 13 slides → ~10, and restructure** — application motivation (atomic swap, payment
  channel, why post-quantum) *before* any technical detail; then a high-level **picture of the
  method**; results; takeaways; then back to the opening questions. Assume an examiner who is
  **not** a blockchain/crypto specialist. ~30 s/slide against 6–8 min. Visuals over text.
- ⚠ **Frame the project as PRACTICAL LAS, not "an implementation of the paper"** — do not limit
  scope to eprint 2020/845; the ML-DSA attempt is a result to report, and benchmarking a full
  ML-DSA build is future work. **Overrides any "reproduce the paper" framing.**
- ⚠ **Soften the exotic-signature motivation claim** — not "no implementations" but "not *all*
  exotic ones implemented"; multi-signatures are actively being implemented. Deck AND report.
- ⚠ **Royce said on record that "D5 needs more optimisation to fit in one transaction."** It is
  unevidenced (D3-only parameters); delete it from the deck, "not evaluated" is the only
  supported wording. Third instance of this exact trap → EVIDENCE-OR-SILENCE.
- ⚠ **Methodology is the weak point** — Royce's own diagnosis, Wang endorsed: *"it's not only
  just the results, but how you get the results."*
- Report: **name the paper explicitly**, never "the paper"; put `SampleInBall` in the challenge
  figure (→ the `c`/`c̃` rule); shorten an over-long caption *into* the body when the paragraph
  beside it is short; keep **LaTeX** tables (never Excel images — an image cannot be edited in
  place), draw.io/PowerPoint → PDF for decorative diagrams; notation table stays in the
  appendix. Add a simpler, colourful swap diagram **before** Fig. 3.5 (`fig:swapflow`), where
  the application is introduced. ⚠ **Fig. 3.5 itself is ACCEPTED, caption included** — the
  friendly diagram is an addition, not a replacement. **Also accepted as they stand:** the
  modified Criterion figure, the Bitcoin structure figure. GitHub link **not** required.
  Chapter-header length: no firm ruling. From ≈28:57 the meeting is reviewing the **report**,
  not the deck; do not re-file those comments as slide feedback.
  ⚠ **Party naming, settled 2026-08-16 — do not "fix" either way.** Deck **slides 3 and 7** use
  **Alice / Bob** outright (numbering shifted when the quantum slide went in — M10's "slide 5" and
  the older "slide 2" no longer locate it; verified 2026-08-22). The report keeps `u₁`/`u₂` for mathematics, and the new intro figure
  `fig:swapidea` names them **once** as "Alice $(u_1)$" / "Bob $(u_2)$" so the friendly picture
  also teaches the notation the chapter then uses. Alice/Bob is standard crypto convention
  (RSA 1978 onward), so it is not an informality problem at MSc level; the risk was
  *inconsistency*, which the joint labelling removes.

**Meeting 9 (2026-08-07) — superseded as latest word by Meeting 10; every ruling DISCHARGED.**
Transcript `meeting9_cleaned_transcript.md` (single ASR source; its §A lists what could not be
resolved). **Everything Royce reported was accepted**, including the Meeting-8 transaction
breakdown (*"now it's much clearer"*) and the figures (*"much better than I thought"*). Its
rulings, all done — see "Done — do not re-queue" and the patched-client benchmark block:
benchmark the patched Bitcoin client (*"if we achieved a better security — so what have I
lost?"*, the only measurement asked for); §1.4 Contributions; conclusions consistent with the
numbers, with the un-refined direction kept as discussion **without** its numbers; explain the
proof-size range; shrink the oversized table; cite SegWit/Taproot; Bitcoin experiment results
in the body with detail in the appendix; the failed statement-Y compression written up as a
negative result in the critical reflection. **Bounded, NOT a directive:** another hash function
against the SHAKE-dominated gas — Wang bounded it himself (*"we have one [working] version;
even if it's not very efficient, it's still acceptable"*).
- ⚠ **A garbled line at ≈38:44** (cleaned file §17) sounds like on-chain verification failing at
  another Dilithium level. Not evidence — D2/D5 were never evaluated. → EVIDENCE-OR-SILENCE.

**Meeting 8 (2026-07-31).** *"You don't need to contain all the stuff — you just
need to make sure that what you have done looks good, looks perfect, looks great."*
- **Results accepted; stop measuring** (*"it's what we expected"*) — the three-configuration
  findings live in Status. Configuration 1 legitimately carries no π (the classical protocol
  specifies only DLEQ). **Scope frozen:** no second signature scheme, no zkVM/RISC-V, no
  functional signatures, no live-network deployment — polish what exists.
- **Terminology rulings, report-wide, every occurrence:** (1) **"transaction" must not mean
  "the signed message"** — in a Bitcoin context it names a predefined format, so use a
  different term for the signed message; (2) **PreSign / PreVerify / Adapt / Ext are
  `functions`, not `protocols`** ("protocol" implies consensus-level design).
- **Report:** word count need **not** be proportional to rubric weighting — more about
  results, less about background. Figure placement rules are in the FOCUS section. Chapter 5
  is titled "Conclusion, critical reflection and future work".
- **Sequencing:** finish Bitcoin/UTXO before any further EVM/Naysayer work; the EVM is a
  discussion of a more advanced solution *after* a complete Bitcoin solution.
- **All of it is satisfied and M9-accepted** — see "Done — do not re-queue" above. Its one
  outstanding deliverable, the 6–8 minute deck, is now M9's item 1 (delivered live next week).

**Meeting 7 (2026-07-24) — Stage 2 retargets from the EVM to Bitcoin/UTXO.**
- Target chain is **Bitcoin / a UTXO chain**: native on-chain LAS verification is infeasible
  against the gas limit, and adaptor signatures are used in practice for UTXO swaps; Bitcoin
  has fees rather than a gas limit and the heavy work stays off-chain.
- **The EVM work is NOT retracted** — the measured native verifier and Naysayer variant are
  retained as *the evidence for why* the venue was chosen.
- **Metrics: gas → time + communication cost**, counting off-chain messages, plus the
  usability finding (heavy pre-transaction computation may need a dedicated PC, not a phone).
- **Permitted simplifications:** no real sockets; π off-chain; refund/timeout are edge cases
  (honest path first); packing in the swap path optional. Exploration/demo, not a product.
- Its report rulings are applied and its deliverable met; the rejection figure stays a
  **cumulative acceptance curve**, not P(exactly k attempts).

## Scope discipline (Meeting 2 onward)

- Target the **Simplified Dilithium-III** set (LAS code is mode-independent; also built under
  modes 2/5 for portability).
- **Do NOT implement or analyse security proofs.** Implement + benchmark + demo only.
- **Two-stage spine:** Stage 1 = standalone LAS + benchmarks ✅; Stage 2 = blockchain
  application ✅ (Bitcoin/UTXO).
- **Two benchmark baselines required:** (i) LAS vs the base signature ✅; (ii) LAS vs a
  **classical adaptor signature** ✅.
- **⚠️ Parameter authority is NIST FIPS 204 (ML-DSA), NOT the LAS paper** (Royce, 2026-08-03;
  **overrides** every earlier "migrate to the paper's parameters" note, including
  `las-context-consolidated.md` §4/§5). The build's `q = 8380417 ≈ 2²³` **is ML-DSA's modulus
  and is therefore correct**, not a compromise: the paper's `q ≈ 2²⁴` would need a new NTT
  table or schoolbook multiplication, and `Q > 2γ` so correctness holds — only the concrete
  MSIS/MLWE margin differs (analysis out of scope). **The `q≈2²⁴` migration is DROPPED, not
  deferred** — it must not reappear in the work queue or in future-work lists. Where paper and
  FIPS 204 differ on a *parameter*, follow FIPS 204 and say so. (This does not touch the
  notation rule: report *mathematics* still uses the paper's symbols.)
  ⚠️ **NEVER cite the paper as PERMITTING it** (`tab:params`' caption said "which the paper
  permits — only the *size* of `q` matters" until 2026-09-01; relayed critique RIGHT).
  `2020-845.md:408` sets `q ≈ 2²⁴` **in order to meet** the M-SIS/M-LWE requirements and only
  *then* says the concrete value may be chosen for a fast NTT — **a licence at a FIXED SIZE,
  which does not cover changing the size**. Supported form, already used by `sec:impl` and
  `sec:res-validity`: FIPS 204's `q≈2²³` rather than the paper's `q≈2²⁴`, correctness
  unaffected (`q>2γ`), security margin changed.
- **⚠️ The "hint optimisation" means building LAS on real NIST ML-DSA** (Royce, 2026-08-03) —
  **not** "shrinking statement `Y`", which is how Meeting 8 phrased it. Its purpose was
  evidential: turn an asserted design claim into a demonstration. **Done, and it corrected the
  claim** — see the ML-DSA block in Status; do not restate the superseded version, and do not
  re-open it as a size optimisation.
- **ML-DSA alignment — required framing, do not drift:** the target set *aligns the reusable
  ML-DSA primitives and challenge-digest strength with ML-DSA-65, while retaining LAS-specific
  distributions, bounds, exact relations, and unoptimised serialization*. Do **not** claim LAS
  *is* ML-DSA-65 or inherits its security category; do **not** claim eprint 2020/845 specifies
  a FIPS-style 256-bit `c_tilde` (it defines `H : {0,1}* → C` only). `fair_paper` is a separate
  historical reproduction set, appendix-only; main benchmarks are the D2/D3/D5-aligned sets
  with D3 as headline.
- **Focus: LAS only** — no alternative-PQ-scheme comparison, per Royce.
- **Hard out-of-scope (supervisor):** Ethereum-consensus multisigs, blind/group signatures,
  heavy ZKP/MPC — one related-work paragraph max. **EXCEPTION (Royce-directed): the Fig. 1
  proof of knowledge π IS implemented** via vendored LaZer; do not re-flag it as out-of-scope.

## ⚠️ CANONICAL NAMING CONVENTION — Rust ⇄ C ⇄ paper (seven-type layout; DO NOT DEVIATE)

The Rust port (`rust/fips204-las/src/`) is **DONE, KAT-PROVEN, and is the AUTHORITY**; the C
mirror (`ref/`) reproduces these names **exactly**. Never invent a variant; if a name looks
wrong, fix it against `docs/paper/LAS_2020_845_NOTATION.md`, not from memory.

**Construction parameters** (paper Section 3 / Table 1):

| paper | Rust (setup.rs) | C (setup.h) | value @ target set |
|---|---|---|---|
| n (module rank) | `N` | `LAS_N` (`-DLAS_N=` sweepable) | 6 |
| ℓ | `ELL` | `ELL` (`-DELL=`) | 5 |
| n+ℓ | `N_PLUS_ELL` | `N_PLUS_ELL` | 11 |
| d (ring degree) | `D` | `LAS_D` (`#define LAS_D N`, params.h N=256) | 256 |
| κ | `KAPPA` | `KAPPA` (`-DKAPPA=`) | 49 |
| γ = κ·d·(n+ℓ) | `GAMMA` | `GAMMA` | — |
| seed length | `LAS_SEEDBYTES` | `LAS_SEEDBYTES` | 32 |

`ref/setup.h`'s *defaults* are the historical paper set (4,4,60); the target set is selected by
`-DLAS_N=6 -DELL=5 -DKAPPA=49` from the Makefile. **Never read the current parameter set off
those defaults.** C-only divergence: **only** `LAS_N` and `LAS_D` keep the `LAS_` prefix
(params.h already owns bare `N`=256 and `D`=13 for the reused primitives). Ring-degree loops in
LAS files read `LAS_D`; reused Dilithium files keep bare `N`.

**Challenge-digest width `LAS_CTILDEBYTES`** is keyed on **(LAS_N, ELL, KAPPA) together** —
dimensions alone cannot separate the D2-aligned set from the paper set, which share (4,4) —
with `#error`/`panic!` on any unrecognised set. FIPS 204 §7.3 Alg. 29 takes a seed in `B^{λ/4}`:

| (n, ℓ, κ) | aligns with | `c_tilde` |
|---|---|---|
| (4, 4, 39) | ML-DSA-44 | 32 |
| (6, 5, 49) | ML-DSA-65 | **48** ← project target |
| (8, 7, 60) | ML-DSA-87 | 64 |
| (4, 4, 60) | none — historical paper reproduction | 32 |

It is **not** keyed on `DILITHIUM_MODE` (the Makefile picks the mode only to satisfy params.h).
`c_tilde` buffers must be sized `LAS_CTILDEBYTES`, never `LAS_SEEDBYTES` — at 48 that mismatch
is a 16-byte overread. PRG-seed sites stay at `LAS_SEEDBYTES`.

⚠️ **The LAS layer is SIX same-named modules in both languages — `setup · las_types · relation ·
basesig · las · serialize`.** Enumerating it as five (dropping `las_types`) is an error already
made once in `fig:repostructure`; Rust declares `pub mod las_types` beside the rest and C has
`ref/las_types.h` (header-only — no `.c`). `relation_zk` is a seventh same-named pair but is the
opt-in π module, deliberately outside the core layer.

**The seven semantic types** (six object types in las_types.rs / las_types.h; `public_params`
stays in setup.rs / setup.h; each owned by one layer):

| paper object | Rust type {fields} | C type {fields} | owner |
|---|---|---|---|
| pp = A = [I\|A'] | `PublicParams { a_prime, seed }` | `public_params { a_prime, seed }` | setup |
| pk = t | `PublicKey { t }` | `public_key { t }` | basesig |
| sk = r | `SecretKey { r }` | `secret_key { r }` | basesig |
| σ = (c, z) | `Signature { c_tilde, z }` | `signature { c_tilde, z }` | basesig |
| σ̂ = (c, ẑ) | `PreSignature { c_tilde, z_hat }` | `pre_signature { c_tilde, z_hat }` | las |
| Y = t' | `Statement([R;N])`, `as_t_prime()` | `statement { t_prime }` | relation |
| y / s (witness) | `Witness([R;N_PLUS_ELL])`, `as_relation_vector()`/`from_relation_vector()` | `witness { value }` | relation |

`witness.value` is NEUTRAL storage: Gen's honest ternary r′ AND Ext's extracted s (relation
R′_A, may exceed norm 1). A statement is pk-shaped but is NOT a public_key; a pre_signature is
sig-shaped but NOT a signature — **never cast or alias**.

**Rejection bounds** (each owned by its scheme, NOT in setup):

| Rust | C | value |
|---|---|---|
| `basesig::BOUND_SIGN` | `BOUND_SIGN` (basesig.h) | γ−κ+1 |
| `las::BOUND_PRESIGN` | `BOUND_PRESIGN` (las.h) | γ−κ |
| — (dropped in Rust) | `BOUND_PRESIGN_K(K)` (las.h) | γ−κ−K+1 — legacy AMHL hook, unused; AMHL is dropped |

**Public function names** (paper/upstream ⇄ Rust ⇄ C):
- setup: `setup_public_params` (both).
- relation (Gen): `gen`/`gen_seed` ⇄ `relation_gen`/`relation_gen_seed`.
- relation_zk (π): `relation_zk::prove`/`relation_zk::proof_verify` ⇄
  `relation_prove`/`relation_proof_verify` (relation_zk.c). Bridge (C-only TU, FFI'd by Rust
  build.rs): `relation_zk_lin_prove`/`relation_zk_lin_verify` (relation_zk_lazer.c).
  **Gate names — NEVER rename:** `PI_ROWS`/`PI_COLS`/`PI_DEG`/`PI_PROOF_MAX_BYTES` (both
  languages), params symbol `las_pi_params` (relation_zk_params.h ⇐ scripts/las_pi_params.py),
  cargo feature `relation-zk`. The batched module's `PI_BATCH_*` names **do not rename or alias
  these** — the unprefixed ones stay the k=1 module's.
- basesig (Algorithm 1, Σ): `keygen`⇄`base_keygen`, `keygen_seed`⇄`base_keygen_seed`,
  `sign_internal`⇄`base_sign_internal`, `sign`⇄`base_sign`, `sign_det`⇄`base_sign_det`,
  `verify_internal`⇄`base_verify_internal`, `verify`⇄`base_verify`; packed
  `keygen_packed`/`sign_packed`/`verify_packed` ⇄ `base_keygen_packed`/`base_sign_packed`/
  `base_verify_packed`.
- las (Algorithm 2): `presign_internal`⇄`las_presign_internal`, `presign`⇄`las_presign`,
  `presign_det`⇄`las_presign_det`, `preverify_internal`⇄`las_preverify_internal`,
  `preverify`⇄`las_preverify`, `adapt`⇄`las_adapt`, `ext`⇄`las_ext`; packed
  `las_presign_packed`/`las_preverify_packed`/`las_adapt_packed`/`las_ext_packed`.
  (Algorithm 1 lives only in basesig — never in las.c.)
- **Gate names — NEVER rename:** `LAS_ATTEMPTS`/`las_attempts`, `BASE_ATTEMPTS`/`base_attempts`,
  `las_expected_attempts`, `LAS_SEEDBYTES`, `LAS_CTILDEBYTES`, serialize bit-widths
  `LAS_{PK,SK,Z}_COEFF_BITS` (`LAS_C_COEFF_BITS` is DELETED — the challenge is no longer
  bit-packed), `LAS_Z_OFFSET`, `LAS_Z_MAX`.

**Serialize sizes** (semantic `*_BYTES`, six typed pack/unpack pairs over 3 encoders):
`PUBLIC_KEY_BYTES`, `SECRET_KEY_BYTES`, `SIGNATURE_BYTES`, `STATEMENT_BYTES`=pk,
`WITNESS_BYTES`=sk, `PRE_SIGNATURE_BYTES`=sig. Pairs: `pack_/unpack_` ×
{`public_key`,`statement`,`secret_key`,`witness`,`signature`,`pre_signature`}. Byte values for
every set are `_Static_assert`-anchored in `ref/serialize.h` and mirrored by
`expected_wire_sizes()` in Rust; the live block prints them — **never retype a size from memory
or from an older document.**

**Locked local conventions** (bit-for-bit): vector split `x_0`/`x_1`; NTT-operand suffixes
`_hat`/`_mont`; products `c_r`/`c_t`; mask counter `mask_nonce`; recomputed challenge digest
`c_tilde_check` (verify byte-compares digests); commitments `w_prime`/`w_plus_t_prime`; object
vars `sigma`/`sigma_hat`/`statement`/`witness`; challenge sampler named after its upstream twin
`sample_in_ball` (the `las_` prefix is reserved for the four Algorithm-2 public ops, not private
helpers).

## LAS paper source-of-truth rule

Before editing LAS labels, benchmark plots, report text, **report equations or symbol/notation
tables**, API documentation, README explanations, or comments explaining the mechanism, read
**FIRST**: `docs/paper/LAS_2020_845_NOTATION.md` — the repo's curated guide derived from
`2020-845.pdf`. If the guide conflicts with the PDF, **the PDF wins**; if a detail is absent
from the guide, check the PDF. Do not invent notation, do not rename variables casually, and
mark uncertain details TODO rather than guessing.

**Report mathematics is governed by §3/§6/§7 of that guide, and the PAPER (not the code) is the
notation authority — never adopt a code symbol for report maths, and never justify report
notation by what the C code names a variable.** Settled forms, to be kept consistent across
`report/latex/` (fix *every* occurrence together — body equations, `tab:params`, `tab:notation`,
captions, figure labels — never one in isolation):
- ring degree: the paper's **`d`** (this build runs at `d = 256`); do not substitute the code's `N`.
- rejection/masking bound: **`γ = κ·d·(n+ℓ)`** (paper form). `M = n+ℓ` may appear only as a
  table-column shorthand, never in equations.
- challenge weight **`κ` is per parameter set** (60 / 39 / 49 / 60 for paper/D2/D3/D5) — never
  hard-code `κ=60` as a global.
- **M-SIS is the KERNEL problem, so finding a short preimage of `Y` is NOT "the M-SIS problem"**
  (2026-09-01, relayed critique RIGHT). Definition 1 asks for a short **non-zero** `v` with
  `Av = 0`; the hardness of the relation runs through the paper's own **augmented** reduction
  (`2020-845.md:226`) — `[A ∥ t]·(r,−1)ᵀ = 0`, hence its instance is `M-SIS_{n,n+ℓ+1}`, one
  column wider than `A`. Report form: a sufficiently short preimage **yields an M-SIS solution
  `(r',−1)ᵀ` for `[A | Y]`**. ⚠️ `sufficiently short` carries the norm bound `β` — dropping it
  widens the claim. The two accompanying suggestions were DECLINED and the reasons hold for the
  next such batch: (a) rewriting `Gen` as "a separate interface" was argued **from C comments**,
  which this section bars — the paper says *Gen runs exactly as KeyGen* and `relation.c:5` cites
  that same line; (b) the "missing" validation steps are **correct** (Alg. 2 steps 11/21/31, and
  `las.c:532` / `las_ext`) but Adapt-runs-PreVerify and Ext's `A·s=Y` check are **already** in
  the report (`02-methodology.tex:319`, `03-results.tex:110-111`, `05-conclusion.tex:125`), so
  adding them to §2.1 buys duplication at a nil word headroom. **Grep the whole report before
  accepting a "detail is missing" critique, not only before dismissing one.** Genuinely absent,
  and Royce's call at ~4 words: PreVerify's own `‖ẑ‖∞ ≤ γ−κ−1` gate.
- **`c` vs `c̃` — two objects; never merge them, never let `H` change type** (four drafts failed
  this on 2026-08-13). The paper's `H : {0,1}* → C` returns the challenge **polynomial** `c`;
  `c̃` is the digest **this implementation** derives it from, following FIPS 204's *pattern*
  `c = SampleInBall(c̃)`. Never write `c̃ = H(…)` — it silently retypes `H`. Attribution is
  three-way and easy to get wrong in *either* direction: FIPS 204 **does** fix ML-DSA's width
  at `λ/4`, the LAS paper fixes **no** encoding, and the *project's* choice is to adopt λ/4 at
  aligned sets and 32 B at the unaligned paper set — so FIPS 204 never "instantiates the paper's
  `H`", but neither is the width simply "a project choice". Wire objects are `(c̃, z)` / `(c̃, ẑ)`
  while arithmetic uses `c`, **derived before first use** (`las_preverify` does exactly this).
  Report-wide 2026-08-13: `fig:lasfuncs`, `fig:flow`, `tab:notation`, both component tables.
  ⚠ **A figure must show `SampleInBall` itself, not leave the link to the prose** (Wang,
  Meeting 10): he read the figure, saw `c`, and asked *"how did we get the `c`?"* — the two
  symbols side by side with no function between them read as a typo, not a derivation.
  ⚠ **Only VERIFICATION re-derives `c` — "every consumer re-derives it" is the absolute-word
  defect** (relayed critique RIGHT, 2026-09-01; fixed in `02-methodology.tex:200` **and**
  `app:serialize`, which carried the same absolute plus "at every decode" — the codec derives
  nothing). `las_ext` **never** derives `c`: it subtracts and checks `A·s=Y`. `las_adapt`
  derives it only through the `las_preverify` it runs (`las.c:532`), never itself.
- **`y` (mask) / `r'` (honest witness) / `s` (extracted) — THREE objects Algorithm 2 names,
  and the same trap as `c`/`c̃`** (Royce caught it 2026-08-21). The paper overloads `y`: the
  mask sampled at Step 2, *and* the pair component of `(Y,y)` — which Adapt parses `r' := y`
  (Step 24), Ext returning `s = z − ẑ` (Step 30). Report maths follows that: `y` **only** the
  mask, `r'` wherever the witness enters arithmetic, `s` for Ext's output; `(Y,y)` stays the
  pair's narrative name. ⚠ `s` is guaranteed only `‖s‖∞ ≤ 2(γ−κ)` (Remark 1, knowledge gap),
  so **"the witness is ternary" is FALSE of `s`** — scope every ternary/norm claim to an
  *honest* `r'`. ⚠ **`fig:lasfuncs` must carry the response `ẑ = y + c·r` (Step 5)**: it was
  missing until 2026-08-21, so the panel named an output it never computed — a figure that
  states a function's inputs, bound and output must state the line producing that output.

**⚠️ IN-TEXT CITATIONS ARE IEEE, AND THE NUMBER IS NEVER A WORD** (Royce, 2026-09-01;
swept report-wide). Author-prominent mentions are **`Esgin et al.\ \cite{...}`** — never a
full author list, and the bracket goes **immediately after the name**, not at sentence end.
Otherwise the bracket postmodifies a real noun or follows a preposition (`Figure~1 in
\cite{...}` — one form, `in` not `of`; `assumed in \cite{...}`); it may never be the subject
of a verb (`[10] assumes`, `the venue [10] assumes`). Naming the authors is optional, so
prefer the shorter prepositional form where *who* is not the point. ⚠️ Choosing that subject
noun is a **claim**: it must be the thing that actually does the verb — the *application
setting* assumes a venue, an *implementation detail* is unprescribed, and neither "LAS" nor
"the specification" is a safe default (three drafts failed this on 2026-09-01). `\bibliographystyle`
is `plain`; `ieeetr.bst` **is** installed if an IEEE reference *list* is ever wanted, but
switching renumbers every citation, so it is Royce's call and needs a full rebuild.

Claim precision for report prose is governed by **EVIDENCE-OR-SILENCE** above — one home, not two.

## Guardrails (standing — never do these without an explicit instruction)

- **Do not create, delete, or switch git branches.**
- **Do not run benchmarks or tests** (no `make`, no executing benches/tests, no build-and-run).
  Edit code/docs and explain; Royce runs the build/bench himself.
- **Do not invent or estimate benchmark numbers.** Only report figures from a real, reproduced
  run; if a number is not measured, say so.
- **Do not hand-edit evidence logs** (`evidence/**`) or generated report artefacts
  (`report/latex/generated/*.tex`, `figures/*.pdf`) — regenerate them by running the tool.
- **Do not put meeting or transcript narrative into code comments** — keep comments technical.
- **Do not repair the dead legacy C Stage-2/AMHL files** — superseded by design.

## Working agreement — token-saving mode (default)

- **Session startup:** this file (its live block = current state), plus the latest `PROGRESS.md`
  section when continuing prior work. Do **not** read every project document.
- **Read on demand only:** `las-context-consolidated.md` (spec) · `docs/STATUS.md` (deliverable ×
  built/tested/documented matrix + reproducing command) · `docs/LAS.md` (design, benchmark
  interpretation) · `docs/02-methodology/THEORY_IMPL_BRIDGE.md` (equation → function) ·
  `docs/paper/LAS_2020_845_NOTATION.md` (before any notation work) · `README.md` ·
  `docs/02-methodology/FUNCTION_MAP.md` · `CONTEXT.md` (long-form handoff; the live block wins on
  disagreement) · `PROGRESS.md` (+ `PROGRESS copy.md`). If the user names files, read those first.
- **Repository scanning:** targeted only — named files, directly included headers/sources,
  relevant evidence logs and doc sections. Explain why before any wider scan.
- **Documentation sync:** when implementation changes affect design, API behaviour, benchmark
  interpretation, report claims or theory mapping, update the relevant doc *and* this file's
  hand-written half. For read-only explanation, diagnosis, Git help or review, do not edit docs
  unless asked.
- **Subagents:** never spawn unless explicitly asked; allowed only for large independent audits
  (benchmark-vs-report, blockchain/gas, theory-vs-implementation, rubric mapping) — never for Git
  questions, terminal output, small edits, or checking a couple of files.
- **Checkpoints** (`/checkpoint`): short, append-only — branch, objective, files touched,
  decisions, evidence used, unresolved risks, next exact action. No reasoning dumps or diffs.
- **Context:** same work → checkpoint then `/compact`; new topic → `/clear`.
- **Who runs what:** Royce runs all build/benchmark commands; Claude reads the resulting logs,
  explains each edit before making it, and gives a read-only diagnosis first unless asked to edit.

## Output discipline

Be direct, evidence-based, scoped.
- **Code audits:** (1) verdict, (2) evidence by file/function, (3) missing or misleading parts,
  (4) exact suggested fix, (5) whether editing is needed. If the inspected files do not prove
  something, say so.
- **Benchmark/report claims:** distinguish measured evidence from interpretation; cite the exact
  evidence log or source file; never invent missing numbers; flag stale or contradictory
  documentation.
- **Edits:** smallest possible change, show the diff, no tests or benchmarks unless requested.

## Reference

- **Objectives (spec):** `las-context-consolidated.md` · **progress tracker:** `docs/STATUS.md`.
- **Meetings:** `meetingN_cleaned_transcript.md` (+ `meeting8_summary.md`); all ten are merged
  into the spec — Meeting-6 **delivery status** in `docs/04-evaluation/SUPERVISOR_DELIVERABLES_GAP.md`.
- **Papers:** LAS = eprint 2020/845 (Esgin, Ersoy, Erkin) · poqeth = eprint 2025/091 ·
  `NIST.FIPS.204.pdf` (+ `docs/paper/NIST_FIPS_204.md`).
- **Design/math/results:** `docs/LAS.md` · theory↔code: `docs/02-methodology/THEORY_IMPL_BRIDGE.md`.
- **Experiment write-ups** (`docs/03-results/`): `MLDSA_HINT_EXPERIMENT.md`,
  `STATEMENT_COMPRESSION_EXPERIMENT.md`, `PROOF_AMORTISATION_EXPERIMENT.md`,
  `SUCCINCT_PQ_PROOF_EXPERIMENT.md`, `LAS-08-performance-measured.md`,
  `GAS_LIMIT_INVESTIGATION.md`, `TWO_LEG_REAL_CLIENT_EXPERIMENT.md`.
- **Application:** `rust/las-swap/README.md`, `docs/02-methodology/STAGE2_UTXO_SWAP_PLAN.md`,
  `BITCOIN_TX_STRUCTURE.md`, `EVM_TX_STRUCTURE.md`, `docs/04-evaluation/IPFS_OFFCHAIN_STORAGE.md`.
- **Reproducibility:** `README.md` · function classification: `docs/02-methodology/FUNCTION_MAP.md`
  · plain-English explainer: `docs/01-introduction/LAS_WALKTHROUGH.md` · build order:
  `docs/04-evaluation/PROJECT_HISTORY_EXPLAINED.md`.
- **Assessment:** `MSc_Report_and_Video_Rubric.md` · writing guidance:
  `docs/references/Lecture5_ResearchWriting_2026_Lin.md`, `muthesis_formatting_rules.md`.

<!-- gitnexus:start -->
# GitNexus — Code Intelligence

This project is indexed by GitNexus as **dilithium-msc-project** (8009 symbols, 14440 relationships, 535 execution flows).

> Index stale? Run `node .gitnexus/run.cjs analyze --index-only` from the project root — it auto-selects an available runner. No `.gitnexus/run.cjs` yet? Bootstrap with `npx`, `bunx`, or `pnpm dlx` — e.g. `bunx gitnexus@latest analyze` (npm 11 npx crash; #1939).

## Always Do

- **MUST run impact analysis before editing.** Use `impact({target: "symbolName", direction: "upstream"})` (MCP) or `node .gitnexus/run.cjs impact "symbolName" --direction upstream --repo .` (CLI fallback); report callers, processes, and risk. Never substitute grep for graph analysis.
- **MUST analyze graph changes before committing.** Use `detect_changes({scope: "all"})` (MCP) or `node .gitnexus/run.cjs detect-changes --scope all --repo .` (CLI fallback). `partial: true` or `truncated: true` is not a clean check — a zero means unseen, not unaffected; re-run it. For regression review: `detect_changes({scope: "compare", base_ref: "main"})` or `node .gitnexus/run.cjs detect-changes --scope compare --base-ref "main" --repo .`.
- **MUST warn the user** if impact analysis returns HIGH or CRITICAL risk before proceeding with edits.
- **MUST treat `risk: UNKNOWN` as unresolved, not as low.** An empty caller set is not evidence the symbol is unused — it can also mean the callers are not resolvable by the index (plain-object property access, dynamic dispatch, cross-language calls). `impact` pairs `UNKNOWN` with a `riskNote` saying so. Confirm with a text search before treating the symbol as safe to change or delete; do not proceed on the strength of a zero.
- When exploring unfamiliar code, use `query({search_query: "concept"})` to find execution flows instead of grepping. It returns process-grouped results ranked by relevance.
- When you need full context on a specific symbol — callers, callees, which execution flows it participates in — use `context({name: "symbolName"})`.
- For security review, `explain({target: "fileOrSymbol"})` lists taint findings (source→sink flows; needs `analyze --pdg`).

## Never Do

- NEVER edit a function, class, or method before MCP/CLI impact analysis.
- NEVER ignore HIGH or CRITICAL risk warnings from impact analysis, and never read `UNKNOWN` as an all-clear — it means the walk could not answer, which is the one verdict that requires confirming by other means.
- NEVER rename symbols with find-and-replace — use `rename` which understands the call graph.
- NEVER commit before MCP/CLI graph change analysis.

## Resources

| Resource | Use for |
| --- | --- |
| `gitnexus://repo/dilithium-msc-project/context` | Codebase overview, check index freshness |
| `gitnexus://repo/dilithium-msc-project/clusters` | All functional areas |
| `gitnexus://repo/dilithium-msc-project/processes` | All execution flows |
| `gitnexus://repo/dilithium-msc-project/process/{name}` | Step-by-step execution trace |

## CLI

| Task | Read this skill file |
| --- | --- |
| Understand architecture / "How does X work?" | `.claude/skills/gitnexus-exploring/SKILL.md` |
| Blast radius / "What breaks if I change X?" | `.claude/skills/gitnexus-impact-analysis/SKILL.md` |
| Trace bugs / "Why is X failing?" | `.claude/skills/gitnexus-debugging/SKILL.md` |
| Rename / extract / split / refactor | `.claude/skills/gitnexus-refactoring/SKILL.md` |
| Tools, resources, schema reference | `.claude/skills/gitnexus-guide/SKILL.md` |
| Index, status, clean, wiki CLI commands | `.claude/skills/gitnexus-cli/SKILL.md` |

<!-- gitnexus:end -->
