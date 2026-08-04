/*
 * relation_zk_lazer_batch.c -- LaZer bridge for the BATCHED pi (amortisation
 * experiment).  See relation_zk_lazer_batch.h for the seam contract.
 *
 * Twin of relation_zk_lazer.c, and one of only two TUs in ref/ that include the
 * vendored LaZer library: ref headers and lazer.h cannot coexist in one TU
 * (both own names like `poly`; params.h owns bare N/Q/D as macros).  Compiled
 * with -I$(LAZER_DIR) and NO ref include path; see the Makefile pi rules.
 *
 * The ONLY thing that changes with k is which generated parameter set is used
 * and how big the statement is.  The proof system, the statement form
 * (A s + t = 0) and the per-instance relation are identical to the k=1 module,
 * so a k-row is comparable with the k=1 row by construction.
 *
 * k=1 dispatches to the COMMITTED las_pi_params -- the set configuration 3
 * ships -- so the baseline of the experiment is the deployed prover itself and
 * not a re-derived lookalike.
 */
#include <stddef.h>
#include <stdint.h>

#include "lazer.h"
#include "relation_zk_params.h"      /* generated: las_pi_params    (k=1, committed) */
#include "relation_zk_params_k2.h"   /* generated: las_pi_params_k2 */
#include "relation_zk_params_k4.h"   /* generated: las_pi_params_k4 */
#include "relation_zk_params_k8.h"   /* generated: las_pi_params_k8 */
#include "relation_zk_lazer_batch.h"

/* lazer_init is idempotent-by-guard here: LaZer requires it once per process. */
static void ensure_lazer_init(void) {
  static int done = 0;
  if(!done) {
    lazer_init();
    done = 1;
  }
}

/* The generated set for this batch size, or NULL if k has none.
 * lin_params_t is LaZer's array-of-1 typedef, so each name below decays to the
 * lin_params_srcptr that lin_prover_init / lin_verifier_init expect. */
static lin_params_srcptr params_for(unsigned int k) {
  switch(k) {
  case 1: return las_pi_params;
  case 2: return las_pi_params_k2;
  case 4: return las_pi_params_k4;
  case 8: return las_pi_params_k8;
  default: return NULL;
  }
}

int relation_zk_batch_lin_prove(unsigned int k,
                                uint8_t *proof, size_t *prooflen,
                                const int64_t *Aext, const int64_t *t,
                                const int64_t *w, const uint8_t ppseed[32]) {
  lin_prover_state_t prover;
  polymat_t A;
  polyvec_t tvec, svec;
  lin_params_srcptr params = params_for(k);
  unsigned int rows, cols;

  if(params == NULL)
    return -1;
  rows = k * PI_BATCH_ROWS_PER;
  cols = k * PI_BATCH_COLS_PER;

  ensure_lazer_init();

  INT_T(p, 1);
  int_set_i64(p, 8380417);
  POLYRING_T(Rq, p, PI_BATCH_DEG);

  polymat_alloc(A, Rq, rows, cols);
  polyvec_alloc(tvec, Rq, rows);
  polyvec_alloc(svec, Rq, cols);

  polymat_set_i64(A, Aext);
  polyvec_set_coeffvec_i64(tvec, t);
  polyvec_neg_self(tvec);                    /* statement form: A s + t = 0 */
  polyvec_set_coeffvec_i64(svec, w);

  lin_prover_init(prover, ppseed, params);
  lin_prover_set_statement(prover, A, tvec);
  lin_prover_set_witness(prover, svec);
  lin_prover_prove(prover, proof, prooflen, NULL);
  lin_prover_clear(prover);

  polymat_free(A);
  polyvec_free(tvec);
  polyvec_free(svec);
  return 0;
}

int relation_zk_batch_lin_verify(unsigned int k,
                                 const uint8_t *proof, size_t prooflen,
                                 const int64_t *Aext, const int64_t *t,
                                 const uint8_t ppseed[32]) {
  lin_verifier_state_t verifier;
  polymat_t A;
  polyvec_t tvec;
  lin_params_srcptr params = params_for(k);
  size_t len = prooflen;
  unsigned int rows, cols;
  int accept;

  if(params == NULL)
    return -1;
  rows = k * PI_BATCH_ROWS_PER;
  cols = k * PI_BATCH_COLS_PER;

  ensure_lazer_init();

  INT_T(p, 1);
  int_set_i64(p, 8380417);
  POLYRING_T(Rq, p, PI_BATCH_DEG);

  polymat_alloc(A, Rq, rows, cols);
  polyvec_alloc(tvec, Rq, rows);

  polymat_set_i64(A, Aext);
  polyvec_set_coeffvec_i64(tvec, t);
  polyvec_neg_self(tvec);                    /* statement form: A s + t = 0 */

  lin_verifier_init(verifier, ppseed, params);
  lin_verifier_set_statement(verifier, A, tvec);
  accept = lin_verifier_verify(verifier, proof, &len);
  lin_verifier_clear(verifier);

  polymat_free(A);
  polyvec_free(tvec);
  return accept ? 0 : -1;
}
