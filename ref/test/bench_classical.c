/* Classical adaptor-signature baseline (objectives Meeting-2, B2.ii).
 *
 * Benchmarks the ECDSA-based adaptor signature from libsecp256k1-zkp
 * (BlockstreamResearch fork of Bitcoin Core's libsecp256k1; module
 * `ecdsa_adaptor`, production code used in Discreet Log Contracts) on the SAME
 * machine and compiler as the LAS benchmarks, so the "price of post-quantum"
 * 2x2 comparison (basic/adaptor x classical/PQ) needs no hardware caveats:
 *
 *     LAS op        <->  secp256k1-zkp op
 *     KeyGen             ec_pubkey_create
 *     Sign/Verify        ecdsa_sign / ecdsa_verify        (classical BASIC row)
 *     PreSign            ecdsa_adaptor_encrypt
 *     PreVerify          ecdsa_adaptor_verify
 *     Adapt              ecdsa_adaptor_decrypt
 *     Ext                ecdsa_adaptor_recover
 *
 * The statement/witness pair (Y, y) maps to (encryption pubkey, deckey32) -
 * like LAS, the statement is literally another key pair.
 *
 * Methodology notes (report these alongside the numbers):
 *  - REUSED implementation (supervisor-sanctioned): we time their code as-is;
 *    only this harness is ours.  Library commit is printed by `make` provenance
 *    (third_party/secp256k1-zkp, see README_LAS.md).
 *  - libsecp256k1 is constant-time, heavily optimised production code; the LAS
 *    side is a reference-style simplified scheme.  The comparison therefore
 *    *flatters* the classical side - state this honestly.
 *  - A structural contrast worth reporting: the ECDSA adaptor pre-signature is
 *    a DIFFERENT object (162 B, with a DLEQ proof) that cannot even be parsed
 *    as an ECDSA signature - the "pre-sig does not verify" property is
 *    syntactic.  In LAS, pre-sig and sig share one format and the tripwire is
 *    cryptographic (the +Y Fiat-Shamir mismatch).
 */
#define _POSIX_C_SOURCE 199309L
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <secp256k1.h>
#include <secp256k1_ecdsa_adaptor.h>

#define NITER 2000           /* >=1000 per objectives Part D; matches bench_las */
#define CHECK(cond, msg) do { if(!(cond)) { \
  fprintf(stderr, "[FAIL] %s\n", msg); return 1; } } while(0)

static double now_us(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

/* Deterministic non-zero filler (xorshift64) - no external RNG dependency. */
static uint64_t rngstate = 0x9E3779B97F4A7C15ull;
static void fill(uint8_t *p, size_t n) {
  size_t i;
  for(i = 0; i < n; ++i) {
    rngstate ^= rngstate << 13; rngstate ^= rngstate >> 7; rngstate ^= rngstate << 17;
    p[i] = (uint8_t)(rngstate >> 56);
  }
}

static uint8_t seckey[NITER][32], msg[NITER][32];
static secp256k1_pubkey pub[NITER];
static secp256k1_ecdsa_signature sig[NITER];
static uint8_t adaptor[NITER][162];

int main(void) {
  secp256k1_context *ctx = secp256k1_context_create(SECP256K1_CONTEXT_NONE);
  secp256k1_pubkey enckey;          /* statement Y */
  uint8_t deckey[32];               /* witness  y  */
  uint8_t deckey_rec[32], buf[72];
  size_t len;
  double t0;
  int i, ok = 1;

  CHECK(ctx != NULL, "context");
  for(i = 0; i < NITER; ++i) { fill(seckey[i], 32); fill(msg[i], 32); }
  fill(deckey, 32);
  CHECK(secp256k1_ec_pubkey_create(ctx, &enckey, deckey) == 1, "statement keygen");

  printf("=== Classical adaptor baseline: libsecp256k1-zkp ecdsa_adaptor ===\n");
  printf("(same machine/compiler as bench_las3; %d iterations/op; reused\n", NITER);
  printf(" production implementation - only this timing harness is ours)\n\n");

  /* KeyGen */
  t0 = now_us();
  for(i = 0; i < NITER; ++i)
    ok &= secp256k1_ec_pubkey_create(ctx, &pub[i], seckey[i]);
  printf("  KeyGen / statement gen %9.2f us\n", (now_us() - t0) / NITER);

  /* basic ECDSA Sign / Verify (the classical-basic row of the 2x2) */
  t0 = now_us();
  for(i = 0; i < NITER; ++i)
    ok &= secp256k1_ecdsa_sign(ctx, &sig[i], msg[i], seckey[i], NULL, NULL);
  printf("  Sign   (ECDSA)        %9.2f us\n", (now_us() - t0) / NITER);
  t0 = now_us();
  for(i = 0; i < NITER; ++i)
    ok &= secp256k1_ecdsa_verify(ctx, &sig[i], msg[i], &pub[i]);
  printf("  Verify (ECDSA)        %9.2f us\n", (now_us() - t0) / NITER);
  CHECK(ok, "basic ECDSA round-trip");

  /* PreSign = adaptor encrypt */
  t0 = now_us();
  for(i = 0; i < NITER; ++i)
    ok &= secp256k1_ecdsa_adaptor_encrypt(ctx, adaptor[i], seckey[i], &enckey,
                                          msg[i], NULL, NULL);
  printf("  PreSign   (encrypt)   %9.2f us\n", (now_us() - t0) / NITER);

  /* PreVerify = adaptor verify */
  t0 = now_us();
  for(i = 0; i < NITER; ++i)
    ok &= secp256k1_ecdsa_adaptor_verify(ctx, adaptor[i], &pub[i], msg[i], &enckey);
  printf("  PreVerify (verify)    %9.2f us\n", (now_us() - t0) / NITER);

  /* Adapt = decrypt with witness */
  t0 = now_us();
  for(i = 0; i < NITER; ++i)
    ok &= secp256k1_ecdsa_adaptor_decrypt(ctx, &sig[i], deckey, adaptor[i]);
  printf("  Adapt     (decrypt)   %9.2f us\n", (now_us() - t0) / NITER);

  /* adapted signatures must pass ordinary ECDSA Verify (normalize: low-S) */
  for(i = 0; i < NITER; ++i) {
    secp256k1_ecdsa_signature_normalize(ctx, &sig[i], &sig[i]);
    ok &= secp256k1_ecdsa_verify(ctx, &sig[i], msg[i], &pub[i]);
  }
  CHECK(ok, "adapted signatures verify as ordinary ECDSA");

  /* Ext = recover witness from (sig, adaptor sig, statement) */
  t0 = now_us();
  for(i = 0; i < NITER; ++i)
    ok &= secp256k1_ecdsa_adaptor_recover(ctx, deckey_rec, &sig[i], adaptor[i], &enckey);
  printf("  Ext       (recover)   %9.2f us\n", (now_us() - t0) / NITER);
  CHECK(ok && memcmp(deckey_rec, deckey, 32) == 0, "recovered witness == witness");

  /* Sizes */
  len = sizeof buf;
  ok &= secp256k1_ecdsa_signature_serialize_der(ctx, buf, &len, &sig[0]);
  CHECK(ok, "DER serialize");
  printf("\n  sizes: pk/statement 33 B (compressed) | sk/witness 32 B |\n");
  printf("         sig 64 B compact (%zu B DER) | pre-sig (adaptor) 162 B\n", len);

  printf("\n  note: the 162 B pre-signature is a syntactically distinct object\n");
  printf("  (ECDSA sig + DLEQ proof); LAS pre-sigs share the signature format\n");
  printf("  and fail Verify cryptographically. Constant-time production code -\n");
  printf("  comparison flatters the classical side vs reference-style LAS.\n");

  secp256k1_context_destroy(ctx);
  printf("=== classical baseline OK ===\n");
  return 0;
}
