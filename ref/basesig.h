#ifndef BASESIG_H
#define BASESIG_H

/*
 * basesig.{c,h} -- the SEPARATE simplified Dilithium-style BASE signature,
 * i.e. Algorithm 1 ("Lattice-Based Signature") of eprint 2020/845.
 *
 * DELIBERATELY STRUCTURED AS A MIRROR OF THE UPSTREAM ML-DSA REFERENCE
 * (ref/sign.c).  The three procedures below correspond one-to-one to the three
 * upstream entry points, so a side-by-side read shows EXACTLY what Algorithm 1
 * simplifies away from ML-DSA:
 *
 *     base_sign_keypair    <->  crypto_sign_keypair            (sign.c:23)
 *     base_sign_signature  <->  crypto_sign_signature_internal (sign.c:85)
 *     base_sign_verify     <->  crypto_sign_verify_internal    (sign.c:289)
 *
 * The names are base-tagged (not the literal crypto_sign_*) on purpose: the real
 * ML-DSA crypto_sign_* live in sign.c and mean OPTIMISED Dilithium; keeping the
 * base names distinct avoids a link clash and keeps `grep` unambiguous.  Inside
 * each function the body reuses sign.c's exact idioms (the seed/SHAKE expansion,
 * the `rej:` rejection loop, the NTT hoisting, the same variable names
 * rho/mat/y/z/w/c/nonce/state) and every block is annotated REUSED / CHANGED /
 * DELETED against the corresponding sign.c line.
 *
 * Algorithm 1 (paper p.7-8), the target of this file:
 *     KeyGen : r <- S_1^{n+l};  t = A r;  (pk, sk) = (t, r)
 *     Sign   : y <- S_g; w = A y; c = H(pk, w,  M); z = y + c r; reject |z|inf > g-k
 *     Verify : w' = A z - c t;   accept iff  c == H(pk, w',  M)
 *
 * What Algorithm 1 DELETES vs ML-DSA (all "for ease of presentation", paper
 * s2.2 / s3.2 -- these are precisely the lines removed below): Power2Round key
 * compression, the high/low-bit Decompose, the hint vector (MakeHint/UseHint,
 * the OMEGA bound), the second (low-bits) rejection test, and hashing only the
 * high bits.  What it CHANGES: eta-sampling -> ternary S_1; the gamma1
 * power-of-two mask -> uniform S_gamma with gamma = kappa*d*(n+l); tau-weight
 * SampleInBall -> kappa-weight challenge; and it hashes the FULL commitment w.
 *
 * basesig depends on las.h ONLY for the shared parameter macros
 * (LAS_N/ELL/KAPPA/GAMMA/...) and the key/signature struct layout
 * (las_pp/las_pk/las_sk/las_sig); las.{c,h} are byte-for-byte untouched.
 * Sharing the parameters keeps the two schemes at the SAME setting (a fair
 * comparison); sharing the struct layout makes their keys and signatures
 * interchangeable -- an Adapted LAS pre-signature is a fully ORDINARY signature
 * that THIS base_sign_verify accepts with no explicit +Y, because
 *
 *     A(z_hat + y) - c t = (A z_hat - c t) + A y = w' + Y      (since Y = A y).
 *
 * basesig reuses only the repo's mode-independent primitives (poly/NTT/SHAKE);
 * its static helpers (matrix product, challenge hash, samplers, norm check) are
 * local copies, so the file compiles and links independently of las.c.
 */

#include <stddef.h>
#include <stdint.h>
#include "las.h"   /* shared parameters + las_pp/las_pk/las_sk/las_sig types ONLY */

/* ---- Rejection-sampling instrumentation (measurement only; no upstream analogue) ----
 * Counts the total rejection-loop attempts performed by base_sign_signature
 * since last reset, mirroring las_attempts for the adaptor path so base and
 * adaptor restart counts can be compared directly.  Never read by the scheme. */
extern unsigned long base_attempts;

/* base_sign_keypair  <->  crypto_sign_keypair (sign.c:23).  Algorithm 1 KeyGen:
 * r <- S_1^{n+l}; t = A r; (pk, sk) = (t, r). */
void base_sign_keypair(las_pk *pk, las_sk *sk, const las_pp *pp);

/* base_sign_signature  <->  crypto_sign_signature_internal (sign.c:85).
 * Algorithm 1 Sign: c = H(pk, w, M), z = y + c r, reject |z|inf > g-k. No Y. */
void base_sign_signature(las_sig *sig, const uint8_t *m, size_t mlen,
                         const las_pk *pk, const las_sk *sk, const las_pp *pp);

/* base_sign_verify  <->  crypto_sign_verify_internal (sign.c:289).
 * Algorithm 1 Verify: w' = A z - c t; accept iff c == H(pk, w', M). 0 on success. */
int  base_sign_verify(const las_sig *sig, const uint8_t *m, size_t mlen,
                      const las_pk *pk, const las_pp *pp);

#endif
