#ifndef LAS_SERIALIZE_H
#define LAS_SERIALIZE_H

/*
 * Byte-level (de)serialisation for LAS objects.
 *
 * The scheme (las.c) works on in-memory `poly` structs, but any realistic
 * deployment - and certainly an on-chain verifier in the style of poqeth
 * (eprint 2025/091) - exchanges objects as BYTE STRINGS.  This module provides
 * the canonical wire/on-chain encoding and, crucially, a *validating* decoder:
 * the verifier must defensively reject malformed input (out-of-range
 * coefficients, non-ternary secrets) rather than trust the bytes.
 *
 * Encoding (LSB-first bit packing; see serialize.c):
 *   pk / statement Y : n   polys, 23 bits/coeff  (value in [0,Q), Q < 2^23)
 *   sk / witness     : n+l polys,  2 bits/coeff  (ternary {-1,0,1})
 *   signature (c,z)  : c as 2-bit ternary (256 coeffs) + z as 18-bit signed
 *                      (centred value in [-(g-k), g-k], offset-encoded)
 *
 * These sizes are the realistic on-wire footprint of THIS (simplified) scheme;
 * see docs/LAS.md Section 8 for the in-memory vs packed vs paper-estimate split.
 */

#include <stddef.h>
#include <stdint.h>
#include "las.h"          /* las_pk / las_sk / las_sig / las_pp, LAS_* params */
#include "params.h"       /* N, Q */

/* Bit widths of the packed fields. */
#define LAS_PK_COEFF_BITS  23                       /* ceil(log2 Q), Q=8380417 */
#define LAS_SK_COEFF_BITS  2                         /* ternary {0,1,2}        */
#define LAS_C_COEFF_BITS   2                         /* challenge is ternary    */
#define LAS_Z_COEFF_BITS   18                        /* 2*(g-k)+1 = 245641<2^18 */

/* Offset used to encode the signed response z as an unsigned 18-bit field. */
#define LAS_Z_OFFSET       (LAS_GAMMA - LAS_KAPPA)   /* 122820 */
#define LAS_Z_MAX          (2 * (LAS_GAMMA - LAS_KAPPA)) /* 245640, fits 18 bits */

/* Serialised sizes in bytes (all divide evenly). */
#define LAS_PK_BYTES  ((LAS_N * N * LAS_PK_COEFF_BITS) / 8)                        /* 2944 */
#define LAS_SK_BYTES  ((LAS_M * N * LAS_SK_COEFF_BITS) / 8)                        /*  512 */
#define LAS_SIG_BYTES (((N * LAS_C_COEFF_BITS) + (LAS_M * N * LAS_Z_COEFF_BITS)) / 8) /* 4672 */

/* Public key / statement.  Pack canonicalises to [0,Q); unpack REJECTS (returns
 * -1) any coefficient >= Q. */
void las_pack_pk(uint8_t out[LAS_PK_BYTES], const las_pk *pk);
int  las_unpack_pk(las_pk *pk, const uint8_t in[LAS_PK_BYTES]);

/* Secret key / ternary witness.  Pack REJECTS a non-ternary input (returns -1,
 * e.g. an AMHL cumulative witness with ||.||inf > 1 is not an sk); unpack
 * rejects the invalid 2-bit code 3. */
int  las_pack_sk(uint8_t out[LAS_SK_BYTES], const las_sk *sk);
int  las_unpack_sk(las_sk *sk, const uint8_t in[LAS_SK_BYTES]);

/* (Pre-)signature (c,z).  Pack REJECTS a non-ternary c or a z coefficient outside
 * [-(g-k), g-k]; unpack rejects the same on decode. */
int  las_pack_sig(uint8_t out[LAS_SIG_BYTES], const las_sig *sig);
int  las_unpack_sig(las_sig *sig, const uint8_t in[LAS_SIG_BYTES]);

/* On-chain-style verifier entry point: decode pk and signature FROM BYTES (with
 * validation) and run ordinary Verify.  Returns 0 iff the bytes decode to valid
 * objects AND the signature verifies.  This is the interface a real integration
 * (Solidity/precompile/circuit) would expose. */
int  las_verify_packed(const uint8_t pk_b[LAS_PK_BYTES],
                       const uint8_t sig_b[LAS_SIG_BYTES],
                       const uint8_t *m, size_t mlen, const las_pp *pp);

#endif
