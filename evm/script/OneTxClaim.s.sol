// SPDX-License-Identifier: MIT
pragma solidity ^0.8.25;

import {AdaptorSwap} from "../src/AdaptorSwap.sol";
import {LASRegister} from "../src/LASRegister.sol";
import {LASVerifyOpt} from "../src/LASVerifierOpt.sol";

/// @title OneTxClaim — set up a real, funded escrow on a real node, and emit the claim
///        calldata for a separately-sent, gas-capped transaction.
///
/// WHY THIS EXISTS, given the Foundry tests already assert the cap. Everything in
/// `test/LASGasBreakdown.t.sol` is a MODEL: `gasleft()` deltas plus an EIP-7623 formula
/// written in Solidity by the same person who wrote the verifier. A model can be wrong
/// in the same direction twice. The claim "on-chain LAS verification fits in one
/// Ethereum transaction" is only really settled when an Ethereum CLIENT — not our
/// arithmetic — accepts the transaction under a gas limit at or below EIP-7825's
/// 16,777,216 and returns a successful receipt. That is what
/// `scripts/run_onchain_one_tx.sh` does with this script:
///
///   tx 1  deploy AdaptorSwap
///   tx 2  fundLASVerified — commits keccak256(A'^, t^, pack(t), M) at fund time
///   ----  (this script ends here; the escrow now exists in COMMITTED node state)
///   tx 3  claimLASVerifiedOpt, sent by `cast send --gas-limit 16777216`
///
/// The split matters twice over. It is what makes tx 3's storage reads COLD, as the
/// project's gas methodology requires; and it is what makes tx 3 a standalone
/// transaction that a node either accepts under the cap or rejects — which is the whole
/// claim, in the only form that is not self-reported.
///
/// Run against `anvil --enable-tx-gas-limit`, which enforces EIP-7825 node-side, so an
/// over-cap transaction is refused by the client rather than by us.
contract OneTxClaim {
    Vm constant vm = Vm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);
    uint256 constant N = 256;
    uint256 constant COIN = 1 ether;

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

    function run() external {
        uint256 pk = vm.envUint("PRIVATE_KEY");
        address payable beneficiary = payable(vm.envAddress("BENEFICIARY"));

        bytes memory aHatP = LASRegister.packNtt(_readPolys("pp_normal.bin", 30));
        bytes memory tHatP = LASRegister.packNtt(_readPolys("t.bin", 6));
        bytes memory tPacked = vm.readFileBinary("test/vectors/t.bin");
        bytes memory message = vm.readFileBinary("test/vectors/msg.bin");
        bytes memory sig = vm.readFileBinary("test/vectors/sig.bin");

        require(tPacked.length == LASVerifyOpt.TPACK_BYTES, "pack(t) width");
        require(aHatP.length == LASVerifyOpt.AHAT_BYTES, "A' width");
        require(tHatP.length == LASVerifyOpt.THAT_BYTES, "t^ width");
        require(sig.length == LASVerifyOpt.SIG_BYTES, "signature width");

        vm.startBroadcast(pk);
        AdaptorSwap swapChain = new AdaptorSwap();
        uint256 id = swapChain.fundLASVerified{value: COIN}(
            beneficiary, uint64(block.timestamp + 1 days), LASRegister.context(aHatP, tHatP, tPacked, message)
        );
        vm.stopBroadcast();

        // Hand the claim to the shell rather than broadcasting it here: only `cast send`
        // can pin an explicit --gas-limit, and an explicit cap is the point.
        bytes memory cd = abi.encodeWithSelector(
            AdaptorSwap.claimLASVerifiedOpt.selector, id, sig, aHatP, tHatP, tPacked, message
        );
        vm.writeFile("onetx/swap.addr", vm.toString(address(swapChain)));
        vm.writeFile("onetx/beneficiary.addr", vm.toString(beneficiary));
        vm.writeFile("onetx/claim.calldata", vm.toString(cd));
        vm.writeFile("onetx/claim.calldata.len", vm.toString(cd.length));
        // The escrowed amount, so the shell asserts the EXACT payout rather than
        // "the balance went up" — a partial or wrong-recipient transfer must fail.
        vm.writeFile("onetx/coin.wei", vm.toString(COIN));
    }
}

/// Minimal Foundry cheatcode interface (the repo deliberately avoids a forge-std dependency).
interface Vm {
    function readFileBinary(string calldata path) external view returns (bytes memory);
    function writeFile(string calldata path, string calldata data) external;
    function envUint(string calldata name) external view returns (uint256);
    function envAddress(string calldata name) external view returns (address);
    function startBroadcast(uint256 privateKey) external;
    function stopBroadcast() external;
    function toString(address value) external pure returns (string memory);
    function toString(bytes calldata value) external pure returns (string memory);
    function toString(uint256 value) external pure returns (string memory);
}
