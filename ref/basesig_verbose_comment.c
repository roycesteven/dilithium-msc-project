/*
 * basesig.c -- simplified Dilithium-style BASE signature = Algorithm 1 of
 * eprint 2020/845, written as a STRUCTURAL MIRROR of the upstream ML-DSA
 * reference ref/sign.c so the diff shows exactly what Algorithm 1 removes.
 *
 *   base_sign_keypair    <->  crypto_sign_keypair            (sign.c:23)
 *   base_sign_signature  <->  crypto_sign_signature_internal (sign.c:85)
 *   base_sign_verify     <->  crypto_sign_verify_internal    (sign.c:289)
 *
 * Each scheme function below carries per-block annotations against the
 * corresponding sign.c lines:
 *   [REUSED]  the sign.c idiom/primitive is used unchanged;
 *   [CHANGED] adapted for Algorithm 1 (different dimensions/distribution/hash);
 *   [DELETED] an ML-DSA size-optimisation Algorithm 1 drops (paper s2.2/s3.2).
 *
 * Standalone: depends on las.h for the shared parameters/types only.  las.c is
 * NOT linked by this file -- the static helpers are local copies, kept
 * behaviourally IDENTICAL to las.c's so that A*r and the challenge hash
 * H(pk,w,M) match las.c bit-for-bit and a LAS-adapted signature verifies here.
 */
#include <stddef.h>
#include <stdint.h>
#include "params.h"
#include "basesig.h"
#include "poly.h"
#include "randombytes.h"
#include "fips202.h"

/* Rejection-sampling attempt counter (measurement only; see basesig.h).
 * No sign.c analogue -- instrumentation added for the benchmark. */
unsigned long base_attempts = 0;

/* ============================ helpers (local copies) ============================
 * Behaviourally identical to las.c's static helpers (duplicated on purpose so
 * basesig is a standalone translation unit).  Correspondence to sign.c/poly.c:
 *   b_sample_ternary   <->  polyvecl/k_uniform_eta   (sign.c:44-45) -- CHANGED (S_1)
 *   b_sample_Sgamma    <->  polyvecl_uniform_gamma1  (sign.c:134)   -- CHANGED (S_gamma)
 *   b_challenge        <->  poly_challenge           (sign.c:153)   -- CHANGED (kappa)
 *   b_Amul             <->  polyvec_matrix_pointwise_montgomery (+ identity block)
 *   b_polymul_prehat   <->  polyvecl_pointwise_poly_montgomery + invntt_tomont
 *   b_hash_challenge   <->  the mu||w1 SHAKE + poly_challenge (sign.c:148-153) -- hashes FULL w
 *   b_pack_poly_canon / b_chknorm_vec / b_poly_equal : small local utilities. */

/* Pack one polynomial into 4 bytes/coeff (canonical [0,Q)) for hashing. */
static void b_pack_poly_canon(uint8_t out[N*4], const poly *a) {
  unsigned int i;
  uint32_t x;
  poly t = *a;
  poly_reduce(&t);
  poly_caddq(&t);
  for(i = 0; i < N; ++i) {
    x = (uint32_t)t.coeffs[i];
    out[4*i+0] = (uint8_t)x;
    out[4*i+1] = (uint8_t)(x >> 8);
    out[4*i+2] = (uint8_t)(x >> 16);
    out[4*i+3] = (uint8_t)(x >> 24);
  }
}

static int b_poly_equal(const poly *a, const poly *b) {
  unsigned int i;
  for(i = 0; i < N; ++i)
    if(a->coeffs[i] != b->coeffs[i])
      return 0;
  return 1;
}

/* Reject if any component has ||.||inf >= B (uses the reused poly_chknorm). */
static int b_chknorm_vec(const poly z[LAS_M], int32_t B) {
  unsigned int j;
  for(j = 0; j < LAS_M; ++j)
    if(poly_chknorm(&z[j], B))
      return 1;
  return 0;
}

/* Second half of the NTT product out = a*b (operands already NTT'd); callers
 * hoist the transforms exactly as sign.c does. */
static void b_polymul_prehat(poly *out, const poly *ahat, const poly *bhat) {
  poly_pointwise_montgomery(out, ahat, bhat);
  poly_invntt_tomont(out);
  poly_reduce(out);
}

/* w = A*v = v_top + A'*v_bot, A=[I|A'], A' (pp->mat) already in NTT domain.
 * The A'*v_bot part is sign.c's polyvec_matrix_pointwise_montgomery; the
 * identity block (+ v_top) is what makes A Hermite-normal-form (no separate s2). */
static void b_Amul(poly w[LAS_N], const las_pp *pp, const poly v[LAS_M]) {
  poly vhat[LAS_ELL], tmp, acc;
  unsigned int i, j, k;

  for(j = 0; j < LAS_ELL; ++j) {
    vhat[j] = v[LAS_N + j];
    poly_ntt(&vhat[j]);
  }
  for(i = 0; i < LAS_N; ++i) {
    for(k = 0; k < N; ++k)
      acc.coeffs[k] = 0;
    for(j = 0; j < LAS_ELL; ++j) {
      poly_pointwise_montgomery(&tmp, &pp->mat[i][j], &vhat[j]);
      poly_add(&acc, &acc, &tmp);
    }
    poly_reduce(&acc);
    poly_invntt_tomont(&acc);
    poly_add(&w[i], &acc, &v[i]);   /* identity block */
    poly_reduce(&w[i]);
    poly_caddq(&w[i]);
  }
}

/* H: challenge poly with ||c||_1 = LAS_KAPPA, ||c||inf = 1.  Same SampleInBall
 * construction as sign.c's poly_challenge, re-instantiated with kappa. */
static void b_challenge(poly *c, const uint8_t seed[LAS_SEEDBYTES]) {
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

/* c = H(pk, commit, M).  Algorithm 1 hashes the FULL commitment w -- this is
 * sign.c's mu||w1Encode(w1) SHAKE (sign.c:148-153) with the DECOMPOSE deleted:
 * no high-bits split, the whole w is bound. */
static void b_hash_challenge(poly *c, const las_pk *pk, const poly commit[LAS_N],
                             const uint8_t *m, size_t mlen) {
  keccak_state state;
  uint8_t buf[N*4];
  uint8_t seed[LAS_SEEDBYTES];
  unsigned int i;

  shake256_init(&state);
  for(i = 0; i < LAS_N; ++i) {
    b_pack_poly_canon(buf, &pk->t[i]);
    shake256_absorb(&state, buf, N*4);
  }
  for(i = 0; i < LAS_N; ++i) {
    b_pack_poly_canon(buf, &commit[i]);
    shake256_absorb(&state, buf, N*4);
  }
  shake256_absorb(&state, m, mlen);
  shake256_finalize(&state);
  shake256_squeeze(seed, LAS_SEEDBYTES, &state);
  b_challenge(c, seed);
}

/* Sample one poly with coefficients uniform in [-GAMMA, GAMMA] (set S_g).
 * Algorithm 1's mask -- replaces sign.c's polyvecl_uniform_gamma1 (whose gamma1
 * is a fixed power of two) with the paper's S_gamma, gamma = kappa*d*(n+l). */
static void b_sample_Sgamma(poly *y, const uint8_t *seed, size_t seedlen, uint16_t nonce) {
  keccak_state state;
  uint8_t buf[SHAKE256_RATE];
  uint8_t nb[2];
  uint32_t t, gmask;
  unsigned int ctr = 0, pos = 0;

  gmask = 1;
  while(gmask < 2u*(uint32_t)LAS_GAMMA)
    gmask <<= 1;
  gmask -= 1;

  nb[0] = (uint8_t)nonce;
  nb[1] = (uint8_t)(nonce >> 8);
  shake256_init(&state);
  shake256_absorb(&state, seed, seedlen);
  shake256_absorb(&state, nb, 2);
  shake256_finalize(&state);
  shake256_squeezeblocks(buf, 1, &state);

  while(ctr < N) {
    if(pos + 3 > SHAKE256_RATE) {
      shake256_squeezeblocks(buf, 1, &state);
      pos = 0;
    }
    t  = buf[pos];
    t |= (uint32_t)buf[pos+1] << 8;
    t |= (uint32_t)buf[pos+2] << 16;
    pos += 3;
    t &= gmask;
    if(t < 2u*(uint32_t)LAS_GAMMA + 1u)
      y->coeffs[ctr++] = (int32_t)t - LAS_GAMMA;
  }
}

/* Sample one poly with coefficients uniform in {-1,0,1} (set S_1, ternary).
 * Algorithm 1's secret -- replaces sign.c's polyvecl/k_uniform_eta (eta=2/4)
 * with the ternary S_1 the paper requires (||r||inf <= 1). */
static void b_sample_ternary(poly *r, const uint8_t *seed, size_t seedlen, uint16_t nonce) {
  keccak_state state;
  uint8_t buf[SHAKE256_RATE];
  uint8_t nb[2], byte, v, s;
  unsigned int ctr = 0, pos = 0;

  nb[0] = (uint8_t)nonce;
  nb[1] = (uint8_t)(nonce >> 8);
  shake256_init(&state);
  shake256_absorb(&state, seed, seedlen);
  shake256_absorb(&state, nb, 2);
  shake256_finalize(&state);
  shake256_squeezeblocks(buf, 1, &state);

  while(ctr < N) {
    if(pos >= SHAKE256_RATE) {
      shake256_squeezeblocks(buf, 1, &state);
      pos = 0;
    }
    byte = buf[pos++];
    for(s = 0; s < 4 && ctr < N; ++s) {
      v = (byte >> (2*s)) & 3;          /* 2 bits: {0,1,2}->{-1,0,1}, reject 3 */
      if(v < 3)
        r->coeffs[ctr++] = (int32_t)v - 1;
    }
  }
}

/* ============================ scheme (Algorithm 1) ============================ */

/* base_sign_keypair  <->  crypto_sign_keypair (sign.c:23-67), Algorithm 1 KeyGen.
 * Block-by-block against sign.c:
 *   [REUSED ] randombytes seed              (sign.c:32)
 *   [DELETED] polyvec_matrix_expand(mat,rho)(sign.c:41)  A=[I|A'] is fixed in pp
 *   [CHANGED] eta-sample s1,s2  ->  ternary r <- S_1^{n+l}   (sign.c:44-45)
 *   [CHANGED] t = A s1 + s2      ->  t = A r  (identity block replaces s2) (sign.c:47-55)
 *   [DELETED] caddq + power2round(t)->t1,t0  (sign.c:58-59)  no key compression
 *   [DELETED] pack_pk / H(pk)->tr / pack_sk  (sign.c:60-64)  struct output, no tr */
void base_sign_keypair(las_pk *pk,          /* paper t: pk->t = t = A r (public key)       */
                       las_sk *sk,          /* paper r: sk->s = r  (secret key, r <-$ S_1) */
                       const las_pp *pp) {  /* paper A: pp = A = [I | A'] (public matrix)  */
  /* [PAPER Alg.1] 1:  procedure KeyGen():    // same as Gen */
  uint8_t seed[LAS_SEEDBYTES];   /* PRG seed used to sample r (no paper symbol)      */
  unsigned int j;                /* index over the n+ℓ components (no paper symbol)  */

  randombytes(seed, LAS_SEEDBYTES);                 /* [REUSED]  sign.c:32     */
  /* [PAPER Alg.1] 2:      r ←$ S₁^(n+ℓ) */
  for(j = 0; j < LAS_M; ++j)                        /* [CHANGED] ternary r     */
    b_sample_ternary(&sk->s[j], seed, LAS_SEEDBYTES, (uint16_t)j);
  /* [PAPER Alg.1] 3:      t = A r */
  b_Amul(pk->t, pp, sk->s);                         /* [CHANGED] t = A r       */
  /* [PAPER Alg.1] 4:      return (pk, sk) = (t, r) */
  /* [PAPER Alg.1] 5:  end procedure */
}

/* base_sign_signature  <->  crypto_sign_signature_internal (sign.c:85-189),
 * Algorithm 1 Sign.  Block-by-block against sign.c:
 *   [DELETED] unpack_sk                      (sign.c:108)  sk is a struct
 *   [CHANGED] mu/rhoprime CRH chain -> fresh 64-byte mask seed (sign.c:110-124)
 *   [CHANGED] NTT(s1) once before loop -> NTT(r) once   (sign.c:128, hoisted)
 *   [DELETED] NTT(s2), NTT(t0)               (sign.c:129-130) no s2/t0
 *   [REUSED ] the `rej:` rejection loop      (sign.c:132)
 *   [CHANGED] uniform_gamma1(y) -> S_gamma(y)(sign.c:134)
 *   [CHANGED] w = A y (via b_Amul)           (sign.c:136-141)
 *   [DELETED] caddq + decompose(w)->w1,w0    (sign.c:144-145) hash full w
 *   [CHANGED] c = H(pk, w, M) (full w)       (sign.c:146-153)
 *   [REUSED ] NTT(c) once per attempt        (sign.c:154, hoisted)
 *   [CHANGED] z = y + c r; reject |z|inf>g-k (sign.c:157-162)
 *   [DELETED] low-bits check  ||w0-cs2||     (sign.c:164-171) second rejection
 *   [DELETED] hint  MakeHint(cs2,ct0)+OMEGA  (sign.c:173-183) hint vector
 *   [DELETED] pack_sig                       (sign.c:186)  struct output */
void base_sign_signature(las_sig *sig,       /* paper σ: output signature σ = (c, z)  */
                         const uint8_t *m,   /* paper M: message                      */
                         size_t mlen,        /* length of M (no paper symbol)         */
                         const las_pk *pk,   /* paper t: pk->t = t (public key)       */
                         const las_sk *sk,   /* paper r: sk->s = r (secret key)       */
                         const las_pp *pp) { /* paper A: pp = A = [I | A']            */
  /* [PAPER Alg.1] 6:  procedure Sign((pk, sk), M): */
  uint8_t seed[64];      /* PRG mask seed (no paper symbol)                 */
  uint16_t nonce = 0;    /* PRG counter (no paper symbol)                   */
  unsigned int j;        /* index over n+ℓ components (no paper symbol)     */
  poly y[LAS_M];         /* paper y: mask, y <-$ Sγ^(n+ℓ)                   */
  poly w[LAS_N];         /* paper w: commitment, w = A y                    */
  poly rhat[LAS_M];      /* paper r in NTT domain: NTT(r) (hoisted)         */
  poly chat;             /* paper c in NTT domain: NTT(c) (hoisted)         */
  poly cr;               /* paper c·r: the product c r                      */
  poly c;                /* paper c: challenge c = H(pk, w, M)              */

  randombytes(seed, 64);                            /* [CHANGED] mask seed     */
  for(j = 0; j < LAS_M; ++j) {                      /* [CHANGED] NTT(r) once   */
    rhat[j] = sk->s[j];                             /*           (sign.c:128)  */
    poly_ntt(&rhat[j]);
  }
  for(;;) {                                         /* [REUSED]  rej: loop     */
    ++base_attempts;                                /* instrumentation only    */
    /* [PAPER Alg.1] 7:      y ←$ Sγ^(n+ℓ) */
    for(j = 0; j < LAS_M; ++j)                      /* [CHANGED] y <- S_gamma  */
      b_sample_Sgamma(&y[j], seed, 64, nonce++);
    /* [PAPER Alg.1] 8:      w = A y */
    b_Amul(w, pp, y);                               /* [CHANGED] w = A y       */
    /* [PAPER Alg.1] 9:      c = H(pk, w, M) */
    b_hash_challenge(&c, pk, w, m, mlen);           /* [CHANGED] c = H(pk,w,M) */
    chat = c;                                       /* [REUSED]  NTT(c) once   */
    poly_ntt(&chat);                                /*           (sign.c:154)  */
    /* [PAPER Alg.1] 10:     z = y + c r, where r := sk */
    for(j = 0; j < LAS_M; ++j) {                    /* [CHANGED] z = y + c r   */
      b_polymul_prehat(&cr, &chat, &rhat[j]);
      poly_add(&sig->z[j], &y[j], &cr);
      poly_reduce(&sig->z[j]);
    }
    /* [PAPER Alg.1] 11:     if ||z||∞ > γ − κ, then Restart */
    if(b_chknorm_vec(sig->z, LAS_BOUND_SIGN))       /* reject |z|inf > g-k     */
      continue;                                     /*           (sign.c:161)  */
    /* [PAPER Alg.1] 12:     return σ = (c, z) */
    sig->c = c;
    return;
  }
  /* [PAPER Alg.1] 13: end procedure */
}

/* base_sign_verify  <->  crypto_sign_verify_internal (sign.c:289-358),
 * Algorithm 1 Verify.  Block-by-block against sign.c:
 *   [DELETED] unpack_pk / unpack_sig         (sign.c:311-312) struct input
 *   [CHANGED] reject |z|inf > g-k            (sign.c:314)
 *   [CHANGED] mu = H(pk||M) (bind pk direct) (sign.c:318-324)
 *   [REUSED ] NTT(c) once, NTT(z) via b_Amul (sign.c:327-333)
 *   [CHANGED] w' = A z - c t (exact)         (sign.c:326-340)
 *   [DELETED] shiftl(t1) + c*t1*2^d          (sign.c:334-336) t not compressed
 *   [DELETED] caddq + use_hint(w1,h)         (sign.c:343-344) w' is exact
 *   [CHANGED] c2 = H(pk, w', M); accept c==c2(sign.c:345-355) */
int base_sign_verify(const las_sig *sig,  /* paper σ: sig = (c, z), signature to verify */
                     const uint8_t *m,    /* paper M: message                          */
                     size_t mlen,         /* length of M (no paper symbol)             */
                     const las_pk *pk,    /* paper t: pk->t = t (public key)           */
                     const las_pp *pp) {  /* paper A: pp = A = [I | A']                */
  /* [PAPER Alg.1] 14: procedure Verify(pk, σ, M): */
  poly w[LAS_N];  /* paper w′: recomputed commitment, w′ = A z − c t          */
  poly chat;      /* paper c in NTT domain: NTT(c) (hoisted)                  */
  poly that;      /* paper t in NTT domain: NTT(t) (hoisted)                  */
  poly ct;        /* paper c·t: the product c t                               */
  poly c2;        /* paper H(pk, w′, M): recomputed challenge, compared to c  */
  unsigned int j; /* index over n components (no paper symbol)                */

  /* [PAPER Alg.1] 15:     Parse (c, z) := σ */
  /* [PAPER Alg.1] 16:     if ||z||∞ > γ − κ, then return 0 */
  if(b_chknorm_vec(sig->z, LAS_BOUND_SIGN))         /* [CHANGED] |z|inf > g-k  */
    return -1;

  /* [PAPER Alg.1] 17:     w′ = A z − c t, where t := pk */
  b_Amul(w, pp, sig->z);                            /* [REUSED]  A z           */
  chat = sig->c;                                    /* [REUSED]  NTT(c) once   */
  poly_ntt(&chat);                                  /*           (sign.c:333)  */
  for(j = 0; j < LAS_N; ++j) {                      /* [CHANGED] w' = A z - c t*/
    that = pk->t[j];
    poly_ntt(&that);
    b_polymul_prehat(&ct, &chat, &that);
    poly_sub(&w[j], &w[j], &ct);
    poly_reduce(&w[j]);
    poly_caddq(&w[j]);
  }
  /* [PAPER Alg.1] 18:     if c ≠ H(pk, w′, M), then return 0 */
  /* [PAPER Alg.1] 19:     return 1 */
  b_hash_challenge(&c2, pk, w, m, mlen);            /* [CHANGED] c2 = H(pk,w',M)*/
  return b_poly_equal(&c2, &sig->c) ? 0 : -1;      /* accept iff c == c2       */
  /* [PAPER Alg.1] 20: end procedure */
}
