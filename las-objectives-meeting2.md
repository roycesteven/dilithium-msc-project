# LAS Project — Objectives (REVISED AFTER MEETING 2)

> **File:** `LAS_OBJECTIVES_MEETING2.md` — supersedes `LAS_OBJECTIVES_FOR_TOP_MARK.md` where they conflict. Keep both files; this one reflects supervisor Wang Zhipeng's priorities from **Meeting 2 (2026-06-08, voice transcript `Voice_260608_135650`)**. Items unchanged from Meeting 1 are marked ⏸ unchanged; items changed are marked 🔁 revised; new items are marked ➕ new.

---

## 0. Executive delta — what Meeting 2 changed

1. 🔁 **AMHL/multi-hop is demoted from "next stage" to OPTIONAL.** Wang: multi-hop locks are "another story… optional. If you do not have time, focus on the first two stages. Once those two are finished, that is already enough." The previous objectives doc treated the AMHL Figure-2 demo as the chosen next milestone — it is now a *bonus tier*, pursued only after Stage 1 + Stage 2 + both benchmarks are airtight.
2. ➕ **A second benchmark baseline is now explicit: a CLASSICAL adaptor signature** (ECDSA- or Schnorr-based adaptor construction — *not* plain ECDSA). Meeting 1 only required LAS vs Dilithium. Meeting 2 requires **two** comparisons: (i) LAS vs basic PQ signature (Dilithium/ML-DSA), (ii) LAS vs classical adaptor signature. For (ii), existing implementations/data may be reused — no need to build it yourself.
3. 🔁 **Parameters: start from the implementation's parameters, not the paper's.** Wang explicitly: do not change parameters at the beginning; run the existing implementation first. Practical meaning: initial builds use Dilithium's `q = 8380417 ≈ 2²³`, even though eprint 2020/845 specifies `q ≈ 2²⁴`. Reconciling to the paper's parameter set (`d=256, n=ℓ=4, q≈2²⁴, κ=60, γ=122880`) is a *later, documented* step — the diff itself becomes reportable content.
4. ➕ **Explicit out-of-scope list:** Ethereum-consensus multi-signatures (requires modifying consensus/client — "more challenging… another story"), blind signatures, group signatures, heavy ZKP/MPC constructions. Do not spend report space on these beyond a one-paragraph related-work mention.
5. ➕ **Report skeleton confirmed by supervisor:** high-level design → which functions were modified/added → key implementation decisions → benchmark results; only the most important code snippets, in the appendix. There will be a page/word limit — full source does not go in the report.
6. ➕ **Publication path named:** implementation/engineering venues (CHES-style). Wang confirmed *no tension* between marks and publication — publishability implies a strong contribution, which supports a high mark.

---

## Part A — Supervisor's success ladder (consolidated, Meetings 1+2)

The two-stage structure is now the official spine of the project:

**Stage 1 — Standalone LAS (the "manageable problem").**
Implement LAS by modifying the Dilithium/ML-DSA reference implementation (digital-signature part only; KEM code untouched). Follow the paper's **simplified** algorithm. Benchmark against pure Dilithium. Wang: "LAS plus Dilithium comparison is already a manageable problem" — this alone is a successful project.

**Stage 2 — Blockchain application (the "good project").**
Integrate LAS into **atomic swap or fair exchange** on a **local testnet / private chain**. Method: take an existing adaptor-signature-based atomic-swap construction (Solidity or otherwise) and **swap only the signature component** for LAS — do not rebuild application logic. Compare gas cost / application-level efficiency.

**Optional tier (only if time remains after Stages 1–2 + benchmarks):**
- AMHL / multi-hop payment demo (LAS Fig. 2, bound `‖ẑ‖∞ > γ−κ−K` for path length K, timeouts, refund paths). ← previously "next stage," now optional.
- Comparison against IAS (isogeny adaptor). Try running their old (~6-year-old) implementation; *even a failed run is reportable data*.
- Second exotic scheme (Falafl remains top of the ranked shortlist). Note: Squirrel/Chipmunk-style consensus multisigs are now explicitly discouraged as integration targets (consensus modification out of scope) — if a second scheme is pursued, keep it application-layer.

**Scope rules (unchanged ⏸):** no security proofs, no detailed security analysis; treat M-SIS/M-LWE hardness as a black box; understand KeyGen/Sign/Verify + PreSign/PreVerify/Adapt/Ext and focus the exposition on the *differences* from basic Dilithium.

---

## Part B — Core deliverables (definition of done, revised)

**B1. LAS implementation (`las.c` / `las.h`) — ⏸ unchanged in substance.**
- Variant **(B)**: Adapt **adds** the witness (`σ = (c, ẑ+y)`), Ext **subtracts** (`y = z−ẑ`), statement folded into Fiat-Shamir hash (`c = H(pk, w+Y, M)`), PreSign rejection bound `‖ẑ‖∞ > γ−κ−1`.
- Paper's **simplified** scheme: Power2Round, hint vector `h`, and high/low-bit decomposition **disabled** (the clean identity `Az − ct = w + Y` requires this).
- Ternary witness via KeyGen.
- Upstream Dilithium untouched (clean diff = visible contribution).
- 🔁 **Parameter note:** baseline build on Dilithium reference params (`q = 8380417`); migration to paper params (`q≈2²⁴, κ=60, γ=122880`) is a separate, documented commit with before/after benchmarks. If migration proves disruptive, staying on reference params is acceptable *provided the deviation from the paper is explicitly justified in the report* (norm-bound arithmetic re-derived for the actual q).

**B2. Benchmark suite — 🔁 expanded to two baselines.**
Metrics Wang named explicitly: **keygen time · public-key size · signature size · signing time · verification time** (signature level), plus **gas cost / application cost** (application level).
- (i) LAS vs **pure Dilithium** (same machine, same build flags; report medians over ≥1000 iterations + std dev).
- (ii) LAS vs **classical adaptor signature** (Schnorr- or ECDSA-based). Reuse existing implementations/published numbers; cite source and hardware caveats. Frame as "the price of post-quantum": X× larger signatures, Y× slower/faster signing.
- Application benchmark: atomic swap with classical adaptor vs atomic swap with LAS — gas + latency.

**B3. Atomic swap / fair exchange demo — ⏸ unchanged (already complete; now officially Stage 2 rather than 'better tier').**
Local/private chain; existing contract skeleton with signature component replaced.

**B4. Report — ➕ structure fixed by supervisor.**
Main body: motivation/framing → related work (one paragraph each: IAS, survey 2022/1151, poqeth as integration precedent; one paragraph on out-of-scope families) → high-level design → **table of Dilithium functions reused vs modified vs added** → key decisions (variant B, simplified scheme, parameter choice, bound handling after Adapt) → benchmark results (both baselines) → critical analysis (limitations: weak pre-signature adaptability / knowledge gap, simplified-scheme tradeoffs) → conclusions. Appendix: selected code (PreSign/Adapt/Ext cores, not full source), full benchmark tables.

**B5. Pre-Meeting-3 deliverables (Wang's explicit asks) — ➕ new, short-fuse.**
1. Confirm the Dilithium reference implementation **builds and runs on your machine** (it does — record exact commit hash + toolchain in the repo README).
2. Literature check: other LAS-like constructions via Google Scholar forward-citations of eprint 2020/845; compare follow-ups against the original. (Largely done in prior survey — condense to a half-page table.)
3. Language decision: **C confirmed** (reference code is C; demo layer may be Python/Solidity).
4. **Function map**: for each function in the Dilithium repo, classify *call-as-is / modify / new*. (See companion mapping table; bring this to the meeting.)

---

## Part C — Marking-rubric overlay (⏸ unchanged logic, updated emphasis)

Distinction (70–80+) = novel contribution + rigorous evaluation against proper baselines + honest critique + clean writing. Meeting 2 strengthens two axes:
- **Evaluation:** the second baseline (classical adaptor) turns a one-column comparison into a 2×2 story (basic/exotic × classical/PQ) that mirrors the project's framing quadrant — examiners reward this symmetry.
- **Critical analysis:** Wang's "let the data speak" instruction means the discussion chapter should quantify the PQ tradeoff (communication = key/signature sizes; computation = sign/verify cost) rather than assert it.

Novelty claim to protect everywhere (⏸ unchanged): **first public implementation of LAS** + **first integration of an exotic PQ signature into a blockchain setting** (poqeth covered only basic PQ signatures).

---

## Part D — Risk register (revised)

| Risk | Status after Meeting 2 |
|---|---|
| Running out of time | Mitigated: Stages 1–2 already complete; everything else is bonus. Wang: "if you focus on the first stage, it should be fine." |
| Parameter mismatch paper-vs-code (2²⁴ vs 2²³) | Now an explicit, supervisor-sanctioned starting point rather than a bug. Document the choice. |
| AMHL complexity blowing the schedule | Defused: AMHL is optional. Only attempt after report draft of Stages 1–2 exists. |
| IAS comparison blocked by dead code | Reframed: a failed build of the 6-year-old IAS artifact is itself a reportable finding. Timebox to 1–2 days. |
| Scope creep into multisig/consensus | Hard-fenced by supervisor. Decline in writing if tempted. |

---

## Part E — Timeline anchor

~3.5 months total (<12 weeks remaining at Meeting 2). Suggested allocation given Stages 1–2 are done: **weeks 1–2** function map + parameter reconciliation + benchmark hardening (two baselines); **weeks 3–4** classical-adaptor comparison + gas measurements; **weeks 5–8** report draft (Stages 1–2 only); **weeks 9+** optional tier (AMHL or IAS attempt) *only if draft is supervisor-approved*; final weeks: polish, appendix, reproducibility README.
