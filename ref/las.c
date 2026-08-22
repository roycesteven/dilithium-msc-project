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
 *   setup_public_params                -   the paper's Setup() -> pp, consumed by BOTH
 *                                basesig.c and las.c; a separate file because
 *                                A is public infrastructure, not scheme code
 *                                (see the [DELETED] notes at basesig.c:136-140
 *                                and basesig.c:267-269)
 *
 *   -- Algorithm 1 (base path; one-to-one with basesig.c) --
 *   base_keygen             <->  base_sign_keypair             (basesig.c:115)
 *   base_keygen_seed         -   deterministic KeyGen body (KAT path; no slot)
 *   base_sign_internal  <->  base_sign_signature_internal  (basesig.c:212)
 *   base_sign           <->  base_sign_signature           (basesig.c:426)
 *   base_sign_det        -   deterministic Sign (KAT path; no slot)
 *   las_sign                <->  base_sign                     (basesig.c:464)
 *   base_verify_internal     <->  base_sign_verify_internal     (basesig.c:499)
 *   base_verify              <->  base_sign_verify              (basesig.c:645)
 *   las_open                <->  base_sign_open                (basesig.c:667)
 *
 *   -- Algorithm 2 (adaptor layer; upstream = the PAPER, names kept) --
 *   las_presign_internal / las_presign / las_presign_det   (adaptor twins of
 *       base_sign_internal / base_sign / base_sign_det)
 *   las_preverify_internal / las_preverify                 (adaptor twins of
 *       base_verify_internal / base_verify)
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
#include <string.h>
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
  double p = (2.0*(double)bound - 1.0) / (2.0*(double)GAMMA + 1.0);
  double acc = 1.0;
  unsigned int e = (unsigned int)N_PLUS_ELL * LAS_D;      /* (n+ell)*d coefficients */
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
static unsigned int las_rej_Sgamma(int32_t *a, unsigned int len, const uint8_t *buf, unsigned int buflen);
static void las_poly_uniform_Sgamma(poly *a, const uint8_t seed[64], uint16_t nonce);
static void las_poly_challenge(poly *c, const uint8_t seed[LAS_CTILDEBYTES]);
static void las_polyw_pack(uint8_t *r, const poly *a);
static void las_polyvec_matrix_pointwise_montgomery(poly t[LAS_N], const poly mat[LAS_N][ELL],
                                                    const poly v[ELL]);
static void las_polyvecl_pointwise_acc_montgomery(poly *w, const poly u[ELL],
                                                  const poly v[ELL]);
static void las_polyvecl_ntt(poly v[ELL]);
static void las_polyvecm_uniform_Sgamma(poly v[N_PLUS_ELL], const uint8_t seed[64], uint16_t nonce);
static void las_polyvecm_reduce(poly v[N_PLUS_ELL]);
static void las_polyvecm_add(poly w[N_PLUS_ELL], const poly u[N_PLUS_ELL], const poly v[N_PLUS_ELL]);
static void las_polyvecm_sub(poly w[N_PLUS_ELL], const poly u[N_PLUS_ELL], const poly v[N_PLUS_ELL]);
static void las_polyvecm_ntt(poly v[N_PLUS_ELL]);
static void las_polyvecm_invntt_tomont(poly v[N_PLUS_ELL]);
static void las_polyvecm_pointwise_poly_montgomery(poly r[N_PLUS_ELL], const poly *a, const poly v[N_PLUS_ELL]);
static int  las_polyvecm_chknorm(const poly v[N_PLUS_ELL], int32_t bound);
static void las_polyvecn_reduce(poly v[LAS_N]);
static void las_polyvecn_caddq(poly v[LAS_N]);
static void las_polyvecn_add(poly w[LAS_N], const poly u[LAS_N], const poly v[LAS_N]);
static void las_polyvecn_sub(poly w[LAS_N], const poly u[LAS_N], const poly v[LAS_N]);
static void las_polyvecn_ntt(poly v[LAS_N]);
static void las_polyvecn_invntt_tomont(poly v[LAS_N]);
static void las_polyvecn_pointwise_poly_montgomery(poly r[LAS_N], const poly *a, const poly v[LAS_N]);
static void las_polyvecn_pack_w(uint8_t r[LAS_N*LAS_D*4], const poly w[LAS_N]);
static void det_seed(uint8_t out[64], uint8_t tag, const secret_key *sk,
                     const statement *Y, const uint8_t *m, size_t mlen);

/* =============== scheme, Algorithm 2 (adaptor layer) ===============
 * No basesig.c/sign.c analogue from here on: these are the adaptor
 * operations LAS adds on top of the base signature (upstream = the PAPER).
 * Annotations therefore quote each function's Algorithm 1 twin in THIS file
 * (las.c:<line>: <code>): PreSign diffs against Sign, PreVerify against
 * Verify, exactly as basesig.c diffs against sign.c. */

/*************************************************
* Name:        las_presign_internal  (adaptor twin of base_sign_internal)
*
* Description: Algorithm 2 PreSign body: base_sign_internal, except the
*              statement is folded into the hash -- c = H(pk, w + Y, M) --
*              and the rejection bound tightens to `bound` (γ−κ−1 single-hop,
*              γ−κ−K AMHL).  Parameterised by the mask seed; same NTT
*              hoisting as the twin.
*
* Returns 0 (success)
**************************************************/
int las_presign_internal(pre_signature *presig,   /* paper σ̂: output pre-signature σ̂ = (c, ẑ)    */
                         const uint8_t *m,  /* paper M: message                            */
                         size_t mlen,       /* length of M (no paper symbol)               */
                         const statement *Y,   /* paper t′ := Y: statement, Y->t_prime = Y = A y_wit */
                         const public_key *pk,  /* paper t: pk->t = t (public key)             */
                         const secret_key *sk,  /* paper r: sk->r = r (secret key)             */
                         const public_params *pp,  /* paper A: A = [I | A'] (A in pp = (A,H))                  */
                         int32_t bound,     /* paper γ−κ−1 (single-hop) / γ−κ−K (AMHL)     */
                         const uint8_t mask_seed[64]) {  /* PRG mask seed (<-> seed) */
  /* [PAPER Alg.2] 1:  procedure PreSign((pk, sk), Y, M): */
  unsigned int j;                /* decls <-> base_sign_internal's, plus w_plus_t_prime     */
  uint8_t t_packed[LAS_N*LAS_D*4];       /* packed pk (fixed hash prefix)                   */
  uint8_t w_packed[LAS_N*LAS_D*4];       /* packed w + Y (the hashed commitment; see below) */
  uint8_t c_tilde[LAS_CTILDEBYTES];  /* challenge seed                                  */
  uint16_t mask_nonce = 0;            /* PRG counter                                     */
  poly y[N_PLUS_ELL];                 /* paper y: mask, y <-$ Sγ^(n+ℓ)                   */
  poly y_1_hat[ELL];            /* NTT buffer, A' half of y                        */
  poly w[LAS_N];                 /* paper w: commitment, w = A y                    */
  poly w_plus_t_prime[LAS_N];                /* paper w + t′: the hashed commitment w + Y       */
  poly r_hat[N_PLUS_ELL];              /* paper r in NTT domain                           */
  poly c;                        /* paper c: challenge polynomial, LOCAL only (SampleInBall(c_tilde)) */
  poly c_hat;                     /* NTT copy of c                                   */
  keccak_state state;
  /* ^[CHANGED] (one extra declaration vs the twin)
   *     poly w[LAS_N];
   * WHY: w_plus_t_prime holds the SHIFTED commitment w + Y of paper Alg. 2 step 4, so w
   * itself stays untouched. */

  /* Compute mu = CRH(tr, pre, msg) */
  las_polyvecn_pack_w(t_packed, pk->t);          /* [REUSED]  las.c:268: las_polyvecn_pack_w(t_packed, pk->t); */

  /* Expand matrix and transform vectors */
  for(j = 0; j < N_PLUS_ELL; ++j)
    r_hat[j] = sk->r[j];                      /* [REUSED]  las.c:274: r_hat[j] = sk->r[j]; */
  las_polyvecm_ntt(r_hat);                    /* [REUSED]  las.c:275: las_polyvecm_ntt(r_hat); */

rej:                                         /* [REUSED]  las.c:279: rej: */
  ++las_attempts;                            /* [REUSED]  las.c:280: ++las_attempts; */

  /* Sample intermediate vector y */
  /* [PAPER Alg.2] 2:      y ←$ Sγ^(n+ℓ) */
  las_polyvecm_uniform_Sgamma(y, mask_seed, mask_nonce++);
                                             /* [REUSED]  las.c:288: las_polyvecm_uniform_Sgamma(y, mask_seed, mask_nonce++); */

  /* Matrix-vector multiplication */
  /* [PAPER Alg.2] 3:      w = A y */
  for(j = 0; j < ELL; ++j)
    y_1_hat[j] = y[LAS_N + j];                  /* [REUSED]  las.c:294: y_1_hat[j] = y[LAS_N + j]; */
  las_polyvecl_ntt(y_1_hat);                    /* [REUSED]  las.c:295: las_polyvecl_ntt(y_1_hat); */
  las_polyvec_matrix_pointwise_montgomery(w, pp->a_prime, y_1_hat);
                                             /* [REUSED]  las.c:296: las_polyvec_matrix_pointwise_montgomery(w, pp->a_prime, y_1_hat); */
  las_polyvecn_reduce(w);                    /* [REUSED]  las.c:298: las_polyvecn_reduce(w); */
  las_polyvecn_invntt_tomont(w);             /* [REUSED]  las.c:299: las_polyvecn_invntt_tomont(w); */
  las_polyvecn_add(w, w, y);                 /* [REUSED]  las.c:300: las_polyvecn_add(w, w, y); */
  las_polyvecn_reduce(w);                    /* [REUSED]  las.c:298: las_polyvecn_reduce(w); */

  /* Decompose w and call the random oracle */
  las_polyvecn_caddq(w);                     /* [REUSED]  las.c:305: las_polyvecn_caddq(w); */
  /* [PAPER Alg.2] 4:      c = H(pk, w + t′, M), where t′ := Y */
  las_polyvecn_add(w_plus_t_prime, w, Y->t_prime);
  las_polyvecn_reduce(w_plus_t_prime);
  las_polyvecn_caddq(w_plus_t_prime);
  /* ^[CHANGED] (no base_sign_internal lines)
   * WHY: THE core adaptor mechanism -- the statement is folded into the
   * commitment before hashing.  Sign hashes w; PreSign hashes w + Y, so the
   * adapted signature (z = ẑ + y_wit gives Az − ct = w + Y) later satisfies
   * the ORDINARY Verify equation for the same c. */
  las_polyvecn_pack_w(w_packed, w_plus_t_prime);
  /* ^[CHANGED] las.c:306: las_polyvecn_pack_w(w_packed, w);
   * WHY: the oracle input is the shifted commitment w + Y, not w. */

  shake256_init(&state);                     /* [REUSED]  las.c:308: shake256_init(&state); */
  shake256_absorb(&state, t_packed, sizeof t_packed);
                                             /* [REUSED]  las.c:309: shake256_absorb(&state, t_packed, sizeof t_packed); */
  shake256_absorb(&state, w_packed, sizeof w_packed);
                                             /* [REUSED]  las.c:311: shake256_absorb(&state, w_packed, sizeof w_packed); */
  shake256_absorb(&state, m, mlen);          /* [REUSED]  las.c:313: shake256_absorb(&state, m, mlen); */
  shake256_finalize(&state);                 /* [REUSED]  las.c:314: shake256_finalize(&state); */
  shake256_squeeze(c_tilde, LAS_CTILDEBYTES, &state);
                                             /* [REUSED]  las.c:315: shake256_squeeze(c_tilde, LAS_SEEDBYTES, &state); */
  las_poly_challenge(&c, c_tilde);             /* [REUSED]  las.c:318: las_poly_challenge(&c, c_tilde); */
  c_hat = c;                                  /* [REUSED]  las.c:319: c_hat = c; */
  poly_ntt(&c_hat);                           /* [REUSED]  las.c:320: poly_ntt(&c_hat); */

  /* Compute z, reject if it reveals secret */
  /* [PAPER Alg.2] 5:      ẑ = y + c r, where r := sk */
  las_polyvecm_pointwise_poly_montgomery(presig->z_hat, &c_hat, r_hat);
                                             /* [REUSED]  las.c:326: las_polyvecm_pointwise_poly_montgomery(sig->z, &c_hat, r_hat);
                                              * (sig -> presig)                        */
  las_polyvecm_invntt_tomont(presig->z_hat);     /* [REUSED]  las.c:328: las_polyvecm_invntt_tomont(sig->z); */
  las_polyvecm_add(presig->z_hat, presig->z_hat, y); /* [REUSED]  las.c:329: las_polyvecm_add(sig->z, sig->z, y); */
  las_polyvecm_reduce(presig->z_hat);            /* [REUSED]  las.c:330: las_polyvecm_reduce(sig->z); */
  /* [PAPER Alg.2] 6:      if ||ẑ||∞ > γ − κ − 1, then Restart */
  if(las_polyvecm_chknorm(presig->z_hat, bound))
    goto rej;
  /* ^[CHANGED] las.c:332: if(las_polyvecm_chknorm(sig->z, BOUND_SIGN))
   * WHY: PreSign rejects at the TIGHTER caller-supplied bound (γ−κ−1
   * single-hop, γ−κ−K AMHL): the ternary witness has ||y_wit||∞ ≤ 1 (≤ K
   * cumulative), so the adapted z = ẑ + y_wit still clears Verify's γ−κ.
   * THE failure mode to watch: loosening this to γ−κ makes adapted
   * signatures overflow the Verify bound and Verify rejects everything. */

  /* Write signature */
  /* [PAPER Alg.2] 7:      return σ̂ = (c, ẑ) */
  memcpy(presig->c_tilde, c_tilde, LAS_CTILDEBYTES);  /* store the 32-byte challenge digest; c is local */
  return 0;                                  /* [REUSED]  las.c:339: return 0; */
  /* [PAPER Alg.2] 8:  end procedure */
}

/*************************************************
* Name:        las_presign  (adaptor twin of base_sign)
*
* Description: Algorithm 2 PreSign(sk, Y, M), random path: fresh mask seed,
*              then the internal at the single-hop bound γ−κ−1.
*
* Returns 0 (success)
**************************************************/
int las_presign(pre_signature *presig,     /* paper σ̂: output pre-signature σ̂ = (c, ẑ) */
                const uint8_t *m,    /* paper M: message                         */
                size_t mlen,         /* length of M (no paper symbol)            */
                const statement *Y,     /* paper t′ := Y: statement                 */
                const public_key *pk,    /* paper t: pk->t = t (public key)          */
                const secret_key *sk,    /* paper r: sk->r = r (secret key)          */
                const public_params *pp) {  /* paper A: A = [I | A'] (A in pp = (A,H))               */
  uint8_t mask_seed[64];      /* [REUSED]  las.c:382: uint8_t mask_seed[64]; */

  randombytes(mask_seed, 64);                     /* [REUSED]  las.c:385: randombytes(mask_seed, 64); */
  return las_presign_internal(presig, m, mlen, Y, pk, sk, pp, BOUND_PRESIGN, mask_seed);
  /* ^[CHANGED] las.c:389: return base_sign_internal(sig, m, mlen, pk, sk, pp, seed);
   * WHY: the adaptor internal additionally takes the statement Y and the
   * single-hop PreSign bound γ−κ−1 (BOUND_PRESIGN). */
}

/*************************************************
* Name:        las_presign_det  (adaptor twin of base_sign_det; KAT path)
*
* Description: Deterministic PreSign: mask seed derived from (sk, Y, M) via
*              det_seed (tag 1 binds the statement Y), single-hop bound.
*
* Returns 0 (success)
**************************************************/
int las_presign_det(pre_signature *presig,     /* paper σ̂: output pre-signature σ̂ = (c, ẑ) */
                    const uint8_t *m,    /* paper M: message                         */
                    size_t mlen,         /* length of M (no paper symbol)            */
                    const statement *Y,     /* paper t′ := Y: statement                 */
                    const public_key *pk,    /* paper t: pk->t = t (public key)          */
                    const secret_key *sk,    /* paper r: sk->r = r (secret key)          */
                    const public_params *pp) {  /* paper A: A = [I | A'] (A in pp = (A,H))               */
  uint8_t mask_seed[64];  /* PRG mask seed, derived from (sk, Y, M) (no paper symbol) */

  det_seed(mask_seed, 1, sk, Y, m, mlen);
  /* ^[CHANGED] las.c:384: det_seed(seed, 0, sk, NULL, m, mlen);
   * WHY: domain tag 1 = presign, and the statement Y is bound into the seed
   * derivation, so pre-signatures for different statements never share mask
   * randomness. */
  return las_presign_internal(presig, m, mlen, Y, pk, sk, pp, BOUND_PRESIGN, mask_seed);
  /* ^[CHANGED] las.c:389: return base_sign_internal(sig, m, mlen, pk, sk, pp, seed);
   * WHY: same Y + single-hop-bound difference as las_presign vs
   * base_sign. */
}

/*************************************************
* Name:        las_preverify_internal  (adaptor twin of base_verify_internal)
*
* Description: Algorithm 2 PreVerify body: base_verify_internal, except the
*              recomputed commitment is shifted by the statement before the
*              hash -- accept iff c == H(pk, w' + Y, M) -- and the norm gate
*              runs at `bound` (γ−κ−1 single-hop, γ−κ−K AMHL).
*
* Returns 0 if pre-signature could be verified correctly and -1 otherwise
**************************************************/
int las_preverify_internal(const pre_signature *presig,  /* paper σ̂: presig = (c, ẑ), pre-sig to verify */
                           const uint8_t *m,       /* paper M: message                            */
                           size_t mlen,            /* length of M (no paper symbol)               */
                           const statement *Y,        /* paper t′ := Y: statement, Y->t_prime = Y          */
                           const public_key *pk,       /* paper t: pk->t = t (public key)             */
                           const public_params *pp,       /* paper A: A = [I | A'] (A in pp = (A,H))                  */
                           int32_t bound) {        /* paper γ−κ−1 (single-hop) / γ−κ−K (AMHL)     */
  /* [PAPER Alg.2] 9:  procedure PreVerify(Y, pk, σ̂, M): */
  unsigned int i, j;             /* decls <-> base_verify_internal's, plus w_prime_plus_t_prime        */
  uint8_t t_packed[LAS_N*LAS_D*4];       /* packed pk                                       */
  uint8_t w_packed[LAS_N*LAS_D*4];       /* packed w' + Y (the hashed commitment)           */
  uint8_t c_tilde[LAS_CTILDEBYTES];  /* challenge seed                                  */
  poly c;                             /* paper c: challenge polynomial, SampleInBall(presig->c_tilde), LOCAL */
  poly c_hat;                     /* NTT copy of c                                   */
  poly z_1_hat[ELL];            /* A' half of ẑ                                    */
  poly w_prime[LAS_N];                 /* paper w′: recomputed commitment w′ = A ẑ − c t  */
  poly w_prime_plus_t_prime[LAS_N];                /* paper w′ + t′: the hashed commitment w′ + Y     */
  poly t_hat[LAS_N];              /* t in the NTT domain                             */
  keccak_state state;
  /* ^[CHANGED] (one extra declaration vs the twin)
   *     poly w_prime[LAS_N];
   * WHY: w_prime_plus_t_prime holds the shifted commitment w′ + Y of paper Alg. 2 step 15. */

  /* [PAPER Alg.2] 10:     Parse (c, ẑ) := σ̂ and t′ := Y */
  /* [PAPER Alg.2] 11:     if ||ẑ||∞ > γ − κ − 1 then */
  /* [PAPER Alg.2] 12:         return 0 */
  /* [PAPER Alg.2] 13:     end if */
  if(las_polyvecm_chknorm(presig->z_hat, bound))
    return -1;
  /* ^[CHANGED] las.c:451: if(las_polyvecm_chknorm(sig->z, BOUND_SIGN))
   * WHY: the pre-signature norm gate runs at the caller-supplied PreSign
   * bound (γ−κ−1 / γ−κ−K), matching what las_presign_internal enforced. */

  /* Compute CRH(H(rho, t1), pre, msg) */
  las_polyvecn_pack_w(t_packed, pk->t);          /* [REUSED]  las.c:456: las_polyvecn_pack_w(t_packed, pk->t); */

  /* Matrix-vector multiplication; compute Az - c_check^dt1 */
  /* [PAPER Alg.2] 14:     w′ = A ẑ − c t, where t := pk */
  las_poly_challenge(&c, presig->c_tilde);    /* c = SampleInBall(c_tilde), from the stored digest */
  c_hat = c;                                  /* NTT copy for c*t below (challenge re-derived) */

  for(j = 0; j < ELL; ++j)
    z_1_hat[j] = presig->z_hat[LAS_N + j];          /* [REUSED]  las.c:465: z_1_hat[j] = sig->z[LAS_N + j]; */
  las_polyvecl_ntt(z_1_hat);                    /* [REUSED]  las.c:466: las_polyvecl_ntt(z_1_hat); */
  las_polyvec_matrix_pointwise_montgomery(w_prime, pp->a_prime, z_1_hat);
                                             /* [REUSED]  las.c:467: las_polyvec_matrix_pointwise_montgomery(w_prime, pp->a_prime, z_1_hat); */

  poly_ntt(&c_hat);                           /* [REUSED]  las.c:470: poly_ntt(&c_hat); */
  for(j = 0; j < LAS_N; ++j)
    t_hat[j] = pk->t[j];                      /* [REUSED]  las.c:472: t_hat[j] = pk->t[j]; */
  las_polyvecn_ntt(t_hat);                    /* [REUSED]  las.c:473: las_polyvecn_ntt(t_hat); */
  las_polyvecn_pointwise_poly_montgomery(t_hat, &c_hat, t_hat);
                                             /* [REUSED]  las.c:474: las_polyvecn_pointwise_poly_montgomery(t_hat, &c_hat, t_hat); */

  las_polyvecn_reduce(w_prime);                    /* [REUSED]  las.c:477: las_polyvecn_reduce(w_prime); */
  las_polyvecn_invntt_tomont(w_prime);             /* [REUSED]  las.c:478: las_polyvecn_invntt_tomont(w_prime); */
  las_polyvecn_add(w_prime, w_prime, presig->z_hat);         /* [REUSED]  las.c:479: las_polyvecn_add(w_prime, w_prime, sig->z);
                                              * (identity block: w += ẑ_top)           */
  las_polyvecn_invntt_tomont(t_hat);          /* [REUSED]  las.c:481: las_polyvecn_invntt_tomont(t_hat); */
  las_polyvecn_sub(w_prime, w_prime, t_hat);              /* [REUSED]  las.c:482: las_polyvecn_sub(w_prime, w_prime, t_hat); */
  las_polyvecn_reduce(w_prime);                    /* [REUSED]  las.c:477: las_polyvecn_reduce(w_prime); */

  /* Reconstruct w1 */
  las_polyvecn_caddq(w_prime);                     /* [REUSED]  las.c:486: las_polyvecn_caddq(w_prime); */
  las_polyvecn_add(w_prime_plus_t_prime, w_prime, Y->t_prime);
  las_polyvecn_reduce(w_prime_plus_t_prime);
  las_polyvecn_caddq(w_prime_plus_t_prime);
  /* ^[CHANGED] (no base_verify_internal lines)
   * WHY: the statement is folded back in before the hash -- PreVerify
   * checks c against H(pk, w′ + Y, M), mirroring what las_presign_internal
   * hashed (paper Alg. 2 step 15). */
  las_polyvecn_pack_w(w_packed, w_prime_plus_t_prime);
  /* ^[CHANGED] las.c:487: las_polyvecn_pack_w(w_packed, w_prime);
   * WHY: the oracle input is the shifted commitment w′ + Y, not w′. */

  /* Call random oracle and verify challenge */
  shake256_init(&state);                     /* [REUSED]  las.c:490: shake256_init(&state); */
  shake256_absorb(&state, t_packed, sizeof t_packed);
                                             /* [REUSED]  las.c:491: shake256_absorb(&state, t_packed, sizeof t_packed); */
  shake256_absorb(&state, w_packed, sizeof w_packed);
                                             /* [REUSED]  las.c:493: shake256_absorb(&state, w_packed, sizeof w_packed); */
  shake256_absorb(&state, m, mlen);          /* [REUSED]  las.c:495: shake256_absorb(&state, m, mlen); */
  shake256_finalize(&state);                 /* [REUSED]  las.c:496: shake256_finalize(&state); */
  shake256_squeeze(c_tilde, LAS_CTILDEBYTES, &state);
                                             /* [REUSED]  las.c:497: shake256_squeeze(c_tilde, LAS_SEEDBYTES, &state); */
  /* [PAPER Alg.2] 15:     if c ≠ H(pk, w′ + t′, M) then */
  /* [PAPER Alg.2] 16:         return 0 */
  /* [PAPER Alg.2] 17:     end if */
  /* accept iff the recomputed DIGEST equals the stored one: the upstream
   * byte-compare loop over the challenge digest (the refactor's polynomial
   * compare is gone; strictly stronger, identical accept/reject on honest paths). */
  for(i = 0; i < LAS_CTILDEBYTES; ++i)
    if(c_tilde[i] != presig->c_tilde[i])
      return -1;                             /* [REUSED]  sign.c:353-355 (CTILDEBYTES bytes) */

  /* [PAPER Alg.2] 18:     return 1 */
  return 0;                                  /* [REUSED]  las.c:555: return 0; */
  /* [PAPER Alg.2] 19: end procedure */
}

/*************************************************
* Name:        las_preverify  (adaptor twin of base_verify)
*
* Description: Algorithm 2 PreVerify(Y, pk, σ̂, M), public entry point at the
*              single-hop bound γ−κ−1.
*
* Returns 0 if pre-signature could be verified correctly and -1 otherwise
**************************************************/
int las_preverify(const pre_signature *presig,  /* paper σ̂: presig = (c, ẑ), pre-sig to verify */
                  const uint8_t *m,       /* paper M: message                            */
                  size_t mlen,            /* length of M (no paper symbol)               */
                  const statement *Y,        /* paper t′ := Y: statement, Y->t_prime = Y          */
                  const public_key *pk,       /* paper t: pk->t = t (public key)             */
                  const public_params *pp) {     /* paper A: A = [I | A'] (A in pp = (A,H))                  */
  return las_preverify_internal(presig, m, mlen, Y, pk, pp, BOUND_PRESIGN);
  /* ^[CHANGED] las.c:524: return base_verify_internal(sig, m, mlen, pk, pp);
   * WHY: the adaptor internal additionally takes the statement Y and the
   * single-hop PreSign bound γ−κ−1 (BOUND_PRESIGN). */
}

/*************************************************
* Name:        las_presign_k  (AMHL K-hop PreSign; adaptor twin of las_presign)
*
* Description: Identical to las_presign but rejects at the tighter bound
*              γ−κ−K (BOUND_PRESIGN_K), reserving norm budget K for the
*              cumulative witness (eprint 2020/845 Fig. 2 / Section 5).
*
* Returns 0 (success)
**************************************************/
int las_presign_k(pre_signature *presig,    /* paper σ̂: output pre-signature σ̂ = (c, ẑ)   */
                  const uint8_t *m,   /* paper M: message                           */
                  size_t mlen,        /* length of M (no paper symbol)              */
                  const statement *Y,    /* paper t′ := Y: (cumulative) statement       */
                  const public_key *pk,   /* paper t: pk->t = t (public key)            */
                  const secret_key *sk,   /* paper r: sk->r = r (secret key)            */
                  const public_params *pp,   /* paper A: A = [I | A'] (A in pp = (A,H))                 */
                  unsigned int nhops) {  /* paper K: number of AMHL hops (tighter bound γ−κ−K) */
  uint8_t mask_seed[64];      /* [REUSED]  las.c:733: uint8_t mask_seed[64]; */

  randombytes(mask_seed, 64);                     /* [REUSED]  las.c:711: randombytes(mask_seed, 64); */
  return las_presign_internal(presig, m, mlen, Y, pk, sk, pp, BOUND_PRESIGN_K(nhops), mask_seed);
  /* ^[CHANGED] las.c:740: return las_presign_internal(presig, m, mlen, Y, pk, sk, pp, BOUND_PRESIGN, mask_seed);
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
int las_preverify_k(const pre_signature *presig,  /* paper σ̂: presig = (c, ẑ), pre-sig to verify */
                    const uint8_t *m,       /* paper M: message                            */
                    size_t mlen,            /* length of M (no paper symbol)               */
                    const statement *Y,        /* paper t′ := Y: (cumulative) statement       */
                    const public_key *pk,       /* paper t: pk->t = t (public key)             */
                    const public_params *pp,       /* paper A: A = [I | A'] (A in pp = (A,H))                  */
                    unsigned int nhops) {   /* paper K: number of AMHL hops (bound γ−κ−K)  */
  return las_preverify_internal(presig, m, mlen, Y, pk, pp, BOUND_PRESIGN_K(nhops));
  /* ^[CHANGED] las.c:867: return las_preverify_internal(presig, m, mlen, Y, pk, pp, BOUND_PRESIGN);
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
int las_adapt(signature *sig,          /* paper σ: output adapted signature σ = (c, ẑ + r′) */
              const pre_signature *presig, /* paper σ̂: presig = (c, ẑ)                          */
              const uint8_t *m,      /* paper M: message                                  */
              size_t mlen,           /* length of M (no paper symbol)                     */
              const statement *Y,       /* paper t′ := Y: statement                          */
              const witness *r_prime,/* paper r' (honest witness), r' := y: r_prime->value = r' (A r' = Y) */
              const public_key *pk,      /* paper t: pk->t = t (public key)                   */
              const public_params *pp) {    /* paper A: A = [I | A'] (A in pp = (A,H))                        */
  /* [PAPER Alg.2] 20: procedure Adapt((Y, y), pk, σ̂, M): */

  /* [PAPER Alg.2] 21:     if PreVerify(Y, pk, σ̂, M) = 0 then */
  /* [PAPER Alg.2] 22:         return ⊥ */
  /* [PAPER Alg.2] 23:     end if */
  if(las_preverify(presig, m, mlen, Y, pk, pp))
    return -1;

  /* [PAPER Alg.2] 24:     Parse (c, ẑ) := σ̂ and r′ := y */
  /* [PAPER Alg.2] 25:     return σ = (c, ẑ + r′) */
  memcpy(sig->c_tilde, presig->c_tilde, LAS_CTILDEBYTES);  /* Adapt preserves the challenge digest */
  las_polyvecm_add(sig->z, presig->z_hat, r_prime->value);
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
*              base_keygen_seed does (quoted below).
*
* Returns 0 (and the witness in y) on success, -1 otherwise
**************************************************/
int las_ext(witness *s,              /* paper s: extracted witness (s->value = s); reserved for Ext */
            const signature *sig,      /* paper σ: sig = (c, z)                            */
            const pre_signature *presig,   /* paper σ̂: presig = (ĉ, ẑ)                         */
            const statement *Y,         /* paper t′ := Y: statement                         */
            const public_params *pp) {      /* paper A: A = [I | A'] (A in pp = (A,H))                       */
  /* [PAPER Alg.2] 27: procedure Ext(Y, σ, σ̂): */
  unsigned int i, j;   /* row / coefficient indices (no paper symbol)            */
  poly s_1_hat[ELL]; /* NTT buffer, A' half of s <-> s_1_hat (base_keygen_seed)  */
  poly a_s[LAS_N];      /* paper A s: the product A s, checked against t′ = Y     */

  /* [PAPER Alg.2] 28:     Parse (c, z) := σ and (ĉ, ẑ) := σ̂ */
  /* [PAPER Alg.2] 29:     Parse t′ := Y */
  /* [PAPER Alg.2] 30:     s = z − ẑ */
  las_polyvecm_sub(s->value, sig->z, presig->z_hat);
  las_polyvecm_reduce(s->value);

  /* [PAPER Alg.2] 31:     if t′ ≠ A s, then return ⊥ */
  for(j = 0; j < ELL; ++j)
    s_1_hat[j] = s->value[LAS_N + j];              /* [REUSED]  las.c:213: s_1_hat[j] = sk->r[LAS_N + j];
                                              * (sk -> the extracted witness y)        */
  las_polyvecl_ntt(s_1_hat);                   /* [REUSED]  las.c:216: las_polyvecl_ntt(s_1_hat); */
  las_polyvec_matrix_pointwise_montgomery(a_s, pp->a_prime, s_1_hat);
                                             /* [REUSED]  las.c:217: las_polyvec_matrix_pointwise_montgomery(pk->t, pp->a_prime, s_1_hat); */
  las_polyvecn_reduce(a_s);                   /* [REUSED]  las.c:226: las_polyvecn_reduce(pk->t); */
  las_polyvecn_invntt_tomont(a_s);            /* [REUSED]  las.c:220: las_polyvecn_invntt_tomont(pk->t); */
  las_polyvecn_add(a_s, a_s, s->value);            /* [REUSED]  las.c:223: las_polyvecn_add(pk->t, pk->t, sk->r); */
  las_polyvecn_reduce(a_s);                   /* [REUSED]  las.c:226: las_polyvecn_reduce(pk->t); */
  las_polyvecn_caddq(a_s);                    /* [REUSED]  las.c:229: las_polyvecn_caddq(pk->t); */
  for(i = 0; i < LAS_N; ++i)
    for(j = 0; j < LAS_D; ++j)
      if(a_s[i].coeffs[j] != Y->t_prime[i].coeffs[j])
        return -1;                           /* coefficient compare, as in base_verify_internal's
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



/* <-> b_rej_Sgamma (basesig.c:786): verbatim copy; upstream twin
 * rej_uniform (poly.c:309) with window 2*GAMMA+1, see basesig.c:770-784. */
static unsigned int las_rej_Sgamma(int32_t *a, unsigned int len,
                                   const uint8_t *buf, unsigned int buflen) {
  unsigned int ctr, pos;
  uint32_t t, gmask;

  gmask = 1;                                 /* smallest 2^k - 1 >= 2*GAMMA (0x7FFFFF there) */
  while(gmask < 2u*(uint32_t)GAMMA)
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

    if(t < 2u*(uint32_t)GAMMA + 1u)
      a[ctr++] = (int32_t)t - GAMMA;
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

  ctr = las_rej_Sgamma(a->coeffs, LAS_D, buf, SHAKE256_RATE);

  while(ctr < LAS_D) {
    shake256_squeezeblocks(buf, 1, &state);
    ctr += las_rej_Sgamma(a->coeffs + ctr, LAS_D - ctr, buf, SHAKE256_RATE);
  }
}

/* <-> b_poly_challenge (basesig.c:860): verbatim copy; upstream twin
 * poly_challenge (poly.c:489) with TAU -> KAPPA and a fixed 32-byte
 * seed, see basesig.c:849-859 for the WHY. */
static void las_poly_challenge(poly *c, const uint8_t seed[LAS_CTILDEBYTES]) {
  unsigned int i, b, pos;
  uint64_t signs;
  uint8_t buf[SHAKE256_RATE];
  keccak_state state;

  shake256_init(&state);
  shake256_absorb(&state, seed, LAS_CTILDEBYTES);
  shake256_finalize(&state);
  shake256_squeezeblocks(buf, 1, &state);

  signs = 0;
  for(i = 0; i < 8; ++i)
    signs |= (uint64_t)buf[i] << 8*i;
  pos = 8;

  for(i = 0; i < LAS_D; ++i)
    c->coeffs[i] = 0;
  for(i = LAS_D - KAPPA; i < LAS_D; ++i) {
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
  for(i = 0; i < LAS_D; ++i) {
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
static void las_polyvec_matrix_pointwise_montgomery(poly t[LAS_N], const poly mat[LAS_N][ELL],
                                                    const poly v[ELL]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    las_polyvecl_pointwise_acc_montgomery(&t[i], mat[i], v);
}

/* <-> b_polyvecl_pointwise_acc_montgomery (basesig.c:936) <->
 * polyvecl_pointwise_acc_montgomery (polyvec.c:113), L -> l. */
static void las_polyvecl_pointwise_acc_montgomery(poly *w, const poly u[ELL],
                                                  const poly v[ELL]) {
  unsigned int i;
  poly t;

  poly_pointwise_montgomery(w, &u[0], &v[0]);
  for(i = 1; i < ELL; ++i) {
    poly_pointwise_montgomery(&t, &u[i], &v[i]);
    poly_add(w, w, &t);
  }
}

/* <-> b_polyvecl_ntt (basesig.c:949) <-> polyvecl_ntt (polyvec.c:81),
 * over the l columns of A'. */
static void las_polyvecl_ntt(poly v[ELL]) {
  unsigned int i;

  for(i = 0; i < ELL; ++i)
    poly_ntt(&v[i]);
}


/* <-> b_polyvecm_uniform_Sgamma (basesig.c:967) <-> polyvecl_uniform_gamma1
 * (polyvec.c:42): same L*nonce + i derivation (m*nonce + i here), so one
 * nonce++ per signing attempt at the call site. */
static void las_polyvecm_uniform_Sgamma(poly v[N_PLUS_ELL], const uint8_t seed[64], uint16_t nonce) {
  unsigned int i;

  for(i = 0; i < N_PLUS_ELL; ++i)
    las_poly_uniform_Sgamma(&v[i], seed, (uint16_t)(N_PLUS_ELL*nonce + i));
}

/* <-> b_polyvecm_reduce (basesig.c:975) <-> polyvecl_reduce (polyvec.c:49). */
static void las_polyvecm_reduce(poly v[N_PLUS_ELL]) {
  unsigned int i;

  for(i = 0; i < N_PLUS_ELL; ++i)
    poly_reduce(&v[i]);
}

/* <-> b_polyvecm_add (basesig.c:983) <-> polyvecl_add (polyvec.c:66). */
static void las_polyvecm_add(poly w[N_PLUS_ELL], const poly u[N_PLUS_ELL], const poly v[N_PLUS_ELL]) {
  unsigned int i;

  for(i = 0; i < N_PLUS_ELL; ++i)
    poly_add(&w[i], &u[i], &v[i]);
}

/* <-> polyveck_sub (polyvec.c:218), K -> m: the same upstream twin as
 * b_polyvecn_sub (basesig.c:1050) at width m.  No basesig.c body: only
 * Ext's s = z - z^ subtracts an m-vector, and the base scheme has no Ext. */
static void las_polyvecm_sub(poly w[N_PLUS_ELL], const poly u[N_PLUS_ELL], const poly v[N_PLUS_ELL]) {
  unsigned int i;

  for(i = 0; i < N_PLUS_ELL; ++i)
    poly_sub(&w[i], &u[i], &v[i]);
}

/* <-> b_polyvecm_ntt (basesig.c:991) <-> polyvecl_ntt (polyvec.c:81),
 * L -> m (the full secret/response vector). */
static void las_polyvecm_ntt(poly v[N_PLUS_ELL]) {
  unsigned int i;

  for(i = 0; i < N_PLUS_ELL; ++i)
    poly_ntt(&v[i]);
}

/* <-> b_polyvecm_invntt_tomont (basesig.c:999) <-> polyvecl_invntt_tomont
 * (polyvec.c:88). */
static void las_polyvecm_invntt_tomont(poly v[N_PLUS_ELL]) {
  unsigned int i;

  for(i = 0; i < N_PLUS_ELL; ++i)
    poly_invntt_tomont(&v[i]);
}

/* <-> b_polyvecm_pointwise_poly_montgomery (basesig.c:1007) <->
 * polyvecl_pointwise_poly_montgomery (polyvec.c:95). */
static void las_polyvecm_pointwise_poly_montgomery(poly r[N_PLUS_ELL], const poly *a, const poly v[N_PLUS_ELL]) {
  unsigned int i;

  for(i = 0; i < N_PLUS_ELL; ++i)
    poly_pointwise_montgomery(&r[i], a, &v[i]);
}

/* <-> b_polyvecm_chknorm (basesig.c:1015) <-> polyvecl_chknorm
 * (polyvec.c:139); poly_chknorm itself is REUSED. */
static int las_polyvecm_chknorm(const poly v[N_PLUS_ELL], int32_t bound) {
  unsigned int i;

  for(i = 0; i < N_PLUS_ELL; ++i)
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
static void las_polyvecn_pack_w(uint8_t r[LAS_N*LAS_D*4], const poly w[LAS_N]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    las_polyw_pack(&r[i*LAS_D*4], &w[i]);
}

/* Deterministic per-(pre)signature mask randomness: seed = SHAKE256(tag, sk,
 * [Y], M).  Makes (pre)signing a deterministic function of its inputs --
 * reproducible KATs and no fresh per-signature randomness to mishandle
 * (nonce-reuse safety).  LAS-only helper (the _det KAT path); no
 * basesig.c/upstream analogue.  The polynomial packer it reuses for Y is
 * las_polyw_pack (the b_polyw_pack twin above). */
static void det_seed(uint8_t out[64], uint8_t tag, const secret_key *sk,
                     const statement *Y, const uint8_t *m, size_t mlen) {
  keccak_state state;
  uint8_t skb[N_PLUS_ELL * LAS_D];
  uint8_t buf[LAS_D * 4];
  unsigned int i, k;

  for(i = 0; i < N_PLUS_ELL; ++i)                      /* ternary sk -> 1 byte/coeff */
    for(k = 0; k < LAS_D; ++k)
      skb[i * LAS_D + k] = (uint8_t)(int8_t)sk->r[i].coeffs[k];

  shake256_init(&state);
  shake256_absorb(&state, &tag, 1);              /* domain: 0=sign, 1=presign  */
  shake256_absorb(&state, skb, sizeof skb);
  if(Y)
    for(i = 0; i < LAS_N; ++i) {                  /* bind the statement Y       */
      las_polyw_pack(buf, &Y->t_prime[i]);
      shake256_absorb(&state, buf, LAS_D * 4);
    }
  shake256_absorb(&state, m, mlen);
  shake256_finalize(&state);
  shake256_squeeze(out, 64, &state);
}

/* ============== end-to-end PACKED-API tier (bytes in/out) ==============
 * The SECOND measured boundary (validating unpack -> core -> pack, all inside
 * the call), mirroring basesig.c's packed tier.  The Algorithm-1 packed twins
 * (KeyGen/Sign/Verify) live in basesig.c; these four are the Algorithm-2
 * adaptor operations at the byte boundary.  Each object decodes with ITS OWN
 * typed codec (statement / witness / pre_signature / signature / public_key /
 * secret_key), so the semantic types stay distinct on the wire.  One codec
 * ref/serialize.{c,h} serves both schemes. */

/*************************************************
* Name:        las_presign_packed  (PreSign at the byte boundary)
*
* Description: validating decode of the statement Y, public key and secret key,
*              then core PreSign (single-hop bound; c = H(pk, w + Y, M)), then
*              pack the pre-signature.
*
* Returns 0 (success), -1 if a key or the statement fails validating decode
**************************************************/
int las_presign_packed(uint8_t presig_b[PRE_SIGNATURE_BYTES], /* packed pre-signature (bytes out) */
                       const uint8_t *m,                      /* paper M: message                 */
                       size_t mlen,                           /* length of M                      */
                       const uint8_t Y_b[STATEMENT_BYTES],    /* packed statement (bytes)         */
                       const uint8_t pk_b[PUBLIC_KEY_BYTES],  /* packed public key (bytes)        */
                       const uint8_t sk_b[SECRET_KEY_BYTES],  /* packed secret key (bytes)        */
                       const public_params *pp) {             /* paper A: A = [I | A'] (A in pp = (A,H)) */
  statement Y;
  public_key pk;
  secret_key sk;
  pre_signature presig;

  if(unpack_statement(&Y, Y_b))
    return -1;
  if(unpack_public_key(&pk, pk_b))
    return -1;
  if(unpack_secret_key(&sk, sk_b))
    return -1;
  las_presign(&presig, m, mlen, &Y, &pk, &sk, pp);
  return pack_pre_signature(presig_b, &presig);
}

/*************************************************
* Name:        las_preverify_packed  (PreVerify at the byte boundary)
*
* Description: validating decode of the statement Y, public key and
*              pre-signature, then core PreVerify (single-hop bound; checks
*              c == H(pk, w' + Y, M)).
*
* Returns 0 if the pre-signature verifies, -1 on decode failure or mismatch
**************************************************/
int las_preverify_packed(const uint8_t presig_b[PRE_SIGNATURE_BYTES], /* packed pre-signature (bytes) */
                         const uint8_t *m,                            /* paper M: message             */
                         size_t mlen,                                 /* length of M                  */
                         const uint8_t Y_b[STATEMENT_BYTES],          /* packed statement (bytes)     */
                         const uint8_t pk_b[PUBLIC_KEY_BYTES],        /* packed public key (bytes)    */
                         const public_params *pp) {                   /* paper A: A = [I | A'] (A in pp = (A,H)) */
  statement Y;
  public_key pk;
  pre_signature presig;

  if(unpack_statement(&Y, Y_b))
    return -1;
  if(unpack_public_key(&pk, pk_b))
    return -1;
  if(unpack_pre_signature(&presig, presig_b))
    return -1;
  return las_preverify(&presig, m, mlen, &Y, &pk, pp);
}

/*************************************************
* Name:        las_adapt_packed  (Adapt at the byte boundary)
*
* Description: validating decode of the pre-signature, statement, honest
*              witness r' (ternary codec) and public key; core Adapt (which
*              PreVerifies); pack the adapted -- fully ordinary -- signature.
*
* Returns 0 on success, -1 on any decode failure or invalid pre-signature
**************************************************/
int las_adapt_packed(uint8_t sig_b[SIGNATURE_BYTES],             /* packed adapted signature (bytes out) */
                     const uint8_t presig_b[PRE_SIGNATURE_BYTES],/* packed pre-signature (bytes)        */
                     const uint8_t *m,                           /* paper M: message                    */
                     size_t mlen,                                /* length of M                         */
                     const uint8_t Y_b[STATEMENT_BYTES],         /* packed statement (bytes)            */
                     const uint8_t r_prime_b[WITNESS_BYTES],     /* packed honest witness r' (bytes)    */
                     const uint8_t pk_b[PUBLIC_KEY_BYTES],       /* packed public key (bytes)           */
                     const public_params *pp) {                  /* paper A: A = [I | A'] (A in pp = (A,H)) */
  statement Y;
  witness r_prime;
  public_key pk;
  pre_signature presig;
  signature sig;

  if(unpack_statement(&Y, Y_b))
    return -1;
  if(unpack_witness(&r_prime, r_prime_b))
    return -1;
  if(unpack_public_key(&pk, pk_b))
    return -1;
  if(unpack_pre_signature(&presig, presig_b))
    return -1;
  if(las_adapt(&sig, &presig, m, mlen, &Y, &r_prime, &pk, pp))
    return -1;
  return pack_signature(sig_b, &sig);
}

/*************************************************
* Name:        las_ext_packed  (Ext at the byte boundary)
*
* Description: validating decode of both signatures and the statement; core
*              Ext (s = z - z_hat, checked against A s == Y); pack the recovered
*              witness s with the ternary codec.  This is the on-chain leak made
*              byte-real: the two byte strings anyone can fetch from the chain
*              yield the witness s.
*
* Returns 0 (and the packed witness s) on success, -1 otherwise
**************************************************/
int las_ext_packed(uint8_t s_b[WITNESS_BYTES],                /* packed extracted witness s (bytes out) */
                   const uint8_t sig_b[SIGNATURE_BYTES],      /* packed adapted signature (bytes)       */
                   const uint8_t presig_b[PRE_SIGNATURE_BYTES],/* packed pre-signature (bytes)          */
                   const uint8_t Y_b[STATEMENT_BYTES],        /* packed statement (bytes)               */
                   const public_params *pp) {                 /* paper A: A = [I | A'] (A in pp = (A,H)) */
  statement Y;
  witness s;
  signature sig;
  pre_signature presig;

  if(unpack_statement(&Y, Y_b))
    return -1;
  if(unpack_signature(&sig, sig_b))
    return -1;
  if(unpack_pre_signature(&presig, presig_b))
    return -1;
  if(las_ext(&s, &sig, &presig, &Y, pp))
    return -1;
  return pack_witness(s_b, &s);
}
