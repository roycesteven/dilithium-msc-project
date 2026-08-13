# Cleaned Transcript — Meeting 9 with Zhipeng Wang

**Date:** 2026-08-07 (inferred — the transcript file was written 2026-08-07 17:15 and Meeting 8 was 2026-07-31; **no meeting summary or recording file exists for this meeting**, so the date is not independently confirmed).
**Sources merged into this file:**
1. `meeting9_original_transcript.md` — the only source: a single ASR pass over the meeting, 00:02–42:40, diarised as "Speaker 1" / "Speaker 2". SHA-256 of the file as received: `e816b14b9b7c54985f653329d53fe07f9ea31c463fe55a3e92f89d151c360e2f` (UTF-16BE; re-encoded to UTF-8 in place — text unchanged byte-for-byte after decoding).
2. There is **no** `meeting9_summary.md` and **no** recording in the working tree, so unlike Meeting 8 there is no second source to cross-check the ASR against. Reconstructions below lean on project context instead, and are marked.

**Speaker mapping (polarity is FLIPPED versus Meeting 8, i.e. back to the Meeting-7 convention):**
- **Speaker 1 = Royce Steven (student)** — opens with "I've made some progress this week", reports the week's experiments, asks the questions.
- **Speaker 2 = Zhipeng Wang (supervisor)** — asks what the numbers mean, gives the rulings.
This mapping is fixed by the first utterance ("the clarification that you asked probably last week") and holds consistently throughout.

**Type:** meaning-preserving cleaned transcript, not a legal/verbatim transcript. Every exchange in the source is represented; nothing is dropped. Genuinely unintelligible fragments are marked `[unclear]`; reconstructed words are in `[square brackets]`.
**Known gaps:** ASR dropouts at ≈03:40–04:10, ≈08:15–08:50, ≈28:01–28:49 and ≈29:07–29:39. The last three utterances (42:17–42:40) are post-meeting pleasantries.

**Main topics:** what a Bitcoin transaction actually signs (a hash of the transaction, carried in the witness) and whether LAS fits — the 520-byte script chunk limit forces chunking, but the binding limit is block **weight**, and the LAS spend sits at ≈2.9% of the standard limit, so it fits; **Bitcoin's real problem is verification, not carriage**; both of last week's open questions answered — LAS verification fits in **one** EVM transaction under EIP-7825's per-transaction gas cap, and a stock Bitcoin node can carry LAS objects; SHAKE dominates the on-chain gas, and Wang asked whether another hash function would be cheaper (with the caveat that one working version is already acceptable); the patched Bitcoin client **must be benchmarked** — when you modify something for better security, always show what it costs; the Meeting-8 transaction breakdown is **accepted** ("now it's much clearer"), with one fix: explain why the proof size is a range rather than a fixed number; statement-Y compression fails, and that negative result is worth reporting; **conclusions must be consistent with the numbers reported** (the LaBRADOR-vs-LaZer wording currently is not); **the central ask of this meeting — add a §1.4 "Contributions" subsection to the introduction** summarising the most important findings and whether the objectives were achieved; figures are "much better than I thought", but a table must not fill a page; cite the Bitcoin improvement solutions (SegWit/Taproot) when saying the original structure cannot carry LAS; **mock the 6–8 minute presentation next week**.

---

## A. Key contextual corrections

| ASR / unclear phrase | Corrected meaning |
|---|---|
| last / the last / loss / Las / last signature / elastic natives | **LAS** (the adaptor-signature scheme, eprint 2020/845) |
| weakness / weightness / white nature / water | **witness** (the SegWit witness field) |
| basic nature / previous nature / prey signature | **pre-signature** |
| contraception / trajectory / traction / real contraception | **transaction** |
| unchained / of change / island chain | **on-chain** / **off-chain** (context decides) |
| the second meet | **SegWit** (the Bitcoin improvement Wang wants cited) |
| snore verification | **Schnorr** verification (BIP340) |
| postcard verification | **post-quantum** verification — i.e. the patched `OP_CHECKLASSIGVERIFY` node |
| the big point line / Bitcoin kind | the **Bitcoin client / code** |
| sheikh / check-in / shake | **SHAKE** (the XOF used for the challenge and the mask) |
| the talented generation | the **challenge generation** (`c = H(pk, w+Y, M)`) |
| mlvsa / ML DC / mld / mldsad / am I Al D | **ML-DSA** (FIPS 204) |
| little bits / low beat / high pits / high beat | **low bits / high bits** (Dilithium's `LowBits`/`HighBits` split) |
| statement why / segment Y | **statement Y** |
| adept epistraction | **Adapt / Ext** (extraction) |
| laser / leisure / Lisa | **LaZer** (the lattice ZKP library used for π) |
| labor ador / laboratory / librar | **LaBRADOR** (the succinct lattice proof system inside LaZer) |
| B22 | **LNP22** (the deployed LaZer proof system LaBRADOR was compared against) |
| Susan Zika proof / so singing / sussiness | **succinct ZK proof** / **succinct** / **succinctness** |
| zayga proof / ck proof | **ZK proof** |
| kith hub | **GitHub** |
| the practical exact front late | **"Practical Exact Proof from Lattices"** (Esgin et al.) — the ZK proof Royce could not find an implementation of |
| stock / stocks / start | **STARK** |
| postponed | **post-quantum** |
| eip 7 80 12 25 | **EIP-7825** (the per-transaction gas cap) |
| the new cap | EIP-7825's per-transaction gas cap |
| flax track fry / flabstrap | **abstract** |
| finger / fingers / feasible (at 37:39) | **figure(s)** |
| appetics | **appendix** |
| work count | **word count** |
| critic reflection | **critical reflection** (the Chapter 5 title) |
| youtub (at 39:56) | **Ethereum** |
| fiscal / historical (at 05:18, 39:22) | the **original** (pre-SegWit) transaction structure |
| number theoretic transformations | **NTT** — the precomputation moved off the on-chain path |
| **"717 meal" (08:08)** | a gas figure, **ASR digits unreliable** — the measured one-transaction figure is ≈16.4 M against EIP-7825's 16,777,216 cap (`evidence/onchain_onetx/latest/verdict.txt`). Do not quote the transcript's digits. |
| **"30 minute" (09:12)** | **30 m[illion]** — the Ethereum *block* gas limit, contrasted with the per-transaction cap |
| **"Bitcoin is more widely used" (40:07)** | ASR says *Bitcoin*; the sense of the sentence (DeFi, number of users) is **Ethereum**. Flagged, not certain. |
| "because 7 is 5, that's well" (02:03) | [unclear] — a size, in the context of "is LAS smaller than the limit?" |
| "Fell upon you together" (02:35) | [unclear] |
| "I got a YouTube means" (09:24) | [unclear] — opener to the block-vs-transaction point |
| "I have a cell phone note" (16:32) | [unclear] |
| "hurrah Hu" (20:37) / "happy hot" (12:41) / "no shark, come on this" (13:37) | [unclear] |
| "it fits only on … Dilithium three … not fits on … Dilithium one" (38:44) | **[unclear and hazardous]** — see the ⚠ note under §17. D2/D5 were never evaluated on-chain; do not turn this line into a report claim. |

---

## B. Meeting summary

**What Royce reported.** He closed out last week's clarification on the UTXO structure: what a Bitcoin transaction signs is not the transaction itself but a hash of it, and the signature travels in the **witness** field. On sizes: Bitcoin's script chunk limit is 520 bytes while the LAS signature is ~6.7 KB, so it cannot be pushed as one element — but the binding constraint is block **weight** (4,000,000 weight units), and the LAS spend weighs ~11,000 WU, ≈2.9% of the standard-transaction limit, so it fits comfortably in the improved (SegWit/Taproot) structure. Wang's conclusion in his own words: "we have some space [to] install the large LAS signature." Royce then answered both questions he had raised the previous week: (1) **LAS verification fits in one EVM transaction** under EIP-7825's per-transaction gas cap, after optimisation and after moving NTT-style precomputation off the on-chain path; (2) **a stock Bitcoin node can carry LAS objects**. He also reported the ML-DSA experiment: the adapted signature verifies under an *unmodified* ML-DSA verification function, because once adapted a pre-signature is just an ordinary signature — but PreVerify cannot naively use ML-DSA, because both the high bits and the low bits must carry commitment+Y or verification fails.

**Bitcoin's real problem is verification, not carriage.** Royce ran two Bitcoin verification experiments: one using Bitcoin's classical Schnorr path (carriage only — it verifies nothing about LAS), and one where he modified the Bitcoin client so it can verify LAS directly, which "can generate the exact [measurement], not only the projection". Wang confirmed the implication ("you are modifying the Bitcoin client, right?") and issued the meeting's sharpest instruction: **benchmark it** — "when you modify something … people will always ask, okay, if we achieved this a better security, so what have I lost?" Royce has not benchmarked the patched client. Royce also noted the security of that modification has not been analysed.

**The Meeting-8 ask is accepted.** Wang's verdict on the transaction breakdown was "it's better, definitely… now it's much clearer". One fix remains: the report shows proof sizes as a *range* rather than a fixed number, and must **explain why**, "otherwise people will ask, okay, why?".

**Consistency between conclusions and numbers.** On the succinct-proof comparison, Wang pushed hard on a wording problem: the text says one thing while the reported numbers say another — by the numbers, LaZer/LNP22 is better than LaBRADOR on both proof size and time. "Just make sure the conclusion and the results, they are consistent… otherwise people will easily challenge you." His practical advice for the direction that was not refined: keep it as discussion, without reporting the actual numbers, so no conflicting results appear. Royce's own explanation — that LaBRADOR's succinctness is asymptotic and the statement here is far too small — is the one that belongs in the text.

**Negative results count.** The statement-Y compression experiment failed (Adapt/Ext break on a compressed statement, because extraction needs the exact statement, and each party must derive it independently). Wang: "even it's failed, some negative results are still helpful" — report why it failed, and put it in the critical reflection.

**The central ask: contributions in the introduction.** Most of the second half was Wang pressing on a gap in the report: there is no place where the main contributions are summarised. The report guideline he wrote asks for it; a reader who has read the background, motivation and objectives still cannot answer "what are your contributions?" or "have you achieved the objectives — all of them, or some?" without reading every detail. Concretely: **add a §1.4 "Contributions" subsection to the introduction**, before the dissertation-structure subsection, keeping the existing content, summarising the most important findings in a few sentences. Wang framed it as academic-paper practice (reviewers read the abstract and the first two pages), noted the examiners here will read everything, and said it is a suggestion rather than a rubric requirement — but that it costs nothing and removes a question.

**Report guidance.** Figures: "much better than I thought" — the Meeting-8 figure ruling is satisfied; Royce still wants to fix one legend. Tables: one currently takes a whole page — reduce it to at most half a page. The Bitcoin experiment goes in the body as main results plus a conclusion, with the detail referred out to the appendix. When saying the original Bitcoin transaction structure cannot carry a LAS signature and the improved one can, **cite the Bitcoin improvement solutions** (SegWit/Taproot), because readers may not know they exist. Word count: Royce raised the 9,000-word ceiling and that captions are free; Wang did not add a new constraint.

**On the SHAKE cost.** SHAKE dominates the on-chain gas. Wang asked whether another hash function would reduce it; Royce replied he had not touched that black box and could try another library. Wang's own answer bounded the work: there may be other issues (security, guarantees), and "we have one [working] version — even if it's not very efficient, it's still acceptable." Not a directive to change the hash.

**Next steps.** Benchmark the modified Bitcoin client; add the contributions subsection; fix the proof-size-range explanation and the table size; make the conclusions consistent with the numbers; **prepare the 6–8 minute presentation and give it live next week for feedback**.

**Next meeting:** next week — with the mock presentation.

---

## C. Cleaned transcript

### 1. What is actually signed: the witness carries the signature, and it signs a hash of the transaction (00:02–01:36)

**Royce (00:02)**
Yeah, I believe that I've made some progress this week. I'm fixing the — during [i.e. following up on] the clarification that you asked, probably last week, if I'm not mistaken: how the UTXO structure [works]. It seems that they have kind of not [a] simple message; I guess they have the structure like — [they] have inputs, and they have the — [the pre-signature] is, I think, it's [put] in [the] witness. And yeah, and [the] witness, if I'm not mistaken, is — it has [a] hash of the message, or [a] hash of the transaction.

**Wang (00:52)**
Yeah, the witness should be secret, of course — they cannot put it on-chain. So basically the signature, and also the corresponding statement, right, where the — on-chain. Okay, yeah. [unclear], you mean in the [inner] transaction? They have a field on what they call a witness, right?

**Wang (01:14)**
But inside the witness, we [would] put the signature.

**Royce (01:18)**
You said it's — sorry, good. Okay. Then I think what it signs, it's not the transaction itself.

**Royce (01:25)**
But just the hash of the transaction, I guess.

**Wang (01:28)**
Uh, okay. Yeah, okay — so, so do they have any size limit for this specific field?

**Royce (01:36)**
I think they do have specific limits. I believe so.

---

### 2. Bitcoin's 520-byte script chunk limit vs the 6.7 KB LAS signature (01:55–03:40)

**Royce (01:55)**
This is Ethereum and—

**Wang (02:03)**
[I'm] asking because I'm not sure if the size for LAS is less than the limit. So [I] just want to make sure, because [unclear — "7 is 5"].

**Royce (02:15)**
Yeah.

**Royce (02:28)**
Let me see if [I have] the limit. I think it's about 500-something. If not, most—

**Wang (02:35)**
[unclear]

**Royce (02:36)**
It's 500 MB.

**Wang (02:38)**
Megabytes? It's so large.

**Royce (02:42)**
Um, sorry. I — [let me] see if it's somewhere. 520 **bytes**.

**Wang (03:00)**
Okay. It's 520 bytes, okay. [unclear]

**Royce (03:05)**
[The] Bitcoin script [chunk limit is 520 bytes] — and the LAS signature is 6,000[-odd] bytes.

**Wang (03:11)**
So which means we cannot put it into it directly.

**Royce (03:19)**
Yes, I don't believe so.

**Wang (03:21)**
Okay. So — any solutions?

**Royce (03:35)**
But I think I [have] fixed it somehow, [I'm] not sure how [to explain] it.

**Wang (03:40)**
Okay, yeah, [unclear].

*[ASR dropout ≈03:40–04:10]*

---

### 3. The binding limit is block weight, not element size — and the LAS spend fits (04:10–06:07)

**Royce (04:10)**
Well, I fixed the Ethereum part, but I think for the [Bitcoin/witness part]—

**Royce (04:31)**
The limit — [it's] 4,[000],000 W[U], I don't know. I'm not quite familiar with it—

**Wang (04:46)**
[unclear]

**Royce (04:48)**
I believe it's—

**Royce (05:07)**
[It's in] weight units. Blocks are capped [at a] weight of 4 million weight units.

**Wang (05:18)**
Okay, that's the improvement over the [original] Bitcoin transaction.

**Royce (05:29)**
Yeah — and [our] settlement weighs 11,000 [WU], against [the limit], 2.9 [%]. So it's [not] huge, [is] my guess. Yeah, it's — yeah, it should be inside the limit.

**Wang (05:48)**
Even by using a new Bitcoin transaction structure.

**Royce (05:52)**
I believe so, okay. Yeah. But I think the only problem with the Bitcoin is the **verification**.

**Wang (06:07)**
Yeah.

---

### 4. Bitcoin's real problem is verification; the patched client is not security-analysed (06:08–07:14)

**Royce (06:08)**
I mean, it's — I try to use it like the **Schnorr** verification, and it works. But yeah, I made some modification[s], like using the — uh, the [post-quantum] verification. And but, yeah, I mean — the security has **not** been analysed for that. Probably.

**Wang (06:32)**
But yeah, it's [unclear], yeah, it's more like—

**Royce (06:39)**
Yeah.

**Wang (06:39)**
Okay, but anyway, so the conclusion is that we can use the new version — not the new version, but today you have this **improved version of Bitcoin transaction**. Yeah, so by using that, it means that, okay, we have some space [to] install the large LAS signature.

**Royce (06:56)**
Yeah. And then—

**Wang (07:07)**
Okay, that's for the — for the structure of [the] real transaction.

---

### 5. Question 1 answered: LAS verification fits in one EVM transaction (07:14–08:15)

**Royce (07:14)**
I think last week — also, like, there are some questions that came up to my mind. I think it's — the first one, like: can [our LAS] verification fit in one [EVM] transaction?

**Royce (07:28)**
And yeah, yeah — so it's slightly — somehow, the **optimized one does fit**.

**Wang (07:37)**
What's [the] conclusion[s], so we have—

**Royce (07:40)**
I mean, the new cap, I believe, like—

**Wang (07:44)**
Has this been implemented at [Ethereum]? Oh, it was — it is still a proposal?

**Royce (07:51)**
No, no, no — it's, I've already implemented this week, and [it] seems that—

**Wang (07:55)**
I'm asking in practice — in practice, Ethereum: do they use this, or are they using it now?

**Royce (08:04)**
I believe it's the — it is the latest Ethereum that—

**Wang (08:08)**
Okay. So if I just summarise, though — the gas cost, it's [1]7 m[illion]. *(ASR digits unreliable — see §A.)*

**Royce (08:15)**
I believe so, the [unclear].

*[ASR dropout ≈08:15–08:50]*

---

### 6. EIP-7825's per-transaction cap vs the block gas limit (08:50–09:46)

**Royce (08:50)**
I'm not sure if they already implemented this one, but—

**Wang (08:56)**
It's **EIP-7825**, right?

**Royce (09:01)**
Or is it still a proposal? So—

**Wang (09:04)**
But anyway, I mean, even [if] it has not been adopted in practice, [we] can just say that we do have a [prospect of a] solution to mitigate the issues we're [facing].

**Royce (09:12)**
I mean, yeah — I mean, the previous one is like 30 m[illion] or something. Yeah, so if we can [meet] the proposal, I mean we can also [solve] the problem.

**Wang (09:21)**
But it's still very close to—

**Royce (09:23)**
Maybe it's a very good [unclear].

**Wang (09:24)**
[unclear] means that if they use that basically for the total size of the whole **block**, right, rather than just for one **transaction** — yeah, okay. It means that, okay, in this way, if we use this LAS, okay, in a single blo[ck] we can already include the one—

**Royce (09:45)**
Yeah.

**Wang (09:46)**
Yeah, yeah, yeah — it's still acceptable.

---

### 7. Question 2 answered: a stock Bitcoin node carries LAS objects; ML-DSA verify needs no modification (09:51–11:19)

**Royce (09:51)**
Yeah, I mean the — how I can implement it, like, to fit in the [one] transaction is just [to] make some redundant [pre]computation, like number-theoretic transform[s] [NTT]. And yeah, I'm [unclear] — [the] next one is like: **can a stock Bitcoin [node] carry LAS's objects? Yes, it can.** And — oh, I think firstly, like, I experimented with the ML-DSA verification.

**Royce (10:38)**
It seems that it **can verify without modifying any verification function from ML-DSA**.

**Wang (10:49)**
Okay, okay — [ML-DSA]?

**Royce (10:50)**
Because it makes sense, I think, because after the pre-signature is being adapted, it just becomes like [an] ordinary signature. Okay, so there is no difference between — yeah, yeah, yeah — and then apparently I can just use the verify function from ML-DSA, so you don't need to create a [new one].

**Royce (11:14)**
Yeah?

**Wang (11:19)**
Okay, [what's] the next [one]?

---

### 8. The gas breakdown: SHAKE dominates — would another hash function be cheaper? (11:27–14:00)

**Royce (11:27)**
[Only] the next one, I believe, is — oh yeah, this is the after-optimisation, I guess. I think, yeah, **SHAKE** is [what] costs a lot of the — [most] of the gas, I guess, so—

**Wang (11:53)**
Uh, sorry — this is for the, for the EV[M].

**Royce (11:58)**
Yeah, [that's] my guess.

**Wang (11:59)**
Okay, so basically you analyse the gas costs. You [give] the breakdown for [the] gas [consumption].

**Wang (12:08)**
Okay, yeah — for the SHAKE. SHAKE means the hash function. Yeah. Okay, yeah.

**Royce (12:15)**
It makes sense. I mean, if we want to optimise it more, probably — I need to think about how to reduce the SHAKE cost.

**Royce (12:26)**
[I mean] SHAKE is used for the — the—

**Wang (12:31)**
Where did we use [SHAKE] in LAS?

**Royce (12:35)**
I think to generate the signature, like the commitment, I guess.

**Wang (12:41)**
Okay, [unclear].

**Royce (12:43)**
I think we [commit] a message and a statement Y, for the **challenge generation**. It's—

**Wang (12:53)**
Okay? Next. But it's basically a hash function, right?

**Wang (13:00)**
It means that we should use another version of hash function. But [whether] — I don't know — [it] improved the — sorry, reduced the gas costs, I don't know.

**Wang (13:13)**
[unclear], so I was thinking: if we try to use another hash function, can we reduce the gas costs here?

**Royce (13:23)**
Probably. But yeah, I didn't touch any — the black box, probably. I can probably use another library that people [say] it's more [efficient].

**Wang (13:37)**
Yeah, you can just [note] one option here — you will wait for others and you could [check] for others. There might be other issues, for example, the security, or the [difficulty] of guaranteeing the security. I mean, [unclear] — I mean, **we have one [working] version; even if it's not very efficient, it's still acceptable.**

**Wang (14:00)**
Okay, what's next?

---

### 9. Two Bitcoin verification experiments — and the ruling to benchmark the patched client (14:04–15:45)

**Royce (14:04)**
Yeah, I think I experimented with 2 verification[s]. One is just using the Bitcoin classical **Schnorr** — it's not LAS verify, it's just [carriage]. But it's apparently that some modification can be made, like for the **LAS verification**, and yeah, it can, like, generate the **exact [measurement], not only the projection**.

**Royce (14:38)**
Yeah.

**Wang (14:40)**
But you know, that's — what you mean is that you are **modifying the Bitcoin client**, right? Yes, okay.

**Royce (14:48)**
So I [took] the code and then [made] some modification.

**Wang (14:53)**
Okay, okay, okay — so what costs will you have? [unclear — asking what the modification costs in speed].

**Royce (15:08)**
I think it's — I think it should be slow[er], but I believe — but **I haven't benchmarked**, I haven't benchmarked. I mean, but I just — it's feasible, [it's a matter of] making some changes to the Bitcoin [code] line, yeah, to benchmark.

**Wang (15:32)**
⚠ **Benchmark it** — because when you modify something [in a] simple [way], [people] will always ask: okay, if we achieved this — a better security — **so what have I lost?** All kinds of places where I lose, yeah.

---

### 10. The transaction breakdown is accepted — but explain why the proof size is a range (15:46–17:15)

**Royce (15:46)**
But is it the table that you are — like the breakdown that you are expecting from last week? Because I think last week you were not happy with this. I mean, it's like it's not—

**Wang (15:57)**
Yeah, **it's better, definitely, it's better**. So now we have the transaction, right? And we have the — yeah, messages, okay. Yeah, kind of makes [sense].

**Wang (16:08)**
I [wonder], you know, why we don't have a fixed number — [the numbers are] floating for the sizes, like this—

**Royce (16:16)**
The proof — yeah, I'm not sure how **LaZer** works, but it's just kind of like [a] range, depends on [the instance]. I'm not sure why. It's okay — our results, I need to, yeah, I think—

**Wang (16:32)**
[unclear]. Do you expect—

**Royce (16:34)**
Explain why it's, it's—

**Wang (16:35)**
⚠ [Explain it] in [the text], yeah — otherwise people will ask, okay, **why? Yeah, it's not a fixed number.**

**Royce (16:41)**
And, um. And then this is probably kind of the — the structure of the transaction, I guess, we have.

**Wang (16:56)**
Okay, yeah, yeah, yeah, yeah — no, you [unclear], makes sense. So it — is okay, [the] witness.

**Royce (17:03)**
The size is the huge one, I believe.

**Wang (17:08)**
Okay, okay — **now it's much clearer, much clear[er].**

---

### 11. The ML-DSA experiment: high bits *and* low bits must carry commitment + Y (17:15–18:29)

**Royce (17:15)**
And then for the next part is — yeah, this is the **ML-DSA experiment** that I made, like, for the PreVerify. It's just — [you] can't naively use the ML-DSA, because I—

**Wang (17:35)**
[unclear]. And also, what [is] the difference between [that] verification and [this] verification?

**Royce (17:44)**
I believe that they have **low bits and high bits**.

**Wang (17:49)**
Okay.

**Royce (17:51)**
So if the high bits is commitment plus statement Y, it means that the low bits [need to be] a commitment plus Y [as well] — [you] cannot [do] probably the high bit only, so **both of them must [be] adapted**, like, on the low [bits] and the high [bits]; otherwise the verify fails, I believe.

---

### 12. Statement Y cannot be compressed — a negative result worth reporting (18:29–21:29)

**Royce (18:29)**
For the next part — and I believe that **the statement Y cannot be reduced**, because for the — for the [Ext]ract, they have to [use the] exact same statement—

**Wang (18:56)**
Well, I mean, [it] cannot be re[used]? You know, you mean that after one transaction? Yeah, after one **atomic swap**.

**Wang (19:05)**
We have to create another statement.

**Royce (19:10)**
Oh no, I mean — because they have kind of optimisation that, I think, the last time it's the future work: to reduce the statement Y. Because I think statement Y is pretty huge, and apparently, from the experiment that I did, **statement Y cannot be reduced**.

**Royce (19:40)**
I think — I believe it's because [Ext] must use the exact statement Y, like they have to — I believe for the optimised ML-DSA [it] doesn't store the statement Y, it's just like the seed. And I believe they have like some kind of — they have to derive the statement by each party.

**Royce (20:09)**
You have to derive [the] statement independent[ly].

**Wang (20:17)**
So [one] cannot do any improvement here, right?

**Royce (20:24)**
I mean, but the **signature** can be reduced, I believe so.

**Wang (20:37)**
Okay, yeah — but anyway, I think this is more for future work, [unclear].

**Royce (20:45)**
But yeah, I'm just testing that if a statement Y can be compressed inside the construction — and it fails.

**Wang (20:57)**
How — how did you compress it? How did you try to compress?

**Royce (21:02)**
Just use the ML-DSA.

**Wang (21:04)**
Okay — [the] high bit and low bit.

**Royce (21:09)**
It seems that the **Adapt/Ext**raction fails using the—

**Wang (21:14)**
The compressed statement.

**Royce (21:16)**
Okay, [yes].

**Wang (21:17)**
⚠ Okay, yeah — you can add [it], [it's] very good as [a] little result. **Even if it's failed, some negative results are still helpful.** Okay.

**Wang (21:29)**
Yeah, um.

---

### 13. LaBRADOR vs LaZer/LNP22: the conclusion must match the numbers you report (21:40–27:45)

**Royce (21:40)**
I am also — I mean, [in] LaZer, I think they have like a **succinct ZK proof**, okay, called **LaBRADOR**, I believe. And it seems that it's not something — I mean, it doesn't reduce the proof. And — oh, I mean, the proof is smaller, I believe, but the prov[ing] and the verification timing is higher.

**Wang (22:19)**
[Are they] post-[quantum]? The first of the 2—

**Royce (22:26)**
This one and this one is post-quantum. I believe that 3 of them is post-[quantum].

**Wang (22:32)**
[unclear] — STARK. All right, STARK[s]—

**Royce (22:34)**
Yeah, but STARK is the one that I use[d]. It's not—

**Wang (22:38)**
And what does it — it is STARK. Are they — **it's not [zero-knowledge]**, okay?

**Wang (22:46)**
I don't know, for this it's okay. I'm not [unclear], okay — the proof size. Oh wait.

**Wang (22:54)**
[Because] it's not [ZK]. That's why it's larger, right? The size—

**Royce (23:00)**
Yeah, since the size is [large], in kilobytes.

**Wang (23:05)**
Okay, [and] the prov[ing] and the verif[ication] time is much — wait, wait, wait, wait, wait.

**Royce (23:12)**
I believe it's the proof, it's—

**Wang (23:15)**
What — why — you should check this. Maybe, I mean, it's a[nother thing] — [the other] perform[s] better, [does it]? Right?

**Royce (23:22)**
Yeah, and **LNP22** is a LaZer one — is, the timing is—

**Wang (23:28)**
The timing is good, and it was — the pro[of size] is also better.

**Royce (23:33)**
Yes, so yeah. The current solution? I'm not sure if I checked the — the **ZK proof that's proposed by the author[s]**, but I can't find it, like, [on] GitHub.

**Wang (23:48)**
Which one — this one?

**Royce (23:51)**
No, no — I think they have propos[ed] their own ZK proof, but I didn't find [an implementation]. I believe this is the **"Practical Exact Proof from Lattices"**. This is—

**Wang (24:07)**
Yeah, I don't think it's the — let's say, so [they're] all the powerful results; you can say that was probably 6 years ago.

**Royce (24:19)**
I mean, they believe that they can — the proof is just only for these 7 KB per—

**Wang (24:26)**
I mean, the chance to do the same estimation — wait, Google it. [Let me] look at [where] this paper was published, in which [venue]. I mean, just a Google [search]. Okay, [unclear]. It's not bad. But okay, I'm not [sure], honest[ly].

**Wang (24:54)**
Okay. Okay. It's fine.

**Wang (24:55)**
Yeah, I know where [it was] published. Okay. [Let's] go back to the table we were discussing.

**Wang (25:08)**
But for these paper[s], yeah — which paper are you referring to for this, the first one?

**Royce (25:14)**
It's, I think — I [used] LaZer, I think.

**Royce (25:28)**
I believe probably this, so—

**Wang (25:54)**
Yeah, this [one], right. But you were saying it was a — sorry — **LaZer**. But what's the third one?

**Royce (26:06)**
The third one is **LaBRADOR**, I guess. It's — I mean, it's under LaZer. But it's **succinct**, I guess.

**Wang (26:17)**
Which one is better? LaZer [or] LaBRADOR — [is] it better, LaBRADOR?

**Royce (26:27)**
The number is — it seems **LaZer is better**. Like, only 30,000 [bytes]; this is like 100,000.

**Wang (26:39)**
⚠ Because — yeah, I'm asking because if you would like to report that in your [thesis], you should **make sure that they are correct**. Otherwise people will be confused, because here you say that this [is better], while it's not — but according to the numbers you reported, we can definitely say that the first one is better.

**Wang (27:03)**
⚠ Yeah — just **make sure the conclusion and the results, they are consistent**. Okay. Otherwise, people will easily challenge you.

**Royce (27:11)**
Yeah, and I'm not sure — I tried to figure out why it says that, because **the succinctness is asymptotic**.

**Wang (27:20)**
I mean, just — just [make] sure, okay, [for] the final [version]. I mean, I was saying that **you don't need to create some conflicting results in your report**. Okay, just — use the [one] that you have already implemented; that's [why], right, the final one.

**Royce (27:41)**
I tried this too, but probably just the—

**Wang (27:45)**
⚠ Yeah, just — [the one] you [didn't] refine — then you can **add in some discussion in your report without reporting the actual numbers**. Oh okay. Okay.

---

### 14. Where does the Bitcoin experiment go — body or appendix? (28:01–30:12)

**Royce (28:01)**
And—

*[ASR dropout ≈28:01–28:49]*

**Royce (28:49)**
I think that's probably almost all of them, but I think the only problem is probably the — I believe that it's Bitcoin verification that has not been — [that] I haven't [benchmarked/finished].

**Wang (29:07)**
[unclear — "the verification"].

*[ASR dropout ≈29:07–29:39]*

**Royce (29:39)**
[unclear]. I'm not sure if I should put the Bitcoin experiment in the report, or just put it in the **appendix**.

**Wang (29:51)**
Maybe you [decide] next week. [unclear] — I don't know, up to you, I—

**Royce (29:56)**
It's not the main—

**Wang (29:58)**
⚠ Yeah, it's probably important — but you can **add some main results, main conclusion here, and then you can refer the [reader] to the appendix**.

**Royce (30:08)**
Yeah, it's not this Bitcoin, so—

**Wang (30:12)**
But in general, you have all results, almost all results, right?

---

### 15. Proof size is off-chain and acceptable; report the failure in the critical reflection (30:18–31:32)

**Royce (30:18)**
I believe so, because [I've] almost done all the — almost the future work.

**Wang (30:30)**
And you have already implemented them all in [the] report.

**Royce (30:35)**
Yeah, okay. And yeah, I just felt that to reduce the proof size — I'm not sure how to. But I mean the proof size, it's okay, probably, because it's **off-chain**.

**Wang (30:48)**
[The] proof size is okay, yeah. The [thing] we would try to optimise is the **on-chain** [part], to reduce the [transaction] costs. But again, okay — since you have [made the] trial, you just **report why you failed**; [even] that [is] very helpful.

**Wang (31:09)**
⚠ [Then you] add something in the **critical reflection**.

**Royce (31:14)**
Yeah, yeah — this is the optimised one. It's almost like — yeah, I need to probably confirm again.

**Wang (31:25)**
Okay, anyway, so my suggestion is okay: try to make sure all [the results] are [nailed] down. Okay.

---

### 16. THE ASK: summarise the contributions in the introduction (31:32–37:26)

**Wang (31:32)**
Okay. Yeah, yeah, yeah, yeah — and again, the one final thing is that, [once you have] finished them all: so maybe I would ask, okay, **can you give me a simple [summary]?**

**Wang (31:48)**
⚠ **What's the most important finding — what [are] the most important findings in your report?** [Have] you summarised [them]? I'm saying, you know, **in the introduction**.

**Royce (31:58)**
Oh, [the] introduction is just like some re[view] of, like, the background — or in the abstract—

**Wang (32:07)**
Yeah, abstract. Well, sometimes I also [look for] some contributions—

**Royce (32:13)**
Oh, okay.

**Wang (32:15)**
So this is [the] abstract. Yeah, [the] abstract is kind of — I mean, a little bit [long]. Okay, this is—

**Royce (32:26)**
In the [introduction] — the context, motivation—

**Wang (32:28)**
It's [all] together, okay. Objectives.

**Royce (32:34)**
It probably needs more—

**Wang (32:39)**
⚠ Okay. So **in the guideline of the report I've mentioned that you should also summarise your main contributions in the introduction.**

**Royce (32:50)**
Main contribution.

**Wang (32:51)**
Yeah — so because, for example, when [I read a paper], I would like to say: okay, what [are] the most important contributions of this work? So basically, I mean, that's more for [an] academic paper — for [an] academic paper. But for this, I'm not sure, you could just [follow] the academic [form]. So I was thinking, you know: introduction, abstract. Introductory, okay.

**Royce (33:13)**
[unclear — describes the project structure].

**Wang (33:24)**
Okay, okay — though it's more like an introductory. Okay, where should we summarise the contributions — conclusions? Kind of just a [section on] contribution, or contributions?

**Wang (33:40)**
[So] now they don't have contributions, okay.

**Royce (33:45)**
But this is not rigid. They say, probably, I can [add it]—

**Wang (33:49)**
I mean, it's not too much — maybe it would, you know, [be] safer to justify [doing] the one thing [that] they suggest.

**Royce (33:55)**
So contribution usually goes into which [section]?

**Wang (34:01)**
I would go to — I mean, but that's more for academic paper again. [If] we want to publish a paper — so normally, when I review papers, I would like to first read the abstract, of course, to say it's interesting or not, and then I will read the introduction. **So the introduction [should] tell me what you have already done in this report.**

**Royce (34:27)**
Or in the — is it — [does] it make sense if I put it in the evaluation?

**Wang (34:32)**
Yeah, but it's a[nother matter]. I mean, because for academic papers you should assume that the reviewers, they are very lazy, because they [only] read the first 2 pages — so you should first sell your most important things in the first 2 pages, right. But here, since you can assume that the examiners, they [will] read all things [anyway]—

**Royce (34:55)**
I probably just [add it to the] introdu[ction].

**Wang (34:58)**
Yeah, you kind of add something [to the] introduction. I mean, you of course could keep the existing content, and you kind of add a [subsection] to summarise what you have done, **after your objectives** — after [the] objectives, okay. Go back to your report, so let's go to the introduction.

**Wang (35:18)**
⚠ So here is the — our objectives, right, aims. And then this is the — I thought you could [add] contribut[ions]: yeah, you could add this one section, like **1.4 Contributions**, and then you can summarise the most important findings here, okay. And then you can — you can — you can kind of—

**Royce (35:41)**
Before this "dissertation [structure]"?

**Wang (35:43)**
Before the [dissertation] structure subsection.

**Royce (35:46)**
Okay.

**Wang (35:48)**
And for me, I think it's fine, because you have [the material] like this. But yeah, that's my suggestion, you know — [it's a] suggestion, because, again, [as] you mentioned, when people are [reading] your report, right — yeah, they [read] the background, they [read] the motivation, they see the objectives, okay. They want to say: yeah, **what are your contributions?** Otherwise they have to go through all the details, and [only] finally can they [find] the contributions. [Better to] summarise the contributions.

**Wang (36:17)**
[Have] you already summarised the contributions somewhere? Oh — no, okay. That's the most important thing, right?

**Wang (36:24)**
⚠ For example, people will ask you: **can you give me 3 or 4 sentences to summarise what you've done? Have you already achieved the objectives?** Because here we don't know, right. You just [list] your objectives; we don't know if you have already achieved them all, or just some of them.

**Royce (36:45)**
Oh, I think they have like "project achievement" or something. But I'm not sure, like, where should I put it?

**Wang (36:53)**
I'll probably check [the guidance]. Okay, this is—

**Royce (36:57)**
You know, it's more just for — yeah. Yeah, it's not this one, yeah?

**Wang (37:06)**
It's not [required] by the [rubric], but yeah, I mean — you don't want to cause any [doubt], right? If you add the [contributions section].

---

### 17. Figures are much better; a table must not fill a page (37:26–39:11)

**Royce (37:26)**
For the — I mean, last time you said that my report has not had enough figure[s].

**Wang (37:37)**
I mean, the figure[s] — results—

**Royce (37:39)**
And, like, there's not enough figure[s].

**Wang (37:43)**
Yeah, have you already raised the number of figures? Let me check. I'm not — now.

**Wang (37:48)**
**It's better. I mean, it's much better than I thought.**

**Royce (37:51)**
Okay — and probably this figure, I need to fix the legend still, and then—

**Wang (38:00)**
Yes, it's better. It's better than that.

**Royce (38:03)**
And probably just add some figure about that. But this is still — I mean, I'm not sure how to fit in 9,000 words. My figure explanation is very long, because [the] figure caption is not counted in the **word count**.

**Wang (38:18)**
[unclear] — that'd be useful, okay? Okay?

**Royce (38:21)**
But this, I don't think it's [acceptable], because like this is one p[age] just for the [figure].

**Wang (38:26)**
⚠ Consider [reducing] it to at least [by] 50% — half of the size of the [page].

**Royce (38:33)**
I'm — sure, this is one **table**.

**Wang (38:35)**
But it's too large, alright — if you want [it] all — you're going, you're like, you know, on your table.

**Royce (38:44)**
Oh yeah, for the EVM part [it] would still — it fits only on [Dilithium three]; [it does] not fit on [the other] Dilithium — one of the experiments that [unclear].

> ⚠ **This utterance is garbled and is a known hazard.** On-chain verification is measured **at D3 only**; `LASVerifyOpt`'s parameters are compile-time D3-only, so D2/D5 were **never evaluated** (CLAUDE.md, EVIDENCE-OR-SILENCE; `docs/03-results/GAS_LIMIT_INVESTIGATION.md` §7). Do not convert this line into a report claim that on-chain verification fails at another level — "not evaluated" is the only supportable wording.

**Wang (39:00)**
Okay — [that's] new [scope] to propose more flexible solutions; and for other blockchains, people will try — [they'll be] pressed to implement them or integrate them in practice.

---

### 18. Cite the Bitcoin improvement solutions: the original can't carry LAS, the improved one can (39:11–40:26)

**Royce (39:11)**
And for the Bitcoin [part], I probably have to look in more detail into it — like, is it feasible? All 3 security [levels]?

**Wang (39:22)**
⚠ Yes, one — and [I would] add the correct **citation**, because maybe some people didn't know the existing — sort of — **Bitcoin improvement solutions**. So you should say that for the [original], or the historical — for the original Bitcoin transaction structure, **we cannot include our LAS signature directly; for the improved version, right, [with] SegWit** [we can].

**Royce (39:51)**
Yeah, it's actually—

**Wang (39:53)**
Yeah, we can. We can include it.

**Royce (39:56)**
Yeah. For nowadays — is Bitcoin still largely used compared to [Ethereum]?

**Wang (40:07)**
It depends. If you say the value, of course, because it's always the largest. But if you were to consider the popularity or the number of users, maybe due to DeFi or decentralised [finance], **[Ethereum]** is more widely used in practice. [Whether one] develops depends on which metrics you are talking about. *(ASR says "Bitcoin" here; the sense of the sentence points to Ethereum — see §A.)*

---

### 19. Bitcoin as gold, Ethereum for daily use (40:26–41:27)

**Royce (40:26)**
Hmm — because I think Bitcoin is still based on UTXO, or even—

**Wang (40:31)**
[unclear] — [Bit]coin, it's more like **gold**. Yeah, so people would like to have it as the backup, but in part it's not very convenient to transfer, because first it's very expensive, second it's very slow.

**Wang (40:47)**
So that's why, for example, in practice you will use cash, or you will use [cards]; you [wouldn't] want to use gold in practice for your daily buying — you don't need to use gold to buy something.

**Royce (41:03)**
Yeah — and after every transaction, it seems that it has to generate [a] new kind of—

**Wang (41:09)**
Output, you mean? For — correct? Okay — yeah, correct. Yeah, yes, [that's] mining, right?

**Wang (41:13)**
Yeah, that's what I would call the mining. [That] analysis, that's for the — [deep] inside the mechanism of Bitcoin protocol. Okay, yeah, yeah, **I think that's good for today.**

---

### 20. Next week: mock the 6–8 minute presentation (41:27–42:40)

**Wang (41:27)**
Yeah, let me know, yeah, if you have any questions. And maybe again — maybe next week, or the week after next — [if] you do have preliminary slides, **we can mock the presentation** if you want. Give me — because [unclear] — you can practise it here if you want; just give me—

**Wang (41:50)**
I don't know, [6] to 8 minutes. Yeah, [a] 6[–8 minute] presentation. Yeah, presentation.

**Wang (41:55)**
And I can give you some feedback if you want. Okay, up to you — we can do it next week, or [the week] after that. We [do] have the next week.

**Royce (42:03)**
Okay, I think next week is the best, probably. Okay. Yeah, so I can probably get some feedback from you and improve on it from your feed[back].

**Wang (42:14)**
Okay. Yeah, good. [I'll] see [you] next week.

**Wang (42:17)**
Yeah, okay, let's see. [Post-meeting pleasantries — "most people are cooking, [I] can smell something".]

**Royce (42:33)**
It's [lunch] time here.

**Wang (42:37)**
Yeah, but I still have another meeting.

**Royce (42:40)**
Okay, okay — I think it's [lunch], yeah.

---

## D. Action items arising

| # | Action | Owner | Source |
|---|---|---|---|
| 1 | ⚠ **Benchmark the patched Bitcoin client** (`OP_CHECKLASSIGVERIFY`). Wang's rule: when you modify something for better security, you must show *what it costs*. Royce stated on record that he has not benchmarked it. Keep the existing framing — carriage-only vs the patched node — and keep "the security of the modification has not been analysed". | Royce | §9 |
| 2 | ⚠ **Add a §1.4 "Contributions" subsection to Chapter 1**, after the objectives and **before** the dissertation-structure subsection. Keep the existing content; summarise the most important findings in 3–4 sentences, and say **whether the objectives were achieved — all of them or some**. Wang's own report guideline asks for this; he called it a strong suggestion, not a rubric requirement. | Royce | §16 |
| 3 | ⚠ **Make the conclusions consistent with the reported numbers.** The succinct-proof text reads as if LaBRADOR wins while the reported numbers say LaZer/LNP22 is better on both size and time. State the numbers' verdict, and give the reason (LaBRADOR's succinctness is asymptotic; this statement is far too small). "Otherwise people will easily challenge you." | Royce | §13 |
| 4 | ⚠ For the direction that was **not** refined, keep it as **discussion without reporting the actual numbers**, so no conflicting results appear in the report. | Royce | §13 |
| 5 | ⚠ **Explain why the proof size is a range, not a fixed number** — in the text, so a reader does not have to ask. | Royce | §10 |
| 6 | ⚠ **Bitcoin experiment placement:** main results + a conclusion in the body, detail referred out to the **appendix**. Wang left the final call to Royce, to settle next week. | Royce | §14 |
| 7 | ⚠ **Cite the Bitcoin improvement solutions** (SegWit/Taproot) where the report says the original transaction structure cannot carry a LAS signature and the improved one can — readers may not know they exist. | Royce | §18 |
| 8 | ⚠ **Report the failed statement-Y compression as a negative result**, with *why* it fails, in the **critical reflection**. "Even if it's failed, some negative results are still helpful." | Royce | §12, §15 |
| 9 | ⚠ **Shrink the oversized table to at most half a page.** Figures are otherwise accepted — "much better than I thought"; Royce still owes one legend fix. | Royce | §17 |
| 10 | ⚠ **Prepare and deliver the 6–8 minute presentation live next week** for Wang's feedback. Royce chose next week over the week after. | Royce | §20 |
| 11 | Optional / bounded: investigate whether **another hash function** would cut the SHAKE-dominated on-chain gas. Wang bounded it himself — other issues (security, guarantees) may apply, and the existing working version is already acceptable. **Not** a directive to change the hash. | Royce | §8 |
| 12 | Make sure **all results are nailed down** and confirmed before the final version. | Royce | §15 |

**Nothing in this meeting reopens a frozen scope item.** Wang did not ask for a new experiment, a second signature scheme, or a new venue; every item above is report work plus the one outstanding benchmark.

**Next meeting:** next week — with the mock presentation.
