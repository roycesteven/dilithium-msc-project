#ifndef LAS_TYPES_H
#define LAS_TYPES_H

/*
 * las_types.h -- the six PROTOCOL object types of the LAS construction
 * (eprint 2020/845), split out of setup.h so the shared setup header holds only
 * the construction parameters and public_params.  C twin of the Rust module
 * src/las_types.rs.
 *
 * These live HERE, below every scheme, so ONE codec (serialize.c) serves all of
 * them and no scheme file includes another's header.  Each type belongs to
 * exactly one layer (ownership below), but is DEFINED here so the codec can see
 * them all -- mirrors how upstream keeps polyvecl/polyveck in polyvec.h rather
 * than sign.h.
 *
 * Ownership (paper model -- Definition 3 + Algorithms 1 and 2):
 *   Algorithm 1 (Sigma) : public_key, secret_key, signature  (owner basesig.c)
 *   hard relation R_A    : statement, witness                 (owner relation.c)
 *   Algorithm 2          : pre_signature                      (owner las.c)
 *
 * public_params (pp = (A, H)) is NOT one of these: it is the shared public
 * setup the parameters are expanded into, not a protocol object, and stays in
 * setup.h.
 *
 * Header layering (a DAG, not a single chain): setup.h is included by this
 * header, which is in turn included by relation.h, serialize.h, basesig.h and
 * las.h; the schemes also include serialize.h.  Mirror of the Rust layering
 * setup -> las_types -> {relation, serialize} -> {basesig, las}.
 */

#include <stdint.h>
#include "poly.h"      /* poly type; pulls in params.h => N (=LAS_D), Q */
#include "setup.h"     /* construction parameters: LAS_N, ELL, N_PLUS_ELL */

typedef struct {
  poly t[LAS_N];
} public_key;  /* pk = t = A r */

typedef struct {
  poly r[N_PLUS_ELL];
} secret_key;  /* sk = r in S_1^(n+ell) */

typedef struct {
  uint8_t c_tilde[LAS_CTILDEBYTES]; /* challenge H digest; c = SampleInBall(c_tilde) local-only */
  poly z[N_PLUS_ELL];
} signature;  /* sigma = (c_tilde, z) */

typedef struct {
  poly t_prime[LAS_N];
} statement;  /* Y = t' = A r' */

typedef struct {
  poly value[N_PLUS_ELL];
} witness;    /* r' from Gen, or s from Ext */

typedef struct {
  uint8_t c_tilde[LAS_CTILDEBYTES]; /* challenge H digest; c = SampleInBall(c_tilde) local-only */
  poly z_hat[N_PLUS_ELL];
} pre_signature;  /* sigma_hat = (c_tilde, z_hat) */

#endif
