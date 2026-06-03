#include <stddef.h>
#include <stdint.h>
#include "amhl.h"
#include "las.h"
#include "poly.h"

/* Zero a witness / statement vector (s_0 = 0, Y_0 = 0). */
static void zero_sk(las_sk *s) {
  unsigned int i, k;
  for(i = 0; i < LAS_M; ++i)
    for(k = 0; k < N; ++k)
      s->s[i].coeffs[k] = 0;
}
static void zero_pk(las_pk *Y) {
  unsigned int i, k;
  for(i = 0; i < LAS_N; ++i)
    for(k = 0; k < N; ++k)
      Y->t[i].coeffs[k] = 0;
}

void amhl_setup_gen(amhl_setup *st, unsigned int nhops, const las_pp *pp) {
  unsigned int j, i;
  las_pk Lj;                       /* L_j = A * l_j (the increment's statement) */

  if(nhops < 1) nhops = 1;
  if(nhops > AMHL_MAXK) nhops = AMHL_MAXK;
  st->nhops = nhops;

  zero_sk(&st->cum[0]);            /* s_0 = 0 */
  zero_pk(&st->Y[0]);             /* Y_0 = 0 */

  for(j = 1; j <= nhops; ++j) {
    /* Reuse KeyGen: l_j <- S_1 (ternary) and L_j = A*l_j in one call. */
    las_keygen(&Lj, &st->incr[j - 1], pp);

    /* Cumulative witness  s_j = s_{j-1} + l_j  (kept small/centred). */
    for(i = 0; i < LAS_M; ++i) {
      poly_add(&st->cum[j].s[i], &st->cum[j - 1].s[i], &st->incr[j - 1].s[i]);
      poly_reduce(&st->cum[j].s[i]);
    }
    /* Cumulative statement  Y_j = Y_{j-1} + L_j = A*s_j  (canonical [0,Q)). */
    for(i = 0; i < LAS_N; ++i) {
      poly_add(&st->Y[j].t[i], &st->Y[j - 1].t[i], &Lj.t[i]);
      poly_reduce(&st->Y[j].t[i]);
      poly_caddq(&st->Y[j].t[i]);
    }
  }
}

int32_t amhl_norm(const las_sk *s) {
  unsigned int i, k;
  int32_t m = 0, a;
  for(i = 0; i < LAS_M; ++i)
    for(k = 0; k < N; ++k) {
      a = s->s[i].coeffs[k];
      if(a < 0) a = -a;
      if(a > m) m = a;
    }
  return m;
}

void amhl_recover_prev(las_sk *prev, const las_sk *cur, const las_sk *incr) {
  unsigned int i;
  for(i = 0; i < LAS_M; ++i) {
    poly_sub(&prev->s[i], &cur->s[i], &incr->s[i]);
    poly_reduce(&prev->s[i]);
  }
}
