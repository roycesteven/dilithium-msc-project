#include <stddef.h>
#include <stdint.h>
#include "params.h"
#include "las.h"
#include "poly.h"
#include "randombytes.h"
#include "fips202.h"

/* Rejection-sampling attempt counter (measurement only; see las.h).
 * Incremented once per rejection-loop iteration in las_signature/las_presign/
 * las_presign_k.  Never read by the scheme itself. */
unsigned long las_attempts = 0;

/* Exact expected attempts/call for the rejection loop at `bound` (see las.h
 * for the derivation).  Instrumentation only -- never called by the scheme.
 * p^((n+ell)*d) via square-and-multiply instead of libm pow(), so las.c keeps
 * zero dependencies beyond the reused Dilithium primitives. */
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

/* ============================ helpers ============================ */

/* Pack one polynomial into 4 bytes/coeff (canonical [0,Q)) for hashing. */
static void pack_poly_canon(uint8_t out[N*4], const poly *a) {
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

static int poly_equal(const poly *a, const poly *b) {
  unsigned int i;
  for(i = 0; i < N; ++i)
    if(a->coeffs[i] != b->coeffs[i])
      return 0;
  return 1;
}

/* Reject if any component has ||.||inf >= B. */
static int chknorm_vec(const poly z[LAS_M], int32_t B) {
  unsigned int j;
  for(j = 0; j < LAS_M; ++j)
    if(poly_chknorm(&z[j], B))
      return 1;
  return 0;
}

/* Second half of the NTT product out = a*b mod (X^N+1, Q), centred: both
 * operands are ALREADY in the NTT domain.  Callers hoist the transforms the
 * same way upstream does -- the invariant operand (secret r, public t) is
 * NTT'd once per call (ref/sign.c:128-130 polyvecl_ntt(&s1) etc. before the
 * rej loop; ml_dsa.rs pre-computes s_1_hat_mont in the key struct) and the
 * challenge once per attempt / per verify (ref/sign.c:154, :333 poly_ntt(&cp))
 * -- instead of re-transforming both operands inside every product. */
static void polymul_prehat(poly *out, const poly *ahat, const poly *bhat) {
  poly_pointwise_montgomery(out, ahat, bhat);
  poly_invntt_tomont(out);
  poly_reduce(out);
}

/* w = A*v = v_top + A'*v_bot, with A=[I|A'], A' (pp->mat) already in NTT domain.
 * Output is canonical [0,Q) (used both as commitment and inside arithmetic). */
static void las_Amul(poly w[LAS_N], const las_pp *pp, const poly v[LAS_M]) {
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

/* H: challenge poly with ||c||_1 = LAS_KAPPA, ||c||inf = 1.
 * Same construction as Dilithium's poly_challenge but with kappa fixed here. */
static void las_challenge(poly *c, const uint8_t seed[LAS_SEEDBYTES]) {
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

/* c = H(pk, commit, M) where commit is the (already w or w+Y) commitment. */
static void hash_challenge(poly *c, const las_pk *pk, const poly commit[LAS_N],
                           const uint8_t *m, size_t mlen) {
  keccak_state state;
  uint8_t buf[N*4];
  uint8_t seed[LAS_SEEDBYTES];
  unsigned int i;

  shake256_init(&state);
  for(i = 0; i < LAS_N; ++i) {
    pack_poly_canon(buf, &pk->t[i]);
    shake256_absorb(&state, buf, N*4);
  }
  for(i = 0; i < LAS_N; ++i) {
    pack_poly_canon(buf, &commit[i]);
    shake256_absorb(&state, buf, N*4);
  }
  shake256_absorb(&state, m, mlen);
  shake256_finalize(&state);
  shake256_squeeze(seed, LAS_SEEDBYTES, &state);
  las_challenge(c, seed);
}

/* Sample one poly with coefficients uniform in [-GAMMA, GAMMA] (set S_g). */
static void sample_Sgamma(poly *y, const uint8_t *seed, size_t seedlen, uint16_t nonce) {
  keccak_state state;
  uint8_t buf[SHAKE256_RATE];
  uint8_t nb[2];
  uint32_t t, gmask;
  unsigned int ctr = 0, pos = 0;

  /* Smallest (2^k - 1) >= 2*GAMMA: the rejection window for this parameter set.
   * For the paper set (GAMMA=122880) this is 0x3FFFF (18 bits), unchanged; for
   * larger sets GAMMA grows and the window widens automatically.  We read 3 bytes
   * (24 bits) per attempt, which covers every supported set (2*GAMMA < 2^24). */
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

/* Sample one poly with coefficients uniform in {-1,0,1} (set S_1, ternary). */
static void sample_ternary(poly *r, const uint8_t *seed, size_t seedlen, uint16_t nonce) {
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

/* ============================ scheme ============================ */

void las_setup(las_pp *pp,          /* paper A: pp = A = [I | A']; pp->mat = A' (NTT domain) */
               const uint8_t seed[LAS_SEEDBYTES]) {  /* public seed expanding A' (no paper symbol) */
  unsigned int i, j;  /* row / column indices over A' (no paper symbol) */
  for(i = 0; i < LAS_SEEDBYTES; ++i)
    pp->seed[i] = seed[i];
  for(i = 0; i < LAS_N; ++i)
    for(j = 0; j < LAS_ELL; ++j)
      poly_uniform(&pp->mat[i][j], seed, (uint16_t)((i << 8) + j));
}

void las_keypair_seed(las_pk *pk,            /* paper t: pk->t = t = A r (public key)       */
                     las_sk *sk,            /* paper r: sk->s = r  (secret key, r <-$ S_1) */
                     const las_pp *pp,      /* paper A: pp = A = [I | A'] (public matrix)  */
                     const uint8_t seed[LAS_SEEDBYTES]) {  /* PRG seed to sample r (no paper symbol) */
  /* [PAPER Alg.1] 1:  procedure KeyGen():    // same as Gen */
  unsigned int j;   /* index over the n+ℓ components (no paper symbol) */
  /* [PAPER Alg.1] 2:      r ←$ S₁^(n+ℓ) */
  for(j = 0; j < LAS_M; ++j)
    sample_ternary(&sk->s[j], seed, LAS_SEEDBYTES, (uint16_t)j);
  /* [PAPER Alg.1] 3:      t = A r */
  las_Amul(pk->t, pp, sk->s);
  /* [PAPER Alg.1] 4:      return (pk, sk) = (t, r) */
  /* [PAPER Alg.1] 5:  end procedure */
}

void las_keypair(las_pk *pk,          /* paper t: pk->t = t = A r (public key) */
                las_sk *sk,          /* paper r: sk->s = r (secret key)       */
                const las_pp *pp) {  /* paper A: pp = A = [I | A']            */
  uint8_t seed[LAS_SEEDBYTES];  /* PRG seed to sample r (no paper symbol) */
  randombytes(seed, LAS_SEEDBYTES);              /* fresh randomness */
  las_keypair_seed(pk, sk, pp, seed);
}

/* Deterministic per-(pre)signature mask randomness: seed = SHAKE256(tag, sk, [Y], M).
 * Makes (pre)signing a deterministic function of its inputs - reproducible KATs and
 * no fresh per-signature randomness to mishandle (nonce-reuse safety). */
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
      pack_poly_canon(buf, &Y->t[i]);
      shake256_absorb(&state, buf, N * 4);
    }
  shake256_absorb(&state, m, mlen);
  shake256_finalize(&state);
  shake256_squeeze(out, 64, &state);
}

/* Shared Sign body, parameterised by the 64-byte mask seed (random or derived).
 * NTT hoisting mirrors ref/sign.c crypto_sign_signature_internal: the secret is
 * invariant across rejection attempts, so NTT(s_j) is paid once per call
 * (sign.c:128 polyvecl_ntt(&s1) before the rej loop), and the challenge is
 * shared by all n+ell products, so NTT(c) is paid once per attempt
 * (sign.c:154 poly_ntt(&cp)). */
static void las_signature_internal(las_sig *sig,       /* paper σ: output signature σ = (c, z)  */
                      const uint8_t *m,   /* paper M: message                      */
                      size_t mlen,        /* length of M (no paper symbol)         */
                      const las_pk *pk,   /* paper t: pk->t = t (public key)       */
                      const las_sk *sk,   /* paper r: sk->s = r (secret key)       */
                      const las_pp *pp,   /* paper A: pp = A = [I | A']            */
                      const uint8_t seed[64]) {  /* PRG mask seed (no paper symbol) */
  /* [PAPER Alg.1] 6:  procedure Sign((pk, sk), M): */
  uint16_t nonce = 0;    /* PRG counter (no paper symbol)                */
  unsigned int j;        /* index over n+ℓ components (no paper symbol)  */
  poly y[LAS_M];         /* paper y: mask, y <-$ Sγ^(n+ℓ)                */
  poly w[LAS_N];         /* paper w: commitment, w = A y                 */
  poly shat[LAS_M];      /* paper r in NTT domain: NTT(r) (hoisted)      */
  poly chat;             /* paper c in NTT domain: NTT(c) (hoisted)      */
  poly cr;               /* paper c·r: the product c r                   */
  poly c;                /* paper c: challenge c = H(pk, w, M)           */

  for(j = 0; j < LAS_M; ++j) {                /* NTT(s) once per call */
    shat[j] = sk->s[j];
    poly_ntt(&shat[j]);
  }

  for(;;) {
    ++las_attempts;                           /* instrumentation only */
    /* [PAPER Alg.1] 7:      y ←$ Sγ^(n+ℓ) */
    for(j = 0; j < LAS_M; ++j)
      sample_Sgamma(&y[j], seed, 64, nonce++);
    /* [PAPER Alg.1] 8:      w = A y */
    las_Amul(w, pp, y);                       /* w = A y           */
    /* [PAPER Alg.1] 9:      c = H(pk, w, M) */
    hash_challenge(&c, pk, w, m, mlen);        /* c = H(pk, w, M)   */
    chat = c;                                  /* NTT(c) once per attempt */
    poly_ntt(&chat);
    /* [PAPER Alg.1] 10:     z = y + c r, where r := sk */
    for(j = 0; j < LAS_M; ++j) {               /* z = y + c r       */
      polymul_prehat(&cr, &chat, &shat[j]);
      poly_add(&sig->z[j], &y[j], &cr);
      poly_reduce(&sig->z[j]);
    }
    /* [PAPER Alg.1] 11:     if ||z||∞ > γ − κ, then Restart */
    if(chknorm_vec(sig->z, LAS_BOUND_SIGN))
      continue;
    /* [PAPER Alg.1] 12:     return σ = (c, z) */
    sig->c = c;
    return;
  }
  /* [PAPER Alg.1] 13: end procedure */
}

void las_signature(las_sig *sig,        /* paper σ: output signature σ = (c, z) */
              const uint8_t *m,    /* paper M: message                     */
              size_t mlen,         /* length of M (no paper symbol)        */
              const las_pk *pk,    /* paper t: pk->t = t (public key)      */
              const las_sk *sk,    /* paper r: sk->s = r (secret key)      */
              const las_pp *pp) {  /* paper A: pp = A = [I | A']           */
  uint8_t seed[64];  /* PRG mask seed (no paper symbol) */
  randombytes(seed, 64);
  las_signature_internal(sig, m, mlen, pk, sk, pp, seed);
}

void las_signature_det(las_sig *sig,        /* paper σ: output signature σ = (c, z) */
                  const uint8_t *m,    /* paper M: message                     */
                  size_t mlen,         /* length of M (no paper symbol)        */
                  const las_pk *pk,    /* paper t: pk->t = t (public key)      */
                  const las_sk *sk,    /* paper r: sk->s = r (secret key)      */
                  const las_pp *pp) {  /* paper A: pp = A = [I | A']           */
  uint8_t seed[64];  /* PRG mask seed, derived from (sk, M) (no paper symbol) */
  det_seed(seed, 0, sk, NULL, m, mlen);        /* tag 0 = sign (no statement) */
  las_signature_internal(sig, m, mlen, pk, sk, pp, seed);
}

int las_verify(const las_sig *sig,  /* paper σ: sig = (c, z), signature to verify */
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
  if(chknorm_vec(sig->z, LAS_BOUND_SIGN))
    return -1;

  /* [PAPER Alg.1] 17:     w′ = A z − c t, where t := pk */
  las_Amul(w, pp, sig->z);                     /* A z               */
  chat = sig->c;                                /* NTT(c) once per call */
  poly_ntt(&chat);                              /* (mirrors ref/sign.c:333) */
  for(j = 0; j < LAS_N; ++j) {                  /* w' = A z - c t    */
    that = pk->t[j];
    poly_ntt(&that);
    polymul_prehat(&ct, &chat, &that);
    poly_sub(&w[j], &w[j], &ct);
    poly_reduce(&w[j]);
    poly_caddq(&w[j]);
  }
  /* [PAPER Alg.1] 18:     if c ≠ H(pk, w′, M), then return 0 */
  /* [PAPER Alg.1] 19:     return 1 */
  hash_challenge(&c2, pk, w, m, mlen);
  return poly_equal(&c2, &sig->c) ? 0 : -1;
  /* [PAPER Alg.1] 20: end procedure */
}

/* Shared PreSign body: like las_signature_internal but hashes (w+Y) and rejects at `bound`
 * (g-k-1 single-hop, or g-k-K for AMHL).  Parameterised by the mask seed.
 * Same NTT hoisting as las_signature_internal (see the comment there). */
static void las_presign_internal(las_sig *presig,   /* paper σ̂: output pre-signature σ̂ = (c, ẑ)    */
                         const uint8_t *m,  /* paper M: message                            */
                         size_t mlen,       /* length of M (no paper symbol)               */
                         const las_pk *Y,   /* paper t′ := Y: statement, Y->t = Y = A y_wit */
                         const las_pk *pk,  /* paper t: pk->t = t (public key)             */
                         const las_sk *sk,  /* paper r: sk->s = r (secret key)             */
                         const las_pp *pp,  /* paper A: pp = A = [I | A']                  */
                         int32_t bound,     /* paper γ−κ−1 (single-hop) / γ−κ−K (AMHL)     */
                         const uint8_t seed[64]) {  /* PRG mask seed (no paper symbol) */
  /* [PAPER Alg.2] 1:  procedure PreSign((pk, sk), Y, M): */
  uint16_t nonce = 0;    /* PRG counter (no paper symbol)                   */
  unsigned int j;        /* index over n+ℓ / n components (no paper symbol) */
  poly y[LAS_M];         /* paper y: mask, y <-$ Sγ^(n+ℓ)                   */
  poly w[LAS_N];         /* paper w: commitment, w = A y                    */
  poly wY[LAS_N];        /* paper w + t′: the hashed commitment w + Y       */
  poly shat[LAS_M];      /* paper r in NTT domain: NTT(r) (hoisted)         */
  poly chat;             /* paper c in NTT domain: NTT(c) (hoisted)         */
  poly cr;               /* paper c·r: the product c r                      */
  poly c;                /* paper c: challenge c = H(pk, w + t′, M)         */

  for(j = 0; j < LAS_M; ++j) {                  /* NTT(s) once per call */
    shat[j] = sk->s[j];
    poly_ntt(&shat[j]);
  }

  for(;;) {
    ++las_attempts;                             /* instrumentation only */
    /* [PAPER Alg.2] 2:      y ←$ Sγ^(n+ℓ) */
    for(j = 0; j < LAS_M; ++j)
      sample_Sgamma(&y[j], seed, 64, nonce++);
    /* [PAPER Alg.2] 3:      w = A y */
    las_Amul(w, pp, y);                         /* w = A y                 */
    /* [PAPER Alg.2] 4:      c = H(pk, w + t′, M), where t′ := Y */
    for(j = 0; j < LAS_N; ++j) {                 /* commit = w + Y          */
      poly_add(&wY[j], &w[j], &Y->t[j]);
      poly_reduce(&wY[j]);
      poly_caddq(&wY[j]);
    }
    hash_challenge(&c, pk, wY, m, mlen);          /* c = H(pk, w+Y, M)       */
    chat = c;                                     /* NTT(c) once per attempt */
    poly_ntt(&chat);
    /* [PAPER Alg.2] 5:      ẑ = y + c r, where r := sk */
    for(j = 0; j < LAS_M; ++j) {                  /* z^ = y + c r            */
      polymul_prehat(&cr, &chat, &shat[j]);
      poly_add(&presig->z[j], &y[j], &cr);
      poly_reduce(&presig->z[j]);
    }
    /* [PAPER Alg.2] 6:      if ||ẑ||∞ > γ − κ − 1, then Restart */
    if(chknorm_vec(presig->z, bound))
      continue;
    /* [PAPER Alg.2] 7:      return σ̂ = (c, ẑ) */
    presig->c = c;
    return;
  }
  /* [PAPER Alg.2] 8:  end procedure */
}

void las_presign(las_sig *presig,     /* paper σ̂: output pre-signature σ̂ = (c, ẑ) */
                 const uint8_t *m,    /* paper M: message                         */
                 size_t mlen,         /* length of M (no paper symbol)            */
                 const las_pk *Y,     /* paper t′ := Y: statement                 */
                 const las_pk *pk,    /* paper t: pk->t = t (public key)          */
                 const las_sk *sk,    /* paper r: sk->s = r (secret key)          */
                 const las_pp *pp) {  /* paper A: pp = A = [I | A']               */
  uint8_t seed[64];  /* PRG mask seed (no paper symbol) */
  randombytes(seed, 64);
  las_presign_internal(presig, m, mlen, Y, pk, sk, pp, LAS_BOUND_PRESIGN, seed);
}

void las_presign_det(las_sig *presig,     /* paper σ̂: output pre-signature σ̂ = (c, ẑ) */
                     const uint8_t *m,    /* paper M: message                         */
                     size_t mlen,         /* length of M (no paper symbol)            */
                     const las_pk *Y,     /* paper t′ := Y: statement                 */
                     const las_pk *pk,    /* paper t: pk->t = t (public key)          */
                     const las_sk *sk,    /* paper r: sk->s = r (secret key)          */
                     const las_pp *pp) {  /* paper A: pp = A = [I | A']               */
  uint8_t seed[64];  /* PRG mask seed, derived from (sk, Y, M) (no paper symbol) */
  det_seed(seed, 1, sk, Y, m, mlen);            /* tag 1 = presign (binds Y) */
  las_presign_internal(presig, m, mlen, Y, pk, sk, pp, LAS_BOUND_PRESIGN, seed);
}

int las_preverify(const las_sig *presig,  /* paper σ̂: presig = (c, ẑ), pre-sig to verify */
                  const uint8_t *m,       /* paper M: message                            */
                  size_t mlen,            /* length of M (no paper symbol)               */
                  const las_pk *Y,        /* paper t′ := Y: statement, Y->t = Y          */
                  const las_pk *pk,       /* paper t: pk->t = t (public key)             */
                  const las_pp *pp) {     /* paper A: pp = A = [I | A']                  */
  /* [PAPER Alg.2] 9:  procedure PreVerify(Y, pk, σ̂, M): */
  poly w[LAS_N];  /* paper w′: recomputed commitment, w′ = A ẑ − c t          */
  poly wY[LAS_N]; /* paper w′ + t′: the hashed commitment w′ + Y              */
  poly chat;      /* paper c in NTT domain: NTT(c) (hoisted)                  */
  poly that;      /* paper t in NTT domain: NTT(t) (hoisted)                  */
  poly ct;        /* paper c·t: the product c t                               */
  poly c2;        /* paper H(pk, w′ + t′, M): recomputed challenge vs c       */
  unsigned int j; /* index over n components (no paper symbol)                */

  /* [PAPER Alg.2] 10:     Parse (c, ẑ) := σ̂ and t′ := Y */
  /* [PAPER Alg.2] 11:     if ||ẑ||∞ > γ − κ − 1 then */
  /* [PAPER Alg.2] 12:         return 0 */
  /* [PAPER Alg.2] 13:     end if */
  if(chknorm_vec(presig->z, LAS_BOUND_PRESIGN))
    return -1;

  /* [PAPER Alg.2] 14:     w′ = A ẑ − c t, where t := pk */
  las_Amul(w, pp, presig->z);                    /* A z^                    */
  chat = presig->c;                               /* NTT(c) once per call    */
  poly_ntt(&chat);
  for(j = 0; j < LAS_N; ++j) {                    /* w' = A z^ - c t         */
    that = pk->t[j];
    poly_ntt(&that);
    polymul_prehat(&ct, &chat, &that);
    poly_sub(&w[j], &w[j], &ct);
    poly_reduce(&w[j]);
    poly_caddq(&w[j]);
  }
  for(j = 0; j < LAS_N; ++j) {                    /* w' + Y                  */
    poly_add(&wY[j], &w[j], &Y->t[j]);
    poly_reduce(&wY[j]);
    poly_caddq(&wY[j]);
  }
  /* [PAPER Alg.2] 15:     if c ≠ H(pk, w′ + t′, M) then */
  /* [PAPER Alg.2] 16:         return 0 */
  /* [PAPER Alg.2] 17:     end if */
  /* [PAPER Alg.2] 18:     return 1 */
  hash_challenge(&c2, pk, wY, m, mlen);            /* check c == H(pk,w'+Y,M) */
  return poly_equal(&c2, &presig->c) ? 0 : -1;
  /* [PAPER Alg.2] 19: end procedure */
}

/* AMHL K-hop PreSign: identical to las_presign but rejects at the tighter bound
 * g-k-K (LAS_BOUND_PRESIGN_K), reserving norm budget K for the cumulative witness. */
void las_presign_k(las_sig *presig,    /* paper σ̂: output pre-signature σ̂ = (c, ẑ)   */
                   const uint8_t *m,   /* paper M: message                           */
                   size_t mlen,        /* length of M (no paper symbol)              */
                   const las_pk *Y,    /* paper t′ := Y: (cumulative) statement       */
                   const las_pk *pk,   /* paper t: pk->t = t (public key)            */
                   const las_sk *sk,   /* paper r: sk->s = r (secret key)            */
                   const las_pp *pp,   /* paper A: pp = A = [I | A']                 */
                   unsigned int nhops) {  /* paper K: number of AMHL hops (tighter bound γ−κ−K) */
  uint8_t seed[64];  /* PRG mask seed (no paper symbol) */
  randombytes(seed, 64);
  las_presign_internal(presig, m, mlen, Y, pk, sk, pp, LAS_BOUND_PRESIGN_K(nhops), seed);
}

int las_preverify_k(const las_sig *presig,  /* paper σ̂: presig = (c, ẑ), pre-sig to verify */
                    const uint8_t *m,       /* paper M: message                            */
                    size_t mlen,            /* length of M (no paper symbol)               */
                    const las_pk *Y,        /* paper t′ := Y: (cumulative) statement       */
                    const las_pk *pk,       /* paper t: pk->t = t (public key)             */
                    const las_pp *pp,       /* paper A: pp = A = [I | A']                  */
                    unsigned int nhops) {   /* paper K: number of AMHL hops (bound γ−κ−K)  */
  /* [PAPER Alg.2] 9:  procedure PreVerify(Y, pk, σ̂, M): */
  poly w[LAS_N];  /* paper w′: recomputed commitment, w′ = A ẑ − c t          */
  poly wY[LAS_N]; /* paper w′ + t′: the hashed commitment w′ + Y              */
  poly chat;      /* paper c in NTT domain: NTT(c) (hoisted)                  */
  poly that;      /* paper t in NTT domain: NTT(t) (hoisted)                  */
  poly ct;        /* paper c·t: the product c t                               */
  poly c2;        /* paper H(pk, w′ + t′, M): recomputed challenge vs c       */
  unsigned int j; /* index over n components (no paper symbol)                */

  /* [PAPER Alg.2] 10:     Parse (c, ẑ) := σ̂ and t′ := Y */
  /* [PAPER Alg.2] 11:     if ||ẑ||∞ > γ − κ − 1 then */
  /* [PAPER Alg.2] 12:         return 0 */
  /* [PAPER Alg.2] 13:     end if */
  if(chknorm_vec(presig->z, LAS_BOUND_PRESIGN_K(nhops)))
    return -1;

  /* [PAPER Alg.2] 14:     w′ = A ẑ − c t, where t := pk */
  las_Amul(w, pp, presig->z);                    /* A z^                    */
  chat = presig->c;                               /* NTT(c) once per call    */
  poly_ntt(&chat);
  for(j = 0; j < LAS_N; ++j) {                    /* w' = A z^ - c t         */
    that = pk->t[j];
    poly_ntt(&that);
    polymul_prehat(&ct, &chat, &that);
    poly_sub(&w[j], &w[j], &ct);
    poly_reduce(&w[j]);
    poly_caddq(&w[j]);
  }
  for(j = 0; j < LAS_N; ++j) {                    /* w' + Y                  */
    poly_add(&wY[j], &w[j], &Y->t[j]);
    poly_reduce(&wY[j]);
    poly_caddq(&wY[j]);
  }
  /* [PAPER Alg.2] 15:     if c ≠ H(pk, w′ + t′, M) then */
  /* [PAPER Alg.2] 16:         return 0 */
  /* [PAPER Alg.2] 17:     end if */
  /* [PAPER Alg.2] 18:     return 1 */
  hash_challenge(&c2, pk, wY, m, mlen);            /* check c == H(pk,w'+Y,M) */
  return poly_equal(&c2, &presig->c) ? 0 : -1;
  /* [PAPER Alg.2] 19: end procedure */
}

int las_adapt(las_sig *sig,          /* paper σ: output adapted signature σ = (c, ẑ + r′) */
              const las_sig *presig, /* paper σ̂: presig = (c, ẑ)                          */
              const uint8_t *m,      /* paper M: message                                  */
              size_t mlen,           /* length of M (no paper symbol)                     */
              const las_pk *Y,       /* paper t′ := Y: statement                          */
              const las_sk *y,       /* paper (Y,y) witness, r′ := y: y->s = y (A y = Y)  */
              const las_pk *pk,      /* paper t: pk->t = t (public key)                   */
              const las_pp *pp) {    /* paper A: pp = A = [I | A']                        */
  /* [PAPER Alg.2] 20: procedure Adapt((Y, y), pk, σ̂, M): */
  unsigned int j;  /* index over n+ℓ components (no paper symbol) */

  /* [PAPER Alg.2] 21:     if PreVerify(Y, pk, σ̂, M) = 0 then */
  /* [PAPER Alg.2] 22:         return ⊥ */
  /* [PAPER Alg.2] 23:     end if */
  if(las_preverify(presig, m, mlen, Y, pk, pp))
    return -1;

  /* [PAPER Alg.2] 24:     Parse (c, ẑ) := σ̂ and r′ := y */
  /* [PAPER Alg.2] 25:     return σ = (c, ẑ + r′) */
  sig->c = presig->c;
  for(j = 0; j < LAS_M; ++j) {                    /* z = z^ + y_wit          */
    poly_add(&sig->z[j], &presig->z[j], &y->s[j]);
    poly_reduce(&sig->z[j]);
  }
  return 0;
  /* [PAPER Alg.2] 26: end procedure */
}

int las_ext(las_sk *y,               /* paper s: output extracted witness s (y->s = s)   */
            const las_sig *sig,      /* paper σ: sig = (c, z)                            */
            const las_sig *presig,   /* paper σ̂: presig = (ĉ, ẑ)                         */
            const las_pk *Y,         /* paper t′ := Y: statement                         */
            const las_pp *pp) {      /* paper A: pp = A = [I | A']                       */
  /* [PAPER Alg.2] 27: procedure Ext(Y, σ, σ̂): */
  poly Ay[LAS_N]; /* paper A s: the product A s, checked against t′ = Y */
  unsigned int j; /* index over n+ℓ / n components (no paper symbol)    */

  /* [PAPER Alg.2] 28:     Parse (c, z) := σ and (ĉ, ẑ) := σ̂ */
  /* [PAPER Alg.2] 29:     Parse t′ := Y */
  /* [PAPER Alg.2] 30:     s = z − ẑ */
  for(j = 0; j < LAS_M; ++j) {                    /* s = z - z^              */
    poly_sub(&y->s[j], &sig->z[j], &presig->z[j]);
    poly_reduce(&y->s[j]);
  }
  /* [PAPER Alg.2] 31:     if t′ ≠ A s, then return ⊥ */
  las_Amul(Ay, pp, y->s);                          /* check A s == Y          */
  for(j = 0; j < LAS_N; ++j)
    if(!poly_equal(&Ay[j], &Y->t[j]))
      return -1;
  /* [PAPER Alg.2] 32:     return s */
  return 0;
  /* [PAPER Alg.2] 33: end procedure */
}
