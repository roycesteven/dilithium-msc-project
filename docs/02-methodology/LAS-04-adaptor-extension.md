<!-- Part of docs/LAS.md, split by report chapter (2026-07-06). Index: docs/LAS.md.
     Section numbering is preserved verbatim, so external references like
     "LAS.md §4" resolve to this file. Do not renumber sections. -->

## 4. The adaptor extension (LAS, variant B)

The single idea that turns the base scheme into an adaptor signature is: **fold the
statement `Y` into the Fiat–Shamir hash during pre-signing.**

```
PreSign(sk, Y, M):
    repeat:
        y ← S_γ^{n+ℓ}
        w = A·y
        c = H(pk, w + Y, M)            # <-- statement folded in
        ẑ = y + c·r
    until ‖ẑ‖∞ ≤ γ − κ − 1            # tighter bound by 1
    return σ̂ = (c, ẑ)

PreVerify(Y, pk, σ̂=(c,ẑ), M):
    if ‖ẑ‖∞ > γ − κ − 1: return false
    w' = A·ẑ − c·t
    return c == H(pk, w' + Y, M)

Adapt((Y, y), σ̂=(c,ẑ)):
    if not PreVerify(...): return ⊥
    return σ = (c, ẑ + y)

Ext(Y, σ=(c,z), σ̂=(c,ẑ)):
    s = z − ẑ
    if A·s ≠ Y: return ⊥
    return s
```

### 4.1 Correctness, line by line
Let `t = A·r` (signer key) and `Y = A·y_w` (statement, witness `y_w`).

- **PreVerify accepts honest pre-signatures.**
  `A·ẑ − c·t = A(y + c·r) − c·(A·r) = A·y = w`. Hence `w' = w` and
  `H(pk, w' + Y, M) = H(pk, w + Y, M) = c`. ✔

- **Adapted signatures verify with the *ordinary* Verify.**
  With `z = ẑ + y_w`,
  `A·z − c·t = A·ẑ + A·y_w − c·t = (w + c·t) + Y − c·t = w + Y`.
  So ordinary Verify recomputes `w'' = w + Y` and checks `H(pk, w'', M)`. But the
  pre-signing challenge was `c = H(pk, w + Y, M)`, so it matches. ✔
  The adapted signature is indistinguishable from one produced directly by `Sign`
  on the *shifted* commitment — no special verifier is needed on-chain.

- **Extraction recovers the witness.**
  `z − ẑ = (ẑ + y_w) − ẑ = y_w`, and `A·(z − ẑ) = A·y_w = Y`, so `Ext` returns
  `y_w` exactly and the `A·s == Y` check passes. ✔

- **The norm budget.** PreSign accepts only `‖ẑ‖∞ ≤ γ − κ − 1`. The witness is
  ternary (`‖y_w‖∞ ≤ 1`), so the adapted response satisfies
  `‖z‖∞ = ‖ẑ + y_w‖∞ ≤ (γ − κ − 1) + 1 = γ − κ`,
  exactly the band ordinary Verify accepts. This one-unit tightening is the whole
  reason Adapt produces in-bounds signatures. ✔
  **Generalisation to K hops (paper only, not implemented here):** were the adapted
  witness a *sum* of up to `K` ternary vectors (`‖y_w‖∞ ≤ K`), PreSign would have to
  accept only `‖ẑ‖∞ ≤ γ − κ − K`, giving `‖z‖∞ ≤ (γ − κ − K) + K = γ − κ` again —
  the `γ−κ−K` bound of eprint 2020/845. Multi-hop locks are out of this project's
  scope, so only the single-hop `K = 1` case is built and measured.
  Setting `K = 1` recovers the single-hop case verbatim.

### 4.2 The "tripwire": a pre-signature is **not** a signature
A pre-signature must fail the *ordinary* verifier — otherwise the statement binding
would be meaningless. Feeding `σ̂ = (c, ẑ)` to `Verify`:
- the norm check passes (`‖ẑ‖∞ ≤ γ − κ − 1 < γ − κ`), so rejection is **not** the
  reason;
- ordinary Verify recomputes `w' = A·ẑ − c·t = w` and checks `H(pk, w, M)`. But
  `c = H(pk, w + Y, M)`. Since `Y ≠ 0` with overwhelming probability,
  `H(pk, w, M) ≠ H(pk, w + Y, M)` and Verify returns false.

This is a **cryptographic** failure (a Fiat–Shamir mismatch caused by the missing
`+Y`), not a formatting or length artefact. `test_las.c` asserts it on every
iteration (test step 5).

### 4.3 Security properties (stated, not proven — out of scope)
LAS satisfies the three standard adaptor-signature notions (proven in eprint
2020/845): *pre-signature correctness*, *pre-signature adaptability* (any valid
pre-signature can be adapted with a valid witness), and *witness extractability*
(a valid pre-signature plus its adapted signature yields the witness). The proofs
rely on Module-SIS/LWE hardness and the forking lemma; we do not reproduce them.

### 4.4 Why variant (B) and not "variant (A)"
An earlier internal sketch ("variant A") put the offset on the *response* — pre-sign
emits `z̃ = z + y` and verification *subtracts* `Y` — which requires the pre-signer
to know the witness `y` and produces an inflated response needing a widened
encoding. The paper's Algorithm 2 is variant (B): the pre-signer needs only the
*statement* `Y` (correct adaptor semantics — the signer must *not* know the witness),
the inflated value never needs special packing, and the adapted signature is an
ordinary Dilithium-shaped signature. We switched to (B) to match the paper and to
keep the on-chain object a standard signature.

---

