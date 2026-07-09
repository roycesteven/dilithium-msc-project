
## Checkpoint — 2026-07-09 10:54

Branch: restructure

Current goal:
- Re-lay-out the 4 scheme files for side-by-side comparison and rename LAS API for uniform provenance chain crypto_sign*→base_sign*→las*.

Done:
- Mirrored basesig.c↔sign.c and las.c↔basesig.c (same slots/order/int returns, helpers at bottom); Rust twins las_basesig.rs/las.rs likewise.
- Renamed las_keygen→las_keypair, las_sign→las_signature, sign_core→las_signature_internal (+ _internal splits) across all C/Rust callers, examples, and project docs.
- Built + ran full suites: 16 C targets PASS (KAT digest matches), Rust cargo test PASS (las_kat parity, doctests); fixed pre-existing rustdoc failure.

Files touched/inspected:
- ref/basesig.{c,h}, ref/las.{c,h}
- rust/fips204-las/src/las.rs, las_basesig.rs
- plus other related files (tests, examples, docs), not listed to keep checkpoint short.

Evidence used:
- none

Open risks:
- report/REPORT_DRAFT.md (~L376/433/787) still uses old names — left for Royce.
- las_verbose_comment.{c,h,rs} renamed but still old layout (annotated copies, unbuilt).

Next action:
- Decide whether to regenerate las_verbose_comment.* to the new layout or drop them.

## Checkpoint 2026-07-09 (mirror-rigor rewrite + two-tier architecture)

Branch: restructure

Current goal:
- Upstream-twin-helper rewrite of las.c (quote basesig.c lines verbatim + WHY), then two-tier architecture: core-crypto (struct) vs end-to-end (packed) API.

Done:
- ref/basesig.c (accepted earlier) built+tested: 4 param sets x 1000 iters PASS.
- ref/las.c rewritten to the same standard: helpers = verbatim copies of basesig.c's b_* twins (las_* prefix), inline SHAKE/matrix/rej composition, every line annotated [REUSED]/[CHANGED]/[DELETED] quoting basesig.c:<line> (Alg-1) or its own Alg-1 twin las.c:<line> (Alg-2); all 207 line citations machine-verified.
- Adopted basesig's z-pipeline order (proved KAT-safe: divergent reduce32 representatives differ by Q and both always fail chknorm).
- Provenance chain completed per Royce: las_sign<->base_sign, las_open<->base_sign_open added.
- Shared setup split out: ref/setup.{c,h} = params + shared types (las_pp/pk/sk/sig) + las_setup (paper Setup()->pp); las.h/basesig.h/serialize.h re-layered (setup.h -> serialize.h -> schemes, mirroring params/polyvec -> packing -> sign).
- End-to-end PACKED-API tier added per Royce (packing inside the call, like sign.c): base_sign_{keypair,signature,verify}_packed in basesig.c; las_{keypair,signature,verify,presign,preverify,adapt,ext}_packed in las.c; serialize.c now pure codec (las_verify_packed moved to las.c, arg order unified with struct tier; callers updated).
- test_serde.c: packed-tier roundtrips + byte-level interlock (base verifier accepts adapted LAS sig through bytes).
- Build clean (zero warnings), ALL 16 C tests PASS, KAT digest 641a176c... matches pinned value.

Files touched:
- ref/las.{c,h}, ref/basesig.{c,h}, ref/setup.{c,h} (new), ref/serialize.{c,h}, ref/Makefile, ref/test/test_serde.c, ref/test/test_contract.c.

Evidence used:
- test run output this session (16/16 PASS incl. test_kat3 pinned digest).

Open risks:
- docs/walkthrough/FUNCTION_MAP/bridge line-number links now stale (doc sync deferred until Royce accepts code).
- basesig.h header still says "las.{c,h} are byte-for-byte untouched" (stale claim).
- las_verbose_comment.* still old layout.

Next action:
- Cycles/op + packed-tier timings in bench drivers (bench_levels.c; bench_criterion.c must stay in lockstep with the Rust driver), then Rust mirror rewrite (las_basesig.rs quotes ml_dsa.rs, las.rs quotes las_basesig.rs, + setup/packed-tier parity), then cargo test KAT parity, then doc sync.
