/*
 * relation_zk.c -- protocol-facing half of the pi module (see relation_zk.h
 * for the design and relation_zk_lazer.h for the seam contract).
 *
 * This half lives in the ref header world: it turns (pp, Y, r') into the flat
 * int64 statement/witness buffers of the bridge seam.
 *
 *   - A' is stored NTT-domain in public_params; its normal-domain entries are
 *     recovered one poly at a time by pushing the constant-1 poly through the
 *     SAME primitive pipeline relation.c uses for A r' (pointwise-montgomery,
 *     reduce, invntt_tomont), so the exported matrix is bit-identical to the
 *     matrix the rest of the build multiplies by.
 *   - The witness is decomposed r' = r_plus - r_minus into the 22 binary
 *     polys the LaZer statement proves; the dummy l2 poly e is 0.
 *   - Coefficients are exported as centered representatives in (-q/2, q/2].
 *
 * Ring-degree loops read LAS_D (the LAS-file convention).
 */
#include <stddef.h>
#include <stdint.h>

#include "params.h"
#include "poly.h"
#include "relation_zk.h"

/* The generated parameter set (ref/relation_zk_params.h) is fixed to the D3
 * engineering set; refuse to build a mismatched statement silently. */
#if PI_ROWS != LAS_N || PI_COLS != 2*N_PLUS_ELL + 1 || PI_DEG != LAS_D
#error "relation_zk: parameter set is generated for the D3 set (n=6, ell=5, d=256); \
regenerate scripts/las_pi_params.py + ref/relation_zk_params.h for this build's set"
#endif

/* Statement staging buffers for the bridge seam.  Static because the matrix
 * alone is 6*23*256 int64s (~283 KiB) -- too large for the stack.  Makes the
 * two public functions non-reentrant, which the header documents. */
static int64_t pi_a_ext[(int64_t)PI_ROWS * PI_COLS * PI_DEG];
static int64_t pi_t[(int64_t)PI_ROWS * PI_DEG];
static int64_t pi_w[(int64_t)PI_COLS * PI_DEG];

/* center a coefficient in [0, Q) to (-Q/2, Q/2] */
static int64_t pi_center(int64_t x) {
  return (x > Q/2) ? x - Q : x;
}

/* Fill pi_a_ext = [I | A' | -I | -A' | 0] and pi_t = t' (centered), from the
 * SAME arithmetic pipeline relation.c applies to r' (so the exported normal-
 * domain A' is exactly the matrix of the build): for each entry,
 * A'[i][j] = caddq(invntt_tomont(reduce(a_prime_hat[i][j] o ntt(1)))). */
static void pi_build_statement(const public_params *pp, const statement *Y) {
  poly one_hat, prod;
  unsigned int i, j, k;

  for(k = 0; k < LAS_D; ++k) one_hat.coeffs[k] = 0;
  one_hat.coeffs[0] = 1;
  poly_ntt(&one_hat);

  for(i = 0; i < LAS_N; ++i) {
    for(j = 0; j < (unsigned int)PI_COLS; ++j) {
      int64_t *dst = &pi_a_ext[((int64_t)i * PI_COLS + j) * PI_DEG];

      if(j < LAS_N || (j >= N_PLUS_ELL && j < N_PLUS_ELL + LAS_N)) {
        /* identity blocks: column j of I (resp. -I) */
        unsigned int col = (j < LAS_N) ? j : j - N_PLUS_ELL;
        int64_t sign = (j < LAS_N) ? 1 : -1;
        for(k = 0; k < LAS_D; ++k) dst[k] = 0;
        if(col == i) dst[0] = sign;
      } else if(j < 2*N_PLUS_ELL) {
        /* A' blocks: NTT -> normal domain via the relation.c pipeline */
        unsigned int col = (j < N_PLUS_ELL) ? j - LAS_N : j - N_PLUS_ELL - LAS_N;
        int64_t sign = (j < N_PLUS_ELL) ? 1 : -1;
        poly_pointwise_montgomery(&prod, &pp->a_prime[i][col], &one_hat);
        poly_reduce(&prod);
        poly_invntt_tomont(&prod);
        poly_reduce(&prod);
        poly_caddq(&prod);
        for(k = 0; k < LAS_D; ++k) dst[k] = sign * pi_center(prod.coeffs[k]);
      } else {
        /* dummy l2 column: all zero */
        for(k = 0; k < LAS_D; ++k) dst[k] = 0;
      }
    }
    for(k = 0; k < LAS_D; ++k)
      pi_t[(int64_t)i * PI_DEG + k] = pi_center(Y->t_prime[i].coeffs[k]);
  }
}

int relation_prove(uint8_t proof[PI_PROOF_MAX_BYTES], size_t *prooflen,
                   const statement *Y, const witness *r_prime,
                   const public_params *pp) {
  unsigned int c, k;

  /* only an honest ternary Gen witness is provable (header contract) */
  for(c = 0; c < N_PLUS_ELL; ++c)
    for(k = 0; k < LAS_D; ++k)
      if(r_prime->value[c].coeffs[k] < -1 || r_prime->value[c].coeffs[k] > 1)
        return -1;

  /* binary decomposition (r_plus || r_minus || 0) */
  for(c = 0; c < N_PLUS_ELL; ++c)
    for(k = 0; k < LAS_D; ++k) {
      int32_t v = r_prime->value[c].coeffs[k];
      pi_w[(int64_t)c                * PI_DEG + k] = (v ==  1);
      pi_w[(int64_t)(N_PLUS_ELL + c) * PI_DEG + k] = (v == -1);
      pi_w[(int64_t)(2*N_PLUS_ELL)   * PI_DEG + k] = 0;
    }

  pi_build_statement(pp, Y);
  return relation_zk_lin_prove(proof, prooflen, pi_a_ext, pi_t, pi_w, pp->seed);
}

int relation_proof_verify(const uint8_t *proof, size_t prooflen,
                          const statement *Y, const public_params *pp) {
  pi_build_statement(pp, Y);
  return relation_zk_lin_verify(proof, prooflen, pi_a_ext, pi_t, pp->seed);
}
