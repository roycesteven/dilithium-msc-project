/* Application-level benchmark for LAS (objectives Part D / C2).
 *
 * The per-operation benchmark (bench_las.c) measures the *signature* dimension.
 * This program measures the *application* dimension that the project objectives
 * also require: the communication and on-chain cost of the two LAS-based
 * blockchain workflows built in this project.
 *
 *   (1) Atomic cross-chain swap  - messages exchanged, bytes off-chain, and the
 *       simulated settlement footprint (the adapted signatures that would be
 *       published on a real chain).
 *   (2) Multi-hop AMHL payment   - cost AS A FUNCTION OF PATH LENGTH K: number of
 *       pre-signatures, directly-measured rejection-sampling restarts, signing
 *       time, settlement-payload bytes, and the cumulative witness-norm growth ||s_j||inf
 *       that motivates the tighter g-k-K PreSign bound.
 *
 * Sizes are reported in the "theoretical packed" model (the realistic on-wire /
 * on-chain encoding for this scheme); see bench_las.c for the in-memory vs packed
 * vs paper-estimate distinction.  Restart counts are MEASURED directly via the
 * las_attempts counter (las.h), not estimated from a timing ratio.
 *
 * Simulation note: per the project scope the ledger is SIMULATED (not a deployed
 * Ethereum/Bitcoin contract), so - exactly as the objectives permit - (pre-)signature
 * and statement sizes are used as the proxy for transaction / settlement cost.
 */
#define _POSIX_C_SOURCE 199309L
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "../randombytes.h"
#include "../las.h"
#include "../amhl.h"
#include "../serialize.h"   /* single source of truth for packed sizes */
#include "../params.h"

#define MLEN     59
#define REPEAT   40            /* routes per K, for stable timing/restart means */

static double now_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

/* ---- packed sizes: the ACTUAL serialised sizes from serialize.c ---- */
static size_t packed_sig_bytes(void) { return LAS_SIG_BYTES; }   /* 4672 */
static size_t packed_pk_bytes(void)  { return LAS_PK_BYTES;  }   /* 2944 */
static size_t packed_sk_bytes(void)  { return LAS_SK_BYTES;  }   /*  512 */

/* ---- (1) atomic swap accounting ---- */
static void bench_swap(const las_pp *pp) {
  uint8_t txA[MLEN], txB[MLEN];
  las_pk pkA, pkB, Y;
  las_sk skA, skB, yw, yext;
  las_sig preA, preB, sigA, sigB;
  size_t sig_b = packed_sig_bytes();
  size_t pk_b  = packed_pk_bytes();
  size_t offchain, onchain_sig, onchain_all;
  double t0, t_ms;
  unsigned long att0;
  int ok = 1;

  randombytes(txA, MLEN);
  randombytes(txB, MLEN);
  las_keygen(&pkA, &skA, pp);          /* Alice */
  las_keygen(&pkB, &skB, pp);          /* Bob   */
  las_keygen(&Y,  &yw,  pp);           /* Bob's fresh statement/witness (Y,y) */

  att0 = las_attempts;
  t0 = now_us();
  /* 2. Alice pre-signs her leg (Alice->Bob on chain A), bound to Y */
  las_presign(&preA, txA, MLEN, &Y, &pkA, &skA, pp);
  /* 3. Bob pre-signs his leg (Bob->Alice on chain B), bound to the same Y */
  las_presign(&preB, txB, MLEN, &Y, &pkB, &skB, pp);
  /* 4. Bob (knows y) adapts+publishes sigma_A on chain A */
  ok &= (las_adapt(&sigA, &preA, txA, MLEN, &Y, &yw, &pkA, pp) == 0);
  /* 5. Alice extracts y from the published sigma_A */
  ok &= (las_ext(&yext, &sigA, &preA, &Y, pp) == 0);
  /* 6. Alice adapts+publishes sigma_B on chain B with the recovered witness */
  ok &= (las_adapt(&sigB, &preB, txB, MLEN, &Y, &yext, &pkB, pp) == 0);
  t_ms = (now_us() - t0) / 1000.0;

  /* both published signatures must verify with the ORDINARY verifier */
  ok &= (las_verify(&sigA, txA, MLEN, &pkA, pp) == 0);
  ok &= (las_verify(&sigB, txB, MLEN, &pkB, pp) == 0);
  /* the pre-signatures must NOT verify (statement binding / tripwire) */
  ok &= (las_verify(&preA, txA, MLEN, &pkA, pp) != 0);
  ok &= (las_verify(&preB, txB, MLEN, &pkB, pp) != 0);

  offchain    = pk_b + 2*sig_b;        /* Y, sigma^_A, sigma^_B */
  onchain_sig = 2*sig_b;               /* sigma_A, sigma_B published */
  onchain_all = onchain_sig + 2*pk_b;  /* + statement Y escrowed on each chain */

  printf("--- (1) Atomic cross-chain swap  (2 parties, 2 chains, no scripts) ---\n");
  printf("  off-chain messages exchanged before settlement:\n");
  printf("    1. Bob  ->Alice : statement Y       %6zu B\n", pk_b);
  printf("    2. Alice->Bob   : pre-sig sigma^_A  %6zu B\n", sig_b);
  printf("    3. Bob  ->Alice : pre-sig sigma^_B  %6zu B\n", sig_b);
  printf("    => 3 messages, %zu B off-chain\n", offchain);
  printf("  simulated settlement footprint (ledger-footprint proxy; on a real chain\n");
  printf("  only the adapted signatures would be published):\n");
  printf("    chain A: sigma_A %6zu B     chain B: sigma_B %6zu B\n", sig_b, sig_b);
  printf("    => 2 signatures, %zu B  (%zu B incl. the 2 escrowed statements Y)\n",
         onchain_sig, onchain_all);
  printf("  end-to-end signing work (2x PreSign + 2x Adapt + Ext): %.2f ms\n", t_ms);
  printf("    rejection-sampling attempts for the 2 pre-signs (measured): %lu\n",
         las_attempts - att0);
  printf("  correctness: adapted sigs verify, pre-sigs do not (binding): %s\n",
         ok ? "OK" : "FAIL");
}

/* ---- (2) multi-hop AMHL cost vs path length K ---- */
static void bench_amhl(const las_pp *pp) {
  /* NB: Dilithium's params.h defines the object-like macro `K` (module
   * dimension), so the path length must NOT be named K here - we use `nh`. */
  static const unsigned int Kset[] = {1, 2, 4, 6, 8};
  uint8_t msg[MLEN];
  las_pk pkP;
  las_sk skP;
  unsigned int ki, j, rep, nh;
  size_t sig_b = packed_sig_bytes();
  size_t pk_b  = packed_pk_bytes();

  randombytes(msg, MLEN);
  las_keygen(&pkP, &skP, pp);          /* one payer key reused across hops (cost-equivalent) */

  printf("\n--- (2) Multi-hop AMHL payment: cost as a function of path length K ---\n");
  printf("  (each route: K distinct cumulative statements Y_j=A*s_j; PreSign bound g-k-K)\n\n");
  printf("   K | bound g-k-K | #presig | attempts/presig | presig time (ms) |"
         " settle sigs  | public stmts | max||s_j||inf\n");
  printf("  ---+-------------+---------+-----------------+------------------+"
         "---------------+-------------+--------------\n");

  for(ki = 0; ki < sizeof(Kset)/sizeof(Kset[0]); ++ki) {
    amhl_setup st;
    las_sig presig, adapted;
    double t0, t_total = 0.0;
    unsigned long att0, att_total = 0;
    unsigned int npresig = 0;
    int32_t maxnorm = 0;
    int ok = 1;

    nh = Kset[ki];

    for(rep = 0; rep < REPEAT; ++rep) {
      amhl_setup_gen(&st, nh, pp);                      /* route setup (not timed) */

      att0 = las_attempts;
      t0 = now_us();
      for(j = 1; j <= nh; ++j)                           /* pre-sign every hop */
        las_presign_k(&presig, msg, MLEN, &st.Y[j], &pkP, &skP, pp, nh);
      t_total += now_us() - t0;
      att_total += las_attempts - att0;
      npresig += nh;

      /* correctness sanity (not timed): last hop adapts with cumulative s_K
       * and verifies; record witness-norm growth on the final route */
      ok &= (las_adapt(&adapted, &presig, msg, MLEN, &st.Y[nh], &st.cum[nh], &pkP, pp) == 0);
      ok &= (las_verify(&adapted, msg, MLEN, &pkP, pp) == 0);
      if(rep == REPEAT - 1)
        for(j = 1; j <= nh; ++j) {
          int32_t nj = amhl_norm(&st.cum[j]);
          if(nj > maxnorm) maxnorm = nj;
        }
    }

    printf("  %2u | %11d | %7u | %15.2f | %16.2f | %9zu B   | %9zu B | %4d %s\n",
           nh,
           LAS_BOUND_PRESIGN_K(nh),
           nh,
           (double)att_total / (double)npresig,
           t_total / 1000.0 / REPEAT,
           (size_t)nh * sig_b,
           (size_t)nh * pk_b,
           maxnorm,
           ok ? "OK" : "FAIL");
  }

  printf("\n  observations:\n");
  printf("   - simulated settlement footprint is linear in K: K adapted signatures\n");
  printf("     + K public statements (a payload proxy, not gas on a deployed chain).\n");
  printf("   - witness norm grows as ||s_j||inf <= j (a sum of j ternary vectors):\n");
  printf("     this is the 'knowledge gap' made concrete, and is exactly why every\n");
  printf("     hop must pre-sign at the tighter g-k-K bound (so z=z^+s_j stays <= g-k).\n");
  printf("   - the g-k-K tightening is negligible: with g=%d, going K=1->8 shrinks the\n",
         LAS_GAMMA);
  printf("     accept band by ~%.4f%%, so attempts/presig is ~flat in K (see table).\n",
         100.0 * 7.0 / (double)(LAS_GAMMA - LAS_KAPPA));
}

static void print_literature(void) {
  printf("\n--- literature comparison (sizes) ---\n");
  printf("  this work (packed)   : sig %zu B, pk/Y %zu B, sk/witness %zu B\n",
         packed_sig_bytes(), packed_pk_bytes(), packed_sk_bytes());
  printf("  paper optimised LAS  : sig ~3210 B   (eprint 2020/845, q~2^24, hints+packing)\n");
  printf("  optimised Dilithium-3: sig  3309 B   (NIST ML-DSA reference, bit-packed)\n");
  printf("  IAS  (eprint 2020/1345): far smaller sigs (isogeny) BUT ~60-bit quantum\n");
  printf("       security (CSIDH-512, below NIST's 128-bit bar) and ~100x slower.\n");
  printf("  => the LAS settlement object is a single ordinary-looking signature per leg;\n");
  printf("     bit-packing this scheme (future work) closes most of the gap to 3309 B.\n");
}

int main(void) {
  uint8_t ppseed[LAS_SEEDBYTES];
  las_pp pp;

  randombytes(ppseed, LAS_SEEDBYTES);
  las_setup(&pp, ppseed);

  printf("=== LAS application-level benchmark  (mode %d) ===\n", DILITHIUM_MODE);
  printf("params: n=%d ell=%d kappa=%d gamma=%d N=%d Q=%d   (routes x%d per K)\n",
         LAS_N, LAS_ELL, LAS_KAPPA, LAS_GAMMA, N, Q, REPEAT);
  printf("NOTE: SIMULATED ledger - all byte figures are an application-level payload /\n");
  printf("      settlement-footprint proxy, NOT measured gas on a deployed Ethereum or\n");
  printf("      Bitcoin contract (real-chain deployment is out of scope, see report).\n\n");

  bench_swap(&pp);
  bench_amhl(&pp);
  print_literature();
  return 0;
}
