# Project context — LAS on Dilithium for blockchain

## One-line goal
Implement LAS (Lattice-based Adaptor Signatures, eprint 2020/845) by reusing the
CRYSTALS-Dilithium reference primitives, then demonstrate it in a post-quantum
blockchain **atomic-swap** scenario, with everything benchmarked and documented.

## Status (living)
- ✅ **LAS implemented and tested** — `ref/las.{c,h}`, scheme **variant (B)** (the
  paper's Algorithm 2). `ref/test/test_las.c` passes 200 iters on Dilithium
  modes 2/3/5, zero compiler warnings.
- ✅ **Atomic-swap demo** — `ref/test/test_swap.c` (narrated two-party, two-chain
  swap with assertions).
- ✅ **Realistic chain integration** — `ref/chain.{c,h}` (scriptless-HTLC ledger:
  accounts, block height, adaptor-locked contracts with claim + timeout-refund) +
  `ref/test/test_pcn.c` (atomic-swap happy path, timeout/refund, multi-hop PCN).
  **Model:** same-Y scriptless HTLC (all hops share one statement). Not AMHL.
- ✅ **Benchmarks** — `ref/test/bench_las.c` (per-op timings with rejection-rate and
  three-column size table) and `ref/test/bench_compare.c` (LAS vs Dilithium-3).
  Measured: Sign=788µs, Verify=189µs, PreSign=814µs, PreVerify=193µs, Adapt=203µs,
  Ext=65µs. Acceptance rate ~23% per attempt (expected for simplified scheme).
  Sizes: in-memory sig=9216B; theoretical packed=4676B; paper's optimised=~3210B
  (different scheme — not directly comparable).
- ✅ **Full design write-up** — `docs/LAS.md` (report source material, includes
  literature/methodology section §1.1 for assessment rubric).
- ✅ **Theory↔implementation bridge** — `docs/THEORY_IMPL_BRIDGE.md` (every paper
  equation mapped to C function/line).
- ✅ **Code pushed to GitHub** — on branch `main`, up to date with `origin/main`.
  PR #1 merged. No unpushed commits.
- **TODO: AMHL** — `test_pcn.c` uses same-Y model. The paper's Adaptor Multi-Hop
  Lock (AMHL) requires per-hop cumulative witnesses `Y_j = A·(l_1+…+l_j)` and
  PreSign bound `γ−κ−K`. Not yet implemented. See `docs/LAS.md §9`.

## Why this project exists
- Blockchains sign with ECDSA/Schnorr; Shor's algorithm breaks both. "Post-quantum"
  = built on lattice/hash problems Shor can't solve.
- NIST standardised *basic* PQ signatures (Dilithium, Falcon, SPHINCS+).
- *Exotic* signatures (multisig, ring, group, **adaptor**) add features but in the
  PQ setting are mostly **paper-only** — little working code, none on a blockchain.
  Closing that gap is the thesis.
- Adaptor signatures enable atomic swaps / payment channels (scriptless scripts).

## Key design fact
An exotic scheme = a basic scheme + extra functions. LAS = Dilithium-style
Fiat-Shamir-with-aborts signature + PreSign / PreVerify / Adapt / Ext. We **reuse
Dilithium's poly/NTT/SHAKE/sampling internals** and do not reinvent lattice
arithmetic. LAS itself is built as a small *self-contained* scheme (its own
dimensions and parameters) layered on those primitives.

## The LAS mechanism (variant B — the paper, Algorithm 2)
Earlier notes described a "variant A" (`z̃ = z + y`, statement subtracted at
verify). That was **superseded**: the paper specifies variant B, implemented here.
- Statement/witness `(Y, y)` is **literally another key pair**: `y ← S_1^{n+ℓ}`
  (ternary), `Y = A·y`. Knowing `Y` doesn't reveal `y` (Module-SIS/LWE hard).
- **The core mechanism: the statement is folded into the Fiat–Shamir hash.**
  Sign uses `c = H(pk, w, M)`; **PreSign uses `c = H(pk, w + Y, M)`**.
- `PreSign(sk,Y,M)`: `ẑ = y + c·r`, reject if `‖ẑ‖∞ > γ−κ−1`. Pre-sig `σ̂=(c,ẑ)`.
- `PreVerify(Y,pk,σ̂,M)`: recompute `w' = Aẑ − c·t`, check `c == H(pk, w'+Y, M)`.
- `Adapt((Y,y),σ̂)`: `σ = (c, ẑ + y)`. Now standard `Verify` sees `Az−ct = w+Y`,
  which matches `c` — so the adapted signature is a **fully ordinary** signature.
- `Ext(Y,σ,σ̂)`: `y = z − ẑ`; return it iff `A·y == Y`.
- **On-chain leak (why swaps are atomic):** publishing the adapted `σ` lets anyone
  holding `σ̂` recover `y = z − ẑ` and complete the matching half of the swap.

## THE failure mode to watch (variant B)
The bound budget, not packing. PreSign rejects at the **tighter** `γ−κ−1`; the
ternary witness has `‖y‖∞ ≤ 1`, so the adapted `z = ẑ + y` satisfies
`‖z‖∞ ≤ γ−κ` and clears ordinary Verify. If you loosen PreSign to `γ−κ`, adapted
signatures can exceed the bound and Verify rejects everything. (`γ = κ·d·(n+ℓ)`
governs the MSIS hardness parameter; the acceptance rate is ~23% per attempt —
expected for the simplified scheme without hint vector.)

## Known caveat (note in thesis, do NOT need to solve)
"Knowledge gap": here the extracted `y` is **exact**; in the paper's relaxed
setting the witness can carry noise that grows across long payment-channel chains.

## Modulus note
Paper uses `q ≈ 2^24`. We reuse Dilithium's NTT, whose root-of-unity table is
fixed to `Q = 8380417 (≈2^23)`, so this build uses that `Q`. `Q > 2γ`, so
correctness holds; only the concrete MSIS/MLWE security margin changes (out of
scope per supervisor). Exact `2^24` would need a new NTT table or schoolbook mult.

## Scope discipline (from supervisor)
- Target dilithium3 build (NIST level ~2/3) — LAS code is mode-independent and is
  built/tested under `-DDILITHIUM_MODE=3` (also 2/5 for portability).
- Do NOT implement/analyse security proofs. Implement + benchmark + demo only.
- Success ladder: (min) working LAS + basic blockchain demo ✅;
  (better) benchmark vs plain Dilithium ✅ (`bench_compare`); (best) a second
  exotic scheme — **open** (needs a choice: ring / threshold / multisig).
- **TODO: AMHL** — same-Y PCN is implemented; AMHL (K-hop bound, distinct per-hop
  statements) is next before claiming full PCN completion. ~120 lines of code.
- **TODO: report scaffolding** — `docs/LAS.md` has the technical content; the
  8000-word dissertation chapter needs to be drafted from it.

## Reference
- LAS paper: eprint 2020/845 (Esgin, Ersoy, Erkin).
- poqeth (integration template): eprint 2025/091.
- Full design + math + results: `docs/LAS.md`.

# Report 
2.1 Report
Reports are expected to be around 8,000 words long. Submissions significantly outside the range of 7,000-9,000 will be penalised. References, appendices,
and figure/diagram/table captions are not included in the word count.
In the new report format, there is no separate “Background” section. Instead, the “Introduction” should present the subject area clearly and include
a concise literature review. The focus should be on depth rather than breadth, highlighting key works necessary to understand the problem and justify
your approach. An extensive literature review is thus not required.

2.2 Video
The video should be 6–8 minutes long, structured with a clear beginning, middle, and end.
The video should complement the report, adding additional insight that was not possible in text alone. This might be, for example, animations or demos
of the software in action, interactive visualisations to highlight key trends, or real-time simulations that help the audience better understand complex
concepts and workflows.
For the format, a simple voice-over on a PowerPoint-style presentation is acceptable, but more creative, polished productions will score higher in the
“Use of Medium” category. A talking-head overlay (you speaking in the corner) is strongly encouraged.

# Assessment Criteria
3 Assessment Criteria
3.1 Report
Please note, the below criteria are not intended to be strict section heading requirements—the exception being that every project should have separate
Abstract and Conclusion sections.
3.1.1 Abstract (5%)
This section is expected to clearly present a concise summary of the project’s purpose, methods, and key results.
• Does the abstract give an appropriate executive summary of the work?
3.1.2 Introductory Material (20%)
You should clearly describe the project setting, scoping the subject area with proper citations and figures, and stating the objectives.
• Does the work effectively establish the context (why this project matters), and clearly explain the subject area (what this project is about), with
proper citations/figures?
• Are the project objectives clearly stated, coherent and appropriate?
3.1.3 Methodology (20%)
You should clearly explain (using figures, diagrams, and tables where appropriate) how the project goals are achieved by describing the methods used.
You must include a justification and reasoning behind selecting these methods, demonstrating their suitability for the project, and consideration of
alternative approaches.
• Is it clear what methods were used to achieve the project goals?
• Is there a well-reasoned justification for why these methods were chosen and compared/contrasted with alternative approaches?
3.1.4 Evaluation and/or Reflection (20%)
You should present the evaluation, testing, or critical reflection (using figures, diagrams, and tables, where appropriate). You should also provide a
justification for the evaluation methods chosen and demonstrate alignment with the project goals.
• Has appropriate evaluation, testing, or critical reflection, relevant to the project’s subject area been considered?
• Is there clear justification and alignment of the evaluation methods with the project goals?
3.1.5 Conclusion (10%)
This section should present clear, well-justified conclusions supported by the project’s outcomes and aligned with its original objectives. It should also
include thoughtful analysis of the project approach, offering well-reasoned suggestions for future work.
• Are the project conclusions clearly stated, drawing on appropriate evidence from the report body, and overall aligned with the objectives as proposed
in the introduction?
• Is there a thoughtful analysis on the project process with well-justified suggestions for future work?
3.1.6 Format and Structure (5%)
This criterion evaluates the organisation and clarity of the report. Your report should have a logical structure with clearly defined sections, properly
numbered figures, tables, and equations, and correctly formatted references. The writing should ideally be clear and well-expressed.
• Is the report well-structured and professionally presented?
• Are references complete, correctly formatted, and consistent with academic standards?
3.1.7 Project Achievement (20%)
This criterion assesses the overall achievement of the project as can be determined from the report. It evaluates two key aspects: ’the complexity, scope
and challenges’ and ’the execution quality’, meaning how well the artefact is built, its reliability, and technical accuracy.
• How challenging/complex is the proposed artefact in terms of design and functionality, and how wide is the scope?
• How effectively is the artefact implemented, in terms of build quality, reliability, and technical accuracy?
3.2 Video
3.2.1 Use of the Medium (40%)
This criterion evaluates how effectively the video format communicates the project through visuals, audio, and editing, ensuring clarity and engagement.
Creativity is encouraged if it enhances the message without excessive production effort.
• How well does the video use visuals and audio to clearly and professionally present the project?
• How much effort and professionalism are evident in the creation of the video?
3.2.2 Complementing the Report (40%)
This assesses whether the video presents aspects of the project that were completed during the project, but not fully covered in the written report,
offering clarification that may be difficult to convey through text or figures alone.
• Does the video reveal additional aspects of the project not included in the report?
• How effectively do these additions demonstrate the completeness of the project and its alignment with the original goals?
3.2.3 Presentation (20%)
This criterion assesses how effectively the video communicates the project within the expected duration (6-8 minutes), balancing clarity and depth with
conciseness. It also evaluates the structure, pacing, and alignment of spoken content with visuals to ensure a smooth and engaging presentation.
• Is the video clear, well-paced, and logically structured, staying within the required duration?