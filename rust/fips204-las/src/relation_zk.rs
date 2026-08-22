//! `relation_zk` — the proof of knowledge **pi** for the hard relation `R_A`
//! (eprint 2020/845, Section 4.1 / Fig. 1): u1 proves knowledge of a witness
//! `r'` with `A r' = t'` and `||r'||inf <= 1` WITHOUT revealing `r'`.  Rust
//! twin of `ref/relation_zk.{c,h}`.
//!
//! Fig. 1 makes pi load-bearing for fairness: u2 pre-signs only after
//! verifying pi, and the Section-4.1 M-SIS argument ("the extracted `s`
//! equals `r'`, hence `||s||inf <= 1`, hence pre-signature adaptability")
//! holds only because pi proves the EXACT ternary bound.
//!
//! Realisation (identical to the C side, not merely name-mirrored): FFI into
//! the SAME C bridge `ref/relation_zk_lazer.c` over the vendored LaZer
//! library, with the SAME generated parameter set `ref/relation_zk_params.h`
//! (knowledge error <= 2^-127 under M-SIS, zero-knowledge under M-LWE, proof
//! ~31 KiB, off-chain only).  LaZer proves per-partition BINARY coefficients
//! or exact l2 bounds, so the ternary statement is encoded by BINARY
//! DECOMPOSITION `r' = r_plus - r_minus`:
//!
//!     [A | -A | 0] * (r_plus || r_minus || e) = t',
//!     r_plus, r_minus BINARY (proven),  l2(e) <= 16 (dummy, honest e = 0).
//!
//! This module is the protocol-facing half: it builds the flat i64
//! coefficient buffers of the bridge seam from the Rust semantic types.  The
//! normal-domain `A'` is recovered from the NTT-domain `pp.a_prime` by
//! pushing the constant-1 polynomial through the SAME arithmetic pipeline
//! `relation::amul` applies to `r'` (mont product, partial reduce, inverse
//! NTT), so the exported matrix is bit-identical to the matrix every other
//! layer multiplies by — and byte-identical to the C export (KAT-locked
//! setup => same `A'`, same pipeline => same buffers => interchangeable
//! proofs across the two ports).
//!
//! Opt-in: `--features relation-zk` (needs the one-time LaZer build, see the
//! repo README).  The parameter set is FIXED to the D3 engineering set
//! (n=6, ell=5, d=256, q=8380417) — compile-time asserted below.

#![allow(warnings)]
#![allow(clippy::all, clippy::pedantic, clippy::nursery)]
// FFI into the C bridge is this module's entire purpose (the crate is
// otherwise unsafe-free; the root denies `unsafe_code` BY NAME, overridden
// here by name for exactly the two extern calls).
#![allow(unsafe_code)]
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

use core::ffi::c_int;

use crate::helpers::{mont_reduce, partial_reduce32, to_mont};
use crate::las_types::{Statement, Witness};
use crate::ntt::{inv_ntt, ntt};
use crate::setup::{PublicParams, D, ELL, N, N_PLUS_ELL};
use crate::types::{R0, T0};
use crate::Q;

/// Rows of the pi statement matrix `[A | -A | 0]` (= paper n).
pub const PI_ROWS: usize = N;
/// Columns: `2(n+ell)` decomposition halves + 1 dummy l2-bounded column the
/// LaZer codegen requires (all-zero matrix column, honest witness e = 0).
pub const PI_COLS: usize = 2 * N_PLUS_ELL + 1;
/// Ring degree of the statement (= paper d).
pub const PI_DEG: usize = D;
/// Upper bound on the wire size of pi (measured ~30.7 KB; length varies
/// slightly per proof).  Twin of the C `PI_PROOF_MAX_BYTES`.
pub const PI_PROOF_MAX_BYTES: usize = 65536;

// The committed parameter set ref/relation_zk_params.h is generated for the
// D3 engineering set only; refuse to build a mismatched statement silently.
const _: () = assert!(
    N == 6 && ELL == 5 && D == 256,
    "relation_zk: parameter set is generated for the D3 set (n=6, ell=5, d=256); \
     regenerate scripts/las_pi_params.py + ref/relation_zk_params.h for this set"
);

const A_LEN: usize = PI_ROWS * PI_COLS * PI_DEG;
const T_LEN: usize = PI_ROWS * PI_DEG;
const W_LEN: usize = PI_COLS * PI_DEG;

// The C bridge (ref/relation_zk_lazer.c, compiled+linked by build.rs): the
// single TU of the whole project that talks to LaZer.
extern "C" {
    fn relation_zk_lin_prove(
        proof: *mut u8,
        prooflen: *mut usize,
        a_ext: *const i64,
        t: *const i64,
        w: *const i64,
        ppseed: *const u8,
    ) -> c_int;
    fn relation_zk_lin_verify(
        proof: *const u8,
        prooflen: usize,
        a_ext: *const i64,
        t: *const i64,
        ppseed: *const u8,
    ) -> c_int;
}

/// center a canonical coefficient in `[0, Q)` to `(-Q/2, Q/2]`
fn center(x: i32) -> i64 {
    if x > Q / 2 {
        i64::from(x - Q)
    } else {
        i64::from(x)
    }
}

/// Fill `a_ext = [I | A' | -I | -A' | 0]` and `t = t'` (centered) for the
/// bridge seam.  `A'` entries leave the NTT domain via the SAME pipeline
/// `relation::amul` uses (mont product against ntt(1), partial reduce,
/// inverse NTT to canonical `[0, Q)`), one entry at a time.  Twin of the C
/// `pi_build_statement` (relation_zk.c).
fn build_statement(a_ext: &mut [i64; A_LEN], t: &mut [i64; T_LEN], y: &Statement, pp: &PublicParams) {
    // ehat = to_mont(partial_reduce(ntt(delta_0))), the constant-1 poly
    let mut e = [R0; 1];
    e[0].0[0] = 1;
    let mut ehat = ntt(&e);
    for x in ehat[0].0.iter_mut() {
        *x = partial_reduce32(*x);
    }
    let em = to_mont(&ehat);

    let t_prime = y.as_t_prime();
    for i in 0..PI_ROWS {
        for j in 0..PI_COLS {
            let dst = (i * PI_COLS + j) * PI_DEG;
            for k in 0..PI_DEG {
                a_ext[dst + k] = 0;
            }
            if j < N || (j >= N_PLUS_ELL && j < N_PLUS_ELL + N) {
                // identity blocks: column of I (resp. -I)
                let col = if j < N { j } else { j - N_PLUS_ELL };
                let sign = if j < N { 1i64 } else { -1i64 };
                if col == i {
                    a_ext[dst] = sign;
                }
            } else if j < 2 * N_PLUS_ELL {
                // A' blocks: NTT -> normal domain via the amul pipeline
                let col = if j < N_PLUS_ELL { j - N } else { j - N_PLUS_ELL - N };
                let sign = if j < N_PLUS_ELL { 1i64 } else { -1i64 };
                let mut acc = T0;
                for n in 0..PI_DEG {
                    acc.0[n] =
                        mont_reduce(i64::from(pp.a_prime[i][col].0[n]) * i64::from(em[0].0[n]));
                }
                for x in acc.0.iter_mut() {
                    *x = partial_reduce32(*x);
                }
                let lo = inv_ntt(&[acc]); // canonical [0, Q)
                for k in 0..PI_DEG {
                    a_ext[dst + k] = sign * center(lo[0].0[k]);
                }
            }
            // j == 2(n+ell): dummy l2 column stays all-zero
        }
        for k in 0..PI_DEG {
            t[i * PI_DEG + k] = center(t_prime[i].0[k]);
        }
    }
}

/// `prove` — pi <- P((t'; r'), {exists r': A r' = t', ||r'||inf <= 1})
/// (Fig. 1).  Writes the proof into `proof` and returns its exact length, or
/// `None` if the witness is not ternary: only an HONEST `Gen` witness can be
/// proven — an Ext-extracted witness from the extended relation `R'_A` (the
/// knowledge gap) is not a valid pi witness.  C twin: `relation_prove`
/// (relation_zk.c).
pub fn prove(
    proof: &mut [u8; PI_PROOF_MAX_BYTES], // out: pi (read back prooflen bytes)
    y: &Statement,                        // paper Y = t' (statement)
    r_prime: &Witness,                    // paper r' (honest ternary witness)
    pp: &PublicParams,                    // paper A: pp = A = [I | A']
) -> Option<usize> {
    let v = r_prime.as_relation_vector();
    for p in v.iter() {
        for &c in p.0.iter() {
            if !(-1..=1).contains(&c) {
                return None;
            }
        }
    }

    let mut a_ext = [0i64; A_LEN];
    let mut t = [0i64; T_LEN];
    let mut w = [0i64; W_LEN];
    build_statement(&mut a_ext, &mut t, y, pp);

    // binary decomposition (r_plus || r_minus || 0)
    for c in 0..N_PLUS_ELL {
        for k in 0..PI_DEG {
            let val = v[c].0[k];
            w[c * PI_DEG + k] = i64::from(val == 1);
            w[(N_PLUS_ELL + c) * PI_DEG + k] = i64::from(val == -1);
        }
    }

    let mut len: usize = 0;
    let rc = unsafe {
        relation_zk_lin_prove(
            proof.as_mut_ptr(),
            &mut len,
            a_ext.as_ptr(),
            t.as_ptr(),
            w.as_ptr(),
            pp.seed.as_ptr(),
        )
    };
    (rc == 0).then_some(len)
}

/// `proof_verify` — verify pi against `(pp, Y)`; Fig. 1's "If verif. of pi
/// ... fails, Abort" gate.  C twin: `relation_proof_verify` (relation_zk.c).
pub fn proof_verify(
    proof: &[u8],      // pi as received (exact length slice)
    y: &Statement,     // paper Y = t' (statement)
    pp: &PublicParams, // paper A: pp = A = [I | A']
) -> bool {
    let mut a_ext = [0i64; A_LEN];
    let mut t = [0i64; T_LEN];
    build_statement(&mut a_ext, &mut t, y, pp);
    let rc = unsafe {
        relation_zk_lin_verify(proof.as_ptr(), proof.len(), a_ext.as_ptr(), t.as_ptr(), pp.seed.as_ptr())
    };
    rc == 0
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::las_types::Witness;
    use crate::setup::{setup_public_params, LAS_SEEDBYTES, N_PLUS_ELL};
    use crate::types::R0;
    use core::array::from_fn;

    /// The prover-contract check the integration test cannot express: a
    /// non-ternary witness (constructible only inside the crate — the public
    /// codec validates ternary) must be refused before any proof is built.
    #[test]
    fn prove_refuses_non_ternary_witness() {
        let ppseed: [u8; LAS_SEEDBYTES] = from_fn(|i| i as u8);
        let pp = setup_public_params(&ppseed);
        let (y, _honest) = crate::relation::gen_seed(&pp, &ppseed);

        let mut bad = [R0; N_PLUS_ELL];
        bad[0].0[0] = 2; // outside S_1: the knowledge-gap shape pi must reject
        let bad = Witness::from_relation_vector(bad);

        let mut proof = [0u8; PI_PROOF_MAX_BYTES];
        assert!(prove(&mut proof, &y, &bad, &pp).is_none());
    }
}
