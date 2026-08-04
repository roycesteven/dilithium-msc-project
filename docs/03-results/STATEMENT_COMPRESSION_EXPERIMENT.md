# Statement compression — can `Y` be made smaller on the wire?

**Status: run 2026-08-04**, all three LAS sets, 100 iterations per candidate.
Evidence: `evidence/statement_compress/latest/statement_compress_set{2,3,5}.log`.
Numbers below are quoted from the target set (D3-aligned, `n=6, ℓ=5, κ=49`) unless said
otherwise; the sets differ only in magnitude, never in verdict.

**Headline: `Y` is incompressible in this construction, and the failure is exact, not
marginal.** Truncation is *invisible* to the adaptor's own functions and *fatal* at both
boundaries — the chain verifier and Ext fail **0/100 at every truncation depth tested,
including b=1**. The one compression that reproduces `Y` exactly (send the seed) is a total
break of the witness. The assertion this experiment was built to test is confirmed, and now
has a mechanism attached to it rather than an argument.

---

## 1. Why this experiment exists

The ML-DSA head-to-head (`docs/03-results/MLDSA_HINT_EXPERIMENT.md`) produced a result
that redirected the size question. Building the adaptor on unmodified ML-DSA **halves the
signature and the public key**, but leaves the statement `Y` **byte-identical**, because
`Y` is `n` full-width polynomials in either construction. The swap payload therefore only
improves to about 0.69×, not 0.49×, and `Y` — not `z` — becomes the object that limits it.

That made "can `Y` be compressed?" the load-bearing open question. The project's answer so
far was an **assertion**: `test_mldsa_hint.c` prints it as structural finding (d), *"the
statement is not compressible the way the public key is — `t` is sent as `t1` via
Power2Round; `Y` enters the identity before rounding and is sent in full."*

An assertion defended by argument alone is one nobody has tested. This experiment tests it,
in exactly the shape the hint experiment used: run the candidate compressions through the
full adaptor contract and report, per property, what survives.

## 2. What it runs

`ref/test/test_statement_compress.c`, built at all three LAS parameter sets
(`test/test_statement_compress{2,3,5}`). Each iteration runs one honest swap leg —
Gen → PreSign → PreVerify → Adapt → Verify → Ext — with a fresh key pair, statement and
message, and gives **every adaptor call the compressed statement**, never the true `Y`.
That is the modelling point: under a compression, the compressed object *is* the statement
the protocol has.

| candidate | what is put on the wire |
|---|---|
| **C0 CONTROL** | `Y` in full, 23-bit canonical packing — the scheme as it stands |
| **C1 TRUNCATE(b)** | `Y` with its `b` low bits cleared — Power2Round's move, applied to the statement. Swept over `b = 1, 2, 4, 8, 13` (13 being ML-DSA's own `d`) |
| **C2 SEED** | not `Y` at all: the 32-byte generator seed, with the receiver recomputing `Y = A r'` |

C1 is applied **identically at PreSign, PreVerify, Adapt and Ext**, so both parties agree
on the same compressed statement — the fairest possible version of the idea. C2 reproduces
`Y` *exactly*, so it is not an approximation at all.

## 3. The properties

| | property | why it is here |
|---|---|---|
| P1 | PreVerify accepts the pre-signature | the adaptor's own check |
| P2 | Adapt succeeds | ditto |
| **P3** | **base Verify ACCEPTS the adapted signature** | **the chain** — what an unmodified consensus verifier is asked to do |
| **P4** | **Ext recovers the exact witness** | **atomicity** — what lets the counterparty complete the second leg |
| P5 | adapted `‖z‖∞ < BOUND_SIGN` | Algorithm 1's own bound |

P3 and P4 are the two the experiment exists for. A compression that breaks either is not a
size trade-off; it removes the reason to use an adaptor at all.

## 4. How to read the output

**Diagnostic, not pass/fail — a `FAILS` row is a result**, exactly as in the hint
experiment. It localises which function a candidate breaks.

The one hard gate is **C0**: the binary exits non-zero if the uncompressed statement does
not hold the contract, because then no row below it is attributable to compression rather
than to a broken harness.

The binary also prints three accounting sections that do not depend on the run:

- **the hint accounting** — what a hint-carried statement would cost against what
  truncation saves;
- **the rejection-sampling escape** — the cost of sampling `r'` so that `A r'` already has
  `b` zero low bits;
- **the compression already applied** — 23-bit canonical packing against one `int32` per
  coefficient, which is the lossless baseline any further scheme must beat.

## 5. Measured result

Target set, 100 iterations per candidate. `Y` = 4416 B against a 6736 B signature.

| candidate | wire | P1 PreVerify | P2 Adapt | **P3 Verify** | **P4 Ext** | P5 bound |
|---|---|---|---|---|---|---|
| C0 control | 4416 B | 100/100 | 100/100 | **100/100** | **100/100** | 100/100 |
| C2 seed | 32 B | 100/100 | 100/100 | **100/100** | **100/100** | 100/100 |
| C1 b=1 | 4224 B | 100/100 | 100/100 | **0/100** | **0/100** | 100/100 |
| C1 b=2 | 4032 B | 100/100 | 100/100 | **0/100** | **0/100** | 100/100 |
| C1 b=4 | 3648 B | 100/100 | 100/100 | **0/100** | **0/100** | 100/100 |
| C1 b=8 | 2880 B | 100/100 | 100/100 | **0/100** | **0/100** | 100/100 |
| C1 b=13 | 1920 B | 100/100 | 100/100 | **0/100** | **0/100** | 100/100 |

The control held at all three sets, so every row is attributable to the compression rather
than to the harness.

**Truncation (C1).** The P1/P2 columns holding while P3/P4 read 0/100 *is* the finding: the
compression is perfectly consistent between the two parties running the adaptor, and
perfectly invisible to the two functions that matter. It is not a marginal failure that a
tolerance could absorb — **b=1, a 4% saving, already fails every one of 100 iterations at
both boundaries.** Truncation at b=1 moved 76,798 of 153,600 coefficients (~50%, i.e. every
odd coefficient), so the statement genuinely changed; by b=13 it moves 153,586 of 153,600.

**Seed (C2).** Every functional property holds 100/100 — `Y` is reproduced *exactly*, so
this is not an approximation — at 4416 B → 32 B, a **138× compression**. And the receiver
recovered the witness in **100/100** iterations. The largest available compression is a
total break, demonstrated rather than argued.

**Hint accounting.** Net **+0 B at every depth** (b=1: saves 192 B, exact repair costs
192 B; b=13: 2496 B against 2496 B). Zero by construction, not coincidence.

**The lossless baseline.** 23-bit canonical packing is 4416 B against 6144 B for one
`int32` per coefficient — **−28%, already applied, and lossless**. That is the figure any
future proposal has to beat.

**The rejection-sampling escape**, sampling `r'` so that `A r'` already has `b` zero low
bits, costs `2^-1536` per attempt at b=1 on the target set.

## 6. What the code establishes structurally

Two of the findings do not depend on the counts, and the harness is built so they are
checkable rather than argued:

**Truncation is invisible to the adaptor and fatal at its boundaries.** PreSign and
PreVerify both hash the statement they were handed, so they agree with each other whatever
that statement is, and Adapt only re-runs PreVerify before adding the witness. But
`base_verify` never sees a statement: it recomputes `A z − c t`, which for an adapted
signature equals `w + Y` with the **true** `Y` — since `A r' = Y` exactly — and hashes
that. The pre-signature committed to `w + Y_truncated`. And Ext's acceptance test *is* the
exact relation `A s == Y`. So the compression is not in the algebra, and neither the chain
nor the extraction can be told about it.

**A hint cannot rescue it.** ML-DSA can compress `t` because the verifier *independently
recomputes an approximation* of the rounded quantity and the hint only repairs the last
carry. No party can compute anything near `Y` without the witness — that is the hardness
assumption the statement rests on — so a hint has no approximation to correct and must
carry the dropped bits outright. The accounting is net zero **by construction**: the bits a
hint would carry are exactly the bits truncation dropped.

**The seed is a total compression and a total break.** Gen derives *both* `Y` and `r'` from
one seed, so the compressed statement **is** the witness. The harness demonstrates this
rather than asserting it: it re-runs Gen on the transmitted bytes and compares the
recovered witness against the real one.

## 7. Scope and limits

- **Functional admissibility only, with one candidate that needs care.** The harness shows
  which compressions break the adaptor *contract* and where. C2 (the seed) **passes every
  functional row** and is rejected on a different ground — the witness recovery the harness
  demonstrates directly, which is a break anyone can check, not a security *analysis*. No
  candidate is reported as both functionally sound and safe. Security analysis proper
  remains out of scope for the project.
- The candidates change **only what is put on the wire**. No scheme file is modified; the
  verifier is the unmodified `base_verify`.
- This settles compression of `Y` **within this construction**. It does not rule out a
  different hard relation whose statement is smaller by design — that is a different
  question, and it is the one the report's future work should point at.

## 8. Reproducing

```bash
./scripts/run_statement_compress.sh              # all three sets
./scripts/run_statement_compress.sh --set 3      # target set only
```

Writes `evidence/statement_compress/<timestamp>/` with the tool's own output plus an
environment record, and repoints `latest`. Never hand-edit a log: to change a number,
change the code and re-run.
