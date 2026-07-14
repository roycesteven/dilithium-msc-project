/*
 * bench_levels.c  --  the single combined LAS benchmark driver.
 *
 *   It produces BOTH the primary protocol-level timings AND the diagnostic
 *   sections from ONE executable and ONE shared benchmark state, so the two can
 *   never drift apart or be measured under different noise / different keys.
 *
 *   PRIMARY (protocol-level) timings -- the headline comparison the supervisor
 *   asked for: the SEPARATE simplified Dilithium-style BASE signature path
 *   vs the LAS ADAPTOR path, at matched parameters, on the same primitives.
 *   This is a primary algorithm-matched, parameter-matched adaptor-overhead
 *   benchmark; it is NOT a vague "LAS vs its own base", and it makes no formal
 *   same-security or NIST-equivalence claim (security proofs are out of scope).
 *
 *   The two paths live in two SEPARATE modules so neither contaminates the other:
 *       BASE path     -> basesig.c  (base_keygen / base_sign / base_verify; no Y)
 *       ADAPTOR path  -> las.c      (las_presign / las_preverify / las_adapt / las_ext)
 *
 *   BOTH API tiers are timed for every primary operation, and every primary line
 *   reports wall-clock microseconds AND cycles (upstream test/cpucycles.h rdtsc):
 *       TIER 1, CORE CRYPTO : the struct API (public_key/secret_key/signature + statement/witness/pre_signature in/out) --
 *                             pure computation, no (de)serialisation;
 *       TIER 2, END-TO-END  : the *_packed byte API (validating unpack -> core ->
 *                             pack INSIDE the call), the boundary upstream sign.c
 *                             exposes and what a wire/on-chain consumer pays.
 *   The tier-2 sign-class calls run the SAME rejection loop (they wrap the core),
 *   so both tiers' Sign/PreSign segments feed the run-validity rejection gate.
 *   las.{c,h} are untouched by the base scheme; basesig shares only las.h's parameter
 *   macros and key/signature struct layout, so both run over matched parameters and
 *   primitives (separate base/adaptor modules over matched parameters and primitives).
 *
 *   BASE  (simplified Dilithium-style signature; no adaptor statement Y):
 *       Sign    : c = H(pk, w,   M)            -- the commitment w is hashed as-is
 *       Verify  : w' = A*z - c*t;  accept iff  c == H(pk, w',  M)
 *   LAS ADAPTOR  (the SAME scheme, with the statement/lock Y bound into the hash):
 *       PreSign  : c = H(pk, w + Y, M)         -- Y (the adaptor lock) is folded in
 *       PreVerify: w' = A*z^ - c*t; accept iff c == H(pk, w' + Y, M)
 *       Adapt    : z = z^ + r'
 *       Ext      : s = z - z^
 *   Why the adapted signature passes the ORDINARY base Verify with no explicit +Y:
 *       A*z - c*t = A*(z^ + y) - c*t = (A*z^ - c*t) + A*y = w' + Y      (Y = A*y).
 *
 *   DIAGNOSTIC sections (SECONDARY; printed after the primary timings, from the SAME
 *   state) are cost-attribution and communication aids for the report -- they do NOT
 *   change any protocol semantics:
 *       A. Rejection-sampling distribution for base_sign and las_presign, read
 *          DIRECTLY off the per-module attempt counters (base_attempts / las_attempts):
 *          average attempts/sig, acceptance %, min, max, p50, p95.  Plus the
 *          run-validity REJECTION GATE (mirrors the Rust drivers): the attempts/call
 *          measured over the TIMED sign-class calls (BOTH tiers) is hard-checked
 *          against the exact expectation las_expected_attempts() within 5 sigma -- a
 *          run whose restart rate deviates from theory aborts instead of producing
 *          invalid evidence.
 *       B. Adapt timing clarification: the real protocol cost ("Adapt checked total",
 *          i.e. las_adapt incl. its internal las_preverify -- this is the protocol
 *          Adapt timing above) plus a diagnostic-only lower bound ("witness-add only",
 *          just z = z_hat + r').  witness-add only is NOT a protocol operation; a real
 *          Adapt MUST pre-verify first.
 *       C. Communication-derived packed byte sizes and ratios, plus the byte-level
 *          atomic-swap payload.  These are BYTE-LEVEL payloads only and are deliberately
 *          NOT mixed with EVM gas (the on-chain cost is a separate axis; see evm/).
 *          Also a protocol-component catalogue (pp=(A,H) seed/expanded A', the internal
 *          mask y and commitment w, the unimplemented proof pi, excluded tx metadata).
 *       D. Operation-level component microbenchmarks.  These time LOCAL COPIES of the
 *          protocol's inner steps -- behaviourally identical to las.c's static helpers,
 *          duplicated here exactly as basesig.c duplicates them, so las.c is not touched
 *          -- for cost ATTRIBUTION only.  They are component estimates, not a measurement
 *          of the protocol entry points (those are the PRIMARY timings above).  Covers the
 *          sign-side (A-product, hash, c*r) AND the verify-side c*t, KeyGen sample-r, and
 *          the Ext breakdown (s=z-z^, A*s, t'==A*s check).
 *       E. Norm-margin diagnostics: the achieved max |z|inf / |z^|inf vs their reject
 *          bounds (the response saturates the band -- why ~63% of attempts reject),
 *          contrasted with the loose AMHL witness-norm budget (see bench_app).
 *
 * No optimised/original CRYSTALS-Dilithium numbers are produced here: that is a
 * different algorithm and is not the comparison made.
 *
 * The benchmark fixes ONE consistent state per run (a key pair, a statement/witness,
 * and the signature / pre-signature / adapted signature derived from THAT key) and
 * gates all timing on the full success-path contract -- the ordinary signature
 * verifies; the pre-signature pre-verifies but FAILS ordinary Verify (the
 * statement-binding tripwire); the adapted signature verifies; and Ext recovers the
 * witness exactly -- so no failure or early-return path is ever timed.  The packed
 * tier's canonical byte objects are the SAME canonical structs packed once via the
 * shared codec, and the identical contract is re-enforced at the byte boundary
 * (packed interlock incl. exact witness-BYTE recovery) before that tier is timed.
 *
 * Build (Makefile sets -DLAS_N/-DELL/-DKAPPA for each parameter set).
 * The L2/L3/L5-style targets are dimension-aligned engineering settings inspired by
 * Dilithium's parameter progression, used to study scaling across matched parameter
 * sets, not to claim formal same-security equivalence.
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
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "../basesig.h"     /* BASE path: base_keygen/base_sign/base_verify + base_attempts + BOUND_SIGN */
#include "../relation.h"    /* relation_gen -> (statement, witness) */
#include "../las.h"         /* ADAPTOR path: las_presign/preverify/adapt/ext + las_attempts + BOUND_PRESIGN */
#include "../params.h"      /* Q */
#include "../poly.h"        /* poly arithmetic for the component microbenchmarks    */
#include "../fips202.h"     /* SHAKE256 for the local challenge-hash copy           */
#include "../serialize.h"   /* shared codec: canonical packed state for the end-to-end tier */
#include "cpucycles.h"      /* upstream cycle counter (rdtsc): cycles/op next to us/op */

/* Reproducibility metadata.  Compiler and build date are always available via the
 * predefined macros below; git commit/branch are injected by the runner
 * (scripts/run_fair_benchmarks.sh) as -DLAS_GIT_COMMIT / -DLAS_GIT_BRANCH, so a plain
 * `make` leaves them "n/a".  CPU model, OS/WSL string and the run date are captured
 * per run by that runner into the run directory's metadata.txt (a portable C program
 * cannot read them).  This is metadata only -- it does not affect any measurement. */
#ifndef LAS_GIT_COMMIT
#define LAS_GIT_COMMIT "n/a"
#endif
#ifndef LAS_GIT_BRANCH
#define LAS_GIT_BRANCH "n/a"
#endif

/* Repetition scheme MIRRORS the Rust driver (rust/fips204-las/examples/
 * bench_levels.rs) exactly, so the two languages collect their evidence
 * identically and the overhead ratios are directly comparable: 5 outer
 * repetitions; 500 inner iterations per repetition for the sign-class
 * operations (each call includes its rejection restarts) and 1000 for the
 * verify-class ones.  The sign-class attempt totals over the TIMED calls feed
 * the run-validity rejection gate (see rejection_gate below). */
#define RUNS       5        /* outer repetitions -> mean +/- sample SD              */
#define NITER_SIGN 500      /* inner iterations per repetition, sign-class          */
#define NITER_FAST 1000     /* inner iterations per repetition, verify-class        */
#define NSIG  2000          /* signing calls sampled for the attempt distribution   */

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
static double g_mean, g_sd;          /* set by MEASURE / MEASURE_SIGN */
static double g_cyc_runs[RUNS];      /* per-rep cycles/op series (same window)   */
static double g_cmean, g_csd;        /* set by MEASURE / MEASURE_SIGN (cycles)   */
static double g_att_runs[RUNS];      /* per-rep per-ATTEMPT us (set by MEASURE_SIGN) */
static unsigned long g_att_total;    /* attempts over the RUNS x NITER_SIGN timed calls */
static volatile long g_sink;

/* Run the op for RUNS x niter_; leave mean/SD (us) in g_mean/g_sd and mean/SD
 * (cycles) in g_cmean/g_csd.  The cycle window (upstream test/cpucycles.h
 * rdtsc) nests directly inside the wall-clock window: two counter reads per
 * repetition, amortised over >= NITER_SIGN iterations -- well under 0.1
 * cycles/op, so the us series (which mirrors the Rust driver) is unaffected.
 * Variadic so op bodies may contain unparenthesised commas. */
#define MEASURE(niter_, ...) do {                                     \
    int br_, bi_;                                                     \
    for(br_ = 0; br_ < RUNS; ++br_) {                                 \
      double bt0_ = now_us();                                         \
      uint64_t bc0_ = cpucycles();                                    \
      for(bi_ = 0; bi_ < (niter_); ++bi_) { __VA_ARGS__; }            \
      g_cyc_runs[br_] = (double)(cpucycles() - bc0_) / (niter_);      \
      g_runs[br_] = (now_us() - bt0_) / (niter_);                     \
    }                                                                \
    stats(g_runs, RUNS, &g_mean, &g_sd);                             \
    stats(g_cyc_runs, RUNS, &g_cmean, &g_csd);                       \
  } while(0)

/* Sign-class variant (mirrors the Rust driver): additionally records, per
 * repetition, the attempt-counter delta -- giving the per-ATTEMPT us series in
 * g_att_runs -- and accumulates the total attempts over the RUNS x NITER_SIGN
 * TIMED calls in g_att_total, which feeds the run-validity rejection gate. */
#define MEASURE_SIGN(counter_, ...) do {                              \
    int br_, bi_;                                                     \
    g_att_total = 0;                                                  \
    for(br_ = 0; br_ < RUNS; ++br_) {                                 \
      unsigned long ba_ = (counter_);                                 \
      double bt0_ = now_us();                                         \
      uint64_t bc0_ = cpucycles();                                    \
      for(bi_ = 0; bi_ < NITER_SIGN; ++bi_) { __VA_ARGS__; }          \
      g_cyc_runs[br_] = (double)(cpucycles() - bc0_) / NITER_SIGN;    \
      g_runs[br_] = (now_us() - bt0_) / NITER_SIGN;                   \
      ba_ = (counter_) - ba_;                                         \
      g_att_runs[br_] = g_runs[br_] * NITER_SIGN / (double)ba_;       \
      g_att_total += ba_;                                             \
    }                                                                \
    stats(g_runs, RUNS, &g_mean, &g_sd);                             \
    stats(g_cyc_runs, RUNS, &g_cmean, &g_csd);                       \
  } while(0)

static int ceil_log2(double x) { return (int)ceil(log2(x)); }

/* coefficientwise equality of two witness/secret vectors */
static int witness_equal(const witness *a, const witness *b) {
  unsigned int j, k;
  for(j = 0; j < N_PLUS_ELL; ++j)
    for(k = 0; k < LAS_D; ++k)
      if(a->value[j].coeffs[k] != b->value[j].coeffs[k])
        return 0;
  return 1;
}

/* ---- attempt-distribution helpers (diagnostic section A) ----------------- */

static int cmp_ul(const void *a, const void *b) {
  unsigned long x = *(const unsigned long *)a, y = *(const unsigned long *)b;
  return (x > y) - (x < y);
}

/* nearest-rank percentile on a sorted ascending array */
static unsigned long pct(const unsigned long *sorted, int n, double p) {
  int idx = (int)ceil(p / 100.0 * (double)n) - 1;
  if(idx < 0)  idx = 0;
  if(idx >= n) idx = n - 1;
  return sorted[idx];
}

static unsigned long att_base[NSIG];
static unsigned long att_pre[NSIG];

/* Run-validity gate -- same 5-sigma check and line format as the Rust drivers
 * (benches/las_bench.rs, examples/bench_levels.rs): the restart rate measured
 * over the TIMED sign-class calls of THIS run must match the exact expectation
 * las_expected_attempts() derived from the paper's rejection bounds (eprint
 * 2020/845 Alg. 1 step 11 / Alg. 2 step 6).  Attempts/call over `calls` i.i.d.
 * geometric draws has SD = E*sqrt(1-1/E), so the band is 5*SD/sqrt(calls)
 * (~+-8% at RUNS x NITER_SIGN = 2500 calls -- a coarse gross-breakage check;
 * the Rust Criterion run's >=100k calls give the tight ~+-1% version).  On
 * failure the run aborts: it is NOT valid evidence. */
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

/* ============================ component copies =============================
 * Local copies of las.c's inner steps, behaviourally identical, duplicated here
 * exactly as basesig.c duplicates them, so las.c is NOT modified.  Used only to
 * ATTRIBUTE cost to the components in diagnostic section D; they are not the
 * protocol (the protocol entry points are the PRIMARY timings). */

static void mc_pack_poly_canon(uint8_t out[LAS_D*4], const poly *a) {
  unsigned int i;
  uint32_t x;
  poly t = *a;
  poly_reduce(&t);
  poly_caddq(&t);
  for(i = 0; i < LAS_D; ++i) {
    x = (uint32_t)t.coeffs[i];
    out[4*i+0] = (uint8_t)x;
    out[4*i+1] = (uint8_t)(x >> 8);
    out[4*i+2] = (uint8_t)(x >> 16);
    out[4*i+3] = (uint8_t)(x >> 24);
  }
}

/* Second half of the NTT product (operands already transformed) -- matches
 * las.c polymul_prehat.  The protocol hoists the transforms: NTT(s) once per
 * Sign/PreSign call, NTT(c) once per attempt / per verify, NTT(t_j) per verify
 * (the upstream ref/sign.c structure); the component lines below time those
 * pieces in the same shape. */
static void mc_polymul_prehat(poly *out, const poly *ahat, const poly *bhat) {
  poly_pointwise_montgomery(out, ahat, bhat);
  poly_invntt_tomont(out);
  poly_reduce(out);
}

/* w = A*v = v_top + A'*v_bot, A=[I|A'], A' (pp->a_prime) already in NTT domain. */
static void mc_Amul(poly w[LAS_N], const public_params *pp, const poly v[N_PLUS_ELL]) {
  poly vhat[ELL], tmp, acc;
  unsigned int i, j, k;

  for(j = 0; j < ELL; ++j) {
    vhat[j] = v[LAS_N + j];
    poly_ntt(&vhat[j]);
  }
  for(i = 0; i < LAS_N; ++i) {
    for(k = 0; k < LAS_D; ++k)
      acc.coeffs[k] = 0;
    for(j = 0; j < ELL; ++j) {
      poly_pointwise_montgomery(&tmp, &pp->a_prime[i][j], &vhat[j]);
      poly_add(&acc, &acc, &tmp);
    }
    poly_reduce(&acc);
    poly_invntt_tomont(&acc);
    poly_add(&w[i], &acc, &v[i]);
    poly_reduce(&w[i]);
    poly_caddq(&w[i]);
  }
}

static void mc_challenge(poly *c, const uint8_t seed[LAS_SEEDBYTES]) {
  unsigned int i, b, pos;
  uint64_t signs;
  uint8_t buf[SHAKE256_RATE];
  keccak_state state;

  shake256_init(&state);
  shake256_absorb(&state, seed, LAS_SEEDBYTES);
  shake256_finalize(&state);
  shake256_squeezeblocks(buf, 1, &state);

  signs = 0;
  for(i = 0; i < 8; ++i)
    signs |= (uint64_t)buf[i] << 8*i;
  pos = 8;

  for(i = 0; i < LAS_D; ++i)
    c->coeffs[i] = 0;
  for(i = LAS_D - KAPPA; i < LAS_D; ++i) {
    do {
      if(pos >= SHAKE256_RATE) {
        shake256_squeezeblocks(buf, 1, &state);
        pos = 0;
      }
      b = buf[pos++];
    } while(b > i);
    c->coeffs[i] = c->coeffs[b];
    c->coeffs[b] = 1 - 2*(signs & 1);
    signs >>= 1;
  }
}

/* c = H(pk, commit, M): the Fiat-Shamir challenge hash (full SHAKE absorb path). */
static void mc_hash_challenge(poly *c, const public_key *pk, const poly commit[LAS_N],
                              const uint8_t *m, size_t mlen) {
  keccak_state state;
  uint8_t buf[LAS_D*4];
  uint8_t seed[LAS_SEEDBYTES];
  unsigned int i;

  shake256_init(&state);
  for(i = 0; i < LAS_N; ++i) {
    mc_pack_poly_canon(buf, &pk->t[i]);
    shake256_absorb(&state, buf, LAS_D*4);
  }
  for(i = 0; i < LAS_N; ++i) {
    mc_pack_poly_canon(buf, &commit[i]);
    shake256_absorb(&state, buf, LAS_D*4);
  }
  shake256_absorb(&state, m, mlen);
  shake256_finalize(&state);
  shake256_squeeze(seed, LAS_SEEDBYTES, &state);
  mc_challenge(c, seed);
}

/* Reject if any component has ||.||inf >= B (the rejection-loop norm check). */
static int mc_chknorm_vec(const poly z[N_PLUS_ELL], int32_t B) {
  unsigned int j;
  for(j = 0; j < N_PLUS_ELL; ++j)
    if(poly_chknorm(&z[j], B))
      return 1;
  return 0;
}

/* commit = w + Y over the n statement polynomials (PreSign/PreVerify inner step). */
static void mc_add_wY(poly out[LAS_N], const poly w[LAS_N], const statement *Y) {
  unsigned int j;
  for(j = 0; j < LAS_N; ++j) {
    poly_add(&out[j], &w[j], &Y->t_prime[j]);
    poly_reduce(&out[j]);
    poly_caddq(&out[j]);
  }
}

/* z = z_hat + r' over the n+l response polynomials (the Adapt witness add).
 * Identical to las_adapt()'s body AFTER its mandatory PreVerify -- see section B. */
static void mc_witness_add(signature *out, const pre_signature *presig, const witness *y) {
  unsigned int j;
  memcpy(out->c_tilde, presig->c_tilde, LAS_CTILDEBYTES);  /* Adapt preserves the challenge digest */
  for(j = 0; j < N_PLUS_ELL; ++j) {
    poly_add(&out->z[j], &presig->z_hat[j], &y->value[j]);
    poly_reduce(&out->z[j]);
  }
}

/* coefficientwise poly equality (the t' == A*s check inside Ext). */
static int mc_poly_equal(const poly *a, const poly *b) {
  unsigned int i;
  for(i = 0; i < LAS_D; ++i)
    if(a->coeffs[i] != b->coeffs[i])
      return 0;
  return 1;
}

/* max centred |coeff| (infinity norm) over a poly / a response vector.  The
 * (pre-)signature responses are poly_reduce'd to (-Q/2, Q/2] and their true
 * magnitude is < gamma << Q/2, so the stored coeff already IS the centred
 * representative and a plain abs gives the infinity norm. */
static int32_t mc_max_abs(const poly *a) {
  int32_t mx = 0, v;
  unsigned int i;
  for(i = 0; i < LAS_D; ++i) { v = a->coeffs[i]; if(v < 0) v = -v; if(v > mx) mx = v; }
  return mx;
}
static int32_t mc_max_abs_vec(const poly z[N_PLUS_ELL]) {
  int32_t mx = 0, v;
  unsigned int j;
  for(j = 0; j < N_PLUS_ELL; ++j) { v = mc_max_abs(&z[j]); if(v > mx) mx = v; }
  return mx;
}

/* Local copy of las.c's ternary sampler -- KeyGen / statement-generation's
 * "sample r" step -- behaviourally identical, duplicated here so las.c is NOT
 * modified.  Used only to attribute KeyGen / Gen cost in diagnostic section D. */
static void mc_sample_ternary(poly *r, const uint8_t *seed, size_t seedlen, uint16_t nonce) {
  keccak_state state;
  uint8_t buf[SHAKE256_RATE];
  uint8_t nb[2], byte, v, s;
  unsigned int ctr = 0, pos = 0;

  nb[0] = (uint8_t)nonce;
  nb[1] = (uint8_t)(nonce >> 8);
  shake256_init(&state);
  shake256_absorb(&state, seed, seedlen);
  shake256_absorb(&state, nb, 2);
  shake256_finalize(&state);
  shake256_squeezeblocks(buf, 1, &state);

  while(ctr < LAS_D) {
    if(pos >= SHAKE256_RATE) { shake256_squeezeblocks(buf, 1, &state); pos = 0; }
    byte = buf[pos++];
    for(s = 0; s < 4 && ctr < LAS_D; ++s) {
      v = (byte >> (2*s)) & 3;            /* 2 bits: {0,1,2}->{-1,0,1}, reject 3 */
      if(v < 3) r->coeffs[ctr++] = (int32_t)v - 1;
    }
  }
}

/* ================================ main ==================================== */

int main(void) {
  uint8_t ppseed[LAS_SEEDBYTES];
  /* Fixed 33-byte message and fixed pp seed 00..1f -- the SAME bytes as the
   * Rust drivers -- so both languages benchmark identical public parameters
   * and message.  KeyGen/mask randomness still comes from the system RNG
   * inside las.c/basesig.c (the Rust side draws it from a fixed-seed ChaCha8
   * instead); the rejection gate validates the resulting restart statistics
   * of every run either way. */
  static const uint8_t msg[] = "bench message, thirty-three bytes";
  const uint8_t *m = msg;
  size_t  mlen = sizeof msg - 1;
  public_params pp, pp2;                          /* pp = canonical; pp2 = Setup scratch */
  public_key    pk, pk2;                          /* pk2 = KeyGen scratch                */
  statement     Y;
  secret_key    sk, sk2;                          /* sk2 = KeyGen scratch                */
  witness       yy, yext;                         /* honest witness r' (Gen); extracted s (Ext) */
  signature     sig, adapted, tmp;
  pre_signature presig, tmp_pre;                  /* tmp_pre = pre_signature scratch     */
  poly    w[LAS_N], wY[LAS_N], cc, cr;
  poly    chat_mc, that_mc, shat_mc[N_PLUS_ELL];  /* pre-NTT'd operands (section D)      */
  unsigned int j;
  int     i;

  /* PRIMARY protocol timings, TIER 1 core crypto (us mean, sd + cycles mean, sd) */
  double su_m, su_s, kg_m, kg_s, sg_m, sg_s, vf_m, vf_s;
  double ps_m, ps_s, pv_m, pv_s, ad_m, ad_s, ex_m, ex_s;
  double su_cm, su_cs, kg_cm, kg_cs, sg_cm, sg_cs, vf_cm, vf_cs;
  double ps_cm, ps_cs, pv_cm, pv_cs, ad_cm, ad_cs, ex_cm, ex_cs;
  /* PRIMARY protocol timings, TIER 2 end-to-end packed (us + cycles) */
  double kgp_m, kgp_s, sgp_m, sgp_s, vfp_m, vfp_s, psp_m, psp_s;
  double pvp_m, pvp_s, adp_m, adp_s, exp_m, exp_s;
  double kgp_cm, kgp_cs, sgp_cm, sgp_cs, vfp_cm, vfp_cs, psp_cm, psp_cs;
  double pvp_cm, pvp_cs, adp_cm, adp_cs, exp_cm, exp_cs;
  /* per-attempt (rejection-normalised) series + attempt totals over the TIMED
   * sign-class calls (set via MEASURE_SIGN; feeds the rejection gate) */
  double sg_att_m, sg_att_s, ps_att_m, ps_att_s;
  unsigned long sg_att_tot, ps_att_tot;
  unsigned long sgp_att_tot, psp_att_tot;   /* packed-tier sign-class attempt totals */
  uint64_t cyc_ovh;                         /* measured cycle-counter read overhead  */

  /* Canonical PACKED state: the SAME canonical structs, packed once via the
   * shared codec (serialize.{c,h}); scratch buffers receive the outputs of the
   * timed producing packed ops so the canonical bytes are never overwritten. */
  static uint8_t pk_b[PUBLIC_KEY_BYTES], Y_b[STATEMENT_BYTES];
  static uint8_t sk_b[SECRET_KEY_BYTES], yw_b[WITNESS_BYTES];
  static uint8_t sig_b[SIGNATURE_BYTES], presig_b[PRE_SIGNATURE_BYTES], adapted_b[SIGNATURE_BYTES];
  static uint8_t pk2_b[PUBLIC_KEY_BYTES], sk2_b[SECRET_KEY_BYTES];    /* scratch */
  static uint8_t sig2_b[SIGNATURE_BYTES], y2_b[WITNESS_BYTES];        /* scratch */

  /* DIAGNOSTIC timings (mean, sd): witness-add only + component microbenchmarks */
  double wo_m, wo_s;
  double am_m, am_s, ch_m, ch_s, mu_m, mu_s, ma_m, ma_s, nk_m, nk_s, wy_m, wy_s;
  double ct_m, ct_s;                       /* c*t over LAS_N pk polys (Verify side) */
  double ntc_m, ntc_s, nts_m, nts_s;        /* NTT(c) per attempt; NTT(s) per call  */
  double kr_m, kr_s;                        /* KeyGen / Gen: sample r (ternary)     */
  double es_m, es_s, ea_m, ea_s, ec_m, ec_s; /* Ext breakdown: s=z-z^, A*s, check  */
  int32_t maxz = 0, maxzhat = 0;            /* achieved norm vs the reject bound    */

  /* DIAGNOSTIC rejection-sampling distribution */
  unsigned long tot_base = 0, tot_pre = 0;
  double avg_base, avg_pre;

  /* packed component sizes (bytes) for THIS parameter set */
  int    pk_bits = ceil_log2((double)Q);                       /* 23 for Q<2^23 */
  int    z_bits  = ceil_log2(2.0*(GAMMA - KAPPA) + 1.0);
  size_t sz_pk   = (size_t)(LAS_N * LAS_D * pk_bits + 7) / 8;
  size_t sz_sk   = (size_t)(N_PLUS_ELL * LAS_D * 2 + 7) / 8;
  size_t sz_c    = LAS_CTILDEBYTES;                           /* challenge digest c_tilde (raw 32 B) */
  size_t sz_z    = (size_t)(N_PLUS_ELL * LAS_D * z_bits + 7) / 8;
  size_t sz_sig  = sz_c + sz_z;
  /* protocol-component catalogue sizes (context rows; see diagnostic section C) */
  int    y_bits  = ceil_log2(2.0*(double)GAMMA + 1.0);      /* mask y in S_gamma   */
  size_t sz_seed = LAS_SEEDBYTES;                              /* the public A' seed  */
  size_t sz_Aexp = (size_t)((size_t)LAS_N * ELL * LAS_D * pk_bits + 7) / 8; /* expanded A' */
  size_t sz_ymask= (size_t)((size_t)N_PLUS_ELL * LAS_D * y_bits + 7) / 8; /* signing mask (internal) */

  for(j = 0; j < LAS_SEEDBYTES; ++j) ppseed[j] = (uint8_t)j;

  /* ---- ONE setup + ONE consistent state per run (used by EVERY measurement) ----
   * Public parameters, a key pair, a statement/witness (Y = A*yy is literally another
   * key pair), and the BASE signature / LAS pre-signature / adapted signature, all
   * derived from THAT key.  KeyGen and Setup are timed below into SCRATCH objects so
   * the canonical state stays intact for the sanity gate and the remaining timings. */
  setup_public_params(&pp, ppseed);                                    /* Setup (public params) */
  base_keygen(&pk, &sk, &pp);                                /* BASE KeyGen           */
  relation_gen(&Y,  &yy, &pp);                                /* statement/witness     */
  base_sign(&sig, m, mlen, &pk, &sk, &pp);                   /* BASE sign (no Y)      */
  las_presign(&presig, m, mlen, &Y, &pk, &sk, &pp);          /* LAS pre-sign (folds Y)*/
  if(las_adapt(&adapted, &presig, m, mlen, &Y, &yy, &pk, &pp) != 0) {
    printf("FATAL: could not establish a valid adapted signature\n");
    return 1;
  }

  /* Refuse to benchmark unless the FULL cross-path success contract holds, so no
   * failure or early-return path is ever timed.  The ordinary-signature checks use
   * the SEPARATE base verifier (base_verify, from basesig.c), proving the two paths
   * interlock:
   *   - the base signature verifies under base_verify;
   *   - the LAS pre-signature pre-verifies, but base_verify REJECTS it (its hash binds
   *     w+Y, the base verifier recomputes H(pk,w',M) without Y -- the tripwire);
   *   - the LAS-adapted signature verifies under the INDEPENDENT base_verify with no
   *     explicit +Y (because A(z^+y)-ct = w'+Y); and
   *   - Ext recovers the witness EXACTLY. */
  {
    /* byte-level relabel of the pre-signature (distinct type) for the tripwire clause */
    signature presig_as_sig;
    uint8_t relabel_b[PRE_SIGNATURE_BYTES];
    if(pack_pre_signature(relabel_b, &presig) != 0 ||
       unpack_signature(&presig_as_sig, relabel_b) != 0) {
      printf("FATAL: tripwire pack/unpack\n");
      return 1;
    }
    if(base_verify(&sig, m, mlen, &pk, &pp) != 0 ||                /* base sig verifies     */
       las_preverify(&presig, m, mlen, &Y, &pk, &pp) != 0 ||      /* presig pre-verifies   */
       base_verify(&presig_as_sig, m, mlen, &pk, &pp) == 0 ||     /* presig is NOT a base sig */
       base_verify(&adapted, m, mlen, &pk, &pp) != 0 ||           /* adapted = base sig    */
       las_ext(&yext, &adapted, &presig, &Y, &pp) != 0 ||         /* Ext succeeds          */
       !witness_equal(&yext, &yy)) {                              /* exact witness recover */
      printf("FATAL: benchmark state inconsistent before timing\n");
      return 1;
    }
  }

  /* Canonical PACKED state for the end-to-end tier: pack the SAME canonical
   * objects once (the codec's pack side rejects out-of-band input, so a failed
   * pack is itself a state error), then re-enforce the identical success
   * contract at the BYTE boundary (mirrors test_serde.c's interlock) so the
   * packed tier never times a failure path either. */
  pack_public_key(pk_b, &pk);
  pack_statement(Y_b, &Y);
  if(pack_secret_key(sk_b, &sk) != 0 || pack_witness(yw_b, &yy) != 0 ||
     pack_signature(sig_b, &sig) != 0 || pack_pre_signature(presig_b, &presig) != 0 ||
     pack_signature(adapted_b, &adapted) != 0) {
    printf("FATAL: could not pack the canonical benchmark state\n");
    return 1;
  }
  if(base_verify_packed(sig_b, m, mlen, pk_b, &pp) != 0 ||     /* base sig verifies (bytes) */
     las_preverify_packed(presig_b, m, mlen, Y_b, pk_b, &pp) != 0 ||/* presig pre-verifies (bytes) */
     base_verify_packed(presig_b, m, mlen, pk_b, &pp) == 0 ||  /* byte-level tripwire   */
     base_verify_packed(adapted_b, m, mlen, pk_b, &pp) != 0 || /* adapted = base sig (bytes) */
     las_ext_packed(y2_b, adapted_b, presig_b, Y_b, &pp) != 0 ||    /* Ext succeeds (bytes)  */
     memcmp(y2_b, yw_b, SECRET_KEY_BYTES) != 0) {                       /* exact witness BYTES   */
    printf("FATAL: packed-tier benchmark state inconsistent before timing\n");
    return 1;
  }

  /* Measure the cycle counter's own read overhead (upstream test/cpucycles.c);
   * reported as metadata -- at two reads per >=NITER_SIGN-iteration repetition
   * it is far below 0.1 cycles/op and is NOT subtracted from any figure. */
  cyc_ovh = cpucycles_overhead();

  /* ============================ PRIMARY TIMINGS ============================
   * Protocol-level operations, TIER 1: core crypto (struct API).  Producing
   * operations write to SCRATCH (pp2/pk2/sk2/tmp/yext) so the canonical state is
   * never mutated; verifies read canonical objects. */
  MEASURE(NITER_FAST, setup_public_params(&pp2, ppseed));
  su_m = g_mean; su_s = g_sd; su_cm = g_cmean; su_cs = g_csd;
  MEASURE(NITER_FAST, base_keygen(&pk2, &sk2, &pp));
  kg_m = g_mean; kg_s = g_sd; kg_cm = g_cmean; kg_cs = g_csd;
  /* BASE path (basesig.c).  Sign-class: MEASURE_SIGN also captures the
   * per-attempt series and the attempt total over the timed calls. */
  MEASURE_SIGN(base_attempts, base_sign(&tmp, m, mlen, &pk, &sk, &pp));
  sg_m = g_mean; sg_s = g_sd; sg_cm = g_cmean; sg_cs = g_csd;
  stats(g_att_runs, RUNS, &sg_att_m, &sg_att_s);
  sg_att_tot = g_att_total;
  MEASURE(NITER_FAST, g_sink += base_verify(&sig, m, mlen, &pk, &pp));
  vf_m = g_mean; vf_s = g_sd; vf_cm = g_cmean; vf_cs = g_csd;
  /* LAS ADAPTOR path (las.c). */
  MEASURE_SIGN(las_attempts, las_presign(&tmp_pre, m, mlen, &Y, &pk, &sk, &pp));
  ps_m = g_mean; ps_s = g_sd; ps_cm = g_cmean; ps_cs = g_csd;
  stats(g_att_runs, RUNS, &ps_att_m, &ps_att_s);
  ps_att_tot = g_att_total;
  MEASURE(NITER_FAST, g_sink += las_preverify(&presig, m, mlen, &Y, &pk, &pp));
  pv_m = g_mean; pv_s = g_sd; pv_cm = g_cmean; pv_cs = g_csd;
  MEASURE(NITER_FAST, g_sink += las_adapt(&tmp, &presig, m, mlen, &Y, &yy, &pk, &pp));
  ad_m = g_mean; ad_s = g_sd; ad_cm = g_cmean; ad_cs = g_csd;
  MEASURE(NITER_FAST, g_sink += las_ext(&yext, &adapted, &presig, &Y, &pp));
  ex_m = g_mean; ex_s = g_sd; ex_cm = g_cmean; ex_cs = g_csd;

  /* TIER 2: end-to-end packed (byte API; validating unpack -> core -> pack
   * INSIDE the call).  Same repetition scheme; producing ops write to byte
   * SCRATCH (pk2_b/sk2_b/sig2_b/y2_b), verifies read the canonical packed
   * objects.  The sign-class packed calls wrap the same rejection loop, so
   * MEASURE_SIGN gates their attempt totals exactly like the core tier's.
   * (Setup has no packed twin: pp is public infrastructure, never on the wire.) */
  MEASURE(NITER_FAST, g_sink += base_keygen_packed(pk2_b, sk2_b, &pp));
  kgp_m = g_mean; kgp_s = g_sd; kgp_cm = g_cmean; kgp_cs = g_csd;
  MEASURE_SIGN(base_attempts, g_sink += base_sign_packed(sig2_b, m, mlen, pk_b, sk_b, &pp));
  sgp_m = g_mean; sgp_s = g_sd; sgp_cm = g_cmean; sgp_cs = g_csd;
  sgp_att_tot = g_att_total;
  MEASURE(NITER_FAST, g_sink += base_verify_packed(sig_b, m, mlen, pk_b, &pp));
  vfp_m = g_mean; vfp_s = g_sd; vfp_cm = g_cmean; vfp_cs = g_csd;
  MEASURE_SIGN(las_attempts, g_sink += las_presign_packed(sig2_b, m, mlen, Y_b, pk_b, sk_b, &pp));
  psp_m = g_mean; psp_s = g_sd; psp_cm = g_cmean; psp_cs = g_csd;
  psp_att_tot = g_att_total;
  MEASURE(NITER_FAST, g_sink += las_preverify_packed(presig_b, m, mlen, Y_b, pk_b, &pp));
  pvp_m = g_mean; pvp_s = g_sd; pvp_cm = g_cmean; pvp_cs = g_csd;
  MEASURE(NITER_FAST, g_sink += las_adapt_packed(sig2_b, presig_b, m, mlen, Y_b, yw_b, pk_b, &pp));
  adp_m = g_mean; adp_s = g_sd; adp_cm = g_cmean; adp_cs = g_csd;
  MEASURE(NITER_FAST, g_sink += las_ext_packed(y2_b, adapted_b, presig_b, Y_b, &pp));
  exp_m = g_mean; exp_s = g_sd; exp_cm = g_cmean; exp_cs = g_csd;

  /* ===================== DIAGNOSTIC A: rejection distribution =====================
   * Per-call attempt counts read DIRECTLY off base_attempts / las_attempts (deltas).
   * Producing operations write to scratch (tmp); canonical pk/sk/Y are read-only. */
  for(i = 0; i < NSIG; ++i) {
    unsigned long before = base_attempts;
    int32_t v;
    base_sign(&tmp, m, mlen, &pk, &sk, &pp);
    att_base[i] = base_attempts - before;
    tot_base   += att_base[i];
    v = mc_max_abs_vec(tmp.z); if(v > maxz) maxz = v;     /* achieved |z|inf */
  }
  for(i = 0; i < NSIG; ++i) {
    unsigned long before = las_attempts;
    int32_t v;
    las_presign(&tmp_pre, m, mlen, &Y, &pk, &sk, &pp);
    att_pre[i] = las_attempts - before;
    tot_pre   += att_pre[i];
    v = mc_max_abs_vec(tmp_pre.z_hat); if(v > maxzhat) maxzhat = v; /* achieved |z^|inf */
  }
  qsort(att_base, NSIG, sizeof att_base[0], cmp_ul);
  qsort(att_pre,  NSIG, sizeof att_pre[0],  cmp_ul);
  avg_base = (double)tot_base / NSIG;
  avg_pre  = (double)tot_pre  / NSIG;

  /* ===================== DIAGNOSTIC B/D: witness-add + components =================
   * witness-add only (z = z_hat + y) is the lower-bound used in both section B and the
   * component list (section D); the protocol Adapt timing above is "Adapt checked total". */
  MEASURE(NITER_FAST, { mc_witness_add(&tmp, &presig, &yy); g_sink += tmp.z[0].coeffs[0]; });
  wo_m = g_mean; wo_s = g_sd;

  mc_Amul(w, &pp, sk.r);                          /* prime a commitment for the hash */
  MEASURE(NITER_FAST, { mc_Amul(w, &pp, sk.r);                       g_sink += w[0].coeffs[0]; });
  am_m = g_mean; am_s = g_sd;
  MEASURE(NITER_FAST, { mc_hash_challenge(&cc, &pk, w, m, mlen);     g_sink += cc.coeffs[0]; });
  ch_m = g_mean; ch_s = g_sd;
  /* pre-NTT'd operands, matching the protocol's hoisting (las.c las_presign_internal) */
  for(j = 0; j < N_PLUS_ELL; ++j) { shat_mc[j] = sk.r[j]; poly_ntt(&shat_mc[j]); }
  chat_mc = cc; poly_ntt(&chat_mc);
  MEASURE(NITER_FAST, { that_mc = cc; poly_ntt(&that_mc);            g_sink += that_mc.coeffs[0]; });
  ntc_m = g_mean; ntc_s = g_sd;
  MEASURE(NITER_FAST, { for(j = 0; j < N_PLUS_ELL; ++j) { shat_mc[j] = sk.r[j]; poly_ntt(&shat_mc[j]); } g_sink += shat_mc[0].coeffs[0]; });
  nts_m = g_mean; nts_s = g_sd;
  MEASURE(NITER_FAST, { mc_polymul_prehat(&cr, &chat_mc, &shat_mc[0]); g_sink += cr.coeffs[0]; });
  mu_m = g_mean; mu_s = g_sd;
  /* per-attempt c*r step exactly as the protocol pays it: NTT(c) once, then
   * pointwise+invNTT per response poly (NTT(s) is per-call, timed above) */
  MEASURE(NITER_FAST, { that_mc = cc; poly_ntt(&that_mc); for(j = 0; j < N_PLUS_ELL; ++j) { mc_polymul_prehat(&cr, &that_mc, &shat_mc[j]); g_sink += cr.coeffs[0]; } });
  ma_m = g_mean; ma_s = g_sd;
  MEASURE(NITER_FAST, g_sink += mc_chknorm_vec(presig.z_hat, BOUND_PRESIGN));
  nk_m = g_mean; nk_s = g_sd;
  MEASURE(NITER_FAST, { mc_add_wY(wY, w, &Y);                        g_sink += wY[0].coeffs[0]; });
  wy_m = g_mean; wy_s = g_sd;

  /* ---- additional component attribution (Verify-side / KeyGen / Ext) ----
   * c*t exactly as Verify/PreVerify pay it: NTT(c) once per call, then
   * NTT(t_j) + pointwise+invNTT per public-key poly (las.c las_verify). */
  MEASURE(NITER_FAST, { that_mc = cc; poly_ntt(&that_mc); for(j = 0; j < LAS_N; ++j) { poly tj = pk.t[j]; poly_ntt(&tj); mc_polymul_prehat(&cr, &that_mc, &tj); g_sink += cr.coeffs[0]; } });
  ct_m = g_mean; ct_s = g_sd;
  /* KeyGen / Gen "sample r": N_PLUS_ELL ternary polys (statement Y generation = KeyGen).
   * The other half of KeyGen, A*r, is the A-product line above. */
  MEASURE(NITER_FAST, { for(j = 0; j < N_PLUS_ELL; ++j) { mc_sample_ternary(&sk2.r[j], ppseed, LAS_SEEDBYTES, (uint16_t)j); g_sink += sk2.r[0].coeffs[0]; } });
  kr_m = g_mean; kr_s = g_sd;
  /* Ext breakdown: s = z - z^ (n+ell polys); A*s; then t' == A*s check (n polys). */
  MEASURE(NITER_FAST, { for(j = 0; j < N_PLUS_ELL; ++j) { poly_sub(&yext.value[j], &adapted.z[j], &presig.z_hat[j]); poly_reduce(&yext.value[j]); } g_sink += yext.value[0].coeffs[0]; });
  es_m = g_mean; es_s = g_sd;
  MEASURE(NITER_FAST, { mc_Amul(w, &pp, yext.value);                 g_sink += w[0].coeffs[0]; });
  ea_m = g_mean; ea_s = g_sd;
  MEASURE(NITER_FAST, { int eq = 1; for(j = 0; j < LAS_N; ++j) eq &= mc_poly_equal(&w[j], &Y.t_prime[j]); g_sink += eq; });
  ec_m = g_mean; ec_s = g_sd;

  /* ================================ report ================================= */
  printf("==========================================================================\n");
  printf(" LAS parameter set: n=%d ell=%d kappa=%d gamma=%d  (d=%d, Q=%d)\n",
         LAS_N, ELL, KAPPA, GAMMA, LAS_D, Q);
  printf("   M = n + ell = %d   (dim of sk=r, witness r', mask y, responses z_hat/z)\n", N_PLUS_ELL);
  printf("   paper notation: pp=(A,H)  pk=t  sk=r  statement Y=t'  witness r'\n");
  printf("   signing mask y -> commitment w = A*y  (hashed into c; NOT transmitted)\n");
  printf("==========================================================================\n");
  printf(" Reproducibility: compiler=%s\n", __VERSION__);
  printf("                  built=%s %s   git=%s (%s)\n", __DATE__, __TIME__,
         LAS_GIT_COMMIT, LAS_GIT_BRANCH);
  printf("                  (CPU / OS-WSL / run-date captured per run by\n");
  printf("                   scripts/run_fair_benchmarks.sh into metadata.txt)\n");
  printf(" %d repetitions x %d (sign-class) / %d (verify-class) iters/op; mean +/- sample SD;\n",
         RUNS, NITER_SIGN, NITER_FAST);
  printf(" single thread, -O3.  Repetition scheme, fixed pp seed and fixed 33-byte message\n");
  printf(" mirror the Rust driver (rust/fips204-las/examples/bench_levels.rs) exactly.\n");
  printf(" Cycles/op via upstream test/cpucycles.{h,c} (rdtsc; invariant-TSC reference\n");
  printf(" cycles), read once per repetition window: measured read overhead %llu cycles,\n",
         (unsigned long long)cyc_ovh);
  printf(" i.e. ~%.4f cycles/op at the %d-iteration sign-class loop -- negligible, not\n",
         (double)cyc_ovh / NITER_SIGN, NITER_SIGN);
  printf(" subtracted.  One setup and one consistent state per run (plus its packed byte\n");
  printf(" image for the end-to-end tier); primary protocol timings first, then\n");
  printf(" diagnostics from the SAME state (cost-attribution / communication aids).\n\n");

  printf("--- WHAT IS COMPARED (separate base/adaptor modules; matched parameters; shared primitives) ---\n");
  printf(" BASE  (simplified Dilithium-style signature; NO adaptor statement t'):\n");
  printf("   Sign      c = H(pk, w,      M)        Verify    c == H(pk, w',      M)\n");
  printf(" LAS ADAPTOR (same scheme; statement/lock Y=t' folded into the hash):\n");
  printf("   PreSign   c = H(pk, w + t', M),  z_hat = y + c*r  (reject |z_hat|inf > gamma-kappa-1)\n");
  printf("   PreVerify c == H(pk, w' + t', M)\n");
  printf("   Adapt     z = z_hat + r'   (final response z;          reject |z|inf > gamma-kappa)\n");
  printf("   Ext       s = z - z_hat   (recovers the witness r')\n");
  printf("   Adapted sig clears ordinary Verify without an explicit +t' because\n");
  printf("     A(z_hat + r') - c*t = (A*z_hat - c*t) + A*r' = w' + t'   (t' = A*r').\n");
  printf("   pi (paper-level off-chain proof of well-formedness) is NOT implemented/measured here.\n");
  printf(" Every operation is timed at BOTH API tiers: TIER 1 core crypto (structs in/out,\n");
  printf(" pure computation) and TIER 2 end-to-end packed (bytes in/out; validating\n");
  printf(" unpack -> core -> pack inside the call, upstream sign.c's boundary).\n\n");

  printf("##########################################################################\n");
  printf("# PRIMARY (protocol-level) timings -- the headline adaptor-overhead table #\n");
  printf("##########################################################################\n");
  printf("--- COMPUTATION, TIER 1: CORE CRYPTO (struct API; pure computation, no codec)\n");
  printf("    (per op: microseconds AND cycles, each mean +/- sample SD) ---\n");
  printf("\n Shared (public params pp=(A,H); KeyGen -> pk=t, sk=r):\n");
  printf("   Setup          %8.2f +/- %6.2f us  %11.0f +/- %9.0f cyc\n", su_m, su_s, su_cm, su_cs);
  printf("   KeyGen         %8.2f +/- %6.2f us  %11.0f +/- %9.0f cyc\n", kg_m, kg_s, kg_cm, kg_cs);
  printf("\n Base path (simplified Dilithium-style, c = H(pk, w, M); no t'):\n");
  printf("   Sign           %8.2f +/- %6.2f us  %11.0f +/- %9.0f cyc\n", sg_m, sg_s, sg_cm, sg_cs);
  printf("   Verify         %8.2f +/- %6.2f us  %11.0f +/- %9.0f cyc\n", vf_m, vf_s, vf_cm, vf_cs);
  printf("\n LAS adaptor path (statement t' folded into the hash, c = H(pk, w+t', M)):\n");
  printf("   PreSign        %8.2f +/- %6.2f us  %11.0f +/- %9.0f cyc\n", ps_m, ps_s, ps_cm, ps_cs);
  printf("   PreVerify      %8.2f +/- %6.2f us  %11.0f +/- %9.0f cyc\n", pv_m, pv_s, pv_cm, pv_cs);
  printf("   Adapt          %8.2f +/- %6.2f us  %11.0f +/- %9.0f cyc\n", ad_m, ad_s, ad_cm, ad_cs);
  printf("   Ext            %8.2f +/- %6.2f us  %11.0f +/- %9.0f cyc\n", ex_m, ex_s, ex_cm, ex_cs);

  printf("\n Adaptor overhead (adaptor op vs the base op it mirrors; core tier, us):\n");
  printf("   PreSign       vs Sign     %8.2f vs %8.2f   (%+.1f%%)\n",
         ps_m, sg_m, 100.0*(ps_m - sg_m)/sg_m);
  printf("   PreVerify     vs Verify   %8.2f vs %8.2f   (%+.1f%%)\n",
         pv_m, vf_m, 100.0*(pv_m - vf_m)/vf_m);
  printf("   Adapt         vs Verify   %8.2f vs %8.2f   (%+.1f%%)\n",
         ad_m, vf_m, 100.0*(ad_m - vf_m)/vf_m);
  printf("   Ext (separate)            %8.2f            (no base analogue)\n", ex_m);

  printf("\n--- COMPUTATION, TIER 2: END-TO-END PACKED (byte API: validating unpack ->\n");
  printf("    core -> pack INSIDE the call -- the boundary upstream sign.c exposes;\n");
  printf("    same repetition scheme and units; Setup has no packed twin) ---\n");
  printf("\n Shared:\n");
  printf("   KeyGen_packed    %8.2f +/- %6.2f us  %11.0f +/- %9.0f cyc\n", kgp_m, kgp_s, kgp_cm, kgp_cs);
  printf("\n Base path (bytes in/out):\n");
  printf("   Sign_packed      %8.2f +/- %6.2f us  %11.0f +/- %9.0f cyc\n", sgp_m, sgp_s, sgp_cm, sgp_cs);
  printf("   Verify_packed    %8.2f +/- %6.2f us  %11.0f +/- %9.0f cyc\n", vfp_m, vfp_s, vfp_cm, vfp_cs);
  printf("\n LAS adaptor path (bytes in/out; las_verify_packed = the on-chain-style entry):\n");
  printf("   PreSign_packed   %8.2f +/- %6.2f us  %11.0f +/- %9.0f cyc\n", psp_m, psp_s, psp_cm, psp_cs);
  printf("   PreVerify_packed %8.2f +/- %6.2f us  %11.0f +/- %9.0f cyc\n", pvp_m, pvp_s, pvp_cm, pvp_cs);
  printf("   Adapt_packed     %8.2f +/- %6.2f us  %11.0f +/- %9.0f cyc\n", adp_m, adp_s, adp_cm, adp_cs);
  printf("   Ext_packed       %8.2f +/- %6.2f us  %11.0f +/- %9.0f cyc\n", exp_m, exp_s, exp_cm, exp_cs);

  printf("\n Adaptor overhead at the packed boundary (adaptor op vs base op, us):\n");
  printf("   PreSign_packed   vs Sign_packed    %8.2f vs %8.2f   (%+.1f%%)\n",
         psp_m, sgp_m, 100.0*(psp_m - sgp_m)/sgp_m);
  printf("   PreVerify_packed vs Verify_packed  %8.2f vs %8.2f   (%+.1f%%)\n",
         pvp_m, vfp_m, 100.0*(pvp_m - vfp_m)/vfp_m);
  printf("   Adapt_packed     vs Verify_packed  %8.2f vs %8.2f   (%+.1f%%)\n",
         adp_m, vfp_m, 100.0*(adp_m - vfp_m)/vfp_m);
  printf("   Ext_packed (separate)              %8.2f            (no base analogue)\n", exp_m);
  printf("   NOTE: these packed-boundary overheads fold in EXTRA CODEC WORK, not just\n");
  printf("   adaptor math -- PreSign_packed/PreVerify_packed additionally decode the\n");
  printf("   pk-sized statement t'; Adapt_packed additionally decodes t' and the witness\n");
  printf("   r' and re-packs the adapted signature.  The CORE tier above isolates the\n");
  printf("   pure adaptor computation (the headline overhead); the codec-cost table\n");
  printf("   below prices the byte boundary itself.\n");

  printf("\n Codec boundary cost (packed minus core, per op, us -- the price of the\n");
  printf(" byte boundary: validating unpack of the inputs + pack of the output;\n");
  printf(" differences of independently measured means, so small values sit within\n");
  printf(" the SDs above):\n");
  printf("   KeyGen_packed    - KeyGen      %+9.2f   (%+6.1f%%)\n", kgp_m - kg_m, 100.0*(kgp_m - kg_m)/kg_m);
  printf("   Sign_packed      - Sign        %+9.2f   (%+6.1f%%)\n", sgp_m - sg_m, 100.0*(sgp_m - sg_m)/sg_m);
  printf("   Verify_packed    - Verify      %+9.2f   (%+6.1f%%)\n", vfp_m - vf_m, 100.0*(vfp_m - vf_m)/vf_m);
  printf("   PreSign_packed   - PreSign     %+9.2f   (%+6.1f%%)\n", psp_m - ps_m, 100.0*(psp_m - ps_m)/ps_m);
  printf("   PreVerify_packed - PreVerify   %+9.2f   (%+6.1f%%)\n", pvp_m - pv_m, 100.0*(pvp_m - pv_m)/pv_m);
  printf("   Adapt_packed     - Adapt       %+9.2f   (%+6.1f%%)\n", adp_m - ad_m, 100.0*(adp_m - ad_m)/ad_m);
  printf("   Ext_packed       - Ext         %+9.2f   (%+6.1f%%)\n", exp_m - ex_m, 100.0*(exp_m - ex_m)/ex_m);

  printf("\n");
  printf("##########################################################################\n");
  printf("# DIAGNOSTICS (secondary; cost-attribution & communication; SAME state)  #\n");
  printf("##########################################################################\n");

  printf("--- A. REJECTION-SAMPLING DISTRIBUTION (%d signing calls each;\n", NSIG);
  printf("       attempts read directly from base_attempts / las_attempts) ---\n");
  printf("   %-22s  %7s %7s %5s %5s %5s %5s\n",
         "operation", "avg", "accept%", "min", "max", "p50", "p95");
  printf("   %-22s  %7.3f %6.1f%% %5lu %5lu %5lu %5lu\n",
         "Base Sign (no t')", avg_base, 100.0 / avg_base,
         att_base[0], att_base[NSIG-1], pct(att_base, NSIG, 50.0), pct(att_base, NSIG, 95.0));
  printf("   %-22s  %7.3f %6.1f%% %5lu %5lu %5lu %5lu\n",
         "LAS PreSign (folds t')", avg_pre, 100.0 / avg_pre,
         att_pre[0], att_pre[NSIG-1], pct(att_pre, NSIG, 50.0), pct(att_pre, NSIG, 95.0));
  printf("   avg = mean attempts/sig; accept%% = 1/avg; both schemes reject under\n");
  printf("   Fiat-Shamir-with-aborts at the bound gamma-kappa (Base Sign) /\n");
  printf("   gamma-kappa-1 (LAS PreSign).\n");
  printf("   Run-validity gates below cover the %d x %d TIMED sign-class calls of EACH\n",
         RUNS, NITER_SIGN);
  printf("   tier of the primary table -- the packed sign-class calls wrap the same\n");
  printf("   rejection loop -- (same 5-sigma check and line format as the Rust drivers):\n");
  rejection_gate("Algorithm 1 Sign", sg_att_tot, (unsigned long)RUNS * NITER_SIGN,
                 las_expected_attempts(BOUND_SIGN));
  rejection_gate("Algorithm 2 PreSign", ps_att_tot, (unsigned long)RUNS * NITER_SIGN,
                 las_expected_attempts(BOUND_PRESIGN));
  rejection_gate("Algorithm 1 Sign (packed tier)", sgp_att_tot,
                 (unsigned long)RUNS * NITER_SIGN,
                 las_expected_attempts(BOUND_SIGN));
  rejection_gate("Algorithm 2 PreSign (packed tier)", psp_att_tot,
                 (unsigned long)RUNS * NITER_SIGN,
                 las_expected_attempts(BOUND_PRESIGN));
  printf("per-attempt diagnostic (rejection-normalised): Sign %.1f +/- %.1f us | "
         "PreSign %.1f +/- %.1f us | overhead %+.1f%%\n\n",
         sg_att_m, sg_att_s, ps_att_m, ps_att_s,
         100.0*(ps_att_m - sg_att_m)/sg_att_m);

  printf("--- B. ADAPT TIMING CLARIFICATION (microseconds, mean +/- SD) ---\n");
  printf("   Adapt checked total      %8.3f +/- %6.3f   (PROTOCOL: las_adapt above, incl. internal PreVerify)\n",
         ad_m, ad_s);
  printf("   witness-add only         %8.3f +/- %6.3f   (DIAGNOSTIC ONLY: z=z_hat+r'; not a protocol op)\n",
         wo_m, wo_s);
  printf("   A real Adapt MUST pre-verify first; the witness-add line is a lower bound,\n");
  printf("   not a usable operation.  Use 'Adapt checked total' for protocol cost.\n\n");

  printf("--- C. COMMUNICATION (packed bytes; BYTE-LEVEL only, NOT EVM gas) ---\n");
  printf("   public key    pk = t              %6zu\n", sz_pk);
  printf("   secret key    sk = r              %6zu\n", sz_sk);
  printf("   statement     Y = t'             %6zu   (%.1f%% of the signature; t' has pk size)\n",
         sz_pk, 100.0*(double)sz_pk/(double)sz_sig);
  printf("   witness       r'                 %6zu   (same packed layout as sk = r)\n", sz_sk);
  printf("   challenge     c_tilde            %6zu   (32-byte H digest)\n", sz_c);
  printf("   response      z (final)          %6zu   (%.1f%% of the signature)\n",
         sz_z, 100.0*(double)sz_z/(double)sz_sig);
  printf("   response      z_hat (pre-sig)    %6zu   (same packed layout as z)\n", sz_z);
  printf("   signature         (c, z)         %6zu\n", sz_sig);
  printf("   pre-signature     (c, z_hat)     %6zu   (same size as the signature)\n", sz_sig);
  printf("   final adapted sig (c, z)         %6zu   (an ordinary signature)\n", sz_sig);
  printf("   commitment    w = A*y            internal computed commitment; hashed into c; not transmitted\n");
  printf("   -- protocol-component catalogue (context; what each object is / why counted or not) --\n");
  printf("   pp=(A,H)  A' seed                %6zu   (public params; only this 32-byte seed is transmitted)\n", sz_seed);
  printf("   pp=(A,H)  expanded A'            %6zu   (n*ell polys; DERIVED from the seed, NOT transmitted)\n", sz_Aexp);
  printf("   y (signing mask, S_gamma)       %6zu   (n+ell polys; INTERNAL, hashed into c via w, NOT transmitted)\n", sz_ymask);
  printf("   w = A*y (commitment)            %6zu   (n polys; INTERNAL, hashed into c, NOT transmitted)\n", sz_pk);
  printf("   pi (NIZK well-formedness proof)    n/a   (paper-level off-chain proof; NOT implemented / NOT measured)\n");
  printf("   tx metadata (addrs/amounts/nonces) excl   (application/ledger layer; EXCLUDED from this accounting)\n");
  printf("   atomic-swap payload (byte-level only):\n");
  printf("     off-chain   = Y + 2*pre-signature        = %6zu   [= t' + 2*(c, z_hat)]\n", sz_pk + 2*sz_sig);
  printf("     settlement  = 2*signature                = %6zu   [= 2*(c, z)]\n", 2*sz_sig);
  printf("     settlement incl. escrowed Y = Y + 2*sig  = %6zu\n", sz_pk + 2*sz_sig);
  printf("   (EVM/keccak gas for on-chain verification is a separate axis; see evm/.)\n\n");

  printf("--- D. COMPONENT MICROBENCHMARKS (microseconds, mean +/- SD) ---\n");
  printf("   Cost-attribution ESTIMATES timing LOCAL COPIES of the inner steps\n");
  printf("   (behaviourally identical to las.c) -- NOT the protocol entry points above.\n");
  printf("   A-product / commitment w = A*y                       %8.3f +/- %6.3f\n", am_m, am_s);
  printf("   challenge hash  c = H(pk, w(+t'), M)                 %8.3f +/- %6.3f\n", ch_m, ch_s);
  printf("   NTT(s)  (n+ell polys; ONCE PER CALL, hoisted)        %8.3f +/- %6.3f\n", nts_m, nts_s);
  printf("   NTT(c)  (one poly; once per attempt / per verify)    %8.3f +/- %6.3f\n", ntc_m, ntc_s);
  printf("   c*r  (one poly; pointwise+invNTT, operands pre-NTT'd) %7.3f +/- %6.3f\n", mu_m, mu_s);
  printf("   c*r  per attempt (NTT(c) + n+ell pointwise+invNTT)   %8.3f +/- %6.3f\n", ma_m, ma_s);
  printf("   norm check  |z|inf over n+ell polys                  %8.3f +/- %6.3f\n", nk_m, nk_s);
  printf("   w + t'   (n polys)                                   %8.3f +/- %6.3f\n", wy_m, wy_s);
  printf("   z_hat + witness r'   (n+ell polys)                   %8.3f +/- %6.3f\n", wo_m, wo_s);
  printf("   c*t  per verify (NTT(c) + n x (NTT(t)+pw+invNTT))    %8.3f +/- %6.3f\n", ct_m, ct_s);
  printf("   KeyGen/Gen: sample r (n+ell ternary polys)           %8.3f +/- %6.3f\n", kr_m, kr_s);
  printf("     (KeyGen = sample r + A*r; A*r is the A-product line above; statement Y gen = KeyGen)\n");
  printf("   Ext: s = z - z_hat   (n+ell polys)                   %8.3f +/- %6.3f\n", es_m, es_s);
  printf("   Ext: A*s   (recompute statement; = an A-product)     %8.3f +/- %6.3f\n", ea_m, ea_s);
  printf("   Ext: t' == A*s check   (n polys)                     %8.3f +/- %6.3f\n", ec_m, ec_s);
  printf("   note: a full Sign/PreSign attempt applies c*r across N_PLUS_ELL = n+ell = %d\n", N_PLUS_ELL);
  printf("         response polynomials; Verify-style challenge multiplication applies\n");
  printf("         c*t across LAS_N = n = %d public-key polynomials.  NTT hoisting\n", LAS_N);
  printf("         follows upstream ref/sign.c: NTT(s) is paid once per CALL (amortised\n");
  printf("         over ~e rejection attempts), NTT(c) once per attempt / per verify.\n");

  printf("\n--- E. NORM-MARGIN DIAGNOSTICS (achieved infinity-norm vs the reject bound;\n");
  printf("       max over the %d sampled accepted signatures from section A) ---\n", NSIG);
  printf("   accepted |z|inf      max %9d   vs accept limit g-k   = %9d   (%.2f%% of band)\n",
         maxz, BOUND_SIGN - 1, 100.0*(double)maxz/(double)(BOUND_SIGN - 1));
  printf("   accepted |z^|inf     max %9d   vs accept limit g-k-1 = %9d   (%.2f%% of band)\n",
         maxzhat, BOUND_PRESIGN - 1, 100.0*(double)maxzhat/(double)(BOUND_PRESIGN - 1));
  printf("   Interpretation: the RESPONSE z/z^ saturates its band (~100%%) -- the bound is\n");
  printf("   the binding constraint and is exactly why ~63%% of attempts are rejected.\n");
  printf("   Contrast the AMHL cumulative WITNESS ||s_j||inf, which is tiny (<=K) vs the\n");
  printf("   g-k-K band: that bound is extremely loose (see bench_app, the K-series table).\n");

  return (int)(g_sink & 0);
}
