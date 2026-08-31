# Cleaned Transcript — Meeting 12 with Zhipeng Wang (full video rehearsal + introduction review)

**Date:** 2026-08-27 — **inferred, not confirmed by a calendar record.** `meeting12_original_transcript.md` was written to disk at 11:04 on 2026-08-27 and the recording runs ≈31 minutes. The date is corroborated from inside the meeting: Royce gives the submission deadline as "the 4th of September… on Friday" (2026-09-04 **is** a Friday), Wang says he will "be back next Monday" and offers a further meeting "on Thursday next week", and Royce commits to sending everything "by Friday, 5pm" — a Thursday-morning meeting is the only reading that makes all four consistent.

**Source:** `meeting12_original_transcript.md` — SHA-256 `a40cac949ac00c7267211ce2435bd39c8b0e95d4b484a32f7bf29c0cef5a01ef` — a **Microsoft Teams live-transcript export**, pasted whole (the repeated "Royce Steven 0 minutes 7 seconds" lines and the leading "Transcript. Use arrow keys to navigate…" block are the export's own UI furniture, not speech). Last timestamped entry 30:55.

> ✅ **THIS SOURCE IS DIARISED, AND THE DIARISATION IS SOUND.** Unlike Meeting 11 (one undiarised Whisper pass) and unlike Meeting 10's Teams export (which stamped *every* line, Wang's included, as "Royce Steven"), this export alternates `RS` / `ZW` speaker blocks and the labels agree with content at every turn that can be checked independently — the eleven-minute delivery is Royce, the instruction-giving and the Overleaf screen-share are Wang. **Speaker attribution in this file is taken from the source, not inferred**, so §D items may be treated as attributable supervisor rulings.
>
> The remaining defect is **word accuracy on domain vocabulary**, which is poor throughout (§A). Turns marked ⟨?⟩ are places where the *words*, not the speaker, could not be settled.

**Type:** meaning-preserving cleaned transcript, not a verbatim or legal record. Unintelligible fragments are marked `[unclear]`; reconstructed words are in `[square brackets]`. Timestamps are the export's.

**Main topics:** Royce delivers the **whole video presentation end to end** (00:31–11:12) for the first time in front of Wang — the Meeting-10 mock was a partial run. Wang's verdict: **content accepted, length rejected.** "The content is okay, but my concern is that it's a bit too long… you should compress it to less than 8 minutes." Delivery ran ≈**10 min 40 s** against the rubric's 6–8. He confirms the earlier rounds are discharged — "you have addressed most of the questions I mentioned before, so I don't have further comments so far" — and asks for the recording by the end of the week so he can comment on it. The second half turns to the **report's introductory material**, at Royce's request, with Wang sharing the PDF on screen: **Figure 1.1(a) is too far from blockchain** (it argues from RSA-2048, which neither chain uses) and should instead cite work on when quantum computers make *current chain signatures* insecure; the **adaptor-signature motivation is too thin** — "one or three sentences", "people were just talking about something very abstract" — and, since the new report format has no Background section, that background belongs *in* the introduction; and the **title must change**: drop "exotic signature schemes", name the adaptor signature, and do **not** say "lattice-based" because "people don't know what is lattice". Structure, objectives, contributions and 28 citations are accepted as they stand. Detailed comments follow on Overleaf as highlight-plus-comment, early next week.

---

## A. Key contextual corrections

Teams' ASR is accurate on plain conversational English and unreliable on every domain term in this project. Every substitution below is applied silently in §C. Royce's delivery was additionally checked line by line against the deck's own `data-notes` SPOKEN scripts in `report/slides/video_deck.template.html`, so reconstructions there follow the written script rather than a guess.

| ASR phrase | Corrected meaning |
|---|---|
| latest / latest based / "latest arithmetic" / "latest signature" / "lab signatures" | **lattice / lattice-based / lattice arithmetic / the lattice signature / LAS signatures** — the single most frequent garble in this transcript |
| "the last paper" (04:46) · "the last construction" (20:51, 24:50) | **the LAS paper** · **the LAS construction** |
| adapter / adaptive signature · "adapted player" | **adaptor signature** · **adaptor layer** |
| "Resign" (03:00, 03:04, 04:14) | **PreSign** |
| "present and verified do have to change" (09:18) | **PreSign and PreVerify do have to change** |
| "the B signatures has the commitment W" (04:02) | **the base signature hashes the commitment $w$** |
| "the statement why" / "the statement wise" | **the statement $Y$** |
| "from Jane Tu" (05:19) | **from chain two** |
| "UTSO" (04:29) | **UTXO** |
| "the Lithium 3" (08:57) · "the lithium trees" (10:51) | **Dilithium-3** |
| "FIPS 20.4" (09:18) | **FIPS 204** |
| "Consensus Road" (07:53) · "the neural is really checking" (08:32) | **a consensus rule** · **the new rule** is really checking |
| "Will a real change take it?" (08:47) · "will it change take it" (02:44) | **Will a real chain take it?** — "chain", not "change", both times |
| "one fan units are deployed and the other needs a…" (07:47) | **one venue needs a deploy, and the other needs a [consensus rule]** |
| "and a span carrying no elliptic curve signature" (08:17) | **a spend carrying no elliptic-curve signature** |
| "the patch node" (08:32) | **the patched node** |
| "a real client might hold claim at 97.8%" (08:57) | **a real client mined a whole claim at 97.8%** |
| "to optimizations I hope will help" (09:18) · "Where run run run?" (09:38) | **two optimisations I hoped would help** · **[they] were run** — the following line restates it cleanly |
| "it reyes on" (00:48) · "is recorded going to be permanent" (01:07) | **it rests on** · **its record is permanent** |
| "The same idea under Pinsk. Payment channel as well." (01:47) | **The same idea underpins payment channels as well.** |
| "what links the truth is a secret" (01:48) | **what links the two is a secret** |
| "So subtract the complete incomplete signature from the complete one" (03:22) | **subtract the incomplete signature from the complete one** |
| "deleting once primitive we use" (03:47) | **Dilithium's own primitives, reused** |
| "the proof and Alice pre-signature boot check out" (04:46) | **[Bob commits nothing until] the proof and Alice's pre-signature both check out** |
| "post content migration" / "post-content version" / "poster quantum" / "post comes quickly" | **post-quantum** (migration / version / security) |
| "for the first finger, our first finger, finger 1.1" (18:23) · "cite some fingers" (19:18) | **Figure 1.1** · **cite some figures** |
| "RSR Bitcoins" (18:49) | **RSA. But Bitcoin[s]** — Wang's point is that Bitcoin and Ethereum do not use RSA |
| "replace some more concurrent things" (19:10) | **more concrete things** |
| "the security of counter blockchains" (19:35) | **the security of current blockchains** |
| "maybe just the current signature schemes will be secure" (19:54) | **the current signature schemes will [no longer] be secure** — the negation is dropped by the ASR; the whole passage is about urgency, and "so here you can see the motivation, the urgency of the replacement" two lines later fixes the sense |
| "The subject error" (21:26) | **the subject area** |
| "here it's regular objectives" (24:32) | **regarding objectives** |
| "let me first array it in more detail" (30:44) | **let me first read it in more detail** |
| "by using the view, or is it view" (28:32) | **using the [Overleaf] review [mode]** — he is choosing between a review comment and a highlight |
| "we got some automics web" (12:53) | **atomic swap** |
| "Is it official enough for this?" (12:14) · "Is it feasible enough?" (12:59) | **Is it visual enough?** ⟨?⟩ — Royce's own next turn ("does it need more visual, or a logo, or more diagrams") fixes the sense; the exact words are not recoverable |
| "Uh, Shu I?" (00:15) | **Shall I…?** |
| "Is it showing not?" (12:31) | **Is it showing or not?** |
| "Can I see backup? Can I see my screen?" (18:38) | **Can you see my screen?** |
| "Pause the content" (18:09) · "Open the dass" (18:12) | **[post-quantum]** · **[unclear]** — Wang muttering while scrolling the PDF |
| "subject here of ladies, ladies signature, DDS, and whatever signatures" (20:36) | **[lattice, lattice signatures, ECDSA] and whatever signatures** ⟨?⟩ — he is reading section headings off the screen; treat the list as indicative only |
| "This is for Shu." (21:13) · "I feel sure it would be different." (26:55) · "it can be straight edges, I intend to see the rest" (25:30) · "Patient methods." (25:51) · "We got a message that we haven't, so method, so…" (26:41) · "That, oh, very late, but…" (29:05) · "I like it." (28:55) | **[unclear]** — seven fragments spoken while reading or scrolling. None is load-bearing; do not reconstruct any of them into a ruling |
| "you are not writing it in a very empty safe way" (24:16) | **[unclear]** — the sentence completes as "just try to make it more… easy to read", which is the usable instruction |
| "Maybe you should change your bit regarding title, regarding for the subsections" (27:13) | **change a bit regarding the titles of the subsections** |

> ⚠️ **The figures Royce speaks in §C.2 are not evidence, and must never be quoted from here.** 97.8 %, 4.6×, 72×, 99 % are the deck's macros (`{{gasOptCapPct}}`, `{{clOvPreSignX}}`, `{{clRatioSig}}`, `{{zPctTarget}}`) as they rendered on the day, read aloud through a lossy ASR. The authority is the regenerated report macros and the evidence run behind them — a number recovered from a transcript of speech is exactly the "retyped number" the project rules forbid.

---

## B. Meeting summary

**Format.** Two halves. First a **complete, uninterrupted delivery of the video presentation** — Royce shares the deck and speaks all thirteen slides, 00:31 to 11:12, with no questions from Wang until the end. Then a working review, with Wang sharing his own screen and scrolling the compiled report PDF, focused on the **introduction**, at Royce's request. Wang had another meeting in ten minutes and said so at 25:12, which is why the second half is deliberately quick and ends with a promise of written comments.

**Verdict on the video: content passes, length does not.** "I think the content is okay, but my concern is that it's a bit too long… you should compress it to less than 8 minutes, right? I maybe I haven't counted the time, but it's more than 10 minutes so far." The remedy he offers is either pace or content: "you should be faster, or maybe you should remove some details." He adds that the earlier rounds of feedback are discharged — "you have addressed most of the questions I mentioned before, so I don't have further comments so far" — and asks for the recording by the end of the week, so he can watch it and comment alongside the report.

**Visualisation is not free.** Asked whether the deck needs more visuals, Wang's answer is a trade, not an addition: "if you would like to add more visualisations, maybe you should remove some content." His reason is the non-specialist examiner, the same audience model as Meeting 10: "I know the background, then I can easily capture the most important ideas in this slide, but I assume that some people… are not doing something in this field. It will be very challenging for them to understand which one is the most important. For example, here there are three numbers, right? Each year — which one is the most important one?" That is the quantum-estimates slide, and it is the same objection he then raises against Figure 1.1(a) in the report.

**Slide count: 13, acknowledged.** In passing — "imagine how many slides we have in total, 13, right?" — which settles Meeting 11's unresolved "you have 10 slides" discrepancy in favour of the deck as built.

**The title must change.** Royce asked whether "exotic signature schemes" should stay. Wang first confirmed the freedom exists: "now you have the freedom to update your title, right?" Then: be more precise; drop "exotic signature schemes" and refer to the adaptor signature. He explicitly ruled out "lattice-based" as the qualifier — "maybe lattice-based is too specific, because people don't know what is lattice" — and proposed **"post-quantum secure adaptor signature"**. Royce read back "…secure adaptor signature scheme in blockchains" and Wang confirmed, "Yes, yes."

**Introductory material — Royce asked, because it is 20 %.** Royce quoted the rubric's introductory-material criteria almost verbatim and said his introduction is four pages (13–17), and that he was "a bit concerned that I'm not doing good enough on the introduction". Wang gave high-level comments on the spot and deferred the rest to Overleaf.

**Figure 1.1(a): argue from the chains, not from RSA.** Wang's first and most concrete report comment. "For the first diagram, Figure 1.1, part (a)… I think you could make it much closer to Bitcoin, or to blockchain itself. Here you are talking about RSA — but Bitcoin and also Ethereum, they are not using RSA. They're using other signatures. If you replace [it with] more concrete things, then it would be more relevant." What he wants instead: cite the published work on **when quantum computers become realistic**, set against **the security of current blockchains**, so the reader can see in how many years today's signature schemes stop being secure — "so here you can see the motivation, the urgency of the replacement or the improvement of existing signatures." His summary judgement on the figure as it stands: "it's not that straightforward, not that direct, to convey the things you would like to say."

**Extend the adaptor-signature motivation.** The second report comment, and the one he pressed hardest. He looked for where the importance and the applications of adaptor signatures are argued and found "one or three sentences" — "I think you would be better to extend it, otherwise people were just talking about something very abstract. We need some concrete motivation, the concrete examples." He framed it as the **second of two required motivations**, the first being post-quantum security, which he judged already done well: "you have already done a good job regarding the post-quantum security motivation, but you also need to introduce why, for this project, we will focus on adaptor signature." And within the exotic family: "even for the exotic signatures, there are still a lot of types… so why did you choose adaptor signature? You should give the importance, you should give the motivation here."

**No Background section means the background goes in the introduction.** Royce read out the rubric's rule — no separate Background section, the Introduction presents the subject area and a concise literature review, depth over breadth, an extensive review not required. Wang accepted the constraint and drew the consequence: "then you should extend a bit for the subject area. If you don't have a specific background section, then you should introduce the important background in the introduction section."

**Post-quantum migration is not only a blockchain story.** Repeating the Meeting-11 motivation instruction, now for the report as well as the deck: "the post-quantum migration is not only for blockchains. We have already observed some big companies… they are currently moving to post-quantum. Nowadays people say that post-quantum is very important, so many companies are replacing their existing signatures with the post-quantum version, so we should also do that for this."

**Structure, objectives, citations: accepted.** Wang walked the contents and found the shape sound — "you have at least five objectives, not bad", contributions, structure, "it's good that you have the critical reflection", "the structure looks okay for me". He counted the bibliography — "28, okay, not bad" — and asked what separates Chapter 3 from Chapter 4; Royce's answer (Chapter 4 is evaluation, achievement of objectives, implementation challenges and limitations, answering the rubric's separate project-achievement weighting) satisfied him. The only structural reservation is minor: "maybe you should change a bit regarding the titles of the subsections."

**Mechanics and dates.** Wang will read the report properly and comment on Overleaf, as **highlights with comments attached** (Royce's preference, when offered the choice). He had planned to send comments by the end of this week but will now aim for **early next week**, to leave time for revisions. Royce will polish report, slides and video and send all three by **Friday 5 pm**; Wang reads them **Sunday or Monday**, is back **Monday**, and offers an **optional further meeting on Thursday** before the **Friday 4 September** deadline.

---

## C. Cleaned transcript

### 1. Screen share and starting (00:07–00:31)

**Royce (00:07)**
Is my screen already visible?

**Wang (00:10)**
Yes, I can see it.

**Royce (00:15)**
Shall I… try the video first, after all?

**Wang (00:24)**
Yeah, up to you.

**Wang (00:27)**
Yeah, okay, we can start with this one. Maybe it's easier.

**Royce (00:31)**
Okay, start.

---

### 2. The video presentation, delivered in full (00:31–11:12)

> This is a single uninterrupted delivery of the thirteen-slide deck. Section breaks below are mine, at slide boundaries, and the timestamps are the export's. Reconstruction here is anchored to `report/slides/video_deck.template.html`'s SPOKEN notes — where the ASR garbled a phrase whose written form exists in the deck, the deck's wording is restored in `[brackets]` and the garble is recorded in §A.
>
> ⚠️ **Delivered length: 00:31 → ≈11:10, i.e. ≈10 minutes 40 seconds** against a 6–8 minute rubric limit. That measured figure — not any derived planning budget — is what Wang responded to. See §D action 1.

#### 2.1 Opening (00:31–00:48)

**Royce (00:31)**
Hello, my name is Royce Steven. This project asks whether the signature behind cross-chain swaps can survive quantum computers, and what it costs to find out.

#### 2.2 Why now — the quantum clock (00:48–01:32)

**Royce (00:48)**
Bitcoin and Ethereum both use an elliptic-curve signature, and a large enough quantum computer could break the maths it rests on. The question has always been how large. These are published estimates. For the same RSA-2048 target the figure has fallen sharply, and the newest one is [for] the curve these two chains actually use. Nobody has built this hardware — what falls is the cost. But the chain cannot wait: its record is [permanent], and the fix is a change the whole network has to agree on.

#### 2.3 The application — an atomic swap (01:32–02:11)

**Royce (01:32)**
Two people on two different blockchains want to trade — no exchange, nobody to trust. An atomic swap makes that safe: either both transfers happen or neither does. [Watch] the coins, though — they never leave their own chain. Each payment settles at home, and what links the two is a secret. The same idea underpins payment channels as well. What ties the two legs together is an adaptor signature — and all of this runs today on elliptic-curve signatures. So why build a new one?

#### 2.4 The stack, and where the gap is (02:11–02:56)

**Royce (02:11)**
Because a blockchain does not run on one signature, it runs on a stack. At the bottom, the elliptic-curve signature[s in use] today. Step one is already under way: migration to the standardised post-quantum basic signature. But look where the route stops. A basic signature only authenticates a message, and everything a [chain] does with signatures — swaps, payment channels — lives in the layer above, where the coverage is uneven. Multi-signatures are being built; the adaptor case is this project. So the question is: can we build one, what does it cost, and will a chain take it?

#### 2.5 The four functions (02:56–03:47)

**Royce (02:56)**
Four functions.

**Royce (03:00)**
There are four functions [in] this lattice-based adaptor signature. **PreSign** produces a signature that is deliberately incomplete. It checks out against a public statement $Y$, but the network will not accept it — you cannot spend it. Whoever knows the matching secret can **Adapt** it into an ordinary signature, and that one goes on chain looking like a normal payment. So — here's the trick — subtract the incomplete signature from the complete one that just appeared, and the secret will fall out. Claiming one leg unlocks the other.

#### 2.6 Methodology — reuse, the one change, the Rust twin (03:47–04:29)

**Royce (03:47)**
So how do you turn a basic signature into an adaptor one?

**Royce (03:51)**
[This is] the methodology. Not by inventing lattice arithmetic — at the bottom, [Dilithium's] own primitives, reused, with zero upstream source functions modified.

**Royce (04:02)**
On the right, the one change that matters: the base signature [hashes] the commitment $w$; PreSign has $w$ plus the statement $Y$. That substitution is what makes a signature adaptable. Then I built it again in Rust, a second independent implementation to check the first — and the two agree byte by byte.

#### 2.7 Demo A — walking the swap (04:29–05:28)

**Royce (04:29)**
Let me walk it. This swaps a UTXO coin with a UTXO coin, across two UTXO ledgers — the setting the LAS paper assumes. [First,] the abort gate: [Bob] commits nothing until the proof and Alice's pre-signature both check out.

**Royce (04:53)**
[Then] the tripwire: Bob tries to spend the pre-signature he holds, and ordinary verification will refuse it. And then Alice, who knows the witness, completes Bob's pre-signature and publishes it.

**Royce (05:10)**
And then the leak. Bob needs nothing further from Alice. Here is that signature, from chain two — and the witness falls out, and he claims the other coin.

#### 2.8 Result 1 — the cost, and what it is measured against (05:28–06:27)

**Royce (05:28)**
And now, the cost. Everything hangs on what it is measured against. Two steps. Step one, classical to post-quantum, is the expensive one — and the step organisations are already taking.

**Royce (05:43)**
And step two is what I measured: post-quantum basic to post-quantum adaptor, run back to back in the same session to limit drift. Measuring just the cryptographic operations, the adaptor overhead stays in a single digit.

**Royce (06:03)**
By comparison, the classical adaptor's [PreSign] is 4.6 times its own Sign, and carries an extra proof — [so it is] less overhead relative to its own base. In absolute time the classical one is still ahead, and that is step one.

#### 2.9 Result 2 — the price is bytes (06:27–07:15)

**Royce (06:27)**
The second result is [that] the price of post-quantum here is mostly not computation but communication. Against a classical ECDSA adaptor, the LAS signature is about 72 times [larger] — and yet it is the bytes that hurt here, not the time.

**Royce (06:46)**
And the signature is almost all one object: the response is 99 % of the signature. And a swap adds one more public object, the statement $Y$. So to make this cheaper, we do not need [to] optimise [the] algorithm. We need to optimise these two objects — as our future work.

#### 2.10 What actually goes on chain (07:15–07:57)

**Royce (07:15)**
So what does it look like on chain? On Bitcoin, an ordinary transaction — the same fields as any payment, the signature travelling in the witness. No swap script, no hash lock, no shared hash and no adaptor-specific marker. One slot simply gets bigger.

**Royce (07:34)**
Ethereum is not further ahead here. It is different in kind. Its signature field is still elliptic-curve, and [the] lattice signature rides in the input data, as an argument to a contract — which is why one venue [needs a] deploy, and the other needs a consensus rule.

#### 2.11 Demo B — the patched node (07:57–08:47)

**Royce (07:57)**
The second demonstration. A settlement fits Bitcoin's size limit comfortably, but a normal node refuses to pass it on — and yet, put in a block, the same stock software accepts the block. So fitting the size limit is not enough: [there is] the relay policy [as well].

**Royce (08:17)**
[I then added lattice verification experimentally,] and a spend carrying no elliptic-curve signature settled a whole two-leg swap across [two] chains.

**Royce (08:32)**
The negative cases I tried are rejected only by the patched node, and that difference is the evidence that the new rule is really checking the LAS signature.

#### 2.12 Settled by measurement (08:47–09:58)

**Royce (08:47)**
Will a real chain take it? Three things, settled by measurement. Does the full verification fit in one transaction? At Dilithium-3, a real client mined a whole claim, at 97.8 % of the per-transaction cap — one measured instance.

**Royce (09:07)**
It's close to the edge. Does the adaptor functionally need a simplified base?

**Royce (09:18)**
I assumed so, and [the] experiment overturned that assumption. PreSign and PreVerify do have to change — but the unmodified FIPS 204 verifier accepts the adapted signature without any modification. And two optimisations I hoped would help…

**Royce (09:38)**
[…were run.]

**Royce (09:42)**
Yeah — and these are the optimisations that I hoped would help,

**Royce (09:50)**
which is compressing the signature and [the] statement $Y$.

#### 2.13 Verdict and implications (09:58–11:12)

**Royce (09:58)**
Back to the three questions. Can we build one? It is yes, in two languages — as [I] implemented [it]. And what does it cost? Single digits in adaptor computation, but 72 times the bytes. And will a chain take it? On Ethereum, through a contract; and on Bitcoin, only with a new rule.

**Royce (10:32)**
For the adaptor layer, it's yes — but as something to deploy, not at the moment. And what stops the deployment is not the adaptor layer. So, three recommendations. For Bitcoin: analyse the consensus rule before anyone [ships] it. For Ethereum: drive the verification cost down — it fits today with almost no margin, at Dilithium-3,

**Royce (10:59)**
the standard security parameter. And for the protocol designers: budget the proof and the statement first — those should be optimised, not the adaptor. That's all from me. Thank you.

---

### 3. Verdict: content okay, too long (11:12–11:39)

**Wang (11:12)**
Okay, good, good. I think the content is okay, but my concern is that it's a bit too long. I mean — yeah, you should compress it to less than 8 minutes, right? I maybe I haven't counted the time, but it's more than 10 minutes so far.

**Royce (11:20)**
Too long. Yeah. Oops.

**Wang (11:32)**
Okay, yeah — **you should be faster, or maybe you should remove some details.**

**Royce (11:33)**
Okay. Faster. [Remove] some details, okay.

---

### 4. Send the recording; comments next week (11:39–12:08)

**Wang (11:39)**
Okay, good. And besides that, I think — yeah, you have addressed most of the questions I mentioned before, so I don't have further comments so far. But maybe you could record a video and also share [it] with me by the deadline. So, I will be back next Monday — yeah, I can have…

**Wang (12:01)**
I can give you some comments next Monday, yeah, if you want.

**Royce (12:04)**
Okay. Oh, okay.

**Wang (12:08)**
Yeah. Do you have any questions regarding the slides, or regarding the things you presented?

---

### 5. Visuals versus density; the slide count (12:08–14:47)

**Royce (12:14)**
Does it need more for the… is it [visual] enough for this? ⟨?⟩ Oh, sorry, I think…

**Wang (12:19)**
No, no, no, I cannot see it now. Please start sharing.

**Royce (12:31)**
Is it showing or not?

**Wang (12:32)**
Yes, I can see.

**Royce (12:39)**
Does it need more visuals — or a logo, or more diagrams?

**Wang (12:52)**
Which one?

**Wang (12:53)**
I think now it's better, right? We can see, okay, we've got some atomic swap…

**Royce (12:59)**
Is it [visual] enough? ⟨?⟩

**Wang (13:03)**
Okay, let me have a look. Yeah, it's okay, it's okay. Maybe — I'm not sure, have you already mentioned that? Maybe you could also say that, okay, **the post-quantum migration is not only for blockchains.** We have already observed some big companies —

**Wang (13:24)**
they are currently moving to post-quantum.

**Wang (13:29)**
Have you mentioned this? Yeah — I mean, again, this is one of the motivations, right? Because nowadays people say that post-quantum is very important, so many companies are replacing their existing signatures with the post-quantum version. So we should also do that for this.

**Wang (13:48)**
But again, I think it's better [now]. I'm not sure if you would like to add more visualisation. If you add more, maybe it will make each slide very — I don't know.

**Wang (14:04)**
Too much content in one slide. Yeah. Imagine how many slides we have in total — **13**, right? So for example, if we want to — I mean, of course, I know the background, then I can easily capture the main, or the most important, ideas in this slide. But I assume that some people, they are out of…

**Royce (14:06)**
Too much, too much [content], I guess.

**Wang (14:23)**
…they're not doing something in this field. Maybe it will be very challenging for them to understand: okay, **which one is the most important?** For example, here there are three numbers, right? Each year — okay, which one is the most important one?

**Royce (14:39)**
Okay, for the…

**Wang (14:40)**
Okay, I got it, I got it. I think **if you would like to add more visualisations, maybe you should remove some content.**

---

### 6. The title must change (14:47–15:47)

**Royce (14:47)**
Okay — so for the title. Is it… should I change it, because it still says "exotic signature schemes"?

**Wang (14:57)**
Yeah. By the way, for this — I wanted to confirm: now you have the freedom to update your title, right?

**Royce (15:06)**
So, more like "implementing lattice-based adaptor…"

**Wang (15:12)**
Yeah, yeah, I think **you can be more precise.**

**Wang (15:17)**
Maybe "lattice-based" is too — I don't know, **too specific, because people don't know what is lattice.** Maybe you could say **post-quantum secure adaptor signature**.

**Royce (15:21)**
Too specific. …adaptor signature.

**Wang (15:31)**
Yeah — **just remove the "exotic signature schemes" [and] refer to the adaptor signature.**

**Royce (15:35)**
"Exotic"… [so] "…secure adaptor signature scheme in blockchains".

**Wang (15:44)**
**Yes, yes.**

---

### 7. Introductory material: Royce's ask (15:47–16:45)

**Royce (15:47)**
And… I think I also wanted [to ask] — could you give me some feedback on my introductory material?

**Wang (16:01)**
The what?

**Royce (16:01)**
Because it's like 20 % of the weighting of the mark. It asks that I should clearly describe the project setting, scoping the subject area with proper presentation and figures, and stating the objectives. And: does the work effectively establish the context, why this project matters, and clearly explain the subject area, what this project is about, with proper citations or figures? And are the project objectives clearly stated, coherent and appropriate?

**Wang (16:31)**
So which one are you referring to? Are you talking about the slides, or are you talking about the report?

**Royce (16:37)**
Both, I guess — and I think more of the report, because the report is like 85 %, I believe, of the total.

**Wang (16:45)**
Yeah. I think, to address — you are talking about the importance of this project, right? What application?

---

### 8. Figure 1.1(a): argue from the chains, not from RSA (16:52–20:36)

**Royce (16:52)**
Um, yes — but I think I haven't asked, in detail, [for] the feedback regarding this [part].

**Wang (17:05)**
The what?

**Royce (17:06)**
I believe it's the introduction chapter.

**Wang (17:14)**
Okay. So — I was planning to give you some comments by the end of this week, but I can give you some now, because I would add more detailed comments on the Overleaf directly. But I can give you some high-level comments so far.

**Wang (17:44)**
…signatures. So now I can see that you have already mentioned that the post-quantum security is important.

**Wang (17:55)**
And you also need to — okay, let me have a look. Maybe the diagram could be… let me have a look. Basic post-quantum signatures. Okay. [unclear]

**Wang (18:23)**
To be honest, for the first diagram — Figure 1.1, right? — the first part, part (a): let me share my screen with you, maybe.

**Wang (18:38)**
Can you see my screen?

**Royce (18:40)**
Yes, I guess.

**Wang (18:41)**
So here — I think **you could make it much closer to Bitcoin, or to blockchain itself**, right? Here you are talking about **RSA**, right? But Bitcoin — and nowadays Bitcoin and also Ethereum — **they are not using RSA**, right? They're using other signatures.

**Wang (19:10)**
Maybe if you replace [it with] some more **concrete** things like that, then it would be **more relevant**, right?

**Wang (19:18)**
Here [it] is more like — we have the high-level picture like this. Maybe you could also say, ideally, **you could cite some figures**. I believe that there are some reports — they have done some research regarding the

**Wang (19:35)**
**development of quantum computers, and when it will become realistic.** So you could also consider **the security of current blockchains**, and also the timing regarding the development of the quantum computers. And then we will see, maybe,

**Wang (19:54)**
in how many years — okay, [in how many years] the current signature schemes will [no longer] be secure. So here you can see the motivation, **the urgency of the replacement or the improvement of existing signatures**.

**Wang (20:12)**
So here it is — okay, because after I read the things you have written, [it] is fine. But the first thing I will say is regarding Figure 1.1. So, going [back] to Figure 1.1 — maybe, after I say it…

**Wang (20:26)**
…**it's not that straightforward** — let's say, **not that direct** — to convey the things you would like to [say].

**Royce (20:32)**
Yeah. I see.

---

### 9. Extend the adaptor-signature motivation (20:36–24:47)

**Wang (20:36)**
Okay, this is regarding Figure 1.1. And [the] subject [area] here: [lattice, lattice signatures, ECDSA] ⟨?⟩ and whatever signatures.

**Wang (20:51)**
And here you also need to — let me check. Why did you mention the LAS construction [here]?

**Royce (21:00)**
[The] thing is in [the] methodology.

**Wang (21:01)**
What did you — when did you say, where do you say **the importance of the adaptor signature, the application of the adaptor signature**?

**Royce (21:13)**
I think I have not. [unclear]

**Wang (21:20)**
I can see that here, right? You have mentioned the…

**Wang (21:26)**
The subject area, of course, [is] important. But here I want to say — okay, anyway: you mentioned that, okay, adaptor signature, [and] then…

**Wang (21:36)**
**Why is it important for blockchains?**

**Royce (21:38)**
Why is [it] important?

**Wang (21:40)**
**You have one or three sentences here, but I think you would be better to extend it.** Otherwise, people were just talking about something very abstract.

**Wang (21:51)**
**We need some concrete motivation, the concrete examples.**

**Wang (21:55)**
Okay — well, this one, you have to [say] more things here.

**Royce (22:00)**
So, it's how many pages? It's page 17.

**Wang (22:05)**
Yeah, there are so many pages.

**Royce (22:07)**
Only four pages of them — introduction [is] only page 13 to page 17.

**Wang (22:14)**
Sorry, I cannot hear you. Can [you] say it again?

**Royce (22:17)**
Hello, can you hear me? Yeah — the introduction is only page 13 to page 17, so it's only four pages. And when I see the rubric, it's 20 % of the report components, so I'm a bit

**Royce (22:35)**
concerned that I'm not doing good enough on the introduction.

**Wang (22:44)**
Let me have a look. But you don't have the background, right? Do you have the background section?

**Royce (22:49)**
Because it says that in the new report format, there is no separate Background section. Instead, the Introduction should present the subject area clearly and include a concise literature review.

**Wang (22:59)**
Yeah — **then you should extend a bit for the subject area**, right? If you don't have a specific background section, **then you should introduce the important background in the introduction section.**

**Royce (23:13)**
And the focus should be on depth rather than breadth, highlighting key works necessary to understand the problem and justify your approach. An extensive review is not required.

**Wang (23:28)**
But for me, I think **you should at least give some introduction regarding applications of [the] adaptor signature.**

**Royce (23:33)**
Adaptor signature — yeah, why it's important.

**Wang (23:35)**
I mean, again, **you should always follow the two important motivations**, right? You have already done a good job regarding the post-quantum security motivation. But you also need to introduce **why, for this project, we will focus on adaptor signature**, right? Yeah — as I said before, right, there are basic signatures.

**Royce (23:50)**
And the [exotic] signatures, yeah.

**Wang (23:55)**
And you have already mentioned that there are exotic signatures. Okay — for exotic signatures, people have already implemented their post-quantum implementation in practice. But even for the exotic signatures, **there are still a lot of types of exotic signatures**, right? So **why did you choose adaptor signature? You should give the importance, you should give the**

**Wang (24:16)**
**motivation here**, right? So here I cannot say it very specifically — [unclear]. So yeah, just try to make it more

**Wang (24:32)**
easy to read.

---

### 10. Objectives, contributions, structure, citations (24:32–28:17)

**Wang (24:32)**
Okay, and here it's regarding objectives. Okay — yeah, **you have at least five objectives, not bad.**

**Wang (24:44)**
Like contributions.

**Wang (24:47)**
…structure.

**Wang (24:50)**
LAS construction, methods. Okay. I think for the introduction — again, **please extend a bit regarding adaptor signature.** And maybe I will have more detailed comments later; I will try to add them maybe early next week, [to] give you some time to update them.

**Wang (25:12)**
And regarding the methods — okay, for today, sorry, I will have another meeting in 10 minutes, so let's go through it very quickly, but I will give you more comments. Or if you want, we can also have another meeting before the deadline next week, because I will be back next Monday.

**Wang (25:30)**
But anyway, this is for the motivation. And for the methods, I would like to [look at] the structure first. So we have the LAS construction — [unclear].

**Wang (25:51)**
[…] methods.

**Wang (25:55)**
Evaluation, results.

**Wang (26:00)**
But what's the difference between

**Wang (26:03)**
Section 3 and Section 4?

**Royce (26:08)**
Section 4…

**Wang (26:08)**
The evaluation is regarding the overall project?

**Royce (26:13)**
I think in Section 4 I put evaluation, and then achievement of the objectives, implementation challenges and limitations.

**Wang (26:20)**
Okay, okay, got it, got it.

**Royce (26:26)**
It's because they also ask about —

**Royce (26:31)**
twenty percent of the report is project achievement.

**Wang (26:34)**
Okay, okay.

**Royce (26:46)**
[And] 20 % is evaluation and/or reflection.

**Wang (26:55)**
But again, regarding detailed sections, I will give you more detailed comments later. But let's check regarding the structure overall. Okay — **it's good that you have the critical reflection.** [unclear] I think **the structure looks okay for me.** I mean, maybe you

**Wang (27:13)**
maybe you should **change a bit regarding the titles of the subsections** — but I will give you some comments later. But in general, you have already [written] the things I would like to say: for example, regarding

**Wang (27:28)**
contributions, objectives, and some important theory, [and] conclusions and critical reflection.

**Wang (27:40)**
And regarding the citations, let me have a look.

**Wang (27:44)**
How many citations do you have?

**Wang (27:50)**
**28. Okay, not bad.**

**Wang (28:12)**
…challenges.

---

### 11. How the written comments will arrive (28:17–29:31)

**Wang (28:17)**
Okay, let's do this: I will read it in more detail and give you some comments. Which way do you prefer? Shall I just add some — for example, if I say something here, I can add some

**Wang (28:32)**
comments directly by using the [Overleaf] review [mode]? Or would you like me to highlight the text in the report directly? Which one do you prefer?

**Royce (28:44)**
Uh — which one is easier for you to do?

**Wang (28:46)**
Sorry — which one [do] you [prefer]? I'm fine with both.

**Royce (28:52)**
I think just highlight, I think —

**Royce (28:55)**
and then put some comment.

**Wang (28:59)**
For example, if I would like to say, okay, what's the —

**Wang (29:05)**
what's the meaning of this one, right? [unclear]

**Wang (29:16)**
For example, this one: I would like to say, okay, what's this? What's the meaning of this one? Then shall I ask questions here, or shall I just highlight it?

**Royce (29:25)**
Oh yeah — you can just put comments, so I can see what's wrong with it. Yeah.

---

### 12. Deliverables and dates (29:31–31:00)

**Wang (29:31)**
Yeah, okay. Maybe it's easier. Okay, good. Yeah, let me know. Oh — another thing is that **maybe you can also share with me your video once you have recorded it**, and I can also give you some comments together.

**Royce (29:34)**
Okay, yes.

**Royce (29:49)**
Okay, I will send the video and the slides as well.

**Wang (29:54)**
Yeah — **maybe by the end of this week**, that would be helpful; then I will have time to watch it.

**Royce (30:00)**
Oh, okay. So I [will] polish the report, the video and the slides by the end of — by **Friday**, I guess.

**Wang (30:08)**
Okay, then I will have some time, maybe **Sunday or Monday**, to read them, and then I will give you some comments.

**Royce (30:15)**
Yeah — is it okay if I polish it until **Friday, 5 pm**, I guess?

**Wang (30:20)**
Yeah, it should be fine. It's totally fine. I will only be available on Sunday, so you have time to work on it.

**Royce (30:27)**
Yeah, so I'll send you the recording on Friday as well, hopefully.

**Wang (30:32)**
Okay, yes. And the deadline is next Thursday, right?

**Royce (30:36)**
The deadline is **the 4th of September**.

**Royce (30:43)**
On Friday.

**Wang (30:44)**
Friday, okay. Then **we could also have another meeting maybe on Thursday next week**, if you want — if you have seen my comments, [and] if you have any questions, we can also have another meeting next week. Okay, let's see — but let me first read it in more detail, okay?

**Royce (30:55)**
Yes.

*Recording ends at ≈31:00.*

---

## D. Action items arising

| # | Action | Owner | Source |
|---|---|---|---|
| 1 | 🟡 **VIDEO — SCRIPT CUT 1,190 → 887 WORDS (2026-08-27); THE RUNTIME GATE IS STILL OPEN.** The full delivery ran **00:31 → ≈11:10, ≈10 min 40 s**, against the rubric's 6–8. Wang: "compress it to less than 8 minutes… you should be faster, or maybe you should remove some details." **This is a measured delivery, and it settles a standing project caution the hard way:** `data-time` is a planning budget derived from a word count at an assumed pace, and the *measured* run exceeded it by minutes — never quote the derived sum as the video's length again. The cut order is already written in `report/slides/VIDEO_PLAN.md` §1. ⚠️ **He licensed both routes and mandated neither** — "be faster, **or** maybe you should remove some details" — so the blocking requirement is the finished video under 8:00; *how* the ≈3 minutes come out is Royce's call. Part of the excess was stumbles and restarted lines (09:38, 10:32), which a clean re-take recovers without cutting anything. | Royce | §3, §2 |
| 2 | ✅ **Visualisation is a trade, not an addition — APPLIED.** Deck slide 2 now carries ONE figure, not three (both RSA points removed), so "which one is the most important one?" has a single answer. — "if you would like to add more visualisations, maybe you should remove some content." His test is the non-specialist examiner facing the three dated quantum estimates: "which one is the most important one?" Make the intended reading of that slide unmistakable, or carry fewer numbers on it. Pairs with item 3, which is the same objection against the report figure. | Royce | §5 |
| 3 | ✅ **REPORT TITLE CHANGED (2026-08-27) — supersedes the "confirmed by supervisor" comment in the source.** `report/latex/report.tex:115–119` carries `\title{Implementing Post-Quantum Secure\\ Exotic Signature Schemes in Blockchains}` above a comment asserting the title was supervisor-confirmed. Wang has now explicitly reopened it ("now you have the freedom to update your title"). ⚠️ **Two different strengths, do not merge them.** The instruction is imperative: **"just remove the exotic signature schemes [and] refer to the adaptor signature"**. Avoiding **"lattice-based"** is a **hedged recommendation**, not a bar — "*maybe* lattice-based is too, I don't know, too specific, because people don't know what is lattice", followed by "*maybe* you could say post-quantum secure adaptor signature". The wording both parties said aloud and agreed: **"…Post-Quantum Secure Adaptor Signature Scheme[s] in Blockchains"**. Changing it touches the title page, the deck's title slide, and the stale code comment. | Royce | §6 |
| 4 | ✅ **FIGURE 1.1(a) REBUILT (2026-08-27) — argues from the chains, RSA removed entirely.** Wang's most concrete report comment, made with the PDF on screen: part (a) "is not that straightforward, not that direct", because "you are talking about RSA, but Bitcoin and also Ethereum, they are not using RSA". He wants it **"much closer to Bitcoin, or to blockchain itself"**, built from cited work on **when quantum computers become realistic** set against **the security of current blockchains**, so the reader sees the urgency directly. **He invited replacement, not retention** — "if you replace [it with] some more concrete things… it would be more relevant" — so the RSA rows are not protected: keep them only if they still serve that argument. ⚠️ **What constrains the redraw is a PROJECT rule, not a Wang ruling, and it survives either way:** each estimate stays at its own target, and the 2019 RSA figure may never be divided by the 2026 secp256k1 one (a ratio across targets is not a trend) — the caption exists to bar exactly that. The `nistir8547` row is the closest thing already present to the "in how many years" element he asked for, and its ≥128-bit / disallowed-after-2035 scoping does not lapse. | Royce | §8 |
| 5 | ✅ **ADAPTOR MOTIVATION EXPANDED (2026-08-27) — atomic swap, payment channels, scriptless settlement, and why this scheme was chosen from the exotic family; paid for by removing a thrice-stated survey point.** Original ask: — the one substantive report shortfall he named.** Currently "one or three sentences… otherwise people were just talking about something very abstract. We need some concrete motivation, the concrete examples." Two distinct things are owed: (a) **why adaptor signatures matter to blockchains**, with applications made concrete; (b) **why the adaptor signature among all the exotic families** — "there are still a lot of types of exotic signatures… so why did you choose adaptor signature?" He framed the post-quantum motivation as already done well, so this is the *second* of the two required motivations, not a rewrite of the first. ⚠️ **The two paragraphs already at `01-introduction.tex:104–122` do NOT discharge this — checked, do not re-argue it.** They were committed 2026-08-25 (`9032d1d`) and `report/latex/report.pdf` was last built 2026-08-26 12:45, both *before* this meeting, so the version on Wang's screen already contained them and he still called the motivation "one or three sentences" and "very abstract" — and Royce, looking at his own document, agreed: "I think I have not." Part (b) is the thinner half: §1.2's "Adaptor signatures" paragraph is definitional, and §1.1 gives the choice one clause ("a representative, high-value exotic scheme"). ⚠️ **Word budget:** the count is at 8,977 against a 9,000 ceiling, so new introduction prose needs an offsetting cut in the same edit — and per the standing rule, pay it out of filler or newly added prose, never out of a qualifier or a justifying clause. | Royce | §9 |
| 6 | ⚠️ **No Background section ⇒ the background goes in the Introduction.** Royce read the rubric rule aloud; Wang accepted it and drew the consequence: "then you should extend a bit for the subject area… you should introduce the important background in the introduction section." Depth over breadth still applies — the rubric says an extensive literature review is *not* required, and Wang did not override that. | Royce | §9 |
| 7 | ✅ **Post-quantum migration is not only a blockchain story — ASKED TWICE, NEVER DONE, NOW APPLIED (2026-08-27).** M11 §7 (03:34): "many [governments?] or many big tech companies, they are migrating… they are replacing their **basic** signatures". M12 §5 (13:03): "the post-quantum migration is not only for blockchains… **Have you mentioned this?**" ⚠️ **Repetition raises priority, not modality or scope** — asking twice is evidence he still does not see it as addressed, so check the artefact; it stays a "maybe you could", and both askings were over the *deck*. It was not there: the only wording in either artefact was an unattributed, **uncited** "standardised, and being adopted now" (`01-introduction.tex:80,97`; deck l. 985), which was also an EVIDENCE-OR-SILENCE defect in its own right. Fixed at **zero word cost** — both claims live inside `fig:whynow`, whose TikZ body and caption are excluded from the count. ⚠️ **Precision that must not drift:** what big platforms migrated first is **key agreement**, not signatures; the signature deployment now cited is **ML-DSA authentication on Cloudflare-to-origin connections (July 2026)**, never the public web PKI, where no post-quantum certificate was in use as of October 2025. Sources added to `refs.bib`: `westerbaan2025pqinternet`, `cloudflare2026pqroadmap`, `valenta2026pqauth`. | Done | §5 + M11 §7 |
| 8 | ✅ **The video's substantive content is accepted and the earlier rounds are discharged** — "the content is okay"; "you have addressed most of the questions I mentioned before, so I don't have further comments so far." Only the changes named in this table remain live (length, density, the title on the title slide, and the suggestion at item 7). Do not reopen settled slides beyond those. | — | §3, §4 |
| 9 | ✅ **Structure, objectives, contributions and citations accepted as they stand** — "at least five objectives, not bad", "it's good that you have the critical reflection", "the structure looks okay for me", "28, okay, not bad". Wang confirmed the Chapter 3 / Chapter 4 split after Royce explained that Chapter 4 carries evaluation plus achievement of objectives, challenges and limitations. Only reservation: "maybe you should change a bit regarding the titles of the subsections" — detail to follow in writing. | — | §10 |
| 10 | ✅ **Meeting 11's slide-count discrepancy is RESOLVED.** Wang counted the deck himself this time — "imagine how many slides we have in total, **13**, right?" — and raised no objection. Meeting 11's action 10 ("you have 10 slides", unresolved) can be closed: the 13-slide deck stands, and M10's "~10" is not a live instruction. | — | §5 |
| 11 | 📅 **Dates, all confirmed in the recording.** Royce: polish report + slides + video and send all three by **Friday 2026-08-28, 17:00**. Wang: reads them **Sunday 2026-08-30 or Monday 2026-08-31**; back **Monday**; written comments on **Overleaf as highlights with attached comments** (Royce's choice when offered), targeted **early next week**; optional further meeting **Thursday 2026-09-03**. Submission deadline **Friday 2026-09-04**. | Both | §4, §11, §12 |

**Nothing in this meeting authorises a new experiment.** Every item is presentation, wording or writing work. The Meeting-10 feature freeze was not mentioned and is not lifted; Wang's own framing — "you have addressed most of the questions I mentioned before" — is consistent with it.

> ⚠️ **Attribution here is source-backed, unlike Meeting 11.** The Teams export is diarised and the labels are corroborated by content throughout, so items 1–7 may be promoted to supervisor rulings in `CLAUDE.md` and merged into `las-context-consolidated.md` without re-listening to audio. What still needs care is **wording, not speaker**: any ruling quoted from §C must be checked against §A first, because the ASR garbles every domain term it touches.
