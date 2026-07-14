#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "../randombytes.h"
#include "../basesig.h"    /* Algorithm 1: base_keygen / base_sign / base_verify + BOUND_SIGN */
#include "../relation.h"   /* hard relation: relation_gen -> (statement, witness)             */
#include "../las.h"        /* Algorithm 2: las_presign / las_preverify / las_adapt / las_ext  */
#include "../serialize.h"  /* byte-level type tripwire (pack pre-sig -> decode as signature)  */
#include "../params.h"     /* Q for the summary print */

#define MLEN 59
/* >=1000 randomised iterations: the objectives' B1 acceptance bar
   ("8-point test passes 100% over >=1000 runs"). Each iteration draws fresh
   public params, keys, statement/witness and message, so this is 1000
   independent end-to-end exercises of the full adaptor contract. */
#define NTESTS 1000

/* Wire-size assumption the byte-level tripwire relies on (holds for every
 * parameter set): a pre-signature is sig-shaped, so its bytes decode as a
 * signature. */
typedef char las_size_relations[(PRE_SIGNATURE_BYTES == SIGNATURE_BYTES) ? 1 : -1];

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
  public_key pk;
  statement Y;
  secret_key sk;
  witness r_prime, s_ext;       /* honest witness r' (Gen); extracted witness s (Ext) */
  pre_signature presig;
  signature sig;
  unsigned int iter;

  for(iter = 0; iter < NTESTS; ++iter) {
    randombytes(ppseed, LAS_SEEDBYTES);
    setup_public_params(&pp, ppseed);                  /* pp = (A, H); expand A = [I|A'] */
    randombytes(m, MLEN);

    if(base_keygen(&pk, &sk, &pp) != 0) {              /* 1. (pk, sk)       */
      fprintf(stderr, "FAIL[1] base_keygen\n");
      return 1;
    }
    if(relation_gen(&Y, &r_prime, &pp) != 0) {         /* 2. (Y, r')        */
      fprintf(stderr, "FAIL[2] relation_gen\n");
      return 1;
    }
    if(las_presign(&presig, m, MLEN, &Y, &pk, &sk, &pp) != 0) {  /* 3. sigma^ */
      fprintf(stderr, "FAIL[3] las_presign\n");
      return 1;
    }

    /* 4. PreVerify must accept */
    if(las_preverify(&presig, m, MLEN, &Y, &pk, &pp) != 0) {
      fprintf(stderr, "FAIL[4] PreVerify rejected honest pre-signature\n");
      return 1;
    }

    /* 5. TRIPWIRE (byte-level): a pre-signature is a DISTINCT type -- decode its
     * bytes AS an ordinary signature; standard Verify must FAIL (hash omits +Y). */
    {
      signature assig;
      uint8_t relabel_b[PRE_SIGNATURE_BYTES];
      if(pack_pre_signature(relabel_b, &presig) != 0 ||
         unpack_signature(&assig, relabel_b) != 0) {
        fprintf(stderr, "FAIL[5] TRIPWIRE pack/unpack\n");
        return 1;
      }
      if(base_verify(&assig, m, MLEN, &pk, &pp) == 0) {
        fprintf(stderr, "FAIL[5] TRIPWIRE: pre-signature passed standard Verify\n");
        return 1;
      }
    }

    /* 6. Adapt with the witness */
    if(las_adapt(&sig, &presig, m, MLEN, &Y, &r_prime, &pk, &pp) != 0) {
      fprintf(stderr, "FAIL[6] Adapt failed (PreVerify inside)\n");
      return 1;
    }

    /* 7. Adapted signature must verify with standard Verify */
    if(base_verify(&sig, m, MLEN, &pk, &pp) != 0) {
      fprintf(stderr, "FAIL[7] adapted signature failed standard Verify\n");
      return 1;
    }

    /* 8. Extract recovers the witness: A*s == Y and s == r' */
    if(las_ext(&s_ext, &sig, &presig, &Y, &pp) != 0) {
      fprintf(stderr, "FAIL[8] Ext: A*s != Y\n");
      return 1;
    }
    if(!witness_equal(&s_ext, &r_prime)) {
      fprintf(stderr, "FAIL[8] Ext: s != r'\n");
      return 1;
    }

    /* sanity: ordinary Sign/Verify round-trip and a forgery check */
    if(base_sign(&sig, m, MLEN, &pk, &sk, &pp) != 0) {
      fprintf(stderr, "FAIL ordinary base_sign\n");
      return 1;
    }
    if(base_verify(&sig, m, MLEN, &pk, &pp) != 0) {
      fprintf(stderr, "FAIL ordinary Sign/Verify round-trip\n");
      return 1;
    }
    m[0] ^= 1;                                        /* flip message bit  */
    if(base_verify(&sig, m, MLEN, &pk, &pp) == 0) {
      fprintf(stderr, "FAIL forgery: Verify accepted altered message\n");
      return 1;
    }
  }

  printf("LAS (variant B, eprint 2020/845 Alg.2) OK: %d/%d iterations "
         "(100%% correctness, 8-point adaptor contract)\n", NTESTS, NTESTS);
  printf("  params: n=%d  ell=%d  kappa=%d  gamma=%d  d=%d  Q=%d\n",
         LAS_N, ELL, KAPPA, GAMMA, LAS_D, Q);
  printf("  bounds: sign reject >=%d  presign reject >=%d   (Q-1)/8=%d\n",
         BOUND_SIGN, BOUND_PRESIGN, (Q-1)/8);
  return 0;
}
