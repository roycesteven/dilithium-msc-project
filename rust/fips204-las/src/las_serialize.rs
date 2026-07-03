//! Byte-level serialisation for LAS objects — Rust port of `ref/serialize.c`
//! (packing side only; the validating decoders live in the C tree and are not
//! needed by the KAT).  LSB-first bit packing over a pre-zeroed buffer.
//!
//! Encoding (Simplified Dilithium-III set, n=6 ell=5 kappa=49):
//!   pk / statement Y : n polys,  23 bits/coeff (canonical [0,Q))      -> 4416 B
//!   sk / witness     : n+l polys, 2 bits/coeff (ternary {-1,0,1})     ->  704 B
//!   signature (c,z)  : c 2-bit ternary + z 19-bit offset-encoded      -> 6752 B

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

use crate::helpers::{center_mod, full_reduce32};
use crate::las::{las_verify, LasPk, LasPp, LasSig, LasSk, LAS_GAMMA, LAS_KAPPA, LAS_M, LAS_N};
use crate::types::{R, R0};
use crate::Q;
use core::array::from_fn;

const N: usize = 256;

/// Bit widths of the packed fields (mirrors ref/serialize.h).
pub const LAS_PK_COEFF_BITS: usize = 23;
/// Ternary secret: 2 bits/coeff.
pub const LAS_SK_COEFF_BITS: usize = 2;
/// Ternary challenge: 2 bits/coeff.
pub const LAS_C_COEFF_BITS: usize = 2;

/// Offset used to encode the signed response z as an unsigned field.
pub const LAS_Z_OFFSET: i32 = LAS_GAMMA - LAS_KAPPA;
/// Max offset-encoded z value.
pub const LAS_Z_MAX: i32 = 2 * (LAS_GAMMA - LAS_KAPPA);
/// z field width, selected from the actual parameter set (C #if ladder).
pub const LAS_Z_COEFF_BITS: usize = if LAS_Z_MAX < (1 << 18) {
    18
} else if LAS_Z_MAX < (1 << 19) {
    19
} else {
    20
};

/// Packed public-key bytes.
pub const LAS_PK_BYTES: usize = (LAS_N * N * LAS_PK_COEFF_BITS) / 8;
/// Packed secret-key bytes.
pub const LAS_SK_BYTES: usize = (LAS_M * N * LAS_SK_COEFF_BITS) / 8;
/// Packed signature bytes.
pub const LAS_SIG_BYTES: usize = ((N * LAS_C_COEFF_BITS) + (LAS_M * N * LAS_Z_COEFF_BITS)) / 8;

// Anchor: these must equal the C test_kat3 build (D3 set) sizes.
const _: () = assert!(LAS_Z_COEFF_BITS == 19);
const _: () = assert!(LAS_PK_BYTES == 4416 && LAS_SK_BYTES == 704 && LAS_SIG_BYTES == 6752);

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

/// Pack a public key / statement (canonicalises to [0,Q)).
pub fn las_pack_pk(pk: &LasPk) -> [u8; LAS_PK_BYTES] {
    let mut out = [0u8; LAS_PK_BYTES];
    let mut bp = 0usize;
    for i in 0..LAS_N {
        for k in 0..N {
            let v = full_reduce32(pk.t[i].0[k]) as u32; // canonical [0,Q)
            bw_put(&mut out, &mut bp, v, LAS_PK_COEFF_BITS);
        }
    }
    out
}

/// Pack a secret key / ternary witness; None if any coefficient is non-ternary.
pub fn las_pack_sk(sk: &LasSk) -> Option<[u8; LAS_SK_BYTES]> {
    let mut out = [0u8; LAS_SK_BYTES];
    let mut bp = 0usize;
    for i in 0..LAS_M {
        for k in 0..N {
            let c = centred(sk.s[i].0[k]);
            if !(-1..=1).contains(&c) {
                return None; // must be ternary
            }
            bw_put(&mut out, &mut bp, (c + 1) as u32, LAS_SK_COEFF_BITS);
        }
    }
    Some(out)
}

/// Pack a (pre-)signature; None on non-ternary c or out-of-band z.
pub fn las_pack_sig(sig: &LasSig) -> Option<[u8; LAS_SIG_BYTES]> {
    let mut out = [0u8; LAS_SIG_BYTES];
    let mut bp = 0usize;

    for k in 0..N {
        let c = centred(sig.c.0[k]);
        if !(-1..=1).contains(&c) {
            return None;
        }
        bw_put(&mut out, &mut bp, (c + 1) as u32, LAS_C_COEFF_BITS);
    }
    for i in 0..LAS_M {
        for k in 0..N {
            let z = centred(sig.z[i].0[k]);
            if z < -LAS_Z_OFFSET || z > LAS_Z_OFFSET {
                return None; // out of band
            }
            bw_put(&mut out, &mut bp, (z + LAS_Z_OFFSET) as u32, LAS_Z_COEFF_BITS);
        }
    }
    Some(out)
}

/* ==================== validating decoders (mirror serialize.c) ==================== */

/// Unpack a public key / statement; None on any coefficient >= Q (defensive).
pub fn las_unpack_pk(input: &[u8; LAS_PK_BYTES]) -> Option<LasPk> {
    let mut bp = 0usize;
    let mut t: [R; LAS_N] = [R0; LAS_N];
    for i in 0..LAS_N {
        for k in 0..N {
            let v = br_get(input, &mut bp, LAS_PK_COEFF_BITS);
            if v >= Q as u32 {
                return None; // defensive: reject >= Q
            }
            t[i].0[k] = v as i32;
        }
    }
    Some(LasPk { t })
}

/// Unpack a secret key / ternary witness; None on the invalid 2-bit code 3.
pub fn las_unpack_sk(input: &[u8; LAS_SK_BYTES]) -> Option<LasSk> {
    let mut bp = 0usize;
    let mut s: [R; LAS_M] = [R0; LAS_M];
    for i in 0..LAS_M {
        for k in 0..N {
            let v = br_get(input, &mut bp, LAS_SK_COEFF_BITS);
            if v > 2 {
                return None; // code 3 is invalid
            }
            s[i].0[k] = v as i32 - 1;
        }
    }
    Some(LasSk { s })
}

/// Unpack a (pre-)signature; None on non-ternary c code or out-of-band z.
pub fn las_unpack_sig(input: &[u8; LAS_SIG_BYTES]) -> Option<LasSig> {
    let mut bp = 0usize;
    let mut c = R0;
    for k in 0..N {
        let v = br_get(input, &mut bp, LAS_C_COEFF_BITS);
        if v > 2 {
            return None;
        }
        c.0[k] = v as i32 - 1;
    }
    let mut z: [R; LAS_M] = [R0; LAS_M];
    for i in 0..LAS_M {
        for k in 0..N {
            let v = br_get(input, &mut bp, LAS_Z_COEFF_BITS);
            if v > LAS_Z_MAX as u32 {
                return None; // out of the encoded band
            }
            z[i].0[k] = v as i32 - LAS_Z_OFFSET;
        }
    }
    Some(LasSig { c, z })
}

/// On-chain-style verifier entry point (mirrors C `las_verify_packed`): decode
/// pk and signature FROM BYTES (with validation) and run ordinary Verify.
/// Returns true iff the bytes decode to valid objects AND the signature verifies.
pub fn las_verify_packed(
    pk_b: &[u8; LAS_PK_BYTES],
    sig_b: &[u8; LAS_SIG_BYTES],
    m: &[u8],
    pp: &LasPp,
) -> bool {
    let pk = match las_unpack_pk(pk_b) {
        Some(pk) => pk,
        None => return false, // malformed pk
    };
    let sig = match las_unpack_sig(sig_b) {
        Some(sig) => sig,
        None => return false, // malformed sig
    };
    las_verify(&sig, m, &pk, pp) // ordinary Verify
}
