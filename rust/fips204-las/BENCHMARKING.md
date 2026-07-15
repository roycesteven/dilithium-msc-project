# Benchmarking guide — Algorithm 1 vs Algorithm 2 (Rust port)

How to run, read, and reproduce the Stage-1 benchmarks of this crate:
the ordinary lattice-based signature (Algorithm 1, `src/basesig.rs`)
versus the LAS adaptor signature (Algorithm 2, `src/las.rs`), both at the
Simplified Dilithium-III setting `(n=6, ℓ=5, κ=49)`.

## TL;DR — four commands

```sh
cd rust/fips204-las
cargo test --lib --tests                      # 1. gate: never benchmark unverified code
cargo bench --bench las_bench                 # 2. standard statistical micro-benchmark (Criterion.rs)
cargo run --release --example bench_levels    # 3. protocol driver: overhead % + rejection counters
cargo run --release --example size_report     # 4. communication cost: component-level packed sizes
```

Where the results appear:

| Output | Location |
| --- | --- |
| Criterion terminal summary (confidence intervals, outliers) | printed during step 2 |
| Criterion HTML report (distribution plots, per-benchmark pages, run history) | `target/criterion/report/index.html` |
| Saved raw log of the committed Criterion run | `bench_las_criterion.log` (this directory) |
| Machine-readable estimates (for baseline diffing) | `target/criterion/<group>/<benchmark>/new/estimates.json` |
| Protocol-driver table + overhead summary | printed during step 3; committed run: `bench_levels_rust.log` |
| Component-size table (communication cost) | printed during step 4; committed run: `size_report_rust.log` |

Criterion version note: upstream pins `criterion = "0.4.0"` (held back for its
MSRV 1.70 policy); this fork bumps that dev-dependency to **0.8.2** — the one
upstream manifest line changed (dev-only, documented in `LAS_PROVENANCE.md`).
0.8.2 generates the HTML report above out of the box.

## The three evidence tools, and why each exists

| Tool | File | What it answers | What it cannot do |
| --- | --- | --- | --- |
| Criterion.rs micro-benchmark | `benches/las_bench.rs` | Statistically defensible per-operation time: warm-up, 300 samples per operation over a 60 s window, outlier classification, bootstrap 95% confidence interval, significance-tested baseline diffing | Does not compute the Algorithm-1-vs-Algorithm-2 overhead summary, and cannot read the rejection-restart counters |
| Protocol driver | `examples/bench_levels.rs` | The overhead summary (PreSign vs Sign, PreVerify vs Verify, Adapt vs Verify), measured attempts/signature from `BASE_ATTEMPTS`/`LAS_ATTEMPTS`, and the per-attempt (rejection-normalised) diagnostic | No confidence intervals or outlier analysis — it reports mean ± SD over 5 repetitions |
| Size report | `examples/size_report.rs` | Communication cost: measured component-level packed sizes (pk, sk, statement Y, witness, challenge c, response z, signature / pre-signature / adapted signature), hard-asserted equal to the C evidence row | Nothing statistical — sizes are deterministic, which is exactly why Criterion is not involved |

They are complementary: **quote per-operation times from Criterion; quote the
overhead percentages and rejection rates from the protocol driver; quote
communication cost from the size report.** All three run on a state gated by
the same success-path contract.

## Why Criterion.rs — mapped to the supervisor's benchmark requirements

Criterion.rs is the de-facto standard statistical benchmark harness of the
Rust ecosystem, and specifically of Rust cryptography. The decisive fact:
**the upstream `fips204` crate itself benchmarks with Criterion**
(`benches/benchmark.rs`, the shared dev-dependency — bumped `0.4.0 → 0.8.2`
here, dev-only), so the LAS measurements use exactly the methodology of the
base crate's author.

Requirement-by-requirement (`las-context-consolidated.md`):

| Supervisor requirement | How this setup meets it |
| --- | --- |
| §7 [Meeting 2]: ≥ 1000 iterations per operation, median + standard deviation, same machine and build flags | Criterion auto-tunes iterations: the committed run used 135 000 (PreSign) up to 2 200 000 (Extract) iterations per operation; it reports mean, median and SD; one machine, one `[profile.bench]` |
| §13.3 [Meeting 3]: ≥ 5 repetitions, mean with variance/SD, never single shots | 300 samples per operation over a 60 s window, Tukey outlier classification, bootstrap 95% confidence interval of the mean |
| §14.3 [Meeting 4]: per-operation timing is the PRIMARY result | Each of KeyGen, Sign, Verify, PreSign, PreVerify, Adapt, Extract is an independent benchmark |
| §14.5 [Meeting 4]: state machine, compiler, flags, iteration and run counts | Recorded below and in the committed log; build profile pinned in `Cargo.toml [profile.bench]` |

What Criterion alone does *not* cover — the §13.5 overhead summary and the
directly measured rejection rate — is exactly what the protocol driver
provides. The pair covers the full requirement list.

Alternative considered and rejected: `iai-callgrind` counts instructions
under Valgrind — a different metric (not wall-clock), which could not be
compared with the C timings at all. Hand-rolled loops (what the protocol
driver does) remain necessary for the counters, but are not a substitute for
the statistical harness.

## Prerequisites

- Rust toolchain: the **library** keeps upstream's MSRV 1.70, but building the
  benches needs a recent stable toolchain (criterion 0.8.2; the committed run
  used `rustc 1.96.0`). No other system dependency — the HTML report uses the
  pure-Rust `plotters` backend, gnuplot is **not** required.
- A quiet machine: close heavy background processes; on laptops, plug in
  (frequency scaling inflates variance). See Troubleshooting for WSL2.

## Step-by-step reproduction

### Step 1 — Verify before you measure

```sh
cd rust/fips204-las
cargo test --test las_kat -- --nocapture   # pinned digest bb6ad0da…260c == C value
cargo test --lib --tests                   # upstream suite + interlock + serde tests
```

If the KAT digest does not match, stop: the port is broken and any timing of
it is meaningless.

### Step 2 — Run the Criterion benchmark

```sh
cargo bench --bench las_bench 2>&1 | tee bench_las_criterion.log
```

This builds with the crate's `[profile.bench]` (`opt-level = 3`, `lto`,
`codegen-units = 1`) and runs seven benchmarks in two groups, each with the
custom config in `criterion_config()`: **300 samples over a 60 s measurement
window per benchmark** (full run ≈ 8 minutes) — sized so the sign-class
attempt distribution converges and the PreSign-vs-Sign difference resolves
above measurement noise:

- **Algorithm 1 — ordinary lattice-based signature:** KeyGen, Sign, Verify
  (from the independent `las_basesig` module);
- **Algorithm 2 — LAS adaptor signature:** PreSign, PreVerify,
  Adapt (including its internal PreVerify), Extract.

Before any measurement the state is asserted against the full success-path
contract (the pre-signature must FAIL the ordinary verifier; the adapted
signature must pass the *independent* base verifier; Ext must recover the
witness exactly), so no failure path is ever timed. The RNG is a fixed-seed
`ChaCha8Rng`, so the workload is reproducible.

### Step 3 — Read a Criterion result block

Real excerpt from the committed run:

```text
Algorithm 2 - LAS adaptor signature/PreSign
                        time:   [447.67 µs 450.14 µs 452.60 µs]
Found 14 outliers among 300 measurements (4.67%)
```

- `time: [low estimate high]` is the **bootstrap 95% confidence interval of
  the mean time per call**; quote the middle value as the point estimate.
- The outlier line is Criterion's Tukey classification of the 300 samples —
  see "How to read the numbers" for why sign-class outliers are expected.
- For the sign-class benchmarks Criterion may stretch past the 60 s target to
  keep all 300 samples (the committed run collected Sign over ≈ 79 s) —
  harmless, the sample count is preserved.

### Step 4 — Run the protocol driver for overheads and rejection counters

```sh
cargo run --release --example bench_levels | tee bench_levels_rust.log
```

This prints the per-operation table (5 repetitions, mean ± SD), the
Algorithm-1-vs-Algorithm-2 overhead percentages, the measured
attempts/signature on both paths, and the per-attempt diagnostic.

### Step 5 — Run the size report for communication cost

```sh
cargo run --release --example size_report | tee size_report_rust.log
```

Deterministic (no statistics needed): it packs live, contract-gated objects
with `serialize.rs` and prints the component-level byte table — see
"Communication cost" below.

## Measured results (committed run)

Measured 2026-07-05 on WSL2 / AMD Ryzen 7 7745HX, `rustc 1.96.0`,
criterion 0.8.2, 300 samples over a 60 s window per operation,
`[profile.bench]` (`opt-level = 3`, `lto`, `codegen-units = 1`); raw log:
`bench_las_criterion.log`; results also saved as baseline `criterion082`.
Times in µs; the mean column is the bootstrap 95% confidence interval
`[low · point estimate · high]`:

| Operation | Mean (µs) [low · point · high] | Median (µs) | Iterations |
| --- | --- | --- | --- |
| Algorithm 1 — KeyGen | 39.03 · 39.13 · 39.25 | 39.18 | 1 500 000 |
| Algorithm 1 — Sign | 448.63 · 451.07 · 453.46 | 448.26 | 181 000 |
| Algorithm 1 — Verify | 79.67 · 79.89 · 80.15 | 79.98 | 768 000 |
| Algorithm 2 — PreSign | 447.67 · 450.14 · 452.60 | 449.91 | 135 000 |
| Algorithm 2 — PreVerify | 80.29 · 80.51 · 80.74 | 80.28 | 768 000 |
| Algorithm 2 — Adapt (including its internal PreVerify) | 82.04 · 82.21 · 82.39 | 82.62 | 768 000 |
| Algorithm 2 — Extract | 27.22 · 27.40 · 27.61 | 26.89 | 2 200 000 |

Adaptor overhead (sign-class quoted from means, verify-class from medians —
see the rules below):

- **PreSign vs Sign: statistically indistinguishable** (−0.2% on means,
  +0.4% on medians; the two confidence intervals almost coincide);
- **PreVerify vs Verify: +0.4%** (medians; on means +0.8% with disjoint
  confidence intervals);
- **Adapt vs Verify: +3.3%** (medians; on means +2.9% with disjoint
  confidence intervals);
- **Extract ≈ 27 µs** — no Algorithm-1 analogue.

The physically expected ordering **Verify < PreVerify < Adapt** is resolved
by the data (Adapt = PreVerify + witness addition + norm check). This agrees
with the protocol driver (+3.9% / −1.2% / +0.4%, `bench_levels_rust.log`,
run 2026-07-03) and with the C headline (+6.7% / +3.1% / +8.1%,
`evidence/latest/`): **the adaptor layer costs at most a few percent per
operation.**

Note on absolute times: the protocol-driver log was captured on 2026-07-03 in
a slower machine state (WSL2/laptop frequency scaling; ≈ 2.5× slower uniformly
across all operations). This is exactly why the rule below says compare
overhead **ratios**, never raw microseconds, across runs, machines or
languages.

## Communication cost — component sizes (measured, committed run)

From `size_report_rust.log` (`cargo run --release --example size_report`),
Simplified Dilithium-III setting, wire format `serialize.rs` — every value
hard-asserted equal to the C evidence row
(`evidence/latest/tables/communication_components.csv`, level L3):

| Component | Bytes | % of signature |
| --- | --- | --- |
| public key pk = t | 4416 | 65.40 |
| secret key sk = r | 704 | 10.43 |
| statement Y = t′ (adaptor lock) | 4416 | 65.40 |
| witness r′ | 704 | 10.43 |
| challenge c_tilde | 32 | 0.48 |
| response z (= ẑ in the pre-signature) | 6688 | 99.52 |
| signature (c, z) | 6720 | 100.00 |
| pre-signature (c, ẑ) | 6720 | 100.00 |
| adapted signature (c, z) | 6720 | 100.00 |

The findings the supervisor asks to state explicitly (§13.2, §14.4):

- **the response z drives the size** (99.52% of the signature); the challenge
  is negligible (32 B);
- **signature, pre-signature and adapted signature are byte-identical in
  size**: Adapt only adds the ternary witness (`‖y‖∞ ≤ 1`) to ẑ, so `‖z‖∞`
  grows by at most 1 and stays inside the same 19-bit packed field;
- the only *extra* object the adaptor protocol communicates is the statement
  `Y` (4416 B = one public key).

Sizes are deterministic, so this is a measurement program, not a Criterion
benchmark; the Rust table covers the Simplified Dilithium-III setting only
(the Rust port hard-codes it) — the four-set sweep remains C-side evidence.

## How to read the numbers — the four rules

1. **Sign and PreSign timing is multimodal by design.** Fiat–Shamir with
   aborts restarts on rejection; acceptance is ≈ 1/e ≈ 37% per attempt, so a
   call takes 1, 2, 3, … passes with geometric probability. Criterion samples
   average tens of calls each, so restarts are *included* in the reported
   mean — which is what a protocol actually pays. Any extra outliers flagged
   for these two benchmarks are the attempt distribution, not measurement
   noise.
2. **Beware restart-count luck at low iteration counts.** The PreSign-vs-Sign
   ratio only stabilises once both paths' average attempt count has converged
   to ≈ e. A 100-iteration pilot of the protocol driver showed a spurious
   +28% overhead purely from luckier restarts on one path; at ≥ 500
   iterations (and under Criterion's thousands) it converges. Cross-check
   with the driver's **per-attempt diagnostic**, which divides out the
   restart counts entirely.
3. **Never compare raw Rust microseconds with raw C microseconds.** Different
   compilers (`rustc` vs `gcc -O3`) and optimisation profiles. The
   cross-language claim is that the **Algorithm-1-vs-Algorithm-2 overhead
   ratios agree**, not the absolute times.
4. **The C evidence remains the report's primary numbers**
   (`evidence/latest/`, produced by `scripts/run_benchmark_suite.sh`). The
   Rust measurements are the independent cross-language confirmation.

## Run validity — the rejection gate

Every timing run of the sign-class operations now **validates itself**: the
Criterion benchmark and the protocol driver count Sign/PreSign calls, read the
`BASE_ATTEMPTS`/`LAS_ATTEMPTS` counters (outside the timed region; the only
in-loop cost is one `Cell` increment, sub-ns against a ~450 µs call), and
**hard-assert** that the restart rate measured *in that very run* matches the
exact theoretical expectation. A run whose rejection sampling misbehaves fails
loudly instead of producing a plausible-looking but invalid log.

The expectation is exact, not the ≈ e approximation
(`las_expected_attempts` in `src/las.rs`, derivation verified against eprint
2020/845): the mask coefficient is uniform on `[-γ, γ]` (2γ+1 values, Table 1),
the secret-dependent shift obeys `‖cr‖∞ ≤ κ` (Fact 1), so each of the
`(n+ℓ)·d` coefficients accepts independently with probability exactly
`(2·bound−1)/(2γ+1)`, giving

- Algorithm 1 Sign (accept `‖z‖∞ ≤ γ−κ`, Alg. 1 step 11): **2.71875**
  attempts/call (acceptance 36.78%),
- Algorithm 2 PreSign (accept `‖ẑ‖∞ ≤ γ−κ−1`, Alg. 2 step 6): **2.77483**
  attempts/call (acceptance 36.04%)

at this build's simplified Dilithium-III set (n=6, ℓ=5, κ=49, γ=137 984). This
is the exact form of the paper's §3.2 design target ("the average number of
restarts in Sign and PreSign is about e < 3").

The tolerance is statistical, not arbitrary: attempts/call over `calls`
i.i.d. geometric draws has SD `E·√(1−1/E) ≈ 2.16`, and the gate allows
`±5·SD/√calls` — about **±1%** at the Criterion run's ≥ 100 k calls (tight:
even the nearest realistic defect, the PreSign bound loosened by one, shifts
the expectation by ~2% ≈ 9σ and is caught) and about **±8%** at the driver's
2 500 calls (coarse gross-breakage check). Expected log line:

```text
rejection gate [Algorithm 2 PreSign]: 135300 calls, measured 2.7761 attempts/call (acceptance 36.02%) vs theory 2.7748 (36.04%), 5-sigma tolerance +-0.0294 => OK
```

Note the two theory values differ **by design** (the `−1` tighter PreSign
bound); the gate therefore also re-confirms, on every run, that the two paths
really run at their distinct paper bounds.

## Comparing two runs (Criterion baselines)

Criterion can diff runs with a significance test — use this before/after any
code change instead of eyeballing two logs:

```sh
cargo bench --bench las_bench -- --save-baseline before
# ... change code ...
cargo bench --bench las_bench -- --baseline before
```

The second run prints the per-benchmark change and a verdict
(`No change in performance detected.` / `Performance has regressed.`).

The committed run saved its results as the named baseline **`criterion082`**,
so any future change can be significance-tested against it directly:
`cargo bench --bench las_bench -- --baseline criterion082`.

## Methodology parity with the C benchmark

Each Rust benchmark tool has a C counterpart that measures the same thing the
same way. There are **two** C drivers, matching the two Rust tools:

- `ref/test/bench_levels.c` ↔ `examples/bench_levels.rs` — the **protocol
  driver** (overhead %, rejection counters, component sizes, cost-attribution
  diagnostics), repeated-run mean ± SD;
- `ref/test/bench_criterion.c` ↔ `benches/las_bench.rs` — the **statistical
  micro-benchmark**, a **1:1 re-implementation of Criterion 0.8's method in C**:
  same 3 s warm-up, same 300-sample linear iteration ramp over a 60 s window
  (`SamplingMode::Linear`), same origin-regression slope + 100 000-resample
  bootstrap 95% CI of the mean, same operation set and order. This is the
  driver that closes the "C methodology ≠ Rust Criterion" gap: the C side no
  longer relies on a different repeated-run scheme for its statistical numbers.

| Methodology element | C protocol driver (`bench_levels.c`) | C Criterion mirror (`bench_criterion.c`) | Rust protocol driver (`bench_levels.rs`) | Rust Criterion (`las_bench.rs`) |
| --- | --- | --- | --- | --- |
| Two independent modules (Algorithm 1 vs Algorithm 2) | `basesig.c` vs `las.c` | same | `basesig.rs` vs `las.rs` | same |
| Success-path contract asserted before timing | yes | yes | yes | yes |
| Pre-signature must FAIL ordinary Verify | asserted | asserted | asserted | asserted |
| Sign-class statistic includes rejection restarts | yes (mean) | yes (mean + bootstrap CI) | yes (mean) | yes (mean + bootstrap CI) |
| Rejection rate measured directly | `base_attempts` / `las_attempts` | same counters, over warm-up + all samples | `BASE_ATTEMPTS` / `LAS_ATTEMPTS` | same, read per run |
| Run-validity rejection gate (measured vs exact theory, 5σ) | coarse (±8% at 2 500 calls) | **tight (~±1% at ≥ 100 k calls)** | coarse (±8% at 2 500 calls) | tight (~±1% at ≥ 100 k calls) |
| Fixed pp seed (`00..1f`) + fixed 33-byte message | yes | yes | yes | yes |
| Sampling / repetition scheme | 5 reps × 500/1000 iters, mean ± SD | **warm-up + 300-sample linear ramp over 60 s** (Criterion method) | 5 reps × 500/1000 iters, mean ± SD | 300 samples over 60 s, warm-up, outliers, 95% CI |
| Point statistics reported | mean ± SD | mean ± SD, median, MAD, min/max, slope, bootstrap 95% CI | mean ± SD | mean, median, SD, bootstrap 95% CI |
| Evidence artefact | `evidence/runs/…` via `run_benchmark_suite.sh` | same run dir (`bench_criterion3.log`) | `bench_levels_rust.log` | `bench_las_criterion.log` + `target/criterion/` |

The only statistic Criterion's own Rust harness computes that the C mirror does
not is the per-*non-mean* bootstrap CIs (median/SD/slope), which live in
Criterion's HTML report and are not quoted in the report tables (those use
mean ± SD for sign-class and median for verify-class — both produced by the C
mirror). Everything that feeds a reported number is computed identically on
both sides.

One residual, honestly-documented difference: the **randomness source** for
KeyGen and the signing masks. The C scheme code draws it from the system RNG
inside `las.c`/`basesig.c`; the Rust drivers pass a fixed-seed ChaCha8. Both
sample the identical distributions (and the same pp/message bytes), so the
workloads are statistically equivalent — and the rejection gate verifies the
restart statistics of every run on both sides. Making the C side literally
seedable would require modifying scheme code, which the provenance rules
forbid for benchmarking convenience.

## Troubleshooting (WSL2 / laptops)

- **High variance / wide confidence intervals:** close background processes,
  plug the laptop in, re-run. Criterion's warm-up absorbs cold caches but not
  a busy CPU.
- **Sign-class benchmarks run past the 60 s target:** harmless — Criterion
  extends the window to keep all 300 samples; results remain valid.
- **Comparing two of your own runs:** use the baseline workflow above;
  Criterion applies the significance test for you.
- **Numbers differ from the committed logs:** expected across machines *and
  across power states* — the two committed Rust logs themselves differ ≈ 2.5×
  in absolute time between 2026-07-03 and 2026-07-05, while the overhead
  ratios agree. Check the *ratios* and the ≈ e attempts/signature (from the
  driver), never the microseconds.

## Scope

This document stops at the Algorithm 1 vs Algorithm 2 comparison. Atomic
swap, PCN/AMHL, `presign_k`, the classical-adaptor baseline and EVM gas are
later-stage work, out of scope here (see `docs/A-appendix/REPRODUCE_LAS_RUST.md`).
