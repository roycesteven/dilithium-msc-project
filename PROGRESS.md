

## Checkpoint — 2026-07-27 12:48

Branch: report

Current goal:
- Build a fully-PQ succinct proof (Winterfell FRI-STARK, no Groth16) for on-chain LAS verification, in new standalone crate rust/las-stark.

Done:
- Stage A: sound STARK norm gadget ||z||inf<=B (real proof, tested).
- Stage A.2 groundwork: native byte-exact full-relation spec (SampleInBall + w' + SHAKE256 challenge) vs C goldens.
- Stage A.2 convolution: sound single negacyclic-conv STARK (b range-checked) at reduced CONV_D=64; cargo test 7/7 green.

Files touched/inspected:
- rust/las-stark/src/conv_air.rs
- rust/las-stark/src/{air,prover}.rs
- rust/las-stark/src/{relation,hashing,vectors,params,lib}.rs
- rust/las-stark/tests/las_stark.rs
- rust/las-stark/{Cargo.toml,README.md}

Evidence used:
- none

Open risks:
- d=256 conv blocked by Winterfell 255-column cap; needs narrow layout (streaming MAC + LogUp lookup, or NTT-transform) + shared z_bot across 30 A'.z_bot convs.
- Hashes (SampleInBall/SHAKE256) not yet in-AIR, so z not yet bound to c_tilde; still a gadget, not a succinct proof of verification.

Next action:
- Implement narrow d=256 convolution (streaming MAC + LogUp lookup) sharing one z_bot to assemble w'.

## Checkpoint — 2026-07-27 (later)

Branch: report

Current goal:
- Narrow d=256 arithmetic proof for on-chain LAS verification (rust/las-stark).

Done:
- rust/las-stark/src/relation_air.rs: NEW STARK proving base_verify constraints (1) ||z||inf<=B
  AND (3) w'_m = z_top[m] + sum_j A'[m][j](x)z_bot[j] - c(x)t[m] at the REAL d=256, all n=6 outputs
  bound to ONE shared z. Narrow layout (4096 rows x 135 main + 15 aux cols) instead of the
  d-wide window that hit Winterfell's 255-column cap.
- Method: random-evaluation (Schwartz-Zippel) argument on Winterfell's AUX trace segment
  (randomness drawn after main commitment). Prover commits z + integer quotients h_m (by X^d+1)
  and g_m (by q); aux segment Horner-evaluates at random x and checks
  sum_m rho_m [P_m(x) - (x^d+1)h_m(x) - q g_m(x)] = 0. Range checks |z|<=B, |h|<2^51, |g|<2^29
  are load-bearing (they lift the F_p identity to Z; unbounded g would make it vacuous).
- 5 new tests in tests/las_stark.rs (witness exists / rejects tampered w', proof round-trip,
  rejects tampered public inputs, rejects tampered proof).
- conv_air.rs (CONV_D=64) demoted to "schoolbook reference, superseded"; README rewritten;
  lib.rs module docs updated; STAGE2_UTXO_SWAP_PLAN.md role-B cell updated.

Files touched:
- rust/las-stark/src/relation_air.rs (new), src/lib.rs, tests/las_stark.rs, README.md
- docs/02-methodology/STAGE2_UTXO_SWAP_PLAN.md

Evidence used:
- none (no benchmarks run)

Verification status:
- `cargo check --tests` clean, no warnings.
- `cargo test --release` = 12/12 PASS (was 7/7 before this change), incl. all 5 new
  relation-AIR tests. No fixes were needed. No benchmark numbers taken.

Open risks:
- Plain `cargo test` (debug) is NOT the supported command: Winterfell's debug-assertion
  "declared degree == measured degree" self-check would trip, since the declared aux
  degrees are deliberately upper bounds. Use --release (README says so).
- Hashes still out of the AIR: c and w' are PUBLIC inputs, so z is bound to (A',t,c,w') but
  NOT to (c_tilde, M). Still not a stand-alone signature-verification proof.
- Public inputs enter as 44 periodic columns (cycle d) => verifier work linear in |public|;
  fine in Rust, would want A' behind a commitment for an on-chain verifier.

Also done (same session, Royce chose option 2):
- rust/las-stark/src/bin/prove_relation.rs + verify_relation.rs (registered in Cargo.toml).
  prove_relation prints trace shape (rows x main+aux cols), public-input size in field
  elements, proof size, and a PHASE SPLIT of prove time (quotient witness / trace build /
  STARK). verify_relation rebuilds the public inputs from the same goldens and prints
  verify time. Both print the "NOT proven: (2) SampleInBall, (4) SHAKE256" caveat.
  `cargo check --release --bins` clean.
- src/bin/bench_stark.rs = THE BENCHMARK OF RECORD + scripts/run_stark_bench.sh writes
  evidence/stark/<ts>/{bench_stark.log,environment.txt} + latest symlink (stage2 layout).
  Protocol aligned with the project (las-context §13.3, §15.6): 3 s DISCARDED WARM-UP,
  then 5 timed reps, mean +- sample (n-1) SD, one machine, one process. Warm-up replaces
  the inner 500/1000-iteration loop the us-scale drivers use (a 0.4 s op needs no timer
  amortisation, it needs a warm cache/allocator).
- MEASURED (evidence/stark/20260729_142953), both AIRs in ONE process, same protocol:
    NormAir     (1) only : prove TOTAL 110.821 +- 0.212 ms, verify 0.470 +- 0.020 ms, 52,876 B
    RelationAir (1)+(3)  : prove TOTAL 442.157 +- 4.501 ms, verify 1.319 +- 0.042 ms, 98,419 B
      phases: witness 1.993, trace 1.306, prover ctor 0.007, Winterfell prove() 438.851
    RATIO: prove 3.99x, verify 2.81x, size 1.86x.
- TWO EARLIER CLAIMS WERE WRONG AND ARE RETRACTED:
    (a) "~1.5-1.6x prove time" came from cold single shots + non-like-for-like spans.
        prove_norm internally does trace build + prover ctor + prove, so it must be
        compared against a RelationAir TOTAL covering the same steps. Correct: 3.99x.
    (b) "FRI is the entire cost" -- Winterfell prove() is trace LDE + Merkle commits +
        constraint evaluation + DEEP + FRI. Nothing here attributes cost to FRI alone.
    Survives: witness+trace+ctor = 3.3 ms = 0.75% of total (arithmetising is ~free).
- Also fixed: pub_inputs.clone() (176 KB) was INSIDE the verify timer in prove_relation
  and verify_relation; hoisted out of the timed region in all three binaries.
- On-chain: NO gas figure claimed. The 98,419 B proof is not the whole payload -- the
  11,008 public field elements also reach the verifier, of which A' (7680, fixed params)
  and t (1536, signer public key) are reusable/committable; only c (256) and w' (1536)
  are signature-specific.
- README Build & run section reordered: the relation CLIs are now step 2/3 (the main
  result), the norm gadget demoted to step 4.

Next action:
- Royce runs: cd rust/las-stark && cargo run --release --bin prove_relation
                                 && cargo run --release --bin verify_relation
  -> gives the quotable d=256 proof size + prove/verify time for the report.
- Then: in-AIR Keccak-f for SampleInBall + SHAKE256 challenge (binds z to c_tilde, M). Large.
