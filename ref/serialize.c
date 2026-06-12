#include <string.h>
#include "serialize.h"
#include "las.h"
#include "poly.h"
#include "params.h"

/* ---- LSB-first bit writer / reader over a pre-zeroed buffer ---- */

static void bw_put(uint8_t *buf, size_t *bitpos, uint32_t val, unsigned bits) {
  unsigned i;
  for(i = 0; i < bits; ++i) {
    if((val >> i) & 1u)
      buf[*bitpos >> 3] |= (uint8_t)(1u << (*bitpos & 7));
    ++*bitpos;
  }
}

static uint32_t br_get(const uint8_t *buf, size_t *bitpos, unsigned bits) {
  uint32_t v = 0;
  unsigned i;
  for(i = 0; i < bits; ++i) {
    if((buf[*bitpos >> 3] >> (*bitpos & 7)) & 1u)
      v |= (1u << i);
    ++*bitpos;
  }
  return v;
}

/* Centred representative of a coefficient in (-Q/2, Q/2]. */
static int32_t centred(int32_t a) {
  a %= Q;
  if(a < 0) a += Q;
  if(a > Q / 2) a -= Q;
  return a;
}

/* ============================ public key ============================ */

void las_pack_pk(uint8_t out[LAS_PK_BYTES], const las_pk *pk) {
  size_t bp = 0;
  unsigned int i, k;
  poly t;
  memset(out, 0, LAS_PK_BYTES);
  for(i = 0; i < LAS_N; ++i) {
    t = pk->t[i];
    poly_reduce(&t);
    poly_caddq(&t);                                  /* canonical [0,Q) */
    for(k = 0; k < N; ++k)
      bw_put(out, &bp, (uint32_t)t.coeffs[k], LAS_PK_COEFF_BITS);
  }
}

int las_unpack_pk(las_pk *pk, const uint8_t in[LAS_PK_BYTES]) {
  size_t bp = 0;
  unsigned int i, k;
  uint32_t v;
  for(i = 0; i < LAS_N; ++i)
    for(k = 0; k < N; ++k) {
      v = br_get(in, &bp, LAS_PK_COEFF_BITS);
      if(v >= (uint32_t)Q) return -1;                /* defensive: reject >= Q */
      pk->t[i].coeffs[k] = (int32_t)v;
    }
  return 0;
}

/* ============================ secret key ============================ */

int las_pack_sk(uint8_t out[LAS_SK_BYTES], const las_sk *sk) {
  size_t bp = 0;
  unsigned int i, k;
  int32_t c;
  memset(out, 0, LAS_SK_BYTES);
  for(i = 0; i < LAS_M; ++i)
    for(k = 0; k < N; ++k) {
      c = centred(sk->s[i].coeffs[k]);
      if(c < -1 || c > 1) return -1;                 /* must be ternary */
      bw_put(out, &bp, (uint32_t)(c + 1), LAS_SK_COEFF_BITS);  /* {-1,0,1}->{0,1,2} */
    }
  return 0;
}

int las_unpack_sk(las_sk *sk, const uint8_t in[LAS_SK_BYTES]) {
  size_t bp = 0;
  unsigned int i, k;
  uint32_t v;
  for(i = 0; i < LAS_M; ++i)
    for(k = 0; k < N; ++k) {
      v = br_get(in, &bp, LAS_SK_COEFF_BITS);
      if(v > 2) return -1;                           /* code 3 is invalid */
      sk->s[i].coeffs[k] = (int32_t)v - 1;
    }
  return 0;
}

/* ============================ signature ============================ */

int las_pack_sig(uint8_t out[LAS_SIG_BYTES], const las_sig *sig) {
  size_t bp = 0;
  unsigned int i, k;
  int32_t c, z;
  memset(out, 0, LAS_SIG_BYTES);

  for(k = 0; k < N; ++k) {                           /* challenge c (ternary) */
    c = centred(sig->c.coeffs[k]);
    if(c < -1 || c > 1) return -1;
    bw_put(out, &bp, (uint32_t)(c + 1), LAS_C_COEFF_BITS);
  }
  for(i = 0; i < LAS_M; ++i)                          /* response z (18-bit) */
    for(k = 0; k < N; ++k) {
      z = centred(sig->z[i].coeffs[k]);
      if(z < -LAS_Z_OFFSET || z > LAS_Z_OFFSET) return -1;  /* out of band */
      bw_put(out, &bp, (uint32_t)(z + LAS_Z_OFFSET), LAS_Z_COEFF_BITS);
    }
  return 0;
}

int las_unpack_sig(las_sig *sig, const uint8_t in[LAS_SIG_BYTES]) {
  size_t bp = 0;
  unsigned int i, k;
  uint32_t v;

  for(k = 0; k < N; ++k) {
    v = br_get(in, &bp, LAS_C_COEFF_BITS);
    if(v > 2) return -1;
    sig->c.coeffs[k] = (int32_t)v - 1;
  }
  for(i = 0; i < LAS_M; ++i)
    for(k = 0; k < N; ++k) {
      v = br_get(in, &bp, LAS_Z_COEFF_BITS);
      if(v > (uint32_t)LAS_Z_MAX) return -1;          /* out of 18-bit band */
      sig->z[i].coeffs[k] = (int32_t)v - LAS_Z_OFFSET;
    }
  return 0;
}

/* ===================== on-chain-style verifier ===================== */

int las_verify_packed(const uint8_t pk_b[LAS_PK_BYTES],
                      const uint8_t sig_b[LAS_SIG_BYTES],
                      const uint8_t *m, size_t mlen, const las_pp *pp) {
  las_pk pk;
  las_sig sig;
  if(las_unpack_pk(&pk, pk_b))   return -1;           /* malformed pk   */
  if(las_unpack_sig(&sig, sig_b)) return -1;          /* malformed sig  */
  return las_verify(&sig, m, mlen, &pk, pp);          /* ordinary Verify */
}
