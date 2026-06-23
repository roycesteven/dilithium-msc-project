/*
 * basesig.c -- simplified Dilithium-style BASE signature (see basesig.h).
 *
 * Standalone: depends on las.h for the shared parameters/types only, and on the
 * repo's mode-independent primitives (poly/NTT/SHAKE).  las.c is NOT linked in by
 * this file -- the static helpers below are local copies, kept behaviourally
 * identical to LAS's so that the challenge hash H(pk, w, M) matches bit-for-bit
 * and a LAS-adapted signature verifies under base_verify().
 */
#include <stddef.h>
#include <stdint.h>
#include "params.h"
#include "basesig.h"
#include "poly.h"
#include "randombytes.h"
#include "fips202.h"

/* Rejection-sampling attempt counter (measurement only; see basesig.h). */
unsigned long base_attempts = 0;

/* ============================ helpers (local copies) ============================
 * Behaviourally identical to the corresponding static helpers in las.c.  They are
 * duplicated here on purpose: basesig is a separate translation unit so it can be
 * built and reasoned about without pulling in the LAS adaptor protocol. */

/* Pack one polynomial into 4 bytes/coeff (canonical [0,Q)) for hashing. */
static void b_pack_poly_canon(uint8_t out[N*4], const poly *a) {
  unsigned int i;
  uint32_t x;
  poly t = *a;
  poly_reduce(&t);
  poly_caddq(&t);
  for(i = 0; i < N; ++i) {
    x = (uint32_t)t.coeffs[i];
    out[4*i+0] = (uint8_t)x;
    out[4*i+1] = (uint8_t)(x >> 8);
    out[4*i+2] = (uint8_t)(x >> 16);
    out[4*i+3] = (uint8_t)(x >> 24);
  }
}

static int b_poly_equal(const poly *a, const poly *b) {
  unsigned int i;
  for(i = 0; i < N; ++i)
    if(a->coeffs[i] != b->coeffs[i])
      return 0;
  return 1;
}

/* Reject if any component has ||.||inf >= B. */
static int b_chknorm_vec(const poly z[LAS_M], int32_t B) {
  unsigned int j;
  for(j = 0; j < LAS_M; ++j)
    if(poly_chknorm(&z[j], B))
      return 1;
  return 0;
}

/* out = a*b mod (X^N+1, Q), centred, via NTT. */
static void b_polymul(poly *out, const poly *a, const poly *b) {
  poly ah = *a, bh = *b;
  poly_ntt(&ah);
  poly_ntt(&bh);
  poly_pointwise_montgomery(out, &ah, &bh);
  poly_invntt_tomont(out);
  poly_reduce(out);
}

/* w = A*v = v_top + A'*v_bot, A=[I|A'], A' (pp->mat) already in NTT domain.
 * Output canonical [0,Q). */
static void b_Amul(poly w[LAS_N], const las_pp *pp, const poly v[LAS_M]) {
  poly vhat[LAS_ELL], tmp, acc;
  unsigned int i, j, k;

  for(j = 0; j < LAS_ELL; ++j) {
    vhat[j] = v[LAS_N + j];
    poly_ntt(&vhat[j]);
  }
  for(i = 0; i < LAS_N; ++i) {
    for(k = 0; k < N; ++k)
      acc.coeffs[k] = 0;
    for(j = 0; j < LAS_ELL; ++j) {
      poly_pointwise_montgomery(&tmp, &pp->mat[i][j], &vhat[j]);
      poly_add(&acc, &acc, &tmp);
    }
    poly_reduce(&acc);
    poly_invntt_tomont(&acc);
    poly_add(&w[i], &acc, &v[i]);   /* identity block */
    poly_reduce(&w[i]);
    poly_caddq(&w[i]);
  }
}

/* H: challenge poly with ||c||_1 = LAS_KAPPA, ||c||inf = 1. */
static void b_challenge(poly *c, const uint8_t seed[LAS_SEEDBYTES]) {
  unsigned int i, b, pos;
  uint64_t signs;
  uint8_t buf[SHAKE256_RATE];
  keccak_state state;

  shake256_init(&state);
  shake256_absorb(&state, seed, LAS_SEEDBYTES);
  shake256_finalize(&state);
  shake256_squeezeblocks(buf, 1, &state);

  signs = 0;
  for(i = 0; i < 8; ++i)
    signs |= (uint64_t)buf[i] << 8*i;
  pos = 8;

  for(i = 0; i < N; ++i)
    c->coeffs[i] = 0;
  for(i = N - LAS_KAPPA; i < N; ++i) {
    do {
      if(pos >= SHAKE256_RATE) {
        shake256_squeezeblocks(buf, 1, &state);
        pos = 0;
      }
      b = buf[pos++];
    } while(b > i);

    c->coeffs[i] = c->coeffs[b];
    c->coeffs[b] = 1 - 2*(signs & 1);
    signs >>= 1;
  }
}

/* c = H(pk, commit, M).  For the base scheme commit is always w (no statement). */
static void b_hash_challenge(poly *c, const las_pk *pk, const poly commit[LAS_N],
                             const uint8_t *m, size_t mlen) {
  keccak_state state;
  uint8_t buf[N*4];
  uint8_t seed[LAS_SEEDBYTES];
  unsigned int i;

  shake256_init(&state);
  for(i = 0; i < LAS_N; ++i) {
    b_pack_poly_canon(buf, &pk->t[i]);
    shake256_absorb(&state, buf, N*4);
  }
  for(i = 0; i < LAS_N; ++i) {
    b_pack_poly_canon(buf, &commit[i]);
    shake256_absorb(&state, buf, N*4);
  }
  shake256_absorb(&state, m, mlen);
  shake256_finalize(&state);
  shake256_squeeze(seed, LAS_SEEDBYTES, &state);
  b_challenge(c, seed);
}

/* Sample one poly with coefficients uniform in [-GAMMA, GAMMA] (set S_g). */
static void b_sample_Sgamma(poly *y, const uint8_t *seed, size_t seedlen, uint16_t nonce) {
  keccak_state state;
  uint8_t buf[SHAKE256_RATE];
  uint8_t nb[2];
  uint32_t t, gmask;
  unsigned int ctr = 0, pos = 0;

  gmask = 1;
  while(gmask < 2u*(uint32_t)LAS_GAMMA)
    gmask <<= 1;
  gmask -= 1;

  nb[0] = (uint8_t)nonce;
  nb[1] = (uint8_t)(nonce >> 8);
  shake256_init(&state);
  shake256_absorb(&state, seed, seedlen);
  shake256_absorb(&state, nb, 2);
  shake256_finalize(&state);
  shake256_squeezeblocks(buf, 1, &state);

  while(ctr < N) {
    if(pos + 3 > SHAKE256_RATE) {
      shake256_squeezeblocks(buf, 1, &state);
      pos = 0;
    }
    t  = buf[pos];
    t |= (uint32_t)buf[pos+1] << 8;
    t |= (uint32_t)buf[pos+2] << 16;
    pos += 3;
    t &= gmask;
    if(t < 2u*(uint32_t)LAS_GAMMA + 1u)
      y->coeffs[ctr++] = (int32_t)t - LAS_GAMMA;
  }
}

/* Sample one poly with coefficients uniform in {-1,0,1} (set S_1, ternary). */
static void b_sample_ternary(poly *r, const uint8_t *seed, size_t seedlen, uint16_t nonce) {
  keccak_state state;
  uint8_t buf[SHAKE256_RATE];
  uint8_t nb[2], byte, v, s;
  unsigned int ctr = 0, pos = 0;

  nb[0] = (uint8_t)nonce;
  nb[1] = (uint8_t)(nonce >> 8);
  shake256_init(&state);
  shake256_absorb(&state, seed, seedlen);
  shake256_absorb(&state, nb, 2);
  shake256_finalize(&state);
  shake256_squeezeblocks(buf, 1, &state);

  while(ctr < N) {
    if(pos >= SHAKE256_RATE) {
      shake256_squeezeblocks(buf, 1, &state);
      pos = 0;
    }
    byte = buf[pos++];
    for(s = 0; s < 4 && ctr < N; ++s) {
      v = (byte >> (2*s)) & 3;          /* 2 bits: {0,1,2}->{-1,0,1}, reject 3 */
      if(v < 3)
        r->coeffs[ctr++] = (int32_t)v - 1;
    }
  }
}

/* ============================ scheme ============================ */

void base_keygen(las_pk *pk, las_sk *sk, const las_pp *pp) {
  uint8_t seed[LAS_SEEDBYTES];
  unsigned int j;
  randombytes(seed, LAS_SEEDBYTES);
  for(j = 0; j < LAS_M; ++j)
    b_sample_ternary(&sk->s[j], seed, LAS_SEEDBYTES, (uint16_t)j);
  b_Amul(pk->t, pp, sk->s);
}

void base_sign(las_sig *sig, const uint8_t *m, size_t mlen,
               const las_pk *pk, const las_sk *sk, const las_pp *pp) {
  uint8_t seed[64];
  uint16_t nonce = 0;
  unsigned int j;
  poly y[LAS_M], w[LAS_N], cr, c;

  randombytes(seed, 64);
  for(;;) {
    ++base_attempts;                          /* instrumentation only */
    for(j = 0; j < LAS_M; ++j)
      b_sample_Sgamma(&y[j], seed, 64, nonce++);
    b_Amul(w, pp, y);                          /* w = A y           */
    b_hash_challenge(&c, pk, w, m, mlen);      /* c = H(pk, w, M)   -- NO statement Y */
    for(j = 0; j < LAS_M; ++j) {               /* z = y + c r       */
      b_polymul(&cr, &c, &sk->s[j]);
      poly_add(&sig->z[j], &y[j], &cr);
      poly_reduce(&sig->z[j]);
    }
    if(b_chknorm_vec(sig->z, LAS_BOUND_SIGN))
      continue;
    sig->c = c;
    return;
  }
}

int base_verify(const las_sig *sig, const uint8_t *m, size_t mlen,
                const las_pk *pk, const las_pp *pp) {
  poly w[LAS_N], ct, c2;
  unsigned int j;

  if(b_chknorm_vec(sig->z, LAS_BOUND_SIGN))
    return -1;

  b_Amul(w, pp, sig->z);                       /* A z               */
  for(j = 0; j < LAS_N; ++j) {                  /* w' = A z - c t    */
    b_polymul(&ct, &sig->c, &pk->t[j]);
    poly_sub(&w[j], &w[j], &ct);
    poly_reduce(&w[j]);
    poly_caddq(&w[j]);
  }
  b_hash_challenge(&c2, pk, w, m, mlen);         /* c == H(pk, w', M) -- NO statement Y */
  return b_poly_equal(&c2, &sig->c) ? 0 : -1;
}
