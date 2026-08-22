//! The hash-derived parts of the LAS base-verify relation, reproduced natively
//! and byte-exactly from the C reference (`ref/basesig.c b_poly_challenge` and
//! the `base_verify_internal` oracle call). These are the pieces Stage A.2 must
//! eventually fold into the AIR (as an in-AIR Keccak); until then this module is
//! the authoritative native spec, validated against the C golden vectors.
//!
//! Challenge digest (Fiat-Shamir):
//!
//! ```text
//! c_tilde = SHAKE256( pack(t) || pack(w') || M )
//! ```
//!
//! where `pack` is `b_polyvecn_pack_w`: each coefficient reduced to `[0,Q)` then
//! 4-byte little-endian, polynomials concatenated in order. Then
//! `c = SampleInBall(c_tilde)` places kappa nonzero +/-1 coefficients.

use crate::params::{D, KAPPA, LAS_CTILDEBYTES, Q};
use crate::vectors::Poly;
use sha3::digest::{ExtendableOutput, Update, XofReader};
use sha3::Shake256;

/// Pack polynomials as C `b_polyvecn_pack_w`: each coeff reduced to `[0,Q)`,
/// 4-byte little-endian, polys concatenated in order.
fn pack_polys(v: &[Poly]) -> Vec<u8> {
    let mut out = Vec::with_capacity(v.len() * D * 4);
    for p in v {
        for &c in p.iter() {
            let canon = (((c % Q) + Q) % Q) as u32;
            out.extend_from_slice(&canon.to_le_bytes());
        }
    }
    out
}

/// SampleInBall: expand `c_tilde` into the challenge polynomial `c` (kappa
/// nonzero +/-1 coefficients), canonical `[0,Q)`. Mirrors C `b_poly_challenge`
/// / the exporter's `export_challenge`. The SHAKE256 output is one stream, so
/// reading it incrementally here yields the same bytes the C block-squeeze does.
pub fn sample_in_ball(c_tilde: &[u8]) -> Poly {
    let mut h = Shake256::default();
    h.update(c_tilde);
    let mut xof = h.finalize_xof();

    let mut sbuf = [0u8; 8];
    xof.read(&mut sbuf);
    let mut signs = u64::from_le_bytes(sbuf);

    let mut c: Poly = [0i64; D];
    let kappa = KAPPA as usize;
    for i in (D - kappa)..D {
        // rejection-sample b in [0, i]
        let b = loop {
            let mut one = [0u8; 1];
            xof.read(&mut one);
            let bb = one[0] as usize;
            if bb <= i {
                break bb;
            }
        };
        c[i] = c[b];
        c[b] = 1 - 2 * ((signs & 1) as i64);
        signs >>= 1;
    }
    // canonicalise -1 -> Q-1 to match the golden c.bin representation
    for x in c.iter_mut() {
        if *x < 0 {
            *x += Q;
        }
    }
    c
}

/// Fiat-Shamir challenge digest `c_tilde = SHAKE256(pack(t) || pack(w') || M)`.
/// Mirrors the oracle call in C `base_verify_internal`.
pub fn challenge_digest(t: &[Poly], w_prime: &[Poly], msg: &[u8]) -> [u8; LAS_CTILDEBYTES] {
    let mut h = Shake256::default();
    h.update(&pack_polys(t));
    h.update(&pack_polys(w_prime));
    h.update(msg);
    let mut xof = h.finalize_xof();
    let mut out = [0u8; LAS_CTILDEBYTES];
    xof.read(&mut out);
    out
}
