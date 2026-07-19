/*
 * Post-quantum atomic swap via LAS -- eprint 2020/845 Section 4.1, Fig. 1,
 * implemented VERBATIM: message order, role of the witness holder, the proof
 * of knowledge pi (ref/relation_zk.{c,h} over the vendored LaZer library),
 * and the abort conditions.  Two parties, two toy ledgers, no scripts.
 *
 *   u1 = Alice: holds coin c1 on chain 1, generates (Y, y) and pi
 *   u2 = Bob  : holds coin c2 on chain 2
 *
 *   u1 -> u2 : { Y, pi, sigma_hat_1, tx1 }     (pre-signs her OWN coin first)
 *   u2 -> u1 : { sigma_hat_2, tx2 }            (only after pi and sigma_hat_1 verify)
 *   u1       : sigma_2 = Adapt((Y,y), sigma_hat_2); publish on chain 2 -> claims c2
 *   u2       : y' = Ext(Y, sigma_2, sigma_hat_2);
 *              sigma_1 = Adapt((Y,y'), sigma_hat_1); publish on chain 1 -> claims c1
 *
 * pi is load-bearing (Section 4.1): it proves knowledge of a TERNARY witness,
 * so by the M-SIS uniqueness argument the y' u2 extracts equals y, hence
 * ||y'||inf <= 1 and u2's Adapt is GUARANTEED to clear the Verify bound.  The
 * demo asserts exactly that after Ext.  Fig. 1 shows the happy path only;
 * the timeout/refund half lives in the chain-level test (test_pcn.c).
 *
 * Prints a narrative and hard-asserts every step (non-zero exit on failure).
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../randombytes.h"
#include "../relation.h"
#include "../relation_zk.h"
#include "../basesig.h"
#include "../las.h"
#include "../serialize.h"

static int witness_equal(const witness *a, const witness *b) {
  unsigned int j, k;
  for(j = 0; j < N_PLUS_ELL; ++j)
    for(k = 0; k < LAS_D; ++k)
      if(a->value[j].coeffs[k] != b->value[j].coeffs[k])
        return 0;
  return 1;
}

static int witness_infnorm_leq1(const witness *a) {
  unsigned int j, k;
  for(j = 0; j < N_PLUS_ELL; ++j)
    for(k = 0; k < LAS_D; ++k)
      if(a->value[j].coeffs[k] < -1 || a->value[j].coeffs[k] > 1)
        return 0;
  return 1;
}

#define FAIL(msg) do { fprintf(stderr, "  [FAIL] %s\n", msg); return 1; } while(0)

static uint8_t pi[PI_PROOF_MAX_BYTES];
static uint8_t wire[PRE_SIGNATURE_BYTES];

int main(void) {
  uint8_t ppseed[LAS_SEEDBYTES];
  public_params pp;
  public_key pk1, pk2;
  secret_key sk1, sk2;
  statement Y;
  witness y, y_ext;
  pre_signature presig1, presig2;
  signature sig1, sig2, forged;
  size_t pilen = 0;

  /* messages = the two transactions being swapped (Fig. 1's tx1, tx2) */
  const uint8_t *tx1 = (const uint8_t *)"tx1: Alice -> Bob, 10 coins on chain 1";
  const uint8_t *tx2 = (const uint8_t *)"tx2: Bob -> Alice, 10 coins on chain 2";
  size_t len1 = strlen((const char *)tx1);
  size_t len2 = strlen((const char *)tx2);

  /* toy ledgers */
  int c1_alice = 10, c1_bob = 0;   /* chain 1 balances */
  int c2_alice = 0,  c2_bob = 10;  /* chain 2 balances */

  printf("=== Post-quantum atomic swap via LAS (Fig. 1, with pi) ===\n\n");

  /* ---- Setup: pp and both key pairs (Algorithm 1 KeyGen) ---- */
  randombytes(ppseed, LAS_SEEDBYTES);
  setup_public_params(&pp, ppseed);
  base_keygen(&pk1, &sk1, &pp);
  base_keygen(&pk2, &sk2, &pp);
  printf("Setup: chain 1: Alice=%d Bob=%d | chain 2: Alice=%d Bob=%d\n\n",
         c1_alice, c1_bob, c2_alice, c2_bob);

  /* ---- u1 (Alice): (Y, y) <- Gen(); pi <- P((t'; r'), ...); PreSign tx1 ---- */
  relation_gen(&Y, &y, &pp);
  if(relation_prove(pi, &pilen, &Y, &y, &pp) != 0)
    FAIL("Alice's relation_prove");
  las_presign(&presig1, tx1, len1, &Y, &pk1, &sk1, &pp);
  printf("1. Alice: (Y,y) = Gen(); pi = PoK{r': A r' = Y, ||r'||inf <= 1} (%zu bytes);\n", pilen);
  printf("   sigma^_1 = PreSign(sk1, Y, tx1).\n");
  printf("   Alice -> Bob : { Y, pi, sigma^_1, tx1 }   (off-chain)\n\n");

  /* ---- u2 (Bob): "If verif. of pi or sigma^_1 fails, Abort" (Fig. 1) ---- */
  if(relation_proof_verify(pi, pilen, &Y, &pp) != 0)
    FAIL("Bob's verification of pi");
  if(las_preverify(&presig1, tx1, len1, &Y, &pk1, &pp) != 0)
    FAIL("Bob's PreVerify of sigma^_1");
  printf("2. Bob: pi verifies (Y really has a ternary preimage) and\n");
  printf("   PreVerify(Y, pk1, sigma^_1, tx1) = true.\n");

  /* a proof must not transfer to a statement it was not made for */
  {
    statement Y_evil;
    witness y_evil;
    relation_gen(&Y_evil, &y_evil, &pp);
    if(relation_proof_verify(pi, pilen, &Y_evil, &pp) == 0)
      FAIL("pi for Y wrongly accepted for a different statement");
    printf("   (pi is rejected against any other statement Y'.)\n");
  }

  /* ---- u2 (Bob): PreSign tx2 under the SAME Y ---- */
  las_presign(&presig2, tx2, len2, &Y, &pk2, &sk2, &pp);
  if(las_preverify(&presig2, tx2, len2, &Y, &pk2, &pp) != 0)
    FAIL("Alice's PreVerify of sigma^_2");
  printf("3. Bob: sigma^_2 = PreSign(sk2, Y, tx2).\n");
  printf("   Bob -> Alice : { sigma^_2, tx2 }   (off-chain; Alice pre-verifies)\n\n");

  /* Neither pre-signature is spendable on its own: an adversary submitting
   * the raw pre-signature bytes as a signature is rejected by the ordinary
   * verifier (the byte route is the only one -- the type system already
   * forbids passing a pre_signature to base_verify). */
  pack_pre_signature(wire, &presig1);
  unpack_signature(&forged, wire);
  if(base_verify(&forged, tx1, len1, &pk1, &pp) == 0)
    FAIL("sigma^_1 bytes wrongly spendable as a signature");
  pack_pre_signature(wire, &presig2);
  unpack_signature(&forged, wire);
  if(base_verify(&forged, tx2, len2, &pk2, &pp) == 0)
    FAIL("sigma^_2 bytes wrongly spendable as a signature");
  printf("   Tripwire: both raw pre-signatures are rejected by ordinary Verify.\n");
  printf("   (Bob cannot claim c1 yet; Alice cannot claim c2 without adapting.)\n\n");

  /* ---- u1 (Alice): sigma_2 = Adapt((Y,y), sigma^_2); abort on bottom;
   * publish on chain 2 => Alice claims c2 ---- */
  if(las_adapt(&sig2, &presig2, tx2, len2, &Y, &y, &pk2, &pp) != 0)
    FAIL("Alice's Adapt of sigma^_2");
  if(base_verify(&sig2, tx2, len2, &pk2, &pp) != 0)
    FAIL("published sigma_2 failed ordinary Verify");
  c2_bob -= 10; c2_alice += 10;
  printf("4. Alice: sigma_2 = Adapt((Y,y), sigma^_2); PUBLISH on chain 2.\n");
  printf("   Verify(pk2, sigma_2, tx2) = true  =>  Alice claims Bob's coin c2.\n");
  printf("   chain 2: Alice=%d Bob=%d\n\n", c2_alice, c2_bob);

  /* ---- u2 (Bob): y' = Ext(Y, sigma_2, sigma^_2);
   * sigma_1 = Adapt((Y,y'), sigma^_1); publish on chain 1 => Bob claims c1 ---- */
  if(las_ext(&y_ext, &sig2, &presig2, &Y, &pp) != 0)
    FAIL("Bob's Ext (A*y' != Y)");
  if(!witness_infnorm_leq1(&y_ext))
    FAIL("extracted y' not ternary -- exactly what pi rules out (Section 4.1)");
  if(!witness_equal(&y_ext, &y))
    FAIL("extracted y' != y (M-SIS uniqueness argument violated)");
  printf("5. Bob: y' = Ext(Y, sigma_2, sigma^_2) from the PUBLIC chain-2 data.\n");
  printf("   ||y'||inf <= 1 and y' == y: the guarantee pi bought (Section 4.1).\n");

  if(las_adapt(&sig1, &presig1, tx1, len1, &Y, &y_ext, &pk1, &pp) != 0)
    FAIL("Bob's Adapt of sigma^_1 with the extracted witness");
  if(base_verify(&sig1, tx1, len1, &pk1, &pp) != 0)
    FAIL("published sigma_1 failed ordinary Verify");
  c1_alice -= 10; c1_bob += 10;
  printf("6. Bob: sigma_1 = Adapt((Y,y'), sigma^_1); PUBLISH on chain 1.\n");
  printf("   Verify(pk1, sigma_1, tx1) = true  =>  Bob claims Alice's coin c1.\n");
  printf("   chain 1: Alice=%d Bob=%d\n\n", c1_alice, c1_bob);

  /* ---- atomicity ---- */
  if(!(c1_alice == 0 && c1_bob == 10 && c2_alice == 10 && c2_bob == 0))
    FAIL("final balances inconsistent");

  printf("=== Swap settled atomically. ===\n");
  printf("Alice's on-chain claim leaked exactly the witness Bob needed; pi\n");
  printf("guaranteed in advance that the leaked witness would be usable.\n");
  return 0;
}
