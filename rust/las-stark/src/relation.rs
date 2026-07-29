//! Native (non-STARK) reference oracle for the LAS base-verify arithmetic
//! relation, over the golden vectors. This is the ground-truth trace generator
//! the STARK layer must agree with, and it is validated directly against the C
//! reference output (`w_prime.bin`), exactly as `evm/src/LASVerifier.sol` is.
//!
//! Relation (per output polynomial `m`, mirrors `ref/basesig.c base_verify` and
//! the exporter `ref/test/export_verify_vector.c`):
//!
//! ```text
//! w'_m = z_top[m] + sum_j A'[m][j] (X) z_bot[j] - c (X) t[m]   (mod q)
//! ```
//!
//! where `(X)` is negacyclic convolution in R_q = Z_q[X]/(X^d + 1),
//! `z_top = z[0..n]`, `z_bot = z[n..n+l]`. The full on-chain relation also
//! checks `||z||inf <= B`, `c = SampleInBall(c_tilde)`, and
//! `c_tilde = SHAKE256(pack(t) || pack(w') || M)`; those hash steps are folded
//! into the AIR in a later stage (see crate docs). This module fixes the
//! arithmetic core.

use crate::hashing::{challenge_digest, sample_in_ball};
use crate::params::{D, ELL, N, N_PLUS_ELL, Q};
use crate::vectors::{Poly, VerifyVector};

/// Schoolbook negacyclic convolution `c = a (X) b mod (X^D + 1, Q)`.
/// Inputs canonical `[0,Q)`, output canonical `[0,Q)`. Ground-truth twin of the
/// exporter's `negacyclic_conv` and of `nttInv(vecMulMod(nttFw(a), nttFw(b)))`.
pub fn negacyclic_conv(a: &Poly, b: &Poly) -> Poly {
    let qi = Q as i128;
    let mut acc = [0i128; D];
    for i in 0..D {
        let ai = a[i] as i128;
        for j in 0..D {
            let prod = (ai * (b[j] as i128)) % qi;
            let k = i + j;
            if k < D {
                acc[k] = (acc[k] + prod) % qi; // X^k, k < d
            } else {
                acc[k - D] = (acc[k - D] - prod) % qi; // X^d = -1
            }
        }
    }
    let mut out = [0i64; D];
    for k in 0..D {
        let mut v = (acc[k] % qi) as i64;
        if v < 0 {
            v += Q;
        }
        out[k] = v;
    }
    out
}

/// Recompute `w'` from the public inputs and the response `z`, canonical `[0,Q)`.
pub fn compute_w_prime(vv: &VerifyVector) -> Vec<Poly> {
    let z_top = &vv.z[0..N];
    let z_bot = &vv.z[N..N_PLUS_ELL];
    let qi = Q as i128;
    let mut out = Vec::with_capacity(N);
    for m in 0..N {
        let mut acc = [0i128; D];
        for k in 0..D {
            acc[k] = (z_top[m][k] as i128) % qi;
        }
        for j in 0..ELL {
            let prod = negacyclic_conv(&vv.a_prime[m][j], &z_bot[j]);
            for k in 0..D {
                acc[k] = (acc[k] + prod[k] as i128) % qi;
            }
        }
        let ct = negacyclic_conv(&vv.c, &vv.t[m]);
        let mut wm = [0i64; D];
        for k in 0..D {
            let mut v = ((acc[k] - ct[k] as i128) % qi) as i64;
            if v < 0 {
                v += Q;
            }
            wm[k] = v;
        }
        out.push(wm);
    }
    out
}

/// Assert the native oracle reproduces the C golden `w_prime.bin` exactly.
/// Returns `Ok(())` on a coefficient-perfect match, else the first mismatch.
pub fn check_relation(vv: &VerifyVector) -> Result<(), String> {
    let got = compute_w_prime(vv);
    for m in 0..N {
        for k in 0..D {
            let g = ((got[m][k] % Q) + Q) % Q;
            let w = ((vv.w_prime[m][k] % Q) + Q) % Q;
            if g != w {
                return Err(format!("w'[{m}][{k}] mismatch: oracle {g} vs golden {w}"));
            }
        }
    }
    Ok(())
}

/// Validate the FULL `base_verify` relation natively against the golden data --
/// the authoritative spec the eventual AIR must match byte-for-byte:
///   (2) `c = SampleInBall(c_tilde)` matches `c.bin`;
///   (3) `w' = z_top + A'*z_bot - c*t` matches `w_prime.bin` (via `check_relation`);
///   (4) `c_tilde = SHAKE256(pack(t) || pack(w') || M)` matches the golden `c_tilde`.
/// (The norm bound `||z||inf <= B`, constraint (1), is what the Stage-A STARK proves.)
pub fn check_full_relation(vv: &VerifyVector) -> Result<(), String> {
    // (3) arithmetic core
    check_relation(vv)?;

    // (2) SampleInBall: expand c_tilde and compare to golden c
    let c = sample_in_ball(&vv.c_tilde);
    for k in 0..D {
        let got = ((c[k] % Q) + Q) % Q;
        let want = ((vv.c[k] % Q) + Q) % Q;
        if got != want {
            return Err(format!("SampleInBall mismatch at coeff {k}: {got} vs golden {want}"));
        }
    }

    // (4) Fiat-Shamir challenge digest
    let ct = challenge_digest(&vv.t, &vv.w_prime, &vv.msg);
    if ct != vv.c_tilde {
        return Err(format!(
            "challenge digest mismatch: recomputed {:02x?}.. vs golden {:02x?}..",
            &ct[..4],
            &vv.c_tilde[..4]
        ));
    }
    Ok(())
}
