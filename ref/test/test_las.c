#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "../randombytes.h"
#include "../las.h"
#include "../params.h"   /* N, Q for the summary print */

#define MLEN 59
#define NTESTS 200

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
  las_pp pp;
  las_pk pk, Y;
  las_sk sk, y, yext;
  las_sig presig, sig, assig;
  unsigned int iter, j;

  for(iter = 0; iter < NTESTS; ++iter) {
    randombytes(ppseed, LAS_SEEDBYTES);
    las_setup(&pp, ppseed);                          /* pp = A            */
    randombytes(m, MLEN);

    las_keygen(&pk, &sk, &pp);                        /* 1. (pk, sk)       */
    las_keygen(&Y, &y, &pp);                          /* 2. (Y, y)         */

    las_presign(&presig, m, MLEN, &Y, &pk, &sk, &pp); /* 3. sigma^         */

    /* 4. PreVerify must accept */
    if(las_preverify(&presig, m, MLEN, &Y, &pk, &pp) != 0) {
      fprintf(stderr, "FAIL[4] PreVerify rejected honest pre-signature\n");
      return 1;
    }

    /* 5. TRIPWIRE: standard Verify on sigma^ must FAIL (hash omits +Y) */
    assig.c = presig.c;
    for(j = 0; j < LAS_M; ++j)
      assig.z[j] = presig.z[j];
    if(las_verify(&assig, m, MLEN, &pk, &pp) == 0) {
      fprintf(stderr, "FAIL[5] TRIPWIRE: pre-signature passed standard Verify\n");
      return 1;
    }

    /* 6. Adapt with the witness */
    if(las_adapt(&sig, &presig, m, MLEN, &Y, &y, &pk, &pp) != 0) {
      fprintf(stderr, "FAIL[6] Adapt failed (PreVerify inside)\n");
      return 1;
    }

    /* 7. Adapted signature must verify with standard Verify */
    if(las_verify(&sig, m, MLEN, &pk, &pp) != 0) {
      fprintf(stderr, "FAIL[7] adapted signature failed standard Verify\n");
      return 1;
    }

    /* 8. Extract recovers the witness: A*y' == Y and y' == y */
    if(las_ext(&yext, &sig, &presig, &Y, &pp) != 0) {
      fprintf(stderr, "FAIL[8] Ext: A*y' != Y\n");
      return 1;
    }
    if(!witness_equal(&yext, &y)) {
      fprintf(stderr, "FAIL[8] Ext: y' != y\n");
      return 1;
    }

    /* sanity: ordinary Sign/Verify round-trip and a forgery check */
    las_sign(&sig, m, MLEN, &pk, &sk, &pp);
    if(las_verify(&sig, m, MLEN, &pk, &pp) != 0) {
      fprintf(stderr, "FAIL ordinary Sign/Verify round-trip\n");
      return 1;
    }
    m[0] ^= 1;                                        /* flip message bit  */
    if(las_verify(&sig, m, MLEN, &pk, &pp) == 0) {
      fprintf(stderr, "FAIL forgery: Verify accepted altered message\n");
      return 1;
    }
  }

  printf("LAS (variant B, eprint 2020/845 Alg.2) OK: %d iterations\n", NTESTS);
  printf("  params: n=%d  ell=%d  kappa=%d  gamma=%d  d(=N)=%d  Q=%d\n",
         LAS_N, LAS_ELL, LAS_KAPPA, LAS_GAMMA, N, Q);
  printf("  bounds: sign reject >=%d  presign reject >=%d   (Q-1)/8=%d\n",
         LAS_BOUND_SIGN, LAS_BOUND_PRESIGN, (Q-1)/8);
  return 0;
}
