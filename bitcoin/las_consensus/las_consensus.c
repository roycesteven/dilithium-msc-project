/* las_consensus.c — LAS verification as a consensus predicate.
 *
 * A thin, total wrapper: expand the public parameters once, then hand bytes to
 * `base_verify_packed`. Nothing here reimplements any part of the scheme, and nothing here
 * may diverge from `ref/` — a second verifier is a second answer.
 *
 * WHY THIS FILE IS C AND NOT C++.  The `ref/` headers carry C11 `_Static_assert` wire-size
 * anchors, which a C++ translation unit cannot parse. The alternatives were to macro over
 * `_Static_assert` (silently disabling the very assertions that pin the wire sizes) or to
 * re-declare the ref API by hand in C++ (two declarations of one ABI, free to drift). Both
 * trade a real safety property for a cosmetic one. Bitcoin Core already links C libraries —
 * libsecp256k1 is C — so the node calls this through the `extern "C"` header directly.
 * Being C also makes the header's "exceptions must not cross this ABI" contract trivially
 * true rather than something a `catch (...)` has to enforce.
 *
 * INITIALISATION.  `A'` is expanded exactly once, on first use, and never written again.
 * `pthread_once` makes that safe under Core's parallel script verification; expanding per
 * call would instead make every measurement a measurement of matrix expansion. After
 * initialisation the parameters are read-only, so concurrent verifiers share them with no
 * further synchronisation.
 */
#include "las_consensus.h"
#include "las_consensus_params.h"

#include <pthread.h>

#include "basesig.h"
#include "serialize.h"
#include "setup.h"

static public_params g_pp;
static pthread_once_t g_once = PTHREAD_ONCE_INIT;

static void init_params(void)
{
  setup_public_params(&g_pp, LAS_CONSENSUS_SEED);
}

size_t LASConsensusSigBytes(void) { return SIGNATURE_BYTES; }
size_t LASConsensusPkBytes(void)  { return PUBLIC_KEY_BYTES; }

void LASConsensusSeed(uint8_t out[32])
{
  size_t i;
  for(i = 0; i < LAS_CONSENSUS_SEED_BYTES; ++i) out[i] = LAS_CONSENSUS_SEED[i];
}

int LASConsensusVerify(const uint8_t* sig, size_t siglen,
                       const uint8_t* pk, size_t pklen,
                       const uint8_t* msg32, size_t msglen)
{
  if(sig == NULL || pk == NULL || msg32 == NULL) return 0;
  /* Lengths are checked here as well as by the caller. The interpreter reassembles these
   * buffers from witness chunks, so this is the last place a reassembly bug can be caught
   * before bytes reach the scheme. */
  if(siglen != SIGNATURE_BYTES) return 0;
  if(pklen  != PUBLIC_KEY_BYTES) return 0;
  /* A 32-byte message is what binds a signature to one transaction. Accepting any other
   * length would let a signer commit to something that is not a BIP341 sighash. */
  if(msglen != LAS_CONSENSUS_MSG_BYTES) return 0;

  pthread_once(&g_once, init_params);
  return base_verify_packed(sig, msg32, msglen, pk, &g_pp) == 0 ? 1 : 0;
}
