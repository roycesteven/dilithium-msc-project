# A succinct post-quantum proof for the role-A relation

**Status: implemented and run 2026-08-04.**
Evidence: `evidence/role_a_stark/latest/{bench_role_a.log,tests.log,environment.txt}`.
Code: `rust/las-stark/src/role_a_air.rs`, `rust/las-stark/src/bin/bench_role_a.rs`.

> ## ⚠️ DISQUALIFIED AS A DROP-IN: π MUST BE ZERO-KNOWLEDGE (Royce, 2026-08-04)
>
> eprint 2020/845 §4.1 requires u₁ to send `Y` **together with a proof π of knowledge of a
> witness r**, and the swap's security depends on π *hiding* that witness: if u₂ learned
> `r` from π it could adapt σ̂₁ itself and take **both** sides of the swap. So π must be
> zero-knowledge, not merely a proof of knowledge.
>
> **The STARK built here is NOT zero-knowledge** — Winterfell's prover adds no zk
> randomisation. It is therefore **not a valid π** for the Fig. 1 protocol, and the
> comparison below is *favourable to it* on an axis where it does not qualify at all. Treat
> this document as a measurement of the succinct-PQ *shape*, not as a candidate prover.
>
> This was originally written here as a scope caveat. That was too weak: it is a
> disqualification, and the correct next step is a proof system that is succinct, PQ **and**
> zk — LaBRADOR, which LaZer ships. **That has now been run: see §6.** Its verdict closes the
> direction — LaBRADOR qualifies on all three counts and still loses to the deployed LNP22 on
> every axis at this statement size, because succinctness is asymptotic and one role-A
> relation is far too small to reach it.

**Headline: it works, it is genuinely post-quantum and transparent, and at this statement
size it does not pay** — and it is not zero-knowledge, so it does not qualify as π. The
STARK proof is **2.53× larger** than LaZer's and total compute is **1.14× worse**. What it
does win is decisive but narrow: **verification is 54× faster than LaZer and 9× faster than
Groth16** (1.4 ms against 75.2 ms and 12.7 ms). That moves essentially the entire cost onto
the prover — a profile that is wrong for a two-party swap and right for a
verifier-constrained setting.

---

## 1. Why this experiment exists

The amortisation experiment (`PROOF_AMORTISATION_EXPERIMENT.md`) closed the "make the
existing proof cheaper" direction for **both** deployed provers:

- **Groth16** batches to a perfect `1/k`, but on a 128 B proof that was never the
  bottleneck;
- **LaZer** batches to a real 43% saving on the object that *is* the bottleneck, but pays
  3.33× per-swap compute for it.

What that leaves is not a cheaper instance of either system but a different *kind*: one that
is **post-quantum** (Groth16 is not) **and** succinct (LaZer is not — its proof is
linear-ish in the statement, not polylogarithmic). A FRI-STARK is the natural candidate:
transparent, hash-based, no pairings.

This experiment builds one and measures it against both baselines on the same relation.

## 2. What is proven

Exactly the statement configurations 2 and 3 prove for the Fig. 1 swap:

```text
    ∃ r : A r = t'   and   ‖r‖∞ ≤ 1,      A = [I_n | A'].
```

The ternary bound is enforced by `r³ = r` — over `F_p` the cubic `X(X−1)(X+1)` has at most
three roots, so this holds **iff** `r ∈ {−1,0,1}`. That is exact, not a relaxation, and it
is literally the constraint the Groth16 circuit uses, so the two provers demonstrably prove
the same predicate.

`A'` is the **real matrix of the C build**, loaded from the golden vectors; `r` is sampled
ternary and `t' = A r` computed from it, exactly as `relation_gen` does.

## 3. How it relates to the existing `relation_air`

`rust/las-stark/src/relation_air.rs` already proved the arithmetic core of `base_verify`,
`w' = z_top + A'·z_bot − c·t` with `‖z‖∞ ≤ B`, at the real degree `d = 256`. The role-A
relation is the **same shape** with `c = 0`, `w' := t'`, `z := r`, and the bound tightened
from `B = 137935` to ternary.

So `role_a_air.rs` reuses that module's architecture verbatim — the narrow
random-evaluation (Schwartz–Zippel) argument over Winterfell's auxiliary trace segment,
which is what gets a `d = 256` relation past Winterfell's 255-column cap — and specialises
three things:

1. **No challenge.** `c` and `t` vanish with the `c·t` term (−2 aux columns, −2 periodic).
2. **Ternary instead of a range check.** `r³ = r` (1 column, 2 constraints) replaces the
   two-sided 19-bit decomposition (38 columns) the loose bound `B` needed.
3. **Retuned quotient ranges, derived not chosen.** With `A'` centred and `r` ternary,
   `|P_m| ≤ ℓ·d·(q−1)/2 + (q−1)/2 + 1 = 5 367 656 449 < 2³³`, hence `|h| < 2³³` and
   `|g| ≤ 2|P|/q = 1280 < 2¹²`. The witness builder re-checks both per instance and refuses
   rather than proving something the AIR cannot bound.

Net trace: main width **135 → 63**, aux width **15 → 13**, at 4096 rows.

**Why the range checks are load-bearing.** The aux argument is an identity in `F_p`; it
lifts to `Z` — which is what makes it mean the relation — only because every committed
coefficient is bounded. Without a bound on `g` the argument is **vacuous**: `q` is
invertible mod `p`, so an unbounded `g` satisfies the equation for *any* claimed `t'`.

## 4. Measured result

5 repetitions after an untimed warm-up; trace 4096 × (63 main + 13 aux).

| prover | proof (B) | prove (ms) | verify (ms) | post-quantum | transparent |
|---|---|---|---|---|---|
| Groth16 over BN254 (config 2) | 128 | 494.1 | 12.7 | **no** | **no** |
| LaZer LNP22 (config 3) | 30 723 | 158.8 | 75.2 | yes | yes |
| **FRI-STARK (this work)** | **77 809** | **266.0 ± 11.6** | **1.4 ± 0.3** | yes | yes |

Baselines are the `k=1` rows of `evidence/amortise/latest` and
`evidence/lazer_amortise/latest` — real runs on this machine, not folklore.

**Against LaZer, the only other post-quantum prover:**

- proof **2.53× larger** (77 809 B vs 30 723 B);
- total compute **1.14× worse** (267.4 ms vs 234.0 ms);
- but verification **53.7× faster** (1.4 ms vs 75.2 ms).

**The cost split is the real finding.** LaZer spends 68% of its compute proving and 32%
verifying. The STARK spends **99.5% proving and 0.5% verifying**. It does not reduce the
work; it relocates almost all of it onto the prover.

**Against Groth16:** the STARK proves 1.9× faster and verifies 9.1× faster, but its proof is
600× larger. A constant-size pairing proof is unbeatable on bytes — at the price of not
being post-quantum and needing a trusted per-circuit setup, which is exactly why
configuration 3 exists.

## 5. What it means

**Succinctness is asymptotic, and one role-A relation is too small to reach it.** At
`n = 6, ℓ = 5, d = 256` the FRI commitment overhead has not paid for itself: a 4096-row
trace produces a 78 KB proof, most of it Merkle authentication paths and FRI layers whose
cost is nearly independent of how small the statement is. This is the honest reading, and
it is a property of the operating point rather than a defect of the construction.

**The verification win is real but lands in the wrong place here.** In a two-party swap each
proof is verified exactly once, so trading 107 ms of extra proving for 74 ms of saved
verification is a straight loss. The profile — expensive to produce, almost free to check —
pays only where one proof is verified many times, or where the verifier is the constrained
party. That second case is precisely the project's *other* open question: on-chain
verification, where verifier cost is the entire problem.

**But it does not solve that question either, and must not be presented as if it did.** A
78 KB proof is itself a large calldata cost, and no Solidity FRI verifier exists in this
project — `rust/las-stark`'s README is explicit that the on-chain verifier is unbuilt. What
this result establishes is narrower and still useful: the *shape* of the trade a succinct PQ
system offers, measured rather than assumed.

## 6. LaBRADOR: the system that actually qualifies — RUN 2026-08-04

Because §4.1 requires zero-knowledge, the honest conclusion of the STARK experiment is not
"it is 2.5× larger" but **"it is the wrong tool, and the right one is succinct, post-quantum
AND zero-knowledge at once."** That system is **LaBRADOR**, which LaZer ships and whose
`gen_params` takes an explicit **`zk` flag**.

Code: `ref/relation_zk_labrador.{c,h}`, `ref/test/bench_labrador_role_a.c`.
Runner: `scripts/run_labrador_role_a.sh` → `evidence/labrador_role_a/latest/`.

**Encoding.** Witness `w = (r₊ ‖ r₋)` with LaBRADOR's **native BIN norm type** — so
`‖r‖∞ ≤ 1` is *proven*, using the same binary decomposition the deployed LNP22 path uses —
plus a quotient `g` with an exact ℓ₂ bound, and `n` constraints `[A | −A]·w − q·g = t'`. The
quotient exists because LaBRADOR works over its own prime `p`, not our `q`; its bound is
load-bearing, since `q` is invertible mod `p` and an unbounded `g` satisfies the equation for
*any* claimed `t'`.

**LOGQ = 38 is forced.** Soundness needs `|[A|−A]w| + |q·g| + |t'| < p/2`. With the declared
bound `‖g‖² ≤ 10⁸` that worst case is ≈1.07×10¹¹ against LOGQ = 38's `p/2 ≈ 1.37×10¹¹` —
**78% of the budget**, and it overflows LOGQ = 36's.

**The declared `g` bound is a stated-margin parameter, not an aligned worst case.** The naive
bound (every term of the negacyclic sum aligned) gives `|g|∞ ≤ 641`, which LaBRADOR *refuses*
to prove exactly — it exceeds the library's own exact-ℓ₂ cap. It is also wildly pessimistic:
the sum has cancellation, and the measured `|g|∞` is ~25–31. So `‖g‖² ≤ 10⁸` is declared
(~1000× the honest value), and the run asserts the honest witness sits inside it.

### Measured

5 repetitions after an untimed warm-up, zero-knowledge **ON**. Encoding gate (LaBRADOR's own
`simple_verify`, run before any proof) **ACCEPT**; every proof verified.

| | proof | prove (ms) | verify (ms) | succinct | PQ | **zk** |
|---|---|---|---|---|---|---|
| Groth16 (config 2) | 128 B | 494.1 | 12.7 | yes | **no** | yes |
| **LNP22 (config 3, deployed)** | **30 723 B** | **158.8** | **75.2** | **no** | yes | yes |
| FRI-STARK (§4) | 77 809 B | 266.0 | 1.4 | yes | yes | **no** |
| **LaBRADOR (this)** | **110.90 KB** | **1588.7 ± 288.8** | **739.1 ± 87.8** | yes | yes | **yes** |

**LaBRADOR is the only row that satisfies all three requirements — and at this statement size
it loses to the deployed LNP22 on every axis**: ~3.7× the proof, ~10× the proving time, ~10×
the verification time.

That is the same lesson the STARK taught, from the other direction. **Succinctness is
asymptotic, and one role-A relation is nowhere near large enough to reach it.** LaBRADOR is
built for very large statements (its headline application is aggregating many proofs); a
28-polynomial statement sits deep in its fixed-overhead regime, so all one measures is the
overhead.

### Caveats that must travel with these numbers

- **The encoding is mine, and may not be LaBRADOR's best.** The mod-`q` quotient adds `n`
  polynomials and forces the largest prime. LaZer's own `python/labrados.py` carries helpers
  for exactly this lifting (`num_pols_in_r`, and a comment about writing `As = t mod p` as
  `As + qGr = t mod q`), which suggests a more economical decomposition exists. So this
  measures *LaBRADOR through this encoding*, not LaBRADOR at its best.
- **Proof size is LaBRADOR's own printed "Estimated proof size"**, not a byte-exact packed
  length: the function returning that (`dch_pack_params_gen`) is hidden by
  `-fvisibility=hidden`. LNP22's 30 723 B *is* byte-exact. Never compare the two silently.
- **Proving time is noisy** (±289 ms on 1589, ~18%).
- Not wired into the swap; no security analysis of the encoding beyond the norm-bound
  argument above.

### Two traps recorded for whoever touches this next

1. **Header mismatch.** LaZer ships `src/labradosNN_py.h` declaring internal ring degree
   `N 64`; the submodule it actually builds defines `N 256`. The struct layouts disagree, so
   a driver built against the shipped header silently corrupts memory. Use the submodule's
   own `src/labrados/labrados_python.h` with `-DLOGQ=NN -DNDEBUG -Isrc/labrados`.
2. **Inverted return convention.** `simple_verify` and `verify` return **1 on success**, the
   opposite of the `0`-on-success used by the setter functions in the same header. Getting
   this backwards makes an honest instance look like a broken encoding — it did here, until
   the encoding gate was read carefully.

`src/labrados` is a git submodule that the README's LaZer clone does **not** fetch; see the
runner's header for the one-time setup.

## 7. A second follow-up, also not taken

**Batching should favour a STARK, and that is directly testable against a measured
baseline.** A STARK's proof grows polylogarithmically in trace length, so `k` instances
should cost far less than `k` proofs — whereas LaZer's batched proof grew sublinearly but
its *compute* grew superlinearly (3.33× per swap at `k=8`). A batched role-A STARK is the
one configuration where the succinct route might beat both.

It is not implemented here. The AIR is fixed at one instance: `EV_LEN = 4096` with 12 live
passes of 16, so `k = 2` needs 22 witness passes and would require `EV_LEN = 8192` and
`N_SEL = 24`. That is a contained change to `role_a_air.rs` but a real one, and it was not
attempted.

## 8. Scope and limits

- **Not reviewed cryptography.** The STARK parameters are Winterfell's via
  `las_stark::proof_options()` (32 FRI queries, blowup 8, quadratic extension). No
  concrete-security analysis of this AIR has been done. Treat the figures as an engineering
  data point; the security levels of the three provers are **not** claimed to be equal.
- **Not zero-knowledge — and this is disqualifying, not a caveat.** eprint 2020/845 §4.1
  needs π to hide the witness (otherwise u₂ adapts σ̂₁ and takes both sides), so a non-zk
  argument is not a valid π at all. Winterfell's prover adds no zk randomisation. The
  comparison above is therefore favourable to the STARK on an axis where it does not
  qualify: LaZer and Groth16 both pay for zero-knowledge and this does not. See the banner
  at the top and §6.
- **Not wired into the swap.** Configurations 2 and 3 are untouched; the KAT and every
  benchmark of record are unaffected.
- **Proof size varies ~1% between runs** (Merkle authentication paths compress differently
  per query set), so quote the evidence log rather than a remembered number.

## 9. Correctness gates

Six tests in `rust/las-stark/tests/las_stark.rs` (all `role_a_*`), passing as part of the
18-test suite:

- the generated instance genuinely satisfies `A r = t'` with `r` ternary — otherwise every
  proof below would be vacuous;
- a **non-ternary** witness is refused (the relation is `‖r‖∞ ≤ 1`, not "small");
- a **wrong statement** is refused (the `q`-division stops being exact);
- proof round-trip: prove → verify accepts;
- a **tampered statement** is rejected — the proof is bound to its `t'`;
- a **tampered proof** is rejected.

The benchmark additionally gates on: the instance satisfying the relation before anything is
timed, a success-path assertion after every timed proof, and a tampered-statement check per
repetition.

## 10. Reproducing

```bash
./scripts/run_role_a_stark.sh
```

Runs the test suite and the benchmark, writing
`evidence/role_a_stark/<timestamp>/` and repointing `latest`. Needs the golden vectors in
`evm/test/vectors/` (regenerate with
`cd ref && make test/export_verify_vector && ./test/export_verify_vector ../evm/test/vectors`).
