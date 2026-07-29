//! STARK argument for ONE negacyclic convolution `p = a (X) b` in
//! `Z_q[X]/(X^d + 1)` -- the arithmetic primitive of the LAS verify equation
//! `w' = z_top + A'*z_bot - c*t`. `a` is a PUBLIC constant vector (baked into the
//! AIR), `b` is the PRIVATE witness, and the output `p` is pinned to the public
//! claimed result.
//!
//! ## IMPORTANT: reduced ring degree (Winterfell column cap)
//!
//! This gadget runs at `CONV_D = 64`, **NOT** the LAS ring degree `d = 256`.
//! Winterfell 0.13 caps a trace at **255 columns**, and the layout below carries
//! the whole `d`-coefficient window as columns, so `d = 256` (>= 256 columns) is
//! physically impossible in this layout. The AIR *logic* (rotation, reduction,
//! range-check) is degree-independent, so `d = 64` faithfully demonstrates and
//! validates it. Scaling to `d = 256` requires a NARROW layout instead:
//! a streaming multiply-accumulate with a LogUp/permutation lookup to bind the
//! reused operands, or an NTT-transform circuit. That is the documented next step.
//!
//! ## Layout (sound, uniform constraints -- no lookups, no periodic columns)
//!
//! Row `k` (k = 0..d) holds the negacyclically-rotated window `bwin_k`, where
//! `p[k] = sum_i a[i] * bwin_k[i]`,
//!   `p[k] = sum_{i<=k} a[i] b[k-i]  -  sum_{i>k} a[i] b[k-i+d]`,
//! so `bwin_0 = [b[0], -b[d-1], ..., -b[1]]` and the window advances by a
//! negacyclic rotation `bwin_{k+1}[i] = bwin_k[i-1]` (i>=1),
//! `bwin_{k+1}[0] = -bwin_k[d-1]`. The negacyclic sign lives in the field
//! (Goldilocks negatives), so the wrap is the degree-1 identity
//! `bwin_next[0] + bwin_cur[d-1] = 0`. As `a[i]` are constants, the dot is degree 1.
//!
//! ## Soundness: b must be a valid ring element
//!
//! The Goldilocks field alone does not force `b`'s coefficients to be valid
//! `mod q` residues, so we ALSO range-check `bwin[0] in [-(Q-1), Q-1]` on every
//! row (two 24-bit decompositions of `bwin[0]+(Q-1)` and `(Q-1)-bwin[0]`). Over
//! rows `0..d`, `bwin[0]` sweeps every coefficient of `b`, so this bounds the whole
//! witness `|b[k]| <= Q-1`. That makes the reduction sound: the true integer dot
//! then has `|D_k| < d*(Q-1)^2 < p/2` (Goldilocks prime `p ~ 2^64`), so its signed
//! value is unique in the field and `dot - p_out - quo*q = 0` with `p_out in [0,q)`
//! (two 23-bit decompositions) and a signed 32-bit quotient `quo = qs - 2^31` pins
//! the canonical residue. Every window `bwin_0` corresponds to a unique `b`, so
//! this proves "exists b: a (X) b = p".
//!
//! ## Scope
//! ONE convolution, `a` public and `b` private. The full verify equation composes
//! many such convolutions with the `z_top` term and a per-coefficient reduction,
//! and all `A'*z_bot` convolutions must SHARE the same witness `z_bot` -- a
//! cross-convolution binding this single gadget does not impose.

use winterfell::{
    crypto::{hashers::Blake3_256, DefaultRandomCoin, MerkleTree},
    math::{fields::f64::BaseElement, FieldElement, ToElements},
    matrix::ColMatrix,
    Air, AirContext, Assertion, AuxRandElements, CompositionPoly, CompositionPolyTrace,
    ConstraintCompositionCoefficients, DefaultConstraintCommitment, DefaultConstraintEvaluator,
    DefaultTraceLde, EvaluationFrame, PartitionOptions, Proof, ProofOptions, Prover, ProverError,
    StarkDomain, TraceInfo, TracePolyTable, TraceTable, TransitionConstraintDegree,
};

use crate::params::Q;

/// Reduced ring degree for this gadget (see the module note on the 255-column cap).
pub const CONV_D: usize = 64;

/// A degree-`CONV_D` polynomial, coefficients canonical `[0,Q)`.
pub type ConvPoly = [i64; CONV_D];

// --- bit widths ------------------------------------------------------------
const P_BITS: usize = 23; // p_out in [0, q) < 2^23
const QS_BITS: usize = 32; // shifted quotient qs = quo + 2^31 in [0, 2^32)
const U0_BITS: usize = 24; // bwin[0]+(Q-1) in [0, 2Q-2] < 2^24

// --- column layout ---------------------------------------------------------
const BWIN_OFF: usize = 0; // CONV_D columns: the rotated window
const P_COL: usize = CONV_D; // 1: reduced output p[k]
const P_BITS_OFF: usize = CONV_D + 1; // 23
const PC_BITS_OFF: usize = P_BITS_OFF + P_BITS; // 23
const QS_COL: usize = PC_BITS_OFF + P_BITS; // 1
const QS_BITS_OFF: usize = QS_COL + 1; // 32
const U0_BITS_OFF: usize = QS_BITS_OFF + QS_BITS; // 24
const U0C_BITS_OFF: usize = U0_BITS_OFF + U0_BITS; // 24
/// Total trace width (192 at CONV_D = 64; must stay <= 255).
pub const CONV_WIDTH: usize = U0C_BITS_OFF + U0_BITS;
/// Trace length: power of two > CONV_D so every output row is covered by the
/// transition divisor (which excludes only the last row). 128 at CONV_D = 64.
pub const CONV_LEN: usize = {
    let mut n = (CONV_D + 1).next_power_of_two();
    if n < 8 {
        n = 8;
    }
    n
};

const QUO_OFFSET: u64 = 1 << 31;

// --- transition-constraint index layout ------------------------------------
const N_ROT: usize = CONV_D;
const C_DOT: usize = N_ROT;
const C_PBOOL: usize = C_DOT + 1;
const C_PCBOOL: usize = C_PBOOL + P_BITS;
const C_QSBOOL: usize = C_PCBOOL + P_BITS;
const C_U0BOOL: usize = C_QSBOOL + QS_BITS;
const C_U0CBOOL: usize = C_U0BOOL + U0_BITS;
const C_PREC: usize = C_U0CBOOL + U0_BITS;
const C_PCREC: usize = C_PREC + 1;
const C_QSREC: usize = C_PCREC + 1;
const C_U0REC: usize = C_QSREC + 1;
const C_U0CREC: usize = C_U0REC + 1;
const N_CONSTRAINTS: usize = C_U0CREC + 1;

/// Native negacyclic convolution `p = a (X) b` over `Z_q[X]/(X^CONV_D + 1)`,
/// canonical `[0,Q)` output -- the oracle the AIR must match.
pub fn negacyclic_conv(a: &ConvPoly, b: &ConvPoly) -> ConvPoly {
    let qi = Q as i128;
    let mut acc = [0i128; CONV_D];
    for i in 0..CONV_D {
        for j in 0..CONV_D {
            let prod = (a[i] as i128) * (b[j] as i128) % qi;
            let k = i + j;
            if k < CONV_D {
                acc[k] = (acc[k] + prod) % qi;
            } else {
                acc[k - CONV_D] = (acc[k - CONV_D] - prod) % qi;
            }
        }
    }
    let mut out = [0i64; CONV_D];
    for k in 0..CONV_D {
        let mut v = (acc[k] % qi) as i64;
        if v < 0 {
            v += Q;
        }
        out[k] = v;
    }
    out
}

/// A deterministic public test instance `(a, b, out = a (X) b)` at `CONV_D`.
pub fn demo_instance() -> (ConvPoly, ConvPoly, ConvPoly) {
    let mut a = [0i64; CONV_D];
    let mut b = [0i64; CONV_D];
    for i in 0..CONV_D {
        a[i] = ((i as i64) * 2_654_435_761 + 12_345).rem_euclid(Q);
        b[i] = ((i as i64) * 40_503 + 7).rem_euclid(Q);
    }
    let out = negacyclic_conv(&a, &b);
    (a, b, out)
}

/// Public inputs: the constant operand `a` and the claimed output `p`.
#[derive(Clone)]
pub struct ConvPublicInputs {
    pub a: Vec<BaseElement>,
    pub p_out: Vec<BaseElement>,
}

impl ToElements<BaseElement> for ConvPublicInputs {
    fn to_elements(&self) -> Vec<BaseElement> {
        let mut v = self.a.clone();
        v.extend_from_slice(&self.p_out);
        v
    }
}

pub struct ConvAir {
    context: AirContext<BaseElement>,
    a: Vec<BaseElement>,
    p_out: Vec<BaseElement>,
}

impl Air for ConvAir {
    type BaseField = BaseElement;
    type PublicInputs = ConvPublicInputs;

    fn new(trace_info: TraceInfo, pub_inputs: ConvPublicInputs, options: ProofOptions) -> Self {
        assert_eq!(CONV_WIDTH, trace_info.width(), "unexpected conv trace width");
        assert_eq!(CONV_LEN, trace_info.length(), "unexpected conv trace length");
        assert_eq!(CONV_D, pub_inputs.a.len(), "a must have CONV_D coefficients");
        assert_eq!(CONV_D, pub_inputs.p_out.len(), "p_out must have CONV_D coefficients");

        let mut degrees = Vec::with_capacity(N_CONSTRAINTS);
        for _ in 0..(N_ROT + 1) {
            degrees.push(TransitionConstraintDegree::new(1)); // rotation + reduction
        }
        for _ in 0..(2 * P_BITS + QS_BITS + 2 * U0_BITS) {
            degrees.push(TransitionConstraintDegree::new(2)); // booleanity
        }
        for _ in 0..5 {
            degrees.push(TransitionConstraintDegree::new(1)); // recompositions
        }
        assert_eq!(degrees.len(), N_CONSTRAINTS);

        ConvAir {
            context: AirContext::new(trace_info, degrees, CONV_D, options),
            a: pub_inputs.a,
            p_out: pub_inputs.p_out,
        }
    }

    fn evaluate_transition<E: FieldElement + From<Self::BaseField>>(
        &self,
        frame: &EvaluationFrame<E>,
        _periodic_values: &[E],
        result: &mut [E],
    ) {
        let cur = frame.current();
        let nxt = frame.next();
        let two = |i: usize| E::from(1u32 << (i as u32));
        let qm1 = E::from(BaseElement::new((Q - 1) as u64));

        // (1) negacyclic rotation of the window
        for i in 1..CONV_D {
            result[i - 1] = nxt[BWIN_OFF + i] - cur[BWIN_OFF + i - 1];
        }
        result[CONV_D - 1] = nxt[BWIN_OFF] + cur[BWIN_OFF + CONV_D - 1];

        // (2) reduction: dot = sum_i a[i]*bwin[i] ; dot - p_out - quo*q = 0
        let mut dot = E::ZERO;
        for i in 0..CONV_D {
            dot += E::from(self.a[i]) * cur[BWIN_OFF + i];
        }
        let p = cur[P_COL];
        let qs = cur[QS_COL];
        let q = E::from(BaseElement::new(Q as u64));
        let offset = E::from(BaseElement::new(QUO_OFFSET));
        result[C_DOT] = dot - p - (qs - offset) * q;

        // (3) booleanity of the bit columns
        for i in 0..P_BITS {
            let b = cur[P_BITS_OFF + i];
            result[C_PBOOL + i] = b * (b - E::ONE);
        }
        for i in 0..P_BITS {
            let b = cur[PC_BITS_OFF + i];
            result[C_PCBOOL + i] = b * (b - E::ONE);
        }
        for i in 0..QS_BITS {
            let b = cur[QS_BITS_OFF + i];
            result[C_QSBOOL + i] = b * (b - E::ONE);
        }
        for i in 0..U0_BITS {
            let b = cur[U0_BITS_OFF + i];
            result[C_U0BOOL + i] = b * (b - E::ONE);
        }
        for i in 0..U0_BITS {
            let b = cur[U0C_BITS_OFF + i];
            result[C_U0CBOOL + i] = b * (b - E::ONE);
        }

        // (4) recompositions
        let mut acc_p = E::ZERO;
        let mut acc_pc = E::ZERO;
        for i in 0..P_BITS {
            acc_p += cur[P_BITS_OFF + i] * two(i);
            acc_pc += cur[PC_BITS_OFF + i] * two(i);
        }
        let mut acc_qs = E::ZERO;
        for i in 0..QS_BITS {
            acc_qs += cur[QS_BITS_OFF + i] * two(i);
        }
        let mut acc_u0 = E::ZERO;
        let mut acc_u0c = E::ZERO;
        for i in 0..U0_BITS {
            acc_u0 += cur[U0_BITS_OFF + i] * two(i);
            acc_u0c += cur[U0C_BITS_OFF + i] * two(i);
        }
        let bwin0 = cur[BWIN_OFF];
        result[C_PREC] = p - acc_p;
        result[C_PCREC] = (qm1 - p) - acc_pc;
        result[C_QSREC] = qs - acc_qs;
        result[C_U0REC] = (bwin0 + qm1) - acc_u0; // => bwin0 >= -(q-1)
        result[C_U0CREC] = (qm1 - bwin0) - acc_u0c; // => bwin0 <= q-1
    }

    fn get_assertions(&self) -> Vec<Assertion<Self::BaseField>> {
        (0..CONV_D).map(|k| Assertion::single(P_COL, k, self.p_out[k])).collect()
    }

    fn context(&self) -> &AirContext<Self::BaseField> {
        &self.context
    }
}

// --- trace construction ----------------------------------------------------

#[inline]
fn field_from_signed(x: i128) -> BaseElement {
    if x >= 0 {
        BaseElement::new(x as u64)
    } else {
        -BaseElement::new((-x) as u64)
    }
}

/// Build the execution trace for `p = a (X) b`. `a`, `b` canonical `[0,Q)`.
pub fn build_conv_trace(a: &ConvPoly, b: &ConvPoly) -> TraceTable<BaseElement> {
    let mut bw = [0i128; CONV_D];
    bw[0] = b[0] as i128;
    for i in 1..CONV_D {
        bw[i] = -(b[CONV_D - i] as i128);
    }

    let qi = Q as i128;
    let mut trace = TraceTable::new(CONV_WIDTH, CONV_LEN);
    for row in 0..CONV_LEN {
        let mut d: i128 = 0;
        for i in 0..CONV_D {
            d += (a[i] as i128) * bw[i];
        }
        let p_out = d.rem_euclid(qi);
        let quo = (d - p_out) / qi;
        let qs = quo + (QUO_OFFSET as i128);

        for i in 0..CONV_D {
            trace.set(BWIN_OFF + i, row, field_from_signed(bw[i]));
        }
        trace.set(P_COL, row, BaseElement::new(p_out as u64));
        for i in 0..P_BITS {
            trace.set(P_BITS_OFF + i, row, BaseElement::new(((p_out >> i) & 1) as u64));
        }
        let pc = (qi - 1) - p_out;
        for i in 0..P_BITS {
            trace.set(PC_BITS_OFF + i, row, BaseElement::new(((pc >> i) & 1) as u64));
        }
        trace.set(QS_COL, row, BaseElement::new(qs as u64));
        for i in 0..QS_BITS {
            trace.set(QS_BITS_OFF + i, row, BaseElement::new(((qs >> i) & 1) as u64));
        }
        let bwin0 = bw[0];
        let u0 = bwin0 + (qi - 1);
        let u0c = (qi - 1) - bwin0;
        for i in 0..U0_BITS {
            trace.set(U0_BITS_OFF + i, row, BaseElement::new(((u0 >> i) & 1) as u64));
        }
        for i in 0..U0_BITS {
            trace.set(U0C_BITS_OFF + i, row, BaseElement::new(((u0c >> i) & 1) as u64));
        }

        // rotate window for the next row
        let last = bw[CONV_D - 1];
        for i in (1..CONV_D).rev() {
            bw[i] = bw[i - 1];
        }
        bw[0] = -last;
    }
    trace
}

// --- prover ----------------------------------------------------------------

pub struct ConvProver {
    options: ProofOptions,
    a: Vec<BaseElement>,
    p_out: Vec<BaseElement>,
}

impl ConvProver {
    pub fn new(options: ProofOptions, a: Vec<BaseElement>, p_out: Vec<BaseElement>) -> Self {
        Self { options, a, p_out }
    }
}

impl Prover for ConvProver {
    type BaseField = BaseElement;
    type Air = ConvAir;
    type Trace = TraceTable<BaseElement>;
    type HashFn = Blake3_256<Self::BaseField>;
    type VC = MerkleTree<Self::HashFn>;
    type RandomCoin = DefaultRandomCoin<Self::HashFn>;
    type TraceLde<E: FieldElement<BaseField = Self::BaseField>> =
        DefaultTraceLde<E, Self::HashFn, Self::VC>;
    type ConstraintCommitment<E: FieldElement<BaseField = Self::BaseField>> =
        DefaultConstraintCommitment<E, Self::HashFn, Self::VC>;
    type ConstraintEvaluator<'a, E: FieldElement<BaseField = Self::BaseField>> =
        DefaultConstraintEvaluator<'a, Self::Air, E>;

    fn get_pub_inputs(&self, _trace: &Self::Trace) -> ConvPublicInputs {
        ConvPublicInputs { a: self.a.clone(), p_out: self.p_out.clone() }
    }

    fn options(&self) -> &ProofOptions {
        &self.options
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

fn poly_to_field(p: &ConvPoly) -> Vec<BaseElement> {
    p.iter().map(|&v| BaseElement::new((((v % Q) + Q) % Q) as u64)).collect()
}

/// Prove `p_out = a (X) b` (negacyclic, mod q). `a`, `p_out` public; `b` witness.
pub fn prove_conv(a: &ConvPoly, b: &ConvPoly, p_out: &ConvPoly) -> Result<Proof, ProverError> {
    let trace = build_conv_trace(a, b);
    let prover = ConvProver::new(crate::proof_options(), poly_to_field(a), poly_to_field(p_out));
    prover.prove(trace)
}

/// Verify a convolution proof against public `a` and claimed output `p_out`.
pub fn verify_conv(proof: Proof, a: &ConvPoly, p_out: &ConvPoly) -> Result<(), String> {
    let pub_inputs = ConvPublicInputs { a: poly_to_field(a), p_out: poly_to_field(p_out) };
    let acceptable = winterfell::AcceptableOptions::OptionSet(vec![crate::proof_options()]);
    winterfell::verify::<
        ConvAir,
        Blake3_256<BaseElement>,
        DefaultRandomCoin<Blake3_256<BaseElement>>,
        MerkleTree<Blake3_256<BaseElement>>,
    >(proof, pub_inputs, &acceptable)
    .map_err(|e| format!("{e:?}"))
}
