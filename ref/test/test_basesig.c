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
 *     [B1] base_sign -> base_verify ACCEPTS                       (honest signature verifies)
 *     [B2] tampered message      -> base_verify REJECTS
 *     [B3] tampered response z   -> base_verify REJECTS
 *     [B4] wrong public key      -> base_verify REJECTS
 *
 *   CROSS-PATH interlock (base verifier vs LAS adaptor path):
 *     [I1] las_presign -> las_preverify ACCEPTS                   (pre-signature well-formed)
 *     [I2] a pre-signature's bytes decoded AS a signature -> base_verify REJECTS
 *          (statement-binding tripwire: the base hash omits +Y; a pre-signature is a
 *           DISTINCT type, so the relabel is done at the byte level)
 *     [I3] las_adapt   -> base_verify ACCEPTS                     (adapted sig is an ordinary
 *                                                                  base signature, no explicit +Y)
 *     [I4] las_ext recovers the witness EXACTLY                   (s == r' and A*s == Y)
 *
 *   NEGATIVE tests (wrong inputs / tampering must be rejected):
 *     [N1] PreVerify under the WRONG statement Y' REJECTS
 *     [N2] Adapt with the WRONG witness -> base_verify REJECTS and Ext FAILS
 *     [N3] tampered pre-signature -> PreVerify REJECTS
 *     [N4] tampered adapted signature -> base_verify REJECTS
 *
 * (Algorithm 1 is now implemented ONCE, in basesig.c; the earlier cross-module
 * [X1]/[X2] "basesig.c vs las.c" equivalence checks are vacuous and removed.)
 *
 * Build (Makefile sets -DLAS_N/-DELL/-DKAPPA per parameter set):
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
#include "../relation.h"
#include "../las.h"
#include "../serialize.h"  /* byte-level [I2] tripwire */
#include "../params.h"     /* Q for the summary print */

#define MLEN   59
#define NTESTS 1000

/* Wire-size assumption the byte-level tripwire relies on (holds for every set). */
typedef char basesig_size_relation[(PRE_SIGNATURE_BYTES == SIGNATURE_BYTES) ? 1 : -1];

/* CHECK: like assert() but never compiled out. On the first failure it reports the
 * message, the iteration, and the source location, then returns non-zero from main. */
#define CHECK(cond, msg) do {                                              \
    if(!(cond)) {                                                          \
      fprintf(stderr, "  [FAIL iter %u] %s  (%s:%d)\n",                    \
              iter, msg, __FILE__, __LINE__);                             \
      return 1;                                                            \
    }                                                                      \
  } while(0)

/* exact equality of two witness vectors (coefficient-for-coefficient) */
static int witness_equal(const witness *a, const witness *b) {
  unsigned int j, k;
  for(j = 0; j < N_PLUS_ELL; ++j)
    for(k = 0; k < LAS_D; ++k)
      if(a->value[j].coeffs[k] != b->value[j].coeffs[k])
        return 0;
  return 1;
}

int main(void) {
  uint8_t ppseed[LAS_SEEDBYTES];
  uint8_t m[MLEN];
  public_params pp;
  public_key    pk;
  statement     Y, Y2;
  secret_key    sk;
  witness       r_prime, r_prime2, s_ext;   /* honest witnesses (Gen); extracted witness (Ext) */
  signature     sig, adapted, bad, tmp;
  pre_signature presig, tmp_pre;
  unsigned int iter, j;

  for(iter = 0; iter < NTESTS; ++iter) {
    randombytes(ppseed, LAS_SEEDBYTES);
    setup_public_params(&pp, ppseed);                    /* pp = (A, H); expand A  */
    randombytes(m, MLEN);

    CHECK(base_keygen(&pk, &sk, &pp) == 0, "base_keygen");                 /* base key pair          */
    CHECK(relation_gen(&Y,  &r_prime,  &pp) == 0, "relation_gen (Y, r')"); /* statement/witness Y=Ar' */
    CHECK(relation_gen(&Y2, &r_prime2, &pp) == 0, "relation_gen (Y2, r'2)"); /* a DIFFERENT statement  */

    /* ---- BASE signature in isolation ---- */
    CHECK(base_sign(&sig, m, MLEN, &pk, &sk, &pp) == 0, "base_sign");
    CHECK(base_verify(&sig, m, MLEN, &pk, &pp) == 0, "[B1] honest base sig must verify");

    /* [B2] tampered message must be rejected */
    {
      uint8_t mbad[MLEN];
      for(j = 0; j < MLEN; ++j) mbad[j] = m[j];
      mbad[0] ^= 0xFF;
      CHECK(base_verify(&sig, mbad, MLEN, &pk, &pp) != 0, "[B2] tampered message must fail");
    }

    /* [B3] tampered response z must be rejected (changes w' => hash mismatch,
     *      or trips the norm bound -- either way base_verify returns nonzero) */
    tmp = sig;
    tmp.z[0].coeffs[0] += 1;
    CHECK(base_verify(&tmp, m, MLEN, &pk, &pp) != 0, "[B3] tampered z must fail");

    /* [B4] verifying an honest signature under the WRONG public key fails.
     *      A statement is pk-shaped; reuse Y's t' as a bogus public key. */
    {
      public_key wrong_pk;
      for(j = 0; j < LAS_N; ++j) wrong_pk.t[j] = Y.t_prime[j];
      CHECK(base_verify(&sig, m, MLEN, &wrong_pk, &pp) != 0, "[B4] wrong public key must fail");
    }

    /* ---- CROSS-PATH interlock: independent base verifier vs LAS adaptor ---- */
    CHECK(las_presign(&presig, m, MLEN, &Y, &pk, &sk, &pp) == 0, "las_presign");
    CHECK(las_preverify(&presig, m, MLEN, &Y, &pk, &pp) == 0, "[I1] honest pre-sig must pre-verify");
    /* [I2] byte-level tripwire: relabel the pre-signature's bytes as a signature */
    {
      signature presig_as_sig;
      uint8_t relabel_b[PRE_SIGNATURE_BYTES];
      CHECK(pack_pre_signature(relabel_b, &presig) == 0, "[I2] pack pre-sig");
      CHECK(unpack_signature(&presig_as_sig, relabel_b) == 0, "[I2] pre-sig bytes decode as sig");
      CHECK(base_verify(&presig_as_sig, m, MLEN, &pk, &pp) != 0,
            "[I2] pre-sig must NOT pass base Verify (tripwire)");
    }

    CHECK(las_adapt(&adapted, &presig, m, MLEN, &Y, &r_prime, &pk, &pp) == 0, "adapt must succeed");
    CHECK(base_verify(&adapted, m, MLEN, &pk, &pp) == 0, "[I3] adapted sig must verify under base_verify");

    CHECK(las_ext(&s_ext, &adapted, &presig, &Y, &pp) == 0, "[I4] Ext must succeed (A*s==Y)");
    CHECK(witness_equal(&s_ext, &r_prime), "[I4] Ext must recover the witness exactly (s==r')");

    /* ---- NEGATIVE tests ---- */

    /* [N1] PreVerify under the WRONG statement Y2 must reject (the pre-sig binds Y). */
    CHECK(las_preverify(&presig, m, MLEN, &Y2, &pk, &pp) != 0, "[N1] PreVerify under wrong Y must fail");

    /* [N2] Adapt with the WRONG witness r_prime2 (which satisfies Y2=A*r'2, not
     *      Y=A*r'): the internal PreVerify on the correct Y still passes, so Adapt
     *      builds a signature, but it encodes A*r'2 = Y2 != Y, so the independent
     *      base verifier rejects it and Ext cannot recover a witness for Y. */
    CHECK(las_adapt(&bad, &presig, m, MLEN, &Y, &r_prime2, &pk, &pp) == 0, "adapt(wrong-witness) builds");
    CHECK(base_verify(&bad, m, MLEN, &pk, &pp) != 0, "[N2] wrong-witness adapted must NOT verify");
    CHECK(las_ext(&s_ext, &bad, &presig, &Y, &pp) != 0, "[N2] Ext must fail on wrong-witness adapt");

    /* [N3] a tampered pre-signature must fail PreVerify. */
    tmp_pre = presig;
    tmp_pre.z_hat[0].coeffs[0] += 1;
    CHECK(las_preverify(&tmp_pre, m, MLEN, &Y, &pk, &pp) != 0, "[N3] tampered pre-signature must fail PreVerify");

    /* [N4] a tampered adapted signature must fail ordinary (base) Verify. */
    tmp = adapted;
    tmp.z[0].coeffs[0] += 1;
    CHECK(base_verify(&tmp, m, MLEN, &pk, &pp) != 0, "[N4] tampered adapted signature must fail Verify");
  }

  printf("test_basesig: ALL CHECKS PASSED\n");
  printf("  parameter set n=%d ell=%d kappa=%d gamma=%d  (d=%d, Q=%d)\n",
         LAS_N, ELL, KAPPA, GAMMA, LAS_D, Q);
  printf("  %d iterations x (4 base + 4 interlock + 4 negative) checks; CHECK-gated\n",
         NTESTS);
  return 0;
}
