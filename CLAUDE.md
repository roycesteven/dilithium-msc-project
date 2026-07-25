# Project context — LAS on Dilithium for blockchain

## ⚠️ FOCUS — the primary comparison (READ FIRST; this has been gotten wrong repeatedly)

**The Stage-1 focus / primary comparison is BASIC SIGNATURE vs LAS ADAPTOR — the cost of
adding the adaptor layer to the base signature.** Everything in the results/evaluation
must lead with this. Precisely (`las-context-consolidated.md` §13.5, §7(e), §14.3):

- **Base-signature path:** Sign / Verify with `c = H(pk, w, M)` (no statement `Y`).
- **LAS adaptor path:** PreSign / PreVerify / Adapt / Extract with `c = H(pk, w+Y, M)`.
- Headline result = the **adaptor overhead**, reported **per operation** at the target
  setting **Simplified Dilithium-III**: PreSign vs Sign `+6.7%`, PreVerify vs Verify
  `+3.1%`, Adapt vs Verify `+8.1%`; Extract has no basic analogue. The two paths share
  algorithm/parameters/primitives, so the difference is purely the adaptor cost.

**NOT the focus — do not drift here:** the four parameter sets (paper / D2 / D3 / D5) are
ONLY a *secondary fairness / parameter-sensitivity* axis (§13.4). Do **not** frame the
results around "across security parameter", "as the scheme scales", or make the parameter
sweep the headline. The across-parameter overhead chart is supporting/appendix material,
**not** a primary body figure.

**Timing rule:** per-operation timing is the PRIMARY timing result (§14.3) — never lead
with cumulative / end-to-end time.

**Presentation rule (Royce):** no table↔chart redundancy (chart in body, exact-number
table in appendix); no abbreviations of scheme/level names in tables/figures.

## ⚠️ CANONICAL NAMING CONVENTION — Rust ⇄ C ⇄ paper (Stage-A seven-type layout; DO NOT DEVIATE)

The Rust port (`rust/fips204-las/src/`) is **DONE, PROVEN (Stage-B KAT digest
`bb6ad0da…260c`), and is the AUTHORITY**. The C mirror (`ref/`) must reproduce
these names **exactly** so the two languages stay faithful to each other and to
the paper↔code notation. Never invent a variant; if a name here looks wrong,
fix it against the paper (`docs/paper/LAS_2020_845_NOTATION.md`), not from memory.

**Construction parameters** (paper Section 3 / Table 1):

| paper | Rust (setup.rs) | C (setup.h) | value @ D3 |
|---|---|---|---|
| n (module rank) | `N` | `LAS_N` (kept; `-DLAS_N=` sweepable) | 6 |
| ℓ | `ELL` | `ELL` (`-DELL=`) | 5 |
| n+ℓ | `N_PLUS_ELL` | `N_PLUS_ELL` | 11 |
| d (ring degree) | `D` | `LAS_D` (`#define LAS_D N`, params.h N=256) | 256 |
| κ | `KAPPA` | `KAPPA` (`-DKAPPA=`) | 49 |
| γ = κ·d·(n+ℓ) | `GAMMA` | `GAMMA` | — |
| seed length | `LAS_SEEDBYTES` | `LAS_SEEDBYTES` | 32 |

C-only divergence: **only** `LAS_N` and `LAS_D` keep the `LAS_` prefix (params.h
already owns bare `N`=256 and `D`=13 for the reused Dilithium primitives). C
ring-degree loops in the **LAS files** read `LAS_D`; the reused Dilithium
primitive files keep bare `N`.

**The seven semantic types** (the six object types' physical home is now
las_types.rs / las_types.h; `public_params` stays in setup.rs / setup.h; each owned by one layer):

| paper object | Rust type {fields} | C type {fields} | owner |
|---|---|---|---|
| pp = A = [I\|A'] | `PublicParams { a_prime, seed }` | `public_params { a_prime, seed }` | setup |
| pk = t | `PublicKey { t }` | `public_key { t }` | basesig |
| sk = r | `SecretKey { r }` | `secret_key { r }` | basesig |
| σ = (c, z) | `Signature { c_tilde, z }` | `signature { c_tilde, z }` | basesig |
| σ̂ = (c, ẑ) | `PreSignature { c_tilde, z_hat }` | `pre_signature { c_tilde, z_hat }` | las |
| Y = t' | `Statement([R;N])`, `as_t_prime()` | `statement { t_prime }` | relation |
| y / s (witness) | `Witness([R;N_PLUS_ELL])`, `as_relation_vector()`/`from_relation_vector()` | `witness { value }` | relation |

`witness.value` is NEUTRAL storage: Gen's honest ternary r′ AND Ext's extracted
s (relation R′_A, may exceed norm 1). A statement is pk-shaped but is NOT a
public_key; a pre_signature is sig-shaped but NOT a signature — never cast/alias.

**Rejection bounds** (each owned by its scheme, NOT in setup):

| Rust | C | value |
|---|---|---|
| `basesig::BOUND_SIGN` | `BOUND_SIGN` (basesig.h) | γ−κ+1 |
| `las::BOUND_PRESIGN` | `BOUND_PRESIGN` (las.h) | γ−κ |
| — (dropped in Rust) | `BOUND_PRESIGN_K(K)` (las.h; AMHL-only, out of scope but kept as hook) | γ−κ−K+1 |

**Public function names** (paper/upstream ⇄ Rust ⇄ C):

- setup: `setup_public_params` (both).
- relation (Gen): `gen`/`gen_seed` ⇄ `relation_gen`/`relation_gen_seed`.
- relation_zk (π, paper §4.1/Fig. 1; ADDED 2026-07-19): `relation_zk::prove`/
  `relation_zk::proof_verify` (Rust relation_zk.rs) ⇄ `relation_prove`/
  `relation_proof_verify` (C relation_zk.c). Bridge (C-only TU, FFI'd by Rust
  build.rs): `relation_zk_lin_prove`/`relation_zk_lin_verify` (relation_zk_lazer.c).
  Gate names — NEVER rename: `PI_ROWS`/`PI_COLS`/`PI_DEG`/`PI_PROOF_MAX_BYTES`
  (both languages), params symbol `las_pi_params` (generated header
  relation_zk_params.h ⇐ scripts/las_pi_params.py), cargo feature `relation-zk`.
- basesig (Algorithm 1, Σ): `keygen`⇄`base_keygen`, `keygen_seed`⇄`base_keygen_seed`,
  `sign_internal`⇄`base_sign_internal`, `sign`⇄`base_sign`, `sign_det`⇄`base_sign_det`,
  `verify_internal`⇄`base_verify_internal`, `verify`⇄`base_verify`; packed
  `keygen_packed`/`sign_packed`/`verify_packed` ⇄ `base_keygen_packed`/
  `base_sign_packed`/`base_verify_packed`. (Old C `base_sign_keypair`,
  `base_sign_signature*`, `base_sign_verify*`, and the zero-caller sm-wrappers
  `base_sign`/`base_sign_open` are RENAMED/DELETED to these.)
- las (Algorithm 2): `presign_internal`⇄`las_presign_internal`, `presign`⇄`las_presign`,
  `presign_det`⇄`las_presign_det`, `preverify_internal`⇄`las_preverify_internal`,
  `preverify`⇄`las_preverify`, `adapt`⇄`las_adapt`, `ext`⇄`las_ext`; packed
  `las_presign_packed`/`las_preverify_packed`/`las_adapt_packed`/`las_ext_packed`.
  (`las_keypair*`/`las_signature*`/`las_verify*`/`las_sign`/`las_open` and their
  packed twins are DELETED from las.c — Algorithm 1 lives only in basesig.)
- **Gate names — NEVER rename:** `LAS_ATTEMPTS`/`las_attempts`,
  `BASE_ATTEMPTS`/`base_attempts`, `las_expected_attempts`, `LAS_SEEDBYTES`,
  `LAS_CTILDEBYTES` (Stage-B; the challenge digest width), serialize bit-widths
  `LAS_{PK,SK,Z}_COEFF_BITS` (`LAS_C_COEFF_BITS` is DELETED — the challenge is no
  longer bit-packed), `LAS_Z_OFFSET`, `LAS_Z_MAX`.

**Serialize sizes** (semantic `*_BYTES`, six typed pack/unpack pairs over 3 encoders):
`PUBLIC_KEY_BYTES`(4416) `SECRET_KEY_BYTES`(704) `SIGNATURE_BYTES`(6720),
`STATEMENT_BYTES`=pk, `WITNESS_BYTES`=sk, `PRE_SIGNATURE_BYTES`=sig. Pairs:
`pack_/unpack_` × {`public_key`,`statement`,`secret_key`,`witness`,`signature`,
`pre_signature`}. `las_verify_packed` is DELETED from serialize (moved to
basesig as `base_verify_packed`).

**Locked local conventions** (bit-for-bit, from the plan; unaffected by the rename):
vector split `x_0`/`x_1`; NTT-operand suffixes `_hat`/`_mont`; products `c_r`/`c_t`;
mask counter `mask_nonce`; recomputed challenge digest `c_tilde_check`
(Stage-B: verify byte-compares digests, not the old polynomial `c_check`); commitments
`w_prime`/`w_plus_t_prime`; object vars `sigma`/`sigma_hat`/`statement`/`witness`;
challenge sampler renamed to its upstream twin `sample_in_ball` (`las_` prefix is
reserved for the four Algorithm-2 public ops, not private helpers).

## Working agreement — token-saving mode

Use token-saving mode by default.

### Default session startup

Always read this `CLAUDE.md` first.

Then read only the **latest relevant section** of `PROGRESS.md` if the task continues previous repository work.

Do **not** automatically read all project documents at the start of every session.

Read these only when needed:

* `las-context-consolidated.md` — when the task needs supervisor objectives, meeting decisions, or project scope confirmation.
* `docs/STATUS.md` — when the task needs the current deliverable/test checklist.
* `docs/LAS.md` — when changing or checking design, benchmark interpretation, report-source text, or implementation claims.
* `docs/02-methodology/THEORY_IMPL_BRIDGE.md` — when checking paper-equation to C-function mapping.
* `docs/paper/LAS_2020_845_NOTATION.md` — when editing LAS labels, benchmark plots, report text, API/README explanations, or protocol comments (see the source-of-truth rule below).
* `README.md` — when checking build/reproducibility instructions.
* `docs/02-methodology/FUNCTION_MAP.md` — when checking reused/modified/new function classification.

If the user names specific files, inspect only those files first. Read additional files only if they are directly included, referenced, or necessary to answer the task accurately.

### Repository scanning

Do not scan the whole repository unless the task clearly requires it.

Prefer targeted inspection:

1. user-named files,
2. directly included headers/source files,
3. relevant evidence logs,
4. relevant documentation sections.

If a wider scan is needed, explain why before doing it.

### Documentation sync

Documentation must stay consistent with code, but do not update documentation for every read-only audit.

When implementation changes affect design, API behaviour, benchmark interpretation, report claims, or theory mapping, update the relevant documentation section.

When the task is only explanation, diagnosis, Git help, or read-only review, do not edit documentation unless explicitly requested.

### Subagent policy

Do not spawn subagents unless the user explicitly asks.

Subagents are allowed only for large independent audits, such as:

* benchmark evidence versus report claims,
* blockchain/gas/application-level audit,
* theory-versus-implementation audit,
* final report/rubric mapping.

Do not use subagents for:

* simple Git questions,
* explaining terminal output,
* small code edits,
* checking one or two files,
* writing short documentation patches.

### Checkpoint policy

Checkpoints must be short and append-only.

A checkpoint should include only:

* current branch,
* current objective,
* files inspected or changed,
* key decisions,
* evidence/logs used,
* unresolved risks,
* next exact action.

Do not include long reasoning, full transcripts, full diffs, or repeated background.

### Context management

If the task is continuing the same work, use `/compact` after saving a short checkpoint.

If the user switches to a different topic, recommend `/clear`.

Do not keep an 8+ hour session alive unless there is a clear reason.

## LAS paper source-of-truth rule

Before editing LAS labels, benchmark plots, report text, **report equations or
symbol/notation tables**, API documentation, README explanations, or comments
explaining the protocol, read **FIRST** (this is the crucial-information authority for
notation — do not decide symbols from memory):

`docs/paper/LAS_2020_845_NOTATION.md`

That file is the repo's curated working guide derived from `2020-845.pdf`. If it
conflicts with `2020-845.pdf`, the PDF wins. If a detail is not present in the
guide, check `2020-845.pdf` before editing. Do not invent paper notation, do not
rename variables casually, and mark uncertain details as TODO rather than
guessing.

**Report mathematical notation is governed by §3/§6/§7 of that guide, and the PAPER
(not the code) is the notation authority — never adopt a code symbol for report maths.**
Settled forms that must stay consistent across `report/latex/` (never introduce a variant,
never change one occurrence in isolation — fix every occurrence together):
- ring degree: the paper's **`d`** (this build runs at `d = 256`, the reused Dilithium
  NTT's ring degree). Use `d` in the report; do **not** substitute the code's `N`.
- rejection/masking bound: **`γ = κ·d·(n+ℓ)`** (paper form — `d`, not `N`; `(n+ℓ)` in
  formulas). `M = n+ℓ` may appear only as a table-column shorthand, not in equations.
- challenge weight **`κ` is per parameter set** (60 / 39 / 49 / 60 for paper/D2/D3/D5),
  *not* a fixed constant — never hard-code `κ=60` as global.

If a report symbol looks inconsistent, reconcile it to the paper across *every* occurrence
(body equations + `tab:params` + `tab:notation` + captions + figure labels), not just the
one in front of you. **Never justify report notation by what the C code names a variable.**

## Guardrails (standing — do NOT do any of these without an explicit instruction)
- **Do not create, delete, or switch git branches** unless explicitly requested.
- **Do not run benchmarks or tests** (no `make`, no executing benches/tests, no
  build-and-run) unless explicitly instructed. Edit code/docs and explain; let
  Royce run the build/bench himself, or wait to be told to.
- **Do not invent or estimate benchmark numbers.** Only ever report figures that
  came from a real, reproduced run; if a number isn't measured, say so — never
  fill a table with plausible-looking values.
- **Do not hand-edit evidence logs** (`evidence/*.log` and any saved measurement
  output). They are captured artefacts; regenerate them by running the tool, not
  by typing numbers.

## Output discipline

Be direct, evidence-based, and scoped.

For code audits, output:

1. Verdict
2. Evidence by file/function
3. Missing or misleading parts
4. Exact suggested fix
5. Whether editing is needed

Do not hallucinate. If the inspected files do not prove something, say so.

For benchmark/report claims:

* distinguish measured evidence from interpretation,
* cite the exact evidence log or source file,
* never invent missing numbers,
* flag stale or contradictory documentation.

For edits:

* first provide a read-only diagnosis unless the user explicitly asks for immediate editing,
* make the smallest possible change,
* show the diff,
* do not run tests or benchmarks unless explicitly requested.

## One-line goal
Implement LAS (Lattice-based Adaptor Signatures, eprint 2020/845) by reusing the
CRYSTALS-Dilithium reference primitives, then demonstrate it in a post-quantum
blockchain **atomic-swap** scenario, with everything benchmarked and documented.

## Status (living)
- ✅ **LAS implemented and tested** — `ref/las.{c,h}`, scheme **variant (B)** (the
  paper's Algorithm 2). `ref/test/test_las.c` passes 1000 iters (objectives' B1
  bar) on Dilithium modes 2/3/5, zero compiler warnings.
- ✅ **Atomic-swap demo** — `ref/test/test_swap.c`, **rewritten 2026-07-19 to paper
  §4.1 Fig. 1 VERBATIM**: witness holder u₁ (Alice) commits first, message order
  `{Y, π, σ̂₁, tx₁}` → `{σ̂₂, tx₂}`, Bob's π+PreVerify abort gate, byte-level
  tripwire via `pack_pre_signature`→`unpack_signature`, post-Ext asserts
  `‖y′‖∞ ≤ 1 ∧ y′ = y`. Opt-in target (needs LaZer, not in `make all`).
- ✅ **Fig. 1 proof of knowledge π (2026-07-19, Royce-directed)** —
  `ref/relation_zk.{c,h}` (relation layer: `relation_prove`/`relation_proof_verify`,
  non-ternary witnesses refused) + `ref/relation_zk_lazer.{c,h}` (the ONLY TU that
  includes `lazer.h`) over the **vendored LaZer** library (`third_party/lazer`,
  git-ignored, reused as-is — secp256k1-zkp posture). Ternary via binary
  decomposition `[A|−A|0]·(r₊‖r₋‖e)=t′` (23rd dummy ℓ2 column: codegen requires
  one ℓ2 partition). Params `ref/relation_zk_params.h` COMMITTED (generated from
  `scripts/las_pi_params.py` by LaZer `sage lin-codegen.sage`; knowledge error
  ≤2⁻¹²⁷ MSIS, ZK MLWE); measured π ≈30.7 KB, off-chain only. Tests
  `test/test_zkp3` + rewritten `test_swap3` (both opt-in). **Rust twin**:
  `src/relation_zk.rs` (`relation_zk::prove`/`proof_verify`) + `build.rs`, cargo
  feature `relation-zk` (default off, KAT gate intact), FFI onto the SAME C
  bridge; tests `las_zkp.rs`/`las_swap.rs`. Docs: `docs/LAS.md §7.6, §5.12, §6.5`,
  `THEORY_IMPL_BRIDGE.md §12.6`, `FUNCTION_MAP.md §3.7`, STATUS D25.
- ✅ **Realistic chain integration** — `ref/chain.{c,h}` (scriptless-HTLC ledger:
  accounts, block height, adaptor-locked contracts with claim + timeout-refund) +
  `ref/test/test_pcn.c` (atomic-swap happy path, timeout/refund, multi-hop PCN).
  **Model:** same-Y scriptless HTLC baseline (all hops share one statement);
  the distinct-statement AMHL is implemented separately (next bullet).
- ✅ **Benchmarks** — `ref/test/bench_las.c` (per-op timings + rejection rate
  measured *directly* via the `las_attempts` counter), `ref/test/bench_compare.c`
  (LAS vs Dilithium-3), and `ref/test/bench_app.c` (application level: swap
  payload + AMHL cost vs path length K, simulated-ledger proxy, not gas).
  Measured: Sign≈790µs, Verify≈190µs, PreSign≈815µs, PreVerify≈195µs, Adapt≈205µs,
  Ext≈65µs. Acceptance ≈37% per attempt (~2.7 attempts/sig), matching the
  `(1−κ/γ)^{(n+ℓ)·N} ≈ e^{−1}` theory. (An older ~23% figure came from a biased
  timing-ratio estimate — superseded, see `docs/LAS.md §8`.)
  Sizes: in-memory sig=8224B; **measured packed (serialize.c)=4640B** (wire =
  `c_tilde ‖ BitPack(z)`); paper's optimised=~3210B (different scheme — not directly comparable).
- ✅ **Full design write-up** — `docs/LAS.md` (report source material, includes
  literature/methodology section §1.1 for assessment rubric).
- ✅ **Theory↔implementation bridge** — `docs/02-methodology/THEORY_IMPL_BRIDGE.md` (every paper
  equation mapped to C function/line).
- ✅ **Code pushed to GitHub** — on branch `main`, up to date with `origin/main`.
  PR #1 merged. No unpushed commits.
- ✅ **AMHL (multi-hop locks)** — `ref/amhl.{c,h}` + `ref/test/test_amhl.c`
  (`make test/test_amhl3`). Distinct per-hop cumulative statements
  `Y_j = A·(l_1+…+l_j)`, PreSign bound `γ−κ−K` (`las_presign_k`/`las_preverify_k`,
  macro `LAS_BOUND_PRESIGN_K`), `chain_fund_swap_k`. Demo hard-asserts wormhole
  resistance, witness-norm growth `‖s_j‖∞≤j`, exact cascade recovery, and a
  timeout/refund path. `test_pcn.c` retained as the same-Y baseline.
  See `docs/LAS.md §7.5` and `docs/02-methodology/THEORY_IMPL_BRIDGE.md §12.5`.
  **Meeting-2 note:** AMHL was re-classified as *optional/bonus* — it happens to
  be done already, but it must not displace Stage-1/2 + benchmark work.
- ✅ **Serialization + byte-level verifier** — `ref/serialize.{c,h}` +
  `ref/test/test_serde.c`. Bit-packed wire encoding (pk 2944B, sk/witness 512B,
  sig 4640B — measured, not formulas; wire = `c_tilde ‖ BitPack(z)`), *validating*
  pk/sk decoder (rejects coeff≥Q, non-ternary code; c_tilde/z decode permissively,
  tamper caught at Verify), and `base_verify_packed` = the byte interface an
  on-chain verifier consumes. Tamper test: all 4640 single-byte flips rejected.
  See `docs/LAS.md §5.10, §6.3`.
- ✅ **Deterministic API + pinned KATs (C4)** — `base_keygen_seed` / `base_sign_det`
  / `las_presign_det` (mask seed = `SHAKE256(tag‖sk‖[Y]‖M)`; shared
  `base_sign_internal`/`las_presign_internal` with the randomised paths) + `ref/test/test_kat.c`
  with a pinned SHAKE256 digest over 4 fully-deterministic vectors. Reproducible
  across runs/machines; cross-check anchor for any future on-chain verifier.
  See `docs/LAS.md §5.11, §6.4`.
- ✅ **Classical adaptor baseline (Meeting-2 B2.ii)** — `ref/test/bench_classical.c`
  (`make test/bench_classical`; needs one-time clone of
  `third_party/secp256k1-zkp`, git-ignored, commit `95b9835`). ECDSA-adaptor
  measured same-machine: KeyGen 31µs, Sign 41, Verify 62, PreSign 189,
  PreVerify 244, Adapt 3, Ext 35; sizes pk 33B / sig 64B / pre-sig 162B.
  2×2 "price of post-quantum" table + analysis in `docs/LAS.md §8.3`.
- ✅ **Reproducibility entry point + function map (Meeting-2 B5 deliverables)** —
  `README.md` (build/run/reproduce; upstream commit hash `2374d22` +
  toolchain recorded, B5.1) and `docs/02-methodology/FUNCTION_MAP.md` (every Dilithium function
  classified call-as-is / modify / new; headline: **zero upstream functions
  modified**, B5.4 — also the report's "reused vs modified vs added" table, B4).

## Why this project exists
- Blockchains sign with ECDSA/Schnorr; Shor's algorithm breaks both. "Post-quantum"
  = built on lattice/hash problems Shor can't solve.
- NIST standardised *basic* PQ signatures (Dilithium, Falcon, SPHINCS+).
- *Exotic* signatures (multisig, ring, group, **adaptor**) add features but in the
  PQ setting are mostly **paper-only** — little working code, none on a blockchain.
  Closing that gap is the thesis.
- Adaptor signatures enable atomic swaps / payment channels (scriptless scripts).

## Key design fact
An exotic scheme = a basic scheme + extra functions. LAS = Dilithium-style
Fiat-Shamir-with-aborts signature + PreSign / PreVerify / Adapt / Ext. We **reuse
Dilithium's poly/NTT/SHAKE/sampling internals** and do not reinvent lattice
arithmetic. LAS itself is built as a small *self-contained* scheme (its own
dimensions and parameters) layered on those primitives.

## The LAS mechanism (variant B — the paper, Algorithm 2)
Earlier notes described a "variant A" (`z̃ = z + y`, statement subtracted at
verify). That was **superseded**: the paper specifies variant B, implemented here.
- Statement/witness `(Y, y)` is **literally another key pair**: `y ← S_1^{n+ℓ}`
  (ternary), `Y = A·y`. Knowing `Y` doesn't reveal `y` (Module-SIS/LWE hard).
- **The core mechanism: the statement is folded into the Fiat–Shamir hash.**
  Sign uses `c = H(pk, w, M)`; **PreSign uses `c = H(pk, w + Y, M)`**.
- `PreSign(sk,Y,M)`: `ẑ = y + c·r`, reject if `‖ẑ‖∞ > γ−κ−1`. Pre-sig `σ̂=(c,ẑ)`.
- `PreVerify(Y,pk,σ̂,M)`: recompute `w' = Aẑ − c·t`, check `c == H(pk, w'+Y, M)`.
- `Adapt((Y,y),σ̂)`: `σ = (c, ẑ + y)`. Now standard `Verify` sees `Az−ct = w+Y`,
  which matches `c` — so the adapted signature is a **fully ordinary** signature.
- `Ext(Y,σ,σ̂)`: `y = z − ẑ`; return it iff `A·y == Y`.
- **On-chain leak (why swaps are atomic):** publishing the adapted `σ` lets anyone
  holding `σ̂` recover `y = z − ẑ` and complete the matching half of the swap.

## THE failure mode to watch (variant B)
The bound budget, not packing. PreSign rejects at the **tighter** `γ−κ−1`; the
ternary witness has `‖y‖∞ ≤ 1`, so the adapted `z = ẑ + y` satisfies
`‖z‖∞ ≤ γ−κ` and clears ordinary Verify. If you loosen PreSign to `γ−κ`, adapted
signatures can exceed the bound and Verify rejects everything. (`γ = κ·d·(n+ℓ)`
governs the MSIS hardness parameter; the acceptance rate is ≈37% per attempt
(~2.7 attempts/sig, `≈ e^{−1}`) — expected for the simplified scheme without
hint vector, and measured directly via the `las_attempts` counter.)

## Known caveat (note in thesis, do NOT need to solve)
"Knowledge gap": here the extracted `y` is **exact**; in the paper's relaxed
setting the witness can carry noise that grows across long payment-channel chains.

## Modulus note
Paper uses `q ≈ 2^24`. We reuse Dilithium's NTT, whose root-of-unity table is
fixed to `Q = 8380417 (≈2^23)`, so this build uses that `Q`. `Q > 2γ`, so
correctness holds; only the concrete MSIS/MLWE security margin changes (out of
scope per supervisor). Exact `2^24` would need a new NTT table or schoolbook mult.

## Scope discipline (from supervisor — Meeting 2, 2026-06-08; see `las-context-consolidated.md`)
- Target dilithium3 build (NIST level ~2/3) — LAS code is mode-independent and is
  built/tested under `-DDILITHIUM_MODE=3` (also 2/5 for portability).
- Do NOT implement/analyse security proofs. Implement + benchmark + demo only.
- **Two-stage spine (official):** Stage 1 = standalone LAS + benchmark vs pure
  Dilithium ✅; Stage 2 = blockchain application (atomic swap / fair exchange) ✅ at
  the simulated-ledger level — but **Stage 2's target chain changed at Meeting 7,
  see the pivot block below.**
- **Benchmarks now need TWO baselines (B2):** (i) LAS vs pure Dilithium ✅
  (`bench_compare`); (ii) LAS vs **classical adaptor signature** ✅
  (`bench_classical` — libsecp256k1-zkp `ecdsa_adaptor`, vendored at `95b9835`,
  reused as-is and measured on the same machine; full 2×2 in `docs/LAS.md §8.3`).
  Headline: PQ price is communication (×29–89 sizes), not computation; LAS's
  adaptor overhead ≈0 vs classical's ~4× (DLEQ); LAS PreVerify absolutely faster.
- **Parameters:** stay on Dilithium's `q=8380417≈2²³` for now (supervisor-
  sanctioned starting point); migration to the paper's `q≈2²⁴` is a *later,
  documented* step with before/after benchmarks — optional if justified.
- **Optional tier only after Stages 1–2 + both baselines are airtight:**
  AMHL ✅ (already done — counts as bonus), on-chain LAS *verification*
  (precompile/zk; swap+gas floor already done), parameter migration to q≈2²⁴.
  **Focus: LAS only** — no alternative-PQ-scheme comparison in scope per Royce.
- **Hard out-of-scope (supervisor):** Ethereum-consensus multisigs, blind/group
  signatures, heavy ZKP/MPC — one related-work paragraph max. **EXCEPTION
  (Royce-directed, 2026-07-19): the Fig. 1 proof of knowledge π IS implemented**
  (via the vendored LaZer library, not hand-rolled) — see the π status bullet;
  do not re-flag it as out-of-scope.
- **TODO: report draft** — supervisor-confirmed skeleton (B4): high-level design →
  function map (✅ `docs/02-methodology/FUNCTION_MAP.md`) → key decisions → benchmark results
  (both baselines) → critical analysis; code snippets only in appendix.
  ~8000 words from `docs/LAS.md`.

## ⚠️ MEETING-7 PIVOT (2026-07-24) — Stage 2 retargets from the EVM to Bitcoin/UTXO

Authority: `las-context-consolidated.md` §16 · transcript `meeting7_cleaned_transcript.md`.
**Read §16 before planning any application work.** Summary of what changed:

- **Target chain.** The Stage-2 application moves from a smart-contract chain to
  **Bitcoin / a UTXO-based chain**. Reason (Wang): native on-chain LAS verification is
  infeasible against the gas limit, and adaptor signatures are used in practice for
  swaps on UTXO chains, not smart-contract chains. Bitcoin has no gas limit — only
  transaction fees — and the heavy work stays off-chain.
- **The EVM work is NOT retracted.** The measured ≈56.5 M-gas native verifier and the
  Naysayer variant are retained as *the evidence for why* the UTXO venue was chosen.
  EVM is deferred to "if we have time".
- **Deliverable = three configurations**, built by reusing a maintained classical
  atomic-swap repo's architecture and replacing its cryptography (signatures first,
  ZKP second): (1) classical adaptor + Groth16, (2) **LAS + Groth16**, (3) **LAS +
  LaZer**. Benchmark all three.
- **Metrics change: gas → time + communication cost**, with off-chain protocol
  messages counted, plus the usability finding (heavy pre-transaction computation may
  need a dedicated PC rather than a phone).
- **Permitted simplifications:** no real sockets (pass messages directly); π stays
  off-chain; refund/timeout are edge cases (honest path first); packing in the swap
  path is optional — if too slow, omit and record as a limitation. It is an
  exploration/demo, not a product.
- **Report rulings applied:** evaluation is its own chapter; critical reflection lives
  in Chapter 5 as its own section (achieved / fell short / would do differently); the
  rejection figure is now a **cumulative acceptance curve**, not P(exactly k attempts).

## Reference
- **Objectives (authoritative):** `las-context-consolidated.md` (Meetings 1+2 merged).
- **Live status / test checklist:** `docs/STATUS.md`.
- LAS paper: eprint 2020/845 (Esgin, Ersoy, Erkin).
- poqeth (integration template): eprint 2025/091.
- Full design + math + results: `docs/LAS.md` · theory↔code: `docs/02-methodology/THEORY_IMPL_BRIDGE.md`.
- Reproducibility: `README.md` · function classification: `docs/02-methodology/FUNCTION_MAP.md`.

## Assessment Criteria and Rubric: see `MSc_Report_and_Video_Rubric.md`
