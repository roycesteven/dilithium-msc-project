//! Measurement: per-phase execution time and per-message communication cost.
//!
//! Meeting-7 replaced gas with **time + communication cost**, and required
//! communication to include *off-chain* protocol messages, not only what reaches
//! a chain. Both are recorded here.
//!
//! # Conventions inherited from Stage 1
//!
//! * **Sample** standard deviation (divisor `n − 1`), matching the C drivers'
//!   `stats()` and the Rust `bench_levels` example. Population SD understates
//!   the error bars at small run counts.
//! * **At least five repetitions**, enforced at construction — the statistical
//!   floor this project has used since Meeting 3.
//! * **Per-operation timing is primary.** A total is derived for context, never
//!   presented as the headline.

use std::collections::BTreeMap;
use std::time::Duration;

/// The statistical floor: never report a mean over fewer runs than this.
pub const MIN_RUNS: usize = 5;

/// Mean and sample standard deviation of a set of per-run measurements.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Stats {
    pub mean_us: f64,
    pub sd_us: f64,
    pub n: usize,
}

impl Stats {
    /// Sample mean and `n − 1` standard deviation, in microseconds.
    pub fn from_durations(runs: &[Duration]) -> Self {
        let n = runs.len();
        assert!(n > 0, "Stats over an empty sample");
        let xs: Vec<f64> = runs.iter().map(|d| d.as_secs_f64() * 1e6).collect();
        let mean = xs.iter().sum::<f64>() / n as f64;
        let sd = if n > 1 {
            let var = xs.iter().map(|x| (x - mean).powi(2)).sum::<f64>() / (n - 1) as f64;
            var.sqrt()
        } else {
            0.0
        };
        Self { mean_us: mean, sd_us: sd, n }
    }
}

/// The protocol phases timed independently, ordered as Fig. 1 performs them.
///
/// `Ext` is adaptor-only: it has no analogue in an ordinary signature scheme.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
pub enum Phase {
    /// `KeyGen` for both parties plus `Gen()` for the statement/witness pair.
    KeyMaterial,
    /// `pi ← P(...)` by `u_1`.
    Prove,
    /// `PreSign` by `u_1` over `tx_1`.
    PreSignU1,
    /// `u_2` verifying `pi` — the first half of Fig. 1's abort gate.
    ProofVerify,
    /// `u_2` running `PreVerify` on `sigma_hat_1` — the second half of the gate.
    PreVerifyU1,
    /// `PreSign` by `u_2` over `tx_2`.
    PreSignU2,
    /// `u_1` adapting `sigma_hat_2` into the publishable `sigma_2`.
    AdaptU1,
    /// Chain 2 validating and accepting `tx_2`.
    SettleChain2,
    /// `u_2` extracting the witness from the published `sigma_2`.
    Ext,
    /// `u_2` adapting `sigma_hat_1` with the extracted witness.
    AdaptU2,
    /// Chain 1 validating and accepting `tx_1`.
    SettleChain1,
}

impl Phase {
    pub const ALL: [Phase; 11] = [
        Phase::KeyMaterial,
        Phase::Prove,
        Phase::PreSignU1,
        Phase::ProofVerify,
        Phase::PreVerifyU1,
        Phase::PreSignU2,
        Phase::AdaptU1,
        Phase::SettleChain2,
        Phase::Ext,
        Phase::AdaptU2,
        Phase::SettleChain1,
    ];

    pub fn label(self) -> &'static str {
        match self {
            Phase::KeyMaterial => "KeyGen x2 + Gen",
            Phase::Prove => "Prove (pi)",
            Phase::PreSignU1 => "PreSign (u1, tx1)",
            Phase::ProofVerify => "ProofVerify (pi)",
            Phase::PreVerifyU1 => "PreVerify (u2 gate)",
            Phase::PreSignU2 => "PreSign (u2, tx2)",
            Phase::AdaptU1 => "Adapt (u1 -> sigma2)",
            Phase::SettleChain2 => "Settle chain 2",
            Phase::Ext => "Ext (u2)",
            Phase::AdaptU2 => "Adapt (u2 -> sigma1)",
            Phase::SettleChain1 => "Settle chain 1",
        }
    }

    /// Whether this phase exists only because a role-A proof is present. Lets
    /// the driver report the proof system's share of the protocol honestly.
    pub fn is_proof_phase(self) -> bool {
        matches!(self, Phase::Prove | Phase::ProofVerify)
    }
}

/// Timings of one complete swap.
#[derive(Debug, Clone, Default)]
pub struct SwapTiming {
    pub phases: BTreeMap<Phase, Duration>,
}

impl SwapTiming {
    /// Time `f`, record it under `phase`, and return its value.
    pub fn measure<T>(&mut self, phase: Phase, f: impl FnOnce() -> T) -> T {
        let t0 = std::time::Instant::now();
        let out = f();
        self.phases.insert(phase, t0.elapsed());
        out
    }

    pub fn total(&self) -> Duration {
        self.phases.values().sum()
    }
}

/// Who sent a message, and over what medium.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Direction {
    /// Off-chain, `u_1` → `u_2`.
    U1ToU2,
    /// Off-chain, `u_2` → `u_1`.
    U2ToU1,
    /// Published on a chain — visible to everyone, and the mechanism by which
    /// the witness leaks.
    OnChain(&'static str),
}

impl Direction {
    pub fn is_onchain(self) -> bool {
        matches!(self, Direction::OnChain(_))
    }

    pub fn label(self) -> String {
        match self {
            Direction::U1ToU2 => "u1 -> u2 (off-chain)".to_string(),
            Direction::U2ToU1 => "u2 -> u1 (off-chain)".to_string(),
            Direction::OnChain(c) => format!("published on {c}"),
        }
    }
}

/// One item transmitted, and its size.
#[derive(Debug, Clone)]
pub struct Message {
    pub name: &'static str,
    pub direction: Direction,
    pub bytes: usize,
}

/// Every byte a swap puts on the wire, off-chain and on-chain.
///
/// Sizes come from the actual buffers the protocol produced, never from a
/// formula — that is the point of the byte-oriented backend interface.
#[derive(Debug, Clone, Default)]
pub struct Transcript {
    pub messages: Vec<Message>,
}

impl Transcript {
    pub fn record(&mut self, name: &'static str, direction: Direction, bytes: usize) {
        self.messages.push(Message { name, direction, bytes });
    }

    pub fn offchain_bytes(&self) -> usize {
        self.messages.iter().filter(|m| !m.direction.is_onchain()).map(|m| m.bytes).sum()
    }

    pub fn onchain_bytes(&self) -> usize {
        self.messages.iter().filter(|m| m.direction.is_onchain()).map(|m| m.bytes).sum()
    }

    pub fn total_bytes(&self) -> usize {
        self.messages.iter().map(|m| m.bytes).sum()
    }

    /// Bytes attributable to the role-A proof alone.
    pub fn proof_bytes(&self) -> usize {
        self.messages.iter().filter(|m| m.name == "pi").map(|m| m.bytes).sum()
    }
}

/// Per-run byte counts for one quantity, reduced only as a whole.
///
/// Deliberately the *only* way this module exposes a byte range. Subtotals are
/// computed inside each run and then reduced across runs; a range built by
/// summing per-message minima would describe a swap that never happened.
#[derive(Debug, Clone)]
pub struct ByteSeries {
    per_run: Vec<usize>,
}

impl ByteSeries {
    fn new(per_run: Vec<usize>) -> Self {
        assert!(!per_run.is_empty(), "ByteSeries over no runs");
        Self { per_run }
    }

    pub fn min(&self) -> usize {
        *self.per_run.iter().min().expect("non-empty")
    }

    pub fn max(&self) -> usize {
        *self.per_run.iter().max().expect("non-empty")
    }

    pub fn mean(&self) -> f64 {
        self.per_run.iter().sum::<usize>() as f64 / self.per_run.len() as f64
    }

    /// True when every run produced the same count — the case the Stage-1
    /// convention "communication sizes are fixed, report once" assumes.
    pub fn is_fixed(&self) -> bool {
        self.min() == self.max()
    }

    /// Formatted for a table: a single number when fixed, a range otherwise.
    pub fn display(&self) -> String {
        if self.is_fixed() {
            format!("{}", self.min())
        } else {
            format!("{}..{}", self.min(), self.max())
        }
    }
}

/// One row of the communication table, aggregated over every run.
#[derive(Debug, Clone)]
pub struct CommRow {
    pub name: &'static str,
    pub direction: Direction,
    pub bytes: ByteSeries,
}

/// Communication cost aggregated across every run of a configuration.
///
/// The Stage-1 convention reports communication **once, without dispersion**,
/// because object sizes are fixed by the parameter set. That is an assumption
/// about the scheme, not a licence to measure one run and print it: a proof
/// system with variable-length output, or an encoder whose size depends on the
/// value, would silently invalidate it. Every run is therefore recorded, the
/// message *sequence* must be identical across runs, and [`Self::varies`] says
/// plainly whether "report once" was justified here.
#[derive(Debug, Clone)]
pub struct CommSummary {
    pub rows: Vec<CommRow>,
    pub runs: usize,
    pub offchain: ByteSeries,
    pub onchain: ByteSeries,
    pub total: ByteSeries,
    pub proof: ByteSeries,
}

impl CommSummary {
    /// Aggregate transcripts.
    ///
    /// Panics if the runs disagree about *which* messages were sent, or in what
    /// order — a protocol-level inconsistency, not something to average away.
    pub fn new(transcripts: &[Transcript]) -> Self {
        assert!(!transcripts.is_empty(), "CommSummary over no runs");
        let first = &transcripts[0];

        for (i, t) in transcripts.iter().enumerate().skip(1) {
            assert_eq!(
                t.messages.len(),
                first.messages.len(),
                "run {i} sent {} messages, run 0 sent {}",
                t.messages.len(),
                first.messages.len()
            );
            for (j, (a, b)) in t.messages.iter().zip(first.messages.iter()).enumerate() {
                assert_eq!(
                    a.name, b.name,
                    "run {i} message {j} is `{}`, run 0 had `{}`",
                    a.name, b.name
                );
                assert_eq!(
                    a.direction, b.direction,
                    "run {i} message {j} (`{}`) travelled a different way than in run 0",
                    a.name
                );
            }
        }

        let rows = (0..first.messages.len())
            .map(|j| CommRow {
                name: first.messages[j].name,
                direction: first.messages[j].direction,
                bytes: ByteSeries::new(
                    transcripts.iter().map(|t| t.messages[j].bytes).collect(),
                ),
            })
            .collect();

        // Each subtotal is evaluated inside a run first, then reduced.
        Self {
            rows,
            runs: transcripts.len(),
            offchain: ByteSeries::new(transcripts.iter().map(|t| t.offchain_bytes()).collect()),
            onchain: ByteSeries::new(transcripts.iter().map(|t| t.onchain_bytes()).collect()),
            total: ByteSeries::new(transcripts.iter().map(|t| t.total_bytes()).collect()),
            proof: ByteSeries::new(transcripts.iter().map(|t| t.proof_bytes()).collect()),
        }
    }

    /// Whether any message varied in size between runs.
    pub fn varies(&self) -> bool {
        self.rows.iter().any(|r| !r.bytes.is_fixed())
    }
}

/// Per-phase statistics aggregated over several swaps.
#[derive(Debug, Clone)]
pub struct Aggregate {
    pub per_phase: BTreeMap<Phase, Stats>,
    pub total: Stats,
    pub runs: usize,
}

impl Aggregate {
    /// Aggregate per-phase timings across runs.
    ///
    /// # Invariants enforced
    ///
    /// * **At least [`MIN_RUNS`] runs.** A mean over fewer repetitions is not a
    ///   defensible figure, and silently reporting one would be worse than
    ///   failing loudly.
    /// * **Every phase is present in *all* runs or in *none*.** A phase that
    ///   appeared in only some runs would otherwise be averaged over a smaller,
    ///   silently different sample than its neighbours — and, worse, over the
    ///   subset of swaps that happened to reach it, which is exactly the subset
    ///   that behaved differently. Such a mean is not comparable with the rest
    ///   of the table, so it is rejected rather than printed. Configurations
    ///   legitimately differ in *which* phases exist (a swap with no role-A
    ///   proof has no `Prove`), and that is the "absent in all runs" case.
    pub fn new(timings: &[SwapTiming]) -> Self {
        assert!(
            timings.len() >= MIN_RUNS,
            "need at least {MIN_RUNS} runs for mean +/- SD, got {}",
            timings.len()
        );

        let mut per_phase = BTreeMap::new();
        for phase in Phase::ALL {
            let present = timings.iter().filter(|t| t.phases.contains_key(&phase)).count();
            if present == 0 {
                continue; // this configuration does not have that phase at all
            }
            assert_eq!(
                present,
                timings.len(),
                "phase `{}` was timed in {present} of {} runs; a phase must be present in every \
                 run or in none, otherwise its mean is taken over a different sample than the \
                 rest of the table",
                phase.label(),
                timings.len()
            );
            let runs: Vec<Duration> = timings.iter().map(|t| t.phases[&phase]).collect();
            per_phase.insert(phase, Stats::from_durations(&runs));
        }

        let totals: Vec<Duration> = timings.iter().map(|t| t.total()).collect();
        Self { per_phase, total: Stats::from_durations(&totals), runs: timings.len() }
    }

    /// Combined mean of the two proof phases, or `None` when the configuration
    /// carries no role-A proof.
    pub fn proof_share_us(&self) -> Option<f64> {
        let sum: f64 =
            self.per_phase.iter().filter(|(p, _)| p.is_proof_phase()).map(|(_, s)| s.mean_us).sum();
        (sum > 0.0).then_some(sum)
    }
}
