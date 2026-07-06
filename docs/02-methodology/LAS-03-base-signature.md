<!-- Part of docs/LAS.md, split by report chapter (2026-07-06). Index: docs/LAS.md.
     Section numbering is preserved verbatim, so external references like
     "LAS.md §3" resolve to this file. Do not renumber sections. -->

## 3. The base signature

Parameters: `n = ℓ = 4`, `n+ℓ = 8`, `κ = 60`, `γ = κ·d·(n+ℓ) = 60·256·8 = 122880`.

```
KeyGen():                              # = Gen
    r ← S_1^{n+ℓ}                      # ternary secret
    t = A·r                            # public key
    return (pk, sk) = (t, r)

Sign(sk, M):
    repeat:
        y ← S_γ^{n+ℓ}
        w = A·y
        c = H(pk, w, M)                # SampleInBall, ‖c‖_1 = κ
        z = y + c·r
    until ‖z‖∞ ≤ γ − κ                 # else resample
    return σ = (c, z)

Verify(pk, σ=(c,z), M):
    if ‖z‖∞ > γ − κ: return false
    w' = A·z − c·t
    return c == H(pk, w', M)
```

**Why Verify works.** For an honest signature,
`A·z − c·t = A(y + c·r) − c·(A·r) = A·y = w`, so `w' = w` and the recomputed
challenge equals `c`. The bound `‖c·r‖∞ ≤ κ` (a `±1`-weight-`κ` challenge times a
ternary vector) guarantees `‖z‖∞ ≤ γ + κ` before rejection, and the accepted band
`‖z‖∞ ≤ γ − κ` leaves room for the adaptor offset (Section 4).

---

