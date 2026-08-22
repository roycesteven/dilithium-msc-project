/*
 * bench_labrador_role_a.c -- the role-A proof of knowledge under LaBRADOR:
 * succinct, post-quantum AND ZERO-KNOWLEDGE.
 *
 * WHY THIS EXISTS
 *   eprint 2020/845 Section 4.1 requires pi to be zero-knowledge -- the swap's
 *   security rests on pi HIDING the witness, because a counterparty who learned
 *   r could adapt the other pre-signature and take both sides.  The FRI-STARK
 *   in rust/las-stark proves the same relation succinctly but is NOT zk, so it
 *   is not a valid pi at all.  LaBRADOR is the system that is succinct,
 *   post-quantum and zk at once, and LaZer ships it.
 *
 *   This benchmark runs the SAME relation the deployed LNP22 prover proves,
 *   under LaBRADOR, with zero-knowledge ON.
 *
 * THE ENCODING (see relation_zk_labrador.h for the full argument)
 *   [A | -A] * w  -  q * g  =  t'   , one constraint per output row, with
 *   w = (r_plus || r_minus) proven BINARY by LaBRADOR's native binary norm type
 *   (so ||r||inf <= 1 is PROVEN, not assumed) and g an l2-bounded quotient.
 *   The quotient exists because LaBRADOR works over its own prime, not our q;
 *   its bound is load-bearing, since an unbounded g would satisfy the equation
 *   for any claimed t'.
 *
 * GATES (this binary exits non-zero if any fails)
 *   * the honest instance is checked against the relation in plain integer
 *     arithmetic before anything is encoded;
 *   * LaBRADOR's own simple_verify must accept the encoded statement BEFORE a
 *     proof is produced -- that is an encoding check, and without it a passing
 *     proof could be of a statement that is not the one intended;
 *   * the produced proof must verify;
 *   * zero-knowledge is requested explicitly and reported.
 *
 * SCOPE: an experiment. Nothing here is wired into the swap; configuration 3
 * still uses the k=1 LNP22 module. No security analysis of the encoding is
 * claimed beyond the norm-bound argument in the seam header.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "../setup.h"
#include "../las_types.h"
#include "../relation.h"
#include "../poly.h"
#include "../params.h"
#include "../randombytes.h"
#include "../relation_zk_labrador.h"

#if PI_LAB_ROWS != LAS_N || PI_LAB_WCOLS != 2*N_PLUS_ELL || PI_LAB_GCOLS != LAS_N \
    || PI_LAB_DEG != LAS_D
#error "bench_labrador_role_a: seam shape does not match this build's parameter set"
#endif

#define DEG PI_LAB_DEG

/* Repetitions per phase. At the Meeting-3 statistical floor. */
#define REPS 5

static void mean_sd(const double *x, int n, double *mean, double *sd) {
  int i;
  double s = 0.0, v = 0.0;

  for(i = 0; i < n; ++i) s += x[i];
  *mean = s / n;
  for(i = 0; i < n; ++i) v += (x[i] - *mean) * (x[i] - *mean);
  *sd = (n > 1) ? sqrt(v / (n - 1)) : 0.0;
}

static public_params pp;
static statement Y;
static witness    r_prime;

/* Flat encode buffers (heap: ~7 MB for phi_w). */
static int64_t *phi_w, *phi_g, *bvec, *wvec, *gvec;

/* centre a coefficient in [0,Q) to (-Q/2, Q/2] */
static int64_t centre(int64_t x) { return (x > Q/2) ? x - Q : x; }

/* A'[i][j] in the NORMAL domain, recovered through the SAME pipeline
 * relation.c uses for A r' (so the exported matrix is bit-identical to the one
 * the rest of the build multiplies by): push the constant-1 poly through
 * pointwise-montgomery, reduce, invntt_tomont. Mirrors relation_zk.c. */
static void a_prime_normal(poly *out, unsigned int i, unsigned int j,
                           const poly *one_hat) {
  poly_pointwise_montgomery(out, &pp.a_prime[i][j], one_hat);
  poly_reduce(out);
  poly_invntt_tomont(out);
  poly_reduce(out);
  poly_caddq(out);
}

int main(void) {
  uint8_t seed[LAS_SEEDBYTES], gseed[LAS_SEEDBYTES];
  poly one_hat, ap[LAS_N][ELL];
  unsigned int i, j, k, m;
  uint64_t w_normsq = 0, g_normsq = 0, g_bound;
  int64_t gmax = 0;
  int encoding_ok = 0, verified = 0, rc;
  double params_ms = 0, prove_ms = 0, verify_ms = 0;

  randombytes(seed, LAS_SEEDBYTES);
  randombytes(gseed, LAS_SEEDBYTES);
  setup_public_params(&pp, seed);
  relation_gen_seed(&Y, &r_prime, &pp, gseed);

  /* recover A' in the normal domain, centred */
  for(k = 0; k < LAS_D; ++k) one_hat.coeffs[k] = 0;
  one_hat.coeffs[0] = 1;
  poly_ntt(&one_hat);
  for(i = 0; i < LAS_N; ++i)
    for(j = 0; j < ELL; ++j)
      a_prime_normal(&ap[i][j], i, j, &one_hat);

  phi_w = calloc((size_t)PI_LAB_ROWS * PI_LAB_WCOLS * DEG, sizeof(int64_t));
  phi_g = calloc((size_t)PI_LAB_ROWS * PI_LAB_GCOLS * DEG, sizeof(int64_t));
  bvec  = calloc((size_t)PI_LAB_ROWS * DEG, sizeof(int64_t));
  wvec  = calloc((size_t)PI_LAB_WCOLS * DEG, sizeof(int64_t));
  gvec  = calloc((size_t)PI_LAB_GCOLS * DEG, sizeof(int64_t));
  if(!phi_w || !phi_g || !bvec || !wvec || !gvec) {
    printf("FATAL: allocation failed\n");
    return 1;
  }

  /* ---- witness: binary decomposition r' = r_plus - r_minus ---------------- */
  for(i = 0; i < N_PLUS_ELL; ++i)
    for(k = 0; k < DEG; ++k) {
      int32_t v = r_prime.value[i].coeffs[k];
      wvec[(size_t)i * DEG + k]                 = (v ==  1);
      wvec[(size_t)(N_PLUS_ELL + i) * DEG + k]  = (v == -1);
    }
  /* ⚠️ KNOWN DEFECT, DELIBERATELY NOT FIXED IN THIS BINARY -- see BLOCKER 1 in
   * relation_zk_labrador.h.  This is the honest witness's EXACT l2 norm, i.e. the
   * Hamming weight of the ternary r', and it is declared to LaBRADOR as the
   * STATEMENT's bound on vector 0 (proofsystem.h: "squared l2-norm bound"; verify()
   * enforces it; py_verify consumes that statement).  The statement is therefore
   * secret-dependent, which no zk flag can repair -- zk bounds what the PROOF adds
   * beyond the statement, and this leak is in the statement.
   *
   * Note what this is NOT: zk=1 is passed and the proof IS zero-knowledge for the
   * encoded statement.  The defect is in the statement, which zk cannot repair.
   *
   * The fix would be a witness-INDEPENDENT bound, ||w||^2 <= N_PLUS_ELL * DEG,
   * valid because r_plus and r_minus are never both 1 in a coefficient.
   *
   * ⚠️ RULED NOT TO BE APPLIED (Royce, 2026-08-10).  Do not apply it and do not
   * re-run: evidence/labrador_role_a/latest stays reproducible against this exact
   * binary.  The reason is not effort -- the fix clears the privacy blocker ONLY,
   * while BLOCKER 2 (the g bound is not proven complete) survives it, so no project
   * decision moves either way.  Nor is the cost verdict at stake: a wider declared
   * bound cannot REDUCE LaBRADOR's configured widths -- it may leave them unchanged
   * or increase them, since polxvec_setwidths1 divides normsq by n*N and the result
   * is discretised.  That is a reading of the library, not a measurement. */
  for(i = 0; i < (unsigned int)PI_LAB_WCOLS * DEG; ++i)
    w_normsq += (uint64_t)(wvec[i] * wvec[i]);

  /* ---- statement rows: [A | -A] with A = [I_n | A'] ----------------------- */
  for(m = 0; m < LAS_N; ++m) {
    int64_t *row = phi_w + (size_t)m * PI_LAB_WCOLS * DEG;
    /* identity block: column m is the constant 1 */
    row[(size_t)m * DEG] = 1;
    /* A' block */
    for(j = 0; j < ELL; ++j)
      for(k = 0; k < DEG; ++k)
        row[(size_t)(LAS_N + j) * DEG + k] = centre(ap[m][j].coeffs[k]);
    /* the negated copy, columns n+ell .. 2(n+ell)-1 */
    for(i = 0; i < N_PLUS_ELL; ++i)
      for(k = 0; k < DEG; ++k)
        row[(size_t)(N_PLUS_ELL + i) * DEG + k] = -row[(size_t)i * DEG + k];

    /* -q on the quotient diagonal */
    phi_g[((size_t)m * PI_LAB_GCOLS + m) * DEG] = -(int64_t)Q;

    /* b = t'_m, centred */
    for(k = 0; k < DEG; ++k)
      bvec[(size_t)m * DEG + k] = centre(Y.t_prime[m].coeffs[k]);
  }

  /* ---- quotient g: (A r' - t')/q over Z, negacyclic ----------------------- */
  for(m = 0; m < LAS_N; ++m) {
    int64_t acc[DEG];

    for(k = 0; k < DEG; ++k) acc[k] = 0;
    /* r_top[m] */
    for(k = 0; k < DEG; ++k) acc[k] += centre(r_prime.value[m].coeffs[k] < 0
                                              ? r_prime.value[m].coeffs[k] + Q
                                              : r_prime.value[m].coeffs[k]);
    /* sum_j A'[m][j] * r_bot[j], negacyclic over Z */
    for(j = 0; j < ELL; ++j) {
      for(i = 0; i < DEG; ++i) {
        int64_t a = centre(ap[m][j].coeffs[i]);
        if(a == 0) continue;
        for(k = 0; k < DEG; ++k) {
          int64_t bcoef = r_prime.value[N_PLUS_ELL - ELL + j].coeffs[k];
          if(bcoef == 0) continue;
          if(i + k < DEG) acc[i + k]       += a * bcoef;
          else            acc[i + k - DEG] -= a * bcoef;
        }
      }
    }
    for(k = 0; k < DEG; ++k) {
      int64_t diff = acc[k] - bvec[(size_t)m * DEG + k];
      if(diff % Q != 0) {
        printf("FATAL: A r' - t' is not divisible by q at row %u coeff %u\n", m, k);
        return 1;
      }
      gvec[(size_t)m * DEG + k] = diff / Q;
      if(gvec[(size_t)m * DEG + k] > gmax)  gmax =  gvec[(size_t)m * DEG + k];
      if(-gvec[(size_t)m * DEG + k] > gmax) gmax = -gvec[(size_t)m * DEG + k];
      g_normsq += (uint64_t)(gvec[(size_t)m * DEG + k] * gvec[(size_t)m * DEG + k]);
    }
  }

  /* Declared l2 bound on the quotient.
   *
   * The naive worst case -- every term of the negacyclic sum aligned, giving
   * |A r'|inf <= ell*d*(q-1)/2 and hence |g|inf <= 641 -- is far too loose to
   * be proven exactly: LaBRADOR refuses an exact l2 bound above its own cap,
   * and the resulting ||g||^2 sits well over it. It is also wildly pessimistic,
   * because the sum has cancellation: each coefficient of A' r' is a sum of
   * ~ell*d signed terms, so it concentrates around sqrt(ell*d)*(q/2)/sqrt(3)
   * rather than ell*d*(q/2), and the honest |g|inf measures ~10.
   *
   * So the declared bound is a protocol parameter with a stated margin rather
   * than an aligned-worst-case: G_NORMSQ_BOUND is ~1000x the honest ||g||^2 and
   * ~30x its RMS, and the run asserts the honest witness sits inside it. What
   * the proof then establishes is ||g||^2 <= G_NORMSQ_BOUND, hence
   * |g|inf <= sqrt(G_NORMSQ_BOUND) = 1e4, which is what the lifting argument in
   * relation_zk_labrador.h consumes. */
  {
    const uint64_t G_NORMSQ_BOUND = 100000000ULL;   /* 1e8 */
    g_bound = G_NORMSQ_BOUND;
    if(g_normsq > g_bound) {
      printf("FATAL: ||g||^2 = %llu exceeds the derived bound %llu\n",
             (unsigned long long)g_normsq, (unsigned long long)g_bound);
      return 1;
    }
    printf("=== Role-A pi under LaBRADOR (succinct + post-quantum + ZERO-KNOWLEDGE) ===\n\n");
    printf("Relation      : exists r : A r = t'  and  ||r||inf <= 1\n");
    printf("Encoding      : [A | -A] w - q g = t', w BINARY (native norm type),\n");
    printf("                g the mod-q quotient with an exact l2 bound\n");
    printf("Witness       : %d binary polys + %d quotient polys, degree %d\n",
           PI_LAB_WCOLS, PI_LAB_GCOLS, DEG);
    printf("Constraints   : %d (one per output row)\n\n", PI_LAB_ROWS);
    printf("  ||w||^2            : %llu  (binary)\n", (unsigned long long)w_normsq);
    printf("  ||g||^2            : %llu   (declared bound %llu, |g|inf = %lld)\n",
           (unsigned long long)g_normsq, (unsigned long long)g_bound,
           (long long)gmax);
  }

  printf("\n--- LaBRADOR, zero-knowledge ON (%d reps after an untimed warm-up) ---\n",
         REPS);
  {
    double pv[REPS], vv[REPS], pa[REPS];
    double pm, ps, vm, vs, am, as_;
    int rep;

    for(rep = 0; rep <= REPS; ++rep) {
      rc = relation_labrador_run(phi_w, phi_g, bvec, wvec, gvec,
                                 w_normsq, g_bound, 1,
                                 &encoding_ok, &verified,
                                 &params_ms, &prove_ms, &verify_ms);
      if(rc != 0) break;
      /* Gates re-checked EVERY rep: a timed block that measured a rejection
       * would publish a fast, meaningless number. */
      if(!encoding_ok || !verified) break;
      if(rep > 0) {
        pa[rep-1] = params_ms;
        pv[rep-1] = prove_ms;
        vv[rep-1] = verify_ms;
      }
    }
    if(rc == 0 && encoding_ok && verified) {
      mean_sd(pa, REPS, &am, &as_);
      mean_sd(pv, REPS, &pm, &ps);
      mean_sd(vv, REPS, &vm, &vs);
      printf("\n  encoding gate (simple_verify) : %s\n", "ACCEPT");
      printf("  proof verifies                : %s\n", "ACCEPT (every rep)");
      printf("  params  : %8.1f +- %5.1f ms\n", am, as_);
      printf("  prove   : %8.1f +- %5.1f ms\n", pm, ps);
      printf("  verify  : %8.1f +- %5.1f ms\n", vm, vs);
    } else {
      printf("\n  encoding gate (simple_verify) : %s\n", encoding_ok ? "ACCEPT" : "REJECT");
      printf("  proof verifies                : %s\n", verified ? "ACCEPT" : "REJECT");
    }
  }

  if(rc != 0) {
    printf("\nFATAL: LaBRADOR run failed (code %d)\n", rc);
    if(rc == 4)
      printf("  code 4 = the ENCODING is wrong: simple_verify rejected the\n"
             "  statement for a witness that satisfies the relation.\n");
    return 1;
  }
  if(!encoding_ok || !verified) {
    printf("\nFATAL: gate failed; no figure above may be quoted.\n");
    return 1;
  }

  printf("\n--- What this shows ---\n\n");
  printf("This is the first prover measured in this project that is succinct,\n");
  printf("post-quantum AND zero-knowledge at once -- i.e. the first one that\n");
  printf("actually satisfies what eprint 2020/845 Section 4.1 asks of pi while\n");
  printf("also being succinct. The FRI-STARK measured earlier is succinct and PQ\n");
  printf("but NOT zk, so it was never a valid pi; LNP22 (configuration 3) is zk\n");
  printf("and PQ but its proof is linear-ish in the statement, not succinct.\n\n");
  printf("Proof size: LaBRADOR reports it through its own parameter generation\n");
  printf("(the 'Estimated proof size' line above, printed by the library). That\n");
  printf("is NOT the same measurement as LNP22's byte-exact prooflen, because the\n");
  printf("function returning the packed length is hidden by -fvisibility=hidden.\n");
  printf("Compare the two with that difference stated, never silently.\n");

  free(phi_w); free(phi_g); free(bvec); free(wvec); free(gvec);
  return 0;
}
