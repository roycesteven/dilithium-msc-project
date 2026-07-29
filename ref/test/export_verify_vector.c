/* export_verify_vector.c — export a COMPLETE golden verification vector for the
 * on-chain (Solidity) LAS verifier, plus an isolated arithmetic golden vector,
 * so each Solidity module can be validated against C ground truth BEFORE the
 * full verifier is assembled.  Superset of export_packed.c (which emits only
 * sig.bin).
 *
 * Deterministic (fixed seeds), so every file is reproducible.  The on-chain
 * verifier mirrors base_verify_internal (ref/basesig.c): it consumes
 *   (A' , t , M , packed adapted sig)
 * and recomputes  w' = z_top + A'*z_bot - c*t  and the challenge hash.
 *
 * DOMAIN NOTE.  The reused on-chain NTT (ZKNox, evm/lib/zknox) is a NORMAL-domain
 * negacyclic NTT (plain mulmod, N^{-1} inverse scaling), NOT ref/ntt.c's
 * Montgomery-domain NTT.  So this exporter emits A' in NORMAL domain (regenerated
 * with the SAME expansion setup uses -- poly_uniform, nonce (i<<8)+j -- but
 * BEFORE the NTT), and t is already canonical.  All poly data is written as raw
 * int32 LITTLE-ENDIAN; every value here is in [0,Q) (non-negative), so the
 * Solidity side reads each coefficient as a plain uint32.
 *
 * Usage:  make test/export_verify_vector && ./test/export_verify_vector <outdir>
 *         (default outdir = "../evm/test/vectors")
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "../basesig.h"    /* base_keygen_seed, public_key { poly t[LAS_N] } */
#include "../relation.h"   /* relation_gen_seed -> (statement, witness)      */
#include "../las.h"        /* las_presign_det / las_adapt                    */
#include "../serialize.h"  /* pack_signature, SIGNATURE_BYTES                */
#include "../poly.h"       /* poly_uniform (same expansion setup uses)       */
#include "../setup.h"      /* LAS_N, ELL, LAS_D, Q, KAPPA, public_params     */
#include "../fips202.h"    /* shake256 stream (for the SampleInBall golden)  */

/* Write one polynomial as LAS_D int32 little-endian (two's complement). */
static void write_poly(FILE *f, const poly *p) {
  unsigned int i;
  for(i = 0; i < LAS_D; ++i) {
    uint32_t x = (uint32_t)p->coeffs[i];      /* two's complement bit pattern */
    uint8_t b[4] = { (uint8_t)x, (uint8_t)(x>>8), (uint8_t)(x>>16), (uint8_t)(x>>24) };
    fwrite(b, 1, 4, f);
  }
}

/* centred representative in (-Q/2, Q/2], mirrors serialize.c centred(). */
static int32_t centred(int32_t a) {
  a %= Q;
  if(a < 0) a += Q;
  if(a > Q / 2) a -= Q;
  return a;
}

/* Write a poly canonicalised to [0,Q): centred(coeff) then +Q if negative.
 * Matches how the Solidity side represents challenge/z coefficients (a signed
 * value -v is compared as its residue Q-v). */
static void write_poly_canonical(FILE *f, const poly *p) {
  unsigned int i;
  for(i = 0; i < LAS_D; ++i) {
    int32_t c = centred(p->coeffs[i]);
    uint32_t canon = (c < 0) ? (uint32_t)(c + Q) : (uint32_t)c;
    uint8_t b[4] = { (uint8_t)canon, (uint8_t)(canon>>8), (uint8_t)(canon>>16), (uint8_t)(canon>>24) };
    fwrite(b, 1, 4, f);
  }
}

/* Replicate b_polyvecn_pack_w / b_polyw_pack (static in basesig.c): pack `count`
 * polys, each reduce+caddq to [0,Q) then 4-byte LE per coeff. Used only for the
 * in-C self-check that w_prime.bin reproduces the challenge digest. */
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

/* FAITHFUL COPY of basesig.c b_poly_challenge (static there) -- the SampleInBall
 * H: KAPPA nonzero coefficients in {-1,+1} from SHAKE256(c_tilde). Ground truth
 * for the reused ZKNox sampleInBallNist(c_tilde, KAPPA, Q). Kept in lockstep
 * with basesig.c; if that changes, change this too. */
static void export_challenge(poly *c, const uint8_t seed[LAS_CTILDEBYTES]) {
  unsigned int i, b, pos;
  uint64_t signs;
  uint8_t buf[SHAKE256_RATE];
  keccak_state state;

  shake256_init(&state);
  shake256_absorb(&state, seed, LAS_CTILDEBYTES);
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

static FILE *openout(const char *dir, const char *name) {
  char path[512];
  snprintf(path, sizeof path, "%s/%s", dir, name);
  FILE *f = fopen(path, "wb");
  if(!f) perror(path);
  return f;
}

/* Schoolbook negacyclic convolution c = a (X) b  mod (X^LAS_D + 1, Q), inputs
 * and output canonical in [0,Q).  This is the ground truth for the ZKNox NTT:
 * nttInv(vecMulMod(nttFw(a), nttFw(b))) MUST equal this. */
static void negacyclic_conv(poly *c, const poly *a, const poly *b) {
  int64_t acc[LAS_D];
  unsigned int i, j, k;
  for(i = 0; i < LAS_D; ++i) acc[i] = 0;
  for(i = 0; i < LAS_D; ++i)
    for(j = 0; j < LAS_D; ++j) {
      int64_t prod = ((int64_t)a->coeffs[i] * (int64_t)b->coeffs[j]) % Q;
      k = i + j;
      if(k < LAS_D) acc[k]         = (acc[k]         + prod) % Q;   /* X^k, k<d      */
      else          acc[k - LAS_D] = (acc[k - LAS_D] - prod) % Q;   /* X^d = -1      */
    }
  for(i = 0; i < LAS_D; ++i) {
    int64_t v = acc[i] % Q;
    if(v < 0) v += Q;
    c->coeffs[i] = (int32_t)v;
  }
}

/* Recover the NORMAL-domain polynomial from its NTT-domain form. setup_public_params
 * samples A' DIRECTLY in NTT domain (poly_uniform output = Â', Dilithium convention),
 * so the on-chain verifier — which uses a different (ZKNox) NTT — needs the normal
 * A'. Uses the Dilithium product idiom "multiply by 1": pointwise_montgomery(Â', ntt(1))
 * then invntt_tomont yields A'·1 = A' in the normal domain, canonicalised to [0,Q).
 * (No manual Montgomery bookkeeping: R factors cancel via the standard idiom.) */
static void recover_normal(poly *out, const poly *ahat, const poly *one_hat) {
  poly_pointwise_montgomery(out, ahat, one_hat);
  poly_invntt_tomont(out);
  poly_reduce(out);
  poly_caddq(out);
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
  uint8_t sig_b[SIGNATURE_BYTES];
  unsigned int i, j;
  FILE *f;
  poly aprime_normal[LAS_N][ELL];   /* A' recovered to NORMAL domain (Stage 5)      */
  poly one_hat;                     /* ntt(1), for the recover_normal idiom          */

  /* Same fixed seeds as export_packed.c, so sig.bin here == las_sig.bin. */
  for(i = 0; i < LAS_SEEDBYTES; ++i) { ppseed[i]=(uint8_t)i; kseed[i]=(uint8_t)(i+1); rseed[i]=(uint8_t)(i+100); }
  for(i = 0; i < 32; ++i) msg[i] = (uint8_t)i;

  setup_public_params(&pp, ppseed);
  if(base_keygen_seed(&pk, &sk, &pp, kseed))                        { fprintf(stderr,"keygen failed\n"); return 1; }
  if(relation_gen_seed(&Y, &r_prime, &pp, rseed))                  { fprintf(stderr,"gen failed\n");    return 1; }
  if(las_presign_det(&presig, msg, 32, &Y, &pk, &sk, &pp))          { fprintf(stderr,"presign failed\n");return 1; }
  if(las_adapt(&adapted, &presig, msg, 32, &Y, &r_prime, &pk, &pp)) { fprintf(stderr,"adapt failed\n"); return 1; }
  if(pack_signature(sig_b, &adapted))                              { fprintf(stderr,"pack failed\n");   return 1; }

  /* Sanity: the adapted signature MUST verify in C — the ground truth the
   * Solidity end-to-end test (Stage 5) must reproduce. */
  if(base_verify(&adapted, msg, 32, &pk, &pp)) { fprintf(stderr,"FATAL: C base_verify rejected the golden sig\n"); return 1; }

  /* ---- verifier inputs ------------------------------------------------ */
  /* A' in NORMAL domain: pp.a_prime is stored in NTT domain (poly_uniform samples
   * it there), so recover the normal-domain A' the on-chain verifier nttFw's with
   * the reused ZKNox NTT. */
  {
    poly one;
    unsigned int k;
    for(k = 0; k < LAS_D; ++k) one.coeffs[k] = (k == 0) ? 1 : 0;
    one_hat = one;
    poly_ntt(&one_hat);
    for(i = 0; i < LAS_N; ++i)
      for(j = 0; j < ELL; ++j)
        recover_normal(&aprime_normal[i][j], &pp.a_prime[i][j], &one_hat);
  }
  if(!(f = openout(dir, "pp_normal.bin"))) return 1;
  for(i = 0; i < LAS_N; ++i)
    for(j = 0; j < ELL; ++j)
      write_poly(f, &aprime_normal[i][j]);
  fclose(f);

  /* t : LAS_N polys, canonical [0,Q) (the public key). */
  if(!(f = openout(dir, "t.bin"))) return 1;
  for(i = 0; i < LAS_N; ++i) write_poly(f, &pk.t[i]);
  fclose(f);

  if(!(f = openout(dir, "msg.bin"))) return 1;
  fwrite(msg, 1, 32, f);
  fclose(f);
  if(!(f = openout(dir, "sig.bin"))) return 1;
  fwrite(sig_b, 1, SIGNATURE_BYTES, f);
  fclose(f);

  /* ---- arithmetic golden vector: schoolbook negacyclic convolution, to
   *      validate the reused ZKNox NTT in isolation (Stage 3) before wiring
   *      the whole verifier.  Inputs via poly_uniform (canonical [0,Q)), with
   *      nonces disjoint from the A' nonces above (max 0x0504). ---------- */
  poly ca, cb, cc;
  poly_uniform(&ca, pp.seed, (uint16_t)0xF001);
  poly_uniform(&cb, pp.seed, (uint16_t)0xF002);
  negacyclic_conv(&cc, &ca, &cb);
  if(!(f = openout(dir, "conv_a.bin")))   return 1;
  write_poly(f, &ca);
  fclose(f);
  if(!(f = openout(dir, "conv_b.bin")))   return 1;
  write_poly(f, &cb);
  fclose(f);
  if(!(f = openout(dir, "conv_out.bin"))) return 1;
  write_poly(f, &cc);
  fclose(f);

  /* ---- Stage-4 goldens ------------------------------------------------ */
  /* c.bin : the challenge polynomial SampleInBall(c_tilde), canonical {0,1,Q-1}
   * (validates the reused ZKNox sampleInBallNist against our C H). */
  poly chal;
  export_challenge(&chal, adapted.c_tilde);
  if(!(f = openout(dir, "c.bin"))) return 1;
  write_poly_canonical(f, &chal);
  fclose(f);

  /* z.bin : the adapted signature's response z (N_PLUS_ELL polys), centred then
   * canonical [0,Q) — the ground truth for the Solidity BitPack19 z-decode of
   * sig.bin, and the input to the norm check ||z||inf <= gamma-kappa. */
  if(!(f = openout(dir, "z.bin"))) return 1;
  for(i = 0; i < N_PLUS_ELL; ++i) write_poly_canonical(f, &adapted.z[i]);
  fclose(f);

  /* w_prime.bin : expected w' = z_top + A'*z_bot - c*t (LAS_N polys, canonical
   * [0,Q)), computed INDEPENDENTLY via schoolbook negacyclic_conv — isolates the
   * Solidity arithmetic (Stage 5) from the hash. z_top = z[0..n-1] (identity
   * block of A=[I|A']), z_bot = z[n..n+ell-1]. */
  {
    poly wprime_arr[LAS_N];
    unsigned int a, bb, k;

    for(a = 0; a < LAS_N; ++a) {
      int64_t acc[LAS_D];
      poly zbot, conv, ct;
      for(k = 0; k < LAS_D; ++k) acc[k] = 0;
      /* A'*z_bot (A' normal-domain, z_bot centred) */
      for(bb = 0; bb < ELL; ++bb) {
        for(k = 0; k < LAS_D; ++k) zbot.coeffs[k] = centred(adapted.z[LAS_N + bb].coeffs[k]);
        negacyclic_conv(&conv, &aprime_normal[a][bb], &zbot);
        for(k = 0; k < LAS_D; ++k) acc[k] = (acc[k] + conv.coeffs[k]) % Q;
      }
      /* + z_top[a] (centred, identity block) */
      for(k = 0; k < LAS_D; ++k) acc[k] = (acc[k] + centred(adapted.z[a].coeffs[k])) % Q;
      /* - c*t[a] */
      negacyclic_conv(&ct, &chal, &pk.t[a]);
      for(k = 0; k < LAS_D; ++k) acc[k] = (acc[k] - ct.coeffs[k]) % Q;
      /* canonical [0,Q) */
      for(k = 0; k < LAS_D; ++k) { int64_t v = acc[k] % Q; if(v < 0) v += Q; wprime_arr[a].coeffs[k] = (int32_t)v; }
    }

    if(!(f = openout(dir, "w_prime.bin"))) return 1;
    for(a = 0; a < LAS_N; ++a) write_poly(f, &wprime_arr[a]);
    fclose(f);

    /* DECISIVE in-C self-check: does this independently-computed w' reproduce the
     * stored challenge?  SHAKE256(pack(t) || pack(w') || M) == c_tilde ? */
    {
      uint8_t tpk[LAS_N*LAS_D*4], wpk[LAS_N*LAS_D*4], ctilde2[LAS_CTILDEBYTES];
      keccak_state st;
      int mism = 0;
      pack_w_local(tpk, pk.t, LAS_N);
      pack_w_local(wpk, wprime_arr, LAS_N);
      shake256_init(&st);
      shake256_absorb(&st, tpk, sizeof tpk);
      shake256_absorb(&st, wpk, sizeof wpk);
      shake256_absorb(&st, msg, 32);
      shake256_finalize(&st);
      shake256_squeeze(ctilde2, LAS_CTILDEBYTES, &st);
      for(k = 0; k < LAS_CTILDEBYTES; ++k) if(ctilde2[k] != adapted.c_tilde[k]) mism = 1;
      fprintf(stderr, "  C self-check: SHAKE256(pack(t)||pack(w')||M) %s c_tilde\n",
              mism ? "!=  (w_prime.bin is WRONG)" : "==  (w_prime.bin OK)");
    }
  }

  fprintf(stderr,
    "Wrote verifier vectors to %s/\n"
    "  pp_normal.bin: %u polys, %u bytes\n"
    "  t.bin:         %u polys, %u bytes\n"
    "  msg.bin:       32 bytes\n"
    "  sig.bin:       %u bytes\n"
    "  conv_a.bin:    1 poly, %u bytes\n"
    "  conv_b.bin:    1 poly, %u bytes\n"
    "  conv_out.bin:  1 poly, %u bytes\n"
    "  c.bin:         1 poly, %u bytes\n"
    "  z.bin:         %u polys, %u bytes\n"
    "  C base_verify(golden sig) = ACCEPT\n"
    "  Solidity verifier must reproduce the same result.\n",
    dir,

    (unsigned int)(LAS_N * ELL),
    (unsigned int)(LAS_N * ELL * LAS_D * 4),

    (unsigned int)LAS_N,
    (unsigned int)(LAS_N * LAS_D * 4),

    (unsigned int)SIGNATURE_BYTES,

    (unsigned int)(LAS_D * 4),
    (unsigned int)(LAS_D * 4),
    (unsigned int)(LAS_D * 4),
    (unsigned int)(LAS_D * 4),

    (unsigned int)N_PLUS_ELL,
    (unsigned int)(N_PLUS_ELL * LAS_D * 4)
  );
  return 0;
}
