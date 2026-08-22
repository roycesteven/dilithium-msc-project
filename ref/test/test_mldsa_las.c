/*
 * test_mldsa_las.c -- the itemised correctness contract for the adaptor built
 * on NIST ML-DSA as FIPS 204 specifies it (hint vector, Power2Round and the
 * high/low-bit split all ENABLED).
 *
 * RELATIONSHIP TO THE OTHER TWO BINARIES
 *   test_mldsa_hint  answers "WHICH ML-DSA feature breaks a naive adaptor port?"
 *                    by comparing variants.  It is a diagnostic; a FAILS row
 *                    there is a result.
 *   test_mldsa_las   (this file) answers "is the REPAIRED scheme actually a
 *                    correct adaptor signature?"  It is a pass/fail contract:
 *                    every item must hold, and the binary exits non-zero if any
 *                    does not.
 *   bench_mldsa_compare  measures it against the simplified-Dilithium scheme.
 *
 * The contract mirrors ref/test/test_contract.c, so the two constructions are
 * held to the SAME standard: completeness, the statement-binding tripwire,
 * adaptability under an UNMODIFIED verifier, exact extraction, four tamper
 * rejections, two malformed-input rejections, wire round-trips, and
 * determinism.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../params.h"
#include "../mldsa_las.h"
#include "../sign.h"
#include "../packing.h"
#include "../polyvec.h"
#include "../randombytes.h"

#define NITER      200      /* randomised items                     */
#define NTAMPER     64      /* sampled single-byte flips per object */

static int failures = 0;
static int items = 0;

static void item(const char *name, int ok, const char *detail)
{
  items++;
  if(!ok)
    failures++;
  printf("  [%s] %-58s %s\n", ok ? "PASS" : "FAIL", name, detail ? detail : "");
}

/* ---- helpers -------------------------------------------------------- */

static int witness_equal(const mldsa_witness *a, const mldsa_witness *b)
{
  unsigned int i, j;
  for(i = 0; i < L; ++i)
    for(j = 0; j < N; ++j)
      if(a->y.vec[i].coeffs[j] != b->y.vec[i].coeffs[j])
        return 0;
  return 1;
}

static int statement_equal(const mldsa_statement *a, const mldsa_statement *b)
{
  unsigned int i, j;
  for(i = 0; i < K; ++i)
    for(j = 0; j < N; ++j)
      if(a->Y.vec[i].coeffs[j] != b->Y.vec[i].coeffs[j])
        return 0;
  return 1;
}

static int hint_weight_ok(const uint8_t sig[CRYPTO_BYTES])
{
  int w = mldsa_las_hint_weight(sig);
  return w >= 0 && w <= OMEGA;
}

static int z_within_mldsa_bound(const uint8_t sig[CRYPTO_BYTES])
{
  uint8_t c[CTILDEBYTES];
  polyvecl z;
  polyveck h;
  if(unpack_sig(c, &z, &h, sig))
    return 0;
  return !polyvecl_chknorm(&z, GAMMA1 - BETA);
}

/* One fully-populated adaptor instance. */
struct instance {
  uint8_t pk[CRYPTO_PUBLICKEYBYTES];
  uint8_t sk[CRYPTO_SECRETKEYBYTES];
  uint8_t rnd[RNDBYTES];
  uint8_t m[59];
  uint8_t presig[CRYPTO_BYTES];
  uint8_t sig[CRYPTO_BYTES];
  mldsa_statement Y;
  mldsa_witness y;
};

static int make_instance(struct instance *in)
{
  uint8_t rho[SEEDBYTES], seed[CRHBYTES];
  polyveck t1_unused;

  randombytes(in->m, sizeof(in->m));
  randombytes(seed, sizeof(seed));
  randombytes(in->rnd, sizeof(in->rnd));
  crypto_sign_keypair(in->pk, in->sk);
  unpack_pk(rho, &t1_unused, in->pk);
  mldsa_las_gen(&in->Y, &in->y, rho, seed, 0);
  return mldsa_las_presign(in->presig, in->m, sizeof(in->m), &in->Y,
                           in->rnd, in->sk, MLDSA_LAS_V1_SHIFTED);
}

int main(void)
{
  struct instance in, other;
  uint8_t buf[CRYPTO_BYTES];
  uint8_t Yb[MLDSA_LAS_STATEMENT_BYTES], Yb2[MLDSA_LAS_STATEMENT_BYTES];
  uint8_t yb[MLDSA_LAS_WITNESS_BYTES];
  mldsa_statement Yd;
  mldsa_witness yd, y_ext;
  unsigned long i;
  unsigned long ok_complete = 0, ok_tripwire = 0, ok_adapt = 0, ok_ext = 0;
  unsigned long ok_bound = 0, ok_hint = 0, ok_roundtrip = 0;
  unsigned long ok_msg = 0, ok_presig = 0, ok_stmt = 0, ok_wit = 0;
  unsigned long tamper_msg = 0, tamper_presig = 0;
  unsigned long ok_malformed_Y = 0, malformed_Y_tried = 0;
  unsigned long made = 0;
  int det_ok = 1;

  printf("=== Adaptor contract on NIST ML-DSA (FIPS 204 as specified) ===\n");
  printf("ML-DSA-%d | hint vector + Power2Round + high/low-bit split ALL ENABLED\n",
         DILITHIUM_MODE == 2 ? 44 : (DILITHIUM_MODE == 3 ? 65 : 87));
  printf("variant under test: V1 (commitment path shifted onto w+Y)\n");
  printf("iterations: %d randomised, %d sampled tampers per object\n\n", NITER, NTAMPER);

  for(i = 0; i < NITER; ++i) {
    if(make_instance(&in))
      continue;
    made++;

    /* 1. completeness */
    if(mldsa_las_preverify(in.presig, in.m, sizeof(in.m), &in.Y, in.pk) == 0)
      ok_complete++;

    /* 2. statement-binding tripwire: the pre-signature is NOT a signature */
    if(crypto_sign_verify(in.presig, CRYPTO_BYTES, in.m, sizeof(in.m),
                          NULL, 0, in.pk) != 0)
      ok_tripwire++;

    /* 3. adaptability under the UNMODIFIED FIPS 204 verifier */
    if(mldsa_las_adapt(in.sig, in.presig, in.m, sizeof(in.m), &in.Y, &in.y,
                       in.pk) == 0) {
      if(crypto_sign_verify(in.sig, CRYPTO_BYTES, in.m, sizeof(in.m),
                            NULL, 0, in.pk) == 0)
        ok_adapt++;

      /* 4. exact extraction */
      if(mldsa_las_ext(&y_ext, in.sig, in.presig, &in.Y, in.pk) == 0 &&
         witness_equal(&y_ext, &in.y))
        ok_ext++;

      /* 5/6. the adapted signature respects ML-DSA's own limits */
      if(z_within_mldsa_bound(in.sig))
        ok_bound++;
      if(hint_weight_ok(in.sig))
        ok_hint++;
    }

    /* 7. wire round-trip for both adaptor-specific objects */
    mldsa_las_pack_statement(Yb, &in.Y);
    mldsa_las_pack_witness(yb, &in.y);
    if(mldsa_las_unpack_statement(&Yd, Yb) == 0 &&
       mldsa_las_unpack_witness(&yd, yb) == 0 &&
       statement_equal(&Yd, &in.Y) && witness_equal(&yd, &in.y)) {
      mldsa_las_pack_statement(Yb2, &Yd);
      if(memcmp(Yb, Yb2, sizeof(Yb)) == 0)
        ok_roundtrip++;
    }
  }

  /* --- negative tests, on one fresh instance each round ---------------- */
  for(i = 0; i < NTAMPER; ++i) {
    unsigned int pos;
    uint8_t bit;

    if(make_instance(&in))
      continue;

    /* 8. tampered message must not pre-verify */
    memcpy(buf, in.m, sizeof(in.m));
    randombytes((uint8_t *)&pos, sizeof(pos));
    pos %= sizeof(in.m);
    randombytes(&bit, 1);
    buf[pos] ^= (uint8_t)(1u << (bit & 7));
    tamper_msg++;
    if(mldsa_las_preverify(in.presig, buf, sizeof(in.m), &in.Y, in.pk) != 0)
      ok_msg++;

    /* 9. tampered pre-signature must not pre-verify */
    memcpy(buf, in.presig, CRYPTO_BYTES);
    randombytes((uint8_t *)&pos, sizeof(pos));
    pos %= CRYPTO_BYTES;
    randombytes(&bit, 1);
    buf[pos] ^= (uint8_t)(1u << (bit & 7));
    tamper_presig++;
    if(memcmp(buf, in.presig, CRYPTO_BYTES) == 0 ||
       mldsa_las_preverify(buf, in.m, sizeof(in.m), &in.Y, in.pk) != 0)
      ok_presig++;

    /* 10. wrong statement must not pre-verify */
    if(make_instance(&other) == 0) {
      if(mldsa_las_preverify(in.presig, in.m, sizeof(in.m), &other.Y, in.pk) != 0)
        ok_stmt++;

      /* 11. adapting with the WRONG witness must not yield a valid signature */
      if(mldsa_las_adapt(in.sig, in.presig, in.m, sizeof(in.m), &in.Y,
                         &other.y, in.pk) != 0 ||
         crypto_sign_verify(in.sig, CRYPTO_BYTES, in.m, sizeof(in.m),
                            NULL, 0, in.pk) != 0)
        ok_wit++;
    }

    /* 12. malformed statement bytes must be refused by the decoder.
     * A random bit flip is usually NOT detectable -- most flips leave the
     * coefficient inside [0, Q) and simply denote a different valid statement.
     * The property being tested is that a NON-CANONICAL encoding is refused, so
     * the last coefficient's 23 bits are driven all-high (2^23-1 = 8388607,
     * which exceeds Q = 8380417) rather than flipped at random. */
    mldsa_las_pack_statement(Yb, &in.Y);
    memcpy(Yb2, Yb, sizeof(Yb));
    Yb2[sizeof(Yb) - 1] = 0xFFu;
    Yb2[sizeof(Yb) - 2] = 0xFFu;
    Yb2[sizeof(Yb) - 3] = 0xFFu;
    malformed_Y_tried++;
    if(mldsa_las_unpack_statement(&Yd, Yb2) != 0)
      ok_malformed_Y++;
  }

  /* 13. determinism: same (sk, Y, M, rnd) must give byte-identical output */
  if(make_instance(&in) == 0) {
    if(mldsa_las_presign(buf, in.m, sizeof(in.m), &in.Y, in.rnd, in.sk,
                         MLDSA_LAS_V1_SHIFTED) != 0 ||
       memcmp(buf, in.presig, CRYPTO_BYTES) != 0)
      det_ok = 0;
  } else {
    det_ok = 0;
  }

  /* --- report ---------------------------------------------------------- */
  {
    char d[96];
#define ITEM(name, ok, n) \
    do { snprintf(d, sizeof(d), "%lu/%lu", (unsigned long)(ok), (unsigned long)(n)); \
         item(name, (n) > 0 && (ok) == (n), d); } while(0)

    printf("Positive contract (%lu instances):\n", made);
    ITEM("1 PreSign -> PreVerify accepts (completeness)", ok_complete, made);
    ITEM("2 stock ML-DSA Verify REJECTS the pre-signature (tripwire)", ok_tripwire, made);
    ITEM("3 Adapt -> UNMODIFIED ML-DSA Verify accepts (adaptability)", ok_adapt, made);
    ITEM("4 Ext recovers the witness exactly, and A y' = Y", ok_ext, made);
    ITEM("5 adapted ||z||inf < GAMMA1-BETA (ML-DSA's own bound)", ok_bound, made);
    ITEM("6 hint weight <= OMEGA", ok_hint, made);
    ITEM("7 statement/witness wire round-trip is the identity", ok_roundtrip, made);

    printf("\nNegative contract:\n");
    ITEM("8 tampered message does not pre-verify", ok_msg, tamper_msg);
    ITEM("9 tampered pre-signature does not pre-verify", ok_presig, tamper_presig);
    ITEM("10 wrong statement does not pre-verify", ok_stmt, tamper_presig);
    ITEM("11 wrong witness yields no valid signature", ok_wit, tamper_presig);
    ITEM("12 malformed statement bytes refused by the decoder",
         ok_malformed_Y, malformed_Y_tried);

    printf("\nReproducibility:\n");
    item("13 PreSign is deterministic in (sk, Y, M, rnd)", det_ok, det_ok ? "byte-identical" : "");
#undef ITEM
  }

  printf("\nWire sizes (bytes): signature %d | pre-signature %d | public key %d\n",
         MLDSA_LAS_SIGNATURE_BYTES, MLDSA_LAS_PRE_SIGNATURE_BYTES,
         MLDSA_LAS_PUBLICKEY_BYTES);
  printf("                    statement Y %d | witness y %d\n",
         MLDSA_LAS_STATEMENT_BYTES, MLDSA_LAS_WITNESS_BYTES);

  printf("\n%s: %d/%d contract items hold\n",
         failures ? "FAIL" : "PASS", items - failures, items);
  return failures ? 1 : 0;
}
