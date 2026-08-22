// SPDX-License-Identifier: MIT
pragma solidity ^0.8.25;

import {CtxShake, shakeInit, shakeUpdate, shakeDigest, shakeSqueeze} from "../lib/zknox/ZKNOX_shake.sol";

/// @title Stage-2 validation of the VENDORED SHAKE256 (ZKNox, evm/lib/zknox/ZKNOX_shake.sol).
///
/// The on-chain LAS verifier needs SHAKE256 in exactly two shapes, both mirrored
/// from ref/basesig.c:
///   (1) MULTI-ABSORB then 32-byte digest  ->  c̃ = SHAKE256(pack(t) ‖ pack(w') ‖ M)
///       (base_verify_internal: three shake256_absorb calls then squeeze 32);
///   (2) absorb c̃, digest 8, then STREAM one byte at a time  ->  SampleInBall
///       (b_poly_challenge: squeezeblocks-driven rejection loop).
///
/// This test anchors the vendored primitive to a NIST known-answer vector and
/// then checks those two usage patterns for self-consistency. forge treats any
/// revert as a failure, so no forge-std dependency is needed (same posture as
/// AdaptorSwap.t.sol).
contract ZKNoxShakeTest {
    function _eq(bytes memory a, bytes memory b, string memory what) internal pure {
        require(keccak256(a) == keccak256(b), what);
    }

    function _shake256(bytes memory input, uint256 outlen) internal pure returns (bytes memory) {
        CtxShake memory ctx = shakeInit();
        ctx = shakeUpdate(ctx, input);
        return shakeDigest(ctx, outlen);
    }

    /// NIST known-answer vector: first 32 bytes of SHAKE256("") .
    function test_shake256_empty_KAT() public pure {
        CtxShake memory ctx = shakeInit();
        bytes memory got = shakeDigest(ctx, 32); // empty message: no absorb needed
        bytes memory want = hex"46b9dd2b0ba88d13233b3feb743eeb243fcd52ea62b81b82b50c27646ed5762f";
        _eq(got, want, "SHAKE256(empty)[0:32] != NIST KAT");
    }

    /// Same KAT reached via an explicit empty absorb (the helper path).
    function test_shake256_empty_viaUpdate() public pure {
        bytes memory got = _shake256(hex"", 32);
        bytes memory want = hex"46b9dd2b0ba88d13233b3feb743eeb243fcd52ea62b81b82b50c27646ed5762f";
        _eq(got, want, "SHAKE256(empty via update) mismatch");
    }

    /// (1) MULTI-ABSORB must equal one-shot absorb: SHAKE256(A ‖ B ‖ C) computed
    /// as three shakeUpdate calls == the same bytes fed in one call. This is the
    /// exact shape of the challenge hash H(pk ‖ w' ‖ M).
    function test_shake256_multiAbsorb_equals_oneShot() public pure {
        bytes memory a = hex"0011223344556677";
        bytes memory b = hex"8899aabbccddeeff0102";
        bytes memory c = hex"cafebabe";

        // one-shot over the concatenation
        bytes memory oneShot = _shake256(bytes.concat(a, b, c), 96);

        // three absorbs, then digest
        CtxShake memory ctx = shakeInit();
        ctx = shakeUpdate(ctx, a);
        ctx = shakeUpdate(ctx, b);
        ctx = shakeUpdate(ctx, c);
        bytes memory multi = shakeDigest(ctx, 96);

        _eq(multi, oneShot, "multi-absorb != one-shot absorb");
    }

    /// (1b) MULTI-ABSORB across MANY blocks with mid-block continuation — the exact
    /// hash the verifier runs: SHAKE256( pack(t)[6144] ‖ pack(w')[6144] ‖ M[32] ).
    /// Three shakeUpdate calls of block-crossing lengths must equal one-shot.
    function test_shake256_multiAbsorb_large_blockcrossing() public pure {
        bytes memory a = new bytes(6144);
        bytes memory b = new bytes(6144);
        bytes memory c = new bytes(32);
        for (uint256 i = 0; i < 6144; i++) {
            a[i] = bytes1(uint8((i * 7 + 1) & 0xff));
            b[i] = bytes1(uint8((i * 13 + 3) & 0xff));
        }
        for (uint256 i = 0; i < 32; i++) c[i] = bytes1(uint8(i));

        bytes memory oneShot = _shake256(bytes.concat(a, b, c), 32);

        CtxShake memory ctx = shakeInit();
        ctx = shakeUpdate(ctx, a);
        ctx = shakeUpdate(ctx, b);
        ctx = shakeUpdate(ctx, c);
        bytes memory multi = shakeDigest(ctx, 32);

        _eq(multi, oneShot, "large multi-absorb != one-shot (verifier hash pattern)");
    }

    /// (2) STREAMING SQUEEZE must equal a bulk digest: digest(64) == digest(8)
    /// followed by squeeze(56). This mirrors the SampleInBall idiom exactly
    /// (ZKNOX_SampleInBall.sampleInBallNist): shakeDigest mutates ctx in place
    /// (memory-by-reference), leaving it in the squeezing phase at offset 8, so
    /// a following shakeSqueeze continues the SAME sponge stream.
    function test_shake256_streamingSqueeze_equals_bulk() public pure {
        bytes memory seed = hex"a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5a5"; // 16-byte seed

        // bulk: 64 bytes in one digest
        bytes memory bulk = _shake256(seed, 64);

        // streamed: absorb, digest 8 (finalizes+pads, leaves ctx squeezing@8),
        // then squeeze 56 more from the same stream — the SampleInBall pattern.
        CtxShake memory ctx = shakeInit();
        ctx = shakeUpdate(ctx, seed);
        bytes memory head = shakeDigest(ctx, 8);
        bytes memory tail;
        (ctx, tail) = shakeSqueeze(ctx, 56);

        bytes memory streamed = bytes.concat(head, tail);
        _eq(streamed, bulk, "streaming squeeze != bulk digest");
    }
}
