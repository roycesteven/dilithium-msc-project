# Proof amortisation — does the role-A proof get cheaper across swaps?

**Status: run 2026-08-04 for BOTH provers**, k = 1, 2, 4, 8 with 5 repetitions each.
Evidence: `evidence/amortise/latest/` (Groth16) and `evidence/lazer_amortise/latest/` (LaZer).

**Headline — batching fails for both provers, for opposite reasons.** That is the result,
and it closes the question rather than deferring it.

| | proof/swap at k=8 | compute/swap at k=8 | why it fails |
|---|---|---|---|
| **Groth16** (config 2) | 128 → 16 B, a perfect `1/k` | flat (+32% proving) | the `1/k` is perfect but lands on a cost that **was already negligible** — 128 B beside a 4416 B statement and 6736 B signature |
| **LaZer** (config 3) | 30723 → 17645 B, **0.57×** | **3.33× worse** | the saving lands on a cost that **does** matter, but is paid for in the one that matters **more** |

Configuration 2 spends ~646 ms in `Prove(π)` — 97% of the whole swap and 47× the next phase
— so shrinking its 128 B proof changes nothing. Configuration 3's proof genuinely is its
dominant communication object, and batching does cut it by 43%; but its role-A proof is
already **98.6% of end-to-end time**, and batching buys those bytes by making per-swap
proving and verification **3.3× worse**. Trading 43% of the bytes for 3.3× the time makes
the dominant cost worse to improve a subordinate one.

---

## 1. Why this experiment exists

The Stage-2 swap study found the **role-A proof of knowledge to be the dominant end-to-end
cost by an order of magnitude** — the single largest item in both time and communication,
larger than the signature work it accompanies. The report's future work therefore asks
whether it can be reduced or amortised, and names amortisation specifically because *a
party opens each swap with a fresh statement*, so today every swap carries its own proof.

A party running many swaps does not have to. It can prove a **batch** of statements at
once. Whether that helps is not obvious, because the three costs behave differently, and
the point of the experiment is to measure where the curves cross rather than to argue about
them.

## 2. What it runs

`rust/las-swap/src/bin/bench_amortise.rs`. For `k = 1, 2, 4, 8` it produces **one Groth16
proof covering `k` instances** of the same relation configuration 2 already proves
(`∃r : A r = t ∧ ‖r‖∞ ≤ 1`), and reports setup, proving, verification and proof size as
totals and per swap.

The statements are the **same statements the swap benchmark proves**: the same master seed
and the same `subseed` tags, so a proving-cost comparison is attributable to the batch and
not to a different instance.

## 3. The batched circuit, and why it proves the same thing

`BatchedRelationCircuit` (`rust/las-swap/src/groth16_circuit.rs`) holds `k` instances and
emits each through **`emit_instance`, the same function the single-instance
`RelationCircuit` uses**. The relation was factored out for exactly this reason: there is
one implementation of the claim, so a batch cannot silently prove something weaker than `k`
single proofs do — including the load-bearing range checks on the quotient, without which a
row equation would be satisfiable for any `r`.

That is enforced in the measurement as well as in the structure. Per batch size the
benchmark corrupts the **last** instance's public input and requires rejection; if a batch
accepted it, the batch would not bind all `k` and any speed-up would be an artefact.

## 4. Measured result

5 repetitions per batch size, mean ± SD, untimed warm-up discarded. Public input is 1536
field elements per instance.

**Totals — one proof covering `k` statements**

| k | setup (ms) | prove (ms) | verify (µs) | proof (B) |
|---|---|---|---|---|
| 1 | 666.8 ± 32.2 | 494.1 ± 32.0 | 12720.7 ± 808.8 | 128 |
| 2 | 1326.3 ± 50.3 | 1026.5 ± 105.7 | 23847.8 ± 539.5 | 128 |
| 4 | 2766.3 ± 144.8 | 2178.4 ± 35.1 | 47097.9 ± 2560.6 | 128 |
| 8 | 5687.3 ± 429.5 | 5227.1 ± 1022.1 | 94879.7 ± 5204.6 | 128 |

**Amortised per swap (total / k)**

| k | prove (ms) | verify (µs) | proof (B) | proof vs k=1 |
|---|---|---|---|---|
| 1 | 494.1 | 12720.7 | 128.0 | 1.00× |
| 2 | 513.2 | 11923.9 | 64.0 | 0.50× |
| 4 | 544.6 | 11774.5 | 32.0 | 0.25× |
| 8 | 653.4 | 11860.0 | 16.0 | 0.12× |

**Proof size amortises perfectly and is the only column that does.** 128 B at every `k` —
three BN254 group elements regardless of what the circuit proves — so the per-swap share is
exactly `1/k`, down to 16 B at k=8.

**Proving does not amortise, and in this run it got worse:** per-swap 494 → 653 ms, about
**+32% at k=8**. Across the three runs captured today the per-swap proving figure was
variously flat and rising, so the defensible claim is *flat to slightly worse* — never
better. The constraint system grows with the batch, so the work is paid per instance either
way; batching moves it rather than removing it.

**Verification does not amortise either:** per-swap 12.7 → 11.9 ms, flat within noise, as
expected when the public input grows linearly with `k`.

**Setup grows with the batch too** (667 ms → 5687 ms), and it is per circuit, so a party
committing to a batch size pays a new trusted setup for it.

---

The predicted shape, for reference — the measurement confirmed all three rows:

| cost | why it does or does not amortise |
|---|---|
| **proof size** | **amortises perfectly.** A Groth16 proof is three group elements whatever the circuit proves, so `k` instances cost the same bytes as one and the per-swap share falls as `1/k`. |
| **proving** | **does not amortise.** The constraint system grows with the batch, so the work is paid per instance either way — batching moves it, it does not remove it. |
| **verification** | **does not amortise.** The public input is `ROWS` field elements per instance, so a verifier of a `k`-batch reads `k` times as much. |

The reading that matters for the project is the one in the headline: for **Groth16** the
`1/k` lands on the 128 B proof, which was never the bottleneck, while the ~646 ms of
proving that *is* the bottleneck stays flat. Batching is therefore not a route to a cheaper
post-quantum swap here — it is a route to a cheaper *proof transmission*, which
configuration 2 did not need.

Where it would matter is a proof system with a large proof and cheap generation. That is
LaZer's profile, and measuring it is the obvious next step this experiment does **not**
take (see §7).

## 4b. Measured result — LaZer (configuration 3)

`ref/test/bench_lazer_amortise.c`, one LaZer proof over `k` instances laid out
**block-diagonally** by `ref/relation_zk_batch.c`: instance `i` occupies rows `[6i, 6i+6)`
and columns `[23i, 23i+23)`, each block being exactly the `[I | A' | −I | −A' | 0]` matrix
the deployed `relation_zk.c` builds, every off-block entry zero. So the batched statement is
the conjunction of `k` copies of the deployed one, and **k=1 dispatches to the committed
`las_pi_params`** — the baseline row is the shipped prover, not a lookalike.

Each `k` needs its own generated parameter set (`ref/relation_zk_params_k{2,4,8}.h`,
committed; regenerate with `scripts/gen_lazer_batch_params.sh`, which needs SageMath).

**Totals**

| k | prove (ms) | verify (ms) | proof (B) |
|---|---|---|---|
| 1 | 158.8 ± 73.3 | 75.2 ± 4.2 | 30723 |
| 2 | 276.2 ± 22.3 | 218.6 ± 8.4 | 46751 |
| 4 | 931.8 ± 92.2 | 692.7 ± 27.6 | 76191 |
| 8 | 3560.5 ± 354.5 | 2676.4 ± 255.1 | 141157 |

**Amortised per swap**

| k | prove (ms) | verify (ms) | proof (B) | proof vs k=1 |
|---|---|---|---|---|
| 1 | 158.8 | 75.2 | 30723.0 | 1.00× |
| 2 | 138.1 | 109.3 | 23375.5 | 0.76× |
| 4 | 233.0 | 173.2 | 19047.8 | 0.62× |
| 8 | 445.1 | 334.6 | 17644.6 | 0.57× |

**The proof does shrink, and it is the first real saving in this whole direction.** LaZer's
proof is *not* constant in `k` — it grows — but **sublinearly**, so per-swap size falls to
**0.57× at k=8, a 43% saving** on configuration 3's dominant communication object. The
measured sizes track the codegen's own predictions (31.3 / 47.7 / 78.1 / 144.7 KiB) closely.

**But compute goes the other way, and further.** Per-swap prove+verify is **3.33× worse** at
k=8 (prove 159 → 445 ms, verify 75 → 335 ms). LaZer's work grows **superlinearly** in the
batch, so each extra instance costs more than the last — the opposite of Groth16, whose
per-swap compute stayed roughly flat.

**For configuration 3 that settles it.** Its role-A proof is already 98.6% of end-to-end
time, so the binding constraint is compute, not communication — and batching buys
communication by spending compute. This is a genuine trade-off, not a free win, and in this
protocol it is the wrong way round.

## 5. The protocol cost that is not in the numbers

Batching is not a free drop-in, and the write-up must say so:

- a batch must be proved **before any of its statements is used**, so a party has to know
  its next `k` swaps in advance;
- every counterparty **verifies the whole batch** to use one statement of it, and receives
  public input for `k − 1` statements that are not theirs;
- the statements in a batch become **linked** — anyone seeing two of them knows they were
  proved together.

None of these is measured here. They are the reason a good per-swap byte figure is not by
itself a recommendation.

## 6. Measurement discipline

The binary follows the rules the ML-DSA experiment established, because each was learned
from a fault that would otherwise have shipped a plausible-looking wrong number:

- an **untimed warm-up** before the first measurement;
- **≥ 5 repetitions** per batch size (the Meeting-3 statistical floor), reported as
  mean ± SD via the existing `metrics::Stats`;
- a **success-path assertion** after every timed block — a timed region that silently
  measured a rejection would publish a fast, meaningless number;
- the **tamper check** described in §3.

A fresh SRS is generated per repetition, so proving is timed against a fresh proving key
rather than one warmed by earlier runs.

## 7. Scope and limits

- **Time and communication only.** No security claim is made about batching, for either
  prover.
- **The batched LaZer parameter sets have not been independently reviewed.** They come from
  the same LaZer codegen as the committed k=1 set and target the same knowledge error, but
  that is provenance, not review.
- **Nothing here is wired into the swap.** Configuration 3 continues to use the k=1 module
  (`relation_zk.{c,h}`); the batched module is an experiment beside it. The deployed prover,
  the KAT and every benchmark of record are untouched.
- **Groth16 is not post-quantum.** It is configuration 2's prover, kept as the controlled
  comparison against LaZer (2 → 3 isolates the proof system). Its amortisation result is
  evidence about *batching*, not about the post-quantum configuration.
- **Only k ∈ {1,2,4,8} has a generated parameter set.** The superlinear compute trend is
  read off four points; a larger `k` might plateau, but it would have to plateau very hard
  to reverse the verdict, since compute is already 3.3× worse at k=8.

## 8. Reproducing

```bash
./scripts/run_amortise_bench.sh      # Groth16  -> evidence/amortise/<ts>/
./scripts/run_lazer_amortise.sh      # LaZer    -> evidence/lazer_amortise/<ts>/
```

Each writes a timestamped directory with the tool's own output plus an environment record,
and repoints `latest`.

The Groth16 binary builds without the `groth16` feature and reports that it needs it, so
`cargo build` stays green either way. Expect several minutes: a fresh circuit-specific setup
is generated per repetition per batch size, and setup is the slowest phase.

The LaZer benchmark needs the vendored LaZer library built once (see the README's
"π / atomic swap" section). SageMath is **not** needed to build or run it — the batched
parameter headers are committed. To regenerate them:

```bash
./scripts/gen_lazer_batch_params.sh          # needs SageMath; ~30 s at k=2
```

Adding a new `k` also needs `params_for()` extended in `ref/relation_zk_lazer_batch.c` and
`PI_BATCH_MAX_K` raised in `ref/relation_zk_lazer_batch.h`.
