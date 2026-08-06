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
5. **Verify before recording.** Line references, dates and "both"/"twice" quantifiers go stale
   silently; an unverified fact recorded here is how contradictions get in.
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

### ⚠️ WORD COUNT — regenerate with `make -C report/latex wordcount`, never trust a stale file

A stale `word.count` once drove several sessions of trimming against the wrong number.
Always regenerate before reasoning about budget. Mechanics that matter:
- Body only: `%TC:ignore` fences in `report.tex` exclude the frontmatter TODO, the
  **appendix** and the bibliography; `-sum=1,1,0,0,0` excludes **captions**. Appendix and
  captions are FREE, and the rubric agrees.
- **Tabular bodies and prose DO count; TikZ picture content and `generated/*.tex` tables do
  NOT.** Moving a reference table into the appendix is the cheapest real saving — already
  done for `tab:tests`, `tab:notation`, `tab:tiers` (now `app:tests`).
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

*Regenerated 2026-08-06 15:48 by `scripts/update_claude_context.py`, which only reads files and git metadata — it never builds, tests, or benchmarks, and never estimates a number. Anything it could not parse says (not found).*

### Repository right now

- Branch **`report`** · HEAD 6c161f4 · 2026-08-05 · evm full
- Working tree: 7 modified tracked file(s), 12 untracked path(s) · no upstream tracking branch
- Recent commits:
  - `6c161f4 2026-08-05 evm full`
  - `bc2fe4a 2026-08-05 unfinished evm`
  - `ae8366f 2026-08-04 labrador`
  - `a44468a 2026-08-04 CLAUDE.md update`
  - `931d37b 2026-08-04 investigate statement compression`

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
- Report word count: **8997** (`report/latex/word.count`, rubric bound 7,000–9,000; `make -C report/latex wordcount`)

### Where the last session stopped

- Last checkpoint in `PROGRESS.md`: Checkpoint — 2026-08-05 — CLAUDE.md: standing "DO NOT REPEAT" rules for all sessions
- Next action recorded there:
  - Trim 8,955 -> 8,000 (955 words). Duplication/filler exhausted; remaining source is the
  - ~40 sentences over 40 words plus further high-level-ing of sec 2.6 (848 w) and 2.7.
  - Fill the frontmatter \TODO (student id, prior degrees, acknowledgements) -- Royce only.
  - ## Checkpoint — 2026-08-03 — remaining-work sweep: AMHL cleanup, Adapt gap, ML-DSA hint experiment, EVM/IPFS write-ups
  - Branch: report. No commits made. Royce mid-session: "skip the report for the moment,
- `CONTEXT.md` (long-form handoff): CONTEXT — session handoff (updated 2026-07-29; ninth-session update first)

### Supervisor meetings on record

- Cleaned transcripts present: 1, 2, 3, 4, 5, 6, 7, 8 (`meetingN_cleaned_transcript.md`)
- Merged into `las-context-consolidated.md` (the objectives spec): meetings 1, 2, 3, 4, 5, 7, 8
- ⚠ **NOT in that file**: meeting 6 — read the transcript/summary directly before planning work it touches (meeting 6 is held in `docs/04-evaluation/SUPERVISOR_DELIVERABLES_GAP.md` by design)

### Freshness tripwires

- ⚠ Source newer than Stage-1 evidence: `ref/relation_zk_labrador.c` (2026-08-04 17:24) > `evidence/latest` (2026-08-04 10:19). Numbers in the report may predate the code — re-run the suite before quoting them.
- `CLAUDE.md` hand-written sections last touched 2026-08-06.

<!-- END AUTO-CONTEXT -->

## The project in brief

**Goal:** implement LAS (Lattice-based Adaptor Signatures, eprint 2020/845) by reusing the
CRYSTALS-Dilithium reference primitives, then demonstrate it in a post-quantum blockchain
**atomic-swap** scenario, benchmarked and documented.

**Why:** blockchains sign with ECDSA/Schnorr, which Shor breaks. NIST standardised *basic* PQ
signatures (Dilithium, Falcon, SPHINCS+), but *exotic* ones (multisig, ring, group, **adaptor**)
are in the PQ setting mostly paper-only — little working code, none on a blockchain. Adaptor
signatures enable atomic swaps / payment channels; closing that implementation gap is the thesis.

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

**Stage 1 — scheme + benchmark (complete).** LAS variant B in C (`ref/las.c`, `basesig.c`,
`setup.c`, `relation.c`) and Rust (`rust/fips204-las/src/`); **Rust is the naming authority**,
KAT-locked to C byte-for-byte.
- Correctness: `test_las` (1000 iters), `test_basesig`, `test_contract` (8-point itemised
  contract), `test_serde` (every single-byte flip rejected), `test_kat` (pinned cross-language
  digest).
- Serialization + byte-level verifier `ref/serialize.{c,h}`: six typed pack/unpack pairs, a
  *validating* decoder, and `base_verify_packed` = the byte interface an on-chain verifier
  consumes. Wire form `c_tilde ‖ BitPack(z)`; `z` is ~99.3% of the signature.
- Deterministic API + pinned KATs: `base_keygen_seed` / `base_sign_det` / `las_presign_det`, mask
  seed `SHAKE256(tag‖sk‖[Y]‖M)`; reproducible across machines.
- Benchmarks: `bench_levels` (primary fair base-vs-LAS, ≥5 runs, mean±SD), `bench_las`,
  `bench_compare` (context only — optimised Dilithium is *not* algorithm-matched),
  `bench_classical` (ECDSA adaptor via vendored `secp256k1-zkp`), Rust Criterion. Two baselines,
  per Meeting 2. Headline: the price of post-quantum here is **communication, not computation**;
  LAS's adaptor overhead is small, where the classical adaptor pays ~4× for its DLEQ proof.

**Stage 2 — the application (complete, Bitcoin/UTXO).** `rust/las-swap/` implements the eprint
2020/845 §4.1 Fig. 1 atomic swap over a UTXO ledger model (ledger takes the signature algorithm
as a parameter, as §4 assumes) and benchmarks **three configurations**: (1) classical ECDSA
adaptor, (2) LAS + Groth16, (3) LAS + LaZer. Honest path plus timeout/refund, one pinned master
seed, measured on **time + communication** including off-chain messages.
- **Attribution rule:** **2→3 is the controlled comparison** (same signature, same relation, only
  the prover differs — lead with it); 1→2/3 is a whole-stack comparison, *not* the cost of the PQ
  signature alone.
- Bitcoin transaction structure: `scripts/gen_bitcoin_tx_data.py` projects measured object sizes
  onto Bitcoin's real wire format (BIP141/144/341), self-checking against two published vB
  figures before emitting → `generated/btcmacros.tex` + `tab:btctx`, plus the
  original-vs-modified diagram in Ch. 2. Prose: `docs/02-methodology/BITCOIN_TX_STRUCTURE.md`.
- **Legacy C Stage-2 is deliberately dead:** `ref/amhl.{c,h}`, `ref/chain.{c,h}`,
  `ref/test/{test_contract,test_swap,test_pcn,bench_app}.c` use the pre-seven-type API and **do
  not compile**; superseded by the Rust Stage-2 evaluation — **do not repair them**.
  `STAGE1_ONLY=1` skips them and still regenerates everything the report consumes.

**Proof of knowledge π (Fig. 1) — Royce-directed scope extension.** `ref/relation_zk.{c,h}`
(`relation_prove`/`relation_proof_verify`; non-ternary witnesses refused) over vendored **LaZer**,
with `ref/relation_zk_lazer.c` as the only TU that includes `lazer.h`. Ternary via binary
decomposition `[A|−A|0]·(r₊‖r₋‖e)=t′`; committed params `ref/relation_zk_params.h` ⇐ LaZer codegen
from `scripts/las_pi_params.py`; Rust twin `relation_zk.rs` FFIs the same C bridge behind cargo
feature `relation-zk` (default off, KAT gate intact). π is **off-chain only**; opt-in targets.

**⚠️ ON-CHAIN VERIFICATION FITS IN ONE TRANSACTION AT D3 — RUN 2026-08-05 (Royce-directed).**
`evm/src/LASVerifierOpt.sol` + `LASShake.sol` + `LASRegister.sol`, entrypoint
`AdaptorSwap.claimLASVerifiedOpt`; mechanism, numbers and caveats in §7 of
`docs/03-results/GAS_LIMIT_INVESTIGATION.md`. **Same predicate, same scheme** — the baseline
measured the *expression*, not LAS. Gated twice: modelled charge (`test/LASGasBreakdown.t.sol`)
**and** a real client receipt (`scripts/run_onchain_one_tx.sh` → `evidence/onchain_onetx/<ts>/`).
- **Never present it as faster-because-weaker** — pinned to `LASVerify` and C ground truth
  (`test/LASVerifierOpt.t.sol`, `test/LASShakeEquiv.t.sol`); do not weaken those pins.
  **`claimLASVerified` stays as the measured baseline — do not delete it.**
- **⚠️ SCOPE IS PART OF THE CLAIM (→ EVIDENCE-OR-SILENCE).** Measured **at D3 with a 32-byte
  signed message** (the Bitcoin sighash case), on the EVM revision named in the evidence.
  Headroom is a small percentage of the cap and is effectively a *message-length budget*,
  since the preimage is `pack(t)‖pack(w')‖M` and the sponge dominates execution. **D2/D5 are
  NOT evaluated** — `LASVerifyOpt`'s parameters are compile-time D3-only, so the library cannot
  run them; say "not evaluated", never that it fails there. The message-length limit is a
  *derivation*, labelled as such in the write-up — not a measurement.
- **⚠️ NEVER MEASURE A CAP GATE UNDER `--gas-report`** (cost a false FAIL once): Foundry's
  inspector is metered inside the measured frame and inflates **both** `gasleft()` deltas and
  `vm.lastCallGas()` by more than the whole headroom, so the gate fails for a reporting reason
  while a real client mines the same transaction. `run_onchain_gas.sh` runs gates in a pass
  **without** the flag and the table in a pass with the gate contracts **excluded**, capturing
  **each pass's exit status separately** — a single `{...} | tee` reports only the last command,
  hiding a failed gate behind a green table. `LasVerifiedOptSwapGas` (report-only) and
  `LasVerifiedOptSwapGate` (asserts) are twins over one base for this reason — never merge them
  or add an assertion to the reporting one.
- **⚠️ GAS ACCOUNTING IS EIP-7623, NEVER EIP-2028** (cost real work once): the charge is
  `21000 + max(4·tokens, 10·tokens)`, `tokens = zero + 4·nonzero` — see `LASTxGas`. The floor
  branch binds for calldata-heavy/compute-light transactions, so the old 16-gas-per-nonzero-byte
  model **understates** them; every reported total must say which branch bound.
  (`TwoLegSwapGas.t.sol`'s helper models only the standard branch — never copy it for a gate.)
- **⚠️ EIP-8051 (ML-DSA precompile) is a CITATION, NOT A ROUTE**: status **Draft**, **Declined
  for Inclusion** in Glamsterdam (EIP-7773), EF roadmap puts PQ precompiles in 2027–28, and it
  covers **NIST level II / ML-DSA-44 only** — not the D3 headline set. Its ETH variant replaces
  SHAKE256, so "stock FIPS-204 verifier accepts it" and "use the ETH variant" cannot both be
  claimed. Any figure from it is a **conditional model computed from the EIP's own constant,
  never a measurement** — label it so. §8 of the same write-up.

**On-chain verification (EVM) — the baseline, retained as evidence.** `evm/src/LASVerifier.sol`,
a complete native verifier over vendored ZKNox ETHDILITHIUM primitives, validated against C and
bound by a fund-time `keccak256(A′,t,M)` commitment; its cost is far above EIP-7825's cap.
Naysayer (optimistic) variant = **negative result**: honest path fits, the `naysayDigest` fraud
proof does not, and an unmineable fraud proof is not one. Gas figures come only from a captured
`forge --gas-report` log parsed by `scripts/plot_onchain_gas.py`; nothing hardcoded.
**⚠️ Meeting 7 is NOT retracted by the one-transaction result above** — the Bitcoin/UTXO pivot
also rests on fees-not-gas-limits, heavy work staying off-chain, and adaptor swaps being used on
UTXO chains in practice; on-chain LAS stays orders of magnitude above a classical `ecrecover`
claim. Do not re-open Stage 2's venue. Detail: `docs/03-results/GAS_LIMIT_INVESTIGATION.md`,
`docs/02-methodology/EVM_TX_STRUCTURE.md`. **IPFS off-chain storage** = **fallback, NOT adopted**
(`docs/04-evaluation/IPFS_OFFCHAIN_STORAGE.md`): the swap needs none of it (π is a direct
party-to-party message), and for the optimistic verifier a data-availability failure becomes a
*soundness* failure.

**`rust/las-stark` — post-quantum succinct proving. TWO modules; never conflate them.**
- `relation_air` (WIP gadget) — FRI-STARK over the *arithmetic core* of `base_verify` (norm
  bound + `w' = z_top + A′·z_bot − c·t`). The Fiat–Shamir hashes are **not** in the AIR, so `z`
  is bound to `(A′,t,c,w')` but **not** to `(c̃, M)`: **never** describe it as a complete proof
  of on-chain verification.
- `role_a_air` + `src/bin/bench_role_a.rs` — **built and measured 2026-08-04 (Royce-directed).**
  Proves the WHOLE role-A statement `∃r : A r = t' ∧ ‖r‖∞ ≤ 1` — the same one configs 2 and 3
  prove, ternary via `r³ = r` exactly as the Groth16 circuit does — so **the gadget caveat above
  does NOT apply here** (the role-A relation contains no hash); do not copy it across. Runner
  `scripts/run_role_a_stark.sh` → `evidence/role_a_stark/<ts>/`; derivation, trace layout,
  numbers and the batched follow-up that was *not* taken are in
  `docs/03-results/SUCCINCT_PQ_PROOF_EXPERIMENT.md`.
  It works but **at this size does not pay** (the finding is the cost split, not a speed-up);
  no concrete-security analysis; security levels **not** claimed equal; **not wired into the
  swap**. Framings in full: the write-up.
  **⚠️ DISQUALIFIED AS π, AND BARRED FROM THE REPORT (Royce, 2026-08-04/05):** eprint 2020/845
  §4.1 needs π to *hide* the witness — if u₂ learned `r` it could adapt σ̂₁ and take both sides
  — so **π must be ZERO-KNOWLEDGE**, and this STARK is not. It is **not a candidate prover**,
  its comparison flatters it on an axis where it does not qualify, and it must **not** appear in
  the report: the report's succinct-PQ story is **LaBRADOR only**. Kept as internal evidence,
  never cited as a result.
**pi under LaBRADOR — succinct + PQ + ZERO-KNOWLEDGE; RUN 2026-08-04 (Royce-directed).**
`ref/relation_zk_labrador.{c,h}` (the THIRD and last vendored-proof-library TU) +
`ref/test/bench_labrador_role_a.c`; runner `scripts/run_labrador_role_a.sh` →
`evidence/labrador_role_a/<ts>/`; encoding, derivation and numbers in §6 of
`SUCCINCT_PQ_PROOF_EXPERIMENT.md`. **In the encoding `[A|−A]w − q·g = t'`, the g bound is
load-bearing** — unbounded g satisfies the equation for any `t'`.
**Result — the direction is CLOSED:** the only prover tested satisfying all of
succinct/PQ/zk, and at this statement size it **loses to the deployed LNP22 on every axis**.
Same lesson as the STARK: succinctness is asymptotic; one role-A relation is far too small.
**Caveats that must travel:** the encoding is ours and may not be LaBRADOR's best; its proof
size is the library's printed *estimate*, not byte-exact like LNP22's `prooflen` — never
compare the two silently.
**Gate names `PI_LAB_*`** — do not rename/alias `PI_ROWS`/`PI_COLS`/`PI_DEG` or `PI_BATCH_*`.
**⚠️ THREE TRAPS (each cost time once):** (1) `src/labrados` is a **git submodule the README's
LaZer clone does NOT fetch** — `git submodule update --init src/labrados`, then
`make liblabrador38.so`; (2) LaZer's shipped `src/labradosNN_py.h` declares internal `N 64`
while the submodule defines `N 256` — struct layouts disagree, so **always** use
`src/labrados/labrados_python.h` with `-DLOGQ=NN -DNDEBUG -Isrc/labrados`; (3) labrados'
`simple_verify`/`verify` return **1 on SUCCESS**, opposite to the setters in the same header.
**LOGQ=38 is forced** by the lifting bound (LOGQ=36 overflows).
- No Groth16 wrap anywhere in `las-stark` — it would defeat post-quantum security.

**ML-DSA adaptor experiment — COMPLETE 2026-08-03.** LAS built on FIPS 204 **as specified**
(hint, Power2Round, high/low-bit split all enabled), zero upstream functions modified, control
verifier = stock `crypto_sign_verify`. `ref/mldsa_las.{c,h}` + `test_mldsa_hint{2,3,5}`
(diagnostic — a FAILS row *is* a result), `test_mldsa_las{2,3,5}` (13/13 contract),
`bench_mldsa_compare{2,3,5}` (both constructions in ONE binary). Runner
`scripts/run_mldsa_hint_experiment.sh` → `evidence/mldsa_hint/<ts>/`; numbers and discussion in
`docs/03-results/MLDSA_HINT_EXPERIMENT.md`. **Never mix its numbers with `evidence/latest/`.**
Durable findings:
- **It CORRECTED the project's own claim** — correct claim now: *PreSign and PreVerify are
  necessarily new algorithms; `Verify` is not.* With the whole commitment path (committed high
  bits, low-bits rejection test **and** `MakeHint`) on `w+Y` and PreSign tightened to
  `GAMMA1−BETA−ETA`, all adaptor properties hold at ML-DSA-44/65/87, **including unmodified FIPS
  204 `crypto_sign_verify` accepting the adapted signature**. Never restate the superseded
  "reference optimisations must be disabled" version.
- **Caveat that travels with the claim:** functional demonstration only — the security of
  committing to `HighBits(w+Y)` is NOT analysed.
- Adaptor overhead is single-digit percent on **both** constructions. **`Y` is byte-identical in
  both** (`K` full-width polynomials either way) while ML-DSA halves signature and public key —
  so **any future size work must target `Y`, not the signature.**

**Two former future-work items, promoted and RUN 2026-08-04 (Royce-directed override of the
Meeting-8 freeze).** Quote the write-ups for numbers, never this file.
- **Statement compression** — `ref/test/test_statement_compress.c` (+`{2,3,5}`, in `all`),
  `scripts/run_statement_compress.sh` → `evidence/statement_compress/latest`, write-up
  `docs/03-results/STATEMENT_COMPRESSION_EXPERIMENT.md`. Hard gate: the full-statement control
  must hold the contract or nothing is attributable. **Verdict CONFIRMED with a mechanism:**
  truncation is invisible to the adaptor's own functions and fatal at both boundaries
  (`base_verify` never sees a statement; Ext's acceptance test IS the exact relation) — at every
  depth, not marginally; the seed candidate compresses totally **and hands the receiver the
  witness**. **Do not reopen `Y` compression inside this construction; the open question is a
  different hard relation whose statement is smaller by design.**
- **Proof amortisation — BOTH provers measured; question CLOSED, batching fails on both for
  opposite reasons.** Groth16: `rust/las-swap/src/bin/bench_amortise.rs` +
  `BatchedRelationCircuit` (relation shared by single and batched circuits + a per-batch tamper
  check, so a batch cannot prove something weaker) → `evidence/amortise/latest` (⚠️ three runs
  that day — **`latest` is the one to quote**). LaZer: `ref/relation_zk_batch.{c,h}`
  (block-diagonal = the conjunction of k copies of the *deployed* statement) +
  `ref/relation_zk_lazer_batch.{c,h}` (the **second** TU that may include `lazer.h`) + committed
  `relation_zk_params_k{2,4,8}.h` ⇐ `scripts/gen_lazer_batch_params.sh` (SageMath at
  `~/micromamba/envs/lazer-sage/bin/sage`), `scripts/run_lazer_amortise.sh`; **k=1 dispatches to
  the COMMITTED `las_pi_params`**. Write-up `PROOF_AMORTISATION_EXPERIMENT.md`.
  **Framings:** Groth16's proof is constant in k so per-swap bytes fall `1/k` **but bytes were
  never the bottleneck** — a statement about *Groth16*, not about batching; LaZer's proof/swap
  drops but per-swap prove+verify gets several times worse (superlinear). ⚠️ **NOT wired into
  the swap**; batched param sets **not independently reviewed**, no security claim about
  batching.
- **NOT attempted, and why** (not a shortfall of effort): *analysing the ML-DSA variant's
  security* is barred by the out-of-scope ruling (a reduction, not code). It stays in Ch. 5.
  ⚠️ **SUPERSEDED 2026-08-05:** this bullet also said one-transaction on-chain verification
  "needs a SHAKE256 precompile, a Merkle-opened dispute, or a succinct proof". That was an
  unevidenced prediction and it is now **falsified** — it was achieved at D3 with none of the
  three (see the ON-CHAIN block). Do not restore it to Ch. 5 future work.

**⚠️ AMHL is DROPPED (Royce, 2026-08-03).** Multi-hop locks are **out of the project** — not a
bonus, not future work, not a deliverable. Do not build on `ref/amhl.{c,h}`, do not revive it,
do not re-add it to any status list or work queue.
*Cleanup state, verified 2026-08-05:* `report/latex/` is **clean** — the only remaining
mention is the background sentence at `01-introduction.tex:75`, which passes the test below.
`docs/` still mentions it in several files (list them with
`grep -rli "amhl\|multi-hop" docs/`). Apply this test per occurrence rather than deleting the
word wholesale: **(a) a claim about *this project's* artefacts or results → must go;
(b) literature/background describing *other people's* work → may stay.**

**Reproducibility spine:** `README.md` (build/run/reproduce, upstream commit `2374d22` and
toolchain recorded), `docs/02-methodology/FUNCTION_MAP.md` (every Dilithium function
classified call-as-is / modify / new — headline: **zero upstream functions modified**),
two-branch diff view (`dilithium-baseline` vs `main`), and runner scripts under `scripts/`
that each write a timestamped evidence directory with raw tool output, an environment/commit
record, a `latest` symlink, and automatic macro/figure regeneration.

## What remains (verified 2026-08-05)

1. **6–8 minute presentation slides** — Wang's deliverable. `report/slides/` currently holds
   only `stage1_summary.html` (2026-07-25); the deck does not exist yet.
2. **Frontmatter `\TODO`s** in `report/latex/report.tex` (student id, prior degrees,
   acknowledgements) — **Royce only**.
3. **AMHL doc cleanup** in `docs/` under the (a)/(b) test above.

**Done — do not re-queue:** the two Chapter 5 future-work bullets those experiments answered
(statement compression, proof reduction) are **rewritten as measured verdicts** in
`05-conclusion.tex` — do not restore them as open questions; the ML-DSA hint result is folded
into Ch. 4 (`04-evaluation.tex` §"Reference optimisations appear to fight the adaptor identity"
records the corrected "sufficient route, not a necessary one" claim); the `q≈2^24` bullet is
gone from Ch. 5; the `Adapt`-vs-ECDSA gap is explained (`03-results.tex:322`, detail in
`docs/03-results/LAS-08-performance-measured.md`); every experiment listed in Status is RUN
with evidence and a write-up; Meeting-8's transaction breakdown + diagrams, terminology split,
Chapter 5 title and functions-not-protocols wording are satisfied.

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

Spec: `las-context-consolidated.md` (§16 = Meeting 7, §17 = Meeting 8). Transcripts:
`meetingN_cleaned_transcript.md` (+ `meeting8_summary.md`); Meeting-6 directives live in
`docs/04-evaluation/SUPERVISOR_DELIVERABLES_GAP.md`. **Read §16 before planning application
work.**

**Meeting 8 (2026-07-31) — latest word.** *"You don't need to contain all the stuff — you just
need to make sure that what you have done looks good, looks perfect, looks great."*
- **Results accepted; stop measuring.** Groth16 = slower generation, smaller proof; LaZer =
  fast generation, much larger proof; PQ proof sizes far above classical. *"It's what we
  expected."* Configuration 1 legitimately carries no π (the classical protocol specifies only
  DLEQ). **Scope frozen:** no second signature scheme, no zkVM/RISC-V, no functional
  signatures, no live-network deployment — polish what exists.
- **Terminology rulings, report-wide, every occurrence:** (1) **"transaction" must not mean
  "the signed message"** — in a Bitcoin context it names a predefined format, so use a
  different term for the signed message; (2) **PreSign / PreVerify / Adapt / Ext are
  `functions`, not `protocols`** ("protocol" implies consensus-level design).
- **Report:** word count need **not** be proportional to rubric weighting — more about
  results, less about background. Figure placement rules are in the FOCUS section. Chapter 5
  is titled "Conclusion, critical reflection and future work".
- **Sequencing:** finish Bitcoin/UTXO before any further EVM/Naysayer work; the EVM is a
  discussion of a more advanced solution *after* a complete Bitcoin solution.
- Satisfied since: the Bitcoin transaction breakdown + two diagrams, the hint experiment
  (reframed by Royce — see Scope discipline), the EVM and IPFS write-ups, the `Adapt`
  explanation. Outstanding from this meeting: the 6–8 minute slide deck.

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
- **Meetings:** `meetingN_cleaned_transcript.md` (+ `meeting8_summary.md`); Meeting-6 directives
  in `docs/04-evaluation/SUPERVISOR_DELIVERABLES_GAP.md`.
- **Papers:** LAS = eprint 2020/845 (Esgin, Ersoy, Erkin) · poqeth = eprint 2025/091 ·
  `NIST.FIPS.204.pdf` (+ `docs/paper/NIST_FIPS_204.md`).
- **Design/math/results:** `docs/LAS.md` · theory↔code: `docs/02-methodology/THEORY_IMPL_BRIDGE.md`.
- **Experiment write-ups** (`docs/03-results/`): `MLDSA_HINT_EXPERIMENT.md`,
  `STATEMENT_COMPRESSION_EXPERIMENT.md`, `PROOF_AMORTISATION_EXPERIMENT.md`,
  `SUCCINCT_PQ_PROOF_EXPERIMENT.md`, `LAS-08-performance-measured.md`,
  `GAS_LIMIT_INVESTIGATION.md`.
- **Application:** `rust/las-swap/README.md`, `docs/02-methodology/STAGE2_UTXO_SWAP_PLAN.md`,
  `BITCOIN_TX_STRUCTURE.md`, `EVM_TX_STRUCTURE.md`, `docs/04-evaluation/IPFS_OFFCHAIN_STORAGE.md`.
- **Reproducibility:** `README.md` · function classification: `docs/02-methodology/FUNCTION_MAP.md`
  · plain-English explainer: `docs/01-introduction/LAS_WALKTHROUGH.md` · build order:
  `docs/04-evaluation/PROJECT_HISTORY_EXPLAINED.md`.
- **Assessment:** `MSc_Report_and_Video_Rubric.md` · writing guidance:
  `docs/references/Lecture5_ResearchWriting_2026_Lin.md`, `muthesis_formatting_rules.md`.
