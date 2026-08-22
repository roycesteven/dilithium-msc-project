# Contextually Cleaned Transcript — Meeting on LAS / Dilithium Implementation

**Source:** `Voice 260608_135650_original.txt` + uploaded audio context.  
**Type:** meaning-preserving cleaned transcript, not certified verbatim.  
**Main topic:** choosing and implementing a post-quantum exotic/adaptor signature project, especially LAS built from Dilithium.

## Key contextual corrections

| Original ASR error | Corrected meaning |
|---|---|
| “adapta T nature”, “adaptive signature” | adaptor signature |
| “latest/lady space” | lattice-based |
| “delete theme / delethium / delithium” | Dilithium / CRYSTALS-Dilithium / ML-DSA |
| “NASA game / K calculation mechanism” | KEM / key encapsulation mechanism |
| “Hims / lower burn / upper burn” | hints / lower bound / upper bound / norm bound |
| “modular 24 / modular 23” | modulus roughly `2^24` in the paper vs `q = 8380417 ≈ 2^23` in Dilithium code |
| “ice dimmy / I say dimmy” | IAS, the isogeny-based adaptor signature |
| “AH ML / multi hock block log” | AMHL / multi-hop locks / multi-hop payment application |
| “autonomic swap” | atomic swap |
| “fair exchange” | fair exchange |
| “scriptless / scrapless blockchain” | scriptless scripts / scriptless blockchain idea |
| “zero proof / zk proof” | zero-knowledge proof, ZKP |
| “MPC” | multi-party computation |
| “pocket / poppy” | poqeth |
| “NIST published it” | research teams proposed/submitted schemes to NIST; NIST standardised selected ones |

---

## Action items from this meeting

1. **Confirm the exact LAS paper** to use as the main reference.
2. **Use the Dilithium/ML-DSA implementation as the base**, but focus on the digital-signature part, not KEM code.
3. **Do not change parameters unnecessarily** at the beginning; start from the parameters used in the chosen implementation.
4. **Choose implementation language**: C is safer because the reference code is in C; Rust is acceptable if the available libraries are good and you can manage the learning curve.
5. **Before next meeting**, prepare:
   - whether there are other constructions besides LAS;
   - which language you want to use;
   - which Dilithium/LAS functions you need to modify or add;
   - whether the base implementation runs on your machine.
6. **Project stage 1:** implement and benchmark standalone LAS / lattice-based adaptor signature.
7. **Project stage 2:** integrate into a blockchain application, likely **atomic swap** or **fair exchange**, possibly on a local/private chain.
8. **Optional advanced applications:** AMHL / multi-hop locks / payment-channel network features, only after stage 1 and stage 2 are finished.
9. **Benchmark against two baselines:**
   - the basic post-quantum signature, e.g. Dilithium/ML-DSA;
   - a classical adaptor-signature construction, e.g. ECDSA/Schnorr-based adaptor signature, if feasible.
10. **Report structure:** include high-level design, what code/functions were modified, key implementation decisions, benchmark results, and only the most important code snippets, probably in the appendix.

---

## Cleaned transcript

### Speaker 1 — Supervisor (00:01)
Okay. Do you have specific questions to discuss? We can follow the things you shared with me.

### Speaker 2 — Student (00:12)
Yes. I am considering the adaptor-signature direction, but I am not sure which paper I should refer to. Am I referencing the correct paper?

### Speaker 1 (00:26)
Yes, the one you showed is the right one. It is the most straightforward and probably the simplest one to start from.

### Speaker 2 (00:46)
If I am not mistaken, that paper was published in 2020, so it may not be standardised.

### Speaker 1 (00:55)
It is not standardised. But to be honest, even several years later, there still are not many clearly better results compared with this one. That is one reason I suggest starting from this paper.

### Speaker 1 (01:15)
It is easier for you to adapt it directly from a standard lattice-based signature implementation. You only need to make several modifications, rather than building everything from scratch.

### Speaker 1 (01:28)
Once you have a fundamental result based on this paper, you can consider adding more. But I suggest starting from a simple solution and simple components.

---

## 1. Which Dilithium implementation / repository to use

### Speaker 2 (01:43)
Am I referencing the correct GitHub repository for Dilithium?

### Speaker 1 (01:48)
That is a good question. I need to check. This one looks relevant.

### Speaker 2 (01:58)
It might not be simplified like the version in the LAS paper, right?

### Speaker 1 (02:04)
Yes. The repository may contain several components. This part is for the digital signature. Another part is for key encapsulation, or KEM, which is a different primitive. You do not need to touch the KEM part. You only need to refer to the digital-signature part.

### Speaker 2 (02:39)
This implementation seems to have more parameters, such as hints, lower bounds, upper bounds, or witness/norm-related parameters.

### Speaker 1 (02:54)
Yes.

### Speaker 2 (02:54)
The simplified version in the paper seems more straightforward.

### Speaker 1 (02:57)
Yes, you can use a simpler version.

### Speaker 2 (03:00)
So I should follow the algorithm in the paper?

### Speaker 1 (03:08)
Yes, follow the algorithm. You can also refer to the implementation because the overall structure should be similar. The implementation may include more details for security or practical optimisation, but for this project we can start from the basic version and simplify fancy parts where appropriate.

### Speaker 1 (04:02)
Maybe the implementation has more checks or more security details. But for this project, start from the basic version and follow the paper. We do not need a production-ready version. It is acceptable to simplify several fancy algorithms, especially if we only need a prototype or smaller security parameters for experiments.

### Speaker 2 (04:39)
So even if the paper uses a modulus around `2^24`, and the GitHub implementation uses something closer to `2^23`, I should not change the parameter at the beginning?

### Speaker 1 (04:59)
Yes. Start from the implementation parameters. Try to run the existing implementation first.

---

## 2. Language choice: C vs Rust

### Speaker 1 (05:01)
Have you decided which language you want to use?

### Speaker 2 (05:10)
They are using C.

### Speaker 1 (05:13)
Yes, C is fine. If you want to use Rust, I am also fine with that.

### Speaker 2 (05:21)
If I use Rust, I have to translate all of these things, right?

### Speaker 1 (05:26)
There may be Rust implementations or libraries. It depends which one you prefer.

### Speaker 2 (05:35)
I have never used Rust. Is the learning curve high?

### Speaker 1 (05:41)
Yes, there is a learning curve. You need to check whether Rust has standard libraries or crypto crates that help you. Rust is widely adopted nowadays, and there may be libraries, but C and C++ are also fine.

### Speaker 1 (06:14)
Rust is not automatically better. The first stage is to implement the basic lattice-based adaptor signature. The second stage is to integrate it into a blockchain system. You can still implement the crypto component in C and use other languages for the blockchain integration.

### Speaker 1 (06:56)
C is fine. You can also use other languages when integrating with the blockchain. The decision depends on the implementation you choose, the libraries available, and which language is more friendly for cryptographic implementation.

---

## 3. IAS / isogeny-based adaptor signatures

### Speaker 2 (07:35)
Last time, you also sent me a paper about the isogeny-based adaptor signature, IAS. Is IAS probably more complicated?

### Speaker 1 (07:43)
Yes, I asked another colleague before, someone more expert in that area. That option is more like future work for this project. For the first stage, it may be more challenging.

### Speaker 1 (08:03)
If you have time, we can think about it. But I suggest starting again from a simple, broad-scope solution.

---

## 4. Checking newer LAS-related papers

### Speaker 2 (08:09)
For LAS, the paper is from 2020. Is there a newer paper? I do not think there is.

### Speaker 1 (08:19)
What you can do is use Google Scholar. Search for the paper and check which newer papers cite it. Then compare the follow-up papers with the original one.

### Speaker 1 (09:47)
You can just check the papers that cite the LAS paper. I can also show you the comparison table once I find it. We have done a simple comparison to summarise related solutions for adaptor signatures.

---

## 5. Two-stage project structure

### Speaker 1 (10:11)
I imagine the project has two stages. The first stage is the standalone lattice-based adaptor-signature implementation and comparison. The second stage is integrating it into a blockchain system, where you can compare things such as gas cost and efficiency.

### Speaker 1 (10:45)
So first, finish the first stage: implement the basic post-quantum adaptor-signature component.

---

## 6. AMHL / multi-hop locks: optional advanced application

### Speaker 2 (10:55)
For the lattice-based part, do I have to implement something like AMHL?

### Speaker 1 (11:04)
Let me check.

### Speaker 2 (11:05)
If I am not mistaken, the terminology is something like AMHL.

### Speaker 2 (11:32)
For simple working, the paper has one statement and then makes it into two statements.

### Speaker 1 (12:24)
I think you are talking about the multi-hop lock / multi-hop payment part.

### Speaker 1 (12:35)
That is another application. It is more advanced compared to the basic application.

### Speaker 1 (12:45)
The basic applications are things like atomic swaps or payment channels. Multi-hop locks add other advanced properties.

### Speaker 1 (12:58)
That is another story. We do not need to consider it at the beginning. Remember, there are two stages.

### Speaker 1 (13:11)
First, implement the basic LAS functionality. Then integrate it into a blockchain construction or application, such as atomic swap or fair exchange. If you have time, you can add other applications such as AMHL / multi-hop locks. But that is optional.

### Speaker 1 (13:42)
Multi-hop locks or other advanced applications are optional. If you do not have time, focus on the first two stages. Once those two are finished, that is already enough.

---

## 7. Atomic swap and fair exchange

### Speaker 2 (14:04)
What is the second application you mentioned?

### Speaker 1 (14:09)
Fair exchange. It is similar to atomic swaps. It is about making sure two related actions happen together.

### Speaker 1 (14:25)
For example, I send coins to you, and you send coins to me. Atomicity means the two transactions should be executed together, or neither should be executed.

### Speaker 2 (14:54)
So it is similar to the scriptless blockchain idea?

### Speaker 1 (14:58)
Yes, similar to scriptless scripts.

### Speaker 1 (15:12)
But again, start from the first stage. Once that is done, then we can think about how to integrate it into applications.

---

## 8. How much maths / polynomial-ring detail is needed?

### Speaker 2 (15:31)
How deeply do I need to understand the specific lattice space, polynomial rings, and libraries such as GMP/NTT-type libraries?

### Speaker 1 (16:08)
Have you read the paper? It has a lot of definitions.

### Speaker 2 (16:16)
I think they have some libraries to make matrix or polynomial operations faster.

### Speaker 1 (16:23)
Yes. You can refer to existing libraries. But at this stage, what you need is to understand the basic idea of lattice-based cryptographic signatures. You do not need to understand every low-level mathematical detail immediately.

### Speaker 1 (17:29)
You are not required to read all the mathematical background first. If you want to read it, that is fine, but I suggest starting from the paper itself.

### Speaker 1 (17:44)
It is similar to coursework where you use some components without understanding every internal detail. Here, you need to code certain components, but you do not need to understand all mathematical details of every building block.

### Speaker 1 (18:10)
You do need to understand the signing process, key-generation process, and verification process. LAS modifies the basic lattice signature, so you need to compare LAS with the basic lattice-based signature and see which parts need to be modified.

### Speaker 1 (18:53)
Some parts may remain the same. Some parts may require new functions, such as hash functions. For standard hash functions, you can use existing standard implementations.

### Speaker 1 (19:11)
You can also ask LLMs about the mathematical details, especially when reading complicated notation. But you must verify the answers and not trust them blindly.

---

## 9. What to prepare before the next meeting

### Speaker 2 (19:48)
Assuming we meet again next week, what do you expect me to have finished?

### Speaker 1 (19:57)
First, find out whether there are other similar constructions besides this paper. Second, decide which language you prefer.

### Speaker 1 (20:11)
If you have time, share the structure of the implementation with me. Tell me which components or functions you think you need to modify.

### Speaker 1 (20:51)
For example, here is the signing function. You should be able to tell me whether you need to modify something inside it to support the functions in the LAS paper.

### Speaker 1 (21:52)
In the base implementation, I believe there are functions you can call directly. You do not need to touch all the details, but you need to know whether you can simply call existing functions or whether you need to modify them.

### Speaker 1 (22:35)
Some APIs may support what you need; others may not. Ideally, you can directly integrate existing functions and only implement the new functions required for LAS, without modifying all the complicated or boring building blocks.

### Speaker 1 (23:18)
There are two approaches. If you go deeper, you understand more low-level details and can modify more. If you stay at a higher level, it may be easier, but you risk losing flexibility because you cannot modify certain parts.

### Speaker 1 (24:10)
This is one of the first decisions: which implementation route you want to take. If you choose the lower-level route, you need more understanding at the beginning. If you choose the higher-level route, it may be easier at first, but you may face limitations later.

### Speaker 1 (24:34)
Rust is okay; it is not too challenging, especially with AI tools and existing crates. You can try and see which option feels better and more friendly for you.

---

## 10. Bounds / witness size / rejection sampling issue

### Speaker 2 (25:05)
I think the signing process is similar. The main difference may be the bound, right?

### Speaker 1 (25:14)
Which bound?

### Speaker 2 (25:25)
I want to make sure that after adaptation, the value still has enough space and remains within the bound.

### Speaker 1 (25:33)
Yes. You need to make sure that after extracting or adding the witness, the result is still within the required bound and does not become too large. You need to think about how to handle that. Check whether there are libraries or existing functions you can call.

---

## 11. Other advanced signature families: threshold, blind, group, multi-signatures

### Speaker 2 (25:56)
I found that some people do research on threshold signatures, blind signatures, group signatures, and schemes combined with ZK proofs or other techniques.

### Speaker 1 (26:19)
Yes, those are also interesting, but they are another story. They may be used to construct advanced applications, but first we need the fundamental LAS implementation. After that, we can consider more advanced functions.

### Speaker 2 (26:53)
I found that more people focus on Ethereum multi-signatures or aggregation.

### Speaker 1 (27:03)
Yes, multi-signatures are important for blockchain consensus. In blockchain consensus, many validators need to sign, and aggregation can reduce the signature size compared with simply putting signatures one by one.

### Speaker 1 (27:31)
But if you want to integrate multi-signatures into Ethereum consensus, that is another story. Adaptor signatures are more application-layer: you do not need to modify the consensus or client.

### Speaker 1 (28:00)
If you apply multi-signatures to Ethereum consensus, you may need to modify the consensus mechanism and replace the signature scheme used there. That is more challenging.

### Speaker 1 (28:54)
Again, it depends on time. We only have around three and a half months, so let us be realistic. Focus on LAS first. If you have time, you can look into multi-signatures later.

### Speaker 2 (29:21)
Blind signatures may be more difficult because they require zero-knowledge proofs or similar techniques.

### Speaker 1 (29:35)
Yes. Some use ZK proofs to hide information; some use MPC or other techniques. Those are more advanced primitives. Here, you are focusing on signatures, so you do not need to consider all of those policies or complex constructions now.

---

## 12. Comparing LAS with IAS / isogeny-based implementation

### Speaker 2 (30:43)
If I build LAS from Dilithium, it is clear what I should do. But if I want to compare it with isogeny-based IAS, they probably already have an initial implementation.

### Speaker 1 (31:00)
I hope so, but I am not sure.

### Speaker 2 (31:03)
I searched and it seems they have one, but it may not be updated. It may be from six years ago.

### Speaker 1 (31:11)
You can try to run it and see whether it works. Even if it does not work perfectly, that is useful information.

---

## 13. What application to benchmark

### Speaker 2 (32:05)
For benchmarking the application, which application should I implement? Should I compare smart-contract gas costs?

### Speaker 1 (32:23)
You can compare using atomic swap or fair exchange. Both are fine. For atomic swaps, there are already adaptor-signature-based constructions from classical signatures. You can run those and see their cost, then run yours.

### Speaker 2 (32:49)
Do I need to run my signature on a real chain?

### Speaker 1 (32:54)
Not necessarily a real chain. You can use a local testnet or private chain. You may write a Solidity implementation or refer to existing atomic-swap implementations, then replace the signature scheme and compare.

---

## 14. Timeline and first goal

### Speaker 2 (33:19)
The project is less than 12 weeks. By the end of the business month, should I already finish something?

### Speaker 1 (33:39)
Let us set first-week goals and then check feasibility.

### Speaker 2 (33:49)
So choose between C and Rust, then identify the functions in Dilithium?

### Speaker 1 (33:58)
Yes. First, make sure you can run the implementation on your own machine. Then identify which functions can be modified.

---

## 15. NIST / CRYSTALS-Dilithium clarification

### Speaker 2 (34:10)
Is CRYSTALS published by NIST?

### Speaker 1 (34:17)
No. NIST is the organisation that ran the standardisation process. Research teams proposed schemes, and NIST selected or standardised some of them.

### Speaker 1 (34:57)
For Rust or other language implementations, you need to check yourself whether they exist and whether they are reliable.

---

## 16. What to include in the report

### Speaker 2 (36:09)
In the report, should I include my code? How much code is appropriate?

### Speaker 1 (36:38)
You can include code in the appendix, but you do not need to include everything. There will likely be a maximum word/page limit. In the main report, include the most important parts: high-level design, what you modified, and your major results.

### Speaker 1 (37:19)
It would be better to show the overall design, what you modified, and then report your evaluation results.

---

## 17. What to compare against

### Speaker 2 (37:46)
Should I compare with pure basic Dilithium?

### Speaker 1 (37:51)
Yes. Compare with two things. First, compare with the pure basic post-quantum signature, such as Dilithium/ML-DSA. Second, if possible, compare with the classical adaptor-signature version.

### Speaker 2 (38:10)
ECDSA?

### Speaker 1 (38:12)
ECDSA is a standard basic signature. For a fair comparison with LAS, we should compare LAS with a classical adaptor signature, such as an ECDSA-based or Schnorr-based adaptor-signature construction, not merely with plain ECDSA.

### Speaker 2 (38:39)
So it is a basic signature plus an extra adaptor function?

### Speaker 1 (38:45)
Yes. You should search for a standard classical adaptor-signature construction, for example ECDSA-based or Schnorr-based.

### Speaker 2 (39:25)
For the classical one, I do not have to build it myself?

### Speaker 1 (39:29)
I think there should be existing data or implementations. Search a bit. This should be easier than the post-quantum one.

---

## 18. Using existing libraries / APIs

### Speaker 2 (39:59)
For the functions, should I just use existing libraries? They have many internal libraries, like polynomial operations.

### Speaker 1 (40:17)
Yes. Use them as standard APIs. You do not need to modify the low-level polynomial or arithmetic libraries unless necessary.

---

## 19. Atomic-swap simulation

### Speaker 2 (40:26)
For atomic swap, should I simulate that I want to transact with another party?

### Speaker 1 (40:37)
Yes. Search how people use classical adaptor signatures to implement atomic swaps. You do not need to modify the whole application logic. You may be able to use existing smart contracts and replace only the signature scheme.

### Speaker 2 (41:05)
Is it possible to do atomic swap using LAS?

### Speaker 1 (41:12)
Yes. That is what I am saying. People already use adaptor signatures to construct atomic swaps. Once you have a LAS implementation ready, you can replace that component in the current atomic-swap construction.

---

## 20. Performance metrics

### Speaker 2 (41:32)
The main difference is that an ECDSA-based adaptor signature is vulnerable to quantum computers, while LAS should be post-quantum. But what about performance?

### Speaker 1 (41:49)
That is a good question. Let the data speak. We do not know until you benchmark it.

### Speaker 1 (42:06)
You should compare key generation time, public-key size, signature size, signing time, and verification time. Then you can say how much performance is lost when using the post-quantum version.

### Speaker 1 (42:46)
There will be trade-offs. Communication means the size of public keys and signatures. Computation means signing and verification cost.

### Speaker 2 (43:10)
So I should create a small simulation/application myself?

### Speaker 1 (43:18)
Yes. That is good.

### Speaker 2 (43:22)
By next week, I should be able to explain the functions and the paper organisation?

### Speaker 1 (43:35)
Yes. Let us do it step by step. We can schedule another meeting later.

---

## 21. Scheduling

### Speaker 1 (43:45)
Ideally, we can meet next week, maybe Thursday or Friday.

### Speaker 2 (43:56)
Next week I may have a three-day workshop.

### Speaker 1 (44:13)
Okay. Let me know your availability and we can schedule a time.

---

## 22. PhD opportunities and publication

### Speaker 2 (44:24)
Do you have any information about PhD opportunities for master's students?

### Speaker 1 (44:36)
This year I do not think we have funding. Maybe next year. For international students, it is always very competitive.

### Speaker 1 (44:51)
If you want to apply for PhD funding, publications are very important. You will compete with candidates from other backgrounds, such as machine learning, where students may already have papers.

### Speaker 1 (45:33)
In security, systems, and crypto, high-quality papers are not easy to get compared with machine learning. But you can prepare as early as possible. Your grades and transcript will also be important.

### Speaker 1 (45:58)
Publication is also important. If you want, we can work together toward that.

### Speaker 2 (46:14)
Can this master’s project become a publication?

### Speaker 1 (46:19)
Yes, it could, if you find something interesting. For example, if you implement something and benchmark it, that may be useful.

### Speaker 1 (46:52)
For a cryptographic hardware or engineering systems venue, implementation matters. The focus would be more on implementation and evaluation.

### Speaker 1 (47:54)
You can search for crypto implementation venues, such as CHES or other crypto/systems-related conferences and workshops, to see what kinds of application or implementation papers they publish.

---

## 23. Marks vs publication

### Speaker 2 (48:15)
I heard from a lecturer that there can be a trade-off between getting good marks and turning the project into a publication. Is there really a trade-off?

### Speaker 1 (48:39)
I do not think so. If your project can be published, that usually means it has a strong contribution and has been reviewed by other researchers or experts. That gives more confidence in your results and can support a high mark.

### Speaker 2 (49:13)
If I want to turn this into a paper, do I need more analysis?

### Speaker 1 (49:20)
Yes. You may need more interesting analysis or contribution beyond simply implementing something. But building something entirely new can be challenging because you do not know whether it will work. Implementation is safer because you can be confident you will get some result.

### Speaker 1 (49:43)
After you become more familiar with the topic, you may see something interesting, and we can start from there.

---

## 24. Risk of not finishing

### Speaker 2 (49:55)
Is there a risk that I cannot finish this within three months?

### Speaker 1 (50:00)
I think if you focus on the first stage, it should be fine. You may not need to compare with all possible implementations.

### Speaker 2 (50:12)
So the basic target is post-quantum adaptor signature plus Dilithium comparison?

### Speaker 1 (50:16)
Yes.

### Speaker 2 (50:18)
LAS plus Dilithium comparison is already a manageable problem?

### Speaker 1 (50:23)
Yes.

### Speaker 2 (50:23)
Is the application similar to how poqeth implements post-quantum signatures on Ethereum?

### Speaker 1 (50:49)
It follows a similar idea in the sense that you integrate a signature scheme with a blockchain application.

### Speaker 2 (50:52)
But here it is different because the application is atomic swap?

### Speaker 1 (50:56)
Yes. Atomic swap is an application built on top of blockchain. It does not mean you are replacing the whole blockchain protocol. You should read atomic-swap papers later to understand it. It is an application using blockchain, not a new blockchain protocol.

### Speaker 1 (51:27)
Good. See you next week. Let me know another suitable time.

### Speaker 2 (51:35)
Okay, thank you.

---

## Addendum (2026-07-20) — content omitted by the ChatGPT cleaning

Audited against `meeting2_transcript_original.md` (840 lines → 616 in the cleaned version). The cleaning is largely faithful; the following were dropped:

### A.1 Dropped: the video question first appears HERE, not in Meeting 6 (37:32)

**Royce (37:32)** — *dropped entirely:*
> Because I still don't have an idea — what should I explain in the video?

(Wang deferred it; the concrete guidance — highlight only the most important points, detail on slides — came in Meeting 6.) The record should show the video deliverable was raised this early.

### A.2 Dropped/garbled: "add a high-level diagram to show the whole design" (36:38–37:19)

**Wang (36:38)** — the cleaned version kept "show the overall design" but dropped the *diagram* instruction (ASR: "add some high level time again" ≈ "add some high-level **diagram**"):
> Imagine your report will have an external examiner review — they might not check your detailed code, so it would be better to add a **high-level diagram** to show the whole design, what you modified, and then report your major results.

This is the earliest request for what later became the architecture/data-flow figures.

### A.3 Dropped fragment: Royce mentioned Ethereum's "Lean" post-quantum work (35:57–36:09)

**Royce (35:57)** — *dropped; ASR garbled ("lean ethium or line PM"):*
> …they have something like "Lean Ethereum"… I think it's very hard.

Kept here as a flagged-unclear aside (likely Ethereum's post-quantum "lean" initiative); Wang did not pick it up.

### A.4 Note: ASR hallucinations correctly discarded

The original contains obvious speech-recognition hallucinations from background noise — e.g. "The Olympic Olympic Games in the Milano Cortina, 2026" (21:29, 35:57) and one garbled profanity (12:24). The cleaned version rightly dropped these; they carried no meeting content.

### A.5 Minor specificity: the coursework analogy (17:44)

Wang's black-box analogy referenced a specific course — "similar to the Security & Privacy of AI coursework, where you were required to code some components without understanding every internal detail." The cleaned version generalised this to "coursework".

---

## Bottom-line interpretation

Your supervisor is steering the project toward a **practical, staged implementation**:

1. **Minimum successful project:** implement LAS by modifying Dilithium/ML-DSA-style signing, then benchmark it against basic Dilithium.
2. **Good project:** add a small blockchain application, probably atomic swap or fair exchange, and benchmark gas/application cost.
3. **Stronger project / publication direction:** compare against classical adaptor signatures and/or IAS, add deeper performance analysis, and show a clear implementation contribution.
4. **Avoid for now:** AMHL/multi-hop locks, Ethereum consensus multi-signatures, blind/group signatures, or heavy ZKP/MPC designs unless the core LAS implementation is finished early.
