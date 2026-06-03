# LAS Project — Context Handoff for Cowork

> Purpose: full project context so Cowork can help verify Claude Code's implementation, continue the build, and draft the thesis. Drop this at the root of your Cowork project. Read it first.

---

## 1. One-line summary

Implement **LAS** (Lattice-based Adaptor Signatures) by extending a cloned CRYSTALS-Dilithium repo with the four adaptor functions (PreSign / PreVerify / Adapt / Ext), then demonstrate it in a blockchain payment scenario. MSc thesis, University of Manchester, supervisor **Wang Zhipeng**.

## 2. Why this project exists (the big picture)

- Blockchains sign transactions with ECDSA/Schnorr. **Shor's algorithm** on a quantum computer breaks both in polynomial time → any public key reveals its private key → wallets drained.
- "**Post-quantum**" = built on math a quantum computer can't crack quickly (lattices, hashes). NIST standardized *basic* PQ signatures: **Dilithium, Falcon, SPHINCS+**.
- **Exotic** signatures (multisig, ring, group, **adaptor**) add features on top of basic signing. In the post-quantum setting they are mostly **paper-only — little working code, none deployed on a blockchain.** That gap is the thesis.
- **Adaptor signatures** (this project) enable atomic swaps, payment channels (Lightning-style), and payment channel networks.
- The framework being extended is **poqeth** (eprint 2025/091, GitHub: ruslan-ilesik/poqeth), which put *basic* PQ signatures on Ethereum. We extend the idea to an *exotic* one.

The 2×2 that frames everything (supervisor's whiteboard):

|              | Classical (Shor-breakable) | Post-quantum (safe)          |
|--------------|----------------------------|------------------------------|
| **Basic**    | ECDSA, Schnorr, BLS        | Dilithium, Falcon, SPHINCS+  |
| **Exotic**   | ring, group, multisig, adaptor | **← TARGET (mostly no code)** |

## 3. Scheme choice: LAS (not IAS)

Two candidate post-quantum adaptor signatures exist:

- **LAS** (eprint **2020/845**, Esgin–Ersoy–Erkin, ESORICS 2020) — **lattice-based**, built on Dilithium. ← **WE BUILD THIS.**
- **IAS** (eprint 2020/1345, Tairi–Moreno-Sanchez–Maffei) — isogeny-based, built on CSI-FiSh. The competitor.

Why LAS:
- Builds directly on **Dilithium** → repo already cloned, direct code reuse. Supervisor's "adapt the Dilithium implementation" plan *is* LAS.
- Standard **M-SIS / M-LWE** assumptions, mature and NIST-vetted; ~100× faster sign/verify than IAS.
- IAS needs CSIDH-512, which offers only **~60-bit** quantum security (below NIST's 128-bit bar) and is far harder to implement.
- The **Survey (eprint 2022/1151)** explicitly recommends LAS over IAS unless on-chain size is paramount, and calls LAS "an acceptable solution for post-quantum blockchain."

The three references the supervisor pointed to, and their roles:
- **2020/1345 (IAS)** — the alternative considered and rejected (cite for the security-level + Dilithium-reuse justification).
- **2022/1151 (Survey)** — literature-review backbone; positions LAS, supplies comparison table, names the knowledge gap.
- **poqeth (GitHub + 2025/091)** — the integration template; basic PQ on Ethereum.
- **2020/845 (LAS)** — *not* directly sent, but it is the actual spec we implement (Algorithm 2).

## 4. THE critical implementation fact — variant (B)

LAS comes in two algebraic arrangements. The paper (2020/845, Algorithm 2) and IAS both confirm **variant (B)**:

- **Pre-signature ẑ is the clean / normal-sized response.**
- **Adapt ADDS the witness:** `σ = (c, ẑ + y)` → the *full* signature carries the offset.
- **Extract SUBTRACTS:** `y = z − ẑ` (full minus pre).

> ⚠️ An earlier draft of this project mistakenly used variant (A) (offset on the pre-sig, Adapt subtracts). That was corrected after reading the paper. **Use variant (B).**

### Exact algorithms (paper's simplified scheme)

```
KeyGen = Gen:   r ← S_1^(n+ℓ);   t = A·r;   (pk, sk) = (t, r)

Sign:           y ← S_γ^(n+ℓ);   w = A·y;   c = H(pk, w, M);   z = y + c·r
                reject if ‖z‖∞ > γ − κ;   return σ = (c, z)

Verify:         reject if ‖z‖∞ > γ − κ;   w' = A·z − c·t
                check c == H(pk, w', M)

PreSign(sk,Y,M):  y ← S_γ^(n+ℓ);   w = A·y;   c = H(pk, w + Y, M);   ẑ = y + c·r
                  reject if ‖ẑ‖∞ > γ − κ − 1;   return σ̂ = (c, ẑ)

PreVerify(Y,pk,σ̂,M): reject if ‖ẑ‖∞ > γ − κ − 1;   w' = A·ẑ − c·t
                     check c == H(pk, w' + Y, M)

Adapt((Y,y),σ̂):  if PreVerify fails return ⊥;   return σ = (c, ẑ + y)

Ext(Y,σ,σ̂):      s = z − ẑ;   if Y ≠ A·s return ⊥;   return s
```

Two non-negotiable details:
1. **The statement Y must be folded into the hash:** `c = H(pk, w + Y, M)`. This `+Y` term is the entire adaptor mechanism. Omitting it = not LAS, not secure.
2. **PreSign uses the STRONGER bound** `‖ẑ‖∞ > γ − κ − 1` (not `γ − κ`), so that after Adapt adds the ≤1-norm witness, the full signature lands at `‖z‖∞ ≤ γ − κ` and passes normal Verify (Lemma 1).

### Decisions settled with Claude Code

- **Variant:** (B).
- **Implement the paper's SIMPLIFIED scheme**, NOT optimized Dilithium. Disable Power2Round, the hint vector `h`, and high/low-bit decomposition. Use the repo only for primitives: NTT, SHAKE/Keccak, polynomial arithmetic, ternary sampling. (Reason: the clean identity `A·z − c·t = w + Y` only holds without the hint/rounding optimizations; bolting the adaptor onto full Dilithium causes hint-mismatch verification failures.)
- **Parameters (from the paper):** ring degree `d = 256`; `n = ℓ = 4`; `q ≈ 2^24`; `κ = 60`; `γ = κ·d·(n+ℓ) = 122880`. Secret/witness coefficients are **ternary** (S_1, i.e. {−1,0,1}). Challenge `c`: `‖c‖_1 = κ = 60`, `‖c‖_∞ = 1`. Signature is `σ = (c, z)`. Signature size ≈ 3210 bytes.
- **Witness = a Dilithium keypair:** statement/witness generation runs KeyGen — `Y` is a public key, `y` its secret key (ternary, norm ≤ 1). NOT a γ-sized vector.
- **File layout:** new `las.c` / `las.h`, keep upstream Dilithium untouched (clean diff = clean contribution + fair benchmarks).

### Known caveat (note in thesis, do NOT need to solve)

The "**knowledge gap**": an honest witness has norm ≤ 1, but an *extracted* witness is only guaranteed norm ≤ 2(γ−κ). Across a long payment-channel path the witness norm grows (LAS handles this by changing the bound to `γ − κ − K` for path length K). This is also the privacy limitation IAS criticizes. Acknowledge it; don't fix it.

### Out of scope (supervisor's instruction)

No security proofs, no detailed security analysis. Implement + benchmark only. Treat the lattice hardness as a black box.

## 5. Current state

**Done:** minimum + better tiers — working LAS implementation + atomic-swap demo + benchmarks vs plain Dilithium.

**Success ladder (supervisor):**
- ✅ min: working exotic PQ signature + integrated into a basic blockchain app
- ✅ better: benchmark vs the basic/classical version (vs Dilithium)
- ⏳ best: a second exotic scheme, or richer integration

**Next stage chosen: realistic chain integration (#2).** Go beyond the standalone swap to an HTLC-style / payment-channel demo closer to the poqeth template — timeouts, refund paths, a **multi-hop AMHL payment** (LAS Fig. 2, with the `‖ẑ‖∞ > γ − κ − K` bound change). This exercises the genuinely novel part of LAS and directly strengthens the "on a blockchain" thesis claim. Keep the chain layer a **simulation or testnet**, not mainnet — the HTLC/timeout/multi-hop logic is what's being demonstrated.

Suggested order after #2: report scaffolding in parallel → second scheme only if time remains → bit-packing/KAT polish last.

## 6. ✅ Verification checklist for Claude Code's work

Use this to confirm the implementation is correct.

### 6a. Test assertions (the core correctness tests, e.g. `make test_las`)

1. `(pk, sk) = KeyGen()`
2. `(Y, y) = KeyGen()`  ← statement/witness is just another keypair
3. `σ̂ = PreSign(sk, Y, M)`
4. `PreVerify(Y, pk, σ̂, M) == true`
5. **`Verify(pk, σ̂, M) == false`** ← pre-sig must FAIL normal verify (hash missing `+Y`). This is expected, not a bug.
6. `σ = Adapt((Y,y), σ̂)`
7. **`Verify(pk, σ, M) == true`** ← adapted sig MUST pass (`ẑ + y` stays within `γ − κ`)
8. `y' = Ext(Y, σ, σ̂)` → assert `A·y' == Y` **and** `y' == y`

### 6b. Code-level checks (review `las.c` / `las.h`)

- [ ] **Hash includes the statement:** PreSign and PreVerify compute `H(pk, w + Y, M)` (with `+Y`), while plain Sign/Verify use `H(pk, w, M)` (without). If `+Y` is missing → wrong.
- [ ] **Adapt ADDS:** `σ = (c, ẑ + y)`. If it subtracts → that's the wrong variant (A).
- [ ] **Extract SUBTRACTS full − pre:** `s = z − ẑ`. (Equivalently `ẑ` from σ̂ minus `z`? No — must be `z − ẑ`.)
- [ ] **PreSign bound is `γ − κ − 1`**, stronger than Sign's `γ − κ`. If both use `γ − κ` → adapted sigs will sometimes fail Verify.
- [ ] **Witness sampling is ternary** (S_1), via KeyGen — not γ-sized.
- [ ] **Simplified scheme:** no Power2Round / no hint vector `h` / no high-low decomposition. Signature is `(c, z)`, not `(c̃, z, h)`.
- [ ] **Parameters:** `d=256, n=ℓ=4, q≈2^24, κ=60, γ=122880`. Mode is the paper's simplified params, not stock Dilithium-2/3.
- [ ] **Primitives reused** from the repo (NTT, SHAKE/Keccak, poly arithmetic) — not reimplemented.
- [ ] **Rejection sampling** for the `+y` offset is wrapped inside the existing Sign loop, not a second loop bolted on after.

### 6c. Benchmark sanity

- [ ] Reports KeyGen, statement-gen, PreSign vs Sign, PreVerify vs Verify, Adapt, Extract times.
- [ ] Signature sizes printed: `|σ̂|` and `|σ|` (expect ≈ 3210 bytes; paper says ~2701 for optimized Dilithium, larger here because unoptimized).
- [ ] Correctness rate 100% over many iterations.
- [ ] PreSign/Sign show a few % more restarts is normal (rejection sampling).

### 6d. For the #2 multi-hop integration (once built)

- [ ] Uses the **stronger bound `γ − κ − K`** for path length K (not `γ − κ − 1`).
- [ ] Sender computes `y_j = Σ_{i≤j} l_i`, `Y_j = A·y_j`; each hop checks the homomorphism `A·l_j + Y_{j-1} = Y_j`.
- [ ] Release propagates backwards: each `I_j` extracts `y_j` from the downstream sig, computes `y_{j-1} = y_j − l_j`, adapts its own pre-sig.
- [ ] Timeout/refund path: if a hop aborts, upstream cannot complete (aEUF-CMA) — demonstrate this.
- [ ] Chain layer is simulation/testnet, not mainnet.

## 7. How to bring work into Cowork for review

Paste any of these and ask Cowork to check against §6:
- Output of `make test_las` (especially assertions 5 and 7).
- `las.c` / `las.h` source.
- Any failing error + the relevant function.
- Benchmark table.

## 8. Reference links

- LAS paper (the spec): https://eprint.iacr.org/2020/845
- IAS (competitor): https://eprint.iacr.org/2020/1345
- Exotic signatures survey: https://eprint.iacr.org/2022/1151
- poqeth (integration template): https://github.com/ruslan-ilesik/poqeth  ·  paper https://eprint.iacr.org/2025/091
- Dilithium reference: https://github.com/pq-crystals/dilithium

## 9. Scope / logistics

- No ethical approval needed.
- Timeline: scoping done; summer = implementation. Currently mid-implementation, core + benchmarks complete, building richer integration.
- Language: C (Dilithium reference base); demo layer can be Python.
