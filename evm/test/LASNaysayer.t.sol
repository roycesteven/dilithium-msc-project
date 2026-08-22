// SPDX-License-Identifier: MIT
pragma solidity ^0.8.25;

import {LASNaysayerSwap, LASNaysayLib} from "../src/LASNaysayer.sol";
import {LASVerify} from "../src/LASVerifier.sol";

/// Adversarial suite for the optimistic Naysayer verifier. Reuses the golden vectors of
/// LASVerifier.t.sol; the committed trace is w_prime.bin. The PURE digest-fault case uses
/// sig_digestfault.bin / w_prime_digestfault.bin from ref/test/export_naysayer_vectors.c.
///
/// Every expected revert is matched by REASON (not just "it reverted"), so a spurious
/// failure (binding revert, OOB Panic, ABI error) cannot pass a soundness assertion.
///
/// GAS: the GasUsed deltas and forge --gas-report measure EXECUTION gas only — they do NOT
/// include the 21,000 intrinsic or the calldata byte cost of the outer transaction, and
/// --isolate does not add those either. For the EIP-7825 (16,777,216) per-tx comparison,
/// take TOTAL = 21,000 + max(4*tokens + execution, 10*tokens) with tokens = zero_bytes +
/// 4*non_zero_bytes — EIP-7623, NOT the old 16-per-non-zero/4-per-zero EIP-2028 model,
/// which understates calldata-heavy txs — or read gasUsed from a real transaction receipt
/// (anvil + `cast send`). The binding path is the LARGEST valid fraud-proof tx. A' is
/// passed as uint256[][] (30 polys x 256 coeffs x 32-byte words) ~= 246 KB + ABI overhead
/// of calldata against ~30 KB in the packed 4-byte form, which is what motivates the
/// row-Merkle variant; but for the tested 32-byte message and committed vectors the
/// binding path is naysayDigest, NOT naysayWprime — wprime's total stays below digest's
/// execution gas alone even charging every wprime calldata byte as non-zero. Execution
/// gas is data-dependent, so re-derive that ordering if the vectors change.
contract LASNaysayerTest {
    Vm constant vm = Vm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);
    uint256 constant N = 256;
    uint256 constant Q = 8380417;

    LASNaysayerSwap nay;
    uint64 timeout;

    event GasUsed(string tag, uint256 gas);

    function setUp() public {
        nay = new LASNaysayerSwap();
        vm.deal(address(this), 100 ether);
        timeout = uint64(block.timestamp + 2 days);
    }
    receive() external payable {}

    // ---- vector loaders ----
    function _readPolys(string memory name, uint256 count) internal view returns (uint256[][] memory p) {
        bytes memory raw = vm.readFileBinary(string.concat("test/vectors/", name));
        require(raw.length == count * N * 4, "bad polys length");
        p = new uint256[][](count);
        uint256 o = 0;
        for (uint256 i = 0; i < count; i++) {
            p[i] = new uint256[](N);
            for (uint256 k = 0; k < N; k++) {
                p[i][k] = uint256(uint8(raw[o])) | (uint256(uint8(raw[o + 1])) << 8)
                    | (uint256(uint8(raw[o + 2])) << 16) | (uint256(uint8(raw[o + 3])) << 24);
                o += 4;
            }
        }
    }
    function _aprime() internal view returns (uint256[][] memory) { return _readPolys("pp_normal.bin", 30); }
    function _t() internal view returns (uint256[][] memory) { return _readPolys("t.bin", 6); }
    function _wprime() internal view returns (uint256[][] memory) { return _readPolys("w_prime.bin", 6); }
    function _wprimeDf() internal view returns (uint256[][] memory) { return _readPolys("w_prime_digestfault.bin", 6); }
    function _msg() internal view returns (bytes memory) { return vm.readFileBinary("test/vectors/msg.bin"); }
    function _sig() internal view returns (bytes memory) { return vm.readFileBinary("test/vectors/sig.bin"); }
    function _sigDf() internal view returns (bytes memory) { return vm.readFileBinary("test/vectors/sig_digestfault.bin"); }

    // ---- fund / claim ----
    function _fund(address payable beneficiary, uint64 to) internal returns (uint256 id) {
        bytes32 ioCommit = keccak256(abi.encode(_t(), _msg()));
        bytes32 aprimeCommit = keccak256(abi.encode(_aprime()));
        id = nay.fund{value: 1 ether}(beneficiary, to, ioCommit, aprimeCommit);
    }
    function _claim(uint256 id, bytes memory sig, uint256[][] memory wprime) internal {
        uint256 bond = nay.MIN_BOND();
        uint256 g = gasleft();
        nay.optimisticClaim{value: bond}(id, sig, wprime);
        emit GasUsed("optimisticClaim", g - gasleft());
    }

    // ---- reason-checked revert helpers ----
    function _revertsWith(bytes memory cd, string memory want) internal returns (bool) {
        (bool ok, bytes memory ret) = address(nay).call(cd);
        return !ok && keccak256(bytes(_reason(ret))) == keccak256(bytes(want));
    }
    function _revertsWithValue(bytes memory cd, uint256 val, string memory want) internal returns (bool) {
        (bool ok, bytes memory ret) = address(nay).call{value: val}(cd);
        return !ok && keccak256(bytes(_reason(ret))) == keccak256(bytes(want));
    }
    function _reason(bytes memory ret) internal pure returns (string memory) {
        if (ret.length < 68) return ""; // not an Error(string) (e.g. Panic/OOB) -> reason mismatch
        assembly { ret := add(ret, 0x04) }
        return abi.decode(ret, (string));
    }

    // ================================ HAPPY PATH ==============================
    // Soundness rests on the golden sig being a GENUINE valid adapted signature (C base_verify
    // accepts it; w_prime.bin is the true arithmetic — validated in LASVerifier.t.sol), so no
    // naysay lands at ANY coefficient. The calls below SPOT-CHECK a spread of coefficients as a
    // sanity cross-check; they are not, on their own, a proof over all coefficients.
    function test_happyPath_finalizes_and_no_naysay_lands() public {
        uint256 id = _fund(payable(address(this)), timeout);
        _claim(id, _sig(), _wprime());

        uint256[3] memory ii = [uint256(0), 6, 10];
        uint256[2] memory kk = [uint256(0), 255];
        for (uint256 a = 0; a < 3; a++) {
            for (uint256 b = 0; b < 2; b++) {
                require(_revertsWith(abi.encodeCall(nay.naysayNorm, (id, _sig(), ii[a], kk[b])), "norm ok"), "norm spot");
            }
        }
        uint256[2] memory wi = [uint256(0), 5];
        for (uint256 a = 0; a < 2; a++) {
            for (uint256 b = 0; b < 2; b++) {
                require(
                    _revertsWith(abi.encodeCall(nay.naysayWprime, (id, LASNaysayerSwap.WprimeProof(_sig(), _wprime(), _aprime(), _t(), _msg(), wi[a], kk[b]))), "wprime ok"),
                    "wprime spot"
                );
            }
        }
        require(_revertsWith(abi.encodeCall(nay.naysayDigest, (id, _sig(), _wprime(), _t(), _msg())), "digest ok"), "digest");

        vm.warp(block.timestamp + nay.CHALLENGE_PERIOD());
        uint256 g = gasleft();
        nay.finalize(id);
        emit GasUsed("finalize", g - gasleft());
        require(nay.credits(address(this)) == 1 ether + nay.MIN_BOND(), "not credited");
    }

    // ============================== NAYSAY: NORM =============================
    function test_naysayNorm_voids_over_bound_z() public {
        uint256 id = _fund(payable(address(this)), timeout);
        bytes memory bad = _sig();
        uint256 zo = LASVerify.CTILDE_BYTES; // z region starts right after c_tilde
        bad[zo] = bytes1(uint8(0xFF)); bad[zo + 1] = bytes1(uint8(0xFF)); bad[zo + 2] = bytes1(uint8(0xFF)); // z[0][0] field -> 0x7FFFF
        _claim(id, bad, _wprime());

        uint256 g = gasleft();
        nay.naysayNorm(id, bad, 0, 0);
        emit GasUsed("naysayNorm", g - gasleft());
        require(nay.stateOf(id) == LASNaysayerSwap.State.OPEN, "leg not reopened");
        require(nay.credits(address(this)) == nay.MIN_BOND(), "bond not slashed");
    }

    // ============================= NAYSAY: WPRIME ============================
    function test_naysayWprime_voids_wrong_coeff() public {
        uint256 id = _fund(payable(address(this)), timeout);
        uint256[][] memory badW = _wprime();
        badW[0][0] = (badW[0][0] + 1) % Q;
        _claim(id, _sig(), badW);

        LASNaysayerSwap.WprimeProof memory pf = LASNaysayerSwap.WprimeProof(_sig(), badW, _aprime(), _t(), _msg(), 0, 0);
        uint256 g = gasleft();
        nay.naysayWprime(id, pf);
        emit GasUsed("naysayWprime_fullA", g - gasleft());
        require(nay.stateOf(id) == LASNaysayerSwap.State.OPEN, "leg not reopened");
        require(nay.credits(address(this)) == nay.MIN_BOND(), "bond not slashed");
    }

    // ===================== NAYSAY: DIGEST (pure C vector) ====================
    // The vector is a PURE digest fault BY CONSTRUCTION: export_naysayer_vectors.c self-checks
    // z within bound at EVERY coefficient (no norm fault anywhere) and computes w'_df as the
    // true arithmetic at EVERY coefficient (no wprime fault anywhere) — THAT is what
    // establishes the universal. The on-chain calls below only SPOT-CHECK selected
    // coefficients as a sanity cross-check; they do not, on their own, prove that no norm or
    // wprime naysay lands at every coefficient.
    function test_naysayDigest_voids_pure_hash_fault() public {
        uint256 id = _fund(payable(address(this)), timeout);
        bytes memory sig = _sigDf();
        uint256[][] memory w = _wprimeDf();
        _claim(id, sig, w);

        uint256[3] memory ii = [uint256(0), 6, 10]; // norm rows span 0..N_PLUS_ELL-1
        uint256[2] memory kk = [uint256(0), 255];
        for (uint256 a = 0; a < 3; a++) {
            for (uint256 b = 0; b < 2; b++) {
                require(_revertsWith(abi.encodeCall(nay.naysayNorm, (id, sig, ii[a], kk[b])), "norm ok"), "norm landed");
            }
        }
        uint256[2] memory wi = [uint256(0), 5]; // wprime rows span 0..N_LAS-1
        for (uint256 a = 0; a < 2; a++) {
            for (uint256 b = 0; b < 2; b++) {
                require(
                    _revertsWith(abi.encodeCall(nay.naysayWprime, (id, LASNaysayerSwap.WprimeProof(sig, w, _aprime(), _t(), _msg(), wi[a], kk[b]))), "wprime ok"),
                    "wprime landed"
                );
            }
        }
        uint256[][] memory tt = _t();
        bytes memory mm = _msg();
        uint256 g = gasleft();
        nay.naysayDigest(id, sig, w, tt, mm);
        emit GasUsed("naysayDigest", g - gasleft());
        require(nay.stateOf(id) == LASNaysayerSwap.State.OPEN, "leg not reopened");
    }

    // ========================= VALIDATION AT CLAIM ==========================
    function test_optimisticClaim_rejects_malformed_trace() public {
        uint256 id = _fund(payable(address(this)), timeout);
        uint256 bond = nay.MIN_BOND();
        uint256[][] memory w1 = _wprime(); w1[0][0] = Q; // coefficient >= Q
        require(_revertsWithValue(abi.encodeCall(nay.optimisticClaim, (id, _sig(), w1)), bond, "bad trace"), "coeff>=Q accepted");
        uint256[][] memory w2 = new uint256[][](5); // wrong outer dimension
        for (uint256 i = 0; i < 5; i++) w2[i] = new uint256[](N);
        require(_revertsWithValue(abi.encodeCall(nay.optimisticClaim, (id, _sig(), w2)), bond, "bad trace"), "wrong dim accepted");
    }

    function test_optimisticClaim_rejects_window_past_timeout() public {
        uint64 tight = uint64(block.timestamp + nay.CHALLENGE_PERIOD() + 30);
        uint256 id = _fund(payable(address(this)), tight);
        vm.warp(block.timestamp + 60); // now + PERIOD now exceeds timeout
        require(
            _revertsWithValue(abi.encodeCall(nay.optimisticClaim, (id, _sig(), _wprime())), nay.MIN_BOND(), "window past timeout"),
            "claim opened past timeout"
        );
    }

    // =========================== POST-DEADLINE ==============================
    function test_naysay_rejected_after_deadline() public {
        uint256 id = _fund(payable(address(this)), timeout);
        bytes memory bad = _sig();
        uint256 zo2 = LASVerify.CTILDE_BYTES;
        bad[zo2] = bytes1(uint8(0xFF)); bad[zo2 + 1] = bytes1(uint8(0xFF)); bad[zo2 + 2] = bytes1(uint8(0xFF));
        _claim(id, bad, _wprime());
        vm.warp(block.timestamp + nay.CHALLENGE_PERIOD()); // at deadline
        require(_revertsWith(abi.encodeCall(nay.naysayNorm, (id, bad, uint256(0), uint256(0))), "window closed"), "late naysay landed");
        nay.finalize(id); // optimistic accept after the window (documented risk)
        require(nay.credits(address(this)) == 1 ether + nay.MIN_BOND(), "not finalized");
    }

    // ======================= FINALIZE / REPLAY GUARD ========================
    function test_finalize_window_and_replay_guard() public {
        uint256 id = _fund(payable(address(this)), timeout);
        _claim(id, _sig(), _wprime());
        require(_revertsWith(abi.encodeCall(nay.finalize, (id)), "window open"), "finalized early");
        vm.warp(block.timestamp + nay.CHALLENGE_PERIOD());
        nay.finalize(id);
        require(_revertsWith(abi.encodeCall(nay.finalize, (id)), "not pending"), "double finalize");
        uint256 id2 = _fund(payable(address(this)), timeout); // settled sig cannot be replayed
        require(_revertsWithValue(abi.encodeCall(nay.optimisticClaim, (id2, _sig(), _wprime())), nay.MIN_BOND(), "sig replay"), "sig replayed");
    }

    // ===================== HOSTILE RECIPIENT (pull-pay) =====================
    function test_contract_recipient_cannot_block_finalize() public {
        Rejector rej = new Rejector(nay);
        vm.deal(address(rej), 1 ether);
        uint256 id = _fund(payable(address(rej)), timeout);
        rej.claim{value: nay.MIN_BOND()}(id, _sig(), _wprime());
        vm.warp(block.timestamp + nay.CHALLENGE_PERIOD());
        nay.finalize(id); // must NOT revert despite a hostile beneficiary
        require(nay.credits(address(rej)) == 1 ether + nay.MIN_BOND(), "not credited");
        (bool ok, bytes memory ret) = address(rej).call(abi.encodeWithSignature("doWithdraw()"));
        require(!ok && keccak256(bytes(_reason(ret))) == keccak256(bytes("withdraw failed")), "hostile withdraw not isolated");
    }
}

/// Beneficiary that must claim itself and rejects incoming ETH (tests pull-payment isolation).
contract Rejector {
    LASNaysayerSwap nay;
    constructor(LASNaysayerSwap n) { nay = n; }
    function claim(uint256 id, bytes calldata sig, uint256[][] calldata wprime) external payable {
        nay.optimisticClaim{value: msg.value}(id, sig, wprime);
    }
    function doWithdraw() external { nay.withdraw(); }
    receive() external payable { revert("no eth"); }
}

/// Minimal Foundry cheatcode interface (mirrors AdaptorSwap.t.sol; no forge-std).
interface Vm {
    function warp(uint256 newTimestamp) external;
    function readFileBinary(string calldata path) external view returns (bytes memory);
    function deal(address who, uint256 newBalance) external;
}
