// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

import {AdaptorSwap} from "../src/AdaptorSwap.sol";

/// Minimal Foundry cheatcode interface (avoids a forge-std dependency / network install).
interface Vm {
    function addr(uint256 privateKey) external pure returns (address);
    function sign(uint256 privateKey, bytes32 digest) external pure returns (uint8, bytes32, bytes32);
    function warp(uint256 newTimestamp) external;
    function readFileBinary(string calldata path) external view returns (bytes memory);
    function deal(address who, uint256 newBalance) external;
}

/// End-to-end on-chain gas benchmark of an adaptor-signature atomic swap, settling
/// once with a CLASSICAL adapted ECDSA signature and once with a POST-QUANTUM LAS
/// adapted signature. Run with:  forge test --gas-report
///
/// The escrow (fund*/refund) is identical for both; only the claim-time signature
/// verification differs, so `--gas-report` isolates the price of post-quantum on-chain.
contract AdaptorSwapTest {
    Vm constant vm = Vm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);
    AdaptorSwap swap;

    address payable constant BENEFICIARY = payable(address(0xB0B));
    uint64 timeout;

    function setUp() public {
        swap = new AdaptorSwap();
        vm.deal(address(this), 100 ether);
        timeout = uint64(block.timestamp + 1000);
    }

    receive() external payable {}

    /* ---- classical leg: adapted ECDSA signature, native ecrecover verification ---- */
    function test_ClassicalSwap() public {
        uint256 funderKey = 0xA11CE;                 // the funder's secp256k1 key
        address funder = vm.addr(funderKey);
        bytes32 claimHash = keccak256("chainA: pay 1 to Bob"); // the claim "tx" digest

        uint256 id = swap.fundClassical{value: 1 ether}(BENEFICIARY, funder, claimHash, timeout);

        // The adapted signature, in an adaptor swap, is an ordinary ECDSA signature.
        (uint8 v, bytes32 r, bytes32 s) = vm.sign(funderKey, claimHash);

        uint256 b0 = BENEFICIARY.balance;
        swap.claimClassical(id, v, r, s);
        require(BENEFICIARY.balance == b0 + 1 ether, "classical claim did not pay");
    }

    /* ---- post-quantum leg: published LAS adapted signature (real 4672-byte blob) ---- */
    function test_LASSwap() public {
        bytes memory sig = vm.readFileBinary("test/las_sig.bin"); // exported by ref/test/export_packed
        require(sig.length == 4672, "expected 4672-byte packed LAS signature");

        uint256 id = swap.fundLAS{value: 1 ether}(BENEFICIARY, timeout);

        uint256 b0 = BENEFICIARY.balance;
        swap.claimLAS(id, sig);
        require(BENEFICIARY.balance == b0 + 1 ether, "LAS claim did not pay");
    }

    /* ---- timeout/refund path (scheme-independent) ---- */
    function test_Refund() public {
        uint256 id = swap.fundClassical{value: 1 ether}(BENEFICIARY, address(0xDEAD), keccak256("x"), timeout);
        uint256 b0 = address(this).balance;
        vm.warp(uint256(timeout) + 1);
        swap.refund(id);
        require(address(this).balance == b0 + 1 ether, "refund did not return escrow");
    }
}
