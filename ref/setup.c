/*
 * setup.c -- the SHARED system setup (paper Setup() -> pp), consumed by EVERY
 * layer (ref/relation.c, ref/serialize.c, ref/basesig.c, ref/las.c).  See
 * setup.h for why this is a separate file: A is fixed public infrastructure,
 * not scheme code -- no layer owns it.
 */
#include <stdint.h>
#include "params.h"
#include "setup.h"
#include "poly.h"

/*************************************************
* Name:        setup_public_params  (shared system setup -- consumed by every
*              layer; no basesig.c/sign.c slot)
*
* Description: Public parameters pp = A = [I | A']: expand A' from a public
*              seed into the NTT domain, poly by poly with the upstream
*              poly_uniform.  A is expanded ONCE here and passed BY PARAMETER
*              into every scheme function alike -- that is why the scheme files
*              carry [DELETED] notes for sign.c's polyvec_matrix_expand
*              (upstream must re-expand A from rho inside every call because rho
*              travels inside each packed key).
**************************************************/
void setup_public_params(public_params *pp,   /* paper A: pp = A = [I | A']; pp->a_prime = A' (NTT domain) */
                         const uint8_t seed[LAS_SEEDBYTES]) {  /* public seed expanding A' (no paper symbol) */
  unsigned int i, j;  /* row / column indices over A' (no paper symbol) */
  for(i = 0; i < LAS_SEEDBYTES; ++i)
    pp->seed[i] = seed[i];
  for(i = 0; i < LAS_N; ++i)
    for(j = 0; j < ELL; ++j)
      poly_uniform(&pp->a_prime[i][j], seed, (uint16_t)((i << 8) + j));
}
