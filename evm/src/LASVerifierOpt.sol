// SPDX-License-Identifier: MIT
pragma solidity ^0.8.25;

import {nttFw, nttInv} from "../lib/zknox/ZKNOX_NTT_dilithium.sol";
import {LASShake} from "./LASShake.sol";

/// @title LASVerifyOpt — the same LAS `base_verify` as `LASVerify` under the
/// registration invariant stated below, re-expressed to fit inside ONE Ethereum
/// transaction.
///
/// `LASVerify` (src/LASVerifier.sol) is numerically complete and validated against C
/// golden vectors, but a verified claim measures ≈56.6M gas execution plus ≈1.65M
/// intrinsic — ≈3.6× EIP-7825's per-transaction cap of 16,777,216, so it cannot be
/// mined as a single transaction. This library computes the same predicate over the
/// same scheme — same q, same κ, same bounds, same SHAKE256 preimage
/// `pack(t) ‖ pack(w') ‖ M`, same FIPS 204 SampleInBall — and is pinned to
/// `LASVerify` byte-for-byte by `test/LASVerifierOpt.t.sol`. Nothing about LAS
/// changes; only how the EVM is asked to evaluate it.
///
/// ⚠️ That equivalence is CONDITIONAL, and the condition is NOT checked here: it holds
/// exactly where the registered parameters are well formed, in the sense fixed by
/// REGISTRATION OBLIGATION below. Unconditionally, this function decides a *different*
/// predicate — the one parameterised by whatever `aHatPacked`/`tHatPacked`/`tPacked`
/// it is handed. Do not describe it as `base_verify` without that qualifier.
///
/// Where the gas went, and what replaced it:
///
///  1. HALF THE TRANSFORMS. `LASVerify` runs 12 forward + 12 inverse NTTs. `t` is
///     fixed public-key material, so — exactly as `AprimeHat` already is, and exactly
///     as EIP-8051's ML-DSA precompile encodes `t1` — it is registered ONCE in NTT
///     domain (`tHatPacked`), removing 6 forward NTTs. And because the inverse NTT is
///     a linear map, `invNTT(Â·ẑ) − invNTT(ĉ·t̂) = invNTT(Â·ẑ − ĉ·t̂)`: the two
///     products are combined in NTT domain and inverted ONCE per row, removing 6
///     inverse NTTs. 24 transforms become 12, with no change to the result.
///
///  2. NO MATRIX IN MEMORY. `AprimeHat`/`t` used to arrive as `uint256[][]` — one
///     32-byte word per 23-bit coefficient, 304,292 bytes of calldata ABI-decoded
///     into ~300 KB of memory. They now arrive as packed `bytes` (4 bytes big-endian
///     per coefficient, 8 per word) and are read straight from CALLDATA by the
///     multiply loops, never copied to memory. Calldata falls to ~50 KB (measured).
///     The intent is also to shrink the EVM's quadratic memory term, but that component
///     is NOT separately metered — only the end-to-end totals and the per-stage split in
///     test/LASGasBreakdown.t.sol are measured, so no figure is claimed for it.
///
///  3. A SPONGE THAT DOES NOT CHURN. See LASShake: the vendored context spends 200
///     memory words on 200 buffer bytes, absorbs one bounds-checked byte at a time,
///     and re-allocates both the buffer and three constant tables on every one of the
///     ~91 permutations a verification needs.
///
///  4. WORD-WISE CODECS. z-decode, the norm gate and `pack(w')` were per-byte,
///     bounds-checked Solidity loops over ~12 KB and 2,816 coefficients; they are now
///     word-wise Yul, and the norm gate is fused into the decode (see `_decodeZ`).
///
/// REGISTRATION OBLIGATION (unchanged in kind from `LASVerify`). `aHatPacked` and
/// `tHatPacked` are supplied already in NTT domain and are NOT re-derived here, and
/// `tPacked` is the normal-domain hash preimage of the same `t`. The invariant this
/// function assumes, and never verifies, is
///
///     aHatPacked = NTT(A')  ∧  tHatPacked = NTT(t)  ∧  tPacked = pack(t)
///
/// for one key `t` and one matrix `A'`. `LASVerify` assumes the first conjunct for
/// `AprimeHat` already; this library adds the two for `t`.
///
/// What the fund-time `lasContext` commitment does and does not do: it binds all three
/// against substitution BY THE CLAIMER — hash the wrong bytes in and the claim reverts
/// before any arithmetic — but it commits to them jointly and proves nothing about
/// their mutual consistency, and no on-chain check derives one from another.
///
/// ⚠️ AN INCONSISTENT REGISTRATION MAKES THE PREDICATE DIFFERENT, AND SOME SUCH
/// REGISTRATIONS MAKE IT WEAKER. Different is what holds in general: the function
/// decides the predicate parameterised by the bytes it was handed, which for a broken
/// invariant is simply not `base_verify` — it may be unsatisfiable, or satisfiable only
/// by a key nobody holds, and no claim is made about which. Weaker is what holds for
/// some, and one is enough to matter: take `aHatPacked = tHatPacked = 0`; every product
/// vanishes, so `w' = z_top` and acceptance collapses to
/// `c_tilde == SHAKE256(tPacked ‖ pack(z_top) ‖ M)[0:48]` — satisfiable by anyone, for
/// any in-bound `z`, with no secret key and no LAS signature. The escrow is then
/// claimable *without* the adapted signature ever being published, which is precisely
/// the leak the swap's atomicity depends on.
///
/// In that case the loss still falls on the registrant, and only on the registrant: the
/// beneficiary is fixed at fund time and the claim entrypoint pays that address whoever
/// calls it, so a bad registration cannot redirect coins — it costs the funder the
/// atomicity the escrow was buying (the counterparty's witness never leaks), not
/// custody. That is why this stays a registrant's obligation rather than a protocol
/// hole; it is NOT a reason to call the failure benign, and NOT a substitute for
/// checking the invariant, which no analysis here covers in general. Re-deriving
/// `NTT(t)` from `tPacked` once at fund time, off the verification path, would
/// discharge the two `t` conjuncts on chain — NOT the `A'` one, which needs the
/// untransformed `A'` (or the seed it expands from) at registration too. Neither is
/// implemented.
///
/// D3 parameters: n=6, ell=5, d=256, q=8380417, κ=49, γ=κ·d·(n+ℓ)=137984.
library LASVerifyOpt {
    uint256 internal constant N = 256; // ring degree d
    uint256 internal constant Q = 8380417;
    uint256 internal constant KAPPA = 49;
    uint256 internal constant N_LAS = 6; // n
    uint256 internal constant ELL = 5;
    uint256 internal constant N_PLUS_ELL = 11;
    uint256 internal constant Z_BITS = 19;
    uint256 internal constant Z_OFFSET = 137935; // γ − κ
    uint256 internal constant BOUND = 137935; // accept iff ‖z‖∞ ≤ γ − κ
    uint256 internal constant CTILDE_BYTES = 48;
    uint256 internal constant SIG_BYTES = 6736; // CTILDE_BYTES + N_PLUS_ELL·N·Z_BITS/8

    /// Packed transport sizes. 4 bytes BIG-endian per coefficient, 8 per 32-byte word,
    /// so a `calldataload` yields 8 coefficients and each extraction is one shift+mask.
    /// (Literal values, not derived expressions, because inline assembly reads them.)
    uint256 internal constant POLY_PACKED_BYTES = 1024; // N · 4
    uint256 internal constant AHAT_BYTES = 30720; // N_LAS · ELL · 1024
    uint256 internal constant THAT_BYTES = 6144; // N_LAS · 1024

    /// `tPacked` is NOT the same encoding: it is the LITTLE-endian 4-byte-per-coefficient
    /// form that `ref/basesig.c b_polyw_pack` emits, because it is hashed verbatim as the
    /// first third of the challenge preimage. The two encodings differ on purpose — one
    /// is a transport format chosen for cheap EVM extraction, the other is a preimage
    /// that must be byte-identical to what the C signer hashed.
    uint256 internal constant TPACK_BYTES = 6144; // N_LAS · 1024
    uint256 internal constant PREIMAGE_FIXED = 12288; // pack(t) ‖ pack(w')

    /// The norm gate, hoisted onto the raw 19-bit field. A coefficient is decoded as
    /// `z = Z_OFFSET − field`, so `field ∈ [0, 2·BOUND]` ⇔ `z ∈ [−BOUND, +BOUND]`
    /// ⇔ `‖z‖∞ ≤ BOUND` on the centred representative — the field comparison IS the
    /// norm check, and needs no canonicalisation first. (Boundary: field = 275870 gives
    /// z = −137935, centred |z| = BOUND, accepted; field = 275871 gives z = −137936, rejected.)
    uint256 internal constant Z_FIELD_MAX = 275870; // 2 · BOUND

    /// @param aHatPacked  n·ell polynomials, row-major (i·ELL+j), NTT domain, 4B BE per coeff.
    /// @param tHatPacked  n polynomials, NTT domain, 4B BE per coeff (public key, registered).
    /// @param tPacked     n polynomials, normal domain, 4B LE per coeff — the hash preimage.
    /// @param message     the signed message M.
    /// @param sig         the packed adapted signature: c_tilde(48) ‖ BitPack19(z).
    function verify(
        bytes calldata aHatPacked,
        bytes calldata tHatPacked,
        bytes calldata tPacked,
        bytes calldata message,
        bytes calldata sig
    ) internal pure returns (bool) {
        if (
            sig.length != SIG_BYTES || aHatPacked.length != AHAT_BYTES || tHatPacked.length != THAT_BYTES
                || tPacked.length != TPACK_BYTES
        ) return false;

        uint256 aOff;
        uint256 tOff;
        assembly {
            aOff := aHatPacked.offset
            tOff := tHatPacked.offset
        }

        // 1–2. decode z and apply the norm gate in the same pass.
        (uint256[][] memory z, bool normOk) = _decodeZ(sig);
        if (!normOk) return false;

        // 3. challenge c = SampleInBall(c_tilde), then to NTT domain.
        uint256[] memory cHat = nttFw(_sampleInBall(sig));

        // z_bot (the ell lower blocks of z) to NTT domain, in place.
        for (uint256 j = 0; j < ELL; j++) {
            z[N_LAS + j] = nttFw(z[N_LAS + j]);
        }

        // 4. preimage = pack(t) ‖ pack(w') ‖ M, assembled in one buffer with no
        //    intermediate copies. pack(t) is already exactly `tPacked`.
        bytes memory pre = new bytes(PREIMAGE_FIXED + message.length);
        uint256 prePtr;
        assembly {
            prePtr := add(pre, 32)
            calldatacopy(prePtr, tPacked.offset, TPACK_BYTES)
            calldatacopy(add(prePtr, PREIMAGE_FIXED), message.offset, message.length)
        }

        _wprimeInto(prePtr + TPACK_BYTES, aOff, tOff, z, cHat);

        // 5. accept iff SHAKE256(preimage)[0:48] == c_tilde.
        uint256 p = LASShake.init();
        LASShake.absorbPad(p, prePtr, pre.length);
        return _digestMatches(p, sig);
    }

    /// Debug/validation twin of `LASVerify.computeWPrime`, returning w' in the SAME
    /// 4-byte little-endian encoding as the C golden `w_prime.bin` — so the arithmetic
    /// can be diffed against C ground truth byte-for-byte, isolated from the hash.
    function computeWPrimePacked(bytes calldata aHatPacked, bytes calldata tHatPacked, bytes calldata sig)
        internal
        pure
        returns (bytes memory wPacked)
    {
        uint256 aOff;
        uint256 tOff;
        assembly {
            aOff := aHatPacked.offset
            tOff := tHatPacked.offset
        }
        (uint256[][] memory z,) = _decodeZ(sig);
        uint256[] memory cHat = nttFw(_sampleInBall(sig));
        for (uint256 j = 0; j < ELL; j++) {
            z[N_LAS + j] = nttFw(z[N_LAS + j]);
        }
        wPacked = new bytes(N_LAS * POLY_PACKED_BYTES);
        uint256 outPtr;
        assembly {
            outPtr := add(wPacked, 32)
        }
        _wprimeInto(outPtr, aOff, tOff, z, cHat);
    }

    // ---- stages (each separately gas-metered by test/LASGasBreakdown.t.sol) ----

    /// w'[i] = z_top[i] + invNTT( Σ_j Â'[i][j]·ẑ_bot[j] − ĉ·t̂[i] ), written packed at
    /// `outPtr`. This is the whole saving of point 1 in the header: the two products
    /// are summed in NTT domain and inverted ONCE per row, and `t̂` is read from
    /// calldata rather than re-transformed. One `acc` buffer serves all n rows.
    /// `z` must already carry z_bot in NTT domain.
    function _wprimeInto(uint256 outPtr, uint256 aOff, uint256 tOff, uint256[][] memory z, uint256[] memory cHat)
        internal
        pure
    {
        uint256[] memory acc = new uint256[](N);
        for (uint256 i = 0; i < N_LAS; i++) {
            uint256 rowOff = aOff + i * ELL * POLY_PACKED_BYTES;
            for (uint256 j = 0; j < ELL; j++) {
                _mulInto(acc, z[N_LAS + j], rowOff + j * POLY_PACKED_BYTES, j == 0 ? 1 : 0);
            }
            _mulSub(acc, cHat, tOff + i * POLY_PACKED_BYTES);
            _packRow(outPtr + i * POLY_PACKED_BYTES, z[i], nttInv(acc));
        }
    }

    /// BitPack19 decode + norm gate, fused, word-wise.
    ///
    /// The 19-bit fields are LSB-first from byte 48. 8 fields span exactly 19 bytes and
    /// 8 divides 256, so every group of 8 coefficients starts on a byte boundary with
    /// zero bit offset: group g begins at byte 48 + 19g. One `calldataload` per group,
    /// byte-reversed once to give a little-endian view, then 8 shift+mask extractions.
    /// (The final group's word runs 13 bytes past the signature; those bytes only reach
    /// bits ≥ 152 of the reversed word, and no extraction reads that high.)
    function _decodeZ(bytes calldata sig) internal pure returns (uint256[][] memory z, bool ok) {
        z = new uint256[][](N_PLUS_ELL);
        for (uint256 i = 0; i < N_PLUS_ELL; i++) {
            z[i] = new uint256[](N);
        }
        uint256 bad;
        assembly {
            let zbase := add(z, 32)
            let sofs := sig.offset
            for { let i := 0 } lt(i, 11) { i := add(i, 1) } {
                let dst := add(mload(add(zbase, mul(i, 32))), 32)
                for { let m := 0 } lt(m, 32) { m := add(m, 1) } {
                    let w := calldataload(add(sofs, add(48, mul(19, add(mul(i, 32), m)))))
                    // 32-byte reversal: big-endian calldata word -> little-endian value
                    w :=
                        or(
                            shr(8, and(w, 0xFF00FF00FF00FF00FF00FF00FF00FF00FF00FF00FF00FF00FF00FF00FF00FF00)),
                            shl(8, and(w, 0x00FF00FF00FF00FF00FF00FF00FF00FF00FF00FF00FF00FF00FF00FF00FF00FF))
                        )
                    w :=
                        or(
                            shr(16, and(w, 0xFFFF0000FFFF0000FFFF0000FFFF0000FFFF0000FFFF0000FFFF0000FFFF0000)),
                            shl(16, and(w, 0x0000FFFF0000FFFF0000FFFF0000FFFF0000FFFF0000FFFF0000FFFF0000FFFF))
                        )
                    w :=
                        or(
                            shr(32, and(w, 0xFFFFFFFF00000000FFFFFFFF00000000FFFFFFFF00000000FFFFFFFF00000000)),
                            shl(32, and(w, 0x00000000FFFFFFFF00000000FFFFFFFF00000000FFFFFFFF00000000FFFFFFFF))
                        )
                    w :=
                        or(
                            shr(64, and(w, 0xFFFFFFFFFFFFFFFF0000000000000000FFFFFFFFFFFFFFFF0000000000000000)),
                            shl(64, and(w, 0x0000000000000000FFFFFFFFFFFFFFFF0000000000000000FFFFFFFFFFFFFFFF))
                        )
                    w := or(shr(128, w), shl(128, w))

                    for { let u := 0 } lt(u, 8) { u := add(u, 1) } {
                        let f := and(shr(mul(19, u), w), 0x7FFFF)
                        bad := or(bad, gt(f, 275870))
                        // canonical residue of (Z_OFFSET − f) in [0, Q)
                        let zc := sub(137935, f)
                        if gt(f, 137935) { zc := sub(8518352, f) } // Q + Z_OFFSET
                        mstore(dst, zc)
                        dst := add(dst, 32)
                    }
                }
            }
        }
        ok = (bad == 0);
    }

    /// FIPS 204 SampleInBall over the fast sponge — identical stream, identical rule
    /// to `lib/zknox/ZKNOX_SampleInBall.sol::sampleInBallNist` at τ = κ = 49: the first
    /// 8 squeezed bytes little-endian are the sign word (which is just lane 0), and the
    /// remaining stream drives the rejection loop.
    function _sampleInBall(bytes calldata sig) internal pure returns (uint256[] memory c) {
        bytes memory ct = new bytes(CTILDE_BYTES);
        uint256 ctp;
        assembly {
            ctp := add(ct, 32)
            calldatacopy(ctp, sig.offset, 48)
        }

        uint256 p = LASShake.init();
        LASShake.absorbPad(p, ctp, CTILDE_BYTES);

        uint256 signInt = LASShake.laneAt(p, 0);
        uint256 pos = 8;

        c = new uint256[](N);
        for (uint256 i = N - KAPPA; i < N; i++) {
            uint256 j;
            while (true) {
                if (pos == 136) {
                    // squeezed one full rate block; re-permute and continue the stream
                    LASShake.permute(p);
                    pos = 0;
                }
                j = (LASShake.laneAt(p, pos >> 3) >> ((pos & 7) << 3)) & 0xff;
                pos++;
                if (j <= i) break;
            }
            c[i] = c[j];
            c[j] = (signInt & 1) == 1 ? Q - 1 : 1;
            signInt >>= 1;
        }
    }

    /// acc = a·b when `set` is 1, else acc += a·b. `a` is read from calldata: 8
    /// coefficients per `calldataload`, 4 bytes big-endian each.
    function _mulInto(uint256[] memory acc, uint256[] memory b, uint256 polyOff, uint256 set) private pure {
        assembly {
            let dst := add(acc, 32)
            let src := add(b, 32)
            for { let m := 0 } lt(m, 32) { m := add(m, 1) } {
                let word := calldataload(add(polyOff, mul(m, 32)))
                for { let u := 0 } lt(u, 8) { u := add(u, 1) } {
                    let prod := mulmod(and(shr(mul(32, sub(7, u)), word), 0xFFFFFFFF), mload(src), 8380417)
                    switch set
                    case 0 { mstore(dst, addmod(mload(dst), prod, 8380417)) }
                    default { mstore(dst, prod) }
                    dst := add(dst, 32)
                    src := add(src, 32)
                }
            }
        }
    }

    /// acc -= ĉ·t̂[i] in NTT domain. `addmod(x, Q − y, Q)` is exact for y ∈ [0, Q):
    /// at y = 0 the addend is Q ≡ 0, which `addmod` reduces away.
    function _mulSub(uint256[] memory acc, uint256[] memory cHat, uint256 polyOff) private pure {
        assembly {
            let dst := add(acc, 32)
            let src := add(cHat, 32)
            for { let m := 0 } lt(m, 32) { m := add(m, 1) } {
                let word := calldataload(add(polyOff, mul(m, 32)))
                for { let u := 0 } lt(u, 8) { u := add(u, 1) } {
                    let prod := mulmod(and(shr(mul(32, sub(7, u)), word), 0xFFFFFFFF), mload(src), 8380417)
                    mstore(dst, addmod(mload(dst), sub(8380417, prod), 8380417))
                    dst := add(dst, 32)
                    src := add(src, 32)
                }
            }
        }
    }

    /// w'[k] = ztop[k] + acc[k] (mod q, canonical), written as 4-byte LITTLE-endian
    /// coefficients — `ref/basesig.c b_polyw_pack`'s encoding — 8 per 32-byte store.
    function _packRow(uint256 outPtr, uint256[] memory ztop, uint256[] memory acc) private pure {
        assembly {
            let zp := add(ztop, 32)
            let ap := add(acc, 32)
            for { let m := 0 } lt(m, 32) { m := add(m, 1) } {
                let word := 0
                for { let u := 0 } lt(u, 8) { u := add(u, 1) } {
                    let x := addmod(mload(zp), mload(ap), 8380417)
                    // 32-bit little-endian of x; x < q < 2^24 so the high byte is always 0
                    let le := or(or(shl(24, and(x, 0xFF)), shl(8, and(x, 0xFF00))), and(shr(8, x), 0xFF00))
                    word := or(word, shl(mul(32, sub(7, u)), le))
                    zp := add(zp, 32)
                    ap := add(ap, 32)
                }
                mstore(add(outPtr, mul(m, 32)), word)
            }
        }
    }

    /// Compare the 48-byte squeeze against c_tilde without materialising either.
    /// SHAKE serialises lanes little-endian, so output bytes 8k..8k+7 ARE lane k, and
    /// 48 = 6 lanes exactly — no partial lane to mask.
    function _digestMatches(uint256 p, bytes calldata sig) private pure returns (bool same) {
        assembly {
            let acc := 0
            for { let k := 0 } lt(k, 6) { k := add(k, 1) } {
                let v := shr(192, calldataload(add(sig.offset, mul(k, 8))))
                v := or(shr(8, and(v, 0xFF00FF00FF00FF00)), shl(8, and(v, 0x00FF00FF00FF00FF)))
                v := or(shr(16, and(v, 0xFFFF0000FFFF0000)), shl(16, and(v, 0x0000FFFF0000FFFF)))
                v := and(or(shr(32, v), shl(32, v)), 0xFFFFFFFFFFFFFFFF)
                acc := or(acc, xor(v, mload(add(p, mul(k, 32)))))
            }
            same := iszero(acc)
        }
    }
}
