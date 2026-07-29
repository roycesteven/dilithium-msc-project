//! LAS construction parameters at the D3 engineering set, mirrored from the
//! authoritative Rust port `rust/fips204-las/src/setup.rs` (which mirrors the C
//! `ref/setup.h`). Copied here (not imported) so this crate stays standalone and
//! does not pull the KAT-locked `fips204` package (and its MSRV 1.70) into a
//! Winterfell build. Keep in lockstep with setup.rs.
//!
//! Notation follows the paper (eprint 2020/845): `n` module rank, `ell` = l,
//! `d` ring degree, `kappa` = k challenge weight, `gamma` = k*d*(n+l).

/// Module rank n. setup.rs `N`.
pub const N: usize = 6;
/// l. setup.rs `ELL`.
pub const ELL: usize = 5;
/// n + l. setup.rs `N_PLUS_ELL`.
pub const N_PLUS_ELL: usize = N + ELL; // 11
/// Ring degree d. setup.rs `D`; reused Dilithium NTT degree.
pub const D: usize = 256;
/// Prime modulus q. Reused Dilithium NTT modulus (~2^23).
pub const Q: i64 = 8_380_417;
/// Challenge weight k (per parameter set); D3 value.
pub const KAPPA: i64 = 49;
/// PRG seed width (bytes). setup.rs `LAS_SEEDBYTES`. Distinct from the
/// challenge-digest width below -- they were once the same value.
pub const LAS_SEEDBYTES: usize = 32;
/// Challenge-digest `c_tilde` width (bytes). setup.rs / setup.h
/// `LAS_CTILDEBYTES`: the FIPS 204 §7.3 `lambda/4` width for the parameter set,
/// which is 48 for this ML-DSA-65-aligned D3 build (32 before that alignment).
pub const LAS_CTILDEBYTES: usize = 48;
/// gamma = k*d*(n+l). setup.rs `GAMMA`.
pub const GAMMA: i64 = KAPPA * (D as i64) * (N_PLUS_ELL as i64); // 137_984

/// Base-signature verify acceptance bound on `||z||inf`.
///
/// `basesig` rejects via `chknorm_vec(z, BOUND_SIGN)` with
/// `BOUND_SIGN = gamma - kappa + 1`; `chknorm` rejects when `|coeff| >= BOUND_SIGN`,
/// so `base_verify` accepts iff `||z||inf <= gamma - kappa`. That accept bound is `B`.
pub const B: i64 = GAMMA - KAPPA; // 137_935

/// The centred window `[-B, B]` shifted to `[0, 2B]`: `2B`.
pub const TWO_B: i64 = 2 * B; // 275_870

/// Bits needed to hold any value in `[0, 2B]`. `2B = 275_870 < 2^19`, so 19 bits
/// suffice (and 18 do not: `2^18 = 262_144 < 2B`).
pub const RANGE_BITS: usize = 19;

/// Total scalar coefficients in the response `z` (the norm sub-relation witness):
/// `(n+l)*d`.
pub const Z_COEFFS: usize = N_PLUS_ELL * D; // 2816

// Compile-time sanity: the arithmetic identities the AIR relies on.
const _: () = {
    assert!(GAMMA == 137_984);
    assert!(B == 137_935);
    assert!(TWO_B == 275_870);
    assert!((1usize << RANGE_BITS) > TWO_B as usize);
    assert!((1usize << (RANGE_BITS - 1)) <= TWO_B as usize);
    assert!(Z_COEFFS == 2816);
};
