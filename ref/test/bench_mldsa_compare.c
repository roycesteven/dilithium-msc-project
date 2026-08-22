/*
 * bench_mldsa_compare.c -- head-to-head: the adaptor on NIST ML-DSA (FIPS 204
 * as specified) against the adaptor on the LAS paper's simplified Dilithium
 * (this project's implementation of record).
 *
 * WHY BOTH IN ONE BINARY
 *   The two constructions are measured in the SAME PROCESS, on the same
 *   machine, with the same compiler flags, in one run -- construction A in
 *   full, then construction B (sequentially, NOT interleaved). Comparing
 *   numbers from two separate runs would add a machine/toolchain caveat that
 *   this design removes; what it does NOT remove is drift within the run, so
 *   thermal or frequency drift between the A block and the B block remains a
 *   caveat and is stated as one.  It is possible because the two
 *   name spaces do not collide: the simplified scheme is parameterised by
 *   LAS_N/ELL/KAPPA (setup.h) and the ML-DSA scheme by K/L/GAMMA1/GAMMA2
 *   (params.h), and no symbol is shared.
 *
 * MEASUREMENT PROTOCOL -- deliberately IDENTICAL to bench_levels.c, so these
 * numbers sit on the same footing as the ones the report already quotes:
 *   - 5 outer repetitions -> mean +/- sample SD (compile-time enforced floor)
 *   - 500 inner iterations for sign-class ops, 1000 for verify-class
 *   - rejection restarts are counted DIRECTLY from the attempt counters, never
 *     inferred from a timing ratio
 *   - a run-validity rejection gate per construction; a run that drifts off
 *     theory fails loudly instead of producing a publishable-looking number
 *
 * THE TWO BASE PARTNERS FOR THE ML-DSA SIDE (both reported, on purpose)
 *   (i)  stock crypto_sign_signature -- real, unmodified ML-DSA. The honest
 *        reference for "what does ML-DSA itself cost".
 *   (ii) mldsa_las_presign(..., VBASE) -- the same instrumented loop with the
 *        statement removed. The CONTROLLED partner for the adaptor overhead,
 *        exactly as basesig.c is the controlled partner for las.c: same file,
 *        same helpers, only the statement differs.
 *   Quoting (ii) for the overhead and (i) for the absolute cost is what keeps
 *   the two questions from contaminating each other.
 *
 * WHAT THIS IS NOT
 *   Not a security comparison. The two constructions have different parameters
 *   and different security arguments, and neither is analysed here (out of
 *   scope). It is a cost comparison between two ENGINEERING routes to the same
 *   adaptor functionality.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "../params.h"
/* simplified-Dilithium LAS (the implementation of record) */
#include "../setup.h"
#include "../las_types.h"
#include "../basesig.h"
#include "../las.h"
#include "../relation.h"
#include "../serialize.h"
/* ML-DSA LAS (the experiment) */
#include "../mldsa_las.h"
#include "../sign.h"
#include "../packing.h"
#include "../randombytes.h"

#ifndef LAS_GIT_COMMIT
#define LAS_GIT_COMMIT "n/a"
#endif
#ifndef LAS_GIT_BRANCH
#define LAS_GIT_BRANCH "n/a"
#endif

#define RUNS       5
#define NITER_SIGN 500
#define NITER_FAST 1000

_Static_assert(RUNS >= 5,
  "benchmark validity requires >= 5 repetitions for a meaningful mean/SD");

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
static double g_mean, g_sd;
static double g_att_runs[RUNS];      /* per-rep us per ATTEMPT (sign-class only) */
static double g_att_mean, g_att_sd;
static unsigned long g_att_total;
static volatile long g_sink;
static int g_counter_fault = 0;      /* set by MEASURE_SIGN's sanity check     */
static int g_contract_fault = 0;     /* set by EXPECT_OK                       */

/* Every timed operation must be measured on its SUCCESS path. If a stale key or
 * a stale object makes an operation fail, its timing describes the rejection
 * path instead -- typically faster, and silently wrong. Each measurement block
 * is therefore followed by an assertion that the operation still succeeds on
 * the state it was measured against. */
#define EXPECT_OK(expr_, what_) do {                                  \
    if((expr_) != 0) {                                                \
      printf("FATAL: success-path contract broken: %s\n", (what_));   \
      g_contract_fault = 1;                                           \
    }                                                                 \
  } while(0)

#define MEASURE(niter_, ...) do {                                     \
    int br_, bi_;                                                     \
    for(br_ = 0; br_ < RUNS; ++br_) {                                 \
      double bt0_ = now_us();                                         \
      for(bi_ = 0; bi_ < (niter_); ++bi_) { __VA_ARGS__; }            \
      g_runs[br_] = (now_us() - bt0_) / (niter_);                     \
    }                                                                 \
    stats(g_runs, RUNS, &g_mean, &g_sd);                              \
  } while(0)

/* Sign-class variant, mirroring bench_levels.c: additionally totals the
 * attempt-counter delta over the TIMED calls (which the rejection gate
 * consumes) and records a per-ATTEMPT microsecond series.
 *
 * Why the per-attempt series matters here. A sign-class call costs roughly
 * (attempts x one attempt), and over 2500 calls the realised attempt count
 * still varies by a few percent between two operations. That noise is the same
 * order as the adaptor overhead being measured, so us/call alone can flip the
 * SIGN of a real 7% effect. us/attempt divides the rejection luck out and
 * isolates the algorithmic cost. Both are reported: us/call is what a caller
 * pays, us/attempt is what the algorithm costs. */
#define MEASURE_SIGN(counter_, niter_, ...) do {                      \
    int br_, bi_;                                                     \
    g_att_total = 0;                                                  \
    for(br_ = 0; br_ < RUNS; ++br_) {                                 \
      unsigned long a0_ = (counter_), da_;                            \
      double bt0_ = now_us(), el_;                                    \
      for(bi_ = 0; bi_ < (niter_); ++bi_) { __VA_ARGS__; }            \
      el_ = now_us() - bt0_;                                          \
      da_ = (counter_) - a0_;                                         \
      /* Every call must make at least one attempt, so a delta below the \
       * iteration count means the counter is not wired to the loop being \
       * timed -- a silent measurement fault, not a slow run. */       \
      if(da_ < (unsigned long)(niter_)) {                             \
        printf("FATAL: attempt counter delta %lu < %d timed calls -- the "\
               "counter is not tracking this loop.\n", da_, (niter_)); \
        g_counter_fault = 1;                                          \
      }                                                               \
      g_runs[br_] = el_ / (niter_);                                   \
      g_att_runs[br_] = da_ ? el_ / (double)da_ : 0.0;                \
      g_att_total += da_;                                             \
    }                                                                 \
    stats(g_runs, RUNS, &g_mean, &g_sd);                              \
    stats(g_att_runs, RUNS, &g_att_mean, &g_att_sd);                  \
  } while(0)

/* PAIRED, INTERLEAVED ratio measurement -- how the overhead figures are made.
 *
 * Measuring Sign in one block and PreSign in another block minutes later lets
 * CPU frequency drift land entirely on one of them. On this machine that
 * produced overheads that bounced between -3% and +8% across runs, and even
 * came out NEGATIVE, which is not a plausible reading of "the adaptor adds
 * work". Alternating the two operations WITHIN each repetition makes drift
 * common-mode: the per-repetition ratio is taken while both were running under
 * the same conditions, and the 5 ratios then give a mean and an SD.
 *
 * The overhead figures this driver reports are therefore ratios-of-pairs, not
 * quotients of two independently reported means. */
#define PAIR_RATIO_ATT(niter_, cA_, aBody_, cB_, bBody_, outm_, outs_) do {  \
    int pr_, pi_; double rs_[RUNS];                                          \
    for(pr_ = 0; pr_ < RUNS; ++pr_) {                                        \
      unsigned long a0_ = (cA_), da_, b0_, db_;                              \
      double t0_ = now_us(), ta_, t1_, tb_;                                  \
      for(pi_ = 0; pi_ < (niter_); ++pi_) { aBody_; }                        \
      ta_ = now_us() - t0_; da_ = (cA_) - a0_;                               \
      b0_ = (cB_); t1_ = now_us();                                           \
      for(pi_ = 0; pi_ < (niter_); ++pi_) { bBody_; }                        \
      tb_ = now_us() - t1_; db_ = (cB_) - b0_;                               \
      rs_[pr_] = (da_ && db_) ? (tb_ / (double)db_) / (ta_ / (double)da_)    \
                              : 0.0;                                         \
    }                                                                        \
    stats(rs_, RUNS, (outm_), (outs_));                                      \
  } while(0)

#define PAIR_RATIO(niter_, aBody_, bBody_, outm_, outs_) do {                \
    int pr_, pi_; double rs_[RUNS];                                          \
    for(pr_ = 0; pr_ < RUNS; ++pr_) {                                        \
      double t0_ = now_us(), ta_, t1_, tb_;                                  \
      for(pi_ = 0; pi_ < (niter_); ++pi_) { aBody_; }                        \
      ta_ = now_us() - t0_;                                                  \
      t1_ = now_us();                                                        \
      for(pi_ = 0; pi_ < (niter_); ++pi_) { bBody_; }                        \
      tb_ = now_us() - t1_;                                                  \
      rs_[pr_] = ta_ > 0.0 ? tb_ / ta_ : 0.0;                                \
    }                                                                        \
    stats(rs_, RUNS, (outm_), (outs_));                                      \
  } while(0)

static int g_gate_failed = 0;

/* Run-validity gate, same 5-sigma check and line format as bench_levels.c.
 * `theory` is the exact expectation where one exists (the simplified scheme,
 * via las_expected_attempts) and FIPS 204's published repetition rate where it
 * does not (ML-DSA, whose three rejection conditions have no closed form here);
 * the source of `theory` is printed so the two cases are never confused. */
static void rejection_gate(const char *label, const char *theory_src,
                           unsigned long attempts, unsigned long calls,
                           double theory)
{
  double measured = (double)attempts / (double)calls;
  double sigma = theory * sqrt(1.0 - 1.0/theory);
  double tol = 5.0 * sigma / sqrt((double)calls);
  int ok = fabs(measured - theory) <= tol;
  printf("rejection gate [%s]: %lu calls, measured %.4f attempts/call "
         "(acceptance %.2f%%) vs %s %.4f (%.2f%%), 5-sigma tolerance +-%.4f => %s\n",
         label, calls, measured, 100.0/measured, theory_src, theory,
         100.0/theory, tol, ok ? "OK" : "FAIL");
  if(!ok) {
    printf("FATAL: rejection gate [%s]: the rejection loop is not behaving as "
           "predicted; the timings from this run must not be published.\n", label);
    g_gate_failed = 1;
  }
}

/* FIPS 204's published expected repetition counts (Table 1 / the standard's own
 * analysis), used as the ML-DSA gate's reference because the three-condition
 * rejection has no single closed form to evaluate here. */
static double mldsa_expected_repetitions(void)
{
#if DILITHIUM_MODE == 2
  return 4.25;
#elif DILITHIUM_MODE == 3
  return 5.1;
#else
  return 3.85;
#endif
}

/* Name of the SIMPLIFIED parameter set, keyed on (LAS_N, ELL, KAPPA) together --
 * dimensions alone cannot separate the D2-aligned set from the historical paper
 * set, which share (4,4).  Same keying rule as LAS_CTILDEBYTES in setup.h, and
 * an unrecognised set is a build error rather than a mislabelled column. */
static const char *simp_level_name(void)
{
#if   LAS_N == 4 && ELL == 4 && KAPPA == 39
  return "Simplified Dilithium-II";
#elif LAS_N == 6 && ELL == 5 && KAPPA == 49
  return "Simplified Dilithium-III";
#elif LAS_N == 8 && ELL == 7 && KAPPA == 60
  return "Simplified Dilithium-V";
#elif LAS_N == 4 && ELL == 4 && KAPPA == 60
  return "Simplified Dilithium (paper set)";
#else
#error "unrecognised (LAS_N, ELL, KAPPA): add it to simp_level_name() rather than mislabelling the column"
#endif
}

static const char *mldsa_level_name(void)
{
#if DILITHIUM_MODE == 2
  return "ML-DSA-44";
#elif DILITHIUM_MODE == 3
  return "ML-DSA-65";
#else
  return "ML-DSA-87";
#endif
}

struct row {
  const char *op;
  double simp_mean, simp_sd;      /* simplified-Dilithium LAS */
  double mldsa_mean, mldsa_sd;    /* ML-DSA LAS               */
  int have_simp, have_mldsa;
};

#define NROW 7
static struct row rows[NROW] = {
  {"KeyGen",    0,0,0,0,0,0}, {"Sign",      0,0,0,0,0,0},
  {"Verify",    0,0,0,0,0,0}, {"PreSign",   0,0,0,0,0,0},
  {"PreVerify", 0,0,0,0,0,0}, {"Adapt",     0,0,0,0,0,0},
  {"Extract",   0,0,0,0,0,0},
};

static void set_simp(int i)  { rows[i].simp_mean = g_mean;  rows[i].simp_sd = g_sd;  rows[i].have_simp = 1; }
static void set_mldsa(int i) { rows[i].mldsa_mean = g_mean; rows[i].mldsa_sd = g_sd; rows[i].have_mldsa = 1; }

int main(void)
{
  /* ---- simplified-Dilithium LAS state ---- */
  public_params pp;
  uint8_t pp_seed[LAS_SEEDBYTES];
  public_key  s_pk;
  secret_key  s_sk;
  statement   s_Y;
  witness     s_wit, s_ext;
  signature   s_sig;
  pre_signature s_presig;

  /* ---- ML-DSA LAS state ---- */
  uint8_t m_pk[CRYPTO_PUBLICKEYBYTES], m_sk[CRYPTO_SECRETKEYBYTES];
  uint8_t m_presig[CRYPTO_BYTES], m_sig[CRYPTO_BYTES], m_stock[CRYPTO_BYTES];
  uint8_t m_rho[SEEDBYTES], m_seed[CRHBYTES], m_rnd[RNDBYTES];
  mldsa_statement m_Y;
  mldsa_witness   m_wit, m_ext;
  polyveck m_t1_unused;
  size_t m_siglen;

  uint8_t msg[59];
  double base_mean_stock = 0, base_sd_stock = 0;
  double base_mean_matched = 0, base_sd_matched = 0;
  unsigned long simp_sign_att, simp_presign_att;
  unsigned long mldsa_base_att, mldsa_presign_att;
  /* per-ATTEMPT microsecond means (rejection luck divided out) */
  double simp_sign_pa = 0, simp_sign_pa_sd = 0;
  double simp_presign_pa = 0, simp_presign_pa_sd = 0;
  double mldsa_sign_pa = 0, mldsa_sign_pa_sd = 0;
  double mldsa_presign_pa = 0, mldsa_presign_pa_sd = 0;
  /* paired, interleaved overhead ratios (drift common-mode) */
  double sp_ps = 0, sp_ps_sd = 0, sp_pv = 0, sp_pv_sd = 0, sp_ad = 0, sp_ad_sd = 0;
  double mp_ps = 0, mp_ps_sd = 0, mp_pv = 0, mp_pv_sd = 0, mp_ad = 0, mp_ad_sd = 0;
  int i;

  printf("=== Adaptor cost: NIST ML-DSA vs the LAS paper's simplified Dilithium ===\n");
  printf("build: commit %s branch %s | %s vs %s "
         "(n=%d, l=%d, kappa=%d)\n",
         LAS_GIT_COMMIT, LAS_GIT_BRANCH, mldsa_level_name(), simp_level_name(),
         LAS_N, ELL, KAPPA);
  printf("protocol: %d repetitions x %d (sign-class) / %d (verify-class) "
         "iterations, mean +/- SD, same process\n\n", RUNS, NITER_SIGN, NITER_FAST);

  randombytes(msg, sizeof(msg));
  randombytes(m_seed, sizeof(m_seed));   /* needed by the warm-up below */

  /* ================= construction A: simplified-Dilithium LAS ============= */
  randombytes(pp_seed, sizeof(pp_seed));
  setup_public_params(&pp, pp_seed);
  if(base_keygen(&s_pk, &s_sk, &pp)) { printf("FATAL: base_keygen\n"); return 1; }
  if(relation_gen(&s_Y, &s_wit, &pp))  { printf("FATAL: relation_gen\n"); return 1; }
  if(base_sign(&s_sig, msg, sizeof(msg), &s_pk, &s_sk, &pp)) { printf("FATAL: base_sign\n"); return 1; }
  if(base_verify(&s_sig, msg, sizeof(msg), &s_pk, &pp)) { printf("FATAL: base_verify\n"); return 1; }
  if(las_presign(&s_presig, msg, sizeof(msg), &s_Y, &s_pk, &s_sk, &pp)) { printf("FATAL: las_presign\n"); return 1; }
  if(las_preverify(&s_presig, msg, sizeof(msg), &s_Y, &s_pk, &pp)) { printf("FATAL: las_preverify\n"); return 1; }
  if(las_adapt(&s_sig, &s_presig, msg, sizeof(msg), &s_Y, &s_wit, &s_pk, &pp)) { printf("FATAL: las_adapt\n"); return 1; }
  if(base_verify(&s_sig, msg, sizeof(msg), &s_pk, &pp)) { printf("FATAL: adapted sig must verify\n"); return 1; }
  if(las_ext(&s_ext, &s_sig, &s_presig, &s_Y, &pp)) { printf("FATAL: las_ext\n"); return 1; }

  /* WARM-UP (untimed). The first measured block of the process pays for cold
   * caches and for the CPU still ramping its clock, which showed up as a ~10%
   * sample SD on the first operation measured -- large enough to swamp the
   * few-percent adaptor overhead this driver exists to report. Both
   * constructions are warmed before either is timed, so neither is advantaged
   * by running first. */
  for(i = 0; i < 200; ++i) {
    g_sink += base_sign(&s_sig, msg, sizeof(msg), &s_pk, &s_sk, &pp);
    g_sink += las_presign(&s_presig, msg, sizeof(msg), &s_Y, &s_pk, &s_sk, &pp);
  }
  {
    uint8_t w_pk[CRYPTO_PUBLICKEYBYTES], w_sk[CRYPTO_SECRETKEYBYTES];
    uint8_t w_sig[CRYPTO_BYTES], w_rnd[RNDBYTES];
    uint8_t w_rho[SEEDBYTES];
    polyveck w_t1;
    mldsa_statement w_Y;
    mldsa_witness w_y;
    size_t w_len;
    crypto_sign_keypair(w_pk, w_sk);
    unpack_pk(w_rho, &w_t1, w_pk);
    mldsa_las_gen(&w_Y, &w_y, w_rho, m_seed, 0);
    for(i = 0; i < 200; ++i) {
      randombytes(w_rnd, RNDBYTES);
      g_sink += crypto_sign_signature(w_sig, &w_len, msg, sizeof(msg), NULL, 0, w_sk);
      g_sink += mldsa_las_presign(w_sig, msg, sizeof(msg), &w_Y, w_rnd, w_sk,
                                  MLDSA_LAS_V1_SHIFTED);
    }
  }

  MEASURE(NITER_FAST, g_sink += base_keygen(&s_pk, &s_sk, &pp));
  set_simp(0);
  /* regenerate a coherent state: the key just changed */
  (void)relation_gen(&s_Y, &s_wit, &pp);

  MEASURE_SIGN(base_attempts, NITER_SIGN,
               g_sink += base_sign(&s_sig, msg, sizeof(msg), &s_pk, &s_sk, &pp));
  set_simp(1);
  simp_sign_att = g_att_total;
  simp_sign_pa = g_att_mean; simp_sign_pa_sd = g_att_sd;

  EXPECT_OK(base_verify(&s_sig, msg, sizeof(msg), &s_pk, &pp),
            "simplified: signature from the timed Sign loop must verify");
  MEASURE(NITER_FAST, g_sink += base_verify(&s_sig, msg, sizeof(msg), &s_pk, &pp));
  set_simp(2);

  MEASURE_SIGN(las_attempts, NITER_SIGN,
               g_sink += las_presign(&s_presig, msg, sizeof(msg), &s_Y, &s_pk, &s_sk, &pp));
  set_simp(3);
  simp_presign_att = g_att_total;
  simp_presign_pa = g_att_mean; simp_presign_pa_sd = g_att_sd;

  EXPECT_OK(las_preverify(&s_presig, msg, sizeof(msg), &s_Y, &s_pk, &pp),
            "simplified: pre-signature from the timed PreSign loop must pre-verify");
  MEASURE(NITER_FAST, g_sink += las_preverify(&s_presig, msg, sizeof(msg), &s_Y, &s_pk, &pp));
  set_simp(4);

  MEASURE(NITER_FAST, g_sink += las_adapt(&s_sig, &s_presig, msg, sizeof(msg),
                                          &s_Y, &s_wit, &s_pk, &pp));
  set_simp(5);

  EXPECT_OK(base_verify(&s_sig, msg, sizeof(msg), &s_pk, &pp),
            "simplified: the ADAPTED signature must verify under the base verifier");
  MEASURE(NITER_FAST, g_sink += las_ext(&s_ext, &s_sig, &s_presig, &s_Y, &pp));
  set_simp(6);
  EXPECT_OK(las_ext(&s_ext, &s_sig, &s_presig, &s_Y, &pp),
            "simplified: Ext must recover a valid witness");

  /* paired overhead ratios for construction A */
  PAIR_RATIO_ATT(NITER_SIGN,
                 base_attempts,
                 g_sink += base_sign(&s_sig, msg, sizeof(msg), &s_pk, &s_sk, &pp),
                 las_attempts,
                 g_sink += las_presign(&s_presig, msg, sizeof(msg), &s_Y, &s_pk,
                                       &s_sk, &pp),
                 &sp_ps, &sp_ps_sd);
  EXPECT_OK(las_preverify(&s_presig, msg, sizeof(msg), &s_Y, &s_pk, &pp),
            "simplified: paired PreSign must still leave a valid pre-signature");
  PAIR_RATIO(NITER_FAST,
             g_sink += base_verify(&s_sig, msg, sizeof(msg), &s_pk, &pp),
             g_sink += las_preverify(&s_presig, msg, sizeof(msg), &s_Y, &s_pk, &pp),
             &sp_pv, &sp_pv_sd);
  PAIR_RATIO(NITER_FAST,
             g_sink += base_verify(&s_sig, msg, sizeof(msg), &s_pk, &pp),
             g_sink += las_adapt(&s_sig, &s_presig, msg, sizeof(msg), &s_Y,
                                 &s_wit, &s_pk, &pp),
             &sp_ad, &sp_ad_sd);

  /* ================= construction B: ML-DSA LAS =========================== */
  crypto_sign_keypair(m_pk, m_sk);
  unpack_pk(m_rho, &m_t1_unused, m_pk);
  randombytes(m_rnd, sizeof(m_rnd));
  mldsa_las_gen(&m_Y, &m_wit, m_rho, m_seed, 0);
  if(mldsa_las_presign(m_presig, msg, sizeof(msg), &m_Y, m_rnd, m_sk,
                       MLDSA_LAS_V1_SHIFTED)) { printf("FATAL: mldsa presign\n"); return 1; }
  if(mldsa_las_preverify(m_presig, msg, sizeof(msg), &m_Y, m_pk)) { printf("FATAL: mldsa preverify\n"); return 1; }
  if(mldsa_las_adapt(m_sig, m_presig, msg, sizeof(msg), &m_Y, &m_wit, m_pk)) { printf("FATAL: mldsa adapt\n"); return 1; }
  if(crypto_sign_verify(m_sig, CRYPTO_BYTES, msg, sizeof(msg), NULL, 0, m_pk)) {
    printf("FATAL: adapted ML-DSA signature must verify under the STOCK verifier\n"); return 1; }
  if(mldsa_las_ext(&m_ext, m_sig, m_presig, &m_Y, m_pk)) { printf("FATAL: mldsa ext\n"); return 1; }

  MEASURE(NITER_FAST, g_sink += crypto_sign_keypair(m_pk, m_sk));
  set_mldsa(0);

  /* STATE COHERENCE after the KeyGen benchmark.
   * The loop above replaced (m_pk, m_sk) NITER_FAST x RUNS times, so every
   * object derived from the old key -- the statement, the pre-signature and the
   * adapted signature -- is now stale. Timing crypto_sign_verify on a stale
   * m_sig would measure the REJECTION path, not verification, and would
   * silently understate it. Rebuild the whole chain against the surviving key
   * and re-assert the success-path contract before anything else is timed
   * (bench_levels.c takes the same posture). */
  unpack_pk(m_rho, &m_t1_unused, m_pk);
  mldsa_las_gen(&m_Y, &m_wit, m_rho, m_seed, 0);
  randombytes(m_rnd, sizeof(m_rnd));
  if(mldsa_las_presign(m_presig, msg, sizeof(msg), &m_Y, m_rnd, m_sk,
                       MLDSA_LAS_V1_SHIFTED) ||
     mldsa_las_adapt(m_sig, m_presig, msg, sizeof(msg), &m_Y, &m_wit, m_pk) ||
     crypto_sign_verify(m_sig, CRYPTO_BYTES, msg, sizeof(msg), NULL, 0, m_pk)) {
    printf("FATAL: could not rebuild a coherent ML-DSA state after KeyGen\n");
    return 1;
  }

  /* FRESH RANDOMNESS PER CALL -- this is not incidental.
   * ML-DSA's rejection loop is driven by rhoprime = CRH(key, rnd, mu). With a
   * fixed (sk, rnd, M) every call restarts the same number of times, so the
   * loop is DETERMINISTIC and its timing describes one draw rather than the
   * rejection distribution. The simplified scheme does not have this problem
   * because base_sign/las_presign sample their mask seed internally. Drawing
   * rnd per call restores the comparison, and it costs the same 32-byte
   * randombytes call that crypto_sign_signature makes internally when built
   * with DILITHIUM_RANDOMIZED_SIGNING (which this target is). The earlier
   * version of this driver got exactly 4.0000 and 2.0000 attempts/call, and
   * the rejection gate is what caught it. */

  /* base partner (i): stock, unmodified ML-DSA */
  MEASURE(NITER_SIGN, g_sink += crypto_sign_signature(m_stock, &m_siglen, msg,
                                                      sizeof(msg), NULL, 0, m_sk));
  base_mean_stock = g_mean; base_sd_stock = g_sd;

  /* base partner (ii): the matched loop, statement removed -- the controlled
   * partner, and the one the adaptor overhead is computed against */
  MEASURE_SIGN(mldsa_las_attempts, NITER_SIGN,
               randombytes(m_rnd, RNDBYTES);
               g_sink += mldsa_las_presign(m_stock, msg, sizeof(msg), &m_Y, m_rnd,
                                           m_sk, MLDSA_LAS_VBASE));
  base_mean_matched = g_mean; base_sd_matched = g_sd;
  mldsa_base_att = g_att_total;
  mldsa_sign_pa = g_att_mean; mldsa_sign_pa_sd = g_att_sd;
  set_mldsa(1);                      /* the table's Sign row = matched partner */
  rows[1].mldsa_mean = base_mean_matched;
  rows[1].mldsa_sd   = base_sd_matched;

  EXPECT_OK(crypto_sign_verify(m_stock, CRYPTO_BYTES, msg, sizeof(msg), NULL, 0, m_pk),
            "ML-DSA: signature from the timed matched-Sign loop must verify");
  EXPECT_OK(crypto_sign_verify(m_sig, CRYPTO_BYTES, msg, sizeof(msg), NULL, 0, m_pk),
            "ML-DSA: the signature Verify is timed on must be valid");
  MEASURE(NITER_FAST, g_sink += crypto_sign_verify(m_sig, CRYPTO_BYTES, msg,
                                                   sizeof(msg), NULL, 0, m_pk));
  set_mldsa(2);

  MEASURE_SIGN(mldsa_las_attempts, NITER_SIGN,
               randombytes(m_rnd, RNDBYTES);
               g_sink += mldsa_las_presign(m_presig, msg, sizeof(msg), &m_Y, m_rnd,
                                           m_sk, MLDSA_LAS_V1_SHIFTED));
  set_mldsa(3);
  mldsa_presign_att = g_att_total;
  mldsa_presign_pa = g_att_mean; mldsa_presign_pa_sd = g_att_sd;

  EXPECT_OK(mldsa_las_preverify(m_presig, msg, sizeof(msg), &m_Y, m_pk),
            "ML-DSA: pre-signature from the timed PreSign loop must pre-verify");
  MEASURE(NITER_FAST, g_sink += mldsa_las_preverify(m_presig, msg, sizeof(msg),
                                                    &m_Y, m_pk));
  set_mldsa(4);

  MEASURE(NITER_FAST, g_sink += mldsa_las_adapt(m_sig, m_presig, msg, sizeof(msg),
                                                &m_Y, &m_wit, m_pk));
  set_mldsa(5);

  EXPECT_OK(crypto_sign_verify(m_sig, CRYPTO_BYTES, msg, sizeof(msg), NULL, 0, m_pk),
            "ML-DSA: the ADAPTED signature must verify under the STOCK verifier");
  MEASURE(NITER_FAST, g_sink += mldsa_las_ext(&m_ext, m_sig, m_presig, &m_Y, m_pk));
  set_mldsa(6);
  EXPECT_OK(mldsa_las_ext(&m_ext, m_sig, m_presig, &m_Y, m_pk),
            "ML-DSA: Ext must recover a valid witness");

  /* paired overhead ratios for construction B */
  PAIR_RATIO_ATT(NITER_SIGN,
                 mldsa_las_attempts,
                 randombytes(m_rnd, RNDBYTES);
                 g_sink += mldsa_las_presign(m_stock, msg, sizeof(msg), &m_Y,
                                             m_rnd, m_sk, MLDSA_LAS_VBASE),
                 mldsa_las_attempts,
                 randombytes(m_rnd, RNDBYTES);
                 g_sink += mldsa_las_presign(m_presig, msg, sizeof(msg), &m_Y,
                                             m_rnd, m_sk, MLDSA_LAS_V1_SHIFTED),
                 &mp_ps, &mp_ps_sd);
  EXPECT_OK(mldsa_las_preverify(m_presig, msg, sizeof(msg), &m_Y, m_pk),
            "ML-DSA: paired PreSign must still leave a valid pre-signature");
  PAIR_RATIO(NITER_FAST,
             g_sink += crypto_sign_verify(m_sig, CRYPTO_BYTES, msg, sizeof(msg),
                                          NULL, 0, m_pk),
             g_sink += mldsa_las_preverify(m_presig, msg, sizeof(msg), &m_Y, m_pk),
             &mp_pv, &mp_pv_sd);
  PAIR_RATIO(NITER_FAST,
             g_sink += crypto_sign_verify(m_sig, CRYPTO_BYTES, msg, sizeof(msg),
                                          NULL, 0, m_pk),
             g_sink += mldsa_las_adapt(m_sig, m_presig, msg, sizeof(msg), &m_Y,
                                       &m_wit, m_pk),
             &mp_ad, &mp_ad_sd);

  /* ================= run-validity gates BEFORE any number is read ========= */
  printf("--- run-validity gates (a failure invalidates every timing above) ---\n");
  rejection_gate("simplified Dilithium: Algorithm 1 Sign", "exact theory",
                 simp_sign_att, (unsigned long)RUNS * NITER_SIGN,
                 las_expected_attempts(BOUND_SIGN));
  rejection_gate("simplified Dilithium: Algorithm 2 PreSign", "exact theory",
                 simp_presign_att, (unsigned long)RUNS * NITER_SIGN,
                 las_expected_attempts(BOUND_PRESIGN));
  rejection_gate("ML-DSA: Sign (matched, no statement)", "FIPS 204 published",
                 mldsa_base_att, (unsigned long)RUNS * NITER_SIGN,
                 mldsa_expected_repetitions());
  rejection_gate("ML-DSA: PreSign (adaptor)", "FIPS 204 published",
                 mldsa_presign_att, (unsigned long)RUNS * NITER_SIGN,
                 mldsa_expected_repetitions());

  /* ================= results ============================================= */
  printf("\n--- per-operation timing (us/op, mean +/- SD over %d repetitions) ---\n", RUNS);
  printf("  %-10s %22s %22s %10s\n", "operation",
         simp_level_name(), mldsa_level_name(), "ML-DSA/simp");
  for(i = 0; i < NROW; ++i) {
    printf("  %-10s %14.2f +/- %5.2f %14.2f +/- %5.2f %9.2fx\n",
           rows[i].op, rows[i].simp_mean, rows[i].simp_sd,
           rows[i].mldsa_mean, rows[i].mldsa_sd,
           rows[i].simp_mean > 0 ? rows[i].mldsa_mean / rows[i].simp_mean : 0.0);
  }
  printf("  (ML-DSA Sign row is the MATCHED partner; stock crypto_sign_signature\n"
         "   measured %.2f +/- %.2f us in the same run)\n",
         base_mean_stock, base_sd_stock);

  printf("\n--- sign-class cost per ATTEMPT (rejection luck divided out) ---\n");
  printf("  %-24s %26s %20s\n", "", simp_level_name(), mldsa_level_name());
  printf("  %-24s %17.2f +/- %5.2f %11.2f +/- %5.2f\n", "Sign, us/attempt",
         simp_sign_pa, simp_sign_pa_sd, mldsa_sign_pa, mldsa_sign_pa_sd);
  printf("  %-24s %17.2f +/- %5.2f %11.2f +/- %5.2f\n", "PreSign, us/attempt",
         simp_presign_pa, simp_presign_pa_sd, mldsa_presign_pa, mldsa_presign_pa_sd);

  printf("\n--- adaptor overhead WITHIN each construction (the primary comparison) ---\n");
  printf("    PAIRED and INTERLEAVED: each pair alternates within every repetition,\n");
  printf("    so clock drift is common-mode; mean +/- SD over the %d per-rep ratios.\n", RUNS);
  printf("  %-26s %22s %22s\n", "pair", simp_level_name(), mldsa_level_name());
  printf("  %-26s %15.1f%% +/- %4.1f %15.1f%% +/- %4.1f\n",
         "PreSign vs Sign (/attempt)",
         100.0 * (sp_ps - 1.0), 100.0 * sp_ps_sd,
         100.0 * (mp_ps - 1.0), 100.0 * mp_ps_sd);
  printf("  %-26s %15.1f%% +/- %4.1f %15.1f%% +/- %4.1f\n", "PreVerify vs Verify",
         100.0 * (sp_pv - 1.0), 100.0 * sp_pv_sd,
         100.0 * (mp_pv - 1.0), 100.0 * mp_pv_sd);
  printf("  %-26s %15.1f%% +/- %4.1f %15.1f%% +/- %4.1f\n", "Adapt vs Verify",
         100.0 * (sp_ad - 1.0), 100.0 * sp_ad_sd,
         100.0 * (mp_ad - 1.0), 100.0 * mp_ad_sd);
  printf("  (Extract has no basic analogue in either construction. PreSign is\n"
         "   compared per ATTEMPT, so the realised restart counts cannot bias it.)\n");

  printf("\n--- rejection sampling (counted directly, never inferred) ---\n");
  printf("  %-24s %26s %20s\n", "", simp_level_name(), mldsa_level_name());
  printf("  %-24s %26.4f %20.4f\n", "attempts/Sign",
         (double)simp_sign_att / (RUNS * NITER_SIGN),
         (double)mldsa_base_att / (RUNS * NITER_SIGN));
  printf("  %-24s %26.4f %20.4f\n", "attempts/PreSign",
         (double)simp_presign_att / (RUNS * NITER_SIGN),
         (double)mldsa_presign_att / (RUNS * NITER_SIGN));

  printf("\n--- communication (bytes on the wire) ---\n");
  printf("  %-24s %26s %20s %10s\n", "object", simp_level_name(),
         mldsa_level_name(), "ratio");
  printf("  %-24s %26d %20d %9.2fx\n", "public key",
         PUBLIC_KEY_BYTES, MLDSA_LAS_PUBLICKEY_BYTES,
         (double)MLDSA_LAS_PUBLICKEY_BYTES / PUBLIC_KEY_BYTES);
  printf("  %-24s %26d %20d %9.2fx\n", "signature",
         SIGNATURE_BYTES, MLDSA_LAS_SIGNATURE_BYTES,
         (double)MLDSA_LAS_SIGNATURE_BYTES / SIGNATURE_BYTES);
  printf("  %-24s %26d %20d %9.2fx\n", "pre-signature",
         PRE_SIGNATURE_BYTES, MLDSA_LAS_PRE_SIGNATURE_BYTES,
         (double)MLDSA_LAS_PRE_SIGNATURE_BYTES / PRE_SIGNATURE_BYTES);
  printf("  %-24s %26d %20d %9.2fx\n", "statement Y",
         STATEMENT_BYTES, MLDSA_LAS_STATEMENT_BYTES,
         (double)MLDSA_LAS_STATEMENT_BYTES / STATEMENT_BYTES);
  printf("  %-24s %26d %20d %9.2fx\n", "witness",
         WITNESS_BYTES, MLDSA_LAS_WITNESS_BYTES,
         (double)MLDSA_LAS_WITNESS_BYTES / WITNESS_BYTES);
  printf("  %-24s %26d %20d %9.2fx\n", "swap payload (sig + Y)",
         SIGNATURE_BYTES + STATEMENT_BYTES,
         MLDSA_LAS_SIGNATURE_BYTES + MLDSA_LAS_STATEMENT_BYTES,
         (double)(MLDSA_LAS_SIGNATURE_BYTES + MLDSA_LAS_STATEMENT_BYTES)
           / (SIGNATURE_BYTES + STATEMENT_BYTES));

  printf("\nNOTE: this is a COST comparison between two engineering routes to the\n");
  printf("same adaptor functionality. It is NOT a security comparison: the two\n");
  printf("constructions have different parameters and different security\n");
  printf("arguments, and neither is analysed here (out of scope).\n");

  if(g_gate_failed || g_counter_fault || g_contract_fault) {
    printf("\nFAIL: %s%s%s -- these timings must not be published.\n",
           g_gate_failed    ? "a rejection gate failed; " : "",
           g_counter_fault  ? "an attempt counter was not tracking its loop; " : "",
           g_contract_fault ? "a timed operation was not on its success path; " : "");
    return 1;
  }
  printf("\nOK: rejection gates, attempt-counter checks and success-path\n");
  printf("    assertions all passed. Caveat: the two constructions are measured\n");
  printf("    sequentially within one process, so intra-run drift is not\n");
  printf("    controlled for.\n");
  return 0;
}
