/*
 * Serialisation tests for LAS (ref/serialize.{c,h}).
 *
 * A robust, deployment-ready implementation must exchange objects as BYTES and
 * decode them defensively (an on-chain verifier cannot trust its input).  This
 * test hard-asserts, over many random instances:
 *
 *   - round-trip:   unpack(pack(x)) == x   for pk, statement, sk, witness, sig
 *                   and pre-signature (each with its OWN typed codec);
 *   - verify-from-bytes: a packed (pk, adapted sig) verifies via the on-chain-style
 *                   entry point base_verify_packed, while a packed PRE-signature does
 *                   NOT (the statement-binding tripwire survives serialisation);
 *   - tamper:       flipping the low bit of every byte of a packed signature makes
 *                   it fail to verify;
 *   - validation:   pack rejects out-of-range inputs; unpack rejects malformed
 *                   key encodings (coeff >= Q, non-ternary code); signature
 *                   decoding is permissive and Verify enforces the response norm.
 *
 * It also prints the MEASURED packed sizes (these realise the "theoretical packed"
 * figures quoted in docs/LAS.md Section 8).
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../randombytes.h"
#include "../basesig.h"   /* Algorithm 1 + its packed tier (base_verify_packed is the byte verifier) */
#include "../relation.h"  /* relation_gen -> (statement, witness) */
#include "../las.h"       /* Algorithm 2 + its packed tier */
#include "../serialize.h"
#include "../params.h"

#define MLEN   59
#define NITER  256

#define CHECK(cond, msg) do { if(!(cond)) { \
  fprintf(stderr, "  [FAIL] %s\n", msg); return 1; } } while(0)

/* Semantic wire-size equalities (hold for every parameter set): a statement is
 * pk-shaped, a witness sk-shaped, a pre-signature sig-shaped. */
typedef char serde_size_relations[
  (STATEMENT_BYTES == PUBLIC_KEY_BYTES &&
   WITNESS_BYTES == SECRET_KEY_BYTES &&
   PRE_SIGNATURE_BYTES == SIGNATURE_BYTES) ? 1 : -1];

static int poly_eq(const poly *a, const poly *b) {
  unsigned int k;
  for(k = 0; k < LAS_D; ++k) if(a->coeffs[k] != b->coeffs[k]) return 0;
  return 1;
}
static int pk_eq(const public_key *a, const public_key *b) {
  unsigned int i;
  for(i = 0; i < LAS_N; ++i) if(!poly_eq(&a->t[i], &b->t[i])) return 0;
  return 1;
}
static int stmt_eq(const statement *a, const statement *b) {
  unsigned int i;
  for(i = 0; i < LAS_N; ++i) if(!poly_eq(&a->t_prime[i], &b->t_prime[i])) return 0;
  return 1;
}
static int sk_eq(const secret_key *a, const secret_key *b) {
  unsigned int i;
  for(i = 0; i < N_PLUS_ELL; ++i) if(!poly_eq(&a->r[i], &b->r[i])) return 0;
  return 1;
}
static int witness_eq(const witness *a, const witness *b) {
  unsigned int i;
  for(i = 0; i < N_PLUS_ELL; ++i) if(!poly_eq(&a->value[i], &b->value[i])) return 0;
  return 1;
}
static int sig_eq(const signature *a, const signature *b) {
  unsigned int i;
  if(memcmp(a->c_tilde, b->c_tilde, LAS_CTILDEBYTES) != 0) return 0;  /* challenge digest */
  for(i = 0; i < N_PLUS_ELL; ++i) if(!poly_eq(&a->z[i], &b->z[i])) return 0;
  return 1;
}
static int presig_eq(const pre_signature *a, const pre_signature *b) {
  unsigned int i;
  if(memcmp(a->c_tilde, b->c_tilde, LAS_CTILDEBYTES) != 0) return 0;  /* challenge digest */
  for(i = 0; i < N_PLUS_ELL; ++i) if(!poly_eq(&a->z_hat[i], &b->z_hat[i])) return 0;
  return 1;
}

int main(void) {
  uint8_t ppseed[LAS_SEEDBYTES];
  public_params pp;
  uint8_t m[MLEN];
  uint8_t pk_b[PUBLIC_KEY_BYTES], sk_b[SECRET_KEY_BYTES];
  uint8_t sig_b[SIGNATURE_BYTES], pre_b[PRE_SIGNATURE_BYTES], adp_b[SIGNATURE_BYTES];
  public_key pk, pk2;
  statement Y, Y2;
  secret_key sk, sk2;
  witness r_prime, r_prime2;
  signature sig, adapted, sig2;
  pre_signature presig, presig2;
  int i;

  randombytes(ppseed, LAS_SEEDBYTES);
  setup_public_params(&pp, ppseed);

  printf("=== LAS serialisation tests (mode %d) ===\n", DILITHIUM_MODE);
  printf("packed sizes: pk/statement=%d B  sk/witness=%d B  sig/pre-sig=%d B\n",
         PUBLIC_KEY_BYTES, SECRET_KEY_BYTES, SIGNATURE_BYTES);

  for(i = 0; i < NITER; ++i) {
    randombytes(m, MLEN);
    CHECK(base_keygen(&pk, &sk, &pp) == 0, "base_keygen");
    CHECK(relation_gen(&Y, &r_prime, &pp) == 0, "relation_gen");
    CHECK(base_sign(&sig, m, MLEN, &pk, &sk, &pp) == 0, "base_sign");
    CHECK(las_presign(&presig, m, MLEN, &Y, &pk, &sk, &pp) == 0, "las_presign");
    CHECK(las_adapt(&adapted, &presig, m, MLEN, &Y, &r_prime, &pk, &pp) == 0, "adapt");

    /* round-trip: public key */
    pack_public_key(pk_b, &pk);
    CHECK(unpack_public_key(&pk2, pk_b) == 0, "unpack pk");
    CHECK(pk_eq(&pk, &pk2), "pk round-trip");

    /* round-trip: statement (pk-shaped, DISTINCT type + codec) */
    {
      uint8_t Y_b[STATEMENT_BYTES];
      pack_statement(Y_b, &Y);
      CHECK(unpack_statement(&Y2, Y_b) == 0, "unpack statement");
      CHECK(stmt_eq(&Y, &Y2), "statement round-trip");
    }

    /* round-trip: secret key (ternary) */
    CHECK(pack_secret_key(sk_b, &sk) == 0, "pack sk");
    CHECK(unpack_secret_key(&sk2, sk_b) == 0, "unpack sk");
    CHECK(sk_eq(&sk, &sk2), "sk round-trip");

    /* round-trip: honest witness (ternary, DISTINCT type + codec) */
    {
      uint8_t rw_b[WITNESS_BYTES];
      CHECK(pack_witness(rw_b, &r_prime) == 0, "pack witness");
      CHECK(unpack_witness(&r_prime2, rw_b) == 0, "unpack witness");
      CHECK(witness_eq(&r_prime, &r_prime2), "witness round-trip");
    }

    /* round-trip: signature (its own codec) */
    CHECK(pack_signature(sig_b, &sig) == 0, "pack sig");
    CHECK(unpack_signature(&sig2, sig_b) == 0, "unpack sig");
    CHECK(sig_eq(&sig, &sig2), "sig round-trip");

    /* round-trip: pre-signature (DISTINCT type, its own codec) */
    CHECK(pack_pre_signature(pre_b, &presig) == 0, "pack presig");
    CHECK(unpack_pre_signature(&presig2, pre_b) == 0, "unpack presig");
    CHECK(presig_eq(&presig, &presig2), "presig round-trip");

    /* round-trip: adapted signature */
    CHECK(pack_signature(adp_b, &adapted) == 0, "pack adapted");
    CHECK(unpack_signature(&sig2, adp_b) == 0, "unpack adapted");
    CHECK(sig_eq(&adapted, &sig2), "adapted round-trip");

    /* verify-from-bytes: adapted sig verifies, pre-sig bytes do NOT (tripwire) */
    CHECK(base_verify_packed(adp_b, m, MLEN, pk_b, &pp) == 0, "verify_packed(adapted)");
    CHECK(base_verify_packed(pre_b, m, MLEN, pk_b, &pp) != 0, "verify_packed(presig) must fail");
    /* the plain ordinary signature also verifies through bytes */
    CHECK(base_verify_packed(sig_b, m, MLEN, pk_b, &pp) == 0, "verify_packed(sig)");
  }
  printf("round-trip + verify-from-bytes: %d iterations OK\n", NITER);

  /* ---- end-to-end PACKED-API tier: unpack -> core -> pack INSIDE the call
   * (the second measured boundary; see basesig.h/las.h) ---- */
  {
    uint8_t ppk_b[PUBLIC_KEY_BYTES], psk_b[SECRET_KEY_BYTES];
    uint8_t Y_b[STATEMENT_BYTES], rw_b[WITNESS_BYTES];
    uint8_t s_b[SIGNATURE_BYTES], p_b[PRE_SIGNATURE_BYTES], a_b[SIGNATURE_BYTES];
    uint8_t w_b[WITNESS_BYTES];
    statement Ys;
    witness rps;

    CHECK(base_keygen_packed(ppk_b, psk_b, &pp) == 0, "base_keygen_packed");
    /* mint a packed statement/witness pair from the relation generator */
    CHECK(relation_gen(&Ys, &rps, &pp) == 0, "relation_gen (statement/witness)");
    pack_statement(Y_b, &Ys);
    CHECK(pack_witness(rw_b, &rps) == 0, "pack_witness");

    CHECK(base_sign_packed(s_b, m, MLEN, ppk_b, psk_b, &pp) == 0, "base_sign_packed");
    CHECK(base_verify_packed(s_b, m, MLEN, ppk_b, &pp) == 0, "base_verify_packed(sign_packed)");
    CHECK(las_presign_packed(p_b, m, MLEN, Y_b, ppk_b, psk_b, &pp) == 0, "las_presign_packed");
    CHECK(las_preverify_packed(p_b, m, MLEN, Y_b, ppk_b, &pp) == 0, "las_preverify_packed");
    CHECK(base_verify_packed(p_b, m, MLEN, ppk_b, &pp) != 0,
          "base_verify_packed(presign_packed) must fail (tripwire through bytes)");
    CHECK(las_adapt_packed(a_b, p_b, m, MLEN, Y_b, rw_b, ppk_b, &pp) == 0, "las_adapt_packed");
    CHECK(base_verify_packed(a_b, m, MLEN, ppk_b, &pp) == 0, "base_verify_packed(adapt_packed)");
    CHECK(las_ext_packed(w_b, a_b, p_b, Y_b, &pp) == 0, "las_ext_packed");
    CHECK(memcmp(w_b, rw_b, WITNESS_BYTES) == 0,
          "las_ext_packed recovers the exact packed (honest, ternary) witness");

    /* base scheme's own packed tier (keygen/sign/verify) */
    CHECK(base_keygen_packed(ppk_b, psk_b, &pp) == 0, "base_keygen_packed (2)");
    CHECK(base_sign_packed(s_b, m, MLEN, ppk_b, psk_b, &pp) == 0, "base_sign_packed (2)");
    CHECK(base_verify_packed(s_b, m, MLEN, ppk_b, &pp) == 0, "base_verify_packed (2)");
    printf("end-to-end packed tier: keygen/sign/verify/presign/preverify/adapt/ext OK\n");
  }

  /* ---- tamper: flipping the low bit of every byte breaks verification.  The
   * encoding has NO padding (8*LAS_CTILDEBYTES + (n+ell)*d*z_bits bits = exactly
   * SIGNATURE_BYTES), so each byte belongs to c_tilde or a USED z field.  Decode
   * is now permissive (c_tilde is raw bytes, z uses FIPS BitUnpack), so every
   * flip is caught at Verify: it changes the stored c_tilde, or changes z (hence
   * w') so the recomputed challenge digest no longer matches. ---- */
  {
    int b, broke = 0;
    for(b = 0; b < SIGNATURE_BYTES; ++b) {
      uint8_t saved = adp_b[b];
      adp_b[b] ^= 0x01;
      if(base_verify_packed(adp_b, m, MLEN, pk_b, &pp) != 0) ++broke;
      adp_b[b] = saved;
    }
    CHECK(broke == SIGNATURE_BYTES, "low-bit flip of every byte must break verification");
    printf("tamper: low-bit flip of all %d bytes rejected\n", SIGNATURE_BYTES);
  }

  /* ---- validation: unpack rejects malformed bytes (separate typed buffers) ---- */
  {
    uint8_t pkbuf[PUBLIC_KEY_BYTES], skbuf[SECRET_KEY_BYTES];
    public_key tpk; secret_key tsk;

    /* pk coeff = 0x7FFFFF >= Q  -> reject */
    memset(pkbuf, 0xFF, sizeof pkbuf);
    CHECK(unpack_public_key(&tpk, pkbuf) == -1, "unpack_pk must reject coeff>=Q");

    /* sk 2-bit code 3 -> reject */
    memset(skbuf, 0xFF, sizeof skbuf);
    CHECK(unpack_secret_key(&tsk, skbuf) == -1, "unpack_sk must reject code 3");

    /* The signature's challenge c_tilde is LAS_CTILDEBYTES of raw bytes and z uses
     * the upstream FIPS BitUnpack: both decode permissively -- any bytes, and any
     * field value, INCLUDING the many that fall outside the valid response band,
     * since the band does not fill the field -- so there is NO decode-time rejection for a
     * signature -- a tampered (c_tilde, z) is caught at Verify instead (the
     * tamper loop above exercises exactly that via base_verify_packed). */
    printf("validation: unpack rejects coeff>=Q (pk) and code-3 sk; sig decode is upstream-permissive\n");
  }

  /* ---- validation: pack rejects out-of-range inputs ---- */
  {
    secret_key bad_sk = sk;
    signature bad_sig = adapted;
    bad_sk.r[0].coeffs[0] = 2;                         /* non-ternary */
    CHECK(pack_secret_key(sk_b, &bad_sk) == -1, "pack_sk must reject non-ternary");
    bad_sig.z[0].coeffs[0] = GAMMA;                    /* > g-k, out of band */
    CHECK(pack_signature(sig_b, &bad_sig) == -1, "pack_sig must reject z out of band");
    printf("validation: pack rejects non-ternary sk and out-of-band z\n");
  }

  printf("=== All serialisation tests passed. ===\n");
  printf("These packed sizes are the realistic on-wire / on-chain object sizes for\n");
  printf("the simplified scheme; base_verify_packed is the byte-level verifier an\n");
  printf("on-chain integration (poqeth-style) would call.\n");
  return 0;
}
