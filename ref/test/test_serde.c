/*
 * Serialisation tests for LAS (ref/serialize.{c,h}).
 *
 * A robust, deployment-ready implementation must exchange objects as BYTES and
 * decode them defensively (an on-chain verifier cannot trust its input).  This
 * test hard-asserts, over many random instances:
 *
 *   - round-trip:   unpack(pack(x)) == x   for pk, sk, and (pre/adapted-)sig;
 *   - verify-from-bytes: a packed (pk, adapted sig) verifies via the on-chain-style
 *                   entry point las_verify_packed, while a packed PRE-signature does
 *                   NOT (the statement-binding tripwire survives serialisation);
 *   - tamper:       flipping any byte of a packed signature makes it fail to verify;
 *   - validation:   pack rejects out-of-range inputs and unpack rejects malformed
 *                   bytes (coeff >= Q, non-ternary code, z outside the 18-bit band).
 *
 * It also prints the MEASURED packed sizes (these realise the "theoretical packed"
 * figures quoted in docs/LAS.md Section 8).
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../randombytes.h"
#include "../las.h"
#include "../serialize.h"
#include "../params.h"

#define MLEN   59
#define NITER  256

#define CHECK(cond, msg) do { if(!(cond)) { \
  fprintf(stderr, "  [FAIL] %s\n", msg); return 1; } } while(0)

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
  uint8_t m[MLEN];
  uint8_t pk_b[LAS_PK_BYTES], sk_b[LAS_SK_BYTES];
  uint8_t sig_b[LAS_SIG_BYTES], pre_b[LAS_SIG_BYTES], adp_b[LAS_SIG_BYTES];
  las_pk pk, Y, pk2;
  las_sk sk, y, sk2;
  las_sig sig, presig, adapted, sig2;
  int i;

  randombytes(ppseed, LAS_SEEDBYTES);
  las_setup(&pp, ppseed);

  printf("=== LAS serialisation tests (mode %d) ===\n", DILITHIUM_MODE);
  printf("packed sizes: pk/Y=%d B  sk/witness=%d B  sig/pre-sig=%d B\n",
         LAS_PK_BYTES, LAS_SK_BYTES, LAS_SIG_BYTES);

  for(i = 0; i < NITER; ++i) {
    randombytes(m, MLEN);
    las_keygen(&pk, &sk, &pp);
    las_keygen(&Y, &y, &pp);
    las_sign(&sig, m, MLEN, &pk, &sk, &pp);
    las_presign(&presig, m, MLEN, &Y, &pk, &sk, &pp);
    CHECK(las_adapt(&adapted, &presig, m, MLEN, &Y, &y, &pk, &pp) == 0, "adapt");

    /* round-trip: pk */
    las_pack_pk(pk_b, &pk);
    CHECK(las_unpack_pk(&pk2, pk_b) == 0, "unpack pk");
    CHECK(pk_eq(&pk, &pk2), "pk round-trip");

    /* round-trip: sk (ternary) */
    CHECK(las_pack_sk(sk_b, &sk) == 0, "pack sk");
    CHECK(las_unpack_sk(&sk2, sk_b) == 0, "unpack sk");
    CHECK(sk_eq(&sk, &sk2), "sk round-trip");

    /* round-trip: signature, pre-signature, adapted signature */
    CHECK(las_pack_sig(sig_b, &sig) == 0, "pack sig");
    CHECK(las_unpack_sig(&sig2, sig_b) == 0, "unpack sig");
    CHECK(sig_eq(&sig, &sig2), "sig round-trip");

    CHECK(las_pack_sig(pre_b, &presig) == 0, "pack presig");
    CHECK(las_unpack_sig(&sig2, pre_b) == 0, "unpack presig");
    CHECK(sig_eq(&presig, &sig2), "presig round-trip");

    CHECK(las_pack_sig(adp_b, &adapted) == 0, "pack adapted");
    CHECK(las_unpack_sig(&sig2, adp_b) == 0, "unpack adapted");
    CHECK(sig_eq(&adapted, &sig2), "adapted round-trip");

    /* verify-from-bytes: adapted sig verifies, pre-sig does NOT (tripwire) */
    CHECK(las_verify_packed(pk_b, adp_b, m, MLEN, &pp) == 0, "verify_packed(adapted)");
    CHECK(las_verify_packed(pk_b, pre_b, m, MLEN, &pp) != 0, "verify_packed(presig) must fail");
    /* the plain ordinary signature also verifies through bytes */
    CHECK(las_verify_packed(pk_b, sig_b, m, MLEN, &pp) == 0, "verify_packed(sig)");
  }
  printf("round-trip + verify-from-bytes: %d iterations OK\n", NITER);

  /* ---- tamper: every single-byte flip of a valid adapted sig fails verify ---- */
  {
    int b, flips = 0;
    for(b = 0; b < LAS_SIG_BYTES; ++b) {
      uint8_t saved = adp_b[b];
      adp_b[b] ^= 0x01;
      if(las_verify_packed(pk_b, adp_b, m, MLEN, &pp) != 0) ++flips;
      adp_b[b] = saved;
    }
    CHECK(flips == LAS_SIG_BYTES, "every byte-flip must break verification");
    printf("tamper: all %d single-byte flips rejected\n", LAS_SIG_BYTES);
  }

  /* ---- validation: unpack rejects malformed bytes ---- */
  {
    uint8_t buf[LAS_SIG_BYTES];
    las_pk tpk; las_sk tsk; las_sig tsig;

    /* pk coeff = 0x7FFFFF >= Q  -> reject */
    memset(buf, 0xFF, LAS_PK_BYTES);
    CHECK(las_unpack_pk(&tpk, buf) == -1, "unpack_pk must reject coeff>=Q");

    /* sk 2-bit code 3 -> reject */
    memset(buf, 0xFF, LAS_SK_BYTES);
    CHECK(las_unpack_sk(&tsk, buf) == -1, "unpack_sk must reject code 3");

    /* sig: valid c, but first z field forced to 0x3FFFF (> LAS_Z_MAX). The z
     * region starts at bit N*LAS_C_COEFF_BITS = 512 = byte 64. */
    CHECK(las_pack_sig(buf, &adapted) == 0, "re-pack adapted for z-validation");
    buf[64] = 0xFF; buf[65] = 0xFF; buf[66] |= 0x03;   /* set the 18 low bits */
    CHECK(las_unpack_sig(&tsig, buf) == -1, "unpack_sig must reject z out of band");
    printf("validation: unpack rejects coeff>=Q, code-3 sk, and z out of band\n");
  }

  /* ---- validation: pack rejects out-of-range inputs ---- */
  {
    las_sk bad_sk = sk;
    las_sig bad_sig = adapted;
    bad_sk.s[0].coeffs[0] = 2;                         /* non-ternary */
    CHECK(las_pack_sk(sk_b, &bad_sk) == -1, "pack_sk must reject non-ternary");
    bad_sig.z[0].coeffs[0] = LAS_GAMMA;                /* > g-k, out of band */
    CHECK(las_pack_sig(sig_b, &bad_sig) == -1, "pack_sig must reject z out of band");
    printf("validation: pack rejects non-ternary sk and out-of-band z\n");
  }

  printf("=== All serialisation tests passed. ===\n");
  printf("These packed sizes are the realistic on-wire / on-chain object sizes for\n");
  printf("the simplified scheme; las_verify_packed is the byte-level verifier an\n");
  printf("on-chain integration (poqeth-style) would call.\n");
  return 0;
}
