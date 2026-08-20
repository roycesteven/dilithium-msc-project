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
   ⚠️ **A "worst case" is only a bound if EVERY free variable is pushed the adverse way**
   (2026-08-15, cost a retracted draft). A D5-vs-gas-cap derivation was written as *proved*
   under "execution can only grow"— but calldata **byte content** was a second free variable,
   and pushing it the other way (all added bytes zero) flips D5 from 74,331 over the cap to
   231,333 under it. Naming one assumption does not make the others disappear. → EVIDENCE-OR-SILENCE
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
- **NOT the focus:** the four parameter sets (paper / D2 / D3 / D5) are only a *secondary
  fairness / parameter-sensitivity* axis (§13.4). Never frame results around "across
  security parameter" or "as the scheme scales"; the across-parameter overhead chart is
  supporting material, not a primary body figure.
- **Timing rule:** per-operation timing is the PRIMARY timing result (§14.3) — never lead
  with cumulative / end-to-end time.
- **Presentation rule:** no table↔chart redundancy — the *chart* carries the body, the
  exact-number *table* goes to the appendix; **figures are embedded between paragraphs in
  the body**, never collected at a chapter end or in the appendix (Meeting 8, overrides the
  older figures-to-appendix habit); group the four LAS function figures side by side; avoid
  single-figure pages; no abbreviations of scheme/level names in tables or figures.
- **⚠️ NO PAGE MAY CONTAIN ONLY FLOATS (Royce, 2026-08-12).** LaTeX gives queued floats a page
  of their own as soon as they fill `\floatpagefraction`; at the old 0.75 that stranded five
  of Ch. 3's floats on text-free pages. `report.tex` now sets it **0.95** under `topfraction`
  0.92. Two knock-on rules: an over-long caption is what makes a float page-sized (Tab. 3.7's
  was cut to fit — caveats kept, mechanism moved to `app:methoddetail`), and a float placed
  beside its own discussion rather than at the section head does not queue. **Re-check the
  whole PDF after any float edit** — placement is global, so a fix here creates one there.
- **Criterion figure is NOT "reproduced unmodified"** — Criterion's 12-unit type renders at
  ~4 pt at `\linewidth`, and its key column spends a fifth of the width on five strings.
  `scripts/gen_criterion_figure.py` enlarges the type, **moves the legend from the right column
  into one row below the plot** (Royce, 2026-08-12), re-flows the margins, crops to the result,
  and folds gnuplot's `10^3` tspans into the glyph `10³`. **The plot interior is the identity
  map** — gated by a coordinate-for-coordinate check (1358 interior coords unchanged); layout is
  derived from the file, so it asserts rather than silently mis-draw. Runs off a captured
  `evidence/criterion/*/presign_pdf.svg`, so the figure rebuilds without re-running the bench.
  ⚠️ The caption must keep listing every one of those changes — the type, the legend, the crop
  and the superscript; "nothing else was touched" is an overclaim while the fold is in the file.

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

*Regenerated 2026-08-20 16:08 by `scripts/update_claude_context.py`, which only reads files and git metadata — it never builds, tests, or benchmarks, and never estimates a number. Anything it could not parse says (not found).*

### Repository right now

- Branch **`report`** · HEAD 3b40c3c · 2026-08-19 · throughput scalability proposal unfinished
- Working tree: 18 modified tracked file(s), 44 untracked path(s) · no upstream tracking branch
- Recent commits:
  - `3b40c3c 2026-08-19 throughput scalability proposal unfinished`
  - `0f6c914 2026-08-19  mcr deck`
  - `ea09bdf 2026-08-18 report deck btc full two leg update`
  - `3800bda 2026-08-18 d2 d3 d5 evm polish report deck not checked`
  - `4cad978 2026-08-17 evm d2 d5 btc two leg swap deck fix`

### Target parameter set — anchors parsed from source

- Simplified Dilithium-III / ML-DSA-65-aligned (n=6, ℓ=5, κ=49): `c_tilde` 48 B · public key 4416 B · secret key 704 B · signature = pre-signature 6736 B (`ref/serialize.h` static assert)
- Other sets (signature bytes): D2-aligned 4640 B · D5-aligned 9184 B · paper reproduction 4640 B
- Pinned KAT digest `b4a10ffb…03be` — C and Rust agree ✅
  - full: `b4a10ffb6e645e5076d1ff5993faa72909232fc71e554b93544141d6590503be`

### Latest measured evidence (pointers only — never retype a number)

- Stage-1 benchmark suite: `evidence/latest` → `runs/20260804_101750` (dir mtime 2026-08-04)
- Stage-2 UTXO swap: `evidence/stage2/latest` → `20260730_162109` (dir mtime 2026-07-30)
- On-chain gas (EVM): `evidence/onchain/latest` → `20260805_174829` (dir mtime 2026-08-05)
- Criterion micro-bench: `evidence/criterion/latest` → `20260730_165134` (dir mtime 2026-07-30)
- las-stark: `evidence/stark/latest` → `20260729_175637` (dir mtime 2026-07-29)
- Report word count: **8981** (`report/latex/word.count`, rubric bound 7,000–9,000; `make -C report/latex wordcount`)

### Where the last session stopped

- Last checkpoint in `PROGRESS.md`: Checkpoint — 2026-08-17 17:55
- Next action recorded there:
  - Run the deck aloud against a clock (N for notes, T to start) and adjust per-slide data-time
  - from what it actually takes; then regenerate the word count.
- `CONTEXT.md` (long-form handoff): CONTEXT — session handoff (updated 2026-07-29; ninth-session update first)

### Supervisor meetings on record

- Cleaned transcripts present: 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 (`meetingN_cleaned_transcript.md`)
- Merged into `las-context-consolidated.md` (the objectives spec): meetings 1, 2, 3, 4, 5, 6, 7, 8, 9, 10

### Freshness tripwires

- ⚠ Source newer than Stage-1 evidence: `ref/relation_zk_labrador.h` (2026-08-10 11:31) > `evidence/latest` (2026-08-04 10:19). Numbers in the report may predate the code — re-run the suite before quoting them.
- `CLAUDE.md` hand-written sections last touched 2026-08-20.

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
MSIS hardness parameter; acceptance ≈37% per attempt (`≈ e^{−1}`) for the simplified scheme
without hint vector, measured directly via the `las_attempts` counter, **never inferred
from timing ratios**.

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
- `scripts/gen_bitcoin_tx_data.py` projects measured object sizes onto Bitcoin's real wire format
  (BIP141/144/341), self-checking against two published vB figures before emitting →
  `generated/btcmacros.tex` + `tab:btctx`; prose `docs/02-methodology/BITCOIN_TX_STRUCTURE.md`.
  ⚠️ **Config 1 must be projected from a DER witness item, never the 64-byte compact ECDSA
  signature** — that error understated the classical baseline and inflated every PQ ratio.
- **Deliberately dead, do NOT repair:** `ref/amhl.{c,h}`, `ref/chain.{c,h}`, and
  `ref/test/{test_contract,test_pcn,bench_app}.c` — pre-seven-type (`las_pp`/`las_pk`/`las_sk`/
  `las_sig`), superseded by the Rust evaluation. `STAGE1_ONLY=1` skips them and still regenerates
  the **Stage-1** artefacts — NOT "everything the report consumes": Stage-2, on-chain, Criterion
  and Bitcoin figures come from their own runners. **`STAGE1_ONLY=1 scripts/run_benchmark_suite.sh`
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
  Honest path only — the tapleaf has **no refund branch**, so timeouts are not implemented.
  ⚠️ **REFUND/TIMEOUT ACROSS VENUES — verified against source 2026-08-19; a relayed
  "EVM ✅ / Bitcoin ❌" table overstated BOTH sides.** **Neither real-client two-leg run
  exercises the timeout/refund recovery path** — say *exercises*, never "implements":
  Bitcoin genuinely has none (no tapleaf branch), but `AdaptorSwapBound.refund` **is**
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
  not a correctness repair.
- **Framings that must not drift:** a patched node is **not** Bitcoin — "cannot settle on
  Bitcoin as it stands" stays true; implementing one of `BITCOIN_TX_STRUCTURE.md` §5.4's
  three routes is **not** a position on which should be adopted; **the rule's security is
  still unanalysed** — that caveat never lapses. Bitcoin binds the transaction, **not the
  chain** (BIP341 sighash has no chain id) — the EVM leg does (`AdaptorSwapBound.legMessage`
  hashes `block.chainid`); state the asymmetry.
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
from Ch.3/4/5, `app:naysayer` deleted, all `gasNaysay*` macros dropped. **SUPERSEDES** the
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
are **purged report-wide**; do not reintroduce them. Meeting 7 still stands on fees-not-gas-limits,
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
- **The claim, in its corrected form:** *PreSign and PreVerify are necessarily new algorithms;
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
  now names **two** optimisations, not three.
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
   "D5 needs more optimisation" line**. **REWORK DONE 2026-08-17** — `video_deck.html` is now
   **10 slides, planned 6:35**, application-first, closing on the opening questions; slides 2/3/4
   are **drawn scenes** (inline SVG on the deck tokens, built in four timed `.st1–.st4` stages, no
   keypress) because the 13→10 pass had only reordered *text*; a **Why · Method · Results ·
   Takeaway stepper** answers "where are we"; the key-map overlay now defaults **off** (it was in
   shot); the ML-DSA attempt got the slide **table** Wang suggested. Demos are now slides **5 and
   8** — fix that pair of numbers wherever it is cited. ⚠ **Under-running is now the timing risk**
   (6:35 is 35 s over the floor), the opposite of the old 7:12 plan.
   ⚠ **RENDER, NEVER REASON, ABOUT LAYOUT (2026-08-17)** — the previous session shipped voids and a
   clipped table that were invisible from the markup. Windows Chrome is reachable from WSL and
   screenshots any slide by deep link; the working command is in `VIDEO_PLAN.md` §3. A layout claim
   without a screenshot is unverified. Plan + beat sheet +
   on-camera claim discipline in `report/slides/VIDEO_PLAN.md`. It is **generated**:
   `scripts/gen_slides.py` fills `video_deck.template.html` from
   `report/latex/generated/*.tex` and embeds the report's own figures, so slides and report
   quote one evidence run (`--check` fails when stale; **edit the template, never the
   output**; re-run after every `sync_report.sh`). Bar geometry is derived from the same
   macros as the labels, so a width cannot drift from its number.
   ⚠ **UoM FORMAT LIVES IN THE HTML DECK — a .pptx conversion was built and REJECTED (Royce,
   2026-08-19).** `video_deck_uom.pptx` + `scripts/gen_slides_pptx.py` are kept **only as a
   fallback** if a submission ever demands PowerPoint — never the file to record, never the file
   to edit, and it goes stale the moment the template changes. The template's chrome was skinned
   onto the deck instead: **Arial**, headline `--uom` #7800A2, dotted rule `--uom-rule` #660066,
   and the Manchester mark top-left on every slide (`#uomlogo` ⇐ `{{UOM_LOGO}}` ⇐
   `report/slides/assets/uom_logo.png`, embedded base64 by `gen_slides.py`'s `ASSETS`). The mark
   is **reproduced** from `Master_169 presentation(2).pptx`, never redrawn — provenance and the
   rebuild recipe are in that directory's `README.md`.
   ⚠ **Brand colour touches CHROME ONLY.** The base / adaptor / reused / warn accents are
   *semantic* — they mean the same thing in the report figures printed beside them — so they are
   never recoloured to match a brand. Both light and dark carry their own `--uom` step.
   ⚠ Rendering the skin caught a **pre-existing** defect: slide 8's card 2 has overflowed its box
   since the two-leg text landed 2026-08-18 (confirmed against `git show HEAD:`, so it is not the
   skin's). Fixed by the reusable `.tight` column knob — **scale a column, never cut a claim.**
   ⚠ **NO CAPTURES — NOT A TERMINAL, NOT A WINDOW SWITCH (Royce, 2026-08-13; a terminal dump
   was already rejected 2026-08-12 — it shows that a run happened, not what happened).**
   **Both demos step INSIDE the deck**: `data-sub="n"` makes a slide consume the forward key
   n times, driven by four attribute rules (`data-w`/`data-only`/`data-until`/`data-settle`)
   — a beat is markup, never per-slide script — and each slide **opens on beat 1**. Slide 5 =
   the swap board (abort gate → tripwire → publish σ₂ → Ext y′ **then** Adapt σ̂₁ and publish;
   never collapse those last two, Fig. 1 has both, and u₂ *does* publish — it needs nothing
   further **from u₁**); slide 9 = the node differential, drawn. `swap_console.html` stays as
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

Spec: `las-context-consolidated.md` (**§15A = M6**, §16 = M7, §17 = M8, §18 = M9, §19 = M10).
Transcripts: `meetingN_cleaned_transcript.md` (+ `meeting8_summary.md`). **Read §16 before
planning application work.** ⚠ **M6 was merged into the spec 2026-08-15** (Royce) — it is
*lettered* 15A, not numbered, so §16–§19 keep the numbers cited across CLAUDE.md and `docs/`;
its per-item delivery **status matrix** stays in
`docs/04-evaluation/SUPERVISOR_DELIVERABLES_GAP.md` §1. Supersedes the old "meeting 6 is not in
that file" warning — the spec now covers every meeting.

**Meeting 10 (date NOT confirmed — 2026-08-14 or -15; never cite a firm one) — LATEST WORD.
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
  ⚠ **Party naming, settled 2026-08-16 — do not "fix" either way.** Deck slide 5 uses **Alice /
  Bob** outright. The report keeps `u₁`/`u₂` for mathematics, and the new intro figure
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
