# LAS Project — Consolidated Context (Meetings 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9 + 10 + 11)

> **Meeting 6 is now IN this file, at §15A** (2026-08-15) — it sits between §15 (M5) and §16
> (M7) so the reading order stays chronological, and it is lettered rather than numbered so
> that §16/§17/§18/§19 keep the numbers CLAUDE.md and the docs already cite. Its
> deliverable **status matrix** (✅/🟡/⬜ per item) stays in
> `docs/04-evaluation/SUPERVISOR_DELIVERABLES_GAP.md`, which is where it was first tracked;
> §15A carries the *directives*, that file carries *how far each one got*.
> **Meeting 7 (§16) retargets Stage 2 from the EVM to Bitcoin/UTXO. Meeting 8 (§17) froze
> the feature set. Meeting 9 (§18) accepted everything reported. Meeting 10 (§19) is the
> LATEST WORD: the mock presentation was reviewed — content accepted, presentation to be
> reworked — and the implementation is FROZEN, with the remaining time going to verifying
> existing results, the writing and the video. Read §16 and §19 before planning any work.**
>
> **THE canonical objectives/context file** (merges Meetings 1, 2, 3, 4, 5, 7, 8 and 9). As
> of 2026-06-13 the older `LAS_OBJECTIVES_FOR_TOP_MARK.md`, `las-objectives-meeting2.md`
> and `docs/archive/LAS_PROJECT_HANDOFF.md` have been **deleted** — their content is
> fully captured here (objectives), in `CLAUDE.md` (project context), and in
> `docs/STATUS.md` (live deliverable/test checklist). This file = the *spec*;
> `docs/STATUS.md` = the *progress tracker*.
> Provenance tags: **[M1]** = Meeting 1 · **[M2]** = Meeting 2 (2026-06-08) ·
> **[M3]** = Meeting 3 (2026-06-18) · **[M4]** = Meeting 4 (June 2026, exact date
> not in transcript) · **[M5]** = Meeting 5 (2026-07-06) · **[M7]** = Meeting 7
> (2026-07-24) · **[M8]** = Meeting 8 (2026-07-31) · **[M9]** = Meeting 9 (2026-08-07) ·
> **[M1→M2]** = set in M1, revised in M2.
> Where the meetings conflict, the **later meeting wins** (M2 over M1; M3 over M2
> for evaluation rigour / fairness — M3 raised the Stage-1 bar, see §13; **M4 raised
> the Stage-1 *presentation* bar and showed Stage 1 is NOT yet supervisor-signed-off**,
> see §14; **M5 added the explainability/reproducibility artefacts — high-level
> diagrams, C⇄Rust cross-check, README split — that must accompany the Stage-1
> figures**, see §15; **M7 moved Stage 2 to Bitcoin/UTXO**, see §16; **M8 froze the
> feature set and made the Bitcoin transaction breakdown the single remaining
> requirement**, see §17; **M9 accepted that breakdown and everything else reported, and
> turned the queue into report fixes + one benchmark**, see §18). **M9 is the latest
> word.**

---

## 1. Project identity & framing [M1]

- MSc Cybersecurity thesis, University of Manchester, supervisor **Wang Zhipeng**. ~3.5 months / <12 weeks remaining as of M2.
- Target: the underexplored **"exotic × post-quantum" quadrant** of the 2×2 signature taxonomy (basic/exotic × classical/post-quantum).
- Primary scheme: **LAS — Lattice-based Adaptor Signatures (eprint 2020/845)**, built by modifying the cloned **CRYSTALS-Dilithium reference repo** (digital-signature part only; KEM code untouched [M2]).
- **Novelty claims to protect everywhere:** (i) first public implementation of LAS; (ii) first integration of an *exotic* PQ signature into a blockchain setting — poqeth covered only *basic* PQ signatures.

## 2. Reference map [M1]

| Reference | Role |
|---|---|
| eprint **2022/1151** | Survey of PQ exotic signatures — the "map" |
| **poqeth** repo + eprint **2025/091** | Integration template: basic PQ signatures on Ethereum |
| eprint **2020/845** (LAS) | The actual algorithm spec (found independently; M2 confirmed it is the right paper and "still the most straightforward starting point — few clearly better results since 2020") |
| pq-crystals/**dilithium** | Implementation base |

[M2] Ongoing duty: check Google Scholar forward-citations of 2020/845 for newer LAS-like constructions; compare follow-ups against the original.

## 3. Scheme decisions (settled, M2-confirmed)

- **LAS chosen** [M1]: lattice-based, 128-bit M-SIS/M-LWE security, direct Dilithium C reuse — the practical PQ adaptor signature for a working implementation. **Focus is LAS only** [Royce, 2026]: no alternative-PQ-scheme comparison in scope.
- **Variant (B)** [M1, corrected against the paper]: Adapt **adds** the witness (`σ = (c, ẑ + y)`); Ext **subtracts** (`y = z − ẑ`); statement folded into the Fiat-Shamir hash (`c = H(pk, w + Y, M)`); PreSign rejection bound `‖ẑ‖∞ > γ − κ − 1`.
- **Simplified scheme is mandatory** [M1, M2 reaffirmed]: Power2Round, hint vector `h`, and high/low-bit decomposition **disabled** — the clean identity `Az − ct = w + Y` only holds without them. [M2] Wang: follow the paper's algorithm; the reference implementation's extra checks/optimizations may be simplified for a prototype.
- **Ternary witnesses** generated via KeyGen (not gamma1-sized) [M1].
- **Language: C confirmed** [M2] (reference code is C; Rust acceptable but learning curve; demo layer may be Python/Solidity).
- Upstream Dilithium left untouched — clean diff = visible contribution [M1].

## 4. Parameters [M1→M2]

- [M1] Target paper parameters: `d=256, n=ℓ=4, q≈2²⁴, κ=60, γ=122880`.
- [M2] **Revision — start from the implementation's parameters, do not change anything at the beginning.** Initial builds use Dilithium's `q = 8380417 ≈ 2²³`. First task is making the existing implementation run as-is.
- ~~Reconciliation: migration to paper params is a *later, documented* step.~~
  **[Royce, 2026-08-03 — SUPERSEDES the two bullets above for parameter authority]
  NIST FIPS 204 (ML-DSA) is the parameter authority, not the LAS paper.** `q = 8380417`
  is ML-DSA's modulus and is therefore **correct**, not a deviation to be apologised for.
  **The `q≈2²⁴` migration is DROPPED, not deferred** — NIST does not ask for it, so it is
  no longer a goal and must not reappear in any work queue or future-work list. Where the
  paper and FIPS 204 differ on a *parameter*, follow FIPS 204 and say so in the report.
  (Report *mathematical notation* is unaffected — it still follows the LAS paper's
  symbols; see `docs/paper/LAS_2020_845_NOTATION.md`.)
- Standing technical concern [M1+M2]: after Adapt adds the witness, the result must remain within the rejection bound — handle and document the post-adaptation norm check explicitly.

## 5. Official project structure [M1→M2 merged]

M2 made the two-stage structure the spine; M1's success ladder maps onto it:

**Stage 1 — Standalone LAS** (= M1 "minimum", Wang: "already a manageable problem")
Implement LAS by modifying Dilithium; benchmark against pure Dilithium. Understand KeyGen/Sign/Verify + PreSign/PreVerify/Adapt/Ext; exposition focuses on the *differences* from basic Dilithium [M1].

**Stage 2 — Blockchain application** (= M1 "better")
**Atomic swap or fair exchange** on a **local testnet / private chain** [M2]. Method: take an existing adaptor-signature-based atomic-swap construction and **replace only the signature component** with LAS — do not rebuild application logic [M2]. Compare gas cost / application efficiency.

**Remaining work** (= M1 "best"). **[Royce, 2026-08-03] This tier is NOT "optional".**
Everything still open is **mandatory**; the items differ only in **order**, ranked by
(1) what Wang instructed/prioritised, (2) low-hanging fruit, (3) novelty value to the
report, (4) feasibility in the time left. The words "optional", "if time permits",
"stretch" and "bonus" are retired from this project's vocabulary — including in the
report's future-work section.
- ~~**AMHL / multi-hop payment demo**~~ — **DROPPED [Royce, 2026-08-03].** Out of the
  project entirely: not a bonus, not future work, not a deliverable. `ref/amhl.{c,h}` is
  dead legacy; existing AMHL claims in the report/docs are a **removal task**. (History:
  M1 chose it as the next stage, M2 demoted it.)
- **NIST ML-DSA hint experiment** [M8 §17.6, redefined by Royce 2026-08-03] — build LAS
  on ML-DSA **as NIST specifies it** (hint vector, Power2Round, high/low decomposition
  enabled) to *demonstrate* that NIST's construction must be modified for LAS to work.
  Highest novelty value in the queue; Wang named it for the week after Meeting 8.
- **On-chain LAS verification** (precompile or zk proof) — the swap protocol + gas floor
  are already measured (`evm/`); native EVM verification is the open piece. M8 §17.4:
  only after the Bitcoin/UTXO solution is fully finished.
- ~~**Parameter migration** to the paper's `q≈2²⁴`~~ — **DROPPED**, see §4: NIST is the
  parameter authority and does not ask for it.
- ~~**Second LAS-family scheme**~~ — **ruled out by Wang in M8 §17.7** (no time; polish
  what exists). Consensus-level multisigs (Squirrel/Chipmunk-style) remain out of scope.

## 6. Scope rules — hard fences

- [M1] **No security proofs, no detailed security analysis.** Treat M-SIS/M-LWE hardness as a black box.
- [M2] **Out of scope:** Ethereum-consensus multi-signatures (requires modifying consensus/client), blind signatures, group signatures, heavy ZKP/MPC constructions. One-paragraph related-work mention max.
- [M2] Adaptor signatures are application-layer: no consensus or client modification needed — keep it that way.
- [M2] Low-level math (polynomial rings, NTT/GMP internals) need not be mastered upfront; use existing libraries as standard APIs, modify only when necessary. LLM assistance permitted for reading notation, but **verify against primary sources**.

## 7. Benchmarks — two types × two baselines [M1+M2]

[M1] Two benchmark *types*: (i) the signature itself, (ii) the application.
[M2] Two *baselines*, with named metrics: **keygen time · public-key size · signature size · signing time · verification time**, plus **gas/application cost**.

| Comparison | Source | Notes |
|---|---|---|
| LAS vs **pure Dilithium** | [M1, M2] | Same machine, same build flags; medians over ≥1000 iters + std dev |
| LAS vs **classical adaptor sig** (Schnorr/ECDSA-based — *not* plain ECDSA) | [M2 new] | Reuse existing implementations/published numbers; cite hardware caveats |
| Atomic swap: classical adaptor vs LAS | [M1 type-ii, M2 confirmed] | Gas + latency on local chain |

[M2] Framing instruction: "let the data speak" — quantify the PQ tradeoff (communication = key/sig sizes; computation = sign/verify cost), don't assert it.

[M3] **Rigour requirements — the Stage-1 benchmark must be *defensible*:**
- (a) **Repetition + dispersion:** run each operation **≥5 times on the same
  machine** (more is better) and report the **mean with variance / standard
  deviation** (error bars), never single shots — randomness and machine load make
  single numbers unreliable. (`bench_levels.c`: 10 runs × 1000 iters/op, mean ± SD.)
- (b) **Component-level size breakdown:** don't just say "larger". Decompose every
  object into its components — challenge `c`, response `z`, public key, statement
  `Y`, witness, pre-signature — give the bytes of each, and name *which component
  drives the size*. (`z` dominates the signature, ~98–99%.)
- (c) **Separate the two cost axes:** *computation cost* = timings
  (KeyGen/Sign/Verify/PreSign/PreVerify/Adapt/Ext); *communication cost* = bytes
  stored/transmitted (pk, sig, pre-sig, `Y`, witness).
- (d) **Fair, same-security-level comparison:** state the parameter set for *every*
  scheme compared, and never pit a weaker-parameter LAS against a stronger-parameter
  Dilithium as if equal. LAS's primitive parameters `(n,ℓ,κ)` are made overridable
  and matched to each Dilithium mode's `(K,L,τ)` (dimension-level match; security
  proofs out of scope), benchmarked at the paper set + D2/D3/D5.
- (e) **The *primary* comparison, stated precisely:** it is the explicit **base
  path (`c = H(pk, w, M)`, no `Y`) vs the LAS adaptor path (`c = H(pk, w+Y, M)`)** —
  see §13.5, implemented in `ref/test/bench_levels.c`.

## 8. Report structure [M2, fixed by supervisor]

Main body: motivation/framing → related work (one paragraph each: survey 2022/1151, poqeth precedent, out-of-scope families) → **high-level design** → **table of Dilithium functions reused vs modified vs added** → key decisions (variant B, simplified scheme, parameter choice, post-Adapt bound handling) → **benchmark results** (both baselines) → critical analysis (weak pre-signature adaptability / knowledge gap [M1, from the paper]; simplified-scheme tradeoffs) → conclusions.
Appendix only: selected code (PreSign/Adapt/Ext cores), full benchmark tables. Page/word limit applies — no full source dump.

[M3] **Write the report *while* implementing** — don't defer all writing to the end (you forget Step-1 detail while doing Step 2). Wang's suggested spine: introduction → background/motivation → methodology → results/evaluation → conclusion/future work → critical reflection. Reconcile with the **official** format (see `CLAUDE.md`): there is *no separate Background section* — fold motivation and a concise literature review into the Introduction; Abstract and Conclusion are mandatory standalone sections. Report authored in LaTeX (`report/latex/`, `muthesis.cls`).

## 9. Marking-rubric overlay [M1, M2-strengthened]

Distinction (70–80+) = novel contribution + rigorous evaluation against proper baselines + honest critique + clean writing. Code alone never reaches distinction — the evaluation and analysis around it do.
[M2 bonus] The second baseline turns the comparison into a 2×2 story (basic/exotic × classical/PQ) mirroring the project's framing quadrant — examiners reward the symmetry.
[M2] **Publication:** implementation/engineering venues (CHES-style). No marks-vs-publication tension — publishability implies a strong, reviewed contribution. Requires deeper analysis than implementation alone; revisit after report draft.
[M3] **Project category — frame it correctly:** this is a **system implementation / evaluation built on existing research**, *not* a new-cryptographic-protocol project. Emphasise "this is *your* system". A genuinely new research question (e.g. a *functional* adaptor signature, or a lattice version of a more advanced primitive) is optional future work, not required for the current marks.

## 10. Current status & near-term deliverables

**Status (2026-06-13 code-complete; M4 caveat below):** *All required Meeting-2
deliverables done & tested.* Stage 1 (LAS + benchmark vs Dilithium), Stage 2 (atomic
swap + scriptless HTLC ledger), **both benchmark baselines** (vs Dilithium-3, vs
classical ECDSA-adaptor), function map, reproducibility README — all ✅. Bonus done:
AMHL multi-hop, byte serialisation + `las_verify_packed`, deterministic API + pinned
KATs. **Open:** report draft (in progress, `report/REPORT_DRAFT.md`), video, and the
optional tier (real gas, param migration to 2²⁴, on-chain LAS verification, second
LAS-family scheme). Full deliverable/test matrix: **`docs/STATUS.md`**.

> **[M4] reality check — the code is ahead of supervisor sign-off.** Despite the
> "all done" status above, in Meeting 4 Wang reviewed the actual figures and did
> **not** sign off Stage 1: the numbers are right but the **presentation is not yet
> defensible** (confusing paper/L2/L3/L5 labels, missing parameter annotations,
> cumulative timing shown instead of per-operation, no PR opened for him to review).
> The single active priority is therefore to **finish the Stage-1 benchmark
> *presentation* and open the baseline-vs-LAS PR** — not to add application code. See
> §14. Stage 2 (atomic swap), local EVM gas, Foundry, and the classical comparison
> are all explicitly deferred until Stage 1 is signed off.
>
> **[M5] update — Stage 1 still not signed off; the bar is now explainability +
> reproducibility.** Meeting 5 (2026-07-06) did not sign off Stage 1 either. Wang's
> line: *"You already have implementation and numbers. Now you must organise them so
> another person can understand, verify, and reproduce them."* The active priority is
> now the **presentation/explainability package**: high-level API + repo-structure
> diagrams, a reused/modified/new components summary, a C⇄Rust size cross-check, a
> tidied benchmark/rejection story, a short+extended README, and a 1–2 slide summary —
> **not** application code. Classical-adaptor comparison is pulled back in as **table
> columns** for the Stage-1 results (refines M4.8); the blockchain/gas classical-vs-LAS
> comparison stays Stage 2. See §15.

**Pre-Meeting-3 deliverables [M2, explicit asks] — all ✅:**
1. Dilithium reference builds & runs on own machine — commit hash + toolchain recorded in `README.md`. ✅
2. Literature check on LAS follow-ups (LAS + survey 2022/1151) — captured in `docs/LAS.md §1.1`. ✅
3. Language decision — C. ✅
4. **Function map** (call-as-is / modify / new; 0 upstream functions modified) — `docs/02-methodology/FUNCTION_MAP.md`. ✅

**Meeting-3 deliverables [M3, 2026-06-18 asks] — Stage-1 defensibility (see §13):**
1. Two-branch code-diff view (`dilithium-baseline` vs `main`) + `docs/02-methodology/CODE_DIFF_VIEW.md`. ✅
2. Fair same-security-level benchmark — overridable `LAS_N/LAS_ELL/LAS_KAPPA`, `ref/test/bench_levels.c` at paper/D2/D3/D5; primary comparison = base path vs adaptor path. ✅
3. ≥5-run mean ± SD on the same machine (10×1000). ✅
4. Component-level size breakdown (c / z / pk / Y / witness / pre-sig; `z` ≈ 98–99%). ✅
5. Report authored while implementing, in LaTeX (`report/latex/`). ◐ in progress.
6. Confirm any separate code/artefact deadline (graded = report + video). ☐ to confirm with supervisor.

## 11. Risk register [M2-updated]

| Risk | Status |
|---|---|
| Time overrun | Mitigated — Stages 1–2 done; rest is bonus. Wang: "focus on the first stage, it should be fine." |
| Parameter mismatch 2²⁴ vs 2²³ | Supervisor-sanctioned starting point, not a bug. Document the choice. |
| AMHL complexity | Defused — optional; only after approved report draft. |
| Scope creep (consensus multisig, blind/group, ZKP/MPC) | Hard-fenced in writing by supervisor. |

## 12. Timeline anchor [M2]

~12 weeks remaining. Suggested: **wks 1–2** function map + parameter reconciliation + benchmark hardening; **wks 3–4** classical-adaptor baseline + gas measurements; **wks 5–8** report draft (Stages 1–2 only); **wks 9+** optional tier *only if* draft supervisor-approved; final weeks: polish, appendix, reproducibility README.

## 13. Meeting-3 directives [M3, 2026-06-18 — make Stage 1 defensible]

Meeting 3 did **not** change scope; it raised the **evaluation bar** for Stage 1
and fixed repository/reporting hygiene. Where M3 refines M2's benchmark asks, **M3
is the latest word**. Wang's framing: the project has made good progress — the next
priority is to make the implementation and evaluation *defensible*, not to add
features. **Finish the first-stage benchmark correctly before expanding the
application layer.**

**13.1 Two-branch repo for a clean code-diff.** Keep one branch with the *original*
Dilithium code and the `main` branch with the LAS work, so the examiner (and Wang)
can see at a glance which files were **reused unchanged, modified, or newly added**
(a PR / branch-compare view). This is the visible-contribution artefact.
*Done:* `dilithium-baseline` vs `main`; `docs/02-methodology/CODE_DIFF_VIEW.md`;
`docs/02-methodology/FUNCTION_MAP.md` gives the per-function classification.

**13.2 Component-level size analysis.** Do not merely report that a LAS key or
signature is "larger". Break each object into its components — challenge `c`,
response `z`, public key, statement `Y`, witness, pre-signature — give the bytes of
each, and explain *which component drives the size* (finding: `z` dominates,
~98–99%). Separate the two cost axes: **computation** = timings; **communication** =
bytes (pk, sig, pre-sig, `Y`, witness).

**13.3 Benchmark repetition & dispersion.** Run each measured operation **≥5 times**
(more is better) on the **same machine**, and report the **mean with variance /
standard deviation** (error bars). Randomness and machine load cause variance — show
it. (`bench_levels.c`: 10 runs × 1000 iters/op, mean ± sample SD.)

**13.4 Fair, same-security-level comparison.** A comparison is fair only if the
schemes sit at the **same (or explicitly stated) security level**. State the
parameter set for *every* scheme compared (ECDSA, ECDSA-adaptor, LAS, Dilithium),
and never compare a weaker-parameter LAS against a stronger-parameter Dilithium as
if equal. LAS's primitive parameters `(n, ℓ, κ)` are matched to each Dilithium
mode's `(K, L, τ)` so public-key/secret dimensions and challenge weight line up — a
*dimension-level* match (not a formal bit-security claim; proofs out of scope). Run
at more than one level (paper set + D2/D3/D5), the way papers compare across NIST
levels. If exact matching is impossible, state the limitation honestly.

**13.5 The primary Stage-1 comparison, stated precisely.** It is **not** a vague
"LAS vs its own base". It is the **simplified Dilithium-style base signature path**
versus the **LAS adaptor path** — a primary algorithm-matched, parameter-matched
adaptor-overhead benchmark over **separate base/adaptor modules over matched
parameters and primitives**:
- *Base path:* `Sign` uses `c = H(pk, w, M)`; `Verify` recomputes `c = H(pk, w', M)`;
  **no statement `Y` enters the hash**.
- *LAS adaptor path:* `PreSign` uses `c = H(pk, w + Y, M)`; `PreVerify` recomputes
  `c = H(pk, w' + Y, M)` (`Y` is the adaptor lock); `Adapt` sets `z = ẑ + y`; then
  ordinary `Verify` passes **without an explicit `+Y`** because
  `A(ẑ + y) − c·t = (Aẑ − c·t) + A·y = w' + Y` (since `Y = A·y`). The leaked witness
  `y = z − ẑ` is exactly what makes a swap atomic. (Benchmark: `ref/test/bench_levels.c`.)

**13.6 Project category & research question.** Frame this as a **system
implementation / evaluation built on existing research** — *not* a new cryptographic
protocol; emphasise "this is *your* system". A genuinely new research question
(functional adaptor signatures, a lattice version of a more advanced primitive) is
possible future work, not required now.

**13.7 Write the report while implementing.** Start now; don't defer all writing to
the end. Wang's spine: introduction → background/motivation → methodology →
results/evaluation → conclusion/future work → critical reflection — reconciled with
the *official* format (no separate Background; concise lit review inside the
Introduction; standalone Abstract + Conclusion). LaTeX in `report/latex/`.

**13.8 Check the artefact/code deadline.** Confirm whether code must be submitted
earlier than the report; if there is no separate deadline, keep polishing code
alongside writing, but manage time. The graded deliverables are the **report and the
video**.

**Meeting-3 next-week priority (verbatim intent):** finish the Stage-1 benchmark,
make the numbers correct and the security-level comparison consistent, and set up the
two-branch code-diff view. Atomic swap / toy ledger remains Stage-2 — do not
over-focus on it yet.

## 14. Meeting-4 directives [M4, June 2026 — make the Stage-1 benchmark *presentation* defensible]

Meeting 4 did **not** change scope or add features. Wang reviewed the actual
benchmark figures on screen and concluded the **results are the right kind of
numbers, but the presentation is not yet defensible**. Where M4 refines M3's
evaluation asks, **M4 is the latest word**. Headline: *the code largely exists; the
figures, their labels, and the explanation do not yet stand on their own.* **Stage 1
is therefore NOT yet supervisor-signed-off** — finishing the benchmark *presentation*
(not more application code) is the single active priority.

**14.1 Figure labels are confusing — define every setting.** The labels "paper",
"L2", "L3", "L5" are not self-explanatory: a reader cannot tell whether they mean LAS
parameter sets, basic-Dilithium parameter sets, or simplified-Dilithium-derived
levels. Every figure/caption must state: "paper" = the **LAS-paper** setting;
"L2/L3/L5-like" = **simplified-Dilithium-derived LAS** parameter sets matched to
Dilithium modes 2/3/5. Make explicit which bars are the **base signature** (blue) vs
**LAS** (orange).

**14.2 Show the key parameters beside every setting.** Annotate each setting with its
actual parameters — `n`, `ℓ`, `M = n+ℓ`, `κ`, `γ`, `N`, security-level label — in the
figure caption or an adjacent table, so the parameter *differences* between
paper/L2/L3/L5 are visible. Wang: comparisons without stated parameters are
misleading. (Royce conceded the paper setting and the L-settings are **not** identical
parameters — that difference must be on the page.)

**14.3 Per-operation timing must be the PRIMARY timing result — not cumulative.**
[Refines M3.] Report `KeyGen`, `Sign`, `Verify`, `PreSign`, `PreVerify`, `Adapt`,
`Ext` **independently and separately**. Rationale (Wang, explicit): in practice these
run on different machines / by different participants (one party signs, another
verifies later), so a single combined "workflow" number is unrealistic. Cumulative /
end-to-end time may appear as **one additional** metric, but the headline must be
per-operation. Benchmarking everything on **one machine is acceptable** — just report
per operation. *Action:* the current lead figure is cumulative time; demote it and
promote the per-operation breakdown.

**14.4 Communication cost = explicit component breakdown.** [Refines M3.2.] Report
bytes for: public key, secret key, challenge `c`, response `z`, signature `(c, z)`,
pre-signature `(c, ẑ)`, adapted signature, and statement `Y`. If signature ≈
pre-signature ≈ adapted signature in size, **explain why** (adapting only adds the
small ternary witness `y` to `ẑ`, so `‖z‖∞`, and therefore the packed size, barely
changes). Consider a clearer/zoomed scale so the small differences are visible.

**14.5 State the benchmark machine.** Record CPU, OS / WSL environment, compiler +
version, build flags, iteration count, and number of runs — so different timings on
another machine are explained. (Already in the methodology; ensure it travels *with*
the figures, since Wang noted he will get different numbers on his own PC.)

**14.6 Keep only 3–4 main figures; summarise findings in 2–3 sentences.** Pick the
3–4 figures that carry the most important findings; move the rest to the appendix /
supporting evidence. Provide a 2–3 sentence plain-language takeaway, e.g. "compared
with the simplified Dilithium-style base signature, LAS increases communication by
×… and computation by …; the adaptor operations (`Adapt`/`Ext`) add ≈… overhead."
Don't make the reader infer the conclusion from many figures.

**14.7 Open a PR / branch-compare and invite Wang.** Open a pull request comparing
the clean Dilithium baseline against the LAS `main` branch and **invite Wang to
review**, so he can inspect which files were reused / modified / added and verify the
implementation matches the intended algorithm. The supporting artefacts already exist
(`dilithium-baseline` branch, `docs/02-methodology/CODE_DIFF_VIEW.md`, `docs/02-methodology/FUNCTION_MAP.md`) — but
the **PR itself was not yet opened/shared** as of M4, and Royce said the repo was "not
fully updated yet." This is a named next-meeting deliverable.

**14.8 Stage ordering reaffirmed — Stage 1 before everything else.** Atomic swap (only
a *simplified* one exists, not the paper's full construction), local EVM gas, Foundry,
and the classical adaptor-signature comparison are all **explicitly deferred** to
later stages. Verbatim intent: "First make the standalone LAS benchmark perfect; then
discuss atomic swap, local EVM gas, Foundry, and classical comparison." Proceed step
by step — Wang expects to "revise things again and again", so do not let application
work displace finishing Stage 1.

**Meeting-4 next-meeting deliverables (verbatim intent):**
1. 3–4 cleaned benchmark figures with self-explanatory labels;
2. key parameters shown for every setting (paper / L2 / L3 / L5);
3. per-operation timing tables (KeyGen / Sign / Verify / PreSign / PreVerify / Adapt / Ext, each independent);
4. component-level communication-size tables (pk, sk, `c`, `z`, `Y`, sig, pre-sig, adapted sig);
5. a 2–3 sentence statement of the main findings;
6. a GitHub PR / branch diff from clean Dilithium → LAS, with Wang invited.

## 15. Meeting-5 directives [M5, 2026-07-06 — make Stage 1 explainable, reproducible, and cross-validated (C ⇄ Rust)]

Meeting 5 did **not** change scope or the Stage-1-first ordering. It doubled down on
M4's verdict (*results are the right numbers, presentation is not yet defensible*) and
added concrete **explainability + reproducibility artefacts**, plus promoted the **Rust
implementation** to an explicit, cross-checked deliverable that goes in the report
alongside C. The meeting was "less about adding new code and more about making the
existing implementation and benchmarks **understandable, reproducible, and
defensible**." Wang's closing framing (verbatim intent):

> *"You already have implementation and numbers. Now you must organise them so another
> person can understand, verify, and reproduce them."*

Where M5 refines earlier meetings, **M5 is the latest word.** Stage 1 is **still not
supervisor-signed-off**; the deliverable is now a *presentation package*, not more code.

**15.1 WSL benchmarking is acceptable — but state the environment.** Running under
**WSL/WSL2 on Windows is fine**; no bare-metal or separate Linux machine is required.
The only hard rule: **every compared scheme runs on the same machine** so the
comparison is fair. Record and ship **with the figures**: CPU, RAM (if possible), OS +
WSL/Windows details, compiler + version, build flags, benchmark framework, iteration
count, and number of runs. Absolute numbers differing on another machine is expected and
not a problem — the environment just has to be documented. (Reinforces M4.5.)

**15.2 High-level API-flow diagram (basic → LAS).** Start the explanation from the
**basic signature API — `KeyGen`, `Sign`, `Verify`** — and only then show how LAS
**adds/modifies** the flow with **`PreSign`, `PreVerify`, `Adapt`, `Ext`**. Explain the
basic path *first*, LAS *second*. Show **where the statement `Y` is set up** (relation
Gen / the adaptor part) — Wang explicitly asked whether it happens in keygen, a
pre-keygen, or another process. Highlight **reused primitives vs modified/new
functions**. The three basic APIs are the *entry point*; dive into file/function detail
only after the conceptual flow is clear. ("If you start from detailed code immediately,
people get lost.")

**15.3 Repository-structure diagram (base C / LAS C / LAS Rust).** Generate a repo
**structure diagram** showing three things side by side: the **original Dilithium base
implementation**, the **C LAS implementation**, and the **Rust LAS implementation**.
**Mark the reused primitives** — NTT, SHAKE/FIPS 202, `randombytes`, polynomial
arithmetic — as shared building blocks. Do **not** lead with "I looked at `sign.c` then
made separate C files" (Wang: "that is already too detailed") — high-level structure and
important functions first.

**15.4 Reused / modified / newly-added components table — presented high-level.** Give
an explicit "**this is the existing scheme, this is the existing implementation, these
are the parts I added or modified**" summary, so a reader does not have to open every
file. For **unmodified** parts, treat each as a **box** ("keygen uses the existing
primitives to generate the keys") and state **why they were not modified**; for
**modified/new** parts state **why the change was needed**. For **each new LAS function**
explain *why it was added and why it is needed*. (`docs/02-methodology/FUNCTION_MAP.md`
already has the per-function classification — M5 wants a **high-level front** on it.)

**15.5 A high-to-low written summary of what changed (simplified Dilithium → LAS).**
Write this **this week**. Conceptual flow first, file/function detail after. This is the
"convince me" artefact: Wang can see results but not *how* they were produced — "I do not
know where to start verifying it. There are too many details. Start from the high level."
You need not master every ML-DSA math/impl detail, but you must be able to say, at a high
level, what each component does and **why you left it unmodified or changed it**.
Additionally (14:55, recovered from the original transcript): the report must also
**summarise the challenges met during the modification** — not only what changed, but
what was hard about changing it.

**15.6 Benchmark methodology, stated explicitly.**
- **Rust / Criterion is accepted and encouraged** — report **sample count (~300)**,
  **warm-up (~3 s)**, **mean**, **median**, **SD**, and confidence intervals if available.
  Proper statistics from Criterion may be *better* than a manual benchmark.
- **The Criterion distribution/statistics output itself is presentation material**
  (27:39–27:45, recovered from the original transcript): Wang, on seeing the PreSign
  distribution plot — "You can report this, I think this is better than what you write
  yourself… can you do the screencast?" I.e. the Criterion visuals (distribution +
  mean/CI) go **in the report and in the screencast/video**, not just in logs.
- **C:** if the same framework is unavailable, an **equivalent repeated-run harness or a
  script that runs many times and records the numbers** is fine; compute **mean ± SD**.
- **Communication sizes are fixed** — report once, no dispersion. **Timing must carry
  average + variation** (≥5 runs, more is better — reaffirms M3.3). Figures may be
  generated from the C repeated-run output.

**15.7 C ⇄ Rust cross-validation (NEW deliverable).** Two implementations now exist
(C + a 2026 Rust GitHub base converted to LAS). **Verify that the key sizes and signature
sizes MATCH between C and Rust** — they do (communication size identical; timing differs,
which is expected and fine). **Matching sizes is the evidence that the implementation is
consistent/correct** ("since the sizes are exactly the same, that is good — it suggests
the implementation is consistent"). **Both implementations go in the report.**

**15.8 Rejection sampling — theory AND measurement, in your own words.** Expected
**≈2.7 attempts** (≈2.67) per pre-signature and **≈36.8% acceptance**, derived by
plugging *this build's* parameters into the paper's rejection-sampling formula. You must
**explain in your own words why this makes sense** (a reader/examiner will ask), and you
must **show the MEASURED attempts from the implementation** (the Sign vs PreSign
counters), not just the theoretical value. Frame it as an interesting report insight:
rejection sampling is the hard part of benchmarking lattice signatures because it drives
the timing/variance. (This is the direct-counter figure already produced; M5 wants both
numbers side by side with a plain-language justification.)
Recovered nuance (28:49): the student's stated method is to run **enough samples that
the measured attempt counts for BOTH Sign and PreSign converge to the theoretical
expectation** — i.e. the theory-vs-measured attempts check is not only an insight but
the **sample-size validity argument** for the timing benchmark itself (if the counters
match theory, the run was long enough and the sampler is healthy). Present it that way.

**15.9 Classical adaptor-signature comparison — pulled into the Stage-1 tables
(refines M4.8).** M4 deferred the classical comparison; **M5 asks to bring it in now as
table columns** (the *blockchain/gas* classical-vs-LAS comparison still waits for
Stage 2). Concretely:
- Add columns to **both** a **computation** table and a **communication** table showing
  the **overhead / increase of LAS vs the classical baseline** (ECDSA/Schnorr-based
  adaptor signature — *not* plain ECDSA).
- Numbers are usable **only after verification**; until then "assume correct but verify".
- **State security-level + implementation caveats** clearly; LAS sits **≈ Dilithium
  level 2**, so compare against that level and say so.
- Ensure **corresponding operations** are compared — highlight the correspondence even
  where the two are not exactly the same object.
- **Include standard deviation.** An ECDSA-vs-LAS diagram is also welcome.
- **Presentation form — tables** (36:27–36:31, 38:39 recovered): the student asked
  "so you prefer it to be a table? I think this one should be a table" and Wang agreed;
  his closing ask is verbatim "**a table for computation and a table for
  communication**". Follow that: both cost comparisons are presented as tables.

**15.10 Reduce & organise the figures.** Do not dump every figure. Keep the **most
important**: **computation cost, communication cost, rejection attempts, and the
C/Rust comparison**. Move the rest to **appendix / supporting logs**. (Consistent with
M4.6's "keep only 3–4 main figures".)

**15.11 Short README + extended README.** The current README is **too detailed**. Add a
**short README with only the key reproduction commands**; keep the **extended README**
(or an appendix) for full detail. Reproducibility = a fresh reader can rebuild and run
from the short command list.

**15.12 Next-meeting deliverable: 1–2 slides with a diagram.** "One or two slides should
be fine — I want to see a picture or diagram." **Summarise the existing implementation,
compare it with the modified LAS, and refer to the pushed code** so Wang can see what was
modified. Goal: convince him **Stage 1 is correct, the methodology is clear, and the
results are organised**. Wang's named remaining task: **verification and validation of
the results** ("they are almost ready").

**15.13 Simplified LAS is sufficient — do NOT expand to full ML-DSA yet.** The
**simplified implementation is acceptable** for this project; the goal is to **show
feasibility of the implementation**. A **full ML-DSA-based LAS is future work**, or only
attempted **if time remains**. Extra practicality discussion / additional numbers are
optional. (Reaffirms the M1/M2 simplified-scheme mandate.)

**15.14 Stage-2 preview (only after Stage 1 is signed off): blockchain integration.**
Local/private chain, **probably Foundry**. Ordering Wang specified:
1. First answer **how adaptor signatures are implemented in a blockchain setting** using
   a **classical** adaptor-signature construction — **do not start with LAS**.
2. Get a **basic ECDSA/classical adaptor-signature workflow running** and generate the
   numbers (**transaction cost / on-chain vs off-chain cost**).
3. **Replace the signature component with LAS** and report the comparison.
A **post-quantum-secure atomic swap = the basic requirement met**; beyond it, consider
**functional adaptor signatures** or other blockchain applications. Open question raised:
a *real* atomic swap may need a **ZK proof that the extracted witness is valid/small**,
and that ZK proof would itself need to be **post-quantum secure** (a further component) —
"start from something standard first… let us see how far we can go." (Recovered detail,
50:14: the ZKP Wang referenced from his AI Security & Privacy material — likely
**Groth16** — "is not post-quantum secure", which is exactly why the PQ-ZKP question
arises; transcribed as "ZKPS16", so treat the name as probable, not certain.)

**Meeting-5 next-meeting deliverables (verbatim intent) — the checklist:**
1. A **1–2 slide summary** built around a **diagram**.
2. **High-level diagram of the BASE signature implementation** (`KeyGen`/`Sign`/`Verify`).
3. **High-level diagram of the LAS implementation** (base + `PreSign`/`PreVerify`/`Adapt`/`Ext`, with where `Y` is set up).
4. **Repository-structure diagram** (base C / LAS C / LAS Rust; reused primitives marked).
5. **Reused / modified / newly-added components table**, high-level.
6. **Benchmark tables:** one for **computation**, one for **communication**.
7. **Rejection-sampling theory vs measured attempts**, explained in your own words.
8. **C ⇄ Rust size cross-check** (pk/sig sizes match across the two implementations).
9. **Short README** with reproduction commands (plus the extended README).
10. **Verified classical adaptor-signature comparison columns** (if available), with security-level + implementation caveats.
11. The **machine/environment statement** travelling *with* the figures.
12. **Summary of the challenges met during the modification** (14:55, recovered) — report material.
13. **Criterion distribution/statistics visuals** reused in the report and the screencast/video (27:39–27:45, recovered).

## 15A. Meeting-6 directives [M6, between 2026-07-06 and 2026-07-19 — two-tier timing; the walkthrough sign-off; start studying the atomic swap]

Source: `meeting6_cleaned_transcript.md`. **Speaker polarity: Speaker 1 = Royce, Speaker 2 =
Wang** — the reverse of Meetings 1–3. **⚠ The date is not in the transcript**; it falls between
Meeting 5 (2026-07-06) and 2026-07-19. Lettered `15A` rather than numbered so §16–§19 keep the
numbers that CLAUDE.md and the docs already cite. **Deliverable status (✅/🟡/⬜ per item) lives
in `docs/04-evaluation/SUPERVISOR_DELIVERABLES_GAP.md` §1** — this section carries the
directives, that file carries how far each one got.

### 15A.1 THE ruling — two-tier timing, never folded together

Royce reported that serialization dominates some LAS operations: for `Adapt`, the core
algorithm is a small fraction of the time once pack/unpack is included (the meeting's own
figures: ≈160 µs core → ≈500 µs with pack/unpack; quote the generated macros, not these).

Wang's reasoning and ruling:
- **Packing does not change communication cost, only computation** — *"the computation time
  changes, but it won't reduce the communication cost."*
- It can be done **once, offline, or in advance** — unpack a public key once and reuse it
  across many verifications. So it is a **one-time / amortisable** cost, and *"I don't think
  it should be included in the headline verification time."*
- **⚠ Report BOTH timings as separate, clearly-labelled cases** — with and without
  pack/unpack — plus a discussion that deployments can amortise it (keep keys unpacked in
  memory). *"It's an interesting fact — it's also important to let people know why, so that in
  the future when they do this, they know what to expect."*
- Optimising the encoding (toward full-Dilithium compressed forms) is **explicitly out of
  scope**: *"that's not the focus point here… you just honestly report the numbers."*

**This is the origin of the two-tier reporting rule.** ⚠ The second tier was later **renamed
from "full-protocol" to "packed tier"** (2026-07-20) so it cannot be read as an end-to-end
protocol cost — use *core tier* / *packed tier* everywhere.

### 15A.2 The walkthrough — Wang quizzed the whole scheme and signed off

Wang had Royce derive the entire modified scheme on the spot: setup, key generation
(`t = A·r`), signing (`z = y + c·r`, `c = H(pk, w, M)`, `w = A·y`), verification
(`w′ = A·z − c·t`, re-hash and compare), then the adaptor path — challenge over `w + Y`, the
**ternary witness bound 1** so the adapted `z` stays inside the bound, PreVerify reconstructing
`w′ + Y`, Adapt (`z = ẑ + y`), and Ext by subtraction (`y = z − ẑ`). Verdict: *"in general,
maybe there are some details, but I think overall **the process looks correct**."*

**This is the Stage-1 correctness sign-off.** What remained after it was *reporting*, not
implementation. Wang's closing check: *"you can now write out the operations by yourself; you
know how it works. Now you should understand how to integrate it into specific applications."*

### 15A.3 The one measurement he asked for — PreSign attempts vs Sign attempts

**⚠ Measure and report the PreSign rejection-attempt count beside Sign's, at the same public
parameters.** Wang's reasoning: *"people might ask: you have a tighter [bound], so maybe you
try more times, right?"* Royce could not answer it on the spot. It is also the explanation for
why PreSign takes longer. **DONE** — it is now the hard-asserted rejection gate in every C and
Rust benchmark driver (Sign 2.71875 vs PreSign 2.77483 expected at the target set, 5σ), and it
must never be weakened, renamed, or inferred from timing ratios.

### 15A.4 Serialization: store the 32-byte challenge seed, not the polynomial

The challenge is no longer packed as a polynomial; only the hash output `c̃` is stored (32 B at
the then-current setting, per FIPS 204), and the challenge polynomial is re-derived with
`SampleInBall`. Wang: *"same seed, same output — that's fine"*, and **⚠ add the trade-off to
the report**: storing the expanded polynomial costs storage, sending only the seed costs
recomputation. ⚠ The width is now **per parameter set** (`LAS_CTILDEBYTES` = 32/48/64, keyed on
`(n, ℓ, κ)`), not a global 32 — see the naming section of `CLAUDE.md`.

### 15A.5 Comparisons — C vs Rust is fair; the ECDSA baseline packs internally

- **⚠ C vs Rust IS a fair comparison and should be reported** — *"if the parameters are the
  same, then why not?"* The meeting's observed spread was ≤ ≈1.07× (key generation largest).
- **⚠ Classical ECDSA baseline: a 3–4 column layout** — LAS without packing, LAS with packing,
  and ECDSA at its own API **annotated as internally packed**. The library is reused
  unmodified: *"you don't want to modify the ECDSA library, do you? So you just report that…
  that's not the crypto part; it's the implementation of the encoding and decoding."*

### 15A.6 Figures and the video

- **⚠ Export figures as high-quality PNG/PDF** — not full-screen screenshots, not a
  text-bearing screenshot PDF — and **⚠ fix the font size of labels and legends**: *"when you
  put a diagram like this in your report, it can be very unclear; people can't read the
  figures."* (Meeting 10 later confirmed the font-size fix landed: *"I like this figure."*)
- **Video (6–8 min): highlight only the essentials**, put the detail on the slides. *"When
  they're listening, you should be speaking to the most important points."*

### 15A.7 Next stage — study the atomic swap BEFORE coding it

**⚠ Understand it before implementing it:** *"you should understand it before you code it."*
Read how atomic swaps work and **why** — the history from hash-locked contracts (HTLCs), their
limitations, and why adaptor signatures replaced them — plus how *classical* adaptor signatures
are deployed in blockchains. Wang's reason is a report reason: *"finally you will write this in
your report, and as a reader people should understand the motivation."* This is the origin of
the background/motivation framing that Meeting 10 §19.1 asks to be pushed to the front of the
deck as well.

### 15A.8 Standing offer, and one out-of-scope note

The LAS paper's first author is **Wang's collaborator**, and Wang offered to relay specific
questions about the paper. The author's own newer directions — LAS's **imperfect correctness**,
and isogeny-based constructions — are **out of scope**: *"we are not that far; let's do this one
first."* ⚠ This is background about someone else's work, not a project deliverable; do not turn
it into a work item.

### 15A.9 Meeting-6 deliverable list

The twelve items are enumerated in `meeting6_cleaned_transcript.md` §D, with per-item status in
`docs/04-evaluation/SUPERVISOR_DELIVERABLES_GAP.md` §1: two-tier timing everywhere; the
pack/unpack discussion; tables **and** figures updated with both tiers; PreSign-vs-Sign attempt
counts; the C-vs-Rust table; the 3–4-column classical layout; the seed-vs-expanded challenge
trade-off; high-quality figures with readable fonts; PR kept updated; the atomic-swap study;
the 6–8 minute video; and the standing offer to relay questions to the LAS author.

## 16. Meeting-7 directives [M7, 2026-07-24 — retarget Stage 2 from the EVM to Bitcoin/UTXO]

Source: `meeting7_cleaned_transcript.md` (merged from the Teams/Stream transcript and the
phone recording). Meeting-6 directives are at **§15A** above; their per-item delivery status
stays in `docs/04-evaluation/SUPERVISOR_DELIVERABLES_GAP.md`.

### 16.1 THE DECISION — Stage 2 moves off the EVM

**Stage 2's application target changes from a smart-contract chain to Bitcoin / a
UTXO-based chain.** Wang's reasoning, in his own terms: full native on-chain LAS
verification "is a bit impossible" against the per-block/per-transaction gas limit, and
it is well known that adaptor signatures are used for atomic swaps on Bitcoin and other
UTXO-based chains **rather than** on smart-contract chains. Bitcoin has no gas limit —
only transaction fees — and the expensive work stays off-chain, so a Bitcoin-side demo
cannot be attacked as infeasible in practice: *"people won't argue that your solution is
not feasible."*

This supersedes the EVM-first framing of §5/§10 for the *application*. It does **not**
retract the EVM work already done: the measured ≈56.5 M gas native verifier and the
Naysayer variant become the **evidence for why** the UTXO venue was chosen, and are
retained as results. The EVM path is deferred to "if we have time".

Corroborating desk survey: `docs/04-evaluation/CLASSICAL_ADAPTOR_ONCHAIN_SURVEY.md`
(finds no classical Solidity adaptor verifier exists, because in a scriptless swap the
chain only ever sees an ordinary signature).

### 16.2 The three configurations to build and benchmark

Reuse an existing, actively-maintained classical atomic-swap repository's architecture
and replace its cryptography. Wang explicitly ordered the two substitutions:
**signatures first, ZKP second.**

| # | Signature | ZKP | Purpose |
|---|---|---|---|
| 1 | classical adaptor (ECDSA) | Groth16 | the classical reference point |
| 2 | **LAS** (post-quantum) | Groth16 | isolates the cost of the PQ *signature* |
| 3 | **LAS** (post-quantum) | **LaZer** (post-quantum) | the fully post-quantum stack |

Each is evaluated for performance; (3) is the goal, (2) is the intermediate step that
makes the signature's contribution separable from the proof system's.

### 16.3 Metrics — gas is replaced by time + communication

Because Bitcoin has no gas metric, the comparison axes become **execution time** and
**communication cost**, and Wang required communication to include **off-chain**
protocol messages, not just what lands on chain. He also asked for the *usability*
consequence to be discussed: heavy pre-transaction computation may not be feasible on a
phone and may need a dedicated PC — that is a reportable finding, not an aside.

### 16.4 Simplifications explicitly permitted

- **No real sockets/ports.** Assume messages can be passed directly between the two
  parties; a two-port simulation is future work, not now.
- **π stays off-chain.** The parties may be assumed to share a secure channel before
  the exchange, so the proof of knowledge never needs to go on chain.
- **Refund / timeout are edge cases.** Implement the honest path first, on the
  understanding that a dishonest counterparty must not cause the honest party to lose
  funds.
- **Packing/unpacking in the swap path is optional.** If it is not efficient enough,
  omit it and record it as a limitation in the critical reflection. Wang's framing:
  *"it's just the exploration… you don't need to build a product."*
- **Calling LaZer's C from Rust is acceptable.** Get a working version first; optimise
  later.

### 16.5 Report rulings

- **Evaluation is its own chapter**, kept separate from methodology — a reader should
  not have to understand the method before seeing the results.
- **Critical reflection goes in Chapter 5** with the conclusion, as at least its own
  subsection: what was achieved (theory + evaluation), what failed, and what would be
  done differently given another chance. A short reflective paragraph may remain in the
  evaluation chapter. (Applied: `04-evaluation.tex` → "Evaluation" + a brief reflection
  section; `05-conclusion.tex` → §Critical reflection with those three subsections.)
- **The rejection figure must change.** Plotting P(*exactly* k attempts) misleads,
  because it peaks at k=1 and decays — backwards to a reader who expects "more attempts
  → more likely to have succeeded". Plot the **cumulative probability of acceptance
  within k attempts** over k = 1…15 instead: it rises from ≈36.8 % and flattens toward
  100 %, and the flattening is itself the message. (Applied:
  `fig_acceptance_cdf` in `scripts/plot_las_paper_figures.py`; the mass function is
  demoted to the appendix.)
- **Overleaf:** share the project with Wang's Manchester address once a reasonably
  complete version exists.

### 16.6 Meeting-7 deliverable list

1. Choose the base repo (maintained; avoid Monero's privacy complexity — prefer two
   similar UTXO chains).
2. Configuration 1 — classical adaptor + Groth16 — built and benchmarked.
3. Configuration 2 — LAS + Groth16 — built and benchmarked.
4. Configuration 3 — LAS + LaZer — built and benchmarked.
5. Time + communication-cost comparison across all three, off-chain messages included.
6. Preparatory check of classical adaptor/atomic-swap cost on Solidity ✅ (§16.1 survey).
7. Report restructure: evaluation chapter, Chapter-5 critical reflection ✅.
8. Cumulative-acceptance figure replacing the mass-function figure ✅ — script, report
   text, and the installed `report/latex/figures/fig_rejection_cdf.pdf` (regenerated
   from `evidence/runs/20260717_084012/tables`; report rebuild still owed).
9. Overleaf shared with Wang.
10. Benchmark the classical repo, noting that some demos stop at Adapt and never
    implement Ext (Wang: extraction is always fast, it is just the final step).

**Open question for Royce to confirm:** whether one week is realistic. Wang said the
three steps were doable, but his answer is **cut off mid-sentence** in both recordings,
so his full caveat is unrecorded.

## 17. Meeting-8 directives [M8, 2026-07-31 — freeze the features; break down the Bitcoin transaction; polish]

Source: `meeting8_cleaned_transcript.md` (merged from `meeting8_original_transcript.md`
and `meeting8_summary.md`; the `.m4a` was not re-transcribed). **Speaker polarity is
flipped versus Meeting 7** — in M8, Speaker 1 = Wang, Speaker 2 = Royce.

Meeting 8 **added no scope**. Wang reviewed the three-configuration UTXO results,
accepted them, and spent most of the meeting on one gap: *the report never says what a
Bitcoin transaction actually contains.* Where M8 refines earlier meetings, **M8 is the
latest word**. Its governing instruction: **"You don't need to contain all the stuff —
you just need to make sure that what you have done looks good, looks perfect, looks
great."**

### 17.1 The results are accepted — stop measuring, start polishing

Royce reported the Rust UTXO comparison across the three M7 configurations: classical
adaptor (no ZKP), LAS + Groth16, LAS + LaZer. Findings: **Groth16 takes longer to
generate but yields a much smaller proof; LaZer generates fast but the proof is much
larger.** Post-quantum proof sizes land ≈30× the classical ones, and as much as ≈300×
on the on-chain components. Wang's verdict: *"it's what we expected for us… people argue
that most of the time the security is good, but the size is not good."* **He asked for
no further measurements.**

Recorded rationale for configuration 1 carrying **no π**: the classical adaptor-signature
protocol on the elliptic curve does not specify one — it only carries a discrete-log
equality proof (DLEQ). Wang accepted this ("so in our construction they don't have that
component?" → confirmed).

### 17.2 THE central ask — break down the Bitcoin transaction, with diagrams

The report treats a "transaction" abstractly, so it cannot say which fields the adaptor
layer changes or why communication grows. Wang wants, explicitly:

1. **What a standard Bitcoin transaction contains** — taken from Bitcoin's own
   definition, not from the LAS paper. His instruction was literally to look up the
   standard structure: *"You could just Google how a Bitcoin transaction looks like.
   This is the standard transaction structure."* Rationale: **"the paper is a very
   oversimplification of the architecture"** and does not specify practice.
2. **Exactly which components the adaptor construction adds** — pre-signature, statement
   `Y`, witness — **and where they sit**. The proof π appears **not** to be included
   on-chain (both agreed; Wang: *"the proof has not been included in the transaction"*).
3. **Two diagrams: the original/standard transaction vs the modified transaction**, with
   the changed fields highlighted. *"If you give a diagram to show that, okay, this is
   the original transaction… and this is the modified transaction, by adding the adaptor
   signature. Then we can see which components have already been changed."*
4. **The breakdown is what justifies the reported communication-size increase** — *"then
   we can reason on the side of why the communication size will be larger."*
5. **Confirm how the witness is actually carried on-chain**, since implementations
   differ (Royce: *"some people usually put the witness into the chain, some don't"* →
   Wang: *"you should make sure how they use the witness here"*).

Wang's closing weight on this item: **"I think that's the most important thing
[remaining]. [Otherwise] I think, yeah, we've done a great job."**

### 17.3 Terminology ruling — "transaction" ≠ "the signed message"

Royce's write-up used "transaction" to mean the message being signed. Wang objected:
in a Bitcoin context "transaction" names a **predefined format**, so **a different term
must be used for the signed message**. *"You would be better to use another term —
because 'transaction' here, we are in the context of blockchain, of Bitcoin… they have a
predefined format, which should follow the definition of a Bitcoin transaction."*

Second terminology ruling: LAS's **PreSign / PreVerify / Adapt / Ext are `functions`,
not `protocols`** — *"'protocol' means more high-level design… for example we have
consensus protocols; for this it's more like a scheme, and inside the scheme we have
some functions."* Both rulings apply report-wide, every occurrence.

### 17.4 Sequencing — finish Bitcoin/UTXO before any further EVM work

Royce asked whether to continue the Naysayer/poqeth optimistic-verification exploration
on the EVM. Wang: **finish the Bitcoin solution properly first.** The same unanswered
question — how the added components sit inside the transaction or the smart contract —
applies to the EVM anyway, so the EVM becomes *a discussion of a more advanced solution*
once a **fully complete finished solution** exists on Bitcoin. *"I would like to see a
fully complete finished solution first. Then you can discuss more advanced solutions."*

The transaction structure is also **ZKP-independent**: *"no matter how we use LaZer or
Groth16, the structure shouldn't differ; the components should be the same."*

### 17.5 Report rulings (refine M4.6 / M5.10 / M7.5)

- **Word count need not be proportional to the rubric's mark weighting.** Write more
  where the interesting work is — results, not background. Wang was content that
  Chapter 3 is the largest chapter. *"You don't need to make them proportional. You just
  make sure that your report is good."* (Royce reported trimming to ≈9,000 words.)
- **Figures are embedded in the text**, between paragraphs / top / bottom of the page —
  **not** collected at the end of a chapter or in the appendix. *"In some other subjects
  like economics they prefer to put the figures in the appendix — but their domain is
  different."* **This overrides the earlier code-and-figures-to-appendix habit.**
- **Group related figures side by side** (e.g. the four LAS function diagrams) and
  **avoid a single figure consuming a whole page** unless it genuinely needs one; always
  leave room for text on the page.
- **Chapter 5 is titled "Conclusion, critical reflection and future work"** — agreed
  verbatim in the meeting.

### 17.6 Future work — named directions and their limits

> **[Royce, 2026-08-03] Two corrections to how this subsection was framed in the
> meeting.** (i) **Nothing below is "optional"** — see §5: everything open is mandatory
> and merely ordered by priority. (ii) **The hint item is not about shrinking `Y`** — its
> real objective is stated in the first bullet's follow-up paragraph.

- **Reducing proof/object size is the most valuable direction.** Royce identified
  **statement `Y` as the largest single component**, present both off-chain and on-chain,
  and proposed a **Dilithium-style hint optimisation** so full public parameters need not
  be transmitted. He had avoided it fearing it breaks `Adapt`/`Ext`, which require both
  parties to derive identical values. Wang: interesting, **try it** (target the week
  after next); the open question is whether **verification and extraction still work**.
  *"Just try it, just try it. I don't know, to be honest."*

  **What that item actually is [Royce, 2026-08-03 — supersedes the in-meeting framing].**
  Not a size micro-optimisation. The objective is to **build LAS on NIST ML-DSA as
  specified** — hint vector `h`, Power2Round, and high/low-bit decomposition all
  **enabled** — rather than on the LAS paper's simplified Dilithium. Its purpose is
  **evidential**: the project currently *asserts* that NIST's ML-DSA construction must be
  modified for LAS to work correctly (the clean identity `Az − ct = w + Y` holds only
  once those features are disabled, §3). Running the experiment converts that assertion
  into a **demonstration** — showing precisely what breaks in `Verify`/`Adapt`/`Ext` when
  the hint machinery is present, and therefore why the simplification was necessary.
  That makes it the **highest-novelty item in the remaining queue**, and mandatory.
- **IPFS / decentralised storage as a documented fallback** for large data, with miners
  or validators referring to the address. Caveats Wang attached: it drags in another
  platform and a **cross-platform bridge**, and raises **who maintains the off-chain
  storage**. *"We can always have a solution, but the question is how good the solution
  is."*
- **Check whether Bitcoin imposes a relevant size limit** (Royce: some, but not as strict
  as the EVM's).
- **Live-network UTXO deployment** (fees, propagation) is future work, not this project.
- **Functional signatures are ruled out** — no implementation exists, so it would mean
  redoing the whole project from scratch.

### 17.7 Scope fences reaffirmed

- **No second signature scheme**, despite the original proposal allowing one — *"I don't
  think we have enough time… you should first focus on polishing what you have already
  done, and make sure the results there are correct."*
- **zkVM / RISC-V is out** unless Bitcoin's virtual machine is RISC-V-based (it is not);
  Wang judged it time-consuming and structurally mismatched.
- **No more features generally** — Royce: *"I think that's it for the project, because if
  I add more features, I'm not sure whether I can put everything in."* Wang agreed.

### 17.8 Open technical question — why is LAS `Adapt` ≈270× ECDSA's?

ECDSA's adaptor `Adapt` costs ≈1.5 µs; LAS's is ≈270× that. Royce could not fully
explain it beyond *"the pre-signature is huge, so even adding small things into the big
things just costs more."* Wang noted ECDSA's adaptor construction is simply very
efficient, and asked him to check that algorithm. **This explanation is still owed.**

### 17.9 Meeting-8 deliverable list

1. **Break down the Bitcoin transaction structure in the report** — which components the
   adaptor construction adds (pre-signature, statement `Y`, witness; π appears to stay
   off-chain) and where each sits in a standard transaction.
2. **Two diagrams: standard transaction vs modified transaction**, changed fields
   highlighted, used to justify the communication-size increase.
3. **Stop calling the signed message a "transaction"** — adopt a distinct term
   report-wide.
4. **Investigate how the pre-signature and witness are carried in a real broadcast
   transaction**, following Bitcoin's documented structure rather than the paper.
5. **Finish Bitcoin/UTXO before any further EVM/Naysayer work.**
6. Report quality over proportional word counts; more on results, less on background.
7. **Embed figures in the text**; group the four LAS function figures side by side; avoid
   single-figure pages.
8. Title Chapter 5 **"Conclusion, critical reflection and future work"**.
9. Call PreSign / PreVerify / Adapt / Ext **functions**, never protocols.
10. *Optional, week after next:* try the **hint-style optimisation to shrink statement
    `Y`**, and check whether verification/extraction survive it.
11. *Optional:* check Bitcoin size limits; document **IPFS-style off-chain storage** as a
    future-work fallback, with its bridge/maintenance caveats.
12. **Do not add a second signature scheme**; zkVM/RISC-V out; functional signatures and
    live-network deployment are future work only.
13. **Draft slides for a 6–8 minute presentation**, to present to Wang **the week after
    next** for comments — the same material underpins the video.
14. **Explain the ≈270× `Adapt` gap** versus ECDSA (§17.8).
15. *Wang's own action:* he has **not yet checked the LAS API/implementation details**
    and will do so later — so the API should be self-checked before then.

**Next meeting:** next week (i.e. week of 2026-08-07).

## 18. Meeting-9 directives [M9, 2026-08-07 — everything accepted; report fixes + one benchmark]

Source: `meeting9_cleaned_transcript.md`. **Single ASR source** — no summary file and no
recording exist for this meeting, so there is no second source to cross-check against;
§A of that file lists every fragment that could not be resolved. **Speaker polarity is
back to the Meeting-7 convention** — Speaker 1 = Royce, Speaker 2 = Wang.

M9 **added no scope and reopened nothing**. Wang accepted the M8 transaction breakdown
(*"now it's much clearer"*), the figures (*"much better than I thought"*), the ML-DSA
result, the failed statement-Y compression, and the succinct-proof comparison. **Where
M9 refines earlier meetings, M9 is the latest word.**

### 18.1 What Royce reported and Wang accepted

- **Bitcoin carriage settled.** What a transaction signs is a *hash* of the transaction,
  carried in the **witness**. Bitcoin's 520-byte script chunk limit forces chunking of
  the LAS signature, but the binding constraint is **block weight** (4,000,000 WU); the
  LAS spend sits at ≈2.9% of the standard-transaction limit. Wang's conclusion: *"we have
  some space [to] install the large LAS signature."*
- **Both M8-era open questions answered:** LAS verification **fits in one EVM
  transaction** under EIP-7825's per-transaction gas cap (contrasted in the meeting with
  the ~30M *block* gas limit), and **a stock Bitcoin node can carry LAS objects**.
- **ML-DSA:** an adapted signature verifies under an **unmodified** ML-DSA verification
  function; PreVerify cannot naively reuse ML-DSA, because high bits *and* low bits must
  both carry commitment + `Y` or verification fails.
- **Bitcoin's real problem is verification, not carriage.** Two experiments: classical
  Schnorr (carriage only) and a modified client that verifies LAS directly, which
  *"can generate the exact [measurement], not only the projection"*.

### 18.2 The one measurement asked for — benchmark the patched client

**⚠ Benchmark the modified Bitcoin client.** Wang's rule, stated generally: *"when you
modify something … [people] will always ask, okay, if we achieved this — a better
security — so what have I lost?"*

**DONE 2026-08-08.** `LASConsensusVerify` timed per input against BIP340 Schnorr and ECDSA,
both sides parsing serialized bytes inside the timed call; reject path = a valid signature
against a different 32-byte digest. Write-up
`docs/03-results/BTC_LAS_CONSENSUS_BENCHMARK.md`; runner `scripts/run_btc_las_bench.sh` →
`evidence/btc_las_bench/latest`. Quote the generated macros, never a retyped figure.
Framing is unchanged: carriage-only vs patched node, and the security of the modification
is still **not analysed** — a timing figure is not a safety argument. The comparison's
security levels are also **not matched** (secp256k1 ≈ Dilithium-II, the node runs
Dilithium-III), so the ratio **overstates** the rule's cost; and the curve baseline is a
pinned libsecp256k1-**zkp** fork, not the library Bitcoin Core vendors.

### 18.3 THE central report ask — summarise the contributions in the introduction

**⚠ Add a §1.4 "Contributions" subsection to Chapter 1**, after the objectives/aims and
**before** the dissertation-structure subsection, keeping the existing content. It must
summarise the most important findings and say **whether the objectives were achieved —
all of them or only some**. Wang: *"in the guideline of the report I've mentioned that
you should also summarise your main contributions in the introduction"*; and *"people
will ask you: can you give me 3 or 4 sentences to summarise what you've done? Have you
already achieved the objectives? Because here we don't know."* He framed it as academic
practice (reviewers read the abstract and first two pages) and as a strong suggestion
rather than a rubric requirement.

### 18.4 Consistency — conclusions must match the numbers reported

**⚠** The succinct-proof text reads as if LaBRADOR wins, while the reported numbers say
LaZer/LNP22 wins on proof size **and** time. Wang: *"just make sure the conclusion and
the results, they are consistent … otherwise people will easily challenge you."* State
the numbers' verdict and the reason (LaBRADOR's succinctness is **asymptotic**; this
statement is far too small). For the direction that was **not** refined: keep it as
**discussion without reporting the actual numbers**, so no conflicting results appear.

### 18.5 The remaining report fixes

1. **⚠ Explain why the proof size is a range**, not a fixed number — in the text.
   *"Otherwise people will ask, okay, why?"*
2. **⚠ Shrink the oversized table to at most half a page.**
3. **⚠ Cite the Bitcoin improvement solutions (SegWit/Taproot)** wherever the report says
   the original transaction structure cannot carry a LAS signature and the improved one
   can — readers may not know they exist.
4. **⚠ Bitcoin experiment placement:** main results and a conclusion in the body, detail
   referred out to the **appendix**. Wang left the final call to Royce.
5. **⚠ Write up the failed statement-Y compression as a negative result, with its
   reason**, in the **critical reflection**. *"Even if it's failed, some negative results
   are still helpful."* This confirms — it does not reopen — the closed verdict.
6. Make sure **all results are nailed down** and confirmed before the final version.

### 18.6 Bounded, and explicitly NOT a directive

Trying **another hash function** against the SHAKE-dominated on-chain gas. Wang raised
it and bounded it himself: there may be security/guarantee issues, and *"we have one
[working] version; even if it's not very efficient, it's still acceptable."*

### 18.7 Presentation

**⚠ Deliver the 6–8 minute presentation live to Wang next week** for feedback — Royce
chose next week over the week after. This is the M8 slide deliverable, now scheduled.

### 18.8 One transcript line that must not become a claim

At ≈38:44 Royce says something garbled about fitting at Dilithium-3 but not at another
level. **On-chain verification is measured at D3 only** — `LASVerifyOpt`'s parameters are
compile-time D3-only, so D2/D5 were never evaluated (`docs/03-results/
GAS_LIMIT_INVESTIGATION.md` §7). "Not evaluated" is the only supportable wording.

**Next meeting:** next week, with the mock presentation.

## 19. Meeting-10 directives [M10 — the mock presentation: content accepted, presentation reworked, code frozen]

Source: `meeting10_cleaned_transcript.md`, consolidated from **two** ASR sources: a Samsung
phone recording (diarised, covers the whole meeting 00:37–47:32) and a Teams export (better
word accuracy, **no diarisation at all** — every line is stamped with the organiser's name).
Attribution and timing come from Samsung, wording from Teams; the offset is
`Samsung ≈ Teams + 1:53`. **Speaker polarity matches Meeting 8** — Speaker 1 = Wang,
Speaker 2 = Royce. **⚠ The date is not established** (2026-08-14 or -15); never cite a firm one.

Royce delivered the 6–8 minute deck end-to-end, then Wang worked through it slide by slide;
**from ≈28:57 the screen is on the REPORT, not the deck** — §19.4 below is report feedback and
must not be re-filed as slide feedback. M10 **adds no scope and asks for no measurement**.
Where it refines earlier meetings, M10 is the latest word.

### 19.1 The verdict — content yes, presentation no

*"Content-wise, it's okay… but for the presentation-wise, you can improve it a bit: the
structure of the slides, and also the visualisation of the results, and also the conclusion
of the presentation."*

- **⚠ Lead with the application, not the technique.** Add a slide on *why adaptor signatures
  matter* — atomic swap, payment channel, and why making them post-quantum secure is the
  point — **before** any technical detail. *"You start the technical details too far, too
  early."*
- **⚠ Assume a non-specialist examiner.** *"Assume that the audience have some computer
  science background in general, but not very specific for security, for crypto, for
  blockchain."*
- **⚠ Show the method as a picture.** *"If you can give me a picture to summarise the process
  of your method, I can easily get it."* Wang was explicit that he follows the current version
  only because he already knows the details.
- **⚠ Close the loop.** End by answering the questions the opening posed: can LAS give a
  post-quantum secure atomic swap, and *"what are the costs we have to suffer from?"* — then
  what remains open.
- **⚠ 13 slides → about 10**, ~30 s each against 6–8 minutes, visuals in place of text. A
  table-of-contents slide and a "where are we" marker were suggested, not required.
- **⚠ Make the conclusion blockchain-specific** — this project is about signatures *for
  blockchains*; say so rather than concluding in general terms.
- **⚠ Slide 5: relabel the parties Alice/Bob** instead of the paper's `u₁`/`u₂`. Royce raised
  it, Wang agreed (*"that's more general, more friendly"*). **Slide only — the report's
  mathematics keeps `u₁`/`u₂`.** ⚠ **"Slide 5" is M10's numbering and no longer locates the
  slide:** in the current 13-slide deck the swap board is **slide 7**, and the motivation
  picture that also names the pair is **slide 3**. Both use Alice/Bob (verified 2026-08-22) —
  the item is done; do not go looking at slide 5.
- **Keep the patched-Bitcoin demonstration.** Wang reaffirmed the two-step design — LAS
  standalone first, then integration into an existing chain — and said the integration problems
  and what was done about them are exactly what to report.

### 19.2 Framing ruling — practical LAS, not "an implementation of the paper"

Royce asked directly whether to frame the project as implementing eprint 2020/845, given that
LAS over ML-DSA has a different rejection-sampling rate from the simplified scheme. Wang:
*"it's more for the practical LAS… you don't need to limit yourself in the scope of the paper,
because you have already attempted the optimisation, so you should summarise it, you should
report it."* **This overrides any "reproduce the paper" framing.** Whether the ML-DSA rejection
result earns a slide is Royce's call (*"do you think it is an interesting finding?"*);
benchmarking a full ML-DSA build is **future work**, not this project.

### 19.3 The one content correction — soften the motivation claim

The claim that post-quantum exotic signatures lack implementations must be softened: *"they
**do** have the proposed ones… maybe they haven't implemented **all** exotic ones."* Exotic
signatures are a family — adaptor, multi-, ring and other advanced schemes — and
**multi-signatures in particular are actively being implemented**. Applies to the deck and to
the same claim wherever it appears in the report.

### 19.4 Report fixes (the screen is on the report from ≈28:57)

- **⚠ Name the paper explicitly** — never a bare "the paper"; add the citation.
- **⚠ Put `SampleInBall` in the challenge figure.** Wang read it, saw `c`, and asked *"how did
  we get the `c`?"* The two symbols side by side with no function between them read as a typo
  rather than a derivation. → §7 of `docs/paper/LAS_2020_845_NOTATION.md` and the `c`/`c̃` rule.
- **⚠ Balance captions against paragraphs.** Captions are not counted in the word budget, but
  *"the caption is too long, the paragraph is too short"* — move material into the body.
- **⚠ Fig. 3.5 (`fig:swapflow`, "The atomic swap, message by message") is ACCEPTED, caption
  included.** What Wang wants is an **addition, not a replacement**: a simpler, more colourful
  diagram placed **before** it, where the application is introduced — two participants, one on
  each chain, one holding Bitcoin and one holding Ether, doing the exchange. The technical
  figure *"is more technical, right — which is good."*
- **Keep LaTeX tables**, not Excel screenshots: an image cannot be edited in place, and column
  widths are adjustable in LaTeX. Keep tables inside the page.
- Use **draw.io** or PowerPoint **exported to PDF** for decorative diagrams (vector quality).
- **Notation table stays in the appendix** — appendix words are not counted. Already the case.
- **Also accepted as they stand:** the modified Criterion figure, Fig. 3.4 (*"I like this
  figure"* — the font-size fix answered his Meeting-9 comment), and the Bitcoin transaction
  structure figure.
- The **GitHub repository link is not required** in the report; it may be added.
- Chapter title vs running header: **no firm ruling** — modify the template if you want, or
  follow it to be safe.

### 19.5 Feature freeze, reaffirmed in the strongest terms

**⚠ "You don't need to do new things — I think you have enough content; now you should make
sure all of them are correct, they are precise."** The implementation is **frozen** with
Wang's agreement (with the caveat that he has not checked the code and it is Royce's call).
The remaining time goes to **verifying every result**, the presentation, the writing and the
video. Timeline stated: *"more than two weeks."*

**⚠ Methodology is the weak point.** Royce's own diagnosis, which Wang endorsed: the method is
not coming across, and *"it's not only just the results, but how you get the results."*

### 19.6 One line from this meeting that must not become a claim

At ≈10:37–10:55 Royce says on record that on-chain verification is *"on the boundary of the gas
limit"* at Dilithium-3 but that **"for Dilithium 5 it needs more optimisation to fit in one
transaction."** Both ASR sources agree he said it, and it is **not evidenced**: `LASVerifyOpt`'s
parameters are compile-time **D3-only**, so D2 and D5 were never built or measured on-chain
(`docs/03-results/GAS_LIMIT_INVESTIGATION.md` §7). The line was deleted from the deck. This is
the **third** appearance of this exact trap, after §18.8 and the retracted "exceeds the block
gas limit" claim.

⚠ **This item's own wording has since been overtaken — do not quote the old form.** When M10
was merged, *"not evaluated"* was the only supportable statement; **that stopped being true on
2026-08-15**, when D2 and D5 were settled. The state is now three-way and **no row may be
restated as another**: **D3 = MEASURED, fits** (a real client's receipt for a *whole claim*) ·
**D2 = MEASURED, fits** (a harness charge for *verification only* — a different boundary, never
interchangeable with D3's) · **D5 = DERIVED, one transaction is exceeded** (a lower bound
computed *from* measured quantities; the bound is arithmetic and is never itself "a measured
total"). What stays unevidenced — and is why this section still stands — is the claim that D5
**"needs more optimisation"**: nothing measures what optimisation would achieve there.
Consequently *"no other parameter set was evaluated"* is now **FALSE**, and was purged
repo-wide on 2026-08-18; if it reappears, it came from an older draft.

## 19A. Meeting-11 directives [M11, 2026-08-21 (inferred) — deck review, second pass; merged 2026-08-21]

Transcript `meeting11_cleaned_transcript.md`. ⚠ **Single ASR pass, no diarisation** — speaker
labels are content inferences (§D of the transcript); Royce confirmed the instructions by
directing that they be fulfilled. The Meeting-10 feature freeze is not mentioned and not lifted.

### 19A.1 Instructions confirmed (already recorded as the 2026-08-21 "second batch")

Lead the deck with quantum-computer background and the urgency — **two motivations, both
required**: atomic swaps matter *and* quantum security matters, the project is where they meet
(§7). Close on "is LAS good or not" plus recommendations for developers and the blockchain
community (§1). Show the transaction structure of Bitcoin (and Ethereum) field by field —
"which field should I add" (§4). Make the swap followable without the maths — the protocol as
states and messages (§12). Real logos and real prices, "because you are doing some scientific
report… even for toy examples, try to be more precise" (§3). Exotic signatures explained on a
slide or prepared as question backup (§8).

### 19A.2 New rulings

- **"Nearly free" must name its base** (§6): the deck's claim is against the **Dilithium-style
  lattice base**, never ECDSA — "we should first translate from ECDSA to Dilithium, and then
  move from Dilithium to LAS." Adaptor-ECDSA vs adaptor-LAS is endorsed as the companion
  comparison (it already exists: `bench_classical`).
- **The coins do not move between chains** (§12, laboured): each payment settles as a
  transaction on its own chain; the swap is the linkage. Every diagram must be unreadable as a
  cross-chain coin transfer.
- **Venue wording** (§10): smart contracts are **"more flexible"**; Bitcoin is **"more
  restricted… because they cannot be modified"** — miners verify directly against transaction
  content; a contract compiles to opcodes the EVM executes. Replaces a framing Wang called "a
  bit misleading".
- **Scope: say the swap is UTXO-with-UTXO** (§13–§16): "it would be safer to just talk about
  UTXO"; the Ethereum implementation may still be reported. **ETH↔USDC is never an example**
  (ERC-20 swaps inside Ethereum need no atomic swap; Ethereum bridges outward). BTC↔ETH is
  doable in principle — "the only different thing is the verification" — and stays a scenario.

### 19A.3 Resolved since the meeting, and what is still open

- **✅ TPS/throughput — RESOLVED by Royce on 2026-08-21 as option (c), "satisfy the intent".**
  Wang demonstrated tx-per-block ÷ block-interval with improvised numbers (**none citable** —
  "let's say 200", "just Google it"), a derivation that collides with the standing EIP-7825
  never-claims-per-block rule, the once-retracted block-limit claim, and the closed throughput
  deliverable's never-combine rule. The chosen resolution answers Wang's question while keeping
  every one of those rules, under **three guards that travel together** — they are recorded in
  `video_deck.template.html` beside the figure, and none may be dropped in isolation:
  1. the per-block figure is the **report's own sanctioned ledger-capacity bound** — Bitcoin's
     block-*weight* ceiling, scaling dimension (2), already in `03-results`;
  2. it is **BITCOIN ONLY** — an EVM per-block figure would be precisely the retracted
     claims-per-block reading of EIP-7825, which is a per-*transaction* cap;
  3. it is quoted as a **size bound with its negative stated on the slide itself** ("not a
     throughput"), and is **never divided by the block interval** — that division is the step
     that manufactures a TPS.
  **No TPS number exists anywhere in the deck (verified 2026-08-22), and none may be added.**
  The supported quantities remain, each on its own and never combined: the per-transaction cap
  percentage, the serial verification rate (never called a network throughput), and the
  block-weight ceiling.
- **❓ "You have 10 slides"** (§2): **still open, and it is Royce's call** — recollection
  against a deliberate 13-slide deck, and explicitly **not** a ruling to cut. §19A.4 records
  what the deck actually costs against the 6–8 minute band.

### 19A.4 Delivery status against the built deck (verified 2026-08-22)

A read of `report/slides/video_deck.html` against **both** meetings' deck items. This section
records *status*, never figures: quote numbers from the macros and the evidence run, never here.

**⚠ How a deck ruling is judged satisfied — the distinction that makes this check meaningful,
and which is easy to get wrong in the strict direction.** The deck's `data-notes` text is the
**spoken script**: `VIDEO_PLAN.md` §3 has it toggled OFF before the take, so it is never *on
screen* — but it is *delivered*, and the video is marked on that delivery. So **"it is only in
the narration" is not by itself a failure.** The real split is between two kinds of ruling:

- **Must be VISUAL** — Wang asked to *see* it: *"even [if it's] screenshots or something — to
  show"* (§4), *"if you could give me a picture to summarise the process of your method"*
  (M10 §11), *"sometimes you can use the actual logo"* (§3), *"add some concrete numbers"*
  (§12). A narrated substitute does **not** discharge these.
- **Narration is SUFFICIENT** — the ruling is about a *claim*, and for the central one Wang
  named the channel himself: *"when you are doing the presentation, you should **say**
  something like this: this is not compared with ECDSA"* (§6). Carrying such a claim on-slide
  as well is **stronger than asked**, not required.

**Verified applied — M10:** lead with the application before any technical detail (slides 2–4)
· the method as a picture (6) · blockchain-specific close (13) · Alice/Bob (3, 7) · the
patched-client demonstration kept (11) · practical-LAS framing, with **no "implementation of
the paper" wording anywhere in the deck**.

**Verified applied — M11, visual class:** both venues' transaction structures field by field
(10) · the swap followable without the maths, with concrete amounts, drawn so **no coin crosses
between chains** — each payment settles as a transaction on its own ledger, the shared secret
being the only element crossing (3) · the currency marks drawn as their own scalable paths (3)
· quantum background and urgency first (2) · verdict plus one card per named audience (13) ·
the exotic family on slide 4, whose stack carries multi-signature, threshold and aggregate
beside the adaptor chip marked "← this project" — exactly §8's *"show that the next thing is
one of the exotic signatures, and emphasise that this is what you have done"*.

**Verified applied — M11, claim class (all three also carried on-slide, i.e. above the bar):**
"nearly free" naming its base (8 — the eyebrow reads "each measured against its own base", with
the classical same-step comparator beside it on one shared scale) · Wang's venue vocabulary,
"more flexible" / "more restricted … cannot be modified" (10, 13) · UTXO-with-UTXO scope in
DEMO A's own eyebrow (7). And **no TPS figure anywhere**.

**Three drifts found. None is a claim defect; all three are documentation lagging the artefact.**

1. ⚠ **The planned runtime is recorded in three places and they disagree.** The deck's own
   per-slide `data-time` attributes are the authority and sum to **7:33** — and the deck's own
   footer **prints `deck 7:33` on every slide**, so this needs no arithmetic to check;
   `VIDEO_PLAN.md` §1
   says **7:29** (its slide-10 row reads 0:40 where the deck says 0:44 — exactly the 4 s gap)
   and repeats 7:29 in its rubric table; `CLAUDE.md` says **7:23**. All three sit inside the
   6–8 minute band, so nothing is at risk except the margin the plan claims to have. **Fix the
   two derived copies from the deck, never the reverse** — and note this is CLAUDE.md's own
   "never paste a number here" rule demonstrating itself.
2. ⚠ **Slide 3's prices are a day old, and the exposure is in the SPOKEN line, not the printed
   one.** The slide prints its own retrieval instant, so as a dated, cited figure it stays
   truthful on a later day. The narration does not: it says the legs are value-matched at spot
   **"this morning"**, which is false the moment the take moves to another date. State the
   standing rule precisely — **a stale price *presented as current* is a wrong number** (the
   repo's shorthand, "a stale price is a wrong number", is the same rule said loosely). Before
   recording, either re-read both sources and re-run `gen_slides.py`, or drop "this morning";
   the asserted quantity is the **ratio**, so both legs move together.
3. ⚠ **`VIDEO_PLAN.md` §2 still describes DEMO A's board as "u₁ · two chains · u₂".** The board
   was relabelled Alice/Bob per §19.1; the prose describing it was not. Artefact correct, prose
   stale.

**Still open:** only the 13-vs-10 slide count (§19A.3) — Royce's call, and the deck's true
**7:33** is the number that decision should be made against.

## 20. Reference links

- LAS spec: https://eprint.iacr.org/2020/845
- Survey: https://eprint.iacr.org/2022/1151
- poqeth: https://github.com/ruslan-ilesik/poqeth · paper https://eprint.iacr.org/2025/091
- Dilithium reference: https://github.com/pq-crystals/dilithium
- Supporting text: Menezes, *A Gentle Introduction to Lattice-Based Cryptography* (eprint 2026/1098) — §7.2 (Dilithium without t compression) ≈ the simplified scheme LAS needs; §5 for M-SIS/M-LWE norms; §11.3 for NTT (use as black-box API).
