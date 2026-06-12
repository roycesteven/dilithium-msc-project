/*
 * Known-Answer Tests (KATs) for LAS  -  reproducibility (objective C4).
 *
 * Every input here is FIXED (public-parameter seed, key seeds, statement seeds,
 * messages), and keygen / sign / presign are DETERMINISTIC (las_keygen_seed,
 * las_sign_det, las_presign_det).  The whole adaptor contract is therefore a pure
 * function of these inputs, so the serialised outputs are byte-for-byte
 * reproducible across machines and compilers.  We fold the packed bytes of
 * (pk, sk, sig, pre-sig, adapted-sig) for several vectors into one SHAKE256 digest
 * and assert it equals a pinned expected value: a single 32-byte fingerprint that
 * locks down the entire implementation.  A cross-implementation verifier
 * (Solidity / circuit, poqeth-style) can be checked against the very same vectors.
 *
 * Per vector we also assert the adaptor contract end-to-end (Verify(sig)=ok,
 * Verify(adapted)=ok, Verify(pre-sig)=fail, Ext recovers the witness) and that the
 * deterministic functions are actually deterministic (re-running gives identical
 * bytes).
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../las.h"
#include "../serialize.h"
#include "../params.h"
#include "../fips202.h"

#define NVEC  4
#define MLEN  33

#define CHECK(cond, msg) do { if(!(cond)) { \
  fprintf(stderr, "  [FAIL] %s\n", msg); return 1; } } while(0)

/* Pinned expected digest (filled in after the first run; see below). */
static const uint8_t EXPECTED[32] = {
  0xf7, 0xfc, 0x40, 0xf0, 0xb7, 0x75, 0x2c, 0xaf,
  0xc0, 0x83, 0xfc, 0xdd, 0xd6, 0xa1, 0x37, 0x59,
  0xfb, 0xde, 0x9b, 0x2a, 0x2d, 0x53, 0x80, 0x45,
  0xcd, 0x0d, 0x62, 0xf8, 0x77, 0x47, 0xe6, 0xb1
};

static int poly_eq(const poly *a, const poly *b) {
  unsigned int k;
  for(k = 0; k < N; ++k) if(a->coeffs[k] != b->coeffs[k]) return 0;
  return 1;
}
static int pk_eq(const las_pk *a, const las_pk *b) {
  unsigned int i;
  for(i = 0; i < LAS_N; ++i) if(!poly_eq(&a->t[i], &b->t[i])) return 0;
  return 1;
}
static int sk_eq(const las_sk *a, const las_sk *b) {
  unsigned int i;
  for(i = 0; i < LAS_M; ++i) if(!poly_eq(&a->s[i], &b->s[i])) return 0;
  return 1;
}
static int sig_eq(const las_sig *a, const las_sig *b) {
  unsigned int i;
  if(!poly_eq(&a->c, &b->c)) return 0;
  for(i = 0; i < LAS_M; ++i) if(!poly_eq(&a->z[i], &b->z[i])) return 0;
  return 1;
}

int main(void) {
  uint8_t ppseed[LAS_SEEDBYTES];
  las_pp pp;
  keccak_state acc;
  uint8_t digest[32];
  unsigned int v, i;

  for(i = 0; i < LAS_SEEDBYTES; ++i) ppseed[i] = (uint8_t)i;   /* fixed A */
  las_setup(&pp, ppseed);
  shake256_init(&acc);

  printf("=== LAS known-answer tests (mode %d, %d vectors) ===\n", DILITHIUM_MODE, NVEC);

  for(v = 0; v < NVEC; ++v) {
    uint8_t kseed[LAS_SEEDBYTES], yseed[LAS_SEEDBYTES], msg[MLEN];
    las_pk pk, Y, pk2;
    las_sk sk, yw, sk2, yext;
    las_sig sig, sig2, presig, adapted;
    uint8_t pk_b[LAS_PK_BYTES], sk_b[LAS_SK_BYTES];
    uint8_t sig_b[LAS_SIG_BYTES], pre_b[LAS_SIG_BYTES], adp_b[LAS_SIG_BYTES];

    for(i = 0; i < LAS_SEEDBYTES; ++i) {
      kseed[i] = (uint8_t)(7u * v + i + 1u);
      yseed[i] = (uint8_t)(7u * v + i + 100u);
    }
    for(i = 0; i < MLEN; ++i) msg[i] = (uint8_t)(37u * v + i);

    /* deterministic keygen / statement / sign / presign / adapt */
    las_keygen_seed(&pk, &sk, &pp, kseed);
    las_keygen_seed(&Y, &yw, &pp, yseed);
    las_sign_det(&sig, msg, MLEN, &pk, &sk, &pp);
    las_presign_det(&presig, msg, MLEN, &Y, &pk, &sk, &pp);
    CHECK(las_adapt(&adapted, &presig, msg, MLEN, &Y, &yw, &pk, &pp) == 0, "adapt");

    /* adaptor contract */
    CHECK(las_verify(&sig, msg, MLEN, &pk, &pp) == 0, "verify sig");
    CHECK(las_verify(&adapted, msg, MLEN, &pk, &pp) == 0, "verify adapted");
    CHECK(las_verify(&presig, msg, MLEN, &pk, &pp) != 0, "presig must not verify");
    CHECK(las_preverify(&presig, msg, MLEN, &Y, &pk, &pp) == 0, "preverify");
    CHECK(las_ext(&yext, &adapted, &presig, &Y, &pp) == 0 && sk_eq(&yext, &yw),
          "ext recovers witness");

    /* determinism: re-running the seeded/deterministic functions is identical */
    las_keygen_seed(&pk2, &sk2, &pp, kseed);
    CHECK(pk_eq(&pk, &pk2) && sk_eq(&sk, &sk2), "keygen_seed deterministic");
    las_sign_det(&sig2, msg, MLEN, &pk, &sk, &pp);
    CHECK(sig_eq(&sig, &sig2), "sign_det deterministic");

    /* serialise and fold into the running KAT digest */
    las_pack_pk(pk_b, &pk);
    CHECK(las_pack_sk(sk_b, &sk) == 0, "pack sk");
    CHECK(las_pack_sig(sig_b, &sig) == 0, "pack sig");
    CHECK(las_pack_sig(pre_b, &presig) == 0, "pack presig");
    CHECK(las_pack_sig(adp_b, &adapted) == 0, "pack adapted");
    shake256_absorb(&acc, pk_b, LAS_PK_BYTES);
    shake256_absorb(&acc, sk_b, LAS_SK_BYTES);
    shake256_absorb(&acc, sig_b, LAS_SIG_BYTES);
    shake256_absorb(&acc, pre_b, LAS_SIG_BYTES);
    shake256_absorb(&acc, adp_b, LAS_SIG_BYTES);
    printf("  vector %u: contract OK, deterministic, serialised\n", v);
  }

  shake256_finalize(&acc);
  shake256_squeeze(digest, 32, &acc);

  printf("  KAT digest: ");
  for(i = 0; i < 32; ++i) printf("%02x", digest[i]);
  printf("\n");

  if(memcmp(digest, EXPECTED, 32) != 0) {
    fprintf(stderr, "  [FAIL] KAT digest mismatch - reproducibility broken "
                    "(or update EXPECTED on a deliberate change)\n");
    return 1;
  }
  printf("=== KAT digest matches pinned expected value. ===\n");
  return 0;
}
