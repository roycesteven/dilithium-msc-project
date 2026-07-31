// SPDX-License-Identifier: MIT
pragma solidity ^0.8.25;

import {sampleInBallNist} from "../lib/zknox/ZKNOX_SampleInBall.sol";
import {CtxShake, shakeInit, shakeUpdate, shakeDigest} from "../lib/zknox/ZKNOX_shake.sol";

/// @title LASNaysayLib — the three locally-checkable invariants of the adapted-LAS
///        verifier (LASVerify.verify), each a self-contained naysay witness. UNAUDITED.
///
/// COMPLETENESS: EVERY invalid transcript admits AT LEAST ONE successful naysay (not
/// "exactly one"). SOUNDNESS: a valid transcript admits NONE.
///   (norm)   ‖z‖∞ ≤ γ−κ                                — normExceedsAt
///   (wprime) w'[i]=z_top[i]+(A'·z_bot)[i]−(c·t)[i], c DERIVED from c̃ — wprimeMismatchAt
///   (digest) c̃ = SHAKE256(pack(t)‖pack(w')‖M)           — digestMismatch
/// The challenge c is deterministic from c̃, so it is not a claimant input. The native
/// verifier's NTT batch never runs in a naysay. Constants mirror LASVerifier.sol; A' is
/// consumed in NORMAL domain — see ref/test/export_naysayer_vectors.c.
library LASNaysayLib {
    uint256 internal constant N = 256;
    uint256 internal constant Q = 8380417;
    uint256 internal constant KAPPA = 49;
    uint256 internal constant N_LAS = 6;
    uint256 internal constant ELL = 5;
    uint256 internal constant N_PLUS_ELL = 11;
    uint256 internal constant Z_BITS = 19;
    uint256 internal constant Z_OFFSET = 137935; // γ − κ
    uint256 internal constant BOUND = 137935;
    /// c_tilde width: FIPS 204 §7.3 lambda/4 for the ML-DSA-65-aligned set.
    uint256 internal constant CTILDE_BYTES = 48;
    uint256 internal constant SIG_BYTES = CTILDE_BYTES + (N_PLUS_ELL * N * Z_BITS) / 8; // 6736

    // dims + canonical range [0,Q); rejects malformed arrays before any indexing.
    function isCanonicalPolys(uint256[][] calldata polys, uint256 count) internal pure returns (bool) {
        if (polys.length != count) return false;
        for (uint256 i = 0; i < count; i++) {
            if (polys[i].length != N) return false;
            for (uint256 k = 0; k < N; k++) {
                if (polys[i][k] >= Q) return false;
            }
        }
        return true;
    }

    function normExceedsAt(bytes calldata sig, uint256 i, uint256 k) internal pure returns (bool) {
        uint256 bit = CTILDE_BYTES * 8 + (i * N + k) * Z_BITS;
        int256 zc = int256(Z_OFFSET) - int256(_readField(sig, bit));
        uint256 zres = zc >= 0 ? uint256(zc) : uint256(int256(Q) + zc);
        uint256 absz = zres <= Q / 2 ? zres : Q - zres;
        return absz > BOUND;
    }

    function deriveChallenge(bytes calldata sig) internal pure returns (uint256[] memory) {
        bytes memory cTilde = sig[0:CTILDE_BYTES];
        return sampleInBallNist(cTilde, KAPPA, Q); // {0,1,Q−1}
    }

    function wprimeMismatchAt(
        uint256[][] calldata aprime,
        uint256[] calldata ti,
        uint256[] memory cPoly,
        uint256[][] memory z,
        uint256 i,
        uint256 k,
        uint256 wClaim
    ) internal pure returns (bool) {
        uint256 acc = z[i][k];
        for (uint256 j = 0; j < ELL; j++) {
            acc = addmod(acc, _convCoeff(aprime[i * ELL + j], z[N_LAS + j], k), Q);
        }
        acc = addmod(acc, Q - _convCoeff(cPoly, ti, k), Q);
        return acc != wClaim;
    }

    // k-th coeff of a·b in Z_q[x]/(x^n+1). Mirrors negacyclic_conv in export_verify_vector.c.
    function _convCoeff(uint256[] memory a, uint256[] memory b, uint256 k) private pure returns (uint256) {
        uint256 pos = 0;
        for (uint256 u = 0; u <= k; u++) pos = addmod(pos, mulmod(a[u], b[k - u], Q), Q);
        uint256 neg = 0;
        for (uint256 u = k + 1; u < N; u++) neg = addmod(neg, mulmod(a[u], b[N + k - u], Q), Q);
        return addmod(pos, Q - neg, Q);
    }

    function digestMismatch(
        bytes calldata sig,
        uint256[][] calldata t,
        uint256[][] calldata wprime,
        bytes calldata message
    ) internal pure returns (bool) {
        CtxShake memory ctx = shakeInit();
        ctx = shakeUpdate(ctx, _packPolys(t, N_LAS));
        ctx = shakeUpdate(ctx, _packPolys(wprime, N_LAS));
        ctx = shakeUpdate(ctx, message);
        bytes memory d = shakeDigest(ctx, CTILDE_BYTES);
        for (uint256 x = 0; x < CTILDE_BYTES; x++) {
            if (d[x] != sig[x]) return true;
        }
        return false;
    }

    function decodeZ(bytes calldata sig) internal pure returns (uint256[][] memory z) {
        z = new uint256[][](N_PLUS_ELL);
        uint256 bit = CTILDE_BYTES * 8;
        for (uint256 i = 0; i < N_PLUS_ELL; i++) {
            z[i] = new uint256[](N);
            for (uint256 k = 0; k < N; k++) {
                int256 zc = int256(Z_OFFSET) - int256(_readField(sig, bit));
                bit += Z_BITS;
                z[i][k] = zc >= 0 ? uint256(zc) : uint256(int256(Q) + zc);
            }
        }
    }

    function _readField(bytes calldata buf, uint256 bitpos) private pure returns (uint256 f) {
        uint256 o = bitpos >> 3;
        uint256 len = buf.length;
        uint256 w = uint256(uint8(buf[o]));
        if (o + 1 < len) w |= uint256(uint8(buf[o + 1])) << 8;
        if (o + 2 < len) w |= uint256(uint8(buf[o + 2])) << 16;
        if (o + 3 < len) w |= uint256(uint8(buf[o + 3])) << 24;
        f = (w >> (bitpos & 7)) & 0x7FFFF;
    }

    function _packPolys(uint256[][] calldata polys, uint256 count) private pure returns (bytes memory out) {
        out = new bytes(count * N * 4);
        uint256 o = 0;
        for (uint256 i = 0; i < count; i++) {
            for (uint256 k = 0; k < N; k++) {
                uint256 x = polys[i][k];
                out[o] = bytes1(uint8(x));
                out[o + 1] = bytes1(uint8(x >> 8));
                out[o + 2] = bytes1(uint8(x >> 16));
                out[o + 3] = bytes1(uint8(x >> 24));
                o += 4;
            }
        }
    }
}

/// @title LASNaysayerSwap — scriptless adaptor-swap escrow settled OPTIMISTICALLY (poqeth
///        Naysayer mode applied to lattice). UNAUDITED research sketch.
///
/// SAFETY: (1) claim only if the whole window fits before timeout; (2) naysays rejected
/// at/after the deadline; (3) trace/param dims+ranges validated; (4) challenge derived
/// from c̃; (5) a voided claim REOPENS to OPEN; (6) usedSig replay guard + per-swap ioCommit
/// bind M — full message-domain separation is UNRESOLVED (see domainDigest); (7) pull-
/// payment, no external call in state transitions; (8) naysayWprime carries full A' as an
/// explicit BASELINE (final design opens row i via a Merkle proof — UNRESOLVED). The gas
/// compared to EIP-7825 must be the LARGEST valid fraud-proof tx (likely naysayWprime).
contract LASNaysayerSwap {
    enum State { EMPTY, OPEN, PENDING, CLAIMED, REFUNDED }

    struct Swap {
        address payer;
        address payable beneficiary;
        uint256 amount;
        uint64 timeout;
        bytes32 ioCommit;     // keccak256(abi.encode(t, message))
        bytes32 aprimeCommit; // keccak256(abi.encode(aprimeNormal))
        State state;
    }

    struct Pending {
        address payable claimer;
        uint256 bond;
        bytes32 sigHash;
        bytes32 traceCommit; // keccak256(abi.encode(wprime))
        uint64 deadline;
    }

    uint256 public constant CHALLENGE_PERIOD = 1 hours;
    uint256 public constant MIN_BOND = 0.01 ether;

    uint256 public nextId;
    mapping(uint256 => Swap) public swaps;
    mapping(uint256 => Pending) public pending;
    mapping(address => uint256) public credits;
    mapping(bytes32 => bool) public usedSig;

    event Funded(uint256 indexed id, address payer, address beneficiary, uint256 amount);
    event ClaimOpened(uint256 indexed id, address claimer, uint64 deadline);
    event Naysaid(uint256 indexed id, string reason, address naysayer);
    event Claimed(uint256 indexed id, bytes32 sigHash);
    event Refunded(uint256 indexed id);

    function fund(address payable beneficiary, uint64 timeout, bytes32 ioCommit, bytes32 aprimeCommit)
        external
        payable
        returns (uint256 id)
    {
        require(msg.value > 0, "no value");
        require(timeout > block.timestamp + CHALLENGE_PERIOD, "timeout inside window");
        id = nextId++;
        swaps[id] = Swap(msg.sender, beneficiary, msg.value, timeout, ioCommit, aprimeCommit, State.OPEN);
        emit Funded(id, msg.sender, beneficiary, msg.value);
    }

    function optimisticClaim(uint256 id, bytes calldata sig, uint256[][] calldata wprime) external payable {
        Swap storage sw = swaps[id];
        require(sw.state == State.OPEN, "not open");
        require(msg.sender == sw.beneficiary, "not beneficiary");
        require(sig.length == LASNaysayLib.SIG_BYTES, "bad sig length");
        require(msg.value >= MIN_BOND, "bond too low");
        require(block.timestamp + CHALLENGE_PERIOD <= sw.timeout, "window past timeout");
        require(LASNaysayLib.isCanonicalPolys(wprime, LASNaysayLib.N_LAS), "bad trace");
        bytes32 sh = keccak256(sig);
        require(!usedSig[sh], "sig replay");
        usedSig[sh] = true;
        pending[id] = Pending({
            claimer: payable(msg.sender),
            bond: msg.value,
            sigHash: sh,
            traceCommit: keccak256(abi.encode(wprime)),
            deadline: uint64(block.timestamp + CHALLENGE_PERIOD)
        });
        sw.state = State.PENDING;
        emit ClaimOpened(id, msg.sender, pending[id].deadline);
    }

    function naysayNorm(uint256 id, bytes calldata sig, uint256 i, uint256 k) external {
        _active(id);
        _bindSig(id, sig);
        require(i < LASNaysayLib.N_PLUS_ELL && k < LASNaysayLib.N, "index oob");
        require(LASNaysayLib.normExceedsAt(sig, i, k), "norm ok");
        _slash(id, "norm");
    }

    /// BASELINE (full-A' calldata) — see header item 8. Args are bundled in a calldata
    /// struct to keep the stack within the legacy (non-viaIR) pipeline's 16-slot limit.
    struct WprimeProof {
        bytes sig;
        uint256[][] wprime;
        uint256[][] aprime;
        uint256[][] t;
        bytes message;
        uint256 i;
        uint256 k;
    }

    function naysayWprime(uint256 id, WprimeProof calldata p) external {
        _active(id);
        _bindSig(id, p.sig);
        _bindTrace(id, p.wprime);
        _bindIo(id, p.t, p.message);
        _bindAprime(id, p.aprime);
        require(p.i < LASNaysayLib.N_LAS && p.k < LASNaysayLib.N, "index oob");
        require(LASNaysayLib.isCanonicalPolys(p.t, LASNaysayLib.N_LAS), "bad t");
        require(LASNaysayLib.isCanonicalPolys(p.aprime, LASNaysayLib.N_LAS * LASNaysayLib.ELL), "bad aprime");
        require(_wprimeFaulty(p), "wprime ok");
        _slash(id, "wprime");
    }

    function _wprimeFaulty(WprimeProof calldata p) private pure returns (bool) {
        uint256[] memory c = LASNaysayLib.deriveChallenge(p.sig);
        uint256[][] memory z = LASNaysayLib.decodeZ(p.sig);
        return LASNaysayLib.wprimeMismatchAt(p.aprime, p.t[p.i], c, z, p.i, p.k, p.wprime[p.i][p.k]);
    }

    function naysayDigest(
        uint256 id,
        bytes calldata sig,
        uint256[][] calldata wprime,
        uint256[][] calldata t,
        bytes calldata message
    ) external {
        _active(id);
        _bindSig(id, sig);
        _bindTrace(id, wprime);
        _bindIo(id, t, message);
        require(LASNaysayLib.isCanonicalPolys(t, LASNaysayLib.N_LAS), "bad t");
        require(LASNaysayLib.digestMismatch(sig, t, wprime, message), "digest ok");
        _slash(id, "digest");
    }

    function finalize(uint256 id) external {
        Swap storage sw = swaps[id];
        Pending storage p = pending[id];
        require(sw.state == State.PENDING, "not pending");
        require(block.timestamp >= p.deadline, "window open");
        sw.state = State.CLAIMED;
        credits[sw.beneficiary] += sw.amount;
        credits[p.claimer] += p.bond;
        bytes32 sh = p.sigHash; // remains in usedSig (settled)
        delete pending[id];
        emit Claimed(id, sh);
    }

    function refund(uint256 id) external {
        Swap storage sw = swaps[id];
        require(sw.state == State.OPEN, "not open");
        require(block.timestamp >= sw.timeout, "before timeout");
        require(msg.sender == sw.payer, "not payer");
        sw.state = State.REFUNDED;
        credits[sw.payer] += sw.amount;
        emit Refunded(id);
    }

    function withdraw() external {
        uint256 amount = credits[msg.sender];
        require(amount > 0, "nothing");
        credits[msg.sender] = 0;
        (bool ok,) = msg.sender.call{value: amount}("");
        require(ok, "withdraw failed");
    }

    function stateOf(uint256 id) external view returns (State) {
        return swaps[id].state;
    }

    /// Swap-specific digest a production signer MUST commit inside M to prevent cross-swap
    /// replay. Enforcement against the golden fixed-M vectors is UNRESOLVED (see header).
    function domainDigest(uint256 id) public view returns (bytes32) {
        Swap storage sw = swaps[id];
        return keccak256(
            abi.encode(block.chainid, address(this), id, sw.payer, sw.beneficiary, sw.amount, sw.timeout)
        );
    }

    function _active(uint256 id) private view {
        require(swaps[id].state == State.PENDING, "not pending");
        require(block.timestamp < pending[id].deadline, "window closed");
    }

    function _bindSig(uint256 id, bytes calldata sig) private view {
        require(keccak256(sig) == pending[id].sigHash, "sig not bound");
    }

    function _bindTrace(uint256 id, uint256[][] calldata wprime) private view {
        require(keccak256(abi.encode(wprime)) == pending[id].traceCommit, "trace not bound");
    }

    function _bindIo(uint256 id, uint256[][] calldata t, bytes calldata message) private view {
        require(keccak256(abi.encode(t, message)) == swaps[id].ioCommit, "io not bound");
    }

    function _bindAprime(uint256 id, uint256[][] calldata aprime) private view {
        require(keccak256(abi.encode(aprime)) == swaps[id].aprimeCommit, "aprime not bound");
    }

    function _slash(uint256 id, string memory reason) private {
        Pending storage p = pending[id];
        uint256 bond = p.bond;
        usedSig[p.sigHash] = false; // bad sig never settled; free it
        delete pending[id];
        swaps[id].state = State.OPEN; // genuinely re-claimable
        credits[msg.sender] += bond;
        emit Naysaid(id, reason, msg.sender);
    }
}
