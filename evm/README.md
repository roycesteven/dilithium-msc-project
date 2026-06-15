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
  signature (`test/las_sig.bin`, exported from the C implementation). Native lattice
  verification (NTT + SHAKE256) is **infeasible in the EVM**, so this charges only
  the unavoidable on-chain **floor**: calldata for 4672 bytes + one keccak256 pass.
  The reported gas is therefore a strict **lower bound** on the true cost.

## Reproduce
```sh
# 1. export a real packed LAS adapted signature from the C side (deterministic)
cd ../ref && make test/export_packed && ./test/export_packed ../evm/test/las_sig.bin

# 2. measure gas on the local EVM
cd ../evm && forge test --gas-report
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
*complete* classical claim, and native verification would add the entire lattice
computation — which is why on-chain PQ verification needs a precompile or a
zk-proof (cf. poqeth for basic PQ). See `docs/LAS.md §8.4`.
