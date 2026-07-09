#ifndef LAS_SETUP_H
#define LAS_SETUP_H

/*
 * setup.{c,h} -- the SHARED system setup, i.e. the paper's Setup() -> pp
 * (eprint 2020/845): the public parameters (n, l, kappa, gamma) and the
 * public matrix A = [I | A'], expanded once from a public seed.
 *
 * This layer is deliberately a SEPARATE file because it is NOT scheme
 * specific: BOTH ref/basesig.c (Algorithm 1, the base signature) and
 * ref/las.c (Algorithm 2, the adaptor scheme) consume the same las_pp by
 * parameter.  That is exactly why both scheme files carry [DELETED] notes
 * for sign.c's polyvec_matrix_expand (basesig.c:136-140, basesig.c:267-269,
 * basesig.c:550-552): upstream must re-expand A from rho inside every call
 * because rho travels inside each packed key, whereas here A is fixed
 * public infrastructure set up once.
 */

#include <stdint.h>
#include "poly.h"      /* poly type; pulls in params.h => N, Q (mode-independent) */

/* ---- LAS parameters (paper Section 3 / Table). Self-contained. ----
 *
 * The three primitive parameters n, ell, kappa are OVERRIDABLE at compile time
 * (-DLAS_N=.. -DLAS_ELL=.. -DLAS_KAPPA=..) so the scheme can be instantiated at
 * parameter sets matched to each NIST security level for a FAIR same-security
 * comparison (see ref/test/bench_levels.c).  The defaults are the paper set, so
 * every existing target builds unchanged.
 *
 *   Set        (n, ell, kappa)   matches Dilithium mode (K,L,tau)   ~NIST level
 *   LAS-paper  (4, 4, 60)        - (paper's own choice)             -
 *   LAS@D2     (4, 4, 39)        Dilithium-2 (4,4,39)               2
 *   LAS@D3     (6, 5, 49)        Dilithium-3 (6,5,49)               3
 *   LAS@D5     (8, 7, 60)        Dilithium-5 (8,7,60)               5
 *
 * Matching n<->K, ell<->L makes the public-key and secret dimensions equal to
 * Dilithium's; matching kappa<->tau makes the challenge weight equal.  This is a
 * DIMENSION-level (not formally bit-security) match; security proofs are out of
 * project scope. */
#ifndef LAS_N
#define LAS_N      4                          /* n   : rows of A, dim of t (=Y) */
#endif
#ifndef LAS_ELL
#define LAS_ELL    4                          /* l   : extra columns of A       */
#endif
#ifndef LAS_KAPPA
#define LAS_KAPPA  60                         /* k   : challenge weight ||c||_1 */
#endif
#define LAS_M      (LAS_N + LAS_ELL)          /* n+l : dim of r, y, z           */
#define LAS_GAMMA  ((int32_t)LAS_KAPPA * 256 * LAS_M)  /* g = k*d*(n+l), d=N=256 */
#define LAS_SEEDBYTES 32

/*
 * Note on the modulus: the paper specifies q ~ 2^24.  We reuse Dilithium's NTT,
 * whose root-of-unity table is fixed to Q = 8380417 (~2^23), so this build uses
 * that Q.  It comfortably satisfies Q > 2*GAMMA, so correctness is unaffected;
 * only the concrete MSIS/MLWE security margin differs (out of scope per CONTEXT).
 */

/* ---- Shared object types (vectors are plain arrays of the repo's degree-N
 * poly).  These live HERE, below both schemes, for the same reason upstream
 * keeps polyvecl/polyveck in polyvec.h rather than sign.h: the key/signature
 * LAYOUT is common infrastructure -- basesig.c, las.c and serialize.c all
 * operate on the same structs, which is what makes a LAS-adapted signature
 * verifiable by the independent base verifier and one codec serve both. ---- */
typedef struct { poly mat[LAS_N][LAS_ELL];          /* A' in NTT domain */
                 uint8_t seed[LAS_SEEDBYTES]; } las_pp;  /* public parameters pp = A */
typedef struct { poly t[LAS_N]; } las_pk;           /* public key / statement  t = A r */
typedef struct { poly s[LAS_M]; } las_sk;           /* secret key / witness    r in S_1 */
typedef struct { poly c; poly z[LAS_M]; } las_sig;  /* (pre-)signature (c, z)  */

/* Setup() -> pp: expand A' from a public seed (shared by basesig.c AND las.c). */
void las_setup(las_pp *pp, const uint8_t seed[LAS_SEEDBYTES]);

#endif
