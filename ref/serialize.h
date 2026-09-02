#ifndef LAS_SERIALIZE_H
#define LAS_SERIALIZE_H

/*
 * Byte-level (de)serialisation for the LAS construction -- the ref/packing.{c,h}
 * twin, C mirror of rust/fips204-las/src/serialize.rs.
 *
 * The scheme works on in-memory `poly` structs, but any realistic deployment --
 * and certainly an on-chain verifier in the style of poqeth (eprint 2025/091) --
 * exchanges objects as BYTE STRINGS.  This module provides the canonical
 * wire/on-chain encoding and, crucially, a *validating* decoder: the verifier
 * must defensively reject malformed input (out-of-range coefficients, non-ternary
 * secrets) rather than trust the bytes.
 *
 * SIX SEMANTIC WRAPPER PAIRS over THREE SHARED PRIVATE ENCODERS (see setup.h for
 * the type ownership).  Pairs with identical wire layouts share one encoder, but
 * the TYPES stay non-interchangeable -- bytes decode into the semantic type the
 * caller names, never "a public_key used as a statement":
 *
 *   encode/decode_canonical_vec  <-  pack_/unpack_public_key  &  _statement
 *   encode/decode_ternary_vec    <-  pack_/unpack_secret_key  &  _witness
 *   encode/decode_chal_response  <-  pack_/unpack_signature   &  _pre_signature
 *
 * The layouts coinciding is itself a paper fact: a statement is pk-shaped (Gen
 * runs as KeyGen), and a pre-signature costs exactly as many bytes as a
 * signature (the "essentially as efficient" claim, byte level).
 *
 * Encoding (see serialize.c):
 *   public key / statement Y : n     polys, 23 bits/coeff (value in [0,Q), Q<2^23)
 *   secret key / witness     : n+ell polys,  2 bits/coeff (ternary {-1,0,1})
 *   signature / pre-signature: c_tilde (per-set challenge digest, raw) + response
 *                              (z or z_hat) packed with FIPS BitPack -- each coeff
 *                              (in [-(g-k), g-k]) as a LAS_Z_COEFF_BITS-bit field
 *                              b-w, LSB-first, byte-identical to the Rust
 *                              conversion::bit_pack.  The z width is selected from
 *                              the parameters (18 bits paper/D2, 19 for D3/D5).
 *
 * pack_witness therefore serialises only HONEST (ternary) witnesses; an extracted
 * cumulative witness (||.||inf > 1, relation R'_A) is deliberately outside this
 * wire form and is never serialised unvalidated.
 */

#include <stddef.h>
#include <stdint.h>
#include "setup.h"        /* LAS_* construction parameters -- the shared system layer */
#include "las_types.h"    /* the six protocol object types; this codec sits BETWEEN
                           * setup.h/las_types.h and the two scheme files, exactly like
                           * upstream packing.{c,h} sits between polyvec.h and sign.c
                           * (NOT basesig.h / las.h / relation.h) */
#include "params.h"       /* N, Q */

/* Bit widths of the packed fields (gate names -- never renamed). */
#define LAS_PK_COEFF_BITS  23                       /* ceil(log2 Q), Q=8380417 */
#define LAS_SK_COEFF_BITS  2                         /* ternary {0,1,2}        */

/* The response (z / z_hat) is FIPS BitPack-encoded: each coeff w becomes the
 * unsigned field b - w in [0, LAS_Z_MAX], LAS_Z_MAX = 2*(gamma-kappa).  Width
 * selected from the actual parameter set at compile time.  (The preprocessor
 * cannot evaluate GAMMA directly because of its (int32_t) cast, so use a
 * cast-free copy here.) */
#define LAS_GAMMA_PP  (KAPPA * LAS_D * N_PLUS_ELL)   /* == GAMMA, usable in #if */
#if   (2 * (LAS_GAMMA_PP - KAPPA)) < (1 << 18)
#define LAS_Z_COEFF_BITS 18                          /* paper (4,4,60), D2 (4,4,39) */
#elif (2 * (LAS_GAMMA_PP - KAPPA)) < (1 << 19)
#define LAS_Z_COEFF_BITS 19                          /* D3 (6,5,49), D5 (8,7,60)    */
#elif (2 * (LAS_GAMMA_PP - KAPPA)) < (1 << 20)
#define LAS_Z_COEFF_BITS 20
#else
#error "LAS z field needs more than 20 bits; extend the LAS_Z_COEFF_BITS table"
#endif

/* Offset used to encode the signed response as an unsigned field (gate names). */
#define LAS_Z_OFFSET       (GAMMA - KAPPA)           /* centre shift = gamma-kappa */
#define LAS_Z_MAX          (2 * (GAMMA - KAPPA))     /* max offset-encoded value   */

/* Serialised sizes in bytes (LAS_D=256 is divisible by 8, so all divide evenly).
 * Paper/D2 sets: pk 2944, sk 512, sig 4640; D3/D5 are larger (wider z + dims).
 * A statement is pk-shaped and a pre-signature is sig-shaped -- same wire size,
 * distinct semantic type. */
#define PUBLIC_KEY_BYTES    ((LAS_N * LAS_D * LAS_PK_COEFF_BITS) / 8)
#define STATEMENT_BYTES     PUBLIC_KEY_BYTES
#define SECRET_KEY_BYTES    ((N_PLUS_ELL * LAS_D * LAS_SK_COEFF_BITS) / 8)
#define WITNESS_BYTES       SECRET_KEY_BYTES
#define SIGNATURE_BYTES     (LAS_CTILDEBYTES + (N_PLUS_ELL * LAS_D * LAS_Z_COEFF_BITS) / 8)
#define PRE_SIGNATURE_BYTES SIGNATURE_BYTES

/* Wire-size anchor for EVERY parameter set this project builds -- the C twin of
 * the `expected_wire_sizes` table in rust/fips204-las/src/serialize.rs.  An
 * unrecognised set is a compile error, never an unchecked build.
 *
 *   (n, ell, kappa)  c_tilde  z bits    pk    sk    sig
 *   (4, 4, 39)            32      18  2944   512   4640   ML-DSA-44-aligned
 *   (6, 5, 49)            48      19  4416   704   6736   ML-DSA-65-aligned target
 *   (8, 7, 60)            64      19  5888   960   9184   ML-DSA-87-aligned
 *   (4, 4, 60)            32      18  2944   512   4640   historical paper repro
 *
 * The only structural change from the pre-FIPS-204-alignment build is the digest
 * width, so every signature/pre-signature grows by exactly LAS_CTILDEBYTES - 32. */
#if   (LAS_N == 4) && (ELL == 4) && (KAPPA == 39)
_Static_assert(LAS_CTILDEBYTES == 32 && LAS_Z_COEFF_BITS == 18 &&
               PUBLIC_KEY_BYTES == 2944 && SECRET_KEY_BYTES == 512 &&
               SIGNATURE_BYTES == 4640, "wire-size anchor: D2-aligned set");
#elif (LAS_N == 6) && (ELL == 5) && (KAPPA == 49)
_Static_assert(LAS_CTILDEBYTES == 48 && LAS_Z_COEFF_BITS == 19 &&
               PUBLIC_KEY_BYTES == 4416 && SECRET_KEY_BYTES == 704 &&
               SIGNATURE_BYTES == 6736, "wire-size anchor: D3-aligned target set");
#elif (LAS_N == 8) && (ELL == 7) && (KAPPA == 60)
_Static_assert(LAS_CTILDEBYTES == 64 && LAS_Z_COEFF_BITS == 19 &&
               PUBLIC_KEY_BYTES == 5888 && SECRET_KEY_BYTES == 960 &&
               SIGNATURE_BYTES == 9184, "wire-size anchor: D5-aligned set");
#elif (LAS_N == 4) && (ELL == 4) && (KAPPA == 60)
_Static_assert(LAS_CTILDEBYTES == 32 && LAS_Z_COEFF_BITS == 18 &&
               PUBLIC_KEY_BYTES == 2944 && SECRET_KEY_BYTES == 512 &&
               SIGNATURE_BYTES == 4640, "wire-size anchor: paper reproduction set");
#else
#error "no wire-size anchor defined for this (LAS_N, ELL, KAPPA) set"
#endif

/* ---- public key (Algorithm-1 object).  Pack canonicalises to [0,Q); unpack
 * REJECTS (returns -1) any coefficient >= Q. ---- */
void pack_public_key(uint8_t out[PUBLIC_KEY_BYTES], const public_key *pk);
int  unpack_public_key(public_key *pk, const uint8_t in[PUBLIC_KEY_BYTES]);

/* ---- statement Y = t' (relation object; SAME 23-bit canonical wire layout as a
 * public key -- Gen runs as KeyGen -- but a DISTINCT semantic type). ---- */
void pack_statement(uint8_t out[STATEMENT_BYTES], const statement *Y);
int  unpack_statement(statement *Y, const uint8_t in[STATEMENT_BYTES]);

/* ---- secret key (Algorithm-1 object).  Pack REJECTS a non-ternary input
 * (returns -1); unpack rejects the invalid 2-bit code 3. ---- */
int  pack_secret_key(uint8_t out[SECRET_KEY_BYTES], const secret_key *sk);
int  unpack_secret_key(secret_key *sk, const uint8_t in[SECRET_KEY_BYTES]);

/* ---- HONEST (ternary, relation R_A) witness.  Same ternary wire form as a
 * secret key; pack REJECTS non-ternary (an extracted R'_A witness, norm > 1, is
 * deliberately outside this form); unpack rejects code 3. ---- */
int  pack_witness(uint8_t out[WITNESS_BYTES], const witness *w);
int  unpack_witness(witness *w, const uint8_t in[WITNESS_BYTES]);

/* ---- signature (c_tilde, z) (Algorithm-1 object).  Pack REJECTS a z coefficient
 * outside [-(g-k), g-k]; unpack is permissive (c_tilde raw, z via FIPS BitUnpack),
 * so a tampered z is caught at Verify, not decode (upstream-faithful). ---- */
int  pack_signature(uint8_t out[SIGNATURE_BYTES], const signature *sig);
int  unpack_signature(signature *sig, const uint8_t in[SIGNATURE_BYTES]);

/* ---- pre-signature (c_tilde, z_hat) (Algorithm-2 object).  Same wire layout and
 * band as a signature (PreVerify enforces the tighter operational bound after
 * decode); DISTINCT semantic type. ---- */
int  pack_pre_signature(uint8_t out[PRE_SIGNATURE_BYTES], const pre_signature *presig);
int  unpack_pre_signature(pre_signature *presig, const uint8_t in[PRE_SIGNATURE_BYTES]);

/* (The end-to-end PACKED-API tier -- base_verify_packed and friends, the
 * functions that unpack -> run the scheme -> pack INSIDE the call, exactly as
 * upstream sign.c does with packing.h -- lives in the scheme files
 * basesig.c/las.c and is declared in basesig.h/las.h.  This file is the CODEC
 * ONLY, the ref/packing.{c,h} twin.) */

#endif
