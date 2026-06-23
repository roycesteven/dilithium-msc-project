#ifndef BASESIG_H
#define BASESIG_H

/*
 * basesig.{c,h} -- the SEPARATE simplified Dilithium-style BASE signature.
 *
 * This is the plain post-quantum signature that LAS extends: KeyGen, Sign, Verify,
 * with NO adaptor statement Y anywhere in the Fiat-Shamir hash.
 *
 *     KeyGen : r <- S_1^{n+l};  t = A r;  (pk, sk) = (t, r)
 *     Sign   : y <- S_g; w = A y; c = H(pk, w,  M); z = y + c r; |z|inf <= g-k
 *     Verify : w' = A z - c t;   accept iff  c == H(pk, w',  M)
 *
 * It is kept deliberately SEPARATE from las.{c,h} so the LAS adaptor protocol is
 * never touched or conflated: las.{c,h} are byte-for-byte unchanged.  basesig
 * depends on las.h ONLY for the shared parameter macros (LAS_N/ELL/KAPPA/GAMMA/...)
 * and the key/signature struct layout (las_pp/las_pk/las_sk/las_sig); all of its
 * signing and verification logic is its own.  Sharing the parameters keeps the two
 * schemes at the SAME security level (a fair comparison), and sharing the struct
 * layout makes their keys and signatures interchangeable.
 *
 * That interchangeability is exactly the point of the Stage-1 comparison: a LAS
 * pre-signature, once Adapted, is a fully ORDINARY signature that THIS independent
 * base verifier accepts -- with no explicit +Y -- because
 *
 *     A(z_hat + y) - c t = (A z_hat - c t) + A y = w' + Y      (since Y = A y).
 *
 * basesig reuses only the repo's mode-independent primitives (poly/NTT/SHAKE); its
 * static helpers (matrix product, challenge hash, rejection sampling, norm check)
 * are local copies, so the file compiles and links independently of las.c.
 */

#include <stddef.h>
#include <stdint.h>
#include "las.h"   /* shared parameters + las_pp/las_pk/las_sk/las_sig types ONLY */

/* ---- Rejection-sampling instrumentation (measurement only) ----
 * Counts the total rejection-loop attempts performed by base_sign since last
 * reset, mirroring las_attempts for the adaptor path so the base and adaptor
 * restart counts can be compared directly.  Never read by the scheme itself. */
extern unsigned long base_attempts;

/* KeyGen = Gen: r<-S_1^(n+l); t=Ar; (pk,sk)=(t,r). */
void base_keygen(las_pk *pk, las_sk *sk, const las_pp *pp);

/* Sign: ordinary simplified Dilithium-style signature; c = H(pk, w, M), no Y. */
void base_sign(las_sig *sig, const uint8_t *m, size_t mlen,
               const las_pk *pk, const las_sk *sk, const las_pp *pp);

/* Verify: recompute w' = A z - c t, accept iff c == H(pk, w', M). 0 on success. */
int  base_verify(const las_sig *sig, const uint8_t *m, size_t mlen,
                 const las_pk *pk, const las_pp *pp);

#endif
