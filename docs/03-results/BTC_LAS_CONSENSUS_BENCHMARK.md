# What `OP_CHECKLASSIGVERIFY` costs a validating node

**Status:** RUN 2026-08-08 (Royce-directed; Wang's one measurement ask from Meeting 9).
**Runner:** `scripts/run_btc_las_bench.sh` → `evidence/btc_las_bench/<ts>/` (+ `latest`).
**Benchmark:** `bitcoin/las_consensus/bench_las_consensus.c`, target `bench_las_consensus`.
**Macros:** `scripts/gen_btc_las_bench_data.py` → `report/latex/generated/btclasbenchmacros.tex`.
**Report:** main result in §res-txstruct; detail in `app:btcnode`.

> **Never retype a figure from this document into the report.** Quote the generated macros,
> which are parsed from the run's own `SUMMARY` line.

## 1. Why it exists

Meeting 9, Wang: *"when you modify something … people will always ask, okay, if we achieved
this a better security — so what have I lost?"* `run_btc_las_node.sh` had already shown the
patched rule **works** — a spend authorised by nothing but a LAS signature is accepted and
mined, every mutation refused by the patched node and accepted by a stock one. What it never
asked is what that costs. A consensus change charges every full node that ever validates the
chain, so "it works" is half an answer.

## 2. What is measured

The verification predicate the script interpreter calls, per input:

| path | what it stands for |
|---|---|
| `LASConsensusVerify` | the new rule, `OP_CHECKLASSIGVERIFY` (0xbb) over `base_verify_packed` |
| BIP340 Schnorr | a Taproot key-path spend |
| ECDSA | a P2WPKH spend |

Per-input script-validation time is the right quantity because it is the one that **scales
with the number of signatures in a block**; everything else a node does is shared with the
unmodified client.

### Two design decisions that make the comparison mean anything

**Both sides start from serialized bytes.** `LASConsensusVerify` receives a *packed*
signature and public key and decodes them itself, because that is what the interpreter hands
it off the witness stack. Timing that against a libsecp256k1 call handed an already-parsed
`secp256k1_xonly_pubkey` would charge the wire codec to the LAS side only — the exact error
this project's tier-matching rule exists to prevent, and one that would inflate the reported
cost of the rule. So each curve loop parses its serialized pubkey (and, for ECDSA, its DER
signature) **inside** the timed block, as a node does per input.

**The reject path is a valid signature against a different 32-byte digest**, not a corrupted
one. A byte flip can trip an early structural or norm check and short-circuit, measuring how
fast the verifier gives up rather than what a rejection costs. A well-formed signature
against the wrong digest forces the whole computation — decode, norm bound, `w' = Az − ct`,
hash — and fails only at the final challenge comparison.

⚠ **That the reject path fails late is established by the input construction, not by the
timing.** The reject/accept ratio is *reported* but deliberately **not gated on**: a ratio
near 1 is a consequence of the design, not evidence for it, and gating would dress the
consequence up as proof while letting ordinary machine variation abort the pipeline. Do not
add such a gate.

The message is described throughout as a plain **32-byte digest**, never "a BIP341 sighash":
the three paths do not share a sighash algorithm (BIP341 covers the Taproot spend the opcode
lives in, BIP143 a P2WPKH ECDSA input). Only the width is common.

### Measurement gates

The project's standing discipline applies: an untimed **warm-up** first (`LASConsensusVerify`
expands its public parameters behind a `pthread_once` on first call); paths **paired and
interleaved within each repetition** so clock drift cannot bias one against another; a
**success-path assertion** closing every timed block (accept loops must have accepted every
time, reject loops rejected every time); deterministic inputs (`base_keygen_seed`,
`base_sign_det`, fixed secp keys). The runner additionally runs the shim **self-test** before
timing anything — a verifier that wrongly rejected would still produce beautifully consistent
numbers.

**Both sides build at `-O3 -fomit-frame-pointer`.** A real bug caught mid-run: the vendored
secp objects compile at `-O3` while the shim `Makefile` defaulted to `-O2`, so the first run
timed an `-O2` lattice verifier against an `-O3` curve. It changed the LAS figure by well
under a percent, but the mismatch was wrong regardless and the flags are now pinned in the
`bench_las_consensus` rule. Do not replace them with the generic `$(CFLAGS)`.

**The runner force-rebuilds** (`make clean` + `make -B`). A flag-only change to the Makefile
does not make an existing binary out of date, so a plain `make` would happily re-run a stale
executable and report it as the current source's numbers.

## 3. Result

Quote the macros, not this section. The settled **shape** across three runs on the machine of
record: one `OP_CHECKLASSIGVERIFY` costs **roughly 11–12× a curve verification** per input.

On rejection, the **only** supported reading is the narrow one: a LAS signature that fails at
the final comparison costs **approximately as much as one that verifies**, so there is no
early exit on that path. It does **not** show that invalid input is free of amplification
generally — malformed inputs take other code paths that were not timed, and the
denial-of-service surface of the rule is not characterised here at all. Note also the
direction of the headline ratio: a block of such inputs is more verification work than the
spends it displaces, so "rejection is not cheap" is a statement about the shape of the
verifier, not reassurance about attack cost.

**Corroboration.** The project's independent Stage-1 harness measures D3 packed-tier `Verify`
at 362 ± 6 µs (`evidence/latest`, run `20260804_101750`). This benchmark, a separate harness
written for the node shim, lands within a few percent of it on the same predicate. Two
independent harnesses agreeing is what makes the figure trustworthy.

## 4. Caveats — separate claims, do not merge them

0. **The curve baseline is not Bitcoin Core's build.** It is a pinned `libsecp256k1-zkp`, a
   *fork* of `libsecp256k1` — the same verification algorithms, but **not** the library the
   patched client links. These are not Core's numbers. A like-for-like node comparison would
   link the patched client's own `libsecp256k1`; that is the obvious refinement if anyone
   wants a deployment figure.
1. **The security of the consensus modification is not analysed.** Whether defining an
   `OP_SUCCESS` opcode as a lattice verification is *safe* as a consensus rule — soundness,
   DoS surface, upgrade path, standardness interaction — is evaluated nowhere in this
   project. **A timing figure is not a safety argument.**
2. **The schemes are not at a matched security level.** `secp256k1` offers roughly 128-bit
   *classical* security, whose engineering match is Simplified Dilithium-**II**, while the
   node compiles the rule at Simplified Dilithium-**III** because that is what it actually
   runs — so measuring D3 is the honest thing to do, but the ratio compares the curve against
   a deliberately **stronger** lattice setting and therefore **overstates** what the rule
   costs relative to a level-matched pairing. Stating the direction of that bias is the
   point. Neither pairing is a formal security-equivalence claim. (The report's classical
   baseline table runs at Simplified Dilithium-II for exactly this reason.)
3. **Scope.** Per-input cryptographic cost only: no block download, UTXO lookup, script
   parsing or signature caching, all of which the stock client pays too.

Nothing here changes the standing framings — a patched node is **not** Bitcoin, "cannot
settle on Bitcoin as it stands" remains true, and implementing one of the three routes takes
no position on which should be adopted.

## 5. What this corrected

The report previously asserted, in both the body and `app:btcnode`, that the rule's
**validation cost is not measured**. That is now false and both statements were rewritten in
the same edit. The remaining unmeasured thing is the modification's *security*, which is a
different claim and still stands.
