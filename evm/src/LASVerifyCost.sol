// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

/// @title LASVerifyCost — a gas-faithful cost probe for *native* on-chain LAS verification.
///
/// WHY THIS EXISTS. `claimLAS` in AdaptorSwap.sol charges only the unavoidable on-chain
/// FLOOR (calldata for the 6720-byte signature + one keccak) and deliberately does NOT
/// verify the lattice signature, on the stated grounds that native verification is
/// "infeasible in the EVM / exceeds the block gas limit". That claim was previously
/// hand-waved. This contract turns it into a MEASURED number: it executes the exact
/// arithmetic op-count of one `base_verify` (ref/basesig.c) so that `forge test --gas-report`
/// prices it, and the test then compares the total against the block gas limit.
///
/// WHAT THIS PROBE IS (AND IS NOT). EVM opcodes are fixed-cost regardless of operand *values*:
/// `mulmod`/`addmod` are 8 gas each, `MLOAD`/`MSTORE` 3 gas, independent of the numbers
/// involved. This probe reproduces the OPERATION COUNT of the dominant polynomial arithmetic
/// in base_verify_internal — 12 forward NTTs, 12 inverse NTTs, 36 pointwise products and 54
/// coefficient passes, over real 256-word memory arrays. It reuses temporary memory and does
/// NOT reproduce the verifier's exact values or exact memory-access pattern. Therefore the
/// result is an arithmetic LOWER-BOUND ESTIMATE, not the exact gas cost of a complete and
/// numerically correct Solidity verifier (which is the documented future work). The twiddle
/// values come from a runtime recurrence purely so the optimiser cannot constant-fold the loops.
///
/// SCOPE. This prices the ARITHMETIC of base_verify (w' = A z - c t). The SHAKE256 challenge
/// hash is priced separately in the test, via the EVM's native keccak256 opcode — a strict
/// LOWER bound, since a faithful SHAKE256 needs a hand-rolled Keccak-f[1600] in bytecode
/// (the native opcode bakes in standard padding + a fixed 256-bit squeeze, so it cannot do
/// LAS's incremental absorb / arbitrary squeeze / rejection sampling). See docs/LAS.md §8.4.
///
/// Module dimensions mirror ref/setup.h; the probe is parametrised by (rowCount = n,
/// columnCount = ell) so the same kernel prices every parameter set. D3 (n=6, ell=5) is the
/// headline; verifyArithLevel2 (4,4) and verifyArithLevel5 (8,7) exist only to show how the
/// arithmetic gas grows with parameter size. Ring degree N = 256, Q = 8380417 (params.h).
contract LASVerifyCost {
    uint256 internal constant Q  = 8380417; // LAS/Dilithium modulus (params.h)
    uint256 internal constant NN = 256;     // poly degree N (ring degree d)

    /// Forward negacyclic NTT, in place: 8 stages × 128 butterflies = 1024 butterflies,
    /// each = 1 modular multiply + 1 add + 1 subtract (mod Q). Structurally identical to
    /// ref/ntt.c `ntt()` (which uses Montgomery reduction; mulmod is the EVM equivalent and
    /// has the same fixed gas cost).
    function _ntt(uint256[256] memory a, uint256[256] memory zetas) internal pure {
        unchecked {
            uint256 k = 0;
            for (uint256 len = 128; len > 0; len >>= 1) {
                for (uint256 start = 0; start < NN; start += (len << 1)) {
                    uint256 zeta = zetas[(++k) & 255];
                    uint256 end = start + len;
                    for (uint256 j = start; j < end; ++j) {
                        uint256 t = mulmod(zeta, a[j + len], Q);
                        a[j + len] = addmod(a[j], Q - t, Q);
                        a[j] = addmod(a[j], t, Q);
                    }
                }
            }
        }
    }

    /// Inverse NTT + Montgomery scaling pass: 1024 butterflies + a final 256-coeff multiply.
    /// Structurally identical to ref/ntt.c `invntt_tomont()`.
    function _invntt(uint256[256] memory a, uint256[256] memory zetas) internal pure {
        unchecked {
            uint256 k = 255;
            for (uint256 len = 1; len < NN; len <<= 1) {
                for (uint256 start = 0; start < NN; start += (len << 1)) {
                    uint256 zeta = zetas[(k--) & 255];
                    uint256 end = start + len;
                    for (uint256 j = start; j < end; ++j) {
                        uint256 tt = a[j];
                        a[j] = addmod(tt, a[j + len], Q);
                        a[j + len] = mulmod(zeta, addmod(tt, Q - a[j + len], Q), Q);
                    }
                }
            }
            uint256 f = 41978; // mont^2/256 final scaling, as in invntt_tomont
            for (uint256 j = 0; j < NN; ++j) a[j] = mulmod(f, a[j], Q);
        }
    }

    /// Pointwise product mod Q (256 modular multiplies) — ref/poly.c `poly_pointwise_montgomery`.
    function _pointwise(uint256[256] memory c, uint256[256] memory a, uint256[256] memory b)
        internal pure
    {
        unchecked { for (uint256 j = 0; j < NN; ++j) c[j] = mulmod(a[j], b[j], Q); }
    }

    /// One coefficient-wise add/sub/reduce pass (256 modular adds) — models poly_add /
    /// poly_sub / poly_reduce / poly_caddq, each an O(N) pass.
    function _addpass(uint256[256] memory c, uint256[256] memory a, uint256[256] memory b)
        internal pure
    {
        unchecked { for (uint256 j = 0; j < NN; ++j) c[j] = addmod(a[j], b[j], Q); }
    }

    /// D3 (n=6, ell=5) is the headline setting; the level-2 (4,4) and level-5 (8,7) wrappers
    /// let the test show how the arithmetic gas grows as the module parameters increase.
    function verifyArithLevel2(uint256 seed) external pure returns (uint256) { return _verifyArith(seed, 4, 4); }
    function verifyArithLevel3(uint256 seed) external pure returns (uint256) { return _verifyArith(seed, 6, 5); }
    function verifyArithLevel5(uint256 seed) external pure returns (uint256) { return _verifyArith(seed, 8, 7); }

    /// Execute the dominant polynomial arithmetic of one `base_verify`: w' = A z − c t, for a
    /// module with `rowCount` rows (n) and `columnCount` columns (ell). This reproduces the
    /// OPERATION COUNT of base_verify_internal; it reuses temporary memory and does not
    /// reproduce the verifier's exact values or memory-access pattern, so the priced result is
    /// an arithmetic LOWER-BOUND ESTIMATE, not the exact gas cost of a complete, numerically
    /// correct Solidity verifier. A fixed set of scratch buffers is REUSED so the figure is not
    /// inflated by memory-expansion artefacts; `seed` makes all operands runtime values (no
    /// constant-folding) and `sink` stops the optimiser eliding the work.
    ///
    /// Op budget (counted from ref/basesig.c base_verify_internal), as a function of (n, ell):
    ///   fwd NTT    = ell + 1 + n                     (z_bot, c, t)
    ///   inv NTT    = 2·n                             (w', c·t)
    ///   pointwise  = n·ell + n                       (A'·z, c·t)
    ///   coeff pass = n·(ell−1) + 5·n                 (matrix accumulate-adds + 5 later ops)
    ///     D2 (4,4) => 9 fwd, 8 inv, 20 pointwise, 32 passes
    ///     D3 (6,5) => 12 fwd, 12 inv, 36 pointwise, 54 passes   <- headline
    ///     D5 (8,7) => 16 fwd, 16 inv, 64 pointwise, 88 passes
    function _verifyArith(uint256 seed, uint256 rowCount, uint256 columnCount)
        internal pure returns (uint256 sink)
    {
        uint256[256] memory zetas;
        unchecked {
            // runtime twiddle recurrence — values are irrelevant to gas, this only defeats
            // constant folding so the loops below are actually executed.
            uint256 z = (seed | 1) % Q;
            for (uint256 i = 0; i < NN; ++i) { z = mulmod(z, 1753, Q); zetas[i] = z; }
        }

        uint256[256] memory op1; // reused: z_bot / t[i] scratch
        uint256[256] memory op2; // reused: A'[i][j] / c_hat scratch
        uint256[256] memory acc; // accumulator / w'[i]
        unchecked {
            for (uint256 j = 0; j < NN; ++j) {
                op1[j] = addmod(zetas[j], seed, Q);
                op2[j] = addmod(zetas[(j * 7) & 255], seed, Q);
            }
        }

        // ---- z_1_hat = NTT(z_bot): columnCount forward NTTs -------------------------
        for (uint256 j = 0; j < columnCount; ++j) { _ntt(op1, zetas); }   // ell fwd NTT

        // ---- w' = A'·z_1_hat : rowCount rows × (columnCount pointwise + (columnCount-1) add)
        // First product writes acc directly (NO accumulator-zeroing pass); the remaining
        // columnCount-1 products are added in. => n·ell pointwise, n·(ell-1) accumulate-adds.
        for (uint256 i = 0; i < rowCount; ++i) {
            _pointwise(acc, op1, op2);                                    // column 0 (direct write)
            for (uint256 jc = 1; jc < columnCount; ++jc) {                // columns 1..ell-1
                _pointwise(op1, op1, op2);
                _addpass(acc, acc, op1);
            }
            sink ^= acc[i & 255];
        }

        // ---- NTT(c) ONCE (outside the row loop) + NTT(t): 1 + rowCount forward NTTs ---
        _ntt(op2, zetas);                                                 // 1 fwd NTT (c_hat)
        for (uint256 i = 0; i < rowCount; ++i) { _ntt(op1, zetas); }      // n fwd NTT (t_hat)

        // ---- c·t : rowCount pointwise -----------------------------------------------
        for (uint256 i = 0; i < rowCount; ++i) { _pointwise(acc, op2, op1); sink ^= acc[i & 255]; }

        // ---- invntt(w') + invntt(c·t) : 2·rowCount inverse NTTs ---------------------
        for (uint256 i = 0; i < rowCount; ++i) { _invntt(acc, zetas); }   // n inv NTT (w')
        for (uint256 i = 0; i < rowCount; ++i) { _invntt(op1, zetas); }   // n inv NTT (c·t)

        // ---- the 5 later per-row operations (reduce, +z_top, −c·t, reduce, caddq) ----
        // 5 operations × rowCount rows = 5·n coefficient passes.
        for (uint256 p = 0; p < 5 * rowCount; ++p) { _addpass(acc, acc, op1); sink ^= acc[p & 255]; }
    }

    /* --------- per-primitive probes: run one primitive `reps` times after a single setup, so
       the test can isolate per-op gas (delta between two rep counts) and reconcile it against
       verifyArith's 12 fwd NTT / 8 inv NTT / 20 pointwise budget. --------- */

    function _seedPair(uint256 seed)
        private pure
        returns (uint256[256] memory z, uint256[256] memory a)
    {
        unchecked {
            uint256 v = (seed | 1) % Q;
            for (uint256 i = 0; i < NN; ++i) { v = mulmod(v, 1753, Q); z[i] = v; a[i] = addmod(v, seed, Q); }
        }
    }

    function nttReps(uint256 reps, uint256 seed) external pure returns (uint256 sink) {
        (uint256[256] memory z, uint256[256] memory a) = _seedPair(seed);
        unchecked { for (uint256 r = 0; r < reps; ++r) { _ntt(a, z); sink ^= a[r & 255]; } }
    }

    function invnttReps(uint256 reps, uint256 seed) external pure returns (uint256 sink) {
        (uint256[256] memory z, uint256[256] memory a) = _seedPair(seed);
        unchecked { for (uint256 r = 0; r < reps; ++r) { _invntt(a, z); sink ^= a[r & 255]; } }
    }

    function pointwiseReps(uint256 reps, uint256 seed) external pure returns (uint256 sink) {
        (uint256[256] memory z, uint256[256] memory a) = _seedPair(seed);
        uint256[256] memory c;
        unchecked { for (uint256 r = 0; r < reps; ++r) { _pointwise(c, a, z); sink ^= c[r & 255]; } }
    }

    // one coefficient pass (256 addmod) — models the per-coeff O(N) cost of poly_reduce /
    // poly_add / poly_sub / poly_caddq, the 54 coefficient passes in base_verify_internal.
    function addpassReps(uint256 reps, uint256 seed) external pure returns (uint256 sink) {
        (uint256[256] memory z, uint256[256] memory a) = _seedPair(seed);
        uint256[256] memory c;
        unchecked { for (uint256 r = 0; r < reps; ++r) { _addpass(c, a, z); sink ^= c[r & 255]; } }
    }
}
