# LAS Project — Objectives for a Top-Mark MSc Dissertation

> What "perfect" looks like, grounded in (a) what supervisor Wang Zhipeng explicitly said, and (b) what an MSc dissertation rubric rewards at distinction level. Use this as the target spec and the acceptance checklist.

---

## How marks are actually won

An MSc CS dissertation is graded on roughly seven axes: **motivation/problem framing · related work · design & methodology · implementation · testing & evaluation · critical analysis & conclusions · writing/presentation**, with credit for **originality and difficulty**. A pass (50s) is "it works." A merit (60s) is "it works and is evaluated." A **distinction (70–80+)** is **a clear novel contribution, rigorously evaluated against proper baselines, honestly critiqued, and written cleanly.** Code alone never reaches distinction — the *evaluation and analysis around it* do.

Your novelty is strong and citable: **the first public implementation of LAS**, and **the first integration of an exotic post-quantum signature into a blockchain setting** (poqeth only did basic PQ signatures). Protect that framing everywhere.

---

## Part A — Supervisor's explicit success ladder

Directly from the meeting. This is the floor for "good," not the ceiling for "perfect."

1. **Minimum (a good mark):** a working **exotic post-quantum signature implementation** (LAS) **+ integration into a basic blockchain application**.
2. **Better:** **+ benchmark against the basic/classical equivalent** (LAS vs plain Dilithium). Wang distinguished **two** benchmark types — (i) the **signature itself**, and (ii) the **application** — both vs the basic version.
3. **Best ("much better"):** **+ a second exotic scheme**, or richer integration.

Wang's scope rules (respect exactly):
- **No security proofs, no detailed security analysis.** Treat lattice hardness as a black box.
- Understand the **core algorithm** (KeyGen/Sign/Verify + PreSign/PreVerify/Adapt/Ext); compare exotic vs basic and **focus on the differences**.
- Things he explicitly wants **reported**: **verification/computation time** and **signature sizes**.

---

## Part B — Core deliverables (definition of done)

What must physically exist, with acceptance criteria.

**B1. LAS implementation (`las.c` / `las.h`)** extending the cloned Dilithium repo.
- Variant **(B)**: Adapt **adds** the witness (`σ = (c, ẑ+y)`), Ext **subtracts** (`y = z−ẑ`), statement folded into the hash (`c = H(pk, w+Y, M)`), PreSign uses the stronger bound `‖ẑ‖∞ > γ−κ−1`.
- Paper's **simplified** scheme (no Power2Round / hint `h` / hi-lo decomposition). Params `d=256, n=ℓ=4, q≈2^24, κ=60, γ=122880`, ternary witness.
- Upstream Dilithium left untouched (clean diff = visible contribution).
- **Accept when** all 8 correctness assertions pass (see §F), correctness rate 100% over ≥1000 runs.

**B2. Blockchain integration demo.**
- Minimum: an **atomic swap** between two parties using LAS (PreSign → Adapt → Ext reveals witness on "chain").
- Done as a **simulation or testnet**, not mainnet.
- **Accept when** the swap completes end-to-end and the fairness property is demonstrated (party A gets coins ⟺ party B can extract the witness and get theirs; abort leaves neither able to cheat).

**B3. Benchmark suite (`bench_las.c` + analysis).**
- See §D for the metric set. **Accept when** it outputs reproducible tables comparing LAS vs Dilithium across size and time, averaged over ≥1000 iterations.

**B4. The dissertation** (see §E). The single biggest mark-bearing artifact.

---

## Part C — What "perfect" adds beyond Wang's minimum

The distinction-level deltas. Aim to land most of these.

**C1. Hit the "best" tier with the richer integration (already chosen #2).** A **multi-hop AMHL payment** (LAS Fig. 2) with timeouts/refund paths. This exercises the genuinely novel part of LAS — most projects only do a single swap. Demonstrating multi-hop + the `γ−κ−K` bound change is a real contribution.

**C2. Both benchmark dimensions, done properly.** Signature-level (LAS vs Dilithium) **and** application-level (cost of the swap / per-hop cost of the multi-hop payment). Tie measured numbers back to the paper's *estimates* (LAS ~3210 B pre-sig, 2701 B Dilithium) and to the survey's comparison table — explain any divergence (you use the unoptimized scheme).

**C3. Genuine critical analysis** (this is where 70 becomes 80):
- The **knowledge gap** — quantify/illustrate its practical effect (witness-norm growth along a path).
- The **privacy limitation** IAS raises about LAS-based PCNs — discuss honestly, don't hide it.
- The **size cost** of using the simplified vs optimized scheme — and what Bai–Galbraith / Dilithium compression would recover.
- **Why LAS over IAS** — security level (128-bit M-SIS/M-LWE vs ~60-bit CSIDH-512), speed, Dilithium reuse. Frame IAS as the alternative considered and rejected.

**C4. Reproducibility & engineering quality.** Deterministic **known-answer test (KAT) vectors**, a clean build (`make test_las`, `make bench_las`), README, and a tidy commit history on the feature branch. Examiners reward "I could rebuild this."

**C5. Optional stretch — a second exotic scheme.** Only if time remains and B1–B3 + C1–C3 are solid. A lattice ring signature (e.g. Falafl, which reuses Dilithium III params) would let you benchmark exotic-vs-exotic. High effort; do not jeopardize the core for it.

---

## Part D — Evaluation plan (the mark-maximising chapter)

Measure all of these; present as tables/plots with brief interpretation.

**Sizes (bytes):** public key, secret key, signature `|σ|`, pre-signature `|σ̂|` — for **LAS and Dilithium** side by side.

**Timings (ms, mean over ≥1000 iters, state CPU/clock):** KeyGen · statement-gen `(y,Y)` · PreSign vs Sign · PreVerify vs Verify · Adapt · Ext.

**Rejection sampling:** average restart count for Sign vs PreSign (PreSign restarts more due to the tighter bound — quantify).

**Correctness:** 100% over the run; report it explicitly.

**Application-level:**
- Atomic swap: total messages, total bytes exchanged, on-chain footprint (the adapted signature only).
- Multi-hop: cost as a **function of path length K** (sizes, restarts, time); demonstrate the **witness-norm growth** that motivates the `γ−κ−K` bound; show a refund/timeout path.
- If you target Ethereum/Solidity: **gas** for on-chain verification (compare to poqeth's basic-PQ gas figures). If simulated: transaction/signature size as the proxy.

**Comparison against literature:** your measured LAS numbers vs the paper's estimates and vs the IAS row in the survey's Table 1.

---

## Part E — Dissertation structure (maps to the rubric)

1. **Introduction** — quantum threat to ECDSA/Schnorr (Shor), the migration to PQ, the gap: exotic PQ signatures are paper-only and unproven on-chain. State the contribution in one paragraph. *(motivation marks)*
2. **Background** — signatures as KeyGen/Sign/Verify; lattices, M-SIS/M-LWE (black-box); Dilithium & Fiat-Shamir-with-Aborts; adaptor-signature definition (PreSign/PreVerify/Adapt/Ext); blockchain primitives (UTXO, HTLC, payment channels, AMHL). *(depth marks)*
3. **Related work** — the 2×2 landscape; survey (2022/1151); LAS (2020/845); IAS (2020/1345) and why LAS wins; poqeth (2025/091) as the integration precedent. *(related-work marks)*
4. **Design** — LAS variant (B), the simplified scheme, the `+Y` hash binding and the `γ−κ−1` / `γ−κ−K` bounds, witness = ternary keypair; the swap and multi-hop protocols. *(methodology marks)*
5. **Implementation** — extending Dilithium, the four functions, what was reused vs added, the simplified-vs-optimized decision and *why* (hint-mismatch). *(implementation marks)*
6. **Evaluation** — everything in §D. *(the heaviest marks)*
7. **Critical analysis & limitations** — §C3 items, honestly. *(distinction-defining marks)*
8. **Conclusion & future work** — recover size via compression; full optimized-Dilithium port; privacy fix; second scheme; mainnet deployment.
9. **Appendices** — KATs, build instructions, parameter table.

Aim ~8,000–9,000 words. Figures: the 2×2, the adaptor flow (PreSign→Adapt→Ext), the multi-hop diagram, benchmark plots.

---

## Part F — How to lose marks (avoid)

- **Wrong variant.** If Adapt subtracts or `+Y` is missing from the hash → it isn't LAS. (8-point test below catches this.)
- **Claiming security you didn't prove.** Scope says black-box; over-claiming invites attack. State assumptions, cite the paper's proofs, stop there.
- **Benchmarks without baselines.** Numbers vs nothing = no marks. Always vs Dilithium (and vs the paper's estimates).
- **Hiding limitations.** The knowledge gap and privacy leak are *known*; discussing them earns analysis marks, ignoring them loses them.
- **A demo that only swaps once.** The multi-hop is the novel bit — single-swap-only caps you below the "best" tier.
- **Unreproducible build.** No KATs / broken `make` reads as unfinished.
- **Real-mainnet scope creep.** Simulation/testnet is expected and sufficient.

---

## Part G — "Perfect" acceptance checklist

**Correctness (the 8-point test, e.g. `make test_las`):**
1. `(pk,sk)=KeyGen()`
2. `(Y,y)=KeyGen()`
3. `σ̂=PreSign(sk,Y,M)`
4. `PreVerify(Y,pk,σ̂,M)==true`
5. **`Verify(pk,σ̂,M)==false`** (expected — pre-sig fails normal verify)
6. `σ=Adapt((Y,y),σ̂)`
7. **`Verify(pk,σ,M)==true`**
8. `y'=Ext(Y,σ,σ̂)` → `A·y'==Y` **and** `y'==y`

**Tier completion:**
- [ ] B1 LAS implementation passes 1–8, 100% over ≥1000 runs
- [ ] B2 atomic-swap demo completes + fairness shown
- [ ] B3 benchmark suite: sizes + timings vs Dilithium
- [x] C1 multi-hop AMHL with timeout/refund + `γ−κ−K` bound
- [ ] C2 both benchmark dimensions, tied to paper/survey numbers
- [ ] C3 critical analysis: knowledge gap, privacy, size tradeoff, LAS-vs-IAS
- [ ] C4 KATs + clean reproducible build + README
- [ ] Dissertation §1–9 drafted, ~8–9k words, figures in place
- [ ] (stretch) C5 second exotic scheme — only if all above solid

**Definition of "perfect":** all B-items + C1–C4 + a dissertation that frames the contribution clearly, evaluates it rigorously against Dilithium, and critiques it honestly. C5 is upside, not a requirement.

---

## Quick reference

- LAS (spec): https://eprint.iacr.org/2020/845
- IAS (rejected alternative): https://eprint.iacr.org/2020/1345
- Survey: https://eprint.iacr.org/2022/1151
- poqeth: https://github.com/ruslan-ilesik/poqeth · https://eprint.iacr.org/2025/091
- Dilithium: https://github.com/pq-crystals/dilithium
