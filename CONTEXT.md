# CONTEXT — session handoff (updated 2026-07-09, second session)

Read this first, then `CLAUDE.md`. Delete once the whole task is finished and
committed. Supersedes the previous version of this file entirely.

## State: C side DONE and GREEN (not yet committed — Royce commits himself)

Build clean (zero warnings), **all 16 C test targets PASS, `test_kat3` pinned
digest `641a176c…` matches** — behavioural identity proven. `git status`:
modified `ref/{las,basesig}.{c,h}`, `ref/serialize.{c,h}`, `ref/Makefile`,
`ref/test/{test_serde,test_contract}.c`, `PROGRESS.md`; new `ref/setup.{c,h}`.
(`PROGRESS copy.md` is a stray user file — leave alone.)

What was done (details in the PROGRESS.md 2026-07-09 checkpoint):

1. **las.c rewritten to Royce's mirror standard** (accepted style of basesig.c):
   no invented helpers — every local helper is a verbatim copy of a basesig.c
   `b_*` twin (renamed `las_*`), each itself a twin of a NAMED upstream
   poly.c/polyvec.c function; scheme composition (SHAKE block, matrix-vector
   sequence, `rej:` loop) inline; every line annotated
   `[REUSED]/[CHANGED]/[DELETED] basesig.c:<line>: <verbatim code>` + WHY.
   Algorithm-2 functions quote their own Algorithm-1 twin (`las.c:<line>`).
   All 207 citations machine-verified (scratchpad scripts:
   `fix_line_refs.py`, `repair_line_refs.py`, `verify_citations.py`).
2. **Chain completed:** `las_sign`↔`base_sign`, `las_open`↔`base_sign_open`
   (Royce's explicit demand: no "(none; see serialize.c)" gaps).
3. **Shared setup split out** (Royce): `ref/setup.{c,h}` = params + shared
   types (`las_pp/las_pk/las_sk/las_sig`) + `las_setup` (paper `Setup()→pp`).
   Layering now mirrors upstream: setup.h (≈params/polyvec) → serialize.{c,h}
   (pure codec ≈packing) → basesig.c/las.c (schemes ≈sign.c).
4. **Two-tier API (Royce: "core crypto AND end-to-end, both!"):** struct tier
   = core crypto cost; new PACKED tier = end-to-end boundary like sign.c
   (unpack→core→pack inside the call): `base_sign_{keypair,signature,verify}_packed`
   in basesig.c, `las_{keypair,signature,verify,presign,preverify,adapt,ext}_packed`
   in las.c. `las_verify_packed` MOVED serialize.c→las.c and its arg order
   unified to `(sig_b, m, mlen, pk_b, pp)` — callers updated
   (test_serde.c, test_contract.c). test_serde.c gained packed-tier
   roundtrips + byte-level interlock checks.
5. z-pipeline order aligned to basesig.c (KAT-safe — proof: divergent
   reduce32 representatives differ by exactly Q, so both always fail the
   chknorm gate; empirically confirmed by the pinned KAT).

Royce's decisions this session (AskUserQuestion): packed tier lives INSIDE
basesig.c/las.c (not serialize.c); ALL ops get packed variants; cycles/op goes
into EXISTING bench drivers.

## Remaining plan (in order)

1. **Cycles/op + packed-tier timings in bench drivers.** Add a cycles counter
   (rdtsc-style, like upstream's test/cpucycles) reporting cycles/op next to
   ns/op, and time BOTH tiers (core struct ops and *_packed ops) in
   `ref/test/bench_levels.c`. **Do NOT weaken/rename the rejection-gate
   assert lines** (memory: benchmark-rejection-gate). `bench_criterion.c`
   mirrors the Rust criterion driver 100% — change it only in lockstep with
   the Rust driver (do together with step 2). Do NOT run benches; Royce runs.
2. **Rust mirror round** (`rust/fips204-las/`): rewrite `las_basesig.rs`
   (quote ml_dsa.rs lines verbatim + WHY, upstream-twin helpers per
   hashing.rs/helpers.rs — notes in the old CONTEXT.md version, git history)
   and `las.rs` (quote las_basesig.rs). Mirror the C architecture: setup
   split + two-tier packed API parity (decide Rust module layout with Royce
   if unclear). KAT gate: `cargo test --offline` — `tests/las_kat.rs` must
   reproduce the C digest byte-for-byte. Watch the rustdoc indented-text
   gotcha (wrap in ```text fences).
3. **Doc sync LAST, only after Royce accepts the code** ("jangan fix
   dokumentasi dulu"): FUNCTION_MAP.md, THEORY_IMPL_BRIDGE.md,
   UPSTREAM_TO_LAS_WALKTHROUGH.md line links, LAS.md §5.10/§6.3
   (las_verify_packed moved + new arg order + new packed tier), README build
   notes (setup.c/serialize.c now in all link lines), CODE_DIFF_VIEW.
   Known stale: basesig.h header claim "las.{c,h} are byte-for-byte
   untouched"; las_verbose_comment.* still old layout.

## Hard constraints (unchanged)

- No benchmarks/tests without instruction EXCEPT the correctness-test builds
  Royce already sanctioned this session (build + run test targets = the gate).
- No branch changes, no commits (Royce commits), no invented numbers.
- basesig.c is ACCEPTED — only append (packed tier was appended below the
  helpers precisely so the sign.c-mirror spine's line numbers stay stable;
  las.c cites them, +4 shift from the serialize.h include already applied).
