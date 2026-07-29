//! Byte-level serialisation for the LAS construction — Rust port of
//! `ref/serialize.c`.  The key/statement/witness fields use an LSB-first bit
//! packer with VALIDATING decoders (malformed bytes -> `None`); the response z
//! reuses the UPSTREAM FIPS BitPack/BitUnpack (`conversion::bit_pack/unpack`),
//! so the signature is the canonical cross-language `c_tilde || BitPack(z)`.
//!
//! Encoding (Simplified Dilithium-III set, n=6 ell=5 kappa=49):
//!   public key / statement : n polys,  23 bits/coeff (canonical [0,Q)) -> 4416 B
//!   secret key / witness   : n+ell polys, 2 bits/coeff (ternary)       ->  704 B
//!   signature / pre-sig    : 32-byte c_tilde digest + BitPack(z) 19-bit -> 6720 B
//!
//! SEMANTIC WRAPPERS over SHARED PRIVATE ENCODERS: the six public object
//! types (see `setup.rs` for ownership) get one typed pack/unpack pair EACH,
//! but pairs with identical wire layouts share one private encoder:
//!
//!   encode_canonical_vec  <- pack_public_key  / pack_statement
//!   encode_ternary_vec    <- pack_secret_key  / pack_witness
//!   encode_chal_response  <- pack_signature   / pack_pre_signature
//!
//! The layouts being identical is itself a paper fact (a statement IS
//! pk-shaped because Gen runs as KeyGen; a pre-signature costs exactly as
//! many bytes as a signature — the "essentially as efficient" claim at the
//! byte level) — but the TYPES stay non-interchangeable: bytes decode into
//! the semantic type the caller names, never "a PublicKey used as a
//! Statement".  Validation rules match the C codec: canonical pk/statement
//! coefficients >= Q rejected; non-ternary 2-bit sk/witness code 3 rejected;
//! the response z is decoded by the upstream FIPS BitUnpack, whose native range
//! is permissive, so a tampered z is caught at Verify, not decode (upstream-
//! faithful; PreVerify still enforces the tighter operational bound on z_hat).
//! `pack_witness` therefore serialises only HONEST (ternary)
//! witnesses — an AMHL cumulative witness (norm > 1) is deliberately outside
//! this wire form, exactly like the C `las_pack_sk` behaviour it mirrors.
//!
//! (The packed ordinary verifier lives with Algorithm 1 —
//! `basesig::verify_packed` — not here: this file is the CODEC ONLY, the
//! ref/packing.{c,h} twin.)

#![allow(warnings)]
#![allow(clippy::all, clippy::pedantic, clippy::nursery)]
// Named allows: crate root denies these BY NAME, and named lints outrank the
// `warnings` group regardless of scope depth.
#![allow(
    absolute_paths_not_starting_with_crate,
    dead_code,
    elided_lifetimes_in_paths,
    explicit_outlives_requirements,
    let_underscore_drop,
    macro_use_extern_crate,
    meta_variable_misuse,
    missing_abi,
    missing_docs,
    non_ascii_idents,
    single_use_lifetimes,
    trivial_casts,
    trivial_numeric_casts,
    unreachable_pub,
    unused_extern_crates,
    unused_import_braces,
    unused_lifetimes,
    unused_macro_rules,
    unused_qualifications,
    unused_results,
    variant_size_differences
)]

use crate::conversion::{bit_pack, bit_unpack};
use crate::helpers::{center_mod, full_reduce32};
// Shared layers only, like C serialize.h #include "setup.h" + "las_types.h"
// (NOT las.h / basesig.h / relation.h): the codec sits below every scheme layer
// and sees the six object types at their physical home, las_types.
use crate::las_types::{PreSignature, PublicKey, SecretKey, Signature, Statement, Witness};
use crate::setup::{D, ELL, GAMMA, KAPPA, LAS_CTILDEBYTES, N, N_PLUS_ELL};
use crate::types::{R, R0};
use crate::Q;

/// Bit widths of the packed fields (mirrors ref/serialize.h).
pub const LAS_PK_COEFF_BITS: usize = 23;
/// Ternary secret: 2 bits/coeff.
pub const LAS_SK_COEFF_BITS: usize = 2;

/// Offset used to encode the signed response (z or z_hat) as an unsigned field.
pub const LAS_Z_OFFSET: i32 = GAMMA - KAPPA;
/// Max offset-encoded response value.
pub const LAS_Z_MAX: i32 = 2 * (GAMMA - KAPPA);
/// Response field width, selected from the actual parameter set (C #if ladder).
pub const LAS_Z_COEFF_BITS: usize = if LAS_Z_MAX < (1 << 18) {
    18
} else if LAS_Z_MAX < (1 << 19) {
    19
} else {
    20
};

/// Packed public-key bytes.
pub const PUBLIC_KEY_BYTES: usize = (N * D * LAS_PK_COEFF_BITS) / 8;
/// Packed statement bytes — a statement is pk-SHAPED (Gen runs as KeyGen),
/// so the wire size coincides; the semantic type does not.
pub const STATEMENT_BYTES: usize = PUBLIC_KEY_BYTES;
/// Packed secret-key bytes.
pub const SECRET_KEY_BYTES: usize = (N_PLUS_ELL * D * LAS_SK_COEFF_BITS) / 8;
/// Packed (honest, ternary) witness bytes — same ternary wire form as a
/// secret key; the semantic type does not coincide.
pub const WITNESS_BYTES: usize = SECRET_KEY_BYTES;
/// Packed signature bytes: the raw challenge digest c_tilde (LAS_CTILDEBYTES,
/// FIPS 204 `lambda/4` for the aligned set — 48 B here) followed by the
/// offset-packed response z (the paper's eq. 7 layout, |c_tilde| + |z|).
pub const SIGNATURE_BYTES: usize = LAS_CTILDEBYTES + (N_PLUS_ELL * D * LAS_Z_COEFF_BITS) / 8;
/// Packed pre-signature bytes — exactly a signature's size (the paper's
/// "essentially as efficient" claim at the byte level).
pub const PRE_SIGNATURE_BYTES: usize = SIGNATURE_BYTES;

/// Expected wire sizes `(c_tilde, z bits, pk, sk, sig)` for every parameter set
/// this project builds, as an anchor against the C twin. An unrecognised set is
/// a compile error, never an unchecked build.
///
/// Derivations: `pk = n*d*23/8`, `sk = (n+ell)*d*2/8`,
/// `sig = |c_tilde| + (n+ell)*d*z_bits/8`, with `z_bits` fixed by
/// `2*(gamma-kappa)` and `|c_tilde|` by the FIPS 204 `lambda/4` alignment
/// (`setup::LAS_CTILDEBYTES`).
const fn expected_wire_sizes() -> (usize, usize, usize, usize, usize) {
    match (N, ELL, KAPPA) {
        //           c_tilde, z bits,  pk,   sk,  sig
        (4, 4, 39) => (32, 18, 2944, 512, 4640), // ML-DSA-44-aligned
        (6, 5, 49) => (48, 19, 4416, 704, 6736), // ML-DSA-65-aligned target
        (8, 7, 60) => (64, 19, 5888, 960, 9184), // ML-DSA-87-aligned
        (4, 4, 60) => (32, 18, 2944, 512, 4640), // historical paper reproduction
        _ => panic!("unrecognised (N, ELL, KAPPA) set: no wire-size anchor defined"),
    }
}

const _: () = {
    let (ct, zbits, pk, sk, sig) = expected_wire_sizes();
    assert!(LAS_CTILDEBYTES == ct);
    assert!(LAS_Z_COEFF_BITS == zbits);
    assert!(PUBLIC_KEY_BYTES == pk);
    assert!(SECRET_KEY_BYTES == sk);
    assert!(SIGNATURE_BYTES == sig);
    // The only structural change from the pre-FIPS-204-alignment build is the
    // digest: every signature/pre-signature grows by exactly |c_tilde| - 32.
    assert!(SIGNATURE_BYTES == ct + (N_PLUS_ELL * D * zbits) / 8);
};

/// LSB-first bit writer over a pre-zeroed buffer (mirrors bw_put).
fn bw_put(buf: &mut [u8], bitpos: &mut usize, val: u32, bits: usize) {
    for i in 0..bits {
        if (val >> i) & 1 == 1 {
            buf[*bitpos >> 3] |= 1u8 << (*bitpos & 7);
        }
        *bitpos += 1;
    }
}

/// LSB-first bit reader (mirrors br_get).
fn br_get(buf: &[u8], bitpos: &mut usize, bits: usize) -> u32 {
    let mut v = 0u32;
    for i in 0..bits {
        if (buf[*bitpos >> 3] >> (*bitpos & 7)) & 1 == 1 {
            v |= 1u32 << i;
        }
        *bitpos += 1;
    }
    v
}

/// Centred representative in (-Q/2, Q/2] (identical to serialize.c centred()).
fn centred(a: i32) -> i32 {
    center_mod(a)
}

/* ================= private shared encoders (one per wire layout) ========= */

/// 23-bit canonical n-vector (public key t / statement t'); canonicalises
/// to [0,Q) on encode.
fn encode_canonical_vec(t: &[R; N]) -> [u8; PUBLIC_KEY_BYTES] {
    let mut out = [0u8; PUBLIC_KEY_BYTES];
    let mut bp = 0usize;
    for i in 0..N {
        for k in 0..D {
            let v = full_reduce32(t[i].0[k]) as u32; // canonical [0,Q)
            bw_put(&mut out, &mut bp, v, LAS_PK_COEFF_BITS);
        }
    }
    out
}

/// Validating inverse of `encode_canonical_vec`; None on any coeff >= Q.
fn decode_canonical_vec(input: &[u8; PUBLIC_KEY_BYTES]) -> Option<[R; N]> {
    let mut bp = 0usize;
    let mut t: [R; N] = [R0; N];
    for i in 0..N {
        for k in 0..D {
            let v = br_get(input, &mut bp, LAS_PK_COEFF_BITS);
            if v >= Q as u32 {
                return None; // defensive: reject >= Q
            }
            t[i].0[k] = v as i32;
        }
    }
    Some(t)
}

/// 2-bit ternary (n+ell)-vector (secret key r / honest witness); None if any
/// coefficient is non-ternary.
fn encode_ternary_vec(s: &[R; N_PLUS_ELL]) -> Option<[u8; SECRET_KEY_BYTES]> {
    let mut out = [0u8; SECRET_KEY_BYTES];
    let mut bp = 0usize;
    for i in 0..N_PLUS_ELL {
        for k in 0..D {
            let c = centred(s[i].0[k]);
            if !(-1..=1).contains(&c) {
                return None; // must be ternary
            }
            bw_put(&mut out, &mut bp, (c + 1) as u32, LAS_SK_COEFF_BITS);
        }
    }
    Some(out)
}

/// Validating inverse of `encode_ternary_vec`; None on the invalid code 3.
fn decode_ternary_vec(input: &[u8; SECRET_KEY_BYTES]) -> Option<[R; N_PLUS_ELL]> {
    let mut bp = 0usize;
    let mut s: [R; N_PLUS_ELL] = [R0; N_PLUS_ELL];
    for i in 0..N_PLUS_ELL {
        for k in 0..D {
            let v = br_get(input, &mut bp, LAS_SK_COEFF_BITS);
            if v > 2 {
                return None; // code 3 is invalid
            }
            s[i].0[k] = v as i32 - 1;
        }
    }
    Some(s)
}

/// `c_tilde || BitPack(z)`: the raw 32-byte challenge digest followed by the
/// response z packed per-polynomial with the UPSTREAM FIPS BitPack (Alg. 17,
/// `conversion::bit_pack`) at the LAS band `a = b = gamma-kappa` (LAS_Z_OFFSET).
/// This is the canonical cross-language encoding — the C mirror packs
/// byte-identically.  `None` only if a response coefficient is out of the band
/// (bit_pack's own precondition); the 32-byte digest half never fails.
fn encode_chal_response(
    c_tilde: &[u8; LAS_CTILDEBYTES],
    z: &[R; N_PLUS_ELL],
) -> Option<[u8; SIGNATURE_BYTES]> {
    let mut out = [0u8; SIGNATURE_BYTES];
    out[..LAS_CTILDEBYTES].copy_from_slice(c_tilde);
    let poly_bytes = 32 * LAS_Z_COEFF_BITS; // = 32*bitlen(a+b) bytes per poly
    for i in 0..N_PLUS_ELL {
        // canonical centred representative; reject out-of-band so bit_pack's
        // precondition (|coeff| <= LAS_Z_OFFSET) holds.
        let mut zc = R0;
        for k in 0..D {
            let zz = centred(z[i].0[k]);
            if zz < -LAS_Z_OFFSET || zz > LAS_Z_OFFSET {
                return None; // out of band
            }
            zc.0[k] = zz;
        }
        let start = LAS_CTILDEBYTES + i * poly_bytes;
        bit_pack(&zc, LAS_Z_OFFSET, LAS_Z_OFFSET, &mut out[start..start + poly_bytes]);
    }
    Some(out)
}

/// Inverse of `encode_chal_response`.  The 32-byte digest is copied out raw (any
/// bytes are a valid c_tilde); z is decoded per-polynomial with the UPSTREAM
/// FIPS BitUnpack (Alg. 19, `conversion::bit_unpack`).  Its native range check
/// is the ONLY z validation here (upstream-faithful): a tampered z that survives
/// decode is caught at Verify (the recomputed-digest byte-compare).
fn decode_chal_response(
    input: &[u8; SIGNATURE_BYTES],
) -> Option<([u8; LAS_CTILDEBYTES], [R; N_PLUS_ELL])> {
    let mut c_tilde = [0u8; LAS_CTILDEBYTES];
    c_tilde.copy_from_slice(&input[..LAS_CTILDEBYTES]);
    let poly_bytes = 32 * LAS_Z_COEFF_BITS;
    let mut z: [R; N_PLUS_ELL] = [R0; N_PLUS_ELL];
    for i in 0..N_PLUS_ELL {
        let start = LAS_CTILDEBYTES + i * poly_bytes;
        z[i] = bit_unpack(&input[start..start + poly_bytes], LAS_Z_OFFSET, LAS_Z_OFFSET).ok()?;
    }
    Some((c_tilde, z))
}

/* ==================== public typed wrappers (six pairs) ================== */

/// Pack a public key t (Algorithm-1 object; canonicalises to [0,Q)).
pub fn pack_public_key(pk: &PublicKey) -> [u8; PUBLIC_KEY_BYTES] {
    encode_canonical_vec(&pk.t)
}

/// Unpack a public key; None on any coefficient >= Q (defensive).
pub fn unpack_public_key(input: &[u8; PUBLIC_KEY_BYTES]) -> Option<PublicKey> {
    Some(PublicKey {
        t: decode_canonical_vec(input)?,
    })
}

/// Pack a statement Y = t' (relation object; same wire layout as a public
/// key — Gen runs as KeyGen — but the semantic types stay distinct).
pub fn pack_statement(statement: &Statement) -> [u8; STATEMENT_BYTES] {
    encode_canonical_vec(statement.as_t_prime())
}

/// Unpack a statement; None on any coefficient >= Q (defensive).
pub fn unpack_statement(input: &[u8; STATEMENT_BYTES]) -> Option<Statement> {
    Some(Statement(decode_canonical_vec(input)?))
}

/// Pack a secret key r (Algorithm-1 object); None if non-ternary.
pub fn pack_secret_key(sk: &SecretKey) -> Option<[u8; SECRET_KEY_BYTES]> {
    encode_ternary_vec(&sk.r)
}

/// Unpack a secret key; None on the invalid 2-bit code 3.
pub fn unpack_secret_key(input: &[u8; SECRET_KEY_BYTES]) -> Option<SecretKey> {
    Some(SecretKey {
        r: decode_ternary_vec(input)?,
    })
}

/// Pack an HONEST (ternary, relation R_A) witness; None if non-ternary —
/// an extracted AMHL-style cumulative witness (norm > 1, relation R'_A) is
/// deliberately outside this wire form and is never serialised unvalidated.
pub fn pack_witness(witness: &Witness) -> Option<[u8; WITNESS_BYTES]> {
    encode_ternary_vec(witness.as_relation_vector())
}

/// Unpack a (ternary) witness; None on the invalid 2-bit code 3.
pub fn unpack_witness(input: &[u8; WITNESS_BYTES]) -> Option<Witness> {
    Some(Witness::from_relation_vector(decode_ternary_vec(input)?))
}

/// Pack a signature sigma = (c_tilde, z); None on out-of-band z (the 32-byte
/// challenge digest never fails).
pub fn pack_signature(sigma: &Signature) -> Option<[u8; SIGNATURE_BYTES]> {
    encode_chal_response(&sigma.c_tilde, &sigma.z)
}

/// Unpack a signature; None on out-of-band z.
pub fn unpack_signature(input: &[u8; SIGNATURE_BYTES]) -> Option<Signature> {
    let (c_tilde, z) = decode_chal_response(input)?;
    Some(Signature { c_tilde, z })
}

/// Pack a pre-signature sigma_hat = (c_tilde, z_hat); same wire layout and band
/// as a signature (PreVerify enforces the tighter operational bound after
/// decode, exactly as the C build does).
pub fn pack_pre_signature(sigma_hat: &PreSignature) -> Option<[u8; PRE_SIGNATURE_BYTES]> {
    encode_chal_response(&sigma_hat.c_tilde, &sigma_hat.z_hat)
}

/// Unpack a pre-signature; None on out-of-band z_hat.
pub fn unpack_pre_signature(input: &[u8; PRE_SIGNATURE_BYTES]) -> Option<PreSignature> {
    let (c_tilde, z_hat) = decode_chal_response(input)?;
    Some(PreSignature { c_tilde, z_hat })
}
