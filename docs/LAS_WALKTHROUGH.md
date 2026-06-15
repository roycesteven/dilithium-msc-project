# LAS, end to end — the plain-English walkthrough

*One document that explains the whole project from zero: what problem it solves,
what was built, how it works, and what the numbers mean — written so that a
non-cryptographer can follow it, and so it doubles as the script spine for the
video. Every claim here is backed by code and tests in this repo; pointers to the
exact files are given so you can verify or demo any part.*

> How to read this: Sections 1–4 are the story (no maths). Section 5 opens the
> box gently. Section 6 is what we actually built. Section 7 is the results in
> plain words. Section 8 is the honest caveats. Section 9 is "run it yourself."
> Section 10 is a glossary — flip to it whenever a word is unfamiliar.

---

## 1. The problem, in one breath

Every coin on Bitcoin or Ethereum is protected by a **digital signature** — a bit
of maths that proves "the owner approved this payment." Today's signatures
(ECDSA, Schnorr) rely on a puzzle that ordinary computers can't solve but a future
**quantum computer can** (via *Shor's algorithm*). When that day comes, anyone
could forge approvals and drain wallets. The fix is **post-quantum** signatures,
built on a different puzzle (lattices) that quantum computers are *not* known to
crack.

The catch: the world has *post-quantum* versions of **basic** signatures
(NIST standardised CRYSTALS-Dilithium), but not of the **fancy** signatures that
make modern blockchains interesting. This project builds and demonstrates one of
those fancy ones — an **adaptor signature** called **LAS** — on top of Dilithium,
and shows it working in a real blockchain scenario (an atomic swap).

---

## 2. The big picture (the 2×2 that frames everything)

```
                 CLASSICAL (breakable by a quantum computer)   POST-QUANTUM (safe)
              ┌───────────────────────────────────────────┬──────────────────────────┐
   BASIC      │  ECDSA, Schnorr  (every coin today)        │  Dilithium / ML-DSA       │
 (just sign)  │                                            │  (NIST standard)          │
              ├───────────────────────────────────────────┼──────────────────────────┤
   EXOTIC     │  adaptor sigs, ring, multisig …            │  ★ mostly EMPTY — paper-  │
 (sign +      │  (e.g. Bitcoin DLCs use ECDSA-adaptors)    │    only, almost no code   │
  a trick)    │                                            │    ←  THIS PROJECT fills  │
              │                                            │       the adaptor cell    │
              └───────────────────────────────────────────┴──────────────────────────┘
```

Our contribution is the **bottom-right cell**: the first public, working
implementation of an *exotic* post-quantum signature (LAS), and the first time one
is demonstrated in a blockchain workflow. We benchmark it against the two cells
next to it — basic post-quantum (Dilithium) and classical-exotic (an ECDSA
adaptor) — to show exactly what going post-quantum costs.

---

## 3. What is an *adaptor* signature? (a locked-box analogy)

A normal signature just says "approved." An **adaptor signature** adds a twist:

> It is a signature with a **secret padlock**. You can *almost* finish it, but the
> last click only happens when someone supplies a specific secret key. And the
> moment that finished signature appears in public, **anyone watching learns the
> secret.**

That double property — "completing it needs the secret" **and** "completing it
reveals the secret" — is the magic. It lets two strangers trade safely:

- **PreSign** → make a *pre-signature*: the locked, not-yet-valid signature.
- **PreVerify** → check a pre-signature is genuine and bound to the right padlock.
- **Adapt** → click it shut using the secret → a *normal, valid* signature.
- **Ext(ract)** → given the locked and the finished versions, recover the secret.

The four words map onto a story in the next section.

---

## 4. The killer use case: an atomic swap (Alice ↔ Bob), no jargon

Alice has 1 coin on chain A; Bob has 1 coin on chain B. They want to trade with
**no trusted middleman** and **no risk of one side running off with both**.

```
   Bob invents a secret key  y  and its public "padlock"  Y.

   1) Bob → Alice:  "here is the padlock Y"
   2) Alice pre-signs "pay Bob on chain A", locked to Y      → σ̂_A  (not spendable yet)
   3) Bob   pre-signs "pay Alice on chain B", locked to Y    → σ̂_B  (not spendable yet)
        ── neither pre-signature can move any coin on its own ──
   4) Bob knows y, so Bob ADAPTS σ̂_A → σ_A, a normal signature, and PUBLISHES it
        → Bob gets Alice's coin on chain A.            (this is the only way for Bob to get paid)
   5) Alice was watching chain A. From the published σ_A she EXTRACTS y.
   6) Now Alice knows y too, so she ADAPTS σ̂_B → σ_B and publishes it
        → Alice gets Bob's coin on chain B.
```

Why it's safe ("atomic"): the *same* padlock `Y` locks both sides. Bob can only
take his coin by publishing something that **hands Alice the key** to take hers.
Before that, Alice literally cannot finish her side. So **either both trades
happen or neither does** — enforced by maths, not by trust. (This exact scenario
runs and self-checks in `ref/test/test_swap.c`.)

We also built the richer version — **paying through several people in a chain**
(a payment-channel network), where each hop has its *own* padlock so no
intermediary can cheat the others. That's the "AMHL" part (Section 6.4).

---

## 5. Opening the box — how LAS works, gently

You can skip this section and still understand the project. But here is the gist,
with the maths kept to a minimum.

**Keys are scrambled vectors.** A secret key `r` is a list of tiny numbers
(each −1, 0, or +1 — we call these *ternary*). The public key is `t = A·r`, where
`A` is a big fixed public grid of numbers and `·` is a special multiplication.
The one-way street: computing `t` from `r` is easy; recovering `r` from `t` is the
hard lattice puzzle (this is the *quantum-safe* part). **A "statement/witness"
`(Y, y)` is just another such key pair** — `Y` is the padlock, `y` is its secret.

**Signing is "commit, challenge, respond — and maybe retry"** (this shape is
called *Fiat–Shamir with aborts*):
1. pick a random mask `y`, compute a commitment `w = A·y`;
2. hash everything into a challenge `c = H(public key, w, message)`;
3. respond `z = y + c·r`;
4. if `z`'s numbers got too big, **throw it away and retry** (this keeps the
   secret hidden and the signature valid). On average it takes ~2.7 tries.

**The single trick that makes it an adaptor signature:** when pre-signing, you
fold the padlock into the hash — `c = H(pk, w + Y, M)` instead of `c = H(pk, w, M)`.
That `+ Y` is the *entire* difference. Its consequences fall out by simple algebra:

- A pre-signature **fails** the normal check (the verifier hashes without `+Y`, so
  the numbers don't match) — that's why it's "not spendable yet."
- **Adapt** adds the secret `y` to the response (`z = ẑ + y`); now the normal
  verifier's arithmetic lines up perfectly and accepts it as an ordinary signature.
- **Extract** subtracts (`y = z − ẑ`) to recover the secret. It works because the
  grid multiplication is *additive*: `A·(z − ẑ) = Y`.

**The one number to respect** — the "norm budget." Adding the secret makes the
response slightly bigger. So pre-signing uses a *slightly tighter* size limit
(`γ−κ−1` instead of `γ−κ`) to leave exactly enough room. Get this off by one and
*every* completed signature is rejected — it is the project's main failure mode,
and it's why the code documents the bound so carefully.

*(The exact equation-by-equation mapping to the C code is in
`docs/THEORY_IMPL_BRIDGE.md`; the design write-up is `docs/LAS.md §3–4`.)*

---

## 6. What we actually built (end to end)

Everything is layered **on top of** the unmodified CRYSTALS-Dilithium reference C
code — we reuse its fast maths (NTT, SHAKE hashing, samplers) and **changed zero
of its functions** (`docs/FUNCTION_MAP.md`). The new pieces:

```
   ┌──────────────────────────────────────────────────────────────────────┐
   │  Dilithium reference primitives (REUSED AS-IS — 0 functions modified)  │
   │  polynomial maths · NTT multiply · SHAKE/Keccak hashing · samplers      │
   └───────────────▲──────────────────────────────────────────────────────┘
                   │ calls
   ┌───────────────┴────────────┐   the scheme: KeyGen / Sign / Verify +
   │  ref/las.{c,h}             │   PreSign / PreVerify / Adapt / Ext
   └───┬───────────┬────────────┘
       │           │
       │           ├── ref/serialize.{c,h}  turn objects into BYTES (+ a
       │           │     validating decoder + las_verify_packed = the
       │           │     interface an on-chain verifier would call)
       │           │
       │           ├── ref/amhl.{c,h}        multi-hop locks (paying via a chain
       │           │     of people, each hop its own padlock)
       │           │
       │           └── ref/chain.{c,h}       a tiny pretend-ledger (accounts,
       │                 balances, timeouts) to run swaps on
       │
       └── evm/AdaptorSwap.sol               a REAL Solidity swap contract, run on
             a local Ethereum (Foundry) to measure on-chain gas
```

What each test program proves (all pass, zero compiler warnings):

- **`test_las`** — the core contract, **1000 random trials** on three security
  levels: pre-sig verifies, pre-sig is *not* spendable, adapted sig *is* a normal
  sig, extracted secret is exactly right. 100% correct.
- **`test_swap`** — the Alice/Bob atomic swap of Section 4, every step asserted.
- **`test_pcn`** — the pretend-ledger: a cross-chain swap, a *timeout-refund*
  (nobody loses coins if a party vanishes), and a multi-hop payment.
- **`test_amhl`** — the proper multi-hop version (§6.4 below).
- **`test_serde`** — bytes round-trip perfectly; **flipping any single one of the
  4672 signature bytes makes verification fail** (tamper-evidence); garbage input
  is rejected.
- **`test_kat`** — runs everything from fixed seeds and checks the output against a
  **pinned fingerprint** (`f7fc40…e6b1`), so the build is bit-for-bit reproducible
  on any machine.

### 6.4 The multi-hop part (AMHL), in one picture

Pay Erin through Bob, Carol, Dave. Each hop gets its **own** padlock built so that
unlocking one hop reveals exactly what's needed to unlock the previous one — and
*nothing* about non-neighbouring hops (so a middleman can't steal). Erin (paid
last in the chain, claims first) pulls her hop, which cascades the secret backward:

```
 Alice ─Y₁→ Bob ─Y₂→ Carol ─Y₃→ Dave ─Y₄→ Erin
   claims last  ◄─────────────────────────  Erin claims first
   "no wormhole": the secret for hop 4 cannot open hop 1 (different padlocks) ✓ asserted
```

A subtlety we *measured*: each hop's secret is a sum of small numbers, so its size
grows with the number of hops (1, 2, 3, 4 …). That growth — the famous "knowledge
gap" — is exactly why deeper routes pre-sign with a slightly tighter budget
(`γ−κ−K` for `K` hops). We show it costs essentially nothing in speed (Section 7).

---

## 7. The results, in plain words

Three honest headline findings (all measured on one machine; full tables in
`docs/LAS.md §8` and `docs/STATUS.md`).

**(a) The price of going post-quantum is SIZE, not SPEED.**
A LAS signature is **4,672 bytes** vs **64 bytes** for classical ECDSA — about
**73× bigger**. But the *time* per operation stays under a millisecond and is only
a few times slower. For a blockchain, the bytes are what hurt (you pay for every
byte stored), so size is the real cost — which motivated us to bit-pack the
signature down from 9,216 bytes in memory to 4,672 on the wire.

**(b) Adding the adaptor "trick" is essentially FREE in LAS.**
In the classical ECDSA adaptor, pre-signing is ~4.6× slower than normal signing
(it needs an extra proof). In LAS, pre-signing ≈ normal signing and pre-verify ≈
normal verify — folding `+Y` into a hash costs almost nothing. LAS's pre-verify is
even *absolutely faster* than the classical adaptor's. This is LAS's quiet win.

**(c) On a real blockchain, the protocol runs but on-chain *verification* is the wall.**
We deployed a real Solidity swap contract on a local Ethereum and settled it with
each scheme (`evm/`). Settling with ECDSA costs **75,709 gas**. Just *publishing* a
LAS signature (4,672 bytes of calldata) — before doing any checking — already costs
**208,400 gas**, ~2.75× more; and fully verifying a lattice signature inside the
EVM is infeasible today (it would blow past the block limit). That's the honest
frontier: the *swap works end-to-end*, but cheap on-chain verification needs future
blockchain support (a precompile or a zero-knowledge proof) — the same wall the
"poqeth" project hit for basic post-quantum signatures.

Plus: rejection sampling accepts ~**37%** of attempts (~2.7 tries/signature),
which matches the textbook prediction `(1 − κ/γ)^{2048} ≈ e⁻¹` — a small sign the
implementation behaves exactly as theory says.

---

## 8. What we are honest about (limitations)

- **The "knowledge gap."** In our setting the recovered secret is *exact*; in the
  general theory it can carry noise that grows along long payment routes. We
  *measure* the growth (the secret's size = number of hops) but don't eliminate it.
- **A parameter shortcut.** The paper uses a modulus `q ≈ 2²⁴`; we reuse
  Dilithium's `q ≈ 2²³` so we can use its fast, audited maths unchanged.
  Correctness is unaffected; only the precise security margin differs — a
  documented, supervisor-approved starting point.
- **Simplified scheme.** We omit Dilithium's compression tricks (hints) so the
  adaptor algebra stays clean; the cost is larger signatures (~40% over the
  paper's optimised estimate). Honest trade for a transparent research artefact.
- **On-chain verification** is not yet feasible natively (Section 7c) — flagged as
  future work, not hidden.
- **Privacy & constant-time** hardening are out of scope.

---

## 9. Run it yourself (for the demo / video)

```sh
cd ref
make test/test_las3   && ./test/test_las3     # core: 1000 trials, the 8-point contract
make test/test_swap3  && ./test/test_swap3    # the Alice↔Bob atomic swap, narrated
make test/test_amhl3  && ./test/test_amhl3    # multi-hop: wormhole-resistance + norm growth
make test/test_serde3 && ./test/test_serde3   # tamper test: all 4672 byte-flips rejected
make test/test_kat3   && ./test/test_kat3     # reproducible fingerprint match
make test/bench_las3      && ./test/bench_las3      # speeds + rejection rate
make test/bench_compare3  && ./test/bench_compare3  # LAS vs basic Dilithium-3
make test/bench_classical && ./test/bench_classical # LAS vs classical ECDSA adaptor
make test/bench_app3      && ./test/bench_app3      # application payload sizes

# on-chain gas (needs Foundry):
make test/export_packed && ./test/export_packed ../evm/test/las_sig.bin
cd ../evm && forge test --gas-report
```
*(The classical baseline needs a one-time clone — see `README_LAS.md`.)*
Good things to film: `test_amhl3` scrolling its wormhole/norm-growth asserts; the
`test_serde3` "all 4672 byte-flips rejected" line; the `bench_classical` 2×2; the
`forge --gas-report` table.

---

## 10. Glossary (flip here any time)

- **Signature** — maths proving the key-owner approved a message.
- **Post-quantum (PQ)** — secure even against a quantum computer.
- **Shor's algorithm** — the quantum attack that breaks today's signatures.
- **Lattice / Module-SIS, Module-LWE** — the hard maths puzzle LAS's safety rests
  on; "recover the short secret from the public key," believed quantum-hard.
- **Dilithium / ML-DSA** — the NIST-standard basic PQ signature we build on.
- **Adaptor signature** — a signature whose completion needs *and reveals* a secret.
- **Statement `Y` / witness `y`** — the public "padlock" and its secret key.
- **PreSign / PreVerify / Adapt / Ext** — make / check / complete-with-secret /
  recover-secret. The four functions that turn Dilithium into LAS.
- **Pre-signature `σ̂`** — the locked, not-yet-valid signature.
- **Fiat–Shamir with aborts** — the commit→challenge→respond→maybe-retry recipe.
- **Rejection sampling** — the "throw away and retry" step that keeps the secret hidden.
- **Norm budget (`γ−κ`, `γ−κ−1`, `γ−κ−K`)** — the size limits that must line up so a
  completed signature still verifies; the project's key correctness knob.
- **Atomic swap** — trustless coin trade across two chains; all-or-nothing.
- **HTLC** — the classic "hash-and-timeout lock" that adaptor signatures replace.
- **AMHL (multi-hop lock)** — paying through a chain of people, each hop its own lock.
- **Wormhole attack** — a multi-hop theft that distinct per-hop locks prevent.
- **Serialisation / packing** — turning objects into the byte string sent on the wire.
- **KAT (known-answer test)** — fixed-input test pinned to an expected fingerprint.
- **Gas** — what an Ethereum operation costs; you pay per byte and per computation.
- **Foundry / anvil** — the toolkit + local Ethereum we measure gas on.
- **poqeth** — prior work that put *basic* PQ signatures on Ethereum (we extend the idea to an exotic one).

---

*Where to go deeper:* `docs/LAS.md` (full design + all benchmark tables),
`docs/THEORY_IMPL_BRIDGE.md` (paper equation → exact C function),
`docs/FUNCTION_MAP.md` (what was reused vs added), `docs/STATUS.md` (the
done/tested checklist), `report/REPORT_DRAFT.md` (the dissertation draft).
