# Cleaned Transcript v2 — Meeting with Wang Zhipeng

**Type:** meaning-preserving cleaned transcript, not a legal/verbatim transcript.  
**Sources used:** original auto-transcript, uploaded whiteboard photos, project papers, and terminology checks for NIST/Dilithium/LAS/poqeth.  
**Confidence note:** I corrected clear speech-recognition errors such as *lady-based → lattice-based*, *harsh-based → hash-based*, *show algorithm → Shor's algorithm*, *invitation/implication → implementation*, *exalted → exotic*, *point side → PreSign*, etc. Where the audio/transcript is genuinely unclear, I preserve the intended meaning and mark uncertainty only when needed.

---

## A. Corrected technical picture from the whiteboard

The supervisor was drawing a **2 × 2 map of signature schemes**:

| | **Classical / non-post-quantum** | **Post-quantum** |
|---|---|---|
| **Basic signatures** | ECDSA, Schnorr, possibly BLS / pairing-based schemes | Dilithium, Falcon, SPHINCS+; NIST schemes; usually C reference implementations |
| **Exotic / advanced signatures** | multi-signature, group signature, ring signature, adaptor signature | post-quantum versions of multi-signature, group signature, ring signature, adaptor signature |

Main message of the meeting:

1. **Basic post-quantum signatures are mature** because NIST candidates/standards usually have implementations and security/efficiency analysis.
2. **Post-quantum exotic signatures are less mature**: many papers are theory-only; some have initial implementations; few are integrated into blockchain.
3. If an exotic paper already has implementation, start from that.
4. If not, use the **basic implementation** underneath it. For LAS, this means adapting a Dilithium-style implementation.
5. Your project should ideally implement a **post-quantum exotic signature** and a **small blockchain application/demo**, then benchmark it.

---

## B. Action items

- [ ] Identify which protocol / paper / signature scheme you will implement.
- [ ] Read the survey and then look for newer papers citing it.
- [ ] Prefer a post-quantum exotic signature paper that already has at least an initial implementation.
- [ ] If no implementation exists, choose a construction built on a basic PQ signature with available code.
- [ ] For LAS: compare the LAS algorithms with the basic Dilithium-style signing algorithm.
- [ ] Focus first on the **core algorithms**, not the full security proof.
- [ ] Implement the signature scheme.
- [ ] Build a basic blockchain-related demo or integration.
- [ ] Benchmark the signature itself and, if possible, benchmark the application.
- [ ] If time allows, compare post-quantum exotic signatures with classical exotic signatures.

---

## C. Cleaned transcript

### Speaker 1 (00:00)
There are several possible directions, not only the ones very close to existing work. You can explore which one is most interesting for you, and then we can focus on that one.

### Speaker 2 (00:13)
For classical blockchain, what signature schemes do they usually use?

### Speaker 1 (00:18)
For classical blockchain, I think they usually use **ECDSA**, and in some cases **Schnorr-based** signatures. It depends on the application, but these are the main classical signature schemes used today. Let me check.

### Speaker 2 (00:56)
For the lattice method, what is actually happening inside? Why do you think it is the most feasible approach?

### Speaker 1 (01:25)
You may know that the US government, through **NIST**, launched a post-quantum cryptography standardisation process. Many people submitted different constructions, and the process went through several rounds. Finally, there were final-round candidates.

### Speaker 1 (01:45)
For digital signatures, if I remember correctly, there were three main final candidates: **two were lattice-based** and **one was hash-based**. This shows that people are trying to standardise these approaches. Lattice-based cryptography is relatively mature. In practice, many systems, not only blockchains but also other digital systems and communication protocols, currently rely on classical signatures such as ECDSA.

### Speaker 1 (02:18)
Because of that, researchers are pushing the lattice-based versions toward practical use. That is why I think lattice-based approaches may be easier: many people have already studied their security and implementation.

### Speaker 2 (02:46)
If I want to understand lattice-based and hash-based signatures, what should I do first to understand those two clearly?

### Speaker 1 (02:56)
You can start by reading the figure or survey that classifies the different post-quantum signature schemes. It will show the categories.

### Speaker 1 (03:07)
There are different types of schemes. For example, hash-based schemes have their own fundamental assumption.

### Speaker 1 (03:16)
There are several categories based on different mathematical assumptions: **lattice-based**, **hash-based**, **isogeny-based**, **code-based**, and **multivariate-based** schemes. You do not need to understand all details immediately. The first step is to see the different directions and categories.

### Speaker 1 (03:49)
Inside each category, there are different constructions. You can think about which type of signature you want to use first. We also need to check whether a paper has an implementation.

### Speaker 2 (04:26)
In Introduction to Cryptography, they taught us about Shor's algorithm.

### Speaker 1 (04:31)
Yes, exactly. **Shor's algorithm** is the important motivation here. It is why classical public-key cryptography can be broken by a sufficiently powerful quantum computer.

### Speaker 1 (04:49)
This one is a lattice-based construction.

### Speaker 1 (05:04)
For this paper, it looks like it may not have an implementation. I would not suggest focusing on a paper where you have to build everything from scratch.

### Speaker 1 (05:20)
It would be better to build your system based on something that other people have already proposed and implemented. For example, you can search directly for an implementation.

### Speaker 1 (05:47)
You should do some investigation to understand which signature schemes already have implementations. Then you can improve or extend one of them.

### Speaker 1 (06:03)
First, try to understand which one is the most feasible for you and which one is the most interesting. Then try to find the overlap between feasibility and interest.

### Speaker 1 (06:15)
After that, we can start from there.

### Speaker 2 (06:17)
Okay. What challenges should I expect from this project?

### Speaker 1 (06:26)
The challenge is that sometimes you need to understand mathematical details.

### Speaker 2 (06:31)
How deep does the mathematical understanding need to be?

### Speaker 1 (06:54)
For example, this paper seems to have been published several years ago. It may not have an implementation yet.

### Speaker 1 (07:04)
The worst case is that you have to build it from scratch. But do not worry too much, because even if the exotic scheme itself does not have an implementation, we may still have an implementation of the **basic version**.

### Speaker 1 (07:23)
By “basic version”, I mean a standard/basic signature scheme. The signatures I am talking about here include things like **blind signatures**, **group signatures**, **ring signatures**, and **adaptor signatures**. These are what we call **exotic** or **advanced** signatures because they have more features than standard signatures.

### Speaker 1 (07:40)
For a typical basic signature scheme, you only have signing and verification: someone signs, and other people verify the signature.

### Speaker 1 (07:53)
For a multi-signature, several people can sign together. That is an advanced feature. Even in the classical world, these advanced constructions are usually built from a basic signature scheme.

### Speaker 1 (08:16)
So we can classify signature schemes along two dimensions.

### Speaker 1 (08:46)
The first dimension is **classical** versus **post-quantum**. Classical means non-post-quantum. The second dimension is **basic signature** versus **exotic/advanced signature**.

### Speaker 1 (08:59)
For classical basic signatures, examples include **ECDSA**, **EdDSA**, **Schnorr**, and possibly other pairing-based schemes.

### Speaker 1 (09:32)
For post-quantum basic signatures, examples include **Dilithium**, **Falcon**, and **SPHINCS+**.

### Speaker 1 (09:52)
For exotic signatures, there are constructions such as **blind signatures**, **group signatures**, **ring signatures**, **adaptor signatures**, **multi-signatures**, and **aggregate signatures**.

### Speaker 1 (10:00)
For some of these constructions, there are papers and sometimes implementations. But many papers on post-quantum exotic signatures only have the paper and no implementation. Some have implementations, but only initial implementations.

### Speaker 1 (10:41)
Many of these exotic constructions are built on top of basic post-quantum signatures.

### Speaker 1 (10:53)
For basic post-quantum signatures, we are more confident that implementations exist because they were submitted to **NIST**, and cryptography researchers have analysed their security and efficiency. That is why I suggest you first read the relevant papers and check whether they have initial implementations.

### Speaker 1 (11:22)
I hope some of them already have implementations. If they do, we can start from there. But they may not yet be integrated into blockchain.

### Speaker 1 (11:30)
Even if an implementation is only an initial implementation and not very practical, it may still be useful as a starting point.

### Speaker 1 (11:41)
The backup solution is: if all the exotic-signature papers have no initial implementation, then you need to understand their constructions and build them from the implementation of the **basic post-quantum signature** underneath them.

### Speaker 1 (12:00)
For example, this is a **lattice-based adaptor signature**, LAS.

### Speaker 2 (12:14)
That is interesting.

### Speaker 1 (12:17)
For LAS, you can see that it has a lattice-based construction and algorithms. But in the paper, the implementation/evaluation section seems to mainly give parameter analysis and performance analysis. It does not appear to provide a full implementation.

### Speaker 1 (12:46)
If you go back to the protocol, it is based on a basic signature scheme related to **Dilithium**. Dilithium has key generation, signing, and verification. For that basic signature scheme, we can be sure there is an implementation.

### Speaker 1 (13:18)
The adaptor signature modifies the basic lattice-based signature. It is not exactly the same, but it has a similar structure.

### Speaker 1 (13:33)
So if we cannot find an implementation of the adaptor signature itself, you could implement the adaptor-signature algorithm based on the implementation of the basic Dilithium-style algorithm. The key is to understand the differences between the two algorithms.

### Speaker 1 (13:56)
You may need to modify several files or add several functions.

### Speaker 1 (14:04)
For adaptor signatures, there are four main functions: **PreSign**, **PreVerify**, **Adapt**, and **Extract**. PreSign is similar to the signing process, and PreVerify is similar to the verification process.

### Speaker 1 (14:22)
You may need to add or implement another algorithm. That is the relationship between the two dimensions of signature schemes.

### Speaker 1 (14:35)
One dimension is the security assumption: classical versus post-quantum. For many current blockchain functionalities, signatures are still classical.

### Speaker 1 (14:50)
Some people have proposed and implemented exotic signature schemes, but many are still based on classical assumptions. These schemes can support blockchain features such as multi-signatures and adaptor signatures, for example for Bitcoin payment channels.

### Speaker 1 (15:21)
The paper I shared with you, **poqeth** — Post-Quantum Ethereum — is about implementing post-quantum signature verification on Ethereum. It implements basic post-quantum signatures, including hash-based schemes and a multivariate-based scheme.

### Speaker 1 (15:32)
But poqeth replaces **basic** signature schemes rather than **exotic** signature schemes. That is why this project is interesting.

### Speaker 1 (15:43)
In the future, besides basic blockchain functionality, people will also want exotic signatures to support more advanced applications.

### Speaker 2 (16:00)
Apart from the cryptography course, I think what we learned is not enough. What else should I learn to be able to master this project?

### Speaker 1 (16:12)
There are two parts. Besides the cryptography part, the second part is how to implement it and integrate it into blockchain.

### Speaker 2 (16:24)
I mean the cryptography part. The cryptography course last time was not enough. What should I learn?

### Speaker 1 (16:32)
It depends on which cryptographic signature scheme you choose. If you choose a lattice-based scheme, first focus on the particular details of that paper.

### Speaker 1 (16:46)
If you choose another type of signature, such as group signatures, then you should understand group signatures. It depends on the scheme you choose.

### Speaker 2 (16:59)
Before the master's project starts in the summer—

### Speaker 1 (17:05)
When does it start?

### Speaker 2 (17:08)
After the exam.

### Speaker 1 (17:09)
So maybe around May or June? Okay.

### Speaker 2 (17:14)
Before I really start doing the project, what should I do?

### Speaker 1 (17:18)
I suggest that you first figure out which protocols or papers you might want to use. Try to read as much as possible. Once you decide, when you have full time to work on the project, you can focus on implementation, because implementation will be the time-consuming part.

### Speaker 1 (17:36)
Before you decide, the choice of paper is important because it determines how difficult the implementation will be. So you should first read as much as possible.

### Speaker 1 (17:49)
If you do not have a better option, you can always try this LAS/adaptor-signature direction. But for this one, again, it seems they do not have an initial implementation for the exotic/adaptor signature. We do have the Dilithium implementation, but you would have to adapt it to the adaptor signature.

### Speaker 1 (18:08)
So that is the backup solution. If you can find a better solution — for example, a paper that already has an initial implementation — that would be better, because then you do not need to modify the basic signature scheme from scratch.

### Speaker 2 (18:24)
Do you have any tips or strategy for approaching this? How should I read the papers and decide what to modify?

### Speaker 1 (18:36)
I suggest that you focus on the **core algorithm**. For example, for this lattice-based one, compare it with the basic version. A typical cryptography paper also has a long and detailed security analysis.

### Speaker 1 (18:56)
You do not need to cover the security analysis in detail, because that is a different task. If the paper discusses applications, you can investigate those later, in the second stage.

### Speaker 1 (19:13)
For the first stage, focus on the algorithm itself. If there is an implementation section, that is important too. But here, they mainly have performance/parameter analysis, not a full implementation.

### Speaker 1 (19:24)
So the core algorithm is the most important thing. Compare it with the basic post-quantum signature version.

### Speaker 2 (19:49)
Once I understand the maths and the algorithm, will implementation be challenging in terms of engineering?

### Speaker 1 (19:58)
It depends on your prior experience. Which language do you plan to use?

### Speaker 2 (20:08)
What language do you recommend?

### Speaker 1 (20:11)
It depends on the scheme you choose. For the basic schemes submitted to NIST, they usually have at least C implementations. So if you do not have other options, you can implement from C.

### Speaker 1 (20:31)
Some schemes may have implementations in Rust. If you find an initial implementation of the exotic post-quantum signature scheme in Rust, you can start from Rust, without needing to focus as much on the fundamental/basic version.

### Speaker 1 (20:49)
There may also be other languages, depending on the paper and available code.

### Speaker 1 (20:54)
It depends on the protocol and implementation. That is why the first stage is important: choose a feasible protocol and a feasible implementation route.

### Speaker 1 (21:12)
Try to pick the most feasible protocol at the beginning. It will make your life easier during the implementation process. Otherwise, you may end up building everything yourself.

### Speaker 2 (21:28)
What do you mean specifically by “protocol”?

### Speaker 1 (21:31)
I mean which paper or construction you would like to follow.

### Speaker 1 (21:43)
The survey paper is guidance to help you understand the area. It is a little old, so you should also read more recent papers that cite it.

### Speaker 1 (21:59)
Do you know how to check which papers cite a paper? You can use Google Scholar.

### Speaker 2 (22:20)
When I read cryptography papers, the mathematical notation feels intimidating. Should I panic, or should I just go through it one by one and try to understand it? At first glance it feels strange and difficult.

### Speaker 1 (22:49)
I would not worry about understanding everything immediately. I would focus on the **differences between the basic version and the exotic version**.

### Speaker 1 (23:01)
Read the algorithm and see which steps have been modified. You only need a high-level view of the basic functionality. Again, for cryptography papers, the most challenging part is usually the security analysis.

### Speaker 1 (23:31)
Since you are not focusing on security proofs, focus on the algorithms. Compared with machine-learning or other algorithms, cryptography uses more mathematical structures.

### Speaker 1 (23:52)
But if you treat some parts as a black box, the maths should not bother you too much.

### Speaker 2 (23:59)
What is the exact definition of post-quantum? I am afraid I may misunderstand it.

### Speaker 1 (24:07)
For classical cryptography, take RSA as an example. The difficulty is related to factorisation: if I give you a very large number, can you find its factors?

### Speaker 1 (24:28)
For a small number, for example, 21 = 3 × 7.

### Speaker 2 (24:34)
The hardness, yes.

### Speaker 1 (24:35)
But if the number is very large, factorisation becomes very hard.

### Speaker 1 (24:54)
For classical computers, this is challenging. We cannot solve it efficiently in polynomial time.

### Speaker 1 (25:07)
But Shor's algorithm is a famous quantum algorithm that can solve factorisation and discrete logarithm problems in polynomial time.

### Speaker 1 (25:35)
That means if someone has a sufficiently powerful quantum computer, they can break the fundamental mathematical assumptions behind many current digital signatures. For example, in Bitcoin or other blockchains, they could potentially recover a private key from public information.

### Speaker 1 (25:57)
That is why people are worried about the development of quantum computing.

### Speaker 1 (26:05)
This motivates migration from classical cryptography to post-quantum cryptography. For blockchains, we also want features beyond ordinary signing. People have already proposed post-quantum exotic signature schemes.

### Speaker 1 (26:24)
But implementation is still underexplored. That is the motivation for this project.

### Speaker 2 (26:32)
So there are not many implementations yet. The theoretical work exists, but implementation is limited.

### Speaker 1 (26:39)
Yes. It is not as mature as basic post-quantum signatures.

### Speaker 2 (26:47)
You mentioned that security analysis is hard. Since we do not have a large quantum computer, how do researchers analyse security?

### Speaker 1 (26:55)
We do not have sufficiently large quantum computers to test these attacks in practice. So security researchers do theoretical analysis based on mathematical assumptions.

### Speaker 1 (27:06)
They need to be clever in proving or analysing security. But for your project, you do not need to focus on this part.

### Speaker 1 (27:19)
You can assume that the schemes are secure according to the paper.

### Speaker 2 (27:25)
For implementation, what is one example of a blockchain application we can implement?

### Speaker 1 (27:33)
It depends on the signature type. For adaptor signatures, examples include **payment channels** and **atomic swaps**.

### Speaker 1 (27:46)
If you focus on multi-signatures, you could implement a multi-signature application. It depends on your choice.

### Speaker 1 (27:58)
You can treat the signature scheme as a package or tool, then build an application on top of it. It could be an application or a protocol modification.

### Speaker 1 (28:13)
If you focus on multi-signatures, you might build a simple application and test it. I think the application part is easier than implementing the signature scheme itself.

### Speaker 1 (28:29)
Any other questions?

### Speaker 2 (28:39)
What is the hard part? Is signature verification heavy in terms of computation?

### Speaker 2 (28:50)
Will it take a lot of time to verify?

### Speaker 1 (28:54)
That is something you may want to report in your dissertation. You can compare it with the conventional/classical version.

### Speaker 2 (29:01)
So should I benchmark it with the classical version?

### Speaker 2 (29:05)
Should I benchmark it with ECDSA, or with Dilithium?

### Speaker 1 (29:10)
Benchmark it with the relevant **basic signature scheme**. If the exotic signature is built from a basic signature, compare it with that basic version.

### Speaker 2 (29:24)
So benchmark the exotic post-quantum scheme against the basic post-quantum scheme?

### Speaker 1 (29:28)
Yes. Ideally, you can also benchmark against the classical counterpart.

### Speaker 1 (29:36)
If you cannot implement the classical comparison, you can do a simple calculation or estimation based on existing results from the literature.

### Speaker 2 (29:47)
So ideally we benchmark classical exotic signatures against post-quantum exotic signatures?

### Speaker 1 (29:51)
Yes, that would be the ideal result. If we can achieve that for one application, that should be fine. If you have time, you can compare more.

### Speaker 2 (30:06)
But the minimum is implementing the scheme and a basic application?

### Speaker 1 (30:15)
Yes. Implement the scheme and a basic application. Once you have that, we can call it a successful project.

### Speaker 2 (30:28)
Do I need to read more than the paper you gave me?

### Speaker 1 (30:36)
Yes, read more. But first understand this paper, then find more recent papers.

### Speaker 2 (30:47)
What are the criteria for a successful implementation? What would get a good mark for this project?

### Speaker 1 (31:01)
For me, the basic requirement is implementation of a **post-quantum exotic signature** and a basic blockchain application. That is the core.

### Speaker 1 (31:20)
If you benchmark it against the classical version, that would be better. If you can implement more signature schemes, that would be even better.

### Speaker 1 (31:37)
That would be much better.

### Speaker 2 (31:37)
Could you clarify the application you mentioned?

### Speaker 1 (31:42)
There are two kinds of benchmarks: the benchmark of the signature itself and the benchmark of the application.

### Speaker 1 (31:52)
Compare them with the classical version.

### Speaker 2 (31:58)
So the implementation is based on the basic version?

### Speaker 1 (32:02)
Yes, because the exotic scheme is built on top of the basic scheme.

### Speaker 2 (32:07)
You mean this paper may not have an implementation?

### Speaker 1 (32:09)
Yes, that is possible.

### Speaker 2 (32:12)
So I should try to build on top of the basic implementation instead of starting from scratch with the exotic technology?

### Speaker 1 (32:20)
Yes. But if you find a paper that already has an implementation, then you do not need to start from the basic code. That would be more convenient.

### Speaker 2 (32:31)
So the paper you shared may not itself include an implementation?

### Speaker 1 (32:37)
I forgot which specific paper I shared. You can check. Maybe it does not have one.

### Speaker 2 (32:48)
I see.

### Speaker 1 (32:51)
Spend some time reading them. If I have time, I will also check the most recent papers to see whether they have any implementation.

### Speaker 2 (33:02)
When are we going to meet again?

### Speaker 1 (33:06)
Maybe next time; it depends on your exams and coursework.

### Speaker 1 (33:13)
Just let me know if you have any updates. We should meet again before the project formally starts. Before that date, try to let me know whether you have identified which scheme you would like to implement.

### Speaker 1 (33:34)
That is the important preparation work. Now it is the end of March, so you have one or two months to investigate. Take your time and try to identify the most feasible solution or signature scheme that you would like to implement.

### Speaker 2 (33:59)
Do I need ethical approval for this project?

### Speaker 1 (34:04)
I do not think you need ethical approval, because we are not doing anything involving human subjects or sensitive data.

### Speaker 2 (34:11)
Okay, so we do not need ethical approval. Thank you so much.

---

## D2. Addendum (2026-07-20) — audit against the original

Audited against `meeting1_transcript_original.md` (498 lines → 509 in the cleaned version). **Verdict: this cleaning is faithful — no substantive exchange was dropped.** Three notes for the record:

1. **(29:36)** Wang's fallback for the classical comparison — *"if you cannot implement the classical comparison, you can do a simple calculation or estimation based on existing results from the literature"* — is present in the cleaned version and worth highlighting: it pre-authorises a literature-based classical baseline if implementation had proved infeasible (in the end it was implemented: `bench_classical`).
2. **(31:20–31:37)** The grading ladder is stated slightly more strongly in the original: basic = PQ exotic signature + basic blockchain application; better = plus classical benchmark; *"much better"* = plus additional signature schemes. The cleaned version has all three rungs.
3. Several genuinely unintelligible fragments (e.g. 05:04 "desk box", 09:16–09:19 letter salad) were rightly summarised rather than invented.

---

## D. Key meaning, in plain words

Your supervisor is **not** saying “just use poqeth.” He is saying:

- poqeth is useful because it shows **basic post-quantum signature verification on Ethereum**;
- your project is more novel because it should target **post-quantum exotic signatures**;
- LAS is a strong candidate because it is a **lattice-based adaptor signature**;
- but LAS may not have full implementation, so the practical route is to adapt a **Dilithium-style implementation**;
- before committing, search for other PQ exotic-signature papers that already have initial implementation, because that may reduce engineering risk.

