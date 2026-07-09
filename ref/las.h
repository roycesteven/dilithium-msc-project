#ifndef LAS_H
#define LAS_H

/*
 * LAS - Lattice-based Adaptor Signature (Esgin, Ersoy, Erkin, eprint 2020/845,
 * Algorithm 2), implemented as the paper's SIMPLIFIED scheme on top of the
 * CRYSTALS-Dilithium reference primitives.
 *
 * las.{c,h} are STRUCTURED AS A MIRROR of ref/basesig.{c,h} (which in turn
 * mirrors the upstream ref/sign.{c,h}), so provenance tracks all the way to
 * the uppermost upstream by a uniform prefix swap crypto_sign* -> base_sign*
 * -> las*:
 *
 *   -- Algorithm 1 (base path) --
 *   crypto_sign_keypair            -> base_sign_keypair            -> las_keypair
 *   crypto_sign_signature_internal -> base_sign_signature_internal -> las_signature_internal
 *   crypto_sign_signature          -> base_sign_signature          -> las_signature
 *   crypto_sign                    -> base_sign                    -> las_sign
 *   crypto_sign_verify_internal    -> base_sign_verify_internal    -> las_verify_internal
 *   crypto_sign_verify             -> base_sign_verify             -> las_verify
 *   crypto_sign_open               -> base_sign_open               -> las_open
 *   plus two LAS-only deterministic KAT slots (no upstream analogue):
 *   las_keypair_seed, las_signature_det.
 *
 *   -- Algorithm 2 (adaptor layer; upstream = the PAPER, names kept) --
 *   las_presign_internal / las_presign / las_presign_det /
 *   las_preverify_internal / las_preverify / las_presign_k / las_preverify_k /
 *   las_adapt / las_ext.
 *
 * This is deliberately NOT optimized Dilithium: there is no Power2Round, no
 * hint vector, and no high/low-bit decomposition.  We reuse only the repo's
 * mode-independent primitives - the NTT, SHAKE/Keccak, modular poly arithmetic
 * (mod Q, degree N) and rejection sampling.  All LAS parameters below are
 * self-contained and independent of DILITHIUM_MODE.
 *
 * Relation (hard MSIS/MLWE):  statement Y = A*y mod q, witness y in S_1.
 * A statement/witness pair is literally just another key pair.
 *
 *   A = [ I_n | A' ] in R_q^{n x (n+l)},  R_q = Z_q[X]/(X^N + 1)
 *   (paper<->code: paper ring degree d = code N = 256; paper module rank n = code LAS_N)
 *
 * Sign:      y<-S_g; w=Ay; c=H(pk, w,   M); z =y+c*r; |z|inf<=g-k
 * PreSign:   y<-S_g; w=Ay; c=H(pk, w+Y, M); z^=y+c*r; |z^|inf<=g-k-1   (Y folded in!)
 * PreVerify: w'=Az^-c*t; check c==H(pk, w'+Y, M)
 * Adapt:     z = z^ + y_wit   (standard Verify then sees Az-ct = w+Y)
 * Ext:       y_wit = z - z^
 */

#include <stddef.h>
#include <stdint.h>
#include "poly.h"      /* poly type; pulls in params.h => N, Q (mode-independent) */
#include "setup.h"     /* SHARED system setup: parameters (LAS_N/ELL/KAPPA/M/GAMMA/
                        * SEEDBYTES), the shared object types and las_setup() -- a
                        * separate file because basesig.c consumes the same setup
                        * (paper Setup() -> pp is not scheme-specific) */
#include "serialize.h" /* SHARED wire codec (the packing.{c,h} twin): byte sizes
                        * LAS_{PK,SK,SIG}_BYTES used by the end-to-end packed-API
                        * tier declared at the bottom of this header */

/* poly_chknorm() rejects when ||.||inf >= bound, so encode the strict ">" tests
 * as bound = (limit)+1. */
#define LAS_BOUND_SIGN     (LAS_GAMMA - LAS_KAPPA + 1)  /* reject |z|inf  > g-k   */
#define LAS_BOUND_PRESIGN  (LAS_GAMMA - LAS_KAPPA)      /* reject |z^|inf > g-k-1 */

/* AMHL K-hop PreSign bound: reject |z^|inf > g-k-K, i.e. accept <= g-k-K.
 * Leaves a norm budget of K for the cumulative witness s_j = l_1+...+l_j
 * (||s_j||inf <= j <= K), so the adapted z = z^ + s_j still satisfies
 * ||z||inf <= (g-k-K) + K = g-k and clears the ordinary Verify bound.
 * For K=1 this collapses to LAS_BOUND_PRESIGN (the single-hop case). */
#define LAS_BOUND_PRESIGN_K(K)  (LAS_GAMMA - LAS_KAPPA - (int32_t)(K) + 1)

/* ---- Types: las_pp / las_pk / las_sk / las_sig live in setup.h (the shared
 * system layer, below both schemes and the codec), mirroring how upstream
 * keeps polyvecl/polyveck in polyvec.h rather than sign.h. ---- */

/* ---- Rejection-sampling instrumentation (measurement only) ----
 * Counts the total number of rejection-loop attempts performed by
 * las_signature, las_presign and las_presign_k since it was last reset.  It
 * does NOT affect the scheme logic in any way; it exists so benchmarks can
 * report the average restart count DIRECTLY (objectives Part D) rather than
 * estimating it from a timing ratio.  One signing call performs (retries + 1)
 * attempts, so the mean attempts/op = las_attempts / (#ops) and mean
 * retries/op = that minus one.
 * Single-threaded use only (the benchmarks are single-threaded). */
extern unsigned long las_attempts;

/* EXACT expected attempts/call of the rejection loop running at `bound`
 * (LAS_BOUND_SIGN or LAS_BOUND_PRESIGN), for validating a measured attempt
 * counter against theory -- the benchmarks' run-validity rejection gate
 * (mirrors the Rust port's las_expected_attempts; instrumentation only,
 * never used by the scheme).  Derivation, verified against eprint 2020/845:
 * the mask coefficient is uniform on [-gamma, gamma] (2*gamma+1 values;
 * Table 1, S_c = {f : |f|inf <= c}) and the secret-dependent shift obeys
 * |c*r|inf <= kappa (Fact 1), so for any bound <= gamma-kappa+1 the chknorm
 * acceptance window |z_i| <= bound-1 (2*bound-1 values; Alg. 1 step 11
 * "reject |z|inf > gamma-kappa" resp. Alg. 2 step 6 "reject |z^|inf >
 * gamma-kappa-1") always lies inside the shifted mask support: each of the
 * (n+ell)*d coefficients accepts independently with probability exactly
 * (2*bound-1)/(2*gamma+1), the attempt count is geometric, and
 *     E[attempts] = ((2*bound-1)/(2*gamma+1))^-((n+ell)*d)
 * -- the exact form of the paper's Section 3.2 design target "the average
 * number of restarts in Sign and PreSign is about e < 3".  At the D3
 * engineering set (6,5,49): Sign 2.71875, PreSign 2.77483 attempts/call. */
double las_expected_attempts(int32_t bound);

/* ==================== Algorithm 1 (base path) ====================
 * Declarations follow basesig.h (which follows sign.h) one-to-one. */

/* (las_setup lives in ref/setup.{c,h}: the SHARED system setup, consumed by
 * basesig.c and las.c alike -- A is public infrastructure, not scheme code.) */

/* las_keypair  <->  base_sign_keypair (basesig.h)
 * KeyGen = Gen: r<-S_1^(n+l); t=Ar; (pk,sk)=(t,r).  Also used to make (Y,y).
 * Returns 0 (success). */
int las_keypair(las_pk *pk, las_sk *sk, const las_pp *pp);

/* Deterministic KeyGen from an explicit 32-byte seed (reproducible KAT
 * vectors).  No basesig.h/sign.h slot.  Returns 0 (success). */
int las_keypair_seed(las_pk *pk, las_sk *sk, const las_pp *pp,
                     const uint8_t seed[LAS_SEEDBYTES]);

/* las_signature_internal  <->  base_sign_signature_internal (basesig.h)
 * Sign body, parameterised by the 64-byte mask seed.  Returns 0 (success). */
int las_signature_internal(las_sig *sig,
                           const uint8_t *m,
                           size_t mlen,
                           const las_pk *pk,
                           const las_sk *sk,
                           const las_pp *pp,
                           const uint8_t seed[64]);

/* las_signature  <->  base_sign_signature (basesig.h)
 * Sign, random path (fresh mask seed).  Returns 0 (success). */
int las_signature(las_sig *sig,
                  const uint8_t *m, size_t mlen,
                  const las_pk *pk, const las_sk *sk,
                  const las_pp *pp);

/* Deterministic Sign: the per-signature mask randomness is derived from
 * (sk, M), so the output is a deterministic function of its inputs.  Same
 * distribution and validity as las_signature; removes the per-signature RNG
 * (no nonce-reuse risk) and enables reproducible known-answer tests.
 * No basesig.h/sign.h slot.  Returns 0 (success). */
int las_signature_det(las_sig *sig,
                      const uint8_t *m, size_t mlen,
                      const las_pk *pk, const las_sk *sk,
                      const las_pp *pp);

/* las_sign  <->  base_sign (basesig.h)
 * Compute signed message.  The signature is a struct, so sm = M (no packed
 * prefix; the byte-level wire interface is ref/serialize.{c,h}).
 * Returns 0 (success). */
int las_sign(las_sig *sig,
             uint8_t *sm, size_t *smlen,
             const uint8_t *m, size_t mlen,
             const las_pk *pk, const las_sk *sk,
             const las_pp *pp);

/* las_verify_internal  <->  base_sign_verify_internal (basesig.h)
 * Verify body: w' = A z - c t; accept iff c == H(pk, w', M).
 * Returns 0 on success, -1 otherwise. */
int las_verify_internal(const las_sig *sig,
                        const uint8_t *m,
                        size_t mlen,
                        const las_pk *pk,
                        const las_pp *pp);

/* las_verify  <->  base_sign_verify (basesig.h)
 * Verify, public entry point.  Returns 0 on success, -1 otherwise. */
int las_verify(const las_sig *sig,
               const uint8_t *m, size_t mlen,
               const las_pk *pk,
               const las_pp *pp);

/* las_open  <->  base_sign_open (basesig.h)
 * Verify signed message (sm holds only M; the signature travels as a struct
 * beside it).  Returns 0 on success, -1 otherwise. */
int las_open(uint8_t *m, size_t *mlen,
             const las_sig *sig,
             const uint8_t *sm, size_t smlen,
             const las_pk *pk,
             const las_pp *pp);

/* ==================== Algorithm 2 (adaptor layer) ====================
 * No basesig.h/sign.h analogue: the four adaptor operations LAS adds, plus
 * their deterministic and AMHL K-hop variants.  Upstream = the PAPER. */

/* PreSign body, parameterised by the rejection bound (g-k-1 single-hop,
 * g-k-K AMHL) and the 64-byte mask seed.  Adaptor twin of
 * las_signature_internal.  Returns 0 (success). */
int las_presign_internal(las_sig *presig,
                         const uint8_t *m, size_t mlen,
                         const las_pk *Y, const las_pk *pk, const las_sk *sk,
                         const las_pp *pp, int32_t bound,
                         const uint8_t seed[64]);

/* PreSign(sk,Y,M), random path, single-hop bound g-k-1.  Returns 0 (success). */
int las_presign(las_sig *presig, const uint8_t *m, size_t mlen,
                const las_pk *Y, const las_pk *pk, const las_sk *sk,
                const las_pp *pp);

/* Deterministic PreSign: mask randomness derived from (sk, Y, M).  Uses the
 * single-hop bound g-k-1, like las_presign; for reproducible adaptor KATs.
 * Returns 0 (success). */
int las_presign_det(las_sig *presig, const uint8_t *m, size_t mlen,
                    const las_pk *Y, const las_pk *pk, const las_sk *sk,
                    const las_pp *pp);

/* PreVerify body, parameterised by the rejection bound.  Adaptor twin of
 * las_verify_internal.  Returns 0 on success, -1 otherwise. */
int las_preverify_internal(const las_sig *presig, const uint8_t *m, size_t mlen,
                           const las_pk *Y, const las_pk *pk, const las_pp *pp,
                           int32_t bound);

/* PreVerify(Y,pk,sigma^,M), single-hop bound.  Returns 0 on success. */
int las_preverify(const las_sig *presig, const uint8_t *m, size_t mlen,
                  const las_pk *Y, const las_pk *pk, const las_pp *pp);

/* AMHL K-hop variants (eprint 2020/845 Fig. 2 / Section 5).  Identical to
 * PreSign/PreVerify except the rejection bound is the tighter g-k-K, reserving a
 * norm budget of K for a cumulative witness of infinity-norm up to K.  Adapt and
 * Ext are unchanged: the adapted signature is still an ordinary signature and the
 * extracted value is the cumulative witness s_j with A*s_j == Y_j. */
int las_presign_k(las_sig *presig, const uint8_t *m, size_t mlen,
                  const las_pk *Y, const las_pk *pk, const las_sk *sk,
                  const las_pp *pp, unsigned int nhops);
int las_preverify_k(const las_sig *presig, const uint8_t *m, size_t mlen,
                    const las_pk *Y, const las_pk *pk, const las_pp *pp,
                    unsigned int nhops);

/* Adapt((Y,y),sigma^): PreVerify, then sigma=(c, z^+y).  Returns 0 on success. */
int las_adapt(las_sig *sig, const las_sig *presig, const uint8_t *m, size_t mlen,
              const las_pk *Y, const las_sk *y, const las_pk *pk, const las_pp *pp);

/* Ext(Y,sigma,sigma^): s=z-z^; returns 0 and s iff A*s==Y, else -1. */
int las_ext(las_sk *y, const las_sig *sig, const las_sig *presig,
            const las_pk *Y, const las_pp *pp);

/* ============== end-to-end PACKED-API tier (bytes in/out) ==============
 * The SECOND measured boundary.  The struct functions above are the CORE
 * CRYPTO tier (pure computation); these are the END-TO-END tier: byte keys
 * and signatures are unpacked/packed INSIDE the call, exactly the boundary
 * upstream sign.c exposes (it packs/unpacks with packing.h inside its API).
 * Same argument positions as the struct twin, byte buffers in place of
 * structs.  All decoders are VALIDATING: malformed bytes -> -1.
 * las_verify_packed is the on-chain-style verifier entry point (what a
 * poqeth-style integration consumes); see ref/test/test_serde.c. */
int las_keypair_packed(uint8_t pk_b[LAS_PK_BYTES], uint8_t sk_b[LAS_SK_BYTES],
                       const las_pp *pp);
int las_signature_packed(uint8_t sig_b[LAS_SIG_BYTES],
                         const uint8_t *m, size_t mlen,
                         const uint8_t pk_b[LAS_PK_BYTES],
                         const uint8_t sk_b[LAS_SK_BYTES],
                         const las_pp *pp);
int las_verify_packed(const uint8_t sig_b[LAS_SIG_BYTES],
                      const uint8_t *m, size_t mlen,
                      const uint8_t pk_b[LAS_PK_BYTES],
                      const las_pp *pp);
int las_presign_packed(uint8_t presig_b[LAS_SIG_BYTES],
                       const uint8_t *m, size_t mlen,
                       const uint8_t Y_b[LAS_PK_BYTES],
                       const uint8_t pk_b[LAS_PK_BYTES],
                       const uint8_t sk_b[LAS_SK_BYTES],
                       const las_pp *pp);
int las_preverify_packed(const uint8_t presig_b[LAS_SIG_BYTES],
                         const uint8_t *m, size_t mlen,
                         const uint8_t Y_b[LAS_PK_BYTES],
                         const uint8_t pk_b[LAS_PK_BYTES],
                         const las_pp *pp);
int las_adapt_packed(uint8_t sig_b[LAS_SIG_BYTES],
                     const uint8_t presig_b[LAS_SIG_BYTES],
                     const uint8_t *m, size_t mlen,
                     const uint8_t Y_b[LAS_PK_BYTES],
                     const uint8_t y_b[LAS_SK_BYTES],
                     const uint8_t pk_b[LAS_PK_BYTES],
                     const las_pp *pp);
int las_ext_packed(uint8_t y_b[LAS_SK_BYTES],
                   const uint8_t sig_b[LAS_SIG_BYTES],
                   const uint8_t presig_b[LAS_SIG_BYTES],
                   const uint8_t Y_b[LAS_PK_BYTES],
                   const las_pp *pp);

#endif
