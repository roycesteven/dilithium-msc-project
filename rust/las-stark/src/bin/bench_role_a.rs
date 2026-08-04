//! **A succinct post-quantum role-A prover, measured against the two deployed ones.**
//!
//! The amortisation experiment closed the "make the existing proof cheaper"
//! direction for both deployed provers: Groth16's proof is a constant 128 B but
//! was never the bottleneck, and LaZer's ~30 KiB does shrink under batching, but
//! only by making per-swap proving and verification 3.3x worse. What that leaves
//! is a different KIND of system -- one that is post-quantum (Groth16 is not)
//! AND succinct (LaZer is not).
//!
//! This binary measures exactly that: a Winterfell FRI-STARK proving the SAME
//! statement, `exists r : A r = t' and ||r||inf <= 1`, and reports proof size,
//! prove time and verify time beside the two measured baselines.
//!
//! Run:
//! ```text
//! cargo run --release --bin bench_role_a
//! ```
//!
//! GATES (this binary exits non-zero if any fails)
//!   * the instance must satisfy the relation before anything is timed, so a
//!     proof can never be of a vacuous statement;
//!   * a success-path assertion after every timed proof;
//!   * a tampered-statement check per repetition -- a prover that verified
//!     against the wrong `t'` would be measuring nothing;
//!   * an untimed warm-up, and >= 5 repetitions.
//!
//! SCOPE: the STARK's parameters are Winterfell's via `las_stark::proof_options`
//! and no concrete-security analysis of this AIR has been done, so the figures
//! are an engineering data point, not a security claim. Winterfell's prover does
//! not add zero-knowledge randomisation, so this is a succinct ARGUMENT of
//! knowledge; a deployed use would need the zk variant and would pay for it.

use std::time::{Duration, Instant};

use las_stark::role_a_air::{
    prove_role_a, verify_role_a, RoleAInstance, RoleAPublicInputs, RoleAWitness, AUX_WIDTH,
    EV_LEN, MAIN_WIDTH,
};
use las_stark::vectors::VerifyVector;

/// Repetitions. At or above the Meeting-3 statistical floor.
const REPS: usize = 5;

/// Measured baselines for the SAME relation, quoted from this repository's
/// evidence so the comparison is against real runs and not folklore.
struct Baseline {
    name: &'static str,
    post_quantum: bool,
    transparent: bool,
    proof_bytes: usize,
    prove_ms: f64,
    verify_ms: f64,
    source: &'static str,
}

const BASELINES: &[Baseline] = &[
    Baseline {
        name: "Groth16 over BN254 (config 2)",
        post_quantum: false,
        transparent: false,
        proof_bytes: 128,
        prove_ms: 494.1,
        verify_ms: 12.7,
        source: "evidence/amortise/latest, k=1 row",
    },
    Baseline {
        name: "LaZer LNP22 (config 3)",
        post_quantum: true,
        transparent: true,
        proof_bytes: 30723,
        prove_ms: 158.8,
        verify_ms: 75.2,
        source: "evidence/lazer_amortise/latest, k=1 row",
    },
];

fn mean_sd(xs: &[f64]) -> (f64, f64) {
    let n = xs.len() as f64;
    let mean = xs.iter().sum::<f64>() / n;
    let var = if xs.len() > 1 {
        xs.iter().map(|x| (x - mean).powi(2)).sum::<f64>() / (n - 1.0)
    } else {
        0.0
    };
    (mean, var.sqrt())
}

fn ms(d: Duration) -> f64 {
    d.as_secs_f64() * 1e3
}

fn main() {
    let dir = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "../../evm/test/vectors".to_string());
    let vv = VerifyVector::load(&dir)
        .unwrap_or_else(|e| panic!("load golden vectors from {dir}: {e}"));

    // The instance uses the REAL A' of the C build; r is sampled here and
    // t' = A r computed from it, exactly as relation_gen does.
    let inst = RoleAInstance::from_vector(&vv, 0xA5A5_1234);
    let pub_inputs = RoleAPublicInputs::from_instance(&inst);

    // Gate: prove nothing until the instance is known to be in the relation.
    let w = RoleAWitness::build(&inst, &pub_inputs)
        .expect("the generated instance must satisfy A r = t' with r ternary");
    assert!(
        w.r.iter().all(|p| p.iter().all(|&v| (-1..=1).contains(&v))),
        "witness must be ternary before anything is timed"
    );

    println!("=== A succinct post-quantum proof for the role-A relation ===\n");
    println!("Relation      : exists r : A r = t'  and  ||r||inf <= 1");
    println!("Proof system  : Winterfell FRI-STARK over Goldilocks (Blake3)");
    println!("Trace         : {EV_LEN} rows x ({MAIN_WIDTH} main + {AUX_WIDTH} aux) columns");
    println!("Setup         : transparent (public coin) -- no trusted setup");
    println!("Repetitions   : {REPS}, after an untimed warm-up\n");

    let mut prove_runs = Vec::with_capacity(REPS);
    let mut verify_runs = Vec::with_capacity(REPS);
    let mut proof_bytes = 0usize;

    for rep in 0..=REPS {
        let timed = rep > 0;

        let t0 = Instant::now();
        let (proof, pi) = prove_role_a(&inst).expect("proving an honest instance");
        let prove_elapsed = t0.elapsed();

        let bytes = proof.to_bytes();

        let t1 = Instant::now();
        let ok = verify_role_a(proof, pi.clone()).is_ok();
        let verify_elapsed = t1.elapsed();

        // Success-path assertion: never publish a timing taken off a rejection.
        assert!(ok, "an honest role-A proof was rejected; timings are meaningless");

        // Soundness tripwire: the proof must be bound to its statement.
        if timed {
            let mut tampered = pi;
            tampered.t_prime[0][0] += 1;
            let reproof =
                prove_role_a(&inst).expect("re-prove for the tamper check").0;
            assert!(
                verify_role_a(reproof, tampered).is_err(),
                "a role-A proof verified against a tampered t': it is not bound to its statement"
            );

            prove_runs.push(ms(prove_elapsed));
            verify_runs.push(ms(verify_elapsed));
            proof_bytes = bytes.len();
        }
    }

    let (p_mean, p_sd) = mean_sd(&prove_runs);
    let (v_mean, v_sd) = mean_sd(&verify_runs);

    println!("--- Measured ---\n");
    println!("  proof size    : {proof_bytes} B");
    println!("  prove         : {p_mean:.1} +- {p_sd:.1} ms");
    println!("  verify        : {v_mean:.1} +- {v_sd:.1} ms\n");

    println!("--- Against the two deployed provers, same relation ---\n");
    println!(
        "  {:<30} {:>10} {:>12} {:>12}  {:^4} {:^4}",
        "prover", "proof (B)", "prove (ms)", "verify (ms)", "PQ", "TS"
    );
    for b in BASELINES {
        println!(
            "  {:<30} {:>10} {:>12.1} {:>12.1}  {:^4} {:^4}",
            b.name,
            b.proof_bytes,
            b.prove_ms,
            b.verify_ms,
            if b.post_quantum { "yes" } else { "NO" },
            if b.transparent { "yes" } else { "NO" }
        );
    }
    println!(
        "  {:<30} {:>10} {:>12.1} {:>12.1}  {:^4} {:^4}",
        "FRI-STARK (this binary)", proof_bytes, p_mean, v_mean, "yes", "yes"
    );
    println!("\n  PQ = post-quantum   TS = transparent (no trusted setup)");
    for b in BASELINES {
        println!("  baseline source: {:<30} {}", b.name, b.source);
    }

    // ---- reading -------------------------------------------------------
    let lazer = &BASELINES[1];
    let groth = &BASELINES[0];
    println!("\n--- What this shows ---\n");

    let vs_lazer = proof_bytes as f64 / lazer.proof_bytes as f64;
    if proof_bytes < lazer.proof_bytes {
        println!(
            "Against the only other POST-QUANTUM prover here, the STARK proof is {:.2}x the\n\
             size of LaZer's ({} B vs {} B) -- a {:.0}% reduction, and unlike batching it\n\
             costs nothing per swap because it is a property of the proof system, not of\n\
             how many swaps are bundled.",
            vs_lazer,
            proof_bytes,
            lazer.proof_bytes,
            100.0 * (1.0 - vs_lazer)
        );
    } else {
        println!(
            "Against the only other POST-QUANTUM prover here, the STARK proof is {:.2}x the\n\
             size of LaZer's ({} B vs {} B). Succinctness is asymptotic: at this statement\n\
             size the FRI commitment overhead has not yet paid for itself, which is the\n\
             honest reading and not a defect of the construction.",
            vs_lazer, proof_bytes, lazer.proof_bytes
        );
    }

    println!(
        "\nAgainst Groth16 the proof is far larger ({} B vs {} B): a constant-size pairing\n\
         proof is unbeatable on bytes. But Groth16 is NOT post-quantum and needs a trusted\n\
         per-circuit setup, which is precisely why configuration 3 exists -- so the\n\
         comparison that matters for a post-quantum swap is the LaZer row.",
        proof_bytes, groth.proof_bytes
    );

    println!(
        "\nTime: prove {:.1} ms vs LaZer's {:.1} ms, verify {:.1} ms vs {:.1} ms.",
        p_mean, lazer.prove_ms, v_mean, lazer.verify_ms
    );

    println!(
        "\nSCOPE: no concrete-security analysis of this AIR has been done, and Winterfell's\n\
         prover adds no zero-knowledge randomisation -- this is a succinct ARGUMENT of\n\
         knowledge, not a zk one, and a deployed use would pay extra for zk. The relation\n\
         proven is exactly the deployed one (same ternary bound, same A'), so the SIZES\n\
         and TIMES are comparable; the security levels are not claimed to be."
    );
}
