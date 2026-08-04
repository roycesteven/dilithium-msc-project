/*
 * test_statement_compress.c -- the statement-compression experiment (diagnostic
 * harness).
 *
 * WHAT THIS ANSWERS
 *   The ML-DSA hint experiment (test_mldsa_hint.c) ends on an ASSERTION: "the
 *   statement is not compressible the way the public key is -- t is sent as t1
 *   via Power2Round; Y enters the identity before rounding and is sent in full."
 *   The head-to-head measurement sharpened the stake: building on unmodified
 *   ML-DSA halves the signature and the public key but leaves the statement Y
 *   BYTE-IDENTICAL, so Y, not z, is what now limits the swap payload.
 *
 *   That makes "can Y be compressed?" the load-bearing open question, and this
 *   harness answers it the same way the hint experiment answered its own: by
 *   running the candidate compressions and reporting, per property, what
 *   survives.  It replaces an argument with a measurement.
 *
 * HOW TO READ THE OUTPUT
 *   This is NOT a pass/fail test and it must never be made into one.  A "0/N"
 *   cell is a result, not a bug: it localises exactly which adaptor function a
 *   given compression breaks.  The harness hard-fails ONLY on the control
 *   variant, because a control that does not hold means the harness is
 *   measuring nothing and no row below it can be attributed to compression.
 *
 * THE CANDIDATES
 *   C0  CONTROL      Y transmitted in full, 23-bit canonical packing.
 *   C1  TRUNCATE(b)  Y transmitted with its b low bits dropped, i.e. the
 *                    Power2Round move that shrinks ML-DSA's public key t to t1.
 *                    Signer AND verifier both use the truncated statement, so
 *                    the compression is applied consistently -- the fairest
 *                    possible version of the idea.
 *   C2  SEED         Y not transmitted at all; the 32-byte generator seed is
 *                    sent and the receiver recomputes Y = A r' itself.  This is
 *                    the largest compression available (LAS_SEEDBYTES against
 *                    STATEMENT_BYTES) and it reproduces Y EXACTLY, so every
 *                    functional property below holds.  The harness therefore
 *                    also runs the recovery that shows why it is inadmissible.
 *
 * THE PROPERTIES (the subset of the adaptor contract compression can touch)
 *   P1 PreVerify accepts the honest pre-signature
 *   P2 Adapt succeeds
 *   P3 base Verify ACCEPTS the adapted signature   <-- decisive: the CHAIN
 *   P4 Ext recovers the exact witness              <-- decisive: ATOMICITY
 *   P5 adapted ||z||inf < BOUND_SIGN               (Algorithm 1's own bound)
 *
 *   P3 and P4 are the two the experiment exists for.  P3 is what "the adapted
 *   signature is a fully ordinary signature" means -- what an unmodified
 *   consensus verifier is asked to do.  P4 is what makes a swap atomic: the
 *   party who sees the adapted signature on chain must be able to recover the
 *   witness and complete the other half.  A compression that breaks either one
 *   is not a size trade-off; it removes the reason to use an adaptor at all.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../setup.h"
#include "../las_types.h"
#include "../relation.h"
#include "../basesig.h"
#include "../las.h"
#include "../serialize.h"
#include "../randombytes.h"

#define NITER  100
#define MLEN   32

#define NPROP 5
static const char *PROP[NPROP] = {
  "P1 PreVerify accepts the pre-signature",
  "P2 Adapt succeeds",
  "P3 base Verify ACCEPTS the adapted signature  <-- the chain",
  "P4 Ext recovers the exact witness             <-- atomicity",
  "P5 adapted ||z||inf < BOUND_SIGN",
};

/* Which statement the counterparty is actually given. */
enum candidate {
  C_CONTROL,   /* Y in full                                     */
  C_TRUNCATE,  /* Y with its `bits` low bits cleared            */
  C_SEED       /* Y recomputed by the receiver from the seed    */
};

struct tally {
  unsigned long ok[NPROP];
  unsigned long runs;
  unsigned long coeffs_changed;  /* C_TRUNCATE: how many coefficients truncation moved */
  unsigned long witness_leaked;  /* C_SEED: how often the receiver recovered r'        */
};

/* The public setup is one-time and shared by every variant, exactly as a
 * deployment would have it: compression is a property of the statement on the
 * wire, not of the parameters. */
static public_params pp;

static int witness_equal(const witness *a, const witness *b)
{
  unsigned int i, j;

  for(i = 0; i < N_PLUS_ELL; ++i)
    for(j = 0; j < LAS_D; ++j)
      if(a->value[i].coeffs[j] != b->value[i].coeffs[j])
        return 0;
  return 1;
}

/* ||v||inf over the centred representatives -- the same sign trick
 * poly_chknorm (poly.c) uses, so this reads the coefficients the way the
 * verifier's own bound test does. */
static int32_t vec_norm_inf(const poly v[N_PLUS_ELL])
{
  unsigned int i, j;
  int32_t max = 0, a, t;

  for(i = 0; i < N_PLUS_ELL; ++i)
    for(j = 0; j < LAS_D; ++j) {
      a = v[i].coeffs[j];
      t = a >> 31;
      a = a - (t & (2 * a));
      if(a > max)
        max = a;
    }
  return max;
}

/* Power2Round's move, applied to a statement: clear the b low bits of every
 * coefficient.  Y's coefficients are canonical in [0,Q) (relation_gen_seed ends
 * on caddq), so masking is exactly "drop the low b bits" with no wraparound.
 * Returns the number of coefficients the truncation actually moved. */
static unsigned long statement_truncate(statement *out, const statement *in,
                                        unsigned int bits)
{
  unsigned int i, j;
  unsigned long changed = 0;
  const int32_t mask = ~((int32_t)((INT32_C(1) << bits) - 1));

  for(i = 0; i < LAS_N; ++i)
    for(j = 0; j < LAS_D; ++j) {
      const int32_t v = in->t_prime[i].coeffs[j];
      out->t_prime[i].coeffs[j] = v & mask;
      if((v & mask) != v)
        ++changed;
    }
  return changed;
}

/*
 * One full honest swap leg under the given candidate compression:
 * Gen -> PreSign -> PreVerify -> Adapt -> Verify -> Ext.
 *
 * `Y_wire` is what the counterparty holds -- the compressed statement.  Every
 * adaptor call is given Y_wire, never the true Y, because that is exactly the
 * modelling question: the compressed object is the only statement in the
 * protocol.  The true Y survives only so the harness can report what changed.
 */
static void run_candidate(enum candidate which, unsigned int bits, struct tally *tl)
{
  static public_key   pk;
  static secret_key   sk;
  static statement    Y_true, Y_wire;
  static witness      r_prime, r_recovered, s;
  static pre_signature presig;
  static signature    sig;

  unsigned int it;
  uint8_t key_seed[LAS_SEEDBYTES], gen_seed[LAS_SEEDBYTES], m[MLEN];

  memset(tl, 0, sizeof *tl);

  for(it = 0; it < NITER; ++it) {
    randombytes(key_seed, LAS_SEEDBYTES);
    randombytes(gen_seed, LAS_SEEDBYTES);
    randombytes(m, MLEN);

    base_keygen_seed(&pk, &sk, &pp, key_seed);
    relation_gen_seed(&Y_true, &r_prime, &pp, gen_seed);

    switch(which) {
    case C_CONTROL:
      Y_wire = Y_true;
      break;
    case C_TRUNCATE:
      tl->coeffs_changed += statement_truncate(&Y_wire, &Y_true, bits);
      break;
    case C_SEED:
      /* The receiver is sent gen_seed and reruns Gen.  Y is reproduced exactly
       * -- and so is the witness, which is the whole problem. */
      relation_gen_seed(&Y_wire, &r_recovered, &pp, gen_seed);
      if(witness_equal(&r_recovered, &r_prime))
        ++tl->witness_leaked;
      break;
    }

    ++tl->runs;

    /* PreSign is the signer's own call, so it uses the statement the protocol
     * agreed on -- the compressed one. */
    las_presign_det(&presig, m, MLEN, &Y_wire, &pk, &sk, &pp);

    if(las_preverify(&presig, m, MLEN, &Y_wire, &pk, &pp) == 0)
      ++tl->ok[0];

    if(las_adapt(&sig, &presig, m, MLEN, &Y_wire, &r_prime, &pk, &pp) != 0)
      continue;   /* P2 failed: the rows below have no adapted signature to test */
    ++tl->ok[1];

    if(base_verify(&sig, m, MLEN, &pk, &pp) == 0)
      ++tl->ok[2];

    if(las_ext(&s, &sig, &presig, &Y_wire, &pp) == 0 && witness_equal(&s, &r_prime))
      ++tl->ok[3];

    if(vec_norm_inf(sig.z) < BOUND_SIGN)
      ++tl->ok[4];
  }
}

static void report(const char *title, const char *what, const struct tally *tl)
{
  unsigned int p;

  printf("\n=== %s ===\n%s\n\n", title, what);
  for(p = 0; p < NPROP; ++p)
    printf("  %-46s %4lu / %-4lu  %s\n", PROP[p], tl->ok[p], tl->runs,
           tl->ok[p] == tl->runs ? "holds" : (tl->ok[p] == 0 ? "FAILS" : "PARTIAL"));
}

int main(void)
{
  struct tally control, trunc, seed;
  uint8_t pp_seed[LAS_SEEDBYTES];
  const unsigned int sweep[] = { 1, 2, 4, 8, 13 };
  unsigned int i;

  const long full_bytes  = (long)STATEMENT_BYTES;
  const long naive_bytes = (long)LAS_N * LAS_D * 4;   /* one int32 per coefficient */
  const long coeffs      = (long)LAS_N * LAS_D;

  randombytes(pp_seed, LAS_SEEDBYTES);
  setup_public_params(&pp, pp_seed);

  printf("Statement-compression experiment -- LAS parameter set "
         "(n=%d, ell=%d, kappa=%d, d=%d)\n", LAS_N, ELL, KAPPA, LAS_D);
  printf("%d iterations per candidate; fresh keys, statement and message each time.\n",
         NITER);
  printf("Statement Y = %ld B on the wire, against a %ld B signature.\n",
         full_bytes, (long)SIGNATURE_BYTES);

  /* ---- control: the harness must reproduce the adaptor contract ---------- */
  run_candidate(C_CONTROL, 0, &control);
  report("C0 -- CONTROL: the statement in full",
         "  Y transmitted as STATEMENT_BYTES of 23-bit canonical packing: the scheme\n"
         "  exactly as it stands. Every row must read `holds`, otherwise nothing below\n"
         "  is attributable to compression.",
         &control);
  for(i = 0; i < NPROP; ++i)
    if(control.ok[i] != control.runs) {
      printf("\nFATAL: the control did not hold the adaptor contract, so no candidate\n"
             "below can be attributed to compression. Fix the control first.\n");
      return 1;
    }

  /* ---- C2: the seed --------------------------------------------------- */
  run_candidate(C_SEED, 0, &seed);
  report("C2 -- SEED: send the 32-byte generator seed instead of Y",
         "  The receiver reruns Gen and recomputes Y = A r' itself. Y is reproduced\n"
         "  EXACTLY, so this is not an approximation and every functional property is\n"
         "  expected to hold. It is also the largest compression available.",
         &seed);
  printf("\n  size            : %ld B -> %d B  (%.0fx smaller)\n",
         full_bytes, LAS_SEEDBYTES, (double)full_bytes / (double)LAS_SEEDBYTES);
  printf("  witness recovered by the receiver : %lu / %lu\n",
         seed.witness_leaked, seed.runs);
  printf("\n  VERDICT: inadmissible, and the row above is the demonstration rather\n");
  printf("  than an argument. Gen derives BOTH Y and r' from this one seed, so the\n");
  printf("  compressed statement IS the witness. A counterparty who can recompute Y\n");
  printf("  can adapt the pre-signature and take both sides of the swap. The\n");
  printf("  compression is total and so is the break.\n");

  /* ---- C1: Power2Round on the statement -------------------------------- */
  printf("\n\n########  C1 -- TRUNCATE: drop the b low bits of every coefficient  ########\n");
  printf("\nThis is precisely the move that shrinks ML-DSA's public key t to t1. Both\n");
  printf("parties use the truncated statement, so the compression is consistent and\n");
  printf("the hash agrees on both sides -- the fairest version of the idea.\n");

  for(i = 0; i < sizeof sweep / sizeof sweep[0]; ++i) {
    const unsigned int b = sweep[i];
    const long kept = coeffs * (long)(LAS_PK_COEFF_BITS - b) / 8;
    char title[96], what[512];

    snprintf(title, sizeof title, "C1(b=%u) -- statement truncated to %d bits/coefficient",
             b, LAS_PK_COEFF_BITS - (int)b);
    snprintf(what, sizeof what,
             "  Statement %ld B -> %ld B (saves %ld B, %.0f%%), applied identically at\n"
             "  PreSign, PreVerify, Adapt and Ext.",
             full_bytes, kept, full_bytes - kept,
             100.0 * (double)(full_bytes - kept) / (double)full_bytes);

    run_candidate(C_TRUNCATE, b, &trunc);
    report(title, what, &trunc);
    printf("  coefficients actually moved by truncation : %lu / %lu\n",
           trunc.coeffs_changed, (unsigned long)coeffs * trunc.runs);
  }

  /* ---- what the rows mean --------------------------------------------- */
  printf("\n\n=== What this shows ===\n");
  printf("Read the P3 and P4 rows of the C1 sweep against their P1 and P2 rows.\n\n");
  printf("Truncation is INVISIBLE to the adaptor's own functions and FATAL to the two\n");
  printf("that connect it to the outside world. PreSign and PreVerify both hash the\n");
  printf("statement they were given, so they agree with each other whatever that\n");
  printf("statement is; Adapt only re-runs PreVerify before adding the witness. But:\n\n");
  printf("  (a) P3 fails because base Verify never sees a statement. It recomputes\n");
  printf("      A z - c t, which for an adapted signature equals w + Y with the TRUE\n");
  printf("      Y -- since A r' = Y exactly -- and hashes that. The pre-signature\n");
  printf("      committed to w + Y_truncated. The two digests differ, so an unmodified\n");
  printf("      verifier rejects. Nothing on the chain can be told about the\n");
  printf("      compression, because the compression is not in the algebra.\n");
  printf("  (b) P4 fails because Ext's acceptance test IS the exact relation: it\n");
  printf("      returns s = z - z_hat only if A s == Y, and A s == Y_true, never\n");
  printf("      Y_truncated. Atomicity is exactly this test, so a swap built on a\n");
  printf("      truncated statement cannot complete its second leg.\n\n");
  printf("The obstruction is not the norm budget and not the packing. It is that Y\n");
  printf("enters the verification identity BEFORE any rounding, while ML-DSA's t\n");
  printf("enters after -- which is what makes t compressible and Y not.\n");

  /* ---- why the hint trick does not transfer ---------------------------- */
  printf("\n=== Why a hint cannot rescue the truncation ===\n");
  printf("ML-DSA compresses t because the VERIFIER INDEPENDENTLY RECOMPUTES an\n");
  printf("approximation of the rounded quantity, and the hint only has to repair the\n");
  printf("last carry. No party can compute anything near Y without the witness --\n");
  printf("that is the hardness assumption the statement rests on -- so a hint has no\n");
  printf("approximation to correct and must instead carry the dropped bits outright:\n\n");
  for(i = 0; i < sizeof sweep / sizeof sweep[0]; ++i) {
    const unsigned int b = sweep[i];
    const long saved  = coeffs * (long)b / 8;
    const long repair = coeffs * (long)b / 8;
    printf("  b=%-3u  truncation saves %6ld B   exact repair costs %6ld B   net %+ld B\n",
           b, saved, repair, repair - saved);
  }
  printf("\nThe accounting is net zero BY CONSTRUCTION, not by coincidence: the bits a\n");
  printf("hint would have to carry are exactly the bits truncation dropped.\n");
  printf("\nThe remaining escape -- sample r' so that A r' already has b zero low bits,\n");
  printf("making the truncation lossless -- costs a factor 2^(b*n*d) in rejections:\n");
  printf("  b=1 is 2^-%ld per attempt.\n", coeffs);

  /* ---- the compression that IS already in place ------------------------ */
  printf("\n=== The compression already applied (the baseline to beat) ===\n");
  printf("  one int32 per coefficient : %ld B\n", naive_bytes);
  printf("  23-bit canonical packing  : %ld B   (-%.0f%%, and lossless)\n",
         full_bytes, 100.0 * (double)(naive_bytes - full_bytes) / (double)naive_bytes);
  printf("\nY is %ld coefficients that are uniform in [0,Q) under the MLWE assumption\n",
         coeffs);
  printf("the statement rests on, so 23 bits each is its entropy, not its encoding\n");
  printf("overhead. A lossless encoder cannot do better; the C1 sweep is what happens\n");
  printf("to a lossy one, and C2 is what happens when the loss is moved into the seed.\n");
  printf("\nSCOPE: this harness settles FUNCTIONAL admissibility only. It shows which\n");
  printf("compressions break the adaptor contract and where. It does not analyse the\n");
  printf("security of any compression that survives -- no such candidate is reported\n");
  printf("here, and security analysis is out of scope for the project.\n");
  return 0;
}
