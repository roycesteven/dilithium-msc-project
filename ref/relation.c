/*
 * relation.c -- the hard-relation generator Gen for LAS (eprint 2020/845,
 * Section 3, Table 1).  See relation.h for the layering.
 *
 * Gen and KeyGen are DISTINCT algorithms that happen to use the SAME
 * deterministic sampling and matrix arithmetic (paper Section 3, p.7: "the
 * statement-witness generation Gen for R_A runs exactly as KeyGen").  They
 * produce DIFFERENT semantic objects and this file keeps them non-interchangeable:
 *
 *     KeyGen (basesig.c):  r       <- S_1^(n+ell);  t       = A r;
 *                          return (pk, sk)      = (t,       r)
 *     Gen    (this file):  r_prime <- S_1^(n+ell);  t_prime = A r_prime;
 *                          return (Y, witness)  = (t_prime, r_prime)
 *
 * Gen returns a (statement, witness) pair, NOT a key pair.  The honest witness
 * is the paper's r' (in the pair (Y, y) the paper writes y for the generic
 * witness and parses r' := y inside Adapt); we name it r_prime throughout and
 * never `y` (which the paper reuses for the S_gamma masking randomness in Sign /
 * PreSign) and never `s` (reserved for Ext's EXTRACTED witness s = z - z_hat).
 *
 * NO INVENTED HELPERS: the static helpers at the bottom are VERBATIM copies of
 * the las.c / basesig.c local twins of the same name (prefix las_/b_ ->
 * relation_), each a one-to-one twin of a NAMED upstream poly.c/polyvec.c
 * function.  Kept as local copies (not shared) so relation.c links independently
 * of the scheme files and never depends on a layer above it.  Ring-degree loops
 * read LAS_D (= params.h N = 256), the LAS-file convention.
 */
#include <stdint.h>
#include "params.h"
#include "relation.h"   /* shared params (setup.h) + object types (las_types.h) */
#include "poly.h"
#include "randombytes.h"
#include "fips202.h"

/* ---- local helpers, DEFINED AT THE BOTTOM of this file: VERBATIM copies of
 * las.c's local twins (prefix las_ -> relation_), only the ones Gen needs:
 *
 *   relation_rej_S1                              <->  las_rej_S1 (las.c)
 *   relation_poly_uniform_S1                     <->  las_poly_uniform_S1
 *   relation_polyvec_uniform_S1                  <->  las_polyvecm_uniform_S1
 *   relation_polyvecl_ntt                        <->  las_polyvecl_ntt
 *   relation_polyvecl_pointwise_acc_montgomery   <->  las_polyvecl_pointwise_acc_montgomery
 *   relation_polyvec_matrix_pointwise_montgomery <->  las_polyvec_matrix_pointwise_montgomery
 *   relation_polyvecn_reduce                     <->  las_polyvecn_reduce
 *   relation_polyvecn_caddq                      <->  las_polyvecn_caddq
 *   relation_polyvecn_add                        <->  las_polyvecn_add
 *   relation_polyvecn_invntt_tomont              <->  las_polyvecn_invntt_tomont
 */
static unsigned int relation_rej_S1(int32_t *a, unsigned int len, const uint8_t *buf, unsigned int buflen);
static void relation_poly_uniform_S1(poly *a, const uint8_t seed[LAS_SEEDBYTES], uint16_t nonce);
static void relation_polyvec_uniform_S1(poly v[N_PLUS_ELL], const uint8_t seed[LAS_SEEDBYTES], uint16_t nonce);
static void relation_polyvecl_ntt(poly v[ELL]);
static void relation_polyvecl_pointwise_acc_montgomery(poly *w, const poly u[ELL], const poly v[ELL]);
static void relation_polyvec_matrix_pointwise_montgomery(poly t[LAS_N], const poly mat[LAS_N][ELL], const poly v[ELL]);
static void relation_polyvecn_reduce(poly v[LAS_N]);
static void relation_polyvecn_caddq(poly v[LAS_N]);
static void relation_polyvecn_add(poly w[LAS_N], const poly u[LAS_N], const poly v[LAS_N]);
static void relation_polyvecn_invntt_tomont(poly v[LAS_N]);

/*************************************************
* Name:        relation_gen  (Gen -> (Y, r_prime) in R_A, random path)
*
* Description: paper Gen(1^lambda) -> (Y, y) in R_A (Definition 3; Section 3:
*              same sampling and arithmetic as KeyGen).  Draw a fresh seed, then
*              the deterministic body.  Produces a (statement, witness) pair --
*              NOT a key pair.
*
* Returns 0 (success)
**************************************************/
int relation_gen(statement *Y,             /* paper Y = t' = A r' (statement)      */
                 witness *r_prime,         /* paper r' (honest witness, r' <-$ S_1) */
                 const public_params *pp) {  /* paper A: pp = A = [I | A']          */
  uint8_t seed[LAS_SEEDBYTES];   /* PRG seed to sample r' (no paper symbol) */

  randombytes(seed, LAS_SEEDBYTES);
  return relation_gen_seed(Y, r_prime, pp, seed);
}

/*************************************************
* Name:        relation_gen_seed  (deterministic Gen; KAT slot)
*
* Description: same deterministic sampling and arithmetic as base_keygen_seed
*              (basesig.c), but a DISTINCT algorithm producing a DISTINCT object:
*              r' <- S_1^(n+ell); Y = t' = A r'; return the (statement, witness)
*              pair (t', r') -- never (public_key, secret_key).
*
* Returns 0 (success)
**************************************************/
int relation_gen_seed(statement *Y,            /* paper Y = t' = A r' (statement)            */
                      witness *r_prime,        /* paper r' (honest witness, r' <-$ S_1)      */
                      const public_params *pp, /* paper A: pp = A = [I | A'] (public matrix)  */
                      const uint8_t seed[LAS_SEEDBYTES]) {  /* PRG seed to sample r' (no paper symbol) */
  unsigned int j;
  poly r_prime_1_hat[ELL];   /* NTT rep of r'_1 (final ell components of r') */

  /* Gen: r' <-$ S_1^(n+ell)  (same ternary sampler as KeyGen). */
  relation_polyvec_uniform_S1(r_prime->value, seed, 0);

  /* Y = t' = A r' = r'_0 + A' r'_1,  where A = [I_n | A']:
   * r'_0 = first n components (identity block), r'_1 = final ell (meet A'). */
  for(j = 0; j < ELL; ++j)
    r_prime_1_hat[j] = r_prime->value[LAS_N + j];   /* r'_1 = final ell components of r' */
  relation_polyvecl_ntt(r_prime_1_hat);
  relation_polyvec_matrix_pointwise_montgomery(Y->t_prime, pp->a_prime, r_prime_1_hat);  /* A' r'_1 */
  relation_polyvecn_reduce(Y->t_prime);
  relation_polyvecn_invntt_tomont(Y->t_prime);

  relation_polyvecn_add(Y->t_prime, Y->t_prime, r_prime->value);  /* + r'_0 (identity block, top n of r') */
  relation_polyvecn_reduce(Y->t_prime);
  relation_polyvecn_caddq(Y->t_prime);

  /* return (Y, witness) = (t', r') in R_A. */
  return 0;
}

/* ==================== helpers (verbatim las.c twins) ==================== */

/* <-> las_rej_S1 (las.c) <-> b_rej_S1 (basesig.c): 2-bit ternary codes,
 * reject code 3, contiguous stream. */
static unsigned int relation_rej_S1(int32_t *a, unsigned int len,
                                    const uint8_t *buf, unsigned int buflen) {
  unsigned int ctr, pos, s;
  uint8_t byte, v;

  ctr = pos = 0;
  while(ctr < len && pos < buflen) {
    byte = buf[pos++];
    for(s = 0; s < 4 && ctr < len; ++s) {
      v = (byte >> (2*s)) & 3;
      if(v < 3)
        a[ctr++] = (int32_t)v - 1;
    }
  }
  return ctr;
}

/* <-> las_poly_uniform_S1 (las.c). */
static void relation_poly_uniform_S1(poly *a, const uint8_t seed[LAS_SEEDBYTES], uint16_t nonce) {
  unsigned int ctr;
  uint8_t buf[SHAKE256_RATE];
  uint8_t nb[2];
  keccak_state state;

  nb[0] = (uint8_t)nonce;
  nb[1] = (uint8_t)(nonce >> 8);
  shake256_init(&state);
  shake256_absorb(&state, seed, LAS_SEEDBYTES);
  shake256_absorb(&state, nb, 2);
  shake256_finalize(&state);
  shake256_squeezeblocks(buf, 1, &state);

  ctr = relation_rej_S1(a->coeffs, LAS_D, buf, SHAKE256_RATE);

  while(ctr < LAS_D) {
    shake256_squeezeblocks(buf, 1, &state);
    ctr += relation_rej_S1(a->coeffs + ctr, LAS_D - ctr, buf, SHAKE256_RATE);
  }
}

/* <-> las_polyvecm_uniform_S1 (las.c): nonce++ per poly over the (n+ell)-vector. */
static void relation_polyvec_uniform_S1(poly v[N_PLUS_ELL], const uint8_t seed[LAS_SEEDBYTES], uint16_t nonce) {
  unsigned int i;

  for(i = 0; i < N_PLUS_ELL; ++i)
    relation_poly_uniform_S1(&v[i], seed, nonce++);
}

/* <-> las_polyvecl_ntt (las.c): over the ell columns of A'. */
static void relation_polyvecl_ntt(poly v[ELL]) {
  unsigned int i;

  for(i = 0; i < ELL; ++i)
    poly_ntt(&v[i]);
}

/* <-> las_polyvecl_pointwise_acc_montgomery (las.c). */
static void relation_polyvecl_pointwise_acc_montgomery(poly *w, const poly u[ELL],
                                                       const poly v[ELL]) {
  unsigned int i;
  poly t;

  poly_pointwise_montgomery(w, &u[0], &v[0]);
  for(i = 1; i < ELL; ++i) {
    poly_pointwise_montgomery(&t, &u[i], &v[i]);
    poly_add(w, w, &t);
  }
}

/* <-> las_polyvec_matrix_pointwise_montgomery (las.c). */
static void relation_polyvec_matrix_pointwise_montgomery(poly t[LAS_N], const poly mat[LAS_N][ELL],
                                                         const poly v[ELL]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    relation_polyvecl_pointwise_acc_montgomery(&t[i], mat[i], v);
}

/* <-> las_polyvecn_reduce (las.c). */
static void relation_polyvecn_reduce(poly v[LAS_N]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    poly_reduce(&v[i]);
}

/* <-> las_polyvecn_caddq (las.c). */
static void relation_polyvecn_caddq(poly v[LAS_N]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    poly_caddq(&v[i]);
}

/* <-> las_polyvecn_add (las.c): adds the first n components (identity block, r'_0). */
static void relation_polyvecn_add(poly w[LAS_N], const poly u[LAS_N], const poly v[LAS_N]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    poly_add(&w[i], &u[i], &v[i]);
}

/* <-> las_polyvecn_invntt_tomont (las.c). */
static void relation_polyvecn_invntt_tomont(poly v[LAS_N]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    poly_invntt_tomont(&v[i]);
}
