// SPDX-License-Identifier: MIT
pragma solidity ^0.8.25;

import {AdaptorSwap} from "../src/AdaptorSwap.sol";
import {LASVerify} from "../src/LASVerifier.sol";
import {LASNaysayerSwap} from "../src/LASNaysayer.sol";

/// Minimal Foundry cheatcode interface (the repo deliberately avoids a forge-std dependency).
interface Vm {
    function addr(uint256 privateKey) external pure returns (address);
    function sign(uint256 privateKey, bytes32 digest) external pure returns (uint8, bytes32, bytes32);
    function warp(uint256 newTimestamp) external;
    function readFileBinary(string calldata path) external view returns (bytes memory);
    function deal(address who, uint256 newBalance) external;
}

/// Self-contained `console.log` shim (staticcall to Foundry's console address), so the
/// table below prints under `forge test -vv` without pulling in forge-std.
library console {
    address constant CONSOLE = 0x000000000000000000636F6e736F6c652e6c6f67;

    function _send(bytes memory payload) private view {
        address target = CONSOLE;
        assembly {
            pop(staticcall(gas(), target, add(payload, 32), mload(payload), 0, 0))
        }
    }

    function log(string memory a) internal view {
        _send(abi.encodeWithSignature("log(string)", a));
    }

    function log(string memory a, uint256 b) internal view {
        _send(abi.encodeWithSignature("log(string,uint256)", a, b));
    }

    function log(string memory a, uint256 b, uint256 c, uint256 d) internal view {
        _send(abi.encodeWithSignature("log(string,uint256,uint256,uint256)", a, b, c, d));
    }
}

/// @title TwoLegSwapGas — the EVM performance baseline for a **scriptless adaptor-signature
///        atomic swap**, measured leg by leg at one fixed protocol boundary, classical
///        against post-quantum.
///
/// WHY THIS EXISTS. Meeting-7 deferred the EVM but asked for the preparatory measurement:
/// what a classical adaptor swap costs on a smart-contract chain, so a future
/// post-quantum-on-EVM comparison has a baseline. `AdaptorSwap.t.sol` and
/// `--gas-report` already price individual functions; what was missing is the **whole
/// two-leg swap at the same boundary**, with the costs a `--gas-report` never shows
/// (intrinsic + calldata), and with the refund branches under the paper's asymmetric
/// timelocks.
///
/// THE BOUNDARY, HELD FIXED ACROSS SCHEMES. Both schemes run the identical protocol of
/// eprint 2020/845 §4.1 Fig. 1; only the on-chain verification of the published adapted
/// signature differs:
///
///   Gen, PreSign, PreVerify, Adapt, Ext, and the proof of knowledge pi are ALL OFF-CHAIN
///   and cost zero gas in every configuration. The chain sees exactly two events per swap
///   — one ordinary signature published per leg — which is the whole point of a scriptless
///   swap and why classical and LAS are comparable here at all.
///
/// | step                       | on chain | measured here                          |
/// |----------------------------|----------|----------------------------------------|
/// | fund leg A (c1, timeout t1)| yes      | exec + calldata + intrinsic            |
/// | fund leg B (c2, timeout t2)| yes      | exec + calldata + intrinsic            |
/// | first claim  (u1 takes c2) | yes      | exec + calldata + intrinsic            |
/// | Ext: u2 recovers y from σ2 | **no**   | zero gas — off-chain, see note below   |
/// | second claim (u2 takes c1) | yes      | exec + calldata + intrinsic            |
/// | refund leg B after t2      | yes      | exec + calldata + intrinsic            |
/// | refund leg A after t1      | yes      | exec + calldata + intrinsic            |
///
/// ROLES AND TIMELOCKS (paper §4.1 Setup, not Fig. 1 — Fig. 1 shows honest-path messages
/// only). u1 holds the witness and claims FIRST, on leg B, which therefore carries the
/// SHORTER timeout t2; u2 reacts on leg A under the LONGER t1. The gap t1 − t2 is u2's
/// safety window, asserted in `SwapSafetyWindowGas`.
///
/// LEG A = chain A, holding u1's coin c1, beneficiary u2, funder-signer u1, timeout t1.
/// LEG B = chain B, holding u2's coin c2, beneficiary u1, funder-signer u2, timeout t2.
/// Two `AdaptorSwap` deployments stand for the two chains, so no leg can observe the other.
///
/// GAS ACCOUNTING. `gasleft()` deltas are EXECUTION gas only: they exclude the 21,000
/// intrinsic and the calldata byte cost that a real transaction pays (EIP-2028: 16 gas per
/// non-zero byte, 4 per zero byte). Those dominate for LAS, whose claim carries a
/// 6720-byte signature, so every row reports execution, calldata bytes, intrinsic and the
/// TOTAL a transaction would actually be charged. That total is the figure to compare
/// against the EIP-7825 per-transaction cap of 16,777,216 gas.
///
/// COLD STATE. Each measured call is the first storage access of its own test transaction:
/// prerequisites are done in `setUp`, which Foundry runs as a separate transaction, so
/// warm-slot discounts do not flatter any step. This is why the steps are split across
/// several contracts rather than sequenced inside one test.
///
/// OFF-CHAIN COSTS ARE NOT INVENTED HERE. Extraction, pre-signing and pi cost time and
/// bytes, but not gas; this harness reports them as zero *on-chain* and nothing more. The
/// off-chain time and communication figures are the Stage-2 measurement
/// (`rust/las-swap`, `evidence/stage2/`), and must be cited from there rather than
/// re-derived in Solidity.
///
/// NOT BUILT. A Groth16 (or other succinct) on-chain verifier for LAS does not exist in
/// this repo, so no row claims one. The implemented alternative verification path is the
/// optimistic Naysayer scheme (`LASNaysayer.sol`), priced in `LasOptimisticClaimGas`.
abstract contract SwapGasBase {
    Vm constant vm = Vm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);
    uint256 constant N = 256;

    /// One leg's value. Equal on both legs so no cost difference comes from the amount.
    uint256 constant COIN = 1 ether;

    uint256 constant U1_KEY = 0xA11CE; // witness holder; claims first, on leg B
    uint256 constant U2_KEY = 0xB0B2;  // reacts; claims second, on leg A

    AdaptorSwap chainA; // holds c1, claimable by u2 with u1's adapted signature
    AdaptorSwap chainB; // holds c2, claimable by u1 with u2's adapted signature

    address u1;
    address u2;

    /// t2 < t1, per §4.1: the leg claimed FIRST carries the SHORTER timeout.
    uint64 t1; // leg A, claimed second
    uint64 t2; // leg B, claimed first

    /// The pre-authorised claim digests. In a real swap these are the sighashes of tx1
    /// and tx2; here they only need to be fixed, distinct messages that the adapted
    /// signatures are over.
    bytes32 constant TX1_SIGHASH = keccak256("tx1: spend c1 to u2");
    bytes32 constant TX2_SIGHASH = keccak256("tx2: spend c2 to u1");

    function setUp() public virtual {
        chainA = new AdaptorSwap();
        chainB = new AdaptorSwap();
        u1 = vm.addr(U1_KEY);
        u2 = vm.addr(U2_KEY);
        vm.deal(address(this), 1000 ether);
        t2 = uint64(block.timestamp + 1 days);  // leg B, first claimed  — shorter
        t1 = uint64(block.timestamp + 2 days);  // leg A, second claimed — longer
        require(t2 < t1, "asymmetric timelock rule violated: need t2 < t1");
    }

    receive() external payable {}

    // ------------------------------------------------------------------ reporting

    /// Intrinsic transaction gas for `cd`: 21,000 plus EIP-2028 calldata pricing.
    function _intrinsic(bytes memory cd) internal pure returns (uint256 g) {
        g = 21_000;
        for (uint256 i = 0; i < cd.length; i++) {
            g += cd[i] == 0 ? 4 : 16;
        }
    }

    /// One table row: what a real transaction for this step would be charged.
    function _row(string memory label, uint256 execGas, bytes memory cd) internal view {
        uint256 intrinsic = _intrinsic(cd);
        console.log(label);
        console.log("  calldata bytes", cd.length);
        console.log("  exec / intrinsic / TOTAL gas", execGas, intrinsic, execGas + intrinsic);
    }

    /// A step that costs nothing on chain, recorded so the boundary is explicit rather
    /// than silently omitted.
    function _offChainRow(string memory label) internal view {
        console.log(label);
        console.log("  calldata bytes", 0);
        console.log("  exec / intrinsic / TOTAL gas", 0, 0, 0);
    }

    // ------------------------------------------------------------- classical legs

    function _classicalSig(uint256 key, bytes32 digest) internal pure returns (uint8 v, bytes32 r, bytes32 s) {
        return vm.sign(key, digest);
    }

    /// Leg A: u1 escrows c1 for u2, redeemable with u1's adapted signature over tx1.
    function _fundClassicalLegA() internal returns (uint256 id) {
        id = chainA.fundClassical{value: COIN}(payable(u2), u1, TX1_SIGHASH, t1);
    }

    /// Leg B: u2 escrows c2 for u1, redeemable with u2's adapted signature over tx2.
    function _fundClassicalLegB() internal returns (uint256 id) {
        id = chainB.fundClassical{value: COIN}(payable(u1), u2, TX2_SIGHASH, t2);
    }

    // ------------------------------------------------------------------ LAS legs

    /// The real 6720-byte packed adapted signature exported from the C implementation.
    function _lasSig() internal view returns (bytes memory sig) {
        sig = vm.readFileBinary("test/las_sig.bin");
        require(sig.length == 6720, "expected 6720-byte packed LAS signature");
    }

    function _fundLasLegA() internal returns (uint256 id) {
        id = chainA.fundLAS{value: COIN}(payable(u2), t1);
    }

    function _fundLasLegB() internal returns (uint256 id) {
        id = chainB.fundLAS{value: COIN}(payable(u1), t2);
    }

    // ------------------------------------------- LAS with full native verification

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

    /// The golden verification context: A' (NTT domain), the public key t, the message,
    /// and the adapted signature that the C implementation accepts.
    function _verifiedInputs()
        internal
        view
        returns (uint256[][] memory AprimeHat, uint256[][] memory t, bytes memory message, bytes memory sig)
    {
        AprimeHat = LASVerify.toNttDomain(_readPolys("pp_normal.bin", 6 * 5));
        t = _readPolys("t.bin", 6);
        message = vm.readFileBinary("test/vectors/msg.bin");
        sig = vm.readFileBinary("test/vectors/sig.bin");
    }
}

/// Step 1–2: funding both legs. Identical work for both schemes except that the LAS
/// escrow stores no signer/claim-digest, so its row is the cheaper one — funding is not
/// where the post-quantum cost lands.
contract SwapFundingGas is SwapGasBase {
    function test_gas_fund_classical_legA() public {
        bytes memory cd = abi.encodeCall(AdaptorSwap.fundClassical, (payable(u2), u1, TX1_SIGHASH, t1));
        uint256 g = gasleft();
        uint256 id = chainA.fundClassical{value: COIN}(payable(u2), u1, TX1_SIGHASH, t1);
        uint256 used = g - gasleft();
        require(address(chainA).balance == COIN && id == 0, "leg A not funded");
        _row("classical | fund leg A (u1 escrows c1 for u2, timeout t1 = LONGER)", used, cd);
    }

    function test_gas_fund_classical_legB() public {
        bytes memory cd = abi.encodeCall(AdaptorSwap.fundClassical, (payable(u1), u2, TX2_SIGHASH, t2));
        uint256 g = gasleft();
        chainB.fundClassical{value: COIN}(payable(u1), u2, TX2_SIGHASH, t2);
        uint256 used = g - gasleft();
        require(address(chainB).balance == COIN, "leg B not funded");
        _row("classical | fund leg B (u2 escrows c2 for u1, timeout t2 = SHORTER)", used, cd);
    }

    function test_gas_fund_las_legA() public {
        bytes memory cd = abi.encodeCall(AdaptorSwap.fundLAS, (payable(u2), t1));
        uint256 g = gasleft();
        chainA.fundLAS{value: COIN}(payable(u2), t1);
        uint256 used = g - gasleft();
        _row("LAS       | fund leg A (u1 escrows c1 for u2, timeout t1 = LONGER)", used, cd);
    }

    function test_gas_fund_las_legB() public {
        bytes memory cd = abi.encodeCall(AdaptorSwap.fundLAS, (payable(u1), t2));
        uint256 g = gasleft();
        chainB.fundLAS{value: COIN}(payable(u1), t2);
        uint256 used = g - gasleft();
        _row("LAS       | fund leg B (u2 escrows c2 for u1, timeout t2 = SHORTER)", used, cd);
    }
}

/// Step 3: the FIRST claim — u1 publishes the adapted signature on leg B and takes c2.
/// This is the step that leaks the witness, and the step whose verification cost is the
/// whole post-quantum question.
contract FirstClaimGas is SwapGasBase {
    uint256 idB;

    function setUp() public override {
        super.setUp();
        idB = chainB.fundClassical{value: COIN}(payable(u1), u2, TX2_SIGHASH, t2);
    }

    function test_gas_firstClaim_classical() public {
        (uint8 v, bytes32 r, bytes32 s) = _classicalSig(U2_KEY, TX2_SIGHASH);
        bytes memory cd = abi.encodeCall(AdaptorSwap.claimClassical, (idB, v, r, s));

        uint256 before = u1.balance;
        uint256 g = gasleft();
        chainB.claimClassical(idB, v, r, s);
        uint256 used = g - gasleft();

        require(u1.balance == before + COIN, "u1 did not receive c2");
        _row("classical | first claim on leg B: u1 takes c2 (ecrecover precompile)", used, cd);
        _offChainRow("both      | Ext: u2 recovers y from the published signature (OFF-CHAIN)");
    }
}

/// Step 3, post-quantum variants of the same claim, at the same boundary.
contract FirstClaimLasGas is SwapGasBase {
    uint256 idB;

    function setUp() public override {
        super.setUp();
        idB = chainB.fundLAS{value: COIN}(payable(u1), t2);
    }

    /// The settlement FLOOR: calldata for the 6720-byte signature plus one keccak256, and
    /// NO lattice verification. A strict lower bound on any real LAS settlement.
    function test_gas_firstClaim_las_floor() public {
        bytes memory sig = _lasSig();
        bytes memory cd = abi.encodeCall(AdaptorSwap.claimLAS, (idB, sig));

        uint256 before = u1.balance;
        uint256 g = gasleft();
        chainB.claimLAS(idB, sig);
        uint256 used = g - gasleft();

        require(u1.balance == before + COIN, "u1 did not receive c2");
        _row("LAS floor | first claim on leg B: u1 takes c2 (NO verification)", used, cd);
    }
}

/// Step 3 with COMPLETE native LAS verification — the honest post-quantum settlement,
/// and the row that decides deployability.
contract FirstClaimLasVerifiedGas is SwapGasBase {
    uint256 idB;

    function setUp() public override {
        super.setUp();
        (uint256[][] memory AprimeHat, uint256[][] memory t, bytes memory message,) = _verifiedInputs();
        bytes32 ctx = keccak256(abi.encode(AprimeHat, t, message));
        idB = chainB.fundLASVerified{value: COIN}(payable(u1), t2, ctx);
    }

    function test_gas_firstClaim_las_verified() public {
        (uint256[][] memory AprimeHat, uint256[][] memory t, bytes memory message, bytes memory sig) =
            _verifiedInputs();
        bytes memory cd = abi.encodeCall(AdaptorSwap.claimLASVerified, (idB, sig, AprimeHat, t, message));

        uint256 before = u1.balance;
        uint256 g = gasleft();
        chainB.claimLASVerified(idB, sig, AprimeHat, t, message);
        uint256 used = g - gasleft();

        require(u1.balance == before + COIN, "u1 did not receive c2");
        _row("LAS full  | first claim on leg B: u1 takes c2 (native base_verify in Solidity)", used, cd);
    }
}

/// Step 4: the SECOND claim — u2, having extracted the witness off-chain, adapts the
/// other pre-signature and takes c1 on leg A. Measured on a chain whose first leg has
/// already settled, so the numbers are those of a real second transaction.
contract SecondClaimGas is SwapGasBase {
    uint256 idA;

    function setUp() public override {
        super.setUp();
        // Leg B settles first (u1 takes c2), exactly as in Fig. 1.
        uint256 idB = chainB.fundClassical{value: COIN}(payable(u1), u2, TX2_SIGHASH, t2);
        (uint8 v, bytes32 r, bytes32 s) = _classicalSig(U2_KEY, TX2_SIGHASH);
        chainB.claimClassical(idB, v, r, s);
        // Leg A is funded and waiting for u2's claim.
        idA = chainA.fundClassical{value: COIN}(payable(u2), u1, TX1_SIGHASH, t1);
    }

    function test_gas_secondClaim_classical() public {
        (uint8 v, bytes32 r, bytes32 s) = _classicalSig(U1_KEY, TX1_SIGHASH);
        bytes memory cd = abi.encodeCall(AdaptorSwap.claimClassical, (idA, v, r, s));

        uint256 before = u2.balance;
        uint256 g = gasleft();
        chainA.claimClassical(idA, v, r, s);
        uint256 used = g - gasleft();

        require(u2.balance == before + COIN, "u2 did not receive c1");
        _row("classical | second claim on leg A: u2 takes c1 (ecrecover precompile)", used, cd);
    }
}

contract SecondClaimLasGas is SwapGasBase {
    uint256 idA;

    function setUp() public override {
        super.setUp();
        uint256 idB = chainB.fundLAS{value: COIN}(payable(u1), t2);
        chainB.claimLAS(idB, vm.readFileBinary("test/las_sig.bin"));
        idA = chainA.fundLAS{value: COIN}(payable(u2), t1);
    }

    function test_gas_secondClaim_las_floor() public {
        bytes memory sig = _lasSig();
        bytes memory cd = abi.encodeCall(AdaptorSwap.claimLAS, (idA, sig));

        uint256 before = u2.balance;
        uint256 g = gasleft();
        chainA.claimLAS(idA, sig);
        uint256 used = g - gasleft();

        require(u2.balance == before + COIN, "u2 did not receive c1");
        _row("LAS floor | second claim on leg A: u2 takes c1 (NO verification)", used, cd);
    }
}

/// The refund branches, under the asymmetric timelocks. These are the paths that make a
/// stalled swap safe, and they are priced here because a deployability argument that
/// ignores the failure branch is incomplete.
contract RefundGas is SwapGasBase {
    uint256 idA;
    uint256 idB;

    function setUp() public override {
        super.setUp();
        idA = chainA.fundClassical{value: COIN}(payable(u2), u1, TX1_SIGHASH, t1);
        idB = chainB.fundClassical{value: COIN}(payable(u1), u2, TX2_SIGHASH, t2);
    }

    function test_gas_refund_legB_after_t2() public {
        vm.warp(uint256(t2));
        bytes memory cd = abi.encodeCall(AdaptorSwap.refund, (idB));

        uint256 g = gasleft();
        chainB.refund(idB);
        uint256 used = g - gasleft();

        require(address(chainB).balance == 0, "leg B not refunded");
        _row("both      | refund leg B after t2 (payer reclaims c2)", used, cd);
    }

    function test_gas_refund_legA_after_t1() public {
        vm.warp(uint256(t1));
        bytes memory cd = abi.encodeCall(AdaptorSwap.refund, (idA));

        uint256 g = gasleft();
        chainA.refund(idA);
        uint256 used = g - gasleft();

        require(address(chainA).balance == 0, "leg A not refunded");
        _row("both      | refund leg A after t1 (payer reclaims c1)", used, cd);
    }
}

/// The safety window itself: between t2 and t1 the first-claimed leg is already
/// refundable while the second is not, which is precisely what gives u2 time to react to
/// the witness u1 revealed. Asserted rather than assumed, because the whole asymmetric
/// timelock rule is worthless if the ordering is set the wrong way round.
contract SwapSafetyWindowGas is SwapGasBase {
    uint256 idA;
    uint256 idB;

    function setUp() public override {
        super.setUp();
        idA = chainA.fundClassical{value: COIN}(payable(u2), u1, TX1_SIGHASH, t1);
        idB = chainB.fundClassical{value: COIN}(payable(u1), u2, TX2_SIGHASH, t2);
    }

    function test_safetyWindow_legA_still_locked_when_legB_refundable() public {
        vm.warp(uint256(t2));

        // Leg B: refundable now.
        chainB.refund(idB);
        require(address(chainB).balance == 0, "leg B should be refundable at t2");

        // Leg A: still locked, so u2 retains the window to claim c1 with the witness.
        (bool ok,) = address(chainA).call(abi.encodeCall(AdaptorSwap.refund, (idA)));
        require(!ok, "leg A must NOT be refundable before t1 -- u2's safety window is gone");

        // And it opens later, so the funds are never stranded.
        vm.warp(uint256(t1));
        chainA.refund(idA);
        require(address(chainA).balance == 0, "leg A should be refundable at t1");
    }
}

/// The implemented alternative verification path: optimistic (Naysayer) settlement of the
/// same claim. Honest settlement is cheap because verification is *asserted* and only
/// disputed if someone objects; the cost of that shift is a challenge window before the
/// payout finalises, which is a latency and liveness cost rather than a gas one.
contract LasOptimisticClaimGas is SwapGasBase {
    LASNaysayerSwap nay;
    uint256 id;

    function setUp() public override {
        super.setUp();
        nay = new LASNaysayerSwap();
        bytes32 ioCommit = keccak256(abi.encode(_readPolys("t.bin", 6), vm.readFileBinary("test/vectors/msg.bin")));
        bytes32 aprimeCommit = keccak256(abi.encode(_readPolys("pp_normal.bin", 30)));
        // `optimisticClaim` may only be called by the beneficiary, so this test contract
        // stands in for u1 (the claimer) rather than using u1's EOA. `fund` additionally
        // requires timeout > now + CHALLENGE_PERIOD, which t2 satisfies.
        id = nay.fund{value: COIN}(payable(address(this)), t2, ioCommit, aprimeCommit);
    }

    function test_gas_firstClaim_las_optimistic() public {
        bytes memory sig = vm.readFileBinary("test/vectors/sig.bin");
        uint256[][] memory wprime = _readPolys("w_prime.bin", 6);
        uint256 bond = nay.MIN_BOND();

        bytes memory cd = abi.encodeCall(LASNaysayerSwap.optimisticClaim, (id, sig, wprime));
        uint256 g = gasleft();
        nay.optimisticClaim{value: bond}(id, sig, wprime);
        uint256 used = g - gasleft();
        _row("LAS optim | first claim, optimistic assertion (no verification on chain)", used, cd);

        vm.warp(block.timestamp + nay.CHALLENGE_PERIOD());

        bytes memory cdFin = abi.encodeCall(LASNaysayerSwap.finalize, (id));
        uint256 g2 = gasleft();
        nay.finalize(id);
        uint256 usedFin = g2 - gasleft();

        require(nay.credits(address(this)) == COIN + bond, "claimer not credited on finalize");
        _row("LAS optim | finalize after the challenge period (payout credited)", usedFin, cdFin);
    }
}
