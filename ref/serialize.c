/*
 * serialize.c -- the LAS wire codec (ref/packing.{c,h} twin), C mirror of
 * rust/fips204-las/src/serialize.rs.  Three private encoders, six typed wrapper
 * pairs over them (see serialize.h).  The response z is packed with FIPS BitPack
 * (b - w, LSB-first, byte-identical to the Rust conversion::bit_pack); the
 * challenge is the raw 32-byte digest c_tilde.  Ring-degree loops read LAS_D
 * (the LAS-file convention).
 */
#include <string.h>
#include "serialize.h"
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

/* ============ private shared encoders (one per wire layout) ============ */

/* 23-bit canonical n-vector (public key t / statement t'); canonicalises to
 * [0,Q) on encode.  Shared by pack_public_key / pack_statement. */
static void encode_canonical_vec(uint8_t out[PUBLIC_KEY_BYTES], const poly t[LAS_N]) {
  size_t bp = 0;
  unsigned int i, k;
  poly tmp;
  memset(out, 0, PUBLIC_KEY_BYTES);
  for(i = 0; i < LAS_N; ++i) {
    tmp = t[i];
    poly_reduce(&tmp);
    poly_caddq(&tmp);                                /* canonical [0,Q) */
    for(k = 0; k < LAS_D; ++k)
      bw_put(out, &bp, (uint32_t)tmp.coeffs[k], LAS_PK_COEFF_BITS);
  }
}

/* Validating inverse of encode_canonical_vec; -1 on any coeff >= Q. */
static int decode_canonical_vec(poly t[LAS_N], const uint8_t in[PUBLIC_KEY_BYTES]) {
  size_t bp = 0;
  unsigned int i, k;
  uint32_t v;
  for(i = 0; i < LAS_N; ++i)
    for(k = 0; k < LAS_D; ++k) {
      v = br_get(in, &bp, LAS_PK_COEFF_BITS);
      if(v >= (uint32_t)Q) return -1;                /* defensive: reject >= Q */
      t[i].coeffs[k] = (int32_t)v;
    }
  return 0;
}

/* 2-bit ternary (n+ell)-vector (secret key r / honest witness r'); -1 if any
 * coefficient is non-ternary.  Shared by pack_secret_key / pack_witness. */
static int encode_ternary_vec(uint8_t out[SECRET_KEY_BYTES], const poly s[N_PLUS_ELL]) {
  size_t bp = 0;
  unsigned int i, k;
  int32_t c;
  memset(out, 0, SECRET_KEY_BYTES);
  for(i = 0; i < N_PLUS_ELL; ++i)
    for(k = 0; k < LAS_D; ++k) {
      c = centred(s[i].coeffs[k]);
      if(c < -1 || c > 1) return -1;                 /* must be ternary */
      bw_put(out, &bp, (uint32_t)(c + 1), LAS_SK_COEFF_BITS);  /* {-1,0,1}->{0,1,2} */
    }
  return 0;
}

/* Validating inverse of encode_ternary_vec; -1 on the invalid 2-bit code 3. */
static int decode_ternary_vec(poly s[N_PLUS_ELL], const uint8_t in[SECRET_KEY_BYTES]) {
  size_t bp = 0;
  unsigned int i, k;
  uint32_t v;
  for(i = 0; i < N_PLUS_ELL; ++i)
    for(k = 0; k < LAS_D; ++k) {
      v = br_get(in, &bp, LAS_SK_COEFF_BITS);
      if(v > 2) return -1;                           /* code 3 is invalid */
      s[i].coeffs[k] = (int32_t)v - 1;
    }
  return 0;
}

/* c_tilde || BitPack(z): the raw 32-byte challenge digest, then the response z
 * packed per-coefficient as the FIPS BitPack value (LAS_Z_OFFSET - z, i.e. b-w)
 * LSB-first -- byte-identical to the Rust conversion::bit_pack(z, gamma-kappa,
 * gamma-kappa).  bw_put is the byte-identical bit primitive, and the (n+ell)
 * polys pack contiguously (each is 256*bits = whole bytes, so per-poly byte
 * alignment holds either way).  -1 only on an out-of-band z coefficient; the
 * digest half never fails.  Shared by pack_signature / pack_pre_signature. */
static int encode_chal_response(uint8_t out[SIGNATURE_BYTES],
                                const uint8_t c_tilde[LAS_CTILDEBYTES],
                                const poly z[N_PLUS_ELL]) {
  size_t bp;
  unsigned int i, k;
  int32_t zz;
  memset(out, 0, SIGNATURE_BYTES);
  memcpy(out, c_tilde, LAS_CTILDEBYTES);              /* raw 32-byte digest */
  bp = 8u * (size_t)LAS_CTILDEBYTES;                  /* z region starts after the digest */
  for(i = 0; i < N_PLUS_ELL; ++i)                     /* response z / z_hat (FIPS BitPack) */
    for(k = 0; k < LAS_D; ++k) {
      zz = centred(z[i].coeffs[k]);
      if(zz < -LAS_Z_OFFSET || zz > LAS_Z_OFFSET) return -1;  /* out of band */
      bw_put(out, &bp, (uint32_t)(LAS_Z_OFFSET - zz), LAS_Z_COEFF_BITS);  /* BitPack: b - w */
    }
  return 0;
}

/* Inverse of encode_chal_response: raw digest out, then z decoded per-coeff as
 * z = LAS_Z_OFFSET - field (FIPS BitUnpack: b - field), byte-identical to the
 * Rust conversion::bit_unpack.  Its range is permissive (every field decodes in
 * range), so there is NO z rejection here -- a tampered z is caught at Verify.
 * Always 0 (kept int for API symmetry with the validating pk/sk codecs). */
static int decode_chal_response(uint8_t c_tilde[LAS_CTILDEBYTES],
                                poly z[N_PLUS_ELL],
                                const uint8_t in[SIGNATURE_BYTES]) {
  size_t bp;
  unsigned int i, k;
  uint32_t v;

  memcpy(c_tilde, in, LAS_CTILDEBYTES);
  bp = 8u * (size_t)LAS_CTILDEBYTES;
  for(i = 0; i < N_PLUS_ELL; ++i)
    for(k = 0; k < LAS_D; ++k) {
      v = br_get(in, &bp, LAS_Z_COEFF_BITS);
      z[i].coeffs[k] = LAS_Z_OFFSET - (int32_t)v;     /* BitUnpack: b - field */
    }
  return 0;
}

/* ==================== public typed wrappers (six pairs) ==================== */

/* ---- public key ---- */
void pack_public_key(uint8_t out[PUBLIC_KEY_BYTES], const public_key *pk) {
  encode_canonical_vec(out, pk->t);
}
int unpack_public_key(public_key *pk, const uint8_t in[PUBLIC_KEY_BYTES]) {
  return decode_canonical_vec(pk->t, in);
}

/* ---- statement Y = t' (same wire layout as a public key; distinct type) ---- */
void pack_statement(uint8_t out[STATEMENT_BYTES], const statement *Y) {
  encode_canonical_vec(out, Y->t_prime);
}
int unpack_statement(statement *Y, const uint8_t in[STATEMENT_BYTES]) {
  return decode_canonical_vec(Y->t_prime, in);
}

/* ---- secret key ---- */
int pack_secret_key(uint8_t out[SECRET_KEY_BYTES], const secret_key *sk) {
  return encode_ternary_vec(out, sk->r);
}
int unpack_secret_key(secret_key *sk, const uint8_t in[SECRET_KEY_BYTES]) {
  return decode_ternary_vec(sk->r, in);
}

/* ---- honest (ternary) witness r' (same ternary wire form as a secret key) ---- */
int pack_witness(uint8_t out[WITNESS_BYTES], const witness *w) {
  return encode_ternary_vec(out, w->value);
}
int unpack_witness(witness *w, const uint8_t in[WITNESS_BYTES]) {
  return decode_ternary_vec(w->value, in);
}

/* ---- signature (c_tilde, z) ---- */
int pack_signature(uint8_t out[SIGNATURE_BYTES], const signature *sig) {
  return encode_chal_response(out, sig->c_tilde, sig->z);
}
int unpack_signature(signature *sig, const uint8_t in[SIGNATURE_BYTES]) {
  return decode_chal_response(sig->c_tilde, sig->z, in);
}

/* ---- pre-signature (c_tilde, z_hat) (same wire layout as a signature; distinct type) ---- */
int pack_pre_signature(uint8_t out[PRE_SIGNATURE_BYTES], const pre_signature *presig) {
  return encode_chal_response(out, presig->c_tilde, presig->z_hat);
}
int unpack_pre_signature(pre_signature *presig, const uint8_t in[PRE_SIGNATURE_BYTES]) {
  return decode_chal_response(presig->c_tilde, presig->z_hat, in);
}

/* (base_verify_packed, the on-chain-style verifier entry point, lives in
 * ref/basesig.c with the rest of the end-to-end packed-API tier: packing belongs
 * INSIDE the scheme's byte-boundary functions, as in upstream sign.c, while this
 * file stays the pure codec.) */
