#ifndef BASESIG_H
#define BASESIG_H

/*
 * basesig.{c,h} -- the SEPARATE simplified Dilithium-style BASE signature,
 * i.e. Algorithm 1 ("Lattice-Based Signature") of eprint 2020/845.
 *
 * DELIBERATELY STRUCTURED AS A MIRROR OF THE UPSTREAM ML-DSA REFERENCE
 * (ref/sign.{c,h}): SAME function count, SAME order, SAME int-return
 * convention; every name is the uniform prefix swap crypto_sign* ->
 * base_sign* (the real ML-DSA crypto_sign_* live in sign.c and mean
 * OPTIMISED Dilithium; distinct names avoid a link clash and keep `grep`
 * unambiguous).  The declarations below follow sign.h one-to-one:
 *
 *     base_sign_keypair             <->  crypto_sign_keypair            (sign.h:10)
 *     base_sign_signature_internal  <->  crypto_sign_signature_internal (sign.h:13)
 *     base_sign_signature           <->  crypto_sign_signature          (sign.h:23)
 *     base_sign                     <->  crypto_sign                    (sign.h:29)
 *     base_sign_verify_internal     <->  crypto_sign_verify_internal    (sign.h:35)
 *     base_sign_verify              <->  crypto_sign_verify             (sign.h:44)
 *     base_sign_open                <->  crypto_sign_open               (sign.h:50)
 *
 * Algorithm 1 (paper p.7-8), the target of this file:
 *     KeyGen : r <- S_1^{n+l};  t = A r;  (pk, sk) = (t, r)
 *     Sign   : y <- S_g; w = A y; c = H(pk, w,  M); z = y + c r; reject |z|inf > g-k
 *     Verify : w' = A z - c t;   accept iff  c == H(pk, w',  M)
 *
 * What Algorithm 1 DELETES vs ML-DSA (all "for ease of presentation", paper
 * s2.2 / s3.2 -- these are precisely the lines removed in basesig.c):
 * Power2Round key compression, the high/low-bit Decompose, the hint vector
 * (MakeHint/UseHint, the OMEGA bound), the second (low-bits) rejection test,
 * hashing only the high bits, and the byte-packed key/signature/ctx APIs
 * (keys and signatures are structs here).  What it CHANGES: eta-sampling ->
 * ternary S_1; the gamma1 power-of-two mask -> uniform S_gamma with
 * gamma = kappa*d*(n+l); tau-weight SampleInBall -> kappa-weight challenge;
 * and it hashes the FULL commitment w.
 *
 * basesig depends on las.h ONLY for the shared parameter macros
 * (LAS_N/ELL/KAPPA/GAMMA/...) and the key/signature struct layout
 * (las_pp/las_pk/las_sk/las_sig); las.{c,h} are byte-for-byte untouched.
 * Sharing the parameters keeps the two schemes at the SAME setting (a fair
 * comparison); sharing the struct layout makes their keys and signatures
 * interchangeable -- an Adapted LAS pre-signature is a fully ORDINARY
 * signature that THIS base_sign_verify accepts with no explicit +Y, because
 *
 *     A(z_hat + y) - c t = (A z_hat - c t) + A y = w' + Y      (since Y = A y).
 *
 * basesig reuses only the repo's mode-independent primitives (poly/NTT/SHAKE);
 * its static helpers (matrix product, challenge hash, samplers, norm check)
 * are local copies, so the file compiles and links independently of las.c.
 */

#include <stddef.h>
#include <stdint.h>
#include "las.h"   /* shared parameters + las_pp/las_pk/las_sk/las_sig types ONLY */

/* ---- Rejection-sampling instrumentation (measurement only; no sign.h analogue) ----
 * Counts the total rejection-loop attempts performed by base_sign_signature
 * since last reset, mirroring las_attempts for the adaptor path so base and
 * adaptor restart counts can be compared directly.  Never read by the scheme. */
extern unsigned long base_attempts;

/* (no DILITHIUM_NAMESPACE(keypair) define: base symbols are mode-independent) */
int base_sign_keypair(las_pk *pk, las_sk *sk, const las_pp *pp);

/* (no DILITHIUM_NAMESPACE(signature_internal) define) */
int base_sign_signature_internal(las_sig *sig,
                                 const uint8_t *m,
                                 size_t mlen,
                                 const las_pk *pk,
                                 const las_sk *sk,
                                 const las_pp *pp,
                                 const uint8_t seed[64]);

/* (no DILITHIUM_NAMESPACE(signature) define) */
int base_sign_signature(las_sig *sig,
                        const uint8_t *m, size_t mlen,
                        const las_pk *pk, const las_sk *sk,
                        const las_pp *pp);

/* (no DILITHIUM_NAMESPACETOP define) */
int base_sign(las_sig *sig, uint8_t *sm, size_t *smlen,
              const uint8_t *m, size_t mlen,
              const las_pk *pk, const las_sk *sk,
              const las_pp *pp);

/* (no DILITHIUM_NAMESPACE(verify_internal) define) */
int base_sign_verify_internal(const las_sig *sig,
                              const uint8_t *m,
                              size_t mlen,
                              const las_pk *pk,
                              const las_pp *pp);

/* (no DILITHIUM_NAMESPACE(verify) define) */
int base_sign_verify(const las_sig *sig,
                     const uint8_t *m, size_t mlen,
                     const las_pk *pk,
                     const las_pp *pp);

/* (no DILITHIUM_NAMESPACE(open) define) */
int base_sign_open(uint8_t *m, size_t *mlen,
                   const las_sig *sig,
                   const uint8_t *sm, size_t smlen,
                   const las_pk *pk,
                   const las_pp *pp);

#endif
