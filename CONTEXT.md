# CONTEXT — session handoff (updated 2026-07-13, sixth session)

Read this first, then `CLAUDE.md`. Supersedes the previous CONTEXT.md entirely.

## TL;DR — where things stand

**Stage A (seven-type / Algorithm-split refactor) is DONE and PROVEN on BOTH
languages.** The Rust port was already done+proven; **this session finished the
C mirror for the in-scope core and the C KAT gate is GREEN**:

- `make test/test_kat3 && ./test/test_kat3` → digest
  `641a176c3eb2125098fdbb7ad16bfa38fb5744b52dd9696beeb7d07be1445a19`
  **MATCHES the pinned value** = the refactor changed **zero bytes**.
- `test/test_las3` (1000/1000), `test/test_basesig3` (all checks),
  `test/test_serde3` (all serde) also built + PASSED, under the full Makefile
  CFLAGS (`-Wall -Wextra -Wpedantic -Wshadow …`) with **zero warnings**.

**Stage B (the c_tilde challenge-lifecycle correction) is NOT started.** See
`/home/melly/.claude/plans/read-context-md-and-upstream-iridescent-gadget.md`
— that plan file is the source of truth for Stage B. It legitimately changes the
wire format (D3 signature 6752→6720 B; paper/D2 4672→4640 B, because a 32-byte
`c_tilde` digest replaces the 64-byte packed ternary `c`), so it is gated on a
**NEW** digest measured from a real run and pinned in both languages — never
invent it. Do not begin Stage B until Royce confirms Stage A is accepted.

## Naming convention — now CANONICAL in CLAUDE.md (do not re-derive)

The exact `Rust ⇄ C ⇄ paper` naming convention (construction params, the seven
types + fields, bounds, function names, gate names, serialize sizes, locked local
conventions) is recorded in **CLAUDE.md** under
"⚠️ CANONICAL NAMING CONVENTION — Rust ⇄ C ⇄ paper". The proven Rust port
(`rust/fips204-las/src/`, KAT digest `641a176c…`) is the AUTHORITY; C mirrors it
exactly, the ONLY divergence being C keeps `LAS_N` (module rank) and `LAS_D`
(`#define LAS_D N`, ring degree) because params.h already owns bare `N`/`D`.
Paper-symbol source of truth for report/comment notation:
`docs/paper/LAS_2020_845_NOTATION.md`.

## What was done this session (all committed to the working tree, NOT git-committed)

C mirror, in-scope core — **complete, compiles clean, tests pass**:

- `ref/relation.{h,c}` — NEW. `relation_gen` / `relation_gen_seed` producing
  `(statement, witness)`; own static ternary-sampler + A-product twins. Witness
  named `r_prime` (honest r′), NEVER `y` (mask) or `s` (Ext's extracted witness).
  Comments keep Gen and KeyGen as DISTINCT algorithms (same math, different
  objects; never "Gen is KeyGen").
- `ref/serialize.{h,c}` — six typed pack/unpack pairs over three private encoders
  (`encode/decode_canonical_vec` / `_ternary_vec` / `_chal_response`); semantic
  `PUBLIC_KEY_BYTES` / `SECRET_KEY_BYTES` / `SIGNATURE_BYTES` +
  `STATEMENT_BYTES`=pk / `WITNESS_BYTES`=sk / `PRE_SIGNATURE_BYTES`=sig.
  `las_verify_packed` absent (moved to basesig as `base_verify_packed`).
- `ref/basesig.{h,c}` — Algorithm 1 only. Renamed `base_keygen` / `base_sign` /
  `base_verify` (+ `_internal`), added `base_keygen_seed`, `base_sign_det`, a
  tag-0-only static `det_seed`, `#define BOUND_SIGN`; packed
  `base_keygen_packed`/`base_sign_packed`/`base_verify_packed`. Old sm-wrappers
  `base_sign`/`base_sign_open` deleted. **Paper-faithful locals** (per Royce's
  reviews): `r_1_hat`, `y_1_hat`, `z_1_hat`, `t_hat`, `c_hat`, `mask_nonce`,
  `c_check`, `w`/`w_prime`, `t_packed`/`w_packed`, `c_tilde`; the 64-byte mask
  seed is `mask_seed` (implementation-only, NO paper symbol). `sk->r`,
  `pp->a_prime`, ring degree `LAS_D`.
- `ref/las.{h,c}` — Algorithm 2 ONLY. Deleted the whole Algorithm-1 block, Gen,
  and the now-unused S1 ternary sampler. Retyped to `statement`/`witness`/
  `pre_signature` (`presig->z_hat`, `Y->t_prime`, Adapt witness `r_prime`, Ext
  witness `s`). `#define BOUND_PRESIGN` / `BOUND_PRESIGN_K(K)`. Packed tier
  rewritten to use the TYPED codecs (`unpack_statement`/`unpack_witness`/etc.).
  `las_ext_packed` documented as ternary/single-hop-witness only.
- Tests: `test_kat.c`, `test_las.c`, `test_serde.c`, `test_basesig.c` retyped.
  Pre-signature tripwires are **byte-level** (`unpack_signature(pack_pre_signature)`
  then Verify must fail). All return codes checked; compile-time size assertions
  added; `test_basesig.c`'s vacuous cross-module [X1]/[X2] checks removed.
- Benches: `bench_levels.c`, `bench_criterion.c`, `export_packed.c` retyped,
  **syntax-clean** (`-Wall -Wextra -Wpedantic`). Rejection-gate lines preserved
  (`LAS_BOUND_*`→`BOUND_*`, `las_expected_attempts` / `base_attempts` /
  `las_attempts` unchanged). NOT run (bench_levels/criterion are ~minutes-long;
  running benches is not sanctioned without an explicit ask).
- `ref/Makefile` — `relation.c` added to every in-scope link line; sweep flags
  `-DLAS_ELL=`/`-DLAS_KAPPA=` → `-DELL=`/`-DKAPPA=` (`-DLAS_N=` kept).

## SCOPE: AMHL + chain are OUT of Stage A (Royce, this session)

These were deliberately NOT retyped and still use the OLD API (`las_pk`, `las_keypair`,
`LAS_ELL`, …), so their `make` targets WILL FAIL — that is expected. Do NOT build
`make all`; build only the in-scope targets.

- Untouched files: `ref/amhl.{h,c}`, `ref/chain.{h,c}`,
  `ref/test/test_amhl.c`, `test_pcn.c`, `test_swap.c`, `test_contract.c`,
  `test/bench_app.c`.
- Their Makefile targets (`test/test_amhl3`, `test/test_pcn3`, `test/test_swap3`,
  `test/test_contract3`, `test/bench_app3`) are unchanged and will not compile.

## Verification state / what Royce should run

- ALREADY verified by Claude this session (sanctioned): `test/test_kat3`,
  `test/test_las3`, `test/test_serde3`, `test/test_basesig3` → build + pass;
  digest matches.
- Royce to run when convenient: the benches (`test/bench_levels3`,
  `test/bench_criterion3` — confirms the rejection gate at RUNTIME) and the
  cross-parameter-set targets (`test/test_las2`, `test/test_las5`,
  `test/test_serde_l2`/`_l3`/`_l5`, `test/test_basesig_paper`/`2`/`5`,
  `test/export_packed`). All are in-scope and should build.

## Hard constraints (unchanged, still binding)

- No git branch changes, no commits (Royce commits), no invented numbers, no
  hand-editing evidence logs.
- Upstream Rust files and upstream C primitives (`poly.c`, `ntt.c`, `sign.c`,
  `packing.c`, `fips202.c`, …) stay untouched — they keep bare `N`/`D`.
- `ref/*_verbose_comment.{c,h}` and `rust/…/*_verbose_comment.rs` are STALE
  unbuilt copies — out of scope.
- Doc sync (`docs/`, `FUNCTION_MAP.md`, `THEORY_IMPL_BRIDGE.md`, `README.md`,
  the LaTeX report) happens LAST, after Royce accepts the code. Stage-B's plan
  file lists the stale doc refs to fix then (sizes, "c as 2-bit ternary",
  component-split %, tamper-count, EVM gas calldata size).

## Scratchpad backups (this session, not in the repo)

`/tmp/claude-1000/-home-melly-dilithium-msc-project/<id>/scratchpad/`:
pre-transform `*.bak` of `basesig.c`, `las.c`, `Makefile`,
`bench_criterion.c`, `bench_levels.c`, plus all the `xform_*.py` transform
scripts (useful to audit exactly what each rename did).

## Immediate next actions for the next session

1. If Royce accepts Stage A: decide whether to (a) retype the out-of-scope
   amhl/chain tier to the seven-type API, and/or (b) start Stage B.
2. Stage B: read the plan file
   `/home/melly/.claude/plans/read-context-md-and-upstream-iridescent-gadget.md`
   in full FIRST; do it in BOTH languages; gate on a NEW digest measured from a
   real Rust `las_kat` run and pinned in both languages.
3. Only after Royce accepts BOTH stages: do the documentation sync, then delete
   this CONTEXT.md.
