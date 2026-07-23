# EVM atomic-swap gas benchmark (Stage 2, application-level / on-chain)

A signature-scheme-agnostic HTLC escrow (`src/AdaptorSwap.sol`) run on Foundry's
local EVM (a private chain), used to compare the **on-chain settlement cost** of an
adaptor-signature atomic swap when the published *adapted* signature is **classical
ECDSA** vs **post-quantum LAS** — exactly the supervisor's "take an atomic swap,
replace the signature scheme, and compare" task.

## What it measures
Both schemes share the same `fund`/`refund` escrow; only the claim-time signature
verification differs, so the gas difference is attributable to the signature:

- `claimClassical` — the adapted ECDSA signature is verified natively with the
  `ecrecover` precompile (this is how a real EVM ECDSA-adaptor swap settles).
- `claimLAS` — the adapted LAS signature is a real 6720-byte packed lattice
  signature (`test/las_sig.bin`, exported from the C implementation at the D3 set,
  wire form `c_tilde ‖ BitPack(z)`; 6684 non-zero / 36 zero bytes → 107,088 gas of
  calldata alone). A *numerically-correct* native lattice verifier (NTT + SHAKE256) in
  the EVM is left as future work, so this charges only the unavoidable on-chain
  **floor**: calldata for the 6720 bytes + one keccak256 pass. The reported gas is a
  strict **lower bound** on the true settlement cost.

The *cost* of that native verification — the thing the floor leaves out — is then
measured separately by `src/LASVerifyCost.sol` + `test/LASVerifyCost.t.sol`, a probe
that reproduces the exact arithmetic op-budget of one `base_verify` at the D3 set
(n=6, ell=5): **12 fwd NTT + 12 inv NTT + 36 pointwise + 54 coefficient passes**. It
reproduces the operation *count* (reusing scratch memory), so its figure is an
arithmetic **lower-bound estimate**, not the exact cost of a numerically-correct
verifier. **Result: ≈16.7M gas** (13.93M arithmetic measured + 2.76M for the SHAKE256
challenge, 92 Keccak-f × 30k, calculated) — **≈220× the classical claim, ≈55% of a 30M
block, but NOT over the block gas limit**. This *quantifies and corrects* the earlier
"exceeds the block gas limit" claim: native LAS verification is prohibitively expensive
(and an engineering burden), not literally impossible. The probe is parametrised
(`verifyArithLevel2/3/5`); D3 is the headline, D2/D5 only show how the cost grows with
the parameter set. See `docs/LAS.md §8.4.1`.

## Reproduce
```sh
# 1. export a real packed LAS adapted signature from the C side (deterministic)
cd ../ref && make test/export_packed && ./test/export_packed ../evm/test/las_sig.bin

# 2. measure gas on the local EVM
cd ../evm && forge test --gas-report

# 3. measure the cost of *native* LAS verification (the experiment behind §8.4.1)
forge test --match-contract LASVerifyCost -vv   # logs the per-op + total breakdown
# (optional) run against a live local testnet node instead:
#   anvil &  ;  forge script ... --broadcast --rpc-url http://127.0.0.1:8545
```
`forge` auto-installs the matching solc on first run. The cheatcodes used
(`sign`, `addr`, `warp`, `readFileBinary`, `deal`) are declared inline, so no
forge-std/network install is required.

## Result (this machine; gas is deterministic for the EVM, not machine-dependent)
| Step | Classical (ECDSA-adaptor) | Post-quantum (LAS) |
|---|---:|---:|
| fund | 180,285 | 139,568 |
| **claim** | **75,709** (settle + full ecrecover verify) | **289,930** (settle only; **no** lattice verify) |
| refund | 39,330 | 39,330 |
| deploy AdaptorSwap | 715,257 | — |

The two `claim` cells are not like-for-like: the classical 75,709 *includes* full
ecrecover verification, whereas the LAS 289,930 is the whole `claimLAS` transaction —
base transaction cost, calldata for the 6720-byte signature (6684 non-zero / 36 zero →
107,088 gas), the state checks/update, the `Claimed` event, and the ETH transfer, plus
one keccak over the signature — but with **no** lattice verification. As a settlement
**floor** (true cost = this + verification) it is already ~3.8× the *complete* classical
claim. The verification it omits is priced separately by `LASVerifyCost` (step 3):
**13.93M gas of arithmetic (measured)** plus **≈2.76M for the SHAKE256 challenge
(calculated from a per-permutation model, not measured)**, i.e. an **estimated ≈16.7M
for one native verification ≈ 220× the classical claim ≈ 55% of a 30M block**. That
16.7M is an *estimate* of the verification cost, not a measurement of a complete
functional verifier (which remains future work) — prohibitively expensive but within a
block, which is why on-chain PQ verification needs a precompile or a zk-proof (cf.
poqeth for basic PQ) on *economic*, not hard-limit, grounds. See `docs/LAS.md §8.4` /
`§8.4.1`.

### Native LAS verification cost (`LASVerifyCost`, deterministic EVM gas; D3 = n=6, ell=5)

Per-primitive gas is measured directly (rep-count delta); each component row is that
measured per-op cost × the op count counted from `base_verify_internal`.

| `base_verify` component | count | gas | basis |
|---|---:|---:|---|
| forward NTT | 12 | 4,537,776 | measured per-op (378,148) × count |
| inverse NTT | 12 | 5,061,072 | measured per-op (421,756) × count |
| pointwise | 36 | 1,692,900 | measured per-op (47,025) × count |
| coefficient passes | 54 | 2,539,350 | measured per-op (47,025) × count |
| **arithmetic, rebuilt from op budget** | | **13,831,098** | sum of the above |
| **arithmetic, direct measurement** | | **13,932,285** | **measured** (`verifyArithLevel3`, incl. ~101k one-off setup) |
| SHAKE256 challenge | 92 Keccak-f | 2,760,000 | calculated (~30k/perm model, **not** measured) |
| **estimated native `base_verify`** | | **≈16,692,285** | measured arithmetic + calculated hash |

The rebuilt subtotal (13,831,098) reconciles with the direct measurement (13,932,285)
to within 0.7% — the residual is the probe's one-off setup — confirming the op-budget
model. Parameter sweep (arithmetic only, supporting material): **D2 9,325,375 · D3
13,932,285 · D5 20,043,754**.
