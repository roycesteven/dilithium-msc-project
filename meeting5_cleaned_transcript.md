# Contextually Cleaned Transcript — LAS Benchmarking, C/Rust Implementations, and Next Steps

**Source:** `Voice 260706_160020_original.txt` + uploaded audio file `Voice(1).mp3`  
**Date implied by filename:** 2026-07-06  
**Type:** meaning-preserving cleaned transcript, not a legal/verbatim transcript.  
**Main topic:** WSL benchmark validity, LAS versus simplified Dilithium benchmark presentation, C and Rust implementations, code-structure explanation, reproducibility, and next-stage blockchain integration.

---

## A. Key contextual corrections

| Noisy ASR phrase | Corrected meaning |
|---|---|
| bell metal | bare metal |
| nooks | Linux |
| WS / windows system | WSL / WSL2 on Windows |
| final reformer | final report |
| configuration of youth | machine / toolchain configuration |
| last / glass | LAS — Lattice-based Adaptor Signature |
| lithium / delete film / digital | Dilithium / simplified Dilithium / ML-DSA context |
| MLVSA / misstandard | ML-DSA |
| rest / Rs / Rastability | Rust |
| NDT / indemn | NTT |
| random bites | randombytes |
| F EPS 2202 | FIPS 202 / SHAKE / Keccak primitives |
| public case / Pablo Kay | public key |
| private K | private key / secret key |
| adaptive signature | adaptor signature |
| prayer / peace sign | PreSign / pre-signing |
| vertical / verification | Verify / verification |
| extract function | Ext / witness extraction |
| samurai | summary / summarise |
| rate of meat | README |
| easy DSA / ECD S.A. | ECDSA |
| automatic swamp / autonomic swab | atomic swap |
| ck proof / Samsung proof | ZK proof / proof that witness is valid and small |
| Foundry / private | local EVM / private chain using Foundry |

---

## B. Meeting summary

The meeting focused less on adding new code and more on making the existing implementation and benchmarks **understandable, reproducible, and defensible**.

Wang confirmed that running benchmarks under **WSL is acceptable**, as long as every compared scheme is run on the same machine and the report clearly states the machine configuration, OS/WSL environment, compiler, and benchmark setup.

The supervisor’s strongest feedback was about **presentation**. The project already has many numbers and at least two implementations, but Wang cannot easily verify correctness unless the work is explained from a high level first. He wants a diagram or short presentation that starts from the basic signature API — `KeyGen`, `Sign`, `Verify` — then shows how LAS adds or modifies the flow with `PreSign`, `PreVerify`, `Adapt`, and `Ext`.

He also wants a clear comparison between the **existing base implementation** and the **modified LAS implementation**. This should not start from low-level files immediately. It should first explain which components are reused, which are modified, and which are newly added. Low-level details such as NTT, SHAKE/FIPS 202, and random sampling can be shown as reused primitives unless they were actually modified.

On benchmarking, Wang accepted the use of Criterion-style benchmarking for Rust, especially if it reports distributions, sample counts, means, and standard deviations. For C, if the same framework is not available, repeated runs with scripts are acceptable. Communication sizes are fixed, but timing results must report averages and variation.

The rejection-sampling discussion was important. The student explained the expected attempt count of around 2.7 and acceptance around 36.8%. Wang said this can be an interesting insight for the report, but it must be explained in the student’s own words and supported by both the theoretical calculation and the measured implementation result.

For comparison with classical adaptor signatures, Wang suggested adding columns that show the increase compared with ECDSA/Schnorr-style classical adaptor signatures, both for communication cost and computation cost. The numbers can be used only after verification and with clear caveats about implementation and security levels.

For the next week, Wang asked for a concise summary: one or two slides are enough. The goal is to convince him that the Stage 1 implementation is correct, that the methodology is clear, and that the results are organised. After that, the project can move to the blockchain stage: local/private-chain implementation, probably using Foundry, first with a classical adaptor-signature workflow and then replacing the signature component with LAS.

---

## C. Action items

1. **Keep WSL results if all schemes are benchmarked on the same machine.**
   - State WSL/Windows/Linux details clearly in the report.
   - Include CPU, RAM if possible, OS, compiler, flags, and benchmark framework.

2. **Create a high-level diagram of the implementation.**
   - Start from basic signature APIs: `KeyGen`, `Sign`, `Verify`.
   - Then show LAS APIs: `KeyGen`, `Sign`, `Verify`, `PreSign`, `PreVerify`, `Adapt`, `Ext`.
   - Highlight reused primitives versus modified/new functions.

3. **Prepare a repository-structure diagram.**
   - Show the original base implementation.
   - Show the C LAS implementation.
   - Show the Rust LAS implementation.
   - Mark where primitives such as NTT, SHAKE/FIPS 202, randombytes, and polynomial arithmetic are reused.

4. **Summarise what changed from simplified Dilithium to LAS.**
   - Do this at a high level first.
   - Then add file/function details only after the conceptual flow is clear.

5. **Explain the benchmark methodology clearly.**
   - For Rust/Criterion: report sample count, warm-up, mean, median, SD, and confidence intervals if available.
   - For C: either use an equivalent repeated-run setup or script repeated executions and calculate mean ± SD.

6. **Report rejection sampling properly.**
   - Explain the expected attempt count, e.g. around 2.7 attempts.
   - Explain the acceptance rate, e.g. around 36.8%.
   - Show the measured attempts from implementation, not only the theoretical value.

7. **Reduce and organise results.**
   - Do not show every figure at once.
   - Select the most important results: computation cost, communication cost, rejection attempts, and C/Rust comparison.
   - Move extra details to appendix or supporting logs.

8. **Add comparison with classical adaptor signatures.**
   - Include ECDSA/Schnorr adaptor-signature numbers if verified.
   - Add columns showing overhead relative to the classical baseline.
   - State security-level and implementation caveats clearly.

9. **Improve reproducibility documentation.**
   - Write a short README with only the key commands.
   - Keep an extended README or appendix for full details.

10. **Next meeting preparation.**
    - Prepare one or two slides.
    - Include a high-level diagram, key modifications, benchmark summary, and links/references to the code.

11. **After Stage 1 is accepted, move to Stage 2.**
    - Implement or reproduce a blockchain adaptor-signature workflow locally.
    - Use Foundry or a private/local chain if appropriate.
    - First make the classical adaptor-signature version work.
    - Then replace the signature component with LAS and compare cost.

12. **Do not over-expand into full production ML-DSA LAS yet.**
    - The simplified implementation is acceptable for the project.
    - A full ML-DSA-based LAS can be discussed as future work or attempted only if time remains.

---

## D. Cleaned transcript

### 1. Benchmark environment: WSL versus bare metal

**Student (00:08)**  
First, for the benchmark, do I need to run it on bare metal, or is it okay to use WSL?

**Supervisor (00:28)**  
What do you mean by that? Do you mean whether you need to run it on another machine?

**Student (00:34)**  
Because I used WSL. Is that okay?

**Supervisor (00:38)**  
Yes, I think that is fine. What you need to do is specify your setup clearly in the final report.

**Supervisor (00:43)**  
You need to state the configuration of the machine and environment you used.

**Student (00:49)**  
Or do I need to run it on Linux?

**Supervisor (00:52)**  
If you have another machine available, you can run it there. But if you do not, that is fine. The important point is that all algorithms or protocols you compare should be run on the same machine. Then the comparison is fair.

**Supervisor (01:10)**  
Of course, someone else might get different absolute numbers on a better machine. That is not a problem. You just need to make the environment clear.

**Student (02:09)**  
I used WSL on Windows.

**Supervisor (02:12)**  
That is fine. If the environment can run all your functions and operations, then do not worry too much about it.

---

### 2. Communication-size table and LAS parameter comparison

**Student (02:32)**  
I think this is the kind of comparison you asked for last time.

**Supervisor (02:37)**  
Okay. This is the comparison. You have fixed the challenge size and the two important numbers here. What do these two numbers mean?

**Student (03:05)**  
I think those numbers are explained in the methodology. This one here shows it.

**Supervisor (03:19)**  
Okay, that is what I wanted to check.

**Supervisor (03:52)**  
So basically this is LAS based on Dilithium, right? With different parameter settings?

**Student**  
Yes.

**Supervisor (03:52)**  
Then I think this is good for this part.

**Student (04:13)**  
Do I need to focus on benchmarking only one setting, or should I include the other three settings as well?

**Supervisor (04:20)**  
It is not bad to include more settings. But I also want you to compare with the base scheme, for example the base Dilithium-style signature, and put that in a table.

**Student (04:50)**  
I think for size it is almost the same.

**Student (05:42)**  
There are two components.

**Supervisor (06:31)**  
So you mean the basic signature and LAS have the same public key and private key?

**Student (06:40)**  
I think the public key is the same. The statement is for the adaptor part. The public and secret keys are for signing.

**Supervisor (06:51)**  
Do they have the same public key and private key?

**Student (06:55)**  
Yes.

**Supervisor (06:57)**  
Then you can make a table like this. It will be easier to understand. You already have the data, so you can convert it into a table.

---

### 3. Rust implementation and comparison with C

**Student (07:12)**  
I also tried a Rust implementation.

**Supervisor (07:18)**  
You tried Rust? That is great.

**Student (07:26)**  
I used a GitHub repository for it.

**Supervisor (07:55)**  
How many stars does it have?

**Student (07:57)**  
Not many. It is a 2026 repository.

**Supervisor (08:02)**  
Okay, that is fine. If I understand correctly, you now have two implementations. One is based on C, and one is based on Rust.

**Supervisor (08:23)**  
If both implement LAS, have you compared whether the key sizes and signature sizes are the same?

**Student (08:32)**  
Yes. The communication size is the same. The timing is a bit different.

**Supervisor (08:44)**  
Timing can differ. I mainly want to check whether the key sizes and signature sizes match.

**Student (09:21)**  
This is for Dilithium-3 in Rust.

**Supervisor (09:26)**  
Okay. Have you compared using the same implementation setting?

**Supervisor (09:46)**  
It looks like the sizes are almost the same. Since this is Rust and the sizes are exactly the same, that is good. It suggests the implementation is consistent.

**Supervisor (10:02)**  
Have you also converted this Rust-based Dilithium implementation into Rust-based LAS?

**Student**  
Yes.

**Supervisor (10:23)**  
Okay, I made a mistake earlier. This is already the LAS version you converted. Good.

---

### 4. Need to explain what was modified

**Supervisor (10:31)**  
Now imagine I am a reader. I would ask: which parts of the code did you modify compared with the original implementation? Have you summarised that somewhere? I tried to check, but it is difficult because there are too many files.

**Supervisor (11:06)**  
You need to summarise it like this: this is the existing scheme, this is the existing implementation, and these are the parts you added or modified.

**Student (11:19)**  
I think it is almost the same. I mainly used the same primitives, such as NTT and helper functions.

**Student (11:55)**  
The C version uses randombytes and FIPS 202/SHAKE primitives.

**Supervisor (12:11)**  
Then you need a better way to show the structure of the repo. Maybe generate a structure diagram.

**Supervisor (12:25)**  
For example, show the main components: key generation, signing, and verification. Then for LAS, show the corresponding components and highlight the modifications compared with the original or basic scheme.

**Supervisor (13:15)**  
I cannot verify all the details by reading every file. It would take too long. I do not need all the low-level file names at the beginning. I need the high-level picture first.

**Supervisor (13:46)**  
Start from the beginning. If we want to use the signature, whether basic Dilithium or LAS, the important APIs are key generation, signing, and verification.

**Supervisor (14:03)**  
Use that as the entry point. Start from the high level, then dive into the details. If you start from detailed code immediately, people may get lost.

**Student (14:28)**  
Okay, from the high level.

**Supervisor (14:29)**  
Yes. You already have the details, but the question is how to present them better and more efficiently.

---

### 5. What “perfect benchmark/implementation” means

**Student (17:21)**  
Last time you said I should make the signature implementation and benchmark “perfect” before moving to the next stage. What is the definition of perfect here?

**Supervisor (17:40)**  
“Perfect” means you can clearly tell me what you have done. You can show me the picture. If I have no introduction, you can introduce your work and convince me.

**Supervisor (17:54)**  
If you can convince me that the modification is correct, the methodology is clear, and the results are clear, then that is good. I can see some results now, but I am not yet sure how you did everything.

**Supervisor (18:18)**  
So now it is not only about coding. It is more about presentation. I need to be confident that you understand the details.

**Supervisor (18:54)**  
You need to guide me step by step. Start with the high-level picture. For basic Dilithium, it has important components such as key generation, signing, and verification. For LAS, it has additional important components such as PreSign, PreVerify, Adapt, and Extract.

**Supervisor (19:14)**  
Then show how the existing code is structured. Whether it is C or Rust, give a picture showing the important components and building blocks, such as NTT, random generation, and hashing.

**Supervisor (20:08)**  
Then tell me which components you modified and where you modified them. If you can explain that correctly, I think the first stage is done well.

**Student (20:26)**  
For the C implementation, I looked at `sign.c` and then made separate C files.

**Supervisor (20:47)**  
That is already too detailed. First explain the high-level repo structure and important functions. Then explain the details.

**Student (21:22)**  
Starting from key generation, Bob creates a secret witness and the public statement, then adapts with it.

**Supervisor (21:34)**  
Start from the basic signature first, not LAS. Explain how the basic signature works: key generation creates a public key and secret key; signing uses the secret key; verification uses the public key.

**Student (21:48)**  
Key generation creates the public key and secret key, then signing uses the secret key, and verification verifies it.

**Supervisor (21:58)**  
Good. Then explain by referring to the code: which function and file relate to key generation, which relate to signing, and which relate to verification.

---

### 6. How deeply ML-DSA / Dilithium internals need to be understood

**Student (22:39)**  
I do not fully understand ML-DSA because the standard has more components and parameters.

**Supervisor (22:51)**  
You do not need to understand every detail if you do not touch those parts. But you need to know why you did not modify them, and why you did modify the parts you changed.

**Supervisor (23:12)**  
Write a summary this week. Start from the original scheme at a high level. List the key components. You do not need to understand every mathematical or implementation detail, but you should know what each component does at a high level.

**Supervisor (23:39)**  
For parts you do not modify, you can treat them as a box. For example, in key generation, say that the implementation uses existing primitives to generate the keys. Then do the same for signing and verification.

**Supervisor (24:14)**  
For LAS, present the important algorithms and highlight the modifications compared with the existing scheme.

**Supervisor (24:37)**  
For example, if there is a statement that must be set up for LAS, explain where it happens: key generation, pre-key generation, or another process. Explain it at a high level first.

**Supervisor (25:14)**  
For each function, explain why you added it and why it is needed.

**Supervisor (25:30)**  
Follow the algorithmic process we just discussed.

**Student (25:36)**  
Can both implementations go into the report?

**Supervisor (25:40)**  
Yes. You can include both the C implementation and the Rust implementation.

---

### 7. Benchmark methodology and Criterion

**Student (25:54)**  
I also made sure I had enough samples for the benchmark.

**Supervisor (26:04)**  
For communication size, the numbers are fixed. But for timing, as I said before, you should run the benchmark at least five times and calculate the average and standard deviation or variance.

**Student (26:26)**  
For Rust, I am using Criterion.

**Supervisor (26:39)**  
Can it output the numbers?

**Student (26:42)**  
Yes, it reports the numbers.

**Supervisor (26:52)**  
That is fine. If it does this for you, that is interesting. Otherwise, you can run it five times manually and calculate the average.

**Student (27:10)**  
It gives something like this for PreSign.

**Supervisor (27:15)**  
This is a distribution, right? This is the mean?

**Student (27:26)**  
I think this one has around 300 samples.

**Supervisor (27:39)**  
That is good. You can report this. It may be better than a simple manual benchmark if it gives proper statistics.

**Student (28:00)**  
I tried to make it similar to the C implementation. It has a warm-up time.

**Supervisor (28:10)**  
Good. For the C version, if you cannot use the same framework, the worst case is that you write a script to run it many times and record the numbers.

**Supervisor (28:38)**  
That is still fine. You can also generate figures from it.

---

### 8. Rejection sampling: theory versus measurement

**Student (28:49)**  
Theoretically, from the paper and the rejection-sampling calculation, I think it needs about 2.7 attempts for a pre-signature.

**Supervisor (29:12)**  
How did you get this number? Did you derive it from the paper?

**Student (29:49)**  
Yes. I put the parameters into the formula.

**Student (30:10)**  
It gives around 36.8% acceptance.

**Supervisor (30:13)**  
That makes sense, but you need to understand it. In the report, people may ask why it makes sense. You should explain it in your own words.

**Supervisor (30:26)**  
You said this is the expected number from the theory. But in your implementation, what is the measured number of sampling attempts? How can we see that?

**Student (31:59)**  
The theoretical number is around 2.67.

**Student (32:07)**  
This one is for Sign, and this one is for PreSign. This is the measurement.

**Supervisor (32:19)**  
Okay, that is good.

**Student (32:26)**  
I read that one difficult part of benchmarking LAS or lattice signatures is the rejection sampling, because it affects timing.

**Supervisor (32:44)**  
That can be an interesting insight in the report. You can include both the theory and the actual numbers from your implementation.

---

### 9. Main benchmark findings and classical adaptor-signature comparison

**Student (33:02)**  
Last time you asked me to summarise in a few points. I think the computation overhead is small.

**Supervisor (33:14)**  
Yes, and communication?

**Student (33:19)**  
The main issue is communication size. Even for the adaptor signature, we have the public statement. The final signature does not grow much, but the statement is additional.

**Student (33:43)**  
The final signature is still around several thousand bytes. The remaining thing is the blockchain-level comparison, which I have not done yet.

**Supervisor (33:56)**  
For the first step, you should compare it with classical adaptor signatures, for example ECDSA-based or Schnorr-based adaptor signatures.

**Student (34:22)**  
Yes. The problem is signature size and public statement size.

**Supervisor (34:27)**  
There are two main things: communication cost and computation cost. For communication cost, you should generate a table.

**Student (34:46)**  
ECDSA, yes.

**Supervisor (34:50)**  
Do we have a post-quantum-insecure classical adaptor signature baseline?

**Student (34:57)**  
I think I have it somewhere, but I have not verified the numbers.

**Supervisor (35:02)**  
You should compare it with LAS.

**Student (35:08)**  
I have some classical numbers, but I have not verified them yet.

**Supervisor (35:41)**  
That is okay. Assume the numbers are correct for now, but verify them. Add another column to show how much larger or slower LAS is compared with ECDSA or a classical adaptor signature.

**Supervisor (36:17)**  
Show the overhead or increase compared with the classical baseline.

**Student (36:34)**  
I also have another diagram comparing ECDSA and LAS.

**Supervisor (36:46)**  
That is fine. It can be useful.

**Supervisor (37:20)**  
Make sure we are comparing corresponding things. They may not be exactly the same, but we need to highlight the correspondence.

**Supervisor (37:31)**  
It is good that you include standard deviation.

---

### 10. Organising results and improving reproducibility

**Student (38:10)**  
This is the library I use. This is the lower and upper bound.

**Supervisor (38:16)**  
Okay. That is interesting. But you need to check the details and make sure the security parameter settings correspond.

**Supervisor (38:39)**  
Finally, we want a table for computation and a table for communication.

**Student (39:04)**  
For ECDSA, I just used existing code.

**Supervisor (39:49)**  
That is fine. You do not need to modify it, but make sure you are comparing the right thing and state the security-level caveat.

**Student (40:12)**  
I think LAS is closer to Dilithium level 2, so I compare with that.

**Supervisor (40:27)**  
Spend one more week making a clear summary of what you have done. Clean up and organise all the results. Then we can move to the next stage.

**Supervisor (40:58)**  
Make a summary of the differences, from high level to low level. You need to convince me what you have done and why it makes sense.

**Supervisor (41:29)**  
Right now, you have numbers and implementation, but I do not know where to start verifying it. There are too many details. Start from the high level again.

**Supervisor (41:45)**  
To improve reproducibility, you should have a clear README telling people how to run your code.

**Supervisor (42:31)**  
Maybe the README you have now is too detailed. You can keep the details, but you should also have a short README with only the most important commands.

**Supervisor (43:23)**  
It is like having a short README and an extended README. The short one gives the main commands, and the extended one contains more details.

---

### 11. Next-week deliverable

**Student (43:37)**  
For next week, do I need to make a presentation?

**Supervisor (43:43)**  
One or two slides should be fine. I want to see a picture or diagram.

**Supervisor (43:53)**  
Summarise the existing implementation, then compare it with the one you modified. Refer to the code you pushed so I can understand what you modified.

**Supervisor (44:29)**  
You now have implementation for the base scheme, implementation for LAS, and numbers comparing them. They are almost ready. The remaining task is verification and validation of the results.

---

### 12. Simplified LAS versus full ML-DSA-based LAS

**Student (44:58)**  
Since this is a simplified implementation, do we need to make the adaptor version using full ML-DSA? Or is it enough to keep ML-DSA ideas but simplify them?

**Supervisor (45:22)**  
I am not sure. You could try if you have time, but for this project the simplified version should be fine.

**Supervisor (45:29)**  
If you want to show practicality, you can add some discussion or try extra numbers. But for this project, the main goal is to show feasibility of the implementation.

**Student (46:08)**  
Even the simplified version is already complex.

**Supervisor (46:12)**  
Yes, that is fine. That is why I want you to summarise it with diagrams, so people can understand the code and compare the two implementations.

---

### 13. Next stage: blockchain integration

**Student (46:37)**  
After I present this next week, what is the next step?

**Supervisor (46:47)**  
Blockchain integration. You can use a private/local chain. I can tell you how to do it later, possibly using Foundry.

**Supervisor (47:00)**  
But before that, make sure this first part is correct.

**Supervisor (47:12)**  
For the next step, first answer how adaptor signatures can be implemented in a blockchain setting. You do not start with LAS immediately. First use a classical adaptor-signature construction.

**Supervisor (47:37)**  
Once you have a basic ECDSA or classical adaptor-signature setup running, generate the corresponding numbers, such as transaction cost or on-chain/off-chain cost. Then replace that signature component with LAS and report the comparison.

**Supervisor (48:09)**  
After that, if you want to improve the project further, you can consider additional applications or variants of adaptor signatures, such as functional adaptor signatures.

**Supervisor (48:39)**  
If we can build a post-quantum-secure atomic swap, then the project has achieved the basic requirement. If we want to push further, we can add more features or other blockchain applications.

**Supervisor (49:05)**  
Let us proceed step by step. If everything you showed me is correct, then you are doing a good job, faster than I expected.

---

### 14. Question about real atomic swap and ZK proof

**Student (49:26)**  
For atomic swap, do I need to implement the real version? For example, does the party that extracts the witness need a ZK proof that the witness is valid and small?

**Supervisor (49:40)**  
Can you repeat the question?

**Student (49:44)**  
For the party extracting the witness, do they need some proof that the witness is the real witness, that it is small, and not invalid?

**Supervisor (50:00)**  
That may require additional components. We can start from something standard first.

**Supervisor (50:14)**  
If we implement the full atomic swap in that way, then yes, we may need ZK proofs. But then we should also consider whether the ZK proof is post-quantum secure. That is another component.

**Supervisor (50:58)**  
This is another thing. Let us see how far we can go.

---

### 15. Scheduling

**Supervisor (52:54)**  
Monday and Tuesday do not work for me. Maybe Wednesday or Thursday. I will let you know.

**Supervisor (53:17)**  
Besides the project, make sure you also take care of your other work. If you need some time for vacation, just let me know.

**Student (53:40)**  
Okay, thank you.

---

## E. Practical meaning for the project now

The immediate task is not to add more features. The immediate task is to make the current implementation explainable.

For the next meeting, prepare:

1. a one- or two-slide summary;
2. a high-level diagram of the base signature implementation;
3. a high-level diagram of the LAS implementation;
4. a table of reused, modified, and newly added components;
5. benchmark tables for computation and communication;
6. rejection-sampling theory versus measured attempts;
7. a short README with reproduction commands;
8. verified classical adaptor-signature comparison numbers, if available.

The key message from Wang is:

> You already have implementation and numbers. Now you must organise them so another person can understand, verify, and reproduce them.
