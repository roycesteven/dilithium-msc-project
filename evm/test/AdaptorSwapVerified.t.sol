// SPDX-License-Identifier: MIT
pragma solidity ^0.8.25;

import {AdaptorSwap} from "../src/AdaptorSwap.sol";
import {LASVerify} from "../src/LASVerifier.sol";

/// @title Stage-6: the atomic swap settled with FULL on-chain LAS verification.
///
/// Exercises fundLASVerified + claimLASVerified end-to-end with the exported golden
/// vectors: the funder commits keccak256(abi.encode(A', t, M)) at fund time; the
/// claimer supplies A' (NTT domain), t, M and the adapted signature; the contract
/// re-derives the commitment, runs LASVerify.verify (the numerically-complete
/// base_verify), and pays out only on success. `forge test --gas-report` prices the
/// full verified settlement (tens of millions of gas — over the 30M block limit).
contract AdaptorSwapVerifiedTest {
    Vm constant vm = Vm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);
    uint256 constant N = 256;

    AdaptorSwap swap;
    address payable constant BENEFICIARY = payable(address(0xB0B));

    function setUp() public {
        swap = new AdaptorSwap();
        vm.deal(address(this), 100 ether);
    }

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

    function _inputs()
        internal
        view
        returns (uint256[][] memory AprimeHat, uint256[][] memory t, bytes memory message, bytes memory sig)
    {
        AprimeHat = LASVerify.toNttDomain(_readPolys("pp_normal.bin", 6 * 5));
        t = _readPolys("t.bin", 6);
        message = vm.readFileBinary("test/vectors/msg.bin");
        sig = vm.readFileBinary("test/vectors/sig.bin");
    }

    /// Full verified settlement: fund with the context commitment, claim with the
    /// golden signature+context, beneficiary is paid.
    function test_LASVerifiedSwap_settles() public {
        (uint256[][] memory AprimeHat, uint256[][] memory t, bytes memory message, bytes memory sig) = _inputs();
        bytes32 ctx = keccak256(abi.encode(AprimeHat, t, message));

        uint64 timeout = uint64(block.timestamp + 1 days);
        uint256 id = swap.fundLASVerified{value: 1 ether}(BENEFICIARY, timeout, ctx);

        uint256 before = BENEFICIARY.balance;
        swap.claimLASVerified(id, sig, AprimeHat, t, message);
        require(BENEFICIARY.balance == before + 1 ether, "beneficiary not paid");
    }

    /// A claimer that substitutes a different t (so the commitment no longer matches)
    /// must be rejected before/at verification — no payout.
    function test_LASVerifiedSwap_rejects_wrong_context() public {
        (uint256[][] memory AprimeHat, uint256[][] memory t, bytes memory message, bytes memory sig) = _inputs();
        bytes32 ctx = keccak256(abi.encode(AprimeHat, t, message));

        uint64 timeout = uint64(block.timestamp + 1 days);
        uint256 id = swap.fundLASVerified{value: 1 ether}(BENEFICIARY, timeout, ctx);

        t[0][0] ^= 1; // substituted public key -> commitment mismatch
        (bool ok,) = address(swap).call(
            abi.encodeWithSelector(swap.claimLASVerified.selector, id, sig, AprimeHat, t, message)
        );
        require(!ok, "wrong-context claim must revert");
        require(BENEFICIARY.balance == 0, "beneficiary wrongly paid");
    }
}

/// Minimal Foundry cheatcode interface (mirrors AdaptorSwap.t.sol; no forge-std).
interface Vm {
    function readFileBinary(string calldata path) external view returns (bytes memory);
    function deal(address who, uint256 newBalance) external;
}
