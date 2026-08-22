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
 *   ⚠️ Those three are properties of the LIBRARY, and they land differently here;
 *   never carry them over as a block.  Post-quantum survives the encoding intact.
 *   Succinctness is not lost either -- but it is asymptotic, and the size advantage
 *   it promises is not visible at this statement size, where the measured proof is
 *   the largest of the four systems compared.  What the contract below bars is
 *   neither of those: it is the composite claim this bridge was built for, and it
 *   is barred TWICE OVER, on the statement handed to the library rather than on the
 *   library -- once on privacy (BLOCKER 1) and once on faithfulness to the target
 *   relation (BLOCKER 2).  The two are independent; fixing one leaves the other.
 *
 * WHAT IS PROVEN
 *   The TARGET relation is the one configurations 2 and 3 prove:
 *
 *       exists r : A r = t'  and  ||r||inf <= 1,     A = [I_n | A'].
 *
 *   The ENCODED statement is a different object -- LaBRADOR constraints over its
 *   own ring, plus two declared l2 bounds:
 *
 *       [A | -A] * w  -  q * g  =  t'          (one constraint per output row)
 *       ||w||^2 <= w_normsq   (w BINARY, native norm type)
 *       ||g||^2 <= g_normsq
 *
 *   with w = (r_plus || r_minus) -- the same decomposition the deployed LNP22 path
 *   uses, so ||r||inf<=1 is proven, not assumed -- and g the mod-q quotient.
 *
 * ⚠️ WHAT MAY AND MAY NOT BE CLAIMED  (the seam's contract; a claim outside it is
 *   an overclaim wherever it appears -- report, docs, commit message, comment)
 *
 *   MAY:      on the instances run, LaBRADOR proves the encoded statement, and a
 *             witness satisfying it yields one for the target relation (the lifting
 *             argument below) -- so ||r||inf <= 1 is proven and not assumed.
 *             That is the SOUNDNESS direction only.
 *   MAY NOT:  that the encoding is FAITHFUL to the target relation, or that this is
 *             a fully satisfactory zero-knowledge proof of exactly that relation.
 *
 *   Two blockers stand between the two, both open.  Neither touches the
 *   experiment's VERDICT (this loses to the deployed LNP22 on size and time);
 *   they bar the qualitative claim.
 *
 *   BLOCKER 1 -- PRIVACY, AND IT IS NOT A ZK FAILURE.  zk=1 IS passed to py_gen_params
 *     and the proof IS zero-knowledge FOR THE ENCODED STATEMENT; what follows is a
 *     defect in that statement, not in the proof.  py_init_statement takes an l2
 *     bound PER WITNESS VECTOR (proofsystem.h: "squared l2-norm bound for each
 *     witness vector"), verify() enforces ||s_i||^2 <= normsq[i], and py_verify
 *     consumes that same statement -- so the declared bounds are verifier-side
 *     data, not prover-side hygiene.  This function does not choose w_normsq; its
 *     caller does, and bench_labrador_role_a.c passes the honest witness's EXACT
 *     norm, which for binary w IS the Hamming weight of the ternary r'.  The
 *     statement, and the parameters generated from it, are therefore a function of
 *     the secret.  The zk flag cannot repair this: zk bounds what the PROOF adds
 *     beyond the statement, and the leak is in the statement.  Scope it honestly --
 *     one statistic of r', of unanalysed consequence for LAS, not a broken proof
 *     system.  The fix is a witness-independent bound: ||w||^2 <= (n+ell)*d, since
 *     r_plus and r_minus are never both 1 in a coefficient.  Widening it can only
 *     raise LaBRADOR's widths (polxvec_setwidths1 divides normsq by n*N), so a
 *     corrected run should not come out smaller or faster -- a reading of the
 *     library, not a measurement.
 *
 *   BLOCKER 2 -- FAITHFULNESS: THE ENCODING IS NOT SHOWN COMPLETE.  ||g||^2 <= g_normsq is an
 *     EXTRA constraint the target relation does not contain, so faithfulness needs
 *     every honest witness of the target relation to satisfy it -- and nothing here
 *     shows that.  The driver's G_NORMSQ_BOUND is a constant, so (unlike w) it
 *     leaks nothing; what is missing is completeness, its whole justification being
 *     a runtime assert that the SAMPLED instance sits inside it.  No bound holding
 *     for ALL honest witnesses has been derived that also fits under LaBRADOR's
 *     exact-l2 cap -- the naive aligned worst case in the driver (|g|inf <= 641)
 *     implies ||g||^2 up to ~6.3e8, several times the declared 1e8.  Honest-prover
 *     failure is thus UNQUANTIFIED: not shown to be zero, and not shown to be
 *     positive either -- the true worst case over honest witnesses has never been
 *     computed, so it may well BE zero.  Two things not to write: "no
 *     witness-independent bound exists" (the defect is completeness, not
 *     dependence), and "the failure probability is not zero" (unwarranted negative).
 *
 * WHY THE QUOTIENT g EXISTS, AND WHY ITS BOUND IS LOAD-BEARING
 *   LaBRADOR works over ITS OWN prime p, not our q, so `A r = t' (mod q)` is not
 *   directly expressible.  Writing the relation over the integers as
 *   `A r - t' = q g` and proving that in R_p is exact for an honest prover.  For
 *   SOUNDNESS the l2 bound on g is essential: q is invertible mod p, so an
 *   unbounded g satisfies the equation for ANY claimed t'.  With w binary and g
 *   bounded every coefficient stays under p/2, so the R_p identity lifts back to Z.
 *   This is the same argument the STARK needed, and it runs in the SOUNDNESS
 *   direction only -- a verifying proof establishes the bound, hence the lift;
 *   BLOCKER 2 is about the other direction and is not touched by it.
 *   ⚠️ Do not write "well inside": at the declared ||g||^2 <= 1e8 the worst case is
 *   ~78% of LOGQ=38's p/2 (see SUCCINCT_PQ_PROOF_EXPERIMENT.md), which is why 38 is
 *   forced and 36 overflows.  It is inside the budget, not comfortably so.
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
