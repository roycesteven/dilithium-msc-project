// SPDX-License-Identifier: MIT
pragma solidity ^0.8.25;

import {LASShake} from "../src/LASShake.sol";
import {console} from "./TwoLegSwapGas.t.sol";

/// @title LASShakeGrowth — the measured `absorbPad` growth used to answer "does on-chain LAS
///        verification still fit one EIP-7825 transaction at Dilithium-V?"
///
/// THIS TEST STATES NO VERDICT. It emits measurements. The decision is assembled in
/// `scripts/derive_onchain_d5_bound.py`, where every subtraction is explicit and auditable —
/// the threshold depends on a client receipt and on a calldata projection, neither of which is
/// knowable inside the EVM.
///
/// WHY IT EXISTS. `LASVerifyOpt` is MEASURED to fit at Simplified Dilithium-III with 363,941
/// gas of headroom against EIP-7825. Whether it fits at Dilithium-V was argued from arithmetic
/// five times and found unsound five times. Every defect is recorded so the sixth attempt does
/// not reintroduce one:
///
///   1. "Execution can only grow" is not a bound. Calldata byte content is a SECOND free
///      variable; pushed the D5-favourable way (every added byte zero) with execution frozen,
///      D5 lands *under* the cap. A worst case binds only when every free variable is pushed
///      the adverse way at once.
///   2. `shake_stage_gas / block_count` does NOT lower-bound one added block: the stage total
///      also carries `init()` and the pad tail, so the quotient OVERstates per-block cost.
///   3. `SampleInBall` is not the only data-dependent stage — `_decodeZ` branches on
///      coefficient value (`if gt(f, 137935)`), so its cost is signature-dependent too.
///      Neither may be assumed to cost at least its D3 value at D5; both are subtracted whole.
///   4. Measuring `absorbPad` at two lengths in two frames is contaminated: `absorbPad`
///      allocates its 168-byte pad scratch INSIDE the timed frame, so memory-expansion gas
///      lands in the measurement and differs between a 12,320-byte and a 16,416-byte buffer.
///   5. The two lengths do not share a tail PATH, so the difference cannot be treated as
///      "30 blocks, tail dropped". At rate 136: D3 leaves rem = 80 → two word-copies plus the
///      `if tb` partial-word branch; D5 leaves rem = 96 → three word-copies and skips that
///      branch. `tail_D5 - tail_D3` is therefore not provably non-negative, and dropping it is
///      NOT conservative.
///
/// HOW BOTH 4 AND 5 ARE REMOVED AT ONCE. Defects 4 and 5 share one root cause: memory state
/// was allowed to vary between measurements, which then forced the tail to be modelled rather
/// than measured. So this harness allocates ONE fixed arena at the LARGER length and varies
/// only `absorbPad`'s `len` argument. Every measurement therefore begins with an identical free
/// memory pointer, the 168-byte scratch is allocated at an identical offset every time, and its
/// expansion cost cancels EXACTLY in the difference. Nothing about the tail needs to be
/// assumed, bounded or dropped — whatever it costs is inside the measured difference:
///
///     preimage = pack(t) ‖ pack(w') ‖ M = 2·n·1024 + |M|,  |M| = 32 as measured on chain
///     D3 (n = 6): 12,320 B          D5 (n = 8): 16,416 B
///
///     deltaAbsorb = absorbGas(16,416) - absorbGas(12,320)
///
/// ⚠ WHAT THIS QUANTITY IS, EXACTLY. It is the exact fixed-arena difference of two `absorbPad`
/// calls, and nothing more. It is NOT the whole verifier hash/check delta.
/// `LASGasBreakdown`'s named SHAKE stage measures `init() + absorbPad()` and stops there; this
/// measurement isolates the `absorbPad` difference alone (`init()` sits outside the timed frame
/// here, and is parameter-independent in any case). The verifier additionally runs
/// `_digestMatches`, whose comparison would be longer at D5 — six lanes against eight — but
/// that positive term is NOT credited.
///
/// ⚠ No claim is made here about the verifier's TOTAL execution growth, and none may be read
/// in. Total growth is NOT provably positive: `_decodeZ` and `SampleInBall` are data-dependent
/// and could cost less on a D5 instance than on the measured D3 one. That is handled where it
/// belongs — in `scripts/derive_onchain_d5_bound.py`, by subtracting both stages whole.
///
/// Never call this the stage delta, and never "exact SHAKE growth": it is exact only as what
/// it literally measures.
///
/// The 12,320 figure is the one `LASGasBreakdown` already attributes its SHAKE stage against,
/// so the two runs are commensurable.
///
/// `permute` is measured separately as an independent fixed-cost unit, so `deltaAbsorb` can be
/// sanity-checked against 30 × permute rather than taken on trust. It is a cross-check, not the
/// bound: unrolled explicitly, because a loop of permutes measures `permute + loop overhead`.
///
/// ⚠ Run WITHOUT `--gas-report`: the inspector is metered inside the measured frame and
/// inflates `gasleft()` deltas. Same rule as the cap gates in `LASGasBreakdown.t.sol`.
contract LASShakeGrowthTest {
    uint256 constant PREIMAGE_D3 = 12_320; // n = 6
    uint256 constant PREIMAGE_D5 = 16_416; // n = 8
    uint256 constant SHAKE256_RATE = 136;

    ShakeHarness h;

    function setUp() public {
        h = new ShakeHarness();
    }

    /// The primary measurement: the exact fixed-arena `absorbPad` difference — see the note
    /// above for what that is and is not. Both calls run against an arena sized for D5, so the
    /// only difference between them is the work `absorbPad` performs: memory expansion cancels,
    /// and the differing pad-tail path is not cancelled but MEASURED, since whatever it costs
    /// is inside the difference.
    function test_absorb_growth_D3_to_D5() public {
        h.absorbGas(PREIMAGE_D3); // warm-up, discarded
        h.absorbGas(PREIMAGE_D5);

        uint256 gasD3 = h.absorbGas(PREIMAGE_D3);
        uint256 gasD5 = h.absorbGas(PREIMAGE_D5);
        require(gasD5 > gasD3, "absorb did not grow with preimage length");

        console.log("absorbPad arena bytes (identical in both calls)", PREIMAGE_D5);
        console.log("absorbPad len D3 / D5", PREIMAGE_D3, PREIMAGE_D5);
        console.log("absorbPad gas D3", gasD3);
        console.log("absorbPad gas D5", gasD5);
        console.log("absorbPad DELTA gas D5 minus D3", gasD5 - gasD3);
    }

    /// Cross-check only. Explicit unrolling, so the difference is exactly eight more `permute`
    /// invocations with no loop counter, comparison or jump mixed in.
    function test_permute_unit_cost() public {
        h.permute8(); // warm-up, discarded
        h.permute16(); // warm-up, discarded

        uint256 g8 = h.permute8();
        uint256 g16 = h.permute16();
        require(g16 > g8, "unrolled permute did not scale");

        uint256 marginal = (g16 - g8) / 8;
        console.log("permute unrolled x8 / x16 gas", g8, g16);
        console.log("permute MARGINAL gas per call", marginal);
        console.log("30 x marginal permute (cross-check only)", 30 * marginal);
    }

    /// Guards the arithmetic a reader will do with these numbers: that the added FULL blocks
    /// number 30, and that each length keeps a short tail block — so the two differ in tail
    /// cost only, never in block accounting.
    function test_block_arithmetic_holds() public view {
        uint256 fullD3 = PREIMAGE_D3 / SHAKE256_RATE;
        uint256 fullD5 = PREIMAGE_D5 / SHAKE256_RATE;
        require(fullD5 - fullD3 == 30, "full-block delta is not 30");
        require(PREIMAGE_D3 % SHAKE256_RATE != 0, "D3 has no short tail block");
        require(PREIMAGE_D5 % SHAKE256_RATE != 0, "D5 has no short tail block");
        console.log("full rate blocks D3 / D5", fullD3, fullD5);
        console.log("tail bytes D3 / D5", PREIMAGE_D3 % SHAKE256_RATE, PREIMAGE_D5 % SHAKE256_RATE);
    }
}

/// Separate contract so each measurement is an external call starting from a clean frame.
/// `permute` and `absorbPad` mutate the sponge in memory, so the optimiser cannot fold the
/// repeated calls together.
contract ShakeHarness {
    uint256 constant ARENA = 16_416; // the LARGER preimage; every call allocates exactly this

    /// Keccak-f has no data-dependent control flow, so the fill cannot change the cost;
    /// filling rather than leaving zeros removes the question instead of relying on that.
    function _arena() internal pure returns (uint256 ptr) {
        bytes memory b = new bytes(ARENA);
        for (uint256 i = 0; i < ARENA; i++) {
            b[i] = bytes1(uint8((i * 31 + 7) & 0xff));
        }
        assembly {
            ptr := add(b, 32)
        }
    }

    /// The arena is ALWAYS `ARENA` bytes and `init()` always follows it, so at the moment the
    /// clock starts the free memory pointer is identical no matter which `len` is passed. That
    /// is what makes the pad-scratch allocation inside `absorbPad` cancel in the difference.
    function absorbGas(uint256 len) external view returns (uint256 used) {
        require(len <= ARENA, "len exceeds arena");
        uint256 ptr = _arena();
        uint256 p = LASShake.init();
        uint256 before = gasleft();
        LASShake.absorbPad(p, ptr, len);
        used = before - gasleft();
    }

    function permute8() external view returns (uint256 used) {
        uint256 p = LASShake.init();
        uint256 before = gasleft();
        LASShake.permute(p);
        LASShake.permute(p);
        LASShake.permute(p);
        LASShake.permute(p);
        LASShake.permute(p);
        LASShake.permute(p);
        LASShake.permute(p);
        LASShake.permute(p);
        used = before - gasleft();
    }

    function permute16() external view returns (uint256 used) {
        uint256 p = LASShake.init();
        uint256 before = gasleft();
        LASShake.permute(p);
        LASShake.permute(p);
        LASShake.permute(p);
        LASShake.permute(p);
        LASShake.permute(p);
        LASShake.permute(p);
        LASShake.permute(p);
        LASShake.permute(p);
        LASShake.permute(p);
        LASShake.permute(p);
        LASShake.permute(p);
        LASShake.permute(p);
        LASShake.permute(p);
        LASShake.permute(p);
        LASShake.permute(p);
        LASShake.permute(p);
        used = before - gasleft();
    }
}
