/* las_btc_tool.c — the signer side of the patched-node experiment, and the shim's own
 * self-test.
 *
 * It exists so that whatever produces a signature and whatever the node verifies agree by
 * CONSTRUCTION: both go through `las_consensus.h`, both use the seed compiled in from
 * `las_consensus_params.h`, and neither holds a second copy of anything.
 *
 * ENTIRELY DETERMINISTIC. Key generation uses `base_keygen_seed` and signing uses
 * `base_sign_det`, both explicit-seed entry points, so a run is reproducible and this
 * program links the SAME aborting `randombytes` stub the node does. If any path here ever
 * reached for entropy it would crash rather than emit an unreproducible run — the tool is
 * held to the node's standard on purpose.
 *
 * WHERE THE SEED'S PROVENANCE IS CHECKED. `las_consensus_params.h` documents the seed as
 * SHA-256("LAS-CONSENSUS-PARAMS-v1"). That relation is NOT asserted here: `ref/` ships
 * SHAKE, not SHA-256, and a check that cannot actually be performed is worse than none.
 * The runner (`scripts/run_btc_las_node.sh`) hashes the preimage in Python and compares it
 * against what `las_btc_tool seed` prints, so the documented derivation is verified against
 * the bytes actually compiled in, on every run.
 *
 * Subcommands:
 *   seed                     print the consensus parameter seed compiled in
 *   keygen <dir> <seedhex>   write pk.bin, sk.bin under the consensus parameters
 *   sign   <dir> <msgfile>   sign a 32-byte message (a BIP341 sighash) -> sig.bin
 *   selftest                 the positive control and 7 negative controls
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "las_consensus.h"
#include "las_consensus_params.h"

#include "basesig.h"
#include "serialize.h"
#include "setup.h"

static int fail(const char *msg)
{
  fprintf(stderr, "FATAL: %s\n", msg);
  return 1;
}

static int read_exact(const char *dir, const char *name, uint8_t *buf, size_t n)
{
  char path[512];
  FILE *f;
  size_t got;
  int extra;
  if(dir) snprintf(path, sizeof path, "%s/%s", dir, name);
  else    snprintf(path, sizeof path, "%s", name);
  f = fopen(path, "rb");
  if(!f) { perror(path); return -1; }
  got = fread(buf, 1, n, f);
  extra = fgetc(f) != EOF;
  fclose(f);
  if(got != n || extra) {
    fprintf(stderr, "%s: expected exactly %zu bytes, got %zu%s\n",
            path, n, got, extra ? " (or more)" : "");
    return -1;
  }
  return 0;
}

static int write_exact(const char *dir, const char *name, const uint8_t *buf, size_t n)
{
  char path[512];
  FILE *f;
  int ok;
  snprintf(path, sizeof path, "%s/%s", dir, name);
  f = fopen(path, "wb");
  if(!f) { perror(path); return -1; }
  ok = fwrite(buf, 1, n, f) == n;
  fclose(f);
  if(!ok) { fprintf(stderr, "%s: short write\n", path); return -1; }
  return 0;
}

static int parse_hex32(const char *hex, uint8_t out[32])
{
  int i;
  if(strlen(hex) != 64) return -1;
  for(i = 0; i < 32; ++i) {
    unsigned v;
    if(sscanf(hex + 2 * i, "%2x", &v) != 1) return -1;
    out[i] = (uint8_t)v;
  }
  return 0;
}

/* Keys are generated under the CONSENSUS parameters, not a local set: a key produced under
 * any other A' would simply never verify on the node, and it is better to be unable to
 * express that than to discover it from a rejected transaction. */
static void consensus_params(public_params *pp)
{
  uint8_t seed[32];
  LASConsensusSeed(seed);
  setup_public_params(pp, seed);
}

static int cmd_seed(void)
{
  uint8_t seed[32];
  int i;
  LASConsensusSeed(seed);
  for(i = 0; i < 32; ++i) printf("%02x", seed[i]);
  printf("\n");
  return 0;
}

static int cmd_keygen(const char *dir, const char *seedhex)
{
  uint8_t kseed[32];
  public_params pp;
  public_key pk;
  secret_key sk;
  static uint8_t pkb[PUBLIC_KEY_BYTES], skb[SECRET_KEY_BYTES];

  if(parse_hex32(seedhex, kseed)) return fail("keygen seed must be 64 hex characters");
  consensus_params(&pp);
  if(base_keygen_seed(&pk, &sk, &pp, kseed)) return fail("base_keygen_seed failed");

  pack_public_key(pkb, &pk);
  if(pack_secret_key(skb, &sk)) return fail("pack_secret_key failed");
  if(write_exact(dir, "pk.bin", pkb, sizeof pkb)) return 1;
  if(write_exact(dir, "sk.bin", skb, sizeof skb)) return 1;
  printf("keygen: pk.bin (%d B), sk.bin (%d B) under the consensus parameters\n",
         PUBLIC_KEY_BYTES, SECRET_KEY_BYTES);
  return 0;
}

static int cmd_sign(const char *dir, const char *msgfile)
{
  uint8_t msg[LAS_CONSENSUS_MSG_BYTES];
  static uint8_t pkb[PUBLIC_KEY_BYTES], skb[SECRET_KEY_BYTES], sigb[SIGNATURE_BYTES];
  public_params pp;
  public_key pk;
  secret_key sk;
  signature sig;

  if(read_exact(NULL, msgfile, msg, sizeof msg)) return 1;
  if(read_exact(dir, "pk.bin", pkb, sizeof pkb)) return 1;
  if(read_exact(dir, "sk.bin", skb, sizeof skb)) return 1;

  consensus_params(&pp);
  if(unpack_public_key(&pk, pkb)) return fail("pk.bin did not decode");
  if(unpack_secret_key(&sk, skb)) return fail("sk.bin did not decode");
  if(base_sign_det(&sig, msg, sizeof msg, &pk, &sk, &pp)) return fail("base_sign_det failed");
  if(pack_signature(sigb, &sig)) return fail("pack_signature failed");

  /* Verify through the SHIM, not through base_verify directly: if the node is going to
   * reject this signature, it must be found here rather than by a failed broadcast. */
  if(!LASConsensusVerify(sigb, sizeof sigb, pkb, sizeof pkb, msg, sizeof msg))
    return fail("the freshly produced signature does not verify through the consensus shim");

  if(write_exact(dir, "sig.bin", sigb, sizeof sigb)) return 1;
  printf("sign: sig.bin (%d B) over the 32-byte message, verified through the shim\n",
         SIGNATURE_BYTES);
  return 0;
}

/* ------------------------------------------------------------------- selftest */

static int g_failures;

static void check(const char *name, int expect_accept, int got)
{
  int accepted = (got == 1);
  int ok = (accepted == (expect_accept != 0));
  printf("  %-46s %-8s %s\n", name, accepted ? "ACCEPT" : "reject",
         ok ? "OK" : "*** WRONG ***");
  if(!ok) ++g_failures;
}

static int cmd_selftest(void)
{
  public_params pp;
  public_key pk, pk_other;
  secret_key sk, sk_other;
  signature s, s_other, s_otherkey;
  static uint8_t sigb[SIGNATURE_BYTES], sigb_other[SIGNATURE_BYTES], sigb_otherkey[SIGNATURE_BYTES];
  static uint8_t pkb[PUBLIC_KEY_BYTES], pkb_other[PUBLIC_KEY_BYTES];
  static uint8_t tmp_sig[SIGNATURE_BYTES], tmp_pk[PUBLIC_KEY_BYTES];
  uint8_t kseed[32], kseed2[32], msg[32], msg_other[32], tmp_msg[32];
  int i;

  consensus_params(&pp);
  for(i = 0; i < 32; ++i) { kseed[i] = (uint8_t)(i + 7); kseed2[i] = (uint8_t)(i + 200); }
  for(i = 0; i < 32; ++i) { msg[i] = (uint8_t)(i * 3 + 1); msg_other[i] = (uint8_t)(i * 5 + 2); }

  if(base_keygen_seed(&pk, &sk, &pp, kseed)) return fail("keygen failed");
  if(base_keygen_seed(&pk_other, &sk_other, &pp, kseed2)) return fail("keygen (other) failed");
  if(base_sign_det(&s, msg, 32, &pk, &sk, &pp)) return fail("sign failed");
  if(base_sign_det(&s_other, msg_other, 32, &pk, &sk, &pp)) return fail("sign (other msg) failed");
  if(base_sign_det(&s_otherkey, msg, 32, &pk_other, &sk_other, &pp)) return fail("sign (other key) failed");

  if(pack_signature(sigb, &s)) return fail("pack failed");
  if(pack_signature(sigb_other, &s_other)) return fail("pack failed");
  if(pack_signature(sigb_otherkey, &s_otherkey)) return fail("pack failed");
  pack_public_key(pkb, &pk);
  pack_public_key(pkb_other, &pk_other);

  printf("las_consensus selftest (consensus seed; %d B sig / %d B pk)\n",
         SIGNATURE_BYTES, PUBLIC_KEY_BYTES);

  /* POSITIVE CONTROL. Without this passing, every rejection below is vacuous: a shim that
   * rejected everything would "pass" all seven negatives. */
  check("positive: genuine (pk, msg, sig)", 1,
        LASConsensusVerify(sigb, SIGNATURE_BYTES, pkb, PUBLIC_KEY_BYTES, msg, 32));

  /* 1. a flipped bit anywhere in the signature must not verify. */
  memcpy(tmp_sig, sigb, SIGNATURE_BYTES);
  tmp_sig[SIGNATURE_BYTES / 2] ^= 0x01;
  check("1. signature bit flipped", 0,
        LASConsensusVerify(tmp_sig, SIGNATURE_BYTES, pkb, PUBLIC_KEY_BYTES, msg, 32));

  /* 2. a flipped bit in the public key: verifying under a key nobody holds. */
  memcpy(tmp_pk, pkb, PUBLIC_KEY_BYTES);
  tmp_pk[PUBLIC_KEY_BYTES / 2] ^= 0x01;
  check("2. public key bit flipped", 0,
        LASConsensusVerify(sigb, SIGNATURE_BYTES, tmp_pk, PUBLIC_KEY_BYTES, msg, 32));

  /* 3. a flipped bit in the message. This is the property that binds a signature to one
   *    transaction, since on the node the message IS the BIP341 sighash. */
  memcpy(tmp_msg, msg, 32);
  tmp_msg[0] ^= 0x01;
  check("3. message bit flipped", 0,
        LASConsensusVerify(sigb, SIGNATURE_BYTES, pkb, PUBLIC_KEY_BYTES, tmp_msg, 32));

  /* 4. wrong signature length — the interpreter reassembles this from witness chunks, so a
   *    length error is a reassembly error and must not be papered over. */
  check("4. signature one byte short", 0,
        LASConsensusVerify(sigb, SIGNATURE_BYTES - 1, pkb, PUBLIC_KEY_BYTES, msg, 32));

  /* 5. wrong public key length, likewise. */
  check("5. public key one byte short", 0,
        LASConsensusVerify(sigb, SIGNATURE_BYTES, pkb, PUBLIC_KEY_BYTES - 1, msg, 32));

  /* 6. a signature over a DIFFERENT message under the same key: replay across messages. */
  check("6. valid signature, wrong message", 0,
        LASConsensusVerify(sigb_other, SIGNATURE_BYTES, pkb, PUBLIC_KEY_BYTES, msg, 32));

  /* 7. a signature under a DIFFERENT key over the same message: replay across keys. */
  check("7. valid signature, wrong key", 0,
        LASConsensusVerify(sigb_otherkey, SIGNATURE_BYTES, pkb, PUBLIC_KEY_BYTES, msg, 32));

  /* A message of any length other than 32 is refused outright: accepting one would let a
   * signer commit to something that is not a BIP341 sighash. */
  check("bonus: message length 31 refused", 0,
        LASConsensusVerify(sigb, SIGNATURE_BYTES, pkb, PUBLIC_KEY_BYTES, msg, 31));
  check("bonus: message length 33 refused", 0,
        LASConsensusVerify(sigb, SIGNATURE_BYTES, pkb, PUBLIC_KEY_BYTES, msg, 33));

  /* Each of the other-message / other-key signatures must verify in ITS OWN context.
   * Without these, negatives 6 and 7 could be passing merely because those signatures were
   * malformed, rather than because binding works. */
  check("control: other-message sig on its own message", 1,
        LASConsensusVerify(sigb_other, SIGNATURE_BYTES, pkb, PUBLIC_KEY_BYTES, msg_other, 32));
  check("control: other-key sig under its own key", 1,
        LASConsensusVerify(sigb_otherkey, SIGNATURE_BYTES, pkb_other, PUBLIC_KEY_BYTES, msg, 32));

  if(g_failures) {
    printf("\nSELFTEST FAILED: %d case(s) behaved wrongly\n", g_failures);
    return 1;
  }
  printf("\nselftest OK: positive control accepted, 7 negative controls rejected,\n"
         "             message-length and cross-context controls behaved as required\n");
  return 0;
}

int main(int argc, char **argv)
{
  if(argc < 2) {
    fprintf(stderr, "usage: %s {seed|keygen <dir> <seedhex>|sign <dir> <msgfile>|selftest}\n",
            argv[0]);
    return 2;
  }
  if(!strcmp(argv[1], "seed"))     return cmd_seed();
  if(!strcmp(argv[1], "selftest")) return cmd_selftest();
  if(!strcmp(argv[1], "keygen")) {
    if(argc < 4) return fail("keygen needs <dir> <seedhex>");
    return cmd_keygen(argv[2], argv[3]);
  }
  if(!strcmp(argv[1], "sign")) {
    if(argc < 4) return fail("sign needs <dir> <msgfile>");
    return cmd_sign(argv[2], argv[3]);
  }
  fprintf(stderr, "unknown subcommand: %s\n", argv[1]);
  return 2;
}
