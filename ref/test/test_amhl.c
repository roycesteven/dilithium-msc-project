/*
 * AMHL - Adaptor Multi-Hop Lock for LAS (eprint 2020/845, Fig. 2 / Section 5).
 *
 * This is the "proper" payment-channel-network construction, and the headline
 * upgrade over the same-Y scriptless HTLC in test_pcn.c.  Two scenarios, each
 * narrated and hard-asserted:
 *
 *   1. K-hop routed payment (happy path).  Every hop carries a DISTINCT
 *      cumulative statement Y_j = A*(l_1+...+l_j).  We show:
 *        - the per-hop witness-norm growth ||s_j||inf <= j (the "knowledge gap"
 *          made concrete - and the reason every hop PreSigns with bound g-k-K);
 *        - the right-to-left cascade with EXACT witness recovery at each hop
 *          (s_{j-1} = s_j - l_j);
 *        - no wormhole: the receiver's secret s_K cannot open a non-adjacent hop.
 *   2. Timeout / refund on a route: an intermediary hop expires and is refunded.
 *
 * The chain stores only public data; PreSign/Adapt happen in the parties' wallets.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../randombytes.h"
#include "../las.h"
#include "../amhl.h"
#include "../chain.h"

#define FAIL(msg) do { fprintf(stderr, "    [FAIL] %s\n", msg); return 1; } while(0)

static size_t mkclaim(uint8_t *buf, long amount, const char *to) {
  char tmp[CHAIN_MSGLEN];
  int n = snprintf(tmp, sizeof tmp, "AMHL: pay %ld to %s", amount, to);
  size_t len;
  if(n < 0) n = 0;
  len = (size_t)n < CHAIN_MSGLEN ? (size_t)n : CHAIN_MSGLEN;
  memcpy(buf, tmp, len);
  return len;
}

static int sk_equal(const las_sk *a, const las_sk *b) {
  unsigned int i, k;
  for(i = 0; i < LAS_M; ++i)
    for(k = 0; k < N; ++k)
      if(a->s[i].coeffs[k] != b->s[i].coeffs[k]) return 0;
  return 1;
}

static int pk_equal(const las_pk *a, const las_pk *b) {
  unsigned int i, k;
  for(i = 0; i < LAS_N; ++i)
    for(k = 0; k < N; ++k)
      if(a->t[i].coeffs[k] != b->t[i].coeffs[k]) return 0;
  return 1;
}

/* ---------------- Scenario 1: K-hop routed payment (happy path) ----------- */
static int scenario_amhl_route(const las_pp *pp) {
  enum { HOPS = 4 };                       /* route U0->U1->U2->U3->U4 (4 hops)  */
  const char *names[HOPS + 1] = {"Alice", "Bob", "Carol", "Dave", "Erin"};
  las_pk pk[HOPS + 1];
  las_sk sk[HOPS + 1];
  amhl_setup st;
  chain net;
  int acc[HOPS + 1];
  uint8_t cm[HOPS + 1][CHAIN_MSGLEN];
  size_t cl[HOPS + 1];
  las_sig presig[HOPS + 1], adapted;
  int cid[HOPS + 1];
  las_sk wit, s_ext, s_prev;
  unsigned int h;
  int j;

  printf("== Scenario 1: AMHL %d-hop routed payment (happy path) ==\n", HOPS);

  for(h = 0; h <= HOPS; ++h) las_keygen(&pk[h], &sk[h], pp);
  amhl_setup_gen(&st, HOPS, pp);          /* sender builds the cumulative locks  */

  /* Distinct per-hop statements + witness-norm growth (||s_j||inf <= j). */
  printf("  per-hop statements and cumulative witness norms:\n");
  for(h = 1; h <= HOPS; ++h)
    printf("    hop %u: Y_%u distinct=%s   ||s_%u||inf = %d  (<= %u)\n",
           h, h, pk_equal(&st.Y[h], &st.Y[h - 1]) ? "NO" : "yes",
           h, (int)amhl_norm(&st.cum[h]), h);
  for(h = 1; h <= HOPS; ++h) {
    if(amhl_norm(&st.cum[h]) > (int32_t)h) FAIL("witness norm exceeds hop index");
    if(pk_equal(&st.Y[h], &st.Y[h - 1])) FAIL("adjacent statements not distinct");
  }

  /* Fund the route.  Hop h: payer U_{h-1} -> payee U_h, amount 10+(K-h), locked
   * to Y_h, pre-signed and pre-verified with the K-hop bound g-k-K.  Timeouts
   * ladder so each puller (right to left) keeps a safety window. */
  chain_init(&net, "AMHL-net", pp);
  for(h = 0; h <= HOPS; ++h)
    acc[h] = chain_account_add(&net, names[h], &pk[h], h == HOPS ? 0 : 100);

  for(h = 1; h <= HOPS; ++h) {
    long amount = 10 + (long)(HOPS - h);
    cl[h] = mkclaim(cm[h], amount, names[h]);
    las_presign_k(&presig[h], cm[h], cl[h], &st.Y[h], &pk[h - 1], &sk[h - 1], pp, HOPS);
    cid[h] = chain_fund_swap_k(&net, acc[h - 1], acc[h], amount, &st.Y[h],
                               &presig[h], cm[h], cl[h], 10u * (HOPS - h + 1), HOPS);
    if(cid[h] < 0) FAIL("fund hop");
    printf("  funded hop %u: %s->%s (%ld), locked to Y_%u\n",
           h, names[h - 1], names[h], amount, h);
  }

  /* No wormhole: the receiver's secret s_K must NOT open the first hop. */
  if(las_adapt(&adapted, &presig[1], cm[1], cl[1], &st.Y[1], &st.cum[HOPS],
               &pk[0], pp) == 0 &&
     chain_claim_swap(&net, cid[1], &adapted) == 0)
    FAIL("wormhole: s_K wrongly opened hop 1");
  printf("  wormhole check: s_%d cannot open hop 1 (distinct statement)  -> OK\n", HOPS);

  /* Cascade: receiver U_K pulls hop K with s_K; each U_{h-1} extracts s_h on
   * chain, recovers s_{h-1} = s_h - l_h, and pulls its own hop. */
  wit = st.cum[HOPS];                      /* receiver holds the full witness s_K */
  for(j = (int)HOPS; j >= 1; --j) {
    h = (unsigned int)j;
    if(las_adapt(&adapted, &presig[h], cm[h], cl[h], &st.Y[h], &wit, &pk[h - 1], pp))
      FAIL("Adapt on hop");
    if(chain_claim_swap(&net, cid[h], &adapted)) FAIL("claim hop");
    printf("  %s pulls hop %u -> paid; s_%u now public\n", names[h], h, h);

    if(h > 1) {
      if(chain_extract_witness(&net, cid[h], &s_ext)) FAIL("extract s_h");
      if(!sk_equal(&s_ext, &st.cum[h])) FAIL("extracted s_h != setup");
      amhl_recover_prev(&s_prev, &s_ext, &st.incr[h - 1]);   /* s_{h-1}=s_h-l_h */
      if(!sk_equal(&s_prev, &st.cum[h - 1])) FAIL("recovered s_{h-1} inexact");
      wit = s_prev;
    }
  }

  /* Final balances: Erin +10, each intermediary +1 fee, Alice -13. */
  if(chain_balance(&net, acc[0]) != 87 || chain_balance(&net, acc[1]) != 101 ||
     chain_balance(&net, acc[2]) != 101 || chain_balance(&net, acc[3]) != 101 ||
     chain_balance(&net, acc[4]) != 10)
    FAIL("final balances");
  printf("  balances: Alice=87 Bob=101 Carol=101 Dave=101 Erin=10");
  printf("  -> paid + per-hop fees, exact recovery OK\n\n");
  return 0;
}

/* ---------------- Scenario 2: timeout / refund on a route ----------------- */
static int scenario_amhl_refund(const las_pp *pp) {
  enum { HOPS = 2 };
  const char *names[HOPS + 1] = {"Alice", "Bob", "Carol"};
  las_pk pk[HOPS + 1];
  las_sk sk[HOPS + 1];
  amhl_setup st;
  chain net;
  int acc[HOPS + 1];
  uint8_t cm[HOPS + 1][CHAIN_MSGLEN];
  size_t cl[HOPS + 1];
  las_sig presig[HOPS + 1];
  int cid[HOPS + 1];
  unsigned int h;

  printf("== Scenario 2: AMHL route times out (refund path) ==\n");
  for(h = 0; h <= HOPS; ++h) las_keygen(&pk[h], &sk[h], pp);
  amhl_setup_gen(&st, HOPS, pp);

  chain_init(&net, "AMHL-net", pp);
  for(h = 0; h <= HOPS; ++h)
    acc[h] = chain_account_add(&net, names[h], &pk[h], h == HOPS ? 0 : 100);

  for(h = 1; h <= HOPS; ++h) {
    long amount = 10 + (long)(HOPS - h);
    cl[h] = mkclaim(cm[h], amount, names[h]);
    las_presign_k(&presig[h], cm[h], cl[h], &st.Y[h], &pk[h - 1], &sk[h - 1], pp, HOPS);
    cid[h] = chain_fund_swap_k(&net, acc[h - 1], acc[h], amount, &st.Y[h],
                               &presig[h], cm[h], cl[h], 10u * (HOPS - h + 1), HOPS);
    if(cid[h] < 0) FAIL("fund hop");
  }
  printf("  route funded; receiver (Carol) goes offline, never reveals s_K\n");

  if(chain_refund_swap(&net, cid[1]) == 0) FAIL("refund allowed before timeout");
  printf("  refund before timeout correctly rejected\n");

  chain_advance(&net, 20);
  for(h = 1; h <= HOPS; ++h)
    if(chain_refund_swap(&net, cid[h])) FAIL("refund after timeout");
  printf("  after timeouts: every hop refunded to its funder\n");

  if(chain_balance(&net, acc[0]) != 100 || chain_balance(&net, acc[1]) != 100 ||
     chain_balance(&net, acc[2]) != 0)
    FAIL("balances not restored");
  printf("  balances restored (no coins lost)  -> safe OK\n\n");
  return 0;
}

int main(void) {
  uint8_t ppseed[LAS_SEEDBYTES];
  las_pp pp;

  randombytes(ppseed, LAS_SEEDBYTES);
  las_setup(&pp, ppseed);

  printf("=== LAS AMHL: multi-hop locks with distinct per-hop statements ===\n\n");
  if(scenario_amhl_route(&pp))  return 1;
  if(scenario_amhl_refund(&pp)) return 1;

  printf("=== All AMHL scenarios settled correctly. ===\n");
  printf("Each hop carries a distinct cumulative statement Y_j = A*(l_1+...+l_j),\n");
  printf("so revealing one hop's witness opens only the adjacent hop (no wormhole);\n");
  printf("the g-k-K PreSign bound absorbs the cumulative witness-norm growth.\n");
  return 0;
}
