// SPDX-License-Identifier: MIT
pragma solidity ^0.8.25;

import {LASVerifyOpt} from "./LASVerifierOpt.sol";
import {LASRegister} from "./LASRegister.sol";

/// @title AdaptorSwapBound — the SETTLEMENT contract for a two-leg LAS atomic swap.
///
/// `AdaptorSwap` is a MEASUREMENT HARNESS. It deliberately offers several claim paths over
/// one escrow so their gas can be compared, including `claimLAS`, which pays out after a
/// length check and one keccak256 and performs NO verification — that is the on-chain FLOOR
/// it exists to price. The consequence is that in `AdaptorSwap` no escrow is ever gated on
/// verification: any OPEN escrow, however it was funded, can be drained through the floor
/// path. That is fine for measuring and fatal for settling.
///
/// This contract is the settling counterpart. One way in, one way out:
///
///   • `claimBound` — full on-chain LAS verification (`LASVerifyOpt`, the same predicate as
///     `ref/basesig.c base_verify`) over a message the CONTRACT DERIVES;
///   • `refund` — the payer reclaims after this leg's timeout.
///
/// There is no floor path, no unverified path and no scheme-agnostic path, so an escrow
/// funded here is claimable only by someone holding an adapted signature that verifies
/// against the registered key over this escrow's own message.
///
/// WHY THE DERIVED MESSAGE MATTERS. eprint 2020/845 Fig. 1 signs tx1 and tx2 — the
/// transactions that move the coins. `AdaptorSwap`'s verified paths sign an opaque blob
/// committed at fund time: valid, but naming neither the chain, nor the contract, nor the
/// escrow, nor the beneficiary, nor the amount. Two legs settled that way are evidence of
/// two valid LAS signatures, not of a swap. Here the message is `legMessage(id)`
/// (`LASRegister.claimMessage`), so a signature authorises exactly one escrow, on one
/// chain, paying one beneficiary one amount.
///
/// TWO-TIMEOUT REFUND RULE (2020/845 §4.1), unchanged from `AdaptorSwap`: the leg claimed
/// FIRST — the one whose settlement reveals the witness — must carry the SHORTER timeout,
/// so the reacting party still has a window to extract and claim the second leg. Each leg
/// is an independent escrow on its own chain, so `refund` can only enforce its own timeout;
/// the asymmetry is the funders' responsibility.
contract AdaptorSwapBound {
    enum State { EMPTY, OPEN, CLAIMED, REFUNDED }

    struct Swap {
        address payer;
        address payable beneficiary;
        uint256 amount;
        uint64 timeout; // unix time after which the payer may refund
        State state;
        bytes32 lasContext; // commitment to (A'_hat, t_hat, pack(t)) fixed at fund time
    }

    uint256 public nextId;
    mapping(uint256 => Swap) public swaps;

    event Funded(uint256 indexed id, address payer, address beneficiary, uint256 amount);
    event Claimed(uint256 indexed id, bytes32 sigTag);
    event Refunded(uint256 indexed id);

    /// Escrow funds claimable only by a verifying adapted LAS signature over
    /// `legMessage(id)`.
    ///
    /// `lasContext` MUST equal `LASRegister.contextBound(aHatPacked, tHatPacked, tPacked)`:
    /// the public parameters A' and the public key t, in both the NTT-domain transport form
    /// the verifier reads and the little-endian form that is hashed into the challenge
    /// preimage. Both are supplied by the claimer at claim time, so both must be pinned
    /// here — otherwise a claimer could hand in a key of their own and have their own
    /// signature verify.
    ///
    /// The message is NOT part of this commitment: it is derived from escrow state, and the
    /// id it depends on is assigned by this very call.
    function fundLASBound(address payable beneficiary, uint64 timeout, bytes32 lasContext)
        external
        payable
        returns (uint256 id)
    {
        require(msg.value > 0, "no value");
        require(lasContext != bytes32(0), "no context");
        id = nextId++;
        swaps[id] = Swap(msg.sender, beneficiary, msg.value, timeout, State.OPEN, lasContext);
        emit Funded(id, msg.sender, beneficiary, msg.value);
    }

    /// The digest escrow `id`'s adapted signature must be over, read from the very state
    /// `claimBound` checks against. This is the getter the off-chain parties call (via
    /// `cast call`) after funding and before pre-signing, so what they sign and what the
    /// contract recomputes are the same bytes by construction rather than by two
    /// implementations happening to agree.
    function legMessage(uint256 id) public view returns (bytes32) {
        Swap storage sw = swaps[id];
        return LASRegister.claimMessage(block.chainid, address(this), id, sw.payer, sw.beneficiary, sw.amount);
    }

    /// The same digest for an escrow that does not exist yet. `fundLASBound` takes `id` from
    /// `nextId`, so a party who knows the payment terms can compute what it will have to
    /// sign before broadcasting, then confirm against `legMessage(id)` afterwards.
    function legMessagePreview(uint256 id, address payer, address beneficiary, uint256 amount)
        external
        view
        returns (bytes32)
    {
        return LASRegister.claimMessage(block.chainid, address(this), id, payer, beneficiary, amount);
    }

    /// Settle with full on-chain LAS verification inside one transaction.
    ///
    /// `message` is still a parameter rather than a memory value because
    /// `LASVerifyOpt.verify` reads it from CALLDATA (`calldatacopy` into the SHAKE256
    /// preimage). It is supplied but never trusted: it must be exactly 32 bytes and equal
    /// the derived digest, so the claimer's copy is checked, not relied upon.
    function claimBound(
        uint256 id,
        bytes calldata sigPacked,
        bytes calldata aHatPacked,
        bytes calldata tHatPacked,
        bytes calldata tPacked,
        bytes calldata message
    ) external {
        Swap storage sw = swaps[id];
        require(sw.state == State.OPEN, "not open");
        require(keccak256(abi.encode(aHatPacked, tHatPacked, tPacked)) == sw.lasContext, "context mismatch");
        require(message.length == 32, "message not 32B");
        bytes32 supplied;
        assembly {
            supplied := calldataload(message.offset)
        }
        require(supplied == legMessage(id), "message not bound");
        require(LASVerifyOpt.verify(aHatPacked, tHatPacked, tPacked, message, sigPacked), "LAS verify failed");
        sw.state = State.CLAIMED;
        emit Claimed(id, keccak256(sigPacked));
        sw.beneficiary.transfer(sw.amount);
    }

    /// Payer reclaims after THIS leg's own timeout if no one claimed.
    function refund(uint256 id) external {
        Swap storage sw = swaps[id];
        require(sw.state == State.OPEN, "not open");
        require(block.timestamp >= sw.timeout, "before timeout");
        require(msg.sender == sw.payer, "not payer");
        sw.state = State.REFUNDED;
        emit Refunded(id);
        payable(sw.payer).transfer(sw.amount);
    }
}
