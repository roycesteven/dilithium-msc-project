#ifndef MLDSA_LAS_H
#define MLDSA_LAS_H

/*
 * mldsa_las.h -- the ML-DSA hint experiment.
 *
 * The rest of this project builds LAS (eprint 2020/845, Algorithm 2) on the
 * paper's SIMPLIFIED Dilithium: no hint vector, no Power2Round, no high/low-bit
 * decomposition.  That simplification is what makes the adaptor identity
 *
 *     A z - c t = w + Y                                             (exact)
 *
 * hold, and the project has so far ASSERTED that NIST's ML-DSA construction has
 * to be modified before an adaptor layer can sit on it.
 *
 * This module turns that assertion into a DEMONSTRATION.  It builds the four
 * adaptor functions directly on ML-DSA as FIPS 204 specifies it -- hint vector
 * h, Power2Round, HighBits/LowBits decomposition all ENABLED, every primitive
 * called from the unmodified upstream sources -- in two variants that differ
 * only in how much of ML-DSA's signing path the adaptor is allowed to touch:
 *
 *   MLDSA_LAS_V0_NAIVE
 *       The naive port: take crypto_sign_signature_internal verbatim and change
 *       ONE thing, the challenge input, from HighBits(w) to HighBits(w + Y).
 *       The hint and the committed w1 are still built around w.  This is what
 *       "put LAS on ML-DSA" means if the hint machinery is treated as a black
 *       box.
 *
 *   MLDSA_LAS_V1_SHIFTED
 *       The best-effort repair: move the ENTIRE commitment path onto w + Y --
 *       the committed high bits, the low-bits rejection test and MakeHint are
 *       all taken around w + Y rather than w.  The signer can do this because Y
 *       is an input to PreSign.
 *
 *   MLDSA_LAS_VBASE
 *       No statement at all: the same instrumented loop with the Y-shift
 *       removed, i.e. crypto_sign_signature_internal reproduced here.  It is
 *       the MATCHED BASELINE -- it makes the rejection-attempt comparison
 *       like-for-like, and because its output must be accepted by the stock
 *       verifier it doubles as a fidelity gate on this file's mirror of
 *       sign.c.  If VBASE ever fails to verify, no other row here means
 *       anything.
 *
 * Both variants are measured by ref/test/test_mldsa_hint.c, which reports which
 * of the adaptor contract's properties survive in each.  Nothing here asserts a
 * predetermined outcome: the harness prints what actually happens.
 *
 * Shapes (ML-DSA-65: A is K x L over R_q):
 *   witness  y  in R_q^L, ||y||_inf <= ETA   (so it can be added to z)
 *   statement Y = A y in R_q^K              (so it shifts A z by exactly Y)
 *
 * Nothing in this file is used by the LAS scheme of record (ref/las.c); it is a
 * standalone experiment and is never linked into the benchmarks.
 */

#include <stddef.h>
#include <stdint.h>
#include "params.h"
#include "polyvec.h"

/* Witness norm bound.  ETA is ML-DSA's own secret-vector bound, reused so the
 * witness is sampled by the upstream sampler without inventing a distribution. */
#define MLDSA_LAS_ETA ETA

/* PreSign must reject at a bound tightened by the witness norm, so that the
 * ADAPTED z = z_hat + y still clears ML-DSA's own ||z||_inf < GAMMA1 - BETA.
 * This is the ML-DSA analogue of LAS's gamma-kappa-1 tightening. */
#define MLDSA_LAS_BOUND_PRESIGN (GAMMA1 - BETA - MLDSA_LAS_ETA)

typedef struct { polyveck Y; } mldsa_statement;
typedef struct { polyvecl y; } mldsa_witness;

typedef enum {
  MLDSA_LAS_V0_NAIVE = 0,   /* only the challenge input carries Y            */
  MLDSA_LAS_V1_SHIFTED = 1, /* commitment, low-bits test and hint all use w+Y */
  MLDSA_LAS_VBASE = 2       /* no statement at all: plain ML-DSA, same loop   */
} mldsa_las_variant;

/* Rejection-sampling attempt counter (measurement only). */
extern unsigned long mldsa_las_attempts;

/*
 * Statement/witness generation: y <- S_ETA^L, Y = A y.
 * rho is the public matrix seed, taken from the ML-DSA public key.
 */
void mldsa_las_gen(mldsa_statement *Y,
                   mldsa_witness *y,
                   const uint8_t rho[SEEDBYTES],
                   const uint8_t seed[CRHBYTES],
                   uint16_t nonce);

/*
 * PreSign: an ML-DSA signature whose challenge commits to w + Y, produced at the
 * tightened bound MLDSA_LAS_BOUND_PRESIGN.  Output is a standard ML-DSA
 * signature blob (c_tilde || z_hat || h), CRYPTO_BYTES long.
 *
 * Returns 0 on success.
 */
int mldsa_las_presign(uint8_t presig[CRYPTO_BYTES],
                      const uint8_t *m, size_t mlen,
                      const mldsa_statement *Y,
                      const uint8_t rnd[RNDBYTES],
                      const uint8_t *sk,
                      mldsa_las_variant variant);

/*
 * PreVerify: recompute A z_hat - c t1 2^d, shift by Y, reconstruct the high bits
 * with the transmitted hint, and re-derive the challenge.  This is ML-DSA's
 * verifier plus one vector addition -- note that it therefore CANNOT be the
 * stock consensus verifier, which knows nothing of Y.
 *
 * Returns 0 if the pre-signature is valid.
 */
int mldsa_las_preverify(const uint8_t presig[CRYPTO_BYTES],
                        const uint8_t *m, size_t mlen,
                        const mldsa_statement *Y,
                        const uint8_t *pk);

/*
 * Adapt: PreVerify, then sigma = (c_tilde, z_hat + y, h).  The hint is copied
 * unchanged -- the adapting party holds the witness, not the signing key, and
 * so could not recompute a hint even if it needed to.  That constraint is the
 * whole point of the experiment.
 *
 * Returns 0 on success, -1 if the pre-signature does not verify.
 */
int mldsa_las_adapt(uint8_t sig[CRYPTO_BYTES],
                    const uint8_t presig[CRYPTO_BYTES],
                    const uint8_t *m, size_t mlen,
                    const mldsa_statement *Y,
                    const mldsa_witness *y,
                    const uint8_t *pk);

/*
 * Ext: y' = z - z_hat, returned iff A y' == Y.
 *
 * Returns 0 on success, -1 if the recovered vector is not a witness for Y.
 */
int mldsa_las_ext(mldsa_witness *y_out,
                  const uint8_t sig[CRYPTO_BYTES],
                  const uint8_t presig[CRYPTO_BYTES],
                  const mldsa_statement *Y,
                  const uint8_t *pk);

/* Hint weight carried by a signature blob (-1 if the blob does not decode). */
int mldsa_las_hint_weight(const uint8_t sig[CRYPTO_BYTES]);

/* ------------------------------------------------------------------------
 * Wire format.
 *
 * The signature, pre-signature and public key already have FIPS 204's own
 * encodings (pack_sig / pack_pk), so they are reused verbatim -- that is the
 * whole point of building on ML-DSA.  Only the two ADAPTOR-SPECIFIC objects
 * need an encoding of their own:
 *
 *   statement Y : a full R_q^K element.  Power2Round compresses the public key
 *                 to t1 (10 bits/coeff) because the low bits are recoverable
 *                 from the hint, but Y enters the verification identity BEFORE
 *                 any rounding, so every coefficient must survive: 23 bits.
 *   witness y   : bounded by ETA, so ML-DSA's own polyeta codec applies
 *                 unchanged.
 *
 * Both decoders VALIDATE (canonical range) rather than trusting their input,
 * matching ref/serialize.c's posture for the simplified scheme.
 * ------------------------------------------------------------------------ */
#define MLDSA_LAS_SIGNATURE_BYTES      CRYPTO_BYTES
#define MLDSA_LAS_PRE_SIGNATURE_BYTES  CRYPTO_BYTES
#define MLDSA_LAS_PUBLICKEY_BYTES      CRYPTO_PUBLICKEYBYTES
#define MLDSA_LAS_STATEMENT_BYTES      (K * N * 23 / 8)
#define MLDSA_LAS_WITNESS_BYTES        (L * POLYETA_PACKEDBYTES)

void mldsa_las_pack_statement(uint8_t r[MLDSA_LAS_STATEMENT_BYTES],
                              const mldsa_statement *Y);
int  mldsa_las_unpack_statement(mldsa_statement *Y,
                                const uint8_t r[MLDSA_LAS_STATEMENT_BYTES]);
void mldsa_las_pack_witness(uint8_t r[MLDSA_LAS_WITNESS_BYTES],
                            const mldsa_witness *y);
int  mldsa_las_unpack_witness(mldsa_witness *y,
                              const uint8_t r[MLDSA_LAS_WITNESS_BYTES]);

#endif
