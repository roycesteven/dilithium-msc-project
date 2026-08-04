/*
 * relation_zk_lazer_batch.h -- seam contract for the BATCHED pi bridge.
 *
 * Twin of relation_zk_lazer.h, for the amortisation experiment: the same LaZer
 * lin frontend, but proving k INDEPENDENT instances of the Fig. 1 relation in
 * ONE proof via a block-diagonal statement matrix.
 *
 * Like its k=1 twin this header is deliberately free of BOTH worlds' headers --
 * no lazer.h, no ref headers -- because it is the only thing the two sides may
 * share (ref headers and lazer.h cannot coexist in a TU: both own names like
 * `poly`, and params.h owns bare N/Q/D as macros).
 *
 * Buffer layout, identical in shape to the k=1 seam and scaled by k:
 *   Aext : row-major, PI_BATCH_DEG coefficients per polynomial entry;
 *          entry (i,j) at offset PI_BATCH_DEG*(i*cols + j), cols = k*PI_BATCH_COLS_PER
 *   t    : k*PI_BATCH_ROWS_PER polynomials, stacked instance by instance
 *   w    : k*PI_BATCH_COLS_PER polynomials, stacked instance by instance
 *
 * The k=1 case deliberately dispatches to the COMMITTED parameter set
 * (las_pi_params, ref/relation_zk_params.h) -- the one configuration 3 ships --
 * so the k=1 row of the experiment IS the deployed prover and the k>1 rows are
 * measured against it rather than against a re-derived baseline.
 *
 * GATE NAMES: PI_BATCH_* are new names for this experiment. They do NOT
 * replace, alias or rename PI_ROWS / PI_COLS / PI_DEG / PI_PROOF_MAX_BYTES,
 * which remain the k=1 module's and must never be renamed.
 */
#ifndef RELATION_ZK_LAZER_BATCH_H
#define RELATION_ZK_LAZER_BATCH_H

#include <stddef.h>
#include <stdint.h>

/* Per-instance block dimensions: one copy of [I | A' | -I | -A' | 0] over
 * Rp = Z_p[X]/(X^256+1). Mirrors PI_ROWS / PI_COLS / PI_DEG of the k=1 seam;
 * relation_zk_batch.c #errors if the build's set disagrees. */
#define PI_BATCH_ROWS_PER 6
#define PI_BATCH_COLS_PER 23
#define PI_BATCH_DEG      256

/* Batch sizes with a generated parameter set. Adding one means generating a
 * header with scripts/gen_lazer_batch_params.sh and extending params_for(). */
#define PI_BATCH_MAX_K 8

/* Upper bound for every generated set here; the largest (k=8) is predicted well
 * under this. The prover writes the true length through prooflen. */
#define PI_BATCH_PROOF_MAX_BYTES 524288

/* Returns 0 on success, -1 if k has no generated parameter set. */
int relation_zk_batch_lin_prove(unsigned int k,
                                uint8_t *proof, size_t *prooflen,
                                const int64_t *Aext, const int64_t *t,
                                const int64_t *w, const uint8_t ppseed[32]);

/* Returns 0 iff the proof verifies; -1 otherwise (including unknown k). */
int relation_zk_batch_lin_verify(unsigned int k,
                                 const uint8_t *proof, size_t prooflen,
                                 const int64_t *Aext, const int64_t *t,
                                 const uint8_t ppseed[32]);

#endif
