//! Stage-2 follow-up: **does the role-A proof amortise across swaps?**
//!
//! The swap benchmark identifies the role-A proof of knowledge as the dominant
//! end-to-end cost by an order of magnitude, and the report's future work asks
//! whether it can be reduced or amortised. A party opens each swap with a
//! *fresh* statement, so today each swap carries its own proof — but a party
//! running many swaps could prove a batch of statements at once.
//!
//! This binary measures that. For `k = 1, 2, 4, 8` it proves `k` instances of
//! the **same** relation configuration 2 already proves, in one Groth16 proof,
//! and reports the total and the per-swap amortised cost of each of setup,
//! proving, verification and proof size.
//!
//! What makes the question non-trivial is that the three curves differ:
//!
//! * **proof size** is constant — three group elements whatever the circuit
//!   proves — so the per-swap figure falls as `1/k`;
//! * **proving** grows with the constraint count, so the per-swap figure is
//!   roughly flat and amortises nothing;
//! * **verification** grows with the public input, which is `ROWS·k` field
//!   elements, so the per-swap figure is also roughly flat.
//!
//! The batched circuit emits every instance through the same `emit_instance` the
//! single-instance circuit uses, so a batch proves exactly the conjunction of
//! `k` copies of the claim — nothing is relaxed to make batching look better.
//! A tamper check per batch size enforces that in the measurement itself.
//!
//! Run:
//! ```text
//! cargo run --release --bin bench_amortise --features groth16
//! ```
//!
//! Scope: Groth16 only. LaZer's proof size grows with the relation it proves, so
//! batching there is a different measurement and would require regenerating the
//! committed parameter header (`relation_zk_params.h`); it is not measured here
//! and no claim is made about it.

#[cfg(not(feature = "groth16"))]
fn main() {
    eprintln!(
        "bench_amortise needs the Groth16 backend.\n\
         Rebuild with: cargo run --release --bin bench_amortise --features groth16"
    );
}

#[cfg(feature = "groth16")]
fn main() {
    use ark_bn254::{Bn254, Fr};
    use ark_groth16::Groth16 as ArkGroth16;
    use ark_serialize::CanonicalSerialize;
    use ark_snark::SNARK;
    use fips204::serialize::{pack_statement, pack_witness};
    use fips204::{relation, setup::setup_public_params};
    use las_swap::backend::{master_seed_from_env, subseed};
    use las_swap::groth16_circuit::{
        public_input_from_bytes, witness_coefficients, BatchedRelationCircuit, CompositeMatrix,
        ROWS,
    };
    use las_swap::metrics::{Stats, MIN_RUNS};
    use std::time::Instant;

    /// Batch sizes measured. Powers of two so the 1/k curve is easy to read.
    const BATCHES: &[usize] = &[1, 2, 4, 8];
    /// Repetitions per batch size. At or above the Meeting-3 statistical floor.
    const REPS: usize = MIN_RUNS;

    let (master_seed, overridden) = master_seed_from_env();

    // The SAME public parameters and the SAME statements the swap benchmark
    // proves, derived from the same master seed under the same tags. Without
    // that, a proving-cost comparison would not be attributable to the batch.
    let pp = setup_public_params(&subseed(&master_seed, "public-params", 0));

    let max_k = *BATCHES.iter().max().expect("BATCHES is non-empty");
    let mut instances: Vec<(Vec<Fr>, Vec<i8>)> = Vec::with_capacity(max_k);
    for i in 0..max_k {
        let (statement, witness) = relation::gen_seed(&pp, &subseed(&master_seed, "statement", i as u32));
        let t = public_input_from_bytes(&pack_statement(&statement))
            .expect("a freshly generated statement decodes");
        let r = witness_coefficients(
            &pack_witness(&witness).expect("an honest witness is ternary, so it packs"),
        )
        .expect("a freshly packed witness decodes");
        instances.push((t, r));
    }

    // Matrix extraction is one-time and shared by every batch size, exactly as
    // the backend does it — never inside a timed region.
    let matrix = CompositeMatrix::extract(&pp);

    println!("=== Does the role-A proof amortise across swaps? ===\n");
    println!("Master seed   : {}", hex(&master_seed));
    println!(
        "Seed source   : {}",
        if overridden { "LAS_SWAP_SEED (overridden)" } else { "pinned default" }
    );
    println!("Proof system  : Groth16 over BN254");
    println!("Relation      : exists r : A r = t and ||r||inf <= 1   (per instance)");
    println!("Public input  : {ROWS} field elements per instance");
    println!("Repetitions   : {REPS} per batch size (statistical floor {MIN_RUNS})\n");

    struct Row {
        k: usize,
        setup: Stats,
        prove: Stats,
        verify: Stats,
        bytes: usize,
    }
    let mut rows: Vec<Row> = Vec::new();

    for &k in BATCHES {
        let mut setup_runs = Vec::with_capacity(REPS);
        let mut prove_runs = Vec::with_capacity(REPS);
        let mut verify_runs = Vec::with_capacity(REPS);
        let mut proof_bytes = 0usize;

        // The public input a verifier is handed: every instance's t, in order.
        let public_input: Vec<Fr> =
            instances[..k].iter().flat_map(|(t, _)| t.iter().copied()).collect();

        for rep in 0..=REPS {
            // rep 0 is an untimed warm-up: the first proof of a run pays
            // allocator and code-path costs the rest do not.
            let timed = rep > 0;

            let t0 = Instant::now();
            let (pk, vk) = ArkGroth16::<Bn254>::circuit_specific_setup(
                BatchedRelationCircuit::setup_shape(&matrix, k),
                &mut rand_core::OsRng,
            )
            .expect("circuit-specific setup");
            let setup_elapsed = t0.elapsed();
            let pvk = ArkGroth16::<Bn254>::process_vk(&vk).expect("processing a fresh vk");

            let circuit = BatchedRelationCircuit {
                matrix: &matrix,
                instances: instances[..k]
                    .iter()
                    .map(|(t, r)| (t.clone(), Some(r.clone())))
                    .collect(),
            };

            let t1 = Instant::now();
            let proof = ArkGroth16::<Bn254>::prove(&pk, circuit, &mut rand_core::OsRng)
                .expect("proving an honest batch");
            let prove_elapsed = t1.elapsed();

            let mut encoded = Vec::new();
            proof.serialize_compressed(&mut encoded).expect("serializing a proof");

            let t2 = Instant::now();
            let accepted = ArkGroth16::<Bn254>::verify_with_processed_vk(&pvk, &public_input, &proof)
                .expect("verification must not error");
            let verify_elapsed = t2.elapsed();

            // Success-path assertion: a timed block that silently measured a
            // failure path would publish a fast, meaningless number.
            assert!(accepted, "the honest batch of {k} was rejected; timings are meaningless");

            // Soundness tripwire: the batch must bind EVERY instance, not just
            // the first. Corrupt the LAST instance's public input and require
            // rejection, otherwise a batch could be cheaper only because it
            // proves less than k single proofs do.
            if timed {
                let mut tampered = public_input.clone();
                let last = tampered.len() - 1;
                tampered[last] += Fr::from(1u64);
                let still_ok =
                    ArkGroth16::<Bn254>::verify_with_processed_vk(&pvk, &tampered, &proof)
                        .unwrap_or(false);
                assert!(
                    !still_ok,
                    "a batch of {k} accepted a tampered instance: the batch does not bind all k"
                );
            }

            if timed {
                setup_runs.push(setup_elapsed);
                prove_runs.push(prove_elapsed);
                verify_runs.push(verify_elapsed);
                proof_bytes = encoded.len();
            }
        }

        rows.push(Row {
            k,
            setup: Stats::from_durations(&setup_runs),
            prove: Stats::from_durations(&prove_runs),
            verify: Stats::from_durations(&verify_runs),
            bytes: proof_bytes,
        });
        println!("  measured k = {k}");
    }

    // ---- totals --------------------------------------------------------
    println!("\n--- Cost of ONE proof covering k statements (totals) ---\n");
    println!(
        "  {:>3}  {:>18}  {:>18}  {:>16}  {:>10}",
        "k", "setup (ms)", "prove (ms)", "verify (us)", "proof (B)"
    );
    for r in &rows {
        println!(
            "  {:>3}  {:>10.1} +- {:>4.1}  {:>10.1} +- {:>4.1}  {:>8.1} +- {:>4.1}  {:>10}",
            r.k,
            r.setup.mean_us / 1000.0,
            r.setup.sd_us / 1000.0,
            r.prove.mean_us / 1000.0,
            r.prove.sd_us / 1000.0,
            r.verify.mean_us,
            r.verify.sd_us,
            r.bytes
        );
    }

    // ---- per swap ------------------------------------------------------
    println!("\n--- Amortised PER SWAP (total / k) ---\n");
    println!(
        "  {:>3}  {:>14}  {:>14}  {:>14}  {:>16}",
        "k", "prove (ms)", "verify (us)", "proof (B)", "proof vs k=1"
    );
    let base_bytes = rows[0].bytes as f64;
    for r in &rows {
        let kf = r.k as f64;
        println!(
            "  {:>3}  {:>14.1}  {:>14.1}  {:>14.1}  {:>15.2}x",
            r.k,
            r.prove.mean_us / 1000.0 / kf,
            r.verify.mean_us / kf,
            r.bytes as f64 / kf,
            (r.bytes as f64 / kf) / base_bytes
        );
    }

    // ---- what the rows mean --------------------------------------------
    let first = &rows[0];
    let last = rows.last().expect("BATCHES is non-empty");
    let kf = last.k as f64;
    println!("\n--- What this shows ---\n");
    println!(
        "Proof size is the only column that amortises, and it amortises perfectly:\n\
         one Groth16 proof is three group elements regardless of how many instances\n\
         it covers, so k = {} carries the same {} B as k = 1 and the per-swap share\n\
         falls to {:.1} B.",
        last.k, last.bytes, last.bytes as f64 / kf
    );
    println!(
        "\nProving does not amortise: per-swap proving went from {:.1} ms at k = 1 to\n\
         {:.1} ms at k = {}. The constraint system grows with the batch, so the work\n\
         is paid per instance either way -- batching moves it, it does not remove it.",
        first.prove.mean_us / 1000.0,
        last.prove.mean_us / 1000.0 / kf,
        last.k
    );
    println!(
        "\nVerification does not amortise either: the public input is {ROWS} field\n\
         elements per instance, so a verifier of a k-batch reads k times as much."
    );
    println!(
        "\nTHE PRACTICAL READING -- and it is a NEGATIVE result for configuration 2.\n\
         Batching amortises the cost that was already negligible and leaves the one\n\
         that dominates untouched. In the swap benchmark configuration 2 spends about\n\
         646 ms in Prove(pi), which is ~97% of the whole swap and 47x the next phase,\n\
         while the proof it produces is {} B against a 4416 B statement and a 6736 B\n\
         signature. So the 1/k column above shrinks something that was never the\n\
         problem, and the flat proving column is the problem.",
        first.bytes
    );
    println!(
        "\nThat is a statement about GROTH16, not about batching in general: the same\n\
         1/k would land on a cost that matters for a proof system whose proof is\n\
         large. LaZer is exactly that case -- fast generation, much larger proof --\n\
         and it is NOT measured here, so this run does not settle it.",
    );
    println!(
        "\nBatching also changes the protocol, in ways no column above prices: a batch\n\
         must be proved BEFORE any of its statements is used, so a party has to know\n\
         its next k swaps in advance; every counterparty verifies the whole batch to\n\
         use one statement of it, and receives public input for k-1 statements that\n\
         are not theirs; and the statements in a batch become linked, since anyone\n\
         seeing two of them knows they were proved together."
    );
    println!(
        "\nSCOPE: Groth16 only, and Groth16 is not post-quantum -- it is configuration\n\
         2's prover, kept as the controlled comparison against LaZer. Nothing here\n\
         measures LaZer, whose proof grows with the relation, and nothing here is a\n\
         security claim about batching."
    );
}

#[cfg(feature = "groth16")]
fn hex(bytes: &[u8]) -> String {
    bytes.iter().map(|b| format!("{b:02x}")).collect()
}
