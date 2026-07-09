/*
 * setup.c -- the SHARED system setup (paper Setup() -> pp), consumed by BOTH
 * ref/basesig.c and ref/las.c.  See setup.h for why this is a separate file:
 * A is fixed public infrastructure, not scheme code -- neither the base
 * signature nor the adaptor scheme owns it.
 */
#include <stdint.h>
#include "params.h"
#include "setup.h"
#include "poly.h"

/*************************************************
* Name:        las_setup  (shared system setup -- consumed by basesig.c AND
*              las.c; no basesig.c/sign.c slot)
*
* Description: Public parameters pp = A = [I | A']: expand A' from a public
*              seed into the NTT domain, poly by poly with the upstream
*              poly_uniform.  A is expanded ONCE here and passed BY PARAMETER
*              into every basesig.c and las.c scheme function alike -- that
*              is why both files carry [DELETED] notes for sign.c's
*              polyvec_matrix_expand (see basesig.c:136-140, basesig.c:267-269,
*              basesig.c:550-552 -- upstream must re-expand A from rho inside
*              every call because rho travels inside each packed key).
**************************************************/
void las_setup(las_pp *pp,          /* paper A: pp = A = [I | A']; pp->mat = A' (NTT domain) */
               const uint8_t seed[LAS_SEEDBYTES]) {  /* public seed expanding A' (no paper symbol) */
  unsigned int i, j;  /* row / column indices over A' (no paper symbol) */
  for(i = 0; i < LAS_SEEDBYTES; ++i)
    pp->seed[i] = seed[i];
  for(i = 0; i < LAS_N; ++i)
    for(j = 0; j < LAS_ELL; ++j)
      poly_uniform(&pp->mat[i][j], seed, (uint16_t)((i << 8) + j));
}
