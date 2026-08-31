# Cleaned Transcript — Meeting 11 with Zhipeng Wang (deck review, second pass)

**Date:** 2026-08-21 — **inferred from file metadata, not confirmed.** `meeting11.mp3` was written to disk at 14:30 on 2026-08-21 and the recording runs 16:14, which puts the start at ≈14:14 if the file was saved at the end of the meeting. No calendar record was consulted. The date is corroborated by content: the five instructions given here are the ones already recorded in `CLAUDE.md` as the "second batch, same day (Wang via Royce, 2026-08-21)", and the deck already carries fixes attributed to that date.

**Source:**
- `meeting11.mp3` — SHA-256 `6983bd355f03605c95d367038e015d8e225bccafb5e382b9c40c05face0f6f76`, 16:14, 64 kbps stereo.
- Transcribed to `meeting11_whisper_transcript.md` — SHA-256 `0b100a1b58b08b60116f44435e04e61ddacb3af7626a3fb832d4ad0956dee3e5` — with faster-whisper 1.2.1, model `large-v3`, CUDA float16, VAD filter on, beam 5. Detected language `en` (p=0.94). 310 segments.

> ⚠️ **ONE SOURCE, AND IT DOES NOT SAY WHO SPOKE.** Unlike Meeting 10 (Samsung + Teams), this meeting has a **single** ASR pass and **no diarisation whatsoever**. Whisper emits timestamps only.
>
> **Every speaker label in this file is an inference from content**, not a property of the source: instruction-giving and blockchain exposition are read as Wang, questions about "my implementation" and the deck's internals as Royce. The polarity is anchored by unambiguous turns — "I think I haven't included" (01:21), "for my implementation, I guess I haven't implemented" (10:39) are Royce; "you should", "maybe you could" throughout are Wang.
>
> **Turns marked ⟨?⟩ are attributions I could not settle from content.** Do not quote any labelled line as evidence of who said it without re-listening to the audio at that timestamp. Nothing in this file should be promoted to a supervisor ruling in `CLAUDE.md` on attribution alone — see §D.

**Type:** meaning-preserving cleaned transcript, not a verbatim or legal record. Unintelligible fragments are marked `[unclear]`; reconstructed words are in `[square brackets]`. Timestamps are Whisper's segment starts.

**Main topics:** A second pass over the video deck, one week after the Meeting-10 mock. Wang works through it from slide 1. The five instructions: **lead with quantum-computer background and urgency** (why an expensive exotic post-quantum signature is worth paying for at all); **close on "is LAS good or not" plus recommendations for developers and the blockchain community**; **show the transaction structure of both Bitcoin and Ethereum**, because "people want to see how it works in practice"; **make the swap followable by someone who does not read the maths**; and **use real logos and real prices**, not invented numbers, "because you are doing a scientific report". Separately he challenges the phrase **"the adaptor layer is nearly free"** — free against *what*? — and rules that the comparison must be named as against the lattice base signature, not ECDSA. A long exchange explains **EVM versus Bitcoin** (state machine, opcodes, gas) and settles that the current framing is "a bit misleading", with "more flexible" as the safer word. Royce discloses that his implementation swaps **UTXO with UTXO**, not Bitcoin with Ethereum; Wang accepts this and advises it is "safer to just talk about UTXO". The meeting closes on **throughput/TPS**, where Wang walks through deriving transactions-per-second from transactions-per-block and the block interval — ⚠️ see §D action 1, this collides with a documented project rule.

---

## A. Key contextual corrections

Whisper's word accuracy is high on plain English and poor on the domain vocabulary. Every substitution below is applied silently in §C.

| ASR phrase | Corrected meaning |
|---|---|
| DLT / Genesium / Genesium signatures | **Dilithium** — the lattice base signature. Confirmed by context at 02:31–03:14, where it is contrasted with ECDSA as the thing LAS is layered on |
| DLS (14:48–15:00) | **BLS** — the consensus-layer aggregate signature. Distinct from the "DLT" garble above; here the topic is signature aggregation on Ethereum's consensus layer |
| adaptive signature / adapter layer / adapter ECDSA | **adaptor signature** / **adaptor layer** — the ASR never spells it; same defect as Meetings 8–10 |
| automatic swap / the ultimate swap / atomic software | **atomic swap** |
| post-cronom / postcard security / post comes quickly | **post-quantum** (security) |
| "migrating from classical signatures to quantum signatures" | **post-quantum signatures** — "quantum signatures" is the ASR dropping the prefix, not a different concept |
| exam client / EAM / EDM | **EVM** — the Ethereum Virtual Machine, and "EVM client" |
| Ops code | **opcode** |
| REST 5 | **RISC-V** — the EVM-to-RISC-V direction Wang mentions as something he is unsure about |
| forages | **bridges** — the cross-chain construction Ethereum uses instead of atomic swaps |
| ERZ20 | **ERC-20** |
| USCC / EGDC / USDC | **USDC** |
| pcdsa / ACDS / ecds | **ECDSA** |
| lab signatures | **LAS** signatures |
| "input D5, decentralized finance" | **DeFi**, decentralised finance. ⚠️ **Not** Dilithium-5 — this is unrelated to the D5 parameter set, do not cross-read it |
| "raise signatures, price signatures, push-code signatures" | **ring signatures, blind signatures, [threshold?] signatures** — the exotic family; the third is not recoverable |
| "a wealth of exotic signatures" | **one of the exotic signatures** |
| "external owner account" | **externally owned account** (EOA) |
| "verification therapy against the transaction content" | **verification directly against** the transaction content |
| "minor" (07:45) | **miners** |
| Raiders (10:37) | **readers** |
| "you should have a website for that" (04:30) | **a slide** for that — the whole passage is about what to put on slides |
| "maybe you can use one site to show" (04:37) | **one slide** to show |
| "the other two comments i have for protection" (05:21) | **for the presentation** |
| "you are making this life very careful" (00:53) | **this slide** |
| "I have two chances" (08:59) | **two chains** — Wang is describing the picture he would draw: one chain each for Alice and Bob |
| "the ball will well-transferred" (09:52) | **Bob will transfer** |
| "one flow can only carry one transaction" (14:07) | **one block** can only carry one transaction |
| "You compare with the countries on TPS" (15:17) | **the current one's TPS** — i.e. the chain's existing throughput |
| "bitcoin is more restricting on there because they have to be modified" (08:06) | **Bitcoin is more restricted … because they cannot be modified.** The negation is dropped by the ASR; the preceding exchange (07:49–07:53, "I don't think this is modifiable" / "it's not") fixes the sense |
| "many doctors or many big companies" (03:39) | **[governments?] or many big [tech] companies** — the first noun is not recoverable; the sense is institutional migration to post-quantum |
| "Maybe, I don't know, PC, TASIC" (10:49) | **[unclear]** — two chain names, not recoverable. Do not guess which |
| "we call it busy" (09:35) | **[unclear]** — probably an address label in the drawing |
| "penalties to address that is, on what, on user" (09:59) | **[unclear]** |
| "for tps 47 transaction" (14:16) | **[unclear]** — ⚠️ do **not** read "47" as a figure; the surrounding numbers (200, 300, 12, 25) are all Wang's improvised round numbers, see §D |
| "you said i've approved the support things" (02:21) | **[unclear]** |
| "the two designers will be going" (00:37) | **[unclear]** |
| "some general company-sized family" (01:51) | **[unclear]** — the sense is the audience coming from a general (non-crypto) background |
| "it will make the explosion of the Ethereum card" (07:08) | **the Ethereum [node/network]** — Wang's illustration of why gas exists |
| "we have two different items, Ethereum and RetroMachine" (11:04) | **[unclear] — Ethereum and EVM-compatible [chains]**, from the Layer-2 list that follows |

> ⚠️ **No numeric figure in this transcript is citable.** Every number spoken here (200 transactions per block, 12 seconds, 300, 25, "47") is Wang improvising round numbers at a whiteboard to demonstrate a *method* of calculation, and he says so — "let's say 200", "let's use 300", "just Google it". None is a measurement, and none may reach the report or the deck. This is the same trap as Meeting 10's §A note on spoken overhead percentages.

---

## B. Meeting summary

**Format.** A working session with the deck on screen, Wang going through it from slide 1. Unlike Meeting 10 there is no uninterrupted delivery — Royce is not presenting, the two are discussing slides directly. The recording opens mid-sentence, so the first seconds of the meeting are not captured.

**The closing slide: answer "is it good", and say what it means for developers.** The recording opens on this instruction: "Are they good or not? … According to what you're [showing] in the results, what kind of recommendations do you have for the developers, for the blockchain community?" The deck must end on a verdict plus practical implications, not on a summary of what was done.

**Precision on the toy examples.** Wang's advice on the motivation slide: practise the delivery, watch the time limit ("we are recording it"), use the **actual logos**, and refer to **real prices** rather than invented ones. His reason is the standard, not the aesthetics: "you are doing some scientific report, so try to be more precise… of course, they're just toy examples, but even for toy examples, try to be more precise."

**Show the transaction structure — for Bitcoin in particular.** Wang asked where the transaction structure was presented and Royce answered that he had not included it. Wang's argument is about the audience a signature paper reaches: "we are giving some formulas, we are giving some algorithms, but in practice… when people are trying to use the adaptor signature [on] the coin, they will say, okay, which field should I add." Screenshots or a field-by-field picture showing how the transaction differs from a basic one would make it tangible.

**"Nearly free" — free against what?** Wang stopped on the signature-generation slide: "you said everything is nearly free… is the basic signature the ECDSA or the Dilithium?" The objection is that the deck also compares against real Bitcoin transactions with ECDSA, so a reader will connect the two claims: "if we compare with the basic Bitcoin transactions, then maybe we cannot say that the adaptor layer is nearly free, because we should first translate from ECDSA to Dilithium, and then we should move from Dilithium to LAS." The ruling: **name the base** — "we should say that we are compared with the Dilithium signatures, rather than the ECDSA." Royce proposed that adaptor-ECDSA versus adaptor-LAS would be the interesting comparison and Wang agreed it could be done. (That comparison already exists in the project — see §D action 2.)

**Lead with quantum computers and the urgency.** Wang's structural instruction, and he tested it by asking Royce which slide showed the importance of post-quantum security — the answer was none. "People will ask, why [are] quantum computers, [post-]quantum signatures important… otherwise people will say, why will we use this complicated and heavy and expensive adaptor signature." The suggested content: institutions and big tech companies are already migrating their **basic** signatures to post-quantum; that is not enough, because more advanced features such as atomic swaps need adaptor signatures, and **those have not started migrating** — which is the project. **Two motivations, both required:** atomic swaps matter, *and* quantum security matters; the project is where the two meet. Wang also wants exotic signatures explained on a slide, or at least prepared as backup for questions.

**EVM versus Bitcoin.** Royce said he had not fully understood the distinction, and Wang gave an extended explanation: an externally owned account calls a smart contract; the transaction carries input data naming the function and its arguments; validators run the client; Ethereum is a world state machine and a transaction moves it from one state to the next; Solidity compiles to opcodes the EVM executes — which is *why* LAS verification can be a contract there. Gas exists to stop an arbitrarily expensive computation. Bitcoin is different: transaction fields are fixed and miners verify directly against the transaction content, so "you have some freedom, but not that much". Wang judged the deck's current framing **"a bit misleading"** and offered the safer word: smart contracts are **"more flexible"**, Bitcoin **"more restricted… because they cannot be modified"**.

**Make the swap followable without the maths.** On the simplified atomic-swap diagram Royce had added after Meeting 10, Wang said it is okay but wants **concrete numbers** and a more concrete drawing: two chains, Alice on Bitcoin, Bob on the other, 1 BTC moving, messages exchanged, and — the point he laboured — the coins do **not** move from one chain to the other. Each transaction happens on its own chain; the swap is the linkage. "Even [if] people don't understand the mathematical details, they don't understand how the LAS signatures work in practice, they can see the protocol, how the protocol works in practice." He flagged this as a personal suggestion rather than a requirement.

**Scope disclosure: UTXO with UTXO.** Royce stated plainly that he has not implemented Bitcoin-with-Ethereum "because they are on different environments" — the implementation swaps a UTXO-based coin with a UTXO-based coin. Wang accepted this: say you swap with another chain, name one. He then explained that ether exists on other chains too (BASE, BSC/BNB Chain, other account-based and EVM-compatible chains), and that within Ethereum, ERC-20 tokens like USDC are governed by smart contracts and swap directly with ETH — so **ETH↔USDC is a bad example**, "people will ask why you need [an atomic swap]". Atomic swaps matter for going *outside* Ethereum, and are "more useful for Bitcoin or UTXO blockchains, because they don't have such kind of fancy smart contracts" — Ethereum uses bridges instead. His ruling: **"it would be safer to just talk about UTXO"**, while noting the Ethereum implementation can still be reported. Asked whether BTC↔ETH is doable in practice, Wang said yes, and that the only difference is where verification happens.

**Throughput and TPS.** Wang returned to the throughput/scalability item from the original proposal and walked through a derivation: if one transaction consumes a whole block, then one transaction per block, and with a ~12-second interval you compute transactions per second — against the chain's current TPS as the comparison. The numbers he used (200 per block, 300, 25) are explicitly improvised. ⚠️ **This conflicts with a standing project rule; it is not recorded as a ruling here.** See §D action 1.

---

## C. Cleaned transcript

### 1. The closing slide: is it good, and what do developers do with it (00:05–00:17)

*The recording begins mid-sentence.*

**Wang (00:05)**
[…] Are they good or not? You know what I mean, right? So here's the final thing: according to what you're [showing] in the results, what kind of recommendations do you have for the developers, for the blockchain community? Okay?

**Royce (00:08)**
Yeah. Okay.

**Wang (00:17)**
And besides that, can you go back to the slides? Check the previous ones. Yeah, we can start from the beginning — the first slide.

---

### 2. Slide count (00:21–00:26)

**Wang (00:21)**
You have 10 slides, right? In total. 10 pages in total.

**Royce (00:24)**
Okay.

> ⚠️ **Discrepancy, unresolved.** `report/slides/video_deck.template.html` contains **13** slide sections, and `CLAUDE.md` records the 13-slide count as deliberate (each addition past Meeting 10's "~10" being a later Wang instruction). Either a different cut was on screen, or Wang is recalling the Meeting-10 target rather than counting. Royce does not correct him and the count is not discussed again. Do not treat this as a ruling to cut to 10.

---

### 3. Motivation slide: real logos, real prices (00:27–01:08)

**Royce (00:27)**
This is the motivation. Here.

**Wang (00:32)**
Yeah. I think you tried to explain it. Maybe you should practise more, because we are recording it, right? Let's make sure that you don't reach the time limit. Okay, the two [unclear] will be going — I mean, it's fine. But then, if you would like — personally, if you would like to make it more beautiful, yeah. Besides, maybe sometimes you can use the actual logo, of course, right? [unclear] And of course, I'm not sure what you're [calling] questions, but you can also refer to the price, to make it more precise. [unclear]

**Wang (00:53)**
Okay, you are making this slide very careful[ly], you know what I mean, right? Not just using the random numbers. Yeah, I don't know what's the price, but [it's] just referring to the price, and you can see the price of what [unclear]. And because you are doing some scientific report, right, so try to be more precise. Of course, they're just toy examples, but yeah — even for toy examples, try to be more precise.

**Wang (01:08)**
Okay, let's see how it works. Let's go back to here. Okay.

---

### 4. Show the transaction structure (01:17–02:09)

**Wang (01:17)**
Where did you present the structure of the transaction?

**Royce (01:21)**
I think I haven't included [it].

**Royce (01:23)**
For the Bitcoin?

**Wang (01:25)**
Yeah, for the Bitcoin, for example. Because people would say, okay, they want to have a much clearer picture of how it will look like in practice. So basically, we are giving some formulas, we are giving some algorithms, but in practice — for example, when people are trying to use the adaptor signature [on] the coin, they will say, okay, which field should I add? So if you could have some actual — even [if it's] screenshots or something — to show, okay, how we will make the transaction different than the basic ones, then I think it would be helpful.

**Wang (01:47)**
Because again, you should consider that, okay — when the [audience] are from some general [unclear] background, then they will see more, I don't know, more tangible things rather than some fancy [ones]. Of course, we should have this sound and also detailed explanation, but if you could have some more visualised, visible slides [showing] the transaction, that would be helpful as well. But you know, I think it's fine.

---

### 5. Where the results start (02:09–02:27) ⟨?⟩

> ⟨?⟩ Attribution in this short passage is not settled. It reads as Royce describing his own deck layout, but it could equally be Wang reading the slide numbers off the screen. The content is not load-bearing either way.

**Royce ⟨?⟩ (02:09)**
Yeah, because I think these two slides [are] not the result yet. I think the result starts from slide six — yeah, so six, seven, eight. I think the results are like the benchmark; I [think] two slides. And this is — yeah. Okay. [unclear]

---

### 6. "Nearly free" — free against what? (02:27–03:23)

**Wang (02:27)**
Oh wait, can [we] go back to the signature generation? You said everything is nearly free.

**Wang (02:31)**
Okay, yes. So this is compared with the — sorry, the basic signature is the ECDSA or the Dilithium?

**Royce (02:37)**
Dilithium.

**Wang (02:38)**
Okay, so…

**Royce (02:39)**
I was [comparing] to the basic signature.

**Wang (02:40)**
[There are] other questions people might ask, right? Here, maybe when you are doing the presentation, you should say something like this: this is **not** compared with ECDSA, right? Because you finally also compared it with the actual Bitcoin transactions, with the ECDSA, right? Then people will ask, okay, so here, are we compared with the basic Bitcoin transactions? It's not, right? Then if we compare with the basic Bitcoin transactions, then maybe we cannot say that the adaptor layer is nearly free, right? Because we should first translate from ECDSA to Dilithium, and then we should move from Dilithium to LAS. You know what I mean, right?

**Wang (03:09)**
So when I was watching this, I was thinking: why would we say that the adaptor layer is nearly free? We should say that we are compared with the **Dilithium** signatures, rather than the ECDSA.

**Royce (03:18)**
Yeah, so the ECDSA part should be interesting to compare with the adaptor ECDSA-based signature.

**Wang (03:23)**
Yeah, you could do that. Maybe you could do it in a smart way.

---

### 7. Lead with quantum computers and the urgency (03:23–04:29)

**Wang (03:23)**
How to do that? Maybe you should see at the beginning — you could give some background of the quantum computers. People will ask, okay, why [are] quantum computers, [post-]quantum signatures important? You should mention that this is very important. Otherwise, people will say, why will we use this complicated and heavy and expensive adaptor signature?

**Wang (03:34)**
You just say that — maybe you should add one slide to show that, okay, nowadays we are seeing, we're observing, that many [governments?] or many big tech companies, they are migrating from classical signatures to [post-]quantum signatures. And we have already seen that, for example, they are replacing their **basic** signatures. But we will see that, okay, but this is not enough — by just replacing the basic signatures. We want to have more advanced signature features, such as the atomic swap. So to achieve such kind of functionality, we should use adaptor signatures. But now they haven't started migrating the adaptor signature. That's why you are doing this, right?

**Royce (04:04)**
Yeah.

**Wang (04:05)**
So here it's more like — okay, you have done, so as we discussed at the very beginning, so for this we should actually have **at least two motivations**. One direction is what you mentioned: that atomic swap is very important, so that's why adaptor signature is very important. But you should also mention that **quantum security is very important**. That's why people are using [it]. So that's why the two directions merge for this project, so you'll have some interesting implementation and exploration. You know what I mean?

---

### 8. A slide for post-quantum, a slide for exotic signatures (04:30–05:01)

**Wang (04:30)**
So you should have a slide for that. Because it would be [in the] title, right? They would like to ask, okay, what [is a] post-quantum signature? Okay, post-quantum signature — okay, that's important. You should explain this. And then you should also explain exotic signatures. So exotic signatures — maybe you can use one slide to show that, okay, there are different exotic signatures.

**Wang (04:39)**
Well, maybe you don't need to share that, but you should prepare that. Oh, sorry — you don't have a presentation [with questions]. It's more like that: you are recording already, but you could show that the next thing is one of the exotic [signatures], and then you should emphasise that this is what you have done. Okay, well, you can do that in your report. Okay, yeah, I think that's my [comment].

---

### 9. Where does the bottleneck happen (05:01–05:27)

**Royce (05:01)**
So it's more like: why is there not much implementation yet, probably. So we must highlight the bottleneck — where the bottleneck happens.

**Wang (05:05)**
Yeah, yeah, you should. Right, you should. So for example, if I ask you: in which slide [do] you show the importance of post-quantum security? Not yet, right? Yeah. So it's more like you assume that. Yeah, yeah — you should first introduce [it], because this is one of the most important motivations, right?

**Wang (05:16)**
Okay, that's the two — I mean, the other two comments I have for the presentation. So maybe, I think the most important one is post-quantum security.

---

### 10. EVM versus Bitcoin (05:27–08:16)

**Royce (05:27)**
So I still haven't quite got the Ethereum and Bitcoin. So Ethereum is smart contract, right? So like, we don't have to change any clients, I guess — maybe it's already everything inside the smart contract, like this?

**Wang (05:34)**
So you know that we have the smart contracts, yes. Let's see. And we will have an externally owned account, which is [not a contract] — but with your wallet you can call this smart contract. Okay, that's when you want to create your transaction. So if you check the transaction, [it] will have some — what we call input data. But here you will input the data you want; sometimes it's about some parameters, and [it] might include the function names and also the corresponding variables. And then, okay, we'll check this.

**Wang (06:12)**
And then normally, what should we do? So in this way, we will — I don't know "we", but the validators, or the Ethereum client — they will run the Ethereum client, and they also have a consensus mechanism. So it's more like what we call a **state machine**. So sometimes Ethereum is more like the world state machine. Which means the transaction will be used to transfer — this is state one of the whole machine, and this is another state.

**Wang (06:29)**
So here the transaction will be converted to what we call some code. The code is a low-level code. So in this way, which means if we can run something in Solidity, then it can convert to the **opcodes**, and then the code can be executed by the EVM client. So that's why we said, okay, if we can implement the functions of LAS — or the verification functions of LAS — in smart contracts, then it can be converted to the opcodes that can be executed by the EVM client machine.

**Royce (06:58)**
So in the sense that Ethereum is more free?

**Wang (07:01)**
Yeah, it's more free, yeah, that's true. So that's why they have a limit, the gas needed in the smart contracts. Otherwise, for example, I just input a very, very complicated calculation, right? Then it will make the explosion of the Ethereum [node]. So that's why — yeah, as I said, the underlying of Ethereum is more like a state machine. So on top of it, you can build your applications, decentralised applications, as we call them. Well, that was the — DeFi, decentralised finance. Okay, so all of those things they are building are smart contracts. And the smart contracts will be converted to [opcodes], which will be executed by the virtual machine of Ethereum. So that's why we call it EVM, Ethereum Virtual Machine. So I think nowadays they're trying to translate to RISC-V or something like that, but this I'm not sure. Have you learned something like this?

**Royce (07:35)**
Yeah, yeah.

**Wang (07:37)**
So basically something like that. So again, like I said, this is application layer, so this is the fundamental layer. So it's different from — because before, here, when we generate a transaction, in the transaction we'll have different fields, and every time when miners are executing the transactions, they will do the verification **directly against the transaction content**, rather than converting [it].

**Royce (07:49)**
And I don't think this is modifiable.

**Wang (07:53)**
Oh yeah, it's not. You have some freedom, but not that much. Yeah, so that's the difference.

**Royce (07:58)**
But again, it's a bit misleading.

**Wang (07:58)**
Yeah, it's kind of misleading, right? So you could say that — maybe, for example, we have smart contracts, maybe **it's more flexible**, let's use the word. Yeah. So Bitcoin is more **restricted**, because they **cannot** be modified.

---

### 11. The presentation as storytelling (08:16–08:34)

**Royce (08:16)**
Okay, so what else? So it's kind of — the presentation is more like a storytelling of the context, of the motivation, and what are the challenges, I guess.

**Wang (08:22)**
So yes. The most important thing is usually you should present the importance of the post-quantum [security].

**Royce (08:26)**
Yeah, so basically it's just [a] very [brief] highlight of the […]

---

### 12. The simplified swap diagram: concrete numbers, concrete drawing (08:34–10:37)

**Royce (08:34)**
I think you asked me to include a more simplified [version] of this diagram first, like regarding the atomic swap. Is it like this?

**Wang (08:53)**
Yeah, it's okay. But if you could, add some concrete numbers.

**Royce (08:57)**
Concrete numbers?

**Wang (08:58)**
[unclear] as you show on the deck.

**Wang (08:59)**
Yeah, yeah, yeah, yes, yes. You can see — okay, I have two chains. If I draw it, I will draw something like this. So for Alice, I will say, okay, this is the one blockchain, and I will say this is Bitcoin. And this is for Bob, and then I will show this is another chain. And you can see that here, we will do some atomic swap. And then we'll show, okay, we'll transfer one BTC — okay, she's very rich. Okay, then we'll do some atomic swap; maybe you should do [the] atomic swap here, and then we'll exchange some messages.

**Wang (09:22)**
And then, okay, in this way, maybe here you will see that — okay, how Alice will transfer one [BTC]. **It does not mean that Alice will transfer the coins directly to another chain.** It's more like that here we have the transaction — like here, we also have its own address, let's say [an] address, [unclear]. And that is why — okay, that is where [the] transfer [happens] — here, this is address [to] address, that is obviously okay. But the transaction is happening [on] the Bitcoin blockchain itself. But the atomic swap will [use] the confirmed transaction, as transaction one, to [signal] to another chain. And once [Bob] reads this, okay, it will do another transaction, transaction two. And in this way, Bob will transfer [to the] address [unclear], for example, [unclear] to [the] address that is on [the] user['s side].

**Wang (10:04)**
So basically, it will show something like this, right? So okay, just try to add [it] to [the] applications.

**Royce (10:09)**
Yeah.

**Wang (10:09)**
Okay, so here is fine, but for me, I would like to show more concrete things. Even [if] people don't understand the math — the mathematical details — they don't understand how the LAS signatures work in practice, but they can see **the protocol**, how the protocol works in practice, right? Protocol means some rules you define, what you define to show the working flow, right? Protocol means that — so you should Google what is the definition of protocol. Okay, for example, you know HTTPS, right? So it's more like HTTPS, how they execute. So we have different states, how each state works.

**Wang (10:34)**
But again, this is just my personal—

**Royce (10:34)**
Yeah, it makes sense.

**Wang (10:35)**
Yeah, just a suggestion. But then you can also make it more — I don't know, maybe more accessible for you or for readers.

---

### 13. Scope disclosure: the implementation swaps UTXO with UTXO (10:39–11:04)

**Royce (10:39)**
Yeah. And for my implementation, I guess I haven't implemented like Bitcoin with Ethereum, because they are on different environments. I think I implemented like UTXO-based coin with UTXO-based coin.

**Wang (10:47)**
That is fine. Then you just say — I [don't] know, for example, you change [ETH] to another chain, right? Maybe, I don't know, [unclear — two chain names]. Yeah, let's see, or something like that.

**Royce (10:53)**
And I think Ethereum-based is not only Ethereum — I think they have the other coins that are based on Ethereum.

**Wang (10:58)**
You mean different chains?

**Royce (10:59)**
The coin that can use Ethereum is not only Ethereum. I guess there are some coins that can use Ethereum.

---

### 14. Ether on other chains; which coins swap on the EVM (11:04–12:01)

**Wang (11:04)**
So we have two different [unclear]: Ethereum and [EVM-compatible chains]. So you also have different Layer 2 blockchains, such as BASE, such as BSC or BNB Chain — they also use the same structure, account-based blockchains, or even [EVM-compatible], so they are able to have this. But if you're talking about the token name, Ether, it's also being used on other blockchains as well, such as BASE. Basically, we also have BASE-ETH.

**Royce (11:27)**
So what kind of coins can I swap on EVM?

**Wang (11:31)**
So EVM — are you talking about the blockchains or the virtual machines?

**Royce (11:34)**
The blockchain.

**Wang (11:35)**
Okay. So it's different, because for Ethereum, Ethereum itself has a whole lot of different tokens. It has coins, ETH. And it also has USDC, maybe USDC — it's ERC-20 tokens. So they are governed by smart contracts, so they can directly swap with ETH; it's inside the Ethereum blockchain. But if you would like to swap the coins **outside** of Ethereum, with other blockchains, then in this way you should use [an] atomic swap. But in general, I think [the] atomic swap is more useful for Bitcoin or UTXO blockchains, because they don't have such kind of fancy smart contracts. Because for Ethereum, they have smart contracts, but they have other constructions — **bridges**. So bridges is what I think they are using to connect the different blockchains.

---

### 15. "Safer to just talk about UTXO" (12:01–12:31)

**Royce (12:01)**
So the basic motivation still makes sense if I implement [the] atomic swap on EVM? Or it's not as interesting as [an] atomic swap on UTXO?

**Wang (12:08)**
But EVM with what?

**Royce ⟨?⟩ (12:11)**
Yeah, yeah, yeah. Something like this, right? Ethereum with…

**Wang (12:13)**
But I think, yeah, **it would be safer to just talk about UTXO** — yeah, UTXO. But yeah, in your report you can do that, but you can also implement it in Ethereum as well, because sometimes you will also have some limitations like this.

**Royce (12:29)**
Oh, okay.

---

### 16. ETH↔USDC is the wrong example (12:32–13:14)

**Royce (12:32)**
So I think the atomic swap that I implement in Ethereum probably is not, like, ETH with USDC — does that make sense, if I give an example like that?

**Wang (12:37)**
No, I don't think so, because people will ask why you [need it]. I mean, USDC, right? USDC is actually [unclear] why you need, like, a […]

**Royce ⟨?⟩ (12:43)**
Yeah, yeah, USDC — say, like, BTC or something like that.

**Royce (12:46)**
Is it possible to make [an] atomic swap on BTC — BTC swapping with ETH — in practice?

**Wang (12:49)**
Yeah, yeah, it's doable.

**Royce (12:50)**
But they are on different environments.

**Wang (12:52)**
Yeah, the only thing, the only different thing, is the **verification**, right? The verification — in Bitcoin, then you just use Bitcoin clients to verify it.

**Royce (12:58)**
Oh yeah, I think I understand. So this is probably the — Alice wants to swap BTC with ETH, and when she got the ETH, I think the ETH will run verification on [the] EVM.

**Wang (13:12)**
Yeah.

**Royce (13:13)**
Oh, makes sense. Yeah, I think I got it.

---

### 17. Throughput and TPS (13:14–16:09)

> ⚠️ **Read §D action 1 before acting on this section.** Everything numeric here is improvised at a whiteboard, and the derivation being demonstrated is one the project has an explicit standing rule against.

**Wang (13:14)**
Okay, so what else?

**Royce (13:16)**
Oh, okay, I think on the proposal[s].

**Wang (13:29)**
On the proposal, you mentioned about the throughput and scalability. It's fine — I mean, that's just for the beginning. So of course, the proposal will be different, but [unclear].

**Royce (13:38)**
Okay, correct. Okay. So the throughput, it should be derived from — like, is it like one transaction, how many seconds? Or in one second, how many transactions can be made?

**Wang (13:43)**
Yeah, something like that.

**Wang (13:48)**
But here we don't consider that. I mean, if you calculate the computation cost, I think that would be fine. Yeah — you want to say, okay, for one transaction, right? We could see that, for example, even one transaction will take over the whole gas limit of one block. Then okay, you can only do one transaction per block, but then you basically calculate the TPS.

**Royce (14:02)**
Okay, so it can be derived from my number, right?

**Wang (14:07)**
You [could] also do something like that. For example, you claim that one **block** can only carry one transaction, including the signature. Now you can calculate the TPS. So you could say, okay, here is the number, right, for TPS [unclear] transaction[s]. Where is it? Where is it — just Google it. Yes, for example, I think it's 12 seconds per block, but inside one block it will be 200 transactions, like this, right? So yeah, you could do some simple calculation.

**Royce (14:34)**
Now, this is still based on ECDSA, or based on what?

**Wang (14:38)**
What do you mean by that?

**Royce (14:39)**
I think Bitcoin is based on the ECDSA. For Ethereum, I'm not sure — is it also ECDSA?

**Wang (14:47)**
Oh, yes, yes.

**Royce (14:48)**
Okay. So no one is using BLS, right?

**Wang (14:51)**
Yes, we are using BLS, but that's for other — other layer. So, other consensus layer. [As for] BLS: if you [aggregate] some signatures together, then we're going to use BLS, because it's easier for them to do the aggregation — signature aggregation.

**Royce (15:00)**
Signature aggregation, yeah.

**Wang (15:03)**
But that's not — you're not touching that part of it?

**Royce (15:04)**
Yeah, that's another type of exotic signatures. So for exotic signatures, as we mentioned at the very beginning, adaptor signature is just one of [the] exotic signatures. We also have multi-signatures, and also ring signatures, blind signatures, [threshold?] signatures, things like that.

**Royce (15:14)**
So if I want to compare TPS, do I compare [with] Ethereum?

**Wang (15:17)**
You compare with the current one's TPS. You know what I mean? So here, for example…

**Royce (15:22)**
But this one…

**Wang (15:23)**
So it's more like that: after we add the LAS transaction, then you could do that. Okay, we know that one is a block, right? One block, another block, and the interval is 12 seconds, if I remember correctly. Okay, this is block *i*, block *i*+1, and inside of each block there will be, let's say, 200 transactions. Okay, that's [how] we calculate TPS: 200 divided by 12. It's about — what's the number? I mean, here is — okay, let's use 300. Okay, 300 is something that is very equal to the number, like this, right? Okay, it's 12 — 20, okay, 25. Let's see.

**Wang (15:59)**
Okay, so but after your implementation, we say that okay, for one block, how many transactions will we have? One transaction. Okay, then now you can do the TPS like this, right? Okay, there's no […]

*Recording ends at 16:14.*

---

## D. Action items arising

| # | Action | Owner | Source |
|---|---|---|---|
| 1 | ⚠️⚠️ **THROUGHPUT/TPS — DO NOT IMPLEMENT WITHOUT ROYCE'S RULING. This request collides with a documented project rule.** Wang walks through deriving TPS as transactions-per-block ÷ block interval, on the premise that one LAS transaction consumes a whole block. `CLAUDE.md` forbids exactly this: EIP-7825 is a **per-transaction** cap, from which only *percentage of cap, whether one transaction fits, and per-transaction headroom* may be derived — **"never claims-per-block"** — and the block-gas-limit comparison is a claim this project has **already publicly retracted once**. Separately, the throughput deliverable is recorded as **closed by derivation** with two scaling dimensions (per-input validation work; ledger capacity) that **must not be combined into a tx/s figure** — "they constrain different things". Three ways to resolve, all Royce's call: (a) explain the retraction to Wang and keep the rule; (b) treat it as a supervisor override and record it as one, which reopens a retracted claim; (c) satisfy the *intent* — a practical sense of cost per transaction — using the per-transaction cap percentage already measured, without manufacturing a TPS number. **Nothing has been changed in the repo and `CLAUDE.md` is untouched.** | Royce | §17 |
| 2 | ✅ **"Nearly free" must name its base — ALREADY APPLIED, and the follow-up needs no new work.** The deck's slide 8 migration ladder (`video_deck.template.html`, comment at l. 1110) separates step 1 (classical → post-quantum basic, which costs bytes) from step 2 (post-quantum basic → post-quantum adaptor, which is what was measured), against the same base signature. Royce's suggestion of comparing adaptor-ECDSA with adaptor-LAS, which Wang endorsed, **already exists**: `bench_classical` measures the secp256k1-zkp ECDSA adaptor on the same machine, and it is one of the two required baselines. Verify the deck and report never let the two ladder rungs read as one number; do **not** open a new measurement under the freeze. | Royce | §6 |
| 3 | ⚠️ **Lead with quantum-computer background and the urgency** — why an expensive exotic post-quantum signature is worth paying for. Two motivations required, both stated: atomic swaps matter, *and* quantum security matters. Wang tested this and found no slide carrying it. **Already recorded in `CLAUDE.md`** as second-batch instruction (1) and applied across deck slides 2–4; this transcript is the primary source confirming it. ⚠️ **One half of §7 was NOT applied and had to be raised again in Meeting 12 —** the *institutional* evidence he asked for here ("many [governments?] or many big tech companies… replacing their **basic** signatures") never reached either artefact, which carried only an uncited "being adopted now". Done 2026-08-27 with citations; see Meeting 12 §D item 7. | Royce | §7, §9 |
| 4 | ⚠️ **Close on "is LAS good or not" plus recommendations for developers and the blockchain community.** **Already recorded and applied** — the deck's closing verdict slide carries one card per audience. Primary source confirmed. | Royce | §1 |
| 5 | ⚠️ **Show the transaction structure of Bitcoin (and Ethereum), field by field** — "which field should I add", screenshots or a concrete picture rather than formulas. **Already recorded and applied** as the "What actually goes on chain" slide. Primary source confirmed. | Royce | §4 |
| 6 | ⚠️ **The swap must be followable by someone who does not read the maths** — the protocol as states and messages, not the mathematics. **Already recorded and applied.** Outstanding sub-item from this pass: Wang additionally wants **concrete numbers on the simplified diagram**, and he laboured one point worth checking in the figure — **the coins do not move between chains**; each transaction settles on its own chain and the swap is the linkage. Confirm the intro figure cannot be misread as a cross-chain coin transfer. | Royce | §12 |
| 7 | ⚠️ **Real logos and real prices, because it is a scientific report** — "not just using the random numbers", "even for toy examples, try to be more precise". **Already recorded and applied**, with the standing caveat that prices are cited claims carrying a source and a UTC instant, and must be re-read before recording on another day. Primary source confirmed. | Royce | §3 |
| 8 | **Reword the EVM/Bitcoin contrast**: smart contracts are **"more flexible"**; Bitcoin is **"more restricted, because [the fields] cannot be modified"**. Wang judged the current framing "a bit misleading". This sits alongside — and does not conflict with — the standing rule that the two venues are never framed as a timeline; both say the difference is structural. Check the deck's wording against Wang's chosen word. | Royce | §10 |
| 9 | **Scope wording: say the swap is UTXO-with-UTXO.** Royce disclosed that Bitcoin-with-Ethereum is not implemented; Wang accepted it and ruled **"it would be safer to just talk about UTXO"**, while allowing the Ethereum work to be reported. Also: **do not use ETH↔USDC as an example** — ERC-20 tokens swap inside Ethereum via smart contracts and need no atomic swap; the motivating example is going outside Ethereum, and Ethereum uses bridges. Check that no artefact implies a live BTC↔ETH swap was run. | Royce | §13, §15, §16 |
| 10 | ❓ **Slide count discrepancy, unresolved.** Wang said "you have 10 slides… 10 pages in total" (§2); the template has 13 and the 13-slide count is deliberate. Royce did not correct him and it was not raised again. Establish whether a different cut was on screen before treating this as either a ruling or an error. | Royce | §2 |

**Nothing in this meeting authorises a new experiment.** Items 3–9 are presentation and wording work; item 2 is already built; item 1 is a conflict to resolve, not a task to start. The Meeting-10 feature freeze is not mentioned in this recording and is not lifted by it.

> ⚠️ **`CLAUDE.md` has NOT been updated from this transcript.** The standing duty is to record supervisor rulings in the session they happen, but this file's speaker labels are inferences from a single undiarised ASR pass, and the one genuinely new item (action 1) would **overturn a documented rule and reopen a retracted claim** on that basis. Confirm the attribution at 13:48–16:03 by re-listening, and decide action 1, before anything here is promoted to a ruling.
