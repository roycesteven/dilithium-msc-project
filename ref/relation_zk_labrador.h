/*
 * relation_zk_labrador.h -- seam contract for the LaBRADOR pi bridge.
 *
 * THIRD lazer-adjacent bridge, after relation_zk_lazer.{c,h} (LNP22, k=1, the
 * DEPLOYED prover) and relation_zk_lazer_batch.{c,h} (LNP22, batched).  Like
 * them this header is deliberately free of BOTH worlds' headers -- no lazer.h,
 * no labrados headers, no ref headers -- because it is the only thing the two
 * sides may share (labrados owns `poly` and `N` exactly as lazer.h and params.h
 * do, so the two cannot coexist in one translation unit).
 *
 * WHY LaBRADOR
 *   eprint 2020/845 Section 4.1 requires pi to be ZERO-KNOWLEDGE: the swap's
 *   security rests on pi hiding the witness, since a counterparty who learned r
 *   could adapt the other pre-signature and take both sides.  The FRI-STARK in
 *   rust/las-stark proves the same relation succinctly but is NOT zk, so it is
 *   not a valid pi.  LaBRADOR is succinct, post-quantum AND zk -- its parameter
 *   generation takes an explicit zk flag -- which makes it the right target.
 *
 * WHAT IS PROVEN
 *   The same statement configurations 2 and 3 prove:
 *
 *       exists r : A r = t'  and  ||r||inf <= 1,     A = [I_n | A'].
 *
 *   encoded as LaBRADOR constraints over its own ring:
 *
 *       [A | -A] * w  -  q * g  =  t'          (one constraint per output row)
 *
 *   with w = (r_plus || r_minus) proven BINARY by LaBRADOR's native binary norm
 *   type -- the same decomposition the deployed LNP22 path uses, so ||r||inf<=1
 *   is proven, not assumed -- and g an l2-bounded quotient.
 *
 * WHY THE QUOTIENT g EXISTS, AND WHY ITS BOUND IS LOAD-BEARING
 *   LaBRADOR works over ITS OWN prime p, not our q, so `A r = t' (mod q)` is not
 *   directly expressible.  Writing the relation over the integers as
 *   `A r - t' = q g` and proving that in R_p is exact for an honest prover.  For
 *   SOUNDNESS the l2 bound on g is essential: q is invertible mod p, so an
 *   unbounded g satisfies the equation for ANY claimed t'.  With w binary and g
 *   bounded every coefficient stays well inside p/2, so the R_p identity lifts
 *   back to Z.  This is the same argument the STARK needed.
 *
 * SCOPE: an experiment.  Nothing here is wired into the swap; configuration 3
 * continues to use the k=1 LNP22 module.
 */
#ifndef RELATION_ZK_LABRADOR_H
#define RELATION_ZK_LABRADOR_H

#include <stddef.h>
#include <stdint.h>

/* Shape of the encoded statement.  Mirrors PI_ROWS / PI_COLS / PI_DEG of the
 * LNP22 seam; relation_zk_labrador.c #errors if the build's set disagrees.
 * GATE NAMES: PI_LAB_* are new. They do NOT rename or alias PI_ROWS / PI_COLS /
 * PI_DEG / PI_PROOF_MAX_BYTES (the k=1 module's) or PI_BATCH_* (the batched
 * module's). */
#define PI_LAB_ROWS  6    /* n: one constraint per output polynomial          */
#define PI_LAB_WCOLS 22   /* 2*(n+ell): the binary (r_plus || r_minus) witness */
#define PI_LAB_GCOLS 6    /* n: one quotient polynomial per constraint         */
#define PI_LAB_DEG   256  /* ring degree d; equals LaBRADOR's own N            */

/* One end-to-end LaBRADOR run over the encoded statement.
 *
 * Buffers are flat int64 arrays of CENTRED coefficients, poly-major then
 * coefficient (poly i occupies [i*PI_LAB_DEG, (i+1)*PI_LAB_DEG)):
 *   phi_w : PI_LAB_ROWS * PI_LAB_WCOLS * PI_LAB_DEG   -- [A | -A], row-major
 *   phi_g : PI_LAB_ROWS * PI_LAB_GCOLS * PI_LAB_DEG   -- -q on the diagonal
 *   b     : PI_LAB_ROWS * PI_LAB_DEG                  -- t'
 *   w     : PI_LAB_WCOLS * PI_LAB_DEG                 -- binary witness
 *   g     : PI_LAB_GCOLS * PI_LAB_DEG                 -- quotient witness
 *
 * `zk` selects LaBRADOR's zero-knowledge parameters (Section 4.1 needs 1).
 *
 * Outputs (all optional, may be NULL):
 *   encoding_ok : 1 iff LaBRADOR's own simple_verify accepts the statement for
 *                 this witness -- i.e. the ENCODING is right, checked before any
 *                 proof is produced.
 *   verified    : 1 iff the produced proof verifies.
 *   *_ms        : wall-clock milliseconds for each phase.
 *
 * Returns 0 on success, non-zero if a LaBRADOR call failed. */
int relation_labrador_run(const int64_t *phi_w, const int64_t *phi_g,
                          const int64_t *b, const int64_t *w, const int64_t *g,
                          uint64_t w_normsq, uint64_t g_normsq, int zk,
                          int *encoding_ok, int *verified,
                          double *params_ms, double *prove_ms, double *verify_ms);

#endif
