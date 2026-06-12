# LAS on Dilithium — build, run, reproduce

Post-quantum **Lattice-based Adaptor Signature** (LAS, Esgin–Ersoy–Erkin, IACR
eprint 2020/845, Algorithm 2 — the *simplified* scheme) implemented on the
CRYSTALS-Dilithium reference primitives, with a scriptless atomic-swap / payment
demo. This file is the reproducibility entry point; the upstream Dilithium
`README.md` is left unmodified.

## Provenance & environment
- **Upstream base:** CRYSTALS-Dilithium / ML-DSA (FIPS 204) reference C code,
  vendored at repo commit `2374d22` ("Initial commit: add Dilithium reference
  code", 2026-06-02). **No upstream source function is modified** — see
  [docs/FUNCTION_MAP.md](docs/FUNCTION_MAP.md).
- **Toolchain (recorded per supervisor request):** `cc (Ubuntu 13.3.0-6ubuntu2)`,
  `GNU Make 4.3`, on Linux (WSL2). Compiled `-O3` under
  `-Wall -Wextra -Wpedantic -Wmissing-prototypes -Wredundant-decls -Wshadow -Wvla
  -Wpointer-arith` — **zero warnings**.
- **Build mode:** `-DDILITHIUM_MODE=3` (target NIST level ~2/3). LAS is
  *mode-independent* (uses only `N`, `Q`), so modes 2/5 behave identically.
- **Parameters (scope note):** this build uses Dilithium's `q = 8380417 ≈ 2²³`,
  not the paper's `q ≈ 2²⁴`; `Q > 2γ` so correctness holds. Reconciliation to the
  paper's parameter set is a separate documented step (see `docs/LAS.md §5.9`).

## Build & run
All targets live under `ref/`:
```sh
cd ref

# --- functional tests (all hard-asserted) ---
make test/test_las3   && ./test/test_las3     # core scheme, 1000 iters, 8-point adaptor contract
make test/test_swap3  && ./test/test_swap3    # narrated 2-party atomic swap
make test/test_pcn3   && ./test/test_pcn3     # scriptless HTLCs: swap / refund / same-Y PCN
make test/test_amhl3  && ./test/test_amhl3    # AMHL multi-hop (bonus tier): wormhole + norm-growth + refund
make test/test_serde3 && ./test/test_serde3   # serialisation: round-trip / verify-from-bytes / tamper
make test/test_kat3   && ./test/test_kat3     # deterministic known-answer test (reproducibility)

# --- benchmarks ---
make test/bench_las3     && ./test/bench_las3      # per-op timings + direct rejection rate
make test/bench_compare3 && ./test/bench_compare3  # LAS vs optimised Dilithium-3
make test/bench_app3     && ./test/bench_app3      # application cost: swap + AMHL-vs-K

# --- classical adaptor baseline (objectives B2.ii) — needs a one-time clone ---
# (vendored, git-ignored; reused production implementation, harness is ours)
git clone --depth 1 https://github.com/BlockstreamResearch/secp256k1-zkp \
    ../third_party/secp256k1-zkp                   # tested at commit 95b9835
make test/bench_classical && ./test/bench_classical  # ECDSA-adaptor, same machine

# core tests also build under modes 2 and 5:
make test/test_las2 && ./test/test_las2
make test/test_las5 && ./test/test_las5
```
`make clean` removes the built test binaries.

## Expected results (what "working" looks like)
- All six functional tests print their narrative and exit `0`; `test_las3` reports
  `1000/1000 iterations (100% correctness)`.
- **KAT fingerprint** (`test_kat3`, deterministic, machine-independent):
  ```
  KAT digest: f7fc40f0b7752cafc083fcddd6a13759fbde9b2a2d538045cd0d62f87747e6b1
  ```
  This single SHAKE256 digest pins keygen + sign + presign + adapt + serialisation
  across 4 fixed vectors; any unintended change to the implementation flips it.
- Measured packed object sizes: **pk/statement 2944 B, sk/witness 512 B,
  signature/pre-signature 4672 B**. Rejection-sampling acceptance ≈ 37 %
  (≈ 2.7 attempts/signature), matching the `e⁻¹` prediction.

## Documentation index
| File | Contents |
|---|---|
| [docs/LAS.md](docs/LAS.md) | Full design / implementation / testing / evaluation write-up (report source material) |
| [docs/FUNCTION_MAP.md](docs/FUNCTION_MAP.md) | Dilithium functions reused / not-used / new (the "clean diff" map) |
| [docs/THEORY_IMPL_BRIDGE.md](docs/THEORY_IMPL_BRIDGE.md) | Every paper equation → C function / line |
| [las-objectives-meeting2.md](las-objectives-meeting2.md) | Current objectives (supervisor Meeting 2) |

## Layout
```
ref/las.{c,h}        LAS scheme (KeyGen/Sign/Verify + PreSign/PreVerify/Adapt/Ext)
ref/serialize.{c,h}  byte-level encoding + validating decoder + las_verify_packed
ref/amhl.{c,h}       multi-hop locks (bonus tier)
ref/chain.{c,h}      toy ledger for the swap / PCN demos
ref/test/            tests + benchmarks
ref/{poly,ntt,reduce,fips202,...}.c   upstream Dilithium primitives (unmodified)
```
