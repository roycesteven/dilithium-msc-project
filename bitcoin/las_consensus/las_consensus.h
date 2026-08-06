/* las_consensus.h — the interface Bitcoin Core's script interpreter calls to verify a LAS
 * signature as a consensus rule.
 *
 * DELIBERATELY NARROW.  One predicate, over bytes, with no allocation, no randomness and no
 * global mutable state after initialisation.  Everything behind it is
 * `ref/basesig.c base_verify_packed`, which this project already designates as the byte
 * interface an on-chain verifier consumes — so no new cryptography is written for the node,
 * and the node and the reference implementation cannot drift apart.
 *
 * DETERMINISM IS A CONSENSUS REQUIREMENT.  Two nodes must agree on every input, always.
 * The public parameters come from a compiled-in seed (las_consensus_params.h) expanded
 * exactly once; the verify path touches no entropy source, and the build links an ABORTING
 * `randombytes` so that an accidental call crashes the node loudly rather than silently
 * making validation non-deterministic.  That abort is a build-time invariant failing, not
 * an input-driven outcome: no witness an attacker can supply reaches it.
 */
#ifndef LAS_CONSENSUS_H
#define LAS_CONSENSUS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Wire size of a packed LAS signature at the compiled-in parameter set. */
size_t LASConsensusSigBytes(void);

/** Wire size of a packed LAS public key at the compiled-in parameter set. */
size_t LASConsensusPkBytes(void);

/** The 32-byte consensus seed actually compiled in (for tooling and evidence). */
void LASConsensusSeed(uint8_t out[32]);

/**
 * Verify a packed LAS signature over a 32-byte message under a packed public key.
 *
 * CONTRACT: returns 1 on success, and 0 on malformed input, wrong length, or verification
 * failure.  Exceptions must not cross this C ABI boundary.  Attacker-supplied witness data
 * can therefore produce nothing but a 0 — which is what makes it safe to call from script
 * evaluation.
 *
 * `msg32` is the BIP341 sighash of the spending transaction.  Fixing the length at 32 is
 * what ties a signature to one transaction rather than to a message of the signer's
 * choosing.
 */
int LASConsensusVerify(const uint8_t* sig, size_t siglen,
                       const uint8_t* pk, size_t pklen,
                       const uint8_t* msg32, size_t msglen);

#ifdef __cplusplus
}
#endif

#endif /* LAS_CONSENSUS_H */
