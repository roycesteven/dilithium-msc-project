// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

/// @title LASVerifyCost — a gas-faithful cost probe for *native* on-chain LAS verification.
///
/// WHY THIS EXISTS. `claimLAS` in AdaptorSwap.sol charges only the unavoidable on-chain
/// FLOOR (calldata for the 4672-byte signature + one keccak) and deliberately does NOT
/// verify the lattice signature, on the stated grounds that native verification is
/// "infeasible in the EVM / exceeds the block gas limit". That claim was previously
/// hand-waved. This contract turns it into a MEASURED number: it executes the exact
/// arithmetic op-count of one `las_verify` (ref/las.c) so that `forge test --gas-report`
/// prices it, and the test then compares the total against the block gas limit.
///
/// WHY A COST PROBE IS FAITHFUL. EVM opcodes are fixed-cost regardless of operand *values*:
/// `mulmod`/`addmod` are 8 gas each, `MLOAD`/`MSTORE` 3 gas, independent of the numbers
/// involved. Therefore a kernel that reproduces the exact *structure* of las_verify — the
/// same 12 forward NTTs, 8 inverse NTTs, 20 pointwise products and coefficient passes, over
/// real 256-word memory arrays — costs the same gas as a correct verifier's arithmetic
/// WITHOUT needing to be numerically correct. We measure cost, not a real challenge; a
/// numerically-correct on-chain verifier is the documented future work. The twiddle values
/// come from a runtime recurrence purely so the optimiser cannot constant-fold the loops.
///
/// SCOPE. This prices the ARITHMETIC of las_verify (w' = A z - c t). The SHAKE256 challenge
/// hash is priced separately in the test, via the EVM's native keccak256 opcode — a strict
/// LOWER bound, since a faithful SHAKE256 needs a hand-rolled Keccak-f[1600] in bytecode
/// (the native opcode bakes in standard padding + a fixed 256-bit squeeze, so it cannot do
/// LAS's incremental absorb / arbitrary squeeze / rejection sampling). See docs/LAS.md §8.4.
///
/// Dimensions mirror ref/las.h and ref/params.h exactly:
///   N = 256 (poly degree), LAS_N = 4 (rows of A), LAS_ELL = 4, Q = 8380417.
contract LASVerifyCost {
    uint256 internal constant Q  = 8380417; // LAS/Dilithium modulus (params.h)
    uint256 internal constant NN = 256;     // poly degree N
    uint256 internal constant LN = 4;       // LAS_N
    uint256 internal constant LL = 4;       // LAS_ELL

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

    /// Execute the full arithmetic of one `las_verify`: w' = A z − c t.
    ///
    /// A small fixed set of scratch buffers is REUSED (a real verifier samples A' on the fly,
    /// keeping memory O(1) in the module dimensions) so the figure is not inflated by
    /// memory-expansion artefacts. Returns a sink so the optimiser cannot elide the work;
    /// `seed` makes all operands runtime values so nothing is constant-folded.
    ///
    /// Op budget reproduced (verified against ref/las.c las_verify):
    ///   las_Amul : 4 fwd NTT (vhat) + 16 pointwise + 16 add + 4 inv NTT + 4 identity-add
    ///   c·t loop : 8 fwd NTT + 4 pointwise + 4 inv NTT + 4 sub-pass
    ///   total    : 12 fwd NTT, 8 inv NTT, 20 pointwise, ~40 coeff passes
    function verifyArith(uint256 seed) external pure returns (uint256 sink) {
        uint256[256] memory zetas;
        unchecked {
            // runtime twiddle recurrence — values are irrelevant to gas, this only defeats
            // constant folding so the loops below are actually executed.
            uint256 z = (seed | 1) % Q;
            for (uint256 i = 0; i < NN; ++i) { z = mulmod(z, 1753, Q); zetas[i] = z; }
        }

        uint256[256] memory op1; // doubles as NTT(c)/vhat scratch
        uint256[256] memory op2; // doubles as t[i] scratch
        uint256[256] memory acc; // accumulator / w'[i]
        unchecked {
            for (uint256 j = 0; j < NN; ++j) {
                op1[j] = addmod(zetas[j], seed, Q);
                op2[j] = addmod(zetas[(j * 7) & 255], seed, Q);
            }
        }

        // ---- las_Amul: A·z = z_top + A'·z_bot (A' is LN×LL polys) --------------------
        for (uint256 c = 0; c < LL; ++c) { _ntt(op1, zetas); }      // 4 fwd NTT (vhat[j])
        for (uint256 i = 0; i < LN; ++i) {
            for (uint256 jc = 0; jc < LL; ++jc) {                    // LN·LL = 16 pointwise + add
                _pointwise(acc, op1, op2);
                _addpass(acc, acc, op2);
            }
            _invntt(acc, zetas);                                     // 4 inv NTT (one per row)
            _addpass(acc, acc, op1);                                 // + identity block (v_top)
            sink ^= acc[i & 255];
        }

        // ---- w' = A·z − c·t : 4× polymul(c, t[i]) -----------------------------------
        for (uint256 i = 0; i < LN; ++i) {
            _ntt(op1, zetas);          // NTT(c)        ── 2 fwd NTT per polymul ×4 = 8
            _ntt(op2, zetas);          // NTT(t[i])
            _pointwise(acc, op1, op2); // 4 pointwise
            _invntt(acc, zetas);       // 4 inv NTT
            _addpass(acc, acc, op2);   // sub/reduce/caddq pass
            sink ^= acc[(i * 9) & 255];
        }
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
}
