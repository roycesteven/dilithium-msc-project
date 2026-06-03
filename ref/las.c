#include <stddef.h>
#include <stdint.h>
#include "params.h"
#include "las.h"
#include "poly.h"
#include "randombytes.h"
#include "fips202.h"

/* ============================ helpers ============================ */

/* Pack one polynomial into 4 bytes/coeff (canonical [0,Q)) for hashing. */
static void pack_poly_canon(uint8_t out[N*4], const poly *a) {
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

static int poly_equal(const poly *a, const poly *b) {
  unsigned int i;
  for(i = 0; i < N; ++i)
    if(a->coeffs[i] != b->coeffs[i])
      return 0;
  return 1;
}

/* Reject if any component has ||.||inf >= B. */
static int chknorm_vec(const poly z[LAS_M], int32_t B) {
  unsigned int j;
  for(j = 0; j < LAS_M; ++j)
    if(poly_chknorm(&z[j], B))
      return 1;
  return 0;
}

/* Full schoolbook-free product via NTT: out = a*b mod (X^N+1, Q), centred. */
static void polymul(poly *out, const poly *a, const poly *b) {
  poly ah = *a, bh = *b;
  poly_ntt(&ah);
  poly_ntt(&bh);
  poly_pointwise_montgomery(out, &ah, &bh);
  poly_invntt_tomont(out);
  poly_reduce(out);
}

/* w = A*v = v_top + A'*v_bot, with A=[I|A'], A' (pp->mat) already in NTT domain.
 * Output is canonical [0,Q) (used both as commitment and inside arithmetic). */
static void las_Amul(poly w[LAS_N], const las_pp *pp, const poly v[LAS_M]) {
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

/* H: challenge poly with ||c||_1 = LAS_KAPPA, ||c||inf = 1.
 * Same construction as Dilithium's poly_challenge but with kappa fixed here. */
static void las_challenge(poly *c, const uint8_t seed[LAS_SEEDBYTES]) {
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

/* c = H(pk, commit, M) where commit is the (already w or w+Y) commitment. */
static void hash_challenge(poly *c, const las_pk *pk, const poly commit[LAS_N],
                           const uint8_t *m, size_t mlen) {
  keccak_state state;
  uint8_t buf[N*4];
  uint8_t seed[LAS_SEEDBYTES];
  unsigned int i;

  shake256_init(&state);
  for(i = 0; i < LAS_N; ++i) {
    pack_poly_canon(buf, &pk->t[i]);
    shake256_absorb(&state, buf, N*4);
  }
  for(i = 0; i < LAS_N; ++i) {
    pack_poly_canon(buf, &commit[i]);
    shake256_absorb(&state, buf, N*4);
  }
  shake256_absorb(&state, m, mlen);
  shake256_finalize(&state);
  shake256_squeeze(seed, LAS_SEEDBYTES, &state);
  las_challenge(c, seed);
}

/* Sample one poly with coefficients uniform in [-GAMMA, GAMMA] (set S_g). */
static void sample_Sgamma(poly *y, const uint8_t *seed, size_t seedlen, uint16_t nonce) {
  keccak_state state;
  uint8_t buf[SHAKE256_RATE];
  uint8_t nb[2];
  uint32_t t;
  unsigned int ctr = 0, pos = 0;

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
    t &= 0x3FFFF;                       /* 18 bits: 2*GAMMA+1 = 245761 < 2^18 */
    if(t < 2u*LAS_GAMMA + 1u)
      y->coeffs[ctr++] = (int32_t)t - LAS_GAMMA;
  }
}

/* Sample one poly with coefficients uniform in {-1,0,1} (set S_1, ternary). */
static void sample_ternary(poly *r, const uint8_t *seed, size_t seedlen, uint16_t nonce) {
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

void las_setup(las_pp *pp, const uint8_t seed[LAS_SEEDBYTES]) {
  unsigned int i, j;
  for(i = 0; i < LAS_SEEDBYTES; ++i)
    pp->seed[i] = seed[i];
  for(i = 0; i < LAS_N; ++i)
    for(j = 0; j < LAS_ELL; ++j)
      poly_uniform(&pp->mat[i][j], seed, (uint16_t)((i << 8) + j));
}

void las_keygen(las_pk *pk, las_sk *sk, const las_pp *pp) {
  uint8_t seed[LAS_SEEDBYTES];
  unsigned int j;
  randombytes(seed, LAS_SEEDBYTES);
  for(j = 0; j < LAS_M; ++j)
    sample_ternary(&sk->s[j], seed, LAS_SEEDBYTES, (uint16_t)j);
  las_Amul(pk->t, pp, sk->s);
}

void las_sign(las_sig *sig, const uint8_t *m, size_t mlen,
              const las_pk *pk, const las_sk *sk, const las_pp *pp) {
  uint8_t seed[64];
  uint16_t nonce = 0;
  unsigned int j;
  poly y[LAS_M], w[LAS_N], cr, c;

  randombytes(seed, 64);
  for(;;) {
    for(j = 0; j < LAS_M; ++j)
      sample_Sgamma(&y[j], seed, 64, nonce++);
    las_Amul(w, pp, y);                       /* w = A y           */
    hash_challenge(&c, pk, w, m, mlen);        /* c = H(pk, w, M)   */
    for(j = 0; j < LAS_M; ++j) {               /* z = y + c r       */
      polymul(&cr, &c, &sk->s[j]);
      poly_add(&sig->z[j], &y[j], &cr);
      poly_reduce(&sig->z[j]);
    }
    if(chknorm_vec(sig->z, LAS_BOUND_SIGN))
      continue;
    sig->c = c;
    return;
  }
}

int las_verify(const las_sig *sig, const uint8_t *m, size_t mlen,
               const las_pk *pk, const las_pp *pp) {
  poly w[LAS_N], ct, c2;
  unsigned int j;

  if(chknorm_vec(sig->z, LAS_BOUND_SIGN))
    return -1;

  las_Amul(w, pp, sig->z);                     /* A z               */
  for(j = 0; j < LAS_N; ++j) {                  /* w' = A z - c t    */
    polymul(&ct, &sig->c, &pk->t[j]);
    poly_sub(&w[j], &w[j], &ct);
    poly_reduce(&w[j]);
    poly_caddq(&w[j]);
  }
  hash_challenge(&c2, pk, w, m, mlen);
  return poly_equal(&c2, &sig->c) ? 0 : -1;
}

void las_presign(las_sig *presig, const uint8_t *m, size_t mlen,
                 const las_pk *Y, const las_pk *pk, const las_sk *sk, const las_pp *pp) {
  uint8_t seed[64];
  uint16_t nonce = 0;
  unsigned int j;
  poly y[LAS_M], w[LAS_N], wY[LAS_N], cr, c;

  randombytes(seed, 64);
  for(;;) {
    for(j = 0; j < LAS_M; ++j)
      sample_Sgamma(&y[j], seed, 64, nonce++);
    las_Amul(w, pp, y);                         /* w = A y                 */
    for(j = 0; j < LAS_N; ++j) {                 /* commit = w + Y          */
      poly_add(&wY[j], &w[j], &Y->t[j]);
      poly_reduce(&wY[j]);
      poly_caddq(&wY[j]);
    }
    hash_challenge(&c, pk, wY, m, mlen);          /* c = H(pk, w+Y, M)       */
    for(j = 0; j < LAS_M; ++j) {                  /* z^ = y + c r            */
      polymul(&cr, &c, &sk->s[j]);
      poly_add(&presig->z[j], &y[j], &cr);
      poly_reduce(&presig->z[j]);
    }
    if(chknorm_vec(presig->z, LAS_BOUND_PRESIGN)) /* tighter bound g-k-1     */
      continue;
    presig->c = c;
    return;
  }
}

int las_preverify(const las_sig *presig, const uint8_t *m, size_t mlen,
                  const las_pk *Y, const las_pk *pk, const las_pp *pp) {
  poly w[LAS_N], wY[LAS_N], ct, c2;
  unsigned int j;

  if(chknorm_vec(presig->z, LAS_BOUND_PRESIGN))
    return -1;

  las_Amul(w, pp, presig->z);                    /* A z^                    */
  for(j = 0; j < LAS_N; ++j) {                    /* w' = A z^ - c t         */
    polymul(&ct, &presig->c, &pk->t[j]);
    poly_sub(&w[j], &w[j], &ct);
    poly_reduce(&w[j]);
    poly_caddq(&w[j]);
  }
  for(j = 0; j < LAS_N; ++j) {                    /* w' + Y                  */
    poly_add(&wY[j], &w[j], &Y->t[j]);
    poly_reduce(&wY[j]);
    poly_caddq(&wY[j]);
  }
  hash_challenge(&c2, pk, wY, m, mlen);            /* check c == H(pk,w'+Y,M) */
  return poly_equal(&c2, &presig->c) ? 0 : -1;
}

int las_adapt(las_sig *sig, const las_sig *presig, const uint8_t *m, size_t mlen,
              const las_pk *Y, const las_sk *y, const las_pk *pk, const las_pp *pp) {
  unsigned int j;

  if(las_preverify(presig, m, mlen, Y, pk, pp))
    return -1;

  sig->c = presig->c;
  for(j = 0; j < LAS_M; ++j) {                    /* z = z^ + y_wit          */
    poly_add(&sig->z[j], &presig->z[j], &y->s[j]);
    poly_reduce(&sig->z[j]);
  }
  return 0;
}

int las_ext(las_sk *y, const las_sig *sig, const las_sig *presig,
            const las_pk *Y, const las_pp *pp) {
  poly Ay[LAS_N];
  unsigned int j;

  for(j = 0; j < LAS_M; ++j) {                    /* s = z - z^              */
    poly_sub(&y->s[j], &sig->z[j], &presig->z[j]);
    poly_reduce(&y->s[j]);
  }
  las_Amul(Ay, pp, y->s);                          /* check A s == Y          */
  for(j = 0; j < LAS_N; ++j)
    if(!poly_equal(&Ay[j], &Y->t[j]))
      return -1;
  return 0;
}
