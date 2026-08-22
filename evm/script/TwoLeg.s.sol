// SPDX-License-Identifier: MIT
pragma solidity ^0.8.25;

import {AdaptorSwapBound} from "../src/AdaptorSwapBound.sol";
import {LASRegister} from "../src/LASRegister.sol";
import {LASVerifyOpt} from "../src/LASVerifierOpt.sol";

/// @title TwoLeg — deploy and fund ONE leg of a two-leg atomic swap, and later build that
///        leg's claim calldata.
///
/// WHY TWO CONTRACTS AND NOT ONE RUN. eprint 2020/845 Fig. 1 has an unavoidable ordering:
/// the message each leg is signed over is `AdaptorSwapBound.legMessage(id)`, and `id` does
/// not exist until the funding transaction is MINED. So funding must finish, the runner
/// must read the digest back off the chain, the C side must pre-sign over it, and only
/// then can a claim be built. `TwoLegFund` is the first half, `TwoLegClaim` the second.
///
/// WHY EACH LEG GETS ITS OWN SCRIPT INVOCATION ON ITS OWN NODE. The swap exchanges two
/// different coins on two different chains. Two escrows on one chain would settle without
/// ever exercising the property that makes the protocol interesting — that u2 learns the
/// witness only from what u1 published on the OTHER chain. The runner therefore starts two
/// anvil instances with different chain ids and runs this script once against each.
///
/// The leg's own public key differs too: leg A is pre-signed under u1's key, leg B under
/// u2's, so `T_FILE` selects `t1.bin` or `t2.bin`. The registered context pins that key,
/// which is what stops a claimer substituting one they hold a signature for.
contract TwoLegFund {
    Vm constant vm = Vm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);
    uint256 constant N = 256;

    function _readPolys(string memory path, uint256 count) internal view returns (uint256[][] memory polys) {
        bytes memory raw = vm.readFileBinary(path);
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
        string memory vecDir = vm.envString("VEC_DIR"); // e.g. test/vectors/swap
        string memory tFile = vm.envString("T_FILE"); // t1.bin | t2.bin
        string memory outDir = vm.envString("OUT_DIR"); // e.g. twoleg/chain1
        uint256 coin = vm.envUint("COIN_WEI");
        uint256 timeout = vm.envUint("TIMEOUT_SECS");

        string memory tPath = string.concat(vecDir, "/", tFile);
        bytes memory aHatP = LASRegister.packNtt(_readPolys(string.concat(vecDir, "/pp_normal.bin"), 30));
        bytes memory tHatP = LASRegister.packNtt(_readPolys(tPath, 6));
        bytes memory tPacked = vm.readFileBinary(tPath);

        require(aHatP.length == LASVerifyOpt.AHAT_BYTES, "A' width");
        require(tHatP.length == LASVerifyOpt.THAT_BYTES, "t^ width");
        require(tPacked.length == LASVerifyOpt.TPACK_BYTES, "pack(t) width");

        vm.startBroadcast(pk);
        AdaptorSwapBound swapc = new AdaptorSwapBound();
        uint256 id = swapc.fundLASBound{value: coin}(
            beneficiary, uint64(block.timestamp + timeout), LASRegister.contextBound(aHatP, tHatP, tPacked)
        );
        vm.stopBroadcast();

        vm.writeFile(string.concat(outDir, "/swap.addr"), vm.toString(address(swapc)));
        vm.writeFile(string.concat(outDir, "/escrow.id"), vm.toString(id));
        vm.writeFile(string.concat(outDir, "/beneficiary.addr"), vm.toString(beneficiary));
        vm.writeFile(string.concat(outDir, "/coin.wei"), vm.toString(coin));
        // The packed parameters, so the claim script rebuilds byte-identical calldata
        // without re-deriving them (a second derivation is a second chance to differ).
        vm.writeFile(string.concat(outDir, "/aHat.hex"), vm.toString(aHatP));
        vm.writeFile(string.concat(outDir, "/tHat.hex"), vm.toString(tHatP));
        vm.writeFile(string.concat(outDir, "/tPacked.hex"), vm.toString(tPacked));
        // What the contract will require the signature to be over. The runner ALSO reads
        // this straight from the node with `cast call`; the two must agree, and the runner
        // checks that they do. Writing it here alone would prove nothing — it is the
        // chain's answer that the parties must sign.
        vm.writeFile(string.concat(outDir, "/legmsg.expected"), vm.toString(swapc.legMessage(id)));
    }
}

/// Build one leg's claim calldata from a signature the runner supplies as a file.
///
/// The signature is read from disk rather than passed as an argument because for leg A it
/// is produced by `extract_and_adapt`, whose only input is the signature observed on the
/// other chain. Keeping the transport as files is what lets the runner assert that the
/// bytes which settled leg A descend from bytes it read out of leg B's mined transaction.
contract TwoLegClaim {
    Vm constant vm = Vm(0x7109709ECfa91a80626fF3989D68f67F5b1DD12D);

    function run() external {
        string memory outDir = vm.envString("OUT_DIR");
        string memory sigPath = vm.envString("SIG_PATH");
        uint256 id = vm.envUint("ESCROW_ID");
        bytes32 legMsg = vm.envBytes32("LEG_MESSAGE");

        bytes memory sig = vm.readFileBinary(sigPath);
        bytes memory aHatP = vm.parseBytes(vm.readFile(string.concat(outDir, "/aHat.hex")));
        bytes memory tHatP = vm.parseBytes(vm.readFile(string.concat(outDir, "/tHat.hex")));
        bytes memory tPacked = vm.parseBytes(vm.readFile(string.concat(outDir, "/tPacked.hex")));

        require(sig.length == LASVerifyOpt.SIG_BYTES, "signature width");
        require(aHatP.length == LASVerifyOpt.AHAT_BYTES, "A' width");
        require(tHatP.length == LASVerifyOpt.THAT_BYTES, "t^ width");
        require(tPacked.length == LASVerifyOpt.TPACK_BYTES, "pack(t) width");

        // `LEG_MESSAGE` is the value the runner read from the node, not a value computed
        // here: the contract derives its own and compares, so a mismatch reverts on chain.
        bytes memory message = abi.encodePacked(legMsg);

        bytes memory cd = abi.encodeWithSelector(
            AdaptorSwapBound.claimBound.selector, id, sig, aHatP, tHatP, tPacked, message
        );
        vm.writeFile(string.concat(outDir, "/claim.calldata"), vm.toString(cd));
        vm.writeFile(string.concat(outDir, "/claim.calldata.len"), vm.toString(cd.length));
        // NOTE: the runner slices the signature back out of the MINED transaction by
        // DECODING the ABI head (argument 1's offset word, then its length word) rather
        // than from a constant written here. A hardcoded offset would silently point at
        // the wrong field if the argument order ever changed; decoding cannot.
    }
}

/// Minimal Foundry cheatcode interface (the repo deliberately avoids a forge-std dependency).
interface Vm {
    function readFile(string calldata path) external view returns (string memory);
    function readFileBinary(string calldata path) external view returns (bytes memory);
    function writeFile(string calldata path, string calldata data) external;
    function envUint(string calldata name) external view returns (uint256);
    function envAddress(string calldata name) external view returns (address);
    function envString(string calldata name) external view returns (string memory);
    function envBytes32(string calldata name) external view returns (bytes32);
    function parseBytes(string calldata value) external pure returns (bytes memory);
    function startBroadcast(uint256 privateKey) external;
    function stopBroadcast() external;
    function toString(address value) external pure returns (string memory);
    function toString(bytes calldata value) external pure returns (string memory);
    function toString(bytes32 value) external pure returns (string memory);
    function toString(uint256 value) external pure returns (string memory);
}
