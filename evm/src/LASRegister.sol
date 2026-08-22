// SPDX-License-Identifier: MIT
pragma solidity ^0.8.25;

import {nttFw} from "../lib/zknox/ZKNOX_NTT_dilithium.sol";

/// @title LASRegister — the REGISTRATION side of `LASVerifyOpt`.
///
/// `LASVerifyOpt.verify` consumes public parameters that are already in NTT domain and
/// already packed. Something has to produce them, and exactly one definition of those
/// encodings may exist — a second, subtly different packer is how a verifier silently
/// starts checking the wrong bytes. This library is that single definition, shared by
/// the tests, the gas harness and the Anvil script.
///
/// TWO ENCODINGS, DELIBERATELY DIFFERENT (see `LASVerifierOpt`'s header):
///   • `packNtt` — 4 bytes BIG-endian per coefficient, 8 per 32-byte word. A transport
///     format, chosen so the verifier extracts a coefficient with one shift+mask from
///     calldata. Used for A' and t in NTT domain.
///   • `packLE` — 4 bytes LITTLE-endian per coefficient. NOT a transport choice: it is
///     the encoding `ref/basesig.c b_polyw_pack` emits, and `pack(t)` is hashed verbatim
///     as the first third of the challenge preimage, so it must be byte-identical to
///     what the C signer hashed.
library LASRegister {
    uint256 internal constant N = 256;

    /// Normal-domain polynomials -> NTT domain -> packed transport form.
    /// Mirrors `setup_public_params` NTT-ing `pp->a_prime` once at setup: this is a
    /// REGISTRATION-time cost, paid once, never per verification.
    /// The input is copied first, so the caller's normal-domain polynomials survive
    /// (`nttFw` transforms in place).
    function packNtt(uint256[][] memory polys) internal pure returns (bytes memory) {
        uint256[][] memory hat = new uint256[][](polys.length);
        for (uint256 i = 0; i < polys.length; i++) {
            uint256[] memory p = new uint256[](N);
            for (uint256 k = 0; k < N; k++) {
                p[k] = polys[i][k];
            }
            hat[i] = nttFw(p);
        }
        return packBE(hat);
    }

    /// 4 bytes big-endian per coefficient.
    function packBE(uint256[][] memory polys) internal pure returns (bytes memory out) {
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

    /// 4 bytes little-endian per coefficient — `b_polyw_pack`'s encoding, i.e. the
    /// challenge-preimage bytes.
    function packLE(uint256[][] memory polys) internal pure returns (bytes memory out) {
        out = new bytes(polys.length * N * 4);
        uint256 o = 0;
        for (uint256 i = 0; i < polys.length; i++) {
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

    /// The fund-time commitment `claimLASVerifiedOpt` re-derives and checks. Defined
    /// here so the funder and the contract cannot drift apart on argument order.
    function context(bytes memory aHatPacked, bytes memory tHatPacked, bytes memory tPacked, bytes memory message)
        internal
        pure
        returns (bytes32)
    {
        return keccak256(abi.encode(aHatPacked, tHatPacked, tPacked, message));
    }

    /// Domain separator for the bound claim message.
    bytes32 internal constant CLAIM_DOMAIN = keccak256("LAS-ADAPTOR-SWAP-CLAIM-v1");

    /// The message an adapted LAS signature must be over to settle escrow `id`.
    ///
    /// eprint 2020/845 Fig. 1 signs tx1 and tx2 — the transactions that move the coins —
    /// not an opaque blob. The EVM gives a contract no transaction object it can hash, so
    /// this digest stands in for one: it names the chain, the escrow contract, the escrow,
    /// and the payment that escrow will make. A signature over it cannot be replayed onto
    /// another chain, another contract, another escrow, or a payment with a different
    /// beneficiary or amount — which is what makes two settled legs evidence of a swap
    /// rather than evidence of two valid signatures.
    ///
    /// NO LEG INDEX. The two legs live on different chains, in different contracts, under
    /// different escrow ids, so `chainId ‖ swapContract ‖ id` already separates them. A leg
    /// field would have to be stored at fund time or supplied by the claimer, and a
    /// claimer-supplied field is exactly the unbound input this derivation removes.
    ///
    /// EXACTLY 32 BYTES, which keeps the signed-message length — and with it the SHAKE256
    /// preimage `pack(t) ‖ pack(w') ‖ M` that dominates verification gas — identical to the
    /// measured one-transaction configuration.
    ///
    /// `internal`, so it inlines into the contract and into tests/scripts with no library
    /// deployment. Off-chain parties do not re-implement it: they read the same digest back
    /// from the chain via `AdaptorSwap.legMessage` / `legMessagePreview`, so what they sign
    /// is what the contract will check.
    function claimMessage(
        uint256 chainId,
        address swapContract,
        uint256 id,
        address payer,
        address beneficiary,
        uint256 amount
    ) internal pure returns (bytes32) {
        return keccak256(abi.encode(CLAIM_DOMAIN, chainId, swapContract, id, payer, beneficiary, amount));
    }

    /// The bound path's fund-time commitment: the verification parameters only.
    ///
    /// The message is deliberately NOT committed here — it is DERIVED from escrow state at
    /// claim time, so storing it would be redundant and would create a second place for the
    /// two to drift apart. (A funder cannot compute it before *broadcasting* the fund call,
    /// since it depends on the id that call assigns; it is readable immediately afterwards.)
    function contextBound(bytes memory aHatPacked, bytes memory tHatPacked, bytes memory tPacked)
        internal
        pure
        returns (bytes32)
    {
        return keccak256(abi.encode(aHatPacked, tHatPacked, tPacked));
    }
}

/// @title LASTxGas — what an Ethereum node actually charges for a transaction.
///
/// **EIP-7623 (live since Pectra), not EIP-2028.** Calldata is priced in *tokens*
/// — 1 per zero byte, 4 per non-zero byte — and the transaction is charged
///
///     21000 + max( 4·tokens + execution ,  10·tokens )
///
/// The first branch reproduces the old EIP-2028 schedule (4 gas per zero byte, 16 per
/// non-zero). The second is a FLOOR that binds when a transaction carries a lot of
/// calldata relative to how much computing it does — which is exactly the shape of a
/// lattice-signature claim. Modelling only the first branch **understates** the charge
/// for any calldata-heavy, compute-light transaction, and the whole point of this work
/// is a comparison against a hard per-transaction ceiling, so the model has to be the
/// real rule rather than the one that happens to bind today.
///
/// (For the D3 LAS claim the crossover sits at roughly 1M gas of execution: below that,
/// the floor is what a node charges. Which branch binds is reported alongside the total
/// so it is never left implicit.)
library LASTxGas {
    uint256 internal constant STANDARD_TOKEN_COST = 4;
    uint256 internal constant TOTAL_COST_FLOOR_PER_TOKEN = 10;
    uint256 internal constant TX_BASE = 21_000;

    /// EIP-7623 calldata tokens.
    function tokens(bytes memory cd) internal pure returns (uint256 t) {
        for (uint256 i = 0; i < cd.length; i++) {
            t += cd[i] == 0 ? 1 : 4;
        }
    }

    /// Total gas a node charges for a transaction with calldata `cd` and `execGas`
    /// execution. `floorBinds` says which branch of the max applied.
    function total(bytes memory cd, uint256 execGas) internal pure returns (uint256 gas, bool floorBinds) {
        uint256 t = tokens(cd);
        uint256 standard = STANDARD_TOKEN_COST * t + execGas;
        uint256 floorCost = TOTAL_COST_FLOOR_PER_TOKEN * t;
        floorBinds = floorCost > standard;
        gas = TX_BASE + (floorBinds ? floorCost : standard);
    }
}
