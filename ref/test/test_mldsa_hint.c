/*
 * test_mldsa_hint.c -- the ML-DSA hint experiment (diagnostic harness).
 *
 * WHAT THIS ANSWERS
 *   The project builds LAS on the LAS paper's SIMPLIFIED Dilithium: no hint
 *   vector, no Power2Round, no high/low-bit decomposition.  Every write-up so
 *   far has ASSERTED that NIST's ML-DSA has to be modified before an adaptor
 *   layer can sit on it, because the adaptor rests on the exact identity
 *   A z - c t = w + Y that those three optimisations destroy.
 *
 *   This harness turns the assertion into a measurement.  It builds the four
 *   adaptor functions on ML-DSA with the hint machinery ENABLED (ref/mldsa_las.c)
 *   in two variants, and reports -- per property, per variant, over many random
 *   iterations -- what actually survives.
 *
 * HOW TO READ THE OUTPUT
 *   This is NOT a pass/fail test and it must never be made into one.  A "0/N"
 *   cell is a result, not a bug: it localises exactly which ML-DSA feature the
 *   adaptor is incompatible with.  The harness only hard-fails on things that
 *   would invalidate the experiment itself (the ML-DSA control round-trip, and
 *   an inconsistency that would mean the harness is measuring nothing).
 *
 * THE PROPERTIES (the same adaptor contract test_contract.c checks for LAS)
 *   P1 PreVerify accepts the honest pre-signature
 *   P2 stock ML-DSA Verify REJECTS the pre-signature          (the tripwire)
 *   P3 Adapt succeeds
 *   P4 stock ML-DSA Verify ACCEPTS the adapted signature      (THE decisive one)
 *   P5 Ext returns a witness, and it is exactly the y used
 *   P6 adapted ||z||_inf < GAMMA1 - BETA                      (ML-DSA's own bound)
 *   P7 hint weight <= OMEGA
 *
 *   P4 is the property the whole experiment exists for: it is what "the adapted
 *   signature is a fully ordinary signature" means, and it is what an unchanged
 *   consensus verifier would be asked to do.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../params.h"
#include "../mldsa_las.h"
#include "../sign.h"
#include "../packing.h"
#include "../polyvec.h"
#include "../randombytes.h"

#define NITER 200

#define NPROP 7
static const char *PROP[NPROP] = {
  "P1 PreVerify accepts pre-signature",
  "P2 stock Verify REJECTS pre-signature (tripwire)",
  "P3 Adapt succeeds",
  "P4 stock Verify ACCEPTS adapted signature  <-- decisive",
  "P5 Ext recovers the exact witness",
  "P6 adapted ||z||inf < GAMMA1-BETA",
  "P7 hint weight <= OMEGA",
};

struct tally {
  unsigned long ok[NPROP];
  unsigned long runs;
  unsigned long attempts;
  unsigned long hint_weight_sum;
  int hint_weight_max;
};

static int witness_equal(const mldsa_witness *a, const mldsa_witness *b)
{
  unsigned int i, j;
  for(i = 0; i < L; ++i)
    for(j = 0; j < N; ++j)
      if(a->y.vec[i].coeffs[j] != b->y.vec[i].coeffs[j])
        return 0;
  return 1;
}

static int adapted_z_in_bound(const uint8_t sig[CRYPTO_BYTES])
{
  uint8_t c[CTILDEBYTES];
  polyvecl z;
  polyveck h;
  if(unpack_sig(c, &z, &h, sig))
    return 0;
  return !polyvecl_chknorm(&z, GAMMA1 - BETA);
}

static void run_variant(mldsa_las_variant variant, struct tally *t)
{
  uint8_t pk[CRYPTO_PUBLICKEYBYTES], sk[CRYPTO_SECRETKEYBYTES];
  uint8_t presig[CRYPTO_BYTES], sig[CRYPTO_BYTES];
  uint8_t rho[SEEDBYTES], seed[CRHBYTES], rnd[RNDBYTES];
  uint8_t m[59];
  mldsa_statement Y;
  mldsa_witness y, y_ext;
  polyveck t1_unused;
  unsigned long i;
  unsigned long attempts_before;
  int w;

  memset(t, 0, sizeof(*t));
  t->hint_weight_max = -1;

  for(i = 0; i < NITER; ++i) {
    randombytes(m, sizeof(m));
    randombytes(seed, sizeof(seed));
    randombytes(rnd, sizeof(rnd));
    crypto_sign_keypair(pk, sk);
    unpack_pk(rho, &t1_unused, pk);

    mldsa_las_gen(&Y, &y, rho, seed, 0);

    attempts_before = mldsa_las_attempts;
    if(mldsa_las_presign(presig, m, sizeof(m), &Y, rnd, sk, variant))
      continue;                                   /* PreSign itself failed */
    t->attempts += mldsa_las_attempts - attempts_before;
    t->runs++;

    /* P1 */
    if(mldsa_las_preverify(presig, m, sizeof(m), &Y, pk) == 0)
      t->ok[0]++;

    /* P2 -- the pre-signature must NOT be a valid ordinary signature */
    if(crypto_sign_verify(presig, CRYPTO_BYTES, m, sizeof(m), NULL, 0, pk) != 0)
      t->ok[1]++;

    /* P3 */
    if(mldsa_las_adapt(sig, presig, m, sizeof(m), &Y, &y, pk) == 0) {
      t->ok[2]++;

      /* P4 -- the decisive property: an UNMODIFIED ML-DSA verifier */
      if(crypto_sign_verify(sig, CRYPTO_BYTES, m, sizeof(m), NULL, 0, pk) == 0)
        t->ok[3]++;

      /* P5 */
      if(mldsa_las_ext(&y_ext, sig, presig, &Y, pk) == 0 &&
         witness_equal(&y_ext, &y))
        t->ok[4]++;

      /* P6 */
      if(adapted_z_in_bound(sig))
        t->ok[5]++;
    }

    /* P7 -- read off the pre-signature; Adapt copies the hint unchanged */
    w = mldsa_las_hint_weight(presig);
    if(w >= 0) {
      t->hint_weight_sum += (unsigned long)w;
      if(w > t->hint_weight_max)
        t->hint_weight_max = w;
      if(w <= OMEGA)
        t->ok[6]++;
    }
  }
}

static void report(const char *name, const char *blurb, const struct tally *t)
{
  int p;
  printf("\n--- %s ---\n%s\n\n", name, blurb);
  if(t->runs == 0) {
    printf("  PreSign produced no pre-signature in %d attempts -- the variant is\n"
           "  not merely incorrect, it does not terminate. That is itself the result.\n",
           NITER);
    return;
  }
  for(p = 0; p < NPROP; ++p)
    printf("  %-52s %6lu / %-6lu  %s\n", PROP[p], t->ok[p], t->runs,
           t->ok[p] == t->runs ? "HOLDS"
                               : (t->ok[p] == 0 ? "FAILS ALWAYS" : "FAILS SOMETIMES"));
  printf("\n  rejection-sampling attempts/PreSign : %.3f\n",
         (double)t->attempts / (double)t->runs);
  printf("  hint weight mean / max / OMEGA      : %.1f / %d / %d\n",
         (double)t->hint_weight_sum / (double)t->runs, t->hint_weight_max, OMEGA);
}

/* Matched baseline: the same instrumented loop with the Y-shift removed, i.e.
 * plain ML-DSA.  Two jobs -- it makes the attempts/PreSign comparison
 * like-for-like, and it is a FIDELITY GATE: if signatures produced by this
 * file's mirror of sign.c are not accepted by the stock verifier, nothing else
 * printed here is interpretable. */
static void run_base(unsigned long *verified, unsigned long *runs,
                     unsigned long *attempts)
{
  uint8_t pk[CRYPTO_PUBLICKEYBYTES], sk[CRYPTO_SECRETKEYBYTES];
  uint8_t sig[CRYPTO_BYTES];
  uint8_t rho[SEEDBYTES], seed[CRHBYTES], rnd[RNDBYTES];
  uint8_t m[59];
  mldsa_statement Y;
  mldsa_witness y;
  polyveck t1_unused;
  unsigned long i, before;

  *verified = *runs = *attempts = 0;
  for(i = 0; i < NITER; ++i) {
    randombytes(m, sizeof(m));
    randombytes(seed, sizeof(seed));
    randombytes(rnd, sizeof(rnd));
    crypto_sign_keypair(pk, sk);
    unpack_pk(rho, &t1_unused, pk);
    mldsa_las_gen(&Y, &y, rho, seed, 0);     /* generated but unused by VBASE */

    before = mldsa_las_attempts;
    if(mldsa_las_presign(sig, m, sizeof(m), &Y, rnd, sk, MLDSA_LAS_VBASE))
      continue;
    *attempts += mldsa_las_attempts - before;
    (*runs)++;
    if(crypto_sign_verify(sig, CRYPTO_BYTES, m, sizeof(m), NULL, 0, pk) == 0)
      (*verified)++;
  }
}

int main(void)
{
  uint8_t pk[CRYPTO_PUBLICKEYBYTES], sk[CRYPTO_SECRETKEYBYTES];
  uint8_t sig[CRYPTO_BYTES];
  uint8_t m[59];
  size_t siglen;
  struct tally v0, v1;
  unsigned long base_ok, base_runs, base_attempts;
  int control_ok;

  printf("=== ML-DSA hint experiment: does LAS compose with FIPS 204 as specified? ===\n");
  printf("ML-DSA-%d  (K=%d, L=%d, TAU=%d, GAMMA1=%d, GAMMA2=%d, OMEGA=%d, ETA=%d)\n",
         DILITHIUM_MODE == 2 ? 44 : (DILITHIUM_MODE == 3 ? 65 : 87),
         K, L, TAU, GAMMA1, (int)GAMMA2, OMEGA, ETA);
  printf("hint vector, Power2Round and high/low-bit decomposition are ALL ENABLED.\n");
  printf("iterations per variant: %d\n", NITER);
  printf("\nwire sizes: ML-DSA signature %d B | public key %d B | statement Y %d B\n",
         CRYPTO_BYTES, CRYPTO_PUBLICKEYBYTES, (int)(K * N * 23 / 8));
  printf("  (the statement is a full R_q^K element: Power2Round compresses the public\n"
         "   key, but it cannot compress Y, which enters the verification identity\n"
         "   before any rounding.)\n");

  /* Control: the harness must be able to make ML-DSA itself work, otherwise a
   * failure below would say nothing about the adaptor. */
  randombytes(m, sizeof(m));
  crypto_sign_keypair(pk, sk);
  crypto_sign_signature(sig, &siglen, m, sizeof(m), NULL, 0, sk);
  control_ok = (crypto_sign_verify(sig, siglen, m, sizeof(m), NULL, 0, pk) == 0);
  printf("\ncontrol: unmodified ML-DSA sign -> verify : %s\n",
         control_ok ? "OK" : "BROKEN");
  if(!control_ok) {
    printf("\nFATAL: the control failed, so nothing below is interpretable.\n");
    return 1;
  }

  /* Fidelity gate: this file's mirror of sign.c, with the adaptor removed,
   * must produce signatures the UNMODIFIED verifier accepts. */
  run_base(&base_ok, &base_runs, &base_attempts);
  printf("fidelity gate: mirrored ML-DSA sign (no statement) -> stock verify : "
         "%lu / %lu\n", base_ok, base_runs);
  if(base_runs == 0 || base_ok != base_runs) {
    printf("\nFATAL: the mirror of sign.c is not faithful, so no variant result\n"
           "below can be attributed to the adaptor. Fix the mirror first.\n");
    return 1;
  }
  printf("baseline attempts/Sign (matched, same loop)  : %.3f\n",
         (double)base_attempts / (double)base_runs);

  run_variant(MLDSA_LAS_V0_NAIVE, &v0);
  report("VARIANT 0 -- the naive port",
         "  ML-DSA's signing path verbatim, with exactly ONE change: the challenge is\n"
         "  hashed over HighBits(w + Y) instead of HighBits(w). The committed high bits,\n"
         "  the low-bits rejection test and MakeHint are all still taken around w.\n"
         "  This is what treating the hint machinery as a black box produces.",
         &v0);

  run_variant(MLDSA_LAS_V1_SHIFTED, &v1);
  report("VARIANT 1 -- the commitment path moved onto w + Y",
         "  The signer knows Y at PreSign time, so the ENTIRE commitment path is taken\n"
         "  around w + Y: the committed high bits, the low-bits rejection test and\n"
         "  MakeHint. PreSign also rejects at GAMMA1 - BETA - ETA so the adapted z still\n"
         "  clears ML-DSA's own bound. This is the most faithful repair available\n"
         "  without weakening ML-DSA or handing the adapter a signing key.",
         &v1);

  printf("\n=== Cost of the adaptor layer on ML-DSA (matched baseline) ===\n");
  if(v1.runs)
    printf("  attempts/PreSign vs attempts/Sign : %.3f vs %.3f  (%+.1f%%)\n",
           (double)v1.attempts / (double)v1.runs,
           (double)base_attempts / (double)base_runs,
           100.0 * (((double)v1.attempts / (double)v1.runs)
                    / ((double)base_attempts / (double)base_runs) - 1.0));
  printf("  statement Y on the wire           : %d B, against a %d B signature\n",
         (int)(K * N * 23 / 8), CRYPTO_BYTES);
  printf("  (Y is larger than the signature it accompanies: Power2Round shrinks the\n");
  printf("   public key to t1, but Y enters the identity before any rounding.)\n");

  printf("\n=== What this shows ===\n");
  printf("Read the P4 row of each variant. P4 is the claim 'the adapted signature is a\n");
  printf("fully ordinary ML-DSA signature', i.e. what an unchanged consensus verifier\n");
  printf("would have to accept.\n\n");
  printf("Structural findings that hold regardless of the counts above, and that the\n");
  printf("code makes checkable:\n");
  printf("  (a) PreSign CANNOT be ML-DSA's Sign. Sign commits to HighBits(w), which\n");
  printf("      contains no statement; any adaptor needs the statement inside the hash,\n");
  printf("      so the signing algorithm is modified by construction.\n");
  printf("  (b) PreVerify CANNOT be ML-DSA's Verify. It has to add Y before UseHint\n");
  printf("      (mldsa_las.c, mldsa_las_preverify), and the stock verifier has no Y.\n");
  printf("      A deployed verifier can therefore check adapted signatures but never\n");
  printf("      pre-signatures.\n");
  printf("  (c) Adapt CANNOT repair a hint. MakeHint needs c*t0 and w - c*s2, both\n");
  printf("      derived from the SIGNER's secret key; the adapting party holds only the\n");
  printf("      witness. Whatever hint PreSign wrote is what the adapted signature\n");
  printf("      carries.\n");
  printf("  (d) The statement is not compressible the way the public key is. t is sent\n");
  printf("      as t1 via Power2Round; Y enters the identity before rounding and is\n");
  printf("      sent in full.\n");
  printf("\nThese are the modifications the simplified scheme avoids by disabling the\n");
  printf("hint, Power2Round and the high/low-bit split -- which is the design decision\n");
  printf("this experiment was built to justify or refute.\n");
  return 0;
}
