/* export_packed.c — write ONE real packed LAS adapted signature (LAS_SIG_BYTES)
 * to a file, so the Foundry/EVM gas benchmark measures the on-chain calldata cost
 * of a genuine LAS signature's exact byte distribution (not a synthetic blob).
 *
 * Fully deterministic (seeded), so the exported bytes are reproducible. Usage:
 *   make test/export_packed && ./test/export_packed ../evm/test/las_sig.bin
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "../las.h"
#include "../serialize.h"

int main(int argc, char **argv) {
  uint8_t ppseed[LAS_SEEDBYTES], kseed[LAS_SEEDBYTES], yseed[LAS_SEEDBYTES], msg[32];
  las_pp pp;
  las_pk pk, Y;
  las_sk sk, yw;
  las_sig presig, adapted;
  uint8_t sig_b[LAS_SIG_BYTES];
  unsigned int i, nz = 0;
  FILE *f;

  for(i = 0; i < LAS_SEEDBYTES; ++i) { ppseed[i] = (uint8_t)i; kseed[i] = (uint8_t)(i+1); yseed[i] = (uint8_t)(i+100); }
  for(i = 0; i < 32; ++i) msg[i] = (uint8_t)i;

  las_setup(&pp, ppseed);
  las_keygen_seed(&pk, &sk, &pp, kseed);
  las_keygen_seed(&Y, &yw, &pp, yseed);
  las_presign_det(&presig, msg, 32, &Y, &pk, &sk, &pp);
  if(las_adapt(&adapted, &presig, msg, 32, &Y, &yw, &pk, &pp)) { fprintf(stderr, "adapt failed\n"); return 1; }
  if(las_pack_sig(sig_b, &adapted)) { fprintf(stderr, "pack failed\n"); return 1; }

  for(i = 0; i < LAS_SIG_BYTES; ++i) if(sig_b[i] != 0) ++nz;

  f = (argc > 1) ? fopen(argv[1], "wb") : stdout;
  if(!f) { perror("fopen"); return 1; }
  fwrite(sig_b, 1, LAS_SIG_BYTES, f);
  if(f != stdout) fclose(f);

  /* report the byte profile so the EVM calldata gas (16/non-zero, 4/zero) is auditable */
  fprintf(stderr, "wrote %d bytes: %u non-zero, %u zero  ->  calldata gas = %u\n",
          LAS_SIG_BYTES, nz, LAS_SIG_BYTES - nz, 16u*nz + 4u*(LAS_SIG_BYTES - nz));
  return 0;
}
