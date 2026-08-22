/*
 * Known-Answer Tests (KATs) for LAS  -  reproducibility (objective C4).
 *
 * Every input here is FIXED (public-parameter seed, key seeds, relation seeds,
 * messages), and keygen / gen / sign / presign are DETERMINISTIC
 * (base_keygen_seed, relation_gen_seed, base_sign_det, las_presign_det).  The
 * whole adaptor contract is therefore a pure function of these inputs, so the
 * serialised outputs are byte-for-byte reproducible across machines and
 * compilers.  We fold the packed bytes of (pk, sk, sig, pre-sig, adapted-sig)
 * for several vectors into one SHAKE256 digest and assert it equals a pinned
 * expected value: a single 32-byte fingerprint that locks down the whole
 * implementation.
 *
 * Per vector we also assert the adaptor contract end-to-end (Verify(sig)=ok,
 * Verify(adapted)=ok, a pre-signature's bytes do NOT verify as an ordinary
 * signature, Ext recovers the witness) and that every deterministic function is
 * actually deterministic (keygen, gen, sign, presign all reproduce byte-for-byte).
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../basesig.h"   /* Algorithm 1: base_keygen_seed / base_sign_det / base_verify */
#include "../relation.h"  /* hard relation: relation_gen_seed -> (statement, witness)     */
#include "../las.h"       /* Algorithm 2: las_presign_det / las_preverify / las_adapt / las_ext */
#include "../serialize.h"
#include "../params.h"
#include "../fips202.h"

#define NVEC  4
#define MLEN  33

#define CHECK(cond, msg) do { if(!(cond)) { \
  fprintf(stderr, "  [FAIL] %s\n", msg); return 1; } } while(0)

/* Compile-time size anchors.  The semantic byte-size EQUALITIES hold for every
 * parameter set (a statement is pk-shaped, a witness sk-shaped, a pre-signature
 * sig-shaped); the concrete values are the Simplified-Dilithium-III set this
 * pinned digest locks (LAS_N=6, ELL=5, KAPPA=49), matching the Rust port. */
typedef char kat_size_relations[
  (STATEMENT_BYTES == PUBLIC_KEY_BYTES &&
   WITNESS_BYTES == SECRET_KEY_BYTES &&
   PRE_SIGNATURE_BYTES == SIGNATURE_BYTES) ? 1 : -1];
/* SIGNATURE_BYTES = LAS_CTILDEBYTES(48) + 11*256*19/8 (6688) = 6736.  The digest
 * is the FIPS 204 lambda/4 width for the ML-DSA-65-aligned target (sec. 7.3,
 * Algorithm 29), not the flat 32 bytes this build previously used, so the
 * signature is 16 B larger and the pinned digest below changed with it. */
typedef char kat_size_d3[
  (PUBLIC_KEY_BYTES == 4416 && SECRET_KEY_BYTES == 704 &&
   LAS_CTILDEBYTES == 48 &&
   SIGNATURE_BYTES == 6736 && LAS_Z_COEFF_BITS == 19) ? 1 : -1];

/* Pinned expected digest (Stage B: the c_tilde challenge lifecycle + the
 * c_tilde || BitPack(z) wire format).  Measured from a real Rust las_kat run and
 * pinned identically in both languages -- the cross-language interoperability
 * gate (tests/las_kat.rs prints the same value). */
/* Regenerated 2026-07-29 when c_tilde moved from a flat 32 bytes to the FIPS 204
 * lambda/4 width (48 B for this ML-DSA-65-aligned set).  The previous value was
 * bb6ad0da...260c; it is NOT preserved, because the wire format legitimately
 * changed.  C and Rust reached this value independently. */
static const uint8_t EXPECTED[32] = {
  0xb4, 0xa1, 0x0f, 0xfb, 0x6e, 0x64, 0x5e, 0x50,
  0x76, 0xd1, 0xff, 0x59, 0x93, 0xfa, 0xa7, 0x29,
  0x09, 0x23, 0x2f, 0xc7, 0x1e, 0x55, 0x4b, 0x93,
  0x54, 0x41, 0x41, 0xd6, 0x59, 0x05, 0x03, 0xbe
};

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
  keccak_state acc;
  uint8_t digest[32];
  unsigned int v, i;

  for(i = 0; i < LAS_SEEDBYTES; ++i) ppseed[i] = (uint8_t)i;   /* fixed A */
  setup_public_params(&pp, ppseed);
  shake256_init(&acc);

  printf("=== LAS known-answer tests (mode %d, %d vectors) ===\n", DILITHIUM_MODE, NVEC);

  for(v = 0; v < NVEC; ++v) {
    uint8_t kseed[LAS_SEEDBYTES], rseed[LAS_SEEDBYTES], msg[MLEN];
    public_key pk, pk2;
    secret_key sk, sk2;
    statement Y, Y2;
    witness r_prime, r_prime2, s_ext;   /* honest witness r' (Gen); extracted witness s (Ext) */
    signature sig, sig2, adapted;
    pre_signature presig, presig2;
    uint8_t pk_b[PUBLIC_KEY_BYTES], sk_b[SECRET_KEY_BYTES];
    uint8_t sig_b[SIGNATURE_BYTES], pre_b[PRE_SIGNATURE_BYTES], adp_b[SIGNATURE_BYTES];

    for(i = 0; i < LAS_SEEDBYTES; ++i) {
      kseed[i] = (uint8_t)(7u * v + i + 1u);       /* key pair seed         */
      rseed[i] = (uint8_t)(7u * v + i + 100u);     /* relation (Y, r') seed  */
    }
    for(i = 0; i < MLEN; ++i) msg[i] = (uint8_t)(37u * v + i);

    /* deterministic keygen / statement-witness / sign / presign / adapt
     * (all return 0 on success; check it) */
    CHECK(base_keygen_seed(&pk, &sk, &pp, kseed) == 0, "keygen_seed");
    CHECK(relation_gen_seed(&Y, &r_prime, &pp, rseed) == 0, "gen_seed");
    CHECK(base_sign_det(&sig, msg, MLEN, &pk, &sk, &pp) == 0, "sign_det");
    CHECK(las_presign_det(&presig, msg, MLEN, &Y, &pk, &sk, &pp) == 0, "presign_det");
    CHECK(las_adapt(&adapted, &presig, msg, MLEN, &Y, &r_prime, &pk, &pp) == 0, "adapt");

    /* adaptor contract */
    CHECK(base_verify(&sig, msg, MLEN, &pk, &pp) == 0, "verify sig");
    CHECK(base_verify(&adapted, msg, MLEN, &pk, &pp) == 0, "verify adapted");
    /* tripwire (byte-level): a pre-signature is a DISTINCT type.  Serialise it,
     * decode those bytes AS an ordinary signature, and confirm Verify rejects it
     * (PreSign hashed w+Y, not w, so the challenge Verify recomputes does not
     * match the stored one). */
    {
      signature presig_as_sig;
      uint8_t relabel_b[PRE_SIGNATURE_BYTES];
      CHECK(pack_pre_signature(relabel_b, &presig) == 0, "pack presig (tripwire)");
      CHECK(unpack_signature(&presig_as_sig, relabel_b) == 0, "presig bytes decode as sig-shaped");
      CHECK(base_verify(&presig_as_sig, msg, MLEN, &pk, &pp) != 0, "presig must not verify");
    }
    CHECK(las_preverify(&presig, msg, MLEN, &Y, &pk, &pp) == 0, "preverify");
    CHECK(las_ext(&s_ext, &adapted, &presig, &Y, &pp) == 0 && witness_eq(&s_ext, &r_prime),
          "ext recovers witness");

    /* determinism: re-running every seeded/deterministic function is identical */
    CHECK(base_keygen_seed(&pk2, &sk2, &pp, kseed) == 0, "keygen_seed re-run");
    CHECK(pk_eq(&pk, &pk2) && sk_eq(&sk, &sk2), "keygen_seed deterministic");
    CHECK(relation_gen_seed(&Y2, &r_prime2, &pp, rseed) == 0, "gen_seed re-run");
    CHECK(stmt_eq(&Y, &Y2) && witness_eq(&r_prime, &r_prime2), "gen_seed deterministic");
    CHECK(base_sign_det(&sig2, msg, MLEN, &pk, &sk, &pp) == 0, "sign_det re-run");
    CHECK(sig_eq(&sig, &sig2), "sign_det deterministic");
    CHECK(las_presign_det(&presig2, msg, MLEN, &Y, &pk, &sk, &pp) == 0, "presign_det re-run");
    CHECK(presig_eq(&presig, &presig2), "presign_det deterministic");

    /* serialise and fold into the running KAT digest */
    pack_public_key(pk_b, &pk);
    CHECK(pack_secret_key(sk_b, &sk) == 0, "pack sk");
    CHECK(pack_signature(sig_b, &sig) == 0, "pack sig");
    CHECK(pack_pre_signature(pre_b, &presig) == 0, "pack presig");
    CHECK(pack_signature(adp_b, &adapted) == 0, "pack adapted");
    shake256_absorb(&acc, pk_b, PUBLIC_KEY_BYTES);
    shake256_absorb(&acc, sk_b, SECRET_KEY_BYTES);
    shake256_absorb(&acc, sig_b, SIGNATURE_BYTES);
    shake256_absorb(&acc, pre_b, PRE_SIGNATURE_BYTES);
    shake256_absorb(&acc, adp_b, SIGNATURE_BYTES);
    printf("  vector %u: contract OK, deterministic, serialised\n", v);
  }

  shake256_finalize(&acc);
  shake256_squeeze(digest, 32, &acc);

  printf("  KAT digest: ");
  for(i = 0; i < 32; ++i) printf("%02x", digest[i]);
  printf("\n");

  if(memcmp(digest, EXPECTED, 32) != 0) {
    fprintf(stderr, "  [FAIL] KAT digest mismatch - reproducibility broken "
                    "(or update EXPECTED on a deliberate change)\n");
    return 1;
  }
  printf("=== KAT digest matches pinned expected value. ===\n");
  return 0;
}
