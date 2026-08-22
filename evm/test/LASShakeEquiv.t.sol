// SPDX-License-Identifier: MIT
pragma solidity ^0.8.25;

import {LASShake} from "../src/LASShake.sol";
import {CtxShake, shakeInit, shakeUpdate, shakeDigest} from "../lib/zknox/ZKNOX_shake.sol";

/// @title LASShake ⇄ vendored ZKNox SHAKE256 equivalence.
///
/// `src/LASShake.sol` is the rewritten sponge that brings on-chain LAS verification
/// under EIP-7825's per-transaction gas cap. It is a re-encoding, NOT a new hash: the
/// only defensible way to say so is to pin it against the vendored implementation that
/// `test/ZKNoxShake.t.sol` already validates against NIST KATs, at every length class
/// the verifier can hit.
///
/// Lengths chosen deliberately around the rate (136 bytes):
///   0                 empty absorb (padding-only block)
///   1, 47             short
///   48                c_tilde — the SampleInBall absorb
///   135, 136, 137     rate−1 / exact rate / rate+1 — where padding and the
///                     "message is a whole number of blocks still needs a pad block"
///                     rule are easiest to get wrong. At 135 the 0x1f and 0x80
///                     markers collide in the same byte.
///   271, 272          two-block boundary
///   12320             the real challenge preimage: pack(t)‖pack(w')‖M at D3 with a
///                     32-byte message = 6144+6144+32, i.e. 90 full blocks + 80 bytes.
///
/// forge treats a revert as failure; no forge-std dependency.
contract LASShakeEquivTest {
    uint256[10] internal LENGTHS = [uint256(0), 1, 47, 48, 135, 136, 137, 271, 272, 12320];

    /// Deterministic, non-constant filler so a byte-index bug cannot pass by accident.
    function _pattern(uint256 len) internal pure returns (bytes memory b) {
        b = new bytes(len);
        for (uint256 i = 0; i < len; i++) {
            b[i] = bytes1(uint8((i * 167 + (i >> 5) * 31 + 7) & 0xff));
        }
    }

    function _vendored(bytes memory input, uint256 outLen) internal pure returns (bytes memory) {
        CtxShake memory ctx = shakeInit();
        ctx = shakeUpdate(ctx, input);
        return shakeDigest(ctx, outLen);
    }

    /// Every length class, squeezing 48 bytes (the c_tilde width the verifier compares).
    function test_equals_vendored_at_all_length_classes() public view {
        for (uint256 i = 0; i < LENGTHS.length; i++) {
            bytes memory input = _pattern(LENGTHS[i]);
            bytes memory got = LASShake.digest(input, 48);
            bytes memory want = _vendored(input, 48);
            require(got.length == want.length, "digest length mismatch");
            for (uint256 k = 0; k < want.length; k++) {
                require(got[k] == want[k], "LASShake != vendored SHAKE256");
            }
        }
    }

    /// A squeeze longer than one rate block forces the output-side re-permutation.
    function test_equals_vendored_across_squeeze_block_boundary() public pure {
        bytes memory input = _pattern(200);
        bytes memory got = LASShake.digest(input, 300); // > RATE, spans 3 output blocks
        bytes memory want = _vendored(input, 300);
        for (uint256 k = 0; k < 300; k++) {
            require(got[k] == want[k], "squeeze past the rate diverges");
        }
    }

    /// The SampleInBall entry condition: lane 0 must equal the first 8 squeezed bytes
    /// read little-endian, which is what `_sampleInBall` relies on for its sign word.
    function test_lane0_is_the_first_eight_output_bytes() public pure {
        bytes memory input = _pattern(48);
        uint256 p = LASShake.init();
        uint256 ptr;
        assembly {
            ptr := add(input, 32)
        }
        LASShake.absorbPad(p, ptr, 48);
        bytes memory out = _vendored(input, 8);
        uint256 le;
        for (uint256 i = 0; i < 8; i++) {
            le |= uint256(uint8(out[i])) << (8 * i);
        }
        require(le == LASShake.laneAt(p, 0), "lane 0 != first 8 output bytes LE");
    }
}
