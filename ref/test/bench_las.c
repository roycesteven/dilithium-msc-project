/* Per-operation timing for LAS, with retry-rate and packed-size reporting.
 *
 * Three distinct "size" numbers are printed and must not be confused:
 *   in-memory  : sizeof() — full int32 per coefficient, no packing
 *   packed      : canonical wire encoding (serialize.h) — = LAS_*_BYTES
 *   paper (opt) : the paper's own estimate (different params, different Q)
 */
#define _POSIX_C_SOURCE 199309L
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "../randombytes.h"
#include "../las.h"
#include "../serialize.h"   /* canonical packed sizes: LAS_*_BYTES / LAS_*_COEFF_BITS */
#include "../params.h"

#define NITER      2000

static double now_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

/* las_sign / las_presign increment the global las_attempts counter once per
 * rejection-loop attempt, so retry counts are MEASURED directly in the
 * rejection-sampling block below (the old timing-ratio estimate is superseded). */

/* ---- packed-size: the CANONICAL wire encoding (serialize.h) ----
 * These match the byte-level (de)serialiser in serialize.{c,h} and the report, so
 * the benchmark agrees with LAS_*_BYTES exactly:
 *   signature (c,z) : c = N=256 ternary coeffs x 2 bits = 64 B; z = LAS_M*N coeffs
 *                     x 18 bits = 4608 B; total LAS_SIG_BYTES = 4672 B.
 *   pk / statement Y: LAS_N*N x 23 bits/coeff = LAS_PK_BYTES = 2944 B.
 *   sk / witness y  : LAS_M*N x  2 bits/coeff = LAS_SK_BYTES =  512 B.
 * Y is the statement / public-key size and is NOT counted inside the signature. */
static size_t packed_sig_bytes(void) { return LAS_SIG_BYTES; }  /* (c,z) only, no Y */
static size_t packed_pk_bytes(void)  { return LAS_PK_BYTES;  }  /* pk = statement Y  */
static size_t packed_sk_bytes(void)  { return LAS_SK_BYTES;  }  /* sk = witness y    */

int main(void) {
  uint8_t ppseed[LAS_SEEDBYTES];
  uint8_t m[59];
  size_t mlen = sizeof m;
  las_pp pp, pp2;
  las_pk pk, Y, pk2;
  las_sk sk, y, sk2, yext;
  las_sig sig, presig, adapted, tmp;
  volatile int sink = 0;
  double t0, t_sign, t_presign;
  int i;

  randombytes(ppseed, LAS_SEEDBYTES);
  las_setup(&pp, ppseed);
  randombytes(m, mlen);
  las_keygen(&pk, &sk, &pp);
  las_keygen(&Y, &y, &pp);
  las_sign(&sig, m, mlen, &pk, &sk, &pp);
  las_presign(&presig, m, mlen, &Y, &pk, &sk, &pp);
  las_adapt(&adapted, &presig, m, mlen, &Y, &y, &pk, &pp);

  printf("=== LAS benchmark  (mode %d, %d iters/op) ===\n", DILITHIUM_MODE, NITER);
  printf("params: n=%d  ell=%d  kappa=%d  gamma=%d  N=%d  Q=%d\n\n",
         LAS_N, LAS_ELL, LAS_KAPPA, LAS_GAMMA, N, Q);

  t0 = now_us();
  for(i = 0; i < NITER; ++i) las_setup(&pp2, ppseed);
  printf("  Setup      %9.2f us\n", (now_us() - t0) / NITER);

  t0 = now_us();
  for(i = 0; i < NITER; ++i) las_keygen(&pk2, &sk2, &pp);
  printf("  KeyGen     %9.2f us   [statement gen = same cost]\n",
         (now_us() - t0) / NITER);

  t0 = now_us();
  for(i = 0; i < NITER; ++i) las_sign(&tmp, m, mlen, &pk, &sk, &pp);
  t_sign = (now_us() - t0) / NITER;
  printf("  Sign       %9.2f us\n", t_sign);

  t0 = now_us();
  for(i = 0; i < NITER; ++i) sink += las_verify(&sig, m, mlen, &pk, &pp);
  printf("  Verify     %9.2f us\n", (now_us() - t0) / NITER);

  t0 = now_us();
  for(i = 0; i < NITER; ++i) las_presign(&tmp, m, mlen, &Y, &pk, &sk, &pp);
  t_presign = (now_us() - t0) / NITER;
  printf("  PreSign    %9.2f us\n", t_presign);

  t0 = now_us();
  for(i = 0; i < NITER; ++i) sink += las_preverify(&presig, m, mlen, &Y, &pk, &pp);
  printf("  PreVerify  %9.2f us\n", (now_us() - t0) / NITER);

  t0 = now_us();
  for(i = 0; i < NITER; ++i) sink += las_adapt(&tmp, &presig, m, mlen, &Y, &y, &pk, &pp);
  printf("  Adapt      %9.2f us\n", (now_us() - t0) / NITER);

  t0 = now_us();
  for(i = 0; i < NITER; ++i) sink += las_ext(&yext, &adapted, &presig, &Y, &pp);
  printf("  Ext        %9.2f us\n", (now_us() - t0) / NITER);

  /* ---- Rejection-sampling retry rate (MEASURED directly) ---- *
   * las_sign / las_presign increment the global las_attempts counter once per
   * rejection-loop attempt (see las.h).  Counting it across NITER signing calls
   * gives the EXACT average attempts/op and acceptance rate - no estimation.
   *
   * NOTE: an earlier version estimated retries from the timing ratio
   * t_sign / t_verify and reported ~23% acceptance (~4.3 attempts).  That
   * estimator is BIASED: one Sign attempt body does LAS_M=8 c*r products plus
   * A*y, whereas one Verify does only LAS_N=4 c*t products plus A*z, so a Sign
   * attempt is dearer than a Verify and the ratio overcounts attempts.  The
   * direct counter below (~37% acceptance, ~2.7 attempts) supersedes it and is
   * corroborated by the theory line.
   */
  {
    unsigned long a0;
    double att_sign, att_presign, p_theory;

    a0 = las_attempts;
    for(i = 0; i < NITER; ++i) las_sign(&tmp, m, mlen, &pk, &sk, &pp);
    att_sign = (double)(las_attempts - a0) / NITER;

    a0 = las_attempts;
    for(i = 0; i < NITER; ++i) las_presign(&tmp, m, mlen, &Y, &pk, &sk, &pp);
    att_presign = (double)(las_attempts - a0) / NITER;

    /* per-attempt acceptance must keep all LAS_M*N response coeffs within
     * +-(g-k); approx (1 - k/g)^(M*N), which is ~e^-1 for these parameters. */
    p_theory = pow(1.0 - (double)LAS_KAPPA / (double)LAS_GAMMA,
                   (double)(LAS_M * N));

    printf("\nRejection-sampling (MEASURED directly via las_attempts, %d sigs):\n", NITER);
    printf("  Sign    : %.3f attempts/sig  -> acceptance %.1f%%, retries %.3f\n",
           att_sign, 100.0 / att_sign, att_sign - 1.0);
    printf("  PreSign : %.3f attempts/sig  -> acceptance %.1f%%, retries %.3f\n",
           att_presign, 100.0 / att_presign, att_presign - 1.0);
    printf("  Theory  : (1 - k/g)^(M*N) = %.1f%% per attempt (~%.2f attempts)\n",
           100.0 * p_theory, 1.0 / p_theory);
    printf("  Rejection sampling is intrinsic to Fiat-Shamir-with-aborts (optimised\n");
    printf("  Dilithium also rejects, targeting a small expected repeat count); this\n");
    printf("  simplified scheme just omits the hint vector, keeping the algebra clear.\n");
  }

  /* ---- Sizes ---- */
  printf("\n--- Object sizes ---\n");
  printf("  In-memory (sizeof, full int32 per coeff):\n");
  printf("    pk / statement Y : %4zu bytes\n", sizeof(las_pk));
  printf("    sk / witness y   : %4zu bytes\n", sizeof(las_sk));
  printf("    sig / pre-sig    : %4zu bytes\n", sizeof(las_sig));

  printf("\n  Theoretical packed (canonical wire encoding, serialize.h):\n");
  printf("    pk / statement Y : %4zu bytes", packed_pk_bytes());
  printf("  [LAS_N*N*%d bits; Q=%d => %d bits/coeff]\n",
         LAS_PK_COEFF_BITS, Q, LAS_PK_COEFF_BITS);
  printf("    sk / witness y   : %4zu bytes", packed_sk_bytes());
  printf("  [LAS_M*N*%d bits; ternary needs %d bits/coeff]\n",
         LAS_SK_COEFF_BITS, LAS_SK_COEFF_BITS);
  printf("    sig / pre-sig    : %4zu bytes", packed_sig_bytes());
  printf("  [c: %zu bytes (N=%d ternary coeffs x %d bits); z: %d bits/coeff => %zu bytes]\n",
         (size_t)((N * LAS_C_COEFF_BITS + 7) / 8),
         N, LAS_C_COEFF_BITS,
         LAS_Z_COEFF_BITS,
         (size_t)((LAS_M * N * LAS_Z_COEFF_BITS + 7) / 8));

  printf("\n  Paper's estimate (~3210 bytes for signature):\n");
  printf("    Refers to the paper's OPTIMISED scheme (q~2^24, n=4, ell=4,\n");
  printf("    bit-packed with hint vector). Not directly comparable to this\n");
  printf("    implementation which uses q~2^23, no hints, and is unoptimised.\n");
  printf("    The correct comparison point is our 'theoretical packed' column above.\n");

  printf("\nNote: this is a TIMING benchmark, not a correctness check -- it times\n");
  printf("operations but does not assert their results. Functional correctness is\n");
  printf("verified separately by test_las / test_basesig / test_contract.\n");

  return (int)(sink & 0);
}
