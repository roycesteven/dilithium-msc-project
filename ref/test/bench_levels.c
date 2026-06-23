/*
 * bench_levels.c  --  primary Stage-1 fair benchmark:
 *                     the simplified Dilithium-style BASE signature path
 *                     vs the LAS ADAPTOR path, at the SAME parameters and on the
 *                     same primitives.  This is the explicit comparison the
 *                     supervisor asked for -- NOT a vague "LAS vs its own base".
 *
 *   The two paths live in two SEPARATE modules so neither contaminates the other:
 *       BASE path     -> basesig.c  (base_keygen / base_sign / base_verify; no Y)
 *       ADAPTOR path  -> las.c      (las_presign / las_preverify / las_adapt / las_ext)
 *   las.{c,h} are untouched by the base scheme; basesig shares only las.h's parameter
 *   macros and key/signature struct layout (so both sit at the same security level and
 *   their keys/signatures are interchangeable -- see the cross-verify contract below).
 *
 *   BASE  (simplified Dilithium-style signature; no adaptor statement Y):
 *       Sign    : c = H(pk, w,   M)            -- the commitment w is hashed as-is
 *       Verify  : w' = A*z - c*t;  accept iff  c == H(pk, w',  M)
 *       No statement Y ever enters the Fiat-Shamir hash.
 *
 *   LAS ADAPTOR  (the SAME scheme, with the statement/lock Y bound into the hash):
 *       PreSign  : c = H(pk, w + Y, M)         -- Y (the adaptor lock) is folded in
 *       PreVerify: w' = A*z^ - c*t; accept iff c == H(pk, w' + Y, M)
 *       Adapt    : z = z^ + y_witness
 *       Ext      : y_witness = z - z^
 *
 *   Why the adapted signature passes the ORDINARY base Verify with no explicit +Y:
 *       A*z - c*t = A*(z^ + y) - c*t = (A*z^ - c*t) + A*y = w' + Y,
 *   because Y = A*y by construction.  Adapt thus turns the pre-signature into a
 *   fully ordinary base signature; the leaked witness y = z - z^ is exactly what
 *   makes an atomic swap atomic.
 *
 * The benchmark times BOTH paths and reports the adaptor overhead, pairing each
 * adaptor operation with the base operation it mirrors:
 *     PreSign  vs Sign,  PreVerify vs Verify,  Adapt vs Verify,  Extract separately.
 * It also reports the packed component sizes. No optimised CRYSTALS-Dilithium numbers
 * are produced here: that is a different algorithm and is not the comparison made.
 *
 * The benchmark fixes one consistent state (a key pair, a statement/witness, and the
 * signature / pre-signature / adapted signature derived from THAT key) and gates the
 * timing on the full success-path contract -- the ordinary signature verifies; the
 * pre-signature pre-verifies but FAILS ordinary Verify (the statement-binding tripwire);
 * the adapted signature verifies; and Ext recovers the witness exactly -- so no failure
 * or early-return path is ever timed. (Adapt is timed including its internal PreVerify,
 * since a real Adapt must perform it.)
 *
 * Build (Makefile sets -DLAS_N/-DLAS_ELL/-DLAS_KAPPA for each parameter set).  Matching
 * (n,ell,kappa) <-> Dilithium's (K,L,tau) keeps the comparison fair at a stated security
 * level (a dimension-level match; security proofs are out of project scope):
 *     make test/bench_levels_paper   # original LAS dimensions (4,4,60)
 *     make test/bench_levels2        # (4,4,39)  ~ Dilithium-2 level
 *     make test/bench_levels3        # (6,5,49)  ~ Dilithium-3 level
 *     make test/bench_levels5        # (8,7,60)  ~ Dilithium-5 level
 *
 * Sizes are computed by formula from the packing field widths: pk/Y at ceil(log2 Q)
 * bits/coeff, sk/witness at 2 bits/coeff (ternary), signature = ternary c (2 bits) plus
 * response z at ceil(log2(2*(gamma-kappa)+1)) bits/coeff.
 */
#define _POSIX_C_SOURCE 199309L
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "../randombytes.h"
#include "../basesig.h"     /* BASE path: base_keygen/base_sign/base_verify (no Y) */
#include "../las.h"         /* ADAPTOR path: las_presign/preverify/adapt/ext (folds Y) */
#include "../params.h"      /* N, Q */

#define RUNS  10
#define NITER 1000

static double now_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

static void stats(const double *x, int n, double *mean, double *sd) {
  double s = 0.0, m, v = 0.0;
  int i;
  for(i = 0; i < n; ++i) s += x[i];
  m = s / n;
  for(i = 0; i < n; ++i) { double d = x[i] - m; v += d * d; }
  *mean = m;
  *sd   = (n > 1) ? sqrt(v / (n - 1)) : 0.0;
}

static double g_runs[RUNS];
static double g_mean, g_sd;          /* set by MEASURE */
static volatile long g_sink;

/* Run `op` for RUNS x NITER; leave mean/SD (us) in g_mean/g_sd. */
#define MEASURE(op) do {                                              \
    int br_, bi_;                                                     \
    for(br_ = 0; br_ < RUNS; ++br_) {                                 \
      double bt0_ = now_us();                                         \
      for(bi_ = 0; bi_ < NITER; ++bi_) { op; }                       \
      g_runs[br_] = (now_us() - bt0_) / NITER;                        \
    }                                                                \
    stats(g_runs, RUNS, &g_mean, &g_sd);                             \
  } while(0)

static int ceil_log2(double x) { return (int)ceil(log2(x)); }

/* coefficientwise equality of two witness/secret vectors */
static int sk_equal(const las_sk *a, const las_sk *b) {
  unsigned int j, k;
  for(j = 0; j < LAS_M; ++j)
    for(k = 0; k < N; ++k)
      if(a->s[j].coeffs[k] != b->s[j].coeffs[k])
        return 0;
  return 1;
}

int main(void) {
  uint8_t ppseed[LAS_SEEDBYTES];
  uint8_t m[59];
  size_t  mlen = sizeof m;
  las_pp  pp;
  las_pk  pk, Y;
  las_sk  sk, yy, yext;
  las_sig sig, presig, adapted, tmp;

  /* base-op and adaptor-op timings (mean, sd) */
  double kg_m, kg_s, sg_m, sg_s, vf_m, vf_s;
  double ps_m, ps_s, pv_m, pv_s, ad_m, ad_s, ex_m, ex_s;

  /* packed component sizes (bytes) for THIS parameter set */
  int    pk_bits = ceil_log2((double)Q);                       /* 23 for Q<2^23 */
  int    z_bits  = ceil_log2(2.0*(LAS_GAMMA - LAS_KAPPA) + 1.0);
  size_t sz_pk   = (size_t)(LAS_N * N * pk_bits + 7) / 8;
  size_t sz_sk   = (size_t)(LAS_M * N * 2 + 7) / 8;
  size_t sz_c    = (size_t)(N * 2 + 7) / 8;
  size_t sz_z    = (size_t)(LAS_M * N * z_bits + 7) / 8;
  size_t sz_sig  = sz_c + sz_z;

  randombytes(m, mlen);
  randombytes(ppseed, LAS_SEEDBYTES);
  las_setup(&pp, ppseed);

  /* KeyGen (shared by both paths) is measured first because every call overwrites
   * pk/sk.  It belongs to the BASE scheme; the adaptor path adds no KeyGen. */
  MEASURE(base_keygen(&pk, &sk, &pp));  kg_m = g_mean; kg_s = g_sd;

  /* Establish ONE consistent state for every remaining measurement: a single key
   * pair, a statement/witness (Y = A*yy is literally another key pair), and the BASE
   * signature / LAS pre-signature / adapted signature all derived from THAT key.
   * Without this, objects built before the KeyGen loop would no longer match the
   * final key, and PreVerify/Adapt would be timed on their reject paths. */
  base_keygen(&pk, &sk, &pp);                                /* BASE KeyGen          */
  base_keygen(&Y,  &yy, &pp);                                /* statement/witness     */
  base_sign(&sig, m, mlen, &pk, &sk, &pp);                   /* BASE sign (no Y)      */
  las_presign(&presig, m, mlen, &Y, &pk, &sk, &pp);          /* LAS pre-sign (folds Y)*/
  if(las_adapt(&adapted, &presig, m, mlen, &Y, &yy, &pk, &pp) != 0) {
    printf("FATAL: could not establish a valid adapted signature\n");
    return 1;
  }

  /* Refuse to benchmark unless the FULL cross-path success contract holds, so no
   * failure or early-return path is ever timed.  Crucially the ordinary-signature
   * checks use the SEPARATE base verifier (base_verify, from basesig.c), proving the
   * two paths interlock:
   *   - the base signature verifies under base_verify;
   *   - the LAS pre-signature pre-verifies, but base_verify REJECTS it (its hash binds
   *     w+Y, the base verifier recomputes H(pk,w',M) without Y -- the tripwire);
   *   - the LAS-adapted signature verifies under the INDEPENDENT base_verify with no
   *     explicit +Y (because A(z^+y)-ct = w'+Y); and
   *   - Ext recovers the witness EXACTLY. */
  if(base_verify(&sig, m, mlen, &pk, &pp) != 0 ||              /* base sig verifies     */
     las_preverify(&presig, m, mlen, &Y, &pk, &pp) != 0 ||    /* presig pre-verifies   */
     base_verify(&presig, m, mlen, &pk, &pp) == 0 ||          /* presig is NOT a base sig */
     base_verify(&adapted, m, mlen, &pk, &pp) != 0 ||         /* adapted = base sig    */
     las_ext(&yext, &adapted, &presig, &Y, &pp) != 0 ||       /* Ext succeeds          */
     !sk_equal(&yext, &yy)) {                                 /* exact witness recover */
    printf("FATAL: benchmark state inconsistent before timing\n");
    return 1;
  }

  /* BASE path operations (basesig.c; write results to tmp; never mutate sig/presig). */
  MEASURE(base_sign(&tmp, m, mlen, &pk, &sk, &pp));          sg_m = g_mean; sg_s = g_sd;
  MEASURE(g_sink += base_verify(&sig, m, mlen, &pk, &pp));   vf_m = g_mean; vf_s = g_sd;
  /* LAS ADAPTOR path operations (las.c). */
  MEASURE(las_presign(&tmp, m, mlen, &Y, &pk, &sk, &pp));     ps_m = g_mean; ps_s = g_sd;
  MEASURE(g_sink += las_preverify(&presig, m, mlen, &Y, &pk, &pp)); pv_m = g_mean; pv_s = g_sd;
  MEASURE(g_sink += las_adapt(&tmp, &presig, m, mlen, &Y, &yy, &pk, &pp)); ad_m = g_mean; ad_s = g_sd;
  MEASURE(g_sink += las_ext(&yext, &adapted, &presig, &Y, &pp)); ex_m = g_mean; ex_s = g_sd;

  printf("==========================================================================\n");
  printf(" LAS parameter set: n=%d ell=%d kappa=%d gamma=%d  (N=%d, Q=%d)\n",
         LAS_N, LAS_ELL, LAS_KAPPA, LAS_GAMMA, N, Q);
  printf("==========================================================================\n");
  printf(" %d runs x %d iters/op; mean +/- sample SD; single thread, -O3.\n\n", RUNS, NITER);

  printf("--- WHAT IS COMPARED (identical code, parameters, primitives) ---\n");
  printf(" BASE  (simplified Dilithium-style signature; NO adaptor statement):\n");
  printf("   Sign     c = H(pk, w,   M)        Verify    c == H(pk, w',  M)\n");
  printf(" LAS ADAPTOR (same scheme; statement/lock Y folded into the hash):\n");
  printf("   PreSign  c = H(pk, w+Y, M)        PreVerify c == H(pk, w'+Y, M)\n");
  printf("   Adapt    z = z_hat + y_witness    Ext       y_witness = z - z_hat\n");
  printf("   Adapted sig clears ordinary Verify without an explicit +Y because\n");
  printf("     A(z_hat+y) - c*t = (A*z_hat - c*t) + A*y = w' + Y    (Y = A*y).\n\n");

  printf("--- COMPUTATION (microseconds, mean +/- sample SD) ---\n");
  printf("\n Base path (simplified Dilithium-style, c = H(pk, w, M); no Y):\n");
  printf("   KeyGen     %8.2f +/- %6.2f\n", kg_m, kg_s);
  printf("   Sign       %8.2f +/- %6.2f\n", sg_m, sg_s);
  printf("   Verify     %8.2f +/- %6.2f\n", vf_m, vf_s);

  printf("\n LAS adaptor path (Y folded into the hash, c = H(pk, w+Y, M)):\n");
  printf("   PreSign    %8.2f +/- %6.2f\n", ps_m, ps_s);
  printf("   PreVerify  %8.2f +/- %6.2f\n", pv_m, pv_s);
  printf("   Adapt      %8.2f +/- %6.2f\n", ad_m, ad_s);
  printf("   Ext        %8.2f +/- %6.2f\n", ex_m, ex_s);

  printf("\n Adaptor overhead (adaptor op vs the base op it mirrors):\n");
  printf("   PreSign   vs Sign     %8.2f vs %8.2f   (%+.1f%%)\n",
         ps_m, sg_m, 100.0*(ps_m - sg_m)/sg_m);
  printf("   PreVerify vs Verify   %8.2f vs %8.2f   (%+.1f%%)\n",
         pv_m, vf_m, 100.0*(pv_m - vf_m)/vf_m);
  printf("   Adapt     vs Verify   %8.2f vs %8.2f   (%+.1f%%)\n",
         ad_m, vf_m, 100.0*(ad_m - vf_m)/vf_m);
  printf("   Ext       (separate)  %8.2f            (no base analogue)\n", ex_m);

  printf("\n--- COMMUNICATION (packed bytes) ---\n");
  printf("   public key  pk        %6zu\n", sz_pk);
  printf("   secret key  sk        %6zu\n", sz_sk);
  printf("   challenge   c         %6zu\n", sz_c);
  printf("   response    z         %6zu   (%.1f%% of the signature)\n",
         sz_z, 100.0*(double)sz_z/(double)sz_sig);
  printf("   signature   (c,z)     %6zu\n", sz_sig);
  printf("   statement   Y         %6zu\n", sz_pk);
  printf("   pre-signature         %6zu\n", sz_sig);

  return (int)(g_sink & 0);
}
