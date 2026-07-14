/*
 * basesig.c -- simplified Dilithium-style BASE signature = Algorithm 1 of
 * eprint 2020/845, written as a STRUCTURAL MIRROR of the upstream ML-DSA
 * reference ref/sign.c: SAME function count, SAME order, SAME return
 * convention (int, 0 = success), SAME inline composition (the SHAKE
 * challenge-hash block, the matrix-vector sequence and the `rej:` loop are
 * written out in the scheme functions exactly where sign.c writes them),
 * names by the uniform prefix swap crypto_sign* -> base_sign*:
 *
 *   base_keygen                   <->  crypto_sign_keypair            (sign.c:23)
 *   base_keygen_seed               -   deterministic KeyGen body (KAT slot; no sign.c slot)
 *   base_sign_internal            <->  crypto_sign_signature_internal (sign.c:85)
 *   base_sign                     <->  crypto_sign_signature          (sign.c:206)
 *   base_sign_det                  -   deterministic Sign (KAT slot; no sign.c slot)
 *   base_verify_internal          <->  crypto_sign_verify_internal    (sign.c:289)
 *   base_verify                   <->  crypto_sign_verify             (sign.c:375)
 *   (base_sign/base_sign_open sm-wrappers dropped: struct signatures, no byte prefix)
 *
 * ANNOTATION CONVENTION (read side by side with sign.c):
 *   [REUSED]  sign.c:<line>: <upstream code>   -- same call, dimensions swapped
 *   [CHANGED] quotes the upstream line(s) verbatim, then states WHY the line
 *             differs here;
 *   [DELETED] quotes the dropped upstream line(s) verbatim, then states WHY
 *             Algorithm 1 does not need them.
 * Upstream's own section comments ("Sample intermediate vector y", ...) are
 * kept verbatim so the two files scroll in lockstep.
 *
 * NO INVENTED HELPERS: every local helper at the bottom of this file is a
 * one-to-one twin of a NAMED upstream poly.c/polyvec.c function with the
 * same body structure -- only dimensions (K,L -> n, l, m=n+l), distributions
 * (eta, gamma1 -> S_1, S_gamma) and weights (tau -> kappa) change.
 *
 * Standalone: depends only on the SHARED layer setup.h (parameters, types,
 * BOUND_SIGN) + serialize.h (packed tier) -- NOT on las.h.  las.c
 * is NOT linked -- the helpers are local copies, behaviourally IDENTICAL to
 * las.c's, so A*r and the challenge hash H(pk,w,M) match las.c bit-for-bit
 * and a LAS-adapted signature verifies here.  ref/las.c mirrors THIS file in
 * turn, so the chain sign.c <-> basesig.c <-> las.c reads pairwise side by
 * side.
 */
#include <stdint.h>
#include <string.h>
#include "params.h"
#include "basesig.h"    /* <-> "sign.h"; polyvec.h/symmetric.h [DELETED]:
                         * vectors are plain poly arrays and the SHAKE stream
                         * calls are written out with fips202.h */
#include "serialize.h"  /* [REUSED] sign.c:4: #include "packing.h"
                         * WHY: the end-to-end PACKED-API tier at the BOTTOM of
                         * this file unpacks/packs inside the call, exactly as
                         * sign.c does; the core (struct) tier stays byte-free */
#include "poly.h"
#include "randombytes.h"
#include "fips202.h"

/* Rejection-sampling attempt counter (measurement only; see basesig.h).
 * No sign.c analogue -- instrumentation added for the benchmark. */
unsigned long base_attempts = 0;

/* ---- local helpers, DEFINED AT THE BOTTOM of this file.  Each is a twin of
 * exactly ONE upstream function (same body structure):
 *
 *   poly.c twins:
 *   b_rej_S1               <->  rej_eta                (poly.c:384)  2-bit S_1 codes
 *   b_poly_uniform_S1      <->  poly_uniform_eta       (poly.c:435)  32-byte seed
 *   b_rej_Sgamma           <->  rej_uniform            (poly.c:309)  window 2*GAMMA+1
 *   b_poly_uniform_Sgamma  <->  poly_uniform_gamma1    (poly.c:467)  rejection, not unpack
 *   b_poly_challenge       <->  poly_challenge         (poly.c:489)  kappa, 32-byte seed
 *   b_polyw_pack           <->  polyw1_pack            (poly.c:888)  full w, 4B/coeff
 *
 *   polyvec.c twins (naming: b_polyvecl_* = the l columns of A';
 *   b_polyvecm_* = the m = n+l secret/response vectors <-> polyvecl_*;
 *   b_polyvecn_* = the n rows/commitments <-> polyveck_*):
 *   b_polyvec_matrix_pointwise_montgomery <-> polyvec_matrix_pointwise_montgomery (polyvec.c:24)
 *   b_polyvecl_pointwise_acc_montgomery   <-> polyvecl_pointwise_acc_montgomery   (polyvec.c:113)
 *   b_polyvecl_ntt                        <-> polyvecl_ntt                        (polyvec.c:81)
 *   b_polyvecm_uniform_S1                 <-> polyvecl_uniform_eta                (polyvec.c:35)
 *   b_polyvecm_uniform_Sgamma             <-> polyvecl_uniform_gamma1             (polyvec.c:42)
 *   b_polyvecm_reduce/add/ntt/invntt_tomont/pointwise_poly_montgomery/chknorm
 *                                         <-> polyvecl_{same}          (polyvec.c:49-147)
 *   b_polyvecn_reduce/caddq/add/sub/ntt/invntt_tomont/pointwise_poly_montgomery
 *                                         <-> polyveck_{same}          (polyvec.c:168-276)
 *   b_polyvecn_pack_w                     <-> polyveck_pack_w1         (polyvec.c:384) */
static unsigned int b_rej_S1(int32_t *a, unsigned int len, const uint8_t *buf, unsigned int buflen);
static void b_poly_uniform_S1(poly *a, const uint8_t seed[LAS_SEEDBYTES], uint16_t nonce);
static unsigned int b_rej_Sgamma(int32_t *a, unsigned int len, const uint8_t *buf, unsigned int buflen);
static void b_poly_uniform_Sgamma(poly *a, const uint8_t seed[64], uint16_t nonce);
static void b_poly_challenge(poly *c, const uint8_t seed[LAS_SEEDBYTES]);
static void b_polyw_pack(uint8_t *r, const poly *a);
static void b_polyvec_matrix_pointwise_montgomery(poly t[LAS_N], const poly mat[LAS_N][ELL],
                                                  const poly v[ELL]);
static void b_polyvecl_pointwise_acc_montgomery(poly *w, const poly u[ELL],
                                                const poly v[ELL]);
static void b_polyvecl_ntt(poly v[ELL]);
static void b_polyvecm_uniform_S1(poly v[N_PLUS_ELL], const uint8_t seed[LAS_SEEDBYTES], uint16_t nonce);
static void b_polyvecm_uniform_Sgamma(poly v[N_PLUS_ELL], const uint8_t seed[64], uint16_t nonce);
static void b_polyvecm_reduce(poly v[N_PLUS_ELL]);
static void b_polyvecm_add(poly w[N_PLUS_ELL], const poly u[N_PLUS_ELL], const poly v[N_PLUS_ELL]);
static void b_polyvecm_ntt(poly v[N_PLUS_ELL]);
static void b_polyvecm_invntt_tomont(poly v[N_PLUS_ELL]);
static void b_polyvecm_pointwise_poly_montgomery(poly r[N_PLUS_ELL], const poly *a, const poly v[N_PLUS_ELL]);
static int  b_polyvecm_chknorm(const poly v[N_PLUS_ELL], int32_t bound);
static void b_polyvecn_reduce(poly v[LAS_N]);
static void b_polyvecn_caddq(poly v[LAS_N]);
static void b_polyvecn_add(poly w[LAS_N], const poly u[LAS_N], const poly v[LAS_N]);
static void b_polyvecn_sub(poly w[LAS_N], const poly u[LAS_N], const poly v[LAS_N]);
static void b_polyvecn_ntt(poly v[LAS_N]);
static void b_polyvecn_invntt_tomont(poly v[LAS_N]);
static void b_polyvecn_pointwise_poly_montgomery(poly r[LAS_N], const poly *a, const poly v[LAS_N]);
static void b_polyvecn_pack_w(uint8_t r[LAS_N*LAS_D*4], const poly w[LAS_N]);
static void det_seed(uint8_t out[64], const secret_key *sk, const uint8_t *m, size_t mlen);

/*************************************************
* Name:        base_keygen  <->  crypto_sign_keypair (sign.c:23)
*
* Description: Algorithm 1 KeyGen, random path: draw a fresh seed, then the
*              deterministic body base_keygen_seed.  Gen (relation.c) uses the
*              same deterministic sampling and arithmetic, but is a DISTINCT
*              algorithm returning a (statement, witness) pair -- not a key pair.
*
* Returns 0 (success)
**************************************************/
int base_keygen(public_key *pk,            /* paper t: pk->t = t = A r (public key)              */
                secret_key *sk,            /* paper r: sk->r = r  (secret key, r <-$ S_1)        */
                const public_params *pp) { /* paper A (in pp = (A, H)): A = [I | A'] */
  uint8_t seed[LAS_SEEDBYTES];
  randombytes(seed, LAS_SEEDBYTES);
  return base_keygen_seed(pk, sk, pp, seed);
  /* body factored out so the KAT slot (test_kat.c) can inject a 32-byte seed;
   * sign.c keeps the body inline. */
}

/*************************************************
* Name:        base_keygen_seed  (deterministic KAT slot; body of base_keygen)
*
* Description: Algorithm 1 KeyGen from an explicit 32-byte seed:
*              r <- S_1^{n+ell}; t = A r; (pk,sk) = (t,r).  Reproducible KAT
*              vectors; no sign.c slot.
*
* Returns 0 (success)
**************************************************/
int base_keygen_seed(public_key *pk,            /* paper t: pk->t = t = A r (public key)              */
                     secret_key *sk,            /* paper r: sk->r = r  (secret key, r <-$ S_1)        */
                     const public_params *pp,   /* paper A (in pp = (A, H)): A = [I | A'] */
                     const uint8_t seed[LAS_SEEDBYTES]) {  /* PRG seed to sample r (no paper symbol) */
  /* [PAPER Alg.1] 1:  procedure KeyGen() */
  unsigned int j;
  poly r_1_hat[ELL];             /* NTT rep of r_1 (final ell components of r); <-> s1hat (sign.c:28) */

  /* [DELETED] sign.c:33-38:
   *     seedbuf[SEEDBYTES+0] = K;  seedbuf[SEEDBYTES+1] = L;
   *     shake256(seedbuf, 2*SEEDBYTES + CRHBYTES, seedbuf, SEEDBYTES+2);
   *     rho = seedbuf;  rhoprime = rho + SEEDBYTES;  key = rhoprime + CRHBYTES;
   * WHY: upstream splits the seed into rho (to re-derive A from the packed
   * key), rhoprime (sampler seed) and key (signing PRF key) because its keys
   * are byte-packed and self-contained.  Here keys stay structs, A lives in
   * pp, and there is no signing PRF key -- the raw 32-byte seed IS the
   * sampler seed. */

  /* Expand matrix */
  /* [DELETED] sign.c:41:
   *     polyvec_matrix_expand(mat, rho);
   * WHY: A = [I | A'] is a system-wide public parameter, expanded ONCE in
   * ref/setup.c setup_public_params and passed in as pp; upstream must
   * re-expand A from rho on every call because rho travels inside each packed
   * key. */

  /* Sample the ternary secret r */
  /* [PAPER Alg.1] 2:      r <-$ S_1^(n+ell) */
  b_polyvecm_uniform_S1(sk->r, seed, 0);
  /* ^[CHANGED] sign.c:44-45:
   *     polyvecl_uniform_eta(&s1, rhoprime, 0);
   *     polyveck_uniform_eta(&s2, rhoprime, L);
   * WHY: the paper's secret is ONE ternary vector r <- S_1^{n+ell} with
   * ||r||inf <= 1 (Table 1) -- this is what caps ||c*r||inf <= kappa (Fact 1)
   * and so fixes the rejection bound gamma-kappa.  There is NO separate error
   * vector s2: with A = [I | A'] in Hermite normal form the identity block
   * makes the top n components of r play s2's role.  eta-coded half-bytes
   * cannot sample {-1,0,1} tightly, so the twin sampler uses 2-bit codes. */

  /* Matrix-vector multiplication */
  /* [PAPER Alg.1] 3:      t = A r = r_0 + A' r_1  (A = [I | A']) */
  for(j = 0; j < ELL; ++j)
    r_1_hat[j] = sk->r[LAS_N + j];           /* r_1 = final ell components of r (meet A') */
  /* ^[CHANGED] sign.c:48:
   *     s1hat = s1;
   * WHY: A = [I | A'] -- only the bottom ell components of r meet A' and need
   * the NTT; the top n components (r_0) pass through the identity block
   * untouched (added below).  Upstream's A is a full K x L matrix, so ALL of
   * s1 is transformed. */
  b_polyvecl_ntt(r_1_hat);                   /* [REUSED]  sign.c:49: polyvecl_ntt(&s1hat);  (L -> ell) */
  b_polyvec_matrix_pointwise_montgomery(pk->t, pp->a_prime, r_1_hat);   /* A' r_1 */
                                             /* [REUSED]  sign.c:50: polyvec_matrix_pointwise_montgomery(&t1, mat, &s1hat); */
  b_polyvecn_reduce(pk->t);                  /* [REUSED]  sign.c:51: polyveck_reduce(&t1);  (K -> n) */
  b_polyvecn_invntt_tomont(pk->t);           /* [REUSED]  sign.c:52: polyveck_invntt_tomont(&t1); */

  /* Add r_0 (identity block: top n components of r) */
  b_polyvecn_add(pk->t, pk->t, sk->r);
  /* ^[CHANGED] sign.c:55:
   *     polyveck_add(&t1, &t1, &s2);
   * WHY: same "+ error" step, but the error IS the top n components of r:
   * t = A r = r_0 + A' r_1 for A = [I | A'].  (sk->r decays to its first
   * n polynomials here.) */
  b_polyvecn_reduce(pk->t);
  /* ^[CHANGED] no upstream line.
   * WHY: upstream's t1 is freshly inverse-NTT'd, already in caddq's domain
   * (-Q,Q); here the identity-block addition can push coefficients outside
   * it, so one reduce restores the domain before caddq. */

  /* Extract t1 and write public key */
  b_polyvecn_caddq(pk->t);                   /* [REUSED]  sign.c:58: polyveck_caddq(&t1); */
  /* [DELETED] sign.c:59:
   *     polyveck_power2round(&t1, &t0, &t1);
   * WHY: no key compression in the paper's simplified scheme -- the verifier
   * recomputes w' with the EXACT t (w' = Az - ct), so t is never split into
   * t1/t0 and no hint is ever needed. */
  /* [DELETED] sign.c:60-64:
   *     pack_pk(pk, rho, &t1);
   *     shake256(tr, TRBYTES, pk, CRYPTO_PUBLICKEYBYTES);
   *     pack_sk(sk, rho, tr, key, &t0, &s1, &s2);
   * WHY: keys are structs (no byte encoding; ref/serialize.c is the separate
   * wire format), and the challenge hash binds the raw t directly, so no
   * key digest tr is precomputed. */

  /* [PAPER Alg.1] 4:      return (pk, sk) = (t, r) */
  return 0;
  /* [PAPER Alg.1] 5:  end procedure */
}

/*************************************************
* Name:        base_sign_internal  <->  crypto_sign_signature_internal (sign.c:85)
*
* Description: Algorithm 1 Sign body, parameterised by the caller-supplied
*              64-byte mask seed (sign.c's internal takes rnd the same way).
*
* Returns 0 (success)
**************************************************/
int base_sign_internal(signature *sig,       /* paper σ: output signature σ = (c, z)  */
                                 const uint8_t *m,   /* paper M: message                      */
                                 size_t mlen,        /* length of M (no paper symbol)         */
                                 const public_key *pk,   /* paper t: pk->t = t (public key)       */
                                 const secret_key *sk,   /* paper r: sk->r = r (secret key)       */
                                 const public_params *pp,   /* paper A: A = [I | A'] (A in pp = (A,H))            */
                                 const uint8_t mask_seed[64]) {  /* mask seed for the S_gamma sampler; implementation-only, NO paper symbol (<-> Dilithium rnd / rho') */
  /* [PAPER Alg.1] 6:  procedure Sign((pk, sk), M): */
  unsigned int j;
  uint8_t t_packed[LAS_N*LAS_D*4];       /* canonical packing of the public key t: first input of H(pk, w, M) */
  uint8_t w_packed[LAS_N*LAS_D*4];       /* canonical packing of the commitment w: second input of H(pk, w, M) */
  uint8_t c_tilde[LAS_SEEDBYTES];  /* challenge seed: <-> the c_tilde bytes (sign.c:152)     */
  uint16_t mask_nonce = 0;            /* [REUSED]  sign.c:97: uint16_t mask_nonce = 0;               */
  poly y[N_PLUS_ELL];                 /* paper y: mask, y <-$ Sγ^(n+ℓ)  <-> polyvecl y          */
  poly y_1_hat[ELL];            /* NTT buffer for the A' half of y (see [CHANGED] below)  */
  poly w[LAS_N];                 /* paper w: commitment, w = A y   <-> polyveck w1         */
  poly r_hat[N_PLUS_ELL];              /* paper r in NTT domain          <-> s1 (NTT'd in place) */
  poly c;                        /* paper c: challenge polynomial, LOCAL only (SampleInBall(c_tilde)) */
  poly c_hat;                     /* NTT copy of c                  <-> poly cp             */
  keccak_state state;            /* [REUSED]  sign.c:101: keccak_state state;              */

  /* [DELETED] sign.c:103-108:
   *     rho = seedbuf; ... unpack_sk(rho, tr, key, &t0, &s1, &s2, sk);
   * WHY: sk is a struct -- there is nothing to unpack, and no t0/tr/key exist
   * (no key compression, no key digest, no signing PRF key). */

  /* Compute mu = CRH(tr, pre, msg) */
  b_polyvecn_pack_w(t_packed, pk->t);
  /* ^[CHANGED] sign.c:110-116:
   *     shake256_init(&state);
   *     shake256_absorb(&state, tr, TRBYTES);
   *     shake256_absorb(&state, pre, prelen);
   *     shake256_absorb(&state, m, mlen);
   *     shake256_finalize(&state);
   *     shake256_squeeze(mu, CRHBYTES, &state);
   * WHY: upstream compresses (key digest tr, ctx prefix, M) into the 64-byte
   * mu ONCE per call and re-absorbs mu each attempt.  The paper's oracle is
   * c = H(pk, w, M) with the RAW public key: so the once-per-call precompute
   * here is packing t canonically (t_packed); M is absorbed directly in the loop
   * below, and there is no ctx prefix. */

  /* Compute rhoprime = CRH(key, rnd, mu) */
  /* [CHANGED] sign.c:118-124:
   *     shake256_init(&state);
   *     shake256_absorb(&state, key, SEEDBYTES);
   *     shake256_absorb(&state, rnd, RNDBYTES);
   *     shake256_absorb(&state, mu, CRHBYTES);
   *     shake256_finalize(&state);
   *     shake256_squeeze(rhoprime, CRHBYTES, &state);
   * WHY: rhoprime (the 64-byte mask-sampler seed) is derived by the CALLER
   * here and passed in as `seed`: base_sign draws it fresh from
   * randombytes; ref/las.c's deterministic path derives it as
   * SHAKE256(tag, sk, [Y], M) -- same role, same width. */

  /* Expand matrix and transform vectors */
  /* [DELETED] sign.c:127:
   *     polyvec_matrix_expand(mat, rho);
   * WHY: A is fixed in pp (see base_keygen). */
  for(j = 0; j < N_PLUS_ELL; ++j)
    r_hat[j] = sk->r[j];
  b_polyvecm_ntt(r_hat);                      /* [REUSED]  sign.c:128: polyvecl_ntt(&s1);
                                              * (hoisted once per call -- the secret is
                                              * invariant across rejection attempts)     */
  /* [DELETED] sign.c:129-130:
   *     polyveck_ntt(&s2);
   *     polyveck_ntt(&t0);
   * WHY: no s2 (identity block of A) and no t0 (no key compression). */

rej:                                         /* [REUSED]  sign.c:132: rej:  */
  ++base_attempts;                           /* instrumentation only; no upstream line
                                              * (added so benchmarks read the restart
                                              * rate directly)                          */

  /* Sample intermediate vector y */
  /* [PAPER Alg.1] 7:      y <-$ Sγ^(n+ℓ) */
  b_polyvecm_uniform_Sgamma(y, mask_seed, mask_nonce++);
  /* ^[CHANGED] sign.c:134:
   *     polyvecl_uniform_gamma1(&y, rhoprime, mask_nonce++);
   * WHY: the paper's mask set is S_gamma = uniform [-gamma, gamma] with
   * gamma = kappa*d*(n+l) -- NOT a power of two like GAMMA1, so upstream's
   * fixed-width bit-unpacking cannot produce it and the twin sampler uses
   * rejection sampling.  Same vector-level call, same seed role (rhoprime ->
   * the 64-byte mask seed), same mask_nonce discipline (mask_nonce++ per attempt,
   * m*mask_nonce + i inside <-> L*mask_nonce + i in polyvecl_uniform_gamma1). */

  /* Matrix-vector multiplication */
  /* [PAPER Alg.1] 8:      w = A y */
  for(j = 0; j < ELL; ++j)
    y_1_hat[j] = y[LAS_N + j];
  /* ^[CHANGED] sign.c:137:
   *     z = y;
   * WHY: upstream copies ALL of y into z and transforms it (its A is full
   * K x L).  Here A = [I | A']: only the bottom l components of y meet A',
   * so only they are copied and transformed; the top n components join via
   * the identity block below.  (A dedicated y_1_hat replaces reusing z: z lives
   * in sig->z here and is written by the response computation instead.) */
  b_polyvecl_ntt(y_1_hat);                      /* [REUSED]  sign.c:138: polyvecl_ntt(&z);  (L -> l) */
  b_polyvec_matrix_pointwise_montgomery(w, pp->a_prime, y_1_hat);
                                             /* [REUSED]  sign.c:139: polyvec_matrix_pointwise_montgomery(&w1, mat, &z); */
  b_polyvecn_reduce(w);                      /* [REUSED]  sign.c:140: polyveck_reduce(&w1); */
  b_polyvecn_invntt_tomont(w);               /* [REUSED]  sign.c:141: polyveck_invntt_tomont(&w1); */
  b_polyvecn_add(w, w, y);
  /* ^[CHANGED] no upstream line in Sign (in KeyGen it is sign.c:55, + s2).
   * WHY: the identity block of A = [I | A'] completes w = A y = y_top + A' y_bot;
   * upstream's Sign has no such addition because its A is a full matrix. */
  b_polyvecn_reduce(w);
  /* ^[CHANGED] no upstream line.
   * WHY: restore caddq's (-Q,Q) domain after the identity-block addition
   * (same reason as in base_keygen). */

  /* Decompose w and call the random oracle */
  b_polyvecn_caddq(w);                       /* [REUSED]  sign.c:144: polyveck_caddq(&w1); */
  /* [DELETED] sign.c:145:
   *     polyveck_decompose(&w1, &w0, &w1);
   * WHY: the paper hashes the FULL commitment w ("for ease of presentation",
   * paper §2.2/§3.2) -- no high/low-bit split, hence also no w0, no second
   * rejection test and no hint vector further down. */
  b_polyvecn_pack_w(w_packed, w);
  /* ^[CHANGED] sign.c:146:
   *     polyveck_pack_w1(sig, &w1);
   * WHY: with no decompose there are no 4/6-bit w1 codes to pack; the twin
   * packs every canonical coefficient of the full w, 4 bytes little-endian,
   * so the oracle binds all of w. */

  shake256_init(&state);                     /* [REUSED]  sign.c:148: shake256_init(&state); */
  shake256_absorb(&state, t_packed, sizeof t_packed);
  /* ^[CHANGED] sign.c:149:
   *     shake256_absorb(&state, mu, CRHBYTES);
   * WHY: the oracle input starts with the raw public key t (packed above)
   * instead of the mu digest -- the paper's c = H(pk, w, M) binds pk
   * directly. */
  shake256_absorb(&state, w_packed, sizeof w_packed);
                                             /* [REUSED]  sign.c:150: shake256_absorb(&state, sig, K*POLYW1_PACKEDBYTES);
                                              * (the packed commitment -- full w here)  */
  shake256_absorb(&state, m, mlen);
  /* ^[CHANGED] no upstream line HERE (M is inside mu, sign.c:114).
   * WHY: with no mu digest the message is absorbed directly -- third input
   * of c = H(pk, w, M). */
  shake256_finalize(&state);                 /* [REUSED]  sign.c:151: shake256_finalize(&state); */
  shake256_squeeze(c_tilde, LAS_SEEDBYTES, &state);
  /* ^[CHANGED] sign.c:152:
   *     shake256_squeeze(sig, CTILDEBYTES, &state);
   * WHY: the digest c_tilde seeds the challenge sampler AND -- as in upstream --
   * IS the stored challenge component of the signature (memcpy'd into
   * sig->c_tilde below); the challenge polynomial c derived from it is local. */
  /* [PAPER Alg.1] 9:      c = H(pk, w, M) */
  b_poly_challenge(&c, c_tilde);
  /* ^[REUSED] sign.c:153:
   *     poly_challenge(&cp, sig);
   * WHY: same SampleInBall construction, with the paper's challenge weight kappa
   * (per parameter set) instead of TAU.  c is a LOCAL arithmetic value only; the
   * stored component is the digest c_tilde (memcpy'd below), exactly as upstream
   * stores c_tilde and re-derives the polynomial via poly_challenge. */
  c_hat = c;
  poly_ntt(&c_hat);
  /* ^[REUSED] sign.c:154:
   *     poly_ntt(&cp);
   * WHY: c_hat is a transformed COPY of c (mirrors the Rust ntt(&[c.clone()]));
   * c itself is not stored.  Once per attempt (hoisted out of the m products). */

  /* Compute z, reject if it reveals secret */
  /* [PAPER Alg.1] 10:     z = y + c r, where r := sk */
  b_polyvecm_pointwise_poly_montgomery(sig->z, &c_hat, r_hat);
                                             /* [REUSED]  sign.c:157: polyvecl_pointwise_poly_montgomery(&z, &cp, &s1);
                                              * (s1 -> r, L -> m; z lives in sig->z)    */
  b_polyvecm_invntt_tomont(sig->z);          /* [REUSED]  sign.c:158: polyvecl_invntt_tomont(&z); */
  b_polyvecm_add(sig->z, sig->z, y);         /* [REUSED]  sign.c:159: polyvecl_add(&z, &z, &y); */
  b_polyvecm_reduce(sig->z);                 /* [REUSED]  sign.c:160: polyvecl_reduce(&z); */
  /* [PAPER Alg.1] 11:     if ||z||∞ > γ − κ, then Restart */
  if(b_polyvecm_chknorm(sig->z, BOUND_SIGN))
    goto rej;
  /* ^[CHANGED] sign.c:161-162:
   *     if(polyvecl_chknorm(&z, GAMMA1 - BETA))
   *       goto rej;
   * WHY: same reject-if-too-large test, same bound SHAPE: upstream's
   * BETA = TAU*ETA; here eta = 1 (ternary secret), so beta = kappa*1 = kappa
   * and the bound is gamma - kappa (paper Alg. 1 step 11). */

  /* [DELETED] sign.c:164-171:
   *     polyveck_pointwise_poly_montgomery(&h, &cp, &s2);
   *     polyveck_invntt_tomont(&h);
   *     polyveck_sub(&w0, &w0, &h);
   *     polyveck_reduce(&w0);
   *     if(polyveck_chknorm(&w0, GAMMA2 - BETA))
   *       goto rej;
   * WHY: the low-bits rejection protects the w0/w1 DECOMPOSITION, which the
   * paper's scheme deleted -- there is no w0 and no s2, so this second
   * rejection test has nothing to check. */
  /* [DELETED] sign.c:173-183:
   *     polyveck_pointwise_poly_montgomery(&h, &cp, &t0);  ... (hints)
   *     if(n > OMEGA)  goto rej;
   * WHY: hints only exist to let the verifier reconstruct high bits from the
   * COMPRESSED t1; with the exact t kept (no power2round) the verifier
   * recomputes w' exactly and no hint vector is needed. */

  /* Write signature */
  /* [PAPER Alg.1] 12:     return σ = (c, z) */
  memcpy(sig->c_tilde, c_tilde, LAS_CTILDEBYTES);
  /* ^[CHANGED] sign.c:186:
   *     pack_sig(sig, sig, &z, &h);
   * WHY: struct output storing the SAME challenge component upstream's pack_sig
   * writes -- the 32-byte digest c_tilde (the challenge polynomial c is local
   * only, see above); z was written into sig->z above; the byte encoding of the
   * whole (c_tilde, z) lives in ref/serialize.c instead. */
  return 0;                                  /* [REUSED]  sign.c:188: return 0; */
  /* [PAPER Alg.1] 13: end procedure */
}

/*************************************************
* Name:        base_sign  <->  crypto_sign_signature (sign.c:206)
*
* Description: Algorithm 1 Sign, random path: fresh mask seed, then the
*              internal.
*
* Returns 0 (success)
**************************************************/
int base_sign(signature *sig,       /* paper σ: output signature σ = (c, z)  */
                        const uint8_t *m,   /* paper M: message                      */
                        size_t mlen,        /* length of M (no paper symbol)         */
                        const public_key *pk,   /* paper t: pk->t = t (public key)       */
                        const secret_key *sk,   /* paper r: sk->r = r (secret key)       */
                        const public_params *pp) { /* paper A: A = [I | A'] (A in pp = (A,H))            */
  uint8_t mask_seed[64];      /* mask seed; implementation-only, NO paper symbol (<-> Dilithium rnd) */

  /* [DELETED] sign.c:218-225:
   *     if(ctxlen > 255)  return -1;
   *     pre[0] = 0;  pre[1] = ctxlen;
   *     for(i = 0; i < ctxlen; i++)  pre[2 + i] = ctx[i];
   * WHY: no ctx-string API in the paper's scheme -- the oracle input is
   * exactly (pk, w, M). */
  randombytes(mask_seed, 64);
  /* ^[CHANGED] sign.c:227-232:
   *     #ifdef DILITHIUM_RANDOMIZED_SIGNING
   *       randombytes(rnd, RNDBYTES);
   *     #else
   *       for(i=0;i<RNDBYTES;i++)  rnd[i] = 0;
   *     #endif
   * WHY: upstream feeds a 32-byte rnd into the rhoprime CRH chain; here the
   * 64-byte mask seed is the randomness itself (there is no CRH chain), so
   * it is always drawn fresh.  The deterministic analogue of the #else
   * branch is ref/las.c base_sign_det (seed derived from (sk, M)). */
  return base_sign_internal(sig, m, mlen, pk, sk, pp, mask_seed);
                                             /* [REUSED]  sign.c:234: crypto_sign_signature_internal(sig,siglen,m,mlen,pre,2+ctxlen,rnd,sk); */
}

/*************************************************
* Name:        base_sign_det  (deterministic Sign; KAT slot; no sign.c analogue)
*
* Description: mask randomness derived from (sk, M) via det_seed (tag 0), then
*              the same internal.  Same distribution/validity as the random
*              base_sign; removes the per-signature RNG (no nonce-reuse risk) and
*              makes the signature a reproducible function of (sk, M).  Sign
*              belongs to the base scheme, so the deterministic slot lives here.
*
* Returns 0 (success)
**************************************************/
int base_sign_det(signature *sig,       /* paper σ: output signature σ = (c, z)  */
                  const uint8_t *m,   /* paper M: message                      */
                  size_t mlen,        /* length of M (no paper symbol)         */
                  const public_key *pk,   /* paper t: pk->t = t (public key)       */
                  const secret_key *sk,   /* paper r: sk->r = r (secret key)       */
                  const public_params *pp) { /* paper A: A = [I | A'] (A in pp = (A,H))            */
  uint8_t mask_seed[64];      /* mask seed from (sk, M); implementation-only, NO paper symbol; tag 0 = sign */
  det_seed(mask_seed, sk, m, mlen);
  return base_sign_internal(sig, m, mlen, pk, sk, pp, mask_seed);
}

/*************************************************
* Name:        base_verify_internal  <->  crypto_sign_verify_internal (sign.c:289)
*
* Description: Algorithm 1 Verify: w' = A z - c t; accept iff c == H(pk,w',M).
*
* Returns 0 if signature could be verified correctly and -1 otherwise
**************************************************/
int base_verify_internal(const signature *sig,  /* paper σ: sig = (c, z), signature to verify */
                              const uint8_t *m,    /* paper M: message                          */
                              size_t mlen,         /* length of M (no paper symbol)             */
                              const public_key *pk,    /* paper t: pk->t = t (public key)           */
                              const public_params *pp) {  /* paper A: A = [I | A'] (A in pp = (A,H))                */
  /* [PAPER Alg.1] 14: procedure Verify(pk, σ, M): */
  unsigned int i, j;             /* loop indices (i for the challenge byte-compare, j for the vector loops) */
  uint8_t t_packed[LAS_N*LAS_D*4];       /* canonical packing of the public key t: first input of H(pk, w', M) */
  uint8_t w_packed[LAS_N*LAS_D*4];       /* canonical packing of the commitment w': second input of H(pk, w', M) */
  uint8_t c_tilde[LAS_SEEDBYTES];  /* challenge seed <-> c2[CTILDEBYTES] (sign.c:302) */
  poly c;                             /* paper c: challenge polynomial, SampleInBall(sig->c_tilde), LOCAL */
  poly c_hat;                     /* NTT copy of c       <-> poly cp (sign.c:303)  */
  poly z_1_hat[ELL];            /* A' half of z        <-> polyvecl z (sign.c:304) */
  poly w_prime[LAS_N];                 /* paper w′            <-> polyveck w1 (sign.c:305) */
  poly t_hat[LAS_N];              /* t in the NTT domain <-> polyveck t1 (sign.c:305) */
  keccak_state state;            /* [REUSED]  sign.c:306: keccak_state state;     */

  /* [DELETED] sign.c:308-309:
   *     if(siglen != CRYPTO_BYTES)  return -1;
   * WHY: struct input, no byte length to check (ref/serialize.c's validating
   * decoder is the byte-level gate). */
  /* [DELETED] sign.c:311-312:
   *     unpack_pk(rho, &t1, pk);
   *     if(unpack_sig(c, &z, &h, sig))  return -1;
   * WHY: pk and sig are structs; there is no hint h to decode at all. */
  /* [PAPER Alg.1] 15:     Parse (c, z) := σ */
  /* [PAPER Alg.1] 16:     if ||z||∞ > γ − κ, then return 0 */
  if(b_polyvecm_chknorm(sig->z, BOUND_SIGN))
    return -1;
  /* ^[CHANGED] sign.c:314:
   *     if(polyvecl_chknorm(&z, GAMMA1 - BETA))  return -1;
   * WHY: same norm gate, bound gamma - kappa (= gamma - beta with eta = 1;
   * see the Sign-side note). */

  /* Compute CRH(H(rho, t1), pre, msg) */
  b_polyvecn_pack_w(t_packed, pk->t);
  /* ^[CHANGED] sign.c:317-324:
   *     shake256(mu, TRBYTES, pk, CRYPTO_PUBLICKEYBYTES);
   *     shake256_init(&state);  ... shake256_squeeze(mu, CRHBYTES, &state);
   * WHY: same reason as in Sign -- the oracle is c = H(pk, w', M) with the
   * raw public key, so the once-per-call step is packing t canonically, not
   * hashing it into mu. */

  /* Matrix-vector multiplication; compute Az - c2^dt1 */
  /* [PAPER Alg.1] 17:     w′ = A z − c t, where t := pk */
  b_poly_challenge(&c, sig->c_tilde);        /* c = SampleInBall(c_tilde), from the stored digest */
  c_hat = c;
  /* ^[REUSED] sign.c:327:
   *     poly_challenge(&cp, c);
   * WHY: the signature stores the challenge DIGEST c_tilde (upstream's c_tilde
   * lifecycle), so Verify re-derives the challenge polynomial locally right here,
   * exactly as upstream sign.c:327 poly_challenge(&cp, c) does after unpacking;
   * c_hat is the transformed copy used for c*t below. */
  /* [DELETED] sign.c:328:
   *     polyvec_matrix_expand(mat, rho);
   * WHY: A is fixed in pp. */

  for(j = 0; j < ELL; ++j)
    z_1_hat[j] = sig->z[LAS_N + j];
  /* ^[CHANGED] (part of) sign.c:330:  polyvecl_ntt(&z);
   * WHY: A = [I | A'] again -- only the bottom l components of z meet A';
   * the top n join through the identity block after the inverse transform. */
  b_polyvecl_ntt(z_1_hat);                      /* [REUSED]  sign.c:330: polyvecl_ntt(&z);  (L -> l) */
  b_polyvec_matrix_pointwise_montgomery(w_prime, pp->a_prime, z_1_hat);
                                             /* [REUSED]  sign.c:331: polyvec_matrix_pointwise_montgomery(&w1, mat, &z); */

  poly_ntt(&c_hat);                           /* [REUSED]  sign.c:333: poly_ntt(&cp); */
  /* [DELETED] sign.c:334:
   *     polyveck_shiftl(&t1);
   * WHY: the shift undoes power2round's 2^d factor; t was never compressed,
   * so there is no factor to restore. */
  for(j = 0; j < LAS_N; ++j)
    t_hat[j] = pk->t[j];
  b_polyvecn_ntt(t_hat);                      /* [REUSED]  sign.c:335: polyveck_ntt(&t1); */
  b_polyvecn_pointwise_poly_montgomery(t_hat, &c_hat, t_hat);
                                             /* [REUSED]  sign.c:336: polyveck_pointwise_poly_montgomery(&t1, &cp, &t1); */

  /* [CHANGED] sign.c:338-340:
   *     polyveck_sub(&w1, &w1, &t1);
   *     polyveck_reduce(&w1);
   *     polyveck_invntt_tomont(&w1);
   * WHY: upstream subtracts c*t from Az entirely in the NTT domain and
   * inverse-transforms once.  Here the identity block of A = [I | A'] must
   * add the NON-transformed z_top to Az, which forces the subtraction into
   * the normal domain: invert both halves first, then add/subtract.  Same
   * number of inverse transforms as ref/las.c (w' is exact either way). */
  b_polyvecn_reduce(w_prime);                      /* [REUSED]  sign.c:339: polyveck_reduce(&w1); */
  b_polyvecn_invntt_tomont(w_prime);               /* [REUSED]  sign.c:340: polyveck_invntt_tomont(&w1); */
  b_polyvecn_add(w_prime, w_prime, sig->z);              /* identity block: w += z_top (A = [I | A']) */
  b_polyvecn_invntt_tomont(t_hat);            /* second inverse transform, for c*t         */
  b_polyvecn_sub(w_prime, w_prime, t_hat);                /* [REUSED]  sign.c:338: polyveck_sub(&w1, &w1, &t1);
                                              * (after the inverse transforms, see WHY)  */
  b_polyvecn_reduce(w_prime);                      /* [REUSED]  sign.c:339: polyveck_reduce(&w1); */

  /* Reconstruct w1 */
  b_polyvecn_caddq(w_prime);                       /* [REUSED]  sign.c:343: polyveck_caddq(&w1); */
  /* [DELETED] sign.c:344:
   *     polyveck_use_hint(&w1, &w1, &h);
   * WHY: w' is exact (no compression anywhere), so there are no high bits to
   * repair and no hint. */
  b_polyvecn_pack_w(w_packed, w_prime);
  /* ^[CHANGED] sign.c:345:
   *     polyveck_pack_w1(buf, &w1);
   * WHY: pack the FULL canonical w' (4 bytes/coeff), as on the Sign side. */

  /* Call random oracle and verify challenge */
  shake256_init(&state);                     /* [REUSED]  sign.c:348: shake256_init(&state); */
  shake256_absorb(&state, t_packed, sizeof t_packed);
  /* ^[CHANGED] sign.c:349:
   *     shake256_absorb(&state, mu, CRHBYTES);
   * WHY: raw pk instead of mu (c = H(pk, w', M)). */
  shake256_absorb(&state, w_packed, sizeof w_packed);
                                             /* [REUSED]  sign.c:350: shake256_absorb(&state, buf, K*POLYW1_PACKEDBYTES); */
  shake256_absorb(&state, m, mlen);
  /* ^[CHANGED] no upstream line HERE (M is inside mu).
   * WHY: third oracle input absorbed directly. */
  shake256_finalize(&state);                 /* [REUSED]  sign.c:351: shake256_finalize(&state); */
  shake256_squeeze(c_tilde, LAS_SEEDBYTES, &state);
                                             /* [REUSED]  sign.c:352: shake256_squeeze(c2, CTILDEBYTES, &state);
                                              * (32-byte challenge seed instead)        */
  /* [PAPER Alg.1] 18:     if c ≠ H(pk, w′, M), then return 0 */
  for(i = 0; i < LAS_CTILDEBYTES; ++i)
    if(c_tilde[i] != sig->c_tilde[i])
      return -1;
  /* ^[REUSED] sign.c:353-355:
   *     for(i = 0; i < CTILDEBYTES; ++i)
   *       if(c[i] != c2[i])  return -1;
   * WHY: accept iff the recomputed DIGEST equals the stored one -- the upstream
   * byte-compare loop over the challenge digest (the refactor's polynomial
   * compare is gone; strictly stronger, identical accept/reject on honest paths). */

  /* [PAPER Alg.1] 19:     return 1 */
  return 0;                                  /* [REUSED]  sign.c:357: return 0; */
  /* [PAPER Alg.1] 20: end procedure */
}

/*************************************************
* Name:        base_verify  <->  crypto_sign_verify (sign.c:375)
*
* Description: Algorithm 1 Verify, public entry point.
*
* Returns 0 if signature could be verified correctly and -1 otherwise
**************************************************/
int base_verify(const signature *sig,  /* paper σ: sig = (c, z), signature to verify */
                     const uint8_t *m,    /* paper M: message                          */
                     size_t mlen,         /* length of M (no paper symbol)             */
                     const public_key *pk,    /* paper t: pk->t = t (public key)           */
                     const public_params *pp) {  /* paper A: A = [I | A'] (A in pp = (A,H))                */
  /* [DELETED] sign.c:386-392:
   *     if(ctxlen > 255)  return -1;
   *     pre[0] = 0;  pre[1] = ctxlen;
   *     for(i = 0; i < ctxlen; i++)  pre[2 + i] = ctx[i];
   * WHY: no ctx-string API (same as the Sign side). */
  return base_verify_internal(sig, m, mlen, pk, pp);
                                             /* [REUSED]  sign.c:394: return crypto_sign_verify_internal(sig,siglen,m,mlen,pre,2+ctxlen,pk); */
}


/* ======================= poly.c twins (local copies) ======================= */

/*************************************************
* Name:        b_rej_S1  <->  rej_eta (poly.c:384)
*
* Description: Sample uniformly random coefficients in {-1,0,1} (set S_1) by
*              performing rejection sampling on array of random bytes.
*              [CHANGED] vs rej_eta:
*                  t0 = buf[pos] & 0x0F;  t1 = buf[pos++] >> 4;   (half-bytes)
*              WHY: S_1 has 3 values, so the candidate unit is a 2-BIT code
*              (four per byte): {0,1,2} -> {-1,0,1}, reject 3.  Same loop
*              shape, same mid-byte ctr checks as rej_eta's t1 branch.
*
* Returns number of sampled coefficients
**************************************************/
static unsigned int b_rej_S1(int32_t *a, unsigned int len,
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

/*************************************************
* Name:        b_poly_uniform_S1  <->  poly_uniform_eta (poly.c:435)
*
* Description: Sample polynomial with uniformly random coefficients in S_1 by
*              performing rejection sampling on the output stream of
*              SHAKE256(seed|nonce).  Same one-block-at-a-time driver as
*              poly_uniform_eta.
*              [CHANGED] vs poly_uniform_eta:
*                  stream256_init(&state, seed, nonce);   (seed[CRHBYTES]=64)
*              WHY: the KeyGen seed here is the raw 32-byte randombytes
*              output (no rhoprime expansion), and stream256_init is written
*              out as its definition: absorb(seed) then absorb(nonce_le16).
**************************************************/
static void b_poly_uniform_S1(poly *a, const uint8_t seed[LAS_SEEDBYTES], uint16_t nonce) {
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
  shake256_squeezeblocks(buf, 1, &state);    /* [REUSED] poly.c:445 squeeze/refill driver */

  ctr = b_rej_S1(a->coeffs, LAS_D, buf, SHAKE256_RATE);

  while(ctr < LAS_D) {
    shake256_squeezeblocks(buf, 1, &state);
    ctr += b_rej_S1(a->coeffs + ctr, LAS_D - ctr, buf, SHAKE256_RATE);
  }
}

/*************************************************
* Name:        b_rej_Sgamma  <->  rej_uniform (poly.c:309)
*
* Description: Sample uniformly random coefficients in [-GAMMA, GAMMA] by
*              performing rejection sampling on array of random bytes.  Same
*              3-bytes-per-candidate loop as rej_uniform.
*              [CHANGED] vs rej_uniform:
*                  t &= 0x7FFFFF;
*                  if(t < Q)  a[ctr++] = t;
*              WHY: the target set is S_gamma, not [0,Q): the mask is the
*              smallest 2^k - 1 >= 2*GAMMA (window as tight as upstream's),
*              the acceptance test is t < 2*GAMMA+1, and the accepted value
*              is shifted by -GAMMA to centre the range on 0.
*
* Returns number of sampled coefficients
**************************************************/
static unsigned int b_rej_Sgamma(int32_t *a, unsigned int len,
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

/*************************************************
* Name:        b_poly_uniform_Sgamma  <->  poly_uniform_gamma1 (poly.c:467)
*
* Description: Sample polynomial with uniformly random coefficients in
*              [-GAMMA, GAMMA] from SHAKE256(seed|nonce).
*              [CHANGED] vs poly_uniform_gamma1:
*                  stream256_squeezeblocks(buf, POLY_UNIFORM_GAMMA1_NBLOCKS, &state);
*                  polyz_unpack(a, buf);
*              WHY: upstream's GAMMA1 is a power of two, so a fixed-length
*              stream can be BIT-UNPACKED with no rejections.  The paper's
*              gamma = kappa*d*(n+l) is not, so the fixed-length unpack is
*              impossible and the driver is poly_uniform_eta's rejection
*              driver (poly.c:449-452): one SHAKE256 block at a time.
**************************************************/
static void b_poly_uniform_Sgamma(poly *a, const uint8_t seed[64], uint16_t nonce) {
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

  ctr = b_rej_Sgamma(a->coeffs, LAS_D, buf, SHAKE256_RATE);

  while(ctr < LAS_D) {
    shake256_squeezeblocks(buf, 1, &state);
    ctr += b_rej_Sgamma(a->coeffs + ctr, LAS_D - ctr, buf, SHAKE256_RATE);
  }
}

/*************************************************
* Name:        b_poly_challenge  <->  poly_challenge (poly.c:489)
*
* Description: Implementation of H. Samples polynomial with KAPPA nonzero
*              coefficients in {-1,1} using the output stream of
*              SHAKE256(seed).  Body is verbatim poly_challenge with
*              [CHANGED]  TAU -> KAPPA          (the paper's challenge weight,
*                                                    per parameter set)
*              [CHANGED]  seed[CTILDEBYTES] -> seed[32]  (the challenge seed is a
*                                                    fixed 32-byte SHAKE digest)
**************************************************/
static void b_poly_challenge(poly *c, const uint8_t seed[LAS_SEEDBYTES]) {
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

/*************************************************
* Name:        b_polyw_pack  <->  polyw1_pack (poly.c:888)
*
* Description: Pack the commitment polynomial for hashing.
*              [CHANGED] vs polyw1_pack (which packs 4/6-bit w1 HIGH-BIT codes):
*              WHY: Algorithm 1 deleted Decompose, so the oracle binds the
*              FULL w -- every canonical coefficient is packed, 4 bytes
*              little-endian.  The reduce+caddq pair canonicalises the input
*              (an identity for the already-canonical callers; upstream's
*              callers canonicalise via caddq before packing, sign.c:144).
**************************************************/
static void b_polyw_pack(uint8_t *r, const poly *a) {
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

/* ===================== polyvec.c twins (local copies) =====================
 * Bodies verbatim from polyvec.c; plain poly arrays replace the
 * polyvecl/polyveck structs and the dimensions are l / m = n+l / n. */

/* <-> polyvec_matrix_pointwise_montgomery (polyvec.c:24).  v spans only the
 * l columns of A' because A = [I | A'] (identity block added by callers). */
static void b_polyvec_matrix_pointwise_montgomery(poly t[LAS_N], const poly mat[LAS_N][ELL],
                                                  const poly v[ELL]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    b_polyvecl_pointwise_acc_montgomery(&t[i], mat[i], v);
}

/* <-> polyvecl_pointwise_acc_montgomery (polyvec.c:113), L -> l. */
static void b_polyvecl_pointwise_acc_montgomery(poly *w, const poly u[ELL],
                                                const poly v[ELL]) {
  unsigned int i;
  poly t;

  poly_pointwise_montgomery(w, &u[0], &v[0]);
  for(i = 1; i < ELL; ++i) {
    poly_pointwise_montgomery(&t, &u[i], &v[i]);
    poly_add(w, w, &t);
  }
}

/* <-> polyvecl_ntt (polyvec.c:81), over the l columns of A'. */
static void b_polyvecl_ntt(poly v[ELL]) {
  unsigned int i;

  for(i = 0; i < ELL; ++i)
    poly_ntt(&v[i]);
}

/* <-> polyvecl_uniform_eta (polyvec.c:35): same nonce++-per-poly discipline;
 * L -> m, eta -> S_1. */
static void b_polyvecm_uniform_S1(poly v[N_PLUS_ELL], const uint8_t seed[LAS_SEEDBYTES], uint16_t nonce) {
  unsigned int i;

  for(i = 0; i < N_PLUS_ELL; ++i)
    b_poly_uniform_S1(&v[i], seed, nonce++);
}

/* <-> polyvecl_uniform_gamma1 (polyvec.c:42): same L*nonce + i derivation
 * (m*nonce + i here), so one nonce++ per signing attempt at the call site. */
static void b_polyvecm_uniform_Sgamma(poly v[N_PLUS_ELL], const uint8_t seed[64], uint16_t nonce) {
  unsigned int i;

  for(i = 0; i < N_PLUS_ELL; ++i)
    b_poly_uniform_Sgamma(&v[i], seed, (uint16_t)(N_PLUS_ELL*nonce + i));
}

/* <-> polyvecl_reduce (polyvec.c:49). */
static void b_polyvecm_reduce(poly v[N_PLUS_ELL]) {
  unsigned int i;

  for(i = 0; i < N_PLUS_ELL; ++i)
    poly_reduce(&v[i]);
}

/* <-> polyvecl_add (polyvec.c:66). */
static void b_polyvecm_add(poly w[N_PLUS_ELL], const poly u[N_PLUS_ELL], const poly v[N_PLUS_ELL]) {
  unsigned int i;

  for(i = 0; i < N_PLUS_ELL; ++i)
    poly_add(&w[i], &u[i], &v[i]);
}

/* <-> polyvecl_ntt (polyvec.c:81), L -> m (the full secret/response vector). */
static void b_polyvecm_ntt(poly v[N_PLUS_ELL]) {
  unsigned int i;

  for(i = 0; i < N_PLUS_ELL; ++i)
    poly_ntt(&v[i]);
}

/* <-> polyvecl_invntt_tomont (polyvec.c:88). */
static void b_polyvecm_invntt_tomont(poly v[N_PLUS_ELL]) {
  unsigned int i;

  for(i = 0; i < N_PLUS_ELL; ++i)
    poly_invntt_tomont(&v[i]);
}

/* <-> polyvecl_pointwise_poly_montgomery (polyvec.c:95). */
static void b_polyvecm_pointwise_poly_montgomery(poly r[N_PLUS_ELL], const poly *a, const poly v[N_PLUS_ELL]) {
  unsigned int i;

  for(i = 0; i < N_PLUS_ELL; ++i)
    poly_pointwise_montgomery(&r[i], a, &v[i]);
}

/* <-> polyvecl_chknorm (polyvec.c:139); poly_chknorm itself is REUSED. */
static int b_polyvecm_chknorm(const poly v[N_PLUS_ELL], int32_t bound) {
  unsigned int i;

  for(i = 0; i < N_PLUS_ELL; ++i)
    if(poly_chknorm(&v[i], bound))
      return 1;

  return 0;
}

/* <-> polyveck_reduce (polyvec.c:168), K -> n. */
static void b_polyvecn_reduce(poly v[LAS_N]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    poly_reduce(&v[i]);
}

/* <-> polyveck_caddq (polyvec.c:183). */
static void b_polyvecn_caddq(poly v[LAS_N]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    poly_caddq(&v[i]);
}

/* <-> polyveck_add (polyvec.c:200). */
static void b_polyvecn_add(poly w[LAS_N], const poly u[LAS_N], const poly v[LAS_N]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    poly_add(&w[i], &u[i], &v[i]);
}

/* <-> polyveck_sub (polyvec.c:218). */
static void b_polyvecn_sub(poly w[LAS_N], const poly u[LAS_N], const poly v[LAS_N]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    poly_sub(&w[i], &u[i], &v[i]);
}

/* <-> polyveck_ntt (polyvec.c:248). */
static void b_polyvecn_ntt(poly v[LAS_N]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    poly_ntt(&v[i]);
}

/* <-> polyveck_invntt_tomont (polyvec.c:264). */
static void b_polyvecn_invntt_tomont(poly v[LAS_N]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    poly_invntt_tomont(&v[i]);
}

/* <-> polyveck_pointwise_poly_montgomery (polyvec.c:271). */
static void b_polyvecn_pointwise_poly_montgomery(poly r[LAS_N], const poly *a, const poly v[LAS_N]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    poly_pointwise_montgomery(&r[i], a, &v[i]);
}

/* <-> polyveck_pack_w1 (polyvec.c:384): full w instead of w1 codes. */
static void b_polyvecn_pack_w(uint8_t r[LAS_N*LAS_D*4], const poly w[LAS_N]) {
  unsigned int i;

  for(i = 0; i < LAS_N; ++i)
    b_polyw_pack(&r[i*LAS_D*4], &w[i]);
}

/*************************************************
* Name:        det_seed  (deterministic mask seed for base_sign_det; LAS-only)
*
* Description: mask seed = SHAKE256(0x00 || sk || M), 64 bytes.  Tag 0 = sign;
*              basesig binds no statement Y, so this is the tag-0-only twin of
*              ref/las.c's det_seed (which also handles tag 1 = presign, binding
*              Y).  Byte-identical to las.c det_seed(tag=0, sk, NULL, M) -- that
*              equality keeps the ordinary-signature KAT vector stable.
**************************************************/
static void det_seed(uint8_t out[64], const secret_key *sk,
                     const uint8_t *m, size_t mlen) {
  keccak_state state;
  uint8_t skb[N_PLUS_ELL * LAS_D];
  uint8_t tag = 0;                               /* domain: 0 = sign */
  unsigned int i, k;

  for(i = 0; i < N_PLUS_ELL; ++i)                      /* ternary sk -> 1 byte/coeff */
    for(k = 0; k < LAS_D; ++k)
      skb[i * LAS_D + k] = (uint8_t)(int8_t)sk->r[i].coeffs[k];

  shake256_init(&state);
  shake256_absorb(&state, &tag, 1);
  shake256_absorb(&state, skb, sizeof skb);
  shake256_absorb(&state, m, mlen);
  shake256_finalize(&state);
  shake256_squeeze(out, 64, &state);
}

/* ============== end-to-end PACKED-API tier (bytes in/out) ==============
 * The SECOND measured boundary.  The struct functions above are the CORE
 * CRYPTO tier (pure lattice/hash computation); the three functions below
 * are the END-TO-END tier: byte keys and signatures are unpacked/packed
 * INSIDE the call, which is exactly the boundary upstream sign.c exposes
 * (its ONLY API is the packed one -- packing.h is called inside
 * crypto_sign_keypair, crypto_sign_signature_internal and
 * crypto_sign_verify_internal).  Benchmarking this tier is like-for-like
 * with upstream's byte API; benchmarking the struct tier isolates the pure
 * computation cost.  The codec is ref/serialize.{c,h} (the packing.{c,h}
 * twin); ONE codec serves basesig.c and las.c because the object layouts
 * are shared (setup.h).  This tier sits BELOW the helper twins so the
 * sign.c-mirror spine above keeps its one-to-one line correspondence. */

/*************************************************
* Name:        base_keygen_packed  (end-to-end tier of base_keygen)
*
* Description: KeyGen at the byte boundary: run the core KeyGen, then pack
*              both keys inside the call.
*              [REUSED] sign.c:60:
*                  pack_pk(pk, rho, &t1);
*              [REUSED] sign.c:64:
*                  pack_sk(sk, rho, tr, key, &t0, &s1, &s2);
*              WHY a separate function (upstream packs inside crypto_sign_keypair
*              itself): the two-tier split keeps the struct core measurable as
*              pure computation; upstream has no struct-level API to preserve.
*
* Returns 0 (success; a freshly sampled sk is always ternary, so packing
*           cannot fail)
**************************************************/
int base_keygen_packed(uint8_t pk_b[PUBLIC_KEY_BYTES],   /* packed public key (bytes out)  */
                             uint8_t sk_b[SECRET_KEY_BYTES],   /* packed secret key (bytes out)  */
                             const public_params *pp) {           /* paper A: A = [I | A'] (A in pp = (A,H))     */
  public_key pk;
  secret_key sk;

  base_keygen(&pk, &sk, pp);
  pack_public_key(pk_b, &pk);
  return pack_secret_key(sk_b, &sk);
}

/*************************************************
* Name:        base_sign_packed  (end-to-end tier of base_sign)
*
* Description: Sign at the byte boundary: unpack the keys (validating),
*              run the core Sign, pack the signature -- all inside the call.
*              [REUSED] sign.c:108:
*                  unpack_sk(rho, tr, key, &t0, &s1, &s2, sk);
*              [REUSED] sign.c:186:
*                  pack_sig(sig, sig, &z, &h);
*              WHY the pk is unpacked too (upstream only unpacks sk): the
*              paper's oracle is c = H(pk, w, M) with the raw public key,
*              while upstream binds the key digest tr, which travels INSIDE
*              its packed sk.
*
* Returns 0 (success), -1 if a key fails validating decode
**************************************************/
int base_sign_packed(uint8_t sig_b[SIGNATURE_BYTES], /* packed signature (bytes out) */
                               const uint8_t *m,             /* paper M: message             */
                               size_t mlen,                  /* length of M                  */
                               const uint8_t pk_b[PUBLIC_KEY_BYTES], /* packed public key (bytes) */
                               const uint8_t sk_b[SECRET_KEY_BYTES], /* packed secret key (bytes) */
                               const public_params *pp) {           /* paper A: A = [I | A'] (A in pp = (A,H))   */
  public_key pk;
  secret_key sk;
  signature sig;

  if(unpack_public_key(&pk, pk_b))
    return -1;
  if(unpack_secret_key(&sk, sk_b))
    return -1;
  base_sign(&sig, m, mlen, &pk, &sk, pp);
  return pack_signature(sig_b, &sig);   /* in-band by the chknorm gate: 0 */
}

/*************************************************
* Name:        base_verify_packed  (end-to-end tier of base_verify)
*
* Description: Verify at the byte boundary: validating decode of pk and
*              signature, then the core Verify -- all inside the call.
*              [REUSED] sign.c:311:
*                  unpack_pk(rho, &t1, pk);
*              [REUSED] sign.c:312:
*                  if(unpack_sig(c, &z, &h, sig))
*                    return -1;
*              WHY validating (upstream's unpack_pk cannot fail): the codec's
*              decoder defensively rejects malformed bytes (coeff >= Q,
*              invalid ternary code, out-of-band z) -- the stance an
*              on-chain verifier must take.
*
* Returns 0 if signature could be verified correctly and -1 otherwise
**************************************************/
int base_verify_packed(const uint8_t sig_b[SIGNATURE_BYTES], /* packed signature (bytes)  */
                            const uint8_t *m,                   /* paper M: message          */
                            size_t mlen,                        /* length of M               */
                            const uint8_t pk_b[PUBLIC_KEY_BYTES],   /* packed public key (bytes) */
                            const public_params *pp) {                 /* paper A: A = [I | A'] (A in pp = (A,H)) */
  public_key pk;
  signature sig;

  if(unpack_public_key(&pk, pk_b))
    return -1;
  if(unpack_signature(&sig, sig_b))
    return -1;
  return base_verify(&sig, m, mlen, &pk, pp);
}
