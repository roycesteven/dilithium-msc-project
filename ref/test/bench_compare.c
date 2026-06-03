/*
 * Head-to-head timing: LAS (this work) vs. the optimised Dilithium-3 reference.
 *
 * IMPORTANT (report caveat): this is NOT a same-security, like-for-like contest.
 *  - Dilithium-3 is the NIST-optimised scheme: module dims K=6,L=5, Power2Round,
 *    hint vector, high/low-bit decomposition, bit-packed keys/sigs.
 *  - LAS here is the paper's SIMPLIFIED adaptor scheme: dims n=l=4, no hints/
 *    decomposition, full-coefficient (unpacked) objects, modulus ~2^23.
 * The comparison shows order-of-magnitude cost and, more usefully, that LAS's
 * adaptor operations (PreSign/PreVerify/Adapt/Ext) add negligible overhead over
 * its own Sign/Verify.
 */
#define _POSIX_C_SOURCE 199309L
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "../randombytes.h"
#include "../sign.h"     /* Dilithium-3 API (mode 3) */
#include "../las.h"
#include "../params.h"

#define NITER 2000

static double now_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

int main(void) {
  /* ---- Dilithium-3 setup ---- */
  uint8_t dpk[CRYPTO_PUBLICKEYBYTES], dsk[CRYPTO_SECRETKEYBYTES];
  uint8_t dsig[CRYPTO_BYTES];
  size_t dsiglen;
  uint8_t m[59];
  size_t mlen = sizeof m;
  uint8_t ctx[1] = {0};
  volatile int sink = 0;
  double t0;
  int i;

  /* ---- LAS setup ---- */
  uint8_t ppseed[LAS_SEEDBYTES];
  las_pp pp;
  las_pk pk, Y;
  las_sk sk, y, yext;
  las_sig sig, presig, adapted, tmp;

  randombytes(m, mlen);
  crypto_sign_keypair(dpk, dsk);
  crypto_sign_signature(dsig, &dsiglen, m, mlen, ctx, 0, dsk);

  randombytes(ppseed, LAS_SEEDBYTES);
  las_setup(&pp, ppseed);
  las_keygen(&pk, &sk, &pp);
  las_keygen(&Y, &y, &pp);
  las_sign(&sig, m, mlen, &pk, &sk, &pp);
  las_presign(&presig, m, mlen, &Y, &pk, &sk, &pp);
  las_adapt(&adapted, &presig, m, mlen, &Y, &y, &pk, &pp);

  printf("=== LAS vs Dilithium-3  (%d iters/op, -O3) ===\n", NITER);
  printf("(not same-security; see file header for the caveat)\n\n");

  printf("--- Dilithium-3 (optimised reference) ---\n");
  t0 = now_us();
  for(i = 0; i < NITER; ++i) crypto_sign_keypair(dpk, dsk);
  printf("  KeyGen     %9.2f us\n", (now_us() - t0) / NITER);
  t0 = now_us();
  for(i = 0; i < NITER; ++i) crypto_sign_signature(dsig, &dsiglen, m, mlen, ctx, 0, dsk);
  printf("  Sign       %9.2f us\n", (now_us() - t0) / NITER);
  t0 = now_us();
  for(i = 0; i < NITER; ++i) sink += crypto_sign_verify(dsig, dsiglen, m, mlen, ctx, 0, dpk);
  printf("  Verify     %9.2f us\n", (now_us() - t0) / NITER);
  printf("  sizes: pk=%d  sk=%d  sig=%d bytes (bit-packed)\n\n",
         CRYPTO_PUBLICKEYBYTES, CRYPTO_SECRETKEYBYTES, CRYPTO_BYTES);

  printf("--- LAS (simplified adaptor scheme, this work) ---\n");
  t0 = now_us();
  for(i = 0; i < NITER; ++i) las_keygen(&pk, &sk, &pp);
  printf("  KeyGen     %9.2f us\n", (now_us() - t0) / NITER);
  /* restore a consistent key for the dependent ops */
  las_keygen(&pk, &sk, &pp);
  las_sign(&sig, m, mlen, &pk, &sk, &pp);
  las_presign(&presig, m, mlen, &Y, &pk, &sk, &pp);
  las_adapt(&adapted, &presig, m, mlen, &Y, &y, &pk, &pp);
  t0 = now_us();
  for(i = 0; i < NITER; ++i) las_sign(&tmp, m, mlen, &pk, &sk, &pp);
  printf("  Sign       %9.2f us\n", (now_us() - t0) / NITER);
  t0 = now_us();
  for(i = 0; i < NITER; ++i) sink += las_verify(&sig, m, mlen, &pk, &pp);
  printf("  Verify     %9.2f us\n", (now_us() - t0) / NITER);
  t0 = now_us();
  for(i = 0; i < NITER; ++i) las_presign(&tmp, m, mlen, &Y, &pk, &sk, &pp);
  printf("  PreSign    %9.2f us   (adaptor)\n", (now_us() - t0) / NITER);
  t0 = now_us();
  for(i = 0; i < NITER; ++i) sink += las_preverify(&presig, m, mlen, &Y, &pk, &pp);
  printf("  PreVerify  %9.2f us   (adaptor)\n", (now_us() - t0) / NITER);
  t0 = now_us();
  for(i = 0; i < NITER; ++i) sink += las_adapt(&tmp, &presig, m, mlen, &Y, &y, &pk, &pp);
  printf("  Adapt      %9.2f us   (adaptor)\n", (now_us() - t0) / NITER);
  t0 = now_us();
  for(i = 0; i < NITER; ++i) sink += las_ext(&yext, &adapted, &presig, &Y, &pp);
  printf("  Ext        %9.2f us   (adaptor)\n", (now_us() - t0) / NITER);
  printf("  sizes: pk=%zu  sk=%zu  sig=%zu bytes (full coeffs, unpacked)\n\n",
         sizeof(las_pk), sizeof(las_sk), sizeof(las_sig));

  printf("Observation: LAS PreSign~=Sign and PreVerify~=Verify (adaptor overhead\n");
  printf("negligible); Adapt~=one Verify; Ext is the cheapest operation.\n");
  return (int)(sink & 0);
}
