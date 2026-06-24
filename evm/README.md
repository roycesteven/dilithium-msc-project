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
- `claimLAS` — the adapted LAS signature is a real 4672-byte packed lattice
  signature (`test/las_sig.bin`, exported from the C implementation). A
  *numerically-correct* native lattice verifier (NTT + SHAKE256) in the EVM is left
  as future work, so this charges only the unavoidable on-chain **floor**: calldata
  for 4672 bytes + one keccak256 pass. The reported gas is therefore a strict
  **lower bound** on the true settlement cost.

The *cost* of that native verification — the thing the floor leaves out — is then
measured separately by `src/LASVerifyCost.sol` + `test/LASVerifyCost.t.sol`, a
gas-faithful probe that runs the exact arithmetic op-budget of one `las_verify`
(12 fwd NTT + 8 inv NTT + 20 pointwise) on the EVM so it can be priced. **Result:
≈12M gas** (10.08M arithmetic measured + ≈1.92M for the SHAKE256 challenge,
calculated) — **≈158× the classical claim, ≈40% of a 30M block, but NOT over the
block gas limit**. This *quantifies and corrects* the earlier "exceeds the block gas
limit" claim: native LAS verification is prohibitively expensive (and an engineering
burden), not literally impossible. See `docs/LAS.md §8.4.1`.

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
| **claim (settlement + sig verify)** | **75,709** (full verify) | **208,400** (floor; no real verify) |
| refund | 39,330 | 39,330 |
| deploy AdaptorSwap | 715,257 | — |

A real LAS signature here is 4649 non-zero / 23 zero bytes → 74,476 gas of calldata
alone (16 gas/non-zero, 4 gas/zero). The settlement **floor** is already ~2.75× the
*complete* classical claim, and *full* native verification is measured at **≈12M gas
≈ 158× the classical claim ≈ 40% of a 30M block** (`LASVerifyCost`, step 3) —
prohibitively expensive but within a block, which is why on-chain PQ verification
needs a precompile or a zk-proof (cf. poqeth for basic PQ) on *economic*, not
hard-limit, grounds. See `docs/LAS.md §8.4` / `§8.4.1`.

### Native LAS verification cost (`LASVerifyCost`, deterministic EVM gas)

| `las_verify` component | gas | basis |
|---|---:|---|
| forward NTT ×12 | 4,537,776 | measured |
| inverse NTT ×8 | 3,374,048 | measured |
| pointwise ×20 | 940,500 | measured |
| coefficient passes (~40) | ≈1,227,720 | measured (residual) |
| **arithmetic subtotal** | **10,080,044** | **measured** |
| SHAKE256 challenge (~64 Keccak-f) | 1,920,000 | calculated (~30k/perm) |
| **native `las_verify`** | **≈12,000,044** | measured + calculated |
