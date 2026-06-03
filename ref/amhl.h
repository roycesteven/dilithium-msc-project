#ifndef AMHL_H
#define AMHL_H

/*
 * AMHL - Adaptor Multi-Hop Lock (Esgin, Ersoy, Erkin, eprint 2020/845, Fig. 2 /
 * Section 5), the "proper" payment-channel-network construction for LAS.
 *
 * Motivation: the same-Y scriptless HTLC (see chain.{c,h} / test_pcn.c) locks
 * EVERY hop of a route to one shared statement Y.  Revealing the witness on any
 * hop lets a party adapt all other hops on the route - the "wormhole" attack.
 * AMHL fixes this by giving every hop a DISTINCT cumulative statement.
 *
 * Setup (run by the sender U_0, who knows the whole K-hop route U_0..U_K):
 *   - sample per-hop increments l_1..l_K  (ternary, ||l_j||inf <= 1)
 *   - cumulative witness  s_0 = 0,  s_j = s_{j-1} + l_j = l_1 + ... + l_j
 *   - per-hop statement   Y_j = A * s_j        (Y_0 = 0)
 *   Hop j (payer U_{j-1} -> payee U_j) is locked to Y_j and opened with s_j.
 *
 * Secret distribution (each party learns only what it needs):
 *   - receiver U_K is given the full cumulative witness s_K  (the "invoice");
 *   - intermediary U_j (1 <= j < K) is given only the increment l_{j+1}.
 *
 * Cascade (claims flow receiver -> sender, right to left):
 *   - U_K pulls hop K with s_K, publishing it; U_{K-1} extracts s_K and computes
 *     s_{K-1} = s_K - l_K to pull hop K-1; and so on down to hop 1.
 *   Because each hop carries a different statement, learning s_j tells a party
 *     nothing about any s_{j'} for a non-adjacent hop - no wormhole.
 *
 * Norm growth (the "knowledge gap" made concrete):
 *   s_j is a sum of j ternary vectors, so ||s_j||inf <= j <= K.  The adapted
 *   response z = z^ + s_j then has ||z||inf <= (g-k-K) + K = g-k, which is why
 *   every hop must PreSign with the tighter K-hop bound g-k-K (las_presign_k).
 */

#include "las.h"

#define AMHL_MAXK 8

typedef struct {
  unsigned int nhops;                 /* number of hops on the route                 */
  las_sk incr[AMHL_MAXK];         /* increments l_1..l_K  (incr[j-1] = l_j)       */
  las_sk cum[AMHL_MAXK + 1];      /* cumulative s_0..s_K  (cum[j] = s_j, cum[0]=0) */
  las_pk Y[AMHL_MAXK + 1];        /* statements Y_0..Y_K  (Y[j] = A*s_j, Y[0]=0)   */
} amhl_setup;

/* Generate the AMHL route setup for K hops (1 <= K <= AMHL_MAXK). */
void amhl_setup_gen(amhl_setup *st, unsigned int nhops, const las_pp *pp);

/* Infinity norm (max |centred coefficient|) of a witness vector - used to
 * illustrate the per-hop norm growth ||s_j||inf <= j. */
int32_t amhl_norm(const las_sk *s);

/* Recover the previous cumulative witness: prev = cur - incr  (s_{j-1} = s_j - l_j).
 * This is what an intermediary U_{j-1} computes after extracting s_j on-chain. */
void amhl_recover_prev(las_sk *prev, const las_sk *cur, const las_sk *incr);

#endif
