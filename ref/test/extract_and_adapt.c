/* extract_and_adapt.c — u2's half of eprint 2020/845 Fig. 1: recover the witness
 * from the adapted signature u1 PUBLISHED ON CHAIN, then adapt the pre-signature
 * u2 is holding and settle the other leg.
 *
 * WHY THIS IS A SEPARATE PROGRAM.  This step is the one that makes an atomic swap
 * atomic, and it only means anything if its input comes from the LEDGER.  If it
 * shared a process with the code that produced sigma_2, nothing would stop it
 * quietly using the in-memory copy, and the run would demonstrate an adaptor
 * signature rather than a settlement.  So this program takes the observed
 * signature as an explicit file argument -- the bytes the runner pulled back out
 * of the mined transaction -- and DELIBERATELY never opens sigma2.bin.
 *
 * The corresponding whole-run assertion (that those bytes are the ones the chain
 * actually carried) belongs to the runner's verdict, which compares what it read
 * from the node against what it retained before broadcasting.
 *
 * What u2 knows: pp, Y, its own sigma_hat_2, u1's sigma_hat_1, both public keys,
 * both leg messages.  It does NOT know y until it computes it here.
 *
 * Usage:  extract_and_adapt <dir> <sigma2_observed_on_chain.bin>
 * Writes: <dir>/witness_extracted.bin, <dir>/sigma1.bin
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../basesig.h"
#include "../las.h"
#include "../serialize.h"
#include "../setup.h"

#define MSG_BYTES 32

static int read_path(const char *path, uint8_t *b, size_t n) {
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

static int read_blob(const char *dir, const char *name, uint8_t *b, size_t n) {
  char path[512];
  snprintf(path, sizeof path, "%s/%s", dir, name);
  return read_path(path, b, n);
}

static int write_blob(const char *dir, const char *name, const uint8_t *b, size_t n) {
  char path[512];
  snprintf(path, sizeof path, "%s/%s", dir, name);
  FILE *f = fopen(path, "wb");
  if(!f) { perror(path); return -1; }
  if(fwrite(b, 1, n, f) != n) { fprintf(stderr, "short write: %s\n", path); fclose(f); return -1; }
  fclose(f);
  return 0;
}

/* Present iff the runner kept the honest witness for cross-checking. */
static int read_optional(const char *dir, const char *name, uint8_t *b, size_t n) {
  char path[512];
  snprintf(path, sizeof path, "%s/%s", dir, name);
  FILE *f = fopen(path, "rb");
  if(!f) return 1; /* absent, not an error */
  size_t got = fread(b, 1, n, f);
  fclose(f);
  return (got == n) ? 0 : 1;
}

int main(int argc, char **argv) {
  const char *dir, *sig2_path;
  uint8_t ppseed[LAS_SEEDBYTES];
  public_params pp;
  uint8_t pk1_b[PUBLIC_KEY_BYTES], pk2_b[PUBLIC_KEY_BYTES], Y_b[STATEMENT_BYTES];
  uint8_t msgA[MSG_BYTES], msgB[MSG_BYTES];
  uint8_t presig1[PRE_SIGNATURE_BYTES], presig2[PRE_SIGNATURE_BYTES];
  uint8_t sigma2_chain[SIGNATURE_BYTES], sigma1[SIGNATURE_BYTES];
  uint8_t y_ext[WITNESS_BYTES], y_honest[WITNESS_BYTES];

  if(argc < 3) {
    fprintf(stderr, "usage: %s <dir> <sigma2_observed_on_chain.bin>\n", argv[0]);
    return 2;
  }
  dir = argv[1];
  sig2_path = argv[2];

  if(read_blob(dir, "pp_seed.bin", ppseed, LAS_SEEDBYTES)) return 1;
  setup_public_params(&pp, ppseed);
  if(read_blob(dir, "pk1.bin", pk1_b, PUBLIC_KEY_BYTES)) return 1;
  if(read_blob(dir, "pk2.bin", pk2_b, PUBLIC_KEY_BYTES)) return 1;
  if(read_blob(dir, "Y.bin", Y_b, STATEMENT_BYTES)) return 1;
  if(read_blob(dir, "legA_msg.bin", msgA, MSG_BYTES)) return 1;
  if(read_blob(dir, "legB_msg.bin", msgB, MSG_BYTES)) return 1;
  if(read_blob(dir, "presig1.bin", presig1, PRE_SIGNATURE_BYTES)) return 1;
  if(read_blob(dir, "presig2.bin", presig2, PRE_SIGNATURE_BYTES)) return 1;

  /* THE CHAIN'S BYTES.  Note the path is an argument, not a name under <dir>:
   * this program has no way to reach sigma2.bin and is not meant to. */
  if(read_path(sig2_path, sigma2_chain, SIGNATURE_BYTES)) return 1;

  /* u2 first satisfies itself that what it observed really is a valid signature
   * on leg B under u1's counterparty key.  If this fails, the chain does not
   * carry what u2 was promised and there is nothing to extract. */
  if(base_verify_packed(sigma2_chain, msgB, MSG_BYTES, pk2_b, &pp)) {
    fprintf(stderr, "FATAL: the signature observed on chain 2 does not verify on leg B\n");
    return 1;
  }

  /* Ext: y = z - z_hat, accepted only if A*y == Y.  That equality IS the check --
   * it is why a wrong or truncated observation cannot yield a usable witness. */
  if(las_ext_packed(y_ext, sigma2_chain, presig2, Y_b, &pp)) {
    fprintf(stderr, "FATAL: Ext refused -- the recovered y does not satisfy A*y = Y\n");
    return 1;
  }
  if(write_blob(dir, "witness_extracted.bin", y_ext, WITNESS_BYTES)) return 1;

  /* EVIDENCE-ONLY cross-check, which u2 itself could not perform: the recovered
   * witness equals the one Gen produced.  Skipped when the runner did not retain
   * it.  Ext's own A*y == Y test above is the operative gate. */
  if(read_optional(dir, "witness.bin", y_honest, WITNESS_BYTES) == 0) {
    if(memcmp(y_ext, y_honest, WITNESS_BYTES)) {
      fprintf(stderr, "FATAL: extracted witness differs from the honest one\n");
      return 1;
    }
    printf("ext: recovered witness is byte-identical to Gen's (cross-check, not u2's knowledge)\n");
  } else {
    printf("ext: recovered witness satisfies A*y = Y (honest witness not retained for comparison)\n");
  }

  /* With y in hand u2 completes leg A. */
  if(las_adapt_packed(sigma1, presig1, msgA, MSG_BYTES, Y_b, y_ext, pk1_b, &pp)) {
    fprintf(stderr, "FATAL: Adapt (u2: sigma_hat_1 -> sigma_1) failed\n");
    return 1;
  }
  if(base_verify_packed(sigma1, msgA, MSG_BYTES, pk1_b, &pp)) {
    fprintf(stderr, "FATAL: adapted sigma_1 does not verify as an ordinary signature\n");
    return 1;
  }
  /* sigma_1 settles leg A and nothing else: it must not verify on leg B. */
  if(!base_verify_packed(sigma1, msgB, MSG_BYTES, pk1_b, &pp)) {
    fprintf(stderr, "FATAL: sigma_1 also verifies on leg B -- the legs are not bound\n");
    return 1;
  }
  if(write_blob(dir, "sigma1.bin", sigma1, SIGNATURE_BYTES)) return 1;

  printf("extract+adapt: y recovered FROM CHAIN BYTES; sigma_1 written and verified (%d bytes)\n",
         SIGNATURE_BYTES);
  printf("next: publish sigma_1 on chain 1 to settle leg A\n");
  return 0;
}
