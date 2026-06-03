#include "las_dummy.h"

#include <stddef.h>

/* ---------- internal helpers ---------- */

static void las_split(polyveck *left, polyvecl *right, const las_vec *v) {
  unsigned int i;
  for (i = 0; i < K; ++i) {
    left->vec[i] = v->vec[i];
  }
  for (i = 0; i < L; ++i) {
    right->vec[i] = v->vec[K + i];
  }
}

static int poly_equal_modq(const poly *a, const poly *b) {
  poly aa = *a;
  poly bb = *b;
  unsigned int i;

  poly_reduce(&aa);
  poly_reduce(&bb);
  poly_caddq(&aa);
  poly_caddq(&bb);

  for (i = 0; i < N; ++i) {
    if (aa.coeffs[i] != bb.coeffs[i]) {
      return 0;
    }
  }
  return 1;
}

static int polyveck_equal_modq(const polyveck *a, const polyveck *b) {
  unsigned int i;
  for (i = 0; i < K; ++i) {
    if (!poly_equal_modq(&a->vec[i], &b->vec[i])) {
      return 0;
    }
  }
  return 1;
}

/* ---------- public helpers ---------- */

void las_vec_zero(las_vec *v) {
  memset(v, 0, sizeof(*v));
}

void las_vec_add(las_vec *out, const las_vec *a, const las_vec *b) {
  unsigned int i;
  for (i = 0; i < K + L; ++i) {
    poly_add(&out->vec[i], &a->vec[i], &b->vec[i]);
    poly_reduce(&out->vec[i]);
  }
}

void las_vec_sub(las_vec *out, const las_vec *a, const las_vec *b) {
  unsigned int i;
  for (i = 0; i < K + L; ++i) {
    poly_sub(&out->vec[i], &a->vec[i], &b->vec[i]);
    poly_reduce(&out->vec[i]);
  }
}

int las_vec_equal(const las_vec *a, const las_vec *b) {
  unsigned int i;
  for (i = 0; i < K + L; ++i) {
    if (!poly_equal_modq(&a->vec[i], &b->vec[i])) {
      return 0;
    }
  }
  return 1;
}

/*
 * Compute Y = [I | A0] * witness
 *
 * witness = (left || right)
 * left  : length K
 * right : length L
 *
 * Output:
 *   Y = left + A0 * right
 */
void las_statement_from_witness(polyveck *Y,
                                const polyvecl mat[K],
                                const las_vec *witness) {
  polyveck left;
  polyvecl right;
  polyvecl right_ntt;
  polyveck tmp;
  unsigned int i;

  las_split(&left, &right, witness);

  right_ntt = right;
  polyvecl_ntt(&right_ntt);

  polyvec_matrix_pointwise_montgomery(&tmp, mat, &right_ntt);
  polyveck_reduce(&tmp);
  polyveck_invntt_tomont(&tmp);

  polyveck_add(Y, &left, &tmp);
  polyveck_reduce(Y);

  for (i = 0; i < K; ++i) {
    poly_caddq(&Y->vec[i]);
  }
}

int las_statement_check(const polyvecl mat[K],
                        const polyveck *Y,
                        const las_vec *witness) {
  polyveck computed;
  las_statement_from_witness(&computed, mat, witness);
  return polyveck_equal_modq(&computed, Y);
}

/* ---------- LAS paper operations ---------- */

/*
 * Adapt((Y,y), pk, sig_hat, M):
 * return sig = (c, zhat + y)
 */
void las_adapt(las_sig *sig,
               const las_presig *presig,
               const las_vec *witness) {
  sig->c = presig->c;
  las_vec_add(&sig->z, &presig->zhat, witness);
}

/*
 * Ext(Y, sig, sig_hat):
 * s = z - zhat
 * check Y = A s
 * return s if valid
 */
int las_extract(las_vec *witness_out,
                const polyvecl mat[K],
                const polyveck *Y,
                const las_sig *sig,
                const las_presig *presig) {
  if (!poly_equal_modq(&sig->c, &presig->c)) {
    return -1;
  }

  las_vec_sub(witness_out, &sig->z, &presig->zhat);

  if (!las_statement_check(mat, Y, witness_out)) {
    las_vec_zero(witness_out);
    return -1;
  }

  return 0;
}