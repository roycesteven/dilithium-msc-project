# LAS Project — Knowledge Mastery Map

> Everything you need to understand to nail this project, from foundations to the specific scheme. Depth is calibrated to the supervisor's scope (security proofs are out; core algorithms, the exotic-vs-basic differences, and the engineering are in).

**Depth key**
- 🎯 **Core** — master deeply; this is literally in your code, your evaluation, or your thesis argument.
- 📘 **Working** — functional understanding; enough to read the papers and modify the code.
- 🗺️ **Context** — just the gist; for framing and the related-work chapter.

---

## Layer 0 — Mathematical foundations

- 🎯 **Modular arithmetic** over `Z_q` (add/multiply mod q). Everything happens mod `q ≈ 2^24`.
- 🎯 **Polynomial (cyclotomic) rings** `R_q = Z_q[X]/(X^d + 1)`, `d = 256`. A single "number" in LAS is a degree-255 polynomial with coefficients mod q. Know how to add and multiply two ring elements (multiplication wraps via `X^d = −1`).
- 🎯 **Modules / vectors & matrices over `R_q`.** Keys and signatures are *vectors of polynomials*; the public matrix `A` is a matrix of polynomials. "Module" just means "vector space but over a ring." You need `A·v` (matrix–vector product over the ring).
- 🎯 **Norms** — `‖·‖∞` (max absolute coefficient) above all; also `‖·‖₁` and `‖·‖₂`. The entire correctness/rejection logic is norm bounds (`γ−κ`, `γ−κ−1`). Be fluent in why a small norm matters.
- 📘 **Probability basics** — uniform distributions, why rejection sampling produces a distribution independent of the secret. Enough to explain restart counts.
- 🗺️ **Finite fields / NTT-friendly primes** — why `q` is chosen so `R_q` supports fast multiplication (leads into NTT, Layer 6).

## Layer 1 — Lattices & hard problems

- 📘 **What a lattice is** — integer linear combinations of basis vectors; the geometric picture (shortest/closest vector).
- 📘 **SVP / CVP** — shortest and closest vector problems; the root hardness.
- 🎯 **SIS and LWE** — Short Integer Solution (find short `v` with `Av = 0`) and Learning With Errors (`As + e` looks random). These *are* the security of your scheme — know what each problem **asserts**.
- 🎯 **Module variants: M-SIS and M-LWE** — the exact assumptions LAS and Dilithium rest on. Know the one-line statement of each and that `n=ℓ=4` sets their dimension here.
- 🎯 **Ajtai's hash `f_A(x) = A·x`** — one-way, collision-resistant under SIS, and **additively homomorphic** (`f_A(a+b)=f_A(a)+f_A(b)`). The homomorphism is *why* Adapt/Extract and the AMHL multi-hop work — this is the single most important structural fact in the project.
- 🗺️ **Worst-case to average-case reduction** (Ajtai) — why lattice problems are believed hard *on average*. Gist only; you treat hardness as black-box.

> Scope note: you do **not** prove any of these are hard. You need to *state* them and *use* the homomorphism. That's it.

## Layer 2 — Cryptography fundamentals

- 🎯 **Digital signatures = (KeyGen, Sign, Verify).** The mental model for everything.
- 🎯 **Hard relation (statement Y, witness y).** `(Y,y) ∈ R` with Y public, y secret, y hard to recover from Y. In LAS the relation is `Y = A·y` with y ternary — i.e. Y is a public key and y its secret key.
- 🎯 **Hash functions + the Random Oracle Model (ROM).** You model `H` as a random oracle; know what that assumption buys and that LAS's security is "in the ROM."
- 🎯 **Fiat–Shamir transform** — turning an interactive identification protocol into a signature by deriving the challenge from a hash. Dilithium and LAS are Fiat–Shamir signatures.
- 🎯 **Schnorr signatures** — the classical ancestor. Understand Schnorr deeply; Dilithium/LAS are "Schnorr over lattices with rejection sampling." Also the classical adaptor signature is Schnorr-based, so this anchors the comparison.
- 📘 **Security notions: EUF-CMA, SUF-CMA**, and the adaptor variant **aEUF-CMA**. Know what each guarantees (you cite them; you don't prove them).
- 📘 **Zero-knowledge proofs (ZKP), high level** — prove you know a witness without revealing it; the commit–challenge–response shape. Needed to understand (a) the knowledge gap, (b) why LAS-based apps need an extra norm proof, (c) ring signatures if you do the stretch goal.

## Layer 3 — Post-quantum context & Dilithium

- 🎯 **Shor's algorithm** — factors / solves discrete log in polynomial time on a quantum computer, breaking ECDSA/Schnorr. The motivation for the whole project. Know *what* it breaks and *why* that empties wallets.
- 🗺️ **Grover's algorithm** — square-root speedup on search; why hash/symmetric sizes merely double. Gist.
- 🗺️ **PQ assumption families** — lattice, hash, code, multivariate. One line each; lets you place LAS in the **lattice** family in related work. *(Scope: this project focuses solely on LAS; no alternative-scheme comparison.)*
- 🗺️ **NIST PQC standardization** — the competition; finalists Dilithium, Falcon (lattice), SPHINCS+ (hash). Framing.
- 🎯 **CRYSTALS-Dilithium — the full scheme.** This is your base. Master: KeyGen/Sign/Verify, **Fiat–Shamir with Aborts**, **rejection sampling** (`‖z‖∞ ≤ γ−β` style bounds and why you restart), the challenge set `C` (`‖c‖₁=κ=60, ‖c‖∞=1`).
- 🎯 **Dilithium's optimizations — and why LAS drops them.** Power2Round, the **hint vector `h`**, high/low-bit **decomposition**, public-key compression. You must understand these well enough to *disable* them: the clean identity `A·z − c·t = w + Y` only holds in the **unoptimized** scheme, so the simplified LAS avoids hint-mismatch verification failures. This is a key implementation-and-thesis point.

## Layer 4 — Adaptor signatures & LAS (the heart)

- 🎯 **Adaptor signature = (PreSign, PreVerify, Adapt, Ext)** on top of a base signature. The concept: a pre-signature that completes into a full signature *only* with a witness, and whose completion *reveals* that witness.
- 🎯 **The three security properties** — **aEUF-CMA** (unforgeable even given a pre-sig), **pre-signature adaptability** (valid pre-sig + witness ⇒ valid sig), **witness extractability** (pre-sig + sig ⇒ witness). State them; the paper proves them.
- 🎯 **LAS Algorithm 1 (the simplified Dilithium-like signature)** — `r←S_1`, `t=Ar`; `y←S_γ`, `w=Ay`, `c=H(pk,w,M)`, `z=y+cr`, reject if `‖z‖∞>γ−κ`.
- 🎯 **LAS Algorithm 2 (the adaptor part), variant (B)** — the exact thing you implement:
  - PreSign: `c = H(pk, w + Y, M)`, `ẑ = y + cr`, **stronger** bound `‖ẑ‖∞ > γ−κ−1`.
  - PreVerify: recompute with `+Y` in the hash, check the `γ−κ−1` bound.
  - **Adapt: `σ = (c, ẑ + y)`** (adds the witness).
  - **Ext: `s = z − ẑ`**, check `A·s = Y` (subtracts; full minus pre).
  - **Why it's shaped this way:** the `+Y` in the hash binds the statement; the tighter `γ−κ−1` bound guarantees `ẑ+y` still satisfies the normal `γ−κ` bound so the adapted sig verifies (Lemma 1); the homomorphism of `f_A` makes `A(z−ẑ)=Y` so extraction recovers the witness.
- 🎯 **The knowledge gap (R ⊆ R′).** Honest witness has norm ≤ 1; an *extracted* witness only guarantees norm ≤ 2(γ−κ). Know what this means practically (witness norm grows along a payment path → the `γ−κ−K` bound for path length K) and that it forces apps to add a norm ZKP. This is prime critical-analysis material.
- 🎯 **Why LAS is the right (and only) choice here** — lattice-based, 128-bit Module-SIS/Module-LWE security, and a *direct* reuse of the Dilithium reference C code (the four adaptor functions are additions, not rewrites). That practicality — a working implementation on a mature, NIST-track primitive — is the whole justification; the project deliberately scopes to LAS alone.

## Layer 5 — Blockchain & applications

- 📘 **Blockchain basics** — decentralized ledger, miners/validators, transactions as scripts.
- 📘 **UTXO model & scripting** — Bitcoin-style coins with spending conditions (signature, hash-preimage, timelocks). LAS's applications assume a UTXO chain supporting these.
- 🎯 **Signatures in blockchain** — how a signature authorizes spending; why replacing the signature scheme matters; why on-chain *size* and *verification cost* are the metrics that count.
- 🎯 **Scriptless scripts / adaptor signatures on-chain** — embedding a condition inside a signature so miners just verify an ordinary signature while parties enforce a hidden condition. The "why adaptor signatures help" argument (lower on-chain cost, fungibility, functionality beyond the script language).
- 🎯 **Atomic swaps** — cross-chain fair exchange; your minimum demo. Know the fairness property and how Extract enforces it.
- 🎯 **Payment channels & the Lightning Network** — create/update/close; off-chain throughput. Context for the channel demo.
- 🎯 **Payment channel networks (PCN) & AMHL (anonymous multi-hop locks)** — multi-hop routing; the sender's `y_j = Σ l_i`, `Y_j = A·y_j` construction; backward release `y_{j-1} = y_j − l_j`. This is your "best-tier" #2 integration (LAS Fig. 2).
- 📘 **HTLC (hash-time-lock contracts)** — the Lightning synchronization primitive AMHL replaces; know its wormhole/privacy issues (motivation for AMHL).
- 📘 **Ethereum / Solidity & gas** — poqeth targets Ethereum; on-chain verification costs gas (SSTORE etc.). Needed if your integration touches Solidity; otherwise use transaction/signature size as the proxy.
- 🎯 **poqeth** — the framework you extend: it put *basic* PQ signatures (W-OTS+, XMSS, SPHINCS+, MAYO) on Ethereum. Your contribution is doing it for an *exotic* one. Know its approach and its gas figures as a baseline.

## Layer 6 — Implementation & evaluation skills

- 🎯 **C programming** — the Dilithium reference is C; you read and extend it. Pointers, structs, fixed-width integer types, arrays of polynomials.
- 🎯 **NTT (Number Theoretic Transform)** — Dilithium's fast polynomial multiplication (the FFT analogue mod q). You don't reinvent it, but you must understand it well enough to call it correctly when computing `A·y`, `A·z`, `A·s`.
- 📘 **SHAKE / Keccak (XOFs)** — the hash/extendable-output function used for `H`, challenge sampling, and expanding `A` from a seed. Know its role; reuse the repo's implementation.
- 📘 **Rejection sampling in code** — wrapping the `+y` offset inside the existing Sign loop; how a restart works.
- 📘 **Constant-time / side-channel awareness** — why crypto avoids secret-dependent branches/timing. Mention in the engineering discussion; full hardening is optional.
- 🎯 **Benchmarking methodology** — averaging over ≥1000 iterations, reporting CPU/clock, measuring sizes (bytes) and times (ms), restart counts, correctness rate. The evaluation chapter lives here.
- 🎯 **Known-Answer Tests (KATs) & build/test discipline** — deterministic test vectors, `make test_las` / `make bench_las`, reproducibility. Examiners reward "I can rebuild this."
- 📘 **Python** — for the swap / multi-hop demo layer driving the compiled C.

## Layer 7 — Research & dissertation skills

- 🎯 **Reading crypto papers (the supervisor's method)** — find the boxed core algorithm; compare against the basic version; focus on the *differences*; **skip the security proofs**; treat unfamiliar structures as black boxes.
- 🎯 **Literature review & citation** — positioning LAS against the survey (2022/1151) and poqeth; the 2×2 framing (basic/exotic × classical/PQ).
- 🎯 **Evaluation as argument** — every number needs a baseline: LAS vs **basic Dilithium-3** (PQ basic) and vs the **classical ECDSA adaptor** (classical exotic), plus the paper's own size estimate.
- 🎯 **Critical analysis** — honest treatment of the knowledge gap, the privacy limitation, the simplified-vs-optimized size tradeoff. This is what separates a distinction from a merit.
- 📘 **Technical writing** — clean prose, figures (the 2×2, the PreSign→Adapt→Ext flow, the multi-hop diagram, benchmark plots), ~8–9k words.

---

## Suggested learning order

1. **Layer 2 + Schnorr** (signatures, Fiat–Shamir, hard relations) — the conceptual spine.
2. **Layer 0** (rings, modules, norms) — the language LAS is written in.
3. **Layer 1** (SIS/LWE, module variants, the `f_A` homomorphism) — the security and the structural trick.
4. **Layer 3 / Dilithium** — your base scheme, including the optimizations you'll disable.
5. **Layer 4 / LAS Algorithm 2** — the scheme itself; re-read with Layers 0–3 in hand and it stops being scary.
6. **Layer 5** (atomic swap → payment channels → AMHL, poqeth) — the application context for integration.
7. **Layer 6** (C, NTT call sites, benchmarking, KATs) — in parallel with implementation.
8. **Layer 7** — throughout, especially as you write up.

## What you can safely keep shallow

Security proofs and reductions (Lemmas/Theorems in the papers); worst-case-to-average-case lattice reductions; QROM subtleties; the deep internals of NTT and Keccak. Wang explicitly put these out of scope — understand *what* they claim, not *how* they're proven.

## Reference

- LAS: https://eprint.iacr.org/2020/845 · Survey: https://eprint.iacr.org/2022/1151
- poqeth: https://github.com/ruslan-ilesik/poqeth · https://eprint.iacr.org/2025/091 · Dilithium: https://github.com/pq-crystals/dilithium
