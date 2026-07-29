# Cleaned Transcript — Meeting 7 with Zhipeng Wang

**Date:** 2026-07-24, 11:31 (Teams).
**Sources (two independent ASR passes over the same meeting, merged):**
1. `meeting7_original_samsung_transcript.md` — phone recording. **Diarised: Speaker 1 = Royce Steven, Speaker 2 = Zhipeng Wang** (same polarity as Meeting 6). Runs 00:15–42:00 but **cuts off mid-sentence** at 42:00 and has short dropouts (≈20:11–20:51, ≈34:05–34:30, ≈41:18–41:36).
2. `meeting7_original_transcript.md` — Microsoft Stream/Teams transcript, 209 caption entries, 00:03–41:34. **Not diarised** (Teams tagged all 209 entries "Royce Steven"), but a *different* ASR engine, so it disambiguates words the phone garbled, and it covers the phone's dropouts.

**How the merge was done:** speaker attribution and timestamps come from the phone transcript (the only diarised source); wording is reconciled against the Stream pass, which is generally the more accurate of the two on technical terms. Where only Stream covers a stretch, that content is included at the Stream timestamp. **Stream timestamps run ≈10–13 s earlier than phone timestamps**; phone timing is used throughout.
**Type:** meaning-preserving cleaned transcript, not a legal/verbatim transcript. Every exchange in either source is represented; nothing is dropped. Genuinely unintelligible fragments are marked `[unclear]`.
**Known gap:** the meeting continued past 42:00. Both recordings end there (Stream stopped at 41:34; the phone cuts off mid-sentence in Wang's answer about the one-week timeline). The final answer is therefore incomplete — see §20.

**Main topics:** pick an existing classical atomic-swap codebase and swap its signature scheme for LAS; full native LAS verification on the EVM is infeasible on gas, so **Stage 2 retargets to Bitcoin/UTXO instead of Solidity**; the **three configurations** to build and benchmark (classical AS + Groth16 / LAS + Groth16 / LAS + LaZer); Bitcoin has no gas limit, so the comparison metrics become time + communication cost; the π ZKP stays off-chain; the rejection-probability figure must be replaced by a cumulative-acceptance plot; packed-tier overhead (up to ≈80 % on Adapt) and "hybrid native" for the classical baseline; report structure — evaluation as its own chapter, critical reflection in Chapter 5; refund/timeout are edge cases, do the happy path first.

---

## A. Key contextual corrections

| ASR / unclear phrase | Corrected meaning |
|---|---|
| last / the last / oz / class / laws | **LAS** (the adaptor-signature scheme, eprint 2020/845) |
| adaptive signature / adaptable signature / adapt signature / a diaper signature / diverse signature / adaptive symmetry / autonomous adapting signature / class adoption lecture | adaptor signature |
| automic swap / automatic swap / autonomic swab / automated this web / auto mix well / automic spread / atomic sum / mix wrap / automaker spread | atomic swap |
| ECT SK / ECDSK / cds / a cds in | ECDSA |
| numpostal content version / non-postal content version / non post content version | **non**-post-quantum (i.e. classical) version |
| postcard one / post part one / poster content version / postal content version / post-income security / postal comp security / post content version | post-quantum version / post-quantum security |
| Monair / Montero / monero / modera / monaro / moner butter / more narrow | Monero |
| group 16 / gruel 60 / Google 60 / growth syst / grow 16 / 6 to 16 / cloud 16 / code 16 / glood 16 / gay piece go sex team | **Groth16** (the classical zk-SNARK) |
| laser / Lacey / lake was in sales | **LaZer** (the lattice ZKP library used for π) |
| laysayer / lay Sierra / delay how to sneak me | **Naysayer** (optimistic-verification proofs) |
| pocket / the paper from pocket / the pocket methods | **poqeth** (eprint 2025/091, the integration template) |
| Spinks / spings | SPHINCS+ |
| fake signatures / the improvement in the basical signatures | the **basic** signatures (i.e. plain, non-adaptor signatures) |
| guess limit / Cas limit / cas limit for 1 block 2 and a 1 | **gas limit** (per block) |
| event / Even / even / amazing virtual machine / Eastern virtual machine | **EVM** (Ethereum Virtual Machine) |
| solidarity / insolidity / LDT | Solidity |
| Bitcoin Tesla | Bitcoin testnet |
| attribute coin network / tag to be the coin | the actual Bitcoin network / testnet Bitcoin |
| utx sotx / other utxo | UTXO(-based chains) |
| pdc / be a coincide / the class trim Bitcoin | BTC / the Bitcoin side / the cross-chain Bitcoin case |
| the honor settlement is 1.1 million / non earnest sweetness | the **honest settlement** ≈ 1.1 M gas / a dishonest witness |
| the hash dispute can cost 30 million cases | the hash-dispute path ≈ 30 M gas |
| over leaf / relief / excellent creek | Overleaf / (TeX locally vs Overleaf) |
| ZKP / zkb / AKP / A.K.P. / akps / ZEP / Z. K. / zayes / CK proof / the GDP that I implement | ZKP (zero-knowledge proof); "the ZKP I implemented" |
| Heavy texture, Sam 3 | rejection sampling |
| geometric model / around 36.8 | geometric model; ≈36.8 % acceptance probability (≈1/e) |
| thirtical results / the thirtical results winning | theoretical results |
| the probability of the exact K entrance | the probability of **exactly k attempts** |
| Coyotes, soap / SoeHub / soe / SE / Kay | filler / "okay" [unclear] |
| country / countertory / contradictory | counter-intuitive |
| the radar slide / the radar is not a user | make it clearer for the **reader** |
| back tier / pecked here | **packed tier** (the with-serialization timing tier) |
| hybrid Nat | **hybrid native** (the classical baseline's mixed native-API tier) |
| ecoding encoding / decoding and coding | encoding/decoding, i.e. packing/unpacking |
| the tea generation | key generation |
| RAS / Russ / rasp / rusty | Rust |
| C. / sea / save / say | C |
| Cava's / convas / canvas | Canvas (the university VLE) |
| fc report video report book of Bloom / MSC report video | the MSc Report and Video rubric/handbook |
| champion 3 / chapter 50 | Chapter 3 / Chapter 5 |
| convers / convolutions / consolidation | conclusion |
| the samurai / critical Tea couple ray | the summary / critical reflection |
| Refaluation and auto reflection conclusion | "Evaluation, and reflection in the conclusion" |
| age cases | edge cases |
| type out / time or test | timeout |
| prey / Parasion / Preparation duration | preparation |
| further stay / further state | a later stage |
| Democrat / radio deployed | a demo / "if you have this ready you can deploy it" |
| day by state / today by day | step by step |
| philosoph | [unclear] — from context, LAS (migrating the scheme from ECDSA to LAS) |
| for years we ever avoid inertia / legal mutations | [unclear] — from context, "let's avoid the EVM for now" / "many implementations" |

---

## B. Meeting summary

**The big decision of this meeting: Stage 2 moves off the EVM and onto Bitcoin/UTXO.** Royce reported that full native LAS verification in Solidity is prohibitively expensive; Wang's reaction was that on-chain LAS verification "is a bit impossible" against the per-block gas limit, and that it is well known that adaptor signatures are used for atomic swaps on Bitcoin and other UTXO-based chains **rather than** on smart-contract chains. Bitcoin has no gas limit — only transaction fees — and heavy computation happens off-chain, so a Bitcoin-side demo cannot be attacked as infeasible. The EVM path is deferred to "if we have time", with a preparatory literature check on how classical adaptor signatures / atomic swaps are implemented in Solidity so that a future PQ-on-EVM measurement has a baseline.

**The concrete Stage-2 plan.** Take an existing, actively-maintained classical atomic-swap repository (Royce had found one; Wang checked its commit activity and approved it) and reuse its architecture, replacing the signature scheme. Wang explicitly separated the two substitutions: **first replace the signatures** (ECDSA → LAS, already implemented), and only afterwards, if there is time, replace the ZKP (Groth16 → a post-quantum ZKP). That yields **three configurations to build and benchmark**:
1. the classical one — classical adaptor signature + Groth16;
2. LAS + Groth16;
3. LAS + a post-quantum ZKP (LaZer).

Wang wants each evaluated for performance, "step by step", so that a working Bitcoin atomic-swap application exists before any EVM work starts. Because Bitcoin has no gas metric, the comparison axes become **time and communication cost** — including off-chain components, since each protocol message costs both bandwidth and local computation (the point that a user might not be able to do the preparation on a phone and would need a dedicated PC, which is a usability finding worth reporting).

**Figures and modelling.** Wang could not read Royce's rejection-sampling figure: it plotted the probability of *exactly* k attempts, so the curve is highest at k = 1 and decays, which looks backwards to a reader who expects "more attempts → higher success". After Royce explained it (most signatures are accepted on the first two or three attempts, so needing 15 is very unlikely), Wang accepted the maths but ruled the presentation must change: **plot cumulative probability of acceptance against attempts 1–15**, which rises from ≈35 % and flattens towards 100 %. He noted the flattening is itself the message. Royce also reported the geometric approximation ≈36.8 % and that larger runs converge to it. Wang asked to read the details of the norm-change-by-one argument himself, and asked for the Overleaf project to be shared with his Manchester email address once a reasonably complete version exists.

**Packing and the classical baseline.** Royce showed the packed-tier overhead — encoding/decoding increases cost significantly, up to ≈80 % on Adapt — and explained that the classical library cannot be measured the same way because its API never exposes unpacked keys except at key generation, hence the "hybrid native" label. Wang accepted this and confirmed the classical packing cannot be removed from an existing API. Whether the swap demo itself uses packing is Royce's call: if it is not efficient enough, ignore it and record it as a limitation in the critical reflection — this is an exploration/demo, not a product.

**Report structure.** Wang ruled that **evaluation should be its own chapter**, kept separate from methodology (readers should not have to understand the method before seeing results), and that **critical reflection belongs in Chapter 5** alongside the conclusion, as at least its own subsection: what was achieved (theory + evaluation), what failed, and what would be done differently given another chance. A short reflective paragraph within the evaluation chapter is acceptable, but the substantive reflection goes in Chapter 5. Royce noted the course document does not mandate this structure; Wang's preference stands. The conclusion being about two pages is fine.

**Other rulings.** The π proof (implemented over LaZer) is off-chain — the two parties can be assumed to share a secure channel before the exchange, so it need not be on-chain. Calling LaZer's C from Rust is acceptable; get a working version first and optimise later. The simulation does not need real network sockets/ports — assume messages can be passed directly, and note that a fuller simulation with two ports is future work. **Refund and timeout paths are edge cases**: implement the honest/normal case first, on the understanding that a dishonest counterparty must not cause the honest user to lose funds. Royce should also benchmark the classical adaptor repo, noting that some demos stop at Adapt and never implement Ext (Wang: extraction is always fast, it is just the final step). Wang closed by asking for results; asked whether one week was feasible, he said the three steps were doable but **[the answer is cut off mid-sentence in both recordings]**.

---

## C. Cleaned transcript

### 1. Which codebase to migrate? (00:15–03:30)

**Royce (00:15)**
And — so on the atomic swap, for the migration: is it like I migrate someone's Bitcoin or wallet [implementation]?

**Wang (00:33)**
So what's your question?

**Royce (00:34)**
I think I found a Monero or Bitcoin wallet. So do you mean by migration that I use their architecture and then change from ECDSA to LAS?

**Wang (00:54)**
Maybe you could try — I'm not sure that's the most efficient way. But have you found any adaptor-signature implementation using the **non**-post-quantum version?

**Royce (01:16)**
I believe there are. I think this one is using atomic swap on Monero–Bitcoin.

**Wang (01:21)**
Okay.

**Royce (01:23)**
I believe they also have an app.

**Wang (01:27)**
But do we have — I mean, for Monero it's complicated, because Monero also provides the privacy-preserving features, and I think here we don't consider that. Ideally you could find two chains that are very similar to each other [to work with], you know what I mean? So you don't even need to consider Monero. Maybe you can consider Monero–Bitcoin, or Bitcoin and another chain which is very similar to Bitcoin.

**Wang (01:56)**
I mean, the simpler the better. I don't want to — of course you can try Monero, but they have other features; it's more complicated.

**Royce (02:07)**
More complicated, okay. So is this the one I should look for, probably? Because it's the one I'd use for the classical comparison.

**Wang (02:17)**
But this is for — let me check. This is for multi-signature, right?

**Royce (02:22)**
I believe they also have an ECDSA adaptor signature.

**Wang (02:27)**
Or is it [unclear]? What is an adaptor signature [in their terms]?

**Royce (02:29)**
Yeah — they have it as experimental.

**Wang (02:32)**
Oh, okay, yeah, you can try this one as well. Just replace this one with the post-quantum version you have already done. But—

**Royce (02:41)**
But this one isn't already an application, right? It's just—

**Wang (02:44)**
I mean, it's fine, it's a demo. Of course, if you have this ready you can deploy it. I think that's for a later stage, because I don't know if we can do it on Bitcoin testnet. Of course you shouldn't deploy it on the actual Bitcoin network, because it's very expensive to deploy things there — you'd have to issue transactions, and you'd have to pay the fees, which means you'd have to use testnet Bitcoin.

**Wang (03:11)**
Okay — you don't need to do that. So let's do this; maybe this could be the one.

**Wang (03:16)**
I'm not sure whether — are they updating the code? Yes, they are updating it, which means they are maintaining it. This is good.

### 2. The plan: replace the signatures first, the ZKP later (03:28–04:35)

**Wang (03:28)**
Okay, so two things. One is that you can use this one to develop your code by replacing the signatures here — from the classical one to the post-quantum one, which means the one you have already implemented. And the second thing is: if they are using ZKP stuff, let's just assume we don't touch the ZKP stuff for now. We first replace the signatures. And then if you have time, you can also replace the Groth16 with a post-quantum-version ZKP — though of course I think it's less efficient. But I do believe we have some post-quantum ZKPs.

**Royce (04:17)**
Also, last time I was trying to use a ZK proof — LaZer.

**Wang (04:21)**
Yeah, LaZer — that's the one that was proposed.

**Royce (04:26)**
It seems like it works.

**Wang (04:28)**
Oh, very good.

### 3. Should it be implemented in Solidity? (04:36–06:01)

**Royce (04:36)**
And do I need to implement it in Solidity?

**Wang (04:39)**
That's for the next stage, but you can. In Solidity it means we can use smart contracts to do that — and for smart contracts we may have other solutions besides the adaptor signature. For the adaptor signature we can make sure that for Bitcoin — the cross-chain case for Bitcoin — maybe, yeah, you can also try the bridges between Bitcoin and Ethereum. But I think that's for the next stage. Let's first focus on this.

**Wang (05:14)**
If you have time, we could explore whether we need to implement it in Solidity — of course you can also do that. But personally I think that because smart contracts are more powerful than Bitcoin — in terms of features, you can provide more features — that's why in practice… I'm not sure whether some people have already implemented adaptor signatures in Solidity. Maybe you can search for it and see whether people are using that or not.

**Wang (05:45)**
So normally, what I understand is that people prefer to use adaptor signatures for atomic swaps related to Bitcoin or other UTXO-based chains, rather than smart-contract-based chains.

**Royce**
Okay, okay.

**Wang**
But let's do it step by step.

### 4. Full native LAS verification on the EVM: gas makes it infeasible (06:01–10:32)

**Wang (06:01)**
I think what we discussed is very challenging. So I was wondering — you said you tried to use the LaZer stuff. Interesting. How does it work?

**Royce (06:15)**
It works. And then I tried to make full native verification, and it seems like the gas cost is really high.

**Wang (06:29)**
But you are doing it on the EVM, right?

**Royce (06:33)**
Yeah, EVM.

**Wang (06:34)**
Okay, which means you are using Solidity to implement it. Interesting. I mean, this is a bit impossible.

**Wang (06:47)**
You know why? Because for one block — in Ethereum, for one block, we have the gas limit. The gas limit for one block. Do you know how much it is?

**Royce (07:05)**
I'm looking for the latest number. If I'm not mistaken, this one I guess is—

**Wang (07:11)**
Yeah, something like that. If I remember correctly, the previous one was like 30 million, and now they've changed it to a larger number. So it means, okay, this is not feasible. But for this one — have you used Groth16?

**Royce**
No, I've not tried.

**Wang**
Okay, sorry — this is not for atomic swap, right? Is this for LAS or not? I'm not sure.

**Royce (07:36)**
This is on-chain verification during the atomic swap.

**Wang (07:39)**
Oh, okay — on-chain verification during the swap.

**Royce (07:43)**
Because LAS itself happens off-chain, so the difficult part is the on-chain verification.

**Wang (07:52)**
Okay. Maybe you can try Groth16 first. With Groth16 you can add another bar here — like "LAS with Groth16" — and check whether the verification is feasible or not.

**Royce (08:07)**
I think if you use Groth16 it's going to be much smaller.

**Wang (08:10)**
Yeah, yeah, of course, but I'd like to see it.

**Royce (08:13)**
And I'm also trying the poqeth methods — the paper from poqeth, which [unclear] — they use Naysayer, like optimistic verification.

**Royce (08:35)**
I think this is the paper you sent me last time.

**Wang (08:39)**
Right, right, yes. But have they implemented the adaptor signature?

**Royce (08:49)**
Briefly — no, they do not. They just implemented the basic signatures, I think.

**Wang (08:52)**
That's the improvement in the basic signatures.

**Royce (08:55)**
I think on SPHINCS+ or May[?].

**Wang (08:59)**
I think they only consider the hash-based signature schemes. Yeah — you can also try to adapt their solution. I mean, they have the — how do you spell it — Naysayer.

**Wang (09:24)**
For this, maybe in this way we can reduce the verification cost a bit.

**Royce (09:31)**
Yeah, but it's still huge if I'm not mistaken. I think I should have—

**Wang (09:36)**
Right, okay, interesting.

**Royce (09:41)**
I think it's expensive.

**Royce (09:59)**
Oh yes — for the honest settlement it's 1.1 million, but if someone makes a dishonest witness—

**Wang (10:14)**
Okay, this number is fine, right? It's less than this one. Okay.

**Royce (10:21)**
But the hash-dispute path can probably cost 30 million gas, if someone uses their own public key or something.

### 5. Are there classical implementations in Solidity? (10:32–11:36)

**Wang (10:32)**
Okay. I was wondering — this is for the post-quantum version, right? Have you found any implementation of the **non**-post-quantum version in Solidity?

**Royce (10:46)**
I believe they have.

**Wang (10:47)**
Yeah, I'd really like to see how much their cost is. Then we'd have a baseline, I think.

**Royce (10:57)**
It's quite small — I think I found it. It's pretty small, because they have pre-compiled verification already.

**Wang (11:09)**
But have they introduced the ZKPs in the implementation?

**Royce (11:14)**
Probably not everyone implements a ZKP.

**Wang (11:18)**
But then how can they achieve the construction of the atomic swap?

**Royce (11:23)**
Oh — the atomic swap?

**Wang (11:26)**
So are you talking about an atomic swap, or just LAS?

**Royce (11:32)**
The atomic swap, yeah.

### 6. Decision: build it for Bitcoin, not Solidity (11:36–13:52)

**Wang (11:36)**
Okay, so let's do this. For the atomic swap we can choose to implement it in Solidity or not, right? So let's do the implementation **without** involving Solidity — because in Solidity, as I said, we have some limitations, we have the gas costs.

**Wang (12:01)**
So let's try the implementation for Bitcoin — the cross-chain Bitcoin case. Okay, this is for this one.

**Royce (12:12)**
Oh yeah, this one — they have—

**Wang (12:14)**
But I don't think they have already implemented that in practice.

**Royce (12:20)**
Yeah, I think there is one GitHub repo that does the pre-compile, but it doesn't have many stars — probably only seven stars.

**Wang (12:28)**
Strictly speaking, I think that's for a later stage.

**Royce (12:30)**
But if I want to do this, it means I have to convert LAS into the full [unclear].

**Wang (12:35)**
It's very complicated. There's a lot of engineering work you'd have to do. So as I said, let's touch the EVM later. Let's first focus on the Bitcoin side, to make sure that we have a feasible implementation of an adaptor signature in practice — an adaptor signature for an atomic swap in practice — because as you said, you have tried some implementation using Solidity, and it seems they are very expensive.

**Wang (13:12)**
So as I said, I think adaptor signatures are very useful for atomic swaps related to Bitcoin transactions, or cross-chain transactions between Bitcoin and other blockchains. So let's try this one — because in this way, maybe it's still very slow, but we don't have a limit, and people won't argue that your solution is not feasible in practice. If you want to achieve post-quantum security, you have to lose something — you lose performance — but that's fine.

**Wang (13:48)**
It depends on the choices of users.

### 7. The three configurations to build and benchmark (13:52–16:35)

**Royce (13:52)**
Yeah — the only thing I haven't tried is the post-quantum ZKP.

**Wang (13:59)**
Yeah, so—

**Royce (14:01)**
…instead of Groth16.

**Wang (14:02)**
Yeah, so let's try to do this. Let me write something here, if you can [see my screen]. So let's try the atomic swap for BTC and also another chain — another UTXO-based chain.

**Wang (14:22)**
I don't know other chains well, so maybe you can build this based on the repo you shared with me. So we will build three things.

**Wang (14:30)**
The first is the classical one. So the classical one — I guess they will also use some ZKPs, right? Okay, maybe this should be Groth16.

**Wang (14:43)**
Okay, done. You will evaluate their performance. Then let's try the second one.

**Wang (14:52)**
[The first] is the classical AS. And let's try LAS, and also the ZKP Groth16. Okay, let's see.

**Wang (15:03)**
…what performance they will have. And then finally, we will use LAS — the post-quantum one — with [post-quantum] ZKPs, right? Then maybe this should be LaZer.

**Wang (15:17)**
Something like this, and then compare the performance. Okay — and then we will have the first product, or the first application, ready. I mean for Bitcoin. And then if we have time — and I believe we will have time — we will try the EVM and Solidity.

**Wang (15:37)**
Let's do it step by step, because this way you can make sure that even if we don't do anything related to the EVM, we have a feasible application ready. That's for Bitcoin — at least people can use Bitcoin to do the atomic swap. And then, once we'd like to move to the EVM, we can say okay, how much cost, how many costs we will have. But again, before you do that, please also do some preparation.

**Wang (16:09)**
Preparation — just to check how the classical adaptor signature or classical atomic swap is implemented in Solidity, because I'm not sure whether there's anything to say related to this. I'm quite curious to see their performance. Then we have a benchmark for the future.

**Wang (16:35)**
If we want to implement the post-quantum version in Solidity running on the EVM, then we'll get a comparable benchmark. Let's see.

### 8. Bitcoin has no gas limit — only transaction fees (16:46–17:27)

**Royce (16:46)**
So for Bitcoin, I don't think they use—

**Wang (16:49)**
No, they don't use the Ethereum Virtual Machine.

**Royce (16:54)**
Bitcoin — what kind of environment [is it]?

**Wang (16:57)**
That's totally different. Here we don't have a limit for the gas costs — in Bitcoin we don't even have gas costs. Here you have to pay the transaction fees. So if we perform some complex, expensive calculation off-chain, you don't need to pay the gas fees. Okay — so that's the plan for this one.

### 9. The π proof is off-chain; C or Rust (17:27–18:32)

**Royce (17:27)**
Because the ZKP that I implemented using LaZer — they require, in Ext, the pre-signature [exchange]. I think you know this bit. Like, one party must send the proof so that later they can extract from the—

**Wang (17:53)**
Okay, you can assume that the two [parties] — of course, there are two—

**Royce (17:59)**
Yes, this one I'm using LaZer for. But I think this one is optional[/off-chain].

**Wang (18:04)**
It's off-chain. It's off-chain, because it's like you and me: I have some Bitcoin, you have some other tokens on another blockchain. Then of course we can assume that we two have some secure communication channel before we do the exchange.

**Royce (18:22)**
So this probably can be fully in C, or in Rust—

**Wang (18:26)**
C or Rust, depends on you.

**Royce (18:29)**
Groth16 is in Rust.

**Wang (18:32)**
The people [who wrote it] are using Rust.

### 10. Rejection sampling, the norm change, and Overleaf (18:51–20:11)

**Royce (18:51)**
Yeah, I think I made some modelling as well in terms of the rejection sampling, but I'm not sure if this is [right].

**Wang (18:59)**
Interesting, okay. So previously we discussed this — it's for stage one. We discussed changing the norm a bit, by one, right? Okay, I'm not sure — so I need to read all of the details. I need to read that as well.

**Wang (19:18)**
Okay, so this one is the—

**Royce (19:21)**
I think I have the [numbers].

**Wang (19:23)**
Okay, [the cost] increased a little bit. This, I think, is acceptable.

**Royce (19:29)**
I have [the number] in Chapter 3.

**Wang (19:34)**
You've already done this one, right? So you have the Overleaf project for this? An Overleaf project?

**Royce (19:41)**
Oh yeah — using LaTeX.

**Wang (19:45)**
Are you using TeX locally, or are you using Overleaf? Have you ever used Overleaf before? Okay, you can share it with me.

**Wang (19:54)**
When you think you have a more ready version, I can also have a look.

**Royce (19:59)**
[I'll] share it on Overleaf. Okay — do I need to know your username or something?

**Wang (20:05)**
Yes, give me — my email address, that is fine. You know my [email]—

**Royce (20:11)**
Yeah, the Manchester [one].

*[≈20:11–20:40: dropout in both recordings — screen sharing/silence.]*

### 11. The geometric model: ≈36.8 % (20:51–22:28)

**Royce (20:51)**
Yeah, using the geometric model — like the approximation — it should be around 36.8.

**Wang (20:59)**
Okay, what is the geometric model?

**Royce (21:03)**
The model that I use to approximate the rejection sampling.

**Wang (21:10)**
Okay, you call it geometric. Okay, let me check this.

**Wang (21:27)**
Okay, kind of makes sense. So, accepted from this one — previously it was okay, and then we got minus one, minus one.

**Wang (21:49)**
I think it kind of makes sense.

**Royce (21:52)**
Yeah, and then if I run it large enough, I guess it should be close to the approximation.

**Wang (22:00)**
Okay, this is the sample number, as you draw here.

**Royce (22:06)**
Okay.

**Wang (22:08)**
So this is the theoretical result?

**Royce (22:11)**
This is approximately the theoretical [value].

**Wang (22:23)**
Yeah, I think it's interesting.

### 12. The rejection-probability figure Wang could not read (22:28–27:16)

**Royce (22:28)**
I think I also have the [diagram for it].

**Royce (22:45)**
[I] tried to plot the probability—

**Wang (22:49)**
Probability of what?

**Royce (22:51)**
The rejection.

**Wang (22:57)**
So this is the attempts, okay, and this is the probability of exactly [k]. Can you explain? I'm not sure I understood this. I know the X [axis] means how many times you retry. What does Y mean?

**Royce (23:15)**
It probably means: if it's 15 attempts until acceptance, then the probability is below 5 % that I need 15 attempts until it's accepted — like for the pre-signature or signature.

**Wang (23:31)**
How do we explain things here? So if it's one, why is it so high?

**Royce (23:40)**
I think it's because the bound should be small. I mean — it's random, of course, it's random sampling.

**Wang (23:55)**
Because I thought that the more you tried [the higher it would be], right?

**Royce (24:02)**
[unclear] I mean, that's the range — the probability of exactly k attempts.

**Wang (24:18)**
I'm not sure if I'm [following] this one.

**Royce (24:25)**
So it's less likely for you to have to sample until 15 times before it's accepted. So it should [decrease].

**Wang (24:35)**
Does this mean — I thought that if we sample multiple times, then the probability will be higher, right?

**Wang (24:41)**
But why is [it decreasing here]?

**Royce (24:45)**
Let's say people sample once, twice — by the third time it's usually already accepted. They don't have to try until 15.

**Wang (24:55)**
Oh, [not] 15 times.

**Royce (24:58)**
But I don't know what the maximum is, like the worst-case scenario.

**Wang (25:07)**
But for the users, normally they should try — that number, right?

**Royce (25:13)**
Yeah, that's on average; it should be around just two to three times.

**Wang (25:22)**
So which means, in many cases you try once and it's already accepted — the percentage is more than 35. Okay. And then "exactly"… okay. I would put something like this, for example. You can plot something like this.

**Wang (25:47)**
This is the number [of attempts], one to 15. And you could plot something like this: the probability of acceptance.

**Royce (26:00)**
Yeah, that's more accessible.

**Wang (26:02)**
And then maybe from one — you know, we're like, I don't know, 35 or something like that — then we can say okay, it will gradually increase. Then in this way we'll see that even if we try a lot of times, it increases very slowly.

**Wang (26:18)**
So which means, okay, we'll consider this one. I think it's easier for readers to understand. We try one time, okay, the probability is like this.

**Wang (26:34)**
And if we try many times, of course you will have a high probability of acceptance — but again the increase is very slow, because it's becoming almost flat, right? It's very close to 100 %.

**Royce (26:50)**
Because once it's accepted, it shouldn't be tried anymore.

**Wang (26:56)**
Oh, it's like a maximum of [15/20] [attempts].

**Royce (26:58)**
Oh, okay. So it's very likely that by the 15th time it's [already] been [accepted rather than] rejected.

**Royce (27:07)**
Yeah, that makes more sense.

**Wang (27:08)**
Yeah, of course. In this way it's kind of — I don't know.

**Royce (27:12)**
Counter-intuitive.

**Wang (27:13)**
Yeah, counter-intuitive — so people will ask why, if I try a lot of times, the probability is still [low]. But after you explained it to me, I think I got it. But let's try to make it clearer for the reader.

### 13. Packed tier and the "hybrid native" classical baseline (27:51–29:54)

**Royce (27:51)**
Oh yes, I think I have made the graph for it. This is the overhead if using — I call it the **packed tier**.

**Wang (28:01)**
Okay, so you mean [with] the encoding and decoding.

**Royce (28:06)**
Yeah — using decoding and encoding, it increases quite significantly, up to 80 % on Adapt.

**Wang (28:16)**
Okay, makes sense. I think the encoding/decoding is very consuming.

**Royce (28:22)**
And what else — yeah, with the classical thing, I don't know how to make the comparison, like for the key generation. They don't include [it], because they just pass it from the [data structure].

**Wang (28:48)**
Yeah, yeah, yeah.

**Royce (28:50)**
So I called it **hybrid native**.

**Wang (28:52)**
Okay, okay.

**Royce (28:55)**
Yeah.

**Wang (28:56)**
So this is without—

**Royce (29:01)**
This is without packing/unpacking.

**Wang (29:03)**
And this is with. Okay?

**Royce (29:05)**
It kind of makes sense — and I believe this is [with] packing/unpacking, except the key generation.

**Wang (29:13)**
Oh, so you mean here they have already added some packing/unpacking stuff. Okay, it's fine, it's fine. But you cannot remove the packing stuff from [their side] — they have already built it into their existing APIs.

**Wang (29:34)**
Yeah, it's fine, because they all say okay, that's fully [built in].

**Royce (29:38)**
So the only thing they don't unpack is only the key generation; and then for the full signature, they just [pack and unpack] the intermediate products. And yeah, this is the gas cost.

**Royce (29:54)**
I'm still not sure [about it].

### 14. Report structure: evaluation vs conclusion and critical reflection (30:07–34:51)

**Royce (30:07)**
And again — I don't know whether "challenges" should be included in the report or not.

**Wang (30:14)**
The challenges are for [which chapter]?

**Royce (30:15)**
[Chapter 4] — I really don't know what to include.

**Wang (30:19)**
Normally I think [Chapter 4] should be the evaluation, and I would prefer to put the reflection into another chapter — Chapter 5, where we have the summary, or conclusion, and the critical reflection.

**Royce (30:41)**
So the reflection [goes] in [Chapter] 5?

**Wang (30:43)**
Yeah, on top of that, another independent chapter. So "evaluation" means it's still your work, right? You are still reporting the important results, the important findings you found — that's very important, because I would like to divide it into another chapter. And then in this Chapter 5 you will summarise what we have already achieved, from the theoretical part and also the evaluation part, and then you will have another subsection to show okay, what you failed at.

**Wang (31:16)**
How would you do it a better way, if you were given another chance to do that? Okay, so it's like this. You've checked the paper, right?

**Wang (31:26)**
You've checked that paper — they also have the conclusion. I believe, yeah, you can see. But here the conclusion is very short.

**Royce (31:42)**
I don't know — [mine] only has like 2 pages for the conclusion.

**Wang (31:45)**
Yeah, it's fine.

**Royce (31:48)**
So for evaluation, what should be in the evaluation?

**Wang (31:53)**
Sorry, what's this? This is conclusion, future work — and what is Chapter 3?

**Royce (32:01)**
Results.

**Wang (32:02)**
Okay — results. Results, evaluation. So normally, when we're writing papers, "evaluation" means the—

**Royce (32:13)**
The reason why.

**Wang (32:14)**
That's the implementation and evaluation results, something like that. So maybe you can change it to another name.

**Wang (32:23)**
What are they called? Okay.

**Royce (32:30)**
[Chapter 4] evaluation, and the reflection [in the] conclusion.

**Wang (32:35)**
Okay, methods, your reflection. You can see they have the method, the methodology, right? Then you should also present your results.

**Wang (32:45)**
Why [not] present your results [separately]?

**Royce (32:47)**
[Chapter 3] is combined with results.

**Wang (32:51)**
Okay, I would prefer to put them separately. It's easier for people to see that — otherwise I first need to understand your method, and then I will [get to the results]. You should have evaluation and testing. Okay, you can also put some critical reflection [there].

**Wang (33:11)**
But—

**Royce (33:13)**
The document says that the structure doesn't have to be like this.

**Wang (33:18)**
Yeah, yeah — they would prefer to put the evaluation independently.

**Royce (33:23)**
In one chapter, and move the reflection to Chapter [5].

**Wang (33:27)**
Yeah, the conclusion, yeah. Because I mean we are talking about the evaluation, right? Of course, maybe while you're talking about the results you can do some reflection already.

**Wang (33:40)**
But it's fine. I mean, maybe you can add a short paragraph regarding the reflection on your evaluation results. But finally, I would like to say it would be better to have an independent subsection, at least, to do the critical reflection — to summarise what we have done.

**Royce (33:59)**
This is all in Chapter 5?

**Wang (34:00)**
So — where did you find this?

**Royce (34:03)**
On Canvas.

**Wang (34:05)**
Canvas — let me check. [unclear]

**Wang (34:30)**
[unclear]

**Royce (34:39)**
It should be in the MSc Report and Video [handbook].

**Wang (34:51)**
Okay, yeah, that's good.

### 15. Follow the paper's protocol strictly? LaZer in C, called from Rust (34:58–36:09)

**Royce (34:58)**
Also for this — I think they have [unclear] probably for this one. Must this one be strict — the proof — or can I relax the proving, or skip it? Should I follow this protocol strictly?

**Wang (35:20)**
It would be better [to follow it]. First of all, you can also check their actual implementation — maybe they have done some optimisation in the implementation. But in general I think the structure should be almost the same.

**Royce (35:34)**
Yeah, so what I'm doing right now is just implementing LaZer in C and Rust — whatever language I'm using, I just call LaZer in C.

**Wang (35:45)**
Okay, yeah — so LaZer is in C generally. But if you use Rust to call C, I don't know how efficient it will be. But anyway, let's try to have a ready version [by] the weekend.

**Wang (36:04)**
We can try to improve the efficiency in time.

### 16. Future work; what to compare when there is no gas (36:09–37:28)

**Royce (36:09)**
And for the future work, I guess — I'm not sure if—

**Wang (36:16)**
So let's first do this. If we can finish this one, and then we have time to finish this one, then this one maybe doesn't need to be included in the future work.

**Wang (36:28)**
This, I think, is important for the next [step].

**Royce (36:30)**
Basically the classical AS performance.

**Wang (36:32)**
Yeah, because as we discussed at the very beginning, we also wanted to do the benchmark — the comparison with the post-quantum version.

**Royce (36:43)**
So if Bitcoin doesn't have a gas-cost metric, what kind of comparison should I show?

**Wang (36:49)**
The time — okay, the time, and also the communication cost.

**Royce (36:53)**
Communication costs.

**Wang (36:54)**
Even for off-chain components, right? Here you can see: I send something to you, you send something to me. Of course that increases my communication cost.

**Wang (37:04)**
And it also introduces computation costs, right? Which means when I issue a transaction, I have to do some preparation before I do that — which means sometimes I cannot do it on my phone, I have to use a heavy machine, some dedicated PC, to do that. This will also affect the usability in practice.

### 17. How realistic must the simulation be? (37:28–38:50)

**Royce (37:28)**
Alright. And communication — but it doesn't have to be like one person using one local port and the other person using [another] local port. It doesn't have to be [that], does it?

**Wang (37:50)**
What do you mean, protocol? Do you mean different blockchains?

**Royce (37:53)**
[I mean] whether the simulation has to be very realistic — like, someone is using a certain local port, and another person is using another local port number, and they communicate like [that].

**Wang (38:10)**
Oh yeah, I got what you mean. No, no, you don't [need to].

**Wang (38:13)**
You don't do it — you just asked me that. Okay, we are doing an implementation; of course you can do some simplification.

**Wang (38:21)**
I assume that okay, the messages can be easily transferred to others. But in the future, if you would simulate the actual atomic swap, of course you can open two ports for them and then have clear communication with each other. But for now let's assume that okay, I can just send a message to you directly. Assume that you have two users and they can both access your machine.

**Royce (38:48)**
Oh — makes sense.

**Wang (38:50)**
Okay. I think that's the plan for next week.

### 18. Refund and timeout: edge cases (38:59–40:14)

**Royce (38:59)**
And does it have to implement refund or timeout as well? Like, the atomic swap sometimes can fail — do I have to implement the protocol for how the refund [works], the refund protocol or timeout protocol?

**Wang (39:16)**
Yes, but that's for the edge cases, I would say. So let's first focus on the normal cases — we assume that of course they are both honest. But of course, if some of them are not honest, it means that the funds will also be transferred back to the honest users, so they won't suffer any loss.

**Royce (39:39)**
Okay. So does the protocol still use packing/unpacking?

**Wang (39:48)**
It depends on you. If you think that it's efficient enough, you can use packing and unpacking. If it's not very efficient, let's just ignore it.

**Wang (39:59)**
And then you also add this limitation, or critical reflection, for this. It's just the exploration, right? You don't need to build a product already — [you're] exploring, your demo, right?

**Wang (40:14)**
[Whether] this is feasible or not.

### 19. Benchmarking the classical adaptor repo (40:18–41:47)

**Royce (40:18)**
Classical adaptor signature.

**Royce (40:32)**
So — exactly. So can I benchmark using this one? Is it possible to benchmark the performance of this?

**Wang (40:47)**
You mean the classical [one]? Yeah, yeah. I don't know this one — this is the one I suggested. Maybe you could find a better implementation than [that] and build things from there. But if you think this is the best one—

**Wang (41:00)**
Yeah, of course you should benchmark [it].

**Royce (41:02)**
That's fine. Yeah, because if I'm not mistaken, some repos don't have the Ext protocol — it's just Adapt, and then they stop.

**Wang (41:09)**
Okay, so they don't have an extraction. I mean, you know, some—

**Royce (41:15)**
Some demos just stop [at Adapt].

**Wang (41:18)**
Yeah, I mean, of course, if you have [it]. But extraction is always very fast, right? We just need to do the final step.

**Royce (41:36)**
And [what] else — yeah, I think that's it.

**Wang (41:47)**
Okay, good, good. I'm looking forward to [seeing] the results.

### 20. Closing: is one week feasible? (41:55–42:00 — recording ends mid-answer)

**Royce (41:55)**
Do you think it's possible to do it in one week?

**Wang (42:00)**
I mean, for the three steps I told you, I think it's doable. But [let's] avoid [the EVM] for now — because again, as I said, I'm not sure there are many implementations of adaptor signatures on the EVM. Because I think it's a little bit [difficult], to be honest — because we have smart… **[recording ends mid-sentence]**

---

## D. Action items arising

| # | Action | Source |
|---|---|---|
| 1 | Take the actively-maintained classical atomic-swap repo and reuse its architecture; replace ECDSA with LAS. Avoid Monero (privacy features add complexity) — prefer two similar UTXO chains, e.g. Bitcoin + a Bitcoin-like chain. | §1, §2, §6 |
| 2 | Build and benchmark **three configurations**: (i) classical adaptor signature + Groth16, (ii) LAS + Groth16, (iii) LAS + LaZer (post-quantum ZKP). | §7 |
| 3 | Target **Bitcoin/UTXO, not Solidity/EVM**, for the Stage-2 application. EVM is deferred; full native LAS verification is infeasible against the block gas limit. | §4, §6 |
| 4 | Since Bitcoin has no gas metric, report **time + communication cost**, including off-chain messages, and discuss the usability implication (preparation may need a dedicated PC, not a phone). | §8, §16 |
| 5 | Replace the "probability of exactly k attempts" figure with a **cumulative probability of acceptance** plot over attempts 1–15 (rises from ≈35 %, flattens toward 100 %). | §12 |
| 6 | Preparatory literature check: how are classical adaptor signatures / atomic swaps implemented in Solidity, and at what cost — to give a future PQ-on-EVM measurement a baseline. | §7 |
| 7 | **Report structure:** evaluation as its own chapter, separate from methodology; critical reflection as at least its own subsection in Chapter 5 with the conclusion (achievements from theory + evaluation, what failed, what would be done differently). | §14 |
| 8 | Share the Overleaf project with Wang's Manchester email once a reasonably complete version exists. | §10 |
| 9 | Wang to read the norm-change-by-one details himself. | §10 |
| 10 | Implement the **happy path first**; refund/timeout are edge cases. Packing in the swap demo is optional — if not efficient enough, omit it and record it as a limitation. | §18 |
| 11 | Benchmark the classical adaptor repo, noting that some demos stop at Adapt and never implement Ext. | §19 |
| 12 | Get a working LaZer version first (C called from Rust is acceptable); optimise later. | §15 |
