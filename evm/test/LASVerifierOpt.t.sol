// SPDX-License-Identifier: MIT
pragma solidity ^0.8.25;

import {LASVerify} from "../src/LASVerifier.sol";
import {LASVerifyOpt} from "../src/LASVerifierOpt.sol";
import {nttFw} from "../lib/zknox/ZKNOX_NTT_dilithium.sol";

/// @title LASVerifyOpt validation — same ACCEPT/REJECT bit as C, same bytes as LASVerify.
///
/// `LASVerifyOpt` exists to fit under EIP-7825's 16,777,216-gas per-transaction cap.
/// A faster verifier that is not the SAME verifier is worth nothing, so it is pinned
/// three ways, from the same C golden vectors `LASVerifier.t.sol` already uses:
///
///   1. w' — the packed w' it produces must equal `w_prime.bin` BYTE-FOR-BYTE. This is
///      the strongest check on the restructured arithmetic (t̂ registered rather than
///      re-transformed; the two products summed in NTT domain and inverted once), and
///      it is byte-level rather than coefficient-level because the packed encoding is
///      itself part of the hash preimage.
///   2. the ACCEPT bit on the golden adapted signature, plus REJECT on a tampered
///      z byte, a tampered c_tilde and a wrong message.
///   3. agreement with the vendored `LASVerify` on every one of those four inputs, so
///      a future change to either implementation cannot silently diverge.
///
/// forge treats a revert as failure; no forge-std dependency.
contract LASVerifierOptTest {
    Vm constant vm = Vm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);
    uint256 constant N = 256;

    LASOptHarness harness;

    function setUp() public {
        harness = new LASOptHarness();
    }

    // ---- vector loading (mirrors LASVerifier.t.sol) ----

    function _readPolys(string memory name, uint256 count) internal view returns (uint256[][] memory polys) {
        bytes memory raw = vm.readFileBinary(string.concat("test/vectors/", name));
        require(raw.length == count * N * 4, "bad polys file length");
        polys = new uint256[][](count);
        uint256 off = 0;
        for (uint256 i = 0; i < count; i++) {
            polys[i] = new uint256[](N);
            for (uint256 k = 0; k < N; k++) {
                polys[i][k] = uint256(uint8(raw[off])) | (uint256(uint8(raw[off + 1])) << 8)
                    | (uint256(uint8(raw[off + 2])) << 16) | (uint256(uint8(raw[off + 3])) << 24);
                off += 4;
            }
        }
    }

    /// Transport encoding for the NTT-domain inputs: 4 bytes BIG-endian per coefficient,
    /// 8 per 32-byte word, so the verifier extracts with one shift+mask per coefficient.
    function _packBE(uint256[][] memory polys) internal pure returns (bytes memory out) {
        out = new bytes(polys.length * N * 4);
        uint256 o = 0;
        for (uint256 i = 0; i < polys.length; i++) {
            for (uint256 k = 0; k < N; k++) {
                uint256 x = polys[i][k];
                out[o] = bytes1(uint8(x >> 24));
                out[o + 1] = bytes1(uint8(x >> 16));
                out[o + 2] = bytes1(uint8(x >> 8));
                out[o + 3] = bytes1(uint8(x));
                o += 4;
            }
        }
    }

    /// The three packed calldata inputs plus message and signature.
    function _optInputs()
        internal
        view
        returns (bytes memory aHat, bytes memory tHat, bytes memory tPacked, bytes memory message, bytes memory sig)
    {
        uint256[][] memory aNormal = _readPolys("pp_normal.bin", 6 * 5);
        for (uint256 i = 0; i < aNormal.length; i++) {
            aNormal[i] = nttFw(aNormal[i]); // registration: A' -> NTT domain, once
        }
        aHat = _packBE(aNormal);

        tHat = _packBE(_toNtt(_readPolys("t.bin", 6))); // registration: t -> NTT domain, once

        // pack(t) is the little-endian 4-byte-per-coefficient form the C signer hashed,
        // which is byte-identical to t.bin itself. Asserted in test_tPacked_is_t_bin.
        tPacked = vm.readFileBinary("test/vectors/t.bin");

        message = vm.readFileBinary("test/vectors/msg.bin");
        sig = vm.readFileBinary("test/vectors/sig.bin");
    }

    function _toNtt(uint256[][] memory polys) internal pure returns (uint256[][] memory out) {
        out = new uint256[][](polys.length);
        for (uint256 i = 0; i < polys.length; i++) {
            uint256[] memory copyPoly = new uint256[](N);
            for (uint256 k = 0; k < N; k++) {
                copyPoly[k] = polys[i][k];
            }
            out[i] = nttFw(copyPoly);
        }
    }

    function _vendoredInputs()
        internal
        view
        returns (uint256[][] memory AprimeHat, uint256[][] memory t, bytes memory message, bytes memory sig)
    {
        AprimeHat = LASVerify.toNttDomain(_readPolys("pp_normal.bin", 6 * 5));
        t = _readPolys("t.bin", 6);
        message = vm.readFileBinary("test/vectors/msg.bin");
        sig = vm.readFileBinary("test/vectors/sig.bin");
    }

    // ---- 1. the encoding invariant ----

    /// pack(t) — the first third of the hash preimage — is exactly the bytes of t.bin.
    /// If this ever stops holding, `tPacked` is not the preimage the C side hashed and
    /// every other test here is measuring the wrong thing.
    function test_tPacked_is_t_bin() public view {
        uint256[][] memory t = _readPolys("t.bin", 6);
        bytes memory raw = vm.readFileBinary("test/vectors/t.bin");
        require(raw.length == LASVerifyOpt.TPACK_BYTES, "t.bin is not TPACK_BYTES");
        uint256 o = 0;
        for (uint256 i = 0; i < 6; i++) {
            for (uint256 k = 0; k < N; k++) {
                uint256 x = t[i][k];
                require(uint8(raw[o]) == uint8(x), "pack(t) byte 0");
                require(uint8(raw[o + 1]) == uint8(x >> 8), "pack(t) byte 1");
                require(uint8(raw[o + 2]) == uint8(x >> 16), "pack(t) byte 2");
                require(uint8(raw[o + 3]) == uint8(x >> 24), "pack(t) byte 3");
                o += 4;
            }
        }
    }

    // ---- 2. arithmetic against C ground truth ----

    /// The restructured w' must be byte-identical to the C golden w_prime.bin.
    function test_wprime_packed_matches_C_golden() public view {
        (bytes memory aHat, bytes memory tHat,,, bytes memory sig) = _optInputs();
        bytes memory got = harness.wprimePacked(aHat, tHat, sig);
        bytes memory want = vm.readFileBinary("test/vectors/w_prime.bin");
        require(got.length == want.length, "w' packed length mismatch");
        for (uint256 i = 0; i < want.length; i++) {
            require(got[i] == want[i], "restructured w' != C golden w_prime.bin");
        }
    }

    // ---- 3. the ACCEPT/REJECT bit, and agreement with the vendored verifier ----

    function test_opt_accepts_golden() public view {
        (bytes memory aHat, bytes memory tHat, bytes memory tPacked, bytes memory message, bytes memory sig) =
            _optInputs();
        require(harness.verify(aHat, tHat, tPacked, message, sig), "optimised verify REJECTED the golden sig");
    }

    function test_opt_rejects_tampered_sig() public view {
        (bytes memory aHat, bytes memory tHat, bytes memory tPacked, bytes memory message, bytes memory sig) =
            _optInputs();
        sig[100] = bytes1(uint8(sig[100]) ^ 0xFF); // in the z region (>= 48)
        require(!harness.verify(aHat, tHat, tPacked, message, sig), "tampered sig was ACCEPTED");
    }

    function test_opt_rejects_tampered_ctilde() public view {
        (bytes memory aHat, bytes memory tHat, bytes memory tPacked, bytes memory message, bytes memory sig) =
            _optInputs();
        sig[0] = bytes1(uint8(sig[0]) ^ 0x01);
        require(!harness.verify(aHat, tHat, tPacked, message, sig), "tampered c_tilde was ACCEPTED");
    }

    function test_opt_rejects_tampered_message() public view {
        (bytes memory aHat, bytes memory tHat, bytes memory tPacked, bytes memory message, bytes memory sig) =
            _optInputs();
        message[0] = bytes1(uint8(message[0]) ^ 0xFF);
        require(!harness.verify(aHat, tHat, tPacked, message, sig), "wrong message was ACCEPTED");
    }

    /// An over-bound z must be rejected by the fused field-level norm gate.
    ///
    /// The gate was moved off the decoded coefficient and onto the raw 19-bit field
    /// (accept iff field <= 2·BOUND), so it is checked in ISOLATION here — tampering
    /// with z also breaks the digest, and a `verify` that returned false would not on
    /// its own prove the gate fired. The first z field starts at byte 48 bit 0; setting
    /// its 19 bits to 1 gives 0x7FFFF = 524287, well past 2·BOUND = 275870.
    function test_opt_rejects_over_bound_z() public view {
        (bytes memory aHat, bytes memory tHat, bytes memory tPacked, bytes memory message, bytes memory sig) =
            _optInputs();
        require(harness.decodeNormOk(sig), "golden sig failed the norm gate");

        sig[48] = bytes1(uint8(0xFF));
        sig[49] = bytes1(uint8(0xFF));
        sig[50] = bytes1(uint8(uint8(sig[50]) | 0x07));

        require(!harness.decodeNormOk(sig), "norm gate passed an over-bound coefficient");
        require(!harness.verify(aHat, tHat, tPacked, message, sig), "over-bound z was ACCEPTED");
    }

    /// Both implementations must return the same bit on the same input — the golden
    /// signature and each tampering. Divergence here means one of them is wrong.
    function test_opt_agrees_with_vendored_verifier() public view {
        for (uint256 case_ = 0; case_ < 4; case_++) {
            (bytes memory aHat, bytes memory tHat, bytes memory tPacked, bytes memory message, bytes memory sig) =
                _optInputs();
            (uint256[][] memory AprimeHat, uint256[][] memory t, bytes memory vMessage, bytes memory vSig) =
                _vendoredInputs();

            if (case_ == 1) {
                sig[100] = bytes1(uint8(sig[100]) ^ 0xFF);
                vSig[100] = bytes1(uint8(vSig[100]) ^ 0xFF);
            } else if (case_ == 2) {
                sig[0] = bytes1(uint8(sig[0]) ^ 0x01);
                vSig[0] = bytes1(uint8(vSig[0]) ^ 0x01);
            } else if (case_ == 3) {
                message[0] = bytes1(uint8(message[0]) ^ 0xFF);
                vMessage[0] = bytes1(uint8(vMessage[0]) ^ 0xFF);
            }

            bool optBit = harness.verify(aHat, tHat, tPacked, message, sig);
            bool refBit = LASVerify.verify(AprimeHat, t, vMessage, vSig);
            require(optBit == refBit, "LASVerifyOpt and LASVerify disagree");
        }
    }
}

/// External wrapper: `LASVerifyOpt` takes `bytes calldata`, so the tests must reach it
/// through a real external call — which is also how the swap contract calls it.
contract LASOptHarness {
    function verify(
        bytes calldata aHatPacked,
        bytes calldata tHatPacked,
        bytes calldata tPacked,
        bytes calldata message,
        bytes calldata sig
    ) external pure returns (bool) {
        return LASVerifyOpt.verify(aHatPacked, tHatPacked, tPacked, message, sig);
    }

    function wprimePacked(bytes calldata aHatPacked, bytes calldata tHatPacked, bytes calldata sig)
        external
        pure
        returns (bytes memory)
    {
        return LASVerifyOpt.computeWPrimePacked(aHatPacked, tHatPacked, sig);
    }

    /// The fused decode+norm gate on its own, so it can be tested without the digest.
    function decodeNormOk(bytes calldata sig) external pure returns (bool ok) {
        (, ok) = LASVerifyOpt._decodeZ(sig);
    }
}

interface Vm {
    function readFileBinary(string calldata path) external view returns (bytes memory);
}
