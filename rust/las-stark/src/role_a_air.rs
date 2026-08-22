//! **Role-A relation as a succinct post-quantum proof.**
//!
//! A succinct argument of knowledge for the SAME statement configuration 2
//! (Groth16) and configuration 3 (LaZer) prove for the Fig. 1 atomic swap.
//! (Succinct and post-quantum -- but NOT zero-knowledge as built; see "Honest
//! scope" at the end of this note.)
//!
//! ```text
//!     exists r : A r = t'   and   ||r||inf <= 1,      A = [I_n | A'].
//! ```
//!
//! ## Why this module exists
//!
//! The amortisation experiment closed the "make the existing proof cheaper"
//! direction for both deployed provers (see
//! `docs/03-results/PROOF_AMORTISATION_EXPERIMENT.md`): Groth16's proof is a
//! constant 128 B but was never the bottleneck, and LaZer's ~30 KiB does shrink
//! under batching but only by making per-swap proving and verification 3.3x
//! worse. What that leaves is not a cheaper *instance* of either system but a
//! different *kind* of system: one that is **post-quantum** (which Groth16 is
//! not) **and** succinct (which LaZer is not -- its proof is linear-ish in the
//! statement, not polylogarithmic).
//!
//! A FRI-STARK is the natural candidate: transparent (no trusted setup, unlike
//! Groth16), post-quantum (soundness rests on the collision resistance of
//! Blake3 and the conjectured soundness of low-degree testing in the ROM -- no
//! pairings, no elliptic curves), and succinct.
//!
//! ## Relationship to `relation_air`
//!
//! [`crate::relation_air`] proves the arithmetic core of `base_verify`:
//! `w' = z_top + A'*z_bot - c*t` with `||z||inf <= B`. The role-A relation is
//! the SAME shape with `c = 0`, `w' := t'`, `z := r`, and the norm bound
//! tightened from `B = 137935` all the way down to **ternary**. This module
//! therefore reuses that module's architecture verbatim -- the narrow
//! random-evaluation (Schwartz-Zippel) argument over Winterfell's auxiliary
//! trace segment, which is what gets a `d = 256` relation past Winterfell's
//! 255-column cap -- and specialises three things:
//!
//! 1. **No challenge.** `c` and the public key `t` disappear with the `c*t`
//!    term, removing two aux columns, one accumulator constraint and two
//!    periodic columns.
//! 2. **Ternary instead of a range check.** `||r||inf <= 1` is enforced by
//!    `r^3 = r` (two constraints, one extra column) rather than the two-sided
//!    19-bit decomposition `relation_air` needs for the loose bound `B`. Over
//!    `F_p` the cubic `X^3 - X = X(X-1)(X+1)` has at most three roots, so
//!    `r^3 = r` holds **iff** `r` is in `{-1, 0, 1}` -- exact, not an
//!    approximation. This is also literally the constraint the Groth16 circuit
//!    uses (`rust/las-swap/src/groth16_circuit.rs`), so the two provers are
//!    demonstrably proving the same predicate.
//! 3. **Retuned quotient ranges.** A ternary witness makes `P_m` much smaller,
//!    so `h`/`g` need far fewer bits (see the soundness note below).
//!
//! Net effect on the trace: main width 135 -> 63, aux width 15 -> 13.
//!
//! ## The argument, and why the range checks are load-bearing
//!
//! Over the integers, for each output `m`:
//!
//! ```text
//!     P_m := r_top[m] + sum_j A'[m][j]*r_bot[j] - t'_m
//!          = (X^d + 1)*h_m + q*g_m
//! ```
//!
//! where `h_m` is the negacyclic-reduction quotient and `g_m` the mod-`q` one.
//! The AIR commits to `r`, `h`, `g`, then the auxiliary segment -- whose
//! randomness is drawn AFTER the main commitment -- Horner-evaluates everything
//! at a random `x` and checks
//! `sum_m rho_m [ P_m(x) - (x^d+1) h_m(x) - q g_m(x) ] == 0`.
//!
//! That is an identity in `F_p`. It lifts to `Z` -- which is what makes it mean
//! the relation -- only because every committed coefficient is range-checked:
//! `||r||inf <= 1`, `|h| < 2^33`, `|g| < 2^12`. With those,
//! `|P_m| <= ell*d*(q-1)/2 + (q-1)/2 + 1 = 5367656449 < 2^33`, so every
//! coefficient of the difference is bounded by
//! `2^33 + 2*2^33 + q*2^12 < 2^36 << p/2 ~ 2^63`.
//! **WITHOUT a bound on `g` the argument is VACUOUS**: `q` is invertible mod
//! `p`, so an unbounded `g` satisfies the equation for ANY claimed `t'`.
//!
//! `r_bot[j](x)` is evaluated ONCE and reused by all `n` outputs, so the `n`
//! equations are bound to one and the same `r` by construction.
//!
//! ## Honest scope
//!
//! This proves the role-A relation itself -- the whole statement, not a
//! fragment. Unlike [`crate::relation_air`] there is no missing Fiat-Shamir
//! chain here, because the role-A relation contains no hash: it is a pure
//! lattice-preimage claim, which is exactly why it is the tractable target.
//!
//! What it is NOT: reviewed cryptography. The parameters are Winterfell's
//! defaults via [`crate::proof_options`] and no concrete-security analysis of
//! this AIR has been done, so treat the measurement as an engineering data
//! point, not a security claim. Zero-knowledge in particular is NOT argued
//! here: Winterfell's prover does not add the zero-knowledge randomisation a
//! deployed use would need, so the size/time figures are for a succinct
//! ARGUMENT of knowledge, not a zk one.

use winterfell::{
    crypto::{hashers::Blake3_256, DefaultRandomCoin, MerkleTree},
    math::{fields::f64::BaseElement, ExtensionOf, FieldElement, ToElements},
    matrix::ColMatrix,
    Air, AirContext, Assertion, AuxRandElements, CompositionPoly, CompositionPolyTrace,
    ConstraintCompositionCoefficients, DefaultConstraintCommitment, DefaultConstraintEvaluator,
    DefaultTraceLde, EvaluationFrame, PartitionOptions, Proof, ProofOptions, Prover, StarkDomain,
    Trace, TraceInfo, TracePolyTable, TransitionConstraintDegree,
};

use crate::params::{D, ELL, N, N_PLUS_ELL, Q};
use crate::relation::negacyclic_conv;
use crate::vectors::{centred, Poly, VerifyVector};

// ---------------------------------------------------------------------------
// shape
// ---------------------------------------------------------------------------

/// Rows per Horner pass = the ring degree `d` (one coefficient per row).
pub const PASS_ROWS: usize = D; // 256
/// Number of passes in the trace (power of two; 12 are live, 4 are padding).
pub const N_PASSES: usize = 16;
/// Trace length.
pub const EV_LEN: usize = PASS_ROWS * N_PASSES; // 4096
/// Live passes 0..11 get a one-hot selector column.
pub const N_SEL: usize = 12;
/// The pass in which the public part of the relation is accumulated. It must
/// come after every `r_bot` pass (0..4).
pub const PASS_PUB: usize = 11;

// Quotient range widths, sized for a TERNARY witness (see the module note).
// Both bounds are powers of two, so ONE decomposition per value suffices
// (`u = v + 2^(k-1)` in `[0, 2^k)` <=> `v` in `[-2^(k-1), 2^(k-1))`).
// The bounds are DERIVED, not chosen: with `A'` centred (`|A'| <= (q-1)/2`) and
// `r` ternary, every coefficient of `P_m` is at most
// `ell*d*(q-1)/2 + (q-1)/2 + 1 = 5367656449 < 2^33`, so `|h| < 2^33`; and
// `|g| <= 2|P|/q = 1280 < 2^12`. `RoleAWitness::build` re-checks both per
// instance and refuses rather than proving something the AIR cannot bound.
/// Bits of the shifted `h` coefficient; bound `|h| < 2^33`.
pub const H_BITS: usize = 34;
const H_SHIFT: i128 = 1 << 33;
/// Bits of the shifted `g` coefficient; bound `|g| < 2^12`.
pub const G_BITS: usize = 13;
const G_SHIFT: i128 = 1 << 12;

// --- main column layout ----------------------------------------------------
const R_VAL: usize = 0;
const R_SQ: usize = 1;
const H_VAL: usize = 2;
const H_U_OFF: usize = H_VAL + 1;
const G_VAL: usize = H_U_OFF + H_BITS;
const G_U_OFF: usize = G_VAL + 1;
const SEL_OFF: usize = G_U_OFF + G_BITS;
/// Total main-segment width (63).
pub const MAIN_WIDTH: usize = SEL_OFF + N_SEL;

// --- aux column layout (all over the extension field E) --------------------
const A_ACC_R: usize = 0; // Horner accumulator of the r slot
const A_ACC_H: usize = 1; // Horner accumulator of the h slot
const A_ACC_G: usize = 2; // Horner accumulator of the g slot
const A_RB: usize = 3; // ELL latches: r_bot[j](x)
const A_RT: usize = A_RB + ELL; // latch: sum_m rho_m * r_top[m](x)
const A_HC: usize = A_RT + 1; // latch: sum_m rho_m * h_m(x)
const A_GC: usize = A_HC + 1; // latch: sum_m rho_m * g_m(x)
const A_PUB: usize = A_GC + 1; // sum_m rho_m * (A'.r_bot - t')(x)
const A_R: usize = A_PUB + 1; // the combined residual; asserted 0 on the last row
/// Total auxiliary-segment width (13).
pub const AUX_WIDTH: usize = A_R + 1;

/// Random elements drawn after the main commitment: `x`, then `rho_0..rho_{n-1}`.
pub const N_AUX_RAND: usize = 1 + N;

// --- periodic column layout (public data; costs no trace width) ------------
const P_ISB: usize = 0; // 1 on the last row of every pass
const P_A: usize = 1; // n*ell columns: A'[m][j] at P_A + m*ELL + j
const P_T: usize = P_A + N * ELL; // n columns: t'[m] (the statement)
const N_PERIODIC: usize = P_T + N; // 37

// --- main transition-constraint layout -------------------------------------
const C_RSQ: usize = 0;
const C_RTER: usize = 1;
const C_HBOOL: usize = 2;
const C_HREC: usize = C_HBOOL + H_BITS;
const C_GBOOL: usize = C_HREC + 1;
const C_GREC: usize = C_GBOOL + G_BITS;
const C_SEL: usize = C_GREC + 1;
const N_MAIN_CONSTRAINTS: usize = C_SEL + N_SEL;

// --- aux transition-constraint layout --------------------------------------
const K_ACC_R: usize = 0;
const K_ACC_H: usize = 1;
const K_ACC_G: usize = 2;
const K_RB: usize = 3; // ELL constraints
const K_RT: usize = K_RB + ELL;
const K_HC: usize = K_RT + 1;
const K_GC: usize = K_HC + 1;
const K_PUB: usize = K_GC + 1;
const K_R: usize = K_PUB + 1;
const N_AUX_CONSTRAINTS: usize = K_R + 1;

// Compile-time sanity on the shape the soundness argument assumes.
const _: () = {
    assert!(MAIN_WIDTH + AUX_WIDTH <= 255); // Winterfell TraceInfo::MAX_TRACE_WIDTH
    assert!(EV_LEN.is_power_of_two());
    assert!(N_SEL <= N_PASSES);
    assert!(N_PLUS_ELL <= N_SEL); // the r slot needs one pass per r polynomial
    assert!(N <= N_SEL); // the h/g slots need one pass per output polynomial
    assert!(PASS_PUB >= ELL); // r_bot latches must be final before the public pass
    assert!(PASS_PUB < N_SEL);
};

// ---------------------------------------------------------------------------
// small helpers
// ---------------------------------------------------------------------------

/// Signed integer -> field element (negatives via field negation).
#[inline]
fn fe(v: i128) -> BaseElement {
    if v >= 0 {
        BaseElement::new(v as u64)
    } else {
        -BaseElement::new((-v) as u64)
    }
}

/// `x^d` by repeated squaring (`d = 256 = 2^8`).
#[inline]
fn pow_d<E: FieldElement>(x: E) -> E {
    debug_assert!(D.is_power_of_two());
    let mut v = x;
    let mut e = D.trailing_zeros();
    while e > 0 {
        v = v * v;
        e -= 1;
    }
    v
}

/// Plain (NON-negacyclic) integer product; output has `2*D - 1` coefficients.
fn plain_mul(a: &[i128; D], b: &[i128; D]) -> Vec<i128> {
    let mut out = vec![0i128; 2 * D - 1];
    for (i, &ai) in a.iter().enumerate() {
        if ai == 0 {
            continue;
        }
        for (j, &bj) in b.iter().enumerate() {
            out[i + j] += ai * bj;
        }
    }
    out
}

#[inline]
fn centred_poly(p: &Poly) -> [i128; D] {
    let mut out = [0i128; D];
    for k in 0..D {
        out[k] = centred(p[k]) as i128;
    }
    out
}

// ---------------------------------------------------------------------------
// instance
// ---------------------------------------------------------------------------

/// A role-A instance: the public `(A', t')` and the private ternary `r`.
///
/// Polynomials are in the canonical `[0, q)` representation the rest of the
/// crate uses; the AIR centres them.
pub struct RoleAInstance {
    pub a_prime: Vec<Vec<Poly>>,
    pub t_prime: Vec<Poly>,
    pub r: Vec<Poly>,
}

/// Deterministic ternary sampler.
///
/// Mirrors the ENCODING of the C sampler `relation_rej_S1` (two-bit codes, code
/// 3 rejected, so the result is uniform on `{-1,0,1}`) but is driven by a local
/// xorshift rather than SHAKE256: this produces a benchmark instance, not a
/// KAT, and nothing downstream compares it against the C build.
fn sample_ternary(state: &mut u64) -> Poly {
    let mut out = [0i64; D];
    let mut k = 0;
    while k < D {
        // xorshift64*
        *state ^= *state >> 12;
        *state ^= *state << 25;
        *state ^= *state >> 27;
        let mut bits = state.wrapping_mul(0x2545_F491_4F6C_DD1D);
        for _ in 0..32 {
            if k == D {
                break;
            }
            let code = (bits & 3) as i64;
            bits >>= 2;
            if code < 3 {
                // 0,1,2 -> -1,0,1, stored canonically
                out[k] = match code - 1 {
                    -1 => Q - 1,
                    v => v,
                };
                k += 1;
            }
        }
    }
    out
}

impl RoleAInstance {
    /// Build an instance from a golden vector's `A'` and a fresh ternary `r`.
    ///
    /// `A'` is the REAL matrix of the C build (loaded from the golden vectors),
    /// so the proof is about the deployed parameter set; `r` is sampled here and
    /// `t' = A r` computed from it, which is exactly what `relation_gen` does.
    pub fn from_vector(vv: &VerifyVector, seed: u64) -> Self {
        let mut state = seed | 1;
        let r: Vec<Poly> = (0..N_PLUS_ELL).map(|_| sample_ternary(&mut state)).collect();

        // t'_m = r_top[m] + sum_j A'[m][j] * r_bot[j]  in R_q
        let mut t_prime = Vec::with_capacity(N);
        for m in 0..N {
            let mut acc = r[m]; // r_top[m]
            for j in 0..ELL {
                let prod = negacyclic_conv(&vv.a_prime[m][j], &r[N + j]);
                for k in 0..D {
                    acc[k] = (acc[k] + prod[k]) % Q;
                }
            }
            for k in 0..D {
                if acc[k] < 0 {
                    acc[k] += Q;
                }
            }
            t_prime.push(acc);
        }

        Self { a_prime: vv.a_prime.clone(), t_prime, r }
    }
}

// ---------------------------------------------------------------------------
// public inputs
// ---------------------------------------------------------------------------

/// The public side of the role-A relation, in CENTRED representation (the
/// centring is what keeps the integer bound on `P_m` -- and hence on `h`/`g` --
/// tight).
#[derive(Clone)]
pub struct RoleAPublicInputs {
    /// `A'` row-major `[m][j]`, `n x ell`.
    pub a_prime: Vec<Vec<[i128; D]>>,
    /// The statement `t' = A r` (`n` polynomials).
    pub t_prime: Vec<[i128; D]>,
}

impl RoleAPublicInputs {
    pub fn from_instance(inst: &RoleAInstance) -> Self {
        Self {
            a_prime: inst
                .a_prime
                .iter()
                .map(|row| row.iter().map(centred_poly).collect())
                .collect(),
            t_prime: inst.t_prime.iter().map(centred_poly).collect(),
        }
    }
}

impl ToElements<BaseElement> for RoleAPublicInputs {
    fn to_elements(&self) -> Vec<BaseElement> {
        let mut v = Vec::with_capacity((N * ELL + N) * D);
        for row in &self.a_prime {
            for p in row {
                v.extend(p.iter().map(|&x| fe(x)));
            }
        }
        for p in &self.t_prime {
            v.extend(p.iter().map(|&x| fe(x)));
        }
        v
    }
}

// ---------------------------------------------------------------------------
// witness
// ---------------------------------------------------------------------------

/// The private side: the centred ternary `r` plus the quotient polynomials
/// `h_m` (by `X^d+1`) and `g_m` (by `q`) that witness `P_m == 0` in `R_q`.
pub struct RoleAWitness {
    /// Centred `r`, `n+ell` polynomials (`r_top` then `r_bot`, as in the vector).
    pub r: Vec<[i128; D]>,
    /// `h_m`, `n` polynomials; `h_m[d-1]` is always 0 (deg <= d-2).
    pub h: Vec<[i128; D]>,
    /// `g_m`, `n` polynomials (deg <= d-1).
    pub g: Vec<[i128; D]>,
}

impl RoleAWitness {
    /// Derive the witness. Fails if the relation does not hold (the `q`-division
    /// would not be exact), if `r` is not ternary, or if a quotient coefficient
    /// escapes the range the AIR enforces.
    pub fn build(inst: &RoleAInstance, pi: &RoleAPublicInputs) -> Result<Self, String> {
        let r: Vec<[i128; D]> = inst.r.iter().map(centred_poly).collect();

        if r.iter().any(|p| p.iter().any(|&v| !(-1..=1).contains(&v))) {
            return Err("||r||inf > 1: the witness is not ternary".into());
        }

        let r_top = &r[0..N];
        let r_bot = &r[N..N_PLUS_ELL];

        let mut h = Vec::with_capacity(N);
        let mut g = Vec::with_capacity(N);
        for m in 0..N {
            // P_m over Z, degree <= 2d-2
            let mut p = vec![0i128; 2 * D - 1];
            for k in 0..D {
                p[k] += r_top[m][k];
                p[k] -= pi.t_prime[m][k];
            }
            for j in 0..ELL {
                let prod = plain_mul(&pi.a_prime[m][j], &r_bot[j]);
                for (k, v) in prod.iter().enumerate() {
                    p[k] += v;
                }
            }

            // P = (X^d + 1)*h + rem  =>  h[k] = P[k+d] (k <= d-2), h[d-1] = 0
            let mut hm = [0i128; D];
            for k in 0..(D - 1) {
                hm[k] = p[k + D];
            }
            // rem[k] = P[k] - h[k]; rem must be divisible by q, and g = rem/q
            let mut gm = [0i128; D];
            for k in 0..D {
                let rem = p[k] - hm[k];
                if rem % (Q as i128) != 0 {
                    return Err(format!(
                        "relation does not hold: residue rem_{m}[{k}] = {rem} is not divisible by q"
                    ));
                }
                gm[k] = rem / (Q as i128);
            }

            if hm.iter().any(|&v| v >= H_SHIFT || v < -H_SHIFT) {
                return Err(format!("h_{m} escapes the |h| < 2^33 range check"));
            }
            if gm.iter().any(|&v| v >= G_SHIFT || v < -G_SHIFT) {
                return Err(format!("g_{m} escapes the |g| < 2^12 range check"));
            }
            h.push(hm);
            g.push(gm);
        }

        Ok(Self { r, h, g })
    }
}

// ---------------------------------------------------------------------------
// trace
// ---------------------------------------------------------------------------

/// A two-segment execution trace (Winterfell's `TraceTable` is single-segment
/// only, so the aux segment forces a small custom `Trace` implementation).
pub struct RoleATrace {
    info: TraceInfo,
    main: ColMatrix<BaseElement>,
}

impl Trace for RoleATrace {
    type BaseField = BaseElement;

    fn info(&self) -> &TraceInfo {
        &self.info
    }

    fn main_segment(&self) -> &ColMatrix<BaseElement> {
        &self.main
    }

    fn read_main_frame(&self, row_idx: usize, frame: &mut EvaluationFrame<BaseElement>) {
        let next_row_idx = (row_idx + 1) % self.info.length();
        self.main.read_row_into(row_idx, frame.current_mut());
        self.main.read_row_into(next_row_idx, frame.next_mut());
    }
}

/// Which polynomial the `r` slot feeds in pass `p` (index into `witness.r`), if
/// any. Passes `0..ell` carry `r_bot[j]`; passes `ell..n+ell` carry `r_top[m]`.
#[inline]
fn r_slot_poly(p: usize) -> Option<usize> {
    if p < ELL {
        Some(N + p) // r_bot[p]
    } else if p < N_PLUS_ELL {
        Some(p - ELL) // r_top[p - ell]
    } else {
        None
    }
}

/// Build the main trace segment.
pub fn build_main_trace(w: &RoleAWitness) -> RoleATrace {
    let mut cols = vec![vec![BaseElement::ZERO; EV_LEN]; MAIN_WIDTH];
    for row in 0..EV_LEN {
        let p = row / PASS_ROWS;
        let k = PASS_ROWS - 1 - (row % PASS_ROWS); // Horner runs high degree -> low

        // r slot: ternary, enforced by r^2 = r*r and r^2*r = r
        let rv = r_slot_poly(p).map_or(0, |idx| w.r[idx][k]);
        cols[R_VAL][row] = fe(rv);
        cols[R_SQ][row] = fe(rv * rv);

        // h slot (power-of-two bound => one decomposition)
        let hv = if p < N { w.h[p][k] } else { 0 };
        cols[H_VAL][row] = fe(hv);
        let hu = hv + H_SHIFT;
        for b in 0..H_BITS {
            cols[H_U_OFF + b][row] = BaseElement::new(((hu >> b) & 1) as u64);
        }

        // g slot
        let gv = if p < N { w.g[p][k] } else { 0 };
        cols[G_VAL][row] = fe(gv);
        let gu = gv + G_SHIFT;
        for b in 0..G_BITS {
            cols[G_U_OFF + b][row] = BaseElement::new(((gu >> b) & 1) as u64);
        }

        // one-hot pass selectors (all zero in the padding passes p >= N_SEL)
        for s in 0..N_SEL {
            cols[SEL_OFF + s][row] =
                if s == p { BaseElement::ONE } else { BaseElement::ZERO };
        }
    }

    let info =
        TraceInfo::new_multi_segment(MAIN_WIDTH, AUX_WIDTH, N_AUX_RAND, EV_LEN, Vec::new());
    RoleATrace { info, main: ColMatrix::new(cols) }
}

/// Build the auxiliary trace segment; mirrors `evaluate_aux_transition` exactly.
fn build_aux_segment<E>(
    main: &ColMatrix<BaseElement>,
    pi: &RoleAPublicInputs,
    rand: &[E],
) -> ColMatrix<E>
where
    E: FieldElement<BaseField = BaseElement>,
{
    assert_eq!(N_AUX_RAND, rand.len(), "unexpected number of aux random elements");
    let x = rand[0];
    let rho = &rand[1..1 + N];

    let mut cols = vec![vec![E::ZERO; EV_LEN]; AUX_WIDTH];

    let mut acc_r = E::ZERO;
    let mut acc_h = E::ZERO;
    let mut acc_g = E::ZERO;
    let mut rb = vec![E::ZERO; ELL];
    let mut rt = E::ZERO;
    let mut hc = E::ZERO;
    let mut gc = E::ZERO;
    let mut pub_acc = E::ZERO;
    let mut r_chk = E::ZERO;

    for row in 0..EV_LEN {
        // ---- write the current row -----------------------------------------
        cols[A_ACC_R][row] = acc_r;
        cols[A_ACC_H][row] = acc_h;
        cols[A_ACC_G][row] = acc_g;
        for j in 0..ELL {
            cols[A_RB + j][row] = rb[j];
        }
        cols[A_RT][row] = rt;
        cols[A_HC][row] = hc;
        cols[A_GC][row] = gc;
        cols[A_PUB][row] = pub_acc;
        cols[A_R][row] = r_chk;

        // ---- compute the next row ------------------------------------------
        let p = row / PASS_ROWS;
        let i = row % PASS_ROWS;
        let k = PASS_ROWS - 1 - i;
        let is_last = i == PASS_ROWS - 1;

        let fin_r = acc_r * x + E::from(main.get(R_VAL, row));
        let fin_h = acc_h * x + E::from(main.get(H_VAL, row));
        let fin_g = acc_g * x + E::from(main.get(G_VAL, row));

        // public part of the relation, evaluated with the latched r_bot
        let mut in_pub = E::ZERO;
        for m in 0..N {
            let mut s = E::ZERO;
            for j in 0..ELL {
                s += rb[j] * E::from(fe(pi.a_prime[m][j][k]));
            }
            s -= E::from(fe(pi.t_prime[m][k]));
            in_pub += rho[m] * s;
        }

        let next_r_chk = rt + pub_acc
            - (pow_d(x) + E::ONE) * hc
            - E::from(BaseElement::new(Q as u64)) * gc;

        if is_last {
            if p < ELL {
                rb[p] += fin_r;
            } else if p < N_PLUS_ELL {
                rt += rho[p - ELL] * fin_r;
            }
            if p < N {
                hc += rho[p] * fin_h;
                gc += rho[p] * fin_g;
            }
        }
        if p == PASS_PUB {
            pub_acc = pub_acc * x + in_pub;
        }
        acc_r = if is_last { E::ZERO } else { fin_r };
        acc_h = if is_last { E::ZERO } else { fin_h };
        acc_g = if is_last { E::ZERO } else { fin_g };
        r_chk = next_r_chk;
    }

    ColMatrix::new(cols)
}

// ---------------------------------------------------------------------------
// AIR
// ---------------------------------------------------------------------------

pub struct RoleAAir {
    context: AirContext<BaseElement>,
    periodic: Vec<Vec<BaseElement>>,
}

impl Air for RoleAAir {
    type BaseField = BaseElement;
    type PublicInputs = RoleAPublicInputs;

    fn new(trace_info: TraceInfo, pub_inputs: RoleAPublicInputs, options: ProofOptions) -> Self {
        // Shape guards: the proof must be for exactly the LAS-sized instance.
        assert_eq!(MAIN_WIDTH, trace_info.main_trace_width(), "unexpected main trace width");
        assert_eq!(AUX_WIDTH, trace_info.aux_segment_width(), "unexpected aux trace width");
        assert_eq!(EV_LEN, trace_info.length(), "unexpected trace length");
        assert_eq!(N, pub_inputs.a_prime.len(), "A' must have n rows");
        assert!(pub_inputs.a_prime.iter().all(|row| row.len() == ELL), "A' rows must be ell wide");
        assert_eq!(N, pub_inputs.t_prime.len(), "t' must have n polynomials");

        // --- periodic columns: public coefficients, high degree first --------
        let mut periodic = vec![Vec::new(); N_PERIODIC];
        let mut isb = vec![BaseElement::ZERO; PASS_ROWS];
        isb[PASS_ROWS - 1] = BaseElement::ONE;
        periodic[P_ISB] = isb;
        let cycle = |p: &[i128; D]| -> Vec<BaseElement> {
            (0..PASS_ROWS).map(|i| fe(p[PASS_ROWS - 1 - i])).collect()
        };
        for m in 0..N {
            for j in 0..ELL {
                periodic[P_A + m * ELL + j] = cycle(&pub_inputs.a_prime[m][j]);
            }
            periodic[P_T + m] = cycle(&pub_inputs.t_prime[m]);
        }

        // --- constraint degrees (order MUST match the evaluators) ------------
        let mut main_degrees = Vec::with_capacity(N_MAIN_CONSTRAINTS);
        main_degrees.push(TransitionConstraintDegree::new(2)); // r_sq = r*r
        main_degrees.push(TransitionConstraintDegree::new(3)); // r_sq*r = r
        for _ in 0..H_BITS {
            main_degrees.push(TransitionConstraintDegree::new(2));
        }
        main_degrees.push(TransitionConstraintDegree::new(1));
        for _ in 0..G_BITS {
            main_degrees.push(TransitionConstraintDegree::new(2));
        }
        main_degrees.push(TransitionConstraintDegree::new(1));
        for _ in 0..N_SEL {
            main_degrees.push(TransitionConstraintDegree::with_cycles(1, vec![PASS_ROWS]));
        }
        assert_eq!(N_MAIN_CONSTRAINTS, main_degrees.len());

        let mut aux_degrees = Vec::with_capacity(N_AUX_CONSTRAINTS);
        for _ in 0..3 {
            // acc_r / acc_h / acc_g: one periodic factor (is_last)
            aux_degrees.push(TransitionConstraintDegree::with_cycles(1, vec![PASS_ROWS]));
        }
        for _ in 0..ELL {
            aux_degrees.push(TransitionConstraintDegree::with_cycles(2, vec![PASS_ROWS]));
        }
        for _ in 0..3 {
            // rt / hc / gc latches
            aux_degrees.push(TransitionConstraintDegree::with_cycles(2, vec![PASS_ROWS]));
        }
        // pub_acc: selector * (accumulator | latch * periodic)
        aux_degrees.push(TransitionConstraintDegree::with_cycles(2, vec![PASS_ROWS]));
        // residual: linear in the latches
        aux_degrees.push(TransitionConstraintDegree::new(1));
        assert_eq!(N_AUX_CONSTRAINTS, aux_degrees.len());

        let context = AirContext::new_multi_segment(
            trace_info,
            main_degrees,
            aux_degrees,
            N_SEL,     // main assertions: the one-hot selector seed
            AUX_WIDTH, // aux assertions: zeroed accumulators + the residual
            options,
        );
        RoleAAir { context, periodic }
    }

    fn get_periodic_column_values(&self) -> Vec<Vec<BaseElement>> {
        self.periodic.clone()
    }

    fn evaluate_transition<E: FieldElement + From<Self::BaseField>>(
        &self,
        frame: &EvaluationFrame<E>,
        periodic_values: &[E],
        result: &mut [E],
    ) {
        let cur = frame.current();
        let nxt = frame.next();
        let isb = periodic_values[P_ISB];
        let pow2 = |i: usize| E::from(BaseElement::new(1u64 << i));

        // --- r: ternary via r^3 = r ------------------------------------------
        // X^3 - X = X(X-1)(X+1) has at most three roots in F_p, so these two
        // constraints hold IFF r is in {-1, 0, 1}. This is the exact bound the
        // role-A relation requires -- not a relaxation of it.
        let r = cur[R_VAL];
        let r_sq = cur[R_SQ];
        result[C_RSQ] = r_sq - r * r;
        result[C_RTER] = r_sq * r - r;

        // --- h: single decomposition against the power-of-two bound ----------
        let mut acc_h = E::ZERO;
        for i in 0..H_BITS {
            let b = cur[H_U_OFF + i];
            result[C_HBOOL + i] = b * (b - E::ONE);
            acc_h += b * pow2(i);
        }
        result[C_HREC] = acc_h - (cur[H_VAL] + E::from(BaseElement::new(H_SHIFT as u64)));

        // --- g ---------------------------------------------------------------
        let mut acc_g = E::ZERO;
        for i in 0..G_BITS {
            let b = cur[G_U_OFF + i];
            result[C_GBOOL + i] = b * (b - E::ONE);
            acc_g += b * pow2(i);
        }
        result[C_GREC] = acc_g - (cur[G_VAL] + E::from(BaseElement::new(G_SHIFT as u64)));

        // --- one-hot pass selectors: shift by one at every pass boundary -----
        // Seeded by the boundary assertions, these are fully determined, so the
        // pass schedule cannot be forged.
        result[C_SEL] = nxt[SEL_OFF] - (E::ONE - isb) * cur[SEL_OFF];
        for s in 1..N_SEL {
            result[C_SEL + s] = nxt[SEL_OFF + s]
                - isb * cur[SEL_OFF + s - 1]
                - (E::ONE - isb) * cur[SEL_OFF + s];
        }
    }

    fn evaluate_aux_transition<F, E>(
        &self,
        main_frame: &EvaluationFrame<F>,
        aux_frame: &EvaluationFrame<E>,
        periodic_values: &[F],
        aux_rand_elements: &AuxRandElements<E>,
        result: &mut [E],
    ) where
        F: FieldElement<BaseField = Self::BaseField>,
        E: FieldElement<BaseField = Self::BaseField> + ExtensionOf<F>,
    {
        let m_cur = main_frame.current();
        let a_cur = aux_frame.current();
        let a_nxt = aux_frame.next();
        let rand = aux_rand_elements.rand_elements();
        let x = rand[0];
        let rho = &rand[1..1 + N];

        let isb = E::from(periodic_values[P_ISB]);
        let live = E::ONE - isb;
        let sel = |s: usize| E::from(m_cur[SEL_OFF + s]);

        // Horner step of each slot, i.e. the value the pass ends on.
        let fin_r = a_cur[A_ACC_R] * x + E::from(m_cur[R_VAL]);
        let fin_h = a_cur[A_ACC_H] * x + E::from(m_cur[H_VAL]);
        let fin_g = a_cur[A_ACC_G] * x + E::from(m_cur[G_VAL]);

        // (a) accumulators advance, and reset on the last row of every pass
        result[K_ACC_R] = a_nxt[A_ACC_R] - live * fin_r;
        result[K_ACC_H] = a_nxt[A_ACC_H] - live * fin_h;
        result[K_ACC_G] = a_nxt[A_ACC_G] - live * fin_g;

        // (b) latches absorb the finished value at the boundary of "their" pass
        for j in 0..ELL {
            result[K_RB + j] = a_nxt[A_RB + j] - a_cur[A_RB + j] - isb * sel(j) * fin_r;
        }
        let mut rsel_top = E::ZERO; // r_top[m] lives in pass ell + m
        let mut rsel_low = E::ZERO; // h_m / g_m live in pass m
        for m in 0..N {
            rsel_top += rho[m] * sel(ELL + m);
            rsel_low += rho[m] * sel(m);
        }
        result[K_RT] = a_nxt[A_RT] - a_cur[A_RT] - isb * rsel_top * fin_r;
        result[K_HC] = a_nxt[A_HC] - a_cur[A_HC] - isb * rsel_low * fin_h;
        result[K_GC] = a_nxt[A_GC] - a_cur[A_GC] - isb * rsel_low * fin_g;

        // (c) the public part: one Horner pass over
        //     sum_m rho_m * ( sum_j A'[m][j][k]*r_bot[j](x) - t'[m][k] )
        //     which equals sum_m rho_m * ( sum_j A'[m][j](x)*r_bot[j](x) - t'[m](x) ).
        //     The SAME latched r_bot[j](x) feeds every m -- this is the shared-
        //     witness binding a per-convolution gadget could not impose.
        let mut in_pub = E::ZERO;
        for m in 0..N {
            let mut s = E::ZERO;
            for j in 0..ELL {
                s += a_cur[A_RB + j] * E::from(periodic_values[P_A + m * ELL + j]);
            }
            s -= E::from(periodic_values[P_T + m]);
            in_pub += rho[m] * s;
        }
        let s_pub = sel(PASS_PUB);
        result[K_PUB] = a_nxt[A_PUB]
            - s_pub * (a_cur[A_PUB] * x + in_pub)
            - (E::ONE - s_pub) * a_cur[A_PUB];

        // (d) the residual: sum_m rho_m * [ P_m(x) - (x^d+1) h_m(x) - q g_m(x) ]
        let q_e = E::from(BaseElement::new(Q as u64));
        result[K_R] = a_nxt[A_R]
            - (a_cur[A_RT] + a_cur[A_PUB] - (pow_d(x) + E::ONE) * a_cur[A_HC] - q_e * a_cur[A_GC]);
    }

    fn get_assertions(&self) -> Vec<Assertion<Self::BaseField>> {
        // Seed the one-hot selector: pass 0 is active on row 0, nothing else is.
        let mut out = Vec::with_capacity(N_SEL);
        out.push(Assertion::single(SEL_OFF, 0, BaseElement::ONE));
        for s in 1..N_SEL {
            out.push(Assertion::single(SEL_OFF + s, 0, BaseElement::ZERO));
        }
        out
    }

    fn get_aux_assertions<E: FieldElement<BaseField = Self::BaseField>>(
        &self,
        _aux_rand_elements: &AuxRandElements<E>,
    ) -> Vec<Assertion<E>> {
        let mut out = Vec::with_capacity(AUX_WIDTH);
        // every accumulator and latch starts at zero ...
        for col in 0..AUX_WIDTH - 1 {
            out.push(Assertion::single(col, 0, E::ZERO));
        }
        // ... and the combined residual must be zero once every latch is final.
        out.push(Assertion::single(A_R, EV_LEN - 1, E::ZERO));
        out
    }

    fn context(&self) -> &AirContext<Self::BaseField> {
        &self.context
    }
}

// ---------------------------------------------------------------------------
// prover
// ---------------------------------------------------------------------------

pub struct RoleAProver {
    options: ProofOptions,
    pub_inputs: RoleAPublicInputs,
}

impl RoleAProver {
    pub fn new(options: ProofOptions, pub_inputs: RoleAPublicInputs) -> Self {
        Self { options, pub_inputs }
    }
}

impl Prover for RoleAProver {
    type BaseField = BaseElement;
    type Air = RoleAAir;
    type Trace = RoleATrace;
    type HashFn = Blake3_256<Self::BaseField>;
    type VC = MerkleTree<Self::HashFn>;
    type RandomCoin = DefaultRandomCoin<Self::HashFn>;
    type TraceLde<E: FieldElement<BaseField = Self::BaseField>> =
        DefaultTraceLde<E, Self::HashFn, Self::VC>;
    type ConstraintCommitment<E: FieldElement<BaseField = Self::BaseField>> =
        DefaultConstraintCommitment<E, Self::HashFn, Self::VC>;
    type ConstraintEvaluator<'a, E: FieldElement<BaseField = Self::BaseField>> =
        DefaultConstraintEvaluator<'a, Self::Air, E>;

    fn get_pub_inputs(&self, _trace: &Self::Trace) -> RoleAPublicInputs {
        self.pub_inputs.clone()
    }

    fn options(&self) -> &ProofOptions {
        &self.options
    }

    fn build_aux_trace<E>(
        &self,
        main_trace: &Self::Trace,
        aux_rand_elements: &AuxRandElements<E>,
    ) -> ColMatrix<E>
    where
        E: FieldElement<BaseField = Self::BaseField>,
    {
        build_aux_segment(
            main_trace.main_segment(),
            &self.pub_inputs,
            aux_rand_elements.rand_elements(),
        )
    }

    fn new_trace_lde<E: FieldElement<BaseField = Self::BaseField>>(
        &self,
        trace_info: &TraceInfo,
        main_trace: &ColMatrix<Self::BaseField>,
        domain: &StarkDomain<Self::BaseField>,
        partition_option: PartitionOptions,
    ) -> (Self::TraceLde<E>, TracePolyTable<E>) {
        DefaultTraceLde::new(trace_info, main_trace, domain, partition_option)
    }

    fn build_constraint_commitment<E: FieldElement<BaseField = Self::BaseField>>(
        &self,
        composition_poly_trace: CompositionPolyTrace<E>,
        num_constraint_composition_columns: usize,
        domain: &StarkDomain<Self::BaseField>,
        partition_options: PartitionOptions,
    ) -> (Self::ConstraintCommitment<E>, CompositionPoly<E>) {
        DefaultConstraintCommitment::new(
            composition_poly_trace,
            num_constraint_composition_columns,
            domain,
            partition_options,
        )
    }

    fn new_evaluator<'a, E: FieldElement<BaseField = Self::BaseField>>(
        &self,
        air: &'a Self::Air,
        aux_rand_elements: Option<AuxRandElements<E>>,
        composition_coefficients: ConstraintCompositionCoefficients<E>,
    ) -> Self::ConstraintEvaluator<'a, E> {
        DefaultConstraintEvaluator::new(air, aux_rand_elements, composition_coefficients)
    }
}

// ---------------------------------------------------------------------------
// entry points
// ---------------------------------------------------------------------------

/// Prove `exists r : A r = t' and ||r||inf <= 1` for a role-A instance.
pub fn prove_role_a(inst: &RoleAInstance) -> Result<(Proof, RoleAPublicInputs), String> {
    let pub_inputs = RoleAPublicInputs::from_instance(inst);
    let witness = RoleAWitness::build(inst, &pub_inputs)?;
    let trace = build_main_trace(&witness);
    let prover = RoleAProver::new(crate::proof_options(), pub_inputs.clone());
    let proof = prover.prove(trace).map_err(|e| format!("{e:?}"))?;
    Ok((proof, pub_inputs))
}

/// Verify a role-A proof against the public `(A', t')`.
pub fn verify_role_a(proof: Proof, pub_inputs: RoleAPublicInputs) -> Result<(), String> {
    let acceptable = winterfell::AcceptableOptions::OptionSet(vec![crate::proof_options()]);
    winterfell::verify::<
        RoleAAir,
        Blake3_256<BaseElement>,
        DefaultRandomCoin<Blake3_256<BaseElement>>,
        MerkleTree<Blake3_256<BaseElement>>,
    >(proof, pub_inputs, &acceptable)
    .map_err(|e| format!("{e:?}"))
}
