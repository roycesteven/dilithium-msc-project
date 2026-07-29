//! AIR for the LAS response norm bound `||z||inf <= B` -- constraint (1) of
//! `base_verify`, and the first sound, completable slice of the on-chain
//! verification relation to arithmetise.
//!
//! WHAT THIS IS. A SOUND range-check gadget over the LAS-sized response `z`:
//! the prover knows `Z_COEFFS` centred coefficients (`z`, private witness) each
//! in `[-B, B]`. Layout: one coefficient per trace row. For row value `v` we
//! store `u = v + B in [0, 2B]` and prove the double range bound via two 19-bit
//! decompositions:
//!
//!   * `u = sum_i b_i 2^i`,      `b_i in {0,1}`   => `0 <= u < 2^19`
//!   * `2B - u = sum_i d_i 2^i`, `d_i in {0,1}`   => `0 <= 2B - u < 2^19`
//!
//! Both together force `0 <= u <= 2B`, i.e. `|v| <= B` (since `2B < 2^19`).
//!
//! WHAT THIS IS NOT (known gap, do not overclaim). This is NOT yet a complete
//! proof of on-chain verification. It commits to `z` (via the STARK trace root
//! inside the proof) and proves `z` is in range, but it does NOT bind that `z`
//! to the public statement `(A', t, c_tilde, M)` -- i.e. it does not prove that
//! this `z` is the one satisfying `w' = z_top + A'*z_bot - c*t` and
//! `c_tilde = SHAKE256(pack(t)||pack(w')||M)`. That binding is the arithmetic +
//! SHAKE256 relation, folded into the AIR in a later stage. Until then this is a
//! range-check gadget, not a stand-alone signature-verification proof.
//!
//! Soundness guards enforced here (besides the range constraints):
//!   * trace length is pinned to `next_pow2(Z_COEFFS)`, so the proof is for
//!     exactly the LAS-sized `z` (rows `0..Z_COEFFS` carry `z`; the remaining
//!     rows are range-valid `v = 0` padding that cannot weaken the `||z||inf`
//!     claim);
//!   * the bound is pinned to `2B` (`pub_inputs.two_b` must equal `2*B`), so a
//!     verifier cannot be tricked into checking a looser bound.

use winterfell::{
    math::{fields::f64::BaseElement, FieldElement, ToElements},
    Air, AirContext, Assertion, EvaluationFrame, ProofOptions, TraceInfo,
    TransitionConstraintDegree,
};

use crate::params::{B, RANGE_BITS, TWO_B, Z_COEFFS};

/// Trace width: `u` + `RANGE_BITS` bits of `u` + `RANGE_BITS` bits of `2B - u`.
pub const TRACE_WIDTH: usize = 1 + 2 * RANGE_BITS;

/// The pinned trace length (power of two >= Z_COEFFS).
pub const TRACE_LEN: usize = Z_COEFFS.next_power_of_two(); // 4096

/// Public inputs: the (pinned) bound `2B`. It is carried explicitly so it is
/// absorbed into the Fiat-Shamir transcript AND checked equal to `2*B` in
/// `NormAir::new`, so neither prover nor verifier can substitute a looser bound.
#[derive(Clone)]
pub struct PublicInputs {
    pub two_b: BaseElement,
}

impl PublicInputs {
    /// The one canonical value: `2*B`.
    pub fn canonical() -> Self {
        Self { two_b: BaseElement::new(TWO_B as u64) }
    }
}

impl ToElements<BaseElement> for PublicInputs {
    fn to_elements(&self) -> Vec<BaseElement> {
        vec![self.two_b]
    }
}

pub struct NormAir {
    context: AirContext<BaseElement>,
    two_b: BaseElement,
}

impl Air for NormAir {
    type BaseField = BaseElement;
    type PublicInputs = PublicInputs;

    fn new(trace_info: TraceInfo, pub_inputs: PublicInputs, options: ProofOptions) -> Self {
        // Guard #2: the trace must be exactly the LAS-sized `z` (pinned length).
        assert_eq!(TRACE_WIDTH, trace_info.width(), "unexpected trace width");
        assert_eq!(TRACE_LEN, trace_info.length(), "trace length must be next_pow2(Z_COEFFS)");
        // Guard #3: the bound must be exactly 2*B; no looser bound is accepted.
        assert_eq!(
            pub_inputs.two_b,
            BaseElement::new(TWO_B as u64),
            "bound must be exactly 2*B"
        );

        // Booleanity of every bit (degree 2), then the two linear recompositions
        // (degree 1). Order MUST match `evaluate_transition` below.
        let mut degrees = vec![TransitionConstraintDegree::new(2); 2 * RANGE_BITS];
        degrees.push(TransitionConstraintDegree::new(1)); // recompose u
        degrees.push(TransitionConstraintDegree::new(1)); // recompose 2B - u
        let num_assertions = 1;

        NormAir {
            context: AirContext::new(trace_info, degrees, num_assertions, options),
            two_b: pub_inputs.two_b,
        }
    }

    fn evaluate_transition<E: FieldElement + From<Self::BaseField>>(
        &self,
        frame: &EvaluationFrame<E>,
        _periodic_values: &[E],
        result: &mut [E],
    ) {
        let cur = frame.current();
        let u = cur[0];

        // (a) booleanity of the u-bits: b*(b-1) = 0.
        for i in 0..RANGE_BITS {
            let b = cur[1 + i];
            result[i] = b * (b - E::ONE);
        }
        // (b) booleanity of the (2B-u)-bits.
        for i in 0..RANGE_BITS {
            let d = cur[1 + RANGE_BITS + i];
            result[RANGE_BITS + i] = d * (d - E::ONE);
        }
        // (c) recompositions: sum_i bit_i * 2^i.
        let mut acc_u = E::ZERO;
        let mut acc_w = E::ZERO;
        for i in 0..RANGE_BITS {
            let two_i = E::from(1u32 << (i as u32));
            acc_u += cur[1 + i] * two_i;
            acc_w += cur[1 + RANGE_BITS + i] * two_i;
        }
        // u = sum b_i 2^i
        result[2 * RANGE_BITS] = acc_u - u;
        // (2B - u) = sum d_i 2^i  <=>  acc_w + u - 2B = 0
        result[2 * RANGE_BITS + 1] = acc_w + u - E::from(self.two_b);
    }

    fn get_assertions(&self) -> Vec<Assertion<Self::BaseField>> {
        // The last row is a v = 0 sentinel (not covered by the transition
        // divisor); pin its `u` column to B so the trace is anchored.
        let last = self.trace_length() - 1;
        vec![Assertion::single(0, last, BaseElement::new(B as u64))]
    }

    fn context(&self) -> &AirContext<Self::BaseField> {
        &self.context
    }
}
