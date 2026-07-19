/*
 * Tests for the pi module (ref/relation_zk.{c,h}): the Fig. 1 proof of
 * knowledge of a ternary witness r' with A r' = t' (eprint 2020/845 §4.1),
 * realised over the vendored LaZer library.
 *
 * Checks (hard-asserts, non-zero exit on failure):
 *   1. completeness  : an honest Gen witness proves and verifies,
 *   2. tamper        : every proof is rejected after a single byte flip,
 *   3. wrong statement: the proof is rejected against a different Y,
 *   4. prover contract: a non-ternary witness is refused (no proof emitted).
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../randombytes.h"
#include "../relation.h"
#include "../relation_zk.h"

#define FAIL(msg) do { fprintf(stderr, "  [FAIL] %s\n", msg); return 1; } while(0)

static uint8_t proof[PI_PROOF_MAX_BYTES];

int main(void) {
  uint8_t ppseed[LAS_SEEDBYTES];
  public_params pp;
  statement Y, Y2;
  witness r_prime, r_prime2;
  size_t prooflen = 0;

  printf("=== pi (relation_zk over LaZer): knowledge of ternary r' with A r' = t' ===\n");

  randombytes(ppseed, LAS_SEEDBYTES);
  setup_public_params(&pp, ppseed);
  relation_gen(&Y, &r_prime, &pp);
  relation_gen(&Y2, &r_prime2, &pp);   /* an unrelated second statement */

  /* 1. completeness */
  if(relation_prove(proof, &prooflen, &Y, &r_prime, &pp) != 0)
    FAIL("relation_prove on an honest Gen witness");
  printf("prove: ok, pi = %zu bytes\n", prooflen);
  if(relation_proof_verify(proof, prooflen, &Y, &pp) != 0)
    FAIL("relation_proof_verify on an honest proof");
  printf("verify (honest): accept\n");

  /* 2. single-byte tamper across the proof (sampled stride to keep it fast) */
  {
    size_t pos;
    for(pos = 0; pos < prooflen; pos += 997) {
      proof[pos] ^= 1;
      if(relation_proof_verify(proof, prooflen, &Y, &pp) == 0) {
        fprintf(stderr, "  [FAIL] tampered proof accepted (byte %zu)\n", pos);
        return 1;
      }
      proof[pos] ^= 1;
    }
    printf("verify (tampered): rejected at every sampled byte\n");
  }

  /* 3. proof does not transfer to a different statement */
  if(relation_proof_verify(proof, prooflen, &Y2, &pp) == 0)
    FAIL("proof for Y accepted against Y2");
  printf("verify (wrong statement): rejected\n");

  /* untampered proof still verifies after the negative tests */
  if(relation_proof_verify(proof, prooflen, &Y, &pp) != 0)
    FAIL("honest proof no longer verifies");

  /* 4. prover refuses a non-ternary witness (Ext-style, knowledge gap) */
  {
    witness bad = r_prime;
    bad.value[0].coeffs[0] = 2;
    if(relation_prove(proof, &prooflen, &Y, &bad, &pp) == 0)
      FAIL("relation_prove accepted a non-ternary witness");
    printf("prove (non-ternary witness): refused\n");
  }

  printf("=== pi tests passed ===\n");
  return 0;
}
