# CONTEXT — session handoff (updated 2026-07-10, third session)

Read this first, then `CLAUDE.md`. Delete once the whole task is finished and
committed. Supersedes the previous version of this file entirely.

## State: C side DONE and GREEN (step 1 of the plan). Rust side: the
## file-naming violation is FIXED and KAT-green (plan steps 0-5 done,
## 2026-07-10, Royce-confirmed by explicitly listing all four files).
## Next = plan step 6 (las.rs mirror rewrite).

### C side (step 1 — cycles/op + packed-tier timings in bench drivers): DONE

`ref/test/bench_levels.c` and `ref/Makefile` modified this session (see git
diff — not yet committed, Royce commits himself):
- Added upstream `test/cpucycles.{c,h}` (rdtsc) to `MEASURE`/`MEASURE_SIGN`:
  every primary timing line now reports `µs ± SD` AND `cyc ± SD`.
- Added the full END-TO-END PACKED tier (`base_sign_keypair_packed`,
  `base_sign_signature_packed`, `base_sign_verify_packed`, `las_*_packed`)
  timed alongside the existing core-struct tier, same repetition scheme.
- Added a byte-level pre-timing contract gate (packed verify, packed
  tripwire, packed adapted-verify, exact witness-BYTE recovery via
  `memcmp`) before the packed tier is timed, mirroring the struct-tier gate.
- Added two more rejection-gate lines for the packed-tier Sign/PreSign
  segments (labels `"... (packed tier)"`) — the ORIGINAL two gate lines are
  BYTE-IDENTICAL, untouched (memory: benchmark-rejection-gate).
- Added a "codec boundary cost" report block (packed − core per op) with an
  explicit NOTE that packed-tier adaptor-overhead % folds in extra codec
  work, not just adaptor math — so nobody mistakes it for the headline.
- Makefile: `test/cpucycles.c` added to `LEVELS_DEPS` and all four
  `bench_levels*` link lines.
- **Built and RAN** `make test/bench_levels3` twice this session (Royce
  said "run it and check the output"): zero warnings, all rejection gates
  (old + new) pass, cycles column internally consistent (~3.59 cyc/ns),
  codec-cost decomposition sane. This was Royce-sanctioned ad-hoc running,
  not a standing permission — don't run benches again without being told.

### Rust side (step 2 — Rust mirror round): IN PROGRESS, blocked

**What exists right now (uncommitted):**
- `rust/fips204-las/src/las_setup.rs` (NEW, untracked) — params
  (`LAS_N/ELL/KAPPA/M/GAMMA/SEEDBYTES`), shared types (`LasPp/LasPk/LasSk/LasSig`),
  and `las_setup()` (calls upstream `expand_a` unmodified). Split out of
  `las.rs` so both `las.rs` and the base file consume the same setup —
  mirrors C's `setup.h` layering. **Content is fine; FILENAME is wrong (see
  below).**
- `rust/fips204-las/src/las_basesig.rs` (rewritten this session, modified) —
  full mirror-standard rewrite of Algorithm 1 against upstream `ml_dsa.rs`:
  every function quotes its exact upstream Rust name + line, every `b_*`
  helper is a named twin of one upstream function (hashing.rs/helpers.rs),
  `[REUSED]/[CHANGED]/[DELETED]` annotations throughout, packed tier at the
  bottom. Compiles (after 3 small `add_vector_ntt` dimension fixes — see
  below). **NOT YET gated against the KAT** (never got to run
  `cargo test --offline` — the session was interrupted before that). **Also
  has the same filename violation.**
- `las.rs` modified: the old inline params/types/`las_setup` body was
  replaced with `pub use crate::las_setup::{...}` re-exports.
- `lib.rs` modified: added `pub mod las_setup;` (also needs the rename fix).

**Royce's rejections this session, in order (all resolved as design rules
except the last, which is still OPEN):**

1. **No invented helper names** — every `b_*`/`las_*` local helper must be a
   twin of a NAMED upstream function, not a helper I made up. **Resolved,
   applied**: `las_basesig.rs`'s helpers are now `b_rej_bounded_poly <->
   rej_bounded_poly (hashing.rs:158)`, `b_expand_s <-> expand_s
   (hashing.rs:252)`, `b_expand_mask <-> expand_mask (hashing.rs:281)`,
   `b_sample_in_ball <-> sample_in_ball (hashing.rs:43)`, `b_w1_encode <->
   w1_encode (encodings.rs:338)`, `b_mat_vec_mul <-> mat_vec_mul
   (helpers.rs:100)` — each also names its C twin in `basesig.c`.
2. **Scheme function names must be the upstream Rust name with the `base_`
   prefix added, not invented C-style names.** Royce's exact example:
   `base_key_gen <-> key_gen (ml_dsa.rs:26)`, `base_key_gen_internal <->
   key_gen_internal (ml_dsa.rs:57)`, `base_sign_internal <-> sign_internal
   (ml_dsa.rs:153)`. **Resolved, applied**: renamed
   `base_sign_keypair`→`base_key_gen`, `base_sign_keypair_seed`→
   `base_key_gen_internal`, `base_sign_signature_internal`→
   `base_sign_internal`, `base_sign_signature`→`base_sign`,
   `base_sign_verify_internal`→`base_verify_internal`,
   `base_sign_verify`→`base_verify` (and the `_packed` twins renamed to
   match: `base_key_gen_packed`, `base_sign_packed`, `base_verify_packed`).
3. **Every function must ALSO name its C-side equivalent** (not just the
   upstream Rust source), so the three-way correspondence
   upstream-Rust ↔ Rust-LAS ↔ C-LAS is explicit. **Resolved, applied**: every
   doc comment now has a `C twin: base_sign_keypair (basesig.c:115)` line
   (or equivalent) alongside the `ml_dsa.rs:<line>` reference.
4. **Do not modify upstream files** (`lib.rs`, `ml_dsa.rs`, `hashing.rs`,
   `helpers.rs`, etc.) — if something needs changing, copy it into a new
   file. Royce then corrected himself: **`lib.rs` MAY be edited** (it
   already carries the three additive LAS module lines from earlier
   sessions — adding more `pub mod` lines to it is the established, accepted
   pattern, not "modifying upstream"). `ml_dsa.rs`/`hashing.rs`/`helpers.rs`/
   etc. remain strictly read-only (only ever `use`d, never edited) — this
   was never in question and stays.
5. **OPEN, NOT YET FIXED**: *"jangan pakai nama 'las_basesig.rs' pakai
   'basesig.rs' untuk BASE dan 'setup.rs' untuk SETUP BOTH! kenapa di
   'basesig.rs' ada 'las_pack_sig'??? fix it! kamu tidak mengikuti desain
   arsitektur implementasi yang aku perintahkan!!!"* — i.e.: **the FILES
   `las_basesig.rs` and `las_setup.rs` are misnamed; they must be
   `basesig.rs` and `setup.rs`** (dropping the redundant `las_` file
   prefix). The session was interrupted before this rename was applied.

**My analysis of point 5 (high confidence, NOT yet confirmed with Royce —
confirm-by-question at the start of next session, don't just silently apply:
this session already got the architecture wrong 4 times before this point,
so verify before acting again):**

- C precedent (`ref/`): the shared/scheme FILES are named plainly —
  `setup.c`, `serialize.c`, `basesig.c`, `las.c` (no `las_` file prefix,
  `las.c` is just the Algorithm-2 scheme's own name). But FUNCTIONS inside
  `serialize.c`/`setup.c` DO keep the `las_` prefix on the function name
  itself (`las_pack_pk`, `las_pack_sk`, `las_pack_sig`, `las_unpack_*`,
  `las_setup`) — because `las_` there means "shared LAS-project
  infrastructure", not "lives in a file called las_something.c". Verified:
  `ref/serialize.h:67`: `void las_pack_pk(...)` inside the file
  `serialize.h`, no `las_` in the filename.
- So the required renames are (all three, not just the two Royce named
  explicitly — `las_serialize.rs` is the SAME pre-existing violation from an
  earlier session, before this one, and should get the same fix now that
  the rule is explicit):
  1. `rust/fips204-las/src/las_setup.rs` → `setup.rs`
  2. `rust/fips204-las/src/las_basesig.rs` → `basesig.rs`
  3. `rust/fips204-las/src/las_serialize.rs` → `serialize.rs`
  4. `las.rs` stays `las.rs` (already correctly named — it IS the LAS
     scheme, not a `las_`-prefixed something-else).
- The function names INSIDE those files (`las_pack_pk`, `las_pack_sig`,
  `las_setup`, etc.) are almost certainly meant to STAY as-is — they
  correctly mirror the C function names 1:1, and Royce's complaint reads as
  "why does a file called basesig.rs import something with a las_ prefix"
  which resolves once the import path becomes `crate::serialize::las_pack_sig`
  instead of `crate::las_serialize::las_pack_sig` (i.e. the redundant `las_`
  disappears from the PATH, not necessarily from the function name) — exactly
  mirroring `basesig.c`'s `#include "serialize.h"` then calling
  `las_pack_pk(...)` directly. **But this is inference, not confirmed — say
  the plan out loud and get an explicit yes/no before touching function
  names, since 4 of my last 5 architecture guesses this session were wrong.**

## Remaining plan (in order)

0-5. ✅ DONE (2026-07-10, Royce-confirmed): renames applied (`git mv` for
   the tracked `basesig.rs`/`serialize.rs`, plain `mv` for the untracked
   `setup.rs`); `lib.rs` module block updated (order setup → serialize →
   basesig → las, doc comments updated); every `crate::las_setup::`/
   `crate::las_serialize::` path and doc-comment filename mention updated in
   `setup.rs`/`basesig.rs`/`las.rs`; the 5 external callers updated (module
   paths AND the renamed base functions — they still called
   `base_sign_keypair`/`base_sign_signature`/`base_sign_verify`, now
   `base_key_gen`/`base_sign`/`base_verify`); `las.rs`'s doc-comment
   provenance chains updated to the new basesig names.  Function names
   inside `setup.rs`/`serialize.rs` KEPT their `las_` prefix (mirrors C),
   and `basesig.rs`'s C-twin citations (`base_sign_keypair (basesig.c:115)`
   etc.) KEPT — those name C functions, not Rust ones.
   **Gate GREEN**: `cargo build --offline` clean; `cargo test --offline
   --test las_kat` passes (pinned digest matches, byte-for-byte C parity);
   `cargo check --offline --all-targets` (tests/examples/benches) clean.
   Note: `tests/las_stage1.rs` compiles but was NOT run (not in the
   sanctioned gate list) — it is the test that actually exercises
   `basesig.rs`'s behaviour (base round-trip + adapted-sig-verifies-under-
   base-verifier), a cheap extra check if Royce sanctions running it.
6. Then continue the ORIGINAL Rust-mirror plan: rewrite `las.rs` itself the
   same way (quote `basesig.rs`'s twins verbatim + WHY; Algorithm-2
   functions quote their OWN Algorithm-1 twin in `las.rs`, exactly like
   `ref/las.c` quotes `ref/basesig.c`), add its packed tier (check where
   `las_verify_packed` currently lives after the `serialize.rs` rename and
   whether it needs moving into `las.rs`, mirroring the C decision that
   `las_verify_packed` lives in `las.c` not `serialize.c`), re-run the KAT
   gate.
7. Machine-verify all `ml_dsa.rs:<line>` / C-twin line citations (a script
   like the C round's `verify_citations.py`, adapted for Rust — scratchpad).
8. Update `examples/bench_levels.rs` (Rust) + `benches/las_bench.rs` +
   `ref/test/bench_criterion.c` **together, in lockstep** (per the original
   plan) — cycles/op parity + packed-tier timing parity with the now-done C
   `bench_levels.c`. Do NOT run benches; Royce runs.
9. Doc sync LAST, only after Royce accepts the Rust code too (same
   "jangan fix dokumentasi dulu" rule as the C round): `FUNCTION_MAP.md`,
   `THEORY_IMPL_BRIDGE.md`, `README.md` Rust build notes, anything in
   `docs/` that names the old Rust file layout.

## Hard constraints (unchanged)

- No benchmarks/tests without instruction EXCEPT the correctness-test /
  KAT-gate builds this plan explicitly calls for (`cargo build`,
  `cargo test --offline --test las_kat`) — those ARE the gate, run them.
  Do NOT run `benches/las_bench.rs` (Criterion) or any C bench target
  without being told.
- No branch changes, no commits (Royce commits), no invented numbers.
- `ref/basesig.c`, `ref/las.c`, `ref/setup.c`, `ref/serialize.c` are
  ACCEPTED C code from the previous session — do not touch them EXCEPT as
  explicitly ordered.  2026-07-11 exception (Royce-ordered): the SIBLING RULE
  below was applied to `ref/setup.h`/`ref/las.h`/`ref/basesig.h` + one stale
  header-comment line in `ref/basesig.c` (:32).  **C NOT rebuilt** — Royce
  runs `make` himself; the change is include-transparent for every las.h
  consumer (las.h includes setup.h) and basesig.c now gets LAS_BOUND_SIGN
  via basesig.h → setup.h.
- **SIBLING RULE (Royce, 2026-07-11, applies C AND Rust from here on):**
  BASE must NOT include/depend on las (`basesig.h` includes `setup.h` +
  `serialize.h`, never `las.h`; `basesig.rs` imports `crate::setup`, never
  `crate::las`).  Anything shared by BOTH schemes must live in the shared
  layer `setup.{h,rs}` — `LAS_BOUND_SIGN` moved there (C `setup.h`, Rust
  `setup.rs`, re-exported by `las.{h,rs}`-equivalent paths so external
  callers are unchanged); adaptor-only bounds (`LAS_BOUND_PRESIGN`[`_K`])
  stay in las.  `serialize.rs` likewise takes types from `crate::setup`
  (its one remaining `crate::las::las_verify` import is the known
  `las_verify_packed`-lives-in-the-wrong-file item, fixed in plan step 6).
- **Royce's review round on basesig.rs (2026-07-11), points 1-4 APPLIED**
  (KAT still green): (1) the ml_dsa.rs:3-14 import block + the two local
  consts are now annotated per-slot ([REUSED]/[CHANGED]/[DELETED] + WHY);
  (2) sibling rule above; (3) header claim reworded to "NO UNMARKED EXTRA
  LINES" (the blanket "NO EXTRA LINES" contradicted the file's own
  `[CHANGED] no upstream line` entries); (4) `b_w1_encode` renamed
  `b_w_encode` ('1' dropped exactly as C's `b_polyw_pack` drops it from
  `polyw1_pack` — it packs the FULL w).  Points 5-6 (RNG Result-API,
  hedged derivation) NOT applied: assessed as documented, deliberate
  C-twin-faithful deviations; Royce did not order them changed.
- Upstream Rust files (`ml_dsa.rs`, `hashing.rs`, `helpers.rs`,
  `high_low.rs`, `ntt.rs`, `types.rs`, `traits.rs`, `encodings.rs`,
  `conversion.rs`) are **read-only** — `use` them, never edit them.
  `lib.rs` may be edited only to add/rename the LAS module declaration
  lines (established pattern from earlier sessions, confirmed OK by Royce
  this session).
- `rust/fips204-las/src/las_verbose_comment.rs` and
  `las_basesig_verbose_comment.rs` are STALE annotated copies from an
  earlier layout (unbuilt) — known stale, ignore/regenerate-later, not in
  scope this round unless Royce asks.
- Naming rule (now settled, apply everywhere in Rust from here on): scheme
  functions = upstream Rust name + `base_`/`las_` prefix (e.g.
  `key_gen`→`base_key_gen`, no invented C-style names); every function/helper
  doc-comment names BOTH its upstream-Rust line AND its C-file:line twin;
  every local helper is a twin of exactly one NAMED upstream function, never
  invented; FILES are named plainly like their C counterparts
  (`setup.rs`/`serialize.rs`/`basesig.rs`/`las.rs`, no redundant `las_` file
  prefix) even though functions inside `setup.rs`/`serialize.rs` keep their
  `las_` function-name prefix (mirrors C exactly).
