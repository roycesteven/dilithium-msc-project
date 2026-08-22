# The ML-DSA adaptor experiment — building LAS on NIST FIPS 204 as specified, and what it costs

**Status:** complete. Built, run, gated, evidence captured — 2026-08-03.
**Evidence:** `evidence/mldsa_hint/latest/` (all three ML-DSA parameter sets).
**Code:** `ref/mldsa_las.{c,h}`, `ref/test/test_mldsa_hint.c`,
`ref/test/test_mldsa_las.c`, `ref/test/bench_mldsa_compare.c`.
**Runner:** `scripts/run_mldsa_hint_experiment.sh`.
**Upstream functions modified: zero** — every rounding, hint, packing and NTT
primitive is called as-is, and the control verifier is the unmodified
`crypto_sign_verify`.

---

## 1. Why this experiment exists

Everywhere else, this project builds LAS on the LAS paper's **simplified**
Dilithium: no hint vector `h`, no Power2Round, no high/low-bit decomposition.
That simplification is what makes the adaptor identity `A z − c t = w + Y` hold
exactly. Until now the project **asserted** that NIST's ML-DSA construction must
be modified before an adaptor layer can sit on it. An assertion is the weakest
possible defence of a design decision a reader is entitled to challenge.

This experiment answers three questions in order, one binary each:

| binary | question | kind |
|---|---|---|
| `test_mldsa_hint` | **Which** ML-DSA feature breaks a naive adaptor port? | diagnostic — a `FAILS` row is a *result* |
| `test_mldsa_las` | Is the repaired scheme a **correct** adaptor signature? | contract — pass/fail, non-zero exit |
| `bench_mldsa_compare` | What does it **cost** against the implementation of record? | benchmark — gated |

Supervisor context: Wang named the hint work and said *"just try it, just try
it."* Royce's clarification (`CLAUDE.md`, 2026-08-03) governs the framing: the
objective is **evidential**, not a size micro-optimisation.

---

## 2. What was built

`ref/mldsa_las.c` is a structural mirror of upstream `ref/sign.c`, written the
way `basesig.c` and `las.c` mirror it — same order, same locals, same
`[REUSED]`/`[CHANGED]`/`[NEW]` annotations — so every deviation from FIPS 204 is
one annotated line.

Shapes at ML-DSA's own parameters (`A` is `K × L` over `R_q`): witness
`y ∈ R_q^L` with `‖y‖∞ ≤ ETA` (so it can be added to `z`), statement
`Y = A y ∈ R_q^K` (so it shifts `A z` by exactly `Y`).

Three variants, differing **only** in how much of ML-DSA's signing path the
adaptor is allowed to touch:

| variant | what changes vs `crypto_sign_signature_internal` |
|---|---|
| **VBASE** | nothing — the Y-shift is removed. The *matched baseline*. |
| **V0 (naive)** | exactly one line: the challenge is hashed over `HighBits(w + Y)` instead of `HighBits(w)`. The committed high bits, the low-bits rejection test and `MakeHint` still use `w`. This is what treating the hint machinery as a black box produces. |
| **V1 (shifted)** | the **entire** commitment path moves onto `w + Y`: committed high bits, low-bits rejection test *and* `MakeHint`. PreSign additionally rejects at `GAMMA1 − BETA − ETA` so the adapted `z = ẑ + y` clears ML-DSA's own bound. |

**Wire format.** The signature, pre-signature and public key reuse FIPS 204's own
encodings — that is the point of building on ML-DSA. Only the two
adaptor-specific objects need one: `Y` is a full `R_q^K` element at 23 bits per
coefficient (Power2Round compresses the public key to `t1`, but `Y` enters the
identity *before* any rounding, so nothing may be dropped), and the witness
reuses ML-DSA's own `polyeta` codec. Both decoders **validate** canonical range,
matching `ref/serialize.c`'s posture.

**The fidelity gate.** VBASE exists because a null result would otherwise be
unattributable: its signatures must be accepted by the *stock*
`crypto_sign_verify`, and it must reproduce FIPS 204's published repetition
rates. If it does not, the mirror is wrong and nothing else is interpretable.

---

## 3. Results

Machine: AMD Ryzen 7 7745HX, WSL2, gcc 13.3.0, `-O3`. 200 iterations per
variant for the diagnostics and contract; 5 × 500/1000 for the benchmark.

### 3.1 The fidelity gate passes and reproduces FIPS 204's own numbers

| set | fidelity gate | measured attempts/Sign | FIPS 204 expected |
|---|---|---|---|
| ML-DSA-44 | 200/200 | 4.246 | ≈ 4.25 |
| ML-DSA-65 | 200/200 | 5.217 | ≈ 5.1 |
| ML-DSA-87 | 200/200 | 3.996 | ≈ 3.85 |

The mirror reproduces the standard's own repetition rates. That is why the
variant rows below are attributable to the adaptor rather than to a porting
mistake.

### 3.2 Which ML-DSA feature breaks the naive port

Adaptor contract per variant (ML-DSA-65; 44 and 87 identical in pattern):

| property | V0 naive | V1 shifted |
|---|---|---|
| P1 PreVerify accepts the pre-signature | **0/200** | 200/200 |
| P2 stock Verify **rejects** the pre-signature (tripwire) | 200/200 | 200/200 |
| P3 Adapt succeeds | **0/200** | 200/200 |
| **P4 stock Verify ACCEPTS the adapted signature** | **0/200** | **200/200** |
| P5 Ext recovers the exact witness | **0/200** | 200/200 |
| P6 adapted `‖z‖∞ < GAMMA1 − BETA` | **0/200** | 200/200 |
| P7 hint weight ≤ OMEGA | 200/200 | 200/200 |

*Reading V0 correctly:* V0 fails at **P1**. Because `Adapt` refuses a
pre-signature that does not pre-verify (eprint 2020/845 Alg. 2, line 21), P3–P6
never execute — their zeros follow from P1 and are not four independent
findings.

### 3.3 The repaired scheme is a correct adaptor signature

`test_mldsa_las` holds V1 to the same itemised contract `test_contract.c` holds
the simplified scheme to. **13/13 items pass at all three parameter sets:**
completeness; the statement-binding tripwire; adaptability under an *unmodified*
verifier; exact extraction; the adapted `z` bound; hint weight; wire round-trip;
plus four tamper rejections (message, pre-signature, wrong statement, wrong
witness), a malformed-statement rejection, and byte-identical determinism in
`(sk, Y, M, rnd)`.

### 3.4 Cost: adaptor overhead within each construction

Paired and **interleaved** — each pair alternates within every repetition, so
clock drift is common-mode; mean ± SD over the 5 per-repetition ratios. PreSign
is compared *per attempt*, so realised restart counts cannot bias it.

| pair | Simplified Dilithium-II | ML-DSA-44 | Simplified Dilithium-III | ML-DSA-65 | Simplified Dilithium-V | ML-DSA-87 |
|---|---|---|---|---|---|---|
| PreSign vs Sign (/attempt) | +1.3 ± 2.1 % | +3.8 ± 1.9 % | +2.2 ± 0.5 % | +2.8 ± 1.8 % | +2.4 ± 0.5 % | +3.5 ± 2.9 % |
| PreVerify vs Verify | +5.1 ± 2.8 % | +0.8 ± 0.9 % | +3.6 ± 0.4 % | +1.7 ± 1.0 % | +3.3 ± 0.3 % | +0.8 ± 1.8 % |
| Adapt vs Verify | +7.3 ± 0.5 % | +6.9 ± 5.4 % | +9.0 ± 4.1 % | +6.4 ± 0.7 % | +7.3 ± 0.7 % | +5.2 ± 1.1 % |

**The adaptor layer is cheap on both constructions** — single-digit percent on
every operation, at every parameter set. Adding the adaptor to real ML-DSA costs
no more than adding it to the simplified scheme.

### 3.5 Cost: the two constructions against each other

Per-operation, µs/op, ML-DSA ÷ simplified (mean ± SD over 5 repetitions):

| operation | Simplified-III | ML-DSA-65 | ratio |
|---|---|---|---|
| KeyGen | 50.70 ± 0.26 | 119.33 ± 0.52 | 2.35× |
| Sign | 480.91 ± 24.87 | 514.73 ± 21.81 | 1.07× |
| Verify | 112.90 ± 0.50 | 110.73 ± 0.79 | 0.98× |
| PreSign | 507.70 ± 13.30 | 523.35 ± 20.39 | 1.03× |
| PreVerify | 116.00 ± 1.48 | 115.36 ± 3.01 | 0.99× |
| Adapt | 120.29 ± 0.21 | 116.19 ± 0.27 | 0.97× |
| Extract | 43.81 ± 1.06 | 92.28 ± 0.55 | 2.11× |

The matched ML-DSA partner (514.73 µs) lands on stock `crypto_sign_signature`
(522.41 µs) measured in the same run — independent confirmation that the mirror
costs what real ML-DSA costs.

**The two constructions cost about the same per signature, for opposite
reasons.** ML-DSA needs roughly twice the restarts (5.22 vs 2.71 attempts/Sign
at level 3) but each attempt is roughly half the price — its mask is
bit-unpacked from `γ1 = 2^19` over `L = 5` polynomials, while the simplified
scheme rejection-samples a mask over `n + ℓ = 11` polynomials from a
non-power-of-two range. The two effects very nearly cancel.

### 3.6 Communication — the finding that actually matters

| object | Simplified-III | ML-DSA-65 | ratio |
|---|---|---|---|
| public key | 4 416 | 1 952 | 0.44× |
| signature | 6 736 | 3 309 | **0.49×** |
| pre-signature | 6 736 | 3 309 | 0.49× |
| **statement `Y`** | 4 416 | 4 416 | **1.00×** |
| witness | 704 | 640 | 0.91× |
| **swap payload (σ + Y)** | 11 152 | 7 725 | **0.69×** |

Building on real ML-DSA **halves the signature and the public key at no
computational cost** — the hint vector and Power2Round are exactly the
optimisations that buy this, and the adaptor survives them. But the statement
`Y` does not shrink *at all*: it is `K` polynomials at full 23-bit width in both
constructions, so at level 3 the two are byte-identical at 4 416 B. The end
result is that the swap payload improves only to 0.69×, not to 0.49×.

**`Y` is larger than the signature it accompanies at every parameter set** (4 416
against 3 309 at level 3). The adaptor's own object, not the signature, becomes
the dominant term the moment the signature is compressed. The same pattern holds
at levels 2 and 5 (0.52×/1.00×/0.71× and 0.50×/1.00×/0.70×).

---

## 4. What the experiment shows

The project's prior assertion — *"NIST's ML-DSA construction must be modified for
LAS to work correctly"* — is **half right, and the half that is wrong is the more
useful half.**

### 4.1 Confirmed — the signer's side must be modified

The naive port is broken at the first step (P1 = 0/200): the transmitted hint
reconstructs `HighBits(w)` while the challenge committed to `HighBits(w + Y)`.
Four modifications are unavoidable, each checkable in the code:

1. **PreSign cannot be ML-DSA's `Sign`.** `Sign` commits to `HighBits(w)`, which
   contains no statement.
2. **The whole commitment path must move, not just the hash input.** This is
   exactly what V0 → V1 isolates, and it is the sharpest result here: `MakeHint`
   and the low-bits rejection test must be taken around `w + Y` too.
3. **PreVerify cannot be ML-DSA's `Verify`.** It must add `Y` *before* `UseHint`,
   and the stock verifier has no `Y`.
4. **PreSign must tighten the bound** to `GAMMA1 − BETA − ETA` — the ML-DSA
   analogue of LAS's `γ − κ − 1` tightening, with the same failure mode if
   loosened.

And a structural constraint no engineering removes:

5. **`Adapt` can never repair a hint.** `MakeHint` needs `c·t0` and `w − c·s2`,
   both from the **signer's** secret key; the adapting party holds only the
   witness. This is why (2) must happen at PreSign time and nowhere else.

### 4.2 Refuted — the *verifier* does not need modifying

**P4 holds 200/200 at all three parameter sets.** With the hint vector,
Power2Round and the high/low-bit decomposition all enabled, the adapted signature
is accepted by the **unmodified** FIPS 204 verifier.

This is stronger and more deployable than the claim it replaces: a post-quantum
adaptor swap can settle against a **standard ML-DSA verifier**. The adaptor
machinery is confined to the two parties; the chain sees an ordinary ML-DSA
signature. Only the signer-side algorithms are new.

### 4.3 The engineering conclusion

Migrating the project to ML-DSA would be a **real but bounded** win: half the
signature, half the public key, the same computation, the same small adaptor
overhead, and compatibility with a standard verifier. It would **not** fix the
communication story, because the statement `Y` is unchanged and becomes the
dominant object once the signature shrinks. Any future size work should target
`Y`, not the signature.

---

## 5. Scope and honesty caveats

- **This is a functional demonstration, not a security proof.** Whether
  committing to `HighBits(w + Y)` preserves ML-DSA's EUF-CMA argument, and
  whether the pre-signature leaks anything about `s1`, are **not** analysed.
  Security analysis is out of scope by supervisor ruling, and that ruling applies
  here. Nothing here claims V1 is *secure* — only that it is *correct* in the
  sense the contract defines.
- **Not a security comparison between the two constructions.** They have
  different parameters and different security arguments. §3.5–3.6 compare two
  engineering routes to the same functionality, nothing more.
- **Timing caveats.** Wall-clock on a loaded laptop under WSL2. The two
  constructions are measured **sequentially within one process** — which removes
  the machine and toolchain caveats of a cross-run comparison, but *not*
  intra-run drift. The overhead figures are paired and interleaved precisely
  because drift was large enough to invert them; the cross-construction ratios in
  §3.5 are not paired and should be read as approximate.
- **200 iterations is a correctness sample, not a statistical study.** It
  separates "always" from "never", which is what P1–P7 ask.
- **This does not change the scheme of record.** `ref/las.c` on the simplified
  scheme remains what the report measures. `mldsa_las.c` is a standalone
  experiment, never linked into the benchmarks or the KAT, and its numbers must
  never be mixed with `evidence/latest/`.

---

## 6. Reproducing

```bash
./scripts/run_mldsa_hint_experiment.sh                # all three sets
./scripts/run_mldsa_hint_experiment.sh --mode 3       # ML-DSA-65 only
./scripts/run_mldsa_hint_experiment.sh --skip-bench   # correctness only
```

or directly:

```bash
cd ref
make test/test_mldsa_hint3    && ./test/test_mldsa_hint3      # diagnostic
make test/test_mldsa_las3     && ./test/test_mldsa_las3       # contract
make test/bench_mldsa_compare3 && ./test/bench_mldsa_compare3 # head-to-head
```

A `FAILS ALWAYS` row in the *diagnostic* is a result. The contract and the
benchmark exit non-zero on any failure: the benchmark gates on the rejection
rates, on the attempt counters actually tracking their loops, and on every timed
operation being on its success path.

### Faults these gates have already caught

Recorded because they would each have produced a plausible-looking wrong number:

1. **Missing FIPS 204 context prefix.** PreSign/PreVerify omitted the two bytes
   `{0,0}` that `crypto_sign_verify` absorbs into `mu`, so P4 failed 0/200 in
   *both* variants — which looked like the headline finding and was a harness
   bug.
2. **Deterministic rejection loop.** The benchmark re-signed one fixed instance,
   so ML-DSA's loop restarted the same number of times every call: exactly
   `4.0000` and `2.0000` attempts/call. The rejection gate caught it.
3. **Stale key.** The KeyGen benchmark replaced the ML-DSA keypair, leaving the
   signature `Verify` was timed on invalid — so `Verify` was timing the
   *rejection* path. Now a success-path assertion follows every timed block.
4. **Drift-inverted overheads.** Measuring Sign and PreSign in separate blocks
   let clock drift land on one of them, producing overheads that swung between
   −3 % and +8 % and sometimes came out negative. Fixed by pairing and
   interleaving.
5. **Mislabelled column.** The simplified column was hardcoded "Dilithium-III"
   at all three modes. Now keyed on `(LAS_N, ELL, KAPPA)` with an `#error` on an
   unrecognised set.
