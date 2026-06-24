// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

import {LASVerifyCost} from "../src/LASVerifyCost.sol";

/// Experiment for the report claim "native on-chain LAS verification exceeds the block gas
/// limit". It MEASURES the gas of the full arithmetic of one `las_verify` on Foundry's local
/// EVM (deterministic; not machine-dependent), CALCULATES the SHAKE256 challenge-hash cost
/// from a per-permutation figure, sums them, and compares the total to the block gas limit.
///
/// Run:  forge test --match-contract LASVerifyCost -vv        (logs the breakdown)
///       forge test --gas-report                              (clean per-function gas)
contract LASVerifyCostTest {
    LASVerifyCost cost;

    // Ethereum mainnet block gas limit: 30,000,000 for years (EIP-1559 cap), raised toward
    // ~36,000,000 during 2025. We compare against the conservative 30M.
    uint256 constant BLOCK_GAS_LIMIT = 30_000_000;

    // SHAKE256 challenge hash, faithful cost (a CALCULATION, not a same-machine measurement):
    //   las_verify's hash_challenge absorbs 8 packed polys × 1024 B = 8192 B (rate 136 B ⇒
    //   ~61 Keccak-f[1600] permutations) and las_challenge adds ~3 more ⇒ ~64 permutations.
    //   A hand-rolled Keccak-f[1600] in EVM bytecode costs ~30,000 gas/permutation
    //   (24 rounds of XOR/ROT/AND over 25 lanes); published SHA3-in-Solidity ports sit in the
    //   25k–35k band. We use 30k as a representative point estimate.
    uint256 constant KECCAKF_PERMUTATIONS = 64;
    uint256 constant GAS_PER_KECCAKF      = 30_000;

    function setUp() public {
        cost = new LASVerifyCost();
    }

    function test_NativeVerifyGas() public view {
        // warm the contract + storage so the measured delta is execution, not the cold-call
        // surcharge, then measure a second call.
        cost.verifyArith(1);
        uint256 g0 = gasleft();
        uint256 sink = cost.verifyArith(0xC0FFEE);
        uint256 arithGas = g0 - gasleft();
        require(sink != 0, "kernel was optimised away");

        // native keccak256 opcode over the 8192-byte absorb input: the deterministic on-chain
        // FLOOR for the hash (unusable for real SHAKE256, but a strict lower bound).
        bytes memory absorbInput = new bytes(8192);
        uint256 g1 = gasleft();
        bytes32 h = keccak256(absorbInput);
        uint256 keccakNativeFloor = g1 - gasleft();
        require(h != bytes32(0), "hash sink");

        // faithful SHAKE256 cost (calculation) + total native-verify estimate.
        uint256 hashFaithful = KECCAKF_PERMUTATIONS * GAS_PER_KECCAKF;
        uint256 totalFloor    = arithGas + keccakNativeFloor; // arith measured + hash native floor
        uint256 totalFaithful = arithGas + hashFaithful;      // arith measured + hash calculated

        _log("LAS verify ARITHMETIC (measured, gas)        ", arithGas);
        _log("hash native-keccak256 FLOOR (measured, gas)  ", keccakNativeFloor);
        _log("hash faithful SHAKE256 (calculated, gas)     ", hashFaithful);
        _log("TOTAL native verify, hash-floor  (gas)       ", totalFloor);
        _log("TOTAL native verify, hash-faithful (gas)     ", totalFaithful);
        _log("block gas limit (30M)                        ", BLOCK_GAS_LIMIT);
        _log("classical ecrecover claim (gas)              ", 75_709);
        _log("native verify as multiple of classical claim ", totalFaithful / 75_709);
        _log("native verify as percent of a 30M block      ", (totalFaithful * 100) / BLOCK_GAS_LIMIT);

        // --- per-primitive reconciliation: isolate one op as the gas delta between 11 and 1
        //     reps (cancels the shared one-off setup), then rebuild the arithmetic total from
        //     the op budget (12 fwd NTT + 8 inv NTT + 20 pointwise) as an independent cross-check.
        uint256 perNtt       = (_gasOf_ntt(11) - _gasOf_ntt(1)) / 10;
        uint256 perInvntt    = (_gasOf_invntt(11) - _gasOf_invntt(1)) / 10;
        uint256 perPointwise = (_gasOf_pointwise(11) - _gasOf_pointwise(1)) / 10;
        uint256 rebuilt = 12 * perNtt + 8 * perInvntt + 20 * perPointwise;
        _log("per forward-NTT (measured, gas)              ", perNtt);
        _log("per inverse-NTT (measured, gas)              ", perInvntt);
        _log("per pointwise   (measured, gas)              ", perPointwise);
        _log("arith rebuilt from op budget (gas)           ", rebuilt);

        // The honest assertion: the arithmetic alone is enormous relative to a real claim, and
        // the full native verify is a large fraction of a whole block — but with EVM-native
        // mulmod it does NOT, by itself, exceed the 30M limit. The true barrier is economic
        // (≈ two orders of magnitude over the classical claim) plus the absence of a
        // SHAKE/NTT precompile, NOT the hard block ceiling. See docs/LAS.md §8.4.
        require(totalFaithful > 20 * 75_709, "expected native verify >> classical claim");
        require(totalFaithful < BLOCK_GAS_LIMIT, "model says it would exceed a block (update docs!)");
    }

    /* ---- gas of one rep-probe call (warm), used to isolate per-primitive cost ---- */
    function _gasOf_ntt(uint256 reps) internal view returns (uint256) {
        cost.nttReps(reps, reps); uint256 g = gasleft(); cost.nttReps(reps, reps + 1); return g - gasleft();
    }
    function _gasOf_invntt(uint256 reps) internal view returns (uint256) {
        cost.invnttReps(reps, reps); uint256 g = gasleft(); cost.invnttReps(reps, reps + 1); return g - gasleft();
    }
    function _gasOf_pointwise(uint256 reps) internal view returns (uint256) {
        cost.pointwiseReps(reps, reps); uint256 g = gasleft(); cost.pointwiseReps(reps, reps + 1); return g - gasleft();
    }

    /* ------- minimal console.log(string,uint256) without a forge-std dependency ------- */
    address constant CONSOLE = 0x000000000000000000636F6e736F6c652e6c6f67;
    function _log(string memory tag, uint256 val) internal view {
        (bool ok, ) = CONSOLE.staticcall(abi.encodeWithSignature("log(string,uint256)", tag, val));
        ok; // ignore — console is a no-op when absent
    }
}
