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

## 2. Quick start — reproduce everything

Run from the repository root. This builds every target, runs every test and benchmark,
and saves one log per artefact under `evidence/`.

```sh
mkdir -p evidence
cd ref

# --- correctness (functional, serialization, known-answer, full contract) ---
make test/test_las2 test/test_las3 test/test_las5
{ ./test/test_las2; ./test/test_las3; ./test/test_las5; } 2>&1 | tee ../evidence/functional_tests.log
make test/test_serde3   && ./test/test_serde3   2>&1 | tee ../evidence/serialization_tests.log
make test/test_kat3     && ./test/test_kat3     2>&1 | tee ../evidence/kat.log
make test/test_contract3 && ./test/test_contract3 2>&1 | tee ../evidence/contract.log

# --- application (atomic swap, scriptless ledger, payload/AMHL costs) ---
make test/test_swap3 && ./test/test_swap3 2>&1 | tee ../evidence/atomic_swap.log
make test/test_pcn3  && ./test/test_pcn3  2>&1 | tee ../evidence/pcn.log
make test/bench_app3 && ./test/bench_app3 2>&1 | tee ../evidence/application_benchmark.log

# --- primary fair benchmark: base path (basesig.c) vs LAS adaptor path (las.c) ---
make test/bench_levels_paper test/bench_levels2 test/bench_levels3 test/bench_levels5
./test/bench_levels_paper 2>&1 | tee ../evidence/fair_paper.log
./test/bench_levels2      2>&1 | tee ../evidence/fair_l2.log
./test/bench_levels3      2>&1 | tee ../evidence/fair_l3.log
./test/bench_levels5      2>&1 | tee ../evidence/fair_l5.log

# --- rejection-sampling acceptance rate (measured directly) ---
make test/bench_las3 && ./test/bench_las3 2>&1 | tee ../evidence/rejection_rate.log
```

`make clean` removes the built binaries.

---

## 3. What each artefact demonstrates

### 3.1 Correctness

| Command | Saves to | What it proves |
|---|---|---|
| `./test/test_las{2,3,5}` | `functional_tests.log` | 1000 iterations per parameter set of the full adaptor cycle: PreSign→PreVerify accepts, PreSign fails ordinary Verify, Adapt→Verify accepts, Extract recovers the witness exactly; plus a one-bit forgery is rejected. |
| `./test/test_serde3` | `serialization_tests.log` | Byte encoding round-trips; all 4672 single-byte flips of a packed signature are rejected; the validating decoder rejects malformed bytes. |
| `./test/test_kat3` | `kat.log` | Deterministic known-answer test: a single SHAKE256 digest pins keygen+sign+presign+adapt+serialization over fixed vectors. |
| `./test/test_contract3` | `contract.log` | One harness that prints the eight-point adaptor correctness contract as labelled PASS lines. |

**What you should see:** `test_las3` reports `1000/1000 iterations (100% correctness)`;
`test_kat3` prints the digest
`f7fc40f0b7752cafc083fcddd6a13759fbde9b2a2d538045cd0d62f87747e6b1`;
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
sit at the same security level. The benchmark pairs each adaptor operation with the base
operation it mirrors (PreSign vs Sign, PreVerify vs Verify, Adapt vs Verify; Extract
reported separately), checks the cross-path contract (a LAS-adapted signature verifies
under the independent `base_verify`), and prints the component-level packed sizes.

| Command | Saves to | Parameter set |
|---|---|---|
| `./test/bench_levels_paper` | `fair_paper.log` | original LAS dimensions (n=ℓ=4, κ=60) |
| `./test/bench_levels2` | `fair_l2.log` | Dilithium-Level-2-aligned (n=ℓ=4, κ=39) |
| `./test/bench_levels3` | `fair_l3.log` | Dilithium-Level-3-aligned (n=6, ℓ=5, κ=49) |
| `./test/bench_levels5` | `fair_l5.log` | Dilithium-Level-5-aligned (n=8, ℓ=7, κ=60) |

**What you should see:** under `COMPUTATION`, each adaptor operation is within a few
percent of its base analogue (the `Adaptor overhead` pairings); under `COMMUNICATION`,
the response `z` is 98.6–99.3% of the signature.

### 3.3 Application — atomic swap and payment channels

| Command | Saves to | What it shows |
|---|---|---|
| `./test/test_swap3` | `atomic_swap.log` | Narrated two-party, two-chain atomic swap; asserts that publishing the adapted signature reveals the witness and that pre-signatures are unspendable. |
| `./test/test_pcn3` | `pcn.log` | Scriptless-ledger demos: cross-chain swap, timeout/refund, and a multi-hop payment. |
| `./test/bench_app3` | `application_benchmark.log` | Measured packed payloads: off-chain negotiation (Y + two pre-signatures), settlement (two adapted signatures), and multi-hop cost as a function of path length K. |

**What you should see:** off-chain negotiation payload `12288 B`; settlement payload
`9344 B` (the two adapted signatures — the reported figure; `15232 B` if the two
escrowed statements are also counted); the multi-hop settlement footprint grows
linearly in K.

### 3.4 Rejection-sampling rate

`./test/bench_las3 → rejection_rate.log`: acceptance ≈ 37% per attempt (≈ 2.7
attempts/signature), matching the `e⁻¹` prediction.

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

## 5. Report table → command → evidence file

| Report table / result | Command | Evidence file |
|---|---|---|
| Functional correctness (modes 2/3/5) | `./test/test_las2`, `test_las3`, `test_las5` | `evidence/functional_tests.log` |
| Serialization / tamper / malformed | `./test/test_serde3` | `evidence/serialization_tests.log` |
| Known-answer test (deterministic) | `./test/test_kat3` | `evidence/kat.log` |
| Correctness contract (8-point) | `./test/test_contract3` | `evidence/contract.log` |
| Adaptor overhead (paper set) | `./test/bench_levels_paper` | `evidence/fair_paper.log` |
| Adaptor overhead across levels | `./test/bench_levels{2,3,5}` | `evidence/fair_l{2,3,5}.log` |
| Communication / component sizes | `./test/bench_levels_paper` | `evidence/fair_paper.log` |
| Atomic-swap narration | `./test/test_swap3` | `evidence/atomic_swap.log` |
| Scriptless-ledger demos | `./test/test_pcn3` | `evidence/pcn.log` |
| Atomic-swap payload | `./test/bench_app3` | `evidence/application_benchmark.log` |
| Multi-hop cost vs K | `./test/bench_app3` | `evidence/application_benchmark.log` |
| Rejection-sampling rate | `./test/bench_las3` | `evidence/rejection_rate.log` |
| Classical adaptor comparison *(optional, §4.1)* | `./test/bench_classical` | `evidence/classical.log` |
| On-chain verification gas *(optional, §4.2)* | `forge test --match-contract LASVerifyCost` | `evidence/gas.log` |

The two *optional* rows require the one-time setup in §4 and are not part of the core
evidence set; their logs (`classical.log`, `gas.log`) are produced only by running §4.1
and §4.2 respectively.

---

## 6. Layout

```
ref/las.{c,h}        LAS scheme (KeyGen/Sign/Verify + PreSign/PreVerify/Adapt/Ext)
ref/serialize.{c,h}  byte-level encoding + validating decoder + las_verify_packed
ref/amhl.{c,h}       multi-hop locks
ref/chain.{c,h}      toy ledger for the swap / payment-channel demos
ref/test/            tests and benchmarks
ref/{poly,ntt,reduce,fips202,...}.c   reused Dilithium primitives (unmodified)
evidence/            saved terminal logs produced by the commands above
```

## 7. Further documentation
| File | Contents |
|---|---|
| [docs/LAS_WALKTHROUGH.md](docs/LAS_WALKTHROUGH.md) | Plain-English, end-to-end explainer |
| [docs/LAS.md](docs/LAS.md) | Full design / implementation / evaluation write-up |
| [docs/THEORY_IMPL_BRIDGE.md](docs/THEORY_IMPL_BRIDGE.md) | Each construction equation → C function |
| [docs/STATUS.md](docs/STATUS.md) | Deliverable / test checklist |
