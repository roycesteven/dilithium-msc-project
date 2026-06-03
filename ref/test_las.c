#include <stdio.h>
#include <string.h>

#include "las_dummy.h"
#include "polyvec.h"
#include "params.h"

static void set_small_witness(las_vec *w) {
  las_vec_zero(w);

  /* left part (K polys) */
  w->vec[0].coeffs[0] = 1;
  w->vec[1].coeffs[1] = -1;
  w->vec[2].coeffs[2] = 1;
  w->vec[3].coeffs[3] = 0;

  /* right part (L polys) */
  w->vec[K + 0].coeffs[0] = 1;
  w->vec[K + 1].coeffs[1] = 1;
  w->vec[K + 2].coeffs[2] = -1;
  w->vec[K + 3].coeffs[3] = 0;
}

static void set_dummy_presig(las_presig *ps) {
  memset(ps, 0, sizeof(*ps));

  /* dummy challenge c */
  ps->c.coeffs[0] = 1;
  ps->c.coeffs[1] = -1;

  /* dummy zhat */
  ps->zhat.vec[0].coeffs[0] = 3;
  ps->zhat.vec[1].coeffs[1] = 2;
  ps->zhat.vec[K + 0].coeffs[2] = -2;
  ps->zhat.vec[K + 1].coeffs[3] = 1;
}

int main(void) {
  uint8_t rho[SEEDBYTES];
  polyvecl mat[K];
  las_vec witness;
  las_vec extracted;
  las_presig ps;
  las_sig sig;
  polyveck Y;

  memset(rho, 0, sizeof(rho));
  polyvec_matrix_expand(mat, rho);

  set_small_witness(&witness);
  set_dummy_presig(&ps);

  las_statement_from_witness(&Y, mat, &witness);

  las_adapt(&sig, &ps, &witness);

  if (las_extract(&extracted, mat, &Y, &sig, &ps) != 0) {
    printf("las_extract: FAIL\n");
    return 1;
  }

  if (!las_vec_equal(&witness, &extracted)) {
    printf("witness mismatch: FAIL\n");
    return 1;
  }

  printf("LAS Adapt/Extract prototype: PASS\n");
  return 0;
}