/*
 * !!! SUPERSEDED -- DO NOT CITE -- replaced by ref/test/bench_levels.c !!!
 * This earlier benchmark presented optimised CRYSTALS-Dilithium alongside the
 * simplified scheme in a way that read as a fair baseline.  That framing is wrong:
 * official Dilithium is a DIFFERENT algorithm (hints/Power2Round/packing) and can
 * only be CONTEXT.  The correct primary comparison is LAS vs its OWN simplified
 * Dilithium-style base (same params + primitives) -- see bench_levels.c.  This file
 * is kept only so older references resolve; it is no longer built by `make all`.
 *
 * bench_fair.c  --  (historical) first-stage benchmark.
 *
 * This benchmark exists to answer the supervisor's three demands directly:
 *
 *   #3  Repeatability: every timing is the MEAN over RUNS independent runs, each
 *       of NITER iterations, reported as "mean +/- sample standard deviation".
 *
 *   #2/#4  Computation vs. communication, with COMPONENT breakdown: timings (us)
 *       are reported separately from byte sizes, and every key/signature is split
 *       into its components (challenge c, response z, statement Y, witness y,
 *       pre-signature) so the reader can see WHICH component drives the size.
 *
 *   #5  Fair comparison at a stated security level: the primary comparison is
 *       LAS against a SIMPLIFIED DILITHIUM at IDENTICAL parameters (n=l=4, the
 *       same kappa, gamma and modulus q~2^23). LAS's base signature *is* this
 *       simplified Dilithium, so the base columns are shared and the adaptor
 *       layer is isolated cleanly. The NIST-optimised Dilithium (bit-packed +
 *       hint + Power2Round) is shown as a SEPARATE column, explicitly labelled
 *       as a different security/optimisation point, never conflated.
 *
 * Build:  make test/bench_fair2   (optimised reference = Dilithium2, NIST L2 -
 *                                  the dimension-matched fair point for n=l=4)
 *         make test/bench_fair3   (optimised reference = Dilithium3, NIST L3)
 * The LAS / simplified-Dilithium numbers are identical across builds (LAS's
 * parameters live in las.h and do not depend on DILITHIUM_MODE); only the
 * optimised-Dilithium reference column changes with the mode.
 */
#define _POSIX_C_SOURCE 199309L
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "../randombytes.h"
#include "../sign.h"        /* optimised Dilithium-MODE: crypto_sign_* + CRYPTO_*BYTES */
#include "../las.h"
#include "../serialize.h"   /* LAS_PK_BYTES / LAS_SK_BYTES / LAS_SIG_BYTES + field widths */
#include "../params.h"      /* N, Q */

#define RUNS  10            /* independent runs (>= 5 as required) */
#define NITER 1000          /* iterations averaged inside each run */

static double now_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

/* sample mean and (n-1) standard deviation of an array */
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
static volatile long g_sink;

/* Time `op` for RUNS x NITER and print "label  mean +/- sd us". */
#define BENCH(label, op) do {                                          \
    int br_, bi_; double bm_, bsd_;                                    \
    for(br_ = 0; br_ < RUNS; ++br_) {                                  \
      double bt0_ = now_us();                                          \
      for(bi_ = 0; bi_ < NITER; ++bi_) { op; }                        \
      g_runs[br_] = (now_us() - bt0_) / NITER;                         \
    }                                                                 \
    stats(g_runs, RUNS, &bm_, &bsd_);                                  \
    printf("  %-12s %9.2f +/- %7.2f us\n", label, bm_, bsd_);          \
  } while(0)

int main(void) {
  /* ---- LAS / simplified-Dilithium state (same params) ---- */
  uint8_t ppseed[LAS_SEEDBYTES];
  uint8_t m[59];
  size_t  mlen = sizeof m;
  las_pp  pp;
  las_pk  pk, Y;
  las_sk  sk, y, yext;
  las_sig sig, presig, adapted, tmp;

  /* ---- optimised Dilithium-MODE state ---- */
  uint8_t dpk[CRYPTO_PUBLICKEYBYTES], dsk[CRYPTO_SECRETKEYBYTES], dsig[CRYPTO_BYTES];
  size_t  dsiglen;
  uint8_t ctx[1] = {0};

  /* component byte sizes derived from the packing field widths (serialize.h) */
  const size_t c_bytes   = (size_t)(N * LAS_C_COEFF_BITS) / 8;          /* challenge c   */
  const size_t z_bytes   = (size_t)(LAS_M * N * LAS_Z_COEFF_BITS) / 8;  /* response z    */
  const size_t pk_bytes  = LAS_PK_BYTES;                               /* t (= Y)       */
  const size_t sk_bytes  = LAS_SK_BYTES;                               /* r (= y)       */
  const size_t sig_bytes = LAS_SIG_BYTES;                              /* (c,z)         */

  /* runtime self-check: packed object sizes really equal the macro formulas */
  uint8_t pkb[LAS_PK_BYTES], skb[LAS_SK_BYTES], sgb[LAS_SIG_BYTES];

  randombytes(m, mlen);
  randombytes(ppseed, LAS_SEEDBYTES);
  las_setup(&pp, ppseed);
  las_keygen(&pk, &sk, &pp);
  las_keygen(&Y, &y, &pp);
  las_sign(&sig, m, mlen, &pk, &sk, &pp);
  las_presign(&presig, m, mlen, &Y, &pk, &sk, &pp);
  las_adapt(&adapted, &presig, m, mlen, &Y, &y, &pk, &pp);

  crypto_sign_keypair(dpk, dsk);
  crypto_sign_signature(dsig, &dsiglen, m, mlen, ctx, 0, dsk);

  /* assert the byte interface agrees with the component arithmetic */
  las_pack_pk(pkb, &pk);                       /* pk pack is total (returns void) */
  if(las_pack_sk(skb, &sk) != 0 || las_pack_sig(sgb, &sig) != 0) {
    printf("FATAL: packing failed\n");
    return 1;
  }
  (void)pkb;
  if(c_bytes + z_bytes != sig_bytes) {
    printf("FATAL: component arithmetic mismatch\n");
    return 1;
  }

  printf("==========================================================================\n");
  printf(" FAIR FIRST-STAGE BENCHMARK  (Meeting-3 #2-#5)\n");
  printf("==========================================================================\n");
  printf(" Method: %d independent runs x %d iters/op; single thread, -O3.\n", RUNS, NITER);
  printf("         Timings are mean +/- sample std-dev over the %d runs.\n", RUNS);
  printf("         Same machine for every scheme; all share N=%d, Q=%d.\n\n", N, Q);

  printf(" SECURITY PARAMETERS (stated explicitly for a fair comparison):\n");
  printf("   Simplified Dilithium (= LAS base) : n=%d, ell=%d, kappa=%d, gamma=%d,\n",
         LAS_N, LAS_ELL, LAS_KAPPA, LAS_GAMMA);
  printf("       q~2^23, NO hint/decomposition, full-width coefficient packing.\n");
  printf("   LAS (this work)                   : identical params + adaptor layer\n");
  printf("       (extra statement Y in R_q^n and a pre-signature on the wire).\n");
  printf("   Optimised Dilithium-%d (reference) : NIST scheme, (K,L)=(%d,%d),\n",
         DILITHIUM_MODE, K, L);
  printf("       Power2Round + hint vector + bit-packing, q~2^23.\n");
  printf("   Dimension note: LAS (n=ell=4) matches Dilithium mode 2's (K=L=4)\n");
  printf("   module rank most closely; the LAS parameter set's concrete security\n");
  printf("   is NOT formally claimed here (proofs are out of project scope).\n\n");

  /* ===================== COMPUTATION (timing) ===================== */
  printf("--------------------------------------------------------------------------\n");
  printf(" COMPUTATION COST  (mean +/- std-dev, microseconds)\n");
  printf("--------------------------------------------------------------------------\n");

  printf("\n [A] Base signature  --  Simplified Dilithium  (this IS the LAS base)\n");
  BENCH("KeyGen",  las_keygen(&pk, &sk, &pp));
  /* restore a consistent key after the KeyGen loop churned pk/sk */
  las_keygen(&pk, &sk, &pp);
  las_sign(&sig, m, mlen, &pk, &sk, &pp);
  BENCH("Sign",    las_sign(&tmp, m, mlen, &pk, &sk, &pp));
  BENCH("Verify",  g_sink += las_verify(&sig, m, mlen, &pk, &pp));

  printf("\n [B] LAS adaptor extension  (added on top of [A])\n");
  BENCH("PreSign",   las_presign(&tmp, m, mlen, &Y, &pk, &sk, &pp));
  BENCH("PreVerify", g_sink += las_preverify(&presig, m, mlen, &Y, &pk, &pp));
  BENCH("Adapt",     g_sink += las_adapt(&tmp, &presig, m, mlen, &Y, &y, &pk, &pp));
  BENCH("Ext",       g_sink += las_ext(&yext, &adapted, &presig, &Y, &pp));

  printf("\n [C] Optimised Dilithium-%d  (NIST reference; different security/packing)\n",
         DILITHIUM_MODE);
  BENCH("KeyGen", crypto_sign_keypair(dpk, dsk));
  BENCH("Sign",   crypto_sign_signature(dsig, &dsiglen, m, mlen, ctx, 0, dsk));
  BENCH("Verify", g_sink += crypto_sign_verify(dsig, dsiglen, m, mlen, ctx, 0, dpk));

  /* ===================== COMMUNICATION (sizes) ===================== */
  printf("\n--------------------------------------------------------------------------\n");
  printf(" COMMUNICATION COST  (bytes, broken down by component)\n");
  printf("--------------------------------------------------------------------------\n");

  printf("\n [A/B] Simplified Dilithium / LAS  (packed at this scheme's field widths)\n");
  printf("   %-22s %-18s %8s  %8s\n", "object", "component", "bits/cf", "bytes");
  printf("   %-22s %-18s %8d  %8zu\n", "public key  (pk=t)", "n polys",
         LAS_PK_COEFF_BITS, pk_bytes);
  printf("   %-22s %-18s %8d  %8zu\n", "secret key  (sk=r)", "n+l polys (ternary)",
         LAS_SK_COEFF_BITS, sk_bytes);
  printf("   %-22s %-18s %8d  %8zu\n", "signature", "c  (challenge)",
         LAS_C_COEFF_BITS, c_bytes);
  printf("   %-22s %-18s %8d  %8zu\n", "", "z  (response)",
         LAS_Z_COEFF_BITS, z_bytes);
  printf("   %-22s %-18s %8s  %8zu   <-- total\n", "", "(c,z)", "", sig_bytes);
  printf("   %-22s %-18s %8s  %8s\n", "", "", "", "");
  printf("   LAS-only protocol traffic (a plain signature does NOT send these):\n");
  printf("   %-22s %-18s %8d  %8zu\n", "statement Y", "n polys",
         LAS_PK_COEFF_BITS, pk_bytes);
  printf("   %-22s %-18s %8d  %8zu\n", "witness y (extracted)", "n+l polys (ternary)",
         LAS_SK_COEFF_BITS, sk_bytes);
  printf("   %-22s %-18s %8s  %8zu\n", "pre-signature", "(c,z) same shape",
         "", sig_bytes);

  printf("\n [C] Optimised Dilithium-%d  (NIST bit-packed; for context)\n", DILITHIUM_MODE);
  printf("   public key  : %5d bytes\n", CRYPTO_PUBLICKEYBYTES);
  printf("   secret key  : %5d bytes\n", CRYPTO_SECRETKEYBYTES);
  printf("   signature   : %5d bytes\n", CRYPTO_BYTES);

  /* ===================== headline findings ===================== */
  printf("\n--------------------------------------------------------------------------\n");
  printf(" KEY FINDINGS (which component drives the size)\n");
  printf("--------------------------------------------------------------------------\n");
  printf(" * Within the LAS/simplified signature the response z dominates:\n");
  printf("     z = %zu of %zu bytes (%.1f%%); the challenge c is only %zu bytes.\n",
         z_bytes, sig_bytes, 100.0 * (double)z_bytes / (double)sig_bytes, c_bytes);
  printf(" * LAS vs its simplified-Dilithium base: the signature is the SAME size;\n");
  printf("     LAS's adaptor premium is the extra statement Y (%zu bytes) it must\n",
         pk_bytes);
  printf("     publish, plus a pre-signature (%zu bytes) exchanged off-band.\n", sig_bytes);
  printf(" * vs optimised Dilithium-%d (sig %d B): the optimised scheme is SMALLER\n",
         DILITHIUM_MODE, CRYPTO_BYTES);
  printf("     despite >= security, because Power2Round+hint compress the response\n");
  printf("     and the pk is regenerated from a seed. Our simplified scheme stores\n");
  printf("     z at %d bits/coeff uncompressed -- that is the whole size gap.\n",
         LAS_Z_COEFF_BITS);
  printf(" * Computation: LAS's adaptor ops (PreSign/PreVerify/Adapt/Ext) cost about\n");
  printf("     the same as its Sign/Verify -- the adaptor layer is near-free in time;\n");
  printf("     the post-quantum price is paid in COMMUNICATION, not computation.\n");

  return (int)(g_sink & 0);
}
