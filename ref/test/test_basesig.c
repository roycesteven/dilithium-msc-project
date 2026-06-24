/*
 * test_basesig.c  --  randomised correctness test for the SEPARATE base signature
 *                     module (basesig.c) and its interlock with the LAS adaptor (las.c).
 *
 * Uses a CHECK() macro (not assert(), so it is never compiled out under -DNDEBUG):
 * on the first violated property it reports the location/iteration and returns a
 * non-zero exit code. A clean run is evidence -- over NTESTS independent random
 * iterations -- that each listed property held; it is a test, not a formal proof.
 * Each iteration draws fresh public parameters, key pairs, a statement/witness, and
 * a message.
 *
 * Properties checked every iteration:
 *
 *   BASE signature (basesig.c), in isolation:
 *     [B1] base_sign -> base_verify ACCEPTS                 (honest signature verifies)
 *     [B2] tampered message      -> base_verify REJECTS
 *     [B3] tampered response z   -> base_verify REJECTS
 *     [B4] wrong public key      -> base_verify REJECTS
 *
 *   CROSS-MODULE equivalence (basesig.c <-> las.c):
 *     [X1] a base_sign signature verifies under LAS's las_verify
 *     [X2] a las_sign  signature verifies under base_verify
 *          (mutual verifiability is evidence that the two modules implement the
 *           SAME construction -- the same challenge hash, A-product, and norm bound)
 *
 *   CROSS-PATH interlock (base verifier vs LAS adaptor path):
 *     [I1] las_presign -> las_preverify ACCEPTS             (pre-signature well-formed)
 *     [I2] las_presign -> base_verify REJECTS               (statement-binding tripwire:
 *                                                            base hash omits +Y)
 *     [I3] las_adapt   -> base_verify ACCEPTS               (adapted sig is an ordinary
 *                                                            base signature, no explicit +Y)
 *     [I4] las_ext recovers the witness EXACTLY             (y' == y and A*y' == Y)
 *
 *   NEGATIVE tests (wrong inputs / tampering must be rejected):
 *     [N1] PreVerify under the WRONG statement Y' REJECTS
 *     [N2] Adapt with the WRONG witness -> base_verify REJECTS and Ext FAILS
 *     [N3] tampered pre-signature -> PreVerify REJECTS
 *     [N4] tampered adapted signature -> base_verify REJECTS
 *
 * Build (Makefile sets -DLAS_N/-DLAS_ELL/-DLAS_KAPPA per parameter set):
 *     make test/test_basesig_paper   # (4,4,60)
 *     make test/test_basesig2        # (4,4,39)
 *     make test/test_basesig3        # (6,5,49)
 *     make test/test_basesig5        # (8,7,60)
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "../randombytes.h"
#include "../basesig.h"
#include "../las.h"
#include "../params.h"   /* N, Q for the summary print */

#define MLEN   59
#define NTESTS 1000

/* CHECK: like assert() but never compiled out. On the first failure it reports the
 * message, the iteration, and the source location, then returns non-zero from main. */
#define CHECK(cond, msg) do {                                              \
    if(!(cond)) {                                                          \
      fprintf(stderr, "  [FAIL iter %u] %s  (%s:%d)\n",                    \
              iter, msg, __FILE__, __LINE__);                             \
      return 1;                                                            \
    }                                                                      \
  } while(0)

/* exact equality of two secret/witness vectors (coefficient-for-coefficient) */
static int witness_equal(const las_sk *a, const las_sk *b) {
  unsigned int j, k;
  for(j = 0; j < LAS_M; ++j)
    for(k = 0; k < N; ++k)
      if(a->s[j].coeffs[k] != b->s[j].coeffs[k])
        return 0;
  return 1;
}

int main(void) {
  uint8_t ppseed[LAS_SEEDBYTES];
  uint8_t m[MLEN];
  las_pp  pp;
  las_pk  pk, Y, Y2;
  las_sk  sk, y, y2, yext;
  las_sig sig_b, sig_l, presig, adapted, bad, tmp;
  unsigned int iter, j;

  for(iter = 0; iter < NTESTS; ++iter) {
    randombytes(ppseed, LAS_SEEDBYTES);
    las_setup(&pp, ppseed);                              /* pp = A                 */
    randombytes(m, MLEN);

    base_keygen(&pk, &sk, &pp);                          /* base key pair          */
    base_keygen(&Y,  &y,  &pp);                          /* statement/witness Y=Ay */
    base_keygen(&Y2, &y2, &pp);                          /* a DIFFERENT statement   */

    /* ---- BASE signature in isolation ---- */
    base_sign(&sig_b, m, MLEN, &pk, &sk, &pp);
    CHECK(base_verify(&sig_b, m, MLEN, &pk, &pp) == 0, "[B1] honest base sig must verify");

    /* [B2] tampered message must be rejected */
    {
      uint8_t mbad[MLEN];
      for(j = 0; j < MLEN; ++j) mbad[j] = m[j];
      mbad[0] ^= 0xFF;
      CHECK(base_verify(&sig_b, mbad, MLEN, &pk, &pp) != 0, "[B2] tampered message must fail");
    }

    /* [B3] tampered response z must be rejected (changes w' => hash mismatch,
     *      or trips the norm bound -- either way base_verify returns nonzero) */
    tmp = sig_b;
    tmp.z[0].coeffs[0] += 1;
    CHECK(base_verify(&tmp, m, MLEN, &pk, &pp) != 0, "[B3] tampered z must fail");

    /* [B4] verifying an honest signature under the WRONG public key fails
     *      (Y is an independent key pair, so Y.t != pk.t) */
    CHECK(base_verify(&sig_b, m, MLEN, &Y, &pp) != 0, "[B4] wrong public key must fail");

    /* ---- CROSS-MODULE equivalence basesig.c <-> las.c ---- */
    CHECK(las_verify(&sig_b, m, MLEN, &pk, &pp) == 0, "[X1] base sig must verify under las_verify");
    las_sign(&sig_l, m, MLEN, &pk, &sk, &pp);
    CHECK(las_verify(&sig_l, m, MLEN, &pk, &pp) == 0, "las sig must verify under las_verify");
    CHECK(base_verify(&sig_l, m, MLEN, &pk, &pp) == 0, "[X2] las sig must verify under base_verify");

    /* ---- CROSS-PATH interlock: independent base verifier vs LAS adaptor ---- */
    las_presign(&presig, m, MLEN, &Y, &pk, &sk, &pp);
    CHECK(las_preverify(&presig, m, MLEN, &Y, &pk, &pp) == 0, "[I1] honest pre-sig must pre-verify");
    CHECK(base_verify(&presig, m, MLEN, &pk, &pp) != 0, "[I2] pre-sig must NOT pass base Verify (tripwire)");

    CHECK(las_adapt(&adapted, &presig, m, MLEN, &Y, &y, &pk, &pp) == 0, "adapt must succeed");
    CHECK(base_verify(&adapted, m, MLEN, &pk, &pp) == 0, "[I3] adapted sig must verify under base_verify");

    CHECK(las_ext(&yext, &adapted, &presig, &Y, &pp) == 0, "[I4] Ext must succeed (A*y'==Y)");
    CHECK(witness_equal(&yext, &y), "[I4] Ext must recover the witness exactly (y'==y)");

    /* ---- NEGATIVE tests ---- */

    /* [N1] PreVerify under the WRONG statement Y2 must reject (the pre-sig binds Y). */
    CHECK(las_preverify(&presig, m, MLEN, &Y2, &pk, &pp) != 0, "[N1] PreVerify under wrong Y must fail");

    /* [N2] Adapt with the WRONG witness y2 (which satisfies Y2=A*y2, not Y=A*y):
     *      the internal PreVerify on the correct Y still passes, so Adapt builds a
     *      signature, but it encodes A*y2 = Y2 != Y, so the independent base verifier
     *      rejects it and Ext cannot recover a witness for Y. */
    CHECK(las_adapt(&bad, &presig, m, MLEN, &Y, &y2, &pk, &pp) == 0, "adapt(wrong-witness) builds");
    CHECK(base_verify(&bad, m, MLEN, &pk, &pp) != 0, "[N2] wrong-witness adapted must NOT verify");
    CHECK(las_ext(&yext, &bad, &presig, &Y, &pp) != 0, "[N2] Ext must fail on wrong-witness adapt");

    /* [N3] a tampered pre-signature must fail PreVerify. */
    tmp = presig;
    tmp.z[0].coeffs[0] += 1;
    CHECK(las_preverify(&tmp, m, MLEN, &Y, &pk, &pp) != 0, "[N3] tampered pre-signature must fail PreVerify");

    /* [N4] a tampered adapted signature must fail ordinary (base) Verify. */
    tmp = adapted;
    tmp.z[0].coeffs[0] += 1;
    CHECK(base_verify(&tmp, m, MLEN, &pk, &pp) != 0, "[N4] tampered adapted signature must fail Verify");
  }

  printf("test_basesig: ALL CHECKS PASSED\n");
  printf("  parameter set n=%d ell=%d kappa=%d gamma=%d  (N=%d, Q=%d)\n",
         LAS_N, LAS_ELL, LAS_KAPPA, LAS_GAMMA, N, Q);
  printf("  %d iterations x (4 base + 2 cross-module + 4 interlock + 4 negative) checks; CHECK-gated\n",
         NTESTS);
  return 0;
}
