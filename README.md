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

Each invocation builds and runs the benchmarks/tests, then calls
`scripts/plot_las_benchmarks.py`, and writes **everything for that run** into a single
**self-contained, timestamped** folder:

```text
evidence/runs/YYYYMMDD_HHMMSS/
  metadata.txt                 # timestamp, git commit/branch/status, compiler, OS/WSL, CPU, command list
  MANIFEST.md                  # human index of this folder
  functional_tests.log         # test_las3
  contract.log                 # test_contract3
  serialization_tests.log      # test_serde3
  kat.log                      # test_kat3
  atomic_swap.log              # test_swap3
  pcn.log                      # test_pcn3
  fair_paper.log fair_l2.log fair_l3.log fair_l5.log   # PRIMARY fair benchmark
  application_benchmark.log    # bench_app3 (L3-like)
  *.csv                        # parsed tables (see §3.2.1)
  *.png  *.pdf                 # report-quality figures (see §3.2.1)
  report_figure_manifest.csv   # which outputs are main / appendix / table-only
```

- **Old runs are never overwritten or deleted.** `evidence/latest` is repointed at the
  newest run (symlink, or a copy where symlinks are unavailable).
- Logs, CSVs, plots, metadata and the manifest are **all** stored directly inside the
  run folder — there are no `logs/`, `tables/` or `figures/` subfolders, and nothing is
  written to repo-level `tables/benchmark`, `figures/benchmark`, or `evidence/final-runs`.
- **Application-benchmark plotting is included** whenever `application_benchmark.log` is
  present; if it is missing or unparseable the suite still produces the fair-benchmark
  CSVs/figures and prints a clear warning.
- The bench_levels binaries are compiled with the git commit/branch baked in (via
  `REPRO_FLAGS`), so each `fair_*.log` carries its own provenance line.

The two earlier scripts `scripts/run_fair_benchmarks.sh` and
`scripts/run_final_evidence.sh` are **deprecated stubs** that print
`Deprecated: use scripts/run_benchmark_suite.sh`.

### 2.1 Re-plot an existing run (no rebuild)

```sh
python3 scripts/plot_las_benchmarks.py --input-dir evidence/latest --output-dir evidence/latest
#   (or point --input-dir at any evidence/runs/<timestamp>/)
```

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
  the statement/lock `Y` folded into the hash (`c = H(pk, w + Y, M)`).

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
witness `r' (=y_witness)`, signing mask `y_mask`, commitment `w=A·y_mask` (computed and
hashed into `c`, **not transmitted**), pre-signature response `z_hat`, final response `z`,
`Ext / Extract` `s = z − z_hat`. The base path and LAS path are reported as two separate
protocols and are never summed together. `π` (the paper's off-chain proof of
well-formedness) is **not** implemented or measured here.

#### 3.2.1 Report-ready figures and tables

`scripts/plot_las_benchmarks.py` (invoked by the suite runner) parses the run folder's
`fair_*.log` and, if present, `application_benchmark.log`, cross-validates the invariants
(`signature = c + z`; `pre-signature = final adapted signature = signature`;
`off-chain = Y + 2·pre-signature`; `settlement = 2·signature`; `M = n + ℓ`; the printed
`z`% matches `z/signature`; and the application off-chain/settlement totals) and **fails
loudly** if a fair value is missing or inconsistent. It deliberately favours **quality
over quantity**: a small set of clean, report-quality figures, with the crowded
percentage/catalogue/rejection charts kept as **CSV tables only**. Everything is written
directly into the same run folder.

Main-report figures (PNG + PDF):

| Figure | Supports the report claim |
|---|---|
| `timing_timeline_base_vs_las` | Base and LAS protocol cost timelines across paper/L2/L3/L5 — two **separate** protocols shown side by side, **not** summed. |
| `protocol_step_timeline` | Cumulative cost per **named** protocol step (Sign/PreSign → Verify/PreVerify → Adapt → Ext·Extract), one panel per level. Shows the full LAS adaptor cycle is only ≈ +27% to +33% over base Sign+Verify and is dominated by the shared rejection-sampling Sign/PreSign step. |
| `timing_overhead_clean` | Adaptor overhead is small: PreSign≈Sign, PreVerify≈Verify, Adapt≈Verify (error bars = SD, % above bars; Ext/Extract shown separately). Two panels (split y-scales). Headline L3. |
| `computation_component_absolute` | Where LAS compute time goes (horizontal, sorted). Titled **“diagnostic component attribution, not full protocol percentage.”** Headline L3. |
| `communication_summary_clean` | Signature/pre-signature/off-chain/settlement byte sizes with exact byte labels; `z`/`z_hat` dominates the signature. Byte-level only, **not** EVM gas. Headline L3. |
| `application_atomic_swap_payload_breakdown` | Atomic-swap payload story: off-chain (`Y + 2·pre-sig`) vs settlement (`2·sig`). **L3-like** (`bench_app3` only). |
| `application_multihop_payload_vs_k` | Multi-hop settlement payload grows linearly in path length K. **L3-like**; only generated when the multi-hop K-series exists. |

Appendix figure: `application_multihop_presign_time_vs_k` (pre-sign time per route vs K, L3-like).

Tables only (no figure by default): `primary_timing.csv`, `adaptor_overhead.csv`,
`rejection_sampling.csv` (avg/accept%/min/max/p50/p95), `communication_components.csv`
(detailed c/z %), `computation_components.csv`, `las_object_catalogue.csv`,
`application_atomic_swap.csv`, `application_multihop_amhl.csv`,
`application_payload_breakdown.csv`. `report_figure_manifest.csv` records, for every
output, whether it is main / appendix / table-only, the claim it supports, and a caution
note. Colours are fixed per series (Base Sign light blue / LAS PreSign dark blue; Base
Verify light orange / LAS PreVerify dark orange; Adapt purple; Ext/Extract green; `c` pale
red, `z` dark red; `Y/t'` teal; witness `r'` green; commitment `w` grey).

**Regeneration policy.** Logs, CSVs and figures only need regenerating when the benchmark
**stdout changes**. The `bench_levels` labels/notation changed in this round, so any older
`fair_*.log` are stale and **must be regenerated** by re-running
`scripts/run_benchmark_suite.sh` (which re-plots automatically). No log or number is ever
edited by hand.

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
| Functional correctness (mode 3) | `functional_tests.log` |
| Correctness contract (8-point) | `contract.log` |
| Serialization / tamper / malformed | `serialization_tests.log` |
| Known-answer test (deterministic) | `kat.log` |
| Atomic-swap narration | `atomic_swap.log` |
| Scriptless-ledger demos | `pcn.log` |
| Adaptor overhead (paper / L2 / L3 / L5) | `fair_paper.log`, `fair_l2.log`, `fair_l3.log`, `fair_l5.log` |
| Primary timing / overhead / rejection / communication / components | `primary_timing.csv`, `adaptor_overhead.csv`, `rejection_sampling.csv`, `communication_components.csv`, `computation_components.csv`, `las_object_catalogue.csv` |
| Application payloads + multi-hop (L3-like) | `application_benchmark.log`, `application_atomic_swap.csv`, `application_payload_breakdown.csv`, `application_multihop_amhl.csv` |
| Report-quality figures (PNG+PDF) | `timing_timeline_base_vs_las`, `timing_overhead_clean`, `computation_component_absolute`, `communication_summary_clean`, `application_atomic_swap_payload_breakdown`, `application_multihop_payload_vs_k` |
| Figure routing (main / appendix / table-only) | `report_figure_manifest.csv` |
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
scripts/run_benchmark_suite.sh   the one supported evidence pipeline (build + run + plot)
scripts/plot_las_benchmarks.py   parse logs -> CSVs + report-quality figures + manifest
evidence/runs/<timestamp>/        one self-contained folder per run (logs + CSVs + figures + metadata)
evidence/latest                   pointer to the newest run (older runs are kept)
```

## 7. Further documentation
| File | Contents |
|---|---|
| [docs/LAS_WALKTHROUGH.md](docs/LAS_WALKTHROUGH.md) | Plain-English, end-to-end explainer |
| [docs/LAS.md](docs/LAS.md) | Full design / implementation / evaluation write-up |
| [docs/THEORY_IMPL_BRIDGE.md](docs/THEORY_IMPL_BRIDGE.md) | Each construction equation → C function |
| [docs/STATUS.md](docs/STATUS.md) | Deliverable / test checklist |
