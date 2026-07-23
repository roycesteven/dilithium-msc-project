// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

import {LASVerifyCost} from "../src/LASVerifyCost.sol";

/// Experiment for the report claim "native on-chain LAS verification exceeds the block gas
/// limit". It MEASURES the arithmetic gas of one `base_verify` (D3 headline, plus D2/D5 for
/// parameter-sensitivity) on Foundry's local EVM (deterministic; not machine-dependent),
/// ESTIMATES the SHAKE256 challenge-hash cost from a per-permutation figure, sums them, and
/// compares the total to the block gas limit.
///
/// Run:  forge test --match-contract LASVerifyCost -vv        (logs the breakdown)
///       forge test --gas-report                              (clean per-function gas)
contract LASVerifyCostTest {
    LASVerifyCost cost;

    // Ethereum mainnet block gas limit: 30,000,000 for years (EIP-1559 cap), raised toward
    // ~36,000,000 during 2025. We compare against the conservative 30M.
    uint256 constant BLOCK_GAS_LIMIT = 30_000_000;

    // SHAKE256 challenge hash, ESTIMATED cost (a CALCULATION, not a same-machine measurement).
    // base_verify_internal recomputes c = H(pk, w', M): it absorbs the packed public key t, the
    // packed commitment w' (each LAS_N·N·4 bytes) and then the message M, so the absorb input
    // scales with n:
    //   D3 (n=6): 2 · 6 · 256 · 4 + |M| = 12321 B ⇒ ⌈12321/136⌉ = 91 Keccak-f[1600] permutations
    //   (SHAKE256 rate 136 B), plus SampleInBall(c_tilde) = 1 more (its 32-B seed fits one block)
    //   ⇒ 92 permutations.
    // A hand-rolled Keccak-f[1600] in EVM bytecode costs ~30,000 gas/permutation (24 rounds of
    // XOR/ROT/AND over 25 lanes); published SHA3-in-Solidity ports sit in the 25k–35k band, so
    // 30k is a representative ESTIMATE (hence the "Estimated" naming below). The native
    // keccak256 opcode CANNOT do real SHAKE256 (fixed padding + 256-bit squeeze), so it only
    // yields a floor, logged separately.
    uint256 constant D3_ROWS              = 6;                                    // LAS_N at D3 (headline)
    uint256 constant MESSAGE_BYTES        = 33;                                   // representative |M|
    uint256 constant SHAKE_ABSORB_BYTES   = 2 * D3_ROWS * 256 * 4 + MESSAGE_BYTES; // 12321
    uint256 constant KECCAKF_PERMUTATIONS = (SHAKE_ABSORB_BYTES + 135) / 136 + 1; // 91 + 1 = 92
    uint256 constant GAS_PER_KECCAKF      = 30_000;

    // Classical ECDSA settlement (ecrecover) claim from the AdaptorSwap gas report; a
    // fixed-cost precompile, so it is independent of the LAS parameter set. Ratio reference
    // only — refresh if AdaptorSwap.claimClassical changes.
    uint256 constant CLASSICAL_CLAIM_GAS = 75_709;

    function setUp() public {
        cost = new LASVerifyCost();
    }

    function test_NativeVerifyGas() public view {
        // ---- headline: D3 (n=6, ell=5). Sub-measurements live in helper frames so this
        //      orchestrator keeps a shallow local stack (avoids solc "stack too deep"). ----
        uint256 arithGas          = _measureLevel3();
        uint256 keccakNativeFloor = _keccakNativeFloor();
        uint256 hashEstimated     = KECCAKF_PERMUTATIONS * GAS_PER_KECCAKF; // arith measured + hash estimated
        uint256 totalEstimated    = arithGas + hashEstimated;

        _log("D3 verify ARITHMETIC (measured, gas)         ", arithGas);
        _log("D3 hash native-keccak256 FLOOR (measured)    ", keccakNativeFloor);
        _log("D3 hash ESTIMATED SHAKE256 (calculated, gas) ", hashEstimated);
        _log("D3 TOTAL native verify, hash-floor  (gas)    ", arithGas + keccakNativeFloor);
        _log("D3 TOTAL native verify, hash-estimated (gas) ", totalEstimated);
        _log("block gas limit (30M)                        ", BLOCK_GAS_LIMIT);
        _log("classical ecrecover claim (gas)              ", CLASSICAL_CLAIM_GAS);
        _log("native verify as multiple of classical claim ", totalEstimated / CLASSICAL_CLAIM_GAS);
        _log("native verify as percent of a 30M block      ", (totalEstimated * 100) / BLOCK_GAS_LIMIT);

        // per-primitive reconciliation (logs the four per-op costs + the rebuilt D3 total)
        _reconcile();

        // parameter-sensitivity: arithmetic gas growth D2 < D3 < D5 (D3 headline, D2/D5 support)
        uint256 gas2 = _measureLevel2();
        uint256 gas5 = _measureLevel5();
        _log("D2 (n=4,ell=4) verify ARITHMETIC (gas)       ", gas2);
        _log("D3 (n=6,ell=5) verify ARITHMETIC (gas)       ", arithGas);
        _log("D5 (n=8,ell=7) verify ARITHMETIC (gas)       ", gas5);

        // The honest assertions: the full native verify is a large fraction of a whole block —
        // but with EVM-native mulmod it does NOT, by itself, exceed the 30M limit; the real
        // barrier is economic plus the absence of a SHAKE/NTT precompile, NOT the hard ceiling.
        // If the block-limit assertion ever fails, that conclusion has flipped — update the docs.
        require(totalEstimated > 20 * CLASSICAL_CLAIM_GAS, "expected native verify >> classical claim");
        require(totalEstimated < BLOCK_GAS_LIMIT, "model says it would exceed a block (update docs!)");
        require(gas2 < arithGas && arithGas < gas5, "arith gas should grow with parameter size");
    }

    /* ---- warm-then-measure one level's arithmetic gas (isolated frames keep the stack shallow):
           the first call warms code/storage so the measured second call is execution only. ---- */
    function _measureLevel2() internal view returns (uint256) {
        cost.verifyArithLevel2(1);
        uint256 g = gasleft(); uint256 s = cost.verifyArithLevel2(0xC0FFEE); uint256 used = g - gasleft();
        require(s != 0, "L2 kernel optimised away");
        return used;
    }
    function _measureLevel3() internal view returns (uint256) {
        cost.verifyArithLevel3(1);
        uint256 g = gasleft(); uint256 s = cost.verifyArithLevel3(0xC0FFEE); uint256 used = g - gasleft();
        require(s != 0, "L3 kernel optimised away");
        return used;
    }
    function _measureLevel5() internal view returns (uint256) {
        cost.verifyArithLevel5(1);
        uint256 g = gasleft(); uint256 s = cost.verifyArithLevel5(0xC0FFEE); uint256 used = g - gasleft();
        require(s != 0, "L5 kernel optimised away");
        return used;
    }

    /* ---- native keccak256 FLOOR over the D3 absorb input (a strict lower bound for the hash) ---- */
    function _keccakNativeFloor() internal view returns (uint256) {
        bytes memory absorbInput = new bytes(SHAKE_ABSORB_BYTES);
        uint256 g = gasleft(); bytes32 h = keccak256(absorbInput); uint256 used = g - gasleft();
        require(h != bytes32(0), "hash sink");
        return used;
    }

    /* ---- per-primitive reconciliation: isolate one op as the gas delta between 11 and 1 reps
           (cancels the shared one-off setup), then rebuild the WHOLE D3 arithmetic from the op
           budget (12 fwd NTT + 12 inv NTT + 36 pointwise + 54 coeff passes) as an independent
           cross-check of the single verifyArithLevel3 measurement (all four op classes). ---- */
    function _reconcile() internal view {
        uint256 perNtt       = (_gasOf_ntt(11) - _gasOf_ntt(1)) / 10;
        uint256 perInvntt    = (_gasOf_invntt(11) - _gasOf_invntt(1)) / 10;
        uint256 perPointwise = (_gasOf_pointwise(11) - _gasOf_pointwise(1)) / 10;
        uint256 perAddpass   = (_gasOf_addpass(11) - _gasOf_addpass(1)) / 10;
        _log("per forward-NTT (measured, gas)              ", perNtt);
        _log("per inverse-NTT (measured, gas)              ", perInvntt);
        _log("per pointwise   (measured, gas)              ", perPointwise);
        _log("per coeff-pass  (measured, gas)              ", perAddpass);
        _log("D3 arith rebuilt from op budget (gas)        ",
             12 * perNtt + 12 * perInvntt + 36 * perPointwise + 54 * perAddpass);
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
    function _gasOf_addpass(uint256 reps) internal view returns (uint256) {
        cost.addpassReps(reps, reps); uint256 g = gasleft(); cost.addpassReps(reps, reps + 1); return g - gasleft();
    }

    /* ------- minimal console.log(string,uint256) without a forge-std dependency ------- */
    address constant CONSOLE = 0x000000000000000000636F6e736F6c652e6c6f67;
    function _log(string memory tag, uint256 val) internal view {
        (bool ok, ) = CONSOLE.staticcall(abi.encodeWithSignature("log(string,uint256)", tag, val));
        ok; // ignore — console is a no-op when absent
    }
}
