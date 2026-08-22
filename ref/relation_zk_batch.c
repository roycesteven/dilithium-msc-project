/*
 * relation_zk_batch.c -- protocol-facing half of the batched pi module (see
 * relation_zk_batch.h for the design, relation_zk_lazer_batch.h for the seam).
 *
 * Twin of relation_zk.c.  The per-instance block it writes is IDENTICAL to the
 * one relation_zk.c writes -- same A' recovery pipeline, same binary
 * decomposition, same centering -- so a k=1 batch and the deployed k=1 module
 * build the same statement.  The only new thing here is the block-diagonal
 * placement.
 *
 * Ring-degree loops read LAS_D (the LAS-file convention).
 */
#include <stddef.h>
#include <stdint.h>

#include "params.h"
#include "poly.h"
#include "relation_zk_batch.h"

/* The batch seam is generated for the D3 engineering set, like the k=1 one;
 * refuse to build a mismatched statement silently. */
#if PI_BATCH_ROWS_PER != LAS_N || PI_BATCH_COLS_PER != 2*N_PLUS_ELL + 1 || PI_BATCH_DEG != LAS_D
#error "relation_zk_batch: parameter sets are generated for the D3 set (n=6, ell=5, d=256); \
regenerate them with scripts/gen_lazer_batch_params.sh for this build's set"
#endif

#define BATCH_ROWS_MAX ((int64_t)PI_BATCH_MAX_K * PI_BATCH_ROWS_PER)
#define BATCH_COLS_MAX ((int64_t)PI_BATCH_MAX_K * PI_BATCH_COLS_PER)

/* Statement staging buffers for the bridge seam.  Static because the matrix at
 * k=8 is 48*184*256 int64s (~18 MiB) -- far too large for the stack.  Makes the
 * two public functions non-reentrant, which the header documents. */
static int64_t pi_a_ext[BATCH_ROWS_MAX * BATCH_COLS_MAX * PI_BATCH_DEG];
static int64_t pi_t[BATCH_ROWS_MAX * PI_BATCH_DEG];
static int64_t pi_w[BATCH_COLS_MAX * PI_BATCH_DEG];

/* center a coefficient in [0, Q) to (-Q/2, Q/2] */
static int64_t pi_center(int64_t x) {
  return (x > Q/2) ? x - Q : x;
}

/*
 * Fill pi_a_ext with k diagonal copies of [I | A' | -I | -A' | 0] and pi_t with
 * the k statements stacked.  Off-block entries are zeroed, which is what makes
 * the instances independent: instance i's witness columns cannot influence
 * instance j's rows.
 *
 * The per-entry arithmetic is the SAME pipeline relation.c applies to r', so
 * the exported normal-domain A' is exactly the matrix of the build:
 * A'[i][j] = caddq(invntt_tomont(reduce(a_prime_hat[i][j] o ntt(1)))).
 */
static void pi_build_statement(unsigned int k, const statement *Y,
                               const public_params *pp) {
  const int64_t rows = (int64_t)k * PI_BATCH_ROWS_PER;
  const int64_t cols = (int64_t)k * PI_BATCH_COLS_PER;
  poly one_hat, prod;
  unsigned int b, i, j, c;
  int64_t r;

  for(c = 0; c < LAS_D; ++c) one_hat.coeffs[c] = 0;
  one_hat.coeffs[0] = 1;
  poly_ntt(&one_hat);

  /* zero everything first: every off-diagonal block stays zero */
  for(r = 0; r < rows * cols * PI_BATCH_DEG; ++r)
    pi_a_ext[r] = 0;

  for(b = 0; b < k; ++b) {
    const int64_t row0 = (int64_t)b * PI_BATCH_ROWS_PER;
    const int64_t col0 = (int64_t)b * PI_BATCH_COLS_PER;

    for(i = 0; i < LAS_N; ++i) {
      for(j = 0; j < (unsigned int)PI_BATCH_COLS_PER; ++j) {
        int64_t *dst = &pi_a_ext[((row0 + i) * cols + col0 + j) * PI_BATCH_DEG];

        if(j < LAS_N || (j >= N_PLUS_ELL && j < N_PLUS_ELL + LAS_N)) {
          /* identity blocks: column j of I (resp. -I) */
          unsigned int col = (j < LAS_N) ? j : j - N_PLUS_ELL;
          int64_t sign = (j < LAS_N) ? 1 : -1;
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
          for(c = 0; c < LAS_D; ++c) dst[c] = sign * pi_center(prod.coeffs[c]);
        }
        /* else: the dummy l2 column, already zero */
      }
      for(c = 0; c < LAS_D; ++c)
        pi_t[(row0 + i) * PI_BATCH_DEG + c] = pi_center(Y[b].t_prime[i].coeffs[c]);
    }
  }
}

int relation_batch_prove(unsigned int k,
                         uint8_t proof[PI_BATCH_PROOF_MAX_BYTES], size_t *prooflen,
                         const statement *Y, const witness *r_prime,
                         const public_params *pp) {
  unsigned int b, c, e;

  if(k == 0 || k > PI_BATCH_MAX_K)
    return -1;

  /* only honest ternary Gen witnesses are provable (header contract) */
  for(b = 0; b < k; ++b)
    for(c = 0; c < N_PLUS_ELL; ++c)
      for(e = 0; e < LAS_D; ++e)
        if(r_prime[b].value[c].coeffs[e] < -1 || r_prime[b].value[c].coeffs[e] > 1)
          return -1;

  /* binary decomposition per instance: (r_plus || r_minus || 0) */
  for(b = 0; b < k; ++b) {
    const int64_t col0 = (int64_t)b * PI_BATCH_COLS_PER;

    for(c = 0; c < N_PLUS_ELL; ++c)
      for(e = 0; e < LAS_D; ++e) {
        int32_t v = r_prime[b].value[c].coeffs[e];
        pi_w[(col0 + c)                * PI_BATCH_DEG + e] = (v ==  1);
        pi_w[(col0 + N_PLUS_ELL + c)   * PI_BATCH_DEG + e] = (v == -1);
        pi_w[(col0 + 2*N_PLUS_ELL)     * PI_BATCH_DEG + e] = 0;
      }
  }

  pi_build_statement(k, Y, pp);
  return relation_zk_batch_lin_prove(k, proof, prooflen, pi_a_ext, pi_t, pi_w, pp->seed);
}

int relation_batch_proof_verify(unsigned int k,
                                const uint8_t *proof, size_t prooflen,
                                const statement *Y, const public_params *pp) {
  if(k == 0 || k > PI_BATCH_MAX_K)
    return -1;

  pi_build_statement(k, Y, pp);
  return relation_zk_batch_lin_verify(k, proof, prooflen, pi_a_ext, pi_t, pp->seed);
}
