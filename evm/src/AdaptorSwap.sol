// SPDX-License-Identifier: MIT
pragma solidity ^0.8.25;

import {LASVerify} from "./LASVerifier.sol";
import {LASVerifyOpt} from "./LASVerifierOpt.sol";

/// @title AdaptorSwap — a signature-scheme-agnostic SCRIPTLESS escrow for adaptor-
///        signature atomic swaps, used to measure the *on-chain* cost of settling a swap
///        with a CLASSICAL (ECDSA) adapted signature vs a POST-QUANTUM (LAS) adapted
///        signature.
///
/// This is the SCRIPTLESS model, NOT a Hash-Time-Locked Contract: there is no hash-
/// preimage check, and the chain never sees or checks the adaptor statement Y. On-chain,
/// the contract only verifies the published *adapted* signature as an ORDINARY signature;
/// the adaptor mechanism (Y and witness extraction) is entirely off-chain. Atomicity
/// comes from that publication — a counterparty holding the pre-signature extracts the
/// witness y off-chain once the adapted signature is on-chain — while the timeout is only
/// a timelock for refund. The LAS paper (2020/845 §4.1) distinguishes classical HTLCs
/// from this scriptless, adaptor-signature version. This contract models exactly that
/// final settlement step, with claim entrypoints differing only in the on-chain
/// verification of the published signature:
///
///   • claimClassical — the adapted ECDSA signature is verified with the native
///     secp256k1 precompile `ecrecover`. This is cheap and is what a real EVM
///     atomic-swap (e.g. an ECDSA-adaptor DLC) settles with.
///
///   • claimLAS — the adapted LAS signature is a LASVerify.SIG_BYTES packed lattice signature
///     (D3 set: n=6, ell=5; wire = c_tilde || BitPack(z)). Native lattice verification
///     (NTT + SHAKE256 over the packed signature) is NOT performed on-chain here. Its
///     exact arithmetic op-budget — 12 forward + 12 inverse NTTs and 36 pointwise
///     products, counted from base_verify at n=6,ell=5 (ref/basesig.c) — is priced
///     separately by the LASVerifyCost cost probe, whose measured EVM gas total for the
///     current parameter set is reported in docs/03-results/GAS_LIMIT_INVESTIGATION.md.
///     Cf. poqeth, which needed dedicated machinery even for *basic* PQ verification.
///     This entrypoint therefore measures the unavoidable on-chain FLOOR — paying
///     calldata gas for the packed signature plus one keccak256 pass over it — a
///     strict LOWER BOUND on the true settlement cost. It is deliberately NOT a real
///     verification; see the report's evaluation section.
///
/// fund*/refund are identical for both schemes, so a gas report attributes any
/// difference to the verification step alone.
///
/// TWO-TIMEOUT REFUND RULE (paper 2020/845 §4.1). A cross-chain swap has two legs, each an
/// escrow created by a fund* call and reclaimable by `refund`. The paper mandates
/// ASYMMETRIC timelocks t2 < t1: the leg CLAIMED FIRST — the coin the witness holder u1
/// redeems, which reveals y — must carry the SHORTER timeout t2, and the leg CLAIMED
/// SECOND — redeemed by the reacting party u2 — the LONGER timeout t1. The gap (t1 − t2)
/// is u2's safety window: even if u1 claims the first leg at the last moment (≈ t2), u2
/// still has until t1 > t2 to extract y and claim the second leg, so a stalling u1 cannot
/// take one coin while the other's refund window lapses. This contract holds each leg
/// independently (the two legs live on two chains), so enforcing t2 < t1 across legs is
/// the funders' responsibility; `refund` itself only enforces a single leg's own timeout.
/// See test/AdaptorSwap.t.sol::test_TwoTimeoutSafetyWindow.
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
        bytes32       lasContext;    // verified LAS: commitment to (A', t, M) fixed at fund time
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
        swaps[id] = Swap(msg.sender, beneficiary, msg.value, funderSigner, claimHash, timeout, State.OPEN, bytes32(0));
        emit Funded(id, msg.sender, beneficiary, msg.value);
    }

    /// Escrow funds whose claim is gated on publishing the ADAPTED LAS signature bytes
    /// (the FLOOR path — claimLAS does not verify).
    function fundLAS(
        address payable beneficiary,
        uint64  timeout
    ) external payable returns (uint256 id) {
        require(msg.value > 0, "no value");
        id = nextId++;
        swaps[id] = Swap(msg.sender, beneficiary, msg.value, address(0), bytes32(0), timeout, State.OPEN, bytes32(0));
        emit Funded(id, msg.sender, beneficiary, msg.value);
    }

    /// Escrow funds for the VERIFIED LAS path. `lasContext` binds the verification
    /// context at fund time: it MUST equal keccak256(abi.encode(AprimeHat, t, message)),
    /// the public parameters A' (NTT domain), the funder public key t, and the claim
    /// message the adapted signature is over. claimLASVerified re-derives this from its
    /// arguments and rejects any mismatch, so a claimer cannot substitute their own
    /// pk/params/message to force a bogus signature to verify.
    function fundLASVerified(
        address payable beneficiary,
        uint64  timeout,
        bytes32 lasContext
    ) external payable returns (uint256 id) {
        require(msg.value > 0, "no value");
        id = nextId++;
        swaps[id] = Swap(msg.sender, beneficiary, msg.value, address(0), bytes32(0), timeout, State.OPEN, lasContext);
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
    /// LASVerify.SIG_BYTES of calldata + one keccak256 over them. NOT a lattice verification.
    function claimLAS(uint256 id, bytes calldata sigPacked) external {
        Swap storage sw = swaps[id];
        require(sw.state == State.OPEN, "not open");
        require(sigPacked.length == LASVerify.SIG_BYTES, "bad LAS sig length");
        bytes32 tag = keccak256(sigPacked); // minimal "touch every byte"; real verify is off-EVM
        sw.state = State.CLAIMED;
        emit Claimed(id, tag);
        sw.beneficiary.transfer(sw.amount);
    }

    /// Settle with FULL, numerically-correct on-chain LAS verification (LASVerify.verify,
    /// reproducing ref/basesig.c base_verify): decode + norm gate + w' = z_top + A'·z_bot
    /// − c·t + SHAKE256 challenge re-derivation. `AprimeHat` (the public parameters A' in
    /// NTT domain), `t` (funder public key), and `message` are supplied by the claimer but
    /// bound to the funded swap: they MUST hash to the `lasContext` committed at fund time,
    /// so no substitution is possible. MEASURED at ≈56.5M gas — it exceeds EIP-7825's
    /// per-transaction gas cap (16,777,216) by ≈3.4×, so it is not executable as a single
    /// mainnet transaction and stands as concrete evidence that THIS evaluated native
    /// Solidity LAS verifier (D3) needs a precompile, a Naysayer/optimistic scheme, or a
    /// succinct proof to be on-chain-viable (cf. poqeth, 2025/091). Wired here to prove the
    /// settlement path is numerically complete and securely bound, not to deploy.
    function claimLASVerified(
        uint256 id,
        bytes calldata sigPacked,
        uint256[][] calldata AprimeHat,
        uint256[][] calldata t,
        bytes calldata message
    ) external {
        Swap storage sw = swaps[id];
        require(sw.state == State.OPEN, "not open");
        require(keccak256(abi.encode(AprimeHat, t, message)) == sw.lasContext, "context mismatch");
        require(LASVerify.verify(AprimeHat, t, message, sigPacked), "LAS verify failed");
        sw.state = State.CLAIMED;
        emit Claimed(id, keccak256(sigPacked));
        sw.beneficiary.transfer(sw.amount);
    }

    /// Settle with full on-chain LAS verification **inside one transaction**.
    ///
    /// Same scheme, same parameters and the same accept/reject predicate as
    /// `claimLASVerified` — `LASVerifyOpt` is pinned to `LASVerify` and to the C golden
    /// vectors by `test/LASVerifierOpt.t.sol` — but re-expressed so that execution plus
    /// intrinsic gas lands under EIP-7825's 16,777,216 cap, which
    /// `test/LASGasBreakdown.t.sol::test_optimised_fits_in_one_transaction` asserts.
    /// This is the entrypoint that could actually be mined; `claimLASVerified` is kept
    /// as the measured baseline it is compared against, not as a deployable path.
    ///
    /// BINDING. `lasContext` must equal
    /// `keccak256(abi.encode(aHatPacked, tHatPacked, tPacked, message))`. That is a
    /// wider commitment than the `claimLASVerified` one: it additionally pins `tHatPacked`
    /// (the public key in NTT domain, registered so the verifier need not re-transform it)
    /// and `tPacked` (the normal-domain hash preimage of the same key). Both are supplied
    /// rather than derived, so both must be committed — otherwise a claimer could hand in
    /// a t̂ unrelated to the t that gets hashed.
    ///
    /// ⚠️ BINDING IS NOT WELL-FORMEDNESS. The commitment stops the CLAIMER substituting;
    /// it does not establish that the FUNDER registered `NTT(A')`, `NTT(t)` and `pack(t)`
    /// of one key pair, and nothing here checks that. Under a registration that breaks
    /// the invariant this entrypoint decides a DIFFERENT predicate from `base_verify` —
    /// not necessarily a weaker one, but some are weaker, and the degenerate case in
    /// LASVerifierOpt's REGISTRATION OBLIGATION is satisfiable with no key at all. That
    /// header also gives the mechanism and why the resulting loss (of atomicity, not of
    /// custody) falls on the funder who registered it. Statements that this path "runs
    /// full base_verify" must carry the invariant as a condition.
    function claimLASVerifiedOpt(
        uint256 id,
        bytes calldata sigPacked,
        bytes calldata aHatPacked,
        bytes calldata tHatPacked,
        bytes calldata tPacked,
        bytes calldata message
    ) external {
        Swap storage sw = swaps[id];
        require(sw.state == State.OPEN, "not open");
        require(
            keccak256(abi.encode(aHatPacked, tHatPacked, tPacked, message)) == sw.lasContext, "context mismatch"
        );
        require(LASVerifyOpt.verify(aHatPacked, tHatPacked, tPacked, message, sigPacked), "LAS verify failed");
        sw.state = State.CLAIMED;
        emit Claimed(id, keccak256(sigPacked));
        sw.beneficiary.transfer(sw.amount);
    }

    /// Payer reclaims the escrow after THIS leg's own timeout if no one claimed. In a
    /// two-leg swap the funders must set t2 < t1 across the legs (see the contract header);
    /// this function enforces only the single timeout stored on the leg being refunded.
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
