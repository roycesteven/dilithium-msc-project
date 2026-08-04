/*
 * relation_zk_batch.h -- protocol-facing half of the BATCHED pi module
 * (amortisation experiment).  Twin of relation_zk.h; see
 * relation_zk_lazer_batch.h for the seam contract.
 *
 * WHAT IT PROVES
 *   ONE proof of k INDEPENDENT instances of the Fig. 1 relation
 *
 *     for each i < k:  exists r'_i : A r'_i = Y_i  and  ||r'_i||inf <= 1
 *
 *   via a BLOCK-DIAGONAL statement matrix: instance i occupies rows
 *   [6i, 6i+6) and columns [23i, 23i+23), each block being exactly the
 *   [I | A' | -I | -A' | 0] matrix relation_zk.c builds for k=1, with every
 *   off-block entry zero.  So the batched statement is the CONJUNCTION of k
 *   copies of the deployed statement -- it cannot prove anything weaker, and
 *   the k=1 case is byte-for-byte the deployed one.
 *
 * WHY IT EXISTS
 *   The Groth16 amortisation run showed batching shrinking a 128 B proof that
 *   was never the bottleneck.  LaZer is the opposite profile -- fast generation,
 *   ~30 KiB proof -- so it is the case where a per-swap proof-size saving would
 *   actually matter.  This module measures whether LaZer's proof grows
 *   sublinearly in the batch, which is what decides it.
 *
 * NOT WIRED INTO THE SWAP.  This is an experiment: configuration 3 continues to
 * use the k=1 module (relation_zk.{c,h}).  Nothing here changes the deployed
 * prover, the KAT, or any benchmark of record.
 *
 * Non-reentrant: the staging buffers are static (18 MiB at k=8).
 */
#ifndef RELATION_ZK_BATCH_H
#define RELATION_ZK_BATCH_H

#include <stddef.h>
#include <stdint.h>

#include "setup.h"                    /* construction parameters + public_params */
#include "las_types.h"                /* statement, witness                      */
#include "relation_zk_lazer_batch.h"  /* PI_BATCH_* + the bridge seam            */

/* Prove k instances at once.  `Y` and `r_prime` are arrays of k elements, in
 * the same order.  Every witness must be ternary (honest Gen output); a
 * non-ternary one is refused, exactly as the k=1 module refuses it.
 * Returns 0 on success, -1 on a bad witness or an unsupported k. */
int relation_batch_prove(unsigned int k,
                         uint8_t proof[PI_BATCH_PROOF_MAX_BYTES], size_t *prooflen,
                         const statement *Y, const witness *r_prime,
                         const public_params *pp);

/* Verify a batched proof against the same k statements, in the same order.
 * Returns 0 iff it verifies. */
int relation_batch_proof_verify(unsigned int k,
                                const uint8_t *proof, size_t prooflen,
                                const statement *Y, const public_params *pp);

#endif
