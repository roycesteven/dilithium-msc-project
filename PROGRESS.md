
## Checkpoint — 2026-07-13 (Stage-A C mirror DONE + KAT gate GREEN)

Branch: restructure

Current goal:
- Stage A: seven-type/Algorithm-split C mirror of the proven Rust. Gate = pinned KAT digest unchanged.

Done (this session):
- C mirror COMPLETE for the in-scope core: ref/relation.{h,c} (new), serialize.{h,c} (six typed pairs), basesig.{h,c} (base_keygen/base_sign/base_verify + base_keygen_seed/base_sign_det/det_seed/base_verify_packed; paper-faithful locals), las.{h,c} (Algorithm-2 only; Alg-1 + S1 sampler deleted; packed tier uses typed codecs; BOUND_PRESIGN[_K]).
- Core tests retyped + BUILT + PASS at D3: test_kat3 (KAT digest 641a176c…5a19 MATCHES — zero bytes changed), test_las3 (1000/1000), test_basesig3 (all checks), test_serde3 (all serde). All under full CFLAGS -Wall -Wextra -Wpedantic -Wshadow…, zero warnings.
- Benches retyped + syntax-clean (-Wall -Wextra -Wpedantic): bench_levels.c, bench_criterion.c, export_packed.c. NOT run (8-min benches; not sanctioned). Rejection-gate lines preserved (LAS_BOUND_*→BOUND_*, las_expected_attempts kept).
- Makefile patched: relation.c added to core link lines; -DLAS_ELL/-DLAS_KAPPA → -DELL/-DKAPPA (LAS_N kept).
- Royce scope decision (this session): AMHL + chain OUT of Stage-A scope. amhl.{c,h}, chain.{c,h}, test_amhl.c, test_pcn.c, test_swap.c, test_contract.c, bench_app.c NOT retyped (still old API; their `make` targets will fail — intentional).
- CLAUDE.md gained a canonical "Rust ⇄ C ⇄ paper" naming-convention section (source of truth for the mirror).

Evidence used:
- In-session build+run of test_kat3/las3/serde3/basesig3 (compiled + executed by Claude this session, sanctioned by Royce). Digest 641a176c…5a19 reproduced.

Open risks:
- Benches compiled but NOT run (Royce should run bench_levels3/bench_criterion3 to confirm the rejection gate passes at runtime, and test_las2/las5, test_serde_l2/l3/l5, test_basesig_paper/2/5 across param sets).
- Out-of-scope amhl/chain tier left on old API — `make all` fails on those targets; build only the in-scope targets.
- Stage B (c_tilde) NOT started; will change wire size (D3 6752→6720) → new digest both langs.

Next action:
- Royce: `make test/test_kat3 && ./test/test_kat3` to reconfirm digest; optionally run benches. Then decide amhl/chain retype + Stage B.

## Checkpoint — 2026-07-12 (c_tilde correction + seven-type refactor)

Branch: restructure

Current goal:
- Stage A: finish seven-type/Algorithm-split refactor + LAS_-prefix param rename (both langs). Stage B: c_tilde challenge-lifecycle correction. Two digest checkpoints.

Done:
- Rust Stage A COMPLETE + PROVEN: basesig.rs (det_seed, packed tier), las.rs (Alg-2 only), lib.rs (pub mod relation), all tests/examples/benches; byte-level presig tripwires. KAT digest 641a176c… UNCHANGED, cargo build/check --all-targets/las_stage1 all pass.
- Applied Royce's naming decision (this session): Rust drops LAS_ → N/ELL/GAMMA/KAPPA/N_PLUS_ELL, ring degree D; BOUND_SIGN→basesig, BOUND_PRESIGN→las. C keeps LAS_N + adds LAS_D alias (params.h N=256/D=13 collision).
- C mirror STARTED: setup.{h,c} done (7 typedefs, setup_public_params, params rename, LAS_D, BOUND_SIGN removed).

Files touched/inspected:
- rust/fips204-las/src/{setup,relation,serialize,basesig,las,lib}.rs
- ref/setup.{h,c}
- plus Rust tests/examples/benches + plan file, not listed to keep checkpoint short.

Evidence used:
- none (KAT run in-session: digest 641a176c…, matched)

Open risks:
- C mirror is large + unbuildable by Claude (Royce runs make); semantic split las_pk→{public_key|statement} etc. per-site.
- Stage B c_tilde changes wire size (D3 6752→6720) → new digest, both langs.

Next action:
- Write ref/relation.{h,c} (new), then serialize.{h,c}, basesig, las, amhl, chain, tests, Makefile.
