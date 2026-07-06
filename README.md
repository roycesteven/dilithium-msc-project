# LAS on Dilithium — build, run, and reproduce the results

Post-quantum **Lattice-based Adaptor Signature** (LAS; Esgin, Ersoy, Erkin, IACR
eprint 2020/845, Algorithm 2 — the *simplified* scheme) implemented on the
CRYSTALS-Dilithium reference primitives, with a scriptless atomic-swap / payment-channel
demonstration.

This file is the reproducibility entry point. Every table reported in the dissertation
is produced by one command below, and each command writes its full terminal output to a
file under `evidence/`. The intent is that anyone can re-run the commands and obtain the
same logs, and that each report table can be traced to one log file.

---

## 1. Environment

- **Base code:** CRYSTALS-Dilithium / ML-DSA (FIPS 204) reference C implementation. The
  lattice core (polynomial arithmetic, NTT, SHAKE, sampling) is reused unchanged; the
  LAS scheme, serialization, demos, and benchmarks are added on top.
- **Toolchain used for the reported numbers:** `gcc` (Ubuntu 13.3.0), `GNU Make 4.3`,
  Linux (Ubuntu 24.04, WSL2), AMD Ryzen 7 7745HX. Compiled at `-O3` under
  `-Wall -Wextra -Wpedantic -Wmissing-prototypes -Wredundant-decls -Wshadow -Wvla
  -Wpointer-arith` with no warnings.
- **Modulus:** this build uses Dilithium's `q = 8380417 ≈ 2²³` (the reused NTT table),
  not the construction's `q ≈ 2²⁴`. Since `q > 2γ`, correctness holds; only the concrete
  security margin differs. See `docs/LAS.md §5.9`.

### Prerequisites
- A C compiler (`gcc` or `clang`) and `make`. Nothing else for the core results.
- *Optional, only for the classical baseline:* a one-time clone of `secp256k1-zkp`
  (commands below).
- *Optional, only for the on-chain gas figure:* [Foundry](https://book.getfoundry.sh)
  (`forge`).

---

## 2. Quick start — the one supported evidence pipeline

There is **one** supported pipeline. From the repository root:

```sh
bash scripts/run_benchmark_suite.sh
```

Each invocation builds and runs the benchmarks/tests, then runs the **two plotting
stages** — `scripts/plot_las_benchmarks.py` (logs → CSV tables + all figures) followed by
`scripts/plot_las_paper_figures.py` (the curated Stage-1 *paper package*) — and writes
**everything for that run** into a single **self-contained, timestamped** folder whose
outputs are organised into subfolders:

```text
evidence/runs/YYYYMMDD_HHMMSS/
  metadata.txt                 # timestamp, git commit/branch/status, compiler, OS/WSL, CPU, command list
  MANIFEST.md                  # human index of this folder
  logs/                        # raw benchmark/test stdout (never hand-edited)
    functional_tests.log contract.log serialization_tests.log kat.log
    atomic_swap.log pcn.log
    fair_paper.log fair_l2.log fair_l3.log fair_l5.log   # PRIMARY fair benchmark
    application_benchmark.log                              # bench_app3 (L3-like)
    classical.log                                         # only if secp256k1-zkp is present
  tables/                      # CSV tables parsed from the logs (+ report_figure_manifest.csv)
  paper_package/               # MAIN Stage-1 package to show the supervisor:
    parameter_sets_paper.tex                  #   Table 1  (parameter settings)
    per_operation_timing_paper.pdf/.png       #   Figure 1 (computation: ordinary vs LAS)
    communication_components_paper.pdf/.png   #   Figure 2 (communication component sizes)
    KEY_FINDINGS.md  paper_figure_manifest.csv  README.md
  appendix_package/            # supporting figures (NOT main, NOT a security comparison):
    adaptor_overhead_paper.pdf/.png           #   multi-setting overhead sweep (scaling context)
    rejection_sampling_paper.pdf/.png         #   acceptance per attempt (~1/e)
  debug_figures/               # cumulative-timing, component-attribution, *_report duplicates, parameter PNGs
  application_package/          # Stage-2 atomic-swap / AMHL multi-hop figures
```

- **Old runs are never overwritten or deleted.** `evidence/latest` is repointed at the
  newest run (symlink, or a copy where symlinks are unavailable).
- Everything for a run lives inside that one run folder, organised into the subfolders
  above; nothing is written to repo-level `tables/benchmark`, `figures/benchmark`, or
  `evidence/final-runs`. `paper_package/` is the curated Stage-1 set to show the
  supervisor; `appendix_package/`, `debug_figures/`, `application_package/`, `tables/`
  and `logs/` are appendix / diagnostic / evidence.
- **Application-benchmark plotting is included** whenever `application_benchmark.log` is
  present; if it is missing or unparseable the suite still produces the fair-benchmark
  CSVs/figures and prints a clear warning.
- The bench_levels binaries are compiled with the git commit/branch baked in (via
  `REPRO_FLAGS`), so each `fair_*.log` carries its own provenance line.

The two earlier scripts `scripts/run_fair_benchmarks.sh` and
`scripts/run_final_evidence.sh` are **deprecated stubs** that print
`Deprecated: use scripts/run_benchmark_suite.sh`.

### 2.1 Re-plot an existing run (no rebuild)

Re-plotting reads the run's `logs/` and regenerates the CSV tables and figures (no
benchmark is re-run). Stage 1 produces the CSV tables; stage 2 produces the curated paper
package from those tables:

```sh
# stage 1: logs -> CSV tables (+ all figures, written into the chosen dir)
python3 scripts/plot_las_benchmarks.py \
    --input-dir evidence/latest/logs --output-dir evidence/latest/tables
# stage 2: CSV tables -> curated paper package + appendix
python3 scripts/plot_las_paper_figures.py --input-dir evidence/latest/tables \
    --output-dir evidence/latest/paper_package --appendix-dir evidence/latest/appendix_package
```

(`run_benchmark_suite.sh` does this organisation automatically; the manual commands above
write a flat figure set into each chosen output dir. Point `--input-dir` at any
`evidence/runs/<timestamp>/logs`.)

### 2.2 Extra targets not in the core suite

These remain available for portability checks and the optional baselines, and are not
part of the self-contained run folder:

```sh
cd ref
make test/test_las2 test/test_las5                 # functional tests at other levels
make test/test_serde_l2 test/test_serde_l3 test/test_serde_l5
make test/test_basesig_paper test/test_basesig2 test/test_basesig3 test/test_basesig5
make test/test_amhl3                                # distinct-statement AMHL demo
```

`make clean` removes the built binaries.

---

## 3. What each artefact demonstrates

### 3.1 Correctness

| Command | Saves to | What it proves |
|---|---|---|
| `./test/test_las{2,3,5}` | `functional_tests.log` | 1000 iterations per parameter set of the full adaptor cycle: PreSign→PreVerify accepts, PreSign fails ordinary Verify, Adapt→Verify accepts, Extract recovers the witness exactly; plus a one-bit forgery is rejected. |
| `./test/test_basesig{_paper,2,3,5}` | `base_signature_tests.log` | 1000 iterations per parameter set of the **separate** base-signature module (`basesig.c`): honest verify; tamper / wrong-key / wrong-statement `Y` / wrong-witness / tampered pre-signature / tampered adapted-signature all rejected; cross-module equivalence with `las.c`; and the adaptor interlock (a LAS-adapted signature verifies under the independent base verifier, a pre-signature does not). |
| `./test/test_serde3` | `serialization_tests.log` | Byte encoding round-trips; all 4672 single-byte flips of a packed signature are rejected; the validating decoder rejects malformed bytes. Also run across the D2/D3/D5-aligned sets (`test_serde_l2/l3/l5`); the packed `z` width is parameter-derived (18 bits at paper/D2, 19 at D3/D5). |
| `./test/test_kat3` | `kat.log` | Deterministic known-answer test: a single SHAKE256 digest pins keygen+sign+presign+adapt+serialization over fixed vectors. |
| `./test/test_contract3` | `contract.log` | One harness that prints the eight-point adaptor correctness contract as labelled PASS lines. |

**What you should see:** `test_las3` reports `1000/1000 iterations (100% correctness)`;
each `test_basesig*` ends with `test_basesig: ALL CHECKS PASSED`;
`test_kat3` prints the digest
`641a176c3eb2125098fdbb7ad16bfa38fb5744b52dd9696beeb7d07be1445a19`;
`test_contract3` ends with `ALL CONTRACT CHECKS PASSED`.

### 3.2 Primary fair benchmark — base signature path vs LAS adaptor path

`bench_levels` measures the **adaptor overhead** by timing two **separate modules** at
the same parameters and on the same primitives:

- **base path** — `ref/basesig.c` (`base_keygen`/`base_sign`/`base_verify`): the
  simplified Dilithium-style signature, `Sign` hashing `c = H(pk, w, M)` and `Verify`
  recomputing `c = H(pk, w', M)`, with **no adaptor statement `Y`**;
- **adaptor path** — `ref/las.c` (PreSign/PreVerify/Adapt/Extract): the same scheme with
  the statement/lock `Y` folded into the hash (`c = H(pk, w + t', M)`, `t' := Y`).

`basesig.c` is deliberately kept out of `las.{c,h}` (the LAS protocol is untouched); it
shares only `las.h`'s parameter macros and key/signature struct layout, so both schemes
use the same parameter set (a dimension-level match — `n,ℓ,κ` — not a formal bit-security
claim). The benchmark pairs each adaptor operation with the base
operation it mirrors (PreSign vs Sign, PreVerify vs Verify, Adapt vs Verify; Extract
reported separately), checks the cross-path contract (a LAS-adapted signature verifies
under the independent `base_verify`), and prints the component-level packed sizes.

`bench_levels` is the **single combined driver**: each run establishes **one** setup and
**one** consistent state (one `las_setup`, one key pair, one statement/witness, one
sig/pre-sig/adapted), gates all timing on the success-path contract, then prints the
**PRIMARY** protocol-level timings (Setup, KeyGen, Sign, Verify, PreSign, PreVerify,
Adapt, Ext) **first**, followed by **diagnostic** sections drawn from that same state:
(A) rejection-sampling distribution read directly off `base_attempts`/`las_attempts`
(avg, accept %, min, max, p50, p95); (B) Adapt timing clarification — "Adapt checked
total" (the protocol Adapt) vs the diagnostic-only "witness-add only" lower bound;
(C) communication-derived packed byte sizes/ratios and the byte-level atomic-swap
payload (**byte-level only, not EVM gas**); and (D) operation-level component
microbenchmarks (cost-attribution estimates over local copies of the inner steps — **not**
the protocol entry points). Macro and micro numbers therefore come from one executable
and one state, so they cannot drift apart.

| Command | Saves to | Parameter set |
|---|---|---|
| `./test/bench_levels_paper` | `fair_paper.log` | original LAS dimensions (n=ℓ=4, κ=60) |
| `./test/bench_levels2` | `fair_l2.log` | Dilithium-Level-2-aligned (n=ℓ=4, κ=39) |
| `./test/bench_levels3` | `fair_l3.log` | Dilithium-Level-3-aligned (n=6, ℓ=5, κ=49) |
| `./test/bench_levels5` | `fair_l5.log` | Dilithium-Level-5-aligned (n=8, ℓ=7, κ=60) |

**What you should see:** under `COMPUTATION`, each adaptor operation is within a few
percent of its base analogue (the `Adaptor overhead` pairings); under `COMMUNICATION`,
the response `z` is 98.6–99.3% of the signature.

The output uses paper notation throughout: `pp=(A,H)`, `pk=t`, `sk=r`, statement `Y=t'`,
witness `r'` (the paper's Algorithm-2 symbol; in the statement–witness pair `(Y, y)` it is
the `y`), masking randomness `y`, commitment `w=A·y` (computed and hashed into `c`,
**not transmitted**), pre-signature response `ẑ` (ASCII `z_hat` in code/CSV), final
response `z`, and `Ext` recovering `s = z − ẑ`. The base path and LAS path are reported as
two separate protocols and are never summed together. `π` (the paper's off-chain proof of
well-formedness) is **not** implemented or measured here.

#### 3.2.1 Report-ready figures and tables

The plotting is **two stages**. `scripts/plot_las_benchmarks.py` parses the run's
`logs/fair_*.log` and, if present, `logs/application_benchmark.log`, cross-validates the
invariants (`signature = c + z`; `pre-signature = adapted signature = signature`;
`off-chain = Y + 2·pre-signature`; `settlement = 2·signature`; `M = n + ℓ`; the printed
`z`% matches `z/signature`; and the application totals) and **fails loudly** if a fair
value is missing or inconsistent. It writes the CSV tables into `tables/` and the full set
of figures into `debug_figures/` and `application_package/`. Then
`scripts/plot_las_paper_figures.py` reads only those CSV tables and writes the small,
curated **paper package** (the research-question comparison: ordinary signature vs LAS
adaptor), favouring **quality over quantity**.

Main paper figures (`paper_package/`, PNG + PDF; the set to show the supervisor):

| Output | Supports the report claim |
|---|---|
| `parameter_sets_paper.tex` | Table 1 — parameter settings (n, ℓ, M, κ, γ, N, Q); engineering settings, **not** NIST levels. |
| `per_operation_timing_paper` | Figure 1 — per-operation **computation**: ordinary signature (KeyGen/Sign/Verify) vs LAS adaptor (PreSign/PreVerify/Adapt/Ext), single headline setting, mean ± SD. |
| `communication_components_paper` | Figure 2 — **communication** component sizes (pk, sk, Y, witness, c, z/ẑ, and the three equal-size signatures); LAS adds the statement Y, the signature does not grow. |

Appendix figures (`appendix_package/`): `adaptor_overhead_paper` (multi-setting overhead
sweep — scaling context, **not** a security-parameter comparison) and
`rejection_sampling_paper` (acceptance ≈ 1/e).

Diagnostic / supporting figures (`debug_figures/`): cumulative timelines
(`timing_timeline_base_vs_las`, `protocol_step_timeline`), overhead pairs
(`timing_overhead_clean`), component attribution (`computation_component_absolute`,
`component_scaling_vs_level`, `verify_ext_attribution_vs_level`), the payload summary
(`communication_summary_clean`), and the `*_report` LaTeX-clean duplicates. Stage-2
application figures live in `application_package/`
(`application_atomic_swap_payload_breakdown`, `application_multihop_payload_vs_k`,
`application_multihop_presign_time_vs_k`, `application_multihop_norm_vs_k`).

Tables (`tables/`): `primary_timing.csv`, `adaptor_overhead.csv`, `rejection_sampling.csv`
(avg/accept%/min/max/p50/p95), `communication_components.csv`, `computation_components.csv`,
`las_object_catalogue.csv`, `parameter_sets.csv`, the `application_*.csv`, and
`report_figure_manifest.csv` (every output's role: main / appendix / table-only);
`paper_package/paper_figure_manifest.csv` does the same for the paper package. Colours are
fixed per series (Base Sign light blue / LAS PreSign dark blue; Base Verify light orange /
LAS PreVerify dark orange; Adapt purple; Ext green; `c` pale red, `z` dark red; `Y/t'`
teal; witness `r'` green; commitment `w` grey).

**Regeneration policy.** Logs, CSVs and figures only need regenerating when the benchmark
**stdout changes**. The `bench_levels` labels/notation changed in this round, so any older
`fair_*.log` are stale and **must be regenerated** by re-running
`scripts/run_benchmark_suite.sh` (which rebuilds, re-runs, and re-plots both stages). No
log or number is ever edited by hand.

### 3.3 Application — atomic swap and payment channels

| Command | Saves to | What it shows |
|---|---|---|
| `./test/test_swap3` | `atomic_swap.log` | Narrated two-party, two-chain atomic swap; asserts that publishing the adapted signature reveals the witness and that pre-signatures are unspendable. |
| `./test/test_pcn3` | `pcn.log` | Scriptless-ledger demos: cross-chain swap, timeout/refund, and a multi-hop payment. |
| `./test/bench_app3` | `application_benchmark.log` | Measured packed payloads: off-chain negotiation (Y + two pre-signatures), settlement (two adapted signatures, and the variant that also counts the two escrowed statements Y), and multi-hop AMHL cost as a function of path length K. **L3-like** (`bench_app3` is built at n=6, ℓ=5, κ=49); the suite parses it into `application_atomic_swap.csv`, `application_payload_breakdown.csv` and `application_multihop_amhl.csv`. |

**What you should see:** off-chain negotiation = `Y + 2·pre-signature`; settlement =
`2·signature` (with a larger variant that also counts the two escrowed statements `Y`);
and the multi-hop settlement footprint growing **linearly in K**. The exact byte figures
are L3-like and are written to the run folder's application CSVs (they are not hard-coded
here, since they depend on the parameter set the binary was built at).

### 3.4 Rejection-sampling rate

The combined `bench_levels` driver reports the rejection distribution (avg, accept %,
min, max, p50, p95 for Base Sign and LAS PreSign) in **diagnostic section A** of each
`fair_*.log`, measured directly from the `base_attempts`/`las_attempts` counters:
acceptance ≈ 37% per attempt (≈ 2.7 attempts/signature), matching the `e⁻¹` prediction.
The plotter collects this into `rejection_sampling.csv` (table only — no figure by
default). The older standalone `bench_las.c`/`bench_compare.c` benchmarks have been
**removed**: their outputs (`bench_las3` rejection rate, `bench_compare3` LAS-vs-Dilithium-3)
are **not** used as primary evidence.

---

## 4. Optional baselines

### 4.1 Classical adaptor signature (functionality-matched baseline)
```sh
git clone --depth 1 https://github.com/BlockstreamResearch/secp256k1-zkp \
    third_party/secp256k1-zkp        # tested at commit 95b9835
cd ref
make test/bench_classical && ./test/bench_classical 2>&1 | tee ../evidence/classical.log
```
A classical secp256k1 ECDSA adaptor signature with the same operation set as LAS,
measured on the same machine. It is the post-quantum-vs-classical reference, compared at
the closest classical security target (≈128-bit, aligned to Level 2).

### 4.2 On-chain verification cost (optional, needs Foundry)
```sh
cd ref && make test/export_packed && ./test/export_packed ../evm/test/las_sig.bin
cd ../evm && forge test --match-contract LASVerifyCost -vv 2>&1 | tee ../evidence/gas.log
```
Reports the gas to verify one packed LAS signature natively in a Solidity contract.

---

## 5. Report table → evidence file

All core evidence comes from **one** `scripts/run_benchmark_suite.sh` invocation, written
into `evidence/runs/<timestamp>/` (also reachable via `evidence/latest/`). Paths below are
relative to that run folder.

| Report table / result | File in the run folder |
|---|---|
| Functional correctness (mode 3) | `logs/functional_tests.log` |
| Correctness contract (8-point) | `logs/contract.log` |
| Serialization / tamper / malformed | `logs/serialization_tests.log` |
| Known-answer test (deterministic) | `logs/kat.log` |
| Atomic-swap narration | `logs/atomic_swap.log` |
| Scriptless-ledger demos | `logs/pcn.log` |
| Adaptor overhead (paper / L2 / L3 / L5) | `logs/fair_paper.log`, `logs/fair_l2.log`, `logs/fair_l3.log`, `logs/fair_l5.log` |
| Primary timing / overhead / rejection / communication / components | `tables/primary_timing.csv`, `tables/adaptor_overhead.csv`, `tables/rejection_sampling.csv`, `tables/communication_components.csv`, `tables/computation_components.csv`, `tables/las_object_catalogue.csv` |
| Application payloads + multi-hop (L3-like) | `logs/application_benchmark.log`, `tables/application_atomic_swap.csv`, `tables/application_payload_breakdown.csv`, `tables/application_multihop_amhl.csv` |
| MAIN paper figures (Table 1 + Fig 1 + Fig 2) | `paper_package/parameter_sets_paper.tex`, `paper_package/per_operation_timing_paper`, `paper_package/communication_components_paper` |
| Appendix figures | `appendix_package/adaptor_overhead_paper`, `appendix_package/rejection_sampling_paper` |
| Diagnostic / Stage-2 figures | `debug_figures/*`, `application_package/*` |
| Figure routing (main / appendix / table-only) | `tables/report_figure_manifest.csv`, `paper_package/paper_figure_manifest.csv` |
| Provenance / index | `metadata.txt`, `MANIFEST.md` |

Extra (not in the core suite — see §2.2, tee manually if needed): functional tests at
other levels, `test_serde_l2/l3/l5`, the base-signature module tests, the distinct-statement
AMHL demo.

| Optional baseline | Command | Evidence file |
|---|---|---|
| Classical adaptor comparison *(§4.1)* | `./test/bench_classical` | `evidence/classical.log` |
| On-chain verification gas *(§4.2)* | `forge test --match-contract LASVerifyCost` | `evidence/gas.log` |

The two *optional* rows require the one-time setup in §4 and are not part of the core
self-contained run folder.

---

## 6. Layout

```text
ref/las.{c,h}        LAS scheme (KeyGen/Sign/Verify + PreSign/PreVerify/Adapt/Ext)
ref/basesig.{c,h}    separate simplified Dilithium-style base signature (no Y; fair-benchmark baseline)
ref/serialize.{c,h}  byte-level encoding + validating decoder + las_verify_packed
ref/amhl.{c,h}       multi-hop locks
ref/chain.{c,h}      toy ledger for the swap / payment-channel demos
ref/test/            tests and benchmarks (bench_levels.c = combined fair driver; bench_app.c = application)
ref/{poly,ntt,reduce,fips202,...}.c   reused Dilithium primitives (unmodified)
scripts/run_benchmark_suite.sh    the one supported evidence pipeline (build + run + plot, both stages)
scripts/plot_las_benchmarks.py    stage 1: parse logs -> CSV tables + all figures + manifest
scripts/plot_las_paper_figures.py stage 2: CSV tables -> curated paper package (Table 1, Fig 1, Fig 2) + appendix
evidence/runs/<timestamp>/        one self-contained folder per run, organised into subfolders:
                                  logs/ tables/ paper_package/ appendix_package/ debug_figures/ application_package/
evidence/latest                   pointer to the newest run (older runs are kept)
```

## 7. Further documentation
| File | Contents |
|---|---|
| [docs/01-introduction/LAS_WALKTHROUGH.md](docs/01-introduction/LAS_WALKTHROUGH.md) | Plain-English, end-to-end explainer |
| [docs/LAS.md](docs/LAS.md) | Full design / implementation / evaluation write-up |
| [docs/02-methodology/THEORY_IMPL_BRIDGE.md](docs/02-methodology/THEORY_IMPL_BRIDGE.md) | Each construction equation → C function |
| [docs/STATUS.md](docs/STATUS.md) | Deliverable / test checklist |
