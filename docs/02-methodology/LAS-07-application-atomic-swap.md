<!-- Part of docs/LAS.md, split by report chapter (2026-07-06). Index: docs/LAS.md.
     Section numbering is preserved verbatim, so external references like
     "LAS.md §7" resolve to this file. Do not renumber sections. -->

## 7. Application: a post-quantum atomic swap (`ref/test/test_swap.c`)

### 7.1 Protocol (paper §4.1, Fig. 1 — implemented verbatim)

Two parties, Alice and Bob, want to swap coins across two ledgers ("chain 1" and
"chain 2") with no trusted third party and no on-chain scripts. The demo follows
the paper's Fig. 1 exactly, including its message order — **the witness holder
u₁ (Alice) commits her own coin first** — and the proof of knowledge **π**
(§7.6), which u₂ (Bob) verifies before pre-signing anything.

```
Setup:  public params A; Alice = u₁ (coin c1 on chain 1); Bob = u₂ (coin c2 on chain 2).

1. Alice:  (Y, y) = Gen();  π = PoK{ r′ : A·r′ = Y ∧ ‖r′‖∞ ≤ 1 }
           σ̂_1 = PreSign(sk1, Y, tx1)          (tx1: Alice→Bob on chain 1)
   Alice → Bob:  { Y, π, σ̂_1, tx1 }                          (off-chain)
2. Bob:    if π or PreVerify(Y, pk1, σ̂_1, tx1) fails → ABORT (Fig. 1's gate)
           σ̂_2 = PreSign(sk2, Y, tx2)          (tx2: Bob→Alice on chain 2)
   Bob → Alice:  { σ̂_2, tx2 }                                (off-chain)
   [Neither pre-signature is spendable: its bytes fail ordinary Verify.]
3. Alice (knows y):  σ_2 = Adapt((Y, y), σ̂_2);  PUBLISH σ_2 on chain 2.
   Verify(pk2, σ_2, tx2)=true  ⇒  Alice claims Bob's coin c2.
4. Bob observes σ_2 on chain 2 and extracts:  y′ = Ext(Y, σ_2, σ̂_2).
   [π's guarantee: ‖y′‖∞ ≤ 1 and y′ = y — asserted by the demo.]
5. Bob:  σ_1 = Adapt((Y, y′), σ̂_1);  PUBLISH σ_1 on chain 1.
   Verify(pk1, σ_1, tx1)=true  ⇒  Bob claims Alice's coin c1.
```

### 7.2 Why it is atomic
The *same* statement `Y` binds both pre-signatures. Alice can only claim Bob's
coin by adapting `σ̂_2`, which publishes `σ_2`; from `σ_2` and `σ̂_2` Bob extracts
`y′`, which is exactly what he needs to adapt `σ̂_1` and claim Alice's coin.
Conversely, **before** `σ_2` is published Bob does not know `y`, so he cannot
complete `σ̂_1` (its raw bytes fail ordinary Verify — the tripwire). So either
both legs settle or neither does. π closes the remaining hole (paper §4.1): it
proves *in advance* that `Y` has a **ternary** preimage, so by the M-SIS
uniqueness argument the `y′` Bob extracts equals `y` and his Adapt is
*guaranteed* to clear the Verify bound — without π, a malicious Alice could
claim c2 and leave Bob with an unadaptable pre-signature. The demo prints this
narrative and asserts every step, including the counterfactual that raw
pre-signature bytes are unspendable and that the extracted `y′` is ternary.

### 7.3 Relation to payment channels
The same mechanism generalises to payment-channel networks (multi-hop payments):
each hop is pre-signed against a statement derived from the same secret, and the
receiver's claim cascades witness revelation back along the path. The "knowledge
gap" caveat (Section 8) is what bounds how long such chains can be in the relaxed
lattice setting; our exact-extraction parameterisation sidesteps it for the demo.

---

### 7.4 A scriptless-script ledger (`ref/chain.{c,h}`, `ref/test/test_pcn.c`)

To show LAS in a setting closer to a real blockchain, `chain.{c,h}` provide a small
ledger abstraction: accounts with balances, a block height, and *adaptor-locked
contracts* — the scriptless analogue of a Hash-Time-Locked Contract (HTLC):

- the **hash lock** is replaced by an adaptor statement `Y` — claiming requires the
  witness `y`;
- the **time lock** is a timeout block height — the funder may refund after it.

The chain stores only public data (public keys, statements, (pre-)signatures); the
secret keys and the `PreSign`/`Adapt` steps live in the parties' wallets, exactly as
on a real chain. `chain_fund_swap` *pre-verifies* the funder's pre-signature before
escrowing funds; `chain_claim_swap` *verifies* the adapted signature, pays the
beneficiary, and records it on-chain (revealing `y` to any watcher); `chain_refund_swap`
is gated on `height ≥ timeout`; `chain_extract_witness` runs `las_ext` on a claimed
contract.

**Important model note:** Scenarios 1–3 use the *same-Y* model — one shared
statement `Y` locks all hops. This is a correct scriptless HTLC but not the paper's
AMHL (Adaptor Multi-Hop Lock). In the same-Y model, observing `y` from any claimed
hop lets a party adapt *all* other hops locked to `Y` — a wormhole-style weakness on
longer paths. The paper's AMHL assigns each hop a *different* cumulative statement
`Y_j = A·(l_1 + … + l_j)` and uses PreSign bound `γ−κ−K` for path length K. **AMHL
is now implemented** (`ref/amhl.{c,h}`, `ref/test/test_amhl.c`) and described in
Section 7.5; the same-Y demo below is retained as the simpler baseline that
isolates the core adaptor mechanism.

`test_pcn.c` runs three hard-asserted scenarios (all pass):

1. **Cross-chain atomic swap (happy path).** Alice locks 10 on chain A to Bob; Bob
   locks 10 on chain B to Alice, both to the same `Y`. Bob claims on A (revealing
   `y`); Alice extracts `y` and claims on B. Both legs settle.
2. **Timeout / refund (unhappy path).** No one claims. Refund *before* timeout is
   rejected; after advancing block height both parties refund — no coins lost. Legs
   use laddered timeouts so the second claimant always has a safety window.
3. **Multi-hop payment (same-Y PCN).** Carol issues invoice `(Y, y)`. Alice pays Bob
   (11, outer hop) and Bob pays Carol (10, inner hop), both locked to `Y`. Carol
   pulls the inner hop with `y` (revealing it); Bob extracts `y` and pulls the outer
   hop, earning his routing fee. Final: Alice=89, Bob=101, Carol=10.

This is a **working** post-quantum scriptless swap and payment-channel
demonstration. The same-Y model is sufficient to prove the adaptor mechanism is
functional end-to-end and is the clearest baseline for isolating it; the
multi-hop-safe AMHL construction that removes its wormhole weakness is the
headline multi-hop artefact and is described next in Section 7.5.

### 7.5 Adaptor Multi-Hop Locks — AMHL (`ref/amhl.{c,h}`, `ref/test/test_amhl.c`)

The same-Y ledger of Section 7.4 proves the adaptor mechanism works, but it is
*not* the paper's payment-channel-network construction. AMHL (LAS Fig. 2 / §5) is,
and it is the headline multi-hop artefact of this project. It removes the
same-Y wormhole weakness by giving every hop on a route a **distinct** statement.

#### 7.5.1 Construction
For a K-hop route `U_0 → U_1 → … → U_K` (sender `U_0`, receiver `U_K`):

```
Setup (run by the sender U_0, who knows the whole route):
    sample increments   l_1, …, l_K  ← S_1          (ternary, ‖l_j‖∞ ≤ 1)
    cumulative witness   s_0 = 0,  s_j = s_{j-1} + l_j = l_1 + … + l_j
    cumulative statement Y_0 = 0,  Y_j = A·s_j       (so Y_j = Y_{j-1} + A·l_j)
    Hop j (payer U_{j-1} → payee U_j) is locked to Y_j and opened with s_j.

Secret distribution (least-privilege):
    receiver U_K      ← s_K                 (the full witness, i.e. the "invoice")
    intermediary U_j  ← l_{j+1}  only        (1 ≤ j < K)

Pre-signing: every hop uses  las_presign_k(…, K)  → bound γ−κ−K.

Cascade (claims flow right→left, receiver first):
    U_K  Adapts hop K with s_K and publishes σ_K           ⇒ s_K becomes public
    U_{j-1} extracts s_j from hop j, computes s_{j-1}=s_j−l_{j+1}, Adapts hop j−1
    … down to hop 1, which the sender's first counterparty U_1 finally pulls.
```

The cumulative statements are built additively from the increment key pairs
(`Y_j = Y_{j-1} + A·l_j`), reusing `base_keygen` to produce each `(A·l_j, l_j)` —
no new lattice arithmetic. Adapt and Ext are **unchanged** from Section 4: Adapt
adds the cumulative witness `s_j`, and Ext returns exactly `s_j` (it satisfies
`A·s_j = Y_j`).

#### 7.5.2 Why each property holds
- **Distinct statements ⇒ no wormhole.** Because `Y_i ≠ Y_j` for `i ≠ j`, learning
  the opener `s_j` of one hop reveals nothing usable about a non-adjacent hop.
  The demo asserts the converse directly: adapting hop 1 with the receiver's secret
  `s_K` yields a signature that ordinary `Verify` **rejects** (it would force
  `A·z−c·t = w + Y_K ≠ w + Y_1`, a Fiat–Shamir mismatch). Only the adjacent
  increment `l_{j+1}` bridges hop `j+1` to hop `j`.
- **Witness-norm growth (the knowledge gap, made concrete).** `s_j` is a sum of `j`
  ternary vectors, so `‖s_j‖∞ ≤ j ≤ K`. The demo prints this growth exactly
  (`‖s_1‖∞ = 1, …, ‖s_4‖∞ = 4`). This is *why* every hop must pre-sign at the
  tighter bound `γ−κ−K`: the adapted response `z = ẑ + s_j` then still satisfies
  `‖z‖∞ ≤ (γ−κ−K) + j ≤ γ−κ` and clears ordinary `Verify`.
- **Exact recovery.** At each hop the extractor recovers `s_j` exactly, and the
  intermediary recomputes `s_{j-1} = s_j − l_{j+1}` exactly (asserted equal to the
  setup value). Extraction is noise-free in this parameterisation; the residual
  *relaxed-relation* knowledge gap is discussed in Section 9.
- **Timeouts/refund.** Hops carry laddered timeouts (outer hops expire last, so
  every puller retains a safety window). The refund scenario advances the block
  height past every timeout and refunds each hop to its funder — no coins lost.

#### 7.5.3 The K-hop bound in practice
The tightening from `γ−κ−1` to `γ−κ−K` is *cryptographically essential* but has
**negligible performance cost**: with `γ = 122880` and `K ≤ 8`, the accepted band
shrinks by at most `K/(γ−κ) ≈ 0.007 %`, so the rejection-sampling acceptance rate
(Section 8) is indistinguishable from the single-hop case. AMHL therefore adds no
per-hop *signing* penalty beyond the obvious linear "K hops ⇒ K pre-signatures."

#### 7.5.4 Demo output (mode 3)
`test_amhl3` runs two hard-asserted scenarios; abridged transcript:

```
== Scenario 1: AMHL 4-hop routed payment (happy path) ==
    hop 1: Y_1 distinct=yes   ‖s_1‖inf = 1  (<= 1)
    hop 2: Y_2 distinct=yes   ‖s_2‖inf = 2  (<= 2)
    hop 3: Y_3 distinct=yes   ‖s_3‖inf = 3  (<= 3)
    hop 4: Y_4 distinct=yes   ‖s_4‖inf = 4  (<= 4)
  funded hops: Alice→Bob(13) Bob→Carol(12) Carol→Dave(11) Dave→Erin(10)
  wormhole check: s_4 cannot open hop 1 (distinct statement)  -> OK
  cascade: Erin→Dave→Carol→Bob pull right-to-left, exact recovery each hop
  balances: Alice=87 Bob=101 Carol=101 Dave=101 Erin=10   (payment + per-hop fee)
== Scenario 2: AMHL route times out (refund path) ==
  refund before timeout rejected; after timeouts every hop refunds  -> safe OK
```

Alice pays 10 to Erin across a 4-hop route; each of the three intermediaries earns
a 1-unit routing fee, and the conservation `87+101+101+101+10 = 400` holds. This is
the genuinely novel part of LAS exercised end-to-end — most adaptor-signature demos
stop at a single swap.


---

### 7.6 The proof of knowledge π (`ref/relation_zk.{c,h}`, `ref/test/test_zkp.c`)

Fig. 1's first message carries `π ← P((t′; r′), {∃ r′ : A·r′ = t′ ∧ ‖r′‖∞ ≤ 1})`.
The **exact** ternary bound is load-bearing (paper §4.1): the fairness argument
runs "extracted `s` = proven `r′` (else `A(s − r′) = 0` breaks M-SIS), hence
`‖s‖∞ ≤ 1`, hence pre-signature adaptability" — an approximate proof-of-knowledge
(or an ℓ₂-only bound) breaks the chain, because a coefficient larger than 1 would
push Adapt's output `z = ẑ + s` over the Verify bound `γ − κ`.

**Realisation.** The paper suggests the Esgin–Nguyen–Seiler proof system; this
build instead reuses the vendored **LaZer** library (`third_party/lazer`,
LNP-based "linear relation with norms" frontend) — the same reuse-not-reinvent
posture as vendoring `secp256k1-zkp` for the classical baseline. LaZer proves
per-partition *binary* coefficients or *exact ℓ₂* bounds (its `wlinf` knob is a
parameter hint, **not** a proven statement), so ternary is encoded by **binary
decomposition** `r′ = r₊ − r₋`:

```
[A | −A | 0] · (r₊ ‖ r₋ ‖ e) = t′,    r₊, r₋ binary (proven),
                                      ℓ₂(e) ≤ 16 (dummy, honest e = 0)
```

Knowledge of binary `(r₊, r₋)` with that relation *is* knowledge of a ternary
preimage `r₊ − r₋` of `t′`. (The all-zero 23rd column exists only because the
LaZer parameter generator requires at least one ℓ₂-bounded partition; it
contributes nothing to the relation.)

**Module layout.** `relation_zk.c` (relation layer: builds the statement from
`pp`/`Y`, decomposes the witness, rejects non-ternary witnesses — an
Ext-extracted `R′_A` witness is not a valid π witness) + `relation_zk_lazer.c`
(the ONLY translation unit that includes `lazer.h`; ref headers and LaZer's
cannot share one). The parameter set `ref/relation_zk_params.h` is committed,
generated from `scripts/las_pi_params.py` by LaZer's `sage lin-codegen.sage`:
knowledge error ≤ 2⁻¹²⁷ under M-SIS, zero-knowledge under M-LWE. Measured proof
≈ 30.7 KB — exchanged **off-chain only** (paper §4.1), so it never enters the
on-chain cost story. Prove + verify complete in well under a second.

**Tests.** `test/test_zkp3`: completeness, single-byte tamper sweep (sampled
stride), wrong-statement rejection, non-ternary-witness refusal. `test_swap3`
wires π into the Fig. 1 gate. Rust twins (`--features relation-zk`, FFI to the
*same* C bridge and parameter set, so both languages run the identical proof
system): `tests/las_zkp.rs`, `tests/las_swap.rs`, plus an in-crate unit test for
the non-ternary refusal (outside the crate a non-ternary witness is
unconstructible by design). Both targets are **opt-in** (not in `make all` /
default features) because they need the one-time LaZer build — see §10 /
README.
