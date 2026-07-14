#ifndef LAS_SETUP_H
#define LAS_SETUP_H

/*
 * setup.{c,h} -- the SHARED system setup, i.e. the paper's Setup() -> pp
 * (eprint 2020/845): the construction parameters (n, ell, kappa, gamma) and the
 * public matrix A = [I | A'] (pp = (A, H)), expanded once from a public seed.
 * The six protocol object types moved to las_types.h (C twin of the Rust
 * src/las_types.rs split); only public_params stays here.
 *
 * This layer is deliberately a SEPARATE file because it is NOT scheme
 * specific: EVERY layer (ref/relation.c, ref/serialize.c, ref/basesig.c and
 * ref/las.c) consumes the same construction by parameter.  That is exactly why
 * both scheme files carry [DELETED] notes for sign.c's polyvec_matrix_expand:
 * upstream must re-expand A from rho inside every call because rho travels
 * inside each packed key, whereas here A is fixed public infrastructure set up
 * once.
 */

#include <stdint.h>
#include "poly.h"      /* poly type; pulls in params.h => N, Q (mode-independent) */

/* ---- LAS construction parameters (paper Section 3 / Table). Self-contained. ----
 *
 * The three primitive parameters LAS_N (n), ELL (ell), KAPPA (kappa) are
 * OVERRIDABLE at compile time (-DLAS_N=.. -DELL=.. -DKAPPA=..) so the scheme
 * can be instantiated at parameter sets matched to each NIST security level for
 * a FAIR same-security comparison (see ref/test/bench_levels.c).  The defaults
 * are the paper set, so every existing target builds unchanged.
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
 * project scope.
 *
 * NAMING (mirrors the Rust port setup.rs): the construction parameters carry
 * NO LAS_ prefix -- n -> LAS_N, ell -> ELL, kappa -> KAPPA, gamma -> GAMMA,
 * n+ell -> N_PLUS_ELL, ring degree d -> LAS_D.  The two exceptions, LAS_N and
 * LAS_D, keep the LAS_ prefix ONLY because C's params.h already defines bare
 * `N` (=256, the ring degree) and `D` (=13, Power2Round) as load-bearing
 * macros for the reused Dilithium primitives; LAS_N (module rank) and LAS_D
 * (ring degree alias) must not collide with them.  The two per-scheme rejection
 * bounds do NOT live here: BOUND_SIGN (Algorithm 1) is in basesig.h,
 * BOUND_PRESIGN (Algorithm 2) is in las.h. */
#ifndef LAS_N
#define LAS_N      4                          /* n   : rows of A, dim of t (=Y) */
#endif
#ifndef ELL
#define ELL        4                          /* ell : extra columns of A       */
#endif
#ifndef KAPPA
#define KAPPA      60                         /* kappa : challenge weight ||c||_1 */
#endif
#define N_PLUS_ELL (LAS_N + ELL)              /* n+ell : dim of r, y, z         */
#define LAS_D      N                          /* d : ring degree = params.h N = 256
                                               * (aliased to dodge params.h N/D) */
#define GAMMA      ((int32_t)KAPPA * LAS_D * N_PLUS_ELL)  /* gamma = kappa*d*(n+ell) */
#define LAS_SEEDBYTES 32
/* Challenge-hash length in bytes: the stored digest c_tilde, the implementation
 * realisation of the paper's H : {0,1}* -> C (the paper's challenge c IS this
 * hash -- eq. 7 counts it as the 32-byte term of |sigma|).  Twin of upstream
 * CTILDEBYTES (params.h); same value as LAS_SEEDBYTES but a DISTINCT knob, as
 * upstream keeps the two separate.  Stored in signature/pre_signature; the
 * challenge polynomial c = SampleInBall(c_tilde) is only ever a local value. */
#define LAS_CTILDEBYTES 32

/*
 * Note on the modulus: the paper specifies q ~ 2^24.  We reuse Dilithium's NTT,
 * whose root-of-unity table is fixed to Q = 8380417 (~2^23), so this build uses
 * that Q.  It comfortably satisfies Q > 2*GAMMA, so correctness is unaffected;
 * only the concrete MSIS/MLWE security margin differs (out of scope per CONTEXT).
 */

/* ---- The construction-wide public parameters pp = (A, H) (paper Section 3);
 * A' is held in the NTT domain.  This is the shared public setup, not a
 * protocol object.  The six protocol object types (public_key, secret_key,
 * signature, statement, witness, pre_signature) live in las_types.h, directly
 * below this layer. ---- */
typedef struct { poly a_prime[LAS_N][ELL];          /* A' in NTT domain */
                 uint8_t seed[LAS_SEEDBYTES]; } public_params;  /* pp = (A,H); A = [I|A'] */

/* Setup() -> pp: expand A' from a public seed (shared by every layer). */
void setup_public_params(public_params *pp, const uint8_t seed[LAS_SEEDBYTES]);

#endif
