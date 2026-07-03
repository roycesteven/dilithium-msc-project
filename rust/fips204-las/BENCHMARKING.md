# Benchmarking guide — Algorithm 1 vs Algorithm 2 (Rust port)

How to run, read, and reproduce the Stage-1 benchmarks of this crate:
the ordinary lattice-based signature (Algorithm 1, `src/las_basesig.rs`)
versus the LAS adaptor signature (Algorithm 2, `src/las.rs`), both at the
Simplified Dilithium-III setting `(n=6, ℓ=5, κ=49)`.

## TL;DR — three commands

```sh
cd rust/fips204-las
cargo test --lib --tests                      # 1. gate: never benchmark unverified code
cargo bench --bench las_bench                 # 2. standard statistical micro-benchmark (Criterion.rs)
cargo run --release --example bench_levels    # 3. protocol driver: overhead % + rejection counters
```

Where the results appear:

| Output | Location |
| --- | --- |
| Criterion terminal summary (confidence intervals, outliers) | printed during step 2 |
| Saved raw log of the committed Criterion run | `bench_las_criterion.log` (this directory) |
| Machine-readable estimates (for baseline diffing) | `target/criterion/<group>/<benchmark>/new/estimates.json` |
| Protocol-driver table + overhead summary | printed during step 3; committed run: `bench_levels_rust.log` |

Note: Criterion 0.4 (the version the upstream crate pins, held back for its
MSRV 1.70 policy) ships with HTML report generation **disabled by default**
(`html_reports` feature). We deliberately leave the upstream manifest
untouched, so there is no `target/criterion/report/index.html` — the terminal
output, the saved log and the baseline workflow below are the deliverables.

## The two benchmark tools, and why both exist

| Tool | File | What it answers | What it cannot do |
| --- | --- | --- | --- |
| Criterion.rs micro-benchmark | `benches/las_bench.rs` | Statistically defensible per-operation time: warm-up, automatic iteration tuning, 100 samples, outlier classification, bootstrap 95% confidence interval, significance-tested baseline diffing | Does not compute the Algorithm-1-vs-Algorithm-2 overhead summary, and cannot read the rejection-restart counters |
| Protocol driver | `examples/bench_levels.rs` | The overhead summary (PreSign vs Sign, PreVerify vs Verify, Adapt vs Verify), measured attempts/signature from `BASE_ATTEMPTS`/`LAS_ATTEMPTS`, and the per-attempt (rejection-normalised) diagnostic | No confidence intervals or outlier analysis — it reports mean ± SD over 5 repetitions |

They are complementary: **quote per-operation times from Criterion; quote the
overhead percentages and rejection rates from the protocol driver.** Both time
the same functions from the two separate modules on a state gated by the same
success-path contract.

## Why Criterion.rs — mapped to the supervisor's benchmark requirements

Criterion.rs is the de-facto standard statistical benchmark harness of the
Rust ecosystem, and specifically of Rust cryptography. The decisive fact:
**the upstream `fips204` crate itself benchmarks with Criterion**
(`benches/benchmark.rs`, the same `criterion = "0.4.0"` dev-dependency), so
the LAS measurements use exactly the methodology of the base crate's author.

Requirement-by-requirement (`las-context-consolidated.md`):

| Supervisor requirement | How this setup meets it |
| --- | --- |
| §7 [Meeting 2]: ≥ 1000 iterations per operation, median + standard deviation, same machine and build flags | Criterion auto-tunes iterations: this run used 5 050 (Sign, PreSign) up to 76 000 (Extract) iterations per operation; it reports mean, median and SD; one machine, one `[profile.bench]` |
| §13.3 [Meeting 3]: ≥ 5 repetitions, mean with variance/SD, never single shots | 100 samples per operation, Tukey outlier classification, bootstrap 95% confidence interval of the mean |
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

- Rust toolchain ≥ 1.70; the committed run used `rustc 1.96.0`. No other
  system dependency.
- A quiet machine: close heavy background processes; on laptops, plug in
  (frequency scaling inflates variance). See Troubleshooting for WSL2.

## Step-by-step reproduction

### Step 1 — Verify before you measure

```sh
cd rust/fips204-las
cargo test --test las_kat -- --nocapture   # pinned digest 641a176c…5a19 == C value
cargo test --lib --tests                   # upstream suite + interlock + serde tests
```

If the KAT digest does not match, stop: the port is broken and any timing of
it is meaningless.

### Step 2 — Run the Criterion benchmark

```sh
cargo bench --bench las_bench 2>&1 | tee bench_las_criterion.log
```

This builds with the crate's `[profile.bench]` (`opt-level = 3`, `lto`,
`codegen-units = 1`) and runs seven benchmarks in two groups:

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
                        time:   [1.0973 ms 1.1276 ms 1.1609 ms]
Found 1 outliers among 100 measurements (1.00%)
```

- `time: [low estimate high]` is the **bootstrap 95% confidence interval of
  the mean time per call**; quote the middle value as the point estimate.
- The outlier line is Criterion's Tukey classification of the 100 samples —
  see "How to read the numbers" for why sign-class outliers are expected.
- The warning `Unable to complete 100 samples in 5.0s` (appears for Sign and
  PreSign) is harmless: Criterion kept 100 samples and simply extended the
  measurement window (~5.8 s).

### Step 4 — Run the protocol driver for overheads and rejection counters

```sh
cargo run --release --example bench_levels | tee bench_levels_rust.log
```

This prints the per-operation table (5 repetitions, mean ± SD), the
Algorithm-1-vs-Algorithm-2 overhead percentages, the measured
attempts/signature on both paths, and the per-attempt diagnostic.

## Measured results (committed run)

Measured 2026-07-03 on WSL2 / AMD Ryzen 7 7745HX, `rustc 1.96.0`,
`[profile.bench]` (`opt-level = 3`, `lto`, `codegen-units = 1`); raw log:
`bench_las_criterion.log`. Values are the bootstrap 95% confidence interval
of the mean, `[low · point estimate · high]`:

| Operation | Time (µs) [low · point · high] | Iterations |
| --- | --- | --- |
| Algorithm 1 — KeyGen | 111.01 · 111.57 · 112.20 | 45 000 |
| Algorithm 1 — Sign | 1 088.5 · 1 113.5 · 1 137.3 | 5 050 |
| Algorithm 1 — Verify | 201.95 · 202.33 · 202.80 | 25 000 |
| Algorithm 2 — PreSign | 1 097.3 · 1 127.6 · 1 160.9 | 5 050 |
| Algorithm 2 — PreVerify | 203.19 · 204.22 · 205.30 | 25 000 |
| Algorithm 2 — Adapt (including its internal PreVerify) | 210.29 · 212.67 · 215.43 | 25 000 |
| Algorithm 2 — Extract | 69.22 · 70.25 · 71.48 | 76 000 |

Point-estimate overheads: PreSign vs Sign **+1.3%** (the two confidence
intervals overlap — statistically indistinguishable), PreVerify vs Verify
**+0.9%**, Adapt vs Verify **+5.1%**; Extract (≈ 70 µs) has no Algorithm-1
analogue. This agrees with the protocol driver's run (+3.9% / −1.2% / +0.4%,
`bench_levels_rust.log`) and with the C headline (+6.7% / +3.1% / +8.1%,
`evidence/latest/`): **the adaptor layer costs at most single-digit percent
per operation**, and the verify-class differences sit inside the noise.

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

## Methodology parity with the C benchmark

The Rust benchmarks deliberately mirror the C driver so both languages
measure the same thing the same way:

| Methodology element | C (`ref/test/bench_levels.c`) | Rust protocol driver (`examples/bench_levels.rs`) | Rust Criterion (`benches/las_bench.rs`) |
| --- | --- | --- | --- |
| Two independent modules (Algorithm 1 vs Algorithm 2) | `basesig.c` vs `las.c` | `las_basesig.rs` vs `las.rs` | same modules |
| Success-path contract asserted before timing | yes | yes | yes |
| Pre-signature must FAIL ordinary Verify | asserted | asserted | asserted |
| Sign-class statistic includes rejection restarts | yes (mean) | yes (mean) | yes (mean, with bootstrap confidence interval) |
| Rejection rate measured directly | `base_attempts` / `las_attempts` counters | `BASE_ATTEMPTS` / `LAS_ATTEMPTS` counters | not applicable — use the driver |
| Per-attempt (rejection-normalised) diagnostic | yes | yes | not applicable |
| Repetition scheme | 10 runs × 1000 iterations, mean ± SD | 5 repetitions × 500/1000 iterations, mean ± SD | 100 samples, warm-up, outlier classification, 95% confidence interval |
| Evidence artefact | `evidence/runs/…` via `run_benchmark_suite.sh` | `bench_levels_rust.log` | `bench_las_criterion.log` + `target/criterion/` |

What Criterion adds on the Rust side (warm-up, outlier classification,
bootstrap confidence intervals, significance-tested baseline diffing) is a
strictly stronger statistical layer on top of the same protocol methodology —
it does not change *what* is measured.

## Troubleshooting (WSL2 / laptops)

- **High variance / wide confidence intervals:** close background processes,
  plug the laptop in, re-run. Criterion's warm-up absorbs cold caches but not
  a busy CPU.
- **`Unable to complete 100 samples in 5.0s` warning:** harmless — Criterion
  extends the measurement window and keeps 100 samples; results remain valid.
- **Comparing two of your own runs:** use the baseline workflow above;
  Criterion applies the significance test for you.
- **Numbers differ from the committed logs:** expected across machines. Check
  that the *overhead ratios* and the ≈ e attempts/signature (from the driver)
  match, not the microseconds.

## Scope

This document stops at the Algorithm 1 vs Algorithm 2 comparison. Atomic
swap, PCN/AMHL, `presign_k`, the classical-adaptor baseline and EVM gas are
later-stage work, out of scope here (see `docs/REPRODUCE_LAS_RUST.md`).
