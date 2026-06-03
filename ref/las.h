#ifndef LAS_H
#define LAS_H

/*
 * LAS - Lattice-based Adaptor Signature (Esgin, Ersoy, Erkin, eprint 2020/845,
 * Algorithm 2), implemented as the paper's SIMPLIFIED scheme on top of the
 * CRYSTALS-Dilithium reference primitives.
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

/* ---- LAS parameters (paper Section 3 / Table). Self-contained. ---- */
#define LAS_N      4                          /* n   : rows of A, dim of t (=Y) */
#define LAS_ELL    4                          /* l   : extra columns of A       */
#define LAS_M      (LAS_N + LAS_ELL)          /* n+l : dim of r, y, z           */
#define LAS_KAPPA  60                         /* k   : challenge weight ||c||_1 */
#define LAS_GAMMA  122880                     /* g = k*d*(n+l) = 60*256*8       */
#define LAS_SEEDBYTES 32

/*
 * Note on the modulus: the paper specifies q ~ 2^24.  We reuse Dilithium's NTT,
 * whose root-of-unity table is fixed to Q = 8380417 (~2^23), so this build uses
 * that Q.  It comfortably satisfies Q > 2*GAMMA, so correctness is unaffected;
 * only the concrete MSIS/MLWE security margin differs (out of scope per CONTEXT).
 */

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

/* ---- Types (vectors are plain arrays of the repo's degree-N poly) ---- */
typedef struct { poly mat[LAS_N][LAS_ELL];          /* A' in NTT domain */
                 uint8_t seed[LAS_SEEDBYTES]; } las_pp;
typedef struct { poly t[LAS_N]; } las_pk;           /* public key / statement  t = A r */
typedef struct { poly s[LAS_M]; } las_sk;           /* secret key / witness    r in S_1 */
typedef struct { poly c; poly z[LAS_M]; } las_sig;  /* (pre-)signature (c, z)  */

/* Public parameters pp = A (expanded from a public seed). */
void las_setup(las_pp *pp, const uint8_t seed[LAS_SEEDBYTES]);

/* KeyGen = Gen: r<-S_1^(n+l); t=Ar; (pk,sk)=(t,r).  Also used to make (Y,y). */
void las_keygen(las_pk *pk, las_sk *sk, const las_pp *pp);

/* Sign / Verify (ordinary signature; Verify returns 0 on success). */
void las_sign(las_sig *sig, const uint8_t *m, size_t mlen,
              const las_pk *pk, const las_sk *sk, const las_pp *pp);
int  las_verify(const las_sig *sig, const uint8_t *m, size_t mlen,
                const las_pk *pk, const las_pp *pp);

/* PreSign(sk,Y,M) / PreVerify(Y,pk,sigma^,M) (returns 0 on success). */
void las_presign(las_sig *presig, const uint8_t *m, size_t mlen,
                 const las_pk *Y, const las_pk *pk, const las_sk *sk, const las_pp *pp);
int  las_preverify(const las_sig *presig, const uint8_t *m, size_t mlen,
                   const las_pk *Y, const las_pk *pk, const las_pp *pp);

/* AMHL K-hop variants (eprint 2020/845 Fig. 2 / Section 5).  Identical to
 * PreSign/PreVerify except the rejection bound is the tighter g-k-K, reserving a
 * norm budget of K for a cumulative witness of infinity-norm up to K.  Adapt and
 * Ext are unchanged: the adapted signature is still an ordinary signature and the
 * extracted value is the cumulative witness s_j with A*s_j == Y_j. */
void las_presign_k(las_sig *presig, const uint8_t *m, size_t mlen,
                   const las_pk *Y, const las_pk *pk, const las_sk *sk,
                   const las_pp *pp, unsigned int nhops);
int  las_preverify_k(const las_sig *presig, const uint8_t *m, size_t mlen,
                     const las_pk *Y, const las_pk *pk, const las_pp *pp,
                     unsigned int nhops);

/* Adapt((Y,y),sigma^): PreVerify, then sigma=(c, z^+y).  Returns 0 on success. */
int  las_adapt(las_sig *sig, const las_sig *presig, const uint8_t *m, size_t mlen,
               const las_pk *Y, const las_sk *y, const las_pk *pk, const las_pp *pp);

/* Ext(Y,sigma,sigma^): s=z-z^; returns 0 and s iff A*s==Y, else -1. */
int  las_ext(las_sk *y, const las_sig *sig, const las_sig *presig,
             const las_pk *Y, const las_pp *pp);

#endif
