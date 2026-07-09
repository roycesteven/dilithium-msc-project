/*
 * las.c -- LAS, Lattice-based Adaptor Signature (eprint 2020/845, Algorithm 2),
 * written as a STRUCTURAL MIRROR of ref/basesig.c (which itself mirrors the
 * upstream ML-DSA reference ref/sign.c the same way): SAME function order,
 * SAME return convention (int, 0 = success), SAME inline composition (the
 * SHAKE challenge-hash block, the matrix-vector sequence and the `rej:` loop
 * are written out in the scheme functions exactly where basesig.c writes
 * them), names by the uniform prefix swap base_sign* -> las* (full chain
 * crypto_sign* -> base_sign* -> las*):
 *
 *   -- shared setup: ref/setup.{c,h}, NOT in this file --
 *   las_setup                -   the paper's Setup() -> pp, consumed by BOTH
 *                                basesig.c and las.c; a separate file because
 *                                A is public infrastructure, not scheme code
 *                                (see the [DELETED] notes at basesig.c:136-140
 *                                and basesig.c:267-269)
 *
 *   -- Algorithm 1 (base path; one-to-one with basesig.c) --
 *   las_keypair             <->  base_sign_keypair             (basesig.c:115)
 *   las_keypair_seed         -   deterministic KeyGen body (KAT path; no slot)
 *   las_signature_internal  <->  base_sign_signature_internal  (basesig.c:212)
 *   las_signature           <->  base_sign_signature           (basesig.c:426)
 *   las_signature_det        -   deterministic Sign (KAT path; no slot)
 *   las_sign                <->  base_sign                     (basesig.c:464)
 *   las_verify_internal     <->  base_sign_verify_internal     (basesig.c:499)
 *   las_verify              <->  base_sign_verify              (basesig.c:645)
 *   las_open                <->  base_sign_open                (basesig.c:667)
 *
 *   -- Algorithm 2 (adaptor layer; upstream = the PAPER, names kept) --
 *   las_presign_internal / las_presign / las_presign_det   (adaptor twins of
 *       las_signature_internal / las_signature / las_signature_det)
 *   las_preverify_internal / las_preverify                 (adaptor twins of
 *       las_verify_internal / las_verify)
 *   las_presign_k / las_preverify_k                        (AMHL bound γ−κ−K)
 *   las_adapt / las_ext                                    (paper only)
 *
 * ANNOTATION CONVENTION (read side by side with basesig.c):
 *   The Algorithm 1 functions quote basesig.c lines verbatim --
 *   [REUSED]  basesig.c:<line>: <code>  -- same line, prefix b_ -> las_;
 *   [CHANGED] quotes the basesig.c line(s) verbatim, then states WHY the
 *             line differs here;
 *   [DELETED] quotes the dropped basesig.c line(s) verbatim, then WHY.
 *   The Algorithm 2 functions have no basesig.c analogue; they quote their
 *   OWN Algorithm 1 twin in THIS file the same way (las.c:<line>: <code>),
 *   so PreSign diffs against Sign and PreVerify against Verify.
 *   basesig.c's section comments (which are sign.c's, kept verbatim there)
 *   are kept verbatim here too, so the three files scroll in lockstep.
 *
 * NO INVENTED HELPERS: every local helper at the bottom of this file is a
 * VERBATIM copy of the basesig.c helper of the same name (prefix las_
 * instead of b_), each of which is a one-to-one twin of a NAMED upstream
 * poly.c/polyvec.c function -- see basesig.c:703-1087 for the per-helper
 * [CHANGED] derivations vs upstream.  Exactly two additions have no
 * basesig.c body: las_polyvecm_sub (the polyveck_sub twin at width m; only
 * Ext subtracts response vectors, and the base scheme has no Ext) and
 * det_seed (mask seed for the deterministic KAT path; LAS-only).
 *
 * The helpers are local copies (not shared) so basesig.c and las.c stay
 * independently linkable; they are behaviourally IDENTICAL, hence A*r and
 * the challenge hash H(pk, w, M) match basesig.c bit-for-bit and a
 * LAS-adapted signature verifies under basesig.c's independent verifier.
 */
#include <stdint.h>
#include "params.h"
#include "las.h"        /* <-> "basesig.h" (basesig.c:41); shared params/types */
#include "serialize.h"  /* [REUSED] basesig.c:44: #include "serialize.h"
                         * (itself [REUSED] sign.c:4: #include "packing.h")
                         * for the end-to-end PACKED-API tier at the bottom */
#include "poly.h"
#include "randombytes.h"
#include "fips202.h"

/* Rejection-sampling attempt counter (measurement only; see las.h).
 * [CHANGED] basesig.c:54:
 *     unsigned long base_attempts = 0;
 * WHY: LAS keeps its own counter so the two schemes' benchmark
 * instrumentation never shares state (bench_levels.c reads both). */
unsigned long las_attempts = 0;

/* Exact expected attempts/call for the rejection loop at `bound` (see las.h
 * for the derivation).  Instrumentation only -- never called by the scheme;
 * no basesig.c analogue (the benchmarks' rejection gate needs the PreSign
 * bound, which only exists here).  p^((n+ell)*d) via square-and-multiply
 * instead of libm pow(), so las.c keeps zero dependencies beyond the reused
 * Dilithium primitives. */
double las_expected_attempts(int32_t bound) {
  double p = (2.0*(double)bound - 1.0) / (2.0*(double)LAS_GAMMA + 1.0);
  double acc = 1.0;
  unsigned int e = (unsigned int)LAS_M * N;      /* (n+ell)*d coefficients */
  while(e) {
    if(e & 1u) acc *= p;
    p *= p;
    e >>= 1u;
  }
  return 1.0 / acc;
}

/* ---- local helpers, DEFINED AT THE BOTTOM of this file: VERBATIM copies of
 * basesig.c's local twins (prefix b_ -> las_), same order:
 *
 *   las_rej_S1               <->  b_rej_S1                 (basesig.c:718)
 *   las_poly_uniform_S1      <->  b_poly_uniform_S1        (basesig.c:748)
 *   las_rej_Sgamma           <->  b_rej_Sgamma             (basesig.c:786)
 *   las_poly_uniform_Sgamma  <->  b_poly_uniform_Sgamma    (basesig.c:825)
 *   las_poly_challenge       <->  b_poly_challenge         (basesig.c:860)
 *   las_polyw_pack           <->  b_polyw_pack             (basesig.c:905)
 *   las_polyvec_matrix_pointwise_montgomery                (basesig.c:927)
 *   las_polyvecl_pointwise_acc_montgomery                  (basesig.c:936)
 *   las_polyvecl_ntt                                       (basesig.c:949)
 *   las_polyvecm_uniform_S1                                (basesig.c:958)
 *   las_polyvecm_uniform_Sgamma                            (basesig.c:967)
 *   las_polyvecm_reduce / add / ntt / invntt_tomont /
 *     pointwise_poly_montgomery / chknorm                  (basesig.c:975-1023)
 *   las_polyvecn_reduce / caddq / add / sub / ntt /
 *     invntt_tomont / pointwise_poly_montgomery / pack_w   (basesig.c:1026-1087)
 *
 * plus the two with no basesig.c body:
 *   las_polyvecm_sub  <->  polyveck_sub (polyvec.c:218) at width m -- the
 *       same upstream twin as b_polyvecn_sub (basesig.c:1050); only Ext's
 *       s = z - z^ subtracts an m-vector, and the base scheme has no Ext;
 *   det_seed  --  mask seed = SHAKE256(tag, sk, [Y], M) for the _det
 *       variants (deterministic KAT path; LAS-only). */
static unsigned int las_rej_S1(int32_t *a, unsigned int len, const uint8_t *buf, unsigned int buflen);
static void las_poly_uniform_S1(poly *a, const uint8_t seed[LAS_SEEDBYTES], uint16_t nonce);
static unsigned int las_rej_Sgamma(int32_t *a, unsigned int len, const uint8_t *buf, unsigned int buflen);
static void las_poly_uniform_Sgamma(poly *a, const uint8_t seed[64], uint16_t nonce);
static void las_poly_challenge(poly *c, const uint8_t seed[LAS_SEEDBYTES]);
static void las_polyw_pack(uint8_t *r, const poly *a);
static void las_polyvec_matrix_pointwise_montgomery(poly t[LAS_N], const poly mat[LAS_N][LAS_ELL],
                                                    const poly v[LAS_ELL]);
static void las_polyvecl_pointwise_acc_montgomery(poly *w, const poly u[LAS_ELL],
                                                  const poly v[LAS_ELL]);
static void las_polyvecl_ntt(poly v[LAS_ELL]);
static void las_polyvecm_uniform_S1(poly v[LAS_M], const uint8_t seed[LAS_SEEDBYTES], uint16_t nonce);
static void las_polyvecm_uniform_Sgamma(poly v[LAS_M], const uint8_t seed[64], uint16_t nonce);
static void las_polyvecm_reduce(poly v[LAS_M]);
static void las_polyvecm_add(poly w[LAS_M], const poly u[LAS_M], const poly v[LAS_M]);
static void las_polyvecm_sub(poly w[LAS_M], const poly u[LAS_M], const poly v[LAS_M]);
static void las_polyvecm_ntt(poly v[LAS_M]);
static void las_polyvecm_invntt_tomont(poly v[LAS_M]);
static void las_polyvecm_pointwise_poly_montgomery(poly r[LAS_M], const poly *a, const poly v[LAS_M]);
static int  las_polyvecm_chknorm(const poly v[LAS_M], int32_t bound);
static void las_polyvecn_reduce(poly v[LAS_N]);
static void las_polyvecn_caddq(poly v[LAS_N]);
static void las_polyvecn_add(poly w[LAS_N], const poly u[LAS_N], const poly v[LAS_N]);
static void las_polyvecn_sub(poly w[LAS_N], const poly u[LAS_N], const poly v[LAS_N]);
static void las_polyvecn_ntt(poly v[LAS_N]);
static void las_polyvecn_invntt_tomont(poly v[LAS_N]);
static void las_polyvecn_pointwise_poly_montgomery(poly r[LAS_N], const poly *a, const poly v[LAS_N]);
static void las_polyvecn_pack_w(uint8_t r[LAS_N*N*4], const poly w[LAS_N]);
static void det_seed(uint8_t out[64], uint8_t tag, const las_sk *sk,
                     const las_pk *Y, const uint8_t *m, size_t mlen);

/* ==================== scheme, Algorithm 1 (base path) ====================
 * (The shared system setup las_setup lives in ref/setup.c -- see the file
 * header; both basesig.c and las.c consume that same las_pp.) */

/*************************************************
* Name:        las_keypair  <->  base_sign_keypair (basesig.c:115)
*
* Description: Algorithm 1 KeyGen, random path: fresh seed, then the
*              deterministic KeyGen body (las_keypair_seed).  Also used to
*              make the statement/witness pair (Y, y) -- it is literally
*              another key pair.
*
* Returns 0 (success)
**************************************************/
int las_keypair(las_pk *pk,          /* paper t: pk->t = t = A r (public key) */
                las_sk *sk,          /* paper r: sk->s = r (secret key)       */
                const las_pp *pp) {  /* paper A: pp = A = [I | A']            */
  uint8_t seed[LAS_SEEDBYTES];   /* <-> seed (basesig.c:120) */

  /* Get randomness for rho, rhoprime and key */
  randombytes(seed, LAS_SEEDBYTES);          /* [REUSED]  basesig.c:124: randombytes(seed, LAS_SEEDBYTES); */
  return las_keypair_seed(pk, sk, pp, seed);
  /* ^[CHANGED] basesig.c:144-200 (the KeyGen body, starting)
   *     b_polyvecm_uniform_S1(sk->s, seed, 0);
   * WHY: the body is factored out into las_keypair_seed so the KAT path
   * (test_kat.c) can inject the 32-byte seed; basesig.c has no KAT slot and
   * keeps the body inline, exactly like sign.c.  Behaviour is identical. */
}

/*************************************************
* Name:        las_keypair_seed  (deterministic KAT slot; the body of
*              base_sign_keypair, basesig.c:115, minus its randombytes line)
*
* Description: Algorithm 1 KeyGen from an explicit 32-byte seed:
*              r <- S_1^{n+l}; t = A r; (pk,sk) = (t,r).  Reproducible KAT
*              vectors; no basesig.c/sign.c slot.
*
* Returns 0 (success)
**************************************************/
int las_keypair_seed(las_pk *pk,            /* paper t: pk->t = t = A r (public key)       */
                     las_sk *sk,            /* paper r: sk->s = r  (secret key, r <-$ S_1) */
                     const las_pp *pp,      /* paper A: pp = A = [I | A'] (public matrix)  */
                     const uint8_t seed[LAS_SEEDBYTES]) {  /* PRG seed to sample r (no paper symbol) */
  /* [PAPER Alg.1] 1:  procedure KeyGen():    // same as Gen */
  unsigned int j;                /* [REUSED]  basesig.c:119: unsigned int j;         */
  poly s1hat[LAS_ELL];           /* [REUSED]  basesig.c:121: poly s1hat[LAS_ELL];    */

  /* [DELETED] basesig.c:124:
   *     randombytes(seed, LAS_SEEDBYTES);
   * WHY: the seed is the caller's parameter here -- las_keypair draws it
   * fresh; test_kat.c injects a pinned one. */

  /* Sample short vectors s1 and s2 */
  /* [PAPER Alg.1] 2:      r <-$ S_1^(n+l) */
  las_polyvecm_uniform_S1(sk->s, seed, 0);   /* [REUSED]  basesig.c:144: b_polyvecm_uniform_S1(sk->s, seed, 0); */

  /* Matrix-vector multiplication */
  /* [PAPER Alg.1] 3:      t = A r */
  for(j = 0; j < LAS_ELL; ++j)
    s1hat[j] = sk->s[LAS_N + j];             /* [REUSED]  basesig.c:157-158: s1hat[j] = sk->s[LAS_N + j];
                                              * (A = [I | A']: only the bottom l
                                              * components of r meet A')               */
  las_polyvecl_ntt(s1hat);                   /* [REUSED]  basesig.c:165: b_polyvecl_ntt(s1hat); */
  las_polyvec_matrix_pointwise_montgomery(pk->t, pp->mat, s1hat);
                                             /* [REUSED]  basesig.c:166: b_polyvec_matrix_pointwise_montgomery(pk->t, pp->mat, s1hat); */
  las_polyvecn_reduce(pk->t);                /* [REUSED]  basesig.c:168: b_polyvecn_reduce(pk->t); */
  las_polyvecn_invntt_tomont(pk->t);         /* [REUSED]  basesig.c:169: b_polyvecn_invntt_tomont(pk->t); */

  /* Add error vector s2 */
  las_polyvecn_add(pk->t, pk->t, sk->s);     /* [REUSED]  basesig.c:172: b_polyvecn_add(pk->t, pk->t, sk->s);
                                              * (the "error" IS the top n components of
                                              * r: t = r_top + A' r_bot)               */
  las_polyvecn_reduce(pk->t);                /* [REUSED]  basesig.c:178: b_polyvecn_reduce(pk->t); */

  /* Extract t1 and write public key */
  las_polyvecn_caddq(pk->t);                 /* [REUSED]  basesig.c:185: b_polyvecn_caddq(pk->t); */

  /* [PAPER Alg.1] 4:      return (pk, sk) = (t, r) */
  return 0;                                  /* [REUSED]  basesig.c:200: return 0; */
  /* [PAPER Alg.1] 5:  end procedure */
}

/*************************************************
* Name:        las_signature_internal  <->  base_sign_signature_internal (basesig.c:212)
*
* Description: Algorithm 1 Sign body, parameterised by the caller-supplied
*              64-byte mask seed.  Same NTT hoisting as basesig.c: NTT(r)
*              once per call (basesig.c:272), NTT(c) once per attempt
*              (basesig.c:365).
*
* Returns 0 (success)
**************************************************/
int las_signature_internal(las_sig *sig,       /* paper σ: output signature σ = (c, z)  */
                           const uint8_t *m,   /* paper M: message                      */
                           size_t mlen,        /* length of M (no paper symbol)         */
                           const las_pk *pk,   /* paper t: pk->t = t (public key)       */
                           const las_sk *sk,   /* paper r: sk->s = r (secret key)       */
                           const las_pp *pp,   /* paper A: pp = A = [I | A']            */
                           const uint8_t seed[64]) {  /* PRG mask seed (<-> seed, basesig.c:218) */
  /* [PAPER Alg.1] 6:  procedure Sign((pk, sk), M): */
  unsigned int j;                /* [REUSED]  basesig.c:220: unsigned int j;           */
  uint8_t tbuf[LAS_N*N*4];       /* [REUSED]  basesig.c:221: uint8_t tbuf[LAS_N*N*4]; (packed pk) */
  uint8_t wbuf[LAS_N*N*4];       /* [REUSED]  basesig.c:222: packed w                  */
  uint8_t cseed[LAS_SEEDBYTES];  /* [REUSED]  basesig.c:223: challenge seed            */
  uint16_t nonce = 0;            /* [REUSED]  basesig.c:224: uint16_t nonce = 0;       */
  poly y[LAS_M];                 /* [REUSED]  basesig.c:225: paper y: mask, y <-$ Sγ^(n+ℓ) */
  poly yhat[LAS_ELL];            /* [REUSED]  basesig.c:226: poly yhat[LAS_ELL]; (A' half of y) */
  poly w[LAS_N];                 /* [REUSED]  basesig.c:227: paper w: commitment, w = A y */
  poly rhat[LAS_M];              /* [REUSED]  basesig.c:228: paper r in NTT domain     */
  poly c;                        /* [REUSED]  basesig.c:229: paper c: challenge        */
  poly chat;                     /* [REUSED]  basesig.c:230: NTT copy of c             */
  keccak_state state;            /* [REUSED]  basesig.c:231: keccak_state state;       */

  /* Compute mu = CRH(tr, pre, msg) */
  las_polyvecn_pack_w(tbuf, pk->t);          /* [REUSED]  basesig.c:239: b_polyvecn_pack_w(tbuf, pk->t);
                                              * (once per call: c = H(pk, w, M) binds
                                              * the raw public key)                    */

  /* Expand matrix and transform vectors */
  for(j = 0; j < LAS_M; ++j)
    rhat[j] = sk->s[j];                      /* [REUSED]  basesig.c:270-271: rhat[j] = sk->s[j]; */
  las_polyvecm_ntt(rhat);                    /* [REUSED]  basesig.c:272: b_polyvecm_ntt(rhat);
                                              * (hoisted once per call -- the secret is
                                              * invariant across rejection attempts)   */

rej:                                         /* [REUSED]  basesig.c:280: rej: */
  ++las_attempts;
  /* ^[CHANGED] basesig.c:281:
   *     ++base_attempts;
   * WHY: separate per-scheme attempt counter (instrumentation only), so the
   * base and LAS benchmark restart rates are measured independently. */

  /* Sample intermediate vector y */
  /* [PAPER Alg.1] 7:      y <-$ Sγ^(n+ℓ) */
  las_polyvecm_uniform_Sgamma(y, seed, nonce++);
                                             /* [REUSED]  basesig.c:287: b_polyvecm_uniform_Sgamma(y, seed, nonce++); */

  /* Matrix-vector multiplication */
  /* [PAPER Alg.1] 8:      w = A y */
  for(j = 0; j < LAS_ELL; ++j)
    yhat[j] = y[LAS_N + j];                  /* [REUSED]  basesig.c:299-300: yhat[j] = y[LAS_N + j]; */
  las_polyvecl_ntt(yhat);                    /* [REUSED]  basesig.c:308: b_polyvecl_ntt(yhat); */
  las_polyvec_matrix_pointwise_montgomery(w, pp->mat, yhat);
                                             /* [REUSED]  basesig.c:309: b_polyvec_matrix_pointwise_montgomery(w, pp->mat, yhat); */
  las_polyvecn_reduce(w);                    /* [REUSED]  basesig.c:311: b_polyvecn_reduce(w); */
  las_polyvecn_invntt_tomont(w);             /* [REUSED]  basesig.c:312: b_polyvecn_invntt_tomont(w); */
  las_polyvecn_add(w, w, y);                 /* [REUSED]  basesig.c:313: b_polyvecn_add(w, w, y);
                                              * (identity block of A = [I | A'])       */
  las_polyvecn_reduce(w);                    /* [REUSED]  basesig.c:317: b_polyvecn_reduce(w); */

  /* Decompose w and call the random oracle */
  las_polyvecn_caddq(w);                     /* [REUSED]  basesig.c:323: b_polyvecn_caddq(w); */
  las_polyvecn_pack_w(wbuf, w);              /* [REUSED]  basesig.c:329: b_polyvecn_pack_w(wbuf, w); */

  shake256_init(&state);                     /* [REUSED]  basesig.c:336: shake256_init(&state); */
  shake256_absorb(&state, tbuf, sizeof tbuf);
                                             /* [REUSED]  basesig.c:337: shake256_absorb(&state, tbuf, sizeof tbuf); */
  shake256_absorb(&state, wbuf, sizeof wbuf);
                                             /* [REUSED]  basesig.c:343: shake256_absorb(&state, wbuf, sizeof wbuf); */
  shake256_absorb(&state, m, mlen);          /* [REUSED]  basesig.c:346: shake256_absorb(&state, m, mlen); */
  shake256_finalize(&state);                 /* [REUSED]  basesig.c:350: shake256_finalize(&state); */
  shake256_squeeze(cseed, LAS_SEEDBYTES, &state);
                                             /* [REUSED]  basesig.c:351: shake256_squeeze(cseed, LAS_SEEDBYTES, &state); */
  /* [PAPER Alg.1] 9:      c = H(pk, w, M) */
  las_poly_challenge(&c, cseed);             /* [REUSED]  basesig.c:358: b_poly_challenge(&c, cseed); */
  chat = c;                                  /* [REUSED]  basesig.c:364: chat = c; */
  poly_ntt(&chat);                           /* [REUSED]  basesig.c:365: poly_ntt(&chat);
                                              * (once per attempt, hoisted out of the
                                              * m products)                            */

  /* Compute z, reject if it reveals secret */
  /* [PAPER Alg.1] 10:     z = y + c r, where r := sk */
  las_polyvecm_pointwise_poly_montgomery(sig->z, &chat, rhat);
                                             /* [REUSED]  basesig.c:374: b_polyvecm_pointwise_poly_montgomery(sig->z, &chat, rhat); */
  las_polyvecm_invntt_tomont(sig->z);        /* [REUSED]  basesig.c:377: b_polyvecm_invntt_tomont(sig->z); */
  las_polyvecm_add(sig->z, sig->z, y);       /* [REUSED]  basesig.c:378: b_polyvecm_add(sig->z, sig->z, y); */
  las_polyvecm_reduce(sig->z);               /* [REUSED]  basesig.c:379: b_polyvecm_reduce(sig->z); */
  /* [PAPER Alg.1] 11:     if ||z||∞ > γ − κ, then Restart */
  if(las_polyvecm_chknorm(sig->z, LAS_BOUND_SIGN))
    goto rej;                                /* [REUSED]  basesig.c:381-382: if(b_polyvecm_chknorm(sig->z, LAS_BOUND_SIGN))
                                              *               goto rej;               */

  /* Write signature */
  /* [PAPER Alg.1] 12:     return σ = (c, z) */
  sig->c = c;                                /* [REUSED]  basesig.c:409: sig->c = c; */
  return 0;                                  /* [REUSED]  basesig.c:414: return 0; */
  /* [PAPER Alg.1] 13: end procedure */
}

/*************************************************
* Name:        las_signature  <->  base_sign_signature (basesig.c:426)
*
* Description: Algorithm 1 Sign, random path: fresh mask seed, then the
*              internal.
*
* Returns 0 (success)
**************************************************/
int las_signature(las_sig *sig,        /* paper σ: output signature σ = (c, z) */
                  const uint8_t *m,    /* paper M: message                     */
                  size_t mlen,         /* length of M (no paper symbol)        */
                  const las_pk *pk,    /* paper t: pk->t = t (public key)      */
                  const las_sk *sk,    /* paper r: sk->s = r (secret key)      */
                  const las_pp *pp) {  /* paper A: pp = A = [I | A']           */
  uint8_t seed[64];      /* [REUSED]  basesig.c:432: uint8_t seed[64]; */

  randombytes(seed, 64);                     /* [REUSED]  basesig.c:440: randombytes(seed, 64); */
  return las_signature_internal(sig, m, mlen, pk, sk, pp, seed);
                                             /* [REUSED]  basesig.c:451: return base_sign_signature_internal(sig, m, mlen, pk, sk, pp, seed); */
}

/*************************************************
* Name:        las_signature_det  (deterministic KAT slot; no basesig.c/sign.c
*              analogue -- the counterpart of sign.c's zeroed-rnd branch, see
*              the [CHANGED] note at basesig.c:441-450)
*
* Description: Deterministic Sign: mask seed derived from (sk, M) via
*              det_seed, then the same internal.  Same distribution and
*              validity as las_signature; removes the per-signature RNG (no
*              nonce-reuse risk) and enables reproducible known-answer tests.
*
* Returns 0 (success)
**************************************************/
int las_signature_det(las_sig *sig,        /* paper σ: output signature σ = (c, z) */
                      const uint8_t *m,    /* paper M: message                     */
                      size_t mlen,         /* length of M (no paper symbol)        */
                      const las_pk *pk,    /* paper t: pk->t = t (public key)      */
                      const las_sk *sk,    /* paper r: sk->s = r (secret key)      */
                      const las_pp *pp) {  /* paper A: pp = A = [I | A']           */
  uint8_t seed[64];  /* PRG mask seed, derived from (sk, M) (no paper symbol) */

  det_seed(seed, 0, sk, NULL, m, mlen);
  /* ^[CHANGED] las.c:359: randombytes(seed, 64);
   * WHY: deterministic path -- the mask seed is derived from the signing
   * inputs (domain tag 0 = sign, no statement to bind) instead of drawn
   * fresh, so the signature is a reproducible function of (sk, M). */
  return las_signature_internal(sig, m, mlen, pk, sk, pp, seed);
                                             /* [REUSED]  las.c:389: return las_signature_internal(sig, m, mlen, pk, sk, pp, seed); */
}

/*************************************************
* Name:        las_sign  <->  base_sign (basesig.c:464)
*
* Description: Compute signed message.  Kept for one-to-one mirroring
*              (chain crypto_sign -> base_sign -> las_sign): the signature
*              is a struct (no packed byte form), so the upstream prepend
*              "sm = sig || m" degenerates to sm = m, exactly as in
*              basesig.c.
*
* Returns 0 (success)
**************************************************/
int las_sign(las_sig *sig,          /* paper σ: output signature σ = (c, z)     */
             uint8_t *sm,           /* output "signed message" (= copy of M)     */
             size_t *smlen,         /* output length of sm (= mlen)              */
             const uint8_t *m,      /* paper M: message                          */
             size_t mlen,           /* length of M (no paper symbol)             */
             const las_pk *pk,      /* paper t: pk->t = t (public key)           */
             const las_sk *sk,      /* paper r: sk->s = r (secret key)           */
             const las_pp *pp) {    /* paper A: pp = A = [I | A']                */
  int ret;                                   /* [REUSED]  basesig.c:472: int ret;    */
  size_t i;                                  /* [REUSED]  basesig.c:473: size_t i;   */

  for(i = 0; i < mlen; ++i)
    sm[mlen - 1 - i] = m[mlen - 1 - i];      /* [REUSED]  basesig.c:475-476: sm[mlen - 1 - i] = m[mlen - 1 - i];
                                              * (backwards copy so sm == m aliasing
                                              * still works, as upstream)             */
  ret = las_signature(sig, sm, mlen, pk, sk, pp);
                                             /* [REUSED]  basesig.c:483: ret = base_sign_signature(sig, sm, mlen, pk, sk, pp); */
  *smlen = mlen;                             /* [REUSED]  basesig.c:485: *smlen = mlen; */
  return ret;                                /* [REUSED]  basesig.c:489: return ret;  */
}

/*************************************************
* Name:        las_verify_internal  <->  base_sign_verify_internal (basesig.c:499)
*
* Description: Algorithm 1 Verify: w' = A z - c t; accept iff c == H(pk,w',M).
*
* Returns 0 if signature could be verified correctly and -1 otherwise
**************************************************/
int las_verify_internal(const las_sig *sig,  /* paper σ: sig = (c, z), signature to verify */
                        const uint8_t *m,    /* paper M: message                          */
                        size_t mlen,         /* length of M (no paper symbol)             */
                        const las_pk *pk,    /* paper t: pk->t = t (public key)           */
                        const las_pp *pp) {  /* paper A: pp = A = [I | A']                */
  /* [PAPER Alg.1] 14: procedure Verify(pk, σ, M): */
  unsigned int i, j;             /* [REUSED]  basesig.c:505: unsigned int i, j;        */
  uint8_t tbuf[LAS_N*N*4];       /* [REUSED]  basesig.c:506: packed pk                 */
  uint8_t wbuf[LAS_N*N*4];       /* [REUSED]  basesig.c:507: packed w'                 */
  uint8_t cseed[LAS_SEEDBYTES];  /* [REUSED]  basesig.c:508: challenge seed            */
  poly c2;                       /* [REUSED]  basesig.c:509: recomputed challenge POLY */
  poly chat;                     /* [REUSED]  basesig.c:510: NTT copy of sig->c        */
  poly zhat[LAS_ELL];            /* [REUSED]  basesig.c:511: A' half of z              */
  poly w[LAS_N];                 /* [REUSED]  basesig.c:512: poly w[LAS_N]; (paper w′)  */
  poly that[LAS_N];              /* [REUSED]  basesig.c:513: t in the NTT domain       */
  keccak_state state;            /* [REUSED]  basesig.c:514: keccak_state state;       */

  /* [PAPER Alg.1] 15:     Parse (c, z) := σ */
  /* [PAPER Alg.1] 16:     if ||z||∞ > γ − κ, then return 0 */
  if(las_polyvecm_chknorm(sig->z, LAS_BOUND_SIGN))
    return -1;                               /* [REUSED]  basesig.c:526-527: if(b_polyvecm_chknorm(sig->z, LAS_BOUND_SIGN))
                                              *               return -1;              */

  /* Compute CRH(H(rho, t1), pre, msg) */
  las_polyvecn_pack_w(tbuf, pk->t);          /* [REUSED]  basesig.c:534: b_polyvecn_pack_w(tbuf, pk->t); */

  /* Matrix-vector multiplication; compute Az - c2^dt1 */
  /* [PAPER Alg.1] 17:     w′ = A z − c t, where t := pk */
  chat = sig->c;                             /* [REUSED]  basesig.c:544: chat = sig->c;
                                              * (the signature carries the challenge
                                              * POLYNOMIAL; copied for the in-place NTT) */

  for(j = 0; j < LAS_ELL; ++j)
    zhat[j] = sig->z[LAS_N + j];             /* [REUSED]  basesig.c:554-555: zhat[j] = sig->z[LAS_N + j]; */
  las_polyvecl_ntt(zhat);                    /* [REUSED]  basesig.c:559: b_polyvecl_ntt(zhat); */
  las_polyvec_matrix_pointwise_montgomery(w, pp->mat, zhat);
                                             /* [REUSED]  basesig.c:560: b_polyvec_matrix_pointwise_montgomery(w, pp->mat, zhat); */

  poly_ntt(&chat);                           /* [REUSED]  basesig.c:563: poly_ntt(&chat); */
  for(j = 0; j < LAS_N; ++j)
    that[j] = pk->t[j];                      /* [REUSED]  basesig.c:568-569: that[j] = pk->t[j]; */
  las_polyvecn_ntt(that);                    /* [REUSED]  basesig.c:570: b_polyvecn_ntt(that); */
  las_polyvecn_pointwise_poly_montgomery(that, &chat, that);
                                             /* [REUSED]  basesig.c:571: b_polyvecn_pointwise_poly_montgomery(that, &chat, that); */

  las_polyvecn_reduce(w);                    /* [REUSED]  basesig.c:583: b_polyvecn_reduce(w); */
  las_polyvecn_invntt_tomont(w);             /* [REUSED]  basesig.c:584: b_polyvecn_invntt_tomont(w); */
  las_polyvecn_add(w, w, sig->z);            /* [REUSED]  basesig.c:585: b_polyvecn_add(w, w, sig->z);
                                              * (identity block: w += z_top)           */
  las_polyvecn_invntt_tomont(that);          /* [REUSED]  basesig.c:586: b_polyvecn_invntt_tomont(that); */
  las_polyvecn_sub(w, w, that);              /* [REUSED]  basesig.c:587: b_polyvecn_sub(w, w, that); */
  las_polyvecn_reduce(w);                    /* [REUSED]  basesig.c:589: b_polyvecn_reduce(w); */

  /* Reconstruct w1 */
  las_polyvecn_caddq(w);                     /* [REUSED]  basesig.c:592: b_polyvecn_caddq(w); */
  las_polyvecn_pack_w(wbuf, w);              /* [REUSED]  basesig.c:597: b_polyvecn_pack_w(wbuf, w); */

  /* Call random oracle and verify challenge */
  shake256_init(&state);                     /* [REUSED]  basesig.c:603: shake256_init(&state); */
  shake256_absorb(&state, tbuf, sizeof tbuf);
                                             /* [REUSED]  basesig.c:604: shake256_absorb(&state, tbuf, sizeof tbuf); */
  shake256_absorb(&state, wbuf, sizeof wbuf);
                                             /* [REUSED]  basesig.c:608: shake256_absorb(&state, wbuf, sizeof wbuf); */
  shake256_absorb(&state, m, mlen);          /* [REUSED]  basesig.c:610: shake256_absorb(&state, m, mlen); */
  shake256_finalize(&state);                 /* [REUSED]  basesig.c:613: shake256_finalize(&state); */
  shake256_squeeze(cseed, LAS_SEEDBYTES, &state);
                                             /* [REUSED]  basesig.c:614: shake256_squeeze(cseed, LAS_SEEDBYTES, &state); */
  /* [PAPER Alg.1] 18:     if c ≠ H(pk, w′, M), then return 0 */
  las_poly_challenge(&c2, cseed);            /* [REUSED]  basesig.c:618: b_poly_challenge(&c2, cseed); */
  for(i = 0; i < N; ++i)
    if(c2.coeffs[i] != sig->c.coeffs[i])
      return -1;                             /* [REUSED]  basesig.c:623-625: if(c2.coeffs[i] != sig->c.coeffs[i])
                                              * (compare the challenge POLYNOMIALS
                                              * coefficient-wise)                      */

  /* [PAPER Alg.1] 19:     return 1 */
  return 0;                                  /* [REUSED]  basesig.c:634: return 0; */
  /* [PAPER Alg.1] 20: end procedure */
}

/*************************************************
* Name:        las_verify  <->  base_sign_verify (basesig.c:645)
*
* Description: Algorithm 1 Verify, public entry point.
*
* Returns 0 if signature could be verified correctly and -1 otherwise
**************************************************/
int las_verify(const las_sig *sig,  /* paper σ: sig = (c, z), signature to verify */
               const uint8_t *m,    /* paper M: message                          */
               size_t mlen,         /* length of M (no paper symbol)             */
               const las_pk *pk,    /* paper t: pk->t = t (public key)           */
               const las_pp *pp) {  /* paper A: pp = A = [I | A']                */
  return las_verify_internal(sig, m, mlen, pk, pp);
                                             /* [REUSED]  basesig.c:655: return base_sign_verify_internal(sig, m, mlen, pk, pp); */
}

/*************************************************
* Name:        las_open  <->  base_sign_open (basesig.c:667)
*
* Description: Verify signed message.  Kept for one-to-one mirroring (chain
*              crypto_sign_open -> base_sign_open -> las_open): the
*              signature travels as a struct beside sm, so sm holds only M,
*              exactly as in basesig.c.
*
* Returns 0 if signed message could be verified correctly and -1 otherwise
**************************************************/
int las_open(uint8_t *m,            /* output message (= copy of sm on success)   */
             size_t *mlen,          /* output length of m                         */
             const las_sig *sig,    /* paper σ: sig = (c, z), signature to verify */
             const uint8_t *sm,     /* input "signed message" (= M)               */
             size_t smlen,          /* length of sm                               */
             const las_pk *pk,      /* paper t: pk->t = t (public key)            */
             const las_pp *pp) {    /* paper A: pp = A = [I | A']                 */
  size_t i;                                  /* [REUSED]  basesig.c:674: size_t i;  */

  *mlen = smlen;                             /* [REUSED]  basesig.c:679: *mlen = smlen; */
  if(las_verify(sig, sm, *mlen, pk, pp))
                                             /* [REUSED]  basesig.c:683: if(base_sign_verify(sig, sm, *mlen, pk, pp)) */
    goto badsig;                             /* [REUSED]  basesig.c:685: goto badsig; */
  else {
    /* All good, copy msg, return 0 */
    for(i = 0; i < *mlen; ++i)               /* [REUSED]  basesig.c:688-690: m[i] = sm[i]; */
      m[i] = sm[i];
    return 0;                                /* [REUSED]  basesig.c:691: return 0;   */
  }

badsig:
  /* Signature verification failed */
  *mlen = 0;                                 /* [REUSED]  basesig.c:694-700, verbatim */
  for(i = 0; i < smlen; ++i)
    m[i] = 0;

  return -1;
}

/* =============== scheme, Algorithm 2 (adaptor layer) ===============
 * No basesig.c/sign.c analogue from here on: these are the adaptor
 * operations LAS adds on top of the base signature (upstream = the PAPER).
 * Annotations therefore quote each function's Algorithm 1 twin in THIS file
 * (las.c:<line>: <code>): PreSign diffs against Sign, PreVerify against
 * Verify, exactly as basesig.c diffs against sign.c. */

/*************************************************
* Name:        las_presign_internal  (adaptor twin of las_signature_internal)
*
* Description: Algorithm 2 PreSign body: las_signature_internal, except the
*              statement is folded into the hash -- c = H(pk, w + Y, M) --
*              and the rejection bound tightens to `bound` (γ−κ−1 single-hop,
*              γ−κ−K AMHL).  Parameterised by the mask seed; same NTT
*              hoisting as the twin.
*
* Returns 0 (success)
**************************************************/
int las_presign_internal(las_sig *presig,   /* paper σ̂: output pre-signature σ̂ = (c, ẑ)    */
                         const uint8_t *m,  /* paper M: message                            */
                         size_t mlen,       /* length of M (no paper symbol)               */
                         const las_pk *Y,   /* paper t′ := Y: statement, Y->t = Y = A y_wit */
                         const las_pk *pk,  /* paper t: pk->t = t (public key)             */
                         const las_sk *sk,  /* paper r: sk->s = r (secret key)             */
                         const las_pp *pp,  /* paper A: pp = A = [I | A']                  */
                         int32_t bound,     /* paper γ−κ−1 (single-hop) / γ−κ−K (AMHL)     */
                         const uint8_t seed[64]) {  /* PRG mask seed (<-> seed) */
  /* [PAPER Alg.2] 1:  procedure PreSign((pk, sk), Y, M): */
  unsigned int j;                /* decls <-> las_signature_internal's, plus wY     */
  uint8_t tbuf[LAS_N*N*4];       /* packed pk (fixed hash prefix)                   */
  uint8_t wbuf[LAS_N*N*4];       /* packed w + Y (the hashed commitment; see below) */
  uint8_t cseed[LAS_SEEDBYTES];  /* challenge seed                                  */
  uint16_t nonce = 0;            /* PRG counter                                     */
  poly y[LAS_M];                 /* paper y: mask, y <-$ Sγ^(n+ℓ)                   */
  poly yhat[LAS_ELL];            /* NTT buffer, A' half of y                        */
  poly w[LAS_N];                 /* paper w: commitment, w = A y                    */
  poly wY[LAS_N];                /* paper w + t′: the hashed commitment w + Y       */
  poly rhat[LAS_M];              /* paper r in NTT domain                           */
  poly c;                        /* paper c: challenge c = H(pk, w + t′, M)         */
  poly chat;                     /* NTT copy of c                                   */
  keccak_state state;
  /* ^[CHANGED] (one extra declaration vs the twin)
   *     poly w[LAS_N];
   * WHY: wY holds the SHIFTED commitment w + Y of paper Alg. 2 step 4, so w
   * itself stays untouched. */

  /* Compute mu = CRH(tr, pre, msg) */
  las_polyvecn_pack_w(tbuf, pk->t);          /* [REUSED]  las.c:268: las_polyvecn_pack_w(tbuf, pk->t); */

  /* Expand matrix and transform vectors */
  for(j = 0; j < LAS_M; ++j)
    rhat[j] = sk->s[j];                      /* [REUSED]  las.c:274: rhat[j] = sk->s[j]; */
  las_polyvecm_ntt(rhat);                    /* [REUSED]  las.c:275: las_polyvecm_ntt(rhat); */

rej:                                         /* [REUSED]  las.c:279: rej: */
  ++las_attempts;                            /* [REUSED]  las.c:280: ++las_attempts; */

  /* Sample intermediate vector y */
  /* [PAPER Alg.2] 2:      y ←$ Sγ^(n+ℓ) */
  las_polyvecm_uniform_Sgamma(y, seed, nonce++);
                                             /* [REUSED]  las.c:288: las_polyvecm_uniform_Sgamma(y, seed, nonce++); */

  /* Matrix-vector multiplication */
  /* [PAPER Alg.2] 3:      w = A y */
  for(j = 0; j < LAS_ELL; ++j)
    yhat[j] = y[LAS_N + j];                  /* [REUSED]  las.c:294: yhat[j] = y[LAS_N + j]; */
  las_polyvecl_ntt(yhat);                    /* [REUSED]  las.c:295: las_polyvecl_ntt(yhat); */
  las_polyvec_matrix_pointwise_montgomery(w, pp->mat, yhat);
                                             /* [REUSED]  las.c:296: las_polyvec_matrix_pointwise_montgomery(w, pp->mat, yhat); */
  las_polyvecn_reduce(w);                    /* [REUSED]  las.c:298: las_polyvecn_reduce(w); */
  las_polyvecn_invntt_tomont(w);             /* [REUSED]  las.c:299: las_polyvecn_invntt_tomont(w); */
  las_polyvecn_add(w, w, y);                 /* [REUSED]  las.c:300: las_polyvecn_add(w, w, y); */
  las_polyvecn_reduce(w);                    /* [REUSED]  las.c:298: las_polyvecn_reduce(w); */

  /* Decompose w and call the random oracle */
  las_polyvecn_caddq(w);                     /* [REUSED]  las.c:305: las_polyvecn_caddq(w); */
  /* [PAPER Alg.2] 4:      c = H(pk, w + t′, M), where t′ := Y */
  las_polyvecn_add(wY, w, Y->t);
  las_polyvecn_reduce(wY);
  las_polyvecn_caddq(wY);
  /* ^[CHANGED] (no las_signature_internal lines)
   * WHY: THE core adaptor mechanism -- the statement is folded into the
   * commitment before hashing.  Sign hashes w; PreSign hashes w + Y, so the
   * adapted signature (z = ẑ + y_wit gives Az − ct = w + Y) later satisfies
   * the ORDINARY Verify equation for the same c. */
  las_polyvecn_pack_w(wbuf, wY);
  /* ^[CHANGED] las.c:306: las_polyvecn_pack_w(wbuf, w);
   * WHY: the oracle input is the shifted commitment w + Y, not w. */

  shake256_init(&state);                     /* [REUSED]  las.c:308: shake256_init(&state); */
  shake256_absorb(&state, tbuf, sizeof tbuf);
                                             /* [REUSED]  las.c:309: shake256_absorb(&state, tbuf, sizeof tbuf); */
  shake256_absorb(&state, wbuf, sizeof wbuf);
                                             /* [REUSED]  las.c:311: shake256_absorb(&state, wbuf, sizeof wbuf); */
  shake256_absorb(&state, m, mlen);          /* [REUSED]  las.c:313: shake256_absorb(&state, m, mlen); */
  shake256_finalize(&state);                 /* [REUSED]  las.c:314: shake256_finalize(&state); */
  shake256_squeeze(cseed, LAS_SEEDBYTES, &state);
                                             /* [REUSED]  las.c:315: shake256_squeeze(cseed, LAS_SEEDBYTES, &state); */
  las_poly_challenge(&c, cseed);             /* [REUSED]  las.c:318: las_poly_challenge(&c, cseed); */
  chat = c;                                  /* [REUSED]  las.c:319: chat = c; */
  poly_ntt(&chat);                           /* [REUSED]  las.c:320: poly_ntt(&chat); */

  /* Compute z, reject if it reveals secret */
  /* [PAPER Alg.2] 5:      ẑ = y + c r, where r := sk */
  las_polyvecm_pointwise_poly_montgomery(presig->z, &chat, rhat);
                                             /* [REUSED]  las.c:326: las_polyvecm_pointwise_poly_montgomery(sig->z, &chat, rhat);
                                              * (sig -> presig)                        */
  las_polyvecm_invntt_tomont(presig->z);     /* [REUSED]  las.c:328: las_polyvecm_invntt_tomont(sig->z); */
  las_polyvecm_add(presig->z, presig->z, y); /* [REUSED]  las.c:329: las_polyvecm_add(sig->z, sig->z, y); */
  las_polyvecm_reduce(presig->z);            /* [REUSED]  las.c:330: las_polyvecm_reduce(sig->z); */
  /* [PAPER Alg.2] 6:      if ||ẑ||∞ > γ − κ − 1, then Restart */
  if(las_polyvecm_chknorm(presig->z, bound))
    goto rej;
  /* ^[CHANGED] las.c:332: if(las_polyvecm_chknorm(sig->z, LAS_BOUND_SIGN))
   * WHY: PreSign rejects at the TIGHTER caller-supplied bound (γ−κ−1
   * single-hop, γ−κ−K AMHL): the ternary witness has ||y_wit||∞ ≤ 1 (≤ K
   * cumulative), so the adapted z = ẑ + y_wit still clears Verify's γ−κ.
   * THE failure mode to watch: loosening this to γ−κ makes adapted
   * signatures overflow the Verify bound and Verify rejects everything. */

  /* Write signature */
  /* [PAPER Alg.2] 7:      return σ̂ = (c, ẑ) */
  presig->c = c;                             /* [REUSED]  las.c:338: sig->c = c;  (sig -> presig) */
  return 0;                                  /* [REUSED]  las.c:339: return 0; */
  /* [PAPER Alg.2] 8:  end procedure */
}

/*************************************************
* Name:        las_presign  (adaptor twin of las_signature)
*
* Description: Algorithm 2 PreSign(sk, Y, M), random path: fresh mask seed,
*              then the internal at the single-hop bound γ−κ−1.
*
* Returns 0 (success)
**************************************************/
int las_presign(las_sig *presig,     /* paper σ̂: output pre-signature σ̂ = (c, ẑ) */
                const uint8_t *m,    /* paper M: message                         */
                size_t mlen,         /* length of M (no paper symbol)            */
                const las_pk *Y,     /* paper t′ := Y: statement                 */
                const las_pk *pk,    /* paper t: pk->t = t (public key)          */
                const las_sk *sk,    /* paper r: sk->s = r (secret key)          */
                const las_pp *pp) {  /* paper A: pp = A = [I | A']               */
  uint8_t seed[64];      /* [REUSED]  las.c:382: uint8_t seed[64]; */

  randombytes(seed, 64);                     /* [REUSED]  las.c:385: randombytes(seed, 64); */
  return las_presign_internal(presig, m, mlen, Y, pk, sk, pp, LAS_BOUND_PRESIGN, seed);
  /* ^[CHANGED] las.c:389: return las_signature_internal(sig, m, mlen, pk, sk, pp, seed);
   * WHY: the adaptor internal additionally takes the statement Y and the
   * single-hop PreSign bound γ−κ−1 (LAS_BOUND_PRESIGN). */
}

/*************************************************
* Name:        las_presign_det  (adaptor twin of las_signature_det; KAT path)
*
* Description: Deterministic PreSign: mask seed derived from (sk, Y, M) via
*              det_seed (tag 1 binds the statement Y), single-hop bound.
*
* Returns 0 (success)
**************************************************/
int las_presign_det(las_sig *presig,     /* paper σ̂: output pre-signature σ̂ = (c, ẑ) */
                    const uint8_t *m,    /* paper M: message                         */
                    size_t mlen,         /* length of M (no paper symbol)            */
                    const las_pk *Y,     /* paper t′ := Y: statement                 */
                    const las_pk *pk,    /* paper t: pk->t = t (public key)          */
                    const las_sk *sk,    /* paper r: sk->s = r (secret key)          */
                    const las_pp *pp) {  /* paper A: pp = A = [I | A']               */
  uint8_t seed[64];  /* PRG mask seed, derived from (sk, Y, M) (no paper symbol) */

  det_seed(seed, 1, sk, Y, m, mlen);
  /* ^[CHANGED] las.c:384: det_seed(seed, 0, sk, NULL, m, mlen);
   * WHY: domain tag 1 = presign, and the statement Y is bound into the seed
   * derivation, so pre-signatures for different statements never share mask
   * randomness. */
  return las_presign_internal(presig, m, mlen, Y, pk, sk, pp, LAS_BOUND_PRESIGN, seed);
  /* ^[CHANGED] las.c:389: return las_signature_internal(sig, m, mlen, pk, sk, pp, seed);
   * WHY: same Y + single-hop-bound difference as las_presign vs
   * las_signature. */
}

/*************************************************
* Name:        las_preverify_internal  (adaptor twin of las_verify_internal)
*
* Description: Algorithm 2 PreVerify body: las_verify_internal, except the
*              recomputed commitment is shifted by the statement before the
*              hash -- accept iff c == H(pk, w' + Y, M) -- and the norm gate
*              runs at `bound` (γ−κ−1 single-hop, γ−κ−K AMHL).
*
* Returns 0 if pre-signature could be verified correctly and -1 otherwise
**************************************************/
int las_preverify_internal(const las_sig *presig,  /* paper σ̂: presig = (c, ẑ), pre-sig to verify */
                           const uint8_t *m,       /* paper M: message                            */
                           size_t mlen,            /* length of M (no paper symbol)               */
                           const las_pk *Y,        /* paper t′ := Y: statement, Y->t = Y          */
                           const las_pk *pk,       /* paper t: pk->t = t (public key)             */
                           const las_pp *pp,       /* paper A: pp = A = [I | A']                  */
                           int32_t bound) {        /* paper γ−κ−1 (single-hop) / γ−κ−K (AMHL)     */
  /* [PAPER Alg.2] 9:  procedure PreVerify(Y, pk, σ̂, M): */
  unsigned int i, j;             /* decls <-> las_verify_internal's, plus wY        */
  uint8_t tbuf[LAS_N*N*4];       /* packed pk                                       */
  uint8_t wbuf[LAS_N*N*4];       /* packed w' + Y (the hashed commitment)           */
  uint8_t cseed[LAS_SEEDBYTES];  /* challenge seed                                  */
  poly c2;                       /* paper H(pk, w′ + t′, M): recomputed challenge   */
  poly chat;                     /* NTT copy of presig->c                           */
  poly zhat[LAS_ELL];            /* A' half of ẑ                                    */
  poly w[LAS_N];                 /* paper w′: recomputed commitment w′ = A ẑ − c t  */
  poly wY[LAS_N];                /* paper w′ + t′: the hashed commitment w′ + Y     */
  poly that[LAS_N];              /* t in the NTT domain                             */
  keccak_state state;
  /* ^[CHANGED] (one extra declaration vs the twin)
   *     poly w[LAS_N];
   * WHY: wY holds the shifted commitment w′ + Y of paper Alg. 2 step 15. */

  /* [PAPER Alg.2] 10:     Parse (c, ẑ) := σ̂ and t′ := Y */
  /* [PAPER Alg.2] 11:     if ||ẑ||∞ > γ − κ − 1 then */
  /* [PAPER Alg.2] 12:         return 0 */
  /* [PAPER Alg.2] 13:     end if */
  if(las_polyvecm_chknorm(presig->z, bound))
    return -1;
  /* ^[CHANGED] las.c:451: if(las_polyvecm_chknorm(sig->z, LAS_BOUND_SIGN))
   * WHY: the pre-signature norm gate runs at the caller-supplied PreSign
   * bound (γ−κ−1 / γ−κ−K), matching what las_presign_internal enforced. */

  /* Compute CRH(H(rho, t1), pre, msg) */
  las_polyvecn_pack_w(tbuf, pk->t);          /* [REUSED]  las.c:456: las_polyvecn_pack_w(tbuf, pk->t); */

  /* Matrix-vector multiplication; compute Az - c2^dt1 */
  /* [PAPER Alg.2] 14:     w′ = A ẑ − c t, where t := pk */
  chat = presig->c;                          /* [REUSED]  las.c:460: chat = sig->c;  (sig -> presig) */

  for(j = 0; j < LAS_ELL; ++j)
    zhat[j] = presig->z[LAS_N + j];          /* [REUSED]  las.c:465: zhat[j] = sig->z[LAS_N + j]; */
  las_polyvecl_ntt(zhat);                    /* [REUSED]  las.c:466: las_polyvecl_ntt(zhat); */
  las_polyvec_matrix_pointwise_montgomery(w, pp->mat, zhat);
                                             /* [REUSED]  las.c:467: las_polyvec_matrix_pointwise_montgomery(w, pp->mat, zhat); */

  poly_ntt(&chat);                           /* [REUSED]  las.c:470: poly_ntt(&chat); */
  for(j = 0; j < LAS_N; ++j)
    that[j] = pk->t[j];                      /* [REUSED]  las.c:472: that[j] = pk->t[j]; */
  las_polyvecn_ntt(that);                    /* [REUSED]  las.c:473: las_polyvecn_ntt(that); */
  las_polyvecn_pointwise_poly_montgomery(that, &chat, that);
                                             /* [REUSED]  las.c:474: las_polyvecn_pointwise_poly_montgomery(that, &chat, that); */

  las_polyvecn_reduce(w);                    /* [REUSED]  las.c:477: las_polyvecn_reduce(w); */
  las_polyvecn_invntt_tomont(w);             /* [REUSED]  las.c:478: las_polyvecn_invntt_tomont(w); */
  las_polyvecn_add(w, w, presig->z);         /* [REUSED]  las.c:479: las_polyvecn_add(w, w, sig->z);
                                              * (identity block: w += ẑ_top)           */
  las_polyvecn_invntt_tomont(that);          /* [REUSED]  las.c:481: las_polyvecn_invntt_tomont(that); */
  las_polyvecn_sub(w, w, that);              /* [REUSED]  las.c:482: las_polyvecn_sub(w, w, that); */
  las_polyvecn_reduce(w);                    /* [REUSED]  las.c:477: las_polyvecn_reduce(w); */

  /* Reconstruct w1 */
  las_polyvecn_caddq(w);                     /* [REUSED]  las.c:486: las_polyvecn_caddq(w); */
  las_polyvecn_add(wY, w, Y->t);
  las_polyvecn_reduce(wY);
  las_polyvecn_caddq(wY);
  /* ^[CHANGED] (no las_verify_internal lines)
   * WHY: the statement is folded back in before the hash -- PreVerify
   * checks c against H(pk, w′ + Y, M), mirroring what las_presign_internal
   * hashed (paper Alg. 2 step 15). */
  las_polyvecn_pack_w(wbuf, wY);
  /* ^[CHANGED] las.c:487: las_polyvecn_pack_w(wbuf, w);
   * WHY: the oracle input is the shifted commitment w′ + Y, not w′. */

  /* Call random oracle and verify challenge */
  shake256_init(&state);                     /* [REUSED]  las.c:490: shake256_init(&state); */
  shake256_absorb(&state, tbuf, sizeof tbuf);
                                             /* [REUSED]  las.c:491: shake256_absorb(&state, tbuf, sizeof tbuf); */
  shake256_absorb(&state, wbuf, sizeof wbuf);
                                             /* [REUSED]  las.c:493: shake256_absorb(&state, wbuf, sizeof wbuf); */
  shake256_absorb(&state, m, mlen);          /* [REUSED]  las.c:495: shake256_absorb(&state, m, mlen); */
  shake256_finalize(&state);                 /* [REUSED]  las.c:496: shake256_finalize(&state); */
  shake256_squeeze(cseed, LAS_SEEDBYTES, &state);
                                             /* [REUSED]  las.c:497: shake256_squeeze(cseed, LAS_SEEDBYTES, &state); */
  /* [PAPER Alg.2] 15:     if c ≠ H(pk, w′ + t′, M) then */
  /* [PAPER Alg.2] 16:         return 0 */
  /* [PAPER Alg.2] 17:     end if */
  las_poly_challenge(&c2, cseed);            /* [REUSED]  las.c:500: las_poly_challenge(&c2, cseed); */
  for(i = 0; i < N; ++i)
    if(c2.coeffs[i] != presig->c.coeffs[i])
      return -1;                             /* [REUSED]  las.c:502: if(c2.coeffs[i] != sig->c.coeffs[i]) */

  /* [PAPER Alg.2] 18:     return 1 */
  return 0;                                  /* [REUSED]  las.c:555: return 0; */
  /* [PAPER Alg.2] 19: end procedure */
}

/*************************************************
* Name:        las_preverify  (adaptor twin of las_verify)
*
* Description: Algorithm 2 PreVerify(Y, pk, σ̂, M), public entry point at the
*              single-hop bound γ−κ−1.
*
* Returns 0 if pre-signature could be verified correctly and -1 otherwise
**************************************************/
int las_preverify(const las_sig *presig,  /* paper σ̂: presig = (c, ẑ), pre-sig to verify */
                  const uint8_t *m,       /* paper M: message                            */
                  size_t mlen,            /* length of M (no paper symbol)               */
                  const las_pk *Y,        /* paper t′ := Y: statement, Y->t = Y          */
                  const las_pk *pk,       /* paper t: pk->t = t (public key)             */
                  const las_pp *pp) {     /* paper A: pp = A = [I | A']                  */
  return las_preverify_internal(presig, m, mlen, Y, pk, pp, LAS_BOUND_PRESIGN);
  /* ^[CHANGED] las.c:524: return las_verify_internal(sig, m, mlen, pk, pp);
   * WHY: the adaptor internal additionally takes the statement Y and the
   * single-hop PreSign bound γ−κ−1 (LAS_BOUND_PRESIGN). */
}

/*************************************************
* Name:        las_presign_k  (AMHL K-hop PreSign; adaptor twin of las_presign)
*
* Description: Identical to las_presign but rejects at the tighter bound
*              γ−κ−K (LAS_BOUND_PRESIGN_K), reserving norm budget K for the
*              cumulative witness (eprint 2020/845 Fig. 2 / Section 5).
*
* Returns 0 (success)
**************************************************/
int las_presign_k(las_sig *presig,    /* paper σ̂: output pre-signature σ̂ = (c, ẑ)   */
                  const uint8_t *m,   /* paper M: message                           */
                  size_t mlen,        /* length of M (no paper symbol)              */
                  const las_pk *Y,    /* paper t′ := Y: (cumulative) statement       */
                  const las_pk *pk,   /* paper t: pk->t = t (public key)            */
                  const las_sk *sk,   /* paper r: sk->s = r (secret key)            */
                  const las_pp *pp,   /* paper A: pp = A = [I | A']                 */
                  unsigned int nhops) {  /* paper K: number of AMHL hops (tighter bound γ−κ−K) */
  uint8_t seed[64];      /* [REUSED]  las.c:733: uint8_t seed[64]; */

  randombytes(seed, 64);                     /* [REUSED]  las.c:711: randombytes(seed, 64); */
  return las_presign_internal(presig, m, mlen, Y, pk, sk, pp, LAS_BOUND_PRESIGN_K(nhops), seed);
  /* ^[CHANGED] las.c:740: return las_presign_internal(presig, m, mlen, Y, pk, sk, pp, LAS_BOUND_PRESIGN, seed);
   * WHY: the AMHL K-hop bound γ−κ−K reserves norm budget K for the
   * cumulative witness (||s_j||∞ ≤ j ≤ K); at K = 1 it collapses to the
   * single-hop bound. */
}

/*************************************************
* Name:        las_preverify_k  (AMHL K-hop PreVerify; adaptor twin of
*              las_preverify)
*
* Description: Identical to las_preverify but at the tighter bound γ−κ−K
*              (same shared internal, different bound).
*
* Returns 0 if pre-signature could be verified correctly and -1 otherwise
**************************************************/
int las_preverify_k(const las_sig *presig,  /* paper σ̂: presig = (c, ẑ), pre-sig to verify */
                    const uint8_t *m,       /* paper M: message                            */
                    size_t mlen,            /* length of M (no paper symbol)               */
                    const las_pk *Y,        /* paper t′ := Y: (cumulative) statement       */
                    const las_pk *pk,       /* paper t: pk->t = t (public key)             */
                    const las_pp *pp,       /* paper A: pp = A = [I | A']                  */
                    unsigned int nhops) {   /* paper K: number of AMHL hops (bound γ−κ−K)  */
  return las_preverify_internal(presig, m, mlen, Y, pk, pp, LAS_BOUND_PRESIGN_K(nhops));
  /* ^[CHANGED] las.c:867: return las_preverify_internal(presig, m, mlen, Y, pk, pp, LAS_BOUND_PRESIGN);
   * WHY: same K-hop bound swap as las_presign_k vs las_presign. */
}

/*************************************************
* Name:        las_adapt  (adaptor operation; no Algorithm 1 twin)
*
* Description: Algorithm 2 Adapt((Y,y), pk, σ̂, M): PreVerify, then
*              σ = (c, ẑ + y).  The adapted signature is a fully ORDINARY
*              signature (standard Verify sees Az − ct = w + Y, which matches
*              the c that PreSign hashed).
*
* Returns 0 on success, -1 if the pre-signature is invalid
**************************************************/
int las_adapt(las_sig *sig,          /* paper σ: output adapted signature σ = (c, ẑ + r′) */
              const las_sig *presig, /* paper σ̂: presig = (c, ẑ)                          */
              const uint8_t *m,      /* paper M: message                                  */
              size_t mlen,           /* length of M (no paper symbol)                     */
              const las_pk *Y,       /* paper t′ := Y: statement                          */
              const las_sk *y,       /* paper (Y,y) witness, r′ := y: y->s = y (A y = Y)  */
              const las_pk *pk,      /* paper t: pk->t = t (public key)                   */
              const las_pp *pp) {    /* paper A: pp = A = [I | A']                        */
  /* [PAPER Alg.2] 20: procedure Adapt((Y, y), pk, σ̂, M): */

  /* [PAPER Alg.2] 21:     if PreVerify(Y, pk, σ̂, M) = 0 then */
  /* [PAPER Alg.2] 22:         return ⊥ */
  /* [PAPER Alg.2] 23:     end if */
  if(las_preverify(presig, m, mlen, Y, pk, pp))
    return -1;

  /* [PAPER Alg.2] 24:     Parse (c, ẑ) := σ̂ and r′ := y */
  /* [PAPER Alg.2] 25:     return σ = (c, ẑ + r′) */
  sig->c = presig->c;
  las_polyvecm_add(sig->z, presig->z, y->s);
  las_polyvecm_reduce(sig->z);
  return 0;
  /* [PAPER Alg.2] 26: end procedure */
}

/*************************************************
* Name:        las_ext  (adaptor operation; no Algorithm 1 twin)
*
* Description: Algorithm 2 Ext(Y, σ, σ̂): s = z − ẑ; return it iff A s == Y.
*              This is the on-chain leak that makes swaps atomic: publishing
*              the adapted σ lets anyone holding σ̂ recover the witness.
*              The A s == Y check recomputes t = A r exactly as
*              las_keypair_seed does (quoted below).
*
* Returns 0 (and the witness in y) on success, -1 otherwise
**************************************************/
int las_ext(las_sk *y,               /* paper s: output extracted witness s (y->s = s)   */
            const las_sig *sig,      /* paper σ: sig = (c, z)                            */
            const las_sig *presig,   /* paper σ̂: presig = (ĉ, ẑ)                         */
            const las_pk *Y,         /* paper t′ := Y: statement                         */
            const las_pp *pp) {      /* paper A: pp = A = [I | A']                       */
  /* [PAPER Alg.2] 27: procedure Ext(Y, σ, σ̂): */
  unsigned int i, j;   /* row / coefficient indices (no paper symbol)            */
  poly s1hat[LAS_ELL]; /* NTT buffer, A' half of s <-> s1hat (las_keypair_seed)  */
  poly Ay[LAS_N];      /* paper A s: the product A s, checked against t′ = Y     */

  /* [PAPER Alg.2] 28:     Parse (c, z) := σ and (ĉ, ẑ) := σ̂ */
  /* [PAPER Alg.2] 29:     Parse t′ := Y */
  /* [PAPER Alg.2] 30:     s = z − ẑ */
  las_polyvecm_sub(y->s, sig->z, presig->z);
  las_polyvecm_reduce(y->s);

  /* [PAPER Alg.2] 31:     if t′ ≠ A s, then return ⊥ */
  for(j = 0; j < LAS_ELL; ++j)
    s1hat[j] = y->s[LAS_N + j];              /* [REUSED]  las.c:213: s1hat[j] = sk->s[LAS_N + j];
                                              * (sk -> the extracted witness y)        */
  las_polyvecl_ntt(s1hat);                   /* [REUSED]  las.c:216: las_polyvecl_ntt(s1hat); */
  las_polyvec_matrix_pointwise_montgomery(Ay, pp->mat, s1hat);
                                             /* [REUSED]  las.c:217: las_polyvec_matrix_pointwise_montgomery(pk->t, pp->mat, s1hat); */
  las_polyvecn_reduce(Ay);                   /* [REUSED]  las.c:226: las_polyvecn_reduce(pk->t); */
  las_polyvecn_invntt_tomont(Ay);            /* [REUSED]  las.c:220: las_polyvecn_invntt_tomont(pk->t); */
  las_polyvecn_add(Ay, Ay, y->s);            /* [REUSED]  las.c:223: las_polyvecn_add(pk->t, pk->t, sk->s); */
  las_polyvecn_reduce(Ay);                   /* [REUSED]  las.c:226: las_polyvecn_reduce(pk->t); */
  las_polyvecn_caddq(Ay);                    /* [REUSED]  las.c:229: las_polyvecn_caddq(pk->t); */
  for(i = 0; i < LAS_N; ++i)
    for(j = 0; j < N; ++j)
      if(Ay[i].coeffs[j] != Y->t[i].coeffs[j])
        return -1;                           /* coefficient compare, as in las_verify_internal's
                                              * challenge check (both sides canonical) */

  /* [PAPER Alg.2] 32:     return s */
  return 0;
  /* [PAPER Alg.2] 33: end procedure */
}

/* ================ helper twins (verbatim local copies) ================
 * Each body is a VERBATIM copy of the basesig.c helper named in its
 * comment (prefix b_ -> las_), in basesig.c's order; basesig.c:703-1087
 * carries the per-helper [CHANGED] derivations vs the NAMED upstream
 * poly.c/polyvec.c functions.  Keeping copies (not sharing) preserves
 * independent linkability; behaviour is bit-for-bit identical. */

/* <-> b_rej_S1 (basesig.c:718): verbatim copy; upstream twin rej_eta
 * (poly.c:384) -- 2-bit S_1 codes, see basesig.c:705-716 for the WHY. */
static unsigned int las_rej_S1(int32_t *a, unsigned int len,
                               const uint8_t *buf, unsigned int buflen) {
  unsigned int ctr, pos, s;
  uint8_t byte, v;

  ctr = pos = 0;
  while(ctr < len && pos < buflen) {
    byte = buf[pos++];
    for(s = 0; s < 4 && ctr < len; ++s) {
      v = (byte >> (2*s)) & 3;
      if(v < 3)
        a[ctr++] = (int32_t)v - 1;
    }
  }
  return ctr;
}

/* <-> b_poly_uniform_S1 (basesig.c:748): verbatim copy; upstream twin
 * poly_uniform_eta (poly.c:435), see basesig.c:735-747 for the WHY. */
static void las_poly_uniform_S1(poly *a, const uint8_t seed[LAS_SEEDBYTES], uint16_t nonce) {
  unsigned int ctr;
  uint8_t buf[SHAKE256_RATE];
  uint8_t nb[2];
  keccak_state state;

  nb[0] = (uint8_t)nonce;                    /* <-> stream256_init(&state, seed, nonce) */
  nb[1] = (uint8_t)(nonce >> 8);
  shake256_init(&state);
  shake256_absorb(&state, seed, LAS_SEEDBYTES);
  shake256_absorb(&state, nb, 2);
  shake256_finalize(&state);
  shake256_squeezeblocks(buf, 1, &state);

  ctr = las_rej_S1(a->coeffs, N, buf, SHAKE256_RATE);

  while(ctr < N) {
    shake256_squeezeblocks(buf, 1, &state);
    ctr += las_rej_S1(a->coeffs + ctr, N - ctr, buf, SHAKE256_RATE);
  }
}

/* <-> b_rej_Sgamma (basesig.c:786): verbatim copy; upstream twin
 * rej_uniform (poly.c:309) with window 2*GAMMA+1, see basesig.c:770-784. */
static unsigned int las_rej_Sgamma(int32_t *a, unsigned int len,
                                   const uint8_t *buf, unsigned int buflen) {
  unsigned int ctr, pos;
  uint32_t t, gmask;

  gmask = 1;                                 /* smallest 2^k - 1 >= 2*GAMMA (0x7FFFFF there) */
  while(gmask < 2u*(uint32_t)LAS_GAMMA)
    gmask <<= 1;
  gmask -= 1;

  ctr = pos = 0;
  while(ctr < len && pos + 3 <= buflen) {    /* [REUSED] poly.c:319, incl. the property that
                                              * the 136-byte SHAKE256 block's last byte is
                                              * discarded (136 = 45*3 + 1)               */
    t  = buf[pos++];
    t |= (uint32_t)buf[pos++] << 8;
    t |= (uint32_t)buf[pos++] << 16;
    t &= gmask;

    if(t < 2u*(uint32_t)LAS_GAMMA + 1u)
      a[ctr++] = (int32_t)t - LAS_GAMMA;
  }
  return ctr;
}

/* <-> b_poly_uniform_Sgamma (basesig.c:825): verbatim copy; upstream twin
 * poly_uniform_gamma1 (poly.c:467) with rejection instead of bit-unpacking
 * (gamma is not a power of two), see basesig.c:811-824 for the WHY. */
static void las_poly_uniform_Sgamma(poly *a, const uint8_t seed[64], uint16_t nonce) {
  unsigned int ctr;
  uint8_t buf[SHAKE256_RATE];
  uint8_t nb[2];
  keccak_state state;

  nb[0] = (uint8_t)nonce;                    /* <-> stream256_init(&state, seed, nonce),
                                              * written out; seed is 64 bytes = CRHBYTES,
                                              * exactly upstream's rhoprime width        */
  nb[1] = (uint8_t)(nonce >> 8);
  shake256_init(&state);
  shake256_absorb(&state, seed, 64);
  shake256_absorb(&state, nb, 2);
  shake256_finalize(&state);
  shake256_squeezeblocks(buf, 1, &state);

  ctr = las_rej_Sgamma(a->coeffs, N, buf, SHAKE256_RATE);

  while(ctr < N) {
    shake256_squeezeblocks(buf, 1, &state);
    ctr += las_rej_Sgamma(a->coeffs + ctr, N - ctr, buf, SHAKE256_RATE);
  }
}

/* <-> b_poly_challenge (basesig.c:860): verbatim copy; upstream twin
 * poly_challenge (poly.c:489) with TAU -> LAS_KAPPA and a fixed 32-byte
 * seed, see basesig.c:849-859 for the WHY. */
static void las_poly_challenge(poly *c, const uint8_t seed[LAS_SEEDBYTES]) {
  unsigned int i, b, pos;
  uint64_t signs;
  uint8_t buf[SHAKE256_RATE];
  keccak_state state;

  shake256_init(&state);
  shake256_absorb(&state, seed, LAS_SEEDBYTES);
  shake256_finalize(&state);
  shake256_squeezeblocks(buf, 1, &state);

  signs = 0;
  for(i = 0; i < 8; ++i)
    signs |= (uint64_t)buf[i] << 8*i;
  pos = 8;

  for(i = 0; i < N; ++i)
    c->coeffs[i] = 0;
  for(i = N - LAS_KAPPA; i < N; ++i) {
    do {
      if(pos >= SHAKE256_RATE) {
        shake256_squeezeblocks(buf, 1, &state);
        pos = 0;
      }

      b = buf[pos++];
    } while(b > i);

    c->coeffs[i] = c->coeffs[b];
    c->coeffs[b] = 1 - 2*(signs & 1);
    signs >>= 1;
  }
}

/* <-> b_polyw_pack (basesig.c:905): verbatim copy; upstream twin
 * polyw1_pack (poly.c:888) packing the FULL canonical w (4 bytes/coeff,
 * little-endian), see basesig.c:894-904 for the WHY. */
static void las_polyw_pack(uint8_t *r, const poly *a) {
  unsigned int i;
  uint32_t x;
  poly t = *a;

  poly_reduce(&t);
  poly_caddq(&t);
  for(i = 0; i < N; ++i) {
    x = (uint32_t)t.coeffs[i];
    r[4*i+0] = (uint8_t)x;
    r[4*i+1] = (uint8_t)(x >> 8);
    r[4*i+2] = (uint8_t)(x >> 16);
    r[4*i+3] = (uint8_t)(x >> 24);
  }
}

/* <-> b_polyvec_matrix_pointwise_montgomery (basesig.c:927) <->
 * polyvec_matrix_pointwise_montgomery (polyvec.c:24).  v spans only the
 * l columns of A' because A = [I | A'] (identity block added by callers). */
static void las_polyvec_matrix_pointwise_montgomery(poly t[LAS_N], const poly mat[LAS_N][LAS_ELL],
                                                    const poly v[LAS_ELL]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    las_polyvecl_pointwise_acc_montgomery(&t[i], mat[i], v);
}

/* <-> b_polyvecl_pointwise_acc_montgomery (basesig.c:936) <->
 * polyvecl_pointwise_acc_montgomery (polyvec.c:113), L -> l. */
static void las_polyvecl_pointwise_acc_montgomery(poly *w, const poly u[LAS_ELL],
                                                  const poly v[LAS_ELL]) {
  unsigned int i;
  poly t;

  poly_pointwise_montgomery(w, &u[0], &v[0]);
  for(i = 1; i < LAS_ELL; ++i) {
    poly_pointwise_montgomery(&t, &u[i], &v[i]);
    poly_add(w, w, &t);
  }
}

/* <-> b_polyvecl_ntt (basesig.c:949) <-> polyvecl_ntt (polyvec.c:81),
 * over the l columns of A'. */
static void las_polyvecl_ntt(poly v[LAS_ELL]) {
  unsigned int i;

  for(i = 0; i < LAS_ELL; ++i)
    poly_ntt(&v[i]);
}

/* <-> b_polyvecm_uniform_S1 (basesig.c:958) <-> polyvecl_uniform_eta
 * (polyvec.c:35): same nonce++-per-poly discipline; L -> m, eta -> S_1. */
static void las_polyvecm_uniform_S1(poly v[LAS_M], const uint8_t seed[LAS_SEEDBYTES], uint16_t nonce) {
  unsigned int i;

  for(i = 0; i < LAS_M; ++i)
    las_poly_uniform_S1(&v[i], seed, nonce++);
}

/* <-> b_polyvecm_uniform_Sgamma (basesig.c:967) <-> polyvecl_uniform_gamma1
 * (polyvec.c:42): same L*nonce + i derivation (m*nonce + i here), so one
 * nonce++ per signing attempt at the call site. */
static void las_polyvecm_uniform_Sgamma(poly v[LAS_M], const uint8_t seed[64], uint16_t nonce) {
  unsigned int i;

  for(i = 0; i < LAS_M; ++i)
    las_poly_uniform_Sgamma(&v[i], seed, (uint16_t)(LAS_M*nonce + i));
}

/* <-> b_polyvecm_reduce (basesig.c:975) <-> polyvecl_reduce (polyvec.c:49). */
static void las_polyvecm_reduce(poly v[LAS_M]) {
  unsigned int i;

  for(i = 0; i < LAS_M; ++i)
    poly_reduce(&v[i]);
}

/* <-> b_polyvecm_add (basesig.c:983) <-> polyvecl_add (polyvec.c:66). */
static void las_polyvecm_add(poly w[LAS_M], const poly u[LAS_M], const poly v[LAS_M]) {
  unsigned int i;

  for(i = 0; i < LAS_M; ++i)
    poly_add(&w[i], &u[i], &v[i]);
}

/* <-> polyveck_sub (polyvec.c:218), K -> m: the same upstream twin as
 * b_polyvecn_sub (basesig.c:1050) at width m.  No basesig.c body: only
 * Ext's s = z - z^ subtracts an m-vector, and the base scheme has no Ext. */
static void las_polyvecm_sub(poly w[LAS_M], const poly u[LAS_M], const poly v[LAS_M]) {
  unsigned int i;

  for(i = 0; i < LAS_M; ++i)
    poly_sub(&w[i], &u[i], &v[i]);
}

/* <-> b_polyvecm_ntt (basesig.c:991) <-> polyvecl_ntt (polyvec.c:81),
 * L -> m (the full secret/response vector). */
static void las_polyvecm_ntt(poly v[LAS_M]) {
  unsigned int i;

  for(i = 0; i < LAS_M; ++i)
    poly_ntt(&v[i]);
}

/* <-> b_polyvecm_invntt_tomont (basesig.c:999) <-> polyvecl_invntt_tomont
 * (polyvec.c:88). */
static void las_polyvecm_invntt_tomont(poly v[LAS_M]) {
  unsigned int i;

  for(i = 0; i < LAS_M; ++i)
    poly_invntt_tomont(&v[i]);
}

/* <-> b_polyvecm_pointwise_poly_montgomery (basesig.c:1007) <->
 * polyvecl_pointwise_poly_montgomery (polyvec.c:95). */
static void las_polyvecm_pointwise_poly_montgomery(poly r[LAS_M], const poly *a, const poly v[LAS_M]) {
  unsigned int i;

  for(i = 0; i < LAS_M; ++i)
    poly_pointwise_montgomery(&r[i], a, &v[i]);
}

/* <-> b_polyvecm_chknorm (basesig.c:1015) <-> polyvecl_chknorm
 * (polyvec.c:139); poly_chknorm itself is REUSED. */
static int las_polyvecm_chknorm(const poly v[LAS_M], int32_t bound) {
  unsigned int i;

  for(i = 0; i < LAS_M; ++i)
    if(poly_chknorm(&v[i], bound))
      return 1;

  return 0;
}

/* <-> b_polyvecn_reduce (basesig.c:1026) <-> polyveck_reduce
 * (polyvec.c:168), K -> n. */
static void las_polyvecn_reduce(poly v[LAS_N]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    poly_reduce(&v[i]);
}

/* <-> b_polyvecn_caddq (basesig.c:1034) <-> polyveck_caddq (polyvec.c:183). */
static void las_polyvecn_caddq(poly v[LAS_N]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    poly_caddq(&v[i]);
}

/* <-> b_polyvecn_add (basesig.c:1042) <-> polyveck_add (polyvec.c:200). */
static void las_polyvecn_add(poly w[LAS_N], const poly u[LAS_N], const poly v[LAS_N]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    poly_add(&w[i], &u[i], &v[i]);
}

/* <-> b_polyvecn_sub (basesig.c:1050) <-> polyveck_sub (polyvec.c:218). */
static void las_polyvecn_sub(poly w[LAS_N], const poly u[LAS_N], const poly v[LAS_N]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    poly_sub(&w[i], &u[i], &v[i]);
}

/* <-> b_polyvecn_ntt (basesig.c:1058) <-> polyveck_ntt (polyvec.c:248). */
static void las_polyvecn_ntt(poly v[LAS_N]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    poly_ntt(&v[i]);
}

/* <-> b_polyvecn_invntt_tomont (basesig.c:1066) <-> polyveck_invntt_tomont
 * (polyvec.c:264). */
static void las_polyvecn_invntt_tomont(poly v[LAS_N]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    poly_invntt_tomont(&v[i]);
}

/* <-> b_polyvecn_pointwise_poly_montgomery (basesig.c:1074) <->
 * polyveck_pointwise_poly_montgomery (polyvec.c:271). */
static void las_polyvecn_pointwise_poly_montgomery(poly r[LAS_N], const poly *a, const poly v[LAS_N]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    poly_pointwise_montgomery(&r[i], a, &v[i]);
}

/* <-> b_polyvecn_pack_w (basesig.c:1082) <-> polyveck_pack_w1
 * (polyvec.c:384): full w instead of w1 codes. */
static void las_polyvecn_pack_w(uint8_t r[LAS_N*N*4], const poly w[LAS_N]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    las_polyw_pack(&r[i*N*4], &w[i]);
}

/* Deterministic per-(pre)signature mask randomness: seed = SHAKE256(tag, sk,
 * [Y], M).  Makes (pre)signing a deterministic function of its inputs --
 * reproducible KATs and no fresh per-signature randomness to mishandle
 * (nonce-reuse safety).  LAS-only helper (the _det KAT path); no
 * basesig.c/upstream analogue.  The polynomial packer it reuses for Y is
 * las_polyw_pack (the b_polyw_pack twin above). */
static void det_seed(uint8_t out[64], uint8_t tag, const las_sk *sk,
                     const las_pk *Y, const uint8_t *m, size_t mlen) {
  keccak_state state;
  uint8_t skb[LAS_M * N];
  uint8_t buf[N * 4];
  unsigned int i, k;

  for(i = 0; i < LAS_M; ++i)                      /* ternary sk -> 1 byte/coeff */
    for(k = 0; k < N; ++k)
      skb[i * N + k] = (uint8_t)(int8_t)sk->s[i].coeffs[k];

  shake256_init(&state);
  shake256_absorb(&state, &tag, 1);              /* domain: 0=sign, 1=presign  */
  shake256_absorb(&state, skb, sizeof skb);
  if(Y)
    for(i = 0; i < LAS_N; ++i) {                  /* bind the statement Y       */
      las_polyw_pack(buf, &Y->t[i]);
      shake256_absorb(&state, buf, N * 4);
    }
  shake256_absorb(&state, m, mlen);
  shake256_finalize(&state);
  shake256_squeeze(out, 64, &state);
}

/* ============== end-to-end PACKED-API tier (bytes in/out) ==============
 * The SECOND measured boundary, mirroring basesig.c's packed tier (which in
 * turn mirrors upstream's ONLY boundary -- sign.c packs/unpacks with
 * packing.h inside its API).  Struct functions above = CORE CRYPTO tier;
 * these = END-TO-END tier: validating unpack -> core -> pack, all inside
 * the call.  One codec (ref/serialize.{c,h}) serves both schemes.  The
 * base-path functions quote basesig.c's packed twins; the adaptor
 * functions quote their OWN base-path packed twin in this file, exactly
 * like the Algorithm 2 struct functions quote their Algorithm 1 twins. */

/*************************************************
* Name:        las_keypair_packed  <->  base_sign_keypair_packed (basesig.c:1119)
*
* Description: KeyGen at the byte boundary: core KeyGen, then pack both
*              keys inside the call.  Also mints packed statement/witness
*              pairs (Y_b, y_b) -- a statement is literally a public key.
*
* Returns 0 (success; a freshly sampled sk is always ternary)
**************************************************/
int las_keypair_packed(uint8_t pk_b[LAS_PK_BYTES],   /* packed public key (bytes out)  */
                       uint8_t sk_b[LAS_SK_BYTES],   /* packed secret key (bytes out)  */
                       const las_pp *pp) {           /* paper A: pp = A = [I | A']     */
  las_pk pk;
  las_sk sk;

  las_keypair(&pk, &sk, pp);                 /* [REUSED]  basesig.c:1125: base_sign_keypair(&pk, &sk, pp); */
  las_pack_pk(pk_b, &pk);                    /* [REUSED]  basesig.c:1126: las_pack_pk(pk_b, &pk); */
  return las_pack_sk(sk_b, &sk);             /* [REUSED]  basesig.c:1127: return las_pack_sk(sk_b, &sk); */
}

/*************************************************
* Name:        las_signature_packed  <->  base_sign_signature_packed (basesig.c:1146)
*
* Description: Sign at the byte boundary: validating unpack of both keys,
*              core Sign, pack the signature -- all inside the call.
*
* Returns 0 (success), -1 if a key fails validating decode
**************************************************/
int las_signature_packed(uint8_t sig_b[LAS_SIG_BYTES], /* packed signature (bytes out) */
                         const uint8_t *m,             /* paper M: message             */
                         size_t mlen,                  /* length of M                  */
                         const uint8_t pk_b[LAS_PK_BYTES], /* packed public key (bytes) */
                         const uint8_t sk_b[LAS_SK_BYTES], /* packed secret key (bytes) */
                         const las_pp *pp) {           /* paper A: pp = A = [I | A']   */
  las_pk pk;
  las_sk sk;
  las_sig sig;

  if(las_unpack_pk(&pk, pk_b))
    return -1;                               /* [REUSED]  basesig.c:1156: if(las_unpack_pk(&pk, pk_b)) */
  if(las_unpack_sk(&sk, sk_b))
    return -1;                               /* [REUSED]  basesig.c:1158: if(las_unpack_sk(&sk, sk_b)) */
  las_signature(&sig, m, mlen, &pk, &sk, pp);
                                             /* [REUSED]  basesig.c:1160: base_sign_signature(&sig, m, mlen, &pk, &sk, pp); */
  return las_pack_sig(sig_b, &sig);          /* [REUSED]  basesig.c:1161: return las_pack_sig(sig_b, &sig); */
}

/*************************************************
* Name:        las_verify_packed  <->  base_sign_verify_packed (basesig.c:1181)
*
* Description: Verify at the byte boundary: validating decode of pk and
*              signature, then the core Verify -- all inside the call.
*              THE on-chain-style verifier entry point: returns 0 iff the
*              bytes decode to valid objects AND the signature verifies --
*              the interface a real integration (Solidity/precompile/
*              circuit) would expose; tamper-tested in ref/test/test_serde.c.
*
* Returns 0 if signature could be verified correctly and -1 otherwise
**************************************************/
int las_verify_packed(const uint8_t sig_b[LAS_SIG_BYTES], /* packed signature (bytes)  */
                      const uint8_t *m,                   /* paper M: message          */
                      size_t mlen,                        /* length of M               */
                      const uint8_t pk_b[LAS_PK_BYTES],   /* packed public key (bytes) */
                      const las_pp *pp) {                 /* paper A: pp = A = [I | A'] */
  las_pk pk;
  las_sig sig;

  if(las_unpack_pk(&pk, pk_b))
    return -1;                               /* [REUSED]  basesig.c:1189: if(las_unpack_pk(&pk, pk_b)) */
  if(las_unpack_sig(&sig, sig_b))
    return -1;                               /* [REUSED]  basesig.c:1191: if(las_unpack_sig(&sig, sig_b)) */
  return las_verify(&sig, m, mlen, &pk, pp); /* [REUSED]  basesig.c:1193: return base_sign_verify(&sig, m, mlen, &pk, pp); */
}

/*************************************************
* Name:        las_presign_packed  (adaptor twin of las_signature_packed)
*
* Description: PreSign at the byte boundary: like las_signature_packed with
*              the packed statement Y_b decoded as well (single-hop bound).
*
* Returns 0 (success), -1 if a key or the statement fails validating decode
**************************************************/
int las_presign_packed(uint8_t presig_b[LAS_SIG_BYTES], /* packed pre-signature (bytes out) */
                       const uint8_t *m,                /* paper M: message                 */
                       size_t mlen,                     /* length of M                      */
                       const uint8_t Y_b[LAS_PK_BYTES], /* packed statement (bytes)         */
                       const uint8_t pk_b[LAS_PK_BYTES],/* packed public key (bytes)        */
                       const uint8_t sk_b[LAS_SK_BYTES],/* packed secret key (bytes)        */
                       const las_pp *pp) {              /* paper A: pp = A = [I | A']       */
  las_pk Y;
  las_pk pk;
  las_sk sk;
  las_sig presig;

  if(las_unpack_pk(&Y, Y_b))
    return -1;
  /* ^[CHANGED] (no las_signature_packed line)
   * WHY: PreSign additionally takes the statement Y; it decodes with the pk
   * codec because a statement IS a public key (Y = A y_wit). */
  if(las_unpack_pk(&pk, pk_b))
    return -1;                               /* [REUSED]  las.c:1430: if(las_unpack_pk(&pk, pk_b)) */
  if(las_unpack_sk(&sk, sk_b))
    return -1;                               /* [REUSED]  las.c:1432: if(las_unpack_sk(&sk, sk_b)) */
  las_presign(&presig, m, mlen, &Y, &pk, &sk, pp);
  /* ^[CHANGED] las.c:1434: las_signature(&sig, m, mlen, &pk, &sk, pp);
   * WHY: the core call is PreSign with the decoded statement (single-hop
   * bound; c = H(pk, w + Y, M)). */
  return las_pack_sig(presig_b, &presig);    /* [REUSED]  las.c:1436: return las_pack_sig(sig_b, &sig); */
}

/*************************************************
* Name:        las_preverify_packed  (adaptor twin of las_verify_packed)
*
* Description: PreVerify at the byte boundary: like las_verify_packed with
*              the packed statement Y_b decoded as well (single-hop bound).
*
* Returns 0 if pre-signature could be verified correctly and -1 otherwise
**************************************************/
int las_preverify_packed(const uint8_t presig_b[LAS_SIG_BYTES], /* packed pre-signature (bytes) */
                         const uint8_t *m,                      /* paper M: message             */
                         size_t mlen,                           /* length of M                  */
                         const uint8_t Y_b[LAS_PK_BYTES],       /* packed statement (bytes)     */
                         const uint8_t pk_b[LAS_PK_BYTES],      /* packed public key (bytes)    */
                         const las_pp *pp) {                    /* paper A: pp = A = [I | A']   */
  las_pk Y;
  las_pk pk;
  las_sig presig;

  if(las_unpack_pk(&Y, Y_b))
    return -1;
  /* ^[CHANGED] (no las_verify_packed line)
   * WHY: the statement Y is decoded too, with the pk codec (same reason as
   * las_presign_packed). */
  if(las_unpack_pk(&pk, pk_b))
    return -1;                               /* [REUSED]  las.c:1459: if(las_unpack_pk(&pk, pk_b)) */
  if(las_unpack_sig(&presig, presig_b))
    return -1;                               /* [REUSED]  las.c:1461: if(las_unpack_sig(&sig, sig_b)) */
  return las_preverify(&presig, m, mlen, &Y, &pk, pp);
  /* ^[CHANGED] las.c:1463: return las_verify(&sig, m, mlen, &pk, pp);
   * WHY: the core call is PreVerify with the decoded statement (checks
   * c == H(pk, w' + Y, M) at the single-hop bound). */
}

/*************************************************
* Name:        las_adapt_packed  (adaptor operation at the byte boundary)
*
* Description: Adapt at the byte boundary: validating decode of the
*              pre-signature, statement, witness and public key; core Adapt
*              (which PreVerifies); pack the adapted -- fully ordinary --
*              signature.  The witness decodes with the sk codec (ternary):
*              a witness IS a secret key.
*
* Returns 0 on success, -1 on any decode failure or invalid pre-signature
**************************************************/
int las_adapt_packed(uint8_t sig_b[LAS_SIG_BYTES],        /* packed adapted signature (bytes out) */
                     const uint8_t presig_b[LAS_SIG_BYTES],/* packed pre-signature (bytes)        */
                     const uint8_t *m,                     /* paper M: message                    */
                     size_t mlen,                          /* length of M                         */
                     const uint8_t Y_b[LAS_PK_BYTES],      /* packed statement (bytes)            */
                     const uint8_t y_b[LAS_SK_BYTES],      /* packed witness (bytes)              */
                     const uint8_t pk_b[LAS_PK_BYTES],     /* packed public key (bytes)           */
                     const las_pp *pp) {                   /* paper A: pp = A = [I | A']          */
  las_pk Y;
  las_sk y;
  las_pk pk;
  las_sig presig;
  las_sig sig;

  if(las_unpack_pk(&Y, Y_b))
    return -1;
  if(las_unpack_sk(&y, y_b))
    return -1;
  if(las_unpack_pk(&pk, pk_b))
    return -1;
  if(las_unpack_sig(&presig, presig_b))
    return -1;
  if(las_adapt(&sig, &presig, m, mlen, &Y, &y, &pk, pp))
    return -1;
  return las_pack_sig(sig_b, &sig);          /* adapted z stays in the codec band:
                                              * |z|inf <= (g-k-1) + 1 = g-k        */
}

/*************************************************
* Name:        las_ext_packed  (adaptor operation at the byte boundary)
*
* Description: Ext at the byte boundary: validating decode of both
*              signatures and the statement; core Ext (s = z - z^, checked
*              against A s == Y); pack the recovered witness with the sk
*              codec.  This is the on-chain leak made byte-real: the two
*              byte strings anyone can fetch from the chain yield y_b.
*
* Returns 0 (and the packed witness) on success, -1 otherwise
**************************************************/
int las_ext_packed(uint8_t y_b[LAS_SK_BYTES],          /* packed extracted witness (bytes out) */
                   const uint8_t sig_b[LAS_SIG_BYTES], /* packed adapted signature (bytes)     */
                   const uint8_t presig_b[LAS_SIG_BYTES],/* packed pre-signature (bytes)       */
                   const uint8_t Y_b[LAS_PK_BYTES],    /* packed statement (bytes)             */
                   const las_pp *pp) {                 /* paper A: pp = A = [I | A']           */
  las_pk Y;
  las_sk y;
  las_sig sig;
  las_sig presig;

  if(las_unpack_pk(&Y, Y_b))
    return -1;
  if(las_unpack_sig(&sig, sig_b))
    return -1;
  if(las_unpack_sig(&presig, presig_b))
    return -1;
  if(las_ext(&y, &sig, &presig, &Y, pp))
    return -1;
  return las_pack_sk(y_b, &y);               /* single-hop witness is ternary; an AMHL
                                              * cumulative witness (|s_j|inf > 1) is
                                              * deliberately outside the sk codec      */
}
