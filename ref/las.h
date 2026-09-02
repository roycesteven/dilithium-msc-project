#ifndef LAS_H
#define LAS_H

/*
 * LAS - Lattice-based Adaptor Signature (Esgin, Ersoy, Erkin, eprint 2020/845,
 * Algorithm 2), implemented as the paper's SIMPLIFIED scheme on top of the
 * CRYSTALS-Dilithium reference primitives.
 *
 * las.{c,h} hold ONLY the four Algorithm-2 adaptor operations (PreSign /
 * PreVerify / Adapt / Ext) plus their deterministic-KAT and AMHL K-hop
 * variants.  Definition 3 says the adaptor scheme INHERITS KeyGen/Sign/Verify
 * from the underlying signature (they live in ref/basesig.{c,h}, Algorithm 1),
 * and the hard-relation generator Gen lives in ref/relation.{c,h}.  Adapt's
 * OUTPUT is a fully ordinary signature sigma = (c, z); it is checked by basesig's
 * base_verify (there is no adaptor-specific final verifier here).
 *
 *   -- Algorithm 2 (adaptor layer; upstream = the PAPER, names kept) --
 *   las_presign_internal / las_presign / las_presign_det /
 *   las_preverify_internal / las_preverify / las_presign_k / las_preverify_k /
 *   las_adapt / las_ext.
 *
 * This is deliberately NOT optimized Dilithium: there is no Power2Round, no
 * hint vector, and no high/low-bit decomposition.  We reuse only the repo's
 * mode-independent primitives - the NTT, SHAKE/Keccak, modular poly arithmetic
 * (mod Q, degree d = LAS_D = 256) and rejection sampling.
 *
 * Relation (hard MSIS/MLWE):  statement Y = A*r' mod q, honest witness r' in S_1
 * (relation.c owns Gen).  A statement is pk-shaped but a DISTINCT type.
 *
 *   A = [ I_n | A' ] in R_q^{n x (n+ell)},  R_q = Z_q[X]/(X^d + 1)
 *   (paper<->code: paper ring degree d = code LAS_D = 256; module rank n = LAS_N)
 *
 * PreSign:   y<-S_g; w=Ay; c=H(pk, w+Y, M); z_hat=y+c*r; |z_hat|inf<=g-k-1  (Y folded in!)
 * PreVerify: w'=A z_hat - c*t; check c==H(pk, w'+Y, M)
 * Adapt:     z = z_hat + r'     (standard Verify then sees Az-ct = w+Y)
 * Ext:       s = z - z_hat
 *
 * Wire note: the challenge travels as the DIGEST c_tilde (LAS_CTILDEBYTES wide,
 * so per parameter set -- 48 B at the target setting, not 32); c is re-derived
 * as SampleInBall(c_tilde) by the paths that verify -- las_preverify, and
 * las_adapt through the las_preverify it runs.  las_ext derives no challenge at
 * all: it subtracts and checks A*s == Y.  An earlier stage of this build stored
 * the expanded polynomial c instead, which is what this note used to describe.
 */

#include <stddef.h>
#include <stdint.h>
#include "poly.h"      /* poly type; pulls in params.h => N (=LAS_D), Q (mode-independent) */
#include "setup.h"     /* shared construction parameters + public_params + setup_public_params() */
#include "las_types.h" /* the six protocol object types (pre_signature is the Algorithm-2 object) */
#include "serialize.h" /* shared wire codec: byte sizes for the packed-API tier below */

/* poly_chknorm() rejects when ||.||inf >= bound, so encode the strict ">" tests
 * as bound = (limit)+1.  (BOUND_SIGN -- the Algorithm-1 bound SHARED with basesig
 * -- lives in basesig.h; only the adaptor-only PreSign bounds live here.) */
#define BOUND_PRESIGN  (GAMMA - KAPPA)                     /* reject |z_hat|inf > g-k-1 */

/* AMHL K-hop PreSign bound: reject |z_hat|inf > g-k-K, i.e. accept <= g-k-K.
 * Leaves a norm budget of K for the cumulative witness s_j (||s_j||inf <= j <= K),
 * so the adapted z = z_hat + s_j still satisfies ||z||inf <= g-k and clears the
 * ordinary Verify bound.  For K=1 this collapses to BOUND_PRESIGN.  Callers must
 * keep the bound positive (nhops in [1, gamma-kappa]); this is the optional AMHL
 * tier -- las_presign_k/las_preverify_k are hooks for chain/amhl callers. */
#define BOUND_PRESIGN_K(K)  (GAMMA - KAPPA - (int32_t)(K) + 1)

/* ---- Rejection-sampling instrumentation (measurement only) ----
 * Counts the total rejection-loop attempts performed by las_presign since it was
 * last reset.  Never used by the scheme; benchmarks reset and read it to report
 * the restart rate DIRECTLY.  Single-threaded use only. */
extern unsigned long las_attempts;

/* EXACT expected attempts/call of the rejection loop running at `bound`
 * (BOUND_SIGN or BOUND_PRESIGN), for validating a measured attempt counter
 * against theory -- the benchmarks' run-validity rejection gate.  Instrumentation
 * only.  At the D3 engineering set (6,5,49): Sign 2.71875, PreSign 2.77483. */
double las_expected_attempts(int32_t bound);

/* ==================== Algorithm 2 (adaptor layer) ====================
 * No basesig.c/sign.c analogue: the four adaptor operations LAS adds, plus their
 * deterministic and AMHL K-hop variants.  Upstream = the PAPER. */

/* PreSign body, parameterised by the rejection bound (g-k-1 single-hop, g-k-K
 * AMHL) and the 64-byte mask seed.  Returns 0 (success). */
int las_presign_internal(pre_signature *presig,
                         const uint8_t *m, size_t mlen,
                         const statement *Y, const public_key *pk, const secret_key *sk,
                         const public_params *pp, int32_t bound,
                         const uint8_t mask_seed[64]);

/* PreSign(sk,Y,M), random path, single-hop bound g-k-1.  Returns 0 (success). */
int las_presign(pre_signature *presig, const uint8_t *m, size_t mlen,
                const statement *Y, const public_key *pk, const secret_key *sk,
                const public_params *pp);

/* Deterministic PreSign: mask randomness derived from (sk, Y, M); single-hop
 * bound; for reproducible adaptor KATs.  Returns 0 (success). */
int las_presign_det(pre_signature *presig, const uint8_t *m, size_t mlen,
                    const statement *Y, const public_key *pk, const secret_key *sk,
                    const public_params *pp);

/* PreVerify body, parameterised by the rejection bound.  Returns 0/-1. */
int las_preverify_internal(const pre_signature *presig, const uint8_t *m, size_t mlen,
                           const statement *Y, const public_key *pk, const public_params *pp,
                           int32_t bound);

/* PreVerify(Y,pk,sigma_hat,M), single-hop bound.  Returns 0 on success. */
int las_preverify(const pre_signature *presig, const uint8_t *m, size_t mlen,
                  const statement *Y, const public_key *pk, const public_params *pp);

/* AMHL K-hop variants (eprint 2020/845 Fig. 2 / Section 5), tighter bound g-k-K.
 * Optional/bonus tier hooks; Adapt and Ext are unchanged.  Callers must validate
 * nhops so BOUND_PRESIGN_K(nhops) stays positive (nhops >= 1). */
int las_presign_k(pre_signature *presig, const uint8_t *m, size_t mlen,
                  const statement *Y, const public_key *pk, const secret_key *sk,
                  const public_params *pp, unsigned int nhops);
int las_preverify_k(const pre_signature *presig, const uint8_t *m, size_t mlen,
                    const statement *Y, const public_key *pk, const public_params *pp,
                    unsigned int nhops);

/* Adapt((Y,r'),sigma_hat): PreVerify, then sigma=(c, z_hat + r').  Returns 0 on success. */
int las_adapt(signature *sig, const pre_signature *presig, const uint8_t *m, size_t mlen,
              const statement *Y, const witness *r_prime, const public_key *pk,
              const public_params *pp);

/* Ext(Y,sigma,sigma_hat): s = z - z_hat; returns 0 and the witness s iff A*s==Y. */
int las_ext(witness *s, const signature *sig, const pre_signature *presig,
            const statement *Y, const public_params *pp);

/* ============== end-to-end PACKED-API tier (bytes in/out) ==============
 * The SECOND measured boundary.  Byte objects are unpacked/packed INSIDE the
 * call with the TYPED codecs; malformed bytes -> -1.  Mirrors basesig.c's packed
 * tier (which holds the Algorithm-1 twins). */
int las_presign_packed(uint8_t presig_b[PRE_SIGNATURE_BYTES],
                       const uint8_t *m, size_t mlen,
                       const uint8_t Y_b[STATEMENT_BYTES],
                       const uint8_t pk_b[PUBLIC_KEY_BYTES],
                       const uint8_t sk_b[SECRET_KEY_BYTES],
                       const public_params *pp);
int las_preverify_packed(const uint8_t presig_b[PRE_SIGNATURE_BYTES],
                         const uint8_t *m, size_t mlen,
                         const uint8_t Y_b[STATEMENT_BYTES],
                         const uint8_t pk_b[PUBLIC_KEY_BYTES],
                         const public_params *pp);
int las_adapt_packed(uint8_t sig_b[SIGNATURE_BYTES],
                     const uint8_t presig_b[PRE_SIGNATURE_BYTES],
                     const uint8_t *m, size_t mlen,
                     const uint8_t Y_b[STATEMENT_BYTES],
                     const uint8_t r_prime_b[WITNESS_BYTES],
                     const uint8_t pk_b[PUBLIC_KEY_BYTES],
                     const public_params *pp);
/* NOTE: las_ext_packed packs the extracted witness s with the TERNARY witness
 * codec (WITNESS_BYTES), so it ONLY applies to an HONEST, ternary, single-hop
 * witness (s = r', ||s||inf <= 1).  A general or AMHL cumulative extracted
 * witness (||s||inf > 1, relation R'_A -- the knowledge gap) is NOT ternary and
 * cannot be packed here; take it from the struct-level las_ext instead. */
int las_ext_packed(uint8_t s_b[WITNESS_BYTES],
                   const uint8_t sig_b[SIGNATURE_BYTES],
                   const uint8_t presig_b[PRE_SIGNATURE_BYTES],
                   const uint8_t Y_b[STATEMENT_BYTES],
                   const public_params *pp);

#endif
