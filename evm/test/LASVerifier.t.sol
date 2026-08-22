// SPDX-License-Identifier: MIT
pragma solidity ^0.8.25;

import {LASVerify} from "../src/LASVerifier.sol";

/// @title Stage-5 end-to-end validation of the full on-chain LAS base verifier.
///
/// The C exporter (ref/test/export_verify_vector.c) hard-asserts that
/// base_verify accepts the golden adapted signature; this test proves the
/// Solidity LASVerify.verify reproduces that ACCEPT bit-for-bit, and rejects
/// tampered signature/message bytes. Inputs come from the exported golden
/// vectors. A' is put into NTT domain ONCE (models registration), exactly as
/// setup_public_params NTTs pp->a_prime once.
///
/// forge treats a revert as failure; no forge-std dependency.
contract LASVerifierTest {
    Vm constant vm = Vm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);
    uint256 constant N = 256;

    /// Read `count` consecutive 256-coeff int32-LE polynomials (canonical [0,Q)).
    function _readPolys(string memory name, uint256 count) internal view returns (uint256[][] memory polys) {
        bytes memory raw = vm.readFileBinary(string.concat("test/vectors/", name));
        require(raw.length == count * N * 4, "bad polys file length");
        polys = new uint256[][](count);
        uint256 off = 0;
        for (uint256 i = 0; i < count; i++) {
            polys[i] = new uint256[](N);
            for (uint256 k = 0; k < N; k++) {
                polys[i][k] = uint256(uint8(raw[off]))
                    | (uint256(uint8(raw[off + 1])) << 8)
                    | (uint256(uint8(raw[off + 2])) << 16)
                    | (uint256(uint8(raw[off + 3])) << 24);
                off += 4;
            }
        }
    }

    /// A' put into NTT domain once (registration), t and the signed message/sig.
    function _inputs()
        internal
        view
        returns (uint256[][] memory AprimeHat, uint256[][] memory t, bytes memory message, bytes memory sig)
    {
        AprimeHat = LASVerify.toNttDomain(_readPolys("pp_normal.bin", 6 * 5)); // 30 polys
        t = _readPolys("t.bin", 6);
        message = vm.readFileBinary("test/vectors/msg.bin");
        sig = vm.readFileBinary("test/vectors/sig.bin");
    }

    /// Debug: the reconstructed w' must equal the C golden w_prime.bin, isolating
    /// the arithmetic (Stage 5) from the hash.
    function test_wprime_matches_C_golden() public view {
        (uint256[][] memory AprimeHat, uint256[][] memory t,, bytes memory sig) = _inputs();
        uint256[][] memory w = LASVerify.computeWPrime(AprimeHat, t, sig);
        uint256[][] memory want = _readPolys("w_prime.bin", 6);
        for (uint256 i = 0; i < 6; i++) {
            for (uint256 k = 0; k < N; k++) {
                require(w[i][k] == want[i][k], "w' != C golden");
            }
        }
    }

    /// Debug: pack(t)‖pack(w_golden)‖M hashed must equal c_tilde = sig[0:CTILDE_BYTES],
    /// using the KNOWN-correct w_prime.bin — isolates pack+hash from arithmetic.
    function test_oracle_with_golden_wprime_matches_ctilde() public view {
        (, uint256[][] memory t,, bytes memory sig) = _inputs();
        bytes memory message = vm.readFileBinary("test/vectors/msg.bin");
        uint256[][] memory wgold = _readPolys("w_prime.bin", 6);
        bytes memory dg = LASVerify.oracle(t, wgold, message);
        for (uint256 i = 0; i < LASVerify.CTILDE_BYTES; i++) {
            require(dg[i] == sig[i], "oracle(pack(t),pack(w_golden),M) != c_tilde");
        }
    }

    /// The golden adapted signature MUST verify (matches C base_verify = ACCEPT).
    function test_verify_accepts_golden() public view {
        (uint256[][] memory AprimeHat, uint256[][] memory t, bytes memory message, bytes memory sig) = _inputs();
        require(LASVerify.verify(AprimeHat, t, message, sig), "on-chain verify REJECTED the golden sig");
    }

    /// Flipping one byte in the z region of the signature must be rejected.
    function test_verify_rejects_tampered_sig() public view {
        (uint256[][] memory AprimeHat, uint256[][] memory t, bytes memory message, bytes memory sig) = _inputs();
        sig[100] = bytes1(uint8(sig[100]) ^ 0xFF); // byte 100 is in the z region (>=32)
        require(!LASVerify.verify(AprimeHat, t, message, sig), "tampered sig was ACCEPTED");
    }

    /// Flipping the challenge digest c_tilde must be rejected.
    function test_verify_rejects_tampered_ctilde() public view {
        (uint256[][] memory AprimeHat, uint256[][] memory t, bytes memory message, bytes memory sig) = _inputs();
        sig[0] = bytes1(uint8(sig[0]) ^ 0x01); // c_tilde byte
        require(!LASVerify.verify(AprimeHat, t, message, sig), "tampered c_tilde was ACCEPTED");
    }

    /// Verifying against a different message must be rejected.
    function test_verify_rejects_tampered_message() public view {
        (uint256[][] memory AprimeHat, uint256[][] memory t, bytes memory message, bytes memory sig) = _inputs();
        message[0] = bytes1(uint8(message[0]) ^ 0xFF);
        require(!LASVerify.verify(AprimeHat, t, message, sig), "wrong message was ACCEPTED");
    }
}

/// Minimal Foundry cheatcode interface (mirrors AdaptorSwap.t.sol; no forge-std).
interface Vm {
    function readFileBinary(string calldata path) external view returns (bytes memory);
}
