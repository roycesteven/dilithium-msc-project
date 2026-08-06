/* export_swap_vectors.c — the off-chain half of a TWO-LEG atomic swap (eprint
 * 2020/845 Fig. 1), driven in phases so that each leg's signed message can come
 * from the chain rather than from a fixed test vector.
 *
 * WHY THIS EXISTS AND export_verify_vector.c DOES NOT SUFFICE.  That tool signs
 * msg[i] = i, a constant.  A signature over a constant verifies happily while
 * proving nothing about WHICH payment it authorised, so two settled legs would
 * evidence two valid LAS signatures rather than a swap.  Here the message is an
 * INPUT: the runner reads the digest the settlement contract will check
 * (AdaptorSwapBound.legMessage, or a BIP341 sighash on the Bitcoin side) and
 * writes it into the working directory before `presign` runs.
 *
 * PHASES, mirroring Fig. 1 exactly.  The escrow ids -- and therefore the leg
 * messages -- do not exist until the funding transactions are mined, which is
 * why setup and presign cannot be one step:
 *
 *   setup    u1: (Y,y) <- Gen, pi <- P((Y,y));  u2: VERIFY pi        [gate]
 *            both: key pairs.  Emits everything the funders need to register.
 *   presign  u1: sigma_hat_1 <- PreSign(sk1, Y, txA)   [txA = leg A message]
 *            u2: sigma_hat_2 <- PreSign(sk2, Y, txB)   [txB = leg B message]
 *            both: PreVerify the pre-signature they RECEIVED               [gate]
 *   adapt    u1: sigma_2 <- Adapt((Y,y), sigma_hat_2)  -> published on chain 2
 *
 * The fourth step -- u2 extracting y from the sigma_2 it observes ON CHAIN and
 * adapting sigma_hat_1 -- is deliberately a SEPARATE program
 * (extract_and_adapt.c), because its input must come from the chain and not
 * from this process's memory.  Keeping it separate is what makes that
 * impossible to fake by accident.
 *
 * WHO SIGNS WHICH LEG (Fig. 1).  tx1 spends u1's coins c1 to u2, so leg A is
 * funded by u1 on chain 1 and pre-signed under sk1; tx2 spends u2's coins c2 to
 * u1, so leg B is funded by u2 on chain 2 and pre-signed under sk2.  u1 claims
 * leg B first (revealing y); u2 then claims leg A.
 *
 * Deterministic (fixed seeds), so an evidence run is reproducible.  Poly data is
 * raw int32 LITTLE-ENDIAN, matching export_verify_vector.c, so t1.bin/t2.bin are
 * simultaneously the polynomial source the registration script NTTs and the
 * `tPacked` bytes hashed into the challenge preimage.
 *
 * Usage:  export_swap_vectors setup   <dir>
 *         export_swap_vectors presign <dir>     (reads legA_msg.bin, legB_msg.bin)
 *         export_swap_vectors adapt   <dir>
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../basesig.h"
#include "../relation.h"
#include "../las.h"
#include "../serialize.h"
#include "../poly.h"
#include "../setup.h"

#ifdef LAS_WITH_PI
#include "../relation_zk.h"
#endif

#define MSG_BYTES 32

/* Fixed seeds.  ppseed matches export_verify_vector.c, so pp_normal.bin here is
 * byte-identical to that tool's -- a free cross-check that the two exporters
 * agree on the public parameters. */
static void fill_seeds(uint8_t ppseed[LAS_SEEDBYTES], uint8_t k1[LAS_SEEDBYTES],
                       uint8_t k2[LAS_SEEDBYTES], uint8_t rs[LAS_SEEDBYTES]) {
  unsigned int i;
  for(i = 0; i < LAS_SEEDBYTES; ++i) {
    ppseed[i] = (uint8_t)i;
    k1[i]     = (uint8_t)(i + 1);
    k2[i]     = (uint8_t)(i + 2);
    rs[i]     = (uint8_t)(i + 100);
  }
}

static FILE *openout(const char *dir, const char *name) {
  char path[512];
  snprintf(path, sizeof path, "%s/%s", dir, name);
  FILE *f = fopen(path, "wb");
  if(!f) perror(path);
  return f;
}

static int write_blob(const char *dir, const char *name, const uint8_t *b, size_t n) {
  FILE *f = openout(dir, name);
  if(!f) return -1;
  if(fwrite(b, 1, n, f) != n) { fprintf(stderr, "short write: %s\n", name); fclose(f); return -1; }
  fclose(f);
  return 0;
}

/* Exact-length read: a short or long file is an error, never a silent truncation. */
static int read_blob(const char *dir, const char *name, uint8_t *b, size_t n) {
  char path[512];
  snprintf(path, sizeof path, "%s/%s", dir, name);
  FILE *f = fopen(path, "rb");
  if(!f) { perror(path); return -1; }
  size_t got = fread(b, 1, n, f);
  int extra = fgetc(f) != EOF;
  fclose(f);
  if(got != n || extra) {
    fprintf(stderr, "%s: expected exactly %zu bytes, got %zu%s\n",
            path, n, got, extra ? " (or more)" : "");
    return -1;
  }
  return 0;
}

/* One polynomial as LAS_D int32 little-endian. */
static void write_poly(FILE *f, const poly *p) {
  unsigned int i;
  for(i = 0; i < LAS_D; ++i) {
    uint32_t x = (uint32_t)p->coeffs[i];
    uint8_t b[4] = { (uint8_t)x, (uint8_t)(x>>8), (uint8_t)(x>>16), (uint8_t)(x>>24) };
    fwrite(b, 1, 4, f);
  }
}

/* Recover the NORMAL-domain polynomial from its NTT-domain form.
 * MUST STAY IDENTICAL to the copies in export_verify_vector.c and
 * export_naysayer_vectors.c -- the on-chain verifier uses a different (ZKNox)
 * normal-domain NTT, and a divergence here silently feeds it the wrong A'. */
static void recover_normal(poly *out, const poly *ahat, const poly *one_hat) {
  poly_pointwise_montgomery(out, ahat, one_hat);
  poly_invntt_tomont(out);
  poly_reduce(out);
  poly_caddq(out);
}

static int write_pp_normal(const char *dir, const public_params *pp) {
  poly one, one_hat, tmp;
  unsigned int i, j, k;
  FILE *f;
  for(k = 0; k < LAS_D; ++k) one.coeffs[k] = (k == 0) ? 1 : 0;
  one_hat = one;
  poly_ntt(&one_hat);
  if(!(f = openout(dir, "pp_normal.bin"))) return -1;
  for(i = 0; i < LAS_N; ++i)
    for(j = 0; j < ELL; ++j) {
      recover_normal(&tmp, &pp->a_prime[i][j], &one_hat);
      write_poly(f, &tmp);
    }
  fclose(f);
  return 0;
}

static int write_t(const char *dir, const char *name, const public_key *pk) {
  unsigned int i;
  FILE *f = openout(dir, name);
  if(!f) return -1;
  for(i = 0; i < LAS_N; ++i) write_poly(f, &pk->t[i]);
  fclose(f);
  return 0;
}

/* ------------------------------------------------------------------ phases */

static int phase_setup(const char *dir, int allow_no_pi) {
  uint8_t ppseed[LAS_SEEDBYTES], k1[LAS_SEEDBYTES], k2[LAS_SEEDBYTES], rs[LAS_SEEDBYTES];
  public_params pp;
  public_key pk1, pk2;
  secret_key sk1, sk2;
  statement Y;
  witness r_prime;
  uint8_t pk_b[PUBLIC_KEY_BYTES], sk_b[SECRET_KEY_BYTES];
  uint8_t Y_b[STATEMENT_BYTES], w_b[WITNESS_BYTES];

  fill_seeds(ppseed, k1, k2, rs);
  setup_public_params(&pp, ppseed);

  if(base_keygen_seed(&pk1, &sk1, &pp, k1)) { fprintf(stderr, "u1 keygen failed\n"); return 1; }
  if(base_keygen_seed(&pk2, &sk2, &pp, k2)) { fprintf(stderr, "u2 keygen failed\n"); return 1; }
  if(relation_gen_seed(&Y, &r_prime, &pp, rs)) { fprintf(stderr, "Gen failed\n"); return 1; }

  /* Fig. 1: u1 -> u2 is (Y, pi); u2 VERIFIES pi BEFORE it pre-signs.  Without
   * that check u2 has no assurance that Y admits a ternary witness at all, so
   * the y it later extracts need not complete leg A -- the atomicity argument
   * does not close.  This is a gate, not a formality. */
#ifdef LAS_WITH_PI
  {
    static uint8_t pi[PI_PROOF_MAX_BYTES];
    size_t pilen = 0;
    uint8_t lenbuf[8];
    unsigned int i;
    if(relation_prove(pi, &pilen, &Y, &r_prime, &pp)) {
      fprintf(stderr, "FATAL: relation_prove failed (u1 could not prove knowledge of y)\n");
      return 1;
    }
    if(relation_proof_verify(pi, pilen, &Y, &pp)) {
      fprintf(stderr, "FATAL: u2 rejected pi -- Fig. 1 aborts here\n");
      return 1;
    }
    if(write_blob(dir, "pi.bin", pi, pilen)) return 1;
    for(i = 0; i < 8; ++i) lenbuf[i] = (uint8_t)(pilen >> (8 * i));
    if(write_blob(dir, "pi.len", lenbuf, 8)) return 1;
    printf("pi: proved and VERIFIED, %zu bytes\n", pilen);
  }
#else
  if(!allow_no_pi) {
    fprintf(stderr,
      "FATAL: built without pi support (LAS_WITH_PI undefined), so Fig. 1's proof-of-\n"
      "       knowledge step cannot run.  Build the vendored LaZer library and rebuild\n"
      "       this target, or pass --no-pi to record deliberately that the run omits it.\n");
    return 1;
  }
  printf("pi: SKIPPED by explicit --no-pi (the run is NOT a full Fig. 1 execution)\n");
#endif
  (void)allow_no_pi;

  if(write_pp_normal(dir, &pp)) return 1;
  if(write_blob(dir, "pp_seed.bin", ppseed, LAS_SEEDBYTES)) return 1;

  pack_public_key(pk_b, &pk1);
  if(write_blob(dir, "pk1.bin", pk_b, PUBLIC_KEY_BYTES)) return 1;
  if(pack_secret_key(sk_b, &sk1)) { fprintf(stderr, "pack sk1 failed\n"); return 1; }
  if(write_blob(dir, "sk1.bin", sk_b, SECRET_KEY_BYTES)) return 1;
  if(write_t(dir, "t1.bin", &pk1)) return 1;

  pack_public_key(pk_b, &pk2);
  if(write_blob(dir, "pk2.bin", pk_b, PUBLIC_KEY_BYTES)) return 1;
  if(pack_secret_key(sk_b, &sk2)) { fprintf(stderr, "pack sk2 failed\n"); return 1; }
  if(write_blob(dir, "sk2.bin", sk_b, SECRET_KEY_BYTES)) return 1;
  if(write_t(dir, "t2.bin", &pk2)) return 1;

  pack_statement(Y_b, &Y);
  if(write_blob(dir, "Y.bin", Y_b, STATEMENT_BYTES)) return 1;
  if(pack_witness(w_b, &r_prime)) { fprintf(stderr, "pack witness failed\n"); return 1; }
  if(write_blob(dir, "witness.bin", w_b, WITNESS_BYTES)) return 1;

  printf("setup: pp_normal.bin t1.bin t2.bin pk{1,2}.bin sk{1,2}.bin Y.bin witness.bin\n");
  printf("next: fund both legs, write legA_msg.bin and legB_msg.bin (32 B each), then `presign`\n");
  return 0;
}

/* NOTE ON TIERS.  These phases use the STRUCT-level calls, because PreSign needs
 * the DETERMINISTIC entry point (las_presign_det, mask seed derived from
 * (sk,Y,M)) and the packed tier exposes only the random one -- an evidence run
 * has to be reproducible.  extract_and_adapt.c is the other way round: its
 * inputs genuinely arrive as bytes from the chain, so it uses the packed tier. */
static int phase_presign(const char *dir) {
  uint8_t ppseed[LAS_SEEDBYTES];
  public_params pp;
  uint8_t pk1_b[PUBLIC_KEY_BYTES], sk1_b[SECRET_KEY_BYTES];
  uint8_t pk2_b[PUBLIC_KEY_BYTES], sk2_b[SECRET_KEY_BYTES];
  uint8_t Y_b[STATEMENT_BYTES];
  uint8_t msgA[MSG_BYTES], msgB[MSG_BYTES];
  uint8_t presig1_b[PRE_SIGNATURE_BYTES], presig2_b[PRE_SIGNATURE_BYTES];
  public_key pk1, pk2;
  secret_key sk1, sk2;
  statement Y;
  pre_signature presig1, presig2;

  if(read_blob(dir, "pp_seed.bin", ppseed, LAS_SEEDBYTES)) return 1;
  setup_public_params(&pp, ppseed);
  if(read_blob(dir, "pk1.bin", pk1_b, PUBLIC_KEY_BYTES)) return 1;
  if(read_blob(dir, "sk1.bin", sk1_b, SECRET_KEY_BYTES)) return 1;
  if(read_blob(dir, "pk2.bin", pk2_b, PUBLIC_KEY_BYTES)) return 1;
  if(read_blob(dir, "sk2.bin", sk2_b, SECRET_KEY_BYTES)) return 1;
  if(read_blob(dir, "Y.bin", Y_b, STATEMENT_BYTES)) return 1;

  if(unpack_public_key(&pk1, pk1_b)) { fprintf(stderr, "pk1 decode failed\n"); return 1; }
  if(unpack_public_key(&pk2, pk2_b)) { fprintf(stderr, "pk2 decode failed\n"); return 1; }
  if(unpack_secret_key(&sk1, sk1_b)) { fprintf(stderr, "sk1 decode failed\n"); return 1; }
  if(unpack_secret_key(&sk2, sk2_b)) { fprintf(stderr, "sk2 decode failed\n"); return 1; }
  if(unpack_statement(&Y, Y_b))      { fprintf(stderr, "Y decode failed\n");   return 1; }

  /* The messages are the chain's, not ours.  A 32-byte length is enforced by
   * read_blob: on the EVM side this is AdaptorSwapBound.legMessage, on the
   * Bitcoin side a BIP341 sighash. */
  if(read_blob(dir, "legA_msg.bin", msgA, MSG_BYTES)) return 1;
  if(read_blob(dir, "legB_msg.bin", msgB, MSG_BYTES)) return 1;
  if(!memcmp(msgA, msgB, MSG_BYTES)) {
    fprintf(stderr, "FATAL: the two legs carry the SAME message -- they are not distinct\n"
                    "       payments, and a signature for one would settle the other.\n");
    return 1;
  }

  /* tx1 spends c1 to u2: leg A, u1's key. */
  if(las_presign_det(&presig1, msgA, MSG_BYTES, &Y, &pk1, &sk1, &pp)) {
    fprintf(stderr, "PreSign (u1, leg A) failed\n"); return 1;
  }
  /* tx2 spends c2 to u1: leg B, u2's key. */
  if(las_presign_det(&presig2, msgB, MSG_BYTES, &Y, &pk2, &sk2, &pp)) {
    fprintf(stderr, "PreSign (u2, leg B) failed\n"); return 1;
  }

  /* Fig. 1: each party PreVerifies the pre-signature it RECEIVED, before any
   * Adapt.  Adapting an unverified pre-signature is how a party publishes a
   * signature that leaks its witness in exchange for nothing. */
  if(las_preverify(&presig1, msgA, MSG_BYTES, &Y, &pk1, &pp)) {
    fprintf(stderr, "FATAL: u2 rejected sigma_hat_1 at PreVerify -- Fig. 1 aborts\n"); return 1;
  }
  if(las_preverify(&presig2, msgB, MSG_BYTES, &Y, &pk2, &pp)) {
    fprintf(stderr, "FATAL: u1 rejected sigma_hat_2 at PreVerify -- Fig. 1 aborts\n"); return 1;
  }

  /* A pre-signature must NOT verify against the other leg: same statement Y,
   * different key and different message.  If this ever passed, the two legs
   * would be interchangeable and the swap would not be binding. */
  if(!las_preverify(&presig2, msgA, MSG_BYTES, &Y, &pk1, &pp)) {
    fprintf(stderr, "FATAL: sigma_hat_2 PreVerified against leg A -- legs not bound\n"); return 1;
  }

  if(pack_pre_signature(presig1_b, &presig1)) { fprintf(stderr, "pack sigma_hat_1 failed\n"); return 1; }
  if(pack_pre_signature(presig2_b, &presig2)) { fprintf(stderr, "pack sigma_hat_2 failed\n"); return 1; }
  if(write_blob(dir, "presig1.bin", presig1_b, PRE_SIGNATURE_BYTES)) return 1;
  if(write_blob(dir, "presig2.bin", presig2_b, PRE_SIGNATURE_BYTES)) return 1;
  printf("presign: sigma_hat_1 and sigma_hat_2 written; both PreVerified; cross-leg PreVerify refused\n");
  return 0;
}

static int phase_adapt(const char *dir) {
  uint8_t ppseed[LAS_SEEDBYTES];
  public_params pp;
  uint8_t pk2_b[PUBLIC_KEY_BYTES], Y_b[STATEMENT_BYTES], w_b[WITNESS_BYTES];
  uint8_t msgB[MSG_BYTES], msgA[MSG_BYTES];
  uint8_t presig2_b[PRE_SIGNATURE_BYTES], sigma2_b[SIGNATURE_BYTES];
  public_key pk2;
  statement Y;
  witness r_prime;
  pre_signature presig2;
  signature sigma2;

  if(read_blob(dir, "pp_seed.bin", ppseed, LAS_SEEDBYTES)) return 1;
  setup_public_params(&pp, ppseed);
  if(read_blob(dir, "pk2.bin", pk2_b, PUBLIC_KEY_BYTES)) return 1;
  if(read_blob(dir, "Y.bin", Y_b, STATEMENT_BYTES)) return 1;
  if(read_blob(dir, "witness.bin", w_b, WITNESS_BYTES)) return 1;
  if(read_blob(dir, "legA_msg.bin", msgA, MSG_BYTES)) return 1;
  if(read_blob(dir, "legB_msg.bin", msgB, MSG_BYTES)) return 1;
  if(read_blob(dir, "presig2.bin", presig2_b, PRE_SIGNATURE_BYTES)) return 1;

  if(unpack_public_key(&pk2, pk2_b))          { fprintf(stderr, "pk2 decode failed\n"); return 1; }
  if(unpack_statement(&Y, Y_b))               { fprintf(stderr, "Y decode failed\n");   return 1; }
  if(unpack_witness(&r_prime, w_b))           { fprintf(stderr, "witness decode failed\n"); return 1; }
  if(unpack_pre_signature(&presig2, presig2_b)) { fprintf(stderr, "sigma_hat_2 decode failed\n"); return 1; }

  /* u1 holds y, so u1 is the one who can adapt.  This is the step that, once
   * published, hands u2 the witness. */
  if(las_adapt(&sigma2, &presig2, msgB, MSG_BYTES, &Y, &r_prime, &pk2, &pp)) {
    fprintf(stderr, "Adapt (u1: sigma_hat_2 -> sigma_2) failed\n"); return 1;
  }
  /* The adapted signature is an ORDINARY signature: it must clear the plain
   * verifier, which is what the settlement contract runs.  Checking it here
   * means a failure is attributed to Adapt rather than to the chain. */
  if(base_verify(&sigma2, msgB, MSG_BYTES, &pk2, &pp)) {
    fprintf(stderr, "FATAL: adapted sigma_2 does not verify as an ordinary signature\n"); return 1;
  }
  /* ...and it settles leg B ONLY.  A sigma_2 that also verified on leg A would
   * mean one signature could take both coins. */
  if(!base_verify(&sigma2, msgA, MSG_BYTES, &pk2, &pp)) {
    fprintf(stderr, "FATAL: sigma_2 also verifies on leg A -- the legs are not bound\n"); return 1;
  }
  if(pack_signature(sigma2_b, &sigma2)) { fprintf(stderr, "pack sigma_2 failed\n"); return 1; }
  if(write_blob(dir, "sigma2.bin", sigma2_b, SIGNATURE_BYTES)) return 1;
  printf("adapt: sigma_2 written and verified as an ordinary signature (%d bytes)\n", SIGNATURE_BYTES);
  printf("next: publish it on chain 2, then read it BACK off the chain for extract_and_adapt\n");
  return 0;
}

int main(int argc, char **argv) {
  int allow_no_pi = 0;
  int i;
  const char *cmd, *dir;

  if(argc < 3) {
    fprintf(stderr, "usage: %s {setup|presign|adapt} <dir> [--no-pi]\n", argv[0]);
    return 2;
  }
  cmd = argv[1];
  dir = argv[2];
  for(i = 3; i < argc; ++i)
    if(!strcmp(argv[i], "--no-pi")) allow_no_pi = 1;

  if(!strcmp(cmd, "setup"))   return phase_setup(dir, allow_no_pi);
  if(!strcmp(cmd, "presign")) return phase_presign(dir);
  if(!strcmp(cmd, "adapt"))   return phase_adapt(dir);

  fprintf(stderr, "unknown phase: %s\n", cmd);
  return 2;
}
