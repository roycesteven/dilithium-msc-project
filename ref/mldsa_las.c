/*
 * mldsa_las.c -- LAS built on NIST ML-DSA *as FIPS 204 specifies it*.
 *
 * Written as a STRUCTURAL MIRROR of the upstream ref/sign.c, the same way
 * basesig.c and las.c mirror it: same order, same locals, same return
 * convention, so the two files scroll side by side and every deviation is one
 * annotated line.  Zero upstream functions are modified; every rounding, hint
 * and packing primitive is called as-is from rounding.c / poly.c / polyvec.c /
 * packing.c.
 *
 * ANNOTATION CONVENTION (read side by side with sign.c):
 *   [REUSED]  sign.c:<line>: <code>   -- same line
 *   [CHANGED] quotes the sign.c line(s), then states WHY this differs
 *   [NEW]     has no sign.c analogue (the adaptor operations)
 *
 * See mldsa_las.h for what the experiment is for.
 */
#include <stdint.h>
#include <string.h>
#include "params.h"
#include "mldsa_las.h"
#include "sign.h"
#include "packing.h"
#include "polyvec.h"
#include "poly.h"
#include "randombytes.h"
#include "symmetric.h"
#include "fips202.h"

unsigned long mldsa_las_attempts = 0;

/* FIPS 204 domain separator for the EMPTY context string: pre = (0, ctxlen).
 * crypto_sign_signature / crypto_sign_verify build this before calling their
 * *_internal forms (sign.c:218-221, sign.c:388-391); the experiment must absorb
 * the identical bytes or its signatures could never be checked by the stock
 * verifier, for reasons that have nothing to do with the adaptor. */
static const uint8_t mldsa_las_pre[2] = {0, 0};
#define MLDSA_LAS_PRELEN 2

/*************************************************
* Name:        mldsa_las_gen  (statement/witness generation)
*
* Description: y <- S_ETA^L, Y = A y.  Mirrors the s1 / t computation of
*              crypto_sign_keypair (sign.c:42-52) exactly, so Y lands in the
*              same representation as the A z the verifier computes.
**************************************************/
void mldsa_las_gen(mldsa_statement *Y,
                   mldsa_witness *y,
                   const uint8_t rho[SEEDBYTES],
                   const uint8_t seed[CRHBYTES],
                   uint16_t nonce)
{
  polyvecl mat[K];
  polyvecl yhat;

  /* [REUSED] sign.c:40: polyvec_matrix_expand(mat, rho); */
  polyvec_matrix_expand(mat, rho);

  /* [CHANGED] sign.c:43: polyvecl_uniform_eta(&s1, rhoprime, 0);
   * WHY: the witness is sampled from the SAME distribution as ML-DSA's own
   * secret vector, so no new distribution is introduced by the experiment. */
  polyvecl_uniform_eta(&y->y, seed, nonce);

  /* [REUSED] sign.c:47-51: t = A s1 (NTT domain, then back) */
  yhat = y->y;
  polyvecl_ntt(&yhat);
  polyvec_matrix_pointwise_montgomery(&Y->Y, mat, &yhat);
  polyveck_reduce(&Y->Y);
  polyveck_invntt_tomont(&Y->Y);
  polyveck_reduce(&Y->Y);

  /* [CHANGED] sign.c:56: polyveck_caddq(&t1);
   * WHY: polyveck_reduce leaves CENTRED representatives, which can be negative.
   * The statement is a wire object, so it is kept canonical in [0, Q) -- both
   * so the 23-bit codec is lossless and so two parties that derive Y
   * independently agree on its bytes.  Every arithmetic use of Y downstream
   * reduces again, so canonicalising here changes no result. */
  polyveck_caddq(&Y->Y);
}

/*************************************************
* Name:        mldsa_las_presign
*
* Description: crypto_sign_signature_internal (sign.c:85) with the commitment
*              moved onto w + Y.  Exactly how much of the commitment path moves
*              is what the two variants differ in -- see mldsa_las.h.
**************************************************/
int mldsa_las_presign(uint8_t presig[CRYPTO_BYTES],
                      const uint8_t *m, size_t mlen,
                      const mldsa_statement *Y,
                      const uint8_t rnd[RNDBYTES],
                      const uint8_t *sk,
                      mldsa_las_variant variant)
{
  unsigned int n;
  uint8_t seedbuf[2*SEEDBYTES + TRBYTES + 2*CRHBYTES];
  uint8_t *rho, *tr, *key, *mu, *rhoprime;
  uint16_t nonce = 0;
  polyvecl mat[K], s1, ymask, z;
  polyveck t0, s2, w1, w0, h;
  polyveck wy, wy1, wy0;      /* [NEW] the shifted commitment w + Y */
  poly cp;
  keccak_state state;

  rho = seedbuf;
  tr = rho + SEEDBYTES;
  key = tr + TRBYTES;
  mu = key + SEEDBYTES;
  rhoprime = mu + CRHBYTES;
  /* [REUSED] sign.c:109: unpack_sk(...) */
  unpack_sk(rho, tr, key, &t0, &s1, &s2, sk);

  /* [REUSED] sign.c:112-117: mu = CRH(tr, pre, msg).
   * `pre` is FIPS 204's domain separator (0, ctxlen, ctx); with the empty
   * context that crypto_sign_verify(..., NULL, 0, ...) uses it is the two bytes
   * {0, 0}.  Omitting it would make mu differ from the stock verifier's and
   * every adapted signature would fail for a HARNESS reason rather than a
   * cryptographic one -- which is precisely the artefact this experiment must
   * not produce. */
  shake256_init(&state);
  shake256_absorb(&state, tr, TRBYTES);
  shake256_absorb(&state, mldsa_las_pre, MLDSA_LAS_PRELEN);
  shake256_absorb(&state, m, mlen);
  shake256_finalize(&state);
  shake256_squeeze(mu, CRHBYTES, &state);

  /* [REUSED] sign.c:120-125: rhoprime = CRH(key, rnd, mu) */
  shake256_init(&state);
  shake256_absorb(&state, key, SEEDBYTES);
  shake256_absorb(&state, rnd, RNDBYTES);
  shake256_absorb(&state, mu, CRHBYTES);
  shake256_finalize(&state);
  shake256_squeeze(rhoprime, CRHBYTES, &state);

  /* [REUSED] sign.c:128-131 */
  polyvec_matrix_expand(mat, rho);
  polyvecl_ntt(&s1);
  polyveck_ntt(&s2);
  polyveck_ntt(&t0);

rej:
  mldsa_las_attempts++;

  /* [REUSED] sign.c:135: sample the masking vector */
  polyvecl_uniform_gamma1(&ymask, rhoprime, nonce++);

  /* [REUSED] sign.c:138-143: w = A ymask */
  z = ymask;
  polyvecl_ntt(&z);
  polyvec_matrix_pointwise_montgomery(&w1, mat, &z);
  polyveck_reduce(&w1);
  polyveck_invntt_tomont(&w1);
  polyveck_caddq(&w1);

  /* [NEW] the adaptor shift: wy = w + Y, brought back into [0, Q).
   * This is the ML-DSA analogue of LAS's c = H(pk, w + Y, M).  VBASE omits the
   * shift entirely and is therefore plain ML-DSA through the same loop. */
  if(variant == MLDSA_LAS_VBASE) {
    wy = w1;
  } else {
    polyveck_add(&wy, &w1, &Y->Y);
    polyveck_reduce(&wy);
    polyveck_caddq(&wy);
  }

  /* [CHANGED] sign.c:146-147:
   *     polyveck_decompose(&w1, &w0, &w1);
   *     polyveck_pack_w1(sig, &w1);
   * WHY: both variants commit to HighBits(w + Y); they differ in whether the
   * REST of the signing path (low-bits test, hint) also moves onto w + Y. */
  polyveck_decompose(&wy1, &wy0, &wy);
  polyveck_decompose(&w1, &w0, &w1);
  polyveck_pack_w1(presig, &wy1);

  /* [REUSED] sign.c:149-154: c_tilde = H(mu, w1-packed); cp = SampleInBall */
  shake256_init(&state);
  shake256_absorb(&state, mu, CRHBYTES);
  shake256_absorb(&state, presig, K*POLYW1_PACKEDBYTES);
  shake256_finalize(&state);
  shake256_squeeze(presig, CTILDEBYTES, &state);
  poly_challenge(&cp, presig);
  poly_ntt(&cp);

  /* [REUSED] sign.c:157-159: z_hat = ymask + c s1 */
  polyvecl_pointwise_poly_montgomery(&z, &cp, &s1);
  polyvecl_invntt_tomont(&z);
  polyvecl_add(&z, &z, &ymask);
  polyvecl_reduce(&z);

  /* [CHANGED] sign.c:160: if(polyvecl_chknorm(&z, GAMMA1 - BETA)) goto rej;
   * WHY: the ADAPTED z = z_hat + y must clear ML-DSA's own bound, so PreSign
   * rejects at a bound tightened by the witness norm.  Loosening this line is
   * exactly the failure mode documented for the simplified scheme. */
  if(polyvecl_chknorm(&z, variant == MLDSA_LAS_VBASE ? GAMMA1 - BETA
                                                     : MLDSA_LAS_BOUND_PRESIGN))
    goto rej;

  /* [CHANGED] sign.c:165-169: the low-bits test, taken around w or around w+Y
   * depending on the variant.  ML-DSA needs HighBits(w - c s2) == HighBits(w);
   * the adaptor needs the same statement about the SHIFTED commitment. */
  polyveck_pointwise_poly_montgomery(&h, &cp, &s2);
  polyveck_invntt_tomont(&h);
  if(variant == MLDSA_LAS_V1_SHIFTED) {
    polyveck_sub(&wy0, &wy0, &h);
    polyveck_reduce(&wy0);
    if(polyveck_chknorm(&wy0, GAMMA2 - BETA))
      goto rej;
  } else {
    polyveck_sub(&w0, &w0, &h);
    polyveck_reduce(&w0);
    if(polyveck_chknorm(&w0, GAMMA2 - BETA))
      goto rej;
  }

  /* [REUSED] sign.c:172-176: the c t0 correction the hint has to carry */
  polyveck_pointwise_poly_montgomery(&h, &cp, &t0);
  polyveck_invntt_tomont(&h);
  polyveck_reduce(&h);
  if(polyveck_chknorm(&h, GAMMA2))
    goto rej;

  /* [CHANGED] sign.c:178-181:
   *     polyveck_add(&w0, &w0, &h);
   *     n = polyveck_make_hint(&h, &w0, &w1);
   * WHY: V0 is the naive port -- the hint is still MakeHint around w, i.e. it
   * reconstructs HighBits(w) while the challenge committed to HighBits(w + Y).
   * V1 moves the hint onto the shifted commitment too, which is the only way a
   * hint computed by the SIGNER can describe the quantity the verifier will
   * reconstruct from the ADAPTED signature. */
  if(variant == MLDSA_LAS_V1_SHIFTED) {
    polyveck_add(&wy0, &wy0, &h);
    n = polyveck_make_hint(&h, &wy0, &wy1);
  } else {
    polyveck_add(&w0, &w0, &h);
    n = polyveck_make_hint(&h, &w0, &w1);
  }
  if(n > OMEGA)
    goto rej;

  /* [REUSED] sign.c:184: pack_sig(sig, sig, &z, &h); */
  pack_sig(presig, presig, &z, &h);
  return 0;
}

/*************************************************
* Name:        mldsa_las_preverify
*
* Description: crypto_sign_verify_internal (sign.c:289) with ONE addition: the
*              reconstructed commitment is shifted by Y before UseHint.  That
*              single line is why the stock consensus verifier cannot check a
*              pre-signature -- it has no Y.
**************************************************/
int mldsa_las_preverify(const uint8_t presig[CRYPTO_BYTES],
                        const uint8_t *m, size_t mlen,
                        const mldsa_statement *Y,
                        const uint8_t *pk)
{
  unsigned int i;
  uint8_t buf[K*POLYW1_PACKEDBYTES];
  uint8_t rho[SEEDBYTES];
  uint8_t mu[CRHBYTES];
  uint8_t c[CTILDEBYTES];
  uint8_t c2[CTILDEBYTES];
  poly cp;
  polyvecl mat[K], z;
  polyveck t1, w1, h;
  keccak_state state;

  /* [REUSED] sign.c:312-313 */
  unpack_pk(rho, &t1, pk);
  if(unpack_sig(c, &z, &h, presig))
    return -1;

  /* [CHANGED] sign.c:315: if(polyvecl_chknorm(&z, GAMMA1 - BETA)) return -1;
   * WHY: a pre-signature is held to the tightened bound; anything looser could
   * not be adapted into a signature ML-DSA's own verifier accepts. */
  if(polyvecl_chknorm(&z, MLDSA_LAS_BOUND_PRESIGN))
    return -1;

  /* [REUSED] sign.c:318-324: mu = CRH(H(pk), pre, msg), empty context */
  shake256(mu, TRBYTES, pk, CRYPTO_PUBLICKEYBYTES);
  shake256_init(&state);
  shake256_absorb(&state, mu, TRBYTES);
  shake256_absorb(&state, mldsa_las_pre, MLDSA_LAS_PRELEN);
  shake256_absorb(&state, m, mlen);
  shake256_finalize(&state);
  shake256_squeeze(mu, CRHBYTES, &state);

  /* [REUSED] sign.c:327-340: w_approx = A z - c t1 2^d */
  poly_challenge(&cp, c);
  polyvec_matrix_expand(mat, rho);
  polyvecl_ntt(&z);
  polyvec_matrix_pointwise_montgomery(&w1, mat, &z);
  poly_ntt(&cp);
  polyveck_shiftl(&t1);
  polyveck_ntt(&t1);
  polyveck_pointwise_poly_montgomery(&t1, &cp, &t1);
  polyveck_sub(&w1, &w1, &t1);
  polyveck_reduce(&w1);
  polyveck_invntt_tomont(&w1);

  /* [NEW] the adaptor shift.  A z_hat - c t = w - c s2 + c t0, so adding Y
   * gives what the ADAPTED signature would reconstruct. */
  polyveck_add(&w1, &w1, &Y->Y);
  polyveck_reduce(&w1);

  /* [REUSED] sign.c:343-345: caddq, UseHint, pack */
  polyveck_caddq(&w1);
  polyveck_use_hint(&w1, &w1, &h);
  polyveck_pack_w1(buf, &w1);

  /* [REUSED] sign.c:348-356: re-derive and byte-compare the challenge */
  shake256_init(&state);
  shake256_absorb(&state, mu, CRHBYTES);
  shake256_absorb(&state, buf, K*POLYW1_PACKEDBYTES);
  shake256_finalize(&state);
  shake256_squeeze(c2, CTILDEBYTES, &state);
  for(i = 0; i < CTILDEBYTES; ++i)
    if(c[i] != c2[i])
      return -1;

  return 0;
}

/*************************************************
* Name:        mldsa_las_adapt          [NEW -- no sign.c analogue]
*
* Description: PreVerify, then sigma = (c_tilde, z_hat + y, h).  The hint is
*              copied verbatim: the adapting party holds the witness, not the
*              signing key, and MakeHint needs c t0 and w - c s2, neither of
*              which it has.  A hint can therefore never be repaired at
*              adaptation time -- if it is wrong, it stays wrong.
**************************************************/
int mldsa_las_adapt(uint8_t sig[CRYPTO_BYTES],
                    const uint8_t presig[CRYPTO_BYTES],
                    const uint8_t *m, size_t mlen,
                    const mldsa_statement *Y,
                    const mldsa_witness *y,
                    const uint8_t *pk)
{
  uint8_t c[CTILDEBYTES];
  polyvecl z;
  polyveck h;

  /* [PAPER Alg.2] 21: if PreVerify(...) = 0 then return bottom */
  if(mldsa_las_preverify(presig, m, mlen, Y, pk))
    return -1;

  if(unpack_sig(c, &z, &h, presig))
    return -1;

  /* [PAPER Alg.2] 25: sigma = (c, z_hat + y) -- the hint rides along unchanged */
  polyvecl_add(&z, &z, &y->y);
  polyvecl_reduce(&z);

  pack_sig(sig, c, &z, &h);
  return 0;
}

/*************************************************
* Name:        mldsa_las_ext            [NEW -- no sign.c analogue]
*
* Description: y' = z - z_hat, accepted iff A y' == Y.
**************************************************/
int mldsa_las_ext(mldsa_witness *y_out,
                  const uint8_t sig[CRYPTO_BYTES],
                  const uint8_t presig[CRYPTO_BYTES],
                  const mldsa_statement *Y,
                  const uint8_t *pk)
{
  unsigned int i, j;
  uint8_t rho[SEEDBYTES];
  uint8_t c1[CTILDEBYTES], c2[CTILDEBYTES];
  polyvecl mat[K], z, zhat, yhat;
  polyveck t1_unused, check;

  if(unpack_sig(c1, &z, &t1_unused, sig))
    return -1;
  if(unpack_sig(c2, &zhat, &t1_unused, presig))
    return -1;

  for(i = 0; i < L; ++i)
    for(j = 0; j < N; ++j)
      y_out->y.vec[i].coeffs[j] = z.vec[i].coeffs[j] - zhat.vec[i].coeffs[j];
  polyvecl_reduce(&y_out->y);

  /* A y' == Y ? */
  unpack_pk(rho, &t1_unused, pk);
  polyvec_matrix_expand(mat, rho);
  yhat = y_out->y;
  polyvecl_ntt(&yhat);
  polyvec_matrix_pointwise_montgomery(&check, mat, &yhat);
  polyveck_reduce(&check);
  polyveck_invntt_tomont(&check);
  polyveck_reduce(&check);

  polyveck_sub(&check, &check, &Y->Y);
  polyveck_reduce(&check);
  polyveck_caddq(&check);
  for(i = 0; i < K; ++i)
    for(j = 0; j < N; ++j)
      if(check.vec[i].coeffs[j] != 0)
        return -1;

  return 0;
}

/* ------------------------------------------------------------------------
 * Wire format for the two adaptor-specific objects (see mldsa_las.h).
 * ------------------------------------------------------------------------ */

/*************************************************
* Name:        mldsa_las_pack_statement / mldsa_las_unpack_statement
*
* Description: Y is a full R_q^K element, so each coefficient needs the full
*              23 bits that Q = 8380417 < 2^23 admits.  A plain bit cursor is
*              used rather than the hand-unrolled shifts of packing.c: this
*              codec is not on any timed path (the comparison benchmark
*              measures the core tier), and clarity is worth more here than
*              the few cycles.
*
*              The decoder REJECTS any coefficient >= Q, so a malformed
*              statement cannot enter the scheme.
**************************************************/
void mldsa_las_pack_statement(uint8_t r[MLDSA_LAS_STATEMENT_BYTES],
                              const mldsa_statement *Y)
{
  unsigned int i, j, bit = 0, b;
  uint32_t c;

  for(i = 0; i < MLDSA_LAS_STATEMENT_BYTES; ++i)
    r[i] = 0;

  for(i = 0; i < K; ++i) {
    for(j = 0; j < N; ++j) {
      c = (uint32_t)Y->Y.vec[i].coeffs[j];
      for(b = 0; b < 23; ++b, ++bit)
        r[bit >> 3] |= (uint8_t)(((c >> b) & 1u) << (bit & 7));
    }
  }
}

int mldsa_las_unpack_statement(mldsa_statement *Y,
                               const uint8_t r[MLDSA_LAS_STATEMENT_BYTES])
{
  unsigned int i, j, bit = 0, b;
  uint32_t c;

  for(i = 0; i < K; ++i) {
    for(j = 0; j < N; ++j) {
      c = 0;
      for(b = 0; b < 23; ++b, ++bit)
        c |= (uint32_t)((r[bit >> 3] >> (bit & 7)) & 1u) << b;
      if(c >= Q)                      /* validating decoder: canonical only */
        return -1;
      Y->Y.vec[i].coeffs[j] = (int32_t)c;
    }
  }
  return 0;
}

/*************************************************
* Name:        mldsa_las_pack_witness / mldsa_las_unpack_witness
*
* Description: the witness is ETA-bounded, so ML-DSA's own polyeta codec
*              applies unchanged -- no new encoding is invented.  The decoder
*              additionally range-checks, because polyeta_unpack alone does not
*              guarantee |coeff| <= ETA for every ETA.
**************************************************/
void mldsa_las_pack_witness(uint8_t r[MLDSA_LAS_WITNESS_BYTES],
                            const mldsa_witness *y)
{
  unsigned int i;
  for(i = 0; i < L; ++i)
    polyeta_pack(r + i*POLYETA_PACKEDBYTES, &y->y.vec[i]);
}

int mldsa_las_unpack_witness(mldsa_witness *y,
                             const uint8_t r[MLDSA_LAS_WITNESS_BYTES])
{
  unsigned int i, j;
  for(i = 0; i < L; ++i)
    polyeta_unpack(&y->y.vec[i], r + i*POLYETA_PACKEDBYTES);
  for(i = 0; i < L; ++i)
    for(j = 0; j < N; ++j)
      if(y->y.vec[i].coeffs[j] < -(int32_t)ETA ||
         y->y.vec[i].coeffs[j] >  (int32_t)ETA)
        return -1;
  return 0;
}

/*************************************************
* Name:        mldsa_las_hint_weight
*
* Description: number of set hint coefficients carried by a signature blob.
**************************************************/
int mldsa_las_hint_weight(const uint8_t sig[CRYPTO_BYTES])
{
  unsigned int i, j;
  int w = 0;
  uint8_t c[CTILDEBYTES];
  polyvecl z;
  polyveck h;

  if(unpack_sig(c, &z, &h, sig))
    return -1;
  for(i = 0; i < K; ++i)
    for(j = 0; j < N; ++j)
      w += (h.vec[i].coeffs[j] != 0);
  return w;
}
