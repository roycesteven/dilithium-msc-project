//! R1CS circuit for the role-A relation, over BN254.
//!
//! Proves, in zero knowledge:
//!
//! ```text
//!     exists r : A r = t  (mod q)   and   ||r||inf <= 1
//! ```
//!
//! the **same** statement `Lazer` proves. Dropping the norm bound would give a
//! weaker claim that does not close the knowledge gap eprint 2020/845 §4.1
//! depends on, and configurations 2 and 3 would stop being comparable.
//!
//! # Why this is expressible at all
//!
//! `v -> A v` is **linear**, and `A` is public. So although the underlying
//! objects are polynomials in `R_q = Z_q[X]/(X^d + 1)`, the whole relation is a
//! fixed matrix–vector product over `Z_q` with *public* coefficients — and a
//! linear combination with public coefficients costs **no** multiplication
//! constraint in R1CS. What remains to constrain is only:
//!
//! 1. the ternary bound on each witness coefficient, and
//! 2. the reduction mod `q`.
//!
//! That is what makes an otherwise hostile-looking lattice statement tractable.
//! It would *not* be tractable if the relation involved products of two secret
//! polynomials.
//!
//! # Why the low-level API
//!
//! The rows are dense: each of the `ROWS = 1536` constraints touches all
//! `COLS = 2816` witness variables. Accumulating such a row with `FpVar`
//! arithmetic (`acc = acc + coeff * var`) is **quadratic** — every `+` clones
//! and extends the accumulated `LinearCombination`, so one row costs
//! `O(COLS^2)` and the circuit as a whole `O(ROWS · COLS^2) ≈ 10^10` operations.
//! That is not slow, it is a hang.
//!
//! Building each row directly as a [`LinearCombination`] and enforcing it once
//! is `O(COLS)` per row, i.e. `O(ROWS · COLS) ≈ 4.3·10^6` in total. The circuit
//! is therefore written against `ConstraintSystemRef` rather than the gadget
//! layer. Everything else (`FpVar`, `Boolean`) would have been more idiomatic
//! and is simply unusable at this density.
//!
//! # Recovering `A`
//!
//! `A` lives inside `PublicParams` in the NTT domain and is not exported. Since
//! the map is linear, evaluating it on unit vectors recovers it column by
//! column: column `j` of the composite matrix `M` is `A · e_j`. That is what
//! [`CompositeMatrix::extract`] does, through the additive `relation::apply_a`
//! accessor, so the circuit encodes exactly the map the signature scheme uses
//! rather than a re-derivation that could drift.
//!
//! # Handling the modulus
//!
//! Constraints live in BN254's scalar field `F_p`, `p ≈ 2^254`, while the
//! relation is mod `q = 8380417 ≈ 2^23`. A row's integer value is bounded by
//! `COLS·(q−1) < 2^35`, so it never wraps in `F_p` and the congruence can be
//! written explicitly with a quotient witness:
//!
//! ```text
//!     sum_j M[o][j]·r_j  −  t_o  −  q·k_o  =  0
//! ```
//!
//! `k_o` **must** be range-checked: without it a prover could pick `k_o` freely
//! and satisfy the equation for an arbitrary `r`, making the proof vacuous. It
//! is offset into `[0, 2^KBITS)` and bit-decomposed.

use ark_bn254::Fr;
use ark_ff::{BigInteger, PrimeField};
use ark_relations::lc;
use ark_relations::r1cs::{
    ConstraintSynthesizer, ConstraintSystemRef, LinearCombination, SynthesisError, Variable,
};

use fips204::relation;
use fips204::serialize::{unpack_statement, unpack_witness, SECRET_KEY_BYTES, STATEMENT_BYTES};
use fips204::setup::{PublicParams, D, N, N_PLUS_ELL};

/// The LAS modulus.
pub const Q: u64 = 8_380_417;

/// Rows of the relation: `n·d` output coefficients.
pub const ROWS: usize = N * D;
/// Columns: `(n+ℓ)·d` witness coefficients.
pub const COLS: usize = N_PLUS_ELL * D;

/// Bits used for the offset quotient `k' = k + K_OFFSET`.
///
/// A row's integer value lies in `±COLS·(q−1) ≈ ±2^34.5`, so after subtracting
/// `t_o ∈ [0,q)` the quotient satisfies `|k| ≤ COLS + 1 = 2817`. Offsetting by
/// `2^13 = 8192` puts `k'` in `[5375, 11009] ⊂ [0, 2^14)`, so 14 bits suffice
/// with margin on both sides.
pub const KBITS: usize = 14;
/// Offset added to the quotient so it can be bit-decomposed as a non-negative.
pub const K_OFFSET: i128 = 1 << 13;

/// The composite matrix `M` with `t = M r mod q`, in the coefficient domain.
///
/// Row-major, `ROWS × COLS`, entries canonical in `[0, q)`.
pub struct CompositeMatrix {
    entries: Vec<u32>,
}

impl CompositeMatrix {
    #[inline]
    pub fn at(&self, row: usize, col: usize) -> u32 {
        self.entries[row * COLS + col]
    }

    /// Recover `M` from `pp` by probing the linear map with unit vectors.
    ///
    /// One-time, and performed in the backend's constructor — never inside a
    /// timed `prove`/`verify`.
    pub fn extract(pp: &PublicParams) -> Self {
        let mut entries = vec![0u32; ROWS * COLS];
        for col in 0..COLS {
            let unit = unit_witness_bytes(col);
            let witness = unpack_witness(&unit).expect("a unit vector is ternary, so it decodes");
            let packed = fips204::serialize::pack_statement(&relation::apply_a(pp, &witness));
            for (row, value) in statement_coefficients(&packed).into_iter().enumerate() {
                entries[row * COLS + col] = value;
            }
        }
        Self { entries }
    }
}

/// Encode the unit vector `e_col` in the ternary wire format.
///
/// The codec stores `c + 1` in two bits, LSB-first, so an all-zero vector is
/// every field set to `1` (bytes `0x55`); position `col` is then set to `2`.
fn unit_witness_bytes(col: usize) -> [u8; SECRET_KEY_BYTES] {
    let mut out = [0x55u8; SECRET_KEY_BYTES];
    let byte = col / 4;
    let shift = (col % 4) * 2;
    out[byte] &= !(0b11 << shift);
    out[byte] |= 0b10 << shift;
    out
}

/// Decode a packed statement into its `ROWS` canonical coefficients (23 bits
/// each, LSB-first — the same bit stream `serialize.rs` uses for a public key).
fn statement_coefficients(packed: &[u8; STATEMENT_BYTES]) -> Vec<u32> {
    const BITS: usize = 23;
    let mut out = Vec::with_capacity(ROWS);
    let mut bp = 0usize;
    for _ in 0..ROWS {
        let mut v = 0u32;
        for i in 0..BITS {
            v |= u32::from((packed[bp >> 3] >> (bp & 7)) & 1) << i;
            bp += 1;
        }
        out.push(v);
    }
    out
}

/// Witness `r`, as `COLS` centred ternary coefficients.
pub fn witness_coefficients(packed: &[u8; SECRET_KEY_BYTES]) -> Option<Vec<i8>> {
    let mut out = Vec::with_capacity(COLS);
    let mut bp = 0usize;
    for _ in 0..COLS {
        let lo = (packed[bp >> 3] >> (bp & 7)) & 1;
        bp += 1;
        let hi = (packed[bp >> 3] >> (bp & 7)) & 1;
        bp += 1;
        let code = lo | (hi << 1);
        if code > 2 {
            return None; // invalid code 3
        }
        out.push(code as i8 - 1);
    }
    Some(out)
}

/// Decode a packed statement to the circuit's public input vector.
pub fn public_input_from_bytes(statement: &[u8]) -> Option<Vec<Fr>> {
    let packed: [u8; STATEMENT_BYTES] = statement.try_into().ok()?;
    unpack_statement(&packed)?; // validate, then use the raw coefficients
    Some(statement_coefficients(&packed).into_iter().map(Fr::from).collect())
}

/// The circuit. `matrix` and `t` are public; `r` is the secret witness, absent
/// during setup when only the constraint *shape* matters.
pub struct RelationCircuit<'a> {
    pub matrix: &'a CompositeMatrix,
    pub t: Vec<Fr>,
    pub r: Option<Vec<i8>>,
}

impl ConstraintSynthesizer<Fr> for RelationCircuit<'_> {
    fn generate_constraints(self, cs: ConstraintSystemRef<Fr>) -> Result<(), SynthesisError> {
        emit_instance(&cs, self.matrix, &self.t, self.r.as_deref())
    }
}

/// `k` independent instances of the relation, sharing one matrix, in a single
/// proof — the amortisation experiment (`bin/bench_amortise.rs`).
///
/// A party opens a swap with a **fresh** statement each time, so today each swap
/// carries its own proof. Groth16's proof is three group elements whatever the
/// circuit proves, so `k` instances in one proof cost the same bytes as one —
/// the per-swap proof size falls as `1/k`. What does *not* amortise is the
/// proving work, which grows with the constraint count, and the verifier's
/// public input, which grows as `ROWS·k`. Measuring where those two curves cross
/// is the point of the experiment.
///
/// Every instance is emitted by the same [`emit_instance`] the single-instance
/// circuit uses, so the batched circuit proves exactly the conjunction of `k`
/// copies of the claim configuration 2 already proves — including the
/// load-bearing range checks. Nothing is relaxed to make the batch cheaper.
pub struct BatchedRelationCircuit<'a> {
    pub matrix: &'a CompositeMatrix,
    /// One `(t, r)` per instance. `r` is `None` during setup, when only the
    /// constraint *shape* matters.
    pub instances: Vec<(Vec<Fr>, Option<Vec<i8>>)>,
}

impl<'a> BatchedRelationCircuit<'a> {
    /// The shape-only circuit for `circuit_specific_setup`: `k` instances with
    /// zero public input and no witness.
    pub fn setup_shape(matrix: &'a CompositeMatrix, k: usize) -> Self {
        BatchedRelationCircuit {
            matrix,
            instances: (0..k).map(|_| (vec![Fr::from(0u64); ROWS], None)).collect(),
        }
    }
}

impl ConstraintSynthesizer<Fr> for BatchedRelationCircuit<'_> {
    fn generate_constraints(self, cs: ConstraintSystemRef<Fr>) -> Result<(), SynthesisError> {
        for (t, r) in &self.instances {
            emit_instance(&cs, self.matrix, t, r.as_deref())?;
        }
        Ok(())
    }
}

/// Emit the constraints for ONE instance of `∃r : M r = t mod q ∧ ‖r‖∞ ≤ 1`.
///
/// The single and batched circuits share this so there is exactly one
/// implementation of the relation: a batch cannot silently prove something
/// weaker than a single proof does.
fn emit_instance(
    cs: &ConstraintSystemRef<Fr>,
    matrix: &CompositeMatrix,
    t_in: &[Fr],
    r_in: Option<&[i8]>,
) -> Result<(), SynthesisError> {
    let q_fr = Fr::from(Q);
    let has_witness = r_in.is_some();
    let t = t_in;

    // Precompute every assignment once. Recomputing a row's quotient inside
    // each of its KBITS closures would repeat a COLS-term dot product 14
    // times per row for no reason.
    let r_i8: &[i8] = r_in.unwrap_or(&[]);
    let quotients: Vec<u64> = if has_witness {
        (0..ROWS).map(|o| row_quotient(matrix, o, r_i8, t)).collect()
    } else {
        Vec::new()
    };

    let val = |assigned: bool, v: Fr| -> Result<Fr, SynthesisError> {
        if assigned {
            Ok(v)
        } else {
            Err(SynthesisError::AssignmentMissing)
        }
    };

    // ---- witness: r, one variable per coefficient -----------------------
    let mut r_vars = Vec::with_capacity(COLS);
    for j in 0..COLS {
        r_vars.push(cs.new_witness_variable(|| {
            val(has_witness, i8_to_fr(r_i8[j]))
        })?);
    }

    // ---- ternary bound: r^3 = r  <=>  r(r-1)(r+1) = 0  <=>  r in {-1,0,1}
    // The norm bound ||r||inf <= 1 that §4.1 requires: proven, not assumed.
    for (j, &rv) in r_vars.iter().enumerate() {
        let sq = cs.new_witness_variable(|| {
            let x = i8_to_fr(r_i8.get(j).copied().unwrap_or(0));
            val(has_witness, x * x)
        })?;
        // r * r = sq
        cs.enforce_constraint(lc!() + rv, lc!() + rv, lc!() + sq)?;
        // sq * r = r   (i.e. r^3 = r)
        cs.enforce_constraint(lc!() + sq, lc!() + rv, lc!() + rv)?;
    }

    // ---- public input: t -------------------------------------------------
    let mut t_vars = Vec::with_capacity(ROWS);
    for o in 0..ROWS {
        let t_o = t[o];
        t_vars.push(cs.new_input_variable(|| Ok(t_o))?);
    }

    // ---- one linear row at a time ---------------------------------------
    for o in 0..ROWS {
        // Quotient bits, range-checking k' so it cannot be chosen freely.
        // Without this the row equation could be satisfied for ANY r and the
        // proof would establish nothing.
        let mut k_bits = Vec::with_capacity(KBITS);
        for b in 0..KBITS {
            let bit = cs.new_witness_variable(|| {
                let k = quotients.get(o).copied().unwrap_or(0);
                val(has_witness, Fr::from((k >> b) & 1))
            })?;
            // booleanity: bit * bit = bit
            cs.enforce_constraint(lc!() + bit, lc!() + bit, lc!() + bit)?;
            k_bits.push(bit);
        }

        // Build the row in ONE pass: O(COLS + KBITS), not O(COLS^2).
        //
        //   sum_j M[o][j]·r_j  −  t_o  −  q·(sum_b 2^b·bit_b − K_OFFSET) = 0
        let mut row = LinearCombination::<Fr>::zero();
        for (j, &rv) in r_vars.iter().enumerate() {
            let m = matrix.at(o, j);
            if m != 0 {
                row = row + (Fr::from(m), rv);
            }
        }
        row = row - (Fr::one_val(), t_vars[o]);
        for (b, &bit) in k_bits.iter().enumerate() {
            row = row - (q_fr * Fr::from(1u64 << b), bit);
        }
        // + q·K_OFFSET on the constant wire
        row = row + (q_fr * Fr::from(K_OFFSET as u64), Variable::One);

        // A linear constraint in R1CS form: row · 1 = 0.
        cs.enforce_constraint(row, lc!() + Variable::One, lc!())?;
    }

    Ok(())
}

/// `Fr::one()` without importing the whole `One` trait at every call site.
trait OneVal {
    fn one_val() -> Self;
}
impl OneVal for Fr {
    fn one_val() -> Self {
        Fr::from(1u64)
    }
}

fn i8_to_fr(v: i8) -> Fr {
    if v < 0 {
        -Fr::from((-v) as u64)
    } else {
        Fr::from(v as u64)
    }
}

/// The offset quotient `k' = k + K_OFFSET` for row `o`, computed over the
/// integers so the witness assignment is exact.
fn row_quotient(matrix: &CompositeMatrix, o: usize, r: &[i8], t: &[Fr]) -> u64 {
    let mut sum: i128 = 0;
    for (j, &rj) in r.iter().enumerate() {
        if rj != 0 {
            sum += i128::from(matrix.at(o, j)) * i128::from(rj);
        }
    }
    let t_o = i128::from(fr_to_u64(t[o]));
    let k = (sum - t_o) / i128::from(Q);
    debug_assert_eq!(sum - t_o, k * i128::from(Q), "row {o} is not congruent mod q");
    (k + K_OFFSET) as u64
}

fn fr_to_u64(v: Fr) -> u64 {
    let bytes = v.into_bigint().to_bytes_le();
    let mut buf = [0u8; 8];
    let n = bytes.len().min(8);
    buf[..n].copy_from_slice(&bytes[..n]);
    u64::from_le_bytes(buf)
}
