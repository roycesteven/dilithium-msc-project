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
 *          measured over the TIMED sign-class calls is hard-checked against the exact
 *          expectation las_expected_attempts() within 5 sigma -- a run whose restart
 *          rate deviates from theory aborts instead of producing invalid evidence.
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
 * witness exactly -- so no failure or early-return path is ever timed.
 *
 * Build (Makefile sets -DLAS_N/-DLAS_ELL/-DLAS_KAPPA for each parameter set).
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
#include <math.h>
#include <time.h>
#include "../basesig.h"     /* BASE path: base_keygen/base_sign/base_verify + base_attempts */
#include "../las.h"         /* ADAPTOR path: las_presign/preverify/adapt/ext + las_attempts  */
#include "../params.h"      /* N, Q */
#include "../poly.h"        /* poly arithmetic for the component microbenchmarks    */
#include "../fips202.h"     /* SHAKE256 for the local challenge-hash copy           */

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
static double g_att_runs[RUNS];      /* per-rep per-ATTEMPT us (set by MEASURE_SIGN) */
static unsigned long g_att_total;    /* attempts over the RUNS x NITER_SIGN timed calls */
static volatile long g_sink;

/* Run the op for RUNS x niter_; leave mean/SD (us) in g_mean/g_sd.
 * Variadic so op bodies may contain unparenthesised commas. */
#define MEASURE(niter_, ...) do {                                     \
    int br_, bi_;                                                     \
    for(br_ = 0; br_ < RUNS; ++br_) {                                 \
      double bt0_ = now_us();                                         \
      for(bi_ = 0; bi_ < (niter_); ++bi_) { __VA_ARGS__; }            \
      g_runs[br_] = (now_us() - bt0_) / (niter_);                     \
    }                                                                \
    stats(g_runs, RUNS, &g_mean, &g_sd);                             \
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
      for(bi_ = 0; bi_ < NITER_SIGN; ++bi_) { __VA_ARGS__; }          \
      g_runs[br_] = (now_us() - bt0_) / NITER_SIGN;                   \
      ba_ = (counter_) - ba_;                                         \
      g_att_runs[br_] = g_runs[br_] * NITER_SIGN / (double)ba_;       \
      g_att_total += ba_;                                             \
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

static void mc_pack_poly_canon(uint8_t out[N*4], const poly *a) {
  unsigned int i;
  uint32_t x;
  poly t = *a;
  poly_reduce(&t);
  poly_caddq(&t);
  for(i = 0; i < N; ++i) {
    x = (uint32_t)t.coeffs[i];
    out[4*i+0] = (uint8_t)x;
    out[4*i+1] = (uint8_t)(x >> 8);
    out[4*i+2] = (uint8_t)(x >> 16);
    out[4*i+3] = (uint8_t)(x >> 24);
  }
}

/* out = a*b mod (X^N+1, Q), centred, via NTT (one challenge*response product). */
static void mc_polymul(poly *out, const poly *a, const poly *b) {
  poly ah = *a, bh = *b;
  poly_ntt(&ah);
  poly_ntt(&bh);
  poly_pointwise_montgomery(out, &ah, &bh);
  poly_invntt_tomont(out);
  poly_reduce(out);
}

/* w = A*v = v_top + A'*v_bot, A=[I|A'], A' (pp->mat) already in NTT domain. */
static void mc_Amul(poly w[LAS_N], const las_pp *pp, const poly v[LAS_M]) {
  poly vhat[LAS_ELL], tmp, acc;
  unsigned int i, j, k;

  for(j = 0; j < LAS_ELL; ++j) {
    vhat[j] = v[LAS_N + j];
    poly_ntt(&vhat[j]);
  }
  for(i = 0; i < LAS_N; ++i) {
    for(k = 0; k < N; ++k)
      acc.coeffs[k] = 0;
    for(j = 0; j < LAS_ELL; ++j) {
      poly_pointwise_montgomery(&tmp, &pp->mat[i][j], &vhat[j]);
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

  for(i = 0; i < N; ++i)
    c->coeffs[i] = 0;
  for(i = N - LAS_KAPPA; i < N; ++i) {
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
static void mc_hash_challenge(poly *c, const las_pk *pk, const poly commit[LAS_N],
                              const uint8_t *m, size_t mlen) {
  keccak_state state;
  uint8_t buf[N*4];
  uint8_t seed[LAS_SEEDBYTES];
  unsigned int i;

  shake256_init(&state);
  for(i = 0; i < LAS_N; ++i) {
    mc_pack_poly_canon(buf, &pk->t[i]);
    shake256_absorb(&state, buf, N*4);
  }
  for(i = 0; i < LAS_N; ++i) {
    mc_pack_poly_canon(buf, &commit[i]);
    shake256_absorb(&state, buf, N*4);
  }
  shake256_absorb(&state, m, mlen);
  shake256_finalize(&state);
  shake256_squeeze(seed, LAS_SEEDBYTES, &state);
  mc_challenge(c, seed);
}

/* Reject if any component has ||.||inf >= B (the rejection-loop norm check). */
static int mc_chknorm_vec(const poly z[LAS_M], int32_t B) {
  unsigned int j;
  for(j = 0; j < LAS_M; ++j)
    if(poly_chknorm(&z[j], B))
      return 1;
  return 0;
}

/* commit = w + Y over the n statement polynomials (PreSign/PreVerify inner step). */
static void mc_add_wY(poly out[LAS_N], const poly w[LAS_N], const las_pk *Y) {
  unsigned int j;
  for(j = 0; j < LAS_N; ++j) {
    poly_add(&out[j], &w[j], &Y->t[j]);
    poly_reduce(&out[j]);
    poly_caddq(&out[j]);
  }
}

/* z = z_hat + r' over the n+l response polynomials (the Adapt witness add).
 * Identical to las_adapt()'s body AFTER its mandatory PreVerify -- see section B. */
static void mc_witness_add(las_sig *out, const las_sig *presig, const las_sk *y) {
  unsigned int j;
  out->c = presig->c;
  for(j = 0; j < LAS_M; ++j) {
    poly_add(&out->z[j], &presig->z[j], &y->s[j]);
    poly_reduce(&out->z[j]);
  }
}

/* coefficientwise poly equality (the t' == A*s check inside Ext). */
static int mc_poly_equal(const poly *a, const poly *b) {
  unsigned int i;
  for(i = 0; i < N; ++i)
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
  for(i = 0; i < N; ++i) { v = a->coeffs[i]; if(v < 0) v = -v; if(v > mx) mx = v; }
  return mx;
}
static int32_t mc_max_abs_vec(const poly z[LAS_M]) {
  int32_t mx = 0, v;
  unsigned int j;
  for(j = 0; j < LAS_M; ++j) { v = mc_max_abs(&z[j]); if(v > mx) mx = v; }
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

  while(ctr < N) {
    if(pos >= SHAKE256_RATE) { shake256_squeezeblocks(buf, 1, &state); pos = 0; }
    byte = buf[pos++];
    for(s = 0; s < 4 && ctr < N; ++s) {
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
  las_pp  pp, pp2;                                /* pp = canonical; pp2 = Setup scratch */
  las_pk  pk, Y, pk2;                             /* pk2 = KeyGen scratch                */
  las_sk  sk, yy, sk2, yext;                      /* sk2 = KeyGen scratch                */
  las_sig sig, presig, adapted, tmp;
  poly    w[LAS_N], wY[LAS_N], cc, cr;
  unsigned int j;
  int     i;

  /* PRIMARY protocol timings (mean, sd) */
  double su_m, su_s, kg_m, kg_s, sg_m, sg_s, vf_m, vf_s;
  double ps_m, ps_s, pv_m, pv_s, ad_m, ad_s, ex_m, ex_s;
  /* per-attempt (rejection-normalised) series + attempt totals over the TIMED
   * sign-class calls (set via MEASURE_SIGN; feeds the rejection gate) */
  double sg_att_m, sg_att_s, ps_att_m, ps_att_s;
  unsigned long sg_att_tot, ps_att_tot;

  /* DIAGNOSTIC timings (mean, sd): witness-add only + component microbenchmarks */
  double wo_m, wo_s;
  double am_m, am_s, ch_m, ch_s, mu_m, mu_s, ma_m, ma_s, nk_m, nk_s, wy_m, wy_s;
  double ct_m, ct_s;                       /* c*t over LAS_N pk polys (Verify side) */
  double kr_m, kr_s;                        /* KeyGen / Gen: sample r (ternary)     */
  double es_m, es_s, ea_m, ea_s, ec_m, ec_s; /* Ext breakdown: s=z-z^, A*s, check  */
  int32_t maxz = 0, maxzhat = 0;            /* achieved norm vs the reject bound    */

  /* DIAGNOSTIC rejection-sampling distribution */
  unsigned long tot_base = 0, tot_pre = 0;
  double avg_base, avg_pre;

  /* packed component sizes (bytes) for THIS parameter set */
  int    pk_bits = ceil_log2((double)Q);                       /* 23 for Q<2^23 */
  int    z_bits  = ceil_log2(2.0*(LAS_GAMMA - LAS_KAPPA) + 1.0);
  size_t sz_pk   = (size_t)(LAS_N * N * pk_bits + 7) / 8;
  size_t sz_sk   = (size_t)(LAS_M * N * 2 + 7) / 8;
  size_t sz_c    = (size_t)(N * 2 + 7) / 8;
  size_t sz_z    = (size_t)(LAS_M * N * z_bits + 7) / 8;
  size_t sz_sig  = sz_c + sz_z;
  /* protocol-component catalogue sizes (context rows; see diagnostic section C) */
  int    y_bits  = ceil_log2(2.0*(double)LAS_GAMMA + 1.0);      /* mask y in S_gamma   */
  size_t sz_seed = LAS_SEEDBYTES;                              /* the public A' seed  */
  size_t sz_Aexp = (size_t)((size_t)LAS_N * LAS_ELL * N * pk_bits + 7) / 8; /* expanded A' */
  size_t sz_ymask= (size_t)((size_t)LAS_M * N * y_bits + 7) / 8; /* signing mask (internal) */

  for(j = 0; j < LAS_SEEDBYTES; ++j) ppseed[j] = (uint8_t)j;

  /* ---- ONE setup + ONE consistent state per run (used by EVERY measurement) ----
   * Public parameters, a key pair, a statement/witness (Y = A*yy is literally another
   * key pair), and the BASE signature / LAS pre-signature / adapted signature, all
   * derived from THAT key.  KeyGen and Setup are timed below into SCRATCH objects so
   * the canonical state stays intact for the sanity gate and the remaining timings. */
  las_setup(&pp, ppseed);                                    /* Setup (public params) */
  base_keygen(&pk, &sk, &pp);                                /* BASE KeyGen           */
  base_keygen(&Y,  &yy, &pp);                                /* statement/witness     */
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
  if(base_verify(&sig, m, mlen, &pk, &pp) != 0 ||              /* base sig verifies     */
     las_preverify(&presig, m, mlen, &Y, &pk, &pp) != 0 ||    /* presig pre-verifies   */
     base_verify(&presig, m, mlen, &pk, &pp) == 0 ||          /* presig is NOT a base sig */
     base_verify(&adapted, m, mlen, &pk, &pp) != 0 ||         /* adapted = base sig    */
     las_ext(&yext, &adapted, &presig, &Y, &pp) != 0 ||       /* Ext succeeds          */
     !sk_equal(&yext, &yy)) {                                 /* exact witness recover */
    printf("FATAL: benchmark state inconsistent before timing\n");
    return 1;
  }

  /* ============================ PRIMARY TIMINGS ============================
   * Protocol-level operations.  Producing operations write to SCRATCH (pp2/pk2/sk2/
   * tmp/yext) so the canonical state is never mutated; verifies read canonical objects. */
  MEASURE(NITER_FAST, las_setup(&pp2, ppseed));              su_m = g_mean; su_s = g_sd;
  MEASURE(NITER_FAST, base_keygen(&pk2, &sk2, &pp));         kg_m = g_mean; kg_s = g_sd;
  /* BASE path (basesig.c).  Sign-class: MEASURE_SIGN also captures the
   * per-attempt series and the attempt total over the timed calls. */
  MEASURE_SIGN(base_attempts, base_sign(&tmp, m, mlen, &pk, &sk, &pp));
  sg_m = g_mean; sg_s = g_sd;
  stats(g_att_runs, RUNS, &sg_att_m, &sg_att_s);
  sg_att_tot = g_att_total;
  MEASURE(NITER_FAST, g_sink += base_verify(&sig, m, mlen, &pk, &pp)); vf_m = g_mean; vf_s = g_sd;
  /* LAS ADAPTOR path (las.c). */
  MEASURE_SIGN(las_attempts, las_presign(&tmp, m, mlen, &Y, &pk, &sk, &pp));
  ps_m = g_mean; ps_s = g_sd;
  stats(g_att_runs, RUNS, &ps_att_m, &ps_att_s);
  ps_att_tot = g_att_total;
  MEASURE(NITER_FAST, g_sink += las_preverify(&presig, m, mlen, &Y, &pk, &pp)); pv_m = g_mean; pv_s = g_sd;
  MEASURE(NITER_FAST, g_sink += las_adapt(&tmp, &presig, m, mlen, &Y, &yy, &pk, &pp)); ad_m = g_mean; ad_s = g_sd;
  MEASURE(NITER_FAST, g_sink += las_ext(&yext, &adapted, &presig, &Y, &pp)); ex_m = g_mean; ex_s = g_sd;

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
    las_presign(&tmp, m, mlen, &Y, &pk, &sk, &pp);
    att_pre[i] = las_attempts - before;
    tot_pre   += att_pre[i];
    v = mc_max_abs_vec(tmp.z); if(v > maxzhat) maxzhat = v; /* achieved |z^|inf */
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

  mc_Amul(w, &pp, sk.s);                          /* prime a commitment for the hash */
  MEASURE(NITER_FAST, { mc_Amul(w, &pp, sk.s);                       g_sink += w[0].coeffs[0]; });
  am_m = g_mean; am_s = g_sd;
  MEASURE(NITER_FAST, { mc_hash_challenge(&cc, &pk, w, m, mlen);     g_sink += cc.coeffs[0]; });
  ch_m = g_mean; ch_s = g_sd;
  MEASURE(NITER_FAST, { mc_polymul(&cr, &cc, &sk.s[0]);              g_sink += cr.coeffs[0]; });
  mu_m = g_mean; mu_s = g_sd;
  MEASURE(NITER_FAST, { for(j = 0; j < LAS_M; ++j) { mc_polymul(&cr, &cc, &sk.s[j]); g_sink += cr.coeffs[0]; } });
  ma_m = g_mean; ma_s = g_sd;
  MEASURE(NITER_FAST, g_sink += mc_chknorm_vec(presig.z, LAS_BOUND_PRESIGN));
  nk_m = g_mean; nk_s = g_sd;
  MEASURE(NITER_FAST, { mc_add_wY(wY, w, &Y);                        g_sink += wY[0].coeffs[0]; });
  wy_m = g_mean; wy_s = g_sd;

  /* ---- additional component attribution (Verify-side / KeyGen / Ext) ----
   * c*t over the LAS_N public-key polys: the per-poly challenge product that
   * Verify and PreVerify pay (the verify-side analogue of c*r). */
  MEASURE(NITER_FAST, { for(j = 0; j < LAS_N; ++j) { mc_polymul(&cr, &cc, &pk.t[j]); g_sink += cr.coeffs[0]; } });
  ct_m = g_mean; ct_s = g_sd;
  /* KeyGen / Gen "sample r": LAS_M ternary polys (statement Y generation = KeyGen).
   * The other half of KeyGen, A*r, is the A-product line above. */
  MEASURE(NITER_FAST, { for(j = 0; j < LAS_M; ++j) { mc_sample_ternary(&sk2.s[j], ppseed, LAS_SEEDBYTES, (uint16_t)j); g_sink += sk2.s[0].coeffs[0]; } });
  kr_m = g_mean; kr_s = g_sd;
  /* Ext breakdown: s = z - z^ (n+ell polys); A*s; then t' == A*s check (n polys). */
  MEASURE(NITER_FAST, { for(j = 0; j < LAS_M; ++j) { poly_sub(&yext.s[j], &adapted.z[j], &presig.z[j]); poly_reduce(&yext.s[j]); } g_sink += yext.s[0].coeffs[0]; });
  es_m = g_mean; es_s = g_sd;
  MEASURE(NITER_FAST, { mc_Amul(w, &pp, yext.s);                     g_sink += w[0].coeffs[0]; });
  ea_m = g_mean; ea_s = g_sd;
  MEASURE(NITER_FAST, { int eq = 1; for(j = 0; j < LAS_N; ++j) eq &= mc_poly_equal(&w[j], &Y.t[j]); g_sink += eq; });
  ec_m = g_mean; ec_s = g_sd;

  /* ================================ report ================================= */
  printf("==========================================================================\n");
  printf(" LAS parameter set: n=%d ell=%d kappa=%d gamma=%d  (N=%d, Q=%d)\n",
         LAS_N, LAS_ELL, LAS_KAPPA, LAS_GAMMA, N, Q);
  printf("   M = n + ell = %d   (dim of sk=r, witness r', mask y, responses z_hat/z)\n", LAS_M);
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
  printf(" One setup and one consistent state per run; primary protocol timings first,\n");
  printf(" then diagnostics from the SAME state (cost-attribution / communication aids).\n\n");

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
  printf("   pi (paper-level off-chain proof of well-formedness) is NOT implemented/measured here.\n\n");

  printf("##########################################################################\n");
  printf("# PRIMARY (protocol-level) timings -- the headline adaptor-overhead table #\n");
  printf("##########################################################################\n");
  printf("--- COMPUTATION (microseconds, mean +/- sample SD) ---\n");
  printf("\n Shared (public params pp=(A,H); KeyGen -> pk=t, sk=r):\n");
  printf("   Setup          %8.2f +/- %6.2f\n", su_m, su_s);
  printf("   KeyGen         %8.2f +/- %6.2f\n", kg_m, kg_s);
  printf("\n Base path (simplified Dilithium-style, c = H(pk, w, M); no t'):\n");
  printf("   Sign           %8.2f +/- %6.2f\n", sg_m, sg_s);
  printf("   Verify         %8.2f +/- %6.2f\n", vf_m, vf_s);
  printf("\n LAS adaptor path (statement t' folded into the hash, c = H(pk, w+t', M)):\n");
  printf("   PreSign        %8.2f +/- %6.2f\n", ps_m, ps_s);
  printf("   PreVerify      %8.2f +/- %6.2f\n", pv_m, pv_s);
  printf("   Adapt          %8.2f +/- %6.2f\n", ad_m, ad_s);
  printf("   Ext            %8.2f +/- %6.2f\n", ex_m, ex_s);

  printf("\n Adaptor overhead (adaptor op vs the base op it mirrors):\n");
  printf("   PreSign       vs Sign     %8.2f vs %8.2f   (%+.1f%%)\n",
         ps_m, sg_m, 100.0*(ps_m - sg_m)/sg_m);
  printf("   PreVerify     vs Verify   %8.2f vs %8.2f   (%+.1f%%)\n",
         pv_m, vf_m, 100.0*(pv_m - vf_m)/vf_m);
  printf("   Adapt         vs Verify   %8.2f vs %8.2f   (%+.1f%%)\n",
         ad_m, vf_m, 100.0*(ad_m - vf_m)/vf_m);
  printf("   Ext (separate)            %8.2f            (no base analogue)\n", ex_m);

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
  printf("   Run-validity gates below cover the %d x %d TIMED sign-class calls from the\n",
         RUNS, NITER_SIGN);
  printf("   primary table (same 5-sigma check and line format as the Rust drivers):\n");
  rejection_gate("Algorithm 1 Sign", sg_att_tot, (unsigned long)RUNS * NITER_SIGN,
                 las_expected_attempts(LAS_BOUND_SIGN));
  rejection_gate("Algorithm 2 PreSign", ps_att_tot, (unsigned long)RUNS * NITER_SIGN,
                 las_expected_attempts(LAS_BOUND_PRESIGN));
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
  printf("   challenge     c                  %6zu\n", sz_c);
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
  printf("   c*r  (one response polynomial; c is sparse)          %8.3f +/- %6.3f\n", mu_m, mu_s);
  printf("   c*r  (all LAS_M response polynomials)                %8.3f +/- %6.3f\n", ma_m, ma_s);
  printf("   norm check  |z|inf over n+ell polys                  %8.3f +/- %6.3f\n", nk_m, nk_s);
  printf("   w + t'   (n polys)                                   %8.3f +/- %6.3f\n", wy_m, wy_s);
  printf("   z_hat + witness r'   (n+ell polys)                   %8.3f +/- %6.3f\n", wo_m, wo_s);
  printf("   c*t  (all LAS_N public-key polys; Verify/PreVerify)  %8.3f +/- %6.3f\n", ct_m, ct_s);
  printf("   KeyGen/Gen: sample r (n+ell ternary polys)           %8.3f +/- %6.3f\n", kr_m, kr_s);
  printf("     (KeyGen = sample r + A*r; A*r is the A-product line above; statement Y gen = KeyGen)\n");
  printf("   Ext: s = z - z_hat   (n+ell polys)                   %8.3f +/- %6.3f\n", es_m, es_s);
  printf("   Ext: A*s   (recompute statement; = an A-product)     %8.3f +/- %6.3f\n", ea_m, ea_s);
  printf("   Ext: t' == A*s check   (n polys)                     %8.3f +/- %6.3f\n", ec_m, ec_s);
  printf("   note: a full Sign/PreSign attempt applies c*r across LAS_M = n+ell = %d\n", LAS_M);
  printf("         response polynomials; Verify-style challenge multiplication applies\n");
  printf("         c*t across LAS_N = n = %d public-key polynomials.\n", LAS_N);

  printf("\n--- E. NORM-MARGIN DIAGNOSTICS (achieved infinity-norm vs the reject bound;\n");
  printf("       max over the %d sampled accepted signatures from section A) ---\n", NSIG);
  printf("   accepted |z|inf      max %9d   vs accept limit g-k   = %9d   (%.2f%% of band)\n",
         maxz, LAS_BOUND_SIGN - 1, 100.0*(double)maxz/(double)(LAS_BOUND_SIGN - 1));
  printf("   accepted |z^|inf     max %9d   vs accept limit g-k-1 = %9d   (%.2f%% of band)\n",
         maxzhat, LAS_BOUND_PRESIGN - 1, 100.0*(double)maxzhat/(double)(LAS_BOUND_PRESIGN - 1));
  printf("   Interpretation: the RESPONSE z/z^ saturates its band (~100%%) -- the bound is\n");
  printf("   the binding constraint and is exactly why ~63%% of attempts are rejected.\n");
  printf("   Contrast the AMHL cumulative WITNESS ||s_j||inf, which is tiny (<=K) vs the\n");
  printf("   g-k-K band: that bound is extremely loose (see bench_app, the K-series table).\n");

  return (int)(g_sink & 0);
}
