/* bench_las_consensus.c — what OP_CHECKLASSIGVERIFY costs a validating node.
 *
 * THE QUESTION THIS ANSWERS.  Adding a consensus rule buys something (a lattice-authorised
 * spend) and costs something.  The cost a *node* pays is script-validation time: every full
 * node re-verifies every signature in every block it accepts.  So the honest comparison is
 * the new predicate against the same signature checks Bitcoin performs — BIP340 Schnorr (a
 * Taproot key-path spend) and ECDSA (a P2WPKH spend) — measured in one process, on one
 * machine, against the same clock.
 *
 * ⚠ WHAT THE BASELINE IS, EXACTLY.  The curve figures come from a PINNED libsecp256k1-zkp
 * build — the vendored library this project already uses for its classical adaptor baseline.
 * That is Blockstream's FORK of libsecp256k1, not the exact copy Bitcoin Core vendors, so
 * these are not "Bitcoin Core's numbers": they are the same verification algorithms from a
 * closely related library, pinned and built here.  Do not describe them as Core's.  A
 * like-for-like node comparison would link the patched client's own libsecp256k1.
 *
 * ⚠ BOTH SIDES START FROM WIRE BYTES.  `LASConsensusVerify` takes a *packed* signature and
 * public key and decodes them itself, because that is what the script interpreter hands it
 * off the witness stack.  Timing that against a libsecp256k1 call that receives an
 * already-parsed `secp256k1_xonly_pubkey` would count the wire codec on the LAS side only —
 * the precise error this project's tier-matching rule exists to prevent, and one that would
 * inflate the reported cost of the new rule.  So each secp256k1 loop parses its serialized
 * pubkey (and, for ECDSA, its DER signature) INSIDE the timed block, exactly as a node does
 * per input.  The two sides are then measured at the same abstraction level.
 *
 * ⚠ THE REJECT PATH IS A VALID SIGNATURE AGAINST THE WRONG MESSAGE, not a corrupted one.
 * Flipping a byte can trip an early structural or norm check and short-circuit, which
 * measures how fast the verifier gives up rather than what a rejection costs.  A well-formed
 * signature checked against a different 32-byte message digest forces the whole computation
 * — decode, norm bound, w' = Az - ct, hash — and fails only at the final challenge
 * comparison.  That is the true worst case, and the adversarially relevant one: an attacker
 * who wants to make nodes work does not need signatures that verify, only signatures that
 * fail late.  The message is described as a plain 32-byte digest throughout because the
 * three paths do not share a sighash algorithm: BIP341 covers the Taproot spend the LAS
 * opcode lives in, while a P2WPKH ECDSA input is BIP143.  Only the width is common.
 *
 * ⚠ TWO SEPARATE SECURITY CAVEATS.  They are different claims and must not be merged.
 *
 *   (a) THE CONSENSUS MODIFICATION IS UNANALYSED.  Whether defining an OP_SUCCESS opcode as
 *       a lattice verification is *safe* as a consensus rule — soundness, DoS surface,
 *       upgrade path, standardness interaction — is not evaluated anywhere in this project.
 *       Nothing measured here bears on it.  A timing figure is not a safety argument.
 *
 *   (b) THE TWO SCHEMES ARE NOT AT A MATCHED SECURITY LEVEL.  The node compiles the shim at
 *       Simplified Dilithium-III (n=6, ell=5, kappa=49), because that is this project's
 *       headline set and what the patched client actually runs — so it is the honest thing
 *       to measure.  But secp256k1 offers roughly 128-bit *classical* security, whose
 *       engineering match is Dilithium-II, not Dilithium-III.  The ratios below therefore
 *       compare Bitcoin's curve against a deliberately STRONGER lattice configuration, and
 *       so OVERSTATE what the rule costs relative to a level-matched comparison.  Stating
 *       the direction of that bias is the point; neither this nor the D2 pairing is a formal
 *       security-equivalence claim.  (The report's classical-baseline table is run at
 *       Simplified Dilithium-II for exactly this reason.)
 *
 * WHAT IS AND IS NOT MEASURED.  This times the *verification predicate* the interpreter
 * calls, not a whole node: no block download, no UTXO lookup, no script parsing, no
 * signature-cache effects.  It is the marginal cryptographic cost of the rule, the quantity
 * that scales with the number of inputs; everything omitted is shared with the unmodified
 * client.
 *
 * MEASUREMENT DISCIPLINE (this project's standing gates).
 *   1. An untimed warm-up precedes the first measurement, so no first-call page fault or
 *      lazy initialisation lands in a measured mean.  `LASConsensusVerify` expands its
 *      public parameters behind a pthread_once on first call — warming up is what keeps that
 *      one-off out of the per-verification figure.
 *   2. The paths are PAIRED AND INTERLEAVED within each repetition, so clock drift or a
 *      frequency change cannot bias one path relative to another.  Comparing figures timed
 *      in separate blocks is how this project once inverted the sign of an overhead.
 *   3. Every timed block ends in a SUCCESS-PATH ASSERTION: an accept loop must have accepted
 *      every time and a reject loop must have rejected every time.  A verifier that silently
 *      failed would otherwise be timed doing nothing and look wonderfully fast.
 *   4. Inputs are deterministic (base_keygen_seed / base_sign_det, fixed secp keys), so a
 *      run is reproducible and the objects timed are the ones the node actually validates.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include "las_consensus.h"
#include "las_consensus_params.h"

#include "basesig.h"
#include "serialize.h"
#include "setup.h"

#include <secp256k1.h>
#include <secp256k1_extrakeys.h>
#include <secp256k1_schnorrsig.h>

#ifndef BENCH_REPS
#define BENCH_REPS 10
#endif
#ifndef BENCH_ITERS
#define BENCH_ITERS 200
#endif

static double now_us(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e6 + (double)ts.tv_nsec / 1e3;
}

static void stats(const double *v, int n, double *mean, double *sd)
{
  double s = 0.0;
  for(int i = 0; i < n; i++) s += v[i];
  *mean = s / n;
  double acc = 0.0;
  for(int i = 0; i < n; i++){ double d = v[i] - *mean; acc += d * d; }
  *sd = (n > 1) ? sqrt(acc / (n - 1)) : 0.0;
}

static int fail(const char *why)
{
  fprintf(stderr, "FAIL: %s\n", why);
  return 1;
}

int main(void)
{
  const int reps = BENCH_REPS, iters = BENCH_ITERS;

  /* ---------------- LAS: the objects the patched node validates ---------------- */
  public_params pp;
  public_key    pk;
  secret_key    sk;
  signature     sig;
  uint8_t sigb[SIGNATURE_BYTES], pkb[PUBLIC_KEY_BYTES];
  uint8_t msg[32], msg_wrong[32];

  setup_public_params(&pp, LAS_CONSENSUS_SEED);

  uint8_t kseed[LAS_SEEDBYTES];
  for(size_t i = 0; i < sizeof kseed;     i++) kseed[i]     = (uint8_t)(0x11 * (i + 1));
  for(size_t i = 0; i < sizeof msg;       i++) msg[i]       = (uint8_t)(0xA0 + i);
  for(size_t i = 0; i < sizeof msg_wrong; i++) msg_wrong[i] = (uint8_t)(0x5C + i);

  if(base_keygen_seed(&pk, &sk, &pp, kseed))             return fail("base_keygen_seed");
  if(base_sign_det(&sig, msg, sizeof msg, &pk, &sk, &pp)) return fail("base_sign_det");
  if(pack_signature(sigb, &sig))                          return fail("pack_signature");
  pack_public_key(pkb, &pk);

  if(!LASConsensusVerify(sigb, sizeof sigb, pkb, sizeof pkb, msg, sizeof msg))
    return fail("the valid LAS signature did not verify — refusing to benchmark");
  /* Same signature, different sighash: must fail, and must fail LATE (see header). */
  if(LASConsensusVerify(sigb, sizeof sigb, pkb, sizeof pkb, msg_wrong, sizeof msg_wrong))
    return fail("a LAS signature verified against the wrong message — refusing to benchmark");

  /* ---------------- secp256k1: what Bitcoin runs today ---------------- */
  secp256k1_context *ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN |
                                                    SECP256K1_CONTEXT_VERIFY);
  if(!ctx) return fail("secp256k1_context_create");

  uint8_t seckey[32];
  for(int i = 0; i < 32; i++) seckey[i] = (uint8_t)(i + 1);

  secp256k1_keypair keypair;
  if(!secp256k1_keypair_create(ctx, &keypair, seckey)) return fail("keypair_create");
  secp256k1_xonly_pubkey xonly;
  if(!secp256k1_keypair_xonly_pub(ctx, &xonly, NULL, &keypair)) return fail("xonly_pub");

  /* Serialized forms — the bytes that actually sit in a witness / scriptPubKey. */
  uint8_t xonly_ser[32];
  if(!secp256k1_xonly_pubkey_serialize(ctx, xonly_ser, &xonly))
    return fail("xonly_pubkey_serialize");

  uint8_t schnorr_sig[64];
  if(!secp256k1_schnorrsig_sign32(ctx, schnorr_sig, msg, &keypair, NULL))
    return fail("schnorrsig_sign32");

  secp256k1_pubkey ecpub;
  if(!secp256k1_ec_pubkey_create(ctx, &ecpub, seckey)) return fail("ec_pubkey_create");
  uint8_t ecpub_ser[33];
  size_t  ecpub_len = sizeof ecpub_ser;
  if(!secp256k1_ec_pubkey_serialize(ctx, ecpub_ser, &ecpub_len, &ecpub,
                                    SECP256K1_EC_COMPRESSED))
    return fail("ec_pubkey_serialize");

  secp256k1_ecdsa_signature ecsig;
  if(!secp256k1_ecdsa_sign(ctx, &ecsig, msg, seckey, NULL, NULL)) return fail("ecdsa_sign");
  uint8_t ecsig_der[72];
  size_t  ecsig_der_len = sizeof ecsig_der;
  if(!secp256k1_ecdsa_signature_serialize_der(ctx, ecsig_der, &ecsig_der_len, &ecsig))
    return fail("ecdsa_signature_serialize_der");

  /* Verify once from bytes, the way the timed loops will, before trusting any of it. */
  {
    secp256k1_xonly_pubkey xp;
    if(!secp256k1_xonly_pubkey_parse(ctx, &xp, xonly_ser)) return fail("xonly parse");
    if(!secp256k1_schnorrsig_verify(ctx, schnorr_sig, msg, 32, &xp))
      return fail("the Schnorr signature did not verify from bytes");
    if(secp256k1_schnorrsig_verify(ctx, schnorr_sig, msg_wrong, 32, &xp))
      return fail("Schnorr verified against the wrong message");

    secp256k1_pubkey ep;
    secp256k1_ecdsa_signature es;
    if(!secp256k1_ec_pubkey_parse(ctx, &ep, ecpub_ser, ecpub_len)) return fail("ec parse");
    if(!secp256k1_ecdsa_signature_parse_der(ctx, &es, ecsig_der, ecsig_der_len))
      return fail("der parse");
    if(!secp256k1_ecdsa_verify(ctx, &es, msg, &ep))
      return fail("the ECDSA signature did not verify from bytes");
    if(secp256k1_ecdsa_verify(ctx, &es, msg_wrong, &ep))
      return fail("ECDSA verified against the wrong message");
  }

  /* ---------------- warm-up: untimed, every path ---------------- */
  volatile long warm = 0;
  for(int i = 0; i < 8; i++){
    secp256k1_xonly_pubkey xp; secp256k1_pubkey ep; secp256k1_ecdsa_signature es;
    warm += LASConsensusVerify(sigb, sizeof sigb, pkb, sizeof pkb, msg, sizeof msg);
    warm += LASConsensusVerify(sigb, sizeof sigb, pkb, sizeof pkb, msg_wrong, 32);
    warm += secp256k1_xonly_pubkey_parse(ctx, &xp, xonly_ser);
    warm += secp256k1_schnorrsig_verify(ctx, schnorr_sig, msg, 32, &xp);
    warm += secp256k1_schnorrsig_verify(ctx, schnorr_sig, msg_wrong, 32, &xp);
    warm += secp256k1_ec_pubkey_parse(ctx, &ep, ecpub_ser, ecpub_len);
    warm += secp256k1_ecdsa_signature_parse_der(ctx, &es, ecsig_der, ecsig_der_len);
    warm += secp256k1_ecdsa_verify(ctx, &es, msg, &ep);
    warm += secp256k1_ecdsa_verify(ctx, &es, msg_wrong, &ep);
  }
  (void)warm;

  double las_ok[BENCH_REPS], las_no[BENCH_REPS];
  double sch_ok[BENCH_REPS], sch_no[BENCH_REPS];
  double ecd_ok[BENCH_REPS], ecd_no[BENCH_REPS];

  for(int r = 0; r < reps; r++){
    double t0; long acc;

    /* --- LAS accept: packed signature + packed key + sighash --- */
    acc = 0; t0 = now_us();
    for(int i = 0; i < iters; i++)
      acc += LASConsensusVerify(sigb, sizeof sigb, pkb, sizeof pkb, msg, sizeof msg);
    las_ok[r] = (now_us() - t0) / iters;
    if(acc != iters) return fail("LAS accept loop did not accept every time");

    /* --- Schnorr accept: parse the 32-byte x-only key, then verify --- */
    acc = 0; t0 = now_us();
    for(int i = 0; i < iters; i++){
      secp256k1_xonly_pubkey xp;
      if(secp256k1_xonly_pubkey_parse(ctx, &xp, xonly_ser))
        acc += secp256k1_schnorrsig_verify(ctx, schnorr_sig, msg, 32, &xp);
    }
    sch_ok[r] = (now_us() - t0) / iters;
    if(acc != iters) return fail("Schnorr accept loop did not accept every time");

    /* --- ECDSA accept: parse the 33-byte key and the DER signature, then verify --- */
    acc = 0; t0 = now_us();
    for(int i = 0; i < iters; i++){
      secp256k1_pubkey ep; secp256k1_ecdsa_signature es;
      if(secp256k1_ec_pubkey_parse(ctx, &ep, ecpub_ser, ecpub_len) &&
         secp256k1_ecdsa_signature_parse_der(ctx, &es, ecsig_der, ecsig_der_len))
        acc += secp256k1_ecdsa_verify(ctx, &es, msg, &ep);
    }
    ecd_ok[r] = (now_us() - t0) / iters;
    if(acc != iters) return fail("ECDSA accept loop did not accept every time");

    /* --- LAS reject: the SAME valid signature against a different sighash --- */
    acc = 0; t0 = now_us();
    for(int i = 0; i < iters; i++)
      acc += LASConsensusVerify(sigb, sizeof sigb, pkb, sizeof pkb, msg_wrong, 32);
    las_no[r] = (now_us() - t0) / iters;
    if(acc != 0) return fail("LAS reject loop accepted something");

    /* --- Schnorr reject: same valid signature, different sighash --- */
    acc = 0; t0 = now_us();
    for(int i = 0; i < iters; i++){
      secp256k1_xonly_pubkey xp;
      if(secp256k1_xonly_pubkey_parse(ctx, &xp, xonly_ser))
        acc += secp256k1_schnorrsig_verify(ctx, schnorr_sig, msg_wrong, 32, &xp);
    }
    sch_no[r] = (now_us() - t0) / iters;
    if(acc != 0) return fail("Schnorr reject loop accepted something");

    /* --- ECDSA reject: same valid signature, different sighash --- */
    acc = 0; t0 = now_us();
    for(int i = 0; i < iters; i++){
      secp256k1_pubkey ep; secp256k1_ecdsa_signature es;
      if(secp256k1_ec_pubkey_parse(ctx, &ep, ecpub_ser, ecpub_len) &&
         secp256k1_ecdsa_signature_parse_der(ctx, &es, ecsig_der, ecsig_der_len))
        acc += secp256k1_ecdsa_verify(ctx, &es, msg_wrong, &ep);
    }
    ecd_no[r] = (now_us() - t0) / iters;
    if(acc != 0) return fail("ECDSA reject loop accepted something");
  }

  double m_las_ok, s_las_ok, m_las_no, s_las_no;
  double m_sch_ok, s_sch_ok, m_sch_no, s_sch_no;
  double m_ecd_ok, s_ecd_ok, m_ecd_no, s_ecd_no;
  stats(las_ok, reps, &m_las_ok, &s_las_ok);
  stats(las_no, reps, &m_las_no, &s_las_no);
  stats(sch_ok, reps, &m_sch_ok, &s_sch_ok);
  stats(sch_no, reps, &m_sch_no, &s_sch_no);
  stats(ecd_ok, reps, &m_ecd_ok, &s_ecd_ok);
  stats(ecd_no, reps, &m_ecd_no, &s_ecd_no);

  printf("OP_CHECKLASSIGVERIFY: what the new consensus rule costs a validating node\n");
  printf("=======================================================================\n");
  printf("parameter set   : Simplified Dilithium-III (n=6, ell=5, kappa=49) -- what the\n");
  printf("                  patched node compiles and therefore what it really costs\n");
  printf("LAS signature   : %zu B     LAS public key : %zu B\n",
         LASConsensusSigBytes(), LASConsensusPkBytes());
  printf("message         : a 32-byte message digest (the paths do not share a sighash\n");
  printf("                  algorithm: BIP341 for the Taproot spend the LAS opcode lives\n");
  printf("                  in, BIP143 for a P2WPKH ECDSA input -- only the width is common)\n");
  printf("curve baseline  : pinned libsecp256k1-zkp (a FORK of libsecp256k1, not the copy\n");
  printf("                  Bitcoin Core vendors -- same algorithms, not Core's numbers)\n");
  printf("boundary        : every path starts from SERIALIZED bytes and parses inside the\n");
  printf("                  timed call, as the script interpreter does per input\n");
  printf("reject path     : a VALID signature against a different 32-byte digest (fails late)\n");
  printf("protocol        : %d repetitions x %d iterations, paired and interleaved\n",
         reps, iters);
  printf("\n");
  printf("per verification (microseconds, mean +- sample SD over %d repetitions)\n", reps);
  printf("  %-34s %10.2f +- %6.2f\n", "OP_CHECKLASSIGVERIFY  (accept)", m_las_ok, s_las_ok);
  printf("  %-34s %10.2f +- %6.2f\n", "OP_CHECKLASSIGVERIFY  (reject)", m_las_no, s_las_no);
  printf("  %-34s %10.2f +- %6.2f\n", "BIP340 Schnorr        (accept)", m_sch_ok, s_sch_ok);
  printf("  %-34s %10.2f +- %6.2f\n", "BIP340 Schnorr        (reject)", m_sch_no, s_sch_no);
  printf("  %-34s %10.2f +- %6.2f\n", "ECDSA                 (accept)", m_ecd_ok, s_ecd_ok);
  printf("  %-34s %10.2f +- %6.2f\n", "ECDSA                 (reject)", m_ecd_no, s_ecd_no);
  printf("\n");
  printf("what the rule costs, per input\n");
  printf("  vs Taproot key path (Schnorr) : %.2fx\n", m_las_ok / m_sch_ok);
  printf("  vs P2WPKH (ECDSA)             : %.2fx\n", m_las_ok / m_ecd_ok);
  printf("  reject vs accept, LAS         : %.2fx\n", m_las_no / m_las_ok);
  printf("  reject vs accept, Schnorr     : %.2fx\n", m_sch_no / m_sch_ok);
  printf("  reject vs accept, ECDSA       : %.2fx\n", m_ecd_no / m_ecd_ok);
  printf("\n");
  printf("WHAT THIS SHOWS: the marginal cost of the new rule is one lattice verification\n");
  printf("per input in place of one elliptic-curve verification, both measured from wire\n");
  printf("bytes. The ratio above is that price. It is a per-input cryptographic cost only --\n");
  printf("no block download, UTXO lookup, script parsing or signature cache is included, all\n");
  printf("of which the stock client pays too.\n");
  printf("\n");
  printf("THREE CAVEATS -- do not merge them:\n");
  printf("  (0) THE CURVE BASELINE IS NOT BITCOIN CORE'S BUILD. It is a pinned\n");
  printf("      libsecp256k1-zkp, a fork of libsecp256k1 -- the same verification\n");
  printf("      algorithms, but not the library the patched client links. A like-for-like\n");
  printf("      node comparison would use that client's own libsecp256k1.\n");
  printf("  (a) CONSENSUS-MODIFICATION SECURITY IS NOT ANALYSED. Whether defining an\n");
  printf("      OP_SUCCESS opcode as a lattice verification is safe as a consensus rule\n");
  printf("      (soundness, DoS surface, upgrade path) is evaluated nowhere in this\n");
  printf("      project. A timing figure is not a safety argument.\n");
  printf("  (b) THE SCHEMES ARE NOT AT A MATCHED SECURITY LEVEL. secp256k1 offers roughly\n");
  printf("      128-bit CLASSICAL security, an engineering match to Dilithium-II, while the\n");
  printf("      node runs Simplified Dilithium-III. These ratios therefore measure Bitcoin's\n");
  printf("      curve against a deliberately STRONGER lattice setting and so OVERSTATE the\n");
  printf("      rule's cost relative to a level-matched pairing. Neither pairing is a formal\n");
  printf("      security-equivalence claim.\n");

  /* Machine-readable, for the evidence directory and any downstream generator. */
  printf("\nSUMMARY reps=%d iters=%d las_param_set=D3 secp_level=classical128 "
         "las_accept_us=%.4f las_accept_sd=%.4f "
         "las_reject_us=%.4f las_reject_sd=%.4f "
         "schnorr_accept_us=%.4f schnorr_accept_sd=%.4f "
         "schnorr_reject_us=%.4f schnorr_reject_sd=%.4f "
         "ecdsa_accept_us=%.4f ecdsa_accept_sd=%.4f "
         "ecdsa_reject_us=%.4f ecdsa_reject_sd=%.4f "
         "ratio_schnorr=%.4f ratio_ecdsa=%.4f "
         "las_sig_bytes=%zu las_pk_bytes=%zu\n",
         reps, iters,
         m_las_ok, s_las_ok, m_las_no, s_las_no,
         m_sch_ok, s_sch_ok, m_sch_no, s_sch_no,
         m_ecd_ok, s_ecd_ok, m_ecd_no, s_ecd_no,
         m_las_ok / m_sch_ok, m_las_ok / m_ecd_ok,
         LASConsensusSigBytes(), LASConsensusPkBytes());

  secp256k1_context_destroy(ctx);
  return 0;
}
