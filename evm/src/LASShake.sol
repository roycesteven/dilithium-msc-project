// SPDX-License-Identifier: MIT
pragma solidity ^0.8.25;

/// @title LASShake — a gas-tight SHAKE256 sponge for the on-chain LAS verifier.
///
/// Bit-for-bit identical to `lib/zknox/ZKNOX_shake.sol` (rate 136, 0x1f domain
/// separator, 0x80 end marker, Keccak-f[1600] × 24 rounds); the KATs in
/// `test/ZKNoxShake.t.sol` apply unchanged and `test/LASShakeEquiv.t.sol` pins
/// the two implementations against each other. Only the ENCODING of the sponge
/// differs, and that is where the gas goes:
///
///   • the vendored context is `struct { uint64[25] state; uint8[200] buff; … }`,
///     i.e. 200 memory WORDS for 200 buffer BYTES, absorbed one byte at a time
///     through two bounds-checked array accesses per byte;
///   • `f1600` re-materialises three 24-entry constant tables (`_keccakPi`,
///     `_keccakRc`, `_keccakRho`) as fresh memory array literals on EVERY call —
///     ~72 words of never-freed memory per permutation, ~91 permutations per
///     verification, which also inflates the quadratic memory term.
///
/// Here the state is 25 lanes at fixed offsets in one flat arena, the round
/// constants are written ONCE per context, rho/pi are unrolled with literal
/// offsets (no table at all), and absorption reads 8 input bytes per `mload`
/// instead of one byte per bounds-checked index. Nothing about the algorithm
/// changes — only the cost of expressing it.
///
/// Context layout at pointer `p` (CTX_BYTES = 1728):
///   p+0    .. p+799   — 25 Keccak lanes, one per 32-byte word, value in the low 64 bits
///   p+800  .. p+1567  — the 24 Keccak-f round constants
///   p+1568 .. p+1791  — 7 words of theta scratch: the 5 column parities C[0..4] at
///                       p+1600 .. p+1728, flanked by wrap-around copies of C[4] and
///                       C[0] so the cyclic neighbours are plain linear offsets
///
/// The theta scratch is in the arena rather than in Yul locals for a mechanical reason:
/// this project builds with `via_ir` but the sponge cannot be annotated `memory-safe`
/// end to end (`absorbPad`'s masked tail read deliberately loads one word past the input
/// buffer), so the optimiser may not spill stack slots to memory. Five parities held
/// across a 24-iteration loop overflow the 16-slot reachable stack; parked in memory,
/// each round needs only a couple of live temporaries.
library LASShake {
    uint256 internal constant RATE = 136; // SHAKE256 rate, bytes (1088 bits)
    uint256 internal constant RC_OFF = 800; // round-constant table offset within a context
    uint256 internal constant THETA_OFF = 1568; // theta scratch base (C[-1] slot)
    uint256 internal constant CTX_BYTES = 1792;
    uint256 internal constant M64 = 0xFFFFFFFFFFFFFFFF;

    /// Allocate a zeroed sponge and write its round-constant table.
    function init() internal pure returns (uint256 p) {
        bytes memory arena = new bytes(CTX_BYTES); // `new bytes` zeroes: lanes start clean
        assembly {
            p := add(arena, 32)
            let r := add(p, RC_OFF)
            mstore(r, 0x0000000000000001)
            mstore(add(r, 32), 0x0000000000008082)
            mstore(add(r, 64), 0x800000000000808a)
            mstore(add(r, 96), 0x8000000080008000)
            mstore(add(r, 128), 0x000000000000808b)
            mstore(add(r, 160), 0x0000000080000001)
            mstore(add(r, 192), 0x8000000080008081)
            mstore(add(r, 224), 0x8000000000008009)
            mstore(add(r, 256), 0x000000000000008a)
            mstore(add(r, 288), 0x0000000000000088)
            mstore(add(r, 320), 0x0000000080008009)
            mstore(add(r, 352), 0x000000008000000a)
            mstore(add(r, 384), 0x000000008000808b)
            mstore(add(r, 416), 0x800000000000008b)
            mstore(add(r, 448), 0x8000000000008089)
            mstore(add(r, 480), 0x8000000000008003)
            mstore(add(r, 512), 0x8000000000008002)
            mstore(add(r, 544), 0x8000000000000080)
            mstore(add(r, 576), 0x000000000000800a)
            mstore(add(r, 608), 0x800000008000000a)
            mstore(add(r, 640), 0x8000000080008081)
            mstore(add(r, 672), 0x8000000000008080)
            mstore(add(r, 704), 0x0000000080000001)
            mstore(add(r, 736), 0x8000000080008008)
        }
    }

    /// Absorb `len` bytes at memory `ptr`, apply SHAKE padding, and permute — the
    /// sponge is then positioned at stream byte 0 of the output.
    function absorbPad(uint256 p, uint256 ptr, uint256 len) internal pure {
        uint256 full = len / RATE;
        for (uint256 b = 0; b < full; b++) {
            _absorbBlock(p, ptr + b * RATE);
            permute(p);
        }
        uint256 rem = len - full * RATE;

        // Final (short) block. Building it in a zeroed scratch buffer keeps the
        // padding logic obviously correct at every `rem`, including rem = RATE-1
        // where the 0x1f and 0x80 markers land in the same byte. 168 = RATE + 32
        // so the masked word-copy below can never write out of bounds.
        bytes memory tail = new bytes(RATE + 32);
        uint256 tp;
        assembly {
            tp := add(tail, 32)
            let src := add(ptr, mul(full, RATE))
            let nw := div(rem, 32)
            for { let i := 0 } lt(i, nw) { i := add(i, 1) } {
                let o := mul(i, 32)
                mstore(add(tp, o), mload(add(src, o)))
            }
            let tb := mod(rem, 32)
            if tb {
                let o := mul(nw, 32)
                // keep the top `tb` bytes of the source word, zero the rest
                mstore(add(tp, o), and(mload(add(src, o)), not(shr(mul(8, tb), not(0)))))
            }
        }
        tail[rem] = bytes1(uint8(tail[rem]) ^ 0x1f);
        tail[RATE - 1] = bytes1(uint8(tail[RATE - 1]) ^ 0x80);

        _absorbBlock(p, tp);
        permute(p);
    }

    /// Lane `i` (0..24) as a 64-bit value. Squeezed output byte `k` of the current
    /// block is `(laneAt(p, k >> 3) >> (8 * (k & 7))) & 0xff` — SHAKE serialises
    /// lanes little-endian, so a lane IS its own 8 output bytes.
    function laneAt(uint256 p, uint256 i) internal pure returns (uint256 v) {
        assembly {
            v := mload(add(p, mul(i, 32)))
        }
    }

    /// Squeeze `n` bytes from an absorbed sponge. The verifier's hot path does NOT use
    /// this — it compares 48 bytes as six lanes via `laneAt` — but cross-implementation
    /// tests and any caller wanting bytes do.
    function squeeze(uint256 p, uint256 n) internal pure returns (bytes memory out) {
        out = new bytes(n);
        uint256 pos = 0;
        for (uint256 k = 0; k < n; k++) {
            if (pos == RATE) {
                permute(p);
                pos = 0;
            }
            out[k] = bytes1(uint8((laneAt(p, pos >> 3) >> ((pos & 7) << 3)) & 0xff));
            pos++;
        }
    }

    /// One-shot SHAKE256(input, n) over a memory buffer.
    function digest(bytes memory input, uint256 n) internal pure returns (bytes memory) {
        uint256 p = init();
        uint256 ptr;
        assembly {
            ptr := add(input, 32)
        }
        absorbPad(p, ptr, input.length);
        return squeeze(p, n);
    }

    /// XOR one RATE-sized block of input into the first 17 lanes.
    function _absorbBlock(uint256 p, uint256 src) private pure {
        assembly {
            for { let w := 0 } lt(w, 17) { w := add(w, 1) } {
                // the 8 bytes at src+8w, read little-endian
                let v := shr(192, mload(add(src, mul(w, 8))))
                v := or(shr(8, and(v, 0xFF00FF00FF00FF00)), shl(8, and(v, 0x00FF00FF00FF00FF)))
                v := or(shr(16, and(v, 0xFFFF0000FFFF0000)), shl(16, and(v, 0x0000FFFF0000FFFF)))
                v := and(or(shr(32, v), shl(32, v)), M64)
                let la := add(p, mul(w, 32))
                mstore(la, xor(mload(la), v))
            }
        }
    }

    /// Keccak-f[1600]. Same 24 rounds, same constants and rotation offsets as the
    /// vendored `f1600`; rho/pi is the standard 24-step chain with the lane offsets
    /// and rotation amounts inlined as literals rather than read from memory tables.
    function permute(uint256 p) internal pure {
        assembly {
            for { let i := 0 } lt(i, 24) { i := add(i, 1) } {
                // ---------------- theta ----------------
                // C[x] = XOR of column x, parked at p+1568+32x (see the layout note).
                // C[x] = XOR of column x. Accumulated one XOR at a time rather than as a
                // nested expression: the nested form materialises enough temporaries to
                // blow the reachable stack under via_ir without memoryguard.
                for { let x := 0 } lt(x, 160) { x := add(x, 32) } {
                    let v := mload(add(p, x))
                    v := xor(v, mload(add(p, add(x, 160))))
                    v := xor(v, mload(add(p, add(x, 320))))
                    v := xor(v, mload(add(p, add(x, 480))))
                    v := xor(v, mload(add(p, add(x, 640))))
                    mstore(add(p, add(1600, x)), v) // C[x] at THETA_OFF+32+32x
                }
                // Wrap-around copies: C[4] one slot BELOW C[0] and C[0] one slot ABOVE
                // C[4]. That turns the cyclic neighbours C[x−1] and C[x+1] into plain
                // linear offsets, so the D loop needs no `mod` (5 gas a time, twice per
                // column, 24 rounds) and stays shallow.
                mstore(add(p, THETA_OFF), mload(add(p, 1728))) // [-1] := C[4]
                mstore(add(p, 1760), mload(add(p, 1600))) // [ 5] := C[0]

                // D[x] = C[x-1] ^ rol64(C[x+1], 1), then A[x,y] ^= D[x].
                for { let x := 0 } lt(x, 160) { x := add(x, 32) } {
                    let base := add(p, add(THETA_OFF, x)) // &C[x-1]
                    let cc := mload(add(base, 64)) // C[x+1]
                    let d := xor(mload(base), and(or(shl(1, cc), shr(63, cc)), M64))
                    let b := add(p, x)
                    mstore(b, xor(mload(b), d))
                    mstore(add(b, 160), xor(mload(add(b, 160)), d))
                    mstore(add(b, 320), xor(mload(add(b, 320)), d))
                    mstore(add(b, 480), xor(mload(add(b, 480)), d))
                    mstore(add(b, 640), xor(mload(add(b, 640)), d))
                }

                // ---------------- rho + pi ----------------
                // 24-step chain: t <- state[1]; for each (lane, rot) pair, the lane
                // is saved, overwritten with rol64(t, rot), and becomes the next t.
                {
                    let t := mload(add(p, 32))
                    let s := 0
                    // (lane 10, rot 1)
                    s := mload(add(p, 320))
                    mstore(add(p, 320), and(or(shl(1, t), shr(63, t)), M64))
                    t := s
                    // (lane 7, rot 3)
                    s := mload(add(p, 224))
                    mstore(add(p, 224), and(or(shl(3, t), shr(61, t)), M64))
                    t := s
                    // (lane 11, rot 6)
                    s := mload(add(p, 352))
                    mstore(add(p, 352), and(or(shl(6, t), shr(58, t)), M64))
                    t := s
                    // (lane 17, rot 10)
                    s := mload(add(p, 544))
                    mstore(add(p, 544), and(or(shl(10, t), shr(54, t)), M64))
                    t := s
                    // (lane 18, rot 15)
                    s := mload(add(p, 576))
                    mstore(add(p, 576), and(or(shl(15, t), shr(49, t)), M64))
                    t := s
                    // (lane 3, rot 21)
                    s := mload(add(p, 96))
                    mstore(add(p, 96), and(or(shl(21, t), shr(43, t)), M64))
                    t := s
                    // (lane 5, rot 28)
                    s := mload(add(p, 160))
                    mstore(add(p, 160), and(or(shl(28, t), shr(36, t)), M64))
                    t := s
                    // (lane 16, rot 36)
                    s := mload(add(p, 512))
                    mstore(add(p, 512), and(or(shl(36, t), shr(28, t)), M64))
                    t := s
                    // (lane 8, rot 45)
                    s := mload(add(p, 256))
                    mstore(add(p, 256), and(or(shl(45, t), shr(19, t)), M64))
                    t := s
                    // (lane 21, rot 55)
                    s := mload(add(p, 672))
                    mstore(add(p, 672), and(or(shl(55, t), shr(9, t)), M64))
                    t := s
                    // (lane 24, rot 2)
                    s := mload(add(p, 768))
                    mstore(add(p, 768), and(or(shl(2, t), shr(62, t)), M64))
                    t := s
                    // (lane 4, rot 14)
                    s := mload(add(p, 128))
                    mstore(add(p, 128), and(or(shl(14, t), shr(50, t)), M64))
                    t := s
                    // (lane 15, rot 27)
                    s := mload(add(p, 480))
                    mstore(add(p, 480), and(or(shl(27, t), shr(37, t)), M64))
                    t := s
                    // (lane 23, rot 41)
                    s := mload(add(p, 736))
                    mstore(add(p, 736), and(or(shl(41, t), shr(23, t)), M64))
                    t := s
                    // (lane 19, rot 56)
                    s := mload(add(p, 608))
                    mstore(add(p, 608), and(or(shl(56, t), shr(8, t)), M64))
                    t := s
                    // (lane 13, rot 8)
                    s := mload(add(p, 416))
                    mstore(add(p, 416), and(or(shl(8, t), shr(56, t)), M64))
                    t := s
                    // (lane 12, rot 25)
                    s := mload(add(p, 384))
                    mstore(add(p, 384), and(or(shl(25, t), shr(39, t)), M64))
                    t := s
                    // (lane 2, rot 43)
                    s := mload(add(p, 64))
                    mstore(add(p, 64), and(or(shl(43, t), shr(21, t)), M64))
                    t := s
                    // (lane 20, rot 62)
                    s := mload(add(p, 640))
                    mstore(add(p, 640), and(or(shl(62, t), shr(2, t)), M64))
                    t := s
                    // (lane 14, rot 18)
                    s := mload(add(p, 448))
                    mstore(add(p, 448), and(or(shl(18, t), shr(46, t)), M64))
                    t := s
                    // (lane 22, rot 39)
                    s := mload(add(p, 704))
                    mstore(add(p, 704), and(or(shl(39, t), shr(25, t)), M64))
                    t := s
                    // (lane 9, rot 61)
                    s := mload(add(p, 288))
                    mstore(add(p, 288), and(or(shl(61, t), shr(3, t)), M64))
                    t := s
                    // (lane 6, rot 20)
                    s := mload(add(p, 192))
                    mstore(add(p, 192), and(or(shl(20, t), shr(44, t)), M64))
                    t := s
                    // (lane 1, rot 44)
                    mstore(add(p, 32), and(or(shl(44, t), shr(20, t)), M64))
                }

                // ---------------- chi (+ iota folded into lane 0) ----------------
                for { let y := 0 } lt(y, 800) { y := add(y, 160) } {
                    let b := add(p, y)
                    let a0 := mload(b)
                    let a1 := mload(add(b, 32))
                    let a2 := mload(add(b, 64))
                    let a3 := mload(add(b, 96))
                    let a4 := mload(add(b, 128))
                    mstore(b, xor(a0, and(xor(a1, M64), a2)))
                    mstore(add(b, 32), xor(a1, and(xor(a2, M64), a3)))
                    mstore(add(b, 64), xor(a2, and(xor(a3, M64), a4)))
                    mstore(add(b, 96), xor(a3, and(xor(a4, M64), a0)))
                    mstore(add(b, 128), xor(a4, and(xor(a0, M64), a1)))
                }
                mstore(p, xor(mload(p), mload(add(add(p, RC_OFF), mul(i, 32)))))
            }
        }
    }
}
