//! `basesig` — the SEPARATE simplified Dilithium-style BASE signature =
//! Algorithm 1 of eprint 2020/845, written as a STRUCTURAL MIRROR of the
//! vendored upstream ML-DSA reference `src/ml_dsa.rs`: SAME function order,
//! SAME step-numbered comments (upstream's own `// 11: ...` lines are kept
//! verbatim so the two files scroll in lockstep), SAME inline composition
//! (the commitment-hash block, the matrix-vector sequence and the pointwise
//! product blocks are written out in the scheme functions exactly where
//! ml_dsa.rs writes them), names by the uniform prefix add onto the upstream
//! names.  Each function also names its C twin (`ref/basesig.c`, the file
//! that mirrors `ref/sign.c` the same way):
//!
//! This module is the ONE canonical Algorithm-1 implementation of the build:
//! Definition 3 (paper §2.3) says the adaptor scheme ΠR,R',Σ *inherits*
//! KeyGen, Sign and Verify from the underlying signature scheme Σ — so they
//! live HERE and only here (the `las` module holds no Algorithm-1 code; an
//! Adapt output is verified by THIS module's `verify`).
//!
//! ```text
//!   keygen        <-> key_gen          (ml_dsa.rs:26)  <-> base_keygen          (basesig.c)
//!   keygen_seed   <-> key_gen_internal (ml_dsa.rs:57)  <-> base_keygen_seed     (basesig.c; the KAT slot)
//!   sign_internal <-> sign_internal    (ml_dsa.rs:153) <-> base_sign_internal   (basesig.c)
//!   sign           -  try_sign_with_rng (lib.rs:287)   <-> base_sign            (basesig.c)
//!   sign_det       -  (deterministic KAT slot; no ml_dsa.rs analogue)
//!                                                      <-> base_sign_det        (basesig.c)
//!   verify_internal <-> verify_internal (ml_dsa.rs:351) <-> base_verify_internal (basesig.c)
//!   verify         -  verify           (lib.rs:383)    <-> base_verify          (basesig.c)
//!   [DELETED]     <-> expand_private (ml_dsa.rs:445) /
//!                     expand_public (ml_dsa.rs:477) /
//!                     private_to_public_key (ml_dsa.rs:502)
//!                     — byte-encoding expansion slots; the struct
//!                     tier has no byte keys.  The PACKED tier at the
//!                     BOTTOM of this file restores that byte boundary
//!                     (twin of basesig.c's packed tier).
//! ```
//!
//! ANNOTATION CONVENTION (read side by side with ml_dsa.rs):
//!
//! ```text
//!   [REUSED]  ml_dsa.rs:<line>: <upstream code>  -- same computation,
//!             dimensions swapped (K,L -> n, ell, m = n+ell)
//!   [CHANGED] quotes the upstream line(s) verbatim, then states WHY the
//!             line differs here;
//!   [DELETED] quotes the dropped upstream line(s) verbatim, then WHY
//!             Algorithm 1 does not need them.
//! ```
//!
//! NO INVENTED HELPERS, NO UNMARKED EXTRA LINES: the upstream primitives
//! `h256_xof`, `ntt`, `inv_ntt`, `to_mont`, `add_vector_ntt`, `mont_reduce`,
//! `partial_reduce32`, `full_reduce32`, `center_mod` and `infinity_norm` are
//! REUSED AS-IS (called directly, zero modifications); every line with no
//! upstream twin is explicitly marked `[CHANGED] ... no upstream line` with
//! its WHY (the reduced-domain partial_reduce32 insertions, the BASE_ATTEMPTS
//! instrumentation counter, and the import/const lines that serve them —
//! nothing unmarked); and every local `b_*` helper at the bottom of this file
//! is a one-to-one twin of exactly ONE NAMED upstream function with the same
//! body structure — only dimensions (K,L -> n, ell, m = n+ell), distributions
//! (eta, gamma_1 -> S_1, S_gamma) and weights (tau -> kappa) change.  Each
//! twin also names its C twin:
//!
//! ```text
//!   b_rej_bounded_poly <-> rej_bounded_poly (hashing.rs:158)   <-> b_rej_S1 + b_poly_uniform_S1
//!                                                                  (basesig.c:718, :748)
//!   b_expand_s         <-> expand_s         (hashing.rs:252)   <-> b_polyvecm_uniform_S1
//!                                                                  (basesig.c:958)
//!   b_expand_mask      <-> expand_mask      (hashing.rs:281)   <-> b_polyvecm_uniform_Sgamma +
//!                                                                  b_poly_uniform_Sgamma + b_rej_Sgamma
//!                                                                  (basesig.c:967, :825, :786)
//!   b_sample_in_ball   <-> sample_in_ball   (hashing.rs:43)    <-> b_poly_challenge (basesig.c:860)
//!   b_w_encode         <-> w1_encode        (encodings.rs:338) <-> b_polyvecn_pack_w + b_polyw_pack
//!                          ('1' dropped: packs the FULL w)         (basesig.c:1082, :905)
//!   b_mat_vec_mul      <-> mat_vec_mul      (helpers.rs:100)   <-> b_polyvec_matrix_pointwise_montgomery
//!                                                                  (basesig.c:927)
//! ```
//!
//! Algorithm 1 (paper p.7-8):
//! ```text
//!     KeyGen : r <- S_1^{n+l};  t = A r;  (pk, sk) = (t, r)
//!     Sign   : y <- S_g; w = A y; c = H(pk, w, M); z = y + c r; reject |z|inf > g-k
//!     Verify : w' = A z - c t;   accept iff  c == H(pk, w', M)
//! ```
//!
//! What Algorithm 1 DELETES vs ML-DSA (all "for ease of presentation", paper
//! s2.2/s3.2): Power2Round key compression, the high/low-bit split, the hint
//! vector (MakeHint/UseHint, the omega bound), the second (low-bits) rejection,
//! and hashing only the high bits.  What it CHANGES: ExpandS(eta) -> ternary
//! S_1; ExpandMask(gamma1) -> uniform S_gamma; SampleInBall(tau) -> kappa-weight
//! challenge; and it hashes the FULL commitment w.
//!
//! Kept SEPARATE from `las.rs`, with NO las.rs dependency: this module's
//! types (`PublicKey`/`SecretKey`/`Signature` — owned here, physically
//! defined in setup.rs with `PublicParams` and the shared bound
//! `BOUND_SIGN`) sit below both schemes (mirrors C `basesig.h` ->
//! `setup.h`).  The challenge hash binds `(pk, w, M)`, so an Adapted LAS
//! pre-signature verifies under THIS module's `verify` with no explicit `+Y`:
//!
//! ```text
//!     A(z_hat + y) - c t = (A z_hat - c t) + A y = w' + Y      (since Y = A y).
//! ```

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

/* ---- the ml_dsa.rs import block (ml_dsa.rs:3-14), mirrored slot by slot;
 * every dropped or added import is annotated, same convention as the code. ---- */

use core::array::from_fn;
// ^[CHANGED] no ml_dsa.rs import line: upstream spells `core::array::from_fn`
// out in full at every use.  Imported once here, so where a quoted upstream
// line reads `core::array::from_fn(...)` the mirroring code reads
// `from_fn(...)` -- same function, display only.
use core::sync::atomic::{AtomicU64, Ordering};
// ^[CHANGED] no upstream line.
// WHY: carries the BASE_ATTEMPTS instrumentation counter (below) only.

use rand_core::CryptoRngCore; // [REUSED] ml_dsa.rs:13: use rand_core::CryptoRngCore;
use sha3::digest::{ExtendableOutput, Update, XofReader};
use sha3::Shake256;
use zeroize::Zeroize;
// ^[CHANGED] no ml_dsa.rs import line (upstream zeroizes via its key structs'
// derives, types.rs).
// WHY: sign/sign_det wipe their sk-equivalent mask seeds after use (the
// upstream secret-material policy applied to this file's loose seed buffers).
// ^[CHANGED] ml_dsa.rs:14: use sha3::digest::XofReader;
// WHY: ml_dsa.rs only READS XOF streams built inside hashing.rs (h256_xof,
// hashing.rs:12-16, which imports Shake256/Update/ExtendableOutput itself at
// hashing.rs:7-8); the sampler twins that build their own SHAKE streams live
// in THIS file (bottom), so their construction imports come with them.

// [DELETED] ml_dsa.rs:3:
//     use crate::encodings::{pk_decode, pk_encode, sig_decode, sig_encode, sk_decode, w1_encode};
// WHY: the byte codecs of the DELETED packed key/signature formats; keys and
// signatures are structs here, and the packed tier at the bottom uses the
// shared codec serialize.rs (the packing.{c,h} twin) instead.  w1_encode's
// slot is the local twin b_w_encode ('1' dropped: full w, HighBits deleted).
use crate::hashing::h256_xof;
// ^[CHANGED] ml_dsa.rs:4:
//     use crate::hashing::{expand_a, expand_mask, expand_s, h256_xof, sample_in_ball};
// WHY: h256_xof is REUSED as-is.  expand_a's slot moved to setup.rs (A is
// public infrastructure expanded once into pp, not per call); expand_mask /
// expand_s / sample_in_ball are replaced by their named local twins
// b_expand_mask / b_expand_s / b_sample_in_ball (bottom of this file).
use crate::helpers::{
    add_vector_ntt, center_mod, full_reduce32, mont_reduce, partial_reduce32,
    to_mont,
};
// ^[CHANGED] ml_dsa.rs:5-8: the same list plus mat_vec_mul.
// WHY: identical REUSED primitives minus mat_vec_mul, replaced by its named
// local twin b_mat_vec_mul (A' is n x ell and the [I | .] identity block is
// applied by the callers).
// [DELETED] ml_dsa.rs:9:
//     use crate::high_low::{high_bits, low_bits, make_hint, power2round, use_hint};
// WHY: the entire high/low-bit machinery is exactly what Algorithm 1 deletes.
use crate::ntt::{inv_ntt, ntt}; // [REUSED] ml_dsa.rs:10: use crate::ntt::{inv_ntt, ntt};
use crate::types::{R, R0, T, T0};
// ^[CHANGED] ml_dsa.rs:11: use crate::types::{PrivateKey, PublicKey, R, T};
// WHY: PrivateKey/PublicKey are the DELETED precompute-carrying key structs
// (their expansion slots expand_private/expand_public are the [DELETED] note
// above the helpers); keys here are the shared setup.rs structs, imported
// below.  R0/T0 (the zero polys) are added because the local b_* twins need
// them HERE -- the upstream originals import them where THEY live
// (hashing.rs:5, helpers.rs:1).
use crate::Q; // RE-IMPORTED (see note below): b_chknorm_vec's `(Q-1)/8` guard needs Q
// [CHANGED] ml_dsa.rs:12: use crate::{D, Q};
// WHY: D only feeds Power2Round's 2^d shifts (ml_dsa.rs:111), deleted with it.
// Q was originally deleted with make_hint, but is re-imported above for
// b_chknorm_vec's `bound > (Q-1)/8` guard (mirrors C poly_chknorm).

// <-> basesig.c: #include "setup.h": the shared system layer -- the
// construction parameters and this module's OWN types (PublicKey / SecretKey /
// Signature, physically defined in setup.rs so the codec below can see them).
// This file has no las.rs or relation.rs dependency (the layers above are
// never looked up at).
use crate::setup::{PublicParams, D, ELL, GAMMA, KAPPA, N, N_PLUS_ELL, LAS_CTILDEBYTES, LAS_SEEDBYTES};
// Owner re-export: the Algorithm-1 (Σ) types belong to THIS module (physical
// home is las_types.rs, see that module's header) — callers import them
// from their owner: `use fips204::basesig::{PublicKey, SecretKey, Signature}`.
pub use crate::las_types::{PublicKey, SecretKey, Signature};
// [REUSED] basesig.c: #include "serialize.h"
// (itself [REUSED] sign.c:4: #include "packing.h")
// WHY: the end-to-end PACKED-API tier at the BOTTOM of this file unpacks/packs
// inside the call, exactly the boundary ml_dsa.rs exposes; the core (struct)
// tier stays byte-free.
use crate::serialize::{
    pack_public_key, pack_secret_key, pack_signature, unpack_public_key, unpack_secret_key,
    unpack_signature, PUBLIC_KEY_BYTES, SECRET_KEY_BYTES, SIGNATURE_BYTES,
};

const SHAKE256_RATE: usize = 136;
// ^[CHANGED] no upstream lines.
// WHY: the SHAKE256 block size for the local b_* twins' C-matching
// block-buffered squeezes; upstream's originals get this from their own files
// (hashing.rs reads its XOFs unbuffered).  The ring degree is `D` (imported
// from setup, = 256), fixed inside types.rs's arrays.

/// Algorithm-1 Sign/Verify rejection bound (chknorm-style: reject `>= bound`,
/// so the strict `>` test is encoded as bound = limit+1): Sign and Verify
/// reject at `||z||inf > gamma-kappa` (Alg. 1 steps 11/16), and an Adapt
/// output must clear exactly this bound (Lemma 1, Eq. (1)).  This is the
/// Algorithm-1 rejection rule, so `basesig` OWNS it (the adaptor-only
/// `BOUND_PRESIGN` lives in `las`).  C twin: `BOUND_SIGN` (basesig.h).
pub const BOUND_SIGN: i32 = GAMMA - KAPPA + 1;

/// Early-exit infinity-norm rejection check: reject (`true`) iff any coefficient
/// has `|coeff| >= bound`.  Byte-for-byte the same algorithm as the C base path
/// (`poly_chknorm`, early-exit at the first offending coeff) and the LAS path
/// (`las.rs::chknorm_vec`) -- NOT the upstream full-scan `infinity_norm().max()`.
/// FAIRNESS: this check runs inside the Sign rejection loop, so an
/// early-exit-vs-full-scan mismatch would make a REJECTED attempt cost
/// differently in the base path than in the LAS path (and than in C), biasing
/// the PreSign-vs-Sign / PreVerify-vs-Verify benchmark overhead.  The decision
/// is identical to `infinity_norm(z) >= bound` (`max|z| >= b` <=> `exists coeff
/// |z| >= b`), so the fix is KAT-preserving.  Mirrors ref/poly.c `poly_chknorm`.
fn b_chknorm_vec(z: &[R; N_PLUS_ELL], bound: i32) -> bool {
    // C poly_chknorm guard: bounds above (Q-1)/8 are rejected outright.
    if bound > (Q - 1) / 8 {
        return true;
    }
    z.iter()
        .flat_map(|p| p.0.iter())
        .any(|&x| x.abs() >= bound)
}

/// Rejection-sampling attempt counter for the BASE path (measurement only;
/// C twin: `base_attempts`, basesig.c:54; no ml_dsa.rs analogue).  Never read
/// by the scheme itself; benchmarks reset and read it to report the restart
/// rate.  Relaxed ordering: single-threaded benchmark instrumentation.
pub static BASE_ATTEMPTS: AtomicU64 = AtomicU64::new(0);

/* ==================== scheme, Algorithm 1 (base path) ====================
 * (The construction-wide setup_public_params lives in setup.rs — see that
 * module's header; every layer consumes that same PublicParams.) */

/// `keygen` <-> `key_gen` (ml_dsa.rs:26); C twin: `base_keygen` (basesig.c).
/// Draw a fresh seed, then the deterministic KeyGen body.
/// The RNG is injected (Rust idiom), exactly as ml_dsa.rs's `key_gen(rng)`.
pub fn keygen(
    pp: &PublicParams,             // paper A: pp = A = [I | A'] (public matrix)
    rng: &mut impl CryptoRngCore,  // CSPRNG for the seed (no paper symbol)
) -> (PublicKey, SecretKey) {      // returns (paper t, paper r) = (pk, sk)
    // 1: ξ ← B^{32}    ▷ Choose random seed
    let mut xi = [0u8; LAS_SEEDBYTES]; // PRG seed to sample r (no paper symbol)
    rng.fill_bytes(&mut xi);
    // ^[CHANGED] ml_dsa.rs:40-41:
    //     let mut xi = [0u8; 32];
    //     rng.try_fill_bytes(&mut xi).map_err(|_| "KeyGen: Random number generator failed")?;
    // WHY: same 32-byte draw; the infallible fill is used because this build
    // has no Result-based API surface (the C twin returns int 0
    // unconditionally, basesig.c:124 randombytes()).

    // 5: return ML-DSA.KeyGen_internal(𝜉)
    keygen_seed(pp, &xi)
    // ^[REUSED] ml_dsa.rs:44: Ok(key_gen_internal::<CTEST, K, L, PK_LEN, SK_LEN>(eta, &xi))
    // (eta gone: the secret set is fixed ternary S_1, see keygen_seed)
}

/// `keygen_seed` <-> `key_gen_internal` (ml_dsa.rs:57); C twin:
/// `base_keygen_seed` (basesig.c).  Algorithm 1 KeyGen from an explicit
/// 32-byte seed — the deterministic KAT slot (Definition 3: KeyGen belongs to
/// Σ, so the seeded slot lives HERE; test_kat's key vectors call this).
/// r <- S_1^{n+l}; t = A r; (pk,sk) = (t,r).
pub fn keygen_seed(
    pp: &PublicParams,         // paper A: pp = A = [I | A']
    xi: &[u8; LAS_SEEDBYTES],  // PRG seed to sample r (<-> xi, ml_dsa.rs:64)
) -> (PublicKey, SecretKey) {
    // [PAPER Alg.1] 1:  procedure KeyGen():    // same as Gen
    //
    // 1: (rho, rho′, 𝐾) ∈ 𝔹^{32} × 𝔹^{64} × 𝔹^{32} ← H(𝜉||IntegerToBytes(𝑘,1)||IntegerToBytes(ℓ,1),128)
    // [DELETED] ml_dsa.rs:68-74:
    //     let mut h2 = h256_xof(&[xi, &[K.to_le_bytes()[0]], &[L.to_le_bytes()[0]]]);
    //     let mut rho = [0u8; 32];
    //     h2.read(&mut rho);
    //     let mut rho_prime = [0u8; 64];
    //     h2.read(&mut rho_prime);
    //     let mut cap_k = [0u8; 32];
    //     h2.read(&mut cap_k);
    // WHY: upstream splits the seed into rho (to re-derive A from the packed
    // key), rho_prime (sampler seed) and K (signing PRF key) because its keys
    // are byte-packed and self-contained.  Here A lives in pp, keys stay
    // structs, and there is no signing PRF key -- the raw 32-byte seed IS the
    // sampler seed.

    // 4: (s_1, s_2) ← ExpandS(ρ′)
    // [PAPER Alg.1] 2:      r ←$ S₁^(n+ℓ)
    let r: [R; N_PLUS_ELL] = b_expand_s(xi); // paper r: the secret vector
    // ^[CHANGED] ml_dsa.rs:79:
    //     let (s_1, s_2): ([R; L], [R; K]) = expand_s::<CTEST, K, L>(eta, &rho_prime);
    // WHY: the paper's secret is ONE ternary vector r <- S_1^{n+l} with
    // ||r||inf <= 1 (Table 1) -- this is what caps ||c*r||inf <= kappa
    // (Fact 1) and so fixes the rejection bound gamma-kappa.  There is NO
    // separate error vector s_2: with A = [I | A'] in Hermite normal form the
    // identity block makes the top n components of r play s_2's role.

    // 3: cap_a_hat ← ExpandA(ρ)    ▷ A is generated and stored in NTT representation as Â
    // [DELETED] ml_dsa.rs:85:
    //     let cap_a_hat: [[T; L]; K] = expand_a::<CTEST, K, L>(&rho);
    // WHY: A = [I | A'] is a system-wide public parameter, expanded ONCE in
    // setup.rs (by this same UNMODIFIED upstream expand_a) and passed in
    // as pp; upstream must re-expand A from rho on every call because rho
    // travels inside each packed key.

    // 5: t ← NTT−1(cap_a_hat ◦ NTT(s_1)) + s_2    ▷ Compute t = As1 + s2
    // [PAPER Alg.1] 3:      t = A r
    let t: [R; N] = {
        // r = (r_0 || r_1): r_0 = top n components (identity block of
        // A = [I | A']), r_1 = bottom ell components (the ones that meet A').
        let r_1: [R; ELL] = from_fn(|j| r[N + j].clone());
        // ^[CHANGED] (input of) ml_dsa.rs:86: let s_1_hat: [T; L] = ntt(&s_1);
        // WHY: A = [I | A'] -- only the BOTTOM l components of r meet A' and
        // need the NTT; the top n components pass through the identity block
        // untouched (added below).  Upstream's A is a full K x L matrix, so
        // ALL of s_1 is transformed.
        let mut r_1_hat: [T; ELL] = ntt(&r_1); // [REUSED] ml_dsa.rs:86: let s_1_hat: [T; L] = ntt(&s_1);
        for p in r_1_hat.iter_mut() {
            for x in p.0.iter_mut() {
                *x = partial_reduce32(*x);
            }
        }
        // ^[CHANGED] no upstream line.
        // WHY: reduce the NTT output to (-Q,Q) before the Montgomery products
        // (mirrors the C poly_reduce placement in basesig.c) so every i64
        // product and i32 accumulator stays in the reduced-domain bounds;
        // representative-neutral (the result is canonicalised below).
        let a_prime_r_1_hat: [T; N] = b_mat_vec_mul(&pp.a_prime, &r_1_hat); // A' r_1 (NTT domain)
        // ^[REUSED] ml_dsa.rs:87: let as1_hat: [T; K] = mat_vec_mul(&cap_a_hat, &s_1_hat);
        let r_0: [R; N] = from_fn(|k| r[k].clone());
        let t_not_reduced: [R; N] = add_vector_ntt(&inv_ntt(&a_prime_r_1_hat), &r_0);
        // ^[CHANGED] ml_dsa.rs:88:
        //     let t_not_reduced: [R; K] = add_vector_ntt(&inv_ntt(&as1_hat), &s_2);
        // WHY: same "+ error" step (UNMODIFIED upstream add_vector_ntt), but
        // the error IS the top n components of r: t = A r = r_0 + A' r_1
        // for A = [I | A'] (r_0 is that explicit n-polynomial prefix).
        from_fn(|k| R(from_fn(|n| full_reduce32(t_not_reduced[k].0[n]))))
        // ^[REUSED] ml_dsa.rs:89-91:
        //     let t: [R; K] = core::array::from_fn(|k| {
        //         R(core::array::from_fn(|n| full_reduce32(t_not_reduced[k].0[n])))
        //     });
    };
    // [DELETED] ml_dsa.rs:92:
    //     power2round(&t)
    // WHY: no key compression in the paper's simplified scheme -- the
    // verifier recomputes w' with the EXACT t (w' = Az - ct), so t is never
    // split into t1/t0 and no hint is ever needed.

    // 8: pk ← pkEncode(ρ, t_1)
    // 9: tr ← H(BytesToBits(pk), 64)
    // [DELETED] ml_dsa.rs:99-101:
    //     let mut tr = [0u8; 64];
    //     let mut h8 = h256_xof(&[&pk_encode::<K, PK_LEN>(&rho, &t_1)]);
    //     h8.read(&mut tr);
    // WHY: keys are structs (no byte encoding at this tier; the wire format
    // is serialize.rs), and the challenge hash binds the raw t directly,
    // so no key digest tr is precomputed.
    // [DELETED] ml_dsa.rs:108-130: the t1_d2_hat_mont / s_1_hat_mont /
    //     s_2_hat_mont / t_0_hat_mont pre-computes and the PublicKey /
    //     PrivateKey struct assembly.
    // WHY: those pre-computes amortise upstream's byte decode; the struct
    // keys here hold the raw polynomials, and the per-call NTT hoist happens
    // in base_sign_internal instead (the same "step 2-4" cost point).

    // 11: return (pk, sk)
    // [PAPER Alg.1] 4:      return (pk, sk) = (t, r)
    // [PAPER Alg.1] 5:  end procedure
    (PublicKey { t }, SecretKey { r })
}

/// `sign_internal` <-> `sign_internal` (ml_dsa.rs:153); C twin:
/// `base_sign_internal` (basesig.c).  Algorithm 1 Sign body,
/// parameterised by the caller-supplied 64-byte mask seed (ml_dsa.rs's
/// internal takes `rnd` the same way).
pub(crate) fn sign_internal(
    m: &[u8],           // paper M: message
    pk: &PublicKey,     // paper t: pk.t = t (public key)
    sk: &SecretKey,     // paper r: sk.r = r (secret key)
    pp: &PublicParams,  // paper A: pp = A = [I | A']
    seed: &[u8; 64],    // PRG mask seed (<-> rnd, ml_dsa.rs:163)
) -> Signature {        // paper σ: returns σ = (c, z)
    // [PAPER Alg.1] 6:  procedure Sign((pk, sk), M):
    //
    // 1: (ρ, K, tr, s_1, s_2, t_0) ← skDecode(sk)
    // 2: s_1_hat ← NTT(s_1)
    let r_hat_mont: [T; N_PLUS_ELL] = {
        let mut r_hat: [T; N_PLUS_ELL] = ntt(&sk.r); // paper r in NTT domain
        for p in r_hat.iter_mut() {
            for x in p.0.iter_mut() {
                *x = partial_reduce32(*x);
            }
        }
        to_mont(&r_hat)
    };
    // ^[CHANGED] ml_dsa.rs:169:
    //     let PrivateKey { rho, cap_k, tr, s_1_hat_mont, s_2_hat_mont, t_0_hat_mont } = esk;
    // (whose s_1_hat_mont was built as `to_mont(&ntt(&s_1))`, ml_dsa.rs:121)
    // WHY: upstream extracts NTT(s_1) ready-made from its pre-computed key
    // struct; the struct key here holds the raw r, so the SAME
    // to_mont(&ntt(..)) hoist happens HERE, once per call (the secret is
    // invariant across rejection attempts), with the reduced-domain
    // partial_reduce32 in between (same reason as in KeyGen).  The FULL
    // (n+ell)-vector r is transformed (the products below run over all n+ell
    // response polynomials).  No s_2/t_0 transforms exist.

    // 5: cap_a_hat ← ExpandA(ρ)    ▷ A is generated and stored in NTT representation as Â
    // [DELETED] ml_dsa.rs:181:
    //     let cap_a_hat: [[T; L]; K] = expand_a::<CTEST, K, L>(rho);
    // WHY: A is fixed in pp (see setup.rs).

    // 6: 𝜇 ← H(BytesToBits(𝑡𝑟)||𝑀 , 64)    ▷ Compute message representative µ
    let mut t_tilde = [0u8; N * D * 4]; // packed pk: plays mu's role as the fixed hash prefix
    b_w_encode(&pk.t, &mut t_tilde);
    // ^[CHANGED] ml_dsa.rs:190 (the 6b path):
    //     h256_xof(&[tr, &[0u8], &[ctx.len().to_le_bytes()[0]], ctx, message])
    // WHY: upstream compresses (key digest tr, ctx prefix, M) into the
    // 64-byte mu ONCE per call and re-absorbs mu each attempt.  The paper's
    // oracle is c = H(pk, w, M) with the RAW public key: so the once-per-call
    // precompute here is packing t canonically (b_w_encode); M is absorbed
    // directly in the loop below, and there is no ctx prefix.

    // 7: ρ′' ← H(K || rnd || µ, 64)    ▷ Compute private random seed
    // [CHANGED] ml_dsa.rs:199-201:
    //     let mut h7 = h256_xof(&[cap_k, &rnd, &mu]);
    //     let mut rho_prime = [0u8; 64];
    //     h7.read(&mut rho_prime);
    // WHY: rho_prime (the 64-byte mask-sampler seed) is derived by the CALLER
    // here and passed in as `seed`: base_sign draws it fresh from the RNG;
    // las.rs's deterministic path derives it as SHAKE256(tag, sk, [Y], M) --
    // same role, same width.

    // 8: κ ← 0    ▷ Initialize counter κ
    let mut mask_nonce = 0u16;
    // ^[CHANGED] ml_dsa.rs:204: let mut kappa_ctr = 0u16;
    // WHY: same counter, renamed -- upstream's "κ" counter name collides with
    // the LAS paper's κ = challenge weight (KAPPA); this is the mask
    // sampler's per-attempt nonce, so it is named as such.

    // 9: (z, h) ← ⊥    ▷ we will handle ⊥ inline with 'continue'
    // [DELETED] ml_dsa.rs:207-209:
    //     let mut z: [R; L];
    //     let mut h: [R; K];
    //     let mut c_tilde = [0u8; LAMBDA_DIV4];
    // WHY: z and the challenge are built and returned inside the loop (struct
    // output); there is no hint h, and the challenge digest is loop-local.

    // 10: while (z, h) = ⊥ do    ▷ Rejection sampling loop (with continue for ⊥)
    loop {                                   // [REUSED] ml_dsa.rs:212: loop {
        BASE_ATTEMPTS.fetch_add(1, Ordering::Relaxed); // instrumentation only; no
                                                       // upstream line (added so benchmarks
                                                       // read the restart rate directly)

        // 11: y ← ExpandMask(ρ′', κ)
        // [PAPER Alg.1] 7:      y ←$ Sγ^(n+ℓ)
        let y: [R; N_PLUS_ELL] = b_expand_mask(seed, mask_nonce);
        // ^[CHANGED] ml_dsa.rs:215:
        //     let y: [R; L] = expand_mask(gamma1, &rho_prime, kappa_ctr);
        // WHY: the paper's mask set is S_gamma = uniform [-gamma, gamma] with
        // gamma = kappa*d*(n+l) -- NOT a power of two like gamma_1, so
        // upstream's fixed-width bit-unpack cannot produce it and the twin
        // sampler rejection-samples.  Same vector-level call, same seed role
        // (rho_prime -> the 64-byte mask seed), same kappa_ctr discipline
        // (+= m per rejected attempt; per-poly nonce = kappa_ctr + r).

        // 12: w ← NTT−1(cap_a_hat ◦ NTT(y))
        // [PAPER Alg.1] 8:      w = A y
        let w: [R; N] = {
            // y = (y_0 || y_1), same split as r: y_0 = identity-block prefix,
            // y_1 = the ell components that meet A'.
            let y_1: [R; ELL] = from_fn(|j| y[N + j].clone());
            // ^[CHANGED] (input of) ml_dsa.rs:219: let y_hat: [T; L] = ntt(&y);
            // WHY: A = [I | A'] -- only the bottom l components of y meet A';
            // the top n join via the identity block below (same as KeyGen).
            let mut y_1_hat: [T; ELL] = ntt(&y_1); // [REUSED] ml_dsa.rs:219: let y_hat: [T; L] = ntt(&y);
            for p in y_1_hat.iter_mut() {
                for x in p.0.iter_mut() {
                    *x = partial_reduce32(*x);
                }
            }
            // ^[CHANGED] no upstream line: reduced domain before the products
            // (same reason as in KeyGen; representative-neutral).
            let a_prime_y_1_hat: [T; N] = b_mat_vec_mul(&pp.a_prime, &y_1_hat); // A' y_1 (NTT domain)
            // ^[REUSED] ml_dsa.rs:220: let ay_hat: [T; K] = mat_vec_mul(&cap_a_hat, &y_hat);
            let y_0: [R; N] = from_fn(|k| y[k].clone());
            let w_not_reduced: [R; N] = add_vector_ntt(&inv_ntt(&a_prime_y_1_hat), &y_0);
            // ^[CHANGED] ml_dsa.rs:221: inv_ntt(&ay_hat)
            // WHY: upstream's Sign has no addition here because its A is a
            // full matrix; the identity block of A = [I | A'] completes
            // w = A y = y_0 + A' y_1 (UNMODIFIED upstream add_vector_ntt
            // exactly as KeyGen's step 5 uses it, ml_dsa.rs:88; y_0 is the
            // explicit n-polynomial prefix of y).
            from_fn(|k| R(from_fn(|n| full_reduce32(w_not_reduced[k].0[n]))))
            // ^[REUSED] ml_dsa.rs:89-91 (the canonicalising from_fn/full_reduce32 wrap)
        };

        // 13: w_1 ← HighBits(w)    ▷ Signer’s commitment
        // [DELETED] ml_dsa.rs:225-226:
        //     let w_1: [R; K] =
        //         core::array::from_fn(|k| R(core::array::from_fn(|n| high_bits(gamma2, w[k].0[n]))));
        // WHY: the paper hashes the FULL commitment w ("for ease of
        // presentation", paper §2.2/§3.2) -- no high/low-bit split, hence
        // also no w_0, no second rejection test and no hint vector below.

        // 15: c_tildẽ ← H(mu||w1Encode(w_1), 𝜆/4)    ▷ commitment hash
        // [PAPER Alg.1] 9:      c = H(pk, w, M)
        let mut w_tilde = [0u8; N * D * 4];
        b_w_encode(&w, &mut w_tilde);           // [REUSED] ml_dsa.rs:232: w1_encode::<K>(gamma2, &w_1, &mut w1_tilde);
        let mut h15 = h256_xof(&[&t_tilde[..], &w_tilde[..], m]);
        let mut c_tilde = [0u8; LAS_CTILDEBYTES];
        h15.read(&mut c_tilde);
        // ^[CHANGED] ml_dsa.rs:233-234:
        //     let mut h15 = h256_xof(&[&mu, &w1_tilde]);
        //     h15.read(&mut c_tilde);
        // WHY: same UNMODIFIED upstream h256_xof, but the oracle input is
        // (packed raw pk, packed full w, M) -- the paper's c = H(pk, w, M)
        // binds pk and M directly.  As in upstream, this 32-byte digest c_tilde
        // IS the stored challenge component of the signature (returned below).

        // 16: c ∈ 𝑅𝑞 ← SampleInBall(c_tilde_1)    ▷ Verifier’s challenge
        let c: R = b_sample_in_ball(&c_tilde);
        // ^[REUSED] ml_dsa.rs:237:
        //     let c: R = sample_in_ball::<CTEST>(tau, &c_tilde);
        // WHY: same SampleInBall construction with the paper's challenge weight
        // kappa (per parameter set) instead of tau.  c is a LOCAL arithmetic
        // value only (feeds c*r below); the STORED component is the digest
        // c_tilde, exactly as upstream sign.c stores c_tilde and re-derives the
        // polynomial via poly_challenge.

        // 17: c_hat ← NTT(c)
        let c_hat: T = {
            let mut ch = ntt(&[c.clone()]);
            for x in ch[0].0.iter_mut() {
                *x = partial_reduce32(*x);
            }
            let [ch] = ch;
            ch
        };
        // ^[REUSED] ml_dsa.rs:240: let c_hat: &T = &ntt(&[c])[0];
        // (once per attempt, shared by all m products below; plus the
        // reduced-domain partial_reduce32, same reason as in KeyGen)

        // 18: ⟨⟨c_s_1⟩⟩ ← NTT−1(c_hat ◦ s_1_hat)
        let c_r: [R; N_PLUS_ELL] = {
            // paper c·r (exact, |c·r|inf <= kappa): upstream's c_s_1 slot,
            // named after the paper's factors (the secret is r, not s_1).
            let c_r_hat: [T; N_PLUS_ELL] = from_fn(|l| {
                T(from_fn(|n| {
                    mont_reduce(i64::from(c_hat.0[n]) * i64::from(r_hat_mont[l].0[n]))
                }))
            });
            let c_r = inv_ntt(&c_r_hat);
            from_fn(|l| R(from_fn(|n| center_mod(c_r[l].0[n]))))
        };
        // ^[REUSED] ml_dsa.rs:243-249:
        //     let cs1_hat: [T; L] = core::array::from_fn(|l| {
        //         T(core::array::from_fn(|n| {
        //             mont_reduce(i64::from(c_hat.0[n]) * i64::from(s_1_hat_mont[l].0[n]))
        //         }))
        //     });
        //     inv_ntt(&cs1_hat)
        // with [CHANGED] the final center_mod wrap (no upstream line):
        // WHY: pins the unique centred representative of c·r, which is EXACT
        // and small (|c·r|inf <= kappa, the paper's Fact 1) -- so the z sum
        // below needs no reduction and the adaptor arithmetic in las.rs
        // (z = z_hat + r', s = z - z_hat) stays exact.

        // 19: ⟨⟨c_s_2⟩⟩ ← NTT−1(c_hat ◦ s_2_hat)
        // [DELETED] ml_dsa.rs:253-260:
        //     let c_s_2: [R; K] = { ... s_2_hat_mont ... inv_ntt(&cs2_hat) };
        // WHY: no error vector s_2 (identity block of A).

        // 20: z ← y + ⟨⟨c_s_1⟩⟩    ▷ Signer’s response
        // [PAPER Alg.1] 10:     z = y + c r, where r := sk
        let z: [R; N_PLUS_ELL] = from_fn(|l| R(from_fn(|n| y[l].0[n] + c_r[l].0[n])));
        // ^[CHANGED] ml_dsa.rs:263-265:
        //     z = core::array::from_fn(|l| {
        //         R(core::array::from_fn(|n| partial_reduce32(y[l].0[n] + c_s_1[l].0[n])))
        //     });
        // WHY: no partial_reduce32 -- both summands are exact centred values
        // (|y| <= gamma, |c·r| <= kappa), so the plain i32 sum IS the exact
        // response and the norm test below sees exact magnitudes.

        // 21: r0 ← LowBits(w − ⟨⟨c_s_2⟩⟩)
        // [DELETED] ml_dsa.rs:268-272:
        //     let r0: [R; K] = core::array::from_fn(|k| {
        //         R(core::array::from_fn(|n| {
        //             low_bits(gamma2, partial_reduce32(w[k].0[n] - c_s_2[k].0[n]))
        //         }))
        //     });
        // WHY: the low-bits value protects the w_0/w_1 DECOMPOSITION, which
        // Algorithm 1 deleted -- there is no w_0 and no s_2.

        // 23: if ||z||∞ ≥ Gamma1 − β or ||r0||∞ ≥ Gamma2 − β then (z, h) ← ⊥    ▷ Validity checks
        // [PAPER Alg.1] 11:     if ||z||∞ > γ − κ, then Restart
        if b_chknorm_vec(&z, BOUND_SIGN) {
            // ^[CHANGED] ml_dsa.rs:277-280:
            //     let z_norm = infinity_norm(&z);
            //     if !CTEST && ((z_norm >= (gamma1 - beta)) || (r0_norm >= (gamma2 - beta))) {
            // WHY: same reject-if-too-large test at bound gamma - kappa + 1
            // (beta = tau*eta with eta = 1 => beta = kappa; reject STRICTLY
            // above gamma - kappa, paper Alg. 1 step 11).  The r0 half is gone
            // with the decomposition, and there is no CTEST path.  The norm
            // check itself is the EARLY-EXIT b_chknorm_vec (matches C
            // poly_chknorm and las.rs chknorm_vec), NOT the full-scan upstream
            // infinity_norm: identical decision, but the SAME rejection-loop
            // work profile as the base C path and the LAS path, so the
            // PreSign-vs-Sign benchmark overhead is measured fairly.

            // 31: κ ← κ + ℓ ▷ Increment counter
            mask_nonce += N_PLUS_ELL as u16; // [REUSED] ml_dsa.rs:281: kappa_ctr += u16::try_from(L)... (L -> n+ell)
            continue;                            // [REUSED] ml_dsa.rs:282: continue;
        }

        // 25: ⟨⟨c_t_0⟩⟩ ← NTT−1(c_hat ◦ t_hat_0)
        // 26: h ← MakeHint(−⟨⟨c_t_0⟩⟩, w − ⟨⟨c_s_2⟩⟩ + ⟨⟨c_t_0⟩⟩)    ▷ Signer’s hint
        // 28: if ||⟨⟨c_t_0⟩⟩||∞ ≥ Gamma2 or the number of 1’s in h is greater than ω, then (z, h) ← ⊥
        // [DELETED] ml_dsa.rs:288-319 (the whole c_t_0 / make_hint / omega block).
        // WHY: hints only exist to let the verifier reconstruct high bits from
        // the COMPRESSED t_1; with the exact t kept (no Power2Round) the
        // verifier recomputes w' exactly and no hint vector is needed.

        // 33: σ ← sigEncode(c_tilde, z mod± q, h)
        // 34: return σ
        // [PAPER Alg.1] 12:     return σ = (c, z)
        return Signature { c_tilde, z };
        // ^[CHANGED] ml_dsa.rs:334-336:
        //     let zmodq: [R; L] =
        //         core::array::from_fn(|l| R(core::array::from_fn(|n| center_mod(z[l].0[n]))));
        //     sig_encode::<CTEST, K, L, LAMBDA_DIV4, SIG_LEN>(gamma1, omega, &c_tilde, &zmodq, &h)
        // WHY: TYPED struct return, not upstream's packed BYTE encoding.  Upstream
        // sig_encode serialises (c_tilde, z, h) into SIG_LEN bytes here; this core
        // tier instead returns the struct Signature { c_tilde, z }, deferring the
        // byte encoding to serialize.rs (the packed tier below).  It carries the
        // SAME challenge component upstream stores -- the 32-byte digest c_tilde --
        // plus z (already the exact centred representative, see the c_s_1
        // center_mod above); only the hint slot h is dropped (feature absent).
    }
    // [PAPER Alg.1] 13: end procedure
}

/// `sign` — Algorithm 1 Sign, random path: fresh 64-byte mask seed,
/// then the internal.  Upstream slot: `try_sign_with_rng` (lib.rs:287,
/// `rnd <- rng`); C twin: `base_sign` (basesig.c, itself <-> sign.c:206).
pub fn sign(
    m: &[u8],                      // paper M: message
    pk: &PublicKey,                // paper t: pk.t = t (public key)
    sk: &SecretKey,                // paper r: sk.r = r (secret key)
    pp: &PublicParams,             // paper A: pp = A = [I | A']
    rng: &mut impl CryptoRngCore,  // CSPRNG for the mask seed (no paper symbol)
) -> Signature {                   // paper σ: returns σ = (c, z)
    let mut rnd = [0u8; 64]; // PRG mask seed (<-> rnd, lib.rs try_sign_with_rng)
    rng.fill_bytes(&mut rnd);
    // ^[CHANGED] lib.rs:293-295 (inside try_sign_with_rng):
    //     let mut rnd = [0u8; 32];
    //     rng.try_fill_bytes(&mut rnd).map_err(|_| "Sign: Random number generator failed")?;
    // WHY: upstream draws a 32-byte rnd that feeds the rho_prime CRH chain
    // (ml_dsa.rs:199); here the 64-byte mask seed IS the randomness itself
    // (there is no CRH chain), so it is drawn fresh at full width.  The
    // deterministic analogue is `sign_det` below (seed derived from (sk, M)).
    let sigma = sign_internal(m, pk, sk, pp, &rnd);
    rnd.zeroize(); // mask seed: knowing it + sigma reveals c*r, hence r
    sigma
    // ^[REUSED] the try_sign_with_rng -> sign_internal delegation (lib.rs:287);
    // C twin basesig.c: return base_sign_internal(sigma, m, mlen, pk, sk, pp, seed);
}

/// `sign_det` — deterministic Sign (the KAT slot; no ml_dsa.rs analogue —
/// the counterpart of upstream's zeroed-rnd deterministic branch); C twin:
/// `base_sign_det` (basesig.c).  Mask randomness derived from (sk, M) via
/// `det_seed` (tag 0), then the same internal.  Same distribution and
/// validity as `sign`; removes the per-signature RNG (no nonce-reuse risk)
/// and makes the signature a reproducible function of (sk, M).
/// Definition 3: Sign belongs to Σ, so the deterministic slot lives HERE
/// (test_kat's ordinary-signature vectors call this).
pub fn sign_det(
    m: &[u8],           // paper M: message
    pk: &PublicKey,     // paper t: pk.t = t (public key)
    sk: &SecretKey,     // paper r: sk.r = r (secret key)
    pp: &PublicParams,  // paper A: pp = A = [I | A']
) -> Signature {        // paper σ: returns σ = (c, z)
    let mut seed = det_seed(sk, m); // PRG mask seed from (sk, M); tag 0 = sign
    let sigma = sign_internal(m, pk, sk, pp, &seed);
    seed.zeroize(); // sk-derived mask seed: wipe
    sigma
}

/// `verify_internal` <-> `verify_internal` (ml_dsa.rs:351); C twin:
/// `base_verify_internal` (basesig.c).  Algorithm 1 Verify body:
/// w' = A z - c t; accept iff c == H(pk, w', M).  The ONLY verifier a final
/// (ordinary or adapted) signature ever meets — a `PreSignature` cannot be
/// passed here (distinct type; PreVerify lives in `las`).
pub(crate) fn verify_internal(
    sigma: &Signature,  // paper σ: sigma = (c, z), signature to verify
    m: &[u8],           // paper M: message
    pk: &PublicKey,     // paper t: pk.t = t (public key)
    pp: &PublicParams,  // paper A: pp = A = [I | A']
) -> bool {
    // [PAPER Alg.1] 14: procedure Verify(pk, σ, M):
    //
    // 1: (ro, t_1) ← pkDecode(pk)  pull out pre-computed elements
    // 2: (c_tilde, z, h) ← sigDecode(σ)    ▷ Signer’s commitment hash c_tilde, response z and hint h
    // 3: if h = ⊥ then return false     ▷ Hint was not properly encoded
    // [DELETED] ml_dsa.rs:365-376:
    //     let PublicKey { rho, tr, t1_d2_hat_mont } = epk;
    //     let Ok((c_tilde, z, h)) ... = sig_decode(gamma1, omega, sig) else { return false; };
    //     let Some(h) = h else { return false };
    // WHY: pk and sig are structs (the validating byte decode is
    // serialize.rs / the packed tier below), and there is no hint h to
    // decode at all.

    // [PAPER Alg.1] 15:     Parse (c, z) := σ
    // [PAPER Alg.1] 16:     if ||z||∞ > γ − κ, then return 0
    if b_chknorm_vec(&sigma.z, BOUND_SIGN) {
        return false;
    }
    // ^[CHANGED] ml_dsa.rs:433-434 (upstream tests this at the END):
    //     let left = infinity_norm(&z) < (gamma1 - beta);
    // WHY: same norm gate at bound gamma - kappa (= gamma1 - beta with eta = 1),
    // moved to the top as the paper's Verify step 16 -- exactly where the C twin
    // tests it (mirroring sign.c:314).  The check is the EARLY-EXIT
    // b_chknorm_vec (matches C poly_chknorm and las.rs chknorm_vec), NOT the
    // full-scan upstream infinity_norm: identical decision, same per-call work
    // profile as the base C path and the LAS path (no extra center_mod/max),
    // so the PreVerify-vs-Verify benchmark overhead is measured fairly.

    // 7: 𝜇 ← (H(BytesToBits(tr)||𝑀′, 64))    ▷ Compute message representative µ
    let mut t_tilde = [0u8; N * D * 4]; // packed pk: plays mu's role as the fixed hash prefix
    b_w_encode(&pk.t, &mut t_tilde);
    // ^[CHANGED] ml_dsa.rs:391 (the 7b path):
    //     h256_xof(&[tr, &[0u8], &[ctx.len().to_le_bytes()[0]], ctx, m])
    // WHY: same reason as on the Sign side -- the oracle is c = H(pk, w', M)
    // with the raw public key, so the once-per-call step is packing t
    // canonically, not hashing it into mu.

    // 8: c ∈ 𝑅𝑞 ← SampleInBall(c_tilde_1)    ▷ Compute verifier’s challenge from c_tilde
    let c: R = b_sample_in_ball(&sigma.c_tilde);
    // ^[REUSED] ml_dsa.rs:400:
    //     let c: R = sample_in_ball::<false>(tau, &c_tilde);
    // WHY: the signature stores the challenge DIGEST c_tilde (upstream's c_tilde
    // lifecycle), so Verify re-derives the challenge polynomial locally right
    // here -- exactly as upstream sign.c:327 poly_challenge(&cp, sig) does after
    // unpacking.  Same SampleInBall construction, challenge weight kappa.

    // 5: cap_a_hat ← ExpandA(ρ)    ▷ A is generated and stored in NTT representation as cap_A_hat
    // 9: w′_Approx ← invNTT(cap_A_hat ◦ NTT(z) - NTT(c) ◦ NTT(t_1 · 2^d)    ▷ w′_Approx = Az − ct1·2^d
    // [PAPER Alg.1] 17:     w′ = A z − c t, where t := pk
    let w_prime: [R; N] = {
        // A z, identity block included (the same sequence as Sign's w = A y;
        // z = (z_0 || z_1), same split convention as r and y):
        let z_1: [R; ELL] = from_fn(|j| sigma.z[N + j].clone());
        let mut z_1_hat: [T; ELL] = ntt(&z_1); // [REUSED] ml_dsa.rs:407: let z_hat: [T; L] = ntt(&z);
        for p in z_1_hat.iter_mut() {
            for x in p.0.iter_mut() {
                *x = partial_reduce32(*x);
            }
        }
        let a_prime_z_1_hat: [T; N] = b_mat_vec_mul(&pp.a_prime, &z_1_hat); // A' z_1 (NTT domain)
        // ^[REUSED] ml_dsa.rs:408: let az_hat: [T; K] = mat_vec_mul(&cap_a_hat, &z_hat);
        let z_0: [R; N] = from_fn(|k| sigma.z[k].clone()); // identity-block prefix of z
        let a_z_not_reduced: [R; N] = add_vector_ntt(&inv_ntt(&a_prime_z_1_hat), &z_0);
        let a_z: [R; N] = from_fn(|k| R(from_fn(|n| full_reduce32(a_z_not_reduced[k].0[n]))));

        // c t, exact (against the RAW t):
        let c_hat: T = {
            let mut ch = ntt(&[c.clone()]);
            for x in ch[0].0.iter_mut() {
                *x = partial_reduce32(*x);
            }
            let [ch] = ch;
            ch
        };
        // ^[REUSED] ml_dsa.rs:410: let c_hat: &T = &ntt(&[c])[0];  (once per call)
        let t_hat_mont: [T; N] = {
            let mut t_hat: [T; N] = ntt(&pk.t);
            for p in t_hat.iter_mut() {
                for x in p.0.iter_mut() {
                    *x = partial_reduce32(*x);
                }
            }
            to_mont(&t_hat)
        };
        // ^[CHANGED] (the NTT(t_1 · 2^d) operand of) ml_dsa.rs:409, built in
        // key_gen_internal as t1_d2_hat_mont (ml_dsa.rs:108-113):
        //     let t1_hat_mont: [T; K] = to_mont(&ntt(&t_1));
        //     to_mont(&core::array::from_fn(|k| {
        //         T(core::array::from_fn(|n| mont_reduce(i64::from(t1_hat_mont[k].0[n]) << D)))
        //     }))
        // WHY: t was never compressed (no Power2Round), so the operand is the
        // EXACT t -- to_mont(&ntt(&t)) with no 2^d shift to restore -- and it
        // is built per call because struct keys carry no pre-computes.
        let c_t: [R; N] = {
            // paper c·t: named after its factors, like c_r on the Sign side.
            let c_t_hat: [T; N] = from_fn(|k| {
                T(from_fn(|n| {
                    mont_reduce(i64::from(c_hat.0[n]) * i64::from(t_hat_mont[k].0[n]))
                }))
            });
            let c_t = inv_ntt(&c_t_hat);
            from_fn(|k| R(from_fn(|n| center_mod(c_t[k].0[n]))))
        };
        // ^[CHANGED] ml_dsa.rs:411-416:
        //     inv_ntt(&core::array::from_fn(|k| {
        //         T(core::array::from_fn(|n| {
        //             az_hat[k].0[n]
        //                 - mont_reduce(i64::from(c_hat.0[n]) * i64::from(t1_d2_hat_mont[k].0[n]))
        //         }))
        //     }))
        // WHY: upstream subtracts c·t from Az entirely in the NTT domain and
        // inverse-transforms once; here the identity block of A = [I | A']
        // must add the NON-transformed z_0 to Az, which forces the
        // subtraction into the normal domain: invert both halves first (the
        // c·t product uses the same mont_reduce/inv_ntt/center_mod shape as
        // Sign's c_r block), then subtract canonically below.  w' is exact
        // either way.  Same structure as the C twin (base_verify_internal).
        from_fn(|k| R(from_fn(|n| full_reduce32(a_z[k].0[n] - c_t[k].0[n]))))
    };

    // 10: w′_1 ← UseHint(h, w′_Approx)    ▷ Reconstruction of signer’s commitment
    // [DELETED] ml_dsa.rs:420-422:
    //     let wp_1: [R; K] = core::array::from_fn(|k| {
    //         R(core::array::from_fn(|n| use_hint(gamma2, h[k].0[n], wp_approx[k].0[n])))
    //     });
    // WHY: w' is exact (no compression anywhere), so there are no high bits
    // to repair and no hint.

    // 12: c_tilde_′ ← H(µ || w1Encode(w′_1), λ/4)     ▷ Hash it; this should match c_tilde
    // [PAPER Alg.1] 18:     if c ≠ H(pk, w′, M), then return 0
    // [PAPER Alg.1] 19:     return 1
    let mut w_tilde = [0u8; N * D * 4];
    b_w_encode(&w_prime, &mut w_tilde);         // [REUSED] ml_dsa.rs:428: w1_encode::<K>(gamma2, &wp_1, &mut tmp);
    let mut h12 = h256_xof(&[&t_tilde[..], &w_tilde[..], m]);
    let mut c_tilde_check = [0u8; LAS_CTILDEBYTES]; // paper H(pk, w′, M): recomputed digest
    h12.read(&mut c_tilde_check);
    c_tilde_check == sigma.c_tilde
    // ^[REUSED] ml_dsa.rs:429-435 (minus the norm conjunct):
    //     let mut h12 = h256_xof(&[&mu, &tmp]);
    //     let mut c_tilde_p = [0u8; LAMBDA_DIV4];
    //     h12.read(&mut c_tilde_p);
    //     let left = infinity_norm(&z) < (gamma1 - beta);
    //     let right = c_tilde == c_tilde_p;
    //     left && right
    // WHY: accept iff the recomputed DIGEST equals the stored one -- a byte
    // compare, exactly upstream's `right = c_tilde == c_tilde_p`.  This restores
    // the upstream c_tilde lifecycle (the refactor's polynomial compare is gone);
    // it is strictly stronger than that polynomial compare and identical on
    // honest paths.  The norm half of upstream's conjunction was already tested
    // at the top (paper step 16).
    // [PAPER Alg.1] 20: end procedure
}

/// `verify` — Algorithm 1 Verify, public entry point (delegates to the
/// internal).  Upstream slot: `verify` (lib.rs:383); C twin:
/// `base_verify` (basesig.c, itself <-> sign.c:375).
/// THE verifier of the whole construction (Definition 3): ordinary
/// signatures AND Adapt outputs are checked here; miners/on-chain code see
/// nothing else.  Returns true iff the signature is valid.
pub fn verify(
    sigma: &Signature,  // paper σ: sigma = (c, z), signature to verify
    m: &[u8],           // paper M: message
    pk: &PublicKey,     // paper t: pk.t = t (public key)
    pp: &PublicParams,  // paper A: pp = A = [I | A']
) -> bool {
    verify_internal(sigma, m, pk, pp)
    // ^[REUSED] the verify -> verify_internal delegation (lib.rs:383);
    // C twin basesig.c: return base_verify_internal(sigma, m, mlen, pk, pp);
}

// [DELETED] `expand_private` (ml_dsa.rs:445) / `expand_public` (ml_dsa.rs:477) /
// `private_to_public_key` (ml_dsa.rs:502): byte-encoding expansion and key
// pre-computes.  The struct tier's keys have no byte encodings, so these
// slots have no analogue here -- their byte boundary is restored by the
// PACKED tier at the bottom of this file (which validates with
// serialize.rs, the packing.{c,h} twin).

/* ==================== helpers (local twins) ====================
 * Defined at the BOTTOM of the file (ml_dsa.rs keeps the originals in
 * hashing.rs / helpers.rs / encodings.rs; the C twin basesig.c keeps its
 * local copies at the bottom the same way, basesig.c:703-1087).
 * Behaviour-identical to las.rs's las_* copies, so A*r and the challenge
 * hash match las.rs bit-for-bit.  Each is a twin of exactly ONE NAMED
 * upstream function -- see the table at the top of the file. */

/*************************************************
 * b_rej_bounded_poly  <->  rej_bounded_poly (hashing.rs:158)
 * C twin: b_rej_S1 + b_poly_uniform_S1 (basesig.c:718, :748)
 *
 * Samples one poly with coefficients uniform in {-1,0,1} (set S_1, ternary):
 * same XOF-init -> squeeze -> reject-per-candidate shape.
 * [CHANGED] hashing.rs:168:
 *     let mut xof = h256_xof(rhos);
 * WHY: the seed layout is seed || nonce_le16 (= upstream expand_s's
 * rho || [r] || [0] bytes for r < 256), and the squeeze is block-buffered
 * (136-byte SHAKE256 blocks; 1-byte units never straddle a block, so the
 * stream is identical) to match the C byte semantics exactly.
 * [CHANGED] hashing.rs:177-180:
 *     let z0 = coeff_from_half_byte::<CTEST>(eta, z[0] & 0x0f);
 *     let z1 = coeff_from_half_byte::<CTEST>(eta, z[0] >> 4);
 * WHY: S_1 has 3 values, so the candidate unit is a 2-BIT code (four per
 * byte): {0,1,2} -> {-1,0,1}, reject 3.  eta-coded half-bytes cannot sample
 * {-1,0,1} tightly.
 *************************************************/
fn b_rej_bounded_poly(seed: &[u8; LAS_SEEDBYTES], nonce: u16) -> R {
    // 2: ctx ← H.Init()
    // 3: ctx ← H.Absorb(ctx, 𝜌)                 (hashing.rs:168, written out)
    let mut h = Shake256::default();
    h.update(seed);
    h.update(&nonce.to_le_bytes());
    let mut rd = h.finalize_xof();
    let mut buf = [0u8; SHAKE256_RATE];
    rd.read(&mut buf);
    let mut pos = 0usize;

    let mut a = R0;
    let mut j = 0usize;
    // 4: while j < 256 do                       (hashing.rs:171)
    while j < D {
        if pos >= SHAKE256_RATE {
            rd.read(&mut buf);
            pos = 0;
        }
        // 5: 𝑧 ← H.Squeeze(ctx, 1)              (hashing.rs:174)
        let z = buf[pos];
        pos += 1;
        // 6-15: the two half-byte candidates -> four 2-bit candidates
        let mut s = 0u8;
        while s < 4 && j < D {
            let v = (z >> (2 * s)) & 3;
            if v < 3 {
                a.0[j] = i32::from(v) - 1;
                j += 1;
            }
            s += 1;
        }
    }
    // 17: return a
    a
}

/*************************************************
 * b_expand_s  <->  expand_s (hashing.rs:252)
 * C twin: b_polyvecm_uniform_S1 (basesig.c:958)
 *
 * Samples the secret vector r <- S_1^{n+l}.
 * [CHANGED] hashing.rs:259-266:
 *     let s1: [R; L] =
 *         core::array::from_fn(|r| rej_bounded_poly::<CTEST>(eta, &[rho, &[r as u8], &[0]]));
 *     let s2: [R; K] =
 *         core::array::from_fn(|r| rej_bounded_poly::<CTEST>(eta, &[rho, &[(r + L) as u8], &[0]]));
 * WHY: ONE ternary vector of m = n+ell polys (there is no separate error
 * vector s_2 -- the identity block of A = [I | A'] gives the top n components
 * of r that role).  Same per-poly nonce derivation: nonce j as two
 * little-endian bytes = upstream's [r as u8], [0].
 *************************************************/
fn b_expand_s(seed: &[u8; LAS_SEEDBYTES]) -> [R; N_PLUS_ELL] {
    from_fn(|j| b_rej_bounded_poly(seed, j as u16))
}

/*************************************************
 * b_expand_mask  <->  expand_mask (hashing.rs:281)
 * C twin: b_polyvecm_uniform_Sgamma + b_poly_uniform_Sgamma + b_rej_Sgamma
 * (basesig.c:967, :825, :786)
 *
 * Samples the mask vector y <- S_gamma^{n+l} (uniform [-gamma, gamma]).
 * Same vector shape, same per-poly nonce derivation (n = mu + r,
 * hashing.rs:293) and the same per-poly XOF seed layout (seed || n_le16,
 * hashing.rs:296).
 * [CHANGED] hashing.rs:286:
 *     let c = 1 + bit_length(gamma1 - 1); // c will either be 18 or 20
 * WHY: upstream's gamma_1 is a power of two, so a fixed-length stream can be
 * BIT-UNPACKED with no rejections; the paper's gamma = kappa*d*(n+l) is not,
 * so the window is the smallest 2^k - 1 >= 2*gamma and candidates are
 * rejection-sampled.
 * [CHANGED] hashing.rs:296-301:
 *     let mut xof = h256_xof(&[rho, &n.to_le_bytes()]);
 *     xof.read(&mut v);
 *     y[r as usize] =
 *         bit_unpack(&v[0..32 * c], gamma1 - 1, gamma1).expect("Alg 34: try_from2 fail");
 * WHY: bit_unpack -> 3-bytes-per-candidate rejection loop: accept t <
 * 2*gamma+1, value t - gamma.  Block-buffered squeeze (136-byte blocks) with
 * refill at pos+3 > RATE: the LAST byte of every SHAKE256 block is DISCARDED,
 * matching the C sampler byte-for-byte (basesig.c:797:
 * `while(ctr < len && pos + 3 <= buflen)`, 136 = 45*3 + 1).
 *************************************************/
fn b_expand_mask(rho: &[u8; 64], mu: u16) -> [R; N_PLUS_ELL] {
    let two_gamma = 2u32 * (GAMMA as u32);
    let mut gmask: u32 = 1; // smallest 2^k - 1 >= 2*gamma (the acceptance window)
    while gmask < two_gamma {
        gmask <<= 1;
    }
    gmask -= 1;

    let mut y = [R0; N_PLUS_ELL];
    // 2: for r from 0 to ℓ − 1 do               (hashing.rs:290; ell -> m)
    for r in 0..N_PLUS_ELL {
        // 3: rho′ ← rho || IntegerToBytes(mu + r, 2)   (hashing.rs:293)
        let n = mu.wrapping_add(r as u16);
        // 4: v ← H(rho′, 32*c)                  (hashing.rs:296-297, block-buffered)
        let mut h = Shake256::default();
        h.update(rho);
        h.update(&n.to_le_bytes());
        let mut rd = h.finalize_xof();
        let mut buf = [0u8; SHAKE256_RATE];
        rd.read(&mut buf);
        let mut pos = 0usize;

        // 5: y[r] ← BitUnpack(v, γ_1 − 1, γ_1)  -> rejection loop (see WHY above)
        let mut ctr = 0usize;
        while ctr < D {
            if pos + 3 > SHAKE256_RATE {
                rd.read(&mut buf);
                pos = 0;
            }
            let t = (u32::from(buf[pos])
                | (u32::from(buf[pos + 1]) << 8)
                | (u32::from(buf[pos + 2]) << 16))
                & gmask;
            pos += 3;
            if t < two_gamma + 1 {
                y[r].0[ctr] = (t as i32) - GAMMA;
                ctr += 1;
            }
        }
        // 6: end for
    }
    // 7: return y
    y
}

/*************************************************
 * b_sample_in_ball  <->  sample_in_ball (hashing.rs:43)
 * C twin: b_poly_challenge (basesig.c:860)
 *
 * Implementation of H: samples the challenge polynomial with KAPPA
 * nonzero coefficients in {-1,1} from SHAKE256(seed).  Same Fisher-Yates
 * construction (sign bits from the first 8 squeezed bytes, then the swap
 * loop with rejected indices).
 * [CHANGED] hashing.rs:43-44:
 *     pub(crate) fn sample_in_ball<const CTEST: bool>(tau: i32, rho: &[u8]) -> R {
 *     let tau = usize::try_from(tau).expect("Alg 29: try_from fail");
 * WHY: the challenge weight is the paper's kappa (per parameter set) instead
 * of tau, and the input is the fixed 32-byte challenge-seed digest.
 * [CHANGED] hashing.rs:62-64:
 *     let mut j = [i.to_le_bytes()[0]]; // remove timing variability
 *     if !CTEST { h_ctx.read(&mut j); };
 * WHY: block-buffered squeeze (136-byte SHAKE256 blocks; 1-byte units never
 * straddle a block, so the stream is identical -- the C byte semantics) and
 * no CTEST path (this build takes no constant-time measurements).
 *************************************************/
fn b_sample_in_ball(rho: &[u8; LAS_CTILDEBYTES]) -> R {
    // 1: c ← 0                                  (hashing.rs:47)
    let mut c = R0;

    // 2: ctx ← H.Init()
    // 3: ctx ← H.Absorb(ctx, 𝜌)                 (hashing.rs:51)
    let mut h = Shake256::default();
    h.update(rho);
    let mut rd = h.finalize_xof();
    let mut buf = [0u8; SHAKE256_RATE];
    rd.read(&mut buf);

    // 4: (ctx, 𝑠) ← H.Squeeze(ctx, 8)
    // 5: ℎ ← BytesToBits(𝑠)                     (hashing.rs:55-56)
    let mut signs = u64::from_le_bytes(buf[0..8].try_into().unwrap());
    let mut pos = 8usize;

    // 6: for 𝑖 from 256 − 𝜏 to 255 do           (hashing.rs:59; tau -> kappa)
    for i in (D - KAPPA as usize)..D {
        // 7: (ctx, 𝑗) ← H.Squeeze(ctx, 1)
        // 8: while 𝑗 > 𝑖 do
        // 9:   (ctx, 𝑗) ← H.Squeeze(ctx, 1)
        // 10: end while                         (hashing.rs:61-74, block-buffered)
        let j = loop {
            if pos >= SHAKE256_RATE {
                rd.read(&mut buf);
                pos = 0;
            }
            let j = buf[pos] as usize;
            pos += 1;
            if j <= i {
                break j;
            }
        };
        // 11: ci ← cj                           (hashing.rs:77: c.0[i] = c.0[usize::from(j[0])];)
        c.0[i] = c.0[j];
        // 12: c_j ← (−1)^{H(ρ)[i+τ−256]         (hashing.rs:80-83; sign bits pre-read above)
        c.0[j] = 1 - 2 * ((signs & 1) as i32);
        signs >>= 1;
        // 13: end for
    }
    // 14: return c
    c
}

/*************************************************
 * b_w_encode  <->  w1_encode (encodings.rs:338)
 * C twin: b_polyvecn_pack_w + b_polyw_pack (basesig.c:1082, :905)
 * (Named WITHOUT w1's '1', exactly as the C twins drop it from polyw1_pack /
 * polyveck_pack_w1: Algorithm 1 deleted HighBits, so this packs the FULL w
 * -- and the pk t, same layout -- never a high-bits w_1.)
 *
 * Packs a commitment vector (or the public key t -- same layout) for
 * hashing: every coefficient canonicalised to [0,Q) and stored as 4 bytes
 * little-endian.  Same out-parameter shape as w1_encode.
 * [CHANGED] vs w1_encode (which packs bitlen((q-1)/(2*gamma2)-1)-bit w_1
 * HIGH-BIT codes via simple_bit_pack):
 * WHY: Algorithm 1 deleted HighBits, so the oracle binds the FULL w -- the
 * 23-bit canonical coefficients need 4 bytes each (the full_reduce32
 * canonicalisation is an identity for already-canonical callers, and makes
 * the packing representative-neutral for the rest).
 *************************************************/
fn b_w_encode(w1: &[R; N], w1_tilde: &mut [u8]) {
    for (k, p) in w1.iter().enumerate() {
        for (n, &coeff) in p.0.iter().enumerate() {
            let x = full_reduce32(coeff) as u32;
            w1_tilde[4 * (k * D + n)..4 * (k * D + n) + 4].copy_from_slice(&x.to_le_bytes());
        }
    }
}

/*************************************************
 * b_mat_vec_mul  <->  mat_vec_mul (helpers.rs:100)
 * C twin: b_polyvec_matrix_pointwise_montgomery (basesig.c:927)
 *
 * Matrix-by-vector multiplication over the NTT domain: w_hat = A'_hat o
 * u_hat.  Body verbatim from mat_vec_mul (same to_mont pre-compute, same
 * mont_reduce accumulation), dimensions K,L -> n,l (u spans only the l
 * columns of A' because A = [I | A']; the identity block is added by the
 * callers after the inverse transform).
 * [CHANGED] one extra line vs helpers.rs:100-114:
 *     (after each row accumulation)  *x = partial_reduce32(*x);
 * WHY: reduce each accumulated row to (-Q,Q) before the inverse NTT (mirrors
 * the C poly_reduce placement, basesig.c:168/:311) so the transform's i32
 * arithmetic stays in the reduced domain; representative-neutral (callers
 * canonicalise with full_reduce32).
 *************************************************/
fn b_mat_vec_mul(a_hat: &[[T; ELL]; N], u_hat: &[T; ELL]) -> [T; N] {
    let mut w_hat = [T0; N];
    let u_hat_mont = to_mont(u_hat);             // [REUSED] helpers.rs:104: let u_hat_mont = to_mont(u_hat);
    for i in 0..N {
        for j in 0..ELL {
            w_hat[i].0.iter_mut().enumerate().for_each(|(n, e)| {
                *e += mont_reduce(i64::from(a_hat[i][j].0[n]) * i64::from(u_hat_mont[j].0[n]));
            });                                  // [REUSED] helpers.rs:107-110, verbatim
        }
        for x in w_hat[i].0.iter_mut() {
            *x = partial_reduce32(*x);           // [CHANGED] see WHY above
        }
    }
    w_hat
}

/// Deterministic per-signature mask randomness for the BASE path:
/// seed = SHAKE256(tag=0 || sk || M), 64 bytes.  Private tag-0-ONLY twin of
/// las.rs's `det_seed` (which also handles tag 1 = presign, binding the
/// statement Y); the base signature has no statement to bind, so this variant
/// is simpler.  Duplicated here per this file's no-las-dependency rule,
/// exactly as the C build keeps a `static det_seed` in both basesig.c and
/// las.c.  Byte-identical to las.rs's `det_seed(0, sk, None, m)`, so
/// `sign_det` reproduces the KAT's ordinary-signature vectors unchanged.
/// LAS-only helper (the _det KAT path); no ml_dsa.rs analogue.
fn det_seed(sk: &SecretKey, m: &[u8]) -> [u8; 64] {
    let mut h = Shake256::default();
    h.update(&[0u8]); // domain: 0 = sign (base path; no statement)

    // ternary sk -> 1 byte/coeff, (uint8_t)(int8_t) semantics: -1 -> 0xFF
    let mut skb = [0u8; N_PLUS_ELL * D];
    for i in 0..N_PLUS_ELL {
        for k in 0..D {
            skb[i * D + k] = sk.r[i].0[k] as i8 as u8;
        }
    }
    h.update(&skb);
    h.update(m);
    skb.zeroize(); // raw sk bytes: wipe (upstream secret-material policy)

    let mut out = [0u8; 64];
    h.finalize_xof().read(&mut out);
    out
}

/* ============== end-to-end PACKED-API tier (bytes in/out) ==============
 * The SECOND measured boundary, mirroring what upstream's ONLY boundary is:
 * ml_dsa.rs signs INTO bytes (sig_encode, ml_dsa.rs:336), verifies FROM
 * bytes (sig: &[u8; SIG_LEN] + sig_decode, ml_dsa.rs:368) and expands byte
 * keys (expand_private / expand_public, ml_dsa.rs:445/477 -- the very slots
 * marked [DELETED] in the struct tier above).  The struct functions above
 * are the CORE CRYPTO tier; these end-to-end twins unpack the byte keys
 * (validating; malformed -> None/false), run the core, and pack the outputs,
 * using the shared codec serialize.rs.  Same argument positions as the
 * struct twin, byte buffers in place of structs.  C twin: the basesig.c
 * packed tier (basesig.c:1089-1194). */

/// `keygen_packed` (end-to-end tier of `keygen`); C twin:
/// `base_keygen_packed` (basesig.c).  KeyGen at the byte
/// boundary -- run the core KeyGen, then pack both keys inside the call.
/// [REUSED] ml_dsa.rs:100: `pk_encode::<K, PK_LEN>(&rho, &t_1)` -- the slot
/// where upstream's KeyGen packs.  A freshly sampled sk is always ternary,
/// so packing cannot fail.
pub fn keygen_packed(
    pp: &PublicParams,             // paper A: pp = A = [I | A']
    rng: &mut impl CryptoRngCore,  // CSPRNG for the seed (no paper symbol)
) -> ([u8; PUBLIC_KEY_BYTES], [u8; SECRET_KEY_BYTES]) {
    let (pk, sk) = keygen(pp, rng);
    let pk_b = pack_public_key(&pk);
    let sk_b = pack_secret_key(&sk).expect("freshly sampled sk is ternary");
    (pk_b, sk_b)
}

/// `sign_packed` (end-to-end tier of `sign`); C twin:
/// `base_sign_packed` (basesig.c).  Sign at the byte
/// boundary -- unpack the keys (validating), run the core Sign, pack the
/// signature, all inside the call.
/// [REUSED] ml_dsa.rs:166 (`skDecode`, resolved via expand_private) and
/// ml_dsa.rs:336 (`sig_encode`).  WHY the pk is unpacked too (upstream only
/// decodes sk): the paper's oracle is c = H(pk, w, M) with the raw public
/// key, while upstream binds the key digest tr, which travels INSIDE its
/// packed sk.  Returns None if a key fails validating decode.
pub fn sign_packed(
    m: &[u8],                          // paper M: message
    pk_b: &[u8; PUBLIC_KEY_BYTES],     // packed public key (bytes)
    sk_b: &[u8; SECRET_KEY_BYTES],     // packed secret key (bytes)
    pp: &PublicParams,                 // paper A: pp = A = [I | A']
    rng: &mut impl CryptoRngCore,      // CSPRNG for the mask seed
) -> Option<[u8; SIGNATURE_BYTES]> {
    let pk = unpack_public_key(pk_b)?;
    let sk = unpack_secret_key(sk_b)?;
    let sigma = sign(m, &pk, &sk, pp, rng);
    pack_signature(&sigma) // in-band by the norm gate: always Some
}

/// `verify_packed` (end-to-end tier of `verify`); C twin:
/// `base_verify_packed` (basesig.c).  Verify at the byte
/// boundary -- validating decode of pk and signature, then the core Verify,
/// all inside the call.
/// [REUSED] ml_dsa.rs:365 (`pkDecode`, resolved via expand_public) and
/// ml_dsa.rs:368-372 (`sig_decode`, whose decode failure returns false --
/// the same defensive stance an on-chain verifier must take).  Returns true
/// iff the bytes decode AND the signature verifies.
pub fn verify_packed(
    sig_b: &[u8; SIGNATURE_BYTES],  // packed signature (bytes)
    m: &[u8],                       // paper M: message
    pk_b: &[u8; PUBLIC_KEY_BYTES],  // packed public key (bytes)
    pp: &PublicParams,              // paper A: pp = A = [I | A']
) -> bool {
    let Some(pk) = unpack_public_key(pk_b) else {
        return false; // malformed pk
    };
    let Some(sigma) = unpack_signature(sig_b) else {
        return false; // malformed sig
    };
    verify(&sigma, m, &pk, pp)
}
