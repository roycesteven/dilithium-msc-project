# LAS Project — Consolidated Context (Meetings 1 + 2 + 3)

> **THE canonical objectives/context file** (merges Meetings 1, 2 and 3). As of
> 2026-06-13 the older `LAS_OBJECTIVES_FOR_TOP_MARK.md`, `las-objectives-meeting2.md`
> and `docs/archive/LAS_PROJECT_HANDOFF.md` have been **deleted** — their content is
> fully captured here (objectives), in `CLAUDE.md` (project context), and in
> `docs/STATUS.md` (live deliverable/test checklist). This file = the *spec*;
> `docs/STATUS.md` = the *progress tracker*.
> Provenance tags: **[M1]** = Meeting 1 · **[M2]** = Meeting 2 (2026-06-08) ·
> **[M3]** = Meeting 3 (2026-06-18) · **[M1→M2]** = set in M1, revised in M2.
> Where the meetings conflict, the **later meeting wins** (M2 over M1; M3 over M2
> for evaluation rigour / fairness — Meeting 3 raised the Stage-1 bar, see §13).

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
- Reconciliation: migration to paper params is a *later, documented* step (separate commit, before/after benchmarks). If migration is disruptive, staying on reference params is acceptable **provided** the deviation is justified in the report and norm-bound arithmetic is re-derived for the actual q.
- Standing technical concern [M1+M2]: after Adapt adds the witness, the result must remain within the rejection bound — handle and document the post-adaptation norm check explicitly.

## 5. Official project structure [M1→M2 merged]

M2 made the two-stage structure the spine; M1's success ladder maps onto it:

**Stage 1 — Standalone LAS** (= M1 "minimum", Wang: "already a manageable problem")
Implement LAS by modifying Dilithium; benchmark against pure Dilithium. Understand KeyGen/Sign/Verify + PreSign/PreVerify/Adapt/Ext; exposition focuses on the *differences* from basic Dilithium [M1].

**Stage 2 — Blockchain application** (= M1 "better")
**Atomic swap or fair exchange** on a **local testnet / private chain** [M2]. Method: take an existing adaptor-signature-based atomic-swap construction and **replace only the signature component** with LAS — do not rebuild application logic [M2]. Compare gas cost / application efficiency.

**Optional tier** (= M1 "best"; only after Stages 1–2 + benchmarks + report draft):
- **AMHL / multi-hop payment demo** [M1→M2 **demoted**]: previously the chosen next stage; M2 explicitly makes it optional ("another story… if you do not have time, focus on the first two stages — that is already enough"). Spec if attempted: LAS Fig. 2, bound `‖ẑ‖∞ > γ − κ − K` for path length K, timeouts, refund paths.
- **On-chain LAS verification** (precompile or zk proof) — the swap protocol + gas floor are already measured (`evm/`); native EVM verification is the open piece.
- **Parameter migration** to the paper's `q≈2²⁴` — a documented before/after step.
- **Second LAS-family scheme** (application-layer only) — far-stretch; consensus-level multisigs (Squirrel/Chipmunk-style) are out of scope.

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

**Status (2026-06-13):** *All required Meeting-2 deliverables done & tested.* Stage 1
(LAS + benchmark vs Dilithium), Stage 2 (atomic swap + scriptless HTLC ledger), **both
benchmark baselines** (vs Dilithium-3, vs classical ECDSA-adaptor), function map,
reproducibility README — all ✅. Bonus done: AMHL multi-hop, byte serialisation +
`las_verify_packed`, deterministic API + pinned KATs. **Open:** report draft (in
progress, `report/REPORT_DRAFT.md`), video, and the optional tier (real gas, param
migration to 2²⁴, on-chain LAS verification, second LAS-family scheme). Full deliverable/test matrix:
**`docs/STATUS.md`**.

**Pre-Meeting-3 deliverables [M2, explicit asks] — all ✅:**
1. Dilithium reference builds & runs on own machine — commit hash + toolchain recorded in `README_LAS.md`. ✅
2. Literature check on LAS follow-ups (LAS + survey 2022/1151) — captured in `docs/LAS.md §1.1`. ✅
3. Language decision — C. ✅
4. **Function map** (call-as-is / modify / new; 0 upstream functions modified) — `docs/FUNCTION_MAP.md`. ✅

**Meeting-3 deliverables [M3, 2026-06-18 asks] — Stage-1 defensibility (see §13):**
1. Two-branch code-diff view (`dilithium-baseline` vs `main`) + `docs/CODE_DIFF_VIEW.md`. ✅
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
*Done:* `dilithium-baseline` vs `main`; `docs/CODE_DIFF_VIEW.md`;
`docs/FUNCTION_MAP.md` gives the per-function classification.

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
versus the **LAS adaptor path**, on identical code/parameters/primitives:
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

## 14. Reference links

- LAS spec: https://eprint.iacr.org/2020/845
- Survey: https://eprint.iacr.org/2022/1151
- poqeth: https://github.com/ruslan-ilesik/poqeth · paper https://eprint.iacr.org/2025/091
- Dilithium reference: https://github.com/pq-crystals/dilithium
- Supporting text: Menezes, *A Gentle Introduction to Lattice-Based Cryptography* (eprint 2026/1098) — §7.2 (Dilithium without t compression) ≈ the simplified scheme LAS needs; §5 for M-SIS/M-LWE norms; §11.3 for NTT (use as black-box API).
