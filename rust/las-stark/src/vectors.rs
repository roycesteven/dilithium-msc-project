//! Loaders for the golden verification vectors exported by the C reference
//! (`ref/test/export_verify_vector.c` -> `evm/test/vectors/*.bin`), plus small
//! `mod q` helpers. These are the SAME ground-truth files the Solidity verifier
//! (`evm/src/LASVerifier.sol`) validates against, so this crate and the on-chain
//! verifier share one source of truth.
//!
//! Wire format (from the exporter): every polynomial is `D` coefficients, each a
//! 4-byte little-endian `int32`. `t`/`A'` are canonical `[0,Q)` already;
//! `z`/`c`/`w'` are written by `write_poly_canonical` (centred, then `+Q` if
//! negative) so they too are non-negative in `[0,Q)`. We therefore read every
//! coefficient as a `u32` in `[0,Q)` and store it as `i64`.

use crate::params::{D, ELL, LAS_CTILDEBYTES, N, N_PLUS_ELL, Q};
use std::io;
use std::path::Path;

/// One ring element: `D` coefficients, canonical residues in `[0,Q)`.
pub type Poly = [i64; D];

#[inline]
fn zero_poly() -> Poly {
    [0i64; D]
}

/// Read `count` polynomials (`count * D` int32-LE coefficients) from `bytes`.
fn read_polys(bytes: &[u8], count: usize) -> io::Result<Vec<Poly>> {
    let need = count * D * 4;
    if bytes.len() < need {
        return Err(io::Error::new(
            io::ErrorKind::UnexpectedEof,
            format!("expected >= {need} bytes for {count} polys, got {}", bytes.len()),
        ));
    }
    let mut out = Vec::with_capacity(count);
    let mut off = 0usize;
    for _ in 0..count {
        let mut p = zero_poly();
        for c in p.iter_mut() {
            let v = u32::from_le_bytes([bytes[off], bytes[off + 1], bytes[off + 2], bytes[off + 3]]);
            *c = v as i64;
            off += 4;
        }
        out.push(p);
    }
    Ok(out)
}

fn read_file(dir: &Path, name: &str) -> io::Result<Vec<u8>> {
    std::fs::read(dir.join(name))
}

/// The complete public+witness verification vector, straight from the goldens.
///
/// Public inputs of the LAS on-chain verify relation: `a_prime`, `t`, `c`
/// (= `SampleInBall(c_tilde)`), `msg`. Private witness: `z` (the response). The
/// verifier's expected commitment `w_prime` is included so the native reference
/// oracle (`relation.rs`) can be checked against C ground truth before any STARK.
pub struct VerifyVector {
    /// A' in NORMAL domain, row-major `[m][j]` (n x ell), from `pp_normal.bin`.
    pub a_prime: Vec<Vec<Poly>>,
    /// Public key `t` (n polys), from `t.bin`.
    pub t: Vec<Poly>,
    /// Challenge polynomial `c = SampleInBall(c_tilde)` (1 poly), from `c.bin`.
    pub c: Poly,
    /// Response `z` (n+ell polys) -- the private witness, from `z.bin`.
    pub z: Vec<Poly>,
    /// Expected commitment `w' = z_top + A'*z_bot - c*t` (n polys), from `w_prime.bin`.
    pub w_prime: Vec<Poly>,
    /// Message M (32 bytes), from `msg.bin`.
    pub msg: Vec<u8>,
    /// Challenge digest `c_tilde` -- the first `LAS_CTILDEBYTES` bytes of the packed
    /// adapted signature `sig.bin` (`c_tilde || BitPack(z)`).
    pub c_tilde: [u8; LAS_CTILDEBYTES],
}

impl VerifyVector {
    /// Load every golden file from a directory (default `evm/test/vectors`).
    pub fn load(dir: impl AsRef<Path>) -> io::Result<Self> {
        let dir = dir.as_ref();

        // pp_normal.bin: n*ell polys, written `for m in 0..N { for j in 0..ELL }`.
        let flat = read_polys(&read_file(dir, "pp_normal.bin")?, N * ELL)?;
        let mut a_prime = Vec::with_capacity(N);
        for m in 0..N {
            a_prime.push(flat[m * ELL..(m + 1) * ELL].to_vec());
        }

        let t = read_polys(&read_file(dir, "t.bin")?, N)?;
        let c = read_polys(&read_file(dir, "c.bin")?, 1)?[0];
        let z = read_polys(&read_file(dir, "z.bin")?, N_PLUS_ELL)?;
        let w_prime = read_polys(&read_file(dir, "w_prime.bin")?, N)?;
        let msg = read_file(dir, "msg.bin")?;

        // c_tilde = first LAS_CTILDEBYTES bytes of the packed adapted signature.
        let sig = read_file(dir, "sig.bin")?;
        if sig.len() < LAS_CTILDEBYTES {
            return Err(io::Error::new(
                io::ErrorKind::UnexpectedEof,
                format!("sig.bin has {} bytes, need >= {LAS_CTILDEBYTES} for c_tilde", sig.len()),
            ));
        }
        let mut c_tilde = [0u8; LAS_CTILDEBYTES];
        c_tilde.copy_from_slice(&sig[..LAS_CTILDEBYTES]);

        Ok(Self { a_prime, t, c, z, w_prime, msg, c_tilde })
    }
}

/// Canonical `[0,Q)` -> centred `(-Q/2, Q/2]`.
#[inline]
pub fn centred(v: i64) -> i64 {
    let mut a = v % Q;
    if a < 0 {
        a += Q;
    }
    if a > Q / 2 {
        a -= Q;
    }
    a
}

/// `||p||_inf` in centred representation.
pub fn norm_inf(p: &Poly) -> i64 {
    p.iter().map(|&v| centred(v).abs()).max().unwrap_or(0)
}

/// `||v||_inf` over a vector of polynomials.
pub fn norm_inf_vec(v: &[Poly]) -> i64 {
    v.iter().map(norm_inf).max().unwrap_or(0)
}

/// Load the golden single-convolution vectors `(conv_a, conv_b, conv_out)` where
/// `conv_out = conv_a (X) conv_b` (negacyclic, mod q) -- the isolated arithmetic
/// golden the C exporter emits for validating one convolution.
pub fn load_conv(dir: impl AsRef<Path>) -> io::Result<(Poly, Poly, Poly)> {
    let dir = dir.as_ref();
    let a = read_polys(&read_file(dir, "conv_a.bin")?, 1)?[0];
    let b = read_polys(&read_file(dir, "conv_b.bin")?, 1)?[0];
    let out = read_polys(&read_file(dir, "conv_out.bin")?, 1)?[0];
    Ok((a, b, out))
}
