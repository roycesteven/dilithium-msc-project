/*
 * test_contract.c  --  consolidated CORRECTNESS-CONTRACT evaluation harness.
 *
 * One harness that prints, as labelled PASS/FAIL lines, every property an adaptor
 * signature must satisfy.  It exists so the Evaluation section / video can point at a
 * single, itemised proof that the LAS implementation is correct and robust, rather
 * than at summary counts spread across test_las / test_serde / test_kat.
 *
 * The eight checks:
 *   [1] PreSign  -> PreVerify accepts            (pre-signature is well-formed)
 *   [2] PreSign  -> ordinary Verify REJECTS      (statement-binding tripwire)
 *   [3] Adapt    -> ordinary Verify accepts      (adapted sig is an ordinary sig)
 *   [4] Ext recovers the witness EXACTLY         (y' == y)
 *   [5a] Tampered message     -> Verify rejects
 *   [5b] Tampered signature   -> rejected        (every single byte-flip)
 *   [6] Malformed packed bytes -> decoder rejects (pk coeff>=Q, sk code-3, sig c/z)
 *   [7] Deterministic API     -> byte-identical on re-run (keygen_seed/sign_det/presign_det)
 *
 * Exit status is non-zero if any check fails.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../randombytes.h"
#include "../las.h"
#include "../serialize.h"
#include "../params.h"

static int g_fail = 0;

static void report(const char *label, int ok) {
  printf("  %-46s %s\n", label, ok ? "PASS" : "FAIL");
  if(!ok) g_fail = 1;
}

/* exact equality of two secret/witness vectors (coefficient-for-coefficient) */
static int sk_equal(const las_sk *a, const las_sk *b) {
  unsigned int j, k;
  for(j = 0; j < LAS_M; ++j)
    for(k = 0; k < N; ++k)
      if(a->s[j].coeffs[k] != b->s[j].coeffs[k])
        return 0;
  return 1;
}

int main(void) {
  uint8_t ppseed[LAS_SEEDBYTES], kseed[LAS_SEEDBYTES], yseed[LAS_SEEDBYTES];
  uint8_t m[59], m2[59];
  size_t  mlen = sizeof m;
  las_pp  pp;
  las_pk  pk, Y;
  las_sk  sk, y, yext;
  las_sig presig, sig;

  randombytes(ppseed, LAS_SEEDBYTES);
  randombytes(kseed, LAS_SEEDBYTES);
  randombytes(yseed, LAS_SEEDBYTES);
  randombytes(m, mlen);

  las_setup(&pp, ppseed);
  las_keypair_seed(&pk, &sk, &pp, kseed);     /* deterministic key   */
  las_keypair_seed(&Y, &y, &pp, yseed);       /* statement/witness   */

  printf("=== LAS adaptor-signature correctness contract (mode %d, n=%d ell=%d kappa=%d) ===\n",
         DILITHIUM_MODE, LAS_N, LAS_ELL, LAS_KAPPA);

  /* [1] PreSign -> PreVerify accepts */
  las_presign(&presig, m, mlen, &Y, &pk, &sk, &pp);
  report("[1] PreSign produces a pre-sig that PreVerify accepts",
         las_preverify(&presig, m, mlen, &Y, &pk, &pp) == 0);

  /* [2] PreSign -> ordinary Verify rejects (the statement-binding tripwire) */
  report("[2] the pre-signature FAILS ordinary Verify (tripwire)",
         las_verify(&presig, m, mlen, &pk, &pp) != 0);

  /* [3] Adapt -> ordinary Verify accepts */
  report("[3] Adapt yields a signature ordinary Verify accepts",
         las_adapt(&sig, &presig, m, mlen, &Y, &y, &pk, &pp) == 0 &&
         las_verify(&sig, m, mlen, &pk, &pp) == 0);

  /* [4] Ext recovers the witness exactly */
  report("[4] Ext recovers the witness exactly (y' == y)",
         las_ext(&yext, &sig, &presig, &Y, &pp) == 0 && sk_equal(&yext, &y));

  /* [5a] tampered message rejected */
  memcpy(m2, m, mlen);
  m2[0] ^= 0x01;
  report("[5a] a tampered message fails Verify",
         las_verify(&sig, m2, mlen, &pk, &pp) != 0);

  /* [5b] tampered signature rejected -- every single byte-flip of the packed sig */
  {
    uint8_t pkb[LAS_PK_BYTES], sgb[LAS_SIG_BYTES];
    size_t i; int bit; unsigned long rejected = 0;
    las_pack_pk(pkb, &pk);
    las_pack_sig(sgb, &sig);
    /* sanity: the clean packed pair verifies */
    int clean_ok = (las_verify_packed(pkb, sgb, m, mlen, &pp) == 0);
    for(i = 0; i < LAS_SIG_BYTES; ++i) {
      for(bit = 0; bit < 8; ++bit) {
        sgb[i] ^= (uint8_t)(1u << bit);
        if(las_verify_packed(pkb, sgb, m, mlen, &pp) != 0) ++rejected;
        sgb[i] ^= (uint8_t)(1u << bit);
      }
    }
    report("[5b] every single bit-flip of the packed signature is rejected",
           clean_ok && rejected == (unsigned long)LAS_SIG_BYTES * 8);
  }

  /* [6] malformed packed bytes rejected by the validating decoder */
  {
    uint8_t b_pk[LAS_PK_BYTES], b_sk[LAS_SK_BYTES], b_sig[LAS_SIG_BYTES];
    las_pk  u_pk; las_sk u_sk; las_sig u_sig;
    int all_reject = 1;

    las_pack_pk(b_pk, &pk);
    memset(b_pk, 0xFF, sizeof b_pk);                 /* every coeff field >= Q */
    if(las_unpack_pk(&u_pk, b_pk) == 0) all_reject = 0;

    las_pack_sk(b_sk, &sk);
    memset(b_sk, 0xFF, sizeof b_sk);                 /* 2-bit code 3 (invalid) */
    if(las_unpack_sk(&u_sk, b_sk) == 0) all_reject = 0;

    las_pack_sig(b_sig, &sig);
    b_sig[0] = 0xFF;                                 /* challenge ternary code 3 */
    if(las_unpack_sig(&u_sig, b_sig) == 0) all_reject = 0;

    las_pack_sig(b_sig, &sig);                       /* fresh, then corrupt only z */
    memset(b_sig + (N * LAS_C_COEFF_BITS) / 8, 0xFF,
           sizeof b_sig - (N * LAS_C_COEFF_BITS) / 8); /* z fields out of band */
    if(las_unpack_sig(&u_sig, b_sig) == 0) all_reject = 0;

    report("[6] malformed packed bytes are rejected (pk>=Q, sk code-3, sig c & z)",
           all_reject);
  }

  /* [7] deterministic API: re-running with identical inputs gives identical bytes */
  {
    las_pk  pk_a, pk_b; las_sk sk_a, sk_b;
    las_sig sg_a, sg_b, ps_a, ps_b;
    uint8_t A[LAS_SIG_BYTES], B[LAS_SIG_BYTES];
    uint8_t Ak[LAS_SK_BYTES], Bk[LAS_SK_BYTES];
    int stable = 1;

    las_keypair_seed(&pk_a, &sk_a, &pp, kseed);
    las_keypair_seed(&pk_b, &sk_b, &pp, kseed);
    las_pack_sk(Ak, &sk_a); las_pack_sk(Bk, &sk_b);
    if(memcmp(Ak, Bk, LAS_SK_BYTES) != 0) stable = 0;

    las_signature_det(&sg_a, m, mlen, &pk_a, &sk_a, &pp);
    las_signature_det(&sg_b, m, mlen, &pk_a, &sk_a, &pp);
    las_pack_sig(A, &sg_a); las_pack_sig(B, &sg_b);
    if(memcmp(A, B, LAS_SIG_BYTES) != 0) stable = 0;

    las_presign_det(&ps_a, m, mlen, &Y, &pk_a, &sk_a, &pp);
    las_presign_det(&ps_b, m, mlen, &Y, &pk_a, &sk_a, &pp);
    las_pack_sig(A, &ps_a); las_pack_sig(B, &ps_b);
    if(memcmp(A, B, LAS_SIG_BYTES) != 0) stable = 0;

    report("[7] deterministic API is byte-stable on re-run",
           stable);
  }

  printf("=== %s ===\n", g_fail ? "CONTRACT FAILED" : "ALL CONTRACT CHECKS PASSED");
  return g_fail;
}
