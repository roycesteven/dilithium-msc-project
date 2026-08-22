// SPDX-License-Identifier: MIT
pragma solidity ^0.8.25;

import {AdaptorSwapBound} from "../src/AdaptorSwapBound.sol";
import {LASRegister} from "../src/LASRegister.sol";

/// @title Transaction binding for the bound settlement path.
///
/// WHAT THIS FILE IS FOR. Two settled legs prove two valid LAS signatures. They prove a
/// *swap* only if each signature authorises the leg it settled and nothing else. These are
/// the controls for that: a signature usable on another escrow, another beneficiary,
/// another amount, another contract or another chain would make the two-leg result
/// meaningless, so each of those is constructed here and required to revert.
///
/// WHY EVERY EXPECTATION NAMES A REVERT REASON. All of these revert either way — a wrong
/// message also fails verification. A test that merely asserted "it reverted" would pass
/// for the wrong reason and would keep passing if the binding check were deleted. So each
/// negative asserts `message not bound` specifically, and the positive control asserts the
/// call got PAST the binding check and died at `LAS verify failed` instead.
///
/// WHY THERE IS NO END-TO-END PAYOUT HERE. The message is derived from escrow state, so a
/// signature that verifies against it can only be produced after the escrow exists — the
/// golden `sig.bin` is over `msg.bin`, not over any `legMessage`. Generating one is the
/// runner's job (`scripts/run_onchain_two_leg.sh`), where the C signer is handed the digest
/// read back from the chain. Binding is decided here; settlement is decided there.
contract AdaptorSwapBoundTest {
    Vm constant vm = Vm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);

    uint256 constant AHAT_BYTES = 30720;
    uint256 constant THAT_BYTES = 6144;
    uint256 constant TPACK_BYTES = 6144;
    uint256 constant SIG_BYTES = 6736;

    AdaptorSwapBound swapc;
    address payable constant BENEFICIARY = payable(address(0xB0B));
    address payable constant OTHER_BENEFICIARY = payable(address(0xCAFE));

    bytes aHat;
    bytes tHat;
    bytes tPack;
    bytes sig;
    bytes32 ctx;

    function setUp() public {
        swapc = new AdaptorSwapBound();
        vm.deal(address(this), 100 ether);

        // Shapes only. These controls never reach the verifier's arithmetic, so the content
        // is irrelevant — what must be real is the LENGTH, because `LASVerifyOpt.verify`
        // length-checks before anything else and a short buffer would make the positive
        // control fail at the wrong place.
        aHat = new bytes(AHAT_BYTES);
        tHat = new bytes(THAT_BYTES);
        tPack = new bytes(TPACK_BYTES);
        sig = new bytes(SIG_BYTES);
        ctx = LASRegister.contextBound(aHat, tHat, tPack);
    }

    function _fund(address payable beneficiary, uint256 amount) internal returns (uint256 id) {
        id = swapc.fundLASBound{value: amount}(beneficiary, uint64(block.timestamp + 1 days), ctx);
    }

    /// Calls `claimBound` and requires it to revert with exactly `reason`.
    function _claimExpectRevert(uint256 id, bytes32 message, string memory reason) internal {
        (bool ok, bytes memory ret) = address(swapc).call(
            abi.encodeWithSelector(
                AdaptorSwapBound.claimBound.selector, id, sig, aHat, tHat, tPack, abi.encodePacked(message)
            )
        );
        require(!ok, "claim must revert");
        require(_reasonIs(ret, reason), string.concat("wrong revert reason, expected: ", reason));
    }

    /// Decodes `Error(string)` and compares. A revert with no reason, or a different
    /// reason, fails — that is the whole point of checking it.
    function _reasonIs(bytes memory ret, string memory expected) internal pure returns (bool) {
        if (ret.length < 68) return false;
        bytes memory sliced = new bytes(ret.length - 4);
        for (uint256 i = 0; i < sliced.length; i++) {
            sliced[i] = ret[i + 4];
        }
        string memory got = abi.decode(sliced, (string));
        return keccak256(bytes(got)) == keccak256(bytes(expected));
    }

    function _stateOf(uint256 id) internal view returns (AdaptorSwapBound.State st) {
        (,,,, st,) = swapc.swaps(id);
    }

    // ---------------------------------------------------------------- positive controls

    /// The getter agrees with the library derivation over the escrow's own terms. If these
    /// two ever drift, every negative below would still pass while the parties sign
    /// something the contract does not check.
    function test_legMessage_matches_derivation() public {
        uint256 id = _fund(BENEFICIARY, 1 ether);
        bytes32 expected =
            LASRegister.claimMessage(block.chainid, address(swapc), id, address(this), BENEFICIARY, 1 ether);
        require(swapc.legMessage(id) == expected, "legMessage disagrees with claimMessage");
    }

    /// The pre-funding preview equals what the escrow reports afterwards, so a party can
    /// commit to signing before broadcasting the fund call.
    function test_preview_matches_legMessage_after_funding() public {
        uint256 nextId = swapc.nextId();
        bytes32 preview = swapc.legMessagePreview(nextId, address(this), BENEFICIARY, 1 ether);
        uint256 id = _fund(BENEFICIARY, 1 ether);
        require(id == nextId, "id not the previewed one");
        require(swapc.legMessage(id) == preview, "preview disagrees with legMessage");
    }

    /// THE POSITIVE CONTROL FOR BINDING. With the correctly derived message the call clears
    /// the binding gate and fails at verification instead. If this ever reports
    /// `message not bound`, the derivation and the getter have diverged and every negative
    /// below is passing vacuously.
    function test_correct_message_reaches_verification() public {
        uint256 id = _fund(BENEFICIARY, 1 ether);
        _claimExpectRevert(id, swapc.legMessage(id), "LAS verify failed");
    }

    // ---------------------------------------------------------------- negative controls

    /// Another escrow's message. This is the cross-leg replay: in the two-leg swap, leg B's
    /// signature presented to leg A.
    function test_rejects_message_of_another_escrow() public {
        uint256 idA = _fund(BENEFICIARY, 1 ether);
        uint256 idB = _fund(BENEFICIARY, 1 ether);
        _claimExpectRevert(idA, swapc.legMessage(idB), "message not bound");
    }

    /// A message naming a different beneficiary — a signature that authorised paying
    /// someone else.
    function test_rejects_changed_beneficiary() public {
        uint256 id = _fund(BENEFICIARY, 1 ether);
        bytes32 wrong = LASRegister.claimMessage(
            block.chainid, address(swapc), id, address(this), OTHER_BENEFICIARY, 1 ether
        );
        _claimExpectRevert(id, wrong, "message not bound");
    }

    /// A message naming a different amount.
    function test_rejects_changed_amount() public {
        uint256 id = _fund(BENEFICIARY, 1 ether);
        bytes32 wrong =
            LASRegister.claimMessage(block.chainid, address(swapc), id, address(this), BENEFICIARY, 2 ether);
        _claimExpectRevert(id, wrong, "message not bound");
    }

    /// A message naming a different payer.
    function test_rejects_changed_payer() public {
        uint256 id = _fund(BENEFICIARY, 1 ether);
        bytes32 wrong =
            LASRegister.claimMessage(block.chainid, address(swapc), id, address(0xDEAD), BENEFICIARY, 1 ether);
        _claimExpectRevert(id, wrong, "message not bound");
    }

    /// CROSS-CHAIN REPLAY. A message derived for another chain id must not settle here.
    /// The live counterpart runs across two real nodes in `scripts/run_onchain_two_leg.sh`;
    /// this is the cheap deterministic version of the same control.
    function test_rejects_other_chain_id() public {
        uint256 id = _fund(BENEFICIARY, 1 ether);
        bytes32 wrong =
            LASRegister.claimMessage(block.chainid + 1, address(swapc), id, address(this), BENEFICIARY, 1 ether);
        _claimExpectRevert(id, wrong, "message not bound");
    }

    /// CROSS-CONTRACT REPLAY. Same escrow terms, different escrow contract — the second leg
    /// of a swap is a different deployment, so this is what stops one leg's signature being
    /// meaningful at the other's address.
    function test_rejects_other_contract_address() public {
        uint256 id = _fund(BENEFICIARY, 1 ether);
        AdaptorSwapBound other = new AdaptorSwapBound();
        bytes32 wrong =
            LASRegister.claimMessage(block.chainid, address(other), id, address(this), BENEFICIARY, 1 ether);
        _claimExpectRevert(id, wrong, "message not bound");
    }

    /// A message of the wrong length is rejected before the comparison, so a caller cannot
    /// smuggle a short or long preimage into the SHAKE256 input.
    function test_rejects_message_wrong_length() public {
        uint256 id = _fund(BENEFICIARY, 1 ether);
        (bool ok, bytes memory ret) = address(swapc).call(
            abi.encodeWithSelector(
                AdaptorSwapBound.claimBound.selector, id, sig, aHat, tHat, tPack, abi.encodePacked(uint128(0))
            )
        );
        require(!ok, "short message must revert");
        require(_reasonIs(ret, "message not 32B"), "wrong revert reason for short message");
    }

    /// Substituted verification parameters are caught by the fund-time commitment, before
    /// the message is even looked at.
    function test_rejects_substituted_context() public {
        uint256 id = _fund(BENEFICIARY, 1 ether);
        bytes memory tHatWrong = new bytes(THAT_BYTES);
        tHatWrong[0] = 0x01;
        (bool ok, bytes memory ret) = address(swapc).call(
            abi.encodeWithSelector(
                AdaptorSwapBound.claimBound.selector,
                id,
                sig,
                aHat,
                tHatWrong,
                tPack,
                abi.encodePacked(swapc.legMessage(id))
            )
        );
        require(!ok, "substituted context must revert");
        require(_reasonIs(ret, "context mismatch"), "wrong revert reason for substituted context");
    }

    // ------------------------------------------------- the reason this contract exists

    /// THE STRUCTURAL GUARANTEE, TESTED BY BEHAVIOUR RATHER THAN BY ABI.
    ///
    /// `AdaptorSwap` exposes `claimLAS`, which pays out after a length check and one
    /// keccak256 and performs no verification, so any escrow there can be drained without a
    /// valid signature — acceptable in a gas harness, fatal in a settlement contract. This
    /// asserts the bound contract has no such door.
    ///
    /// Each attempt sends COMPLETE, well-formed calldata for the path in question against a
    /// live OPEN escrow. That matters: a bare 4-byte selector would revert on argument
    /// decoding whether or not the function existed, so such a test would pass vacuously
    /// and keep passing if an unverified path were added. Here, if any of these paths
    /// existed, the call would succeed and pay the beneficiary — so the assertion is that
    /// nothing executed AND no funds moved AND the escrow is still open.
    function test_unverified_claim_paths_cannot_move_funds() public {
        uint256 id = _fund(BENEFICIARY, 1 ether);
        uint256 balanceBefore = BENEFICIARY.balance;
        bytes32 msg32 = swapc.legMessage(id);

        (bool ok1,) = address(swapc).call(abi.encodeWithSignature("claimLAS(uint256,bytes)", id, sig));
        require(!ok1, "claimLAS executed on the bound contract");

        (bool ok2,) = address(swapc).call(
            abi.encodeWithSignature(
                "claimClassical(uint256,uint8,bytes32,bytes32)", id, uint8(27), bytes32(0), bytes32(0)
            )
        );
        require(!ok2, "claimClassical executed on the bound contract");

        (bool ok3,) = address(swapc).call(
            abi.encodeWithSignature(
                "claimLASVerifiedOpt(uint256,bytes,bytes,bytes,bytes,bytes)",
                id,
                sig,
                aHat,
                tHat,
                tPack,
                abi.encodePacked(msg32)
            )
        );
        require(!ok3, "claimLASVerifiedOpt executed on the bound contract");

        require(BENEFICIARY.balance == balanceBefore, "funds moved without verification");
        require(_stateOf(id) == AdaptorSwapBound.State.OPEN, "escrow no longer open");
    }

    /// The same, for the one path that IS meant to exist: it must still be gated. A claim
    /// with a syntactically perfect but unverifiable signature moves nothing.
    function test_bound_claim_with_invalid_signature_moves_nothing() public {
        uint256 id = _fund(BENEFICIARY, 1 ether);
        uint256 balanceBefore = BENEFICIARY.balance;
        _claimExpectRevert(id, swapc.legMessage(id), "LAS verify failed");
        require(BENEFICIARY.balance == balanceBefore, "funds moved on a failed verification");
        require(_stateOf(id) == AdaptorSwapBound.State.OPEN, "escrow no longer open");
    }
}

/// Minimal Foundry cheatcode interface (mirrors AdaptorSwapVerified.t.sol; no forge-std).
interface Vm {
    function deal(address who, uint256 newBalance) external;
}
