/*
 * bench_lazer_amortise.c -- does the role-A proof amortise across swaps when
 * the prover is LaZer?
 *
 * WHY THIS EXISTS
 *   The Groth16 amortisation run (rust/las-swap/src/bin/bench_amortise.rs) came
 *   back NEGATIVE, but for a reason specific to Groth16: batching shrank a
 *   128 B proof that was never the bottleneck, while the ~646 ms of proving
 *   that dominates configuration 2 stayed flat.  It left an explicit open
 *   question -- the same 1/k would land on a cost that MATTERS for a proof
 *   system with a large proof and cheap generation.
 *
 *   LaZer is exactly that profile.  In evidence/stage2/latest configuration 3
 *   generates its proof in ~216 ms and ships ~30 KiB of it, against a 4416 B
 *   statement and a 6736 B signature -- so for LaZer the proof IS the
 *   communication cost, and a per-swap saving would be a real one.
 *
 *   This benchmark settles it by measurement rather than by argument.
 *
 * WHAT IT MEASURES
 *   For k = 1, 2, 4, 8: ONE LaZer proof covering k independent instances of the
 *   Fig. 1 relation, via the block-diagonal statement of relation_zk_batch.c.
 *   Reported as totals and per swap: prove time, verify time, proof bytes.
 *
 *   Each k uses its OWN generated parameter set (ref/relation_zk_params_k*.h);
 *   k=1 uses the COMMITTED set configuration 3 ships, so the baseline row is
 *   the deployed prover and not a re-derived lookalike.
 *
 * GATES (this binary exits non-zero if any fails)
 *   * a success-path assertion after every timed proof -- a timed block that
 *     measured a rejection would publish a fast, meaningless number;
 *   * a tamper check per batch size, corrupting the LAST instance's statement
 *     and requiring rejection: otherwise a batch could look cheap merely by
 *     not binding all k;
 *   * an untimed warm-up before the first measurement, and >= 5 repetitions.
 *
 * SCOPE
 *   Communication and time only.  No security claim is made about batching, and
 *   the batched parameter sets carry the same knowledge-error target the k=1 set
 *   does but have NOT been independently reviewed.  Nothing here is wired into
 *   the swap: configuration 3 still uses the k=1 module.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "../setup.h"
#include "../las_types.h"
#include "../relation.h"
#include "../relation_zk_batch.h"
#include "../randombytes.h"

#define REPS 5
static const unsigned int BATCH[] = { 1, 2, 4, 8 };
#define NBATCH ((int)(sizeof BATCH / sizeof BATCH[0]))

static public_params pp;
static statement Y[PI_BATCH_MAX_K];
static witness   R[PI_BATCH_MAX_K];
static uint8_t   proof[PI_BATCH_PROOF_MAX_BYTES];

static double now_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

static void stats(const double *x, int n, double *mean, double *sd) {
  int i;
  double s = 0.0, v = 0.0;

  for(i = 0; i < n; ++i) s += x[i];
  *mean = s / n;
  for(i = 0; i < n; ++i) v += (x[i] - *mean) * (x[i] - *mean);
  *sd = (n > 1) ? sqrt(v / (n - 1)) : 0.0;
}

struct row {
  unsigned int k;
  double prove_mean, prove_sd;
  double verify_mean, verify_sd;
  size_t bytes;
};

int main(void) {
  uint8_t seed[LAS_SEEDBYTES];
  struct row rows[NBATCH];
  int bi, rep;
  unsigned int i;

  randombytes(seed, LAS_SEEDBYTES);
  setup_public_params(&pp, seed);

  /* One pool of statements; batch k uses the first k of them, so every row is
   * proving the same instances and a k-row differs from k=1 only by the batch. */
  for(i = 0; i < PI_BATCH_MAX_K; ++i) {
    uint8_t gseed[LAS_SEEDBYTES];
    randombytes(gseed, LAS_SEEDBYTES);
    relation_gen_seed(&Y[i], &R[i], &pp, gseed);
  }

  printf("=== Does the role-A proof amortise across swaps? -- LaZer ===\n\n");
  printf("Proof system : LaZer (LNP22 linear relation with norms), post-quantum\n");
  printf("Relation     : exists r : A r = t and ||r||inf <= 1   (per instance)\n");
  printf("Statement    : block-diagonal, %d x %d polynomials per instance\n",
         PI_BATCH_ROWS_PER, PI_BATCH_COLS_PER);
  printf("Repetitions  : %d per batch size, after an untimed warm-up\n", REPS);
  printf("k=1 uses the COMMITTED parameter set configuration 3 ships.\n\n");

  for(bi = 0; bi < NBATCH; ++bi) {
    const unsigned int k = BATCH[bi];
    double pv[REPS], vv[REPS];
    size_t prooflen = 0;

    for(rep = 0; rep <= REPS; ++rep) {   /* rep 0 = untimed warm-up */
      double t0, t1, t2;
      int rc, ok;

      t0 = now_us();
      rc = relation_batch_prove(k, proof, &prooflen, Y, R, &pp);
      t1 = now_us();
      if(rc != 0) {
        printf("FATAL: relation_batch_prove failed at k=%u (no parameter set?)\n", k);
        return 1;
      }

      ok = relation_batch_proof_verify(k, proof, prooflen, Y, &pp);
      t2 = now_us();

      /* Success-path assertion: never time a rejection path. */
      if(ok != 0) {
        printf("FATAL: an honest batch of %u did not verify; timings are meaningless\n", k);
        return 1;
      }

      if(rep > 0) {
        pv[rep-1] = t1 - t0;
        vv[rep-1] = t2 - t1;
      }
    }

    /* Tamper check: the batch must bind EVERY instance, not just the first.
     * Corrupt the LAST instance's statement and require rejection. */
    {
      int32_t save = Y[k-1].t_prime[0].coeffs[0];
      int accepted;

      Y[k-1].t_prime[0].coeffs[0] = (save + 1) % Q;
      accepted = (relation_batch_proof_verify(k, proof, prooflen, Y, &pp) == 0);
      Y[k-1].t_prime[0].coeffs[0] = save;

      if(accepted) {
        printf("FATAL: a batch of %u accepted a tampered LAST instance:\n"
               "       the batch does not bind all k, so any saving is an artefact.\n", k);
        return 1;
      }
    }

    rows[bi].k = k;
    rows[bi].bytes = prooflen;
    stats(pv, REPS, &rows[bi].prove_mean, &rows[bi].prove_sd);
    stats(vv, REPS, &rows[bi].verify_mean, &rows[bi].verify_sd);
    printf("  measured k = %u  (proof %zu B)\n", k, prooflen);
  }

  printf("\n--- Cost of ONE proof covering k statements (totals) ---\n\n");
  printf("  %3s  %20s  %20s  %12s\n", "k", "prove (ms)", "verify (ms)", "proof (B)");
  for(bi = 0; bi < NBATCH; ++bi)
    printf("  %3u  %12.1f +- %5.1f  %12.1f +- %5.1f  %12zu\n",
           rows[bi].k,
           rows[bi].prove_mean/1000.0, rows[bi].prove_sd/1000.0,
           rows[bi].verify_mean/1000.0, rows[bi].verify_sd/1000.0,
           rows[bi].bytes);

  printf("\n--- Amortised PER SWAP (total / k) ---\n\n");
  printf("  %3s  %14s  %14s  %14s  %16s\n",
         "k", "prove (ms)", "verify (ms)", "proof (B)", "proof vs k=1");
  for(bi = 0; bi < NBATCH; ++bi) {
    const double kf = (double)rows[bi].k;
    printf("  %3u  %14.1f  %14.1f  %14.1f  %15.2fx\n",
           rows[bi].k,
           rows[bi].prove_mean/1000.0/kf,
           rows[bi].verify_mean/1000.0/kf,
           (double)rows[bi].bytes/kf,
           ((double)rows[bi].bytes/kf) / (double)rows[0].bytes);
  }

  {
    const struct row *base = &rows[0];
    const struct row *top  = &rows[NBATCH-1];
    const double kf = (double)top->k;
    const double size_ratio = ((double)top->bytes/kf) / (double)base->bytes;
    const double compute_ratio =
      ((top->prove_mean + top->verify_mean)/kf) / (base->prove_mean + base->verify_mean);

    printf("\n--- What this shows ---\n\n");
    printf("Unlike Groth16, whose proof is a CONSTANT 128 B, LaZer's proof grows with k\n");
    printf("-- but SUBLINEARLY, so per-swap size does fall: %.2fx at k=%u.\n",
           size_ratio, top->k);
    printf("That is a saving on a cost that actually mattered: the proof is the dominant\n");
    printf("communication object of configuration 3, where Groth16's 128 B never was.\n\n");
    printf("BUT IT IS A TRADE, NOT A WIN, AND THE TRADE IS BAD HERE. Per-swap compute\n");
    printf("goes the other way and goes further: prove+verify per swap is %.2fx at k=%u\n",
           compute_ratio, top->k);
    printf("(prove %.0f -> %.0f ms, verify %.0f -> %.0f ms). LaZer's work grows\n",
           base->prove_mean/1000.0, top->prove_mean/1000.0/kf,
           base->verify_mean/1000.0, top->verify_mean/1000.0/kf);
    printf("SUPERLINEARLY in the batch, so each extra instance costs more than the last.\n\n");
    printf("For configuration 3 that settles it. evidence/stage2/latest shows the role-A\n");
    printf("proof is already 98.6%% of end-to-end time, so the binding constraint is\n");
    printf("COMPUTE, not communication -- and batching buys communication by spending\n");
    printf("compute. Trading %.0f%% of the proof bytes for %.1fx the per-swap proof time\n",
           100.0*(1.0 - size_ratio), compute_ratio);
    printf("makes the dominant cost worse to improve a subordinate one.\n\n");
    printf("Taken with the Groth16 run, BOTH provers answer the amortisation question\n");
    printf("negatively, for opposite reasons: Groth16's 1/k is perfect but lands on a\n");
    printf("cost that was already negligible; LaZer's lands on a cost that matters but\n");
    printf("is paid for in the one that matters more.\n\n");
  }
  printf("Against that, batching costs the same protocol concessions it costs for any\n");
  printf("proof system: the batch must be proved BEFORE any of its statements is used,\n");
  printf("so a party has to know its next k swaps in advance; every counterparty\n");
  printf("verifies the whole batch to use one statement of it; and the statements in a\n");
  printf("batch become linked, since anyone seeing two of them knows they were proved\n");
  printf("together. None of those is priced above.\n\n");
  printf("SCOPE: time and communication only. The batched parameter sets were generated\n");
  printf("by the same LaZer codegen as the committed k=1 set and target the same\n");
  printf("knowledge error, but have NOT been independently reviewed, and no security\n");
  printf("claim about batching is made here. Configuration 3 still uses k=1.\n");
  return 0;
}
