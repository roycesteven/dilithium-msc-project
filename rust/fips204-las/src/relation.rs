//! `relation` — the HARD-RELATION layer of LAS (eprint 2020/845): the
//! statement–witness generator `Gen` for the relation
//! `R_A = { (Y, y) = (t, r) : t = A r, ||r||inf <= 1 }` (Table 1), together
//! with the semantic types it owns, [`Statement`] and [`Witness`]
//! (physically defined in `setup.rs` so the codec below both schemes can see
//! them; re-exported here by their owner).  Rust twin of `ref/relation.{c,h}`.
//!
//! Paper model (Section 3, p.7): *"The statement-witness generation Gen for
//! R_A runs exactly as KeyGen."*  So `gen_seed` is the SAME mathematics as
//! `basesig::keygen_seed` — `r' <-$ S_1^(n+ell); t' = A r'` — but it
//! constructs the DISTINCT relation types `(Statement, Witness)`, never
//! `(PublicKey, SecretKey)`: a statement is pk-shaped yet the API keeps the
//! two non-interchangeable (no casts, no aliases).
//!
//! This layer sits between `setup` and the two schemes
//! (`setup -> relation / serialize -> basesig / las`): `las` consumes
//! statements and witnesses; `basesig` never sees them.  Extracted witnesses
//! (Ext's `s = z - ẑ`) live in the EXTENDED relation `R'_A`
//! (`||.||inf <= 2(gamma-kappa)`, the knowledge gap, p.9) — same [`Witness`]
//! type, not assumed ternary.
//!
//! NO INVENTED HELPERS: the two private helpers at the bottom are verbatim
//! twins of the `las.rs` helpers of the same name (which mirror the C helper
//! twins; duplication instead of sharing keeps the layering acyclic, exactly
//! like the C files keep local copies for independent linkability).

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

use core::array::from_fn;

use rand_core::CryptoRngCore;
use sha3::digest::{ExtendableOutput, Update, XofReader};
use sha3::Shake256;
use zeroize::Zeroize;

use crate::helpers::{full_reduce32, mont_reduce, partial_reduce32, to_mont};
use crate::ntt::{inv_ntt, ntt};
use crate::types::{R, R0, T0};

use crate::setup::{PublicParams, D, ELL, N, N_PLUS_ELL, LAS_SEEDBYTES};
// Owner re-export: Statement and Witness belong to THIS layer (physical home
// is las_types.rs, see that module's header).
pub use crate::las_types::{Statement, Witness};

const SHAKE256_RATE: usize = 136;

/// `apply_a` — evaluate the public linear map `Y = A v` on a caller-supplied
/// relation vector.
///
/// ADDITIVE, READ-ONLY, KAT-NEUTRAL (dilithium-msc-project, Stage 2): this
/// introduces no new mathematics — it is a thin public wrapper over the existing
/// private `amul`, which `gen_seed` already uses — and no existing code path is
/// changed, so the pinned KAT digest is unaffected. There is deliberately **no
/// C twin**: `ref/` has no caller, and adding one would put an unused function
/// in the implementation of record.
///
/// WHY IT EXISTS. `A` is expanded into `PublicParams` in the NTT domain and is
/// `pub(crate)`, so an external consumer cannot see the matrix. Stage 2's
/// Groth16 backend must encode the relation `A r = t` as R1CS constraints with
/// `A`'s coefficients as *public constants*, which means it has to recover the
/// matrix. Because `v -> A v` is linear, evaluating it on the unit vectors
/// recovers `A` column by column — this function is what makes that possible
/// without exposing internals or duplicating `expand_a` outside the crate.
///
/// Callers get the canonical `[0,Q)` representative, exactly as `gen_seed` does.
pub fn apply_a(
    pp: &PublicParams,   // paper A: pp = A = [I | A']
    v: &Witness,         // paper relation vector (need NOT be ternary)
) -> Statement {         // paper t' = A v
    Statement(amul(pp, v.as_relation_vector()))
}

/// `gen` — the paper's `Gen(1^lambda) -> (Y, y) in R_A` (Definition 3;
/// Section 3: "runs exactly as KeyGen").  Random path: fresh seed, then the
/// deterministic body.  Math-twin of `basesig::keygen` with relation types
/// constructed.  C twin: `relation_gen` (relation.c).
pub fn gen(
    pp: &PublicParams,             // paper A: pp = A = [I | A']
    rng: &mut impl CryptoRngCore,  // CSPRNG for the seed (no paper symbol)
) -> (Statement, Witness) {        // returns (paper Y, paper y) = (t', r')
    let mut seed = [0u8; LAS_SEEDBYTES]; // PRG seed to sample r' (no paper symbol)
    rng.fill_bytes(&mut seed);
    let out = gen_seed(pp, &seed);
    seed.zeroize(); // the witness is derivable from this seed: wipe
    out
}

/// `gen_seed` — deterministic Gen from an explicit 32-byte seed (reproducible
/// KATs).  Byte-identical mathematics to `basesig::keygen_seed`:
/// `r' <-$ S_1^(n+ell); t' = A r'` — the ONE private generation core the
/// paper prescribes ("Gen runs exactly as KeyGen"), constructing the distinct
/// semantic pair.  C twin: `relation_gen_seed` (relation.c).
pub fn gen_seed(
    pp: &PublicParams,           // paper A: pp = A = [I | A']
    seed: &[u8; LAS_SEEDBYTES],  // PRG seed to sample r' (no paper symbol)
) -> (Statement, Witness) {
    // [PAPER Alg.1] 1-2 (Gen = KeyGen):  r' <-$ S_1^(n+ell)
    let r_prime: [R; N_PLUS_ELL] = from_fn(|j| sample_ternary(seed, j as u16));
    // [PAPER Alg.1] 3 (Gen = KeyGen):    t' = A r'
    let t_prime = amul(pp, &r_prime);
    // [PAPER Alg.1] 4 (Gen = KeyGen):    return (Y, y) = (t', r')
    (Statement(t_prime), Witness::from_relation_vector(r_prime))
}

/* ============================ helpers ============================
 * Verbatim twins of the las.rs helpers of the same name (which carry the
 * full provenance notes down to the upstream/C twins); duplicated here so
 * the relation layer never depends on a scheme module. */

/// w = A*v = v_top + A'*v_bot, with A=[I|A'], A' (pp.a_prime) already in the
/// NTT domain.  Output canonical [0,Q).  Twin of `las.rs amul`.
fn amul(pp: &PublicParams, v: &[R; N_PLUS_ELL]) -> [R; N] {
    // vhat = NTT(v_bot), reduced to (-Q,Q) before use (representative-neutral).
    let vbot: [R; ELL] = from_fn(|j| v[N + j].clone());
    let mut vhat = ntt(&vbot);
    for p in vhat.iter_mut() {
        for x in p.0.iter_mut() {
            *x = partial_reduce32(*x);
        }
    }
    let vm = to_mont(&vhat);

    let mut w: [R; N] = [R0; N];
    for i in 0..N {
        let mut acc = T0;
        for j in 0..ELL {
            for n in 0..D {
                acc.0[n] += mont_reduce(i64::from(pp.a_prime[i][j].0[n]) * i64::from(vm[j].0[n]));
            }
        }
        // Mirrors C poly_reduce(&acc) before the inverse NTT (keeps i32 bounds).
        for x in acc.0.iter_mut() {
            *x = partial_reduce32(*x);
        }
        let lo = inv_ntt(&[acc]); // canonical [0,Q)
        for n in 0..D {
            // identity block + canonicalise (C: poly_add; poly_reduce; poly_caddq)
            w[i].0[n] = full_reduce32(lo[0].0[n] + v[i].0[n]);
        }
    }
    w
}

/// Sample one poly with coefficients uniform in {-1,0,1} (set S_1, ternary).
/// Twin of `las.rs sample_ternary` (2-bit codes, reject 3; contiguous stream).
fn sample_ternary(seed: &[u8; LAS_SEEDBYTES], nonce: u16) -> R {
    let mut h = Shake256::default();
    h.update(seed);
    h.update(&nonce.to_le_bytes());
    let mut rd = h.finalize_xof();
    let mut buf = [0u8; SHAKE256_RATE];
    rd.read(&mut buf);
    let mut pos = 0usize;

    let mut r = R0;
    let mut ctr = 0usize;
    while ctr < D {
        if pos >= SHAKE256_RATE {
            rd.read(&mut buf);
            pos = 0;
        }
        let byte = buf[pos];
        pos += 1;
        let mut s = 0u8;
        while s < 4 && ctr < D {
            let v = (byte >> (2 * s)) & 3;
            if v < 3 {
                r.0[ctr] = i32::from(v) - 1;
                ctr += 1;
            }
            s += 1;
        }
    }
    r
}
