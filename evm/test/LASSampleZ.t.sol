// SPDX-License-Identifier: MIT
pragma solidity ^0.8.25;

import {sampleInBallNist} from "../lib/zknox/ZKNOX_SampleInBall.sol";
import {LASVerify} from "../src/LASVerifier.sol";

/// @title Stage-4 validation: SampleInBall reuse + BitPack19 z-decode + norm check.
///
///  (a) SampleInBall: the reused ZKNox sampleInBallNist(c_tilde, KAPPA=49, q) must
///      equal our C challenge H(c_tilde) (ref/basesig.c b_poly_challenge), exported
///      canonical {0,1,Q-1} as c.bin.
///  (b) z-decode: LAS's wire z is c_tilde(32B) ‖ BitPack19(z), LSB-first, each 19-bit
///      field = LAS_Z_OFFSET - centred(z) (ref/serialize.c). Decoding the z region
///      must reproduce the exported z.bin (N_PLUS_ELL polys, canonical [0,Q)). This
///      is OUR encoding (ML-DSA's z packing differs), so it is written here, not
///      reused, and folded into the verifier at Stage 5.
///  (c) norm: accept iff ‖z‖∞ <= gamma-kappa (=137935); base_verify's first gate.
///
/// forge treats a revert as failure; no forge-std dependency.
contract LASSampleZTest {
    Vm constant vm = Vm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);

    uint256 constant N = 256;
    uint256 constant Q = 8380417;
    uint256 constant KAPPA = 49;               // D3 challenge weight = ML-DSA-65 tau
    uint256 constant N_PLUS_ELL = 11;          // n + ell = 6 + 5
    uint256 constant CTILDE_BYTES = LASVerify.CTILDE_BYTES; // FIPS 204 lambda/4
    uint256 constant Z_BITS = 19;              // LAS_Z_COEFF_BITS (D3)
    uint256 constant Z_OFFSET = 137935;        // gamma - kappa, gamma = KAPPA*N*N_PLUS_ELL = 137984
    uint256 constant BOUND = 137935;           // accept iff ||z||inf <= gamma-kappa

    function _readPoly(string memory name) internal view returns (uint256[] memory a) {
        bytes memory raw = vm.readFileBinary(string.concat("test/vectors/", name));
        require(raw.length == N * 4, "bad poly file length");
        a = new uint256[](N);
        for (uint256 i = 0; i < N; i++) {
            uint256 off = i * 4;
            a[i] = uint256(uint8(raw[off]))
                | (uint256(uint8(raw[off + 1])) << 8)
                | (uint256(uint8(raw[off + 2])) << 16)
                | (uint256(uint8(raw[off + 3])) << 24);
        }
    }

    /// LSB-first bit reader (mirrors ref/serialize.c br_get).
    function _readBits(bytes memory buf, uint256 bitpos, uint256 nbits) internal pure returns (uint256 v) {
        for (uint256 i = 0; i < nbits; i++) {
            uint256 bp = bitpos + i;
            if ((uint8(buf[bp >> 3]) >> (bp & 7)) & 1 == 1) v |= (uint256(1) << i);
        }
    }

    /// Decode sig.bin's z region (after the c_tilde digest) into canonical [0,Q).
    function _decodeZ(bytes memory sig) internal pure returns (uint256[][] memory z) {
        z = new uint256[][](N_PLUS_ELL);
        uint256 bit = CTILDE_BYTES * 8; // z region starts right after c_tilde
        for (uint256 i = 0; i < N_PLUS_ELL; i++) {
            z[i] = new uint256[](N);
            for (uint256 k = 0; k < N; k++) {
                uint256 field = _readBits(sig, bit, Z_BITS);
                bit += Z_BITS;
                // z_centred = Z_OFFSET - field, in [-Z_OFFSET, Z_OFFSET] for in-band sigs;
                // represent as residue mod Q.
                int256 zc = int256(Z_OFFSET) - int256(field);
                z[i][k] = zc >= 0 ? uint256(zc) : uint256(int256(Q) + zc);
            }
        }
    }

    /// ||z||inf <= BOUND, using the centred absolute value of each residue.
    function _normOk(uint256[][] memory z) internal pure returns (bool) {
        for (uint256 i = 0; i < N_PLUS_ELL; i++) {
            for (uint256 k = 0; k < N; k++) {
                uint256 zc = z[i][k];
                uint256 absz = zc <= Q / 2 ? zc : Q - zc;
                if (absz > BOUND) return false;
            }
        }
        return true;
    }

    // (a) SampleInBall reuse == C challenge golden.
    function test_sampleInBall_matches_C_challenge() public view {
        bytes memory sig = vm.readFileBinary("test/vectors/sig.bin");
        bytes memory cTilde = new bytes(CTILDE_BYTES);
        for (uint256 i = 0; i < CTILDE_BYTES; i++) cTilde[i] = sig[i];

        uint256[] memory c = sampleInBallNist(cTilde, KAPPA, Q);
        uint256[] memory want = _readPoly("c.bin");
        require(c.length == N && want.length == N, "len");
        for (uint256 i = 0; i < N; i++) require(c[i] == want[i], "SampleInBall != C challenge");
    }

    // (b) BitPack19 z-decode == C-decoded z golden.
    function test_bitpack19_zdecode_matches_C() public view {
        bytes memory sig = vm.readFileBinary("test/vectors/sig.bin");
        uint256[][] memory z = _decodeZ(sig);

        bytes memory zraw = vm.readFileBinary("test/vectors/z.bin");
        require(zraw.length == N_PLUS_ELL * N * 4, "zbin len");
        for (uint256 i = 0; i < N_PLUS_ELL; i++) {
            for (uint256 k = 0; k < N; k++) {
                uint256 off = (i * N + k) * 4;
                uint256 want = uint256(uint8(zraw[off]))
                    | (uint256(uint8(zraw[off + 1])) << 8)
                    | (uint256(uint8(zraw[off + 2])) << 16)
                    | (uint256(uint8(zraw[off + 3])) << 24);
                require(z[i][k] == want, "z decode mismatch");
            }
        }
    }

    // (c) norm gate: golden z passes, an over-bound coefficient fails.
    function test_norm_accepts_golden_rejects_overbound() public view {
        bytes memory sig = vm.readFileBinary("test/vectors/sig.bin");
        uint256[][] memory z = _decodeZ(sig);
        require(_normOk(z), "golden z should pass norm");

        z[0][0] = BOUND + 1; // one coefficient just over gamma-kappa
        require(!_normOk(z), "over-bound z should fail norm");
    }
}

/// Minimal Foundry cheatcode interface (mirrors AdaptorSwap.t.sol; no forge-std).
interface Vm {
    function readFileBinary(string calldata path) external view returns (bytes memory);
}
