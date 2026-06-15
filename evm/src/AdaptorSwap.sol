// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

/// @title AdaptorSwap — a signature-scheme-agnostic HTLC escrow for adaptor-signature
///        atomic swaps, used to measure the *on-chain* cost of settling a swap with a
///        CLASSICAL (ECDSA) adapted signature vs a POST-QUANTUM (LAS) adapted signature.
///
/// An adaptor swap settles by publishing the *adapted* signature on-chain, where it is
/// verified as an ORDINARY signature (the adaptor magic is off-chain). This contract
/// models exactly that final settlement step, with two claim entrypoints differing
/// only in the on-chain verification of the published signature:
///
///   • claimClassical — the adapted ECDSA signature is verified with the native
///     secp256k1 precompile `ecrecover`. This is cheap and is what a real EVM
///     atomic-swap (e.g. an ECDSA-adaptor DLC) settles with.
///
///   • claimLAS — the adapted LAS signature is a 4672-byte packed lattice signature.
///     Native lattice verification (NTT + SHAKE256 over the packed signature) is
///     INFEASIBLE in the EVM (it would exceed the block gas limit; cf. poqeth, which
///     needed dedicated machinery even for *basic* PQ verification). This entrypoint
///     therefore measures the unavoidable on-chain FLOOR — paying calldata gas for the
///     4672-byte signature plus one keccak256 pass over it — which is a strict LOWER
///     BOUND on the true cost. It is deliberately NOT a real verification; see the
///     report's evaluation section.
///
/// fund*/refund are identical for both schemes, so a gas report attributes any
/// difference to the verification step alone.
contract AdaptorSwap {
    enum State { EMPTY, OPEN, CLAIMED, REFUNDED }

    struct Swap {
        address       payer;
        address payable beneficiary;
        uint256       amount;
        address       funderSigner; // classical: signer recovered from the adapted ECDSA sig
        bytes32       claimHash;     // the pre-authorised claim "transaction" digest
        uint64        timeout;       // unix time after which the payer may refund
        State         state;
    }

    uint256 public nextId;
    mapping(uint256 => Swap) public swaps;

    event Funded(uint256 indexed id, address payer, address beneficiary, uint256 amount);
    event Claimed(uint256 indexed id, bytes32 sigTag);
    event Refunded(uint256 indexed id);

    /// Escrow funds whose claim is gated on an ADAPTED ECDSA signature over `claimHash`
    /// by `funderSigner` (the classical-adaptor settlement object).
    function fundClassical(
        address payable beneficiary,
        address funderSigner,
        bytes32 claimHash,
        uint64  timeout
    ) external payable returns (uint256 id) {
        require(msg.value > 0, "no value");
        id = nextId++;
        swaps[id] = Swap(msg.sender, beneficiary, msg.value, funderSigner, claimHash, timeout, State.OPEN);
        emit Funded(id, msg.sender, beneficiary, msg.value);
    }

    /// Escrow funds whose claim is gated on publishing the ADAPTED LAS signature bytes.
    function fundLAS(
        address payable beneficiary,
        uint64  timeout
    ) external payable returns (uint256 id) {
        require(msg.value > 0, "no value");
        id = nextId++;
        swaps[id] = Swap(msg.sender, beneficiary, msg.value, address(0), bytes32(0), timeout, State.OPEN);
        emit Funded(id, msg.sender, beneficiary, msg.value);
    }

    /// Settle with a classical adapted ECDSA signature (native precompile verification).
    function claimClassical(uint256 id, uint8 v, bytes32 r, bytes32 s) external {
        Swap storage sw = swaps[id];
        require(sw.state == State.OPEN, "not open");
        address rec = ecrecover(sw.claimHash, v, r, s);
        require(rec != address(0) && rec == sw.funderSigner, "bad ECDSA sig");
        sw.state = State.CLAIMED;
        emit Claimed(id, bytes32(uint256(uint160(rec))));
        sw.beneficiary.transfer(sw.amount);
    }

    /// Settle with a published LAS adapted signature. Charges the on-chain FLOOR:
    /// 4672 bytes of calldata + one keccak256 over them. NOT a lattice verification.
    function claimLAS(uint256 id, bytes calldata sigPacked) external {
        Swap storage sw = swaps[id];
        require(sw.state == State.OPEN, "not open");
        require(sigPacked.length == 4672, "bad LAS sig length"); // LAS_SIG_BYTES
        bytes32 tag = keccak256(sigPacked); // minimal "touch every byte"; real verify is off-EVM
        sw.state = State.CLAIMED;
        emit Claimed(id, tag);
        sw.beneficiary.transfer(sw.amount);
    }

    /// Payer reclaims the escrow after the timeout if no one claimed.
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
