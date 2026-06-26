# Cleaned Transcript — Meeting 4 with Zhipeng Wang

**Source:** `meeting4_transcript.md`  
**Type:** meaning-preserving cleaned transcript, not a legal/verbatim transcript.  
**Main topic:** LAS benchmark figures, parameter clarity, cumulative vs per-operation timing, communication cost, code review via branch/PR comparison, and next-step prioritisation.

---

## A. Key contextual corrections

| Auto-transcript / unclear phrase | Corrected meaning |
|---|---|
| adapter signature / adaptive signature | adaptor signature |
| pray signing / pro signing | PreSign / pre-signing |
| pre verification | PreVerify / pre-verification |
| LAS / last / glass | LAS, Lattice-based Adaptor Signature |
| delete them / deletium / lithium | Dilithium / simplified Dilithium |
| L2, L3, L5 | Dilithium-level-inspired parameter sets; must be clearly labelled |
| fingers | figures |
| accumulative time | cumulative time |
| computation codes | computation cost |
| communication codes | communication cost |
| PK / SK | public key / secret key |
| Zahid / said / zed | `z`, the response component |
| C | challenge `c` |
| Y | public statement `Y` |
| automic / automate swap / auto mix work | atomic swap |
| local EVM / Foundry | local EVM benchmark using Foundry, later stage |
| PR proof of request | pull request for code review |
| standard delete film | standard Dilithium baseline |

---

## B. Meeting summary

This meeting focused on whether the current LAS benchmark figures are understandable and defensible.

Wang’s main feedback was that the current figures contain useful results, but the presentation is confusing because labels such as **paper**, **L2**, **L3**, and **L5** are not self-explanatory. A reader will not know whether these refer to LAS parameter sets, basic Dilithium parameter sets, or simplified-Dilithium-derived levels. The report and figure captions must clearly state the key parameters for each setting.

He also strongly recommended reporting operation timings **independently**, not only as cumulative workflow time. Cumulative time is useful for an end-to-end process, but in practice signing, verification, pre-signing, adaptation, and extraction may happen on different machines and by different participants. Therefore, the report should show separate timings for `Sign`, `Verify`, `PreSign`, `PreVerify`, `Adapt`, and `Ext`.

For communication cost, Wang wants clear component-level reporting: public key, secret key, challenge `c`, response `z`, signature `(c,z)`, pre-signature, adapted signature, and statement `Y`. If the signature and pre-signature are almost the same size, the report should explain why.

Wang repeatedly emphasised that the immediate priority is **Stage 1: make the standalone signature benchmark perfect**. Atomic swap, local EVM gas, Foundry, and classical adaptor-signature comparison are valid later steps, but they should not distract from finalising the core LAS benchmark first.

The key next action is to open or prepare a branch comparison / pull request between the clean Dilithium baseline and the LAS implementation, then share it with Wang so he can inspect which files were modified and whether the implementation matches the intended algorithm.

---

## C. Cleaned transcript

### 1. Screen sharing and start of benchmark discussion

**Zhipeng Wang (00:06)**  
Is there a screen?

**Royce Steven (00:13)**  
Can you see my screen?

**Zhipeng Wang (00:17)**  
Yes, I can see your VS Code.

**Royce Steven (00:41)**  
There are some things that I found.

**Royce Steven (01:45)**  
For adaptor overhead, the most expensive part seems to be Adapt versus Verify.

**Zhipeng Wang (01:51)**  
Okay, that is good. So pre-signing and pre-verification are shown here?

---

### 2. Confusion about “paper”, L2, L3, and L5 labels

**Zhipeng Wang (02:01)**  
Wait, which one is this? What do L2, L3, and L5 mean?

**Royce Steven (02:11)**  
They are reduced-dimension settings derived from Dilithium levels, because in this implementation I use simplified Dilithium.

**Zhipeng Wang (02:23)**  
Right. So “paper” means the LAS paper, correct?

**Royce Steven (02:28)**  
Yes. “Paper” means the LAS paper setting.

**Zhipeng Wang (02:34)**  
Okay. So the first one is the LAS paper setting, and the second, third, and fourth are different security-level settings?

**Zhipeng Wang (02:47)**  
Are those levels for the basic signature, or for LAS?

**Royce Steven (03:11)**  
L2, L3, and L5 are taken from Dilithium.

**Zhipeng Wang (03:17)**  
Okay. Then I am confused about what the bars mean. Are you comparing overhead against the base signature? Is this time?

**Royce Steven (04:03)**  
Yes, this is time.

**Zhipeng Wang (04:10)**  
Do you have another figure showing verification time, signing time, and pre-signing time?

---

### 3. Cumulative time should not replace per-operation timing

**Zhipeng Wang (04:45)**  
You are reporting cumulative time, right? You first do signing, verification, adaptation, and extraction. That is okay as an end-to-end view, but I suggest reporting their times independently and separately.

**Zhipeng Wang (05:04)**  
In practice, signing and verification are normally done by different people or on different machines. For example, I generate a signature first, and then you verify it later. So it is a bit strange to consider only the combined time, because the algorithms happen in different places.

**Zhipeng Wang (05:33)**  
I want to emphasise this: it would be better to report the time independently, rather than only cumulative time.

**Zhipeng Wang (05:47)**  
Cumulative time represents the whole process. Theoretically that is fine, but in practice we usually care about the time for each algorithm or function separately.

**Royce Steven (06:12)**  
Is it okay if I benchmark everything on one machine?

**Zhipeng Wang (06:16)**  
Yes, that is fine. Using one machine is fine. Just report the timing independently.

**Royce Steven (06:22)**  
Okay, independently. Got it.

---

### 4. Parameter differences must be shown directly in the figures/report

**Zhipeng Wang (06:24)**  
Let us go back to the cumulative-time figure. I am wondering about the numbers.

**Zhipeng Wang (06:59)**  
I am still confused. Let us consider the first two figures. The first setting is the paper setting, with parameters like `n = 4` and `ell = 4`, right? Then for L2, on the right, you want to have similar or corresponding parameters?

**Royce Steven (07:32)**  
I think it is not exactly the same. There are parameter differences.

**Zhipeng Wang (07:41)**  
Then you should report that here. Other readers will not know what L2, L3, and L5 mean. They will want to check the differences between the parameter settings.

**Zhipeng Wang (08:00)**  
It is very important to indicate the key parameters for all settings.

**Royce Steven (08:07)**  
Yes, I need to include the other parameters as well to make it clearer.

---

### 5. Atomic swap exists, but Stage 1 benchmark comes first

**Zhipeng Wang (08:18)**  
This is for computation cost. What about communication cost?

**Zhipeng Wang (08:30)**  
You have already implemented atomic swap?

**Royce Steven (08:38)**  
Not a full atomic swap. It is a simplified atomic swap. I have not implemented the full atomic swap from the paper.

**Zhipeng Wang (08:46)**  
That is why I suggest doing it step by step. I have the feeling that you want to do everything together. That is okay, but we will revise things again and again, so it is better to proceed step by step.

**Zhipeng Wang (09:06)**  
Let us not consider atomic swap at the moment. First, make sure the signature comparison and signature benchmarks are perfect. Then we can go to atomic swap.

**Zhipeng Wang (09:25)**  
For computation cost, you also need to report communication cost: signature size, key size, and so on. Do you have figures for that?

---

### 6. Communication-cost figure: component sizes

**Zhipeng Wang (11:42)**  
Here you have different bars: `pk`, `sk`, and so on. `pk` is public key and `sk` is secret key?

**Royce Steven (11:45)**  
Yes. And `c` is the challenge.

**Zhipeng Wang (11:49)**  
So what is `z`?

**Royce Steven (11:55)**  
`z` is the response.

**Zhipeng Wang (12:06)**  
Okay. The signature is `(c, z)`, correct?

**Royce Steven (12:14)**  
Yes, the signature is `c` and `z`.

**Zhipeng Wang (12:20)**  
And the pre-signature?

**Royce Steven (12:25)**  
The pre-signature is also the challenge and `z_hat`.

**Zhipeng Wang (12:33)**  
We have the ordinary signature, the pre-signature, and the final adapted signature. What is the difference between the final adapted signature and the ordinary signature?

**Zhipeng Wang (12:52)**  
From the figure, it looks like all three are the same size. Are they the same?

**Royce Steven (13:16)**  
The difference is very small. Between the pre-signature and the signature, we only add a small witness. That is why the size is not very different.

**Zhipeng Wang (13:31)**  
Okay, I see. That is fine. Maybe you can make the scale clearer if you want the difference to be visible.

---

### 7. Figure labels must state whether values are LAS or simplified Dilithium

**Zhipeng Wang (13:49)**  
For the “paper” label, I now understand that it means the LAS paper. But I am still not sure whether L2 means L2-like LAS or L2-like Dilithium.

**Royce Steven (14:07)**  
L2-like means the corresponding simplified setting.

**Zhipeng Wang (14:13)**  
So you are comparing the LAS paper setting with L2, L3, and L5 simplified settings?

**Royce Steven (14:24)**  
Yes.

**Zhipeng Wang (14:56)**  
This is okay, but readers may ask: what exactly is being compared between LAS and the standard scheme? Again, this involves the parameter question I mentioned earlier.

---

### 8. Basic signature versus LAS bars

**Zhipeng Wang (16:59)**  
Here, for L2-like, L3-like, and L5-like, are these basic signatures with different parameters, or LAS with different parameters?

**Royce Steven (17:25)**  
For the blue bars, it is simplified Dilithium. For the orange/yellow bars, it is simplified LAS.

**Zhipeng Wang (17:41)**  
Okay. But even for Dilithium, you also have key generation, signature generation, and verification.

**Zhipeng Wang (18:17)**  
I think I understand. But again, can you report the parameters somewhere? I want to see the differences between the settings.

**Zhipeng Wang (18:31)**  
For example, what is `k` equal to? You mentioned there are several parameters. These need to be visible.

**Royce Steven (18:39)**  
I think I have it somewhere in the report.

---

### 9. Share repo / branch comparison and focus on three or four key figures

**Zhipeng Wang (19:44)**  
You already have the repository, right? You can share it with me if you want.

**Royce Steven (19:55)**  
I have not fully updated it yet.

**Zhipeng Wang (19:59)**  
That is fine. You can share it when ready. But again, first make the first benchmark ready, then we will start discussing atomic swap.

**Zhipeng Wang (20:18)**  
For example, I may ask you: in two or three sentences, compared with the basic Dilithium-style signature, how much communication cost does LAS increase or reduce?

**Zhipeng Wang (20:41)**  
You should report this in a transparent way. When I read the figures, I should not have to guess what each bar means.

**Zhipeng Wang (21:07)**  
You can provide more details, but you should also have a few figures that summarise the most important findings.

**Royce Steven (21:32)**  
Is this something like what you expect?

**Zhipeng Wang (21:36)**  
Yes, something like that. But you should include all the parameters, otherwise the comparison may be misleading.

**Zhipeng Wang (21:58)**  
Regarding time, you also need to state what machine you used. If I run the algorithm on my own PC later, I may get different timing results because the machine is different.

**Royce Steven (22:17)**  
I think I put that in the benchmark methodology.

**Zhipeng Wang (22:20)**  
Good.

---

### 10. Report structure and methodology concerns

**Royce Steven (22:58)**  
Is this the correct structure for the report, with introduction and objectives?

**Zhipeng Wang (23:11)**  
It may be a bit early to finalise the structure, because you may modify it later. You can share the details with me when ready, and I can give more comments.

**Royce Steven (23:40)**  
I am not sure if I am doing it correctly, especially the methodology. I am afraid I am missing something.

**Zhipeng Wang (23:50)**  
The numbers are the kind of numbers I expect to see, but I have not checked all the details. You have shown the numbers, but not all the implementation details.

**Zhipeng Wang (24:09)**  
Please pick three or four important figures, and then we can focus on them. If something looks inconsistent, we can check the code to see whether there is a mistake.

**Zhipeng Wang (24:29)**  
If you want to discuss the method details, you should show me the code. You should share which parts you modified compared with the existing implementation.

**Zhipeng Wang (24:51)**  
As I mentioned before, you can create a pull request comparing your work with the standard Dilithium code. Then we can see the differences between your modified algorithm and standard Dilithium.

---

### 11. Pull request / code review

**Zhipeng Wang (25:20)**  
From what I can see now, you have imported a clean Dilithium baseline. Where is your implementation?

**Royce Steven (25:36)**  
This is the baseline, and my work is in `main`.

**Zhipeng Wang (25:42)**  
Have you created a pull request and invited me to review it?

**Royce Steven (25:51)**  
Okay.

**Zhipeng Wang (25:52)**  
Open it, and then in the next meeting we can discuss the details. Also prepare three or four important figures. We will discuss the methodology and use those figures to check whether the results are correct.

**Zhipeng Wang (26:12)**  
That should be the task for the next meeting. Once this part is ready, we can move to the next steps.

---

### 12. Local EVM / Foundry and classical comparison are later steps

**Royce Steven (26:24)**  
For gas-cost comparison, is it appropriate to use a local EVM?

**Zhipeng Wang (26:35)**  
That is for the following stage. Of course, you should do it locally. In the future, we can use Foundry. But I do not think you should worry about that at this moment. First, make the current benchmark ready.

**Royce Steven (27:00)**  
And comparison with classical signatures is also the next step?

**Zhipeng Wang (27:03)**  
Yes, that is the next step. We have already discussed the plan, but for now let us focus on the first stage. Once the first stage is ready, then we can move to the other stages.

**Royce Steven (27:17)**  
Thank you, Doctor.

**Zhipeng Wang (27:15)**  
Good. Thank you. Bye.

---

## D. Corrected action items

1. **Fix benchmark figure labels.**
   - Clearly define “paper”, “L2-like”, “L3-like”, and “L5-like”.
   - State whether each setting is LAS, simplified Dilithium, or a Dilithium-derived LAS parameter set.

2. **Add key parameters beside benchmark settings.**
   - Include key parameters such as `n`, `ell`, `M = n + ell`, `kappa`, `gamma`, `N`, and relevant security-level labels.
   - Make parameter differences visible in the report or figure captions.

3. **Report per-operation timing.**
   - Do not rely only on cumulative workflow time.
   - Report `KeyGen`, `Sign`, `Verify`, `PreSign`, `PreVerify`, `Adapt`, and `Ext` independently.
   - Cumulative time may be included as an additional end-to-end metric, but not as the main timing result.

4. **Explain why per-operation timing matters.**
   - Signing, verification, adaptation, and extraction may be executed by different participants or on different machines.
   - Benchmarking on one machine is acceptable, but the results should still be reported per operation.

5. **Report communication cost clearly.**
   - Include public key size, secret key size, challenge `c`, response `z`, statement `Y`, signature `(c,z)`, pre-signature, and adapted signature.
   - Explain if ordinary signatures, pre-signatures, and adapted signatures have the same or nearly the same size.

6. **Summarise the key findings in two or three sentences.**
   - For example: compared with the simplified Dilithium-style base signature, state whether LAS increases computation, communication, or both, and by how much.
   - Do not make the reader infer the conclusion from many figures.

7. **Keep only three or four main figures for discussion.**
   - The figures should summarise the most important findings.
   - Extra figures can be moved to the appendix or kept as supporting evidence.

8. **State benchmark machine details.**
   - Include CPU, OS/WSL environment, compiler, build flags, iteration count, and number of runs.
   - This explains why timings may differ on another machine.

9. **Prepare a branch comparison / pull request.**
   - Compare the LAS implementation branch against the clean Dilithium baseline.
   - Invite Wang to review it.
   - Use the diff to show which files were reused, modified, or newly added.

10. **Prioritise Stage 1 before Stage 2.**
    - Current priority: standalone LAS signature benchmark and correctness.
    - Later: atomic swap, local EVM gas, Foundry, and classical adaptor-signature comparison.

---

## E. Practical meaning for the project now

The immediate priority is not to add more application code. The immediate priority is to make the **signature benchmark and its explanation defensible**.

For the next meeting, prepare:

1. three or four cleaned benchmark figures;
2. clear parameter labels for every setting;
3. per-operation timing tables;
4. communication-size/component-size tables;
5. a short explanation of the main findings;
6. a GitHub PR or branch diff from clean Dilithium to LAS.

The key message from Wang is:

> First make the standalone LAS benchmark perfect. Then discuss atomic swap, local EVM gas, Foundry, and classical adaptor-signature comparison.
