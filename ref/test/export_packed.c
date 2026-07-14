/* export_packed.c — write ONE real packed LAS adapted signature (SIGNATURE_BYTES)
 * to a file, so the Foundry/EVM gas benchmark measures the on-chain calldata cost
 * of a genuine LAS signature's exact byte distribution (not a synthetic blob).
 *
 * Fully deterministic (seeded), so the exported bytes are reproducible. Usage:
 *   make test/export_packed && ./test/export_packed ../evm/test/las_sig.bin
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "../basesig.h"    /* base_keygen_seed */
#include "../relation.h"   /* relation_gen_seed -> (statement, witness) */
#include "../las.h"        /* las_presign_det / las_adapt */
#include "../serialize.h"

int main(int argc, char **argv) {
  uint8_t ppseed[LAS_SEEDBYTES], kseed[LAS_SEEDBYTES], rseed[LAS_SEEDBYTES], msg[32];
  public_params pp;
  public_key pk;
  statement Y;
  secret_key sk;
  witness r_prime;
  pre_signature presig;
  signature adapted;
  uint8_t sig_b[SIGNATURE_BYTES];
  unsigned int i, nz = 0;
  FILE *f;

  for(i = 0; i < LAS_SEEDBYTES; ++i) { ppseed[i] = (uint8_t)i; kseed[i] = (uint8_t)(i+1); rseed[i] = (uint8_t)(i+100); }
  for(i = 0; i < 32; ++i) msg[i] = (uint8_t)i;

  setup_public_params(&pp, ppseed);
  if(base_keygen_seed(&pk, &sk, &pp, kseed))            { fprintf(stderr, "keygen failed\n"); return 1; }
  if(relation_gen_seed(&Y, &r_prime, &pp, rseed))       { fprintf(stderr, "gen failed\n"); return 1; }
  if(las_presign_det(&presig, msg, 32, &Y, &pk, &sk, &pp)) { fprintf(stderr, "presign failed\n"); return 1; }
  if(las_adapt(&adapted, &presig, msg, 32, &Y, &r_prime, &pk, &pp)) { fprintf(stderr, "adapt failed\n"); return 1; }
  if(pack_signature(sig_b, &adapted)) { fprintf(stderr, "pack failed\n"); return 1; }

  for(i = 0; i < SIGNATURE_BYTES; ++i) if(sig_b[i] != 0) ++nz;

  f = (argc > 1) ? fopen(argv[1], "wb") : stdout;
  if(!f) { perror("fopen"); return 1; }
  fwrite(sig_b, 1, SIGNATURE_BYTES, f);
  if(f != stdout) fclose(f);

  /* report the byte profile so the EVM calldata gas (16/non-zero, 4/zero) is auditable */
  fprintf(stderr, "wrote %d bytes: %u non-zero, %u zero  ->  calldata gas = %u\n",
          SIGNATURE_BYTES, nz, SIGNATURE_BYTES - nz, 16u*nz + 4u*(SIGNATURE_BYTES - nz));
  return 0;
}
