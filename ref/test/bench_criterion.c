/*
 * bench_criterion.c  --  C mirror of the Rust Criterion.rs statistical benchmark
 *                        (rust/fips204-las/benches/las_bench.rs), so the C and
 *                        Rust implementations are measured under the SAME
 *                        methodology, not merely "similar" ones.
 *
 * What is replicated 1:1 from Criterion 0.8 with the project's config
 * (300 samples / 60 s measurement window; warm-up left at Criterion's 3 s
 * default), per benchmarked operation:
 *
 *   1. WARM-UP: run the operation in doubling batches until >= 3 s has
 *      elapsed; estimate the mean time per iteration from the whole warm-up.
 *   2. LINEAR SAMPLING (Criterion SamplingMode::Linear): choose the smallest
 *      d >= 1 such that running sample i with i*d iterations, i = 1..300,
 *      fits the 60 s window:  d = ceil(T / (t_est * N(N+1)/2)).  Sample i's
 *      VALUE is elapsed_i / (i*d) -- microseconds per iteration.
 *   3. STATISTICS over the 300 sample values: mean, sample SD, median, MAD,
 *      min, max; plus Criterion's regression point estimate ("slope": the
 *      least-squares slope through the origin of elapsed vs iterations) and
 *      a 100 000-resample bootstrap 95% confidence interval for the mean
 *      (Criterion's default resample count).  Criterion additionally
 *      bootstraps CIs for the other statistics; those live in its HTML
 *      report and are not needed for the report tables, which quote
 *      mean +/- SD (sign-class) and median (verify-class).
 *
 * Everything else mirrors las_bench.rs exactly:
 *   - same fixed pp seed (00..1f) and the same 33-byte message;
 *   - ONE consistent state, asserted against the full success-path contract
 *     before any measurement (the pre-signature must FAIL the ordinary
 *     verifier), so no failure path is ever timed;
 *   - same operation set and order: Algorithm 1 KeyGen / Sign / Verify, then
 *     Algorithm 2 PreSign / PreVerify / Adapt (incl. its internal PreVerify) /
 *     Extract;
 *   - the REJECTION GATE: sign-class calls are counted across warm-up AND
 *     measurement (exactly as the Rust closure counts b.iter calls in both
 *     phases) and the measured attempts/call is hard-asserted against the
 *     exact theory las_expected_attempts() within 5 sigma -- identical check
 *     and line format as the Rust drivers.  A drifting run aborts: it is NOT
 *     valid evidence.
 *
 * Differences that CANNOT be mirrored (stated, not hidden):
 *   - keygen/mask randomness comes from the system RNG (the C API has no
 *     injectable RNG); the Rust driver uses a fixed-seed ChaCha8.  The
 *     rejection gate validates the restart statistics of every run either way.
 *   - raw times are NOT comparable across languages (different compilers and
 *     optimisation profiles); compare Algorithm 1 vs Algorithm 2 ratios only.
 *
 * Expected duration: 7 ops x (3 s warm-up + 60 s measurement) ~ 8 minutes,
 * matching the Rust run.
 *
 * Build/run (Simplified Dilithium-III set, pairing the Rust port's fixed set):
 *     make test/bench_criterion3 && ./test/bench_criterion3
 */
#define _POSIX_C_SOURCE 199309L
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "../basesig.h"     /* Algorithm 1: base_keygen/base_sign/base_verify   */
#include "../las.h"         /* Algorithm 2: las_presign/preverify/adapt/ext     */
#include "../params.h"      /* N, Q */

#define CRIT_WARMUP_S   3.0     /* Criterion default warm_up_time               */
#define CRIT_MEAS_S    60.0     /* .measurement_time(Duration::from_secs(60))   */
#define CRIT_SAMPLES   300      /* .sample_size(300)                            */
#define CRIT_RESAMPLES 100000   /* Criterion default bootstrap resample count   */

static double now_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

/* xorshift64* PRNG for the bootstrap (measurement post-processing only). */
static uint64_t rng_state = 0x4c41534245ULL;   /* "LASBE" */
static uint64_t rng_next(void) {
  uint64_t x = rng_state;
  x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
  rng_state = x;
  return x * 0x2545F4914F6CDD1DULL;
}

static double g_samp[CRIT_SAMPLES];   /* per-iteration us, sample value i      */
static double g_x[CRIT_SAMPLES];      /* iterations in sample i                */
static double g_y[CRIT_SAMPLES];      /* elapsed us of sample i                */
static volatile long g_sink;

typedef struct {
  unsigned long d, total_iters, warm_iters;
  double mean, sd, median, mad, min, max, slope;
  double ci_lo, ci_hi;                /* bootstrap 95% CI of the mean          */
} crit_result;

static int cmp_d(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b;
  return (x > y) - (x < y);
}

static double median_of(double *sorted, int n) {
  return (n & 1) ? sorted[n/2] : 0.5 * (sorted[n/2 - 1] + sorted[n/2]);
}

static void crit_stats(crit_result *r) {
  double buf[CRIT_SAMPLES], dev[CRIT_SAMPLES];
  double s = 0.0, v = 0.0, sxy = 0.0, sxx = 0.0;
  int i, k;

  for(i = 0; i < CRIT_SAMPLES; ++i) s += g_samp[i];
  r->mean = s / CRIT_SAMPLES;
  for(i = 0; i < CRIT_SAMPLES; ++i) {
    double dd = g_samp[i] - r->mean;
    v += dd * dd;
  }
  r->sd = sqrt(v / (CRIT_SAMPLES - 1));

  memcpy(buf, g_samp, sizeof buf);
  qsort(buf, CRIT_SAMPLES, sizeof buf[0], cmp_d);
  r->min = buf[0];
  r->max = buf[CRIT_SAMPLES - 1];
  r->median = median_of(buf, CRIT_SAMPLES);
  for(i = 0; i < CRIT_SAMPLES; ++i) dev[i] = fabs(g_samp[i] - r->median);
  qsort(dev, CRIT_SAMPLES, sizeof dev[0], cmp_d);
  r->mad = median_of(dev, CRIT_SAMPLES);

  /* Criterion's regression estimate: least-squares slope through the origin
   * of (iterations, elapsed) -- us per iteration. */
  for(i = 0; i < CRIT_SAMPLES; ++i) { sxy += g_x[i] * g_y[i]; sxx += g_x[i] * g_x[i]; }
  r->slope = sxy / sxx;

  /* percentile bootstrap, 95% CI of the mean (CRIT_RESAMPLES resamples) */
  {
    static double means[CRIT_RESAMPLES];
    for(k = 0; k < CRIT_RESAMPLES; ++k) {
      double m = 0.0;
      for(i = 0; i < CRIT_SAMPLES; ++i)
        m += g_samp[rng_next() % CRIT_SAMPLES];
      means[k] = m / CRIT_SAMPLES;
    }
    qsort(means, CRIT_RESAMPLES, sizeof means[0], cmp_d);
    r->ci_lo = means[(int)(0.025 * CRIT_RESAMPLES)];
    r->ci_hi = means[(int)(0.975 * CRIT_RESAMPLES)] ;
  }
}

static void crit_print(const char *group, const char *op, const crit_result *r) {
  printf("%s/%s\n", group, op);
  printf("  sampling: %d samples, linear ramp i*d with d=%lu (max %lu iters/sample), "
         "%lu measured iters (+%lu warm-up)\n",
         CRIT_SAMPLES, r->d, (unsigned long)CRIT_SAMPLES * r->d,
         r->total_iters, r->warm_iters);
  printf("  time:   slope %10.3f us   mean %10.3f us +/- %8.3f  "
         "(bootstrap 95%% CI of mean [%10.3f, %10.3f])\n",
         r->slope, r->mean, r->sd, r->ci_lo, r->ci_hi);
  printf("          median %9.3f us   MAD %10.3f us   min %10.3f   max %10.3f\n\n",
         r->median, r->mad, r->min, r->max);
}

/* Criterion warm-up + linear sampling around an arbitrary statement.  Variadic
 * so op bodies may contain unparenthesised commas.  Leaves the result in res_. */
#define CRIT_BENCH(res_, ...) do {                                            \
    double t0_, el_, test_, dd_;                                              \
    unsigned long wi_ = 0, batch_ = 1, it_, i_;                               \
    int si_;                                                                  \
    t0_ = now_us();                                                           \
    for(;;) {                             /* 1. warm-up, doubling batches */  \
      for(i_ = 0; i_ < batch_; ++i_) { __VA_ARGS__; }                         \
      wi_ += batch_;                                                          \
      el_ = now_us() - t0_;                                                   \
      if(el_ >= CRIT_WARMUP_S * 1e6) break;                                   \
      batch_ *= 2;                                                            \
    }                                                                         \
    test_ = el_ / (double)wi_;                                                \
    dd_ = (CRIT_MEAS_S * 1e6) /                                               \
          (test_ * 0.5 * CRIT_SAMPLES * (CRIT_SAMPLES + 1.0));                \
    (res_).d = (dd_ > 1.0) ? (unsigned long)ceil(dd_) : 1;                    \
    (res_).warm_iters = wi_;                                                  \
    (res_).total_iters = 0;                                                   \
    for(si_ = 1; si_ <= CRIT_SAMPLES; ++si_) {  /* 2. linear sampling */      \
      it_ = (unsigned long)si_ * (res_).d;                                    \
      t0_ = now_us();                                                         \
      for(i_ = 0; i_ < it_; ++i_) { __VA_ARGS__; }                            \
      el_ = now_us() - t0_;                                                   \
      g_samp[si_ - 1] = el_ / (double)it_;                                    \
      g_x[si_ - 1] = (double)it_;                                             \
      g_y[si_ - 1] = el_;                                                     \
      (res_).total_iters += it_;                                              \
    }                                                                         \
    crit_stats(&(res_));                       /* 3. statistics */            \
  } while(0)

/* Run-validity gate -- IDENTICAL check and line format to the Rust drivers
 * (benches/las_bench.rs rejection_gate): attempts/call over `calls` i.i.d.
 * geometric draws has SD = E*sqrt(1-1/E); band = 5*SD/sqrt(calls).  At this
 * driver's full call counts (warm-up + 300-sample ramp, >=100k sign-class
 * calls) the band is the tight ~+-1% version.  On failure the run aborts. */
static void rejection_gate(const char *label, unsigned long attempts,
                           unsigned long calls, double theory) {
  double measured = (double)attempts / (double)calls;
  double sigma = theory * sqrt(1.0 - 1.0/theory);
  double tol = 5.0 * sigma / sqrt((double)calls);
  int ok = fabs(measured - theory) <= tol;
  printf("rejection gate [%s]: %lu calls, measured %.4f attempts/call "
         "(acceptance %.2f%%) vs theory %.4f (%.2f%%), 5-sigma tolerance +-%.4f => %s\n",
         label, calls, measured, 100.0/measured, theory, 100.0/theory, tol,
         ok ? "OK" : "FAIL");
  if(!ok) {
    printf("FATAL: rejection gate [%s]: the rejection loop is not behaving as "
           "designed -- this run is NOT valid evidence\n", label);
    exit(1);
  }
}

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
  static const uint8_t msg[] = "bench message, thirty-three bytes";
  const uint8_t *m = msg;
  size_t  mlen = sizeof msg - 1;
  las_pp  pp;
  las_pk  pk, Y, pk2;                       /* pk2/sk2/tmp = producing-op scratch */
  las_sk  sk, yy, sk2, yext;
  las_sig sig, presig, adapted, tmp;
  crit_result r_kg, r_sg, r_vf, r_ps, r_pv, r_ad, r_ex;
  unsigned long sg_calls, ps_calls, att0;
  unsigned long sg_attempts, ps_attempts;
  unsigned int j;

  for(j = 0; j < LAS_SEEDBYTES; ++j) ppseed[j] = (uint8_t)j;

  /* ---- one consistent state, gated on the full success-path contract
   * (identical assertions to las_bench.rs) ---- */
  las_setup(&pp, ppseed);
  base_keygen(&pk, &sk, &pp);
  base_keygen(&Y, &yy, &pp);
  base_sign(&sig, m, mlen, &pk, &sk, &pp);
  las_presign(&presig, m, mlen, &Y, &pk, &sk, &pp);
  if(las_adapt(&adapted, &presig, m, mlen, &Y, &yy, &pk, &pp) != 0) {
    printf("FATAL: could not establish a valid adapted signature\n");
    return 1;
  }
  if(base_verify(&sig, m, mlen, &pk, &pp) != 0 ||          /* ordinary sig verifies */
     las_preverify(&presig, m, mlen, &Y, &pk, &pp) != 0 || /* presig pre-verifies   */
     base_verify(&presig, m, mlen, &pk, &pp) == 0 ||       /* presig FAILS Verify   */
     base_verify(&adapted, m, mlen, &pk, &pp) != 0 ||      /* adapted verifies      */
     las_ext(&yext, &adapted, &presig, &Y, &pp) != 0 ||    /* Ext succeeds          */
     !sk_equal(&yext, &yy)) {                              /* exact witness recover */
    printf("FATAL: benchmark state inconsistent before timing\n");
    return 1;
  }

  printf("==========================================================================\n");
  printf(" C mirror of the Rust Criterion benchmark (benches/las_bench.rs)\n");
  printf(" LAS parameter set: n=%d ell=%d kappa=%d gamma=%d  (N=%d, Q=%d)\n",
         LAS_N, LAS_ELL, LAS_KAPPA, LAS_GAMMA, N, Q);
  printf(" Criterion-equivalent config: warm-up %.0f s; %d samples over %.0f s,\n",
         CRIT_WARMUP_S, CRIT_SAMPLES, CRIT_MEAS_S);
  printf(" linear iteration ramp (SamplingMode::Linear); sample value = us/iter;\n");
  printf(" mean/SD/median/MAD/min/max + origin-regression slope + %d-resample\n",
         CRIT_RESAMPLES);
  printf(" bootstrap 95%% CI of the mean.  Compiler: %s.\n", __VERSION__);
  printf(" Raw times are NOT comparable with the Rust log (different compiler and\n");
  printf(" optimisation profile); compare Algorithm 1 vs Algorithm 2 ratios only.\n");
  printf(" Full run ~ 8 minutes (7 ops x ~63 s), matching the Rust run.\n");
  printf("==========================================================================\n\n");

  /* ---- Algorithm 1: the ordinary signature (basesig.c) ---- */
  CRIT_BENCH(r_kg, base_keygen(&pk2, &sk2, &pp));
  crit_print("Algorithm 1 - ordinary lattice-based signature", "KeyGen", &r_kg);

  att0 = base_attempts;
  CRIT_BENCH(r_sg, base_sign(&tmp, m, mlen, &pk, &sk, &pp));
  sg_attempts = base_attempts - att0;
  sg_calls = r_sg.warm_iters + r_sg.total_iters;
  crit_print("Algorithm 1 - ordinary lattice-based signature", "Sign", &r_sg);

  CRIT_BENCH(r_vf, g_sink += base_verify(&sig, m, mlen, &pk, &pp));
  crit_print("Algorithm 1 - ordinary lattice-based signature", "Verify", &r_vf);

  rejection_gate("Algorithm 1 Sign", sg_attempts, sg_calls,
                 las_expected_attempts(LAS_BOUND_SIGN));
  printf("\n");

  /* ---- Algorithm 2: the LAS adaptor signature (las.c) ---- */
  att0 = las_attempts;
  CRIT_BENCH(r_ps, las_presign(&tmp, m, mlen, &Y, &pk, &sk, &pp));
  ps_attempts = las_attempts - att0;
  ps_calls = r_ps.warm_iters + r_ps.total_iters;
  crit_print("Algorithm 2 - LAS adaptor signature", "PreSign", &r_ps);

  CRIT_BENCH(r_pv, g_sink += las_preverify(&presig, m, mlen, &Y, &pk, &pp));
  crit_print("Algorithm 2 - LAS adaptor signature", "PreVerify", &r_pv);

  CRIT_BENCH(r_ad, g_sink += las_adapt(&tmp, &presig, m, mlen, &Y, &yy, &pk, &pp));
  crit_print("Algorithm 2 - LAS adaptor signature",
             "Adapt (including its internal PreVerify)", &r_ad);

  CRIT_BENCH(r_ex, g_sink += las_ext(&yext, &adapted, &presig, &Y, &pp));
  crit_print("Algorithm 2 - LAS adaptor signature", "Extract", &r_ex);

  rejection_gate("Algorithm 2 PreSign", ps_attempts, ps_calls,
                 las_expected_attempts(LAS_BOUND_PRESIGN));
  printf("\n");

  /* ---- adaptor-overhead summary (mean-based, sign-class; median, verify-class) ---- */
  printf("--- Adaptor overhead (Algorithm 2 op vs the Algorithm 1 op it mirrors) ---\n");
  printf("   PreSign   vs Sign     mean   %10.3f vs %10.3f us   (%+.1f%%)\n",
         r_ps.mean, r_sg.mean, 100.0*(r_ps.mean - r_sg.mean)/r_sg.mean);
  printf("   PreVerify vs Verify   median %10.3f vs %10.3f us   (%+.1f%%)\n",
         r_pv.median, r_vf.median, 100.0*(r_pv.median - r_vf.median)/r_vf.median);
  printf("   Adapt     vs Verify   median %10.3f vs %10.3f us   (%+.1f%%)\n",
         r_ad.median, r_vf.median, 100.0*(r_ad.median - r_vf.median)/r_vf.median);
  printf("   Extract   (no Algorithm 1 analogue)  median %10.3f us\n", r_ex.median);

  return (int)(g_sink & 0);
}
