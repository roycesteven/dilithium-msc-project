// SPDX-License-Identifier: MIT
pragma solidity ^0.8.25;

import {nttFw, nttInv} from "../lib/zknox/ZKNOX_NTT_dilithium.sol";
import {vecMulMod} from "../lib/zknox/ZKNOX_dilithium_utils.sol";
import {sampleInBallNist} from "../lib/zknox/ZKNOX_SampleInBall.sol";
import {CtxShake, shakeInit, shakeUpdate, shakeDigest} from "../lib/zknox/ZKNOX_shake.sol";

/// @title LASVerify — a NUMERICALLY-COMPLETE on-chain verifier for the hint-less
/// LAS base signature (Algorithm 1 of eprint 2020/845, the simplified-Dilithium
/// scheme), assembled from vendored ZKNox primitives (SHAKE256, NTT, SampleInBall)
/// each validated against C ground truth in Stages 2–4.
///
/// It verifies the ORDINARY base signature — i.e. the *adapted* LAS signature,
/// which by construction is a fully ordinary signature — mirroring
/// ref/basesig.c base_verify_internal exactly:
///
///   1. parse (c_tilde, z) from the wire (c_tilde ‖ BitPack19(z));
///   2. reject if ‖z‖∞ > γ − κ;
///   3. c = SampleInBall(c_tilde);
///   4. w' = z_top + A'·z_bot − c·t   (A = [I | A'], mod q, canonical);
///   5. accept iff c_tilde == SHAKE256( pack(t) ‖ pack(w') ‖ M ).
///
/// DOMAIN / COST (mirrors the C reference exactly). In ref/basesig.c, pp->a_prime
/// is STORED in NTT domain (setup_public_params NTTs it once). So the matrix here
/// is supplied ALREADY in NTT domain (`AprimeHat`, computed once at registration)
/// and is NEVER NTT'd per verification. Each verify performs only:
///   • 12 forward NTTs — 5 (z_bot) + 1 (c) + 6 (t);
///   • n·ell + n = 36 pointwise multiplies (A'·z_bot, then c·t);
///   • 12 inverse NTTs — 6 (A'·z_bot rows) + 6 (c·t rows),
/// i.e. exactly base_verify_internal's op budget. The reused NTT is a normal-domain
/// negacyclic NTT (Stage 3); AprimeHat is nttFw() of the normal A', so w' is an
/// equivalent convolution and is bit-identical after canonicalisation — no protocol
/// step is simplified.
///
/// D3 parameters: n=6, ell=5, d=256, q=8380417, κ=49, γ=κ·d·(n+ℓ)=137984.
library LASVerify {
    uint256 internal constant N = 256;          // ring degree d
    uint256 internal constant Q = 8380417;
    uint256 internal constant KAPPA = 49;        // challenge weight
    uint256 internal constant N_LAS = 6;         // n (rows of A, dim of t)
    uint256 internal constant ELL = 5;           // ell (columns of A')
    uint256 internal constant N_PLUS_ELL = 11;   // n + ell (dim of z)
    uint256 internal constant Z_BITS = 19;       // LAS_Z_COEFF_BITS (D3)
    uint256 internal constant Z_OFFSET = 137935; // γ − κ
    uint256 internal constant BOUND = 137935;    // accept iff ‖z‖∞ ≤ γ − κ
    uint256 internal constant SIG_BYTES = 32 + (N_PLUS_ELL * N * Z_BITS) / 8; // 6720

    /// Precompute the registered matrix: A' (NORMAL domain, row-major n*ell) into
    /// NTT domain, ONCE at registration. Mirrors setup_public_params NTT-ing
    /// pp->a_prime. `AprimeNormal[i*ELL+j]` -> `AprimeHat[i*ELL+j]`. (nttFw mutates
    /// in place; the returned handles are the same arrays.)
    function toNttDomain(uint256[][] memory AprimeNormal) internal pure returns (uint256[][] memory AprimeHat) {
        AprimeHat = AprimeNormal;
        for (uint256 idx = 0; idx < N_LAS * ELL; idx++) AprimeHat[idx] = nttFw(AprimeHat[idx]);
    }

    /// @param AprimeHat n*ell polynomials, row-major (AprimeHat[i*ELL+j]), NTT domain
    ///                  (registered once via toNttDomain — NEVER NTT'd per verify).
    /// @param t         n polynomials, canonical [0,Q) (the public key).
    /// @param message   the signed message M.
    /// @param sig       the packed adapted signature: c_tilde(32) ‖ BitPack19(z).
    /// @return true iff the signature verifies.
    function verify(
        uint256[][] memory AprimeHat,
        uint256[][] memory t,
        bytes memory message,
        bytes memory sig
    ) internal pure returns (bool) {
        if (sig.length != SIG_BYTES) return false;

        // 1–2. decode z and check the norm bound.
        uint256[][] memory z = _decodeZ(sig);
        if (!_normOk(z)) return false;

        // 3–4. w' = z_top + A'·z_bot − c·t (and pack(t) for the oracle).
        (uint256[][] memory wprime, bytes memory tPacked) = _wprime(AprimeHat, t, sig, z);

        // 5. c_tilde' = SHAKE256( pack(t) ‖ pack(w') ‖ M ); accept iff == c_tilde.
        bytes memory wPacked = _packPolys(wprime, N_LAS);
        CtxShake memory ctx = shakeInit();
        ctx = shakeUpdate(ctx, tPacked);
        ctx = shakeUpdate(ctx, wPacked);
        ctx = shakeUpdate(ctx, message);
        bytes memory cCheck = shakeDigest(ctx, 32);
        for (uint256 i = 0; i < 32; i++) if (cCheck[i] != sig[i]) return false;
        return true;
    }

    /// Debug/validation entry point: returns the reconstructed w' (Stage-5 golden
    /// w_prime.bin), so the arithmetic can be validated in isolation from the hash.
    function computeWPrime(uint256[][] memory AprimeHat, uint256[][] memory t, bytes memory sig)
        internal
        pure
        returns (uint256[][] memory wprime)
    {
        uint256[][] memory z = _decodeZ(sig);
        (wprime,) = _wprime(AprimeHat, t, sig, z);
    }

    /// Steps 3–4: challenge, then w'[i] = z_top[i] + (A'·z_bot)[i] − (c·t)[i]
    /// (mod q, canonical). Returns pack(t) too since it must be taken from the
    /// NORMAL-domain t before the c·t NTT destroys it. A' is ALREADY in NTT domain.
    function _wprime(uint256[][] memory AprimeHat, uint256[][] memory t, bytes memory sig, uint256[][] memory z)
        private
        pure
        returns (uint256[][] memory wprime, bytes memory tPacked)
    {
        // 3. challenge polynomial from the digest c_tilde = sig[0:32].
        bytes memory cTilde = new bytes(32);
        for (uint256 i = 0; i < 32; i++) cTilde[i] = sig[i];
        uint256[] memory cHat = sampleInBallNist(cTilde, KAPPA, Q); // {0,1,Q-1}

        // Pack t (canonical, NORMAL domain) BEFORE any NTT destroys it.
        tPacked = _packPolys(t, N_LAS);

        // Forward NTTs: the ell columns of z_bot (reused across rows), and c.
        uint256[][] memory zbotHat = new uint256[][](ELL);
        for (uint256 j = 0; j < ELL; j++) zbotHat[j] = nttFw(z[N_LAS + j]);
        cHat = nttFw(cHat);

        // 4. w'[i] = z_top[i] + (A'·z_bot)[i] − (c·t)[i], canonical mod q.
        wprime = new uint256[][](N_LAS);
        for (uint256 i = 0; i < N_LAS; i++) {
            uint256[] memory acc = new uint256[](N); // A'·z_bot row i, NTT domain
            for (uint256 j = 0; j < ELL; j++) {
                acc = _vecAddMod(acc, vecMulMod(AprimeHat[i * ELL + j], zbotHat[j]));
            }
            uint256[] memory az = nttInv(acc);                          // normal domain
            uint256[] memory ct = nttInv(vecMulMod(cHat, nttFw(t[i]))); // c·t, normal domain
            uint256[] memory ztop = z[i];
            uint256[] memory w = new uint256[](N);
            for (uint256 k = 0; k < N; k++) {
                w[k] = addmod(addmod(ztop[k], az[k], Q), Q - ct[k], Q);
            }
            wprime[i] = w;
        }
    }

    /// Debug/validation: the oracle digest c_tilde' = SHAKE256(pack(t)‖pack(w')‖M),
    /// exposed so pack+hash can be validated with a known-correct w' (w_prime.bin).
    function oracle(uint256[][] memory t, uint256[][] memory wprime, bytes memory message)
        internal
        pure
        returns (bytes memory)
    {
        CtxShake memory ctx = shakeInit();
        ctx = shakeUpdate(ctx, _packPolys(t, N_LAS));
        ctx = shakeUpdate(ctx, _packPolys(wprime, N_LAS));
        ctx = shakeUpdate(ctx, message);
        return shakeDigest(ctx, 32);
    }

    // ---- helpers -------------------------------------------------------------

    /// Read one LSB-first 19-bit field at bit offset `bitpos` (mirrors br_get, but
    /// by a byte window, not bit-by-bit). A 19-bit field with a 0..7 bit offset spans
    /// at most 26 bits => 4 bytes; the 4th byte is guarded so the final field (whose
    /// window would run one byte past the 6720-byte signature) does not read OOB —
    /// those out-of-range bits are masked away by the 19-bit mask regardless.
    function _readField(bytes memory buf, uint256 bitpos) private pure returns (uint256 f) {
        uint256 byteOff = bitpos >> 3;
        uint256 len = buf.length;
        uint256 word = uint256(uint8(buf[byteOff]));
        if (byteOff + 1 < len) word |= uint256(uint8(buf[byteOff + 1])) << 8;
        if (byteOff + 2 < len) word |= uint256(uint8(buf[byteOff + 2])) << 16;
        if (byteOff + 3 < len) word |= uint256(uint8(buf[byteOff + 3])) << 24;
        f = (word >> (bitpos & 7)) & 0x7FFFF; // 19-bit mask
    }

    /// Decode the z region (after the 32-byte c_tilde) into canonical [0,Q).
    /// z = LAS_Z_OFFSET − field  (ref/serialize.c decode_chal_response).
    function _decodeZ(bytes memory sig) private pure returns (uint256[][] memory z) {
        z = new uint256[][](N_PLUS_ELL);
        uint256 bit = 32 * 8;
        for (uint256 i = 0; i < N_PLUS_ELL; i++) {
            z[i] = new uint256[](N);
            for (uint256 k = 0; k < N; k++) {
                int256 zc = int256(Z_OFFSET) - int256(_readField(sig, bit));
                bit += Z_BITS;
                z[i][k] = zc >= 0 ? uint256(zc) : uint256(int256(Q) + zc);
            }
        }
    }

    /// ‖z‖∞ ≤ BOUND using the centred absolute value of each residue.
    function _normOk(uint256[][] memory z) private pure returns (bool) {
        for (uint256 i = 0; i < N_PLUS_ELL; i++) {
            for (uint256 k = 0; k < N; k++) {
                uint256 zc = z[i][k];
                uint256 absz = zc <= Q / 2 ? zc : Q - zc;
                if (absz > BOUND) return false;
            }
        }
        return true;
    }

    /// Elementwise (a + b) mod Q.
    function _vecAddMod(uint256[] memory a, uint256[] memory b) private pure returns (uint256[] memory r) {
        r = new uint256[](a.length);
        for (uint256 k = 0; k < a.length; k++) r[k] = addmod(a[k], b[k], Q);
    }

    /// Pack `count` canonical polynomials, 4 bytes little-endian per coefficient
    /// (mirrors ref/basesig.c b_polyw_pack, the oracle's pack(t)/pack(w')).
    function _packPolys(uint256[][] memory polys, uint256 count) private pure returns (bytes memory out) {
        out = new bytes(count * N * 4);
        uint256 o = 0;
        for (uint256 i = 0; i < count; i++) {
            uint256[] memory p = polys[i];
            for (uint256 k = 0; k < N; k++) {
                uint256 x = p[k];
                out[o] = bytes1(uint8(x));
                out[o + 1] = bytes1(uint8(x >> 8));
                out[o + 2] = bytes1(uint8(x >> 16));
                out[o + 3] = bytes1(uint8(x >> 24));
                o += 4;
            }
        }
    }
}
