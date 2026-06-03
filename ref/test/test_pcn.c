/*
 * Realistic chain integration for LAS: scriptless HTLCs on a toy ledger.
 *
 * Three scenarios, each narrated and hard-asserted:
 *   1. Cross-chain atomic swap (happy path)         - both legs settle, y cascades
 *   2. Cross-chain swap timeout (refund path)        - no one claims, both refund
 *   3. Multi-hop payment Alice->Bob->Carol (a PCN)   - one secret cascades a route
 *
 * The chain stores only public data; PreSign/Adapt happen in the parties' wallets.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../randombytes.h"
#include "../las.h"
#include "../chain.h"

#define FAIL(msg) do { fprintf(stderr, "    [FAIL] %s\n", msg); return 1; } while(0)

/* Build a claim "transaction" string into a byte buffer. */
static size_t mkclaim(uint8_t *buf, const char *chn, long amount, const char *to) {
  char tmp[CHAIN_MSGLEN];
  int n = snprintf(tmp, sizeof tmp, "%s: pay %ld to %s", chn, amount, to);
  size_t len;
  if(n < 0) n = 0;
  len = (size_t)n < CHAIN_MSGLEN ? (size_t)n : CHAIN_MSGLEN;
  memcpy(buf, tmp, len);
  return len;
}

/* ---------------- Scenario 1: cross-chain atomic swap (happy path) -------- */
static int scenario_swap_happy(const las_pp *pp) {
  las_pk pkA, pkB, Y;
  las_sk skA, skB, y, yext;
  chain chA, chB;
  uint8_t cmA[CHAIN_MSGLEN], cmB[CHAIN_MSGLEN];
  size_t lA, lB;
  las_sig presigA, presigB, adaptedA, adaptedB;
  int aA, aB, bA, bB, cidA, cidB;

  printf("== Scenario 1: cross-chain atomic swap (happy path) ==\n");
  las_keygen(&pkA, &skA, pp);
  las_keygen(&pkB, &skB, pp);
  las_keygen(&Y, &y, pp);                       /* Bob's swap secret (Y, y) */

  chain_init(&chA, "chainA", pp);
  chain_init(&chB, "chainB", pp);
  aA = chain_account_add(&chA, "Alice", &pkA, 100);
  bA = chain_account_add(&chA, "Bob",   &pkB, 0);
  aB = chain_account_add(&chB, "Alice", &pkA, 0);
  bB = chain_account_add(&chB, "Bob",   &pkB, 100);

  lA = mkclaim(cmA, "chainA", 10, "Bob");
  lB = mkclaim(cmB, "chainB", 10, "Alice");

  /* Alice locks 10 on chain A to Bob (timeout 20); Bob locks 10 on chain B to
   * Alice (timeout 10 - the second claimant gets the longer safety window). */
  las_presign(&presigA, cmA, lA, &Y, &pkA, &skA, pp);
  cidA = chain_fund_swap(&chA, aA, bA, 10, &Y, &presigA, cmA, lA, 20);
  if(cidA < 0) FAIL("fund chainA");
  las_presign(&presigB, cmB, lB, &Y, &pkB, &skB, pp);
  cidB = chain_fund_swap(&chB, bB, aB, 10, &Y, &presigB, cmB, lB, 10);
  if(cidB < 0) FAIL("fund chainB");
  printf("  funded: chainA Alice->Bob(10), chainB Bob->Alice(10), both locked to Y\n");

  /* Bob claims on chain A using the witness -> publishes adapted sig (reveals y) */
  if(las_adapt(&adaptedA, &presigA, cmA, lA, &Y, &y, &pkA, pp)) FAIL("Bob Adapt A");
  if(chain_claim_swap(&chA, cidA, &adaptedA)) FAIL("Bob claim A");
  printf("  Bob claims on chainA -> gets 10; witness now public on chainA\n");

  /* Alice watches chain A, extracts y, claims on chain B */
  if(chain_extract_witness(&chA, cidA, &yext)) FAIL("Alice extract from chainA");
  if(las_adapt(&adaptedB, &presigB, cmB, lB, &Y, &yext, &pkB, pp)) FAIL("Alice Adapt B");
  if(chain_claim_swap(&chB, cidB, &adaptedB)) FAIL("Alice claim B");
  printf("  Alice extracts y from chainA, claims on chainB -> gets 10\n");

  if(chain_balance(&chA, aA) != 90 || chain_balance(&chA, bA) != 10 ||
     chain_balance(&chB, aB) != 10 || chain_balance(&chB, bB) != 90)
    FAIL("final balances");
  printf("  balances: chainA Alice=90 Bob=10 | chainB Alice=10 Bob=90  -> atomic OK\n\n");
  return 0;
}

/* ---------------- Scenario 2: timeout / refund (unhappy path) ------------- */
static int scenario_swap_refund(const las_pp *pp) {
  las_pk pkA, pkB, Y;
  las_sk skA, skB, y;
  chain chA, chB;
  uint8_t cmA[CHAIN_MSGLEN], cmB[CHAIN_MSGLEN];
  size_t lA, lB;
  las_sig presigA, presigB;
  int aA, bA, aB, bB, cidA, cidB;

  printf("== Scenario 2: cross-chain swap times out (refund path) ==\n");
  las_keygen(&pkA, &skA, pp);
  las_keygen(&pkB, &skB, pp);
  las_keygen(&Y, &y, pp);

  chain_init(&chA, "chainA", pp);
  chain_init(&chB, "chainB", pp);
  aA = chain_account_add(&chA, "Alice", &pkA, 100);
  bA = chain_account_add(&chA, "Bob",   &pkB, 0);
  aB = chain_account_add(&chB, "Alice", &pkA, 0);
  bB = chain_account_add(&chB, "Bob",   &pkB, 100);

  lA = mkclaim(cmA, "chainA", 10, "Bob");
  lB = mkclaim(cmB, "chainB", 10, "Alice");

  las_presign(&presigA, cmA, lA, &Y, &pkA, &skA, pp);
  cidA = chain_fund_swap(&chA, aA, bA, 10, &Y, &presigA, cmA, lA, 20);
  las_presign(&presigB, cmB, lB, &Y, &pkB, &skB, pp);
  cidB = chain_fund_swap(&chB, bB, aB, 10, &Y, &presigB, cmB, lB, 10);
  if(cidA < 0 || cidB < 0) FAIL("fund");
  printf("  both legs funded; Bob goes offline and never reveals y\n");

  /* refund must be impossible before the time lock */
  if(chain_refund_swap(&chB, cidB) == 0) FAIL("refund allowed before timeout");
  printf("  refund before timeout correctly rejected\n");

  chain_advance(&chB, 10);                         /* height reaches B's timeout */
  if(chain_refund_swap(&chB, cidB)) FAIL("Bob refund chainB after timeout");
  chain_advance(&chA, 20);                          /* height reaches A's timeout */
  if(chain_refund_swap(&chA, cidA)) FAIL("Alice refund chainA after timeout");
  printf("  after timeouts: Bob refunds chainB, Alice refunds chainA\n");

  if(chain_balance(&chA, aA) != 100 || chain_balance(&chA, bA) != 0 ||
     chain_balance(&chB, aB) != 0   || chain_balance(&chB, bB) != 100)
    FAIL("balances not restored");
  printf("  balances restored to initial (no coins lost)  -> safe OK\n\n");
  return 0;
}

/* ---------------- Scenario 3: multi-hop payment (PCN) --------------------- */
static int scenario_pcn_multihop(const las_pp *pp) {
  las_pk pkA, pkB, pkC, Y;
  las_sk skA, skB, skC, y, yext;
  chain net;
  uint8_t cmAB[CHAIN_MSGLEN], cmBC[CHAIN_MSGLEN];
  size_t lAB, lBC;
  las_sig presigAB, presigBC, adaptedBC, adaptedAB;
  int A, B, C, cidAB, cidBC;

  printf("== Scenario 3: multi-hop payment Alice -> Bob -> Carol (PCN) ==\n");
  las_keygen(&pkA, &skA, pp);
  las_keygen(&pkB, &skB, pp);
  las_keygen(&pkC, &skC, pp);
  las_keygen(&Y, &y, pp);                          /* Carol's invoice secret */

  chain_init(&net, "PCN", pp);
  A = chain_account_add(&net, "Alice", &pkA, 100);
  B = chain_account_add(&net, "Bob",   &pkB, 100); /* Bob provides liquidity */
  C = chain_account_add(&net, "Carol", &pkC, 0);

  /* Carol issues invoice Y; pay 10, 1 fee per hop. Alice routes 11 via Bob. */
  lAB = mkclaim(cmAB, "PCN", 11, "Bob");
  lBC = mkclaim(cmBC, "PCN", 10, "Carol");

  /* Hop 1: Alice -> Bob, 11, locked to Y, timeout 30 (outer/longer). */
  las_presign(&presigAB, cmAB, lAB, &Y, &pkA, &skA, pp);
  cidAB = chain_fund_swap(&net, A, B, 11, &Y, &presigAB, cmAB, lAB, 30);
  /* Hop 2: Bob -> Carol, 10, locked to the SAME Y, timeout 20 (inner/shorter). */
  las_presign(&presigBC, cmBC, lBC, &Y, &pkB, &skB, pp);
  cidBC = chain_fund_swap(&net, B, C, 10, &Y, &presigBC, cmBC, lBC, 20);
  if(cidAB < 0 || cidBC < 0) FAIL("fund hops");
  printf("  routed: Alice->Bob(11) and Bob->Carol(10), both locked to invoice Y\n");

  /* Carol pulls the inner hop with her secret y -> reveals y on Bob->Carol. */
  if(las_adapt(&adaptedBC, &presigBC, cmBC, lBC, &Y, &y, &pkB, pp)) FAIL("Carol Adapt");
  if(chain_claim_swap(&net, cidBC, &adaptedBC)) FAIL("Carol claim");
  printf("  Carol claims Bob->Carol -> gets 10; witness now public\n");

  /* Bob learns y from the inner hop and pulls the outer hop from Alice. */
  if(chain_extract_witness(&net, cidBC, &yext)) FAIL("Bob extract");
  if(las_adapt(&adaptedAB, &presigAB, cmAB, lAB, &Y, &yext, &pkA, pp)) FAIL("Bob Adapt");
  if(chain_claim_swap(&net, cidAB, &adaptedAB)) FAIL("Bob claim");
  printf("  Bob extracts y, claims Alice->Bob -> gets 11 (recovers 10 + 1 fee)\n");

  if(chain_balance(&net, A) != 89 || chain_balance(&net, B) != 101 ||
     chain_balance(&net, C) != 10)
    FAIL("final balances");
  printf("  balances: Alice=89 Bob=101 Carol=10  -> Carol paid, Bob earned fee OK\n\n");
  return 0;
}

int main(void) {
  uint8_t ppseed[LAS_SEEDBYTES];
  las_pp pp;

  randombytes(ppseed, LAS_SEEDBYTES);
  las_setup(&pp, ppseed);

  printf("=== LAS on a toy chain: scriptless HTLCs ===\n\n");
  if(scenario_swap_happy(&pp))   return 1;
  if(scenario_swap_refund(&pp))  return 1;
  if(scenario_pcn_multihop(&pp)) return 1;

  printf("=== All scenarios settled correctly. ===\n");
  printf("Adaptor signatures replace hash-locks (claim reveals the witness) and the\n");
  printf("block-height timeout replaces the time-lock refund path - no on-chain\n");
  printf("scripts, post-quantum secure.\n");
  return 0;
}
