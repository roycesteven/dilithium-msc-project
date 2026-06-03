# Project context — LAS on Dilithium for blockchain

## One-line goal
Implement LAS (Lattice-based Adaptor Signatures, eprint 2020/845) by reusing the
CRYSTALS-Dilithium reference primitives, then demonstrate it in a post-quantum
blockchain **atomic-swap** scenario, with everything benchmarked and documented.

## Status (living)
- ✅ **LAS implemented and tested** — `ref/las.{c,h}`, scheme **variant (B)** (the
  paper's Algorithm 2). `ref/test/test_las.c` passes 200 iters on Dilithium
  modes 2/3/5, zero compiler warnings.
- ✅ **Atomic-swap demo** — `ref/test/test_swap.c` (narrated two-party, two-chain
  swap with assertions).
- ✅ **Benchmarks** — `ref/test/bench_las.c` (per-operation timings).
- ✅ **Full design write-up** — `docs/LAS.md` (report source material).
- ⏳ Push to GitHub blocked in this environment (HTTP 403, read-only token);
  work is committed locally + exported as `las-variant-b.patch`.

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
keeps the rejection-sampling acceptance rate high.)

## Known caveat (note in thesis, do NOT need to solve)
"Knowledge gap": here the extracted `y` is **exact**; in the paper's relaxed
setting the witness can carry noise that grows across long payment-channel chains.

## Modulus note
Paper uses `q ≈ 2^24`. We reuse Dilithium's NTT, whose root-of-unity table is
fixed to `Q = 8380417 (≈2^23)`, so this build uses that `Q`. `Q > 2γ`, so
correctness holds; only the concrete MSIS/MLWE security margin changes (out of
scope per supervisor). Exact `2^24` would need a new NTT table or schoolbook mult.

## Scope discipline (from supervisor)
- Target dilithium3 build (NIST level ~2/3) — LAS code is mode-independent and is
  built/tested under `-DDILITHIUM_MODE=3` (also 2/5 for portability).
- Do NOT implement/analyse security proofs. Implement + benchmark + demo only.
- Success ladder: (min) working LAS + basic blockchain demo ✅;
  (better) benchmark vs plain Dilithium (timings in `bench_las`); (best) a second
  exotic scheme.

## Reference
- LAS paper: eprint 2020/845 (Esgin, Ersoy, Erkin).
- poqeth (integration template): eprint 2025/091.
- Full design + math + results: `docs/LAS.md`.
