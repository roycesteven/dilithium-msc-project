// SPDX-License-Identifier: MIT
pragma solidity ^0.8.25;

import {nttFw, nttInv} from "../lib/zknox/ZKNOX_NTT_dilithium.sol";
import {vecMulMod} from "../lib/zknox/ZKNOX_dilithium_utils.sol";

/// @title Stage-3 validation of the VENDORED normal-domain NTT (ZKNox).
///
/// The on-chain LAS verifier computes w' = z_top + A'*z_bot - c*t, where each
/// A'[i][j]*z_bot[j] and c*t[i] is a negacyclic polynomial product in the ring
/// Z_q[X]/(X^256+1), q = 8380417. We reuse ZKNox's audited NTT for that product:
///
///     a (X) b  ==  nttInv( vecMulMod( nttFw(a), nttFw(b) ) )
///
/// This is a NORMAL-domain negacyclic NTT (plain mulmod, N^{-1} inverse scaling),
/// a different-but-EQUIVALENT convolution to ref/ntt.c's Montgomery-domain NTT.
/// Since base_verify canonicalises w' (mod q) BEFORE hashing it, any correct
/// convolution yields the identical w' and hence the identical challenge. This
/// test pins the convolution against a schoolbook golden computed in C
/// (ref/test/export_verify_vector.c: negacyclic_conv), so the reuse is proven
/// correct in isolation before the full verifier is assembled (Stage 5).
///
/// forge treats a revert as a failure, so no forge-std dependency is needed.
contract LASNttTest {
    Vm constant vm = Vm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);
    uint256 constant N = 256;

    /// Read a 256-coefficient int32-LE poly file (all values canonical [0,Q)).
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

    function _eq(uint256[] memory a, uint256[] memory b, string memory what) internal pure {
        require(a.length == b.length, what);
        for (uint256 i = 0; i < a.length; i++) require(a[i] == b[i], what);
    }

    /// nttInv(nttFw(a)) == a : the transform pair is a clean inverse (nttFw does
    /// not scale, nttInv scales by N^{-1} mod q).
    function test_ntt_roundtrip() public view {
        uint256[] memory a = _readPoly("conv_a.bin");
        uint256[] memory back = nttInv(nttFw(a)); // both mutate in place & return a
        _eq(back, _readPoly("conv_a.bin"), "nttInv(nttFw(a)) != a");
    }

    /// The authoritative check: ZKNox's NTT convolution equals the C schoolbook
    /// negacyclic convolution a (X) b mod (X^256+1, q).
    function test_ntt_negacyclic_conv_matches_C_golden() public view {
        uint256[] memory ah = nttFw(_readPoly("conv_a.bin")); // -> NTT(a)
        uint256[] memory bh = nttFw(_readPoly("conv_b.bin")); // -> NTT(b)
        uint256[] memory got = nttInv(vecMulMod(ah, bh));      // -> a (X) b
        _eq(got, _readPoly("conv_out.bin"), "ZKNox conv != C schoolbook golden");
    }
}

/// Minimal Foundry cheatcode interface (mirrors AdaptorSwap.t.sol; no forge-std).
interface Vm {
    function readFileBinary(string calldata path) external view returns (bytes memory);
}
