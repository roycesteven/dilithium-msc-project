# Cleaned Transcript — Meeting 10 with Zhipeng Wang (mock presentation review)

**Date:** 2026-08-14 or 2026-08-15 — **inferred, not confirmed.** Meeting 9 was Friday 2026-08-07 and Wang asked there for the 6–8 minute presentation to be delivered live "next week"; the deck was committed 2026-08-13 (`bc33734`); both transcript files were exported Saturday 2026-08-15 13:45–13:54. No recording, no meeting summary and no calendar record was consulted, so the day is not established. Do not cite a firm date for this meeting.

**Sources merged into this file:**
1. `meeting10_samsung_transcript.md` — phone ASR, **00:37–47:32**, diarised "Speaker 1" / "Speaker 2". SHA-256 as received: `d5ebb440368e4acfe79596e41ce5c00e453db4b36dc6dd128799f1c534b6132b` (UTF-16BE; **re-encoded to UTF-8 in place** — text verified identical after decoding, so the hash above no longer matches the file on disk). Lower word accuracy, but it is the **only source that says who spoke**, and it is the only source for the opening (00:37–02:00) and the post-meeting exchange (44:43–47:32).
2. `meeting10_teams_transcript.md` — Teams ASR, **0:08–42:16**. SHA-256: `a99d8abfed79dfc6978f63c463819abcacfc80d19198787f8d58b0304c0f8010`. Much higher word accuracy, but **diarisation is absent**: every utterance, Wang's included, is labelled "Royce Steven" (the export stamped the organiser's name on the whole meeting). Treat its speaker labels as meaningless.

**How the merge was done.** Wording comes from Teams where the two disagree and Teams is intelligible; attribution and timing come from Samsung. **Timestamps in this file are Samsung's**, because Samsung covers the whole meeting. The offset is stable: `Samsung ≈ Teams + 1:53`.

**Speaker mapping (same polarity as Meeting 8, flipped versus Meeting 9):**
- **Speaker 1 = Zhipeng Wang (supervisor)** — opens with "so what's the plan today?", gives the rulings.
- **Speaker 2 = Royce Steven (student)** — presents the deck, asks the questions.
This is fixed by the opening exchange and holds throughout, with the diarisation-merge exceptions listed next.

**Known defects in the sources, and how they were resolved:**
- Samsung's diariser **merges turns** at ≈27:32, ≈42:32 and ≈43:50–44:07, putting a Wang sentence inside a Royce block. Split by sense in §17, §31 and §31; each split is noted inline.
- Samsung mentions "13 slides" only as garble ("how we've seen you have things to nothing", 19:50) where Teams has it plainly. The 13-slide count is discussed **twice** — at ≈19:50 and again at ≈40:24 — and this is not an ASR duplication; both sources carry both passages.
- Teams ends at 42:16 (Samsung 44:09); everything after that is Samsung-only.
- ASR dropouts / unintelligible runs: ≈00:37–01:20, ≈26:30, ≈34:06–34:20, ≈45:03–46:50.

**Type:** meaning-preserving cleaned transcript, not a verbatim or legal record. Every exchange present in either source is represented. Unintelligible fragments are marked `[unclear]`; reconstructed words are in `[square brackets]`.

**Main topics:** Royce delivered the 6–8 minute deck end-to-end and Wang critiqued it slide by slide. Wang's verdict: **content is fine, presentation is not**. The claim that post-quantum exotic signatures "have no implementations" must be **softened** — multi-signatures in particular are being implemented. The deck **starts on technical detail too early**: it needs a motivation/application slide first (atomic swap, payment channel) and a high-level **method picture**, then results, then a closing summary that returns to the opening questions. **Fewer slides — 13 is too many, aim for ~10** — and more visuals, less text. The project should be framed as **practical LAS, not "an implementation of the paper"**, and the ML-DSA attempt should be reported rather than hidden. In the report: name the paper explicitly rather than "the paper"; add `SampleInBall` to the figure so `c` and `c̃` are visibly connected; keep LaTeX tables (not Excel images); balance over-long captions against over-short paragraphs; the notation table belongs in the appendix. Figures already fixed (the Criterion plot, the Bitcoin structure figure) are **accepted**. **No new work**: freeze the code, verify every result, spend the remaining time on the presentation, the writing and the video. Methodology is the weakest part of the delivery.

---

## A. Key contextual corrections

| ASR / unclear phrase | Corrected meaning |
|---|---|
| adaptive signature / adapted signature / a depth signature / adapt signature | **adaptor signature** — Wang and Royce both say *adaptor*; neither ASR ever spells it |
| last / the last / loss / ladies / latest / LAS signature / velocity nature | **LAS** (eprint 2020/845) |
| exotic / egoitic / ecosystemic / exortic / elevator signatures | **exotic** signatures (the advanced/extended family: adaptor, multi-, ring, group) |
| mouthy / mouthed signature | **multi-signature** |
| automic swap / automatic swap / automated swap / automatic swipe / automic web / water mega swap / automaker sware | **atomic swap** |
| in channel (Teams) / payment channel (Samsung) | **payment channel** — Samsung is the clearer source here |
| post compound secure / postponed / posted quantum | **post-quantum** secure |
| weakness / weightness / white nature | **witness** |
| presign meters / prey signature / basic nature | **pre-signature** |
| statement why / segment Y | **statement Y** |
| challenged in the nations / talented generation | **challenge generation** |
| resign and berry vine / presign and berry | **PreSign and PreVerify** |
| the 45 / for the 45 it's not | **Verify** — i.e. the *unmodified* FIPS 204 `crypto_sign_verify` |
| fips to 0.4 / fips to syrup wine 4204 | **FIPS 204** |
| mldsa / ML ESA / ML DSA loss / am I Al D | **ML-DSA** |
| high bid / high pits / high B / low bid / low B | **high bits / low bits** (Dilithium's `HighBits`/`LowBits` split) |
| laser / a laser / Lisa | **LaZer** (the lattice ZKP library used for π) |
| growth 16 / last end growth 16 | **Groth16** |
| electric curve | **elliptic curve** |
| VTM / DV game / on 10 verification / on gene verification | **EVM** / **on-chain verification** |
| gasoline | **gas limit** |
| finger / fingers / feasible / feature (at 31:33) | **figure(s)** |
| jawpoint IO / drawpoint I/O / dual point I | **draw.io** |
| latex label / lattix / mathematics tables | **LaTeX** (tables) |
| meltation table / notation table | **notation table** (`tab:notation`) |
| appetics / openings / in projects (36:23) | **appendix** |
| C tilda / say hat / AC and C and CNC | **`c̃`** (the challenge digest) versus **`c`** (the challenge polynomial) |
| cane functions / pain functions | **`SampleInBall`** — the function Wang wants shown between `c̃` and `c` |
| mutasis / panfasia | **`muthesis`** (the Manchester thesis LaTeX class) |
| political reflection / critic reflection | **critical reflection** (the Chapter 5 title) |
| glass is not safe / lady's proble / lattice problem | **lattice** (assumptions) — the post-meeting chat about a claimed break |
| **"solve for this sentence" / "you solve to this sentence" (02:44)** | **"soften this sentence"** — both ASRs garble the same word; the sense is unambiguous from what follows |
| **"they can fix the coin" (06:13, Teams 5:00)** | **spend the coin** — both sources garble it; "spend" is the UTXO verb Royce uses elsewhere in the same passage |
| **"Okay, 12 minutes, okay" (05:15)** | **[unclear]** — Teams has nothing at this point; do not read a timing figure into it |
| **"2.9 … 6.9% … minus 4%" (07:12–08:04)** | ⚠ **ASR-captured speech, NOT a citable figure.** These are Royce's spoken per-operation overhead numbers (Adapt ≈ Adapt-plus-PreVerify minus PreVerify). The only authoritative values are the regenerated report macros `\ovPreSign` / `\ovPreVerify` / `\ovAdapt`. Never lift these digits into the report or the deck. |
| **"99% of it … the challenge is only 0.7%" (08:04)** | ⚠ Two *different* 99% claims in one breath: `z`'s share of the **signature bytes**, and π's share of the **end-to-end swap time**. Both are macro-backed quantities; quote the macro, never the transcript. |
| **"for Dilithium 5 it needs more optimization to fit in one transaction" (10:37–10:55)** | ⚠⚠ **HAZARDOUS — the claim is not evidenced.** Both sources agree Royce said it, so it is transcribed faithfully, but `LASVerifyOpt`'s parameters are **compile-time D3-only**: D2 and D5 were never built or measured on-chain. The supported statement is "**not evaluated at D2/D5**", never that it fails or needs more work there. This is the same trap as the garbled Meeting-9 line at ≈38:44 — see §D, action 1. |
| **"modify the rules and policy" (09:31–10:09)** | Precision note: the patched-node verdicts come from `generateblock submit=false` (**consensus**), not `testmempoolaccept` (**policy**), and the attribution rests on the stock-accepts/patched-rejects **differential**. The spoken phrasing is looser than the experiment. |
| **"the size of the proof for LaZer depends on the input size" (25:31)** | Cross-check: the report's stated mechanism is narrower and is the one to use — LaZer **Huffman-codes the Gaussian responses**, so the length follows the sampled values (`tab:stage2-comm` caption). Royce's in-meeting phrasing is not wrong, but it is not the explanation the report gives. |
| "How have you stopped me to do your application for the posterior share" (00:37) | [unclear] — screen-share small talk before the presentation |
| "There is summarized effort … where does the things" (01:07) | [unclear] |
| "no shark come on this" / "Bixby" / "hurrah Hu" | [unclear] — phone-assistant wake-word contamination in the Samsung recording |

---

## B. Meeting summary

**Format.** Royce presented the full 6–8 minute deck without interruption (Wang: "you go first … maybe you can first go through all the slides, then I will give you some comments later"), then Wang went back to slide 1 and worked through it. Roughly the first third of the meeting is Royce's delivery; the rest is Wang's critique plus Royce's questions about the report.

**The one content correction.** The motivation slide claims post-quantum exotic signatures lack implementations. Wang: soften it. "It's a bit tricky, because they *do* have the proposed ones… maybe they haven't implemented all exotic ones." Exotic signatures are a family — adaptor, multi-, ring, and other advanced schemes — and **multi-signatures in particular are actively being implemented** by developers. The safe form is that not *all* exotic ones have been implemented, not that there are none.

**The main critique: the deck is a report, not a presentation.** Wang's repeated point, in several forms:
- **Start with why, not what.** Add a slide (or two) on the *application* motivation before any technical detail: adaptor signatures matter because they enable atomic swaps and payment channels, and making that post-quantum secure is the point. "You start the technical details too far, too early."
- **Assume a non-specialist examiner.** The second examiner may not be a blockchain/crypto person: "assume the audience have some computer science background in general, but not very specific for security, for crypto, for blockchain."
- **Show the method as a picture.** "If you can give me a picture to summarise the process of your method, I can easily get it." Wang was explicit that he himself follows the talk only because he already knows the details.
- **Close the loop.** End by returning to the questions posed at the start: can LAS give a post-quantum secure atomic swap, and what does it cost? "Have you addressed the questions you proposed at the very beginning? If not, what conclusion can we have for now, and what are the questions for future work?"
- **Fewer slides, more visuals.** 13 is too many; at ~30 seconds a slide, aim for **about 10**. Cut text, add diagrams; a table of contents slide and a "where are we" marker would help.
- **Be specific in the conclusion** — this is a project about signatures *for blockchains*, so say that.

**Framing ruling: practical LAS, not "an implementation of the paper".** Royce asked directly whether to frame the project as implementing the paper, given that ML-DSA-based LAS has a different rejection-sampling rate from the simplified scheme. Wang: "it's more for the practical LAS… you don't need to limit yourself in the scope of the paper. You have already attempted the optimisation, so you should summarise it, you should report it." On whether the ML-DSA rejection result is worth a slide: Wang put the judgement back on Royce — "do you think it is an interesting finding? If yes, highlight it. If you think it's just some trial result, then leave it" — and suggested a table in the *slides* even though the report keeps it as prose. Benchmarking a full ML-DSA build is **future work**, not this project.

**The Bitcoin demonstration is worth showing.** Royce doubted whether the patched-client demo belonged in the deck. Wang: yes — the project has two steps by design, first LAS standalone with no blockchain, then integrating it into an existing chain, and the integration problems and what you did about them are exactly what to report. The GitHub repository link is **not required** in the report, though it may be added.

**Report fixes.** At ≈28:57 the screen moves **off the deck and onto the report**; everything from §21 onward is report feedback. Name the paper explicitly instead of writing "the paper" — add the citation. The figure showing the challenge must make `c̃ → c` visible: Wang read it and asked "we have `c` — but how did we get `c`?", and the fix is to put **`SampleInBall`** in the figure rather than leave it to the surrounding text. Keep **LaTeX** tables rather than Excel screenshots (an image cannot be edited in place, and column widths can be adjusted in LaTeX); use draw.io or PowerPoint exported to **PDF** for decorative diagrams. Captions are not counted in the word budget, but a page-long caption next to a two-line paragraph is an imbalance — move some of it into the text. The notation table belongs in the **appendix**, which is not counted either. The already-modified Criterion figure (Fig. 3.4) and the Bitcoin structure figure are accepted as they stand. **Report Fig. 3.5** (`fig:swapflow`) is accepted too — caption included — but Wang wants a **simpler, more colourful diagram to come first**, where the application is introduced: two participants, one on each chain, one holding Bitcoin and one holding Ether, swapping. Separately, on **slide 5 of the deck**, Royce raised that he labels the parties `u₁`/`u₂` after the paper, and Wang agreed **Alice and Bob** would be friendlier there.

**Chapter title vs running header.** Royce asked whether the Chapter 5 running header may be shorter than the full title ("Conclusion, critical reflection and future work"). Wang: you can modify the template if you want, but if you want to be safe just follow it; a full-length header is too long. No firm ruling.

**Feature freeze, and what the remaining time is for.** Royce proposed freezing the implementation. Wang agreed — with the caveat that he has not checked the code details and it is Royce's call — and then made the priority explicit: **"you don't need to do new things, I think you have enough content; now you should make sure all of them are correct, they are precise."** Verify every result. Spend the time on the presentation, the writing and the video. Royce identified his own weak point — the method is not coming across — and Wang agreed that **methodology is the thing to fix**: "it's not only just the results, but how you get the results."

**Timeline.** "You still have more than two weeks."

**Post-meeting.** A brief exchange about a paper claiming lattice assumptions were broken. Wang: this happens most years, the claims are usually found flawed on review, and only a genuine break of the underlying problem would matter — at which point "many crypto researchers will change their topic". No action.

---

## C. Cleaned transcript

### 1. Opening (00:37–02:00) — *Samsung only*

**Wang (00:37)**
[unclear — screen-share small talk] … okay.

**Wang (00:57)**
Okay, so what's the plan today?

**Royce (01:07)**
Hopefully — I try [to]. [unclear] summarised [it]. Okay? [unclear]

---

### 2. Motivation slide: soften the "no implementations" claim (02:01–04:30)

**Royce (02:01)**
Yeah, basically, I try to summarise [it] in the slide. I think this is the research question — the research question, I mean, like our — my project, like the motivation.

**Wang (02:22)**
[You mean] the motivation—

**Royce (02:23)**
Yeah, the motivation: that there is not enough implementations, demonstration, or kind of [analysis] that analysed the paper in practice.

**Wang (02:44)**
[I have] one comment there — can [we] go back? Yeah, so here, maybe it would be safer that you **[soften]** this sentence, because you say they don't have the exotic ones. It's a bit tricky, because they **do** have the proposed ones. You can see that in practice—

**Wang (03:07)**
Maybe they haven't, let's say, implemented **all** exotic ones. Okay, like that, right?

**Royce (03:13)**
[Okay.]

**Wang (03:15)**
Because in practice some people are trying. Exotic signatures contain a lot of signature schemes — what we are doing is adaptor signature, but there [are] also multi-signature, ring signature, and other advanced signature schemes. For [the] others, some developers are trying to implement them in practice. So, I mean — okay, just try to make it safer.

**Royce (03:45)**
Okay. About the classical one?

**Wang (03:52)**
[The classical adaptor], no — [or] the post-quantum? No, for the **exotic**. So what is the exotic signature?

**Royce (03:57)**
[An exotic] signature is [where] they are adding some functionality on top.

**Wang (04:01)**
Yeah, yeah, yeah — so besides adaptor signature there [are] others. So for other signatures, especially multi-signatures, many people are working on it, many people are implementing it. So that's why I'm saying, this [claim] "they don't have the exotic ones" — maybe they have. So just change a bit of the wording here.

**Royce (04:27)**
Okay, thank you.

---

### 3. The LAS functions, and Wang defers his comments (04:30–05:25)

**Royce (04:30)**
And this is probably the core algorithm **functions** of the LAS adaptor signature. It is based on the base signature; [you] start with pre-signing. It's not a fully [valid] signature until the other party, who has the secret, adapts it.

**Royce (04:57)**
After [that, they] can publish it on-chain to claim. And then the other party — it leaks the secret, and the other party will extract and adapt.

**Wang (05:15)**
Okay, [unclear — "12 minutes"], okay. Yeah, you go first, let's try. Yeah, maybe you can first go through all the slides, then I will try to add some — give you some comments later.

---

### 4. How LAS is built: reused primitives, statement Y, adapt, extract (05:25–07:12)

**Royce (05:25)**
Yeah — the LAS adaptor signature is built on top of the basic signature, and we just — all the primitives are reused: for [the] challenge generation, and all the arithmetic calculations, and for the sampling as well. And then how we do it is: firstly, the person who holds the witness—

**Royce (06:13)**
—will create a statement `Y`, the adaptor public statement `Y`. And if they try to spend a pre-signature, it will [be] rejected, [because] it has not been a fully [valid] signature. And then, after the pre-signature is adapted with [by] the person who holds the witness, it will become a full signature and they can **[spend]** the coin. After it [is] published, it will leak the secret to [the] other party. Yeah, basically the secret will be extracted.

**Royce (07:12)**
And then the other party just learns the witness by extracting, and then adapts on the extracted witness, and then [they] can claim the coin from user one — or the other party — on chain one.

---

### 5. Per-operation timing (07:12–08:04)

> ⚠ The percentages below are ASR-captured **speech**. The authoritative figures are the regenerated macros `\ovPreSign` / `\ovPreVerify` / `\ovAdapt`. Do not lift these digits.

**Royce (07:12, cont.)**
Basically, the pre-signing is the most expensive one in terms of timing, and then the second one is pre-verification. And Adapt is — since Adapt is including PreVerify, so the Adapt itself is only [~2.9], like [~6.9%]—

**Royce (08:04)**
—minus [~4%], yeah, [~2.9]. And the Extract is the least [in] timing.

---

### 6. The adaptor cost is not LAS itself — the proof dominates (08:04–09:31)

**Royce (08:04, cont.)**
Because the [cost] of the adaptor computation is not the LAS itself. Even [if] we optimise the LAS algorithm, it won't make much difference, since — oh, I'm sorry, I think this one is — oh yeah, this one is the LAS **signature**, and it's [~99]% of it, like it's the response [`z`]; the challenge is only like [~0.7]%.

**Royce (08:04, cont.)**
And this is the **proof**: from end to end, it's [~99]% of the end-to-end timing. So if the LAS is optimised, it's not effective — if we only optimise the LAS, the core bottleneck is the proof, that takes a lot of time. So even if we try to optimise LAS, this does not make a huge difference.

**Royce (09:31)**
It will be more effective if we [find] some way to reduce the proof — to generate the proof, the time.

---

### 7. The Bitcoin demonstration (09:31–10:37)

**Royce (09:31, cont.)**
I think this is the demonstration of the Bitcoin. On the original Bitcoin, everything is accepted, since the Bitcoin doesn't verify LAS. But I try to make some modifications—

**Royce (10:09)**
—to modify the rules and policy. And after it verifies LAS, the negative [case] — the **tampered** signature — is rejected.

> Cross-check: the verdicts in the evidence come from `generateblock submit=false` (**consensus**), and the attribution rests on the stock-node/patched-node **differential**, not on the mempool policy check. The spoken phrasing "rules and policy" is looser than the experiment.

---

### 8. On-chain verification, one transaction (10:37–10:55)

**Royce (10:37)**
After [that], we try to make some optimisations on the on-chain verification. Yeah, we try to make [it fit] on the EVM [at Dilithium] 3 — it's on the boundary of the gas limit. But for [Dilithium] 5—

**Royce (10:55)**
—it needs more optimisation to fit in one transaction.

> ⚠⚠ **Both sources agree this was said, and it was not evidenced when said.** `LASVerifierOpt`'s parameters are compile-time **D3-only**, so at the time of the meeting D2 and D5 had never been built or measured on-chain. **Settled later the same week (2026-08-15):** D2 measured (verification only) and fits; D5 **derived** to exceed one transaction — a lower bound *computed from* measured quantities, the bound itself being arithmetic, not a measurement. The supported statement is now "*derived from measured quantities, one transaction is exceeded at Dilithium-V*" — **still never** that it "needs more optimisation", which remains an unevidenced claim about what optimisation would achieve. See §D action 1.

---

### 9. The ML-DSA experiment (10:55–12:30)

**Royce (10:55, cont.)**
So I did some experiment also on the unmodified FIPS 204 / ML-DSA — [for] some [functions] modification is unavoidable. Like PreSign and PreVerify: [these] are new algorithms, they need modifications. But for **[Verify]** it's not—

**Royce (11:34)**
—we can just use the unmodified verifier. For the optimised LAS, the signature will be smaller than the statement `Y`.

**Royce (11:46)**
So — yeah, the compressed signature makes the statement `Y` larger than the signature itself. But yeah, this has not been analysed: [whether] the signature is unforgeable or not.

**Royce (12:17)**
Yeah, I think that's [it]. I'm not sure.

**Wang (12:21)**
Okay.

**Royce (12:30)**
Yeah, okay.

---

### 10. Wang restarts from slide 1: lead with the application, not the technique (12:31–14:32)

**Wang (12:31)**
Oh, okay — so maybe we can start from the beginning. Yeah, just go to the first slide, [and] we can go through them one by one. [unclear] It's okay.

**Wang (12:42)**
The motivation is good. And what's the next slide? Okay — here I think maybe it would be helpful to add some slides, or maybe one slide, to show the motivation, the background of the **application** of adaptor signature. So imagine people will say: okay, here we know that there are different signatures, right?

**Wang (13:05)**
There are some different exotic signatures. But the question is, **why would we like to focus on the adaptor signature?** So you [should] show the importance of adaptor signature. [And] the importance of adaptor signature — we will see that it can be applied. We should show that it can be applied in **atomic swap**, in **payment channel**, something like that. So here you don't need to introduce details, but you should show that it's very important to build the adaptor signature to make it **post-quantum secure**. You know what I mean, right? So here, maybe you start the technical details too far, too early.

**Wang (13:41)**
Too early — you know what I mean, right? Imagine the second examiner is not that expert in blockchain crypto, but they would like to understand what's the motivation, why you would like to do this. You should tell them, right? You should assume that the audience have some computer science background in general, but **not** very specific for security, for crypto, for blockchain. Okay. And then here, yeah, regarding the technical details, I think it's good, it's okay — you introduce [them]. But maybe you should pay—

**Wang (14:14)**
—attention to the **timing**. If you allocate some time to the motivation, to the background, to show the importance, maybe you should just highlight the **most important** implementation results you have already achieved. Okay?

---

### 11. Summarise the method as a picture (14:32–17:34)

**Wang (14:32)**
Yeah, I think here it's good, right? You have the motivation. Okay — this is for the atomic swap.

**Wang (14:42)**
Okay, this is for atomic swap. So I was wondering, maybe somewhere you should highlight — you should **briefly introduce the method**.

**Wang (15:01)**
Yeah, so — where is it? So basically people will ask you: okay, how did you modify the existing base signature to make it a LAS? My suggestion is that, because we only have how many minutes — six or eight? Six to eight minutes. So that's why you don't need to give too many technical details here, but you should try to summarise the **concept**, and the **method**, at a very high level. So for example, if you could give me a **picture** to summarise the process of your method, I can easily get it. But now, when I was listening to you — maybe I'm okay, because I know the details, right?

**Wang (15:50)**
But imagine [I am] a reader or an audience [member]: I don't know the background of this, but I would like to quickly capture what you have done, why you would like to do this — the motivation — and how did you do that — the method.

**Wang (16:07)**
What are the most interesting **results**? And then what are the most important **takeaways**? That should be enough, right? Okay, so try to reorganise this. I mean, it's okay—

**Wang (16:19)**
—that we have a lot of sound technical details; of course, [they] can support your conclusion. But imagine, this is a bit different, right? When we are doing the presentation, it's more like you should present your results in just 6 minutes.

**Wang (16:36)**
And in a very — how to say — **easy to follow** [way]. Yeah, people can easily capture what you have done. And maybe finally, here is what you have done — [and] finally, you can also go back to the motivation you proposed at the very beginning, just to show: okay, if we use our LAS, can we be able to [build] a post-quantum secure atomic swap protocol? Okay, if yes, what are the **costs** we have to suffer from? So basically at the beginning you propose some motivation, propose the challenges, propose the questions; and finally you need to summarise: okay, have you addressed the questions you proposed at the very beginning? If not, okay, what kind of conclusion can we have for now? What are the questions for the future work?

**Wang (17:34)**
I think that's when we [would] have a better structure for the presentation.

**Royce (17:39)**
I see.

---

### 12. Make the conclusion blockchain-specific (17:45–18:16)

**Wang (17:45)**
Yeah, here you have some — yeah, you can see, you have some interesting conclusions. We always have open questions, right? But again, because this is — we are more focusing on the — I mean, [what] we would like to [do] in this project, right?

**Wang (18:06)**
We are focusing on [adaptor] signature **for blockchains**, right? So for your conclusion, you should be more specific.

---

### 13. Pictures instead of text (18:16–19:41)

**Royce (18:16)**
For this one — should I show this? Probably it's not the main point of the—

**Wang (18:27)**
Well, yeah, you can. Anything — one slide should be fine, but you don't need to highlight the details. Personally, I would like to have some **pictures**. If you can draw some attractive or beautiful pictures to show the concept, to show the complicated or complex method you adopted, then I think [people] have a better understanding of what you have done.

**Wang (18:52)**
So here, if I'm listening to you, do I have to read all the text? Which is quite a puzzle, to be honest. But yeah, that's true, right? When people would like to see that — so you can show me what is **high bits**, what is **low bits**, and why we would like to do that. Then if you can show some pictures, people will say: maybe they cannot get all the details, but they can get the **high-level concept** of your method.

**Wang (19:16)**
Then that's the goal — that's why we would like to do the presentation here. Otherwise, people can easily go to your **report**; they can see all the details. But here it's more like you should summarise the method, you should summarise the complicated or complex concepts in a very—

**Wang (19:35)**
—how would I say — in a more **friendly** way. Then people can follow you.

---

### 14. Slide count and pacing (19:41–20:05)

**Royce (19:41)**
Because it's only 6 to 8 minutes. One slide is probably just—

**Wang (19:46)**
Yeah, one slide.

**Royce (19:47)**
—probably like 30 seconds, something.

**Wang (19:50)**
How many slides do you have? [Thirteen] is too much. Yeah, maybe you can just — I don't know.

**Wang (19:56)**
It's okay, it depends on how fast you would like to go when you are presenting those slides.

**Royce (20:05)**
Yeah, so it's better to make [it] visual, so that [people] can easily capture [it], get [the] image in quickly.

---

### 15. A table of contents and a progress marker (20:14–21:00)

**Wang (20:14)**
Yes. And also at the beginning, you can also have — I don't know if you have the time — but at the beginning, maybe you can also have the **table of contents**, to show the structure; or you can use another way to show the structure. And then at the top, maybe you can also show: okay, **where are we**?

**Wang (20:37)**
For example, this is the introduction, and there is a method, and then there is a conclusion. Then it's another helpful method to let [the] audience follow your presentation better. But anyway, I mean, **content-wise, it's okay**.

**Wang (21:00)**
But yeah, **presentation-wise, you can improve it a bit**: the structure of the slides, and the visualisation of the results, and also the conclusion of the presentation.

---

### 16. Framing: practical LAS, not "an implementation of the paper" (21:17–24:00)

**Royce (21:17)**
Yeah, so I want to ask about the rejection sampling — like, the simplified LAS and the ML-DSA LAS are a bit different.

**Wang (21:29)**
[Go on.]

**Royce (21:31)**
[If I'm] using the original ML-DSA, the rejection sampling is higher.

**Wang (21:37)**
So—

**Royce (21:39)**
Should I just frame this project as **the implementation of the paper**, or [as a practical] implementation?

**Wang (21:47)**
I mean — yeah, I think it's more for the **practical LAS**. So maybe you don't need to limit yourself in the scope of the paper, because you have already done [it], right? You have already attempted the optimisation, so you should summarise this, you should report it, which is also helpful.

**Royce (22:09)**
Because in the report, I just — like, there is no table for it, [it's] just in one section, I believe. One section.

**Wang (22:19)**
[In one] section.

**Royce (22:24)**
Yeah, just this one.

**Wang (22:25)**
Yeah, I mean, that's fine. Then in the **slides**, you could just either use half of the slide, [or] one slide, to show that you have attempted [it]. Yes.

**Royce (22:37)**
So yeah, I just present the number in the paragraph. I don't show it on the tables or figures.

**Wang (22:58)**
Yeah, it's fine — in a report you can do it like this. But in the slides, you can [add] some table, right?

**Royce (23:06)**
Yeah, because the ML-DSA LAS — the rejection sampling, it's twice [as high]. So I just report [it] as a finding, like—

**Wang (23:17)**
Do you think it is an interesting finding? If yes, maybe you can highlight it. If you think that it's just some trial — I don't know, solution-trial results — then just leave it.

**Royce (23:29)**
Because I'm afraid that people are more interested in the **benchmarking**, like, how [it would be] if we use the ML-DSA.

**Wang (23:39)**
You can calculate it as **future work**. I mean, of course, at the beginning it's not the focus of this project — but you have extended the [scope] by capturing, or by attempting to implement, something like this.

**Royce (23:58)**
Yeah.

---

### 17. What the markers actually read (24:23–25:31)

**Royce (24:23)**
So the markers should — like, if they read the [report], they should be able to understand what the main technology [is], without probably any access to the repository.

**Wang (24:40)**
So what do you mean — do you mean, should we add the link of the report? What's your question?

**Royce (24:49)**
My question is: I assume that the marker does **not** have access to the [repository]. They probably don't want to look at it.

**Wang (24:57)**
[I] don't think they have time to. But if [they're] interested, [maybe they'll] check. So—

**Royce (25:03)**
They just probably have to understand everything through the video, and—

**Wang (25:08)**
Yes, yes — and the **report**. But the video is to summarise what you have done, and to help the examiner understand what you did very quickly; and then if they are interested, then they will also go into the details of your report — [or] what we call the dissertation.

---

### 18. Why the LaZer proof size varies (25:31–26:45)

**Royce (25:31)**
So — regarding the number for the LaZer.

**Wang (25:53)**
Hello?

**Royce (25:55)**
Because the size of the proof for LaZer depends on the [input], like — but for LAS and **Groth16**, I think it's fixed, because it's based on elliptic curve. Yeah, they have a fixed setting for Groth16. For LaZer, it depends on the input size, so that's why the LaZer [proof size] increases. So would that be a question?

**Royce (26:30)**
No.

**Wang (26:31)**
Well then I think — yeah, because you just follow the schemes; you use them as **black boxes**, you're not modifying them, [so] it's fine. Just report what you achieved, honestly, and that should be fine. It's not your fault, right?

**Wang (26:45)**
That's [it].

> Cross-check: the report's stated mechanism for the range is narrower — LaZer Huffman-codes the Gaussian responses, so proof length follows the sampled values. Use that, not "input size".

---

### 19. Is the patched-Bitcoin demo worth showing? (26:49–28:43)

**Royce (26:49)**
I'm not sure if this is the correct demonstration — like, the first demonstration [is the] atomic swap, and the second demonstration is — I don't know if this is interesting. Like, is it the core idea of the project to [show] the modification of the Bitcoin, something [like that]?

**Wang (27:18)**
So, what's your question?

**Royce (27:21)**
I think this is the modification on Bitcoin. Is it interesting to show?

**Wang (27:27)**
[You] modified the Bitcoin, or not?

**Royce (27:32)**
I'm modifying on the [client], modified, so that an invalid signature will be rejected.

*[Samsung merges the next sentence into Royce's turn; it is Wang's.]*

**Wang (27:40)**
See, that's interesting, yeah?

**Wang (27:45)**
Yeah, it's okay — because here you just show how we should implement this in practice, right? So yeah, it's also important to show that. Again, as we discussed at the very beginning, for this project we should have at least **two steps**: the first step is to implement LAS itself without involving any blockchain or Bitcoin components; and then the second step is to try to **integrate** LAS into an existing blockchain system, Bitcoin or Ethereum. So when we are integrating it, then we will have some issues.

**Wang (28:28)**
Of course, you can report what you did to **address** the issues, to make it — let's say — able to be plugged into the system.

---

### 20. The GitHub link is not required (28:43–28:57)

**Royce (28:43)**
Okay, so it's confirmed that [the] GitHub [repository] does not have to be in the report, right? Just confirming.

**Wang (28:50)**
No, no — just [the] concept. But you can add it if you want. Okay.

---

### 21. Report Fig. 3.5 (`fig:swapflow`): keep the technical one, add a friendly one first (28:57–30:53)

> **Scope of this section (Royce, confirming):** by this point the screen has moved **off the
> deck and onto the report**, and everything here is about **report Figure 3.5**, `fig:swapflow`
> ("The atomic swap, message by message", `chapters/03-results.tex`) — *except* Royce's turn at
> **30:37**, which is about **slide 5 of the deck**. Sections 23–29 below are likewise report
> feedback, not deck feedback.

**Royce (28:57)**
Yeah, I just — [about] last time. So if this figure is already [okay]—

**Wang (29:21)**
Usually it's okay, like — easy to understand, for me, yes. Okay. But you can also add — this is for what? This is for [the] atomic swap, or the signature?

**Royce (29:32)**
Atomic swap. So if this figure caption is acceptable — [or] if this [is too long]?

**Wang (29:38)**
Yeah, I think for me it's acceptable. But let me see. But again, you can see here — it's still bad.

**Royce (29:43)**
[There's] no paragraph.

**Wang (29:46)**
And maybe at the beginning of the application, you can add a more **colourful but simpler diagram**, to show how [the] atomic swap works in general. So this [one] is more technical, right — which is good, which will show the technical details. But at the beginning, maybe when you are talking about atomic swap, people will [wonder]: so how does atomic swap look like? Okay, we have two participants—

**Wang (30:15)**
—one is on one chain, one is on the other chain. Then one has some Bitcoin—

**Wang (30:20)**
—one has some Ether. [When] they would like to do the exchange, then how will we build the atomic swap? Just add some icons, some colourful diagram — then people will [find it] more friendly, and probably [clearer].

*[This turn is about **slide 5 of the deck** — the swap board, which labels the parties `u₁`/`u₂` — not about Fig. 3.5.]*

**Royce (30:37)**
I think, because this is — I'm following the paper, using `u₁` and `u₂`. Probably [it would] make it more friendly, like Alice [and] Bob?

**Wang (30:45)**
Yeah, **Alice and Bob** — that's more general, more friendly.

---

### 22. Still too much detail for 6–8 minutes (30:53–31:00)

**Royce (30:53)**
[This is] still too much details for 6 to 8 minutes.

**Wang (30:57)**
[Agreed.]

**Royce (31:00)**
This is more like half an hour.

---

### 23. The modified Criterion figure is accepted (31:00–32:01)

**Royce (31:00, cont.)**
Regarding what else, if I could [ask] — I think last time you made a comment about the figure that I take directly from [Criterion], and this [one] is a bit modified. Is [that okay]?

**Wang (31:22)**
Yeah, that's good. I mean, the font size, right? The size of text — it looks better.

**Wang (31:28)**
Okay, good. To me, I like this figure. Yeah, but yeah, it's okay.

**Royce (31:33)**
Is there any figure that you still don't like?

**Wang (31:38)**
I mean, I don't have figures [I] don't like. My suggestion [is] that we can always make figures better — but for this, I think it's good.

**Royce (31:49)**
Regarding this?

**Wang (31:52)**
Yeah, it's good, it's good.

**Royce (31:56)**
So what's that? I think this is the Bitcoin [transaction] structure.

**Wang (32:01)**
Bitcoin structure, yeah. Okay, good. Yes, it's good.

---

### 24. LaTeX tables, not Excel screenshots (32:06–34:06)

**Royce (32:06)**
Is it better to use tables from **Excel** or something? … Because I heard some people say that — I think from the workshop, previous-year students said, with presentation, that it's better to make [them] like [images]. I don't know.

**Wang (32:31)**
Yeah, I think for me — I mean, they are not crypto researchers. For crypto researchers, [what] you [see is] — okay, Excel. I mean, I think, even for many—

**Royce (32:41)**
Because they say that [the] LaTeX table is sometimes like — sometimes it's like this, like out of [the margin].

**Wang (32:53)**
But you can always **adjust** it, right? You know how to adjust [the widths], right?

**Royce (32:57)**
Okay, so [I'll] just use LaTeX-format tables like this.

**Wang (33:00)**
Yeah, yeah — but again, you should keep them **in the page**, [in] the size. Of course it's easy to do that, right? I mean — for me, I haven't used Excel for quite a long time, especially when I have to do some budget breakdown when I'm applying for grants, where they asked me to use Excel.

**Wang (33:21)**
Of course, [then] I have to use it. But for me, I think — I mean, I like coding, right? [LaTeX] is kind of coding — coding for writing, right?

**Wang (33:29)**
So I can easily control which position I would like to make, and which colour I would like to add. I mean, okay — up to you, up to you. But for me, my [suggestion is]—

**Royce (33:39)**
—[to] use [LaTeX] tables, yeah.

**Wang (33:46)**
So if you use Excel, then how can you put that in[to the document]?

**Royce (33:50)**
[As] an image.

**Wang (33:52)**
[An] image. So if you want to modify it, then you have to go to Excel to modify it — [it's] the image. It's — gosh, I don't know what to say. But yeah, you decide.

---

### 25. Name the paper explicitly (34:22–35:00)

**Royce (34:22)**
For this one — probably I shouldn't say "[the] papers"; I should [be] more explicit. Which paper is it?

**Wang (34:30)**
"The paper" — which paper? Yeah.

**Royce (34:32)**
That's why I need to probably—

**Wang (34:34)**
Yeah, if you would like to refer to that paper, then you can [add]—

**Royce (34:39)**
—[a] citation.

**Wang (34:40)**
Or you can say in general how we do that. [And] this — is this a figure, or is this [something] you drew by using LaTeX?

**Royce (34:53)**
This one is [all] LaTeX. I [don't know] when to use Excel and make a picture out of it.

---

### 26. draw.io / PowerPoint exported to PDF (35:00–35:39)

**Wang (35:00)**
But yeah — sometimes, maybe if you would like to draw some, let's say, more beautiful, more colourful figures, of course you can use **draw.io**. Sometimes I use draw.io, or use PowerPoint, and then I will export the PDF, and then I put the PDF inside the [document].

**Royce (35:22)**
Oh, so it will export as PDF. Okay, so it's higher — high quality.

**Wang (35:26)**
High quality. draw.io.

**Royce (35:32)**
draw.io, yeah. [unclear]

**Royce (35:39)**
Oh, I know this one. Yeah.

---

### 27. Word budget and the notation table (35:39–36:43)

**Royce (35:39, cont.)**
And — yeah, I'm not sure if it's a bit too bad, because in the paper we can probably explain [more].

**Royce (36:00)**
[There] is no word restriction or something. I don't [know]—

**Wang (36:03)**
No, what?

**Royce (36:05)**
Because sometimes I have — I don't know, [there are] too many variables, like — sometimes I don't have the word budget. Okay, [and] I'm afraid that people—

**Wang (36:23)**
[They] do not count the words in [the] **appendix**. So you can [put] the table in [the] appendix — what we call a **notation table**, right? Summarise what's the meaning of the parameter[s].

---

### 28. Make `c̃ → c` visible: put `SampleInBall` in the figure (36:43–38:34)

**Royce (36:43)**
Yeah, I think this is the modification that I try to make from the original paper.

**Wang (36:51)**
Since [it's the] original — I think it's good. Okay.

**Royce (36:56)**
But if you read this one, do you understand [that] there is a `c` and a `c̃`?

**Wang (37:06)**
[It] worked. Okay, we have `c`, I can see `c` — but **how did we get the `c`?**

**Royce (37:12)**
I explained [it] in this one, like — it's just like a digest of it.

**Wang (37:20)**
Ah, I see. So maybe it would be better to add the **[`SampleInBall`] function** inside it—

**Wang (37:27)**
—then people [won't be confused]. People might be confused, right? So this is `c`, then why is this one `c̃` — what's that, how to call that?

**Royce (37:39)**
Yeah — oh, this is — yeah, it makes sense.

**Wang (37:54)**
Yeah, for me — well, [when] I'm reading this, I would ask: okay, here is `c`, then what's this one? How did we get it?

**Royce (38:03)**
Like, I should probably add like this one. There should be like this.

**Wang (38:11)**
Yeah, yeah — something like that. There should be one **function** like that.

**Royce (38:17)**
[This one should include `SampleInBall`,] otherwise people will be confused, like: why is [it], how [do we] need [it]?

**Wang (38:21)**
Yeah, yeah, that's true. [Anyway,] you can make another pass proof-reading it.

**Wang (38:34)**
They're not big issues.

---

### 29. Caption length vs paragraph length (38:36–39:25)

**Royce (38:36)**
So I don't know — like, this line, is [it acceptable to have] this kind of caption like this, or is it too long?

**Wang (38:45)**
It's okay, it's okay. It's okay. But yeah, and you can—

**Wang (38:52)**
Yeah, you can try to make it **shorter** and add something into the text. Again, they don't count the caption.

**Royce (39:01)**
I believed [so], though.

**Wang (39:03)**
Okay, just double-check. I don't know, to be honest.

**Royce (39:08)**
Yeah, but it makes my paragraph very short.

**Wang (39:13)**
Very short — the caption is too long, [the] paragraph is too short. I mean, yeah, you can keep — you can—

**Wang (39:19)**
—I don't know, modify a bit to keep the **balance**.

---

### 30. Who marks it (39:25–40:08)

**Royce (39:25)**
So the marker will be like people from cryptography, or any people from [the] department?

**Wang (39:35)**
Sometimes we will have [an] external second marker — external means other universities. You know, I don't know; I mean, it depends. Maybe sometimes [from our own school], yeah.

**Wang (39:48)**
I think maybe from other universities — but that's why they call [them] external. No worries. I mean, we are strong, right?

**Wang (39:59)**
You have done good work. Don't worry about it. Let's try our best to make it [good].

**Royce (40:08)**
Yeah.

---

### 31. Thirteen slides is too many — aim for ten (40:24–41:01)

**Royce (40:24)**
So for this, 13 slides — do you think it's a good amount of slides, or should it be [fewer]?

**Wang (40:31)**
Yes, maybe I would choose—

**Royce (40:34)**
—fewer slides, but more visual [results]?

**Wang (40:39)**
[Yes,] more visual results. Because imagine if you have 12 slides, right? Which means that for each slide you [only] have **30 seconds**. Do you think that you can present one slide within 30 seconds?

**Royce (40:53)**
Probably a maximum [of] 10 slides.

**Wang (40:55)**
Ten slides — more or less 10 slides. That sounds [right].

---

### 32. Chapter title vs running header (41:23–42:32)

**Royce (41:23)**
I'm wondering one thing as well: if I put the chapter as "Conclusion, critical reflection and future work", is it okay [if] the header is "Conclusion and future work", or must it match exactly?

**Wang (41:37)**
How did you generate [the headers]?

**Royce (41:39)**
I just used the template they gave me — `muthesis`, I guess.

**Wang (41:49)**
You can modify [it] if you want.

**Royce (41:53)**
I just used the template that they gave me.

**Wang (41:59)**
[That's] something you [can] define. But [if] you feel [you want to be] much safer, then just follow [it].

**Royce (42:05)**
Because if I put everything, it's just too long.

**Wang (42:09)**
Yeah, it's too long. It's fine, yeah.

---

### 33. Freeze the code; verify the results; fix the methodology delivery (42:32–44:21)

*[Samsung merges Wang's "It's okay" into Royce's turn at 42:32, and mis-attributes Wang's 43:50–44:07 replies to Royce. Split by sense below.]*

**Wang (42:32)**
[It's] okay, yeah, okay.

**Royce (42:34)**
So yeah, I just find it a bit difficult how to present it.

**Wang (42:39)**
Yeah, just **practice** it. You still have **more than two weeks**, right? Yeah, just try.

**Wang (42:44)**
Yeah, you have done [well].

**Royce (42:46)**
And in terms of the implementation — it's just for the coding part. I think I can just **freeze** it.

**Wang (42:56)**
I think so — if you think they are ready. Because I haven't checked the details; I don't think I have time to check all the details. But yeah, you decide, right? Your call.

**Royce (43:06)**
But I believe that it's more valuable to spend the time to understand [it] in details, and the concept, so I can—

**Wang (43:16)**
Yeah, it makes sense, makes sense. Now it's good to—

**Royce (43:21)**
—focus on the **quality**, [not] the quantity.

**Wang (43:24)**
[Yes —] the quality of the presentation, especially the presentation, the writing, and also the video.

**Royce (43:32)**
And make sure that [you] verify all the results.

**Wang (43:38)**
Yeah — verify the results. Yes, verify the results. **You don't need to do new things.**

**Wang (43:41)**
I think you have enough content. So now you should make [sure] that all of them are **correct**, they are **precise**, yeah.

**Royce (43:50)**
I think what I'm still lacking is delivering the **method** that I'm using — [it's] still not clear, [so] probably the marker still [has] questions from my presentation, like: what is the method?

**Wang (44:00)**
Yeah, I think **methodology is very important**.

**Wang (44:07)**
It's not only just the results, but **how you get the results**. Yeah, okay.

**Wang (44:21)**
Okay, great — [I'll] see you [next time].

**Royce (44:23)**
Yeah, [you] too.

---

### 34. Post-meeting: the "lattices are broken" claim (44:43–47:32) — *Samsung only*

**Royce (44:43)**
Um, did you see the new paper that people said that [lattices are] not safe anymore? Something [like that].

**Wang (44:53)**
[LWE? Lattice?]

**Royce (44:57)**
[Yeah,] but people need to verify [it], I guess.

**Wang (45:03)**
[unclear]

**Royce (45:06)**
Yeah, I believe so.

**Wang (45:15)**
Yeah, [there have been] some papers. But you know, people [need to] see how [they are] reviewed.

**Royce (45:21)**
Yeah, I forgot which paper it is.

**Royce (46:12)**
[unclear — reaction to the announcement; it shocked a lot of people.]

**Royce (46:48)**
Oh, this paper [was wrong].

**Wang (46:53)**
Yeah, this paper — [they] find some flaws in the paper.

**Royce (46:57)**
It's — oh, it's not — yeah, it's not funny.

**Wang (47:01)**
I mean, almost every year, people will publish a paper [saying]: okay, I can break a lattice problem.

**Royce (47:08)**
But after the verification, it's not true.

**Wang (47:11)**
I mean, anyway — it's only [a] worry if they have been [confirmed]. If the underlying mathematical problems have been broken, then okay, many crypto researchers, they will change their topic.

**Royce (47:26)**
Yeah, because I [was] shocked when I saw the post, like: so if it's not safe, what's the point [of what] I'm doing?

**Wang (47:32)**
[unclear — the method is still worth exploring.]

---

## D. Action items arising

| # | Action | Owner | Source |
|---|---|---|---|
| 1 | ⚠⚠ **Do not let the "D5 needs more optimisation" claim reach the deck.** Royce said it on record (§8) and it is **not evidenced**. Checked 2026-08-15: the deck template and the report are both already clean — the slide body says *"no other parameter set was evaluated"* and the notes say *"one parameter set and one message length"*, so this was an ad-lib past his own text. **Five attempts to close it by derivation failed review before one held**; **settled 2026-08-15** — **D3 = measured fits** (a client receipt for a whole claim) · **D2 = measured fits, ~65% of the cap** (a harness charge for *verification only* — a narrower boundary, not interchangeable with D3's) · **D5 = derived to exceed one transaction**, the bound computed from measured quantities. **Still unsupported, and the reason the action stands:** that D5 "needs more optimisation" — nothing measures what optimisation would achieve there. The report, appendix, deck and `GAS_LIMIT_INVESTIGATION.md` §7 were brought into line on 2026-08-18; the "no other parameter set was evaluated" wording that was true when this file was written is now **false** and has been removed. | Royce | §8 |
| 2 | ⚠ **Soften the motivation claim about exotic-signature implementations.** Not "there are no implementations of post-quantum exotic signatures" but "not *all* exotic ones have been implemented" — multi-signatures in particular are actively being implemented. Applies to the deck **and** to the same claim wherever it appears in the report. | Royce | §2 |
| 3 | ⚠ **Restructure the deck: motivation → method → results → takeaways → back to the opening questions.** Add an application/motivation slide (atomic swap, payment channel; why post-quantum matters) **before** any technical detail, and a **high-level picture of the method**. Close by answering the questions posed at the start, plus what remains open. | Royce | §10, §11 |
| 4 | ⚠ **Cut the deck from 13 slides to about 10**, and replace text with visuals. Budget ~30 s per slide against 6–8 minutes. Optionally add a table-of-contents slide and a "where are we" marker. | Royce | §14, §31 |
| 5 | ⚠ **Make the conclusion blockchain-specific** — this project is about adaptor signatures *for blockchains*; say so rather than concluding in general terms. | Royce | §12 |
| 6 | ⚠ **Frame the project as practical LAS, not "an implementation of the paper."** Do not limit the scope to the paper; the ML-DSA attempt is a result and should be summarised and reported. Full ML-DSA benchmarking is **future work**. | Royce | §16 |
| 7 | ⚠ **Add `SampleInBall` to the challenge figure** so `c̃ → c` is visible in the figure itself, not only in the surrounding text. Wang read the figure and could not see where `c` came from. | Royce | §28 |
| 8 | ⚠ **Name the paper explicitly** wherever the report says "the paper" — add the citation. (Reconfirms the standing rule.) | Royce | §25 |
| 9 | ⚠ **Balance over-long captions against over-short paragraphs** — shorten the caption and move material into the body text. Captions are not counted, but the visual imbalance is the problem. | Royce | §29 |
| 10 | **Keep LaTeX tables**; do not switch to Excel screenshots (an image cannot be edited in place; LaTeX column widths are adjustable). Keep tables inside the page. Use draw.io or PowerPoint **exported to PDF** for decorative diagrams. | Royce | §24, §26 |
| 11a | **Report:** add a simpler, more colourful atomic-swap diagram **before** Fig. 3.5 (`fig:swapflow`), where the application is introduced — two participants, one chain each, Bitcoin vs Ether. **Keep Fig. 3.5 and its caption**; both are accepted. | Royce | §21 |
| 11b | **Deck slide 5:** relabel the parties **Alice/Bob** instead of the paper's `u₁`/`u₂`. Applies to the slide only — the report's mathematics keeps the paper's notation. | Royce | §21 (30:37) |
| 12 | **Keep the notation table in the appendix** (appendix words are not counted). Already the case; this is confirmation, not new work. | Royce | §27 |
| 13 | **Keep the patched-Bitcoin demonstration in the deck** — the two-step structure (LAS standalone, then integration into an existing chain, with the integration problems and their fixes) is exactly what to show. | Royce | §19 |
| 14 | ⚠ **Freeze the implementation. No new work.** "You don't need to do new things — I think you have enough content; now you should make sure all of them are correct, they are precise." **Verify every result**; spend the remaining time on the presentation, the writing and the video. | Royce | §33 |
| 15 | ⚠ **Fix the methodology delivery** — Royce's own diagnosis, which Wang endorsed: the method is not coming across. "It's not only just the results, but how you get the results." | Royce | §33 |
| 16 | The **GitHub repository link is not required** in the report; it may be added if wanted. No action needed. | — | §20 |

**Accepted as they stand:** the modified Criterion figure (font size fix — "I like this figure"), the Bitcoin transaction structure figure, the technical atomic-swap figure and its caption, and the deck's **content** ("content-wise, it's okay"). The chapter-title-vs-running-header question got no firm ruling — Wang's advice was to follow the template if in doubt.

**Nothing in this meeting adds an experiment or reopens a frozen scope item.** Every item above is presentation or report work, plus one claim to delete.

**Timeline stated:** "more than two weeks."
