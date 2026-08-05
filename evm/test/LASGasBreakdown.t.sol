// SPDX-License-Identifier: MIT
pragma solidity ^0.8.25;

import {AdaptorSwap} from "../src/AdaptorSwap.sol";
import {LASVerify} from "../src/LASVerifier.sol";
import {LASVerifyOpt} from "../src/LASVerifierOpt.sol";
import {LASShake} from "../src/LASShake.sol";
import {LASRegister, LASTxGas} from "../src/LASRegister.sol";
import {nttFw} from "../lib/zknox/ZKNOX_NTT_dilithium.sol";
import {console} from "./TwoLegSwapGas.t.sol";

/// @title LASGasBreakdown — where the gas goes in on-chain LAS verification, and whether
///        it fits in ONE Ethereum transaction.
///
/// EIP-7825 caps a single transaction at 16,777,216 gas (2²⁴). The vendored-primitive
/// verifier `LASVerify` measures ≈56.6M execution plus ≈1.65M intrinsic — ≈3.6× the cap —
/// which is the measured evidence behind the Meeting-7 pivot to Bitcoin/UTXO. This test
/// does two things:
///
///   1. ATTRIBUTES that cost stage by stage, so the optimisation is driven by measurement
///      rather than by intuition about which loop looks expensive. (The earlier op-budget
///      probe `LASVerifyCost.sol` priced only the polynomial ARITHMETIC and modelled the
///      hash at a published per-permutation figure; the split below is measured.)
///   2. GATES the optimised path: `test_optimised_fits_in_one_transaction` FAILS if the
///      total a real transaction would be charged — execution plus 21,000 plus EIP-2028
///      calldata — is not strictly under the cap. Like the rejection-rate gate on the
///      C/Rust benchmark drivers, this exists so a regression fails loudly instead of
///      quietly producing a publishable-looking number.
///
/// GAS ACCOUNTING matches `TwoLegSwapGas.t.sol` exactly so the rows are comparable:
/// `gasleft()` deltas are EXECUTION only, and every headline total adds 21,000 plus
/// 16 gas per non-zero and 4 per zero calldata byte. Both verifiers are driven through
/// a real external call carrying real calldata, so ABI decoding is charged to whichever
/// verifier incurs it — that is a genuine part of the difference, not an artefact.
///
/// forge treats a revert as failure; no forge-std dependency.
contract LASGasBreakdownTest {
    Vm constant vm = Vm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);
    uint256 constant N = 256;

    /// EIP-7825: the ceiling this whole exercise is against.
    uint256 constant TX_GAS_CAP = 16_777_216;

    OldVerifyHarness oldH;
    NewVerifyHarness newH;
    StageHarness stages;

    function setUp() public {
        oldH = new OldVerifyHarness();
        newH = new NewVerifyHarness();
        stages = new StageHarness();
    }

    // ---- inputs -------------------------------------------------------------

    function _readPolys(string memory name, uint256 count) internal view returns (uint256[][] memory polys) {
        bytes memory raw = vm.readFileBinary(string.concat("test/vectors/", name));
        require(raw.length == count * N * 4, "bad polys file length");
        polys = new uint256[][](count);
        uint256 off = 0;
        for (uint256 i = 0; i < count; i++) {
            polys[i] = new uint256[](N);
            for (uint256 k = 0; k < N; k++) {
                polys[i][k] = uint256(uint8(raw[off])) | (uint256(uint8(raw[off + 1])) << 8)
                    | (uint256(uint8(raw[off + 2])) << 16) | (uint256(uint8(raw[off + 3])) << 24);
                off += 4;
            }
        }
    }

    function _packBE(uint256[][] memory polys) internal pure returns (bytes memory out) {
        out = new bytes(polys.length * N * 4);
        uint256 o = 0;
        for (uint256 i = 0; i < polys.length; i++) {
            for (uint256 k = 0; k < N; k++) {
                uint256 x = polys[i][k];
                out[o] = bytes1(uint8(x >> 24));
                out[o + 1] = bytes1(uint8(x >> 16));
                out[o + 2] = bytes1(uint8(x >> 8));
                out[o + 3] = bytes1(uint8(x));
                o += 4;
            }
        }
    }

    function _toNtt(uint256[][] memory polys) internal pure returns (uint256[][] memory out) {
        out = new uint256[][](polys.length);
        for (uint256 i = 0; i < polys.length; i++) {
            uint256[] memory p = new uint256[](N);
            for (uint256 k = 0; k < N; k++) {
                p[k] = polys[i][k];
            }
            out[i] = nttFw(p);
        }
    }

    /// What a node charges: EIP-7623's max(standard, floor), NOT EIP-2028. See
    /// `LASTxGas` for why the distinction is load-bearing for a calldata-heavy claim.
    function _txGas(bytes memory cd, uint256 execGas) internal pure returns (uint256 gas, bool floorBinds) {
        return LASTxGas.total(cd, execGas);
    }

    /// Execution gas of a staticcall carrying exactly `cd`, plus the returned bool.
    function _measure(address target, bytes memory cd) internal view returns (uint256 execGas, bool verdict) {
        uint256 before = gasleft();
        (bool okCall, bytes memory ret) = target.staticcall(cd);
        execGas = before - gasleft();
        require(okCall, "verifier call reverted");
        verdict = abi.decode(ret, (bool));
    }

    // ---- the two whole-transaction totals ------------------------------------

    function test_breakdown_and_totals() public view {
        bytes memory message = vm.readFileBinary("test/vectors/msg.bin");
        bytes memory sig = vm.readFileBinary("test/vectors/sig.bin");

        // --- vendored-primitive verifier, driven exactly as claimLASVerified drives it
        uint256[][] memory AprimeHat = LASVerify.toNttDomain(_readPolys("pp_normal.bin", 30));
        uint256[][] memory t = _readPolys("t.bin", 6);
        bytes memory cdOld =
            abi.encodeWithSelector(OldVerifyHarness.run.selector, AprimeHat, t, message, sig);
        (uint256 execOld, bool okOld) = _measure(address(oldH), cdOld);
        require(okOld, "baseline verifier rejected the golden signature");
        (uint256 totalOld, bool floorOld) = _txGas(cdOld, execOld);

        // --- optimised verifier
        bytes memory aHatP = _packBE(_toNtt(_readPolys("pp_normal.bin", 30)));
        bytes memory tHatP = _packBE(_toNtt(_readPolys("t.bin", 6)));
        bytes memory tPacked = vm.readFileBinary("test/vectors/t.bin");
        bytes memory cdNew =
            abi.encodeWithSelector(NewVerifyHarness.run.selector, aHatP, tHatP, tPacked, message, sig);
        (uint256 execNew, bool okNew) = _measure(address(newH), cdNew);
        require(okNew, "optimised verifier rejected the golden signature");
        (uint256 totalNew, bool floorNew) = _txGas(cdNew, execNew);

        console.log("EIP-7825 per-transaction gas cap", TX_GAS_CAP);
        console.log("");
        console.log("BASELINE  LASVerify (vendored primitives)");
        console.log("  calldata bytes / tokens", cdOld.length, LASTxGas.tokens(cdOld));
        console.log("  exec / EIP-7623 TOTAL", execOld, totalOld);
        console.log("  EIP-7623 floor binds (1=yes)", floorOld ? 1 : 0);
        console.log("OPTIMISED LASVerifyOpt");
        console.log("  calldata bytes / tokens", cdNew.length, LASTxGas.tokens(cdNew));
        console.log("  exec / EIP-7623 TOTAL", execNew, totalNew);
        console.log("  EIP-7623 floor binds (1=yes)", floorNew ? 1 : 0);
        console.log("  TOTAL as percent of the cap", (totalNew * 100) / TX_GAS_CAP);

        // Stage attribution for the baseline: the two halves it exposes.
        uint256 gArith = stages.oldArithmetic(AprimeHat, t, sig);
        uint256 gHash = stages.oldOracle(t, _readPolys("w_prime.bin", 6), message);
        console.log("");
        console.log("BASELINE split: arithmetic(decode+SampleInBall+24 NTT+pointwise)", gArith);
        console.log("BASELINE split: pack(t)+pack(w')+SHAKE256 oracle", gHash);

        // Stage attribution for the optimised path.
        uint256[6] memory g = stages.newStages(aHatP, tHatP, tPacked, message, sig);
        console.log("");
        console.log("OPTIMISED stage: decode z + norm gate", g[0]);
        console.log("OPTIMISED stage: SampleInBall (challenge)", g[1]);
        console.log("OPTIMISED stage: 6 forward NTTs (z_bot, c)", g[2]);
        console.log("OPTIMISED stage: w' = pointwise + 6 inverse NTTs + pack", g[3]);
        console.log("OPTIMISED stage: SHAKE256 over the 12,320-byte preimage", g[4]);
        console.log("OPTIMISED stage: preimage assembly (calldatacopy t, M)", g[5]);
    }

    /// THE GATE. A single Ethereum transaction may spend at most 16,777,216 gas
    /// (EIP-7825). Execution plus intrinsic must be strictly under it, or on-chain LAS
    /// verification is once again not a single transaction and the claim must be
    /// withdrawn from the report.
    function test_optimised_fits_in_one_transaction() public view {
        bytes memory message = vm.readFileBinary("test/vectors/msg.bin");
        bytes memory sig = vm.readFileBinary("test/vectors/sig.bin");
        bytes memory aHatP = _packBE(_toNtt(_readPolys("pp_normal.bin", 30)));
        bytes memory tHatP = _packBE(_toNtt(_readPolys("t.bin", 6)));
        bytes memory tPacked = vm.readFileBinary("test/vectors/t.bin");

        bytes memory cd = abi.encodeWithSelector(NewVerifyHarness.run.selector, aHatP, tHatP, tPacked, message, sig);
        (uint256 execGas, bool verdict) = _measure(address(newH), cd);
        require(verdict, "optimised verifier rejected the golden signature");

        (uint256 total, bool floorBinds) = _txGas(cd, execGas);
        console.log("one-transaction TOTAL (EIP-7623)", total);
        console.log("EIP-7825 per-transaction cap", TX_GAS_CAP);
        console.log("EIP-7623 floor binds (1=yes)", floorBinds ? 1 : 0);
        require(total < TX_GAS_CAP, "on-chain LAS verification does NOT fit in one transaction");
    }

    /// The baseline must still be over the cap — the negative result this work is
    /// measured against. If this ever stops holding, the "3.6x over" claim is stale.
    function test_baseline_still_exceeds_the_cap() public view {
        bytes memory message = vm.readFileBinary("test/vectors/msg.bin");
        bytes memory sig = vm.readFileBinary("test/vectors/sig.bin");
        uint256[][] memory AprimeHat = LASVerify.toNttDomain(_readPolys("pp_normal.bin", 30));
        uint256[][] memory t = _readPolys("t.bin", 6);

        bytes memory cd = abi.encodeWithSelector(OldVerifyHarness.run.selector, AprimeHat, t, message, sig);
        (uint256 execGas,) = _measure(address(oldH), cd);
        (uint256 total,) = _txGas(cd, execGas);
        require(total > TX_GAS_CAP, "baseline no longer exceeds the cap");
    }
}

// ---- harnesses ------------------------------------------------------------

contract OldVerifyHarness {
    function run(uint256[][] calldata AprimeHat, uint256[][] calldata t, bytes calldata message, bytes calldata sig)
        external
        pure
        returns (bool)
    {
        return LASVerify.verify(AprimeHat, t, message, sig);
    }
}

contract NewVerifyHarness {
    function run(
        bytes calldata aHatPacked,
        bytes calldata tHatPacked,
        bytes calldata tPacked,
        bytes calldata message,
        bytes calldata sig
    ) external pure returns (bool) {
        return LASVerifyOpt.verify(aHatPacked, tHatPacked, tPacked, message, sig);
    }
}

/// Stage-level metering. Each stage is measured where it actually runs — inside a
/// calldata-receiving external call — so no stage is flattered by having its inputs
/// already in memory.
contract StageHarness {
    function oldArithmetic(uint256[][] calldata AprimeHat, uint256[][] calldata t, bytes calldata sig)
        external
        view
        returns (uint256 used)
    {
        uint256 s = gasleft();
        LASVerify.computeWPrime(AprimeHat, t, sig);
        used = s - gasleft();
    }

    function oldOracle(uint256[][] calldata t, uint256[][] calldata wprime, bytes calldata message)
        external
        view
        returns (uint256 used)
    {
        uint256 s = gasleft();
        LASVerify.oracle(t, wprime, message);
        used = s - gasleft();
    }

    function newStages(
        bytes calldata aHatPacked,
        bytes calldata tHatPacked,
        bytes calldata tPacked,
        bytes calldata message,
        bytes calldata sig
    ) external view returns (uint256[6] memory g) {
        uint256 aOff;
        uint256 tOff;
        assembly {
            aOff := aHatPacked.offset
            tOff := tHatPacked.offset
        }

        uint256 s = gasleft();
        (uint256[][] memory z, bool ok) = LASVerifyOpt._decodeZ(sig);
        g[0] = s - gasleft();
        require(ok, "golden signature failed the norm gate");

        s = gasleft();
        uint256[] memory c = LASVerifyOpt._sampleInBall(sig);
        g[1] = s - gasleft();

        s = gasleft();
        uint256[] memory cHat = nttFw(c);
        for (uint256 j = 0; j < 5; j++) {
            z[6 + j] = nttFw(z[6 + j]);
        }
        g[2] = s - gasleft();

        s = gasleft();
        bytes memory pre = new bytes(12288 + message.length);
        uint256 prePtr;
        assembly {
            prePtr := add(pre, 32)
            calldatacopy(prePtr, tPacked.offset, 6144)
            calldatacopy(add(prePtr, 12288), message.offset, message.length)
        }
        g[5] = s - gasleft();

        s = gasleft();
        LASVerifyOpt._wprimeInto(prePtr + 6144, aOff, tOff, z, cHat);
        g[3] = s - gasleft();

        s = gasleft();
        uint256 p = LASShake.init();
        LASShake.absorbPad(p, prePtr, pre.length);
        g[4] = s - gasleft();
    }
}

/// @title The single-transaction claim, priced at the SWAP level.
///
/// `LASGasBreakdownTest` prices the verifier alone; this prices the whole settlement the
/// way `TwoLegSwapGas.t.sol` prices `claimLASVerified` — same escrow, same storage writes,
/// same transfer, only the verifier differs. It is the TIGHTER of the two numbers (the
/// escrow work sits on top of verification), so it is the one that must be gated.
///
/// SPLIT INTO TWO SUBCLASSES, for a reason that is not cosmetic. This measurement has to
/// appear in `forge test --gas-report` so the `claimLASVerifiedOpt` row exists for
/// `scripts/plot_onchain_gas.py` — but `--gas-report` INFLATES every in-test gas
/// measurement (`gasleft()` deltas and `vm.lastCallGas()` alike, ~688k gas on this call)
/// because Foundry's inspector is metered inside the measured frame, and that is MORE
/// than the real headroom under the EIP-7825 cap. An assertion measured under the flag
/// would therefore fail for a reporting reason while a real client mines the very same
/// transaction. So:
///
///   • `LasVerifiedOptSwapGas`  — reports only; runs in the --gas-report pass, no gate.
///   • `LasVerifiedOptSwapGate` — asserts the cap; run WITHOUT --gas-report by
///                                scripts/run_onchain_gas.sh, so it cannot be falsified.
///
/// Both inherit the same cold-state setup and the same measurement, so they cannot drift.
/// The authoritative gate remains scripts/run_onchain_one_tx.sh, which takes gasUsed from
/// a real client receipt with no inspector in the frame at all.
abstract contract LasVerifiedOptSwapBase {
    Vm constant vm = Vm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);
    uint256 constant N = 256;
    uint256 constant COIN = 1 ether;
    uint256 constant TX_GAS_CAP = 16_777_216;

    AdaptorSwap swapChain;
    address payable beneficiary;
    uint256 swapId;

    /// COLD STATE, per `TwoLegSwapGas.t.sol`'s methodology note. Foundry runs `setUp`
    /// as its OWN transaction, so funding here means the measured claim below is the
    /// first access of every `swaps[id]` slot it touches and pays the 2,100-gas COLD
    /// SLOAD, not the 100-gas warm one. Funding inside the measured test would have
    /// flattered the claim by pre-warming the escrow it then reads — the exact
    /// discount this project's own gas methodology exists to exclude.
    function setUp() public {
        swapChain = new AdaptorSwap();
        beneficiary = payable(address(uint160(0xBEEF)));
        vm.deal(address(this), 100 ether);

        (bytes memory aHatP, bytes memory tHatP, bytes memory tPacked, bytes memory message,) = _inputs();
        swapId = swapChain.fundLASVerified{value: COIN}(
            beneficiary,
            uint64(block.timestamp + 1 days),
            LASRegister.context(aHatP, tHatP, tPacked, message)
        );
    }

    receive() external payable {}

    function _readPolys(string memory name, uint256 count) internal view returns (uint256[][] memory polys) {
        bytes memory raw = vm.readFileBinary(string.concat("test/vectors/", name));
        require(raw.length == count * N * 4, "bad polys file length");
        polys = new uint256[][](count);
        uint256 off = 0;
        for (uint256 i = 0; i < count; i++) {
            polys[i] = new uint256[](N);
            for (uint256 k = 0; k < N; k++) {
                polys[i][k] = uint256(uint8(raw[off])) | (uint256(uint8(raw[off + 1])) << 8)
                    | (uint256(uint8(raw[off + 2])) << 16) | (uint256(uint8(raw[off + 3])) << 24);
                off += 4;
            }
        }
    }

    /// The five packed calldata arguments. `LASRegister` owns both encodings so the
    /// funder, the tests and the Anvil script cannot drift apart on them.
    function _inputs()
        internal
        view
        returns (bytes memory aHatP, bytes memory tHatP, bytes memory tPacked, bytes memory message, bytes memory sig)
    {
        aHatP = LASRegister.packNtt(_readPolys("pp_normal.bin", 30));
        tHatP = LASRegister.packNtt(_readPolys("t.bin", 6));
        tPacked = vm.readFileBinary("test/vectors/t.bin");
        message = vm.readFileBinary("test/vectors/msg.bin");
        sig = vm.readFileBinary("test/vectors/sig.bin");
    }

    /// Fund, then claim, measuring what a real transaction would be charged.
    /// Returns the EIP-7623 total so both subclasses price the identical operation.
    function _measureClaim() internal returns (uint256 execGas, uint256 total, bool floorBinds, uint256 cdLen) {
        (bytes memory aHatP, bytes memory tHatP, bytes memory tPacked, bytes memory message, bytes memory sig) =
            _inputs();

        bytes memory cd = abi.encodeWithSelector(
            AdaptorSwap.claimLASVerifiedOpt.selector, swapId, sig, aHatP, tHatP, tPacked, message
        );
        uint256 before = gasleft();
        (bool okCall,) = address(swapChain).call(cd);
        execGas = before - gasleft();
        require(okCall, "claimLASVerifiedOpt reverted");
        require(beneficiary.balance == COIN, "beneficiary was not paid");

        (total, floorBinds) = LASTxGas.total(cd, execGas);
        cdLen = cd.length;
        console.log("LAS 1-tx | full on-chain verification, single transaction");
        console.log("  calldata bytes / tokens", cdLen, LASTxGas.tokens(cd));
        console.log("  exec / EIP-7623 TOTAL / cap", execGas, total, TX_GAS_CAP);
        console.log("  EIP-7623 floor binds (1=yes)", floorBinds ? 1 : 0);
    }
}

/// Reporting only — supplies the `claimLASVerifiedOpt` row to `--gas-report`. Carries no
/// assertion ON PURPOSE; see the base contract's header.
contract LasVerifiedOptSwapGas is LasVerifiedOptSwapBase {
    function test_gas_claim_las_verified_opt() public {
        _measureClaim();
    }
}

/// THE SWAP-LEVEL GATE. Same operation, same cold state, but asserted — and run without
/// `--gas-report`, so the inspector cannot inflate it past the cap. This is the tighter
/// of the two modelled gates: verification plus the escrow's own storage and transfer.
contract LasVerifiedOptSwapGate is LasVerifiedOptSwapBase {
    function test_swap_settlement_fits_in_one_transaction() public {
        (, uint256 total,,) = _measureClaim();
        require(total < TX_GAS_CAP, "swap settlement does not fit in one transaction");
    }
}

interface Vm {
    function readFileBinary(string calldata path) external view returns (bytes memory);
    function deal(address who, uint256 newBalance) external;
}
