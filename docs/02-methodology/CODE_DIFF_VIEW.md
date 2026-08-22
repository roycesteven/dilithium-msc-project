# Code-Difference View — original Dilithium vs. LAS (this work)

> Created for Meeting-3 action item #1 (supervisor: Wang Zhipeng, 2026-06-18).
> Purpose: let the supervisor and the second marker see, at a glance, exactly
> **which files were reused unchanged, which were modified, and which were added**
> when building LAS on top of the CRYSTALS-Dilithium reference implementation.

## The two-branch layout

| Branch | Contents | Anchor commit |
|---|---|---|
| `dilithium-baseline` | The **pristine** CRYSTALS-Dilithium reference code, exactly as imported. Nothing from this project added. | `2374d22` ("Initial commit: add Dilithium reference code") |
| `main` | The full project: the same Dilithium primitives **plus** the LAS adaptor-signature layer, serialization, chain demo, benchmarks, and tests. | latest |

`main` is a strict superset of `dilithium-baseline`: the baseline is literally
the first commit of `main`'s own history, so the comparison has zero drift.

### How to view the diff

GitHub (after the baseline branch is pushed — see below):

```
https://github.com/roycesteven/dilithium-msc-project/compare/dilithium-baseline...main
```

Locally:

```bash
# summary of every file that differs (added / modified / deleted)
git diff --name-status dilithium-baseline main -- ref/

# full line-level diff of one file
git diff dilithium-baseline main -- ref/las.c

# prove the upstream primitives were not touched (no output == identical)
git diff dilithium-baseline main -- ref/poly.c ref/ntt.c ref/sign.c ref/packing.c \
  ref/polyvec.c ref/reduce.c ref/rounding.c ref/params.h ref/fips202.c
```

> **Pushing the baseline branch (outward-facing — run when ready):**
> `git push -u origin dilithium-baseline`
> This is the only step needed before the GitHub `compare/...` URL above works.

## The three categories

### 1. Reused **unchanged** (the entire lattice/crypto core)

Every CRYSTALS-Dilithium primitive is **byte-for-byte identical** between the two
branches. LAS calls them as a library; it does not fork them.

| File | Role reused by LAS |
|---|---|
| `ref/poly.c`, `ref/poly.h` | polynomial arithmetic, sampling, norm checks |
| `ref/polyvec.c`, `ref/polyvec.h` | vector-of-polynomials operations |
| `ref/ntt.c`, `ref/ntt.h` | number-theoretic transform (the `Q=8380417` root table) |
| `ref/reduce.c`, `ref/rounding.c` | modular reduction, rounding |
| `ref/packing.c` | Dilithium's own (de)serialisation (used by the baseline scheme) |
| `ref/sign.c` | the optimised Dilithium scheme (the benchmark reference column) |
| `ref/fips202.c`, `ref/symmetric-shake.c` | SHAKE128/256 / Keccak |
| `ref/params.h`, `ref/api.h`, `ref/config.h` | parameters and API |
| `ref/randombytes.c` | RNG shim |

Verified mechanically: see the "prove the upstream primitives were not touched"
command above — it produces **no output**.

### 2. **Modified** (build glue only — no algorithm changes)

| File | Change | Why |
|---|---|---|
| `ref/Makefile` | added build targets for the LAS sources, demos, benchmarks, and tests | to compile the new code; the existing Dilithium targets are untouched |
| `ref/test/.gitignore` | added the new test/benchmark binaries | keep compiled artefacts out of git |

No upstream `.c`/`.h` source file was modified. The headline claim in
`docs/02-methodology/FUNCTION_MAP.md` — **zero upstream functions modified** — is therefore
confirmed at the file level by this diff.

### 3. **Added** (all of the project's own work)

| File | What it is |
|---|---|
| `ref/las.c`, `ref/las.h` | the LAS scheme: KeyGen, Sign, Verify + the adaptor operations PreSign, PreVerify, Adapt, Ext (paper Algorithm 2, "variant B") |
| `ref/basesig.c`, `ref/basesig.h` | the **separate** simplified Dilithium-style base signature (`base_keygen`/`base_sign`/`base_verify`, `c = H(pk, w, M)`, no statement `Y`) — the fair baseline for `bench_levels`; kept out of `las.{c,h}` so the LAS protocol is untouched, shares only `las.h`'s parameter macros + struct layout |
| `ref/serialize.c`, `ref/serialize.h` | byte-level wire/on-chain encoding + validating decoder + `base_verify_packed` |
| `ref/amhl.c`, `ref/amhl.h`, `ref/chain.c`, `ref/chain.h` | **dead legacy, not part of the project** — exploratory multi-hop/ledger code left on the pre-restructure API; does not compile and is not repaired (dropped 2026-08-03) |
| `ref/test/test_las.c` | LAS correctness test (1000 iters, modes 2/3/5) |
| `ref/test/test_basesig.c` | **CHECK**-gated base-signature correctness (1000 iters, paper/2/3/5): honest verify, tamper/wrong-key rejection, cross-module equivalence with `las.c`, cross-path interlock (tripwire + adapted-verifies-under-base + exact Ext), plus 4 negative tests (wrong statement, wrong witness, tampered pre-signature, tampered adapted signature) |
| `ref/test/test_serde.c` | serialization round-trip + single-byte-tamper rejection + validating decoder; swept across parameter sets (`test_serde3` paper dims + `test_serde_l2/l3/l5`) |
| `ref/test/test_kat.c` | deterministic API + pinned known-answer vectors |
| `ref/test/test_contract.c` | consolidated correctness-contract harness (itemised 8-point PASS) |
| `ref/test/test_swap.c` | Fig. 1 atomic-swap demo (the Stage-2 evaluation is `rust/las-swap/`; `test_pcn.c` / `test_amhl.c` are dead legacy) |
| `ref/test/bench_las.c` | per-op timing + measured rejection rate |
| `ref/test/bench_compare.c` | LAS vs. optimised Dilithium-3 |
| `ref/test/bench_levels.c` | **primary fair benchmark**: the separate base path (`basesig.c`) vs the LAS adaptor path (`las.c`), adaptor-overhead pairing + cross-verify contract; official Dilithium = context only ("not algorithm-matched"); ≥5 runs with std-dev; component-level size breakdown |
| `ref/test/bench_fair.c` | **removed** — an earlier benchmark that read official Dilithium as a fair baseline (wrong); replaced by `bench_levels.c` |
| `ref/test/bench_app.c` | application-level cost vs. path length |
| `ref/test/bench_classical.c` | classical ECDSA-adaptor baseline (libsecp256k1-zkp) |
| `ref/test/export_packed.c` | exports a real packed signature for the EVM gas benchmark |

### Removed

| File | Why |
|---|---|
| `ref/sign copy.c` | a local study copy of the Dilithium signer, never part of the build; removed to keep the tree clean. |

## One-line summary for the report

> LAS is built **additively** on the Dilithium reference: 100 % of the lattice
> core (poly/NTT/SHAKE/sampling, 12 source files) is reused verbatim, the only
> modified files are build glue (`Makefile`, `.gitignore`), and the adaptor
> scheme, serialization, demos, and benchmarks are all new code. This is the
> "reused vs. modified vs. added" table required for the report (Meeting-2 B4).
