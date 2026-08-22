/* randombytes_abort.c — the entropy source a consensus build must never reach.
 *
 * `ref/basesig.c` calls `randombytes` on its KEYGEN and SIGN paths. A node only ever
 * VERIFIES, so those paths are unreachable in this build — but "unreachable" is a claim,
 * and linking the real implementation would let a mistake turn into silently
 * non-deterministic validation: two nodes drawing different entropy could reach different
 * conclusions about the same block, which is a chain split rather than a bug report.
 *
 * So the symbol is defined, and it aborts. If verification ever calls it, the node dies
 * immediately and visibly at the point of the error. That is the strictly better failure:
 * a crash is a bug you can find, a consensus divergence is one you cannot.
 *
 * This file is the reason the LAS sources can be linked into consensus code at all.
 */
#include <stdio.h>
#include <stdlib.h>

#include "randombytes.h"

void randombytes(uint8_t *out, size_t outlen)
{
  (void)out;
  (void)outlen;
  fprintf(stderr,
          "FATAL: randombytes() was called inside a consensus build of LAS verification.\n"
          "       Verification must be deterministic: two nodes drawing different entropy\n"
          "       could validate the same block differently. This build links an aborting\n"
          "       stub precisely so that this cannot happen quietly.\n");
  abort();
}
