/* Per-operation timing for LAS. Averages wall-clock microseconds per call. */
#define _POSIX_C_SOURCE 199309L
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include "../randombytes.h"
#include "../las.h"
#include "../params.h"

#define NITER 2000

static double now_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

int main(void) {
  uint8_t ppseed[LAS_SEEDBYTES];
  uint8_t m[59];
  size_t mlen = sizeof m;
  las_pp pp, pp2;
  las_pk pk, Y, pk2;
  las_sk sk, y, sk2, yext;
  las_sig sig, presig, adapted, tmp;
  volatile int sink = 0;
  double t0;
  int i;

  randombytes(ppseed, LAS_SEEDBYTES);
  las_setup(&pp, ppseed);
  randombytes(m, mlen);
  las_keygen(&pk, &sk, &pp);
  las_keygen(&Y, &y, &pp);
  las_sign(&sig, m, mlen, &pk, &sk, &pp);
  las_presign(&presig, m, mlen, &Y, &pk, &sk, &pp);
  las_adapt(&adapted, &presig, m, mlen, &Y, &y, &pk, &pp);

  printf("=== LAS benchmark (mode %d, %d iters/op) ===\n", DILITHIUM_MODE, NITER);
  printf("params: n=%d ell=%d kappa=%d gamma=%d N=%d Q=%d\n\n",
         LAS_N, LAS_ELL, LAS_KAPPA, LAS_GAMMA, N, Q);

  t0 = now_us();
  for(i = 0; i < NITER; ++i) las_setup(&pp2, ppseed);
  printf("  Setup      %9.2f us\n", (now_us() - t0) / NITER);

  t0 = now_us();
  for(i = 0; i < NITER; ++i) las_keygen(&pk2, &sk2, &pp);
  printf("  KeyGen     %9.2f us\n", (now_us() - t0) / NITER);

  t0 = now_us();
  for(i = 0; i < NITER; ++i) las_sign(&tmp, m, mlen, &pk, &sk, &pp);
  printf("  Sign       %9.2f us\n", (now_us() - t0) / NITER);

  t0 = now_us();
  for(i = 0; i < NITER; ++i) sink += las_verify(&sig, m, mlen, &pk, &pp);
  printf("  Verify     %9.2f us\n", (now_us() - t0) / NITER);

  t0 = now_us();
  for(i = 0; i < NITER; ++i) las_presign(&tmp, m, mlen, &Y, &pk, &sk, &pp);
  printf("  PreSign    %9.2f us\n", (now_us() - t0) / NITER);

  t0 = now_us();
  for(i = 0; i < NITER; ++i) sink += las_preverify(&presig, m, mlen, &Y, &pk, &pp);
  printf("  PreVerify  %9.2f us\n", (now_us() - t0) / NITER);

  t0 = now_us();
  for(i = 0; i < NITER; ++i) sink += las_adapt(&tmp, &presig, m, mlen, &Y, &y, &pk, &pp);
  printf("  Adapt      %9.2f us\n", (now_us() - t0) / NITER);

  t0 = now_us();
  for(i = 0; i < NITER; ++i) sink += las_ext(&yext, &adapted, &presig, &Y, &pp);
  printf("  Ext        %9.2f us\n", (now_us() - t0) / NITER);

  printf("\nsizes (bytes, full coeffs): pk/Y=%zu  sk/witness=%zu  sig/presig=%zu\n",
         sizeof(las_pk), sizeof(las_sk), sizeof(las_sig));
  return (int)(sink & 0);   /* keep 'sink' live without affecting exit code */
}
