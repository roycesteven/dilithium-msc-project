# las-stark — a hash-based STARK toward post-quantum on-chain LAS verification

A transparent, hash-based **FRI-STARK** toolkit (built on
[Winterfell](https://github.com/facebook/winterfell)) targeting the LAS on-chain
verification relation (eprint 2020/845).

> **Status: work in progress — NOT yet a succinct proof of on-chain verification.**
> Today this crate proves, as a real STARK, the **arithmetic core** of `base_verify`
> at the real ring degree `d = 256` — constraints (1) `‖z‖∞ ≤ B` and (3)
> `w' = z_top + A'·z_bot − c·t`, for all `n` output polynomials bound to one shared
> `z` (`src/relation_air.rs`). The Fiat–Shamir hashes — (2) `SampleInBall` and (4)
> the `SHAKE256` challenge — are **not** in the AIR: `c` and `w'` are still public
> inputs. So `z` is bound to `(A', t, c, w')` but **not** to `(c̃, M)`. That, and the
> on-chain EVM verifier, are roadmap (below), not done.

## Goal (motivation, not current state)

A numerically-complete native LAS verifier in Solidity (`evm/src/LASVerifier.sol`)
costs **≈56.5M gas** — above EIP-7825's **16,777,216** per-transaction cap, so it
cannot run on-chain as one transaction. The documented fixes are a precompile, an
optimistic (Naysayer) scheme, or a **succinct proof**. A Groth16/pairing wrap would
make a succinct proof cheap on-chain **but is not post-quantum**, defeating the point
of a PQ signature. This crate pursues the *post-quantum* succinct-proof route.

**Why a FRI-STARK is the PQ choice (stated precisely).** A FRI-STARK is *plausibly
post-quantum*: its soundness rests on the collision-resistance of the hash (Blake3)
**and** the conjectured soundness of low-degree testing (FRI) / Reed–Solomon
proximity, analysed in the random-oracle model. It uses **no** elliptic-curve or
pairing assumptions — that is the property a Groth16 wrap would lose. (Concrete
security is also bounded by the field size and query count; see STARK parameters.)

## What is proven today — and what is not

**Proven (real, end-to-end STARK, `src/relation_air.rs`):** the **arithmetic core of
`base_verify` at the real ring degree `d = 256`** — constraints (1) and (3) together:

```text
(1)  ‖z‖∞ ≤ B                              (B = γ − κ = 137 935)
(3)  w'_m = z_top[m] + Σ_j A'[m][j] ⊛ z_bot[j] − c ⊛ t[m]   (mod q, negacyclic)
```

for **every** `m < n`, with `A'`, `t`, `c`, `w'` public and one **shared** private `z`
((n+ℓ)·d = 2816 coefficients). The 30 `A'·z_bot` convolutions are bound to the *same*
`z_bot` by construction — each `z_bot[j](x)` is evaluated once and reused by all six
output equations — which is exactly the cross-convolution binding a per-convolution
gadget cannot impose.

**How it stays narrow (this is what got past the 255-column cap).** The AIR never
materialises a convolution. It runs a **random-evaluation (Schwartz–Zippel) argument**
on Winterfell's **auxiliary trace segment**, whose random elements are drawn *after*
the main trace is committed. The prover commits to `z` and to integer quotient
polynomials `h_m` (by `X^d+1`) and `g_m` (by `q`) satisfying, over ℤ,

```text
P_m(X) = z_top[m] + Σ_j A'[m][j]·z_bot[j] − c·t[m] − w'[m] = (X^d+1)·h_m(X) + q·g_m(X)
```

then the aux segment Horner-evaluates every polynomial at the verifier's random `x`
and checks the single scalar equation `Σ_m ρ_m·[P_m(x) − (x^d+1)h_m(x) − q·g_m(x)] = 0`.
Trace: **4096 rows × 135 main + 15 aux columns** at `d = 256`, versus a `d`-wide
window that needs ≥ 256 columns for the same degree.

**Why the range checks are load-bearing (do not relax them).** A field identity over
`F_p` implies the *integer* identity only if every coefficient stays below `p/2`. The
AIR enforces `|z| ≤ B` (tight — this *is* constraint (1)), `|h| < 2^51` and
`|g| < 2^29`, so every coefficient of the residual is `≲ 2^53 ≪ p/2 ≈ 2^63` and the
identity lifts to ℤ. **Without a bound on `g` the argument would be vacuous**: `q` is
invertible mod `p`, so an unbounded `g` satisfies the equation for *any* claimed `w'`.
Soundness error: `6/|E|` for the `ρ`-combination plus `510/|E|` for Schwartz–Zippel,
with `|E| ≈ 2^128` (quadratic extension of Goldilocks).

**Not proven (known gap — do not overclaim).** Constraints (2) `c = SampleInBall(c̃)`
and (4) `c̃ = SHAKE256(pack(t) ‖ pack(w') ‖ M)` are **not** in the AIR — `c` and `w'`
are taken as public inputs. So `z` is bound to `(A′, t, c, w')` but **not** to
`(c̃, M)`; a caller must still be given `c` and `w'` and trust them. This is therefore
**a proof of the lattice arithmetic of verification, not of the Fiat–Shamir chain**,
and not yet a stand-alone signature-verification proof.

`src/air.rs` (`NormAir`) remains as the standalone Stage-A range-check gadget for
constraint (1) alone; `relation_air` subsumes it (it enforces the same tight `‖z‖∞ ≤ B`
with the same two 19-bit decompositions, over the same 2816 coefficients).

### Measured

Benchmark of record: `./scripts/run_stark_bench.sh` → `evidence/stark/latest/`.
Protocol, aligned with the rest of the project (`las-context-consolidated.md` §13.3,
§15.6): **3 s discarded warm-up, then 5 timed repetitions, mean ± sample (n−1) SD**,
one machine, one process. Both AIRs are measured back to back in that one process, so
the comparison between them is controlled. Golden vector `‖z‖∞ = 137 885 ≤ B`.

Figures below are from the run after the `c_tilde` FIPS 204 alignment (48-byte digest,
6736-byte signature); the golden vectors were regenerated first, so `c`, `w'` and `z`
all differ from the pre-alignment run.

| quantity | NormAir — (1) only | RelationAir — (1)+(3), `d = 256` |
|---|---|---|
| trace | 4096 × 39 (single segment) | 4096 × 135 main + 15 aux |
| public inputs | 1 field element | 11 008 field elements |
| prove **total** | **110.033 ± 0.418 ms** | **435.651 ± 7.421 ms** |
| verify | **0.470 ± 0.043 ms** | **1.259 ± 0.044 ms** |
| proof size | 53 292 B | 98 384 B |

**Cost of proving (1)+(3) rather than (1) alone: prove 3.96×, verify 2.68×, proof size
1.85×.** The prove ratio is taken on totals spanning the *same* steps: `prove_norm`
internally builds its trace and constructs its prover, so it is compared against a
RelationAir timer covering witness build + trace build + prover construction + `prove()`,
not against `prove()` alone.

Two things this measurement corrects, both of which changed the answer materially:

- **Warm-up matters more than the AIR does here.** Cold single-shot runs gave ≈315 ms
  for NormAir and ≈516 ms for RelationAir; warm, the same code gives 110.8 ms and
  442.2 ms. The earlier cold figures also produced a *wrong* ratio (≈1.5×) — the
  controlled measured figure is **3.99×**. SD fell from ~48% of the mean to ~1%.
- **`prove()` is not FRI.** It is the entire Winterfell prover: trace low-degree
  extension, Merkle commitments, constraint evaluation over the LDE domain, DEEP
  composition *and* FRI. Attributing its cost to FRI alone is unsupported by anything
  measured here.

What *is* supported: witness + trace + prover construction together are 3.3 ms, i.e.
**0.75% of the total** — arithmetising the relation is nearly free; the STARK prover
is essentially the whole cost.

**On-chain cost is not measured and no gas figure is claimed.** Note also that the
98 419-byte proof is not the whole on-chain payload: the 11 008 public field elements
(`A'`, `t`, `c`, `w'`) must also reach the verifier. Of these, `A'` (7680) is fixed
public parameters and `t` (1536) is the signer's public key — both reusable, so both
could be committed or stored once rather than sent per verification. Only `c` (256)
and `w'` (1536) are signature-specific. Sizing that payload, and its gas, is Stage-B
work that has not been done.

## Stage A.2 groundwork: native full-relation spec (done, byte-exact — but native, not a STARK)

The *complete* `base_verify` relation is reproduced **natively** and validated
against the C golden vectors — `src/relation.rs` (`check_full_relation`) +
`src/hashing.rs` check all of:

- (2) `SampleInBall(c̃) == c` (golden `c.bin`);
- (3) `w' == z_top + A'·z_bot − c·t` (golden `w_prime.bin`);
- (4) `SHAKE256(pack(t) ‖ pack(w') ‖ M) == c̃` (golden `c_tilde`).

This is the authoritative **spec and trace generator** the AIR must match. It is
**not** a STARK proof — it is plain Rust that recomputes the relation. Turning it
into an AIR is the remaining Stage-A.2 work (roadmap).

## Schoolbook convolution gadget (`conv_air.rs`, superseded — kept for reference)

`src/conv_air.rs` is a **real, sound, tested** STARK for ONE negacyclic
convolution `p = a ⊛ b` in `Z_q[X]/(X^d+1)`, done the direct way: it carries the
negacyclically-rotated window of `b` as trace columns (uniform degree-1 rotation
constraints), reduces mod `q` with a range-checked signed quotient, and
range-checks `bwin[0] ∈ [−(Q−1), Q−1]` every row so `b` is a valid ring element.

It runs at `CONV_D = 64`, **not** the LAS degree `d = 256`, because the window
costs `d` columns and Winterfell 0.13 caps a trace at **255**; and it binds only
one convolution, so it could not force the 30 `A'·z_bot` products to share one
`z_bot`. Both limits are what `relation_air.rs` removes. It is kept as the
schoolbook cross-check of the arithmetic (tested: `conv_gadget_roundtrip`,
`conv_gadget_wrong_output_rejected`) and is **not** part of the current claim.

## Layout

| file | role |
|---|---|
| `src/params.rs` | LAS D3 constants (mirrors `rust/fips204-las/src/setup.rs`) |
| `src/vectors.rs` | loaders for the C golden vectors in `evm/test/vectors/` |
| `src/relation.rs` | native oracle: `w' = z_top + A'·z_bot − c·t` + full relation, checked vs C golden |
| `src/hashing.rs` | native SampleInBall + SHAKE256 challenge digest, checked vs C golden |
| `src/air.rs` | the `NormAir` range-check AIR (bound + length guards) |
| `src/conv_air.rs` | schoolbook negacyclic-convolution AIR `p = a ⊛ b` (reduced degree; superseded) |
| `src/relation_air.rs` | **the current claim**: constraints (1)+(3) at `d = 256`, shared `z`, narrow aux-segment layout; `prove_relation` / `verify_relation` |
| `src/prover.rs` | Winterfell prover/verifier wiring; `prove_norm` / `verify_norm` |
| `src/bin/{prove,verify}.rs` | norm-gadget CLIs: emit/consume `proof.bin`, print size + timing |
| `src/bin/{prove_relation,verify_relation}.rs` | relation-AIR artefact CLIs: emit/consume `relation_proof.bin` |
| `src/bin/bench_stark.rs` | **the benchmark of record**: both AIRs, one process, 3 s warm-up + ≥5 reps, mean ± SD, like-for-like totals; captured by `scripts/run_stark_bench.sh` into `evidence/stark/` |
| `tests/las_stark.rs` | oracle/full-relation-native, norm round-trip + tamper/wrong-bound, conv round-trip + tamper, relation witness + round-trip + tampered-public-input/tampered-proof rejection |

This is a **standalone** crate — it is **not** a member of the KAT-locked `fips204`
package, so the LAS Rust port's KAT gate and MSRV 1.70 are untouched.

## STARK parameters

Blake3 hash + FRI (transparent). Base field: 64-bit Goldilocks with a quadratic
extension for FRI/DEEP soundness. `proof_options()` in `src/lib.rs`: 32 queries,
blowup 8, grinding 0, FRI folding 8 — ~96-bit conjectured security for this config.

## Build & run (you run these — nothing here is benchmarked or fabricated)

```sh
# 0. ensure the golden vectors exist (deterministic export from the C reference)
cd ref && make test/export_verify_vector && ./test/export_verify_vector ../evm/test/vectors

# 1. tests: full-relation-native, oracle-vs-golden, prove/verify round-trip, tamper +
#    wrong-bound reject, and the d=256 relation AIR (witness, round-trip, tamper reject).
#    Use --release: proving is far faster, and it also switches off Winterfell's
#    debug-assertion degree self-check (see the [profile.test] note in Cargo.toml).
cd ../rust/las-stark && cargo test --release

# 2. THE BENCHMARK OF RECORD: both AIRs, one process, 3 s warm-up + 5 reps,
#    mean +/- SD. Writes evidence/stark/<timestamp>/ and updates evidence/stark/latest.
./scripts/run_stark_bench.sh               # from the repo root

# 2b. artefact CLI: prove constraints (1)+(3) at d = 256 and write the proof out
cargo run --release --bin prove_relation   # writes relation_proof.bin

# 3. verify it against the public (A', t, c, w') rebuilt from the same goldens
cargo run --release --bin verify_relation  # reads relation_proof.bin

# 4. the standalone Stage-A norm gadget only (constraint (1) alone)
cargo run --release --bin prove            # writes proof.bin
cargo run --release --bin verify           # reads proof.bin
```

Every number those binaries print is measured on the run that printed it. Nothing in
this crate is benchmarked into `evidence/`, and no figure here is estimated.

## Roadmap (not done)

- **Stage A.2 (hashes)** — an **in-AIR Keccak-f** for `SampleInBall` and the
  `SHAKE256` challenge, so `z` is bound to `(A′, t, c̃, M)` rather than to
  `(A′, t, c, w')`. Substantial. Only when the arithmetic *and* the hashes are
  in-AIR is there a succinct *proof of verification*.
- **Public-input cost (relevant to Stage B, not to soundness)** — `A′`, `t`, `c`,
  `w'` enter the AIR as 44 periodic columns of cycle `d`, so a verifier does work
  linear in the public input (≈ 43·256 field operations) on top of the FRI checks.
  That is fine for the Rust verifier and standard for public inputs, but an
  on-chain verifier would want `A′` (fixed public parameters) behind a commitment
  instead of as calldata.
- **Stage B (on-chain)** — a native EVM (Solidity) FRI verifier for the resulting
  proof, measured on Foundry against the ≈56.5M-gas native verifier, targeting the
  16.77M per-transaction cap.
