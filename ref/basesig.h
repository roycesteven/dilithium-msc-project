#ifndef BASESIG_H
#define BASESIG_H

/*
 * basesig.{c,h} -- the SEPARATE simplified Dilithium-style BASE signature,
 * i.e. Algorithm 1 ("Lattice-Based Signature") of eprint 2020/845.  This is the
 * ONE canonical Algorithm-1 implementation of the build (Definition 3: the
 * adaptor scheme INHERITS KeyGen/Sign/Verify from the base signature, so they
 * live HERE and only here; las.c holds Algorithm 2 only, and an Adapt output is
 * verified by THIS file's base_verify).
 *
 * STRUCTURED AS A MIRROR OF THE UPSTREAM ML-DSA REFERENCE (ref/sign.{c,h}):
 * SAME order, SAME int-return convention; names by the uniform prefix swap
 * crypto_sign* -> base_*.  Declarations follow sign.h one-to-one:
 *
 *   base_keygen                   <->  crypto_sign_keypair            (sign.c:23)
 *   base_keygen_seed               -   deterministic KeyGen body (KAT slot; no sign.c slot)
 *   base_sign_internal            <->  crypto_sign_signature_internal (sign.c:85)
 *   base_sign                     <->  crypto_sign_signature          (sign.c:206)
 *   base_sign_det                  -   deterministic Sign (KAT slot; no sign.c slot)
 *   base_verify_internal          <->  crypto_sign_verify_internal    (sign.c:289)
 *   base_verify                   <->  crypto_sign_verify             (sign.c:375)
 *   (the base_sign/base_sign_open sm-wrappers were dropped: struct signatures
 *    carry no byte prefix, so "sm = sig || m" degenerates to sm = m.)
 *
 * Algorithm 1 (paper p.7-8), the target of this file:
 *     KeyGen : r <- S_1^{n+ell};  t = A r;  (pk, sk) = (t, r)
 *     Sign   : y <- S_g; w = A y; c = H(pk, w,  M); z = y + c r; reject |z|inf > g-k
 *     Verify : w' = A z - c t;   accept iff  c == H(pk, w',  M)
 *
 * basesig does NOT include las.h: everything Algorithm 1 needs -- the
 * construction parameters (setup.h) and its OWN object types (public_key /
 * secret_key / signature, defined in the shared las_types.h) -- lives below
 * both schemes.  A LAS-adapted pre-signature is a fully ORDINARY signature that
 * THIS base_verify accepts with no explicit +Y, because
 *
 *     A(z_hat + r') - c t = (A z_hat - c t) + A r' = w' + Y      (since Y = A r').
 *
 * basesig reuses only the repo's mode-independent primitives (poly/NTT/SHAKE);
 * its static helpers (matrix product, challenge hash, samplers, norm check) are
 * local copies, so the file compiles and links independently of las.c.
 */

#include <stddef.h>
#include <stdint.h>
#include "setup.h"     /* SHARED layer: construction parameters + public_params -- NOT las.h */
#include "las_types.h" /* SHARED object types (public_key/secret_key/signature owned here) */
#include "serialize.h" /* SHARED codec: {PUBLIC_KEY,SECRET_KEY,SIGNATURE}_BYTES for the packed tier */

/* Algorithm-1 Sign/Verify rejection bound (chknorm-style: reject `>= bound`, so
 * the strict `>` test is encoded as bound = limit+1): Sign and Verify reject at
 * ||z||inf > gamma-kappa (Alg. 1 steps 11/16), and an Adapt output must clear
 * exactly this bound.  This is the Algorithm-1 rejection rule, so basesig OWNS
 * it (the adaptor-only BOUND_PRESIGN lives in las.h).  Moved here from setup.h. */
#define BOUND_SIGN  (GAMMA - KAPPA + 1)

/* ---- Rejection-sampling instrumentation (measurement only; no sign.h analogue) ----
 * Counts the total rejection-loop attempts performed by base_sign_internal since
 * last reset, mirroring las_attempts for the adaptor path so base and adaptor
 * restart counts can be compared directly.  Never read by the scheme. */
extern unsigned long base_attempts;

/* base_keygen  <->  crypto_sign_keypair.  KeyGen: r<-S_1^(n+ell); t=Ar; (pk,sk)=(t,r).
 * (Gen (relation.c) uses the same sampling/arithmetic but is a distinct algorithm
 * returning a (statement, witness) pair.)  Returns 0 (success). */
int base_keygen(public_key *pk, secret_key *sk, const public_params *pp);

/* Deterministic KeyGen body from an explicit 32-byte seed (reproducible KAT
 * vectors).  No sign.h slot.  Returns 0 (success). */
int base_keygen_seed(public_key *pk, secret_key *sk, const public_params *pp,
                     const uint8_t seed[LAS_SEEDBYTES]);

/* base_sign_internal  <->  crypto_sign_signature_internal.  Sign body,
 * parameterised by the 64-byte mask seed (implementation randomness, no paper
 * symbol; Dilithium's rnd).  Returns 0 (success). */
int base_sign_internal(signature *sig,
                       const uint8_t *m, size_t mlen,
                       const public_key *pk, const secret_key *sk,
                       const public_params *pp, const uint8_t mask_seed[64]);

/* base_sign  <->  crypto_sign_signature.  Sign, random path (fresh mask seed).
 * Returns 0 (success). */
int base_sign(signature *sig,
              const uint8_t *m, size_t mlen,
              const public_key *pk, const secret_key *sk,
              const public_params *pp);

/* base_sign_det  (KAT slot).  Deterministic Sign: the per-signature mask
 * randomness is derived from (sk, M), so the output is a deterministic function
 * of its inputs.  Same distribution/validity as base_sign.  Returns 0 (success). */
int base_sign_det(signature *sig,
                  const uint8_t *m, size_t mlen,
                  const public_key *pk, const secret_key *sk,
                  const public_params *pp);

/* base_verify_internal  <->  crypto_sign_verify_internal.  Verify body:
 * w' = A z - c t; accept iff c == H(pk, w', M).  The ONLY verifier a final
 * (ordinary or adapted) signature ever meets -- a pre_signature cannot be passed
 * here (distinct type; PreVerify lives in las.c).  0 on success, -1 otherwise. */
int base_verify_internal(const signature *sig,
                         const uint8_t *m, size_t mlen,
                         const public_key *pk, const public_params *pp);

/* base_verify  <->  crypto_sign_verify.  Verify, public entry point.
 * Returns 0 on success, -1 otherwise. */
int base_verify(const signature *sig,
                const uint8_t *m, size_t mlen,
                const public_key *pk, const public_params *pp);

/* ============== end-to-end PACKED-API tier (bytes in/out) ==============
 * The SECOND measured boundary, mirroring upstream's ONLY boundary (sign.c
 * packs/unpacks bytes inside its API with packing.h).  The struct functions
 * above are the CORE CRYPTO tier; these end-to-end twins unpack the byte keys
 * (validating; malformed -> -1), run the core, and pack the outputs, using the
 * shared codec ref/serialize.{c,h}.  base_verify_packed is the byte interface an
 * on-chain verifier consumes (it moved here from serialize, where the codec now
 * stays pure). */
int base_keygen_packed(uint8_t pk_b[PUBLIC_KEY_BYTES],
                       uint8_t sk_b[SECRET_KEY_BYTES],
                       const public_params *pp);
int base_sign_packed(uint8_t sig_b[SIGNATURE_BYTES],
                     const uint8_t *m, size_t mlen,
                     const uint8_t pk_b[PUBLIC_KEY_BYTES],
                     const uint8_t sk_b[SECRET_KEY_BYTES],
                     const public_params *pp);
int base_verify_packed(const uint8_t sig_b[SIGNATURE_BYTES],
                       const uint8_t *m, size_t mlen,
                       const uint8_t pk_b[PUBLIC_KEY_BYTES],
                       const public_params *pp);

#endif
