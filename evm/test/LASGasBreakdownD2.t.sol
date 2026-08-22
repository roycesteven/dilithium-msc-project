// SPDX-License-Identifier: MIT
pragma solidity ^0.8.25;

import {LASVerifyOptD2} from "../src/LASVerifierOptD2.sol";
import {LASTxGas} from "../src/LASRegister.sol";
import {nttFw} from "../lib/zknox/ZKNOX_NTT_dilithium.sol";
import {console} from "./TwoLegSwapGas.t.sol";

/// @title LASGasBreakdownD2 — what one on-chain LAS verification costs at Simplified
///        Dilithium-II, measured on the golden instance.
///
/// ⚠ WHAT THIS ESTABLISHES, AND WHAT IT DOES NOT. It measures ONE instance: the golden
/// `msg.bin` / `sig.bin` exported by `ref/test/export_verify_vector2`. `SampleInBall` (a
/// rejection loop) and `_decodeZ` (branches on coefficient value) are DATA-DEPENDENT, so a
/// single measurement is not an upper bound over all D2 inputs. The supported claim is
/// "the measured D2 golden instance fits in one transaction", NEVER "D2 fits" universally.
/// The D3 result carries the same instance caveat, for the same reason.
///
/// What the headroom is good for: if it greatly exceeds the combined cost of the two
/// data-dependent stages, the margin is robust to instance-to-instance variation. That is an
/// observation about how much room there is, not a proof over the input space — no
/// distribution of the rejection loop is established here.
///
/// WHY D2 IS MEASURED RATHER THAN DERIVED. D5 needed a LOWER bound (to show the cap is
/// exceeded), and a lower bound tolerates dropping unknown stages. D2 needs an UPPER bound,
/// which may drop nothing — including the two data-dependent stages, whose worst case is not
/// derivable. Arithmetic could only reduce D2 to "fits unless execution exceeds D3's by more
/// than 630,665 gas"; closing that needed a measurement.
///
/// WHY A SEPARATE VERIFIER CONTRACT. `LASVerifierOptD2.sol` is a copy of `LASVerifierOpt.sol`
/// with the parameter set re-instantiated, NOT a parameterisation of it. `LASVerifierOpt` is
/// pinned to C ground truth and to the measured D3 receipt; a mode flag or virtual dispatch
/// would move the very baselines those measurements record. Same reasoning as
/// `AdaptorSwap` / `AdaptorSwapBound`.
///
/// WHAT CHANGED FROM D3. Constants n 6→4, ell 5→4, kappa 49→39, c_tilde 48→32 B, and — because
/// gamma = kappa·d·(n+ell) shrinks — `Z_BITS` 19→18, which rewrites the assembly unpacker's
/// field width, mask (0x7FFFF→0x3FFFF), stride and bound literals. The group-of-8
/// byte-alignment trick still holds: 8 × 18 = 144 bits = 18 whole bytes. The 32-byte
/// word-reversal masks are width-independent and unchanged.
///
/// GAS ACCOUNTING is EIP-7623 via `LASTxGas`, identical to the D3 test, so the rows compare.
///
/// ⚠ Run WITHOUT `--gas-report`: the inspector is metered inside the measured frame.
contract LASGasBreakdownD2Test {
    Vm constant vm = Vm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);
    uint256 constant N = 256;
    uint256 constant TX_GAS_CAP = 16_777_216;

    // D2-aligned: n = 4, ell = 4 -> A' is 16 polynomials, t is 4.
    uint256 constant A_POLYS = 16;
    uint256 constant T_POLYS = 4;

    D2VerifyHarness h;

    function setUp() public {
        h = new D2VerifyHarness();
    }

    function _readPolys(string memory name, uint256 count) internal view returns (uint256[][] memory polys) {
        bytes memory raw = vm.readFileBinary(string.concat("test/vectors/d2/", name));
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

    function _toNtt(uint256[][] memory polys) internal pure returns (uint256[][] memory out) {
        out = new uint256[][](polys.length);
        for (uint256 i = 0; i < polys.length; i++) {
            uint256[] memory p = new uint256[](N);
            for (uint256 k = 0; k < N; k++) {
                p[k] = polys[i][k];
            }
            out[i] = nttFw(p);
        }
    }

    function _measure(address target, bytes memory cd) internal view returns (uint256 execGas, bool verdict) {
        uint256 before = gasleft();
        (bool okCall, bytes memory ret) = target.staticcall(cd);
        execGas = before - gasleft();
        require(okCall, "verifier call reverted");
        verdict = abi.decode(ret, (bool));
    }

    function _buildCalldata() internal view returns (bytes memory cd) {
        bytes memory message = vm.readFileBinary("test/vectors/d2/msg.bin");
        bytes memory sig = vm.readFileBinary("test/vectors/d2/sig.bin");
        require(sig.length == 4640, "D2 signature is not 4640 bytes");
        bytes memory aHatP = _packBE(_toNtt(_readPolys("pp_normal.bin", A_POLYS)));
        bytes memory tHatP = _packBE(_toNtt(_readPolys("t.bin", T_POLYS)));
        bytes memory tPacked = vm.readFileBinary("test/vectors/d2/t.bin");
        cd = abi.encodeWithSelector(D2VerifyHarness.run.selector, aHatP, tHatP, tPacked, message, sig);
    }

    /// Reports what a node would charge for the golden instance. The ACCEPT assertion is
    /// load-bearing: timing a path that rejects would measure the early exit, not verification.
    function test_d2_golden_instance_total() public view {
        bytes memory cd = _buildCalldata();
        (uint256 exec, bool ok) = _measure(address(h), cd);
        require(ok, "D2 verifier rejected the golden signature");

        (uint256 total, bool floorBinds) = LASTxGas.total(cd, exec);

        console.log("EIP-7825 per-transaction gas cap", TX_GAS_CAP);
        console.log("D2 golden instance: calldata bytes / tokens", cd.length, LASTxGas.tokens(cd));
        console.log("D2 golden instance: exec / EIP-7623 TOTAL", exec, total);
        console.log("D2 golden instance: EIP-7623 floor binds (1=yes)", floorBinds ? 1 : 0);
        console.log("D2 golden instance: TOTAL as percent of the cap", (total * 100) / TX_GAS_CAP);
        if (total < TX_GAS_CAP) {
            console.log("D2 golden instance: headroom under the cap", TX_GAS_CAP - total);
        } else {
            console.log("D2 golden instance: OVER the cap by", total - TX_GAS_CAP);
        }
    }

    /// The gate. Scoped to the golden instance by construction — it is the only instance
    /// available. Fails loudly if that instance ever stops fitting, so a regression cannot
    /// quietly produce a publishable-looking number.
    function test_d2_golden_instance_fits_in_one_transaction() public view {
        bytes memory cd = _buildCalldata();
        (uint256 exec, bool ok) = _measure(address(h), cd);
        require(ok, "D2 verifier rejected the golden signature");
        (uint256 total,) = LASTxGas.total(cd, exec);
        require(total < TX_GAS_CAP, "measured D2 golden instance does NOT fit in one transaction");
        console.log("D2 golden instance one-transaction TOTAL (EIP-7623)", total);
    }
}

contract D2VerifyHarness {
    function run(
        bytes calldata aHatPacked,
        bytes calldata tHatPacked,
        bytes calldata tPacked,
        bytes calldata message,
        bytes calldata sig
    ) external pure returns (bool) {
        return LASVerifyOptD2.verify(aHatPacked, tHatPacked, tPacked, message, sig);
    }
}

interface Vm {
    function readFileBinary(string calldata path) external view returns (bytes memory);
}
