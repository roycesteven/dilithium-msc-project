/*
 * relation_zk_lazer.c -- LaZer bridge half of the pi module (see
 * relation_zk_lazer.h for the seam contract and relation_zk.h for the design).
 *
 * The ONLY translation unit in ref/ that includes the vendored LaZer library
 * (third_party/lazer): ref headers and lazer.h cannot coexist in one TU (both
 * own names like `poly`; params.h owns bare N/Q/D as macros).  Compiled with
 * -I$(LAZER_DIR) and NO ref include path; see the Makefile pi rules.
 *
 * Statement form: LaZer's lin frontend proves knowledge of s with As + t = 0,
 * s satisfying the per-partition norms of the generated parameter set
 * ref/relation_zk_params.h (22 binary polys, one l2<=16 dummy).  Callers hand
 * in +t' so this file owns the single negation into LaZer's convention,
 * mirroring the shipped kyber1024 demo (polyvec_neg_self).
 */
#include <stddef.h>
#include <stdint.h>

#include "lazer.h"
#include "relation_zk_params.h"   /* generated: lin_params_t las_pi_params */
#include "relation_zk_lazer.h"

/* lazer_init is idempotent-by-guard here: LaZer requires it once per process. */
static void ensure_lazer_init(void) {
  static int done = 0;
  if(!done) {
    lazer_init();
    done = 1;
  }
}

int relation_zk_lin_prove(uint8_t proof[PI_PROOF_MAX_BYTES], size_t *prooflen,
                          const int64_t *Aext, const int64_t *t,
                          const int64_t *w, const uint8_t ppseed[32]) {
  lin_prover_state_t prover;
  polymat_t A;
  polyvec_t tvec, svec;

  ensure_lazer_init();

  INT_T(p, 1);
  int_set_i64(p, 8380417);
  POLYRING_T(Rq, p, PI_DEG);

  polymat_alloc(A, Rq, PI_ROWS, PI_COLS);
  polyvec_alloc(tvec, Rq, PI_ROWS);
  polyvec_alloc(svec, Rq, PI_COLS);

  polymat_set_i64(A, Aext);
  polyvec_set_coeffvec_i64(tvec, t);
  polyvec_neg_self(tvec);                    /* statement form: A s + t = 0 */
  polyvec_set_coeffvec_i64(svec, w);

  lin_prover_init(prover, ppseed, las_pi_params);
  lin_prover_set_statement(prover, A, tvec);
  lin_prover_set_witness(prover, svec);
  lin_prover_prove(prover, proof, prooflen, NULL);
  lin_prover_clear(prover);

  polymat_free(A);
  polyvec_free(tvec);
  polyvec_free(svec);
  return 0;
}

int relation_zk_lin_verify(const uint8_t *proof, size_t prooflen,
                           const int64_t *Aext, const int64_t *t,
                           const uint8_t ppseed[32]) {
  lin_verifier_state_t verifier;
  polymat_t A;
  polyvec_t tvec;
  size_t len = prooflen;
  int accept;

  ensure_lazer_init();

  INT_T(p, 1);
  int_set_i64(p, 8380417);
  POLYRING_T(Rq, p, PI_DEG);

  polymat_alloc(A, Rq, PI_ROWS, PI_COLS);
  polyvec_alloc(tvec, Rq, PI_ROWS);

  polymat_set_i64(A, Aext);
  polyvec_set_coeffvec_i64(tvec, t);
  polyvec_neg_self(tvec);                    /* statement form: A s + t = 0 */

  lin_verifier_init(verifier, ppseed, las_pi_params);
  lin_verifier_set_statement(verifier, A, tvec);
  accept = lin_verifier_verify(verifier, proof, &len);
  lin_verifier_clear(verifier);

  polymat_free(A);
  polyvec_free(tvec);
  return accept ? 0 : -1;
}
