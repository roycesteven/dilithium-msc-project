/*
 * Post-quantum atomic swap demonstration using LAS (eprint 2020/845).
 *
 * Two parties (Alice, Bob), two ledgers ("chain A", "chain B"), no trusted third
 * party and no on-chain scripts.  The swap is made atomic by a single shared
 * statement Y: Bob can only claim Alice's coin by publishing an adapted signature,
 * which reveals the witness y that Alice needs to claim Bob's coin.
 *
 * Prints a narrative and hard-asserts every step (returns non-zero on failure).
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../randombytes.h"
#include "../las.h"
#include "../params.h"

static int witness_equal(const las_sk *a, const las_sk *b) {
  unsigned int j, k;
  for(j = 0; j < LAS_M; ++j)
    for(k = 0; k < N; ++k)
      if(a->s[j].coeffs[k] != b->s[j].coeffs[k])
        return 0;
  return 1;
}

#define FAIL(msg) do { fprintf(stderr, "  [FAIL] %s\n", msg); return 1; } while(0)

int main(void) {
  uint8_t ppseed[LAS_SEEDBYTES];
  las_pp pp;
  las_pk pkA, pkB, Y;
  las_sk skA, skB, y, yext;
  las_sig presig_A, presig_B, sig_A, sig_B;

  /* messages = the two transactions being swapped */
  const uint8_t *txA = (const uint8_t *)"tx_A: Alice -> Bob, 10 coins on chain A";
  const uint8_t *txB = (const uint8_t *)"tx_B: Bob -> Alice, 10 coins on chain B";
  size_t lenA = strlen((const char *)txA);
  size_t lenB = strlen((const char *)txB);

  /* toy ledgers */
  int A_alice = 10, A_bob = 0;     /* chain A balances */
  int B_alice = 0,  B_bob = 10;    /* chain B balances */

  printf("=== Post-quantum atomic swap via LAS ===\n\n");

  /* ---- Setup ---- */
  randombytes(ppseed, LAS_SEEDBYTES);
  las_setup(&pp, ppseed);
  las_keygen(&pkA, &skA, &pp);
  las_keygen(&pkB, &skB, &pp);
  las_keygen(&Y, &y, &pp);          /* Bob draws the statement/witness (Y, y) */
  printf("Setup: A (chain A: Alice=%d Bob=%d) | (chain B: Alice=%d Bob=%d)\n",
         A_alice, A_bob, B_alice, B_bob);
  printf("Bob generates statement/witness (Y, y) = KeyGen; keeps y secret.\n\n");

  /* ---- 1. Bob sends Y to Alice (statement only) ---- */
  printf("1. Bob -> Alice: statement Y (not the witness y).\n");

  /* ---- 2. Alice pre-signs tx_A bound to Y; Bob pre-verifies ---- */
  las_presign(&presig_A, txA, lenA, &Y, &pkA, &skA, &pp);
  printf("2. Alice pre-signs tx_A bound to Y -> sigma^_A ; sends to Bob.\n");
  if(las_preverify(&presig_A, txA, lenA, &Y, &pkA, &pp) != 0)
    FAIL("Bob's PreVerify of sigma^_A");
  printf("   Bob: PreVerify(Y, pkA, sigma^_A, tx_A) = true.\n");

  /* ---- 3. Bob pre-signs tx_B bound to the same Y; Alice pre-verifies ---- */
  las_presign(&presig_B, txB, lenB, &Y, &pkB, &skB, &pp);
  printf("3. Bob pre-signs tx_B bound to the SAME Y -> sigma^_B ; sends to Alice.\n");
  if(las_preverify(&presig_B, txB, lenB, &Y, &pkB, &pp) != 0)
    FAIL("Alice's PreVerify of sigma^_B");
  printf("   Alice: PreVerify(Y, pkB, sigma^_B, tx_B) = true.\n");

  /* Neither pre-signature is spendable on its own (the tripwire). */
  if(las_verify(&presig_A, txA, lenA, &pkA, &pp) == 0)
    FAIL("sigma^_A wrongly accepted by ordinary Verify");
  if(las_verify(&presig_B, txB, lenB, &pkB, &pp) == 0)
    FAIL("sigma^_B wrongly accepted by ordinary Verify");
  printf("   Neither pre-signature is spendable: ordinary Verify rejects both.\n");
  printf("   (Alice cannot finish sigma^_B yet - she does not know y.)\n\n");

  /* ---- 4. Bob adapts with y and publishes on chain A ---- */
  if(las_adapt(&sig_A, &presig_A, txA, lenA, &Y, &y, &pkA, &pp) != 0)
    FAIL("Bob's Adapt of sigma^_A");
  if(las_verify(&sig_A, txA, lenA, &pkA, &pp) != 0)
    FAIL("published sigma_A failed ordinary Verify");
  A_alice -= 10; A_bob += 10;        /* tx_A executes */
  printf("4. Bob: sigma_A = Adapt((Y,y), sigma^_A); PUBLISH on chain A.\n");
  printf("   Verify(pkA, sigma_A, tx_A) = true  =>  Bob claims Alice's coin.\n");
  printf("   chain A: Alice=%d Bob=%d\n\n", A_alice, A_bob);

  /* ---- 5. Alice observes sigma_A and extracts the witness ---- */
  if(las_ext(&yext, &sig_A, &presig_A, &Y, &pp) != 0)
    FAIL("Alice's Ext (A*y' != Y)");
  if(!witness_equal(&yext, &y))
    FAIL("extracted witness y' != y");
  printf("5. Alice observes sigma_A on chain A and extracts y' = Ext(Y, sigma_A, sigma^_A).\n");
  printf("   Check: A*y' == Y and y' == y (exact recovery).\n\n");

  /* ---- 6. Alice adapts sigma^_B with the extracted witness, publishes on chain B ---- */
  if(las_adapt(&sig_B, &presig_B, txB, lenB, &Y, &yext, &pkB, &pp) != 0)
    FAIL("Alice's Adapt of sigma^_B with extracted witness");
  if(las_verify(&sig_B, txB, lenB, &pkB, &pp) != 0)
    FAIL("published sigma_B failed ordinary Verify");
  B_bob -= 10; B_alice += 10;        /* tx_B executes */
  printf("6. Alice: sigma_B = Adapt((Y,y'), sigma^_B); PUBLISH on chain B.\n");
  printf("   Verify(pkB, sigma_B, tx_B) = true  =>  Alice claims Bob's coin.\n");
  printf("   chain B: Alice=%d Bob=%d\n\n", B_alice, B_bob);

  /* ---- atomicity check ---- */
  if(!(A_alice == 0 && A_bob == 10 && B_alice == 10 && B_bob == 0))
    FAIL("final balances inconsistent");

  printf("=== Swap settled atomically. ===\n");
  printf("Both legs executed; Bob's claim forced the on-chain reveal of y that\n");
  printf("enabled Alice's claim. Either both settle or neither does.\n");
  return 0;
}
