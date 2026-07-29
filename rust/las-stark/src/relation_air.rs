//! STARK for the ARITHMETIC relation of LAS `base_verify` -- constraints (1)
//! and (3) ONLY, never the whole verification -- at the real ring degree
//! `d = 256`, with ONE shared response `z` across all six output polynomials:
//!
//! ```text
//! (1)  ||z||inf <= B                                        (B = gamma - kappa)
//! (3)  w'_m = z_top[m] + sum_j A'[m][j] (X) z_bot[j] - c (X) t[m]   (mod q, negacyclic)
//! ```
//!
//! for every `m < n`, where `A'`, `t`, `c`, `w'` are PUBLIC and `z` is the
//! private witness. This supersedes `conv_air.rs`, which proved ONE convolution
//! at a reduced degree `CONV_D = 64` and could not bind the 30 `A'*z_bot`
//! convolutions to a single shared `z_bot`.
//!
//! ## Why this layout (the 255-column cap, and how it is dodged)
//!
//! `conv_air.rs` carries the whole rotated `d`-coefficient window as trace
//! COLUMNS, so it needs `>= d` columns and dies at Winterfell's 255-column cap
//! for `d = 256`. This AIR is NARROW instead: it never materialises a
//! convolution. It uses a random-evaluation (Schwartz-Zippel) argument -- the
//! standard way to check a polynomial identity in O(d) rows and O(1) columns --
//! run over Winterfell's AUXILIARY trace segment, whose random elements are
//! drawn AFTER the main trace is committed (which is exactly what makes the
//! challenge point unpredictable to the prover, hence sound).
//!
//! ## The identity that is proven
//!
//! Over the integers, for each output index `m`, define the degree-`(2d-2)`
//! polynomial (all public operands taken in CENTRED representation):
//!
//! ```text
//! P_m(X) = z_top[m](X) + sum_j A'[m][j](X)*z_bot[j](X) - c(X)*t[m](X) - w'[m](X)
//! ```
//!
//! Relation (3) holds  <=>  `P_m == 0` in `Z_q[X]/(X^d+1)`  <=>  there exist
//! INTEGER polynomials `h_m` (deg <= d-2) and `g_m` (deg <= d-1) with
//!
//! ```text
//! P_m(X) = (X^d + 1)*h_m(X) + q*g_m(X)                              (over Z)
//! ```
//!
//! The prover commits to `z`, `h_m`, `g_m` in the MAIN trace (with range
//! checks), then the verifier's random `x` (and independent `rho_0..rho_5`) are
//! drawn, and the aux segment evaluates every polynomial at `x` by Horner and
//! checks the single scalar equation
//!
//! ```text
//! sum_m rho_m * [ P_m(x) - (x^d + 1)*h_m(x) - q*g_m(x) ] == 0
//! ```
//!
//! ## Soundness (why the bounds are load-bearing -- do not relax them)
//!
//! * The `rho`-combination is over already-EVALUATED scalars, so a nonzero term
//!   survives except with probability `<= n/|E| = 6/|E|`.
//! * Schwartz-Zippel: if the polynomial `D_m = P_m - (X^d+1)h_m - q*g_m` is
//!   nonzero over `F_p`, then `D_m(x) = 0` with probability `<= 510/|E|`.
//!   `E` is the quadratic extension of Goldilocks, so `|E| ~ 2^128`.
//! * A `F_p` identity only implies the INTEGER identity if every coefficient is
//!   smaller than `p/2`. That is what the range checks buy: `|z| <= B` (tight,
//!   which is also constraint (1)), `|h| < 2^51`, `|g| < 2^29`. Then every
//!   coefficient of `D_m` is bounded by `|P| + 2*2^51 + q*2^29 ~ 2^53 << p/2 ~
//!   2^63`, so the identity lifts to `Z` and relation (3) follows.
//!   WITHOUT a bound on `g` the argument is VACUOUS: `q` is invertible mod `p`,
//!   so an unbounded `g` can satisfy the equation for ANY claimed `w'`.
//! * `z_bot[j](x)` is evaluated ONCE and reused by all six `m`, so the six
//!   output equations are bound to one and the same `z` by construction -- the
//!   cross-convolution binding `conv_air.rs` could not impose.
//!
//! ## What is still NOT proven (known gap -- do not overclaim)
//!
//! Constraints (2) `c = SampleInBall(c_tilde)` and (4)
//! `c_tilde = SHAKE256(pack(t) || pack(w') || M)` are NOT in this AIR: `c` and
//! `w'` are taken as public inputs. So this proves the lattice arithmetic of
//! verification, not the Fiat-Shamir hash chain. Folding the hashes in (an
//! in-AIR Keccak-f) is the remaining Stage-A.2 work.
//!
//! ## Trace shape
//!
//! `EV_LEN = 4096` rows = 16 "passes" of `d = 256` rows; pass `p` feeds
//! coefficient `d-1-i` of one polynomial per slot at row offset `i`, so each
//! pass is one Horner evaluation. Slots: one `z` slot (11 passes: 5 `z_bot`
//! then 6 `z_top`), one `h` slot (6 passes), one `g` slot (6 passes); pass 11
//! accumulates the public part (which needs `z_bot(x)` and `c(x)` already
//! latched). Public coefficients enter as PERIODIC columns (cycle `d`), so they
//! cost no trace width.

use winterfell::{
    crypto::{hashers::Blake3_256, DefaultRandomCoin, MerkleTree},
    math::{fields::f64::BaseElement, ExtensionOf, FieldElement, ToElements},
    matrix::ColMatrix,
    Air, AirContext, Assertion, AuxRandElements, CompositionPoly, CompositionPolyTrace,
    ConstraintCompositionCoefficients, DefaultConstraintCommitment, DefaultConstraintEvaluator,
    DefaultTraceLde, EvaluationFrame, PartitionOptions, Proof, ProofOptions, Prover, StarkDomain,
    Trace, TraceInfo, TracePolyTable, TransitionConstraintDegree,
};

use crate::params::{B, D, ELL, N, N_PLUS_ELL, Q, RANGE_BITS, TWO_B};
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
/// come after every `z_bot` pass (0..4) and after the `c` pass (0).
pub const PASS_PUB: usize = 11;

// Range-check widths. `h`/`g` use POWER-OF-TWO bounds, so ONE decomposition per
// value suffices (`u = v + 2^(k-1)` in `[0, 2^k)` <=> `v` in `[-2^(k-1), 2^(k-1))`).
// `z` needs the TIGHT, non-power-of-two bound `B`, so it keeps the two-sided
// decomposition of `NormAir` (`u = z + B` and `2B - u`, both `RANGE_BITS` wide).
/// Bits of the shifted `h` coefficient; bound `|h| < 2^51`.
pub const H_BITS: usize = 52;
const H_SHIFT: i128 = 1 << 51;
/// Bits of the shifted `g` coefficient; bound `|g| < 2^29`.
pub const G_BITS: usize = 30;
const G_SHIFT: i128 = 1 << 29;

// --- main column layout ----------------------------------------------------
const Z_VAL: usize = 0;
const Z_U_OFF: usize = Z_VAL + 1;
const Z_W_OFF: usize = Z_U_OFF + RANGE_BITS;
const H_VAL: usize = Z_W_OFF + RANGE_BITS;
const H_U_OFF: usize = H_VAL + 1;
const G_VAL: usize = H_U_OFF + H_BITS;
const G_U_OFF: usize = G_VAL + 1;
const SEL_OFF: usize = G_U_OFF + G_BITS;
/// Total main-segment width (135).
pub const MAIN_WIDTH: usize = SEL_OFF + N_SEL;

// --- aux column layout (all over the extension field E) --------------------
const A_ACC_Z: usize = 0; // Horner accumulator of the z slot
const A_ACC_H: usize = 1; // Horner accumulator of the h slot
const A_ACC_G: usize = 2; // Horner accumulator of the g slot
const A_ACC_C: usize = 3; // Horner accumulator of the public challenge c
const A_ZB: usize = 4; // ELL latches: z_bot[j](x)
const A_CX: usize = A_ZB + ELL; // latch: c(x)
const A_ZT: usize = A_CX + 1; // latch: sum_m rho_m * z_top[m](x)
const A_HC: usize = A_ZT + 1; // latch: sum_m rho_m * h_m(x)
const A_GC: usize = A_HC + 1; // latch: sum_m rho_m * g_m(x)
const A_PUB: usize = A_GC + 1; // sum_m rho_m * (A'.z_bot - c.t - w')(x)
const A_R: usize = A_PUB + 1; // the combined residual; asserted 0 on the last row
/// Total auxiliary-segment width (15).
pub const AUX_WIDTH: usize = A_R + 1;

/// Random elements drawn after the main commitment: `x`, then `rho_0..rho_{n-1}`.
pub const N_AUX_RAND: usize = 1 + N;

// --- periodic column layout (public data; costs no trace width) ------------
const P_ISB: usize = 0; // 1 on the last row of every pass
const P_C: usize = 1; // challenge c
const P_A: usize = 2; // n*ell columns: A'[m][j] at P_A + m*ELL + j
const P_T: usize = P_A + N * ELL; // n columns: t[m]
const P_W: usize = P_T + N; // n columns: w'[m]
const N_PERIODIC: usize = P_W + N; // 44

// --- main transition-constraint layout -------------------------------------
const C_ZBOOL: usize = 0;
const C_ZREC: usize = C_ZBOOL + 2 * RANGE_BITS;
const C_HBOOL: usize = C_ZREC + 2;
const C_HREC: usize = C_HBOOL + H_BITS;
const C_GBOOL: usize = C_HREC + 1;
const C_GREC: usize = C_GBOOL + G_BITS;
const C_SEL: usize = C_GREC + 1;
const N_MAIN_CONSTRAINTS: usize = C_SEL + N_SEL;

// --- aux transition-constraint layout --------------------------------------
const K_ACC_Z: usize = 0;
const K_ACC_H: usize = 1;
const K_ACC_G: usize = 2;
const K_ACC_C: usize = 3;
const K_CX: usize = 4;
const K_ZB: usize = 5; // ELL constraints
const K_ZT: usize = K_ZB + ELL;
const K_HC: usize = K_ZT + 1;
const K_GC: usize = K_HC + 1;
const K_PUB: usize = K_GC + 1;
const K_R: usize = K_PUB + 1;
const N_AUX_CONSTRAINTS: usize = K_R + 1;

// Compile-time sanity on the shape the soundness argument assumes.
const _: () = {
    assert!(MAIN_WIDTH + AUX_WIDTH <= 255); // Winterfell TraceInfo::MAX_TRACE_WIDTH
    assert!(EV_LEN.is_power_of_two());
    assert!(N_SEL <= N_PASSES);
    assert!(N_PLUS_ELL <= N_SEL); // the z slot needs one pass per z polynomial
    assert!(N <= N_SEL); // the h/g slots need one pass per output polynomial
    assert!(PASS_PUB >= ELL); // z_bot latches must be final before the public pass
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
// public inputs
// ---------------------------------------------------------------------------

/// The public side of the relation, in CENTRED representation (the centring is
/// what keeps the integer bound on `P_m` -- and hence on `h`/`g` -- tight).
#[derive(Clone)]
pub struct RelationPublicInputs {
    /// `A'` row-major `[m][j]`, `n x ell`.
    pub a_prime: Vec<Vec<[i128; D]>>,
    /// Public key `t` (`n` polynomials).
    pub t: Vec<[i128; D]>,
    /// Challenge polynomial `c`.
    pub c: [i128; D],
    /// Claimed commitment `w'` (`n` polynomials).
    pub w_prime: Vec<[i128; D]>,
}

impl RelationPublicInputs {
    /// Centre the public part of a golden verification vector.
    pub fn from_vector(vv: &VerifyVector) -> Self {
        Self {
            a_prime: vv
                .a_prime
                .iter()
                .map(|row| row.iter().map(centred_poly).collect())
                .collect(),
            t: vv.t.iter().map(centred_poly).collect(),
            c: centred_poly(&vv.c),
            w_prime: vv.w_prime.iter().map(centred_poly).collect(),
        }
    }
}

impl ToElements<BaseElement> for RelationPublicInputs {
    fn to_elements(&self) -> Vec<BaseElement> {
        let mut v = Vec::with_capacity((N * ELL + 2 * N + 1) * D);
        for row in &self.a_prime {
            for p in row {
                v.extend(p.iter().map(|&x| fe(x)));
            }
        }
        for p in &self.t {
            v.extend(p.iter().map(|&x| fe(x)));
        }
        v.extend(self.c.iter().map(|&x| fe(x)));
        for p in &self.w_prime {
            v.extend(p.iter().map(|&x| fe(x)));
        }
        v
    }
}

// ---------------------------------------------------------------------------
// witness
// ---------------------------------------------------------------------------

/// The private side: the centred response `z` plus the quotient polynomials
/// `h_m` (by `X^d+1`) and `g_m` (by `q`) that witness `P_m == 0` in `R_q`.
pub struct RelationWitness {
    /// Centred `z`, `n+ell` polynomials (`z_top` then `z_bot`, as in the vector).
    pub z: Vec<[i128; D]>,
    /// `h_m`, `n` polynomials; `h_m[d-1]` is always 0 (deg <= d-2).
    pub h: Vec<[i128; D]>,
    /// `g_m`, `n` polynomials (deg <= d-1).
    pub g: Vec<[i128; D]>,
}

impl RelationWitness {
    /// Derive the witness from a golden vector. Fails if the relation does not
    /// actually hold (the `q`-division would not be exact) or if a coefficient
    /// escapes the range the AIR enforces.
    pub fn build(vv: &VerifyVector, pi: &RelationPublicInputs) -> Result<Self, String> {
        let z: Vec<[i128; D]> = vv.z.iter().map(centred_poly).collect();
        let z_top = &z[0..N];
        let z_bot = &z[N..N_PLUS_ELL];

        let mut h = Vec::with_capacity(N);
        let mut g = Vec::with_capacity(N);
        for m in 0..N {
            // P_m over Z, degree <= 2d-2
            let mut p = vec![0i128; 2 * D - 1];
            for k in 0..D {
                p[k] += z_top[m][k];
                p[k] -= pi.w_prime[m][k];
            }
            for j in 0..ELL {
                let prod = plain_mul(&pi.a_prime[m][j], &z_bot[j]);
                for (k, v) in prod.iter().enumerate() {
                    p[k] += v;
                }
            }
            let ct = plain_mul(&pi.c, &pi.t[m]);
            for (k, v) in ct.iter().enumerate() {
                p[k] -= v;
            }

            // P = (X^d + 1)*h + r  =>  h[k] = P[k+d] (k <= d-2), h[d-1] = 0
            let mut hm = [0i128; D];
            for k in 0..(D - 1) {
                hm[k] = p[k + D];
            }
            // r[k] = P[k] - h[k]; r must be divisible by q, and g = r/q
            let mut gm = [0i128; D];
            for k in 0..D {
                let r = p[k] - hm[k];
                if r % (Q as i128) != 0 {
                    return Err(format!(
                        "relation does not hold: residue r_{m}[{k}] = {r} is not divisible by q"
                    ));
                }
                gm[k] = r / (Q as i128);
            }

            if hm.iter().any(|&v| v >= H_SHIFT || v < -H_SHIFT) {
                return Err(format!("h_{m} escapes the |h| < 2^51 range check"));
            }
            if gm.iter().any(|&v| v >= G_SHIFT || v < -G_SHIFT) {
                return Err(format!("g_{m} escapes the |g| < 2^29 range check"));
            }
            h.push(hm);
            g.push(gm);
        }

        if z.iter().any(|p| p.iter().any(|&v| v > B as i128 || v < -(B as i128))) {
            return Err("||z||inf > B: the response fails base-verify constraint (1)".into());
        }
        Ok(Self { z, h, g })
    }
}

// ---------------------------------------------------------------------------
// trace
// ---------------------------------------------------------------------------

/// A two-segment execution trace (Winterfell's `TraceTable` is single-segment
/// only, so the aux segment forces a small custom `Trace` implementation).
pub struct RelationTrace {
    info: TraceInfo,
    main: ColMatrix<BaseElement>,
}

impl Trace for RelationTrace {
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

/// Which polynomial the `z` slot feeds in pass `p` (index into `witness.z`), if any.
/// Passes `0..ell` carry `z_bot[j]`; passes `ell..n+ell` carry `z_top[m]`.
#[inline]
fn z_slot_poly(p: usize) -> Option<usize> {
    if p < ELL {
        Some(N + p) // z_bot[p]
    } else if p < N_PLUS_ELL {
        Some(p - ELL) // z_top[p - ell]
    } else {
        None
    }
}

/// Build the main trace segment.
pub fn build_main_trace(w: &RelationWitness) -> RelationTrace {
    let mut cols = vec![vec![BaseElement::ZERO; EV_LEN]; MAIN_WIDTH];
    for r in 0..EV_LEN {
        let p = r / PASS_ROWS;
        let k = PASS_ROWS - 1 - (r % PASS_ROWS); // Horner runs high degree -> low

        // z slot (range-checked against the TIGHT bound B, two-sided)
        let zv = z_slot_poly(p).map_or(0, |idx| w.z[idx][k]);
        cols[Z_VAL][r] = fe(zv);
        let zu = zv + B as i128; // in [0, 2B]
        let zw = TWO_B as i128 - zu; // in [0, 2B]
        for b in 0..RANGE_BITS {
            cols[Z_U_OFF + b][r] = BaseElement::new(((zu >> b) & 1) as u64);
            cols[Z_W_OFF + b][r] = BaseElement::new(((zw >> b) & 1) as u64);
        }

        // h slot (power-of-two bound => one decomposition)
        let hv = if p < N { w.h[p][k] } else { 0 };
        cols[H_VAL][r] = fe(hv);
        let hu = hv + H_SHIFT;
        for b in 0..H_BITS {
            cols[H_U_OFF + b][r] = BaseElement::new(((hu >> b) & 1) as u64);
        }

        // g slot
        let gv = if p < N { w.g[p][k] } else { 0 };
        cols[G_VAL][r] = fe(gv);
        let gu = gv + G_SHIFT;
        for b in 0..G_BITS {
            cols[G_U_OFF + b][r] = BaseElement::new(((gu >> b) & 1) as u64);
        }

        // one-hot pass selectors (all zero in the padding passes p >= N_SEL)
        for s in 0..N_SEL {
            cols[SEL_OFF + s][r] =
                if s == p { BaseElement::ONE } else { BaseElement::ZERO };
        }
    }

    let info =
        TraceInfo::new_multi_segment(MAIN_WIDTH, AUX_WIDTH, N_AUX_RAND, EV_LEN, Vec::new());
    RelationTrace { info, main: ColMatrix::new(cols) }
}

/// Build the auxiliary trace segment; mirrors `evaluate_aux_transition` exactly.
fn build_aux_segment<E>(
    main: &ColMatrix<BaseElement>,
    pi: &RelationPublicInputs,
    rand: &[E],
) -> ColMatrix<E>
where
    E: FieldElement<BaseField = BaseElement>,
{
    assert_eq!(N_AUX_RAND, rand.len(), "unexpected number of aux random elements");
    let x = rand[0];
    let rho = &rand[1..1 + N];

    let mut cols = vec![vec![E::ZERO; EV_LEN]; AUX_WIDTH];

    let mut acc_z = E::ZERO;
    let mut acc_h = E::ZERO;
    let mut acc_g = E::ZERO;
    let mut acc_c = E::ZERO;
    let mut zb = vec![E::ZERO; ELL];
    let mut cx = E::ZERO;
    let mut zt = E::ZERO;
    let mut hc = E::ZERO;
    let mut gc = E::ZERO;
    let mut pub_acc = E::ZERO;
    let mut r_chk = E::ZERO;

    for r in 0..EV_LEN {
        // ---- write the current row -----------------------------------------
        cols[A_ACC_Z][r] = acc_z;
        cols[A_ACC_H][r] = acc_h;
        cols[A_ACC_G][r] = acc_g;
        cols[A_ACC_C][r] = acc_c;
        for j in 0..ELL {
            cols[A_ZB + j][r] = zb[j];
        }
        cols[A_CX][r] = cx;
        cols[A_ZT][r] = zt;
        cols[A_HC][r] = hc;
        cols[A_GC][r] = gc;
        cols[A_PUB][r] = pub_acc;
        cols[A_R][r] = r_chk;

        // ---- compute the next row ------------------------------------------
        let p = r / PASS_ROWS;
        let i = r % PASS_ROWS;
        let k = PASS_ROWS - 1 - i;
        let is_last = i == PASS_ROWS - 1;

        let fin_z = acc_z * x + E::from(main.get(Z_VAL, r));
        let fin_h = acc_h * x + E::from(main.get(H_VAL, r));
        let fin_g = acc_g * x + E::from(main.get(G_VAL, r));
        let fin_c = acc_c * x + E::from(fe(pi.c[k]));

        // public part of the relation, evaluated with the latched z_bot / c(x)
        let mut in_pub = E::ZERO;
        for m in 0..N {
            let mut s = E::ZERO;
            for j in 0..ELL {
                s += zb[j] * E::from(fe(pi.a_prime[m][j][k]));
            }
            s -= cx * E::from(fe(pi.t[m][k]));
            s -= E::from(fe(pi.w_prime[m][k]));
            in_pub += rho[m] * s;
        }

        let next_r_chk = zt + pub_acc
            - (pow_d(x) + E::ONE) * hc
            - E::from(BaseElement::new(Q as u64)) * gc;

        if is_last {
            if p < ELL {
                zb[p] += fin_z;
            } else if p < N_PLUS_ELL {
                zt += rho[p - ELL] * fin_z;
            }
            if p == 0 {
                cx += fin_c;
            }
            if p < N {
                hc += rho[p] * fin_h;
                gc += rho[p] * fin_g;
            }
        }
        if p == PASS_PUB {
            pub_acc = pub_acc * x + in_pub;
        }
        acc_z = if is_last { E::ZERO } else { fin_z };
        acc_h = if is_last { E::ZERO } else { fin_h };
        acc_g = if is_last { E::ZERO } else { fin_g };
        acc_c = if is_last { E::ZERO } else { fin_c };
        r_chk = next_r_chk;
    }

    ColMatrix::new(cols)
}

// ---------------------------------------------------------------------------
// AIR
// ---------------------------------------------------------------------------

pub struct RelationAir {
    context: AirContext<BaseElement>,
    periodic: Vec<Vec<BaseElement>>,
}

impl Air for RelationAir {
    type BaseField = BaseElement;
    type PublicInputs = RelationPublicInputs;

    fn new(trace_info: TraceInfo, pub_inputs: RelationPublicInputs, options: ProofOptions) -> Self {
        // Shape guards: the proof must be for exactly the LAS-sized instance.
        assert_eq!(MAIN_WIDTH, trace_info.main_trace_width(), "unexpected main trace width");
        assert_eq!(AUX_WIDTH, trace_info.aux_segment_width(), "unexpected aux trace width");
        assert_eq!(EV_LEN, trace_info.length(), "unexpected trace length");
        assert_eq!(N, pub_inputs.a_prime.len(), "A' must have n rows");
        assert!(pub_inputs.a_prime.iter().all(|row| row.len() == ELL), "A' rows must be ell wide");
        assert_eq!(N, pub_inputs.t.len(), "t must have n polynomials");
        assert_eq!(N, pub_inputs.w_prime.len(), "w' must have n polynomials");

        // --- periodic columns: public coefficients, high degree first --------
        let mut periodic = vec![Vec::new(); N_PERIODIC];
        let mut isb = vec![BaseElement::ZERO; PASS_ROWS];
        isb[PASS_ROWS - 1] = BaseElement::ONE;
        periodic[P_ISB] = isb;
        let cycle = |p: &[i128; D]| -> Vec<BaseElement> {
            (0..PASS_ROWS).map(|i| fe(p[PASS_ROWS - 1 - i])).collect()
        };
        periodic[P_C] = cycle(&pub_inputs.c);
        for m in 0..N {
            for j in 0..ELL {
                periodic[P_A + m * ELL + j] = cycle(&pub_inputs.a_prime[m][j]);
            }
            periodic[P_T + m] = cycle(&pub_inputs.t[m]);
            periodic[P_W + m] = cycle(&pub_inputs.w_prime[m]);
        }

        // --- constraint degrees (order MUST match the evaluators) ------------
        let mut main_degrees = Vec::with_capacity(N_MAIN_CONSTRAINTS);
        for _ in 0..(2 * RANGE_BITS) {
            main_degrees.push(TransitionConstraintDegree::new(2)); // z booleanity
        }
        for _ in 0..2 {
            main_degrees.push(TransitionConstraintDegree::new(1)); // z recomposition
        }
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
            // acc_z / acc_h / acc_g: one periodic factor (is_last)
            aux_degrees.push(TransitionConstraintDegree::with_cycles(1, vec![PASS_ROWS]));
        }
        // acc_c also multiplies the periodic c column
        aux_degrees.push(TransitionConstraintDegree::with_cycles(1, vec![PASS_ROWS, PASS_ROWS]));
        // cx: selector * accumulator, and is_last * c
        aux_degrees.push(TransitionConstraintDegree::with_cycles(2, vec![PASS_ROWS, PASS_ROWS]));
        for _ in 0..ELL {
            aux_degrees.push(TransitionConstraintDegree::with_cycles(2, vec![PASS_ROWS]));
        }
        for _ in 0..3 {
            // zt / hc / gc latches
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
            N_SEL,      // main assertions: the one-hot selector seed
            AUX_WIDTH,  // aux assertions: zeroed accumulators + the residual
            options,
        );
        RelationAir { context, periodic }
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

        // --- z: two-sided range check against the tight bound B --------------
        let mut acc_u = E::ZERO;
        let mut acc_w = E::ZERO;
        for i in 0..RANGE_BITS {
            let b = cur[Z_U_OFF + i];
            let d = cur[Z_W_OFF + i];
            result[C_ZBOOL + i] = b * (b - E::ONE);
            result[C_ZBOOL + RANGE_BITS + i] = d * (d - E::ONE);
            acc_u += b * pow2(i);
            acc_w += d * pow2(i);
        }
        let b_e = E::from(BaseElement::new(B as u64));
        // u = z + B and 2B - u = B - z, both < 2^RANGE_BITS => |z| <= B
        result[C_ZREC] = acc_u - (cur[Z_VAL] + b_e);
        result[C_ZREC + 1] = acc_w + cur[Z_VAL] - b_e;

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
        let fin_z = a_cur[A_ACC_Z] * x + E::from(m_cur[Z_VAL]);
        let fin_h = a_cur[A_ACC_H] * x + E::from(m_cur[H_VAL]);
        let fin_g = a_cur[A_ACC_G] * x + E::from(m_cur[G_VAL]);
        let fin_c = a_cur[A_ACC_C] * x + E::from(periodic_values[P_C]);

        // (a) accumulators advance, and reset on the last row of every pass
        result[K_ACC_Z] = a_nxt[A_ACC_Z] - live * fin_z;
        result[K_ACC_H] = a_nxt[A_ACC_H] - live * fin_h;
        result[K_ACC_G] = a_nxt[A_ACC_G] - live * fin_g;
        result[K_ACC_C] = a_nxt[A_ACC_C] - live * fin_c;

        // (b) latches absorb the finished value at the boundary of "their" pass
        result[K_CX] = a_nxt[A_CX] - a_cur[A_CX] - isb * sel(0) * fin_c;
        for j in 0..ELL {
            result[K_ZB + j] = a_nxt[A_ZB + j] - a_cur[A_ZB + j] - isb * sel(j) * fin_z;
        }
        let mut rsel_top = E::ZERO; // z_top[m] lives in pass ell + m
        let mut rsel_low = E::ZERO; // h_m / g_m live in pass m
        for m in 0..N {
            rsel_top += rho[m] * sel(ELL + m);
            rsel_low += rho[m] * sel(m);
        }
        result[K_ZT] = a_nxt[A_ZT] - a_cur[A_ZT] - isb * rsel_top * fin_z;
        result[K_HC] = a_nxt[A_HC] - a_cur[A_HC] - isb * rsel_low * fin_h;
        result[K_GC] = a_nxt[A_GC] - a_cur[A_GC] - isb * rsel_low * fin_g;

        // (c) the public part: one Horner pass over
        //     sum_m rho_m * ( sum_j A'[m][j][k]*z_bot[j](x) - t[m][k]*c(x) - w'[m][k] )
        //     which equals sum_m rho_m * ( sum_j A'[m][j](x)*z_bot[j](x)
        //                                  - c(x)*t[m](x) - w'[m](x) ).
        //     The SAME latched z_bot[j](x) feeds every m -- this is the shared-
        //     witness binding a per-convolution gadget cannot express.
        let mut in_pub = E::ZERO;
        for m in 0..N {
            let mut s = E::ZERO;
            for j in 0..ELL {
                s += a_cur[A_ZB + j] * E::from(periodic_values[P_A + m * ELL + j]);
            }
            s -= a_cur[A_CX] * E::from(periodic_values[P_T + m]);
            s -= E::from(periodic_values[P_W + m]);
            in_pub += rho[m] * s;
        }
        let s_pub = sel(PASS_PUB);
        result[K_PUB] = a_nxt[A_PUB]
            - s_pub * (a_cur[A_PUB] * x + in_pub)
            - (E::ONE - s_pub) * a_cur[A_PUB];

        // (d) the residual: sum_m rho_m * [ P_m(x) - (x^d+1) h_m(x) - q g_m(x) ]
        let q_e = E::from(BaseElement::new(Q as u64));
        result[K_R] = a_nxt[A_R]
            - (a_cur[A_ZT] + a_cur[A_PUB] - (pow_d(x) + E::ONE) * a_cur[A_HC] - q_e * a_cur[A_GC]);
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

pub struct RelationProver {
    options: ProofOptions,
    pub_inputs: RelationPublicInputs,
}

impl RelationProver {
    pub fn new(options: ProofOptions, pub_inputs: RelationPublicInputs) -> Self {
        Self { options, pub_inputs }
    }
}

impl Prover for RelationProver {
    type BaseField = BaseElement;
    type Air = RelationAir;
    type Trace = RelationTrace;
    type HashFn = Blake3_256<Self::BaseField>;
    type VC = MerkleTree<Self::HashFn>;
    type RandomCoin = DefaultRandomCoin<Self::HashFn>;
    type TraceLde<E: FieldElement<BaseField = Self::BaseField>> =
        DefaultTraceLde<E, Self::HashFn, Self::VC>;
    type ConstraintCommitment<E: FieldElement<BaseField = Self::BaseField>> =
        DefaultConstraintCommitment<E, Self::HashFn, Self::VC>;
    type ConstraintEvaluator<'a, E: FieldElement<BaseField = Self::BaseField>> =
        DefaultConstraintEvaluator<'a, Self::Air, E>;

    fn get_pub_inputs(&self, _trace: &Self::Trace) -> RelationPublicInputs {
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

/// Prove constraints (1) and (3) of `base_verify` for a golden vector at the
/// real degree `d = 256`, with one shared `z` across all `n` output polynomials.
pub fn prove_relation(vv: &VerifyVector) -> Result<(Proof, RelationPublicInputs), String> {
    let pub_inputs = RelationPublicInputs::from_vector(vv);
    let witness = RelationWitness::build(vv, &pub_inputs)?;
    let trace = build_main_trace(&witness);
    let prover = RelationProver::new(crate::proof_options(), pub_inputs.clone());
    let proof = prover.prove(trace).map_err(|e| format!("{e:?}"))?;
    Ok((proof, pub_inputs))
}

/// Verify a relation proof against the public `(A', t, c, w')`.
pub fn verify_relation(proof: Proof, pub_inputs: RelationPublicInputs) -> Result<(), String> {
    let acceptable = winterfell::AcceptableOptions::OptionSet(vec![crate::proof_options()]);
    winterfell::verify::<
        RelationAir,
        Blake3_256<BaseElement>,
        DefaultRandomCoin<Blake3_256<BaseElement>>,
        MerkleTree<Blake3_256<BaseElement>>,
    >(proof, pub_inputs, &acceptable)
    .map_err(|e| format!("{e:?}"))
}
