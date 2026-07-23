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

    /* ---- post-quantum leg: published LAS adapted signature (real 6720-byte blob) ---- */
    function test_LASSwap() public {
        bytes memory sig = vm.readFileBinary("test/las_sig.bin"); // exported by ref/test/export_packed (D3)
        require(sig.length == 6720, "expected 6720-byte packed LAS signature");

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

    /* ---- paper §4.1 two-timeout rule: t2 < t1 preserves the second claimant's window ----
     *
     * Two legs of one cross-chain swap (both settled classically here — the escrow is
     * scheme-agnostic). u1 holds the witness and redeems the FIRST leg (revealing y); u2
     * reacts on the SECOND leg. The first-claimed leg carries the shorter timeout t2, the
     * second the longer t1. We show that even after t2 has elapsed, u2's leg is still
     * claimable (not yet refundable), so u2 keeps a safety window until t1. */
    function test_TwoTimeoutSafetyWindow() public {
        uint256 u1key = 0xA11CE; address payable u1 = payable(vm.addr(u1key)); // witness holder, claims 1st
        uint256 u2key = 0xB0B;   address payable u2 = payable(vm.addr(u2key)); // reacts, claims 2nd

        uint64 t2 = uint64(block.timestamp + 100); // first-claimed leg (u2 funds coin for u1)
        uint64 t1 = uint64(block.timestamp + 200); // second-claimed leg (u1 funds coin for u2)
        require(t2 < t1, "two-timeout invariant t2 < t1 violated");

        bytes32 claimFirst  = keccak256("chain2: c2 -> u1"); // u2's coin, redeemed first by u1
        bytes32 claimSecond = keccak256("chain1: c1 -> u2"); // u1's coin, redeemed second by u2

        // Both legs escrowed. funderSigner is the payer's key (whose adapted sig unlocks it).
        uint256 legFirst  = swap.fundClassical{value: 1 ether}(u1, vm.addr(u2key), claimFirst,  t2);
        uint256 legSecond = swap.fundClassical{value: 1 ether}(u2, vm.addr(u1key), claimSecond, t1);

        // u1 redeems the first leg at the last moment before its timeout (this reveals y).
        vm.warp(uint256(t2) - 1);
        (uint8 v1, bytes32 r1, bytes32 s1) = vm.sign(u2key, claimFirst);
        uint256 u1Before = u1.balance;
        swap.claimClassical(legFirst, v1, r1, s1);
        require(u1.balance == u1Before + 1 ether, "u1 did not receive c2");

        // The first leg's timeout has now elapsed — but because t1 > t2, the SECOND leg is
        // still NOT refundable, so u2 retains a window to react.
        vm.warp(uint256(t2));
        (bool tooEarly,) = address(swap).call(abi.encodeWithSelector(swap.refund.selector, legSecond));
        require(!tooEarly, "second leg wrongly refundable before t1 (safety window lost)");

        // u2 extracts y off-chain and claims the second leg within the window.
        (uint8 v2, bytes32 r2, bytes32 s2) = vm.sign(u1key, claimSecond);
        uint256 u2Before = u2.balance;
        swap.claimClassical(legSecond, v2, r2, s2);
        require(u2.balance == u2Before + 1 ether, "u2 lost the safety window");
    }
}
