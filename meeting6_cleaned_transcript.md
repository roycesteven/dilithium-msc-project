# Cleaned Transcript — Meeting 6 with Zhipeng Wang

**Source:** `meeting6_original_transcipt.md` (UTF-16 auto-transcript; in this recording **Speaker 1 = Royce Steven, Speaker 2 = Zhipeng Wang** — the reverse of Meetings 1–3).
**Type:** meaning-preserving cleaned transcript, not a legal/verbatim transcript. Every exchange of the original is represented; nothing is dropped. Genuinely unintelligible fragments are marked `[unclear]`.
**Date:** between Meeting 5 (2026-07-06) and 2026-07-19 (exact date not in the transcript).
**Main topics:** packing/unpacking (serialization) timing and the two-tier reporting decision; full walkthrough of the modified scheme (Wang quizzing Royce); PreSign vs Sign rejection attempts; the 32-byte challenge-seed change; C vs Rust comparison; ECDSA-baseline column layout; figure quality (PNG/PDF, font sizes); the 6–8-minute video; next stage = study atomic swap (HTLC history → adaptor signatures); PR review; contact with the LAS author.

---

## A. Key contextual corrections

| ASR / unclear phrase | Corrected meaning |
|---|---|
| newspaper / knees of paper | the NIST paper — FIPS 204 (ML-DSA) |
| the last paper / oz / las | the LAS paper, eprint 2020/845 |
| pecking / packy / unpacky / Peck / Peggy / Pakistan and pecking | packing / unpacking (serialization encode/decode) |
| invites / bites / buys / budgets / Vita shop | bytes / byte encoding |
| safe instruct / safe invites / data struck | saved as a struct / saved in bytes / data struct |
| a depth / at pecking it did the first significant move | Adapt (the Adapt operation) |
| FBI / fpi / people's fpi | API (the reference implementation's API) |
| public cake / public a / public Parliament / public department | public key / public parameters |
| hedge / has / hesh / Hesse / hanching | hash / hashing (SHAKE) |
| sample impulter | SampleInBall (FIPS 204 challenge sampler) |
| seat / seats / ship / sip / the seed that we generate from the house | seed (the 32-byte challenge seed c̃ squeezed from the hash) |
| chickmi / the signature ride sees that ntk | c̃ (c-tilde); the signature is (c̃, z) |
| prayer signing / pray signing / presign nature | PreSign / pre-signing |
| prelarification / preverification / pro | PreVerify / pre-verification |
| adaptary / attack signature / tap signature / adaptive signature / class adoption lecture | adaptor signature |
| diverse signature | adaptor signature |
| dilightium / lithium / denisium / delete film | Dilithium |
| the wine loc / why | the statement Y / the witness y |
| Nebulae / debut | w (the commitment) |
| the security P / signing was / seldom / white | the secret r / the signing mask y |
| mass Assigning masks plus a challenge time the secret key | z = y + c·r (mask plus challenge times secret) |
| the public key is 3 | the public key is t = A·r |
| Z is Y plus CR and T is a times R | z = y + c·r and t = A·r |
| reconstruct commitment by using X1 | reconstruct the commitment w′ = A·z − c·t |
| 0 − 1 norm / the bone is 0−1 | witness norm bound 1 (ternary coefficients in {−1, 0, 1}) |
| 2 points 71 / 2.7 / Tottenhoita | expected rejection attempts ≈ 2.72 (Sign) vs ≈ 2.77 (PreSign) |
| 4400 nights | ≈ 4416 bytes (the packed statement Y = public-key size at the target setting) |
| 160 milliseconds → 500 | ≈ 160 µs (core) → ≈ 500 µs (with pack/unpack), i.e. 0.5 ms |
| 3 seconds level | typical blockchain transaction/confirmation latency (seconds), against which 0.5 ms is negligible |
| ecds APIs including backing unbacking | the secp256k1 ECDSA API includes packing/unpacking internally |
| 3 columns or 4 columns | add 3–4 columns: LAS without packing, LAS with packing, ECDSA (native API) |
| C and Ross | C and Rust |
| 1:07 | ≈ 1.07× (largest C-vs-Rust ratio, key generation) |
| Ken saviet has PNG / PG or PF | can you save it as PNG or PDF |
| phone size | font size (of figure labels and legends) |
| automatic square / automic spram / mix wrap / automated this web | atomic swap |
| harsh luck / harsh log | hash lock / hash-locked contract (HTLC) |
| For architects | in blockchains (how adaptor signatures are implemented in blockchains) |
| vertical there is updated | the PR (pull request) is updated |
| Cody | the code |
| one of my color tests | one of my collaborators (the LAS author) |
| isol / he's moving to isol | isogeny-based constructions (the author's newer research direction) |
| oz has imperfection, correctness | LAS has imperfect correctness (the author's stated limitation) |
| screenhouse | screencast |
| 75 version / full game | the full (non-simplified) Dilithium version |
| hints | the hint vector of full Dilithium (omitted in the simplified base) |
| shake 128 / 256 bits | SHAKE-128 (public matrix expansion) / SHAKE-256 (secret-key derivation) |

---

## B. Meeting summary

Royce reported a new measurement: serialization (packing/unpacking) dominates some LAS operations — for Adapt, the core algorithm is only ~10–20% of the time once packing/unpacking is included (~160 µs core vs ~500 µs with pack/unpack). Wang's ruling: packing does **not** change communication cost, only computation; it can often be done **once, offline, or in advance** (e.g. unpack a public key once and reuse it), so it must not be silently folded into "the verification time". **Report both timings — with and without packing/unpacking — as separate, clearly-labelled cases**, and add a discussion that deployments can amortise or optimise it (keep keys unpacked in memory). This is the origin of the two-tier (core vs full-protocol) reporting rule.

Wang then had Royce walk through the entire modified scheme on the whiteboard — setup, key generation (t = A·r), signing (z = y + c·r, challenge c = H(pk, w, M)), verification (w′ = A·z − c·t), and the adaptor path (challenge over w + Y, ternary witness bound 1 so adapted z stays inside the bound, PreVerify with +Y, Adapt, Extract by subtraction) — and pronounced the process correct overall. During the walkthrough he asked one thing Royce could not answer on the spot: **the measured number of rejection attempts for PreSign compared with Sign** (Sign showed ≈2.7; the PreSign bound is tighter, so people will ask how many more tries it needs) — this must be measured and reported, at the same public parameters.

Royce described the serialization change: the challenge is no longer stored as a packed polynomial; only the 32-byte hash output (c̃) is stored, per FIPS 204, and the challenge polynomial is re-derived with SampleInBall — a computation-vs-storage trade-off Wang asked to be discussed in the report. On C vs Rust, Wang ruled the comparison **is** fair and reportable since the parameters are identical (differences ≤ ~1.07×, key generation). For the classical ECDSA baseline, whose API packs/unpacks internally and cannot be changed, Wang asked for a 3–4-column layout: LAS without packing, LAS with packing, ECDSA as-is (annotated).

Remaining agreed deliverables: update the tables and figures with the pack/unpack numbers already measured; export figures as high-quality PNG/PDF (not screenshots) with legible label/legend font sizes; keep the PR updated for code review. Next stage: **study the atomic swap before coding it** — read its history (why hash-locked solutions came first, what their limitations are, why adaptor signatures replaced them) and how classical adaptor signatures are deployed in blockchains, so the report's motivation section is grounded. The video (6–8 min) should highlight only the most important points; slides can carry the detail. Wang confirmed the LAS paper's first author is his collaborator and offered to relay questions; the author's own research moved on (imperfect correctness of LAS; isogeny-based directions), but that is beyond this project's scope.

---

## C. Cleaned transcript

### 1. Packing/unpacking: separate timing or not? (00:00–05:35)

**Royce (00:00)**
I think I may have made a mistake — or I should refer to the NIST paper [FIPS 204] first. In the LAS paper the algorithm is simplified: it doesn't include packing/unpacking. So my question for the week: do I need to make a separate timing — one that is core…

**Royce (00:35)**
…computation timing, and the other including packing and unpacking?

**Wang (00:41)**
Let's just start — I'm going to get confused here. Which paper contains the packing and unpacking?

**Royce (00:51)**
The NIST paper?

**Wang (00:53)**
The NIST paper.

**Royce (00:55)**
Yes — and actually the API from the Dilithium [reference implementation] also includes packing and unpacking fully. You can see…

**Wang (01:15)**
Yes, but those are engineering changes — how they organise the information. Does it affect the main algorithm of the signature scheme?

**Royce (01:38)**
It doesn't affect the algorithm, but since the communication size is big — whenever I want to create a signature…

**Royce (01:50)**
…I need to decode, that is unpack, the secret key first.

**Wang (01:54)**
Okay, so let me check: in your current implementation, are you using packing and unpacking?

**Royce (02:09)**
I'm adding packing and unpacking now.

**Wang (02:12)**
So how much does it save, in terms of communication cost?

**Royce (02:19)**
In terms of communication cost it's the same — but in terms of computation, adding packing/unpacking changes things significantly [e.g. for Adapt].

**Wang (02:30)**
So the computation time changes, but it won't reduce the communication cost.

**Wang (02:43)**
Then what's the goal of having packing/unpacking — to reduce the size?

**Royce (02:49)**
No — because the secret key and public key cannot be saved as a struct; they must be saved in bytes. So every time the other party receives the public key, they must decode the bytes into the data struct.

**Wang (03:10)**
But can they do this, say, before they perform the signature? For example: you tell me you will release a signature tomorrow — then I can prepare your public key today, right?

**Wang (03:27)**
So I think it's fine. People can do it offline, or in advance.

**Wang (03:39)**
We can just report it as a **one-time cost**, right? After I set up your public key, I can use it to verify your signature 1 and your signature 2.

**Wang (03:55)**
So they don't need to redo it every time — they only need to do it once, if I understood correctly.

**Royce (04:05)**
From my understanding: if I shut down my computer, it must be saved in a file — in bytes. Whenever I make a signature, I must convert the bytes into the data struct.

**Royce (04:19)**
So I must unpack it.

**Wang (04:21)**
Okay — but again, I think it's still fine, because it depends on the application. You can report it, of course, but you can present it as an **additional** computation cost.

**Wang (04:37)**
It may depend on the application or the deployment — maybe you keep your machine alive. But I don't think it should be included in the headline verification time. You can include it, but you just need to specify: this is not the typical verification time — some people can do this in advance, before they run the verification algorithm.

**Royce (05:03)**
Yes — but if I look at the reference API, it always includes it.

**Wang (05:07–05:17)**
Every time? … Okay, then it's fine, you can follow this too. But how much does it change? You know what I care about: the verification time — and the generation time as well.

### 2. How big is the packing overhead? Report both. (05:35–11:00)

**Royce (05:35)**
It's mostly the public statement — it's really huge.

**Wang (05:44)**
Having a statement — but you're talking about computation time rather than communication cost now, right? [Packing] doesn't increase the communication cost.

**Royce (05:56)**
It doesn't increase the communication cost. But because the object is huge, whenever I pack or unpack it, it also takes a lot of time.

**Wang (06:06–06:11)**
I'm talking about the size. But you were saying "it's very huge" — what is very huge?

**Royce (06:15)**
The bytes — whenever I convert bytes into the struct, it takes time.

**Royce (06:24)**
And whenever I save my signature into bytes it also takes longer. So it increases the computation cost significantly.

**Wang (06:35)**
Okay, so how much? Do we have a breakdown of that?

**Royce (06:43)**
I have to check again, but from what I get: for **Adapt** it's around **80%** [of the time spent in pack/unpack].

**Wang (07:01)**
80% of the time is used in the pack and unpack?

**Royce (07:06)**
Yes — with packing/unpacking included, the core Adapt is only around 10%; the pack/unpack is around 80%.

**Wang (07:15)**
Okay — then you report **both**: the case with packing and without packing.

**Royce (07:34)**
Yes — from ~9% to ~80%, depending on the operation. Without packing/unpacking, the core algorithm is only ~9% [of the full-path time]; packing/unpacking is ~80% — about a 70-point difference.

**Wang (08:00)**
And in the existing paper [the LAS paper] — did they include this kind of packing and unpacking?

**Royce (08:07)**
No. They even use very simplified algorithms — for the challenge generation they only say c = H(…): the hash function directly generates c. Whereas actually, in the NIST paper — which is more like the real algorithm — after hashing you must **sample** the challenge (SampleInBall) using the seed generated from the hash.

**Royce (08:55)**
In the paper it's very simple.

**Wang (08:58)**
Okay — but in your case, are you using the full version?

**Royce (09:08)**
Still simplified, for sure.

**Wang (09:10)**
So you simplify something — you still simplified something [relative to full Dilithium].

**Royce (09:20)**
Yes, still simplified — I'm not using the hints. From what I know, the full scheme is only an optimisation on top.

**Royce (09:38)**
The serialization — packing/unpacking — in the end is still the same. In the LAS paper, probably because it's a short paper, they don't show the full algorithm at that scope.

**Wang (09:56)**
It's fine. You can follow what you're doing now. You don't need to follow all the details — but again, **I think you should report the pack and unpack time**.

**Wang (10:12)**
And you can add some discussion: improvements/optimisations are possible — for example, store the public key or the private key [unpacked] — if you keep it in memory, you don't need to redo it every time.

**Wang (10:36)**
I think it's an interesting fact — it's also important to let people know why, so that in the future when they do this, they know what to expect.

**Royce (10:45)**
So I should make the timing separate…

**Wang (10:48)**
Yes — separate.

**Royce (10:49)**
…for the [core] protocol and with the packing.

**Wang (10:51)**
With the packing. And you can say that the packing can be done just one time — one-time cost — or [amortised]; it depends on the application. Okay — now, what else is there for this one?

### 3. Walkthrough request: show exactly what was modified (11:09–13:35)

**Wang (11:09)**
If I remember correctly, we discussed last time: this week you can introduce to me the whole process — which parts you have modified — and make sure it's correct. Then, if everything's good, we can move to the next stage.

**Royce (11:29)**
From the key generation…

**Wang (11:33)**
Yes. Do we have a slide to show it, or a document — something you will include in your report — to show the differences between the base Dilithium signature and…

**Royce (11:50)**
…the adaptor signature.

**Royce (12:05)**
First, I use the Dilithium core, and then I remove some parameters that are not in the [LAS] paper and simplify it. So I start from Dilithium and make the simplified base. And the base starts from the setup — I create the public parameters.

**Wang (12:40)**
So setup generates the parameters; and key generation generates the keys — public key and private key. Are they the same [as Dilithium's]?

**Royce (12:52)**
For the public key, almost the same — just using the dimensions/parameters from the [LAS] paper.

**Wang (13:03)**
Okay, go ahead.

**Royce (13:05)**
For the public-matrix generation I use SHAKE-128.

**Wang (13:19)**
Through hash functions?

**Royce (13:21)**
Yes — and for the secret key, SHAKE-256 [256-bit] generation.

**Wang (13:28)**
Okay, go ahead.

### 4. The adaptor setup: statement into the hash; ternary witness (13:37–16:01)

**Wang (13:37)**
And for the adaptor signature you have some additional setup.

**Royce (13:41)**
Yes — for the adaptor signature, the challenge hash function doesn't only take the public key, commitment, and message: I also add the commitment together with the — what do you call it — the adaptor…

**Wang (14:09)**
That is the **statement**.

**Royce (14:12)**
Yes, the statement.

**Wang (14:14–14:17)**
And Y is… the statement Y.

**Royce (14:19)**
And y, in our case, is the witness.

**Wang (14:21)**
Okay, so this is the setup for the adaptor signature. Let me compare: for the basic scheme the input of the hash function is the public key [commitment and message]; here you also add [the statement]. And what is this w here?

**Royce (14:46)**
w is the commitment — produced from the multiplication of the matrix A and the mask y.

**Wang (14:56–15:03)**
Which means A multiplied by [the mask]. Okay, go ahead. So the only different thing is what we add…

**Wang (15:18)**
…the statement Y over the commitment in the hash. Okay, exactly.

**Royce (15:24)**
And after the hashing, we sample the challenge [SampleInBall] using the seed generated from the hash function.

**Wang (15:37)**
And you won't reject it? You won't tamper — okay, you make sure. So here's a minor difference:

**Wang (15:45)**
the norm bound [of the witness] is 1.

**Royce (15:49)**
1 — because I need to adapt with the witness, and make sure that after the addition [z + y] it is still inside the bound.

### 5. Wang's open question: PreSign attempts vs Sign attempts (16:01–17:23)

**Wang (16:01)**
Yes, that makes sense. So — in the pre-signing process of the adaptor signature, how many tries do you need? Compared to the number you showed here, 2.7 [for Sign] — I want to know, for this one, what is…

**Wang (16:20)**
…the number of tries — the attempts? For Sign you try ~2.7 times; here I was wondering how many times you have to try?

**Royce (16:36)**
I should have the number, but… [checks]

**Wang (16:49)**
Do we have it?

**Royce (16:52)**
I think it doesn't differ much in time, but I haven't made an analysis of that, actually.

**Wang (16:58)**
Of course people might ask: you have a tighter [bound], so maybe you try more times, right? I would like to see — **at the same public parameters** —

**Wang (17:10)**
how many more times you have to try. It would be better to report it.

**Royce (17:16)**
And to use it to answer why it takes longer — yes.

**Wang (17:23)**
Anyway, go ahead — just remember to add it to my list. Then we get to verification.

### 6. Whiteboard quiz: sign, verify, presign, preverify, adapt, extract (17:29–27:00)

**Royce (17:29)**
After the LAS PreSign, the other party does the pre-verification.

**Royce (17:38)**
We construct the challenge, but using this kind of… it's abstract.

**Wang (17:52–18:00)**
Okay — let me ask: what are the inputs of the verification algorithm? This is a function, right? What are the inputs of this verify function?

**Royce (18:06)**
The input… we reconstruct the commitment first, and then we put the commitment into the hash function and compare whether the reconstructed hash is the same.

**Wang (18:25)**
Okay — let me go through the processes. Key generation: it's a function; the input is the public parameters (pp), and it generates the pk [and sk].

**Royce (18:47)**
Yes, correct.

**Wang (18:48)**
Okay. In signing, normally we have the message and the SK, and it generates, let's say, σ, the signature. In the verifying process we will have this σ, and also…

**Wang (19:09)**
…we need to output valid or not valid. So here — can you tell me what the signature looks like?

**Royce (19:21–19:29)**
c and the response z. The signature σ is the challenge and z.

**Wang (19:34)**
What is z?

**Royce (19:35)**
z is the signing mask plus the challenge times the secret key [z = y + c·r].

**Wang (19:49)**
Okay. And what is the public key?

**Royce (19:53)**
The public key is t = A·r, with A the public parameter.

**Wang (20:08–20:25)**
Okay, thanks. — You can write down the process for signing first. Can you tell me how we get the c?

**Royce (20:32)**
For the challenge, we hash the public key, the commitment, and the message.

**Wang (20:41)**
And what is the commitment — how do we get it?

**Royce (20:54)**
It is the public matrix times the signing mask [w = A·y].

**Wang (21:02)**
So what is y?

**Royce (21:05)**
y is the signing mask.

**Wang (21:11)**
Is [A] in the public parameters?

**Royce (21:17)**
Yes, it's in the public parameters.

**Wang (21:18)**
From the setup generation. Okay, go ahead. And then you verify — you can refer to those two [equations] if you want.

**Royce (21:33)**
For verifying: it tries to reconstruct — because we always send the c̃; the signature is (c̃, z) — so it reconstructs

**Royce (21:58)**
the commitment using this formula: z = y + c·r and t = A·r, so w′ = A·z − c·t. We get the reconstructed commitment and hash it again. If it matches, then verification returns 1, otherwise 0.

**Wang (22:42)**
So you first calculate, then reconstruct the commitment using [A·z − c·t]. Okay.

**Royce (22:52)**
Yes — we put it into the hash function and compare with the initial challenge.

**Wang (23:02)**
Okay — that's the basic signature scheme. Next.

**Royce (23:07)**
For the adaptor signature, the only difference: the challenge is over pk, commitment **plus Y**… This [Y] is from the other party who creates the witness — the secret.

**Royce (23:52)**
After the challenge, it generates the pre-signature. The response is the same form [ẑ = y + c·r], and we reconstruct the commitment as well.

**Royce (24:36)**
And the other party does the pre-verification: reconstruct the commitment **plus the statement Y**, and

**Royce (25:00)**
if it matches the challenge, the pre-verification is successful.

**Wang (25:12)**
Okay, good. So — how to do the adaption, and also the [extraction]?

**Royce (25:20)**
After PreVerify succeeds, the party will adapt — complete the signature using the witness [z = ẑ + y].

**Royce (25:42)**
After Adapt, he publishes the full signature on the chain — and the other party can extract the witness: because Alice generated the pre-signature, he can just subtract the pre-signature from the full signature [y = z − ẑ].

**Wang (26:26)**
Okay, yes.

**Royce (26:32)**
After it's extracted, he can also complete the [other] full signature as well.

**Wang (26:50)**
Okay — good, good. In general, maybe there are some details, but I think overall **the process looks correct**.

### 7. Where packing sits in the flow; statement size (27:00–30:30)

**Wang (27:00–27:19)**
Can we come back — I want to see the comparison. So this is what you modified. And this is for the packing and unpacking, right?

**Wang (27:33)**
In the process, which steps involve the packing/unpacking — key generation, the signing process…?

**Royce (27:47)**
Before the signing — the first thing the signing process runs is decoding the SK, which is saved in bytes. It must decode the SK before it can use it.

**Royce (28:09)**
And at the end of the algorithm it encodes the signature.

**Royce (28:27)**
For Verify it's the same: decode the PK [and the signature] — this one doesn't return bytes, just 1 or 0. Same for PreSign and PreVerify.

**Royce (28:52)**
For Adapt: it must decode the pre-signature… and [encode the completed signature].

**Wang (29:25)**
So it's basically the encoding and decoding processes that take a lot of time — the ones you call packing/unpacking.

**Royce (29:35)**
Yes — and Adapt must also decode the… the statement, I guess.

**Wang (29:43)**
Okay. In general, how large is the statement?

**Royce (29:48)**
The statement is about 4416 bytes [the public-key size at the target setting].

**Wang (29:58)**
Okay. You just honestly report the numbers, and you can say that in practice some optimisation is possible — but it's not our job to do that optimisation; it depends on the developer.

**Royce (30:08)**
Yes — if we had to optimise the packing/unpacking, I would have to change my implementation towards the full Dilithium to reduce the byte sizes.

**Wang (30:22)**
Yes — that's fine, but that's not the focus point here, not the central point. Let's continue.

### 8. Fairness tripwires Royce added (30:34–31:16)

**Royce (30:34)**
I also hash the pre-signature, the signature, and everything, and compare the bytes — the digest is the same — to make sure it's the same protocol. I'm also checking whether my pre-verification does an early abort or a full check of the loop, to make sure the timing comparison is fair.

**Wang (31:13)**
Okay, good — makes sense.

### 9. Serialization change: store only the 32-byte challenge seed c̃ (31:16–34:03)

**Royce (31:16)**
What changed from my last implementation: I don't pack the challenge as a polynomial any more — I only save the challenge as the output of the hash function [c̃]. Every time I decode, I re-derive the challenge polynomial with SampleInBall using the seed I decoded.

**Royce (31:56)**
Because the hash output is only a seed, and from the seed I must sample the challenge. So I only save the hash output — which is **32 bytes**, according to this [FIPS 204] paper.

**Royce (32:25)**
Last time it was 64 bytes, so I changed it to 32 bytes.

**Wang (32:31–32:43)**
From which number… okay — so you save it, then you can recalculate: run the hash/sampling again; same seed, same output. That's fine.

**Wang (33:01)**
So we saved [bytes] — from [the packed polynomial] down to 32.

**Royce (33:08)**
[States the byte saving; exact figures unclear in the recording.]

**Wang (33:11)**
Okay. It's not a huge saving, but it means there is a **trade-off between computation time and storage cost** —

**Wang (33:21)**
if you don't want to recompute, you store more; if you're okay to rerun the hash/sampling, you just send the seed.

**Royce (33:30)**
I'm following the NIST protocol — it doesn't save the polynomial, it just saves the seed.

**Wang (33:37–33:57)**
That's fine, it makes sense. But anyway, **add that kind of discussion in your report**: in practice, if you don't want to recompute, you can store the expanded output directly; if you accept recomputing the hash/sampling, you send only the seed.

### 10. PreSign vs Sign rejection numbers revisited (34:03–34:45)

**Royce (34:03)**
For the rejection sampling — comparing Sign and PreSign — it should be slightly different…

**Wang (34:11)**
Just the one [bound difference], right?

**Royce (34:14)**
Probably milliseconds.

**Wang (34:17–34:25)**
Let's see. Maybe it won't be, say, 2.3 vs 2.9…

**Royce (34:30)**
It's like 2.7-something versus 2.71 — yes.

**Wang (34:35)**
Yes — just report it.

**Royce (34:36)**
[PreSign] should be slightly larger.

**Wang (34:41)**
Yes, I think so.

### 11. What's still missing: put the pack/unpack numbers in the tables (34:45–36:30)

**Royce (34:45)**
Is there still something missing from my implementation?

**Wang (35:08)**
I don't think so. For this first stage it's okay — of course there are some numbers to report, but you have already shared concrete numbers with me.

**Royce (35:20)**
I just ran the packing/unpacking [measurements], but I haven't put them in the tables yet.

**Wang (35:27)**
Yes — that's what you should do this week: summarise what you have done, and **update the tables and the figures** — you already have them without pack/unpack;

**Wang (35:39)**
now add the with-unpack / without-unpack versions.

**Royce (35:43)**
This one without packing is only ~160 µs; with packing it becomes ~500 µs.

**Wang (35:54)**
Okay — so report them both.

**Royce (36:01)**
Doesn't this show that my unpacking is not optimised?

**Wang (36:07)**
It's 0.5 milliseconds — I think it's acceptable, especially in the application setting, which is the second step. Transactions are at the seconds level, so 0.5 ms is acceptable.

### 12. C vs Rust: fair to compare? Yes — same parameters. (36:28–38:10)

**Royce (36:28)**
I don't know whether this should be included in the report — I'm comparing between C and Rust; I'm not sure whether it's a fair comparison.

**Wang (36:44)**
If the parameters are the same — then why not? You can report it.

**Royce (36:53)**
Some operations are slightly faster, some slightly slower.

**Royce (37:07)**
It's interesting: for key generation, Sign, Verify and PreSign, Rust is a bit slower; but for PreVerify and Extract… the difference is almost the same.

**Wang (37:28)**
I think it's okay — not that huge, right?

**Royce (37:31)**
It's not drastic.

**Wang (37:33)**
Yes — just include it. It's not bad.

**Royce (37:36)**
The maximum is about 1.07×…

**Wang (37:40)**
…for key generation, right? Okay. I mean it's fine, not that huge. And you implemented the Extract function too, right? Okay — I think it's fine.

### 13. The ECDSA baseline's API includes packing: 3–4-column layout (38:10–40:02)

**Royce (38:10)**
The only thing I still have to look at: the ECDSA API includes packing/unpacking [internally], so I should adjust the comparison.

**Wang (38:24)**
Yes — against the packed [tier].

**Royce (38:28)**
Its numbers will probably be larger than these, because this is probably still core-algorithm time.

**Wang (38:36)**
This is ~90 µs — it's fine; it's still under a millisecond.

**Royce (38:48)**
This number I still need to update, because I'm afraid it includes packing in it.

**Wang (38:54)**
Then check it and, for the comparison, you can add **3 or 4 columns**: for LAS, one column without packing/unpacking and one with; for ECDSA…

**Royce (39:14)**
But if the ECDSA one includes packing, then the comparison should be…

**Wang (39:20)**
It's still fine. You just say: this [ECDSA column] is with packing — you don't want to modify the ECDSA library, do you? So you just report that.

**Wang (39:28)**
For LAS you still have the two options: with packing and without packing.

**Royce (39:37)**
Even this [ECDSA] one is probably [packed]…

**Wang (39:38)**
Even if it's packed — in practice people can always do it, because that's not the crypto part; it's the implementation of the encoding and decoding.

### 14. This week's task list + start studying the atomic swap (39:54–41:20)

**Royce (39:54)**
The only thing missing is that I haven't made the table for the [pack/unpack results].

**Wang (40:02)**
Okay — good. Let's summarise. What you should do: update the remaining numbers, **update the tables and also the figures**. And — if you have time, only if you have time — you can start the next step: figure out how adaptor signatures are used in the atomic swap.

**Wang (40:34)**
Also investigate how the basic/classical adaptor signatures have been implemented in blockchains. Next week we can start discussing how to do it: first understand how it works, then check how people implemented the classical version of adaptor signatures in blockchains, and then we can discuss how to migrate that to the post-quantum version.

**Royce (41:15)**
For this [atomic-swap protocol], I've still got no idea.

**Wang (41:20)**
Yes, right — that's the new [stage].

### 15. The video: 6–8 minutes, highlight only the essentials (41:35–42:46)

**Royce (41:35)**
For the video — it's 6 to 8 minutes. I'm still unsure about it.

**Wang (41:43)**
What will you do — a video with a screencast you upload? What's your issue?

**Royce (41:49)**
It's only 6–8 minutes. How can I make the slides and explain everything in that time? It would have to be very high level.

**Wang (42:00)**
Yes — you should highlight the most important things. For example, the details you just mentioned — packing, encoding/decoding — maybe put the detail on the slide; when you're talking, highlight only the most important message. People can watch the screen and listen to you at the same time; when they're listening, you should be speaking to the most important points.

**Wang (42:34–42:40)**
That's the final thing — don't worry. Once you get to it, you can make a beautiful video/screencast.

### 16. Figures: export as PNG/PDF, fix font sizes (42:46–44:20)

**Royce (42:46)**
To include this in the report or the screencast — do I just screenshot it as-is?

**Wang (42:57)**
Can you save it as PNG — not the whole screen, just the two pictures? Can you save them?

**Royce (43:11)**
Just the [figures]?

**Wang (43:13)**
Yes — and then you add the explanation in your report. If you just put the [screenshot] PDF in your report, I don't think it will be very [readable].

**Royce (43:28)**
I'll extract these two without the [surrounding text].

**Wang (43:30)**
Yes, without the text. The worst case is a screenshot — it's fine — but ideally we should have some **high-quality**

**Royce (43:43)**
pictures, yes.

**Wang (43:44)**
High-quality pictures — save them as PNG or PDF.

**Wang (43:53–43:58)**
And check the **font size of the labels and also the legends** — when you put a diagram like this in your report, it can be very unclear; people can't read the figures. Okay.

### 17. How to start on the atomic swap; motivation history (44:20–46:23)

**Royce (44:20)**
How should I start learning the atomic swap?

**Wang (44:24)**
Just read the papers. Try to figure out how it works, and why we need it — you can tell me, in your opinion, why do we need the atomic swap?

**Royce (44:38)**
Because we want to remove the intermediary when we swap two different coins.

**Wang (44:48)**
Yes — that's part of the motivation. Check — maybe there are others. And then: before the adaptor signature, do we have other solutions? Say, the hash-locked solutions — you should compare them.

**Wang (45:10)**
You should read those solutions to understand why people proposed the adaptor signature — why people don't just use the other solutions. This is important:

**Wang (45:24)**
because finally you will write this in your report, and as a reader people should understand the motivation. You should read the **history of the atomic swap**: why we moved from one technique to another. What you said is very important — for an exchange, especially a cross-chain swap, we want to remove the central party. People first proposed hash-lock solutions [HTLCs], but it turned out they have some limitations, and that's why people proposed the adaptor signature.

**Wang (46:08)**
That's the context/background. It would be better that you learn it by heart — you will have a better understanding of the motivation.

### 18. Wrap-up: PR review, code check (46:23–47:08)

**Wang (46:23)**
I think that's it for today. I don't have other comments. So far you have done a good job. I might take some time to check the code if I have time, and we meet next week.

**Royce (46:37)**
I think I'll update the…

**Wang (46:39)**
Yes — just make sure the **PR is updated**.

**Royce (46:42)**
Yes — and I'll push the new updates.

**Wang (46:49)**
Every time you push new updates, the differences will be there. Okay, good. Any other questions?

### 19. The LAS author is Wang's collaborator (47:08–48:21)

**Royce (47:08)**
One more question — I read this from the author of the LAS [paper]…

**Wang (47:18)**
Yes — he's one of my collaborators; I've collaborated with him. If you have any specific questions about this, I can ask him directly. Is this from his own [work]?

**Royce (47:31)**
He's actually saying there is a limitation of adaptor signatures — a drawback of LAS…

**Wang (47:41)**
Okay.

**Royce (47:42)**
…and he's moving to [isogeny-based directions].

**Wang (47:44)**
Yes — that's why. But we are not that far; let's do this one first.

**Royce (47:53)**
His research question is: since LAS has imperfect correctness, he's moving [beyond it].

**Wang (48:00)**
Yes — let's finish this one first.

### 20. Understanding check and close (48:09–49:01)

**Royce (48:09)**
So I must study the atomic swap first, before I can implement it.

**Wang (48:13)**
Yes, of course — you should understand it before you code it.

**Royce (48:21)**
And for the adaptor signature — is my understanding good enough, or should I improve it?

**Wang (48:46)**
I think, yes — you can now write out the operations by yourself; you know how it works. Now you should understand how to integrate it into specific applications, like the atomic swap. Okay, good.

**Royce (49:01)**
Thank you so much.

---

## D. Corrected action items (= the Meeting-6 deliverables, due before the next meeting)

1. **Report timings in two clearly-separated cases**: core (no packing/unpacking) and with packing/unpacking. Never fold pack/unpack silently into the headline verification time; state explicitly that it is not the typical verification cost and can be done in advance/once.
2. **Add discussion of the pack/unpack cost**: it is an additional computation cost, application/deployment-dependent, amortisable (keys can be kept unpacked in memory, one-time cost); optimisation is possible (full-Dilithium-style compressed encodings) but deliberately out of scope — report the honest numbers.
3. **Update all tables AND figures with the pack/unpack (two-tier) numbers** that were measured but not yet tabulated. Include the ~160 µs → ~500 µs Adapt case; note 0.5 ms is acceptable versus seconds-level transaction latency.
4. **Measure and report the PreSign rejection-attempt count next to Sign's** (≈2.72 vs ≈2.77 expected), at the same public parameters, and use it to explain why PreSign takes longer.
5. **Report the C vs Rust comparison** — it is fair because the parameters are identical; include it with the observed ≤ ~1.07× spread (key generation largest).
6. **Classical ECDSA comparison: 3–4-column layout** — LAS without packing, LAS with packing, ECDSA at its own (packed) API — with an explicit annotation that the ECDSA library packs internally and is reused unmodified.
7. **Add the seed-vs-expanded-challenge discussion to the report**: storing only the 32-byte c̃ (FIPS-204 style) versus storing the expanded polynomial is a computation-vs-storage trade-off; this implementation follows FIPS 204 (store the seed, re-derive with SampleInBall).
8. **Export report figures as high-quality PNG/PDF** (no full-screen screenshots, no text-bearing screenshot PDFs) and **fix the font sizes of labels and legends** so figures are readable in the report.
9. **Keep the GitHub PR updated** (push regularly so Wang can review the diff).
10. **Start the atomic-swap study (Stage 2)** — only after the above: read how atomic swaps work; the history hash-locks (HTLC) → limitations → adaptor signatures; how classical adaptor signatures are deployed in blockchains; write this motivation into the report background. Next meeting: discuss migration to the post-quantum version.
11. **Video (6–8 min)**: highlight only the most important points; put detail on slides; final-stage task, not urgent.
12. *(Offer)* Specific questions about the LAS paper can be relayed by Wang to its author (his collaborator); the author's newer directions (imperfect correctness of LAS, isogeny-based schemes) are out of scope.

---

## E. Practical meaning for the project now

Wang signed off on the **correctness of the walkthrough** ("overall, the process looks correct") — the remaining Stage-1 work is *reporting*, not implementation:

1. two-tier timing everywhere (core vs with-pack/unpack), with the tier stated;
2. PreSign-vs-Sign attempt counts measured and reported;
3. C-vs-Rust table included (fair — same parameters);
4. classical comparison re-laid-out in 3–4 columns with the ECDSA packed-API caveat;
5. seed-vs-expanded challenge trade-off discussed;
6. figures re-exported at high quality with readable label/legend fonts;
7. PR kept up to date.

Then, and only then: study the atomic swap (HTLC history → adaptor signatures) as the entry to Stage 2.
