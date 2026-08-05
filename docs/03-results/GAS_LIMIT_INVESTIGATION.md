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

## 7. Bringing it inside one transaction (2026-08-05)

Section 5's result is a **negative** one: ≈56.5M gas, ≈3.4× the per-transaction cap,
therefore not mineable as one transaction. That is an honest measurement of **one
implementation**, and it is worth being precise about what it does and does not show.
It shows that *this* Solidity verifier does not fit. It does **not** show that the
predicate itself is too big for the cap — nobody had asked how much of the 56.5M was
LAS and how much was the way LAS had been expressed in Solidity.

So we asked. `src/LASVerifierOpt.sol` (+ `src/LASShake.sol`) computes the **identical
predicate over the identical scheme** — same q, same κ, same bounds, same
`SHAKE256(pack(t)‖pack(w')‖M)` preimage, same FIPS 204 SampleInBall — restructured
along four lines:

1. **Half the transforms.** The baseline runs 12 forward + 12 inverse NTTs. `t` is fixed
   public-key material, so it is registered once in NTT domain exactly as `A'` already
   was, removing 6 forward NTTs. And the inverse NTT is a **linear map**, so
   `invNTT(Â·ẑ) − invNTT(ĉ·t̂) = invNTT(Â·ẑ − ĉ·t̂)`: the two products are combined in
   NTT domain and inverted once per row, removing 6 inverse NTTs. 24 → 12, same result.
2. **No matrix in memory.** `A'` and `t` used to arrive as `uint256[][]` — one 32-byte
   word per 23-bit coefficient, **304,292 bytes of calldata** decoded into ~300 KB of
   memory, which also drives the EVM's *quadratic* memory charge. They now arrive packed
   (4 bytes per coefficient, 8 per word) and are read straight from **calldata** by the
   multiply loops.
3. **A sponge that does not churn.** The vendored SHAKE256 keeps a 200-**byte** buffer in
   200 memory **words**, absorbs one bounds-checked byte at a time, and re-allocates that
   buffer plus three 24-entry constant tables on every one of the ~91 permutations a
   verification needs. The replacement keeps 25 lanes at fixed offsets, writes the round
   constants once per context, and absorbs 8 input bytes per `mload`.
4. **Word-wise codecs.** The z-decode, the norm gate and `pack(w')` were per-byte,
   bounds-checked Solidity loops over ~12 KB and 2,816 coefficients. The norm gate also
   moved onto the *raw 19-bit field*: since `z = Z_OFFSET − field`, the condition
   `field ≤ 2·BOUND` **is** `‖z‖∞ ≤ BOUND`, with no canonicalisation first.

**A faster verifier that is not the same verifier is worth nothing**, so it is pinned
three ways in `evm/test/LASVerifierOpt.t.sol`: the packed `w'` must equal the C golden
`w_prime.bin` byte-for-byte; the ACCEPT bit on the golden adapted signature and REJECT on
a tampered z byte / tampered `c_tilde` / wrong message must hold; and it must agree with
the vendored `LASVerify` on all four. The rewritten sponge is separately pinned to the
vendored SHAKE256 in `evm/test/LASShakeEquiv.t.sol` at ten input lengths chosen around
the 136-byte rate.

**The gate, at two levels.** A claim this specific should not rest on our own arithmetic,
so it is checked twice, by instruments that can fail independently.

*Level 1 — the model.* `evm/test/LASGasBreakdown.t.sol::test_optimised_fits_in_one_transaction`
fails unless the modelled transaction charge is strictly under 16,777,216, and a companion
test asserts the **baseline still exceeds** it, so the comparison cannot go stale in either
direction. The charge is **EIP-7623**, not EIP-2028:

```text
tokens = zero_bytes·1 + nonzero_bytes·4
charge = 21000 + max( 4·tokens + execution , 10·tokens )
```

The first branch reproduces the familiar 16-gas-per-non-zero-byte schedule; the second is
a **floor** that binds when a transaction carries a lot of calldata relative to how much
computing it does — the exact shape of a lattice-signature claim. Modelling only the first
branch understates the charge for any calldata-heavy, compute-light transaction, so
`LASTxGas` (in `evm/src/LASRegister.sol`) implements the real `max` and every reported
total says **which branch bound**. (For the D3 claim the crossover sits near 1M gas of
execution.) `TwoLegSwapGas.t.sol`'s helper models only the standard branch; its published
rows are all execution-dominated, so none of them moves — but the gate uses the correct
rule regardless.

*Level 2 — a real client.* `./scripts/run_onchain_one_tx.sh` settles the swap on
**anvil at a pinned `osaka` hardfork with `--enable-tx-gas-limit`**, so EIP-7825 is
enforced by the node rather than by us, and the evidence is the **receipt**:

- fund and claim are **separate mined transactions**, so the claim's escrow reads are
  genuinely **cold** — funding inside the measured call would pre-warm the slots it reads;
- the claim is sent by `cast send --gas-limit 16777216` — an explicit cap, not an
  estimate — and the limit **actually on the mined transaction** is read back and asserted,
  rather than assumed from the flag;
- before trusting any of it, a **differential control** sends the same trivial transfer at
  the cap and at cap+1. Only *accepted at the cap, refused one gas over it* attributes the
  refusal to EIP-7825; a bare non-zero exit code would equally mean a bad nonce or a dead
  RPC. Accepted at both ⇒ the cap is not enforced and the run aborts; refused at both ⇒
  inconclusive, and explicitly **not** read as "enforced";
- the calldata the node received is compared **byte for byte** against what was built from
  the freshly exported vectors (equal length is not equal bytes), and that expected
  calldata is retained in the evidence directory so the check is auditable afterwards;
- the escrow payout is asserted **exactly**, so a partial or misdirected transfer fails;
- the vectors are re-exported by `ref/test/export_verify_vector` *for the run*, and their
  digests recorded, so the receipt cannot later be attributed to a different signature;
- **a client rejection is preserved as a result**: the node's own error text, its log, the
  control outcome and an enumerated list of every unmet post-condition are written to
  `evidence/onchain_onetx/<ts>/verdict.txt` and the script exits non-zero.

### The result (measured 2026-08-05)

**A real client mined it.** `evidence/onchain_onetx/latest/` — anvil at `osaka` with
`--enable-tx-gas-limit`, claim sent at an explicit `--gas-limit 16777216`:

| | |
|---|---:|
| `gasUsed`, from the client's receipt | **16,413,275** |
| EIP-7825 per-transaction cap | 16,777,216 |
| headroom | **363,941 gas (2.2%)** |
| beneficiary paid | exactly the escrowed 1 ETH |
| calldata received vs expected | byte-identical, 50,148 B |

The cap-enforcement control passed on the way in: the node accepted a trivial transfer at
exactly the cap and refused it one gas over with `tx.gas_limit > env.cfg.tx_gas_limit_cap`,
so the acceptance means what it claims. The untraced in-test model agrees with the receipt
to **0.2%** (16,443,126 modelled swap-level total vs 16,413,275 charged), which is the
cross-check that makes the cheap `forge test` gate trustworthy between node runs.

The baseline remains ~3.2× the cap. Stage attribution of the optimised path (execution
15,654,854) — from `evidence/onchain/latest/gas_report.log`, pass 1:

| stage | gas | share |
|---|---:|---:|
| **SHAKE256 over the 12,320-byte preimage** | **10,572,481** | **68%** |
| w′ = pointwise + 6 inverse NTTs + pack | 3,237,053 | 21% |
| 6 forward NTTs (z_bot, c) | 1,098,741 | 7% |
| decode z + norm gate | 577,760 | 4% |
| SampleInBall | 154,229 | 1% |
| preimage assembly | 8,703 | <1% |

Arithmetic fell from 20,730,606 to 5,076,486 (≈4×) and the hash from 26,335,785 to
10,572,481 (≈2.5×). **The sponge is now the whole story**, at ≈116,181 gas per Keccak-f
permutation across 91 permutations.

**⚠️ Two caveats that must travel with this result.**

1. **It fits, but only just — and the margin is a message-length budget.** The measured
   headroom is 363,941 gas. *Derived* from that and the measured per-permutation cost
   (10,572,481 ÷ 91 = 116,181 gas): the spare gas is worth ≈3.1 Keccak-f permutations,
   i.e. **≈400 bytes of additional signed message** before the cap is reached, since the
   preimage is `6144 + 6144 + |M|` and each additional 136 bytes costs one permutation.
   **That 400-byte figure is a derivation, not a measurement** — no run at a longer message
   has been made. What *is* measured is the 32-byte case, which is the realistic one: in the
   Bitcoin/UTXO setting the signed value is a 32-byte sighash.

   **D2 and D5 are NOT evaluated.** `LASVerifyOpt`'s parameters (`N_LAS`, `ELL`,
   `CTILDE_BYTES`, `Z_BITS`, …) are compile-time constants fixed to D3, so the library
   cannot be run at another set without being re-parameterised first. Nothing here supports
   a claim that it fails, or succeeds, at D2 or D5 — that is an open question, not a result.
   *(An earlier draft of this document asserted it "breaks at D5". That was written from
   plausibility, never measured or calculated, and is retracted.)*

   Do not state "on-chain LAS verification fits in one transaction" without the parameter
   set, the message length and the EVM revision attached.
2. **`--gas-report` inflates in-test gas measurement.** Foundry's inspector is metered
   inside the measured call frame, adding ≈688k gas to `gasleft()` deltas *and* to
   `vm.lastCallGas()` — **more than the entire headroom**. A cap assertion measured under
   the flag fails for a reporting reason while a real client mines the same transaction.
   `run_onchain_gas.sh` therefore runs the gates in a pass **without** `--gas-report` and
   the reporting table in a pass with the gate contracts **excluded**, capturing each
   pass's exit status separately so a failing gate cannot hide behind a green table.
   Foundry's own `--gas-report` table is unaffected; only in-test measurement is.

> **Numbers.** Every figure for this section comes from a real run — `run_onchain_gas.sh`
> → `evidence/onchain/latest/gas_report.log` (per-stage attribution plus a
> `claimLASVerifiedOpt` row directly comparable with `claimLASVerified` in the same
> `--gas-report` table), and `run_onchain_one_tx.sh` →
> `evidence/onchain_onetx/latest/{verdict.txt,claim_receipt.json}`. **Do not quote a
> figure for this section from prose — read it from the log.** Until those runs exist, the
> honest statement is "designed and gated to fit", not "fits".

**What this does and does not change.** If the gate passes, the claim becomes: *native
on-chain LAS verification at the D3 set is executable as a single Ethereum transaction,
and the earlier 56.5M figure measured the expression rather than the scheme.* It does
**not** make on-chain verification cheap — it stays orders of magnitude above a classical
`ecrecover` claim, so the Meeting-7 reasoning for choosing Bitcoin/UTXO (fees, off-chain
heavy work, adaptor signatures used there in practice) is untouched. And it says nothing
about the *other* parameter sets, or about anything other than this predicate.

---

## 8. The standardisation route: EIP-8051

Section 6 recommended "a precompile or a zero-knowledge proof". That recommendation now
has a specific, citable object behind it.

**[EIP-8051](https://eips.ethereum.org/EIPS/eip-8051), "Precompile for ML-DSA signature
verification"** (Renaud Dubois, Simon Masson, Oct 2025) specifies precompiles at
`0x12` (`VERIFY_MLDSA`) and `0x13` (`VERIFY_MLDSA_ETH`) at **4,500 gas**. Two details
matter to this project specifically:

- Its authors are the **ZKNox** authors — the same people whose ETHDILITHIUM primitives
  `evm/lib/zknox/` vendors. The EIP is, in effect, the standardisation of the library
  this project already builds on.
- Its design choices independently corroborate two of §7's: the `0x13` variant replaces
  SHAKE256 with a Keccak-based PRNG, and its 20,512-byte public-key input carries the
  matrix expanded with `t1` **already in NTT domain** — precisely the "register it once,
  never re-transform it" move in §7 point 1.

Combined with this project's **ML-DSA adaptor result** (`docs/03-results/MLDSA_HINT_EXPERIMENT.md`)
— LAS built on FIPS 204 as specified, with unmodified `crypto_sign_verify` accepting the
*adapted* signature — a precompile-based settlement is a coherent design: verify the
adapted signature in the precompile, settle in the same call, keep `Ext` off-chain.

**Four constraints that must travel with that claim**, or it overstates:

1. **It is not deployed.** EIP-8051 is **Draft**, and is listed under **"Declined for
   Inclusion"** in the Glamsterdam hardfork meta ([EIP-7773](https://eips.ethereum.org/EIPS/eip-7773)).
   The Ethereum Foundation's post-quantum roadmap ([pq.ethereum.org](https://pq.ethereum.org/))
   places PQ signature-verification precompiles in 2027–2028 forks. No ML-DSA precompile
   is callable on Ethereum today, so **any figure for this route is a conditional model
   computed from the EIP's own constant, never a measurement** — and must be labelled so.
2. **NIST level II only.** The EIP explicitly covers **ML-DSA-44**. This project's
   headline set is ML-DSA-65-aligned (D3). Taking this route means dropping a security
   level, which the report must state rather than quietly compare across.
3. **`0x13` is not FIPS 204.** It replaces SHAKE256. So "accepted by the *stock* FIPS-204
   verifier" and "verified by the EVM-optimised variant" cannot both be claimed; only
   `0x12` is the FIPS-faithful one.
4. **The adaptor security caveat still applies.** The ML-DSA adaptor construction is a
   *functional* demonstration; the security of committing to `HighBits(w+Y)` is not
   analysed (out of scope — no security proofs in this project).

The two routes are complementary, and the honest framing is that they answer different
questions. §7 answers *"can it be done on the EVM as it exists?"* with a measurement.
§8 answers *"what is the path to it being cheap?"* with a citation and a stated timeline.

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
