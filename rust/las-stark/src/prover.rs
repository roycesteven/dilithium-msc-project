//! Winterfell prover/verifier wiring for `NormAir`: build the execution trace
//! from a response `z`, produce a Blake3/FRI STARK proof, and verify it. The
//! associated-type block mirrors the Winterfell 0.13 default prover stack
//! (Blake3 hash, Merkle vector commitment, DEEP-FRI).

use winterfell::{
    crypto::{hashers::Blake3_256, DefaultRandomCoin, MerkleTree},
    math::{fields::f64::BaseElement, FieldElement},
    matrix::ColMatrix,
    AuxRandElements, CompositionPoly, CompositionPolyTrace, ConstraintCompositionCoefficients,
    DefaultConstraintCommitment, DefaultConstraintEvaluator, DefaultTraceLde, PartitionOptions,
    Proof, ProofOptions, Prover, ProverError, StarkDomain, TraceInfo, TracePolyTable,
    TraceTable,
};

use crate::air::{NormAir, PublicInputs, TRACE_LEN, TRACE_WIDTH};
use crate::params::{B, RANGE_BITS, TWO_B, Z_COEFFS};
use crate::vectors::{centred, Poly};

/// Build the range-check trace from the response `z` (n+l polynomials).
/// Rows `0..Z_COEFFS` carry `z`'s centred coefficients; the remaining rows are
/// range-valid `v = 0` padding up to the pinned power-of-two length.
pub fn build_norm_trace(z: &[Poly]) -> TraceTable<BaseElement> {
    let mut flat: Vec<i64> = Vec::with_capacity(Z_COEFFS);
    for p in z {
        for &v in p.iter() {
            flat.push(centred(v));
        }
    }
    assert_eq!(flat.len(), Z_COEFFS, "expected exactly (n+l)*d response coefficients");
    flat.resize(TRACE_LEN, 0); // v = 0 sentinels

    let mut trace = TraceTable::new(TRACE_WIDTH, TRACE_LEN);
    for (row, &v) in flat.iter().enumerate() {
        // For honest z, v in [-B, B] so u, w = 2B - u are both in [0, 2B].
        let u = (v + B) as u64;
        let w = (TWO_B - (v + B)) as u64;
        trace.set(0, row, BaseElement::new(u));
        for i in 0..RANGE_BITS {
            trace.set(1 + i, row, BaseElement::new((u >> i) & 1));
            trace.set(1 + RANGE_BITS + i, row, BaseElement::new((w >> i) & 1));
        }
    }
    trace
}

pub struct NormProver {
    options: ProofOptions,
}

impl NormProver {
    pub fn new(options: ProofOptions) -> Self {
        Self { options }
    }
}

impl Prover for NormProver {
    type BaseField = BaseElement;
    type Air = NormAir;
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

    fn get_pub_inputs(&self, _trace: &Self::Trace) -> PublicInputs {
        // The one canonical public input: the pinned bound 2B.
        PublicInputs::canonical()
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

/// Prove `||z||inf <= B` for the response `z`. Returns a Blake3/FRI STARK proof.
pub fn prove_norm(z: &[Poly]) -> Result<Proof, ProverError> {
    let trace = build_norm_trace(z);
    let prover = NormProver::new(crate::proof_options());
    prover.prove(trace)
}

/// Verify a proof against the supplied public inputs. Returns `Err` on any
/// verification failure (and `NormAir::new` rejects a bound other than `2B`).
pub fn verify_with_pub_inputs(proof: Proof, pub_inputs: PublicInputs) -> Result<(), String> {
    let acceptable = winterfell::AcceptableOptions::OptionSet(vec![crate::proof_options()]);
    winterfell::verify::<
        NormAir,
        Blake3_256<BaseElement>,
        DefaultRandomCoin<Blake3_256<BaseElement>>,
        MerkleTree<Blake3_256<BaseElement>>,
    >(proof, pub_inputs, &acceptable)
    .map_err(|e| format!("{e:?}"))
}

/// Verify a proof against the canonical bound `2B`.
pub fn verify_norm(proof: Proof) -> Result<(), String> {
    verify_with_pub_inputs(proof, PublicInputs::canonical())
}
