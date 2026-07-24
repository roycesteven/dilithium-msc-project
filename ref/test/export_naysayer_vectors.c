/* export_naysayer_vectors.c — export a PURE DIGEST-FAULT vector for the optimistic
 * Naysayer verifier (evm/src/LASNaysayer.sol) test suite.
 *
 * The signature is a valid adapted LAS signature whose c_tilde has ONE byte flipped,
 * with z UNCHANGED. The exported trace w'_df is the TRUE arithmetic for the tampered
 * challenge c = SampleInBall(c_tilde'), so:
 *   - norm(z) is within bound (z unchanged)              -> naysayNorm must NOT land
 *   - w'_df matches the derived-c arithmetic             -> naysayWprime must NOT land
 *   - SHAKE256(pack(t)||pack(w'_df)||M) != c_tilde'       -> ONLY naysayDigest lands
 * This exercises the load-bearing, irreducible digest naysay in isolation.
 *
 * Reuses pp_normal.bin / t.bin / msg.bin from export_verify_vector.c (SAME seeds), so run
 * that FIRST. Emits only:  sig_digestfault.bin , w_prime_digestfault.bin
 *
 * Deterministic. A' is emitted implicitly via the shared pp_normal.bin; here only the
 * NORMAL-domain A' is recovered internally to compute w'_df.
 *
 * Usage:  make test/export_naysayer_vectors && ./test/export_naysayer_vectors <outdir>
 *         (default outdir = "../evm/test/vectors")
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../basesig.h"
#include "../relation.h"
#include "../las.h"
#include "../serialize.h"
#include "../poly.h"
#include "../setup.h"
#include "../fips202.h"

static void write_poly(FILE *f, const poly *p) {
  unsigned int i;
  for(i = 0; i < LAS_D; ++i) {
    uint32_t x = (uint32_t)p->coeffs[i];
    uint8_t b[4] = { (uint8_t)x, (uint8_t)(x>>8), (uint8_t)(x>>16), (uint8_t)(x>>24) };
    fwrite(b, 1, 4, f);
  }
}

static int32_t centred(int32_t a) {
  a %= Q;
  if(a < 0) a += Q;
  if(a > Q / 2) a -= Q;
  return a;
}

/* FAITHFUL COPY of basesig.c b_poly_challenge (as in export_verify_vector.c): SampleInBall
 * H over SHAKE256(c_tilde), KAPPA nonzero coeffs in {-1,+1}. Ground truth for the reused
 * ZKNox sampleInBallNist that LASNaysayer derives on-chain. */
static void export_challenge(poly *c, const uint8_t seed[LAS_SEEDBYTES]) {
  unsigned int i, b, pos;
  uint64_t signs;
  uint8_t buf[SHAKE256_RATE];
  keccak_state state;
  shake256_init(&state);
  shake256_absorb(&state, seed, LAS_SEEDBYTES);
  shake256_finalize(&state);
  shake256_squeezeblocks(buf, 1, &state);
  signs = 0;
  for(i = 0; i < 8; ++i) signs |= (uint64_t)buf[i] << 8*i;
  pos = 8;
  for(i = 0; i < LAS_D; ++i) c->coeffs[i] = 0;
  for(i = LAS_D - KAPPA; i < LAS_D; ++i) {
    do {
      if(pos >= SHAKE256_RATE) { shake256_squeezeblocks(buf, 1, &state); pos = 0; }
      b = buf[pos++];
    } while(b > i);
    c->coeffs[i] = c->coeffs[b];
    c->coeffs[b] = 1 - 2*(int32_t)(signs & 1);
    signs >>= 1;
  }
}

static void pack_w_local(uint8_t *r, const poly *v, unsigned int count) {
  unsigned int i, k;
  for(i = 0; i < count; ++i) {
    poly t = v[i];
    poly_reduce(&t);
    poly_caddq(&t);
    for(k = 0; k < LAS_D; ++k) {
      uint32_t x = (uint32_t)t.coeffs[k];
      r[4*(i*LAS_D+k)+0] = (uint8_t)x;
      r[4*(i*LAS_D+k)+1] = (uint8_t)(x >> 8);
      r[4*(i*LAS_D+k)+2] = (uint8_t)(x >> 16);
      r[4*(i*LAS_D+k)+3] = (uint8_t)(x >> 24);
    }
  }
}

static void negacyclic_conv(poly *c, const poly *a, const poly *b) {
  int64_t acc[LAS_D];
  unsigned int i, j, k;
  for(i = 0; i < LAS_D; ++i) acc[i] = 0;
  for(i = 0; i < LAS_D; ++i)
    for(j = 0; j < LAS_D; ++j) {
      int64_t prod = ((int64_t)a->coeffs[i] * (int64_t)b->coeffs[j]) % Q;
      k = i + j;
      if(k < LAS_D) acc[k]         = (acc[k]         + prod) % Q;
      else          acc[k - LAS_D] = (acc[k - LAS_D] - prod) % Q;
    }
  for(i = 0; i < LAS_D; ++i) {
    int64_t v = acc[i] % Q;
    if(v < 0) v += Q;
    c->coeffs[i] = (int32_t)v;
  }
}

static void recover_normal(poly *out, const poly *ahat, const poly *one_hat) {
  poly_pointwise_montgomery(out, ahat, one_hat);
  poly_invntt_tomont(out);
  poly_reduce(out);
  poly_caddq(out);
}

static FILE *openout(const char *dir, const char *name) {
  char path[512];
  snprintf(path, sizeof path, "%s/%s", dir, name);
  FILE *f = fopen(path, "wb");
  if(!f) perror(path);
  return f;
}

int main(int argc, char **argv) {
  const char *dir = (argc > 1) ? argv[1] : "../evm/test/vectors";

  uint8_t ppseed[LAS_SEEDBYTES], kseed[LAS_SEEDBYTES], rseed[LAS_SEEDBYTES], msg[32];
  public_params pp;
  public_key pk;
  statement Y;
  secret_key sk;
  witness r_prime;
  pre_signature presig;
  signature adapted;
  uint8_t sig_b[SIGNATURE_BYTES], sig_df[SIGNATURE_BYTES], ctilde_df[LAS_CTILDEBYTES];
  poly aprime_normal[LAS_N][ELL], one_hat, chal_df, wprime_df[LAS_N];
  unsigned int i, j, k;
  FILE *f;

  /* Same fixed seeds as export_verify_vector.c: pp/pk/msg identical to the shared vectors. */
  for(i = 0; i < LAS_SEEDBYTES; ++i) { ppseed[i]=(uint8_t)i; kseed[i]=(uint8_t)(i+1); rseed[i]=(uint8_t)(i+100); }
  for(i = 0; i < 32; ++i) msg[i] = (uint8_t)i;

  setup_public_params(&pp, ppseed);
  if(base_keygen_seed(&pk, &sk, &pp, kseed))                        { fprintf(stderr,"keygen failed\n"); return 1; }
  if(relation_gen_seed(&Y, &r_prime, &pp, rseed))                  { fprintf(stderr,"gen failed\n");    return 1; }
  if(las_presign_det(&presig, msg, 32, &Y, &pk, &sk, &pp))          { fprintf(stderr,"presign failed\n");return 1; }
  if(las_adapt(&adapted, &presig, msg, 32, &Y, &r_prime, &pk, &pp)) { fprintf(stderr,"adapt failed\n"); return 1; }
  if(pack_signature(sig_b, &adapted))                              { fprintf(stderr,"pack failed\n");   return 1; }
  if(base_verify(&adapted, msg, 32, &pk, &pp)) { fprintf(stderr,"FATAL: C rejected the base adapted sig\n"); return 1; }

  /* Tamper: flip ONE c_tilde byte; z unchanged. c_tilde is the first LAS_CTILDEBYTES of the
   * packed signature (pack_signature writes c_tilde first). */
  memcpy(sig_df, sig_b, SIGNATURE_BYTES);
  memcpy(ctilde_df, adapted.c_tilde, LAS_CTILDEBYTES);
  sig_df[0]    ^= 0x01;
  ctilde_df[0] ^= 0x01;

  /* Recover NORMAL-domain A' (pp.a_prime is stored in NTT domain). */
  {
    poly one;
    for(k = 0; k < LAS_D; ++k) one.coeffs[k] = (k == 0) ? 1 : 0;
    one_hat = one;
    poly_ntt(&one_hat);
    for(i = 0; i < LAS_N; ++i)
      for(j = 0; j < ELL; ++j)
        recover_normal(&aprime_normal[i][j], &pp.a_prime[i][j], &one_hat);
  }

  /* Consistent trace for the TAMPERED challenge: c = SampleInBall(c_tilde'),
   * w'_df = z_top + A'*z_bot - c*t. */
  export_challenge(&chal_df, ctilde_df);
  for(i = 0; i < LAS_N; ++i) {
    int64_t acc[LAS_D];
    poly zbot, conv, ct;
    for(k = 0; k < LAS_D; ++k) acc[k] = 0;
    for(j = 0; j < ELL; ++j) {
      for(k = 0; k < LAS_D; ++k) zbot.coeffs[k] = centred(adapted.z[LAS_N + j].coeffs[k]);
      negacyclic_conv(&conv, &aprime_normal[i][j], &zbot);
      for(k = 0; k < LAS_D; ++k) acc[k] = (acc[k] + conv.coeffs[k]) % Q;
    }
    for(k = 0; k < LAS_D; ++k) acc[k] = (acc[k] + centred(adapted.z[i].coeffs[k])) % Q;
    negacyclic_conv(&ct, &chal_df, &pk.t[i]);
    for(k = 0; k < LAS_D; ++k) acc[k] = (acc[k] - ct.coeffs[k]) % Q;
    for(k = 0; k < LAS_D; ++k) { int64_t v = acc[k] % Q; if(v < 0) v += Q; wprime_df[i].coeffs[k] = (int32_t)v; }
  }

  if(!(f = openout(dir, "sig_digestfault.bin"))) return 1;
  fwrite(sig_df, 1, SIGNATURE_BYTES, f);
  fclose(f);
  if(!(f = openout(dir, "w_prime_digestfault.bin"))) return 1;
  for(i = 0; i < LAS_N; ++i) write_poly(f, &wprime_df[i]);
  fclose(f);

  /* Self-checks: (a) z within bound; (b) the trace is genuinely a DIGEST fault. */
  {
    int32_t bound = (int32_t)(GAMMA - KAPPA);
    int over = 0;
    for(i = 0; i < N_PLUS_ELL; ++i)
      for(k = 0; k < LAS_D; ++k) { int32_t v = centred(adapted.z[i].coeffs[k]); if(v < 0) v = -v; if(v > bound) over = 1; }
    fprintf(stderr, "  z norm %s bound (naysayNorm must %s)\n", over ? ">" : "<=", over ? "LAND" : "REVERT");
  }
  {
    uint8_t tpk[LAS_N*LAS_D*4], wpk[LAS_N*LAS_D*4], d[LAS_CTILDEBYTES];
    keccak_state st;
    int mism = 0;
    pack_w_local(tpk, pk.t, LAS_N);
    pack_w_local(wpk, wprime_df, LAS_N);
    shake256_init(&st);
    shake256_absorb(&st, tpk, sizeof tpk);
    shake256_absorb(&st, wpk, sizeof wpk);
    shake256_absorb(&st, msg, 32);
    shake256_finalize(&st);
    shake256_squeeze(d, LAS_CTILDEBYTES, &st);
    for(k = 0; k < LAS_CTILDEBYTES; ++k) if(d[k] != ctilde_df[k]) mism = 1;
    fprintf(stderr, "  SHAKE256(pack(t)||pack(w'_df)||M) %s c_tilde'  (naysayDigest must %s)\n",
            mism ? "!=" : "==", mism ? "LAND" : "REVERT — VECTOR IS WRONG");
    if(!mism) { fprintf(stderr, "FATAL: digest matched; not a digest fault\n"); return 1; }
  }

  fprintf(stderr, "Wrote sig_digestfault.bin (%u bytes) and w_prime_digestfault.bin (%u polys) to %s/\n",
          (unsigned int)SIGNATURE_BYTES, (unsigned int)LAS_N, dir);
  return 0;
}
