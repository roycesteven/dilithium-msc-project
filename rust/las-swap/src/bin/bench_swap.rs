//! Stage-2 benchmark: the three configurations of the Fig. 1 atomic swap,
//! compared on **execution time** and **communication cost**.
//!
//! Run:
//! ```text
//! cargo run --release --bin bench_swap --features secp256k1,relation-zk
//! ```
//!
//! Configurations whose backends are not linked are reported as unavailable and
//! skipped. Nothing is ever estimated or filled in.

use las_swap::backend::{configurations, master_seed_from_env, ConfigStatus, Configuration, RoleAProof};
use las_swap::metrics::{Aggregate, CommSummary, Phase, SwapTiming, MIN_RUNS};
use las_swap::protocol::{run_refund, run_swap};
use las_swap::utxo::Chain;

/// Swaps per configuration. Above the Meeting-3 statistical floor of 5.
const RUNS: usize = 10;

/// The two UTXO chains. Nominal block times differ the way Bitcoin's and
/// Litecoin's do; nothing in validation depends on them — they only let
/// timeouts be quoted in wall-clock terms.
const CHAIN1: (&str, u32) = ("chain 1 (Bitcoin-like, 600 s blocks)", 600);
const CHAIN2: (&str, u32) = ("chain 2 (Litecoin-like, 150 s blocks)", 150);

struct Measured<'a> {
    config: &'a Configuration,
    timing: Aggregate,
    comm: CommSummary,
}

fn main() {
    let set = configurations();
    let (master_seed, _) = master_seed_from_env();

    println!("=== Stage 2: atomic swap on UTXO chains (eprint 2020/845 Fig. 1) ===\n");
    println!("Master seed        : {}", hex(&master_seed));
    println!(
        "Seed source        : {}",
        if set.seed_overridden { "LAS_SWAP_SEED (overridden)" } else { "pinned default" }
    );
    println!("Swaps per config   : {RUNS}  (statistical floor {MIN_RUNS})");
    println!("Chains             : {} <-> {}", CHAIN1.0, CHAIN2.0);
    println!("LAS parameter setup: {:?}  (one-time, shared by configs 2 and 3)", set.las_setup);
    println!(
        "\nTiming tier        : PACKED — every operation includes wire encode/decode.\n\
         \x20                     Not comparable with Stage-1 core-tier numbers.\n"
    );

    let mut measured: Vec<Measured> = Vec::new();

    for config in &set.configs {
        println!("--------------------------------------------------------------------");
        println!("Configuration {}: {}", config.number, config.label);
        println!("  signature : {}  [{}]", config.scheme.name(), config.scheme.param_note());
        match &config.role_a {
            RoleAProof::Required(z) => {
                println!("  role-A pi : {}", z.name());
                println!("              {}", z.note());
                println!("              setup: {} ({:?})", z.setup_kind().describe(), z.setup_duration());
            }
            RoleAProof::NotRequired { reason } => {
                println!("  role-A pi : none required");
                println!("              {reason}");
            }
        }
        println!(
            "  binding   : {} ({})",
            config.scheme.binding().short_hex(),
            config.scheme.relation().describe()
        );
        println!("  fully PQ  : {}", yes_no(config.fully_post_quantum()));

        match config.status() {
            ConfigStatus::Ready => {}
            ConfigStatus::BackendMissing(m) => {
                println!("\n  NOT MEASURED — backend unavailable:\n    {m}\n");
                continue;
            }
            ConfigStatus::RelationMismatch { scheme, zkp } => {
                println!(
                    "\n  NOT MEASURED — the proof system does not speak about this scheme's \
                     statements:\n    scheme proves over {}\n    zkp proves over    {}\n",
                    scheme.describe(),
                    zkp.describe()
                );
                continue;
            }
            ConfigStatus::BindingMismatch => {
                println!(
                    "\n  NOT MEASURED — same relation but different public parameters: the proof \
                     would concern a different A than the signatures use.\n"
                );
                continue;
            }
        }

        match measure(config, &master_seed) {
            Ok((timing, comm)) => {
                report_config(config, &timing, &comm);
                measured.push(Measured { config, timing, comm });
            }
            Err(e) => println!("\n  RUN FAILED: {e}\n"),
        }
    }

    report_comparison(&measured);
}

/// Run `RUNS` swaps of one configuration and aggregate them.
fn measure(config: &Configuration, master_seed: &[u8; 32]) -> Result<(Aggregate, CommSummary), String> {
    let scheme = config.scheme.as_ref();
    let mut timings = Vec::with_capacity(RUNS);
    let mut transcripts = Vec::with_capacity(RUNS);

    // Run-validity gate, on its own batch — see `check_rejection_gate`.
    check_rejection_gate(scheme, master_seed)?;

    for iteration in 0..RUNS as u32 {
        let mut timing = SwapTiming::default();

        // Key material is derived deterministically from the pinned seed, and is
        // itself a timed phase (Fig. 1 runs Gen() inside the protocol).
        let inputs = timing
            .measure(Phase::KeyMaterial, || scheme.derive_inputs(master_seed, iteration))
            .ok_or_else(|| format!("iteration {iteration}: could not derive inputs"))?;

        let mut chain1 = Chain::new(CHAIN1.0, scheme, CHAIN1.1);
        let mut chain2 = Chain::new(CHAIN2.0, scheme, CHAIN2.1);

        let outcome = run_swap(config, &inputs, &mut chain1, &mut chain2, &mut timing)
            .map_err(|e| format!("iteration {iteration}: {e:?}"))?;

        if !outcome.extracted_matches {
            return Err(format!(
                "iteration {iteration}: extracted witness != generated witness — the \
                 Section 4.1 M-SIS uniqueness argument did not hold"
            ));
        }

        timings.push(timing);
        transcripts.push(outcome.transcript);
    }

    // Timeout/refund edge case, once per configuration, outside the timed set.
    {
        let inputs = scheme
            .derive_inputs(master_seed, u32::MAX)
            .ok_or("refund scenario: could not derive inputs")?;
        let mut chain = Chain::new(CHAIN1.0, scheme, CHAIN1.1);
        run_refund(config, &inputs, &mut chain, 144).map_err(|e| format!("refund: {e:?}"))?;
    }

    Ok((Aggregate::new(&timings), CommSummary::new(&transcripts)))
}

/// PreSign calls used by the rejection gate.
///
/// Sized so the gate can actually resolve breakage. Attempts per call are
/// geometric with `p = 1/E`, so the per-call SD is `sqrt(1-p)/p ≈ 2.22` at the
/// D3 set; over `n` calls the 5-sigma half-width is `5 · 2.22 / sqrt(n)`.
///
/// * at `n = 20` the band is ±2.48 — wider than the distance from the expected
///   2.775 to a **completely broken** loop that never rejects (1.0), so such a
///   loop would pass. Useless.
/// * at `n = 2000` the band is ±0.248, and a never-rejecting loop misses by
///   1.775 ≈ 36 sigma.
///
/// The swap loop itself makes only `2 × RUNS` PreSign calls, so the gate runs on
/// its own batch rather than piggybacking on it. That also keeps it independent
/// of `RUNS`, which is chosen for timing stability, not for this.
const GATE_CALLS: usize = 2000;

/// The project's standing run-validity gate: the measured rejection-loop rate
/// must agree with the closed form.
///
/// If the sampler were broken, or the bound wrong, the attempt rate would move —
/// so agreement is evidence that the sampler is healthy and the parameters are
/// what the scheme thinks they are. Schemes with no rejection loop (ECDSA) are
/// skipped rather than assigned a rate of 1.0, which would imply a loop that
/// does not exist.
///
/// Messages vary across the batch: `presign` takes the deterministic path, so a
/// fixed message would repeat one attempt count instead of sampling the
/// distribution.
fn check_rejection_gate(
    scheme: &dyn las_swap::backend::AdaptorScheme,
    master_seed: &[u8; 32],
) -> Result<(), String> {
    let Some(expected) = scheme.expected_presign_attempts() else {
        return Ok(()); // no rejection loop in this scheme
    };
    let Some(before) = scheme.presign_attempts_counter() else {
        return Err("scheme declares an expected attempt rate but exposes no counter".into());
    };

    let inputs = scheme
        .derive_inputs(master_seed, 0)
        .ok_or("rejection gate: could not derive inputs")?;

    for i in 0..GATE_CALLS {
        let msg = format!("las-swap/rejection-gate/{i}");
        scheme
            .presign(msg.as_bytes(), &inputs.statement, &inputs.pk1, &inputs.sk1)
            .ok_or_else(|| format!("rejection gate: PreSign failed at call {i}"))?;
    }

    let after = scheme
        .presign_attempts_counter()
        .ok_or("rejection gate: counter disappeared mid-batch")?;
    let measured = after.saturating_sub(before) as f64 / GATE_CALLS as f64;

    let p = 1.0 / expected;
    let sigma = (1.0 - p).sqrt() / p;
    let tolerance = 5.0 * sigma / (GATE_CALLS as f64).sqrt();

    println!(
        "\n  Rejection gate: {measured:.4} attempts/PreSign over {GATE_CALLS} calls\n\
         \x20                 expected {expected:.5}, tolerance +/- {tolerance:.4} (5 sigma)"
    );

    if (measured - expected).abs() > tolerance {
        return Err(format!(
            "rejection gate FAILED: {measured:.4} attempts/PreSign vs expected {expected:.5} \
             (tolerance +/- {tolerance:.4} over {GATE_CALLS} calls)"
        ));
    }
    Ok(())
}

fn report_config(config: &Configuration, agg: &Aggregate, comm: &CommSummary) {
    println!("\n  Execution time, per protocol phase (mean +/- sample SD over {} swaps):", agg.runs);
    for phase in Phase::ALL {
        if let Some(s) = agg.per_phase.get(&phase) {
            println!("    {:<22} {:>12.1} +/- {:>9.1} us", phase.label(), s.mean_us, s.sd_us);
        }
    }
    println!(
        "    {:<22} {:>12.1} +/- {:>9.1} us   (context only, not the headline)",
        "end-to-end", agg.total.mean_us, agg.total.sd_us
    );
    if let Some(p) = agg.proof_share_us() {
        println!(
            "    role-A proof share     {:>12.1} us   ({:.1}% of end-to-end)",
            p,
            100.0 * p / agg.total.mean_us
        );
    }
    println!(
        "    one-time setup         signature {:?}, proof {}",
        config.scheme.setup_duration(),
        match &config.role_a {
            RoleAProof::Required(z) => format!("{:?}", z.setup_duration()),
            RoleAProof::NotRequired { .. } => "n/a".to_string(),
        }
    );
    println!("      (setup is excluded from every phase above)");

    println!(
        "\n  Communication cost, per message (observed over {} swaps{}):",
        comm.runs,
        if comm.varies() { ", sizes VARY between runs" } else { ", identical in every run" }
    );
    for row in &comm.rows {
        println!(
            "    {:<22} {:>12} B   {}",
            row.name,
            row.bytes.display(),
            row.direction.label()
        );
    }
    println!("    {:-<22} {:->14}", "", "");
    println!("    {:<22} {:>12} B   off-chain", "subtotal", comm.offchain.display());
    println!("    {:<22} {:>12} B   on-chain", "subtotal", comm.onchain.display());
    println!("    {:<22} {:>12} B   per swap", "TOTAL", comm.total.display());

    println!("\n  Object sizes (fixed per scheme, reported once):");
    let s = config.scheme.as_ref();
    println!("    public key {:>8} B   secret key {:>8} B", s.pk_bytes(), s.sk_bytes());
    println!("    statement  {:>8} B   witness    {:>8} B", s.statement_bytes(), s.witness_bytes());
    println!(
        "    signature  {:>8} B   pre-signature {:>5} B  (overhead {} B)",
        s.signature_bytes(),
        s.pre_signature_bytes(),
        s.presignature_overhead_bytes()
    );
    println!();
}

fn report_comparison(measured: &[Measured]) {
    println!("====================================================================");

    if measured.len() < 2 {
        println!(
            "Only {} configuration(s) measured — no comparison table.\n\
             Link the missing backends (see the notes above) and re-run.",
            measured.len()
        );
        return;
    }

    println!("Comparison\n");
    println!("{:<7} {:<34} {:>15} {:>14}", "Config", "Stack", "End-to-end us", "Bytes/swap");
    for m in measured {
        println!(
            "{:<7} {:<34} {:>15.1} {:>14}",
            m.config.number,
            truncate(m.config.label, 34),
            m.timing.total.mean_us,
            m.comm.total.display()
        );
    }

    // The one controlled comparison: same signature, same A, same inputs.
    println!();
    let las: Vec<&Measured> = measured.iter().filter(|m| m.config.number >= 2).collect();
    if las.len() == 2 {
        let (a, b) = (las[0], las[1]);
        println!("Controlled comparison {} -> {} (identical signature, A and inputs;", a.config.number, b.config.number);
        println!("only the role-A proof system differs):");
        println!(
            "  time   {:>12.1} -> {:>12.1} us   ({:+.1}%)",
            a.timing.total.mean_us,
            b.timing.total.mean_us,
            100.0 * (b.timing.total.mean_us - a.timing.total.mean_us) / a.timing.total.mean_us
        );
        println!(
            "  bytes  {:>12} -> {:>12}      ({:+.1}%)",
            a.comm.total.display(),
            b.comm.total.display(),
            100.0 * (b.comm.total.mean() - a.comm.total.mean()) / a.comm.total.mean()
        );
        println!("  proof  {:>12} -> {:>12} B", a.comm.proof.display(), b.comm.proof.display());
    } else {
        println!("Controlled comparison 2 -> 3 unavailable: both LAS configurations");
        println!("must be measured for the proof-system delta to be attributable.");
    }

    println!(
        "\nNote: 1 -> 2/3 changes the signature scheme, the relation, and whether a\n\
         role-A proof exists at all. Report it as classical baseline versus\n\
         post-quantum stack, not as the cost of the signature alone; the\n\
         signature-only comparison is the Stage-1 result (docs/LAS.md 8.3)."
    );
}

fn hex(bytes: &[u8]) -> String {
    bytes.iter().map(|b| format!("{b:02x}")).collect()
}

fn yes_no(b: bool) -> &'static str {
    if b {
        "yes"
    } else {
        "no"
    }
}

fn truncate(s: &str, n: usize) -> String {
    if s.chars().count() <= n {
        s.to_string()
    } else {
        s.chars().take(n - 1).chain("…".chars()).collect()
    }
}
