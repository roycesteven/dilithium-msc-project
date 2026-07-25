//! LAS construction parameters at the D3 engineering set, mirrored from the
//! authoritative Rust port `rust/fips204-las/src/setup.rs` (which mirrors the C
//! `ref/setup.h`). These MUST stay in lockstep with that file; they are copied
//! here (not imported) only so this crate stays standalone and does not pull the
//! KAT-locked `fips204` package (and its MSRV 1.70) into a Winterfell build.
//!
//! Notation follows the paper (eprint 2020/845): `n` module rank, `ell` = ℓ,
//! `d` ring degree, `kappa` = κ challenge weight, `gamma` = γ = κ·d·(n+ℓ).

/// Module rank n (paper). setup.rs `N`.
pub const N: usize = 6;
/// ℓ (paper). setup.rs `ELL`.
pub const ELL: usize = 5;
/// n + ℓ. setup.rs `N_PLUS_ELL`.
pub const N_PLUS_ELL: usize = N + ELL; // 11
/// Ring degree d (paper). setup.rs `D`; reused Dilithium NTT degree.
pub const D: usize = 256;
/// Prime modulus q. Reused Dilithium NTT modulus (≈2^23), not the paper's ≈2^24
/// (documented supervisor-sanctioned starting point).
pub const Q: i64 = 8_380_417;
/// Challenge weight κ (paper), per parameter set; D3 value.
pub const KAPPA: i64 = 49;
/// γ = κ·d·(n+ℓ). setup.rs `GAMMA`.
pub const GAMMA: i64 = KAPPA * (D as i64) * (N_PLUS_ELL as i64); // 137_984

/// Base-signature verify acceptance bound on ‖z‖∞.
///
/// `basesig` rejects with `chknorm_vec(z, BOUND_SIGN)` where
/// `BOUND_SIGN = γ − κ + 1`; `chknorm` rejects when `|coeff| ≥ BOUND_SIGN`, so
/// `base_verify` accepts iff `‖z‖∞ ≤ γ − κ`. That accept bound is `B` here.
pub const B: i64 = GAMMA - KAPPA; // 137_935

/// Width of the centred window [−B, B] shifted to [0, 2B]: `2B`.
pub const TWO_B: i64 = 2 * B; // 275_870

/// Number of bits needed to hold any value in [0, 2B]. `2B = 275_870 < 2^19`,
/// so 19 bits is sufficient (and 18 is not: `2^18 = 262_144 < 2B`).
pub const RANGE_BITS: usize = 19;

/// Total number of scalar coefficients in the response `z` (the private witness
/// of the norm sub-relation): (n+ℓ)·d.
pub const Z_COEFFS: usize = N_PLUS_ELL * D; // 2816

const _: () = {
    assert!(GAMMA == 137_984);
    assert!(B == 137_935);
    assert!(TWO_B == 275_870);
    assert!((1usize << RANGE_BITS) > TWO_B as usize);
    assert!((1usize << (RANGE_BITS - 1)) <= TWO_B as usize);
};
