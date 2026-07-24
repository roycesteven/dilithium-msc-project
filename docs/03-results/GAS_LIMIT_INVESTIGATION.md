# Why we measured the on-chain gas cost — the reasoning, in plain English

*This document explains the **reasoning** behind one specific piece of work: turning
the claim "verifying a LAS signature on a blockchain costs too much gas" from a
guess into a measured, defensible number. It is written for someone new to the
topic. Every term is explained the first time it appears; there is a mini-glossary
at the end. The companion deep-dive (with the exact tables) is `docs/LAS.md §8.4.1`;
the code is in `evm/src/LASVerifyCost.sol` and `evm/test/LASVerifyCost.t.sol`.*

> How to read this: Section 0 is a 60-second recap of the whole project so this page
> stands on its own. Sections 1–3 are the problem and the key ideas (no maths).
> Section 4 is the experiment and why it is trustworthy. Section 5 is the result and
> the honest correction. Section 6 is why this matters for the report's marks.

---

## 0. The whole project in 60 seconds (so this page makes sense alone)

- Coins on Bitcoin/Ethereum are protected by **digital signatures**. Today's
  signatures will be breakable by a future **quantum computer**. The fix is
  **post-quantum** signatures, built on lattice maths quantum computers can't crack.
- The world already has *basic* post-quantum signatures (the NIST standard,
  **Dilithium**). This project builds a *fancier* one — an **adaptor signature**
  called **LAS** — which is the ingredient that powers **atomic swaps** (safely
  trading a coin on one chain for a coin on another, with no trusted middleman).
- We implemented LAS in C, demonstrated the atomic swap, benchmarked it, and then
  asked: **what happens if we try to use it on a real blockchain (Ethereum)?**

That last question is where this document lives.

---

## 1. What "gas" and "the block gas limit" mean

Ethereum is a shared computer that thousands of machines run in lockstep. Because
running code on everyone's machine isn't free, every tiny operation has a price
measured in a unit called **gas**. Adding two numbers costs a few gas; storing data
costs a lot more; hashing costs gas per byte; and so on. When you send a transaction
you pay for the total gas it burns.

Crucially, Ethereum bundles transactions into **blocks**, and **each block has a
hard ceiling on total gas** — the **block gas limit**, currently **60 million** (raised by
validator vote in late 2025; for years it was ~30M). There is also a **second, tighter
ceiling that a single transaction cannot exceed**: **EIP-7825** caps any one transaction at
**16,777,216 gas (2²⁴)**, regardless of how much room the block has. The rule is simple and absolute:

> If a single operation would cost **more gas than the per-transaction cap** (or than
> fits in one block), it **physically cannot run on-chain as one transaction.** No amount
> of money fixes it — there is nowhere to put it.

So "exceeds the gas limit" is not a vague complaint about cost. It is a specific,
*falsifiable* claim that means **"this is impossible on Ethereum as a single transaction,
full stop."** That strength is exactly why it needs evidence. (As §5 shows with a *measured*
verifier, the real binding ceiling for LAS turns out to be the **per-transaction cap**, not
the block limit.)

---

## 2. The claim we inherited — and why it smelled wrong

Our earlier write-up said, in effect:

> *"Verifying a LAS signature on-chain would dwarf / exceed the block gas limit, so
> it's infeasible."*

It read confidently, but **there was no number behind it.** My supervisor's feedback
item 8 said exactly this: *"'exceeds block gas limit' → there must be an experiment
or a calculation!"* In other words: **don't assert it, prove it.**

I had a specific reason to be suspicious that the claim might be *wrong*. Ethereum's
virtual machine (the **EVM**) has a **built-in instruction for modular
multiplication** — `mulmod` — and it costs only **8 gas**. "Modular multiplication"
(multiply two numbers, then take the remainder after dividing by a fixed number) is
*the* dominant operation inside lattice signature verification. If the single most
common heavy operation is nearly free, then maybe the whole thing isn't as
astronomically expensive as "exceeds the block gas limit" implies. The honest way to
settle it is to **measure**, not to argue.

---

## 3. What "verifying a LAS signature" actually involves

You don't need the maths, just the **shapes of the work**, because cost depends on
*how many* operations there are, not on what the numbers are. Verification does two
things:

1. **A big polynomial calculation.** It recomputes a value `w' = A·ẑ − c·t`. Here
   `A`, `ẑ`, `t` are vectors/matrices of **polynomials** (lists of 256 numbers each).
   Multiplying polynomials fast uses a standard trick called the **NTT** (Number
   Theoretic Transform — think "the integer cousin of the FFT"). By reading our C
   code I counted the *exact* workload of one verification at the D3 parameter set
   (n=6, ℓ=5): **12 forward NTTs + 12 inverse NTTs + 36 "pointwise" multiplications**,
   plus **54 bookkeeping passes** (add/subtract/reduce) over the 256-number arrays.

2. **One hash.** It feeds the result into **SHAKE256** (a hash function from the same
   family as Ethereum's built-in `keccak256`) and checks the output matches the
   signature. This is roughly **64 "rounds" of the core hashing permutation**.

That's the whole job: a fixed, countable list of multiplications, additions, and
hashing rounds. Countable is the key word — it means we can price it.

---

## 4. The experiment, and why a "fake" verifier gives a *real* number

Here is the clever part, and the bit worth understanding.

I wrote a small Ethereum program (`LASVerifyCost.sol`) that performs the **exact
same number and type of operations** as a real LAS verifier — the same 12 forward
NTTs, 12 inverse NTTs, 36 pointwise multiplications, 54 array passes — but it
does **not** compute the cryptographically correct answer. It multiplies real
256-number arrays in memory using `mulmod`, but with stand-in values. Because it
reproduces the operation *count* rather than the exact values and memory-access
pattern, the figure it yields is an **arithmetic lower-bound estimate**, not the exact
gas of a complete, numerically-correct verifier.

Why is measuring a "fake" verifier still a *real* cost measurement? Because of one
fact about the EVM:

> **An EVM instruction costs the same gas no matter what numbers it operates on.**
> `mulmod` is 8 gas whether you multiply 2×3 or two giant numbers. Reading from
> memory is 3 gas regardless of the value.

So gas depends only on **the structure of the work** (how many of each instruction),
not on the **values**. A program with the identical instruction mix has the
identical gas cost. We're measuring **price**, and price is value-blind.

> **UPDATE (2026-07-23): the numerically-correct on-chain verifier was subsequently
> built** — `evm/src/LASVerifier.sol` (`library LASVerify`), assembled from the vendored
> ZKNox ETHDILITHIUM primitives (SHAKE256, NTT, SampleInBall) and validated end-to-end
> against the C reference: it *accepts the real adapted signature and rejects tampered
> bytes* (`evm/test/LASVerifier.t.sol`, 6/6). So the figure below is **no longer an
> estimate** — see the measured row in §5. The real verifier costs **more** than the
> op-budget probe, because the probe reproduced only the polynomial-arithmetic op *count*
> and omitted what a correct verifier must also pay for: the real Solidity SHAKE256
> permutations, the bit-unpacking of `z`, canonical packing, and ABI/memory overhead.

To make sure the optimiser couldn't secretly delete my loops as "useless work," I
fed the arrays from a runtime input and returned a value that depends on the result.
And to make the headline number trustworthy, I also **measured each piece in
isolation** (one NTT, one inverse NTT, one pointwise multiply) and checked that
adding them back up reproduces the single big measurement. It does — so the number
isn't an accident of how I wrote one function.

I ran it on a local Ethereum (Foundry's built-in EVM). EVM gas is **deterministic** —
the same code always costs the same gas on any machine — so unlike the timing
benchmarks, these numbers don't wobble run-to-run.

---

## 5. The result — and the honest correction

| What | Gas | How we got it |
|---|---:|---|
| **REAL: one full on-chain LAS verified settlement (`claimLASVerified`)** | **56,538,682** | **measured on the EVM** — the complete verifier `LASVerify.verify` |
| For comparison: a complete classical claim (`claimClassical`, incl. `ecrecover`) | 75,751 | measured |
| For comparison: **EIP-7825 per-transaction gas cap** | **16,777,216** | Ethereum (2²⁴) |
| For comparison: the block gas limit | 60M | Ethereum (validator-set, late 2025) |
| *historical op-budget estimate (superseded by the measured row above):* | | |
| — the polynomial calculation (`A·ẑ − c·t`) | 13.93 million | measured (op-count reproduction) |
| — the SHAKE256 hash (~92 permutations) | ~2.76 million | calculated (~30k gas each, a model) |
| — one native LAS verification (estimate) | ≈ 16.7 million | measured arithmetic + calculated hash |

Read those rows together:

- **The real, measured cost is ≈ 56.5 million gas** — the whole `claimLASVerified`
  transaction, which runs the *complete* `base_verify` (Algorithm 1): the
  simplified-Dilithium ordinary-signature check the adaptor settles into. This is the
  honest headline; the ≈16.7M figure was an op-budget *lower-bound estimate* and is now
  superseded. The real number is larger because it also pays for the real Solidity
  SHAKE256, the bit-unpacking of `z`, canonical packing, and ABI/memory overhead.
- **It is ≈ 746× the entire classical claim.** The 75,751-gas baseline is the *whole*
  classical `claimClassical` transaction — settlement **plus** its ECDSA `ecrecover`
  verification (which runs in a native precompile, not in EVM bytecode). LAS runs the
  entire verifier in Solidity bytecode, hence the gulf.
- **It exceeds the per-transaction gas cap, not the block.** Under EIP-7825, a
  single Ethereum transaction may use at most **16,777,216 gas (2²⁴)**. At ≈56.5M,
  `claimLASVerified` is **≈3.4× that cap**, so it **cannot execute as one mainnet
  transaction.** Its raw demand is below the 60-million block gas limit — so the binding
  ceiling is the **per-transaction cap**, not the block limit.

So the original claim — *"exceeds the block gas limit / impossible"* — was an
**overstatement, and I retracted it.** The measured, precise version:

> The evaluated native **Solidity** LAS verifier (D3) is **prohibitively expensive**
> (≈746× a complete classical claim, and a real engineering burden — a hand-built
> SHAKE256 and NTT in EVM code) and **exceeds EIP-7825's per-transaction gas cap**, so it
> is not executable as a single mainnet transaction — though it does *not* exceed the
> block gas limit. The barrier is **economics, a per-transaction budget, and missing
> built-in support**, *not* an impossibility. This is a result for *this* D3 LAS
> instance in Solidity, not a claim about all post-quantum verification.

That distinction matters: "impossible" and "possible but absurdly expensive" lead to
different conclusions about the future. Our recommendation (use a dedicated
**precompile** — a built-in accelerator — or a **zero-knowledge proof** of
verification) is the same either way, but now it rests on a measured *economic* case
instead of a wrong *impossibility* case.

I also wrote down the honest caveats so no one over-trusts the figure: it's a
**lower bound** (a real verifier also has to unpack the signature and rebuild the
matrix `A`, which add gas but not a whole new order of magnitude), and the hashing
cost uses a published per-round figure rather than a same-machine measurement.

---

## 6. Why I did it this way (the reasoning, summarised)

- **The supervisor asked for evidence, not assertion.** A claim as strong as
  "impossible on Ethereum" must be backed by a number. That's good science and it's
  literally how the "Evaluation" marks are awarded.
- **I suspected the claim was false**, because the EVM's cheap `mulmod` undercuts the
  "too expensive to even fit" intuition. Following that suspicion to a measurement —
  and being willing to *correct our own earlier claim* — is exactly the kind of
  honest, critical analysis examiners reward more than confident hand-waving.
- **A cost probe is the right tool** because it gives a real gas figure without the
  weeks of work a correct on-chain verifier would need — and the EVM's value-blind
  pricing makes the probe rigorous, not a shortcut.
- **I cross-checked and caveated** so the number is defensible: independent
  per-operation measurements that reconcile with the total, and an explicit statement
  of what the figure leaves out.

**Files this produced / changed**

- New experiment: `evm/src/LASVerifyCost.sol`, `evm/test/LASVerifyCost.t.sol`
  (run: `cd evm && forge test --match-contract LASVerifyCost -vv`).
- Deep-dive write-up with the full tables: `docs/LAS.md §8.4.1`.
- Synced everywhere the old claim lived: `docs/STATUS.md`, `report/REPORT_DRAFT.md`
  (abstract, §4.3, threats-to-validity, conclusion), `evm/README.md`, and the
  comment in `evm/src/AdaptorSwap.sol`.

---

## Mini-glossary

- **Gas** — Ethereum's unit for "how much work a computation is." You pay for it.
- **Block gas limit** — the maximum total gas one block can contain (~30 million).
  Anything bigger can't run on-chain at all.
- **EVM** — the Ethereum Virtual Machine, the shared computer that runs the code.
- **`mulmod` / `addmod`** — built-in EVM instructions for modular multiply/add,
  8 gas each, *regardless of the numbers involved*.
- **Signature verification** — recomputing a value from a signature and checking it
  matches; how the chain confirms a payment was really approved.
- **NTT (Number Theoretic Transform)** — a fast way to multiply polynomials; the
  integer relative of the FFT. The bulk of the verification's arithmetic.
- **Pointwise multiplication** — multiplying two lists element-by-element; cheap,
  used inside the NTT-based polynomial product.
- **SHAKE256 / Keccak** — the hash function LAS uses; same family as Ethereum's
  built-in `keccak256`.
- **Precompile** — a built-in, hand-optimised operation the EVM offers natively
  (e.g. `ecrecover` for normal signatures). A LAS precompile is the obvious fix.
- **Cost probe** — a program written to have the *same gas cost* as the real thing,
  so you can measure price without building the full correct thing.
- **Deterministic** — always gives the exact same result; EVM gas is deterministic,
  so these numbers don't change machine-to-machine.
