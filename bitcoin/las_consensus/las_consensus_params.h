/* las_consensus_params.h — the ONE definition of the consensus-fixed LAS public
 * parameters, shared by the C++ shim Bitcoin Core links against and by the C tooling that
 * produces signatures for it.
 *
 * WHY A FIXED SEED IS A CONSENSUS CONSTANT.  LAS's public parameters are the matrix
 * A = [I | A'], expanded from a seed.  A verifier and a signer that disagree about A
 * disagree about everything, so on a chain the seed cannot be per-transaction data: it is
 * a network parameter, exactly as the choice of the secp256k1 curve is for BIP340.  Fixing
 * it here also lets a node expand A' ONCE at startup instead of per input, which is the
 * difference between measuring verification and measuring matrix expansion.
 *
 * WHERE THE VALUE COMES FROM.  It is not arbitrary and it is not a private choice:
 *
 *     seed = SHA-256("LAS-CONSENSUS-PARAMS-v1")
 *          = e2a16befb5800a121a9b49918f0183ef6812069998e929717a5f34ea692e5998
 *
 * Anyone can recompute it from the printed preimage, which is the point — a hard-coded
 * constant nobody can re-derive is a constant nobody can check.  `las_btc_tool seed`
 * prints the bytes actually compiled in, and the selftest asserts they hash back to the
 * preimage, so this comment cannot drift away from the code.
 *
 * ONE DEFINITION.  Both the shim (C++, linked into the node) and the signing tool (C)
 * include this header.  Two copies of a consensus parameter is how a signer and a verifier
 * quietly stop agreeing.
 */
#ifndef LAS_CONSENSUS_PARAMS_H
#define LAS_CONSENSUS_PARAMS_H

#include <stdint.h>

#define LAS_CONSENSUS_SEED_PREIMAGE "LAS-CONSENSUS-PARAMS-v1"
#define LAS_CONSENSUS_SEED_BYTES 32

static const uint8_t LAS_CONSENSUS_SEED[LAS_CONSENSUS_SEED_BYTES] = {
  0xe2, 0xa1, 0x6b, 0xef, 0xb5, 0x80, 0x0a, 0x12,
  0x1a, 0x9b, 0x49, 0x91, 0x8f, 0x01, 0x83, 0xef,
  0x68, 0x12, 0x06, 0x99, 0x98, 0xe9, 0x29, 0x71,
  0x7a, 0x5f, 0x34, 0xea, 0x69, 0x2e, 0x59, 0x98
};

/* The signed message is a BIP341 sighash: always exactly 32 bytes. */
#define LAS_CONSENSUS_MSG_BYTES 32

#endif /* LAS_CONSENSUS_PARAMS_H */
