#ifndef LAS_H
#define LAS_H

#include <stdint.h>
#include <string.h>

#include "params.h"
#include "poly.h"
#include "polyvec.h"

/*
 * LAS prototype data model:
 * - LAS paper uses vectors of length n + ell.
 * - Dilithium ref splits vectors into K and L.
 * So we model a LAS vector as K+L polynomials.
 */
typedef struct {
  poly vec[K + L];
} las_vec;

typedef struct {
  poly c;      /* challenge */
  las_vec zhat; /* pre-signature response */
} las_presig;

typedef struct {
  poly c;   /* challenge */
  las_vec z; /* full-signature response */
} las_sig;

/* Basic vector helpers */
void las_vec_zero(las_vec *v);
void las_vec_add(las_vec *out, const las_vec *a, const las_vec *b);
void las_vec_sub(las_vec *out, const las_vec *a, const las_vec *b);
int las_vec_equal(const las_vec *a, const las_vec *b);

/* Build/check LAS statement Y = [I | A0] * s */
void las_statement_from_witness(polyveck *Y,
                                const polyvecl mat[K],
                                const las_vec *witness);

int las_statement_check(const polyvecl mat[K],
                        const polyveck *Y,
                        const las_vec *witness);

/* LAS paper algorithms */
void las_adapt(las_sig *sig,
               const las_presig *presig,
               const las_vec *witness);

int las_extract(las_vec *witness_out,
                const polyvecl mat[K],
                const polyveck *Y,
                const las_sig *sig,
                const las_presig *presig);

#endif