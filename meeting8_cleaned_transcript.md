# Cleaned Transcript — Meeting 8 with Zhipeng Wang

**Date:** 2026-07-31, 11:28 (Teams).
**Sources merged into this file:**
1. `meeting8_original_transcript.md` — single ASR pass over the meeting recording, UTF-16, 00:00–48:26, diarised as "Speaker 1" / "Speaker 2".
2. `meeting8_summary.md` — the auto-generated meeting summary (agenda bullets + action items). Its content is folded into §B and §D below.
3. `meeting8_recording.m4a` — the source audio. **It was not re-transcribed for this file** (no ASR tooling available in the working environment), so wording below is reconstructed from source 1, cross-checked against source 2 and project context, not from a fresh listen of the audio.

**Speaker mapping (note: polarity is FLIPPED versus Meeting 7):**
- **Speaker 1 = Zhipeng Wang (supervisor)** — he asks for the summary of the week's work, gives the rulings.
- **Speaker 2 = Royce Steven (student)** — he reports the UTXO/ZKP measurements and asks the questions.
This mapping is confirmed by the summary's action items, all of which are assigned to Speaker 2.

**Type:** meaning-preserving cleaned transcript, not a legal/verbatim transcript. Every exchange in the source is represented; nothing is dropped. Genuinely unintelligible fragments are marked `[unclear]`; reconstructed words are in `[square brackets]`.
**Known gaps:** the ASR has dropouts at ≈01:01–01:35, ≈09:26–10:13, ≈30:45–31:46 and ≈46:53–48:26. The recording's last two utterances (46:53, 48:26) are post-meeting pleasantries.

**Main topics:** three-configuration ZKP/signature comparison (classical no-ZKP / Groth16 / LaZer) — Groth16 slower to generate but smaller proofs, LaZer faster but much larger; post-quantum proof sizes ≈30–300× classical, which is expected and acceptable; **the central ask of this meeting — break down the Bitcoin transaction structure and show, with a diagram, the original transaction vs the one modified by the adaptor signature**, specifying exactly which components (pre-signature, statement Y, witness, proof) are added and where; "transaction" must not be used loosely to mean "the signed message"; finish the Bitcoin/UTXO path before touching the EVM/Naysayer work; report quality over proportional word counts; figures embedded in the text, not dumped in the appendix; Chapter 5 titled "Conclusion, critical reflection and future work"; future work = reduce statement-Y / proof size (possibly via a hint-style optimisation), IPFS-style off-chain storage as a fallback; draft 6–8 minute presentation slides the week after next.

---

## A. Key contextual corrections

| ASR / unclear phrase | Corrected meaning |
|---|---|
| last / the last / glass / laws / the last protocol / half 4 protocol / the last nature | **LAS** (the adaptor-signature scheme, eprint 2020/845) |
| laser / Lacey | **LaZer** (the lattice ZKP library used for π) |
| growth 16 / clock 16 / the 216 / 2 16 | **Groth16** (the classical zk-SNARK) |
| finger / fing / fingers / graph | **figure(s)** |
| unchecked / on chin / uncheck / on jin / un-chained | **on-chain** |
| trasection / intersection / traction / tract ion / trans section / contract section | **transaction** |
| weakness / weightness / winning list / water / watar | **witness** |
| basic nature / busy nature / episode nature / prey signature / pretty signature / price signature / presidation | pre-signature / adaptor signature (context decides) |
| statement why / segment Y / stadium | **statement Y** |
| nice security parameter / nets niss already | **NIST** security parameter |
| hin / hinge / hands / Hank solution / hidden optimis | **hint** (Dilithium's hint vector) / the hint-based optimisation |
| EV. M. / EPM / UV / ub / event | **EVM** |
| risk of 5 / risk 5 / a Euro risk fire | **RISC-V** |
| the 0 knowledge here told machines | **zero-knowledge virtual machines (zkVMs)** |
| the pop, the one that poked implements | **poqeth** (eprint 2025/091, the integration template) |
| laysayer / naysayer | **Naysayer** (optimistic verification: assume valid until challenged) |
| discreetly logarithm equ | **discrete-log equality proof (DLEQ)** |
| zayp / ckp / cds / GDP / zkb | **ZKP** |
| ecds / ECSA / ecd / a cds | **ECDSA** |
| the ras / RAS / rusty | **Rust** |
| seat / sea | **C** |
| the adept / depth / dept | **Adapt** |
| extra / extraction | **Ext / Extract** |
| work count / wood count / the world (in "requirement for the world") | **word count** |
| Croatian, critical reflection of the future | **"Conclusion, critical reflection and future work"** (the Chapter 5 title) |
| a little coin punch / a little coin | the **Bitcoin** part |
| Utah | **UTXO** |
| ipfs | **IPFS** (decentralised storage) |
| Cas / quas platform bridges | **cross-platform bridges** |
| 6286 | **6 to 8 minutes** (presentation length) |
| a hulk out would confirm | [how] would [a node] confirm |
| symmetric / symmetry (re: Adapt/Extract, public parameters) | [unclear] — from context, *deterministic / identical on both sides* |
| 165 spice | [unclear] — a size in bytes |
| pyramid / mom's mom / Staatsman wire | [unclear] |

---

## B. Meeting summary

**What Royce reported.** He ran the UTXO-side comparison in Rust (not C) across the three configurations: the classical adaptor signature (no ZKP), LAS + Groth16, and LAS + LaZer. For the classical configuration he deliberately used no ZKP, because the classical adaptor-signature protocol on the elliptic curve does not specify one — it only carries a discrete-log-equality proof. The measurements behave as expected: **Groth16 takes longer to generate but produces a much smaller proof; LaZer generates quickly but the proof is much larger.** Post-quantum ZKP proof sizes come out roughly 30× — and on the on-chain components as much as ~300× — the classical ones. Wang's verdict on the numbers was that they are "what we expected": the well-known trade-off where post-quantum security is fine but the sizes are not. He did not ask for more measurements; he asked for the existing work to be polished.

**The central ask: break down the Bitcoin transaction.** Most of the meeting was Wang pressing on a gap in the report: it never says what a transaction actually *contains*. Royce's write-up uses "transaction" loosely to mean the message being signed, which Wang objected to — in a Bitcoin context "transaction" has a predefined format that must follow Bitcoin's definition, so a different term should be used for the signed message. Wang wants the report to state exactly which components are added to a standard transaction (pre-signature, statement Y, witness, and whether the proof goes in at all — Royce believes it does not) and to show this with **two diagrams: the original/standard Bitcoin transaction, and the transaction modified by adding the adaptor signature**, highlighting which fields changed. That breakdown is also what justifies the reported communication-size increase. His practical instruction was blunt: Google the standard Bitcoin transaction structure and follow it; the LAS paper is an oversimplification of the architecture and does not specify this. He also flagged that Royce should confirm how the witness is actually carried on-chain, since implementations differ.

**Sequencing: Bitcoin first, EVM only if time.** Royce raised that he had been exploring the EVM again — specifically Naysayer-style optimistic verification (as in poqeth), where verification is assumed valid until someone disputes it — and asked whether to continue. Wang said to finish the Bitcoin/UTXO solution properly first: the same unanswered question ("how do the added components sit inside the transaction / smart contract") applies to the EVM anyway, so a fully complete finished solution on Bitcoin comes first, and the EVM becomes a discussion of a more advanced solution. Also discussed: the LAS Adapt cost is ~270× ECDSA's (~1.5 µs), which Royce could not fully explain beyond the pre-signature being large; Wang noted ECDSA's adaptor construction is simply very efficient.

**Report guidance.** Word count per chapter need not be proportional to the rubric's mark weighting — write more where the interesting work is (results, not background); Royce's largest chapter is Chapter 3, which Wang was content with. Figures must be embedded near the text that references them, not collected at the end of a chapter or in the appendix ("in some other subjects like economics they put figures in the appendix — their domain is different"), and a single figure should not consume a whole page unless it genuinely needs to; four related figures can go side by side. Chapter 5's title was agreed as "Conclusion, critical reflection and future work". Wang also corrected terminology: LAS's PreSign/PreVerify/Adapt/Ext are **functions**, not protocols — "protocol" implies high-level design like a consensus protocol.

**Future work and scope limits.** Reducing proof size is the most challenging and most valuable direction; Royce identified **statement Y as the largest single component**, present both off-chain and on-chain, and floated using a Dilithium-style hint optimisation to avoid transmitting full public parameters — he had not used it because he suspects it breaks Adapt/Extract when both parties must derive identical values. Wang's response: interesting, try it if there is time (target it for the week after next), but the open question is whether verification and extraction still work. As a fallback for large data, Wang suggested decentralised storage (IPFS) with miners referring to it — noting this drags in another component/bridge, and "we can always have a solution, but the question is how good the solution is". Adding a second signature scheme (as the original proposal allowed) is out — there is no time; polish what exists. A zkVM/RISC-V direction was raised and effectively ruled out unless Bitcoin's VM were RISC-V-based. Taking the UTXO work to a live network (fees, propagation) and functional signatures are future work, not this project — the latter has no existing implementation and would mean redoing the whole project.

**Next steps.** Fix the Bitcoin transaction breakdown; keep studying the work so it can be explained end-to-end in the video; start draft slides for a **6–8 minute presentation** to give to Wang the week after next for comments. Wang has not yet checked the LAS API/implementation details himself and will do so later.

**Next meeting:** next week.

---

## C. Cleaned transcript

### 1. Opening: figures belong in the report, not all in the appendix (00:00–00:36)

**Wang (00:00)**
Right? You don't need to [put everything in] the figures.

**Royce (00:04)**
I don't, yeah — [unclear].

**Wang (00:10)**
Anyway, okay, then that's [unclear].

**Royce (00:13)**
The only thing I did was, like, I threw everything into the appendix — [unclear].

**Wang (00:20)**
Yeah, you kind of — I'm saying, you know, it's — but yeah, just [keep] the most interesting figures…

**Royce (00:26)**
Yeah, figures in the—

**Wang (00:28)**
Yeah, in the report. To show a result. Okay, so would you like to first summarise what we have done?

---

### 2. This week's work: the UTXO comparison; the proof size is large (00:36–01:57)

**Royce (00:36)**
Oh, yeah. What I've done for this week is I tried to compare it with the UTXO — on the UTXO, okay?

**Wang (00:47)**
[unclear] the atomic swap.

**Royce (00:52)**
Apparently the proof size is pretty large.

**Wang (00:56)**
[unclear — "the proof size of LAS?"]

**Royce (01:01)**
Of, yeah — of the LAS…

*[ASR dropout ≈01:01–01:35]*

**Royce (01:35)**
Yeah, of course.

**Royce (01:49)**
Oh yeah, I did something like a simulation on the — using **Rust**, I'm not using C.

**Wang (01:57)**
Okay.

---

### 3. The three configurations — and why the classical one has no ZKP (01:58–02:45)

**Royce (01:58)**
I compared the classical [adaptor signature] with LAS + Groth16, and LAS — you've seen — with LaZer.

**Wang (02:07)**
Okay.

**Royce (02:08)**
For the classical, I didn't use any ZKP, because I think for the classical — the protocol they've done doesn't specify a ZKP during the [exchange].

**Wang (02:20)**
Okay — so in our construction they don't have that component?

**Royce (02:25)**
Yeah, they only have like a discrete-logarithm equality [proof — DLEQ].

**Wang (02:31)**
Oh, okay.

**Royce (02:32)**
Okay, they don't — there's no ZKP in the protocol for the classical adaptor signature on the elliptic curve.

**Wang (02:41)**
Okay, okay.

---

### 4. Timing vs proof size: Groth16 slower but smaller, LaZer faster but bigger (02:45–04:12)

**Royce (02:45)**
For the Groth16, it is—

**Wang (02:49)**
This is [on] both sides — what's the unit of the size, right?

**Royce (02:53)**
Yeah, this is the — personally, probably I start with the timing. I've also already updated the [table for the] paper.

**Wang (03:01)**
Okay.

**Royce (03:04)**
The timing — [Groth16] takes longer, but the proof size in bytes is smaller. Yes, sure, this one. Yeah, yeah. For using Groth16, the time to generate the proof is larger, but the size is smaller. But using LaZer—

**Royce (03:35)**
It's like the time is quite small, but—

**Wang (03:39)**
The time — why don't I check that? It's almost the same, right? I mean, the timing — why?

**Royce (03:46)**
I just wonder.

**Wang (03:48)**
Okay, the proof — okay.

**Royce (03:50)**
[These are] microseconds.

**Wang (03:52)**
Wait, wait — [I was reading] seconds. Microseconds. So—

**Royce (03:56)**
Almost — [unclear]. One second is one million microseconds.

**Wang (04:04)**
Okay, so this is less than a minute, alright?

**Royce (04:07)**
Yeah, not bad. I mean—

**Wang (04:09)**
Yeah, I mean, yeah — LAS, [unclear].

---

### 5. Post-quantum proof sizes: 30× — and ~300× on-chain (04:12–05:18)

**Royce (04:12)**
The only problem with post-quantum ZK[P], obviously, usually, yes — is just the size. It's almost like, I don't know, a hundred times probably.

**Wang (04:29)**
I — yeah, but [what I see is] 30. Thirty times, okay? But this is often [the case].

**Wang (04:42)**
[unclear], okay? Here are the on-chain components, right — 300 times. It's okay, we are — [unclear]. Yeah, I mean, it's what we expected for us. That's why — I mean, people argue that most of the time the security is good, but the size is not good. The on-chain [part].

---

### 6. The core question: what is the structure of the transaction? (05:18–07:53)

**Wang (05:18)**
Okay, which is one of the transactions? Wait — transaction. Um, okay, so sorry, here I'm getting confused.

**Wang (05:31)**
So for the — for the transaction, the transaction sizes… what's the structure? What's the structure of the transaction?

**Royce (05:44)**
The structure of the—

**Wang (05:45)**
What would it contain? What are the components, and—

**Royce (05:52)**
For the off-chain—

**Wang (05:54)**
Here — I mean, because you said that we have a transaction, transaction one and transaction two, right? So I was wondering: what kind of [fields] have we changed inside the transaction? Have we changed the public key, or anything else?

**Royce (06:17)**
For the pre-signature — this one, the pre-signature consists of a challenge and a response. Uh…

**Wang (06:24)**
Yeah, the challenge and the response. Yeah — but as you said, this is in a signature, right?

**Royce (06:30)**
This one is [the] pre-signature and transaction one. It's a bit weird.

**Wang (06:39)**
Well, because — wait, okay, yeah.

**Royce (06:44)**
Yeah, I should break down what is—

**Wang (06:48)**
Yeah, because you can see, I'm not sure if the transaction has already contained the witness [or] the signatures. Oh, this is the proof — you can see, this is the proof, right? This is the proof.

**Wang (07:03)**
The proof has not been included in the transaction, so—

**Royce (07:07)**
I don't think the proof is included.

**Wang (07:09)**
The proof [doesn't need] to be included in a transaction — but I was wondering, inside of the transaction, what kind of other stuff should be included?

**Royce (07:17)**
On-chain.

**Wang (07:18)**
Yeah. Because this is the [key] question — why the [components]? We also have the transaction — what [does] the transaction [look like] here?

**Royce (07:29)**
It's transaction one, the statement Y, the proof and the pre-signature. Oh — apparently the pre-signature also includes the transaction. Yeah, the transaction is like the message, I guess. So… let's say it's a good question.

**Wang (07:53)**
Sorry, okay — so the transaction means the [signed] transaction, or the—

---

### 7. "Transaction" is being used to mean "the signed message" — use another term (07:59–09:26)

**Royce (07:59)**
I think [the] transaction is kind of like the message.

**Wang (08:03)**
Message, okay. So then you would be better to use another term — because "transaction" here, we are in the context of blockchain, of Bitcoin, right? So there we are talking about *the* transaction. So normally the transactions, they will have the — [they have] a predefined format, which should follow the definition of [a] Bitcoin [transaction].

**Royce (08:38)**
It doesn't specify what it should be, so I just felt like using, like, the static size of [unclear], so I just made it [that way].

**Wang (08:51)**
Yeah, yeah — so that's why I was wondering why this is larger than that, larger than that, very much larger than the overall.

**Royce (09:01)**
Like, um — it makes sense. It makes sense. Oh, I think because it includes statement Y, I think, if I'm not mistaken.

**Wang (09:13)**
So let me start — okay, here is: if it included [the] statement… here you can check. That does mean that the statement in the [transaction is] unchanged.

**Royce (09:26)**
I believe it is, because the signature is just the pre-signature added by the witness, or the secret.

*[ASR dropout ≈09:26–10:13]*

---

### 8. The ask: a diagram of the original vs the modified transaction (10:13–13:12)

**Wang (10:13)**
I'm going to show a diagram.

**Wang (10:33)**
Yeah, I guess sometimes — so I wanted to say, here, this is a typical [Bitcoin] transaction; that's the structure of a transaction. So I would like to say: after we added the witness, [after we added] the adaptor signature, how the transaction will be changed. Okay — which component will be added at that moment, [what] we add. So we'll have two [versions]. Okay, how are we changing this? Because here — so I wanted you to make sure: okay, how are we integrating into the existing Bitcoin transaction?

**Wang (11:15)**
Okay, so that's what I mean. Right here they are just the [theory/papers], so they don't specify in practice how the structure of the transaction will look like — but we are implementing [it]. It's very important to specify which component you should add, which [field] or other stuff. Okay, so again, as you said, it would be better to break down the transaction to show the different components, and it would be helpful to add a diagram in your report. For example, imagine [the reader]: you don't know how the transaction looks like — then if you give a picture, if you give a diagram to show that, okay, this is the original transaction…

**Wang (11:59)**
…and this is [the] modified transaction, by adding [the] adaptor signature. Then we can see, okay, which components have already been changed. Then we [can reason] on the side of why the communication size — the size — will be larger.

**Wang (12:15)**
I see, it's okay. Yeah — here it's kind of abstract. Yeah, we should…

**Wang (12:20)**
We should give more details here: break down the structure, and also highlight which part has been modified. Yeah, [then] we can have a better understanding. So I mean, the results, I think, are good — not [necessarily] good, but it's what you expected.

**Wang (12:43)**
Alright, it's fine, because we can argue that that's what we [expected]. Yeah, we still need [things to be] more efficient.

**Royce (12:53)**
So some people usually [put] the witness into the chain. Some people don't have—

**Wang (12:58)**
I don't know. I mean, for this, you should make sure — right, you should make sure how they use the witness here.

**Wang (13:05)**
You could just Google how a Bitcoin transaction looks like. This is the standard transaction structure.

---

### 9. Challenge size follows the NIST security parameter (13:12–14:51)

**Royce (13:12)**
And then I think [I] also make something, just on the challenge — I just followed the NIST security parameter, I guess. So I believe this is like [the size] for the security parameter times 8 [bits]. So for 32 [bytes] it's times 8, if I'm not mistaken.

**Royce (13:41)**
So we shrank the [165-byte] size a little bit, [unclear]. So yeah, I just make sure that according to the NIST security parameter [it is] already [right]. So yeah, I just try to make things as small as possible, to decrease [the size], except for the — the thing. Except this is just more like a simplified version of it, I guess. So — I found it like the signature percentage?

**Royce (14:24)**
I guess, across the security parameters, it's—

**Wang (14:29)**
What is [z] — it's the response, right? Yeah, yeah. So okay, yeah, yeah, yeah, it's okay. Make this — I'm going to [call it] challenge, [and] it's [something] like a—

**Royce (14:37)**
And fixed the [format] of the signature [system].

**Wang (14:42)**
So yeah, the signature will have two components, say — and that [c] is the challenge, [and z] is the response.

---

### 10. "Investigate how it integrates into the actual transaction" (14:51–15:41)

**Royce (14:51)**
Okay, yeah. Here, for the — yeah, actually, regarding the implementation, I don't have the clear picture. I think I have to—

**Wang (15:07)**
Yeah, you should investigate a bit more, especially how it can be integrated into the actual transaction, right? How we will modify that. So for example, when they're sending the transaction, how we add — I don't know — the pre-signature, how we add the witness. Yeah.

**Wang (15:29)**
Yeah, I think that's the most important thing [remaining]. [Otherwise] I think, yeah, we've done a great job.

---

### 11. Why is LAS Adapt ~270× ECDSA's? (15:41–17:44)

**Royce (15:41)**
Then, for — yeah, [key generation] is also very simple. For the key generation, I compared, since it's the native API with the [unclear] core, I guess — because they don't [pack] bytes, so it's like 2 times. And the most expensive, but I guess, is the Adapt. So the ECDSA compared with the [LAS one], I guess it's like two hundred…

**Royce (16:14)**
…seventy times.

**Wang (16:14)**
Okay, sorry — [can you remind] me: in the Adapt function of LAS. Yeah — what was the [reason] why it costs so much?

**Royce (16:27)**
I think because it involved — I think the only huge difference, I think — it shouldn't be that expensive, because we're just adding some secret into it. But apparently the ECDSA is very cheap on [its side]. Yeah, I think because the signature itself, it's kind of — I mean, the pre-signature is huge.

**Royce (17:05)**
So even adding small things into the big things just costs [more]. [unclear] I don't know why.

**Wang (17:17)**
Okay, okay.

**Royce (17:18)**
Yeah, I don't know why the ECDSA is just very small. I guess it makes — it's only like 1.5 microseconds.

**Wang (17:26)**
Have you checked the algorithm of ECDSA — how the adaptor signature looks like there? You can see it's very, very fast.

**Wang (17:36)**
It's very efficient, yeah. So that's why — that's why we can say the size is already small.

---

### 12. The EVM and Naysayer: finish Bitcoin first (17:44–19:49)

**Royce (17:44)**
And — yeah, I mean, I need to investigate more why it's 250–270 times. I'm not sure if I'm going a bit too far on the EVM. I've tried using, like, Naysayer, and then—

**Wang (18:09)**
The what, like? It's—

**Royce (18:12)**
The — poqeth, the one that poqeth implements. So it's kind of like we don't verify everything at [the] first stage: assume that the verification is fine, until someone [disputes it].

**Wang (18:29)**
Yeah, there were two [paths] — dispute, or challenging.

**Royce (18:35)**
But yeah, I'm not sure whether I should move on to the EVM, or I just focus on the—

**Wang (18:40)**
We should have first finished this one, and [each] time we prefer to focus on that one — and then you can also think of: can we apply that technique on top of Bitcoin? Yeah. So I mean, you can have this discussion as well, but yeah, I would suggest that let's first figure out how it works properly here, right?

**Wang (18:59)**
Yeah, I mean, because for that it's the same, right — even for the EVM. If you want to do that, it also needs to specify how the added things [sit] inside of the [transaction], or inside the smart contract. Yeah. Okay. Yeah, I would suggest that let's first fix this issue, and then [use] the time [you have]. So of course you can definitely try [the] EVM, but as [I said], I would like to see a fully complete finished solution first.

**Wang (19:32)**
Yeah, [then] you can discuss more advanced solutions.

**Royce (19:35)**
Yeah, because I've kind of lost — I think just, yeah. So for the LAS [unclear].

---

### 13. Same structure whichever ZKP: the pre-signatures and proofs must go in the transaction (19:49–21:37)

**Wang (19:49)**
So I mean, for LaZer — no matter how we use LaZer or Groth16, I mean, the structure shouldn't [differ]; the components, they should be the same, right, for the same [protocol]. Again, the most important thing is to figure out how we put the [ZK] proofs — so the pre-signatures, and those proofs — inside the transaction.

**Royce (20:08)**
I see.

**Wang (20:09)**
Because here it's like that we calculate — you calculated the transaction and also the pre-signature. So my imagination is that the signature should be put inside of the transaction.

**Royce (20:19)**
[Where is it] in your [view], in the transaction? The signature, the signature? Yeah.

**Wang (20:23)**
Otherwise, [how] would [a node] confirm that it's [correctly signed]? [unclear]

**Royce (20:33)**
Yeah, I think the transaction[s are] inside the pre-signature [message]. If I'm looking from the algorithm—

**Wang (20:40)**
The one—

**Royce (20:41)**
I think, looking from the algorithm — I think the transaction, because we are like pre-signing on the — I [take] the public key and then statement Y, and [the] transaction.

**Wang (20:55)**
I mean, it's more like here — it's more like people who sign, they are signing the transaction. So that's why you can see: of course, the signature — the pre-signature — should correspond to the transaction. But again, I was saying, okay: how [will] the transaction look like?

**Wang (21:16)**
And how we put the witness, the signatures, on-chain.

**Royce (21:22)**
So, is the transaction not just like a kind of static message or something? Or is it like in a [chain], or on the blob, or something?

**Wang (21:37)**
So can you Google the Bitcoin transaction? How [does] a Bitcoin transaction look like? Just Google it, okay?

---

### 14. The paper oversimplifies the architecture; follow the standard structure (22:02–23:52)

**Royce (22:02)**
So the paper is just a very oversimplification of the architecture, I would say.

**Wang (22:08)**
Yeah, that's [right].

**Royce (22:21)**
Actually the structure [is] something like this — the pro[of]…

**Wang (22:32)**
I see. So you're going to see — you should contain some [of these fields] at least, because a lot of other people can verify [it]. You'll see — I was supposed to wonder [how] in practice [we] hook up the signature with the transaction.

**Wang (22:57)**
And if you don't [do] that, then how [do they] confirm the transaction?

**Royce (23:01)**
I see. So I just, like, follow this structure probably. Yes.

**Wang (23:13)**
Yes, yes — the standard [one], yeah. Just, yeah, just tell me, okay: which part, which parts should be modified compared to the standard transaction? I see.

**Royce (23:41)**
So the next thing that I should do — just fix the Bitcoin part. And then—

---

### 15. Priorities: make sure the Bitcoin part is ready; don't over-add features (23:52–24:42)

**Wang (23:52)**
If you have time you [can] try [more], then you have — yeah, absolutely as well. Okay. If you don't have time, let's say [make sure] the Bitcoin part [is] ready.

**Wang (24:01)**
[That] should be okay. Okay — what do you do now?

**Royce (24:08)**
I think that's it for the project, because if I [add] more features, I'm not sure whether I can put everything [in].

**Wang (24:19)**
Yeah, yeah, right. Yeah. Just make sure that the project is good.

**Wang (24:28)**
You don't need to contain all [the] stuff — you just need to make sure [that] what you have done looks good, looks perfect, looks great.

---

### 16. Future work: reduce the proof size; IPFS as a fallback for large data (24:42–27:57)

**Royce (24:42)**
So, actually, the future work that I mentioned in the report — is it correct? Or the one that, if I have time, I should reduce, like, the proof size — the [size] on the LAS protocol?

**Wang (24:56)**
Yes, yes, that's the most challenging part, I think. Yeah, because you can see here — where is it? It's very expensive, right?

**Wang (25:04)**
And now this one right there — the size of… yeah, it's very, very expensive. It [would be] more practical [if it were] more [compact], right? More or less.

**Royce (25:15)**
Yeah, I've already used LaZer — it's, oh, even it's faster, but it just generates more [size], quite simply.

**Wang (25:23)**
Very — of course, the on-chain parts [are] very important. People don't want to pay more.

**Royce (25:29)**
Yeah, yeah, yeah. And if I have time as well, like — [get] the on-chain verification [in]to one transaction that fits in, like, the limit on the EVM.

**Wang (25:46)**
Yes, yeah, yeah, that's another [one]. Yeah, I mean, this is [fine], alright. Here we don't have the limit, but maybe you should well check for Bitcoin.

**Wang (25:55)**
Should we have a limit for this?

**Royce (25:59)**
Yeah, I think Bitcoin[ has] some. I mean, it's — but not as strict as the EVM, I would say.

**Wang (26:09)**
Yeah, I just have a little [concern]. I mean, we can always have the backup solution, which means you can ask people to — you can put their proofs on top of a decentralised storage system, and the miners can refer to there. But there, it means that, okay, in this way we will have to involve another component, another…

**Wang (26:29)**
…blockchain, or another platform. Okay — do you know IPFS?

**Royce (26:36)**
[unclear]

**Wang (26:38)**
Yeah, there are some — IPFS, yeah? Yes, like this. Yeah, so it's a decentralised storage platform.

**Wang (26:49)**
So basically, sometimes, if you would like to put a somewhat large, uh, data like this, you can put that — but you don't want to invoke any centralised components, then you can refer to that platform. Which means, if the size is very, very huge — like if we are doing some models, you can put the models there, and then you can ask the verifiers, or the miners, or [validators]: they can refer to the data [at that] address. So here again, it means that we have to invoke some — at least it's some cross-platform bridges.

**Wang (27:33)**
So what I want to say is that we can always have a solution, but the question is how good the solution is. I see. Okay.

**Wang (27:42)**
Yeah, you [could] add this in your future work — your discussion — [about putting] something off-chain. But the question is, if you put something off-chain, who will maintain the storage of the off-chain components?

---

### 17. Scope: no second signature scheme — polish what exists (27:57–28:45)

**Royce (27:57)**
Yeah, I'm also wondering that. I think in the proposal we mentioned this project might [involve] more than one signature. So in my case, I [only did] one signature.

**Wang (28:09)**
What do you mean? That's a single—

**Royce (28:10)**
Like during the second [stage], it's—

**Wang (28:13)**
No, no, no. I mean, that's the ideal thing. But I mean, before we start doing a project, we don't know how far we could go, right? Because at the moment, timing — yeah. [That's what] I was focused [on]. Yeah, if you have time you can do that, but I don't think we have enough time, right?

**Wang (28:30)**
You should first focus on polishing what you have already done, and make sure the results there are correct. You should also make sure your report is good.

---

### 18. Word count need not be proportional to the mark weighting (28:45–30:07)

**Royce (28:45)**
And for the word count for each chapter — should it be, like, proportionate to the weighting?

**Royce (28:56)**
Uh, to the [mark] weight?

**Wang (28:58)**
I don't know, to be honest. I don't know what's — what do you have?

**Wang (29:02)**
I don't know how hard the requirement is for the word [count].

**Royce (29:06)**
Yeah, let's see — for example, for the introduction, it requires, like, the rubric says 20%. Part of the [others], 0.3 for the—

**Wang (29:19)**
I mean, that's [the] quota for the scores, right? Yeah.

**Royce (29:22)**
But I think my introduction is—

**Wang (29:25)**
I think — no, I don't think that the word [count] will [matter].

**Royce (29:29)**
[unclear]

**Wang (29:30)**
Yeah, you don't need to make them, let's say, proportional. You just make sure that your report is good. For example, if you have done a lot of interesting stuff, right? Of course, you should [write] more stuff, more things — [about] your results, rather than the background.

**Royce (29:48)**
Yeah, I think the most word count is on Chapter 3.

**Wang (29:54)**
And [the] evaluation chapters—

**Royce (29:55)**
I thought you saw the discussion and—

**Wang (29:59)**
Yeah, well, of course, yeah, that's what you have done. Yeah, yeah, that's the most interesting stuff.

---

### 19. The Bitcoin part: learn it from the documentation (30:07–30:45)

**Royce (30:07)**
Yeah. To be honest, I'm kind of lost, like, for this week, actually. So for the Bitcoin part, it's just learn from the website, I will say — yeah, for how do people implement it. Yeah?

**Wang (30:36)**
Yeah, that's the [standard] Bitcoin transaction. Just follow the structure there, okay.

**Royce (30:45)**
It's — I think I'm also…

*[ASR dropout ≈30:45–31:46]*

---

### 20. zkVMs and RISC-V: probably out of scope (31:46–32:53)

**Royce (31:46)**
So for something like — I think I found this, [it's] wonderful, like… I'm not sure this is still in the scope of the project, but I found, like, the zero-knowledge virtual machines. That's—

**Wang (32:03)**
Okay. Yeah, but that's for the — to prove for the RISC-V. It's not… so maybe it's kind of challenging if you want to integrate that in your system.

**Wang (32:19)**
Because for this, it's more for — I'm not sure [whether] Bitcoin's virtual machine [runs] with a RISC-V. If they are using RISC-V, maybe you can also try this. But if they are not [using] RISC-V, which means this is totally another structure of virtual machine, then you don't need to — maybe you don't need to try it.

**Wang (32:43)**
Yeah, it's a — it's time-consuming, I'd say. But yeah, if you want to do this [and take it] further — yeah, [I would] first [say] try that if you have time.

---

### 21. Word count: 9000 words; Chapter 5's title (32:53–34:23)

**Royce (32:53)**
I think for this week I've spent a lot of time, like, to make sure the report fits, like, 9000 words — because previously it's just way over it. Yeah.

**Royce (33:28)**
So for the structure of the report, is it already correct to provide, like, Chapter 5? I think, after 5—

**Wang (33:39)**
What is [it] for 5? No — it's conclusion and critical reflection.

**Royce (33:50)**
Do I need to include [it]?

**Wang (33:55)**
[unclear] — that should be fine enough. Okay, by changing [the] title.

**Royce (34:00)**
The title should be—

**Wang (34:02)**
Shouldn't it be good? "Conclusion, critical reflection [and] future [work]"? [unclear]

**Wang (34:14)**
Yeah, that's good. That's good. I like it.

**Wang (34:20)**
Okay, good.

---

### 22. So what should I do next? — the transaction breakdown (34:23–35:05)

**Royce (34:23)**
It's so funny to speak — so what should I do? [What] should [I] actually be doing? So I don't [want to be] probably, like, wasting time going [in the wrong] direction.

**Wang (34:34)**
I mean, as we discussed, right — the breakdown of the transaction. Yeah, to make sure that everything's good.

**Royce (34:45)**
So for the [breakdown], I'm also putting [it] in a report on—

**Wang (34:53)**
This is in which se[ction]—

**Royce (34:55)**
Oh, like — oh, just, yeah?

**Wang (34:57)**
Yeah, you can add some — some [detail] on it.

---

### 23. Limitations discussion: rejection bound budget; the hint optimisation; statement Y is the biggest object (35:05–39:31)

**Royce (35:05)**
Yeah, yeah. Yeah, I think there are five challenges — six, I guess. So the first one is the rejection bound budget that I should modify from…

**Royce (35:21)**
…Dilithium. And then, they also have optimisations that probably make the Adapt and Extract not going to be, like — what do you call it — like symmetric. Because — yeah, I mean, they don't have [it] for the optimised [version]. I believe that the public parameters generation, it's not, like, symmetric — like, because they use, like, [the] hint or something, so they don't actually generate the full public parameters.

**Royce (36:04)**
Okay, so somebody using the Dilithium, they kind of have to derive the public parameters using the hint, okay?

**Wang (36:13)**
Okay, [what] are you doing [about it]?

**Royce (36:14)**
So I don't think—

**Wang (36:16)**
[In] your implementation — have you also used that?

**Royce (36:20)**
I didn't use it, because I believe [in] the Adapt and extraction process it's not going to be working if both parties [are not] having equal, like, [values] — statement Y [unclear].

**Wang (36:38)**
That's an interesting question.

**Royce (36:41)**
Or should I try to—

**Wang (36:43)**
I mean, [if] you have time, you can try it. But what kind of gains can you achieve? Where are you saving?

**Wang (36:50)**
Where are you saving — sometime like the computation?

**Royce (36:54)**
No, I see [it in] the size.

**Wang (36:56)**
It's the size of—

**Royce (36:58)**
The size of statement Y — that's probably, like, the most huge one, [is] my belief.

**Wang (37:06)**
Okay. [And] the on-chain part?

**Royce (37:11)**
I think that — yes, I think statement Y is on-chain as well.

**Royce (37:36)**
Here we go: statement Y is huge, as a public parameter, I would say. So if that can be reduced?

**Royce (37:46)**
I think that will also reduce the on-chain part, because statement Y is always used.

**Wang (37:56)**
Yeah, you're going to try. If you have a [go at it], maybe you can put it for the week after.

**Royce (38:00)**
Next week — yeah, we'll [do it] after next week. Yeah.

**Wang (38:02)**
Yeah, because it's an interesting direction to go. I don't think you have tried this. It's more or less like some engineering improvement here, so if we can use this solution to reduce the size of the statement — okay, that would be very helpful in practice. But again, as you mentioned, we are not sure if they can — I don't know [if] we can still do the extraction properly. Yeah.

**Royce (38:36)**
But probably I can try. I probably—

**Wang (38:40)**
But if they use hints, how do they guarantee the verification process is correct?

**Royce (38:49)**
I think — I'm not sure. Probably the problem [is] going to be on the extraction, but probably — but…

**Wang (38:58)**
But we can make sure that the verification parts can be correct, right — should be correct. If so, the only different thing is the extraction part, right? But Adapt — or the Adapt part. Yeah, I mean, just try it, just try it. I don't know, to be honest.

**Royce (39:19)**
The problem is on the off-chain, when they do the — probably. But I'm not sure, I'm just probably able to try.

**Wang (39:28)**
Yeah, just try — just try, yeah.

---

### 24. "Functions", not "protocols"; figures go in the text, side by side (39:31–41:55)

**Royce (39:31)**
Yeah. And I think the LAS — like, [the] four protocols: [they're] like Adapt, Extract, PreSign and—

**Wang (39:44)**
I would have called them **functions** rather than protocols. So "protocol" means more high-level design. So for example, we [have] consensus protocols; and for this, it's more like a [scheme], and inside of [the scheme]…

**Wang (40:01)**
…we have some functions.

**Royce (40:03)**
Can I see — so they have, like, four functions? So I'm not sure: should I put everything, like, the four functions, into the figure?

**Royce (40:13)**
Because the figure, I don't think, is [counted] in the word count.

**Wang (40:21)**
So yeah, you can — you know, you have four graphs, right? Four figures — then you can just [put] them side by side. That's okay.

**Royce (40:31)**
So I don't think it's the correct way, like, [that] I put the figure kind of at the end of the chapter — [it] should be in between.

**Wang (40:40)**
[At] the bottom, between the paragraphs. Then people — imagine that we are reading the report, right, we are reading the text; of course, you would like to refer to the corresponding figures, right? It's more friendly. I mean, of course, maybe — I know that in some other subjects, like economics, maybe they would like — well, when they're publishing papers, they prefer to [put] the figures in the appendix. Yeah, but their domain, it's different. I see. And again, [what] I would suggest is that [you] try to avoid the case that only one figure…

**Wang (41:16)**
…takes the whole [page]. Yeah — takes everything on one page.

**Royce (41:20)**
You know, this should be in detail.

**Wang (41:22)**
Yeah, you can [put] it in between, or [at] the top, or on the bottom. Just make sure that, okay, we have some space for some text. You can accept [it] except [when] the figure is very huge.

**Wang (41:34)**
[Then] it can actually take the whole space of one page.

**Royce (41:40)**
Oh, okay. So there should be some paragraph in between.

**Wang (41:43)**
Yeah, in between. You can move this, I don't know, [to] the bottom; you can have some text.

**Wang (41:50)**
Okay.

---

### 25. Future work: UTXO on a live network; functional signatures ruled out (41:55–43:23)

**Royce (41:55)**
So, one more thing, probably — [for] the future work. Is it correct, one [of them]? It's like, take the UTXO [work and] study — do [it on] a live network.

**Royce (42:07)**
I'm not sure if that's possible — sorry, like, to take the UTXO, so [to] study, do a live network. Oh, probably we need to study the fees and, like, the propagation, and—

**Wang (42:20)**
Yeah, and also the function of that [unclear], in which…

**Royce (42:23)**
Oh, okay. So — I'm not sure if you want to. Do you think that it's feasible, like, for the remaining time, to explore the functional signature[s]?

**Wang (42:45)**
Because they don't have [an] implementation, yeah, so you should be able to build things from scratch. So yeah, it's very [demanding] — so it's more like [the fact] that you have to redo the whole process of what we have done for this project.

**Royce (43:00)**
And it's probably not going to fit in the report.

**Wang (43:03)**
Yeah, yeah, I don't say you need to do that.

**Royce (43:06)**
Yeah. Surprisingly, like, the report is, like, pretty short for me.

**Wang (43:10)**
Yeah, yeah, yeah, yeah, [I agree]. Yeah, because you [only] have three months, right, [that were] left — three months. So yeah, that's why you don't need to, like, [write] 100 pages.

---

### 26. Remaining time: be able to explain everything; the blockchain part is what's lacking (43:23–45:02)

**Royce (43:23)**
So [for] the remaining time, I just [want] to make sure that I study everything and [am] able to explain everything that I have. Yeah, yeah, like in—

**Wang (43:32)**
In the video, right — in the review, yeah. And [since] you don't [have to] have this — [unlike] the defence, the face-to-face interview, [the] face[-to-face] presentation — usually make sure that [when] recording, that the report [is] as perfect as you can make [it].

**Royce (43:52)**
Okay, okay. So what do you think [is] still lacking for me is, like — I think the [blockchain] part, I think, is still lacking. I'm aware of that, and what I think — I'm still aware that the blockchain implementation is still [incomplete].

**Wang (44:09)**
Yeah, yeah, yeah, especially for the transaction components. Yeah, yeah, yeah — because you should make sure that it's already done. If that is ready, I think, yeah, you are already good enough for the final report. But if you have time, you can also try to [add] stuff.

**Royce (44:21)**
The EVM, yeah — and make the optimised [version].

**Wang (44:25)**
Yeah, the hint solution — to try, to try. Yeah, there are two [things] to do this, if you want to, like, try. But if you don't want, if you don't have time to try them, I think it's also acceptable for me as it is, okay. But just make sure that the UTXO [part] is still [solid].

**Royce (44:41)**
Today's — [the UTXO] part is the most important one. After the evaluation, [should] I do the EVM first, or the hint optimisation?

**Wang (44:49)**
Yeah, up to you. [unclear]

**Royce (44:53)**
And then—

**Wang (44:54)**
And then, of course, the report. It's like, yeah — the report, and also the video. Yeah.

---

### 27. Presentation: draft slides, 6–8 minutes, the week after next (45:02–45:52)

**Royce (45:02)**
Okay, okay, okay. So in terms of explainability — so should I study more? What, in terms of explainability?

**Wang (45:11)**
[unclear]

**Royce (45:12)**
And as far as I [can] explain everything to you — so I think it's a big picture.

**Wang (45:18)**
Yeah, yeah, yeah, you can try. You can make some slides. You can start making some slides, right.

**Wang (45:22)**
When you record the video, I believe you should use some slides, right. You can try. Yeah — okay, maybe the week after next week, you can make some draft of your slides.

**Wang (45:35)**
And you can give me a presentation here, like 10 minutes. I mean, of course — how long should it be? 6 to 8 [minutes]. Okay, you can also try that, like that.

**Wang (45:46)**
You can give me a presentation. I can — I can give you some comments or suggestions.

---

### 28. Is the LAS API already correct? (45:52–46:53)

**Royce (45:52)**
And for the API parts of the LAS signature — I think, is it safe already, or should [I check whether] there is some, like, critical [issue]?

**Wang (46:03)**
Probably [you should] tell [me] what—

**Royce (46:05)**
I think that the LAS signature API, or the functions that [are] already implemented — on the blockchain.

**Wang (46:17)**
Because you—

**Royce (46:18)**
[unclear] — makes sense. So my implementation should probably—

**Wang (46:24)**
[For] now I haven't [had time] to check the details, but [I assume] you'll have done it correctly. Yeah, just — you should [do] a check, and finally I will check it.

**Royce (46:32)**
I mean, I've tried my best to check it, but yeah, I hope it's — yeah — correct.

**Wang (46:37)**
Okay, please, okay. Okay, good. I think, yeah, that's it for today. Yeah, okay, good. Okay, okay, okay — see you next week.

**Wang (46:52)**
Yeah, see you.

**Royce (46:53)**
Yeah, thank you, Doctor. Okay. I feel like this week I'm not [unclear]…

**Royce (48:26)**
[Post-meeting] — see you next [week].

---

## D. Action items arising

| # | Action | Owner | Source |
|---|---|---|---|
| 1 | **Break down the Bitcoin transaction structure in the report** — state exactly which components the adaptor-signature protocol adds (pre-signature, statement Y, witness; the proof appears to stay off-chain) and where they sit in a standard transaction. | Royce | §6, §8, §14 |
| 2 | **Add diagrams: original/standard Bitcoin transaction vs the modified transaction**, highlighting which fields changed. Use them to justify the reported communication-size increase. | Royce | §8, §14 |
| 3 | Stop using "transaction" to mean "the signed message" — use a different term, since "transaction" has a fixed Bitcoin format. | Royce | §7 |
| 4 | Investigate how the pre-signature and witness are actually integrated into a real transaction when it is broadcast (check how existing implementations carry the witness); Google/read the standard Bitcoin transaction structure rather than relying on the paper, which oversimplifies the architecture. | Royce | §8, §10, §13, §19 |
| 5 | **Finish the Bitcoin/UTXO solution before any further EVM work.** Naysayer/poqeth-style optimistic verification stays a discussion of a more advanced solution; the same "where do the added components go" question applies to the EVM anyway. | Royce | §12 |
| 6 | Report quality over proportional word counts — chapter length need not match the rubric's mark weighting; write more about results, less about background. Report currently trimmed to ≈9000 words. | Royce | §18, §21 |
| 7 | **Embed figures in the text** (between paragraphs, top or bottom of the page), not collected at the end of a chapter or in the appendix; group the four LAS function figures side by side; only genuinely large figures may take a full page. | Royce | §24 |
| 8 | Title Chapter 5 **"Conclusion, critical reflection and future work"**. | Royce | §21 |
| 9 | Call PreSign / PreVerify / Adapt / Ext **functions**, not protocols. | Royce | §24 |
| 10 | Optional, target the week after next: try the **hint-style optimisation to shrink statement Y** (the largest object, and on-chain), and check whether verification/extraction still work. If there is no time, that is acceptable. | Royce | §23, §26 |
| 11 | Optional: check whether Bitcoin imposes a relevant size limit; if data is too large, consider decentralised storage (IPFS) as a documented fallback in future work — noting it drags in another platform/bridge and raises the question of who maintains off-chain storage. | Royce | §16 |
| 12 | Do **not** add a second signature scheme (as the proposal allowed) — no time; polish what exists and confirm the results are correct. zkVM/RISC-V is out unless Bitcoin's VM is RISC-V-based. Functional signatures and live-network UTXO deployment are future work only. | Royce | §17, §20, §25 |
| 13 | **Prepare draft slides for a 6–8 minute presentation** to give to Wang the week after next, for comments; the same material underpins the video. | Royce | §27 |
| 14 | Investigate why LAS Adapt is ~270× ECDSA's (~1.5 µs) — current explanation is only that the pre-signature is large. | Royce | §11 |
| 15 | Wang to check the LAS API/implementation details himself at a later point. | Wang | §28 |

**Next meeting:** next week.
