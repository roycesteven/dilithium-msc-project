/*
 * bench_levels.c  --  PRIMARY (fair) benchmark + Dilithium context.
 *
 * METHODOLOGY (corrected 2026-06-22 per supervisor):
 *
 *   PRIMARY / FAIR comparison = LAS against its OWN simplified Dilithium-style base,
 *   using the SAME parameters and the SAME primitives.  LAS's base signature *is* a
 *   simplified Dilithium (Fiat-Shamir with aborts, ternary secrets, NO hint vector);
 *   LAS is that base plus the four adaptor operations.  So the only thing that varies
 *   is the adaptor layer, and the fair result is its OVERHEAD over the base.  Each
 *   adaptor op is paired with the base op it mirrors algorithmically:
 *
 *        simplified Sign    vs  LAS PreSign
 *        simplified Verify  vs  LAS PreVerify
 *        simplified Verify  vs  LAS Adapt
 *        LAS Ext            reported separately (no base analogue)
 *
 *   CONTEXT ONLY = official optimised CRYSTALS-Dilithium.  It is a DIFFERENT algorithm
 *   (hint vector, Power2Round, high/low-bit decomposition, bit-packing) and is NOT the
 *   fair baseline.  Its module dimensions are matched here only so the context numbers
 *   are at the same security level; it is explicitly labelled "not algorithm-matched".
 *
 * Build (Makefile sets -DDILITHIUM_MODE and the matching -DLAS_* together):
 *     make test/bench_levels_paper   # LAS paper set (n,l,k = 4,4,60)
 *     make test/bench_levels2        # n,l,k = 4,4,39 ; Dilithium-2 context
 *     make test/bench_levels3        # n,l,k = 6,5,49 ; Dilithium-3 context
 *     make test/bench_levels5        # n,l,k = 8,7,60 ; Dilithium-5 context
 *
 * Sizes are computed by formula from the packing field widths (not serialize.c, whose
 * constants are fixed to the paper set): pk/Y at ceil(log2 Q)=23 bits/coeff, sk/witness
 * at 2 bits/coeff (ternary), signature = ternary c (2 bits) + response z at
 * ceil(log2(2*(gamma-kappa)+1)) bits/coeff.
 */
#define _POSIX_C_SOURCE 199309L
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "../randombytes.h"
#include "../sign.h"        /* optimised Dilithium-MODE (context only) */
#include "../las.h"
#include "../params.h"      /* N, Q, K, L */

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

int main(void) {
  uint8_t ppseed[LAS_SEEDBYTES];
  uint8_t m[59];
  size_t  mlen = sizeof m;
  las_pp  pp;
  las_pk  pk, Y;
  las_sk  sk, yy, yext;
  las_sig sig, presig, adapted, tmp;

  uint8_t dpk[CRYPTO_PUBLICKEYBYTES], dsk[CRYPTO_SECRETKEYBYTES], dsig[CRYPTO_BYTES];
  size_t  dsiglen;
  uint8_t ctx[1] = {0};

  /* base-op and adaptor-op timings (mean, sd) */
  double kg_m, kg_s, sg_m, sg_s, vf_m, vf_s;          /* base: KeyGen/Sign/Verify */
  double ps_m, ps_s, pv_m, pv_s, ad_m, ad_s, ex_m, ex_s; /* adaptor ops */
  double dk_m, dk_s, ds_m, ds_s, dv_m, dv_s;          /* Dilithium context */

  /* size formulas (bytes) for THIS parameter set */
  int    pk_bits = ceil_log2((double)Q);                       /* 23 */
  int    z_bits  = ceil_log2(2.0*(LAS_GAMMA - LAS_KAPPA) + 1.0);
  size_t sz_pk   = (size_t)(LAS_N * N * pk_bits + 7) / 8;
  size_t sz_sk   = (size_t)(LAS_M * N * 2 + 7) / 8;
  size_t sz_c    = (size_t)(N * 2 + 7) / 8;
  size_t sz_z    = (size_t)(LAS_M * N * z_bits + 7) / 8;
  size_t sz_sig  = sz_c + sz_z;

  randombytes(m, mlen);
  randombytes(ppseed, LAS_SEEDBYTES);
  las_setup(&pp, ppseed);
  las_keygen(&pk, &sk, &pp);
  las_keygen(&Y, &yy, &pp);
  las_sign(&sig, m, mlen, &pk, &sk, &pp);
  las_presign(&presig, m, mlen, &Y, &pk, &sk, &pp);
  las_adapt(&adapted, &presig, m, mlen, &Y, &yy, &pk, &pp);
  crypto_sign_keypair(dpk, dsk);
  crypto_sign_signature(dsig, &dsiglen, m, mlen, ctx, 0, dsk);

  /* ---- measure base ops (the simplified Dilithium base = LAS KeyGen/Sign/Verify) ---- */
  MEASURE(las_keygen(&pk, &sk, &pp));            kg_m = g_mean; kg_s = g_sd;
  las_keygen(&pk, &sk, &pp);                      /* restore a consistent key */
  las_sign(&sig, m, mlen, &pk, &sk, &pp);
  MEASURE(las_sign(&tmp, m, mlen, &pk, &sk, &pp)); sg_m = g_mean; sg_s = g_sd;
  MEASURE(g_sink += las_verify(&sig, m, mlen, &pk, &pp)); vf_m = g_mean; vf_s = g_sd;
  /* ---- measure adaptor ops ---- */
  MEASURE(las_presign(&tmp, m, mlen, &Y, &pk, &sk, &pp)); ps_m = g_mean; ps_s = g_sd;
  MEASURE(g_sink += las_preverify(&presig, m, mlen, &Y, &pk, &pp)); pv_m = g_mean; pv_s = g_sd;
  MEASURE(g_sink += las_adapt(&tmp, &presig, m, mlen, &Y, &yy, &pk, &pp)); ad_m = g_mean; ad_s = g_sd;
  MEASURE(g_sink += las_ext(&yext, &adapted, &presig, &Y, &pp)); ex_m = g_mean; ex_s = g_sd;
  /* ---- measure Dilithium context ops ---- */
  MEASURE(crypto_sign_keypair(dpk, dsk)); dk_m = g_mean; dk_s = g_sd;
  MEASURE(crypto_sign_signature(dsig, &dsiglen, m, mlen, ctx, 0, dsk)); ds_m = g_mean; ds_s = g_sd;
  MEASURE(g_sink += crypto_sign_verify(dsig, dsiglen, m, mlen, ctx, 0, dpk)); dv_m = g_mean; dv_s = g_sd;

  printf("==========================================================================\n");
  printf(" LAS parameter set: n=%d ell=%d kappa=%d gamma=%d  (N=%d, Q=%d)\n",
         LAS_N, LAS_ELL, LAS_KAPPA, LAS_GAMMA, N, Q);
  printf("==========================================================================\n");
  printf(" %d runs x %d iters/op; mean +/- sample SD; single thread, -O3.\n\n", RUNS, NITER);

  printf("--------------------------------------------------------------------------\n");
  printf(" PRIMARY (FAIR): LAS vs its OWN simplified Dilithium base\n");
  printf("   same algorithm, same parameters, same primitives; only the adaptor varies\n");
  printf("--------------------------------------------------------------------------\n");
  printf(" Base scheme (simplified Dilithium = LAS KeyGen/Sign/Verify):\n");
  printf("   KeyGen   %8.2f +/- %6.2f us\n", kg_m, kg_s);
  printf("   Sign     %8.2f +/- %6.2f us\n", sg_m, sg_s);
  printf("   Verify   %8.2f +/- %6.2f us\n", vf_m, vf_s);
  printf("\n Adaptor overhead (each adaptor op vs the base op it mirrors):\n");
  printf("   %-22s %8.2f vs %8.2f us   (%+.1f%%)\n",
         "PreSign   vs Sign",   ps_m, sg_m, 100.0*(ps_m - sg_m)/sg_m);
  printf("   %-22s %8.2f vs %8.2f us   (%+.1f%%)\n",
         "PreVerify vs Verify", pv_m, vf_m, 100.0*(pv_m - vf_m)/vf_m);
  printf("   %-22s %8.2f vs %8.2f us   (%+.1f%%)\n",
         "Adapt     vs Verify", ad_m, vf_m, 100.0*(ad_m - vf_m)/vf_m);
  printf("   %-22s %8.2f +/- %6.2f us   (reported separately, no base analogue)\n",
         "Ext", ex_m, ex_s);
  printf("   [adaptor SDs: PreSign %.2f, PreVerify %.2f, Adapt %.2f]\n",
         ps_s, pv_s, ad_s);

  printf("\n Communication (packed bytes; the signature is identical to the base):\n");
  printf("   public key pk        %6zu\n", sz_pk);
  printf("   secret key sk        %6zu\n", sz_sk);
  printf("   signature (c,z)      %6zu   [c=%zu, z=%zu; z is %.1f%% of the sig]\n",
         sz_sig, sz_c, sz_z, 100.0*(double)sz_z/(double)sz_sig);
  printf("   statement Y (LAS+)   %6zu   (extra vs a basic signature)\n", sz_pk);
  printf("   pre-signature (LAS+) %6zu\n", sz_sig);

  printf("\n--------------------------------------------------------------------------\n");
  printf(" CONTEXT ONLY: optimised CRYSTALS-Dilithium-%d  (NOT algorithm-matched)\n",
         DILITHIUM_MODE);
  printf("   different algorithm (hint vector, Power2Round, bit-packing); module dims\n");
  printf("   (K=%d,L=%d) matched only to place the context at the same security level.\n",
         K, L);
  printf("--------------------------------------------------------------------------\n");
  printf("   KeyGen   %8.2f +/- %6.2f us        public key %6d\n", dk_m, dk_s, CRYPTO_PUBLICKEYBYTES);
  printf("   Sign     %8.2f +/- %6.2f us        secret key %6d\n", ds_m, ds_s, CRYPTO_SECRETKEYBYTES);
  printf("   Verify   %8.2f +/- %6.2f us        signature  %6d\n", dv_m, dv_s, CRYPTO_BYTES);

  return (int)(g_sink & 0);
}
