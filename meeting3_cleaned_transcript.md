# Cleaned Transcript — Meeting 3 with Wang Zhipeng

**Type:** Meaning-preserving cleaned transcript
**Main topic:** Git/code management, LAS benchmarking, fair comparison, security parameters, report writing, and next-week priorities.

---

## A. Key contextual corrections

| ASR / unclear phrase | Corrected meaning |
|---|---|
| gate hub rappo | GitHub repository |
| original dynam / original diamond | original Dilithium codebase |
| last / Alas | LAS |
| fought from the origin | forked from the origin / original branch |
| value / vowels / scribbs | files / scripts |
| APR | PR / pull request, or code comparison view |
| communication cost Y | communication cost, especially statement `Y` |
| AS base | adaptor-signature base / base signature |
| standard EDC DSA | standard ECDSA |
| challenge and fortressing Y | challenge `c`, response `z`, and/or statement `Y` |
| SDD | standard deviation |
| carpa / capa / coffee | security parameter / kappa / Dilithium parameter `k` |
| NEST | NIST |
| simplify mistake | simplified Dilithium / simplified NIST-style comparison |
| toy lighter / toiletr | toy ledger |
| Atomics swap | atomic swap |
| exact research | existing research |
| system, not algorithm | project is a system implementation/evaluation, not a new cryptographic protocol |
| artefact / code deadline | possible code/artifact submission deadline |

---

## B. Meeting summary

Wang’s main message was that the project has made good progress, but the next priority is to make the implementation and evaluation **defensible**.

First, he advised creating a clean GitHub repository structure with two branches: one branch should keep the original Dilithium code, and the main branch should contain the LAS implementation. This allows Wang and the examiner to compare the code differences clearly: which files were kept, which files were modified, and which files were newly added.

Second, he emphasised that the report needs deeper analysis of size and benchmark results. If LAS signatures or keys are larger than the baseline, the report should not only say “larger”; it should explain why by breaking objects into components, such as challenge, response, public key, statement, or witness. The reader should be able to see which component causes the increase.

Third, benchmarks should be repeated at least five times. The report should show average results and, where possible, variance or standard deviation. He also warned that comparisons must be fair: schemes should be compared at the same or clearly stated security level. If LAS uses weaker or different parameters than Dilithium or another baseline, the report must state this honestly.

Fourth, Wang clarified that the current project is mainly a **system implementation project based on existing research**, not a project proposing a new cryptographic protocol. A new research question could be developed later by adding new functionality on top of adaptor signatures, but the current priority is to complete a reliable LAS implementation and evaluation.

Finally, he advised writing the report while implementing, rather than leaving all writing until the end. He suggested a standard structure: introduction, background/motivation, methodology, results/evaluation, conclusion, and future work. He also told Melly to check whether there is a separate code/artifact deadline before the final report deadline.

---

## C. Cleaned transcript

### 1. GitHub repository and code comparison

**Speaker 1 (00:01)**  
I was saying that it would be a good idea to have a GitHub repository with two branches. One branch should keep the original Dilithium code, and the main branch should contain the LAS version you are working on. Then every time we can compare the differences and make sure the modifications are correct.

**Speaker 1 (00:33)**  
Because finally, if we want to evaluate what you have created, it is better to compare it against the original. Since there are many files and scripts, it may be better to create a PR or at least a clear branch comparison.

**Speaker 2 (00:48)**  
So, one main branch, and one branch from the original?

**Speaker 1 (00:54)**  
Yes. The original branch should keep the original files, and then you compare it with your modified branch. Then we can easily see which files you modified, which files you added, and which files remain the same as the original.

**Speaker 2 (01:08)**  
Yes, from now I will put it in.

**Speaker 1 (01:18)**  
You know how to compare branches, right? That would be helpful. You can also invite me next week. If I have time, I can review the code directly and leave some comments. That may be faster.

**Speaker 1 (01:41)**  
From a high-level comparison, we can see the idea, but we still need to see how the code actually looks. So first, let us make sure the first step works and that what you have already implemented is correct.

**Speaker 1 (02:06)**  
It sounds like you already have a working version of LAS. But first, please organise the repository and compare it with the original, so we can clearly identify the differences.

---

### 2. Report needs deeper component-size analysis

**Speaker 1 (02:46)**  
What I suggest is that you give more detailed analysis here.

**Speaker 1 (03:01)**  
For example, in the signing process or signature construction, you have components such as the public key and signature elements. If the LAS signature is larger than another signature, you should compare the components.

**Speaker 1 (03:19)**  
Go back to the paper. The signature may have two components. Another signature may also have two components. As a reader, I would ask: if both signatures have two components, why is one larger than the other?

**Speaker 1 (03:55)**  
It would be better to show the size of each component for each scheme. Then we can explain which component causes the larger size. In the report, you should not only show that one is larger; readers will ask why. Since you are not proposing a completely new protocol, you need strong analysis of your results.

**Speaker 1 (04:17)**  
So maybe divide the signature into two parts, for example one part for the challenge and one part for `z`, and then report the size of each component.

**Speaker 2 (04:41)**  
If I am not mistaken, the size is the same, Doctor. I need to check because I have many features.

**Speaker 2 (05:00)**  
Sign and PreSign are almost the same.

**Speaker 1 (05:08)**  
Yes, but you are talking about computation cost. I am talking about communication cost.

**Speaker 2 (05:16)**  
Communication cost, including `Y`?

**Speaker 1 (05:18)**  
Yes, communication cost. For example, if you compare with the original work or a classical adaptor-signature construction, you need to show what is being compared.

**Speaker 2 (05:46)**  
No, I was comparing with the adaptor-signature base.

**Speaker 1 (05:48)**  
Okay, then you should add that clearly. Since you also compare LAS with Dilithium in your slides, and they have a similar signature structure, I want to see which component contributes to the size.

**Speaker 1 (06:19)**  
The timing table in microseconds is about computation cost. I am talking about communication cost. Communication cost also has components, as shown in the paper. You should report the size of each component so we can see which component increases the signature size.

**Speaker 2 (06:50)**  
You mean the challenge and the response `z`?

**Speaker 1 (06:53)**  
Yes. How many bytes each element takes.

---

### 3. Benchmark repetition, average, variance, standard deviation

**Speaker 1 (07:10)**  
For signing computation cost, I suggest that you run the computation at least five times and calculate the average.

**Speaker 2 (07:26)**  
For the signature benchmark?

**Speaker 1 (07:30)**  
Yes, for the benchmark. The more runs the better, but at least five should be fine. The results can vary because of randomness, your machine, or other processes running on the same machine.

**Speaker 1 (07:50)**  
You should run the comparisons on the same machine. When you plot bars or other results, you can add variance or standard deviation.

**Speaker 1 (08:03)**  
Then you can present the benchmark more convincingly.

---

### 4. Security parameters and fair comparison

**Speaker 2 (08:24)**  
This does not include the challenge yet.

**Speaker 1 (08:35)**  
You need to confirm the component sizes. If the figure shows that one secret key or signature is larger than another, you need to confirm why.

**Speaker 2 (09:32)**  
I think this is because of the parameters. The paper uses `k = 4` or similar, but Dilithium mode may use `k = 6`. I am using something like `k = 4`.

**Speaker 1 (10:07)**  
Then that is important. You must make sure you compare them at the same security level. Otherwise, it is not fair. If one scheme uses stronger security parameters and another uses weaker parameters, then the comparison is not fair.

**Speaker 1 (10:39)**  
When reporting comparisons, you need to specify the security parameters for every scheme — for example ECDSA, adaptor ECDSA, LAS, Dilithium, or whatever you compare.

**Speaker 1 (10:58)**  
Even within one scheme, there can be different security levels, such as 128-bit or 256-bit. You need to specify these. Otherwise, one scheme may look better simply because it uses weaker parameters.

**Speaker 2 (11:22)**  
I am not sure how to compare fairly, because the optimised scheme has more parameters. Should I make Dilithium simplified?

**Speaker 1 (11:40)**  
You could do that. That can be one comparison. Since we are not building a production system, that is fine.

**Speaker 2 (11:51)**  
So the main difference would be that LAS has PreSign, PreVerify, pre-signature, and the simplified base?

**Speaker 1 (11:59)**  
Yes. You can also run another comparison by increasing parameters. This is normally what people do in papers: compare at one security level, then compare at another security level.

**Speaker 1 (12:33)**  
NIST also has different security levels. Ideally, we follow their levels if possible. If not, it is still okay for research, but we should clearly state what security levels or parameter sets we use.

**Speaker 2 (12:59)**  
So first use the security parameters from the paper, and then maybe change them?

**Speaker 1 (13:03)**  
Yes, or map them to the relevant NIST security levels if possible.

**Speaker 1 (14:19)**  
You can start from Dilithium mode 2 or a smaller parameter set, but ideally compare it with the corresponding LAS parameters at the same level. Then you can argue that, in practice, you can achieve this security level.

**Speaker 1 (15:18)**  
So compare 75-bit with 75-bit, or compare Dilithium mode 2 with corresponding LAS parameters. The important thing is: compare at the same level.

---

### 5. Next-week priority: finish first-stage benchmark correctly

**Speaker 1 (15:18)**  
For next week, I think you should focus on this first stage. Finish the benchmark and make sure the numbers are correct and the security-level comparison is consistent.

**Speaker 1 (15:53)**  
If we have time later, we can consider more things, such as other post-quantum adaptor signatures or a second signature scheme. But first make sure this first one is correct and perfectly implemented.

**Speaker 2 (16:31)**  
So what should I focus on for next week?

**Speaker 1 (16:34)**  
Focus on finishing the benchmark for the first stage. Make sure the numbers you show me are correct. Make sure the comparison is consistent in terms of security levels. Also create the GitHub repository so we can track code comparison and differences.

---

### 6. Atomic swap / toy ledger is second stage, not the immediate priority

**Speaker 2 (17:30)**  
I want to make sure whether these are the correct steps for the atomic swap. Is this what you are expecting?

**Speaker 1 (17:48)**  
That is more for the second stage. We can discuss it, but the first stage should be finished first.

**Speaker 2 (18:23)**  
Is it like this: I need to create a toy ledger to simulate the atomic swap?

**Speaker 1 (18:44)**  
Yes, something like that. But do not worry about that too much at the moment. If you have time, you can also look at classical adaptor-signature-based atomic swap constructions, and then next week we can discuss that.

**Speaker 1 (19:04)**  
But again, focus first on the benchmark for the first stage. Once the signature and pre-signature are ready, the second step will be much easier.

---

### 7. Research question and project category

**Speaker 2 (19:26)**  
I am wondering whether this project has a research question.

**Speaker 1 (19:30)**  
That is a good question. If you want a new research question, one possibility is to build new features on top of adaptor signatures, for example a functional adaptor signature or a lattice-based version of something more advanced.

**Speaker 1 (20:46)**  
That would be more like proposing a new research question. It would require you to propose something new, not just implement what has already been proposed. You can read about this if you are interested.

**Speaker 1 (21:02)**  
You can search for functional adaptor signatures. That may give you another direction, but it is not necessary for the current stage.

**Speaker 2 (22:43)**  
I also want to confirm which category this project belongs to.

**Speaker 1 (22:52)**  
This project is more like developing a system based on existing research. You are not building your own new cryptographic protocol.

**Speaker 1 (23:25)**  
You are building and evaluating a system. It is not primarily a new algorithm project. Imagine you release software similar to a digital-signature implementation. That is the category.

**Speaker 1 (24:04)**  
So we should emphasise that it is your system.

---

### 8. Start writing report while implementing

**Speaker 2 (24:26)**  
Should I start writing the report now as well?

**Speaker 1 (24:42)**  
Yes, you can start writing. It is good to write down your thoughts gradually. Otherwise, while doing step 2, you may forget details from step 1. It is a good idea to record what you are doing during the project.

**Speaker 1 (25:36)**  
The deadline is in September, but you should start writing from the beginning. That will be more careful.

**Speaker 2 (26:20)**  
Should I follow this kind of structure: first introduction?

**Speaker 1 (26:28)**  
Yes. Introduction, background, motivation, methodology, results, evaluation, conclusion, and future work. You can also include critical reflection.

**Speaker 2 (26:46)**  
What should methodology contain?

**Speaker 1 (26:59)**  
You can summarise the methods you used: adaptor signatures, how they are applied, what implementation you modified, what design choices you made, and how you constructed the prototype.

**Speaker 1 (27:26)**  
You can write about how you compared the schemes, how you identified the differences, what you modified, and what you added compared with the existing implementation.

**Speaker 1 (27:45)**  
Many students spend three or four weeks writing. If you start writing early, you can add results as you implement them.

---

### 9. Code/artifact deadline and when to stop implementing

**Speaker 2 (28:10)**  
When should I stop implementing?

**Speaker 1 (28:17)**  
It depends. You should check the deadline. Maybe they ask you to submit the artifact or code several weeks before the final report, so please check this.

**Speaker 1 (28:34)**  
For some projects, students may need to submit code three or four weeks before the final report. That gives time to finish writing the report.

**Speaker 2 (28:57)**  
From what I read, it says to ask the supervisor whether artifact/code submission is required. The main things are the report and the video.

**Speaker 1 (29:09)**  
Ideally, I encourage you to submit code or provide evidence showing what you have done.

**Speaker 1 (29:35)**  
If there is no strict code deadline, you can keep polishing the code until the report submission. There is always something to improve, but you need to manage time.

**Speaker 2 (30:03)**  
So code and report writing should happen at the same time?

**Speaker 1 (30:07)**  
Yes, that would be a good idea.

---

### 10. Next meeting

**Speaker 1 (30:18)**  
Good work for this meeting. Let us meet next week.

**Speaker 2 (30:34)**  
When are you free next week?

**Speaker 1 (30:40)**  
Maybe the same time as today.

---

## D. Corrected action items

1. **Create/organise a GitHub repository with two branches:**
   - one branch preserving the original Dilithium code;
   - one main/current branch containing LAS changes.

2. **Prepare a clear code-difference view** for Wang:
   - files reused unchanged;
   - files modified;
   - files newly added;
   - rationale for each major change.

3. **Improve the report’s size analysis:**
   - do not only report total key/signature sizes;
   - break down sizes by component, e.g. challenge `c`, response `z`, public key, statement `Y`, witness, pre-signature;
   - explain which component causes size increase.

4. **Separate computation cost from communication cost:**
   - computation cost = timing, e.g. KeyGen, Sign, Verify, PreSign, PreVerify, Adapt, Ext;
   - communication cost = bytes transmitted/stored, e.g. signatures, pre-signatures, statements.

5. **Run benchmarks at least five times:**
   - compute average;
   - include variance or standard deviation where possible;
   - run comparisons on the same machine.

6. **Make security-level comparisons fair:**
   - explicitly state parameter sets/security levels for LAS, Dilithium, ECDSA/adaptor ECDSA, or any other baseline;
   - avoid comparing a lower-security LAS configuration with a higher-security Dilithium configuration as if they were equal;
   - if exact matching is hard, state the limitation clearly.

7. **For next week, prioritise the first-stage benchmark:**
   - ensure LAS numbers are correct;
   - ensure comparison is consistent;
   - do not overfocus on atomic swap details yet.

8. **Keep atomic swap/toy ledger as second-stage work:**
   - acceptable direction, but not the immediate priority;
   - possible later comparison with classical adaptor-signature-based atomic swaps.

9. **Frame the project correctly:**
   - current project = system implementation/evaluation based on existing research;
   - not a new cryptographic protocol unless you later add a genuinely new feature.

10. **Start writing the report now:**
   - introduction;
   - background/motivation;
   - methodology;
   - results/evaluation;
   - conclusion/future work;
   - critical reflection.

11. **Check code/artifact submission rules:**
   - confirm whether code must be submitted earlier than the report;
   - if no separate deadline exists, continue polishing code while writing.

12. **Next meeting:**
   - next week, likely same time.

---

## E. Practical meaning for the project now

The supervisor’s priority is not “add more features immediately.” The priority is:

1. make the LAS implementation traceable against original Dilithium;
2. make benchmark numbers reliable;
3. make comparisons fair by security level;
4. explain size differences by component;
5. start writing the report while implementation details are still fresh.

The most important instruction is:

> Finish the first-stage LAS benchmark correctly before expanding the application layer.
