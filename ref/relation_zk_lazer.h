#ifndef RELATION_ZK_LAZER_H
#define RELATION_ZK_LAZER_H

/*
 * relation_zk_lazer.{c,h} -- the LaZer BRIDGE half of the pi module (see
 * relation_zk.h for the protocol-facing half and the design rationale).
 *
 * This header is deliberately PLAIN C (stdint/stddef only): it is the seam
 * between two header worlds that cannot share a translation unit -- ref/'s
 * poly.h/params.h (macros N, Q, D, ...) and the vendored LaZer library's
 * lazer.h (its own poly/polyvec/polymat types).  relation_zk.c (ref world)
 * builds the statement as flat int64 coefficient buffers; relation_zk_lazer.c
 * (lazer world) loads them into LaZer types and runs the LNP prover/verifier
 * with the generated parameter set ref/relation_zk_params.h.
 *
 * Buffer layout = LaZer's polymat_set_i64 / polyvec_set_coeffvec_i64 layout:
 * row-major, PI_DEG coefficients per polynomial entry; matrix entry (i,j) at
 * offset PI_DEG*(i*PI_COLS + j).
 */

#include <stddef.h>
#include <stdint.h>

/* Dimensions of the pi statement [A | -A | 0] over Z_q[X]/(X^256+1).  MUST
 * match scripts/las_pi_params.py (the sage codegen spec that generated
 * ref/relation_zk_params.h): rows = n = 6, cols = 2*(n+ell) + 1 = 23 (the +1
 * is the dummy l2-bounded column the codegen requires), deg = d = 256.
 * relation_zk.c compile-time-asserts these against LAS_N/N_PLUS_ELL/LAS_D. */
#define PI_ROWS 6
#define PI_COLS 23
#define PI_DEG  256

/* Upper bound on the wire size of pi.  The generated parameter set reports
 * ~31.3 KiB; measured 30727 bytes.  The exact length varies slightly per proof
 * (entropy coding); callers pass a buffer this big and read back *prooflen. */
#define PI_PROOF_MAX_BYTES 65536

/* Prove: statement (Aext, t) with witness w.  t is passed as +t' (centered
 * representatives); the bridge negates it internally into LaZer's As + t = 0
 * statement form.  Returns 0 on success. */
int relation_zk_lin_prove(uint8_t proof[PI_PROOF_MAX_BYTES], size_t *prooflen,
                          const int64_t *Aext, const int64_t *t,
                          const int64_t *w, const uint8_t ppseed[32]);

/* Verify: same statement encoding, no witness.  0 = accept, -1 = reject. */
int relation_zk_lin_verify(const uint8_t *proof, size_t prooflen,
                           const int64_t *Aext, const int64_t *t,
                           const uint8_t ppseed[32]);

#endif
