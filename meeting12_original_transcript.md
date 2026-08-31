
Transcript. Use arrow keys to navigate between transcript entries. Select an entry to navigate the media to the time of the entry.


Search

AI-generated content may be incorrect
RS

Royce Steven
0 minutes 7 seconds0:07
Royce Steven 0 minutes 7 seconds
Is my screen already visible?
ZW

Zhipeng Wang
0 minutes 10 seconds0:10
Zhipeng Wang 0 minutes 10 seconds
Yes, I can see it.
RS

Royce Steven
0 minutes 15 seconds0:15
Royce Steven 0 minutes 15 seconds
Uh, Shu I?
Royce Steven 0 minutes 18 seconds
Try the video first, after all.
ZW

Zhipeng Wang
0 minutes 24 seconds0:24
Zhipeng Wang 0 minutes 24 seconds
Uh, yeah, up to you, yeah.
RS

Royce Steven
0 minutes 26 seconds0:26
Royce Steven 0 minutes 26 seconds
Oh, okay.
ZW

Zhipeng Wang
0 minutes 27 seconds0:27
Zhipeng Wang 0 minutes 27 seconds
Yeah, okay, we can start this. Yeah, yeah, yeah, that's about this one. Maybe it's easier, yeah.
RS

Royce Steven
0 minutes 29 seconds0:29
Royce Steven 0 minutes 29 seconds
Okay.
Royce Steven 0 minutes 31 seconds
Okay, start. Okay. Hello, my name is Royce Steven. This project asks whether the signature behind cross-chain swaps can survive quantum computers and what it costs to find out.
Royce Steven 0 minutes 48 seconds
Bitcoin and Ethereum both use an elliptic curve signature and a large enough quantum computer could break the math it reyes on. The question has always been how large. These are published estimates for the same RSA
Royce Steven 1 minute 7 seconds
2048 target. The figure has fallen sharply and the newest one is the curve that the curve these two chains actually use. Nobody has built this hardware or what falls is the cost but the chain cannot wait is recorded going to be permanent and the fix is a change.
Royce Steven 1 minute 27 seconds
The whole network has to agree on.
Royce Steven 1 minute 32 seconds
Two people on two different blockchains want to trade, no exchange, nobody to trust. An atomic swap makes that safe. Either both transfer happen or neither does. What's the coins? They never leave their own chain. Each payment settles at home and what links the truth is a secret. The same idea under Pinsk.
Royce Steven 1 minute 55 seconds
Payment channel as well. What ties the two legs together is an adapter signature and all of this runs today on elliptic curve signatures. So why build a new one?
Royce Steven 2 minutes 11 seconds
Because A blockchain does not run on one signature, it runs on a stack at the bottom, the elliptic curve signature in yesterday, step one is already underway, migration to the to the standardized post quantum basic signature, but look where the route stops, basic signature.
Royce Steven 2 minutes 31 seconds
only authenticates a message and everything Jin does with signatures, swaps payment channels, lives in the layer above where the coverage is uneven.
Royce Steven 2 minutes 44 seconds
Uh, multi-signature are being built; the adapter case is this project, so the question is, can we build one and what does it cost, and will it change take it?
Royce Steven 2 minutes 56 seconds
Four functions.
Royce Steven 3 minutes
There are four functions of this latest based adapter signature. Resign produce a signature that is deliberately incomplete. It checked out against a public statement why, but the network will not accept it as you cannot spend it. Whoever knows the matching secret can adapt.
Royce Steven 3 minutes 18 seconds
It into an ordinary signature, and...
Royce Steven 3 minutes 22 seconds
That one goes on chain looking like a normal payment. So subtract, here's the trick. So subtract the complete incomplete signature from the complete one that just appeared, and the secret will fall out, claiming one leg unlocks the other.
Royce Steven 3 minutes 43 seconds
So, how do you turn a basic signature into an adapter one?
Royce Steven 3 minutes 47 seconds
Oh, I used the methodology not by inventing latest arithmetic at the bottom, deleting once primitive we use with zero upstream source functions modified.
Royce Steven 4 minutes 2 seconds
On the right, the one change that matters, the B signatures has the commitment W. Resign has W plus the statement Y. That substitution is what makes a signature adaptable. Then I build it again in Rust, a second independent implementation.
Royce Steven 4 minutes 21 seconds
To check on the first implementation, and the two agree byte by byte.
Royce Steven 4 minutes 29 seconds
Let me walk in. This is swap UTSO coin with a UTXO coin across 2 UTXO ledgers. The setting the last paper assumes is the abort gate.
Royce Steven 4 minutes 46 seconds
What commits nothing until the proof and Alice pre-signature boot check out.
Royce Steven 4 minutes 53 seconds
the tripwire, Bob tries to spend the pre-signature he holds and ordinary verification will refuse it. And then Alice, who knows the witness, completes Bob's pre-signatures and publices it.
Royce Steven 5 minutes 10 seconds
And then the leak.
Royce Steven 5 minutes 13 seconds
Bob needs nothing further from Alice. Here is that signature from Jane Tu.
Royce Steven 5 minutes 19 seconds
and the witness falls out and he claims the other coin.
Royce Steven 5 minutes 28 seconds
And then now is the cost. Everything hangs on one on what it is measures against two steps. Step one, classical to post quantum is the expensive one. And then the step organizations are.
Royce Steven 5 minutes 43 seconds
And then, yeah, the step organizations are already taking, and then step two is what I mentioned: post quantum basic to post quantum adapter, run back-to-back in the same session to limit drift, measuring just the cryptographic operations that the adapter overheads stays in a single digit.
Royce Steven 6 minutes 3 seconds
By comparison, the classical adapter design is 4.6 times its own sign and carries an extra proof, less overhead related to its own base. In absolute time, the classical one is still ahead and that...
Royce Steven 6 minutes 22 seconds
Is the step one?
Royce Steven 6 minutes 24 seconds
And then.
Royce Steven 6 minutes 27 seconds
The second result is the price of post quantum. Here is mostly not computation, but communications. Against a classical ECDSA adapter, the last signature is about 72 times higher, and yet it is the bytes that hurt here not the time.
Royce Steven 6 minutes 46 seconds
And the signature is almost all one object. The response is 99% of the signatures. A swap adds, and then a swap adds one more public object, the statement wise. So to make this cheaper, we do not need optimized algorithm.
Royce Steven 7 minutes 5 seconds
We need to optimize these two objects as our future work.
Royce Steven 7 minutes 15 seconds
So what does it look like on chain, on Bitcoin, on ordinary transaction? The same fields as any payment, the signatures traveling in the witness. No swap script, no hash lock, no shared hash and no adapter specific marker. And one slot simply gets bigger.
Royce Steven 7 minutes 34 seconds
on Ethereum is not further ahead here. It is a different in kind. Its signature field is still elliptic curve and latest signature writes in the input data as an argument to a contract, which is why one fan units are deployed and the other needs a
Royce Steven 7 minutes 53 seconds
Consensus Road.
Royce Steven 7 minutes 57 seconds
The second demonstration is settlement fits Bitcoin's size limit comfortably, but normal node refuses to pass it on and yet put in a block. The same stock software accepts the block. So fitting the size limit is not enough.
Royce Steven 8 minutes 17 seconds
the relay policy and a span carrying no elliptic curve signature settled a whole 2 leg swap across the chains.
Royce Steven 8 minutes 32 seconds
The negative cases I tried are rejected only by the patch node, and that difference is the evidence that the neural is really checking the last signature.
Royce Steven 8 minutes 47 seconds
Will a real change take it? Three things settled by measurement. Does the full verification fit in one transaction? At the Lithium 3, a real client might hold claim at 97.8% of the per transaction cap one measured instance.
Royce Steven 9 minutes 7 seconds
Uh, it's close to the edge. Uh, does the adapters uh functionally does the adapter functionally need a simple like simplified uh...
Royce Steven 9 minutes 18 seconds
Base, I assume so, and experiment often that assumption present and verified do have to change, but the unmodified FIPS 20.4 verified accepts the adapted signature without any modification and to optimizations I hope will help.
Royce Steven 9 minutes 38 seconds
Where run run run?
Royce Steven 9 minutes 42 seconds
Yeah, and this is the the optimizations that I I hope would help.
Royce Steven 9 minutes 50 seconds
which is compressing the signature and statement Y.
Royce Steven 9 minutes 58 seconds
Back to the three questions, can we build one? It is yes in two languages as the one that I implemented. And what does it cost? A single digits in adapter computation, but it's 72 times the bytes. And will the chain take it on Ethereum on
Royce Steven 10 minutes 18 seconds
through a contract and on Bitcoin only with a new rule. So it's less feasible for the adapted player. It is, yes, as something to deploy.
Royce Steven 10 minutes 32 seconds
For the adapter layer, it's yes, but for something to deploy at not at the moment, and what's what stops the the the the deployment is not the adapter layer, so the three recommendations for a Bitcoin is analyze the consensus rule before anyone sees it, and for...
Royce Steven 10 minutes 51 seconds
Ethereum Dr. the verification cost down. It fits today with almost no margin for the lithium trees.
Royce Steven 10 minutes 59 seconds
this standard security parameter and for the protocol designers, the budget, the proof and the statement first should be optimized, not the adapter. That's all from me. Thank you.
ZW
Zhipeng Wang
11 minutes 12 seconds11:12
Zhipeng Wang 11 minutes 12 seconds
Okay, good, good. I think the content is okay, but my concern is that it's a bit too long. I mean, yeah, you should compress it to less than 8 minutes, right? I mean, I maybe I haven't accounted the time, but it's more than 10 minutes so far.
RS
Royce Steven
11 minutes 20 seconds11:20
Royce Steven 11 minutes 20 seconds
Too long.
Royce Steven 11 minutes 26 seconds
Yeah.
Royce Steven 11 minutes 32 seconds
Ops.
ZW
Zhipeng Wang
11 minutes 32 seconds11:32
Zhipeng Wang 11 minutes 32 seconds
Okay, yeah, you should be faster or maybe you should remove some details.
RS
Royce Steven
11 minutes 33 seconds11:33
Royce Steven 11 minutes 33 seconds
Okay.
Royce Steven 11 minutes 35 seconds
Faster.
Royce Steven 11 minutes 38 seconds
Give us some details, OK?
ZW
Zhipeng Wang
11 minutes 39 seconds11:39
Zhipeng Wang 11 minutes 39 seconds
Okay, good. And besides that, I think, yeah, you have addressed most of the questions I mentioned before, so I don't have further comments so far. But it may be you could record a video and also share with me by the deadline. So I will be back next Monday. Yeah, I can have.
RS
Royce Steven
11 minutes 54 seconds11:54
Royce Steven 11 minutes 54 seconds
Okay.
ZW
Zhipeng Wang
12 minutes 1 second12:01
Zhipeng Wang 12 minutes 1 second
I can give you some comments next Monday. Yeah, if you want.
RS
Royce Steven
12 minutes 4 seconds12:04
Royce Steven 12 minutes 4 seconds
Okay.
Royce Steven 12 minutes 6 seconds
Oh, okay. Smiley.
ZW
Zhipeng Wang
12 minutes 8 seconds12:08
Zhipeng Wang 12 minutes 8 seconds
Yeah, do you have any questions regarding the slides or regarding the things you presented?
RS
Royce Steven
12 minutes 14 seconds12:14
Royce Steven 12 minutes 14 seconds
Does it need more for the... Is it official enough for this? Oh, sorry, I think...
ZW
Zhipeng Wang
12 minutes 19 seconds12:19
Zhipeng Wang 12 minutes 19 seconds
No, no, no, no, I cannot say it now. Please start sharing.
RS
Royce Steven
12 minutes 22 seconds12:22
Royce Steven 12 minutes 22 seconds
Soe.
Royce Steven 12 minutes 31 seconds
Is it showing not?
ZW
Zhipeng Wang
12 minutes 32 seconds12:32
Zhipeng Wang 12 minutes 32 seconds
Yes, I can see.
RS
Royce Steven
12 minutes 39 seconds12:39
Royce Steven 12 minutes 39 seconds
Is does it need more more like visual or a logo or a more diagram or visuals or?
ZW
Zhipeng Wang
12 minutes 52 seconds12:52
Zhipeng Wang 12 minutes 52 seconds
Which one?
RS
Royce Steven
12 minutes 53 seconds12:53
Royce Steven 12 minutes 53 seconds
Or does it?
ZW
Zhipeng Wang
12 minutes 53 seconds12:53
Zhipeng Wang 12 minutes 53 seconds
I think now it's better, right? You can, we can see, OK, we got some automics web.
Zhipeng Wang 12 minutes 59 seconds
Uh, okay.
RS
Royce Steven
12 minutes 59 seconds12:59
Royce Steven 12 minutes 59 seconds
Is it feasible enough?
ZW
Zhipeng Wang
13 minutes 3 seconds13:03
Zhipeng Wang 13 minutes 3 seconds
Okay, let me have a look. Yeah, it's okay. It's okay. Maybe, I'm not sure, have you already mentioned that? Maybe you could also say that, okay, the post content migration is not only for blockchains. We have already observed some big companies.
Zhipeng Wang 13 minutes 24 seconds
They are currently moving to post-content version.
RS
Royce Steven
13 minutes 25 seconds13:25
Royce Steven 13 minutes 25 seconds
Mhm.
ZW
Zhipeng Wang
13 minutes 29 seconds13:29
Zhipeng Wang 13 minutes 29 seconds
Have you mentioned this? Yeah, maybe, I mean, that's again, this is one of the motivations, alright, 'cause nowadays people people say that poster quantum is very important, so yeah, we should do many companies are replacing their existing signatures with the with the.
RS
Royce Steven
13 minutes 35 seconds13:35
Royce Steven 13 minutes 35 seconds
Yeah.
ZW
Zhipeng Wang
13 minutes 48 seconds13:48
Zhipeng Wang 13 minutes 48 seconds
Post content version, so we should also do that for this, but again, I think it's better. I'm not sure if you would like to add more or more visualization. If you add more, maybe it will make each slide very, I don't know.
Zhipeng Wang 14 minutes 4 seconds
Too much content in one slide, yeah, yeah, yeah. Imagine that how many slides we have in total, 13, right? So, for example, if we want to, I mean, of course, I know the background, then I can easily capture the main or the most important ideas in this slide, but I assume that some people they are out of.
RS
Royce Steven
14 minutes 6 seconds14:06
Royce Steven 14 minutes 6 seconds
Too much, too much work, I guess.
Royce Steven 14 minutes 21 seconds
Yeah.
ZW
Zhipeng Wang
14 minutes 23 seconds14:23
Zhipeng Wang 14 minutes 23 seconds
They are not, they're not doing something in this field. Maybe they, it will be very challenging for them to understand, okay, which one is the most important? For example, here there are three numbers, right? Each year, okay, which one is the most important one?
RS
Royce Steven
14 minutes 31 seconds14:31
Royce Steven 14 minutes 31 seconds
Yeah.
Royce Steven 14 minutes 39 seconds
Okay, for the...
ZW
Zhipeng Wang
14 minutes 40 seconds14:40
Zhipeng Wang 14 minutes 40 seconds
Okay, I got it. I got it. I think if you would like to add more visualizations.
Zhipeng Wang 14 minutes 45 seconds
Maybe you should remove some content.
RS
Royce Steven
14 minutes 47 seconds14:47
Royce Steven 14 minutes 47 seconds
Okay, so for the title, is it, should I change it because it still says exotic signatures schemes?
ZW
Zhipeng Wang
14 minutes 57 seconds14:57
Zhipeng Wang 14 minutes 57 seconds
Yeah, uh, by the way, for this, I I was want to uh confirm now you you have the freedom to change your to update your title, right?
RS
Royce Steven
15 minutes 6 seconds15:06
Royce Steven 15 minutes 6 seconds
So, more like implementing latest base additive.
ZW
Zhipeng Wang
15 minutes 12 seconds15:12
Zhipeng Wang 15 minutes 12 seconds
Yeah, yeah, yeah, I think you can be more precise.
RS
Royce Steven
15 minutes 13 seconds15:13
Royce Steven 15 minutes 13 seconds
And.
Royce Steven 15 minutes 16 seconds
Oh, okay.
ZW
Zhipeng Wang
15 minutes 17 seconds15:17
Zhipeng Wang 15 minutes 17 seconds
Maybe latest based is too, I don't know, too specific because people don't know what is latest. Maybe you could say post a quantum secure adapt signature.
RS
Royce Steven
15 minutes 21 seconds15:21
Royce Steven 15 minutes 21 seconds
Too specific.
ZW
Zhipeng Wang
15 minutes 29 seconds15:29
Zhipeng Wang 15 minutes 29 seconds
Right, you.
RS
Royce Steven
15 minutes 30 seconds15:30
Royce Steven 15 minutes 30 seconds
adapter signature.
ZW
Zhipeng Wang
15 minutes 31 seconds15:31
Zhipeng Wang 15 minutes 31 seconds
Yeah, just remove the exotic signature schemes by referring to the adapted signature.
RS
Royce Steven
15 minutes 35 seconds15:35
Royce Steven 15 minutes 35 seconds
Exotic.
Royce Steven 15 minutes 40 seconds
secure adapter signature scheme in blockchains.
ZW
Zhipeng Wang
15 minutes 44 seconds15:44
Zhipeng Wang 15 minutes 44 seconds
Yes, yes.
RS
Royce Steven
15 minutes 47 seconds15:47
Royce Steven 15 minutes 47 seconds
And...
Royce Steven 15 minutes 50 seconds
I think I also...
Royce Steven 15 minutes 53 seconds
Wanna get, could you give me some feedback to on my introductory material?
ZW
Zhipeng Wang
16 minutes 1 second16:01
Zhipeng Wang 16 minutes 1 second
The what?
RS
Royce Steven
16 minutes 1 second16:01
Royce Steven 16 minutes 1 second
Because it's like 20% of the weighting of the mark, it asks that I should clearly describe the project setting, scoping the subject area with proper presentation and figures and stating the objectives. And does the work effectively?
Royce Steven 16 minutes 20 seconds
effectively establish the context why this project matters and clearly explain the subject area what this project is about with proper citations or figures. And are the project objectives clearly stated, coherent and appropriate?
ZW
Zhipeng Wang
16 minutes 31 seconds16:31
Zhipeng Wang 16 minutes 31 seconds
So which one are you referring to? You are talking about the slides or you are talking about a report?
RS
Royce Steven
16 minutes 37 seconds16:37
Royce Steven 16 minutes 37 seconds
Um...
Royce Steven 16 minutes 39 seconds
Both, I guess, and I think more of the report, because the report is like eighty-five percent, I believe, of total, yeah.
ZW
Zhipeng Wang
16 minutes 45 seconds16:45
Zhipeng Wang 16 minutes 45 seconds
Yeah, I think to address, you are you are talking about the importance of this project, right? What application?
RS
Royce Steven
16 minutes 52 seconds16:52
Royce Steven 16 minutes 52 seconds
Um, yes, but I think I haven't uh asked.
Royce Steven 16 minutes 58 seconds
Like, in detail, the feedback regarding to this case.
ZW
Zhipeng Wang
17 minutes 5 seconds17:05
Zhipeng Wang 17 minutes 5 seconds
The what?
RS
Royce Steven
17 minutes 6 seconds17:06
Royce Steven 17 minutes 6 seconds
I believe it's the introduction, I believe, chapter.
ZW
Zhipeng Wang
17 minutes 10 seconds17:10
Zhipeng Wang 17 minutes 10 seconds
Okay.
Zhipeng Wang 17 minutes 14 seconds
So my, I was planning to give you some comments by end of this week, but I can give you some now because I would add more detailed comments on the Overleaf directly, but I can give you some high level comments so far. So again, as I said before, so maybe.
RS
Royce Steven
17 minutes 19 seconds17:19
Royce Steven 17 minutes 19 seconds
Okay.
Royce Steven 17 minutes 43 seconds
Yeah.
ZW
Zhipeng Wang
17 minutes 44 seconds17:44
Zhipeng Wang 17 minutes 44 seconds
signatures. So now I can see that you have already mentioned that the post quantum security is important.
Zhipeng Wang 17 minutes 55 seconds
And you also need to, okay, let me have a look. Maybe the diagram could be, let me have a look. Basic postcode signatures. Okay.
Zhipeng Wang 18 minutes 9 seconds
Pause the content.
Zhipeng Wang 18 minutes 12 seconds
Open the dass.
Zhipeng Wang 18 minutes 23 seconds
To be honest, for the first finger, for first diagram, our first finger, finger 1.1, right? The first part, part A, let me share with you my screen maybe.
RS
Royce Steven
18 minutes 29 seconds18:29
Royce Steven 18 minutes 29 seconds
Yeah.
Royce Steven 18 minutes 31 seconds
Mhm.
Royce Steven 18 minutes 34 seconds
Yeah.
ZW
Zhipeng Wang
18 minutes 38 seconds18:38
Zhipeng Wang 18 minutes 38 seconds
Can I see backup? Can I see my screen?
RS
Royce Steven
18 minutes 40 seconds18:40
Royce Steven 18 minutes 40 seconds
Yes, I guess.
ZW
Zhipeng Wang
18 minutes 41 seconds18:41
Zhipeng Wang 18 minutes 41 seconds
So here, so maybe I think you could make it more
Zhipeng Wang 18 minutes 49 seconds
closer, much closer to Bitcoin or to meet to blockchain itself, right? Here you are talking about RSA, right? But RSR Bitcoins, they are nowadays Bitcoins and also Ethereum, they are not using RSA, right? They're using other signatures.
RS
Royce Steven
18 minutes 56 seconds18:56
Royce Steven 18 minutes 56 seconds
Mhm.
Royce Steven 18 minutes 58 seconds
Mhm.
Royce Steven 19 minutes 4 seconds
Tu.
Royce Steven 19 minutes 7 seconds
Yeah, Dan, what?
ZW
Zhipeng Wang
19 minutes 10 seconds19:10
Zhipeng Wang 19 minutes 10 seconds
Maybe if you replace some more concurrent things like that, then okay, it would be more relevant, right?
RS
Royce Steven
19 minutes 10 seconds19:10
Royce Steven 19 minutes 10 seconds
Yes.
Royce Steven 19 minutes 17 seconds
Okay.
ZW
Zhipeng Wang
19 minutes 18 seconds19:18
Zhipeng Wang 19 minutes 18 seconds
Here is more like, we have the high level picture like this. Maybe you could also say, ideally, you could cite some fingers. I believe that there are some reports. They have the research, they have done some research regarding the.
RS
Royce Steven
19 minutes 33 seconds19:33
Royce Steven 19 minutes 33 seconds
Mhm.
ZW
Zhipeng Wang
19 minutes 35 seconds19:35
Zhipeng Wang 19 minutes 35 seconds
development of quantum computers and it was when it will become true or when it will become realistic. So you could also consider the security of counter blockchains and also the timing regarding the development of the quantum computers. And then we will see maybe
RS
Royce Steven
19 minutes 48 seconds19:48
Royce Steven 19 minutes 48 seconds
Mhm.
ZW
Zhipeng Wang
19 minutes 54 seconds19:54
Zhipeng Wang 19 minutes 54 seconds
in how many years? Okay, it will be maybe just the current signature schemes will be secure. So here you can see the motivation, the urgency of the replacement or the improvement of existing signatures.
RS
Royce Steven
20 minutes 4 seconds20:04
Royce Steven 20 minutes 4 seconds
Yeah.
Royce Steven 20 minutes 11 seconds
Okay.
ZW
Zhipeng Wang
20 minutes 12 seconds20:12
Zhipeng Wang 20 minutes 12 seconds
So, here is is is OK, because after I read the things you you you have written is fine, but the first thing I I will say is regarding figure one, so we're going to figure one, maybe after I say it.
Zhipeng Wang 20 minutes 26 seconds
Uh, it's not that straightforward, let's say, not that direct to convey the things you would like to see.
RS
Royce Steven
20 minutes 32 seconds20:32
Royce Steven 20 minutes 32 seconds
Yeah.
Royce Steven 20 minutes 35 seconds
I SE.
ZW
Zhipeng Wang
20 minutes 36 seconds20:36
Zhipeng Wang 20 minutes 36 seconds
OK, this is regarding figure one. OK, uh, and subject here of ladies, ladies signature, DDS, and whatever signatures.
Zhipeng Wang 20 minutes 47 seconds
A.
Zhipeng Wang 20 minutes 51 seconds
And here you also need to let me check. Why did you mention the last construction?
RS
Royce Steven
21 minutes21:00
Royce Steven 21 minutes
Thing is in methodology.
ZW
Zhipeng Wang
21 minutes 1 second21:01
Zhipeng Wang 21 minutes 1 second
What, what, what did you, when did you, when did you say, where did you, where do you say the importance of the adaptive signature application of the adaptive signature?
RS
Royce Steven
21 minutes 13 seconds21:13
Royce Steven 21 minutes 13 seconds
Rob, I think I have not. This is for Shu.
ZW
Zhipeng Wang
21 minutes 20 seconds21:20
Zhipeng Wang 21 minutes 20 seconds
I can see that here, right? You have mentioned the...
Zhipeng Wang 21 minutes 26 seconds
The subject error, of course, stay important, but here I want to say, OK, OK, OK, anyhow, anyway, you, you mentioned that, OK, adapt signature then.
RS
Royce Steven
21 minutes 26 seconds21:26
Royce Steven 21 minutes 26 seconds
Yeah.
Royce Steven 21 minutes 36 seconds
Yeah.
ZW
Zhipeng Wang
21 minutes 36 seconds21:36
Zhipeng Wang 21 minutes 36 seconds
Why is it important for branches?
RS
Royce Steven
21 minutes 38 seconds21:38
Royce Steven 21 minutes 38 seconds
Why is important?
ZW
Zhipeng Wang
21 minutes 40 seconds21:40
Zhipeng Wang 21 minutes 40 seconds
You have one or three sentences here, but I think you would be better to extend it. Otherwise, people were just talking about something very abstract.
RS
Royce Steven
21 minutes 41 seconds21:41
Royce Steven 21 minutes 41 seconds
Yeah.
Royce Steven 21 minutes 48 seconds
Yeah.
Royce Steven 21 minutes 51 seconds
Yeah.
ZW
Zhipeng Wang
21 minutes 51 seconds21:51
Zhipeng Wang 21 minutes 51 seconds
We need some concrete motivation, the concrete examples.
RS
Royce Steven
21 minutes 54 seconds21:54
Royce Steven 21 minutes 54 seconds
Yeah.
ZW
Zhipeng Wang
21 minutes 55 seconds21:55
Zhipeng Wang 21 minutes 55 seconds
OK, well, this one you have to set more things here.
RS
Royce Steven
22 minutes22:00
Royce Steven 22 minutes
So, it's how many pages? It's page 17.
ZW
Zhipeng Wang
22 minutes 5 seconds22:05
Zhipeng Wang 22 minutes 5 seconds
Yeah, there are so many pages.
RS
Royce Steven
22 minutes 7 seconds22:07
Royce Steven 22 minutes 7 seconds
Only, only, only four pages of them. Introduction, only page 13 to page 17.
ZW
Zhipeng Wang
22 minutes 14 seconds22:14
Zhipeng Wang 22 minutes 14 seconds
Sorry, I cannot hear you. Can I say it again?
RS
Royce Steven
22 minutes 17 seconds22:17
Royce Steven 22 minutes 17 seconds
Hi, hello, can you can you hear me? Yeah, the introduction is only page 13 to page 17, so it's only four pages, and when I saw see the rubric, it's 20% of the report components, so I'm I'm a bit...
ZW
Zhipeng Wang
22 minutes 18 seconds22:18
Zhipeng Wang 22 minutes 18 seconds
Yeah, I can hear you now.
RS
Royce Steven
22 minutes 35 seconds22:35
Royce Steven 22 minutes 35 seconds
Concerned that I'm not doing.
Royce Steven 22 minutes 38 seconds
Good enough on the introduction.
ZW
Zhipeng Wang
22 minutes 42 seconds22:42
Zhipeng Wang 22 minutes 42 seconds
Yeah.
Zhipeng Wang 22 minutes 44 seconds
Let me have a look. But you don't have the background, right? Do you have the background section?
RS
Royce Steven
22 minutes 49 seconds22:49
Royce Steven 22 minutes 49 seconds
Because it says that in the new report format, there is no separate background section. Instead, the introduction should present the subject area clearly and include a concise literature review.
ZW
Zhipeng Wang
22 minutes 59 seconds22:59
Zhipeng Wang 22 minutes 59 seconds
Yeah, then you should extend a bit for the subject area, right? If you don't have a specific background section, then you should introduce the important background in the introduction section.
RS
Royce Steven
23 minutes 5 seconds23:05
Royce Steven 23 minutes 5 seconds
Mhm, mhm.
Royce Steven 23 minutes 10 seconds
In the introduction.
Royce Steven 23 minutes 13 seconds
And the focus should be on depth rather than breadth, highlighting key works necessary to understand the problem and justify your approach.
Royce Steven 23 minutes 22 seconds
An an extensive review is does not required.
ZW
Zhipeng Wang
23 minutes 28 seconds23:28
Zhipeng Wang 23 minutes 28 seconds
But for me, I think you should at least give some instruction regarding applications of adaptive signature.
RS
Royce Steven
23 minutes 33 seconds23:33
Royce Steven 23 minutes 33 seconds
Adapter signature, because yeah, why it's important.
ZW
Zhipeng Wang
23 minutes 35 seconds23:35
Zhipeng Wang 23 minutes 35 seconds
I mean, again, you should always follow the two important motivations, right? You have already done a good job in regarding the post quantum security motivation, but you also need to introduce why we should, for this project, we will focus on adapt signature, right? Yeah, as I, as I said before, right, there are basic signatures.
RS
Royce Steven
23 minutes 48 seconds23:48
Royce Steven 23 minutes 48 seconds
Yeah.
Royce Steven 23 minutes 50 seconds
And the third signature, yeah.
ZW
Zhipeng Wang
23 minutes 55 seconds23:55
Zhipeng Wang 23 minutes 55 seconds
And you have already mentioned that there is exotic signatures. OK, for exotic signatures, people have already implemented that their post quantum implementation in practice. But for the even for the exotic signatures, there are still a lot of types of exotic signatures, right? So why would you like to, why did you choose adaptive signature? You should give the importance, you should give the
RS
Royce Steven
24 minutes 3 seconds24:03
Royce Steven 24 minutes 3 seconds
Mhm.
Royce Steven 24 minutes 10 seconds
Yeah.
Royce Steven 24 minutes 13 seconds
Okay.
ZW
Zhipeng Wang
24 minutes 16 seconds24:16
Zhipeng Wang 24 minutes 16 seconds
motivation here, right? So here I cannot say it very specifically, very, you are not writing it in a very empty safe way. So yeah, just to try to make it more.
RS
Royce Steven
24 minutes 17 seconds24:17
Royce Steven 24 minutes 17 seconds
Yeah.
Royce Steven 24 minutes 26 seconds
Okay.
Royce Steven 24 minutes 32 seconds
Okay.
ZW
Zhipeng Wang
24 minutes 32 seconds24:32
Zhipeng Wang 24 minutes 32 seconds
Easy to read. OK, and here it's regular objectives. OK, yeah, you have at least five objectives, not bad.
RS
Royce Steven
24 minutes 43 seconds24:43
Royce Steven 24 minutes 43 seconds
Yeah.
ZW
Zhipeng Wang
24 minutes 44 seconds24:44
Zhipeng Wang 24 minutes 44 seconds
Like contributions.
RS
Royce Steven
24 minutes 47 seconds24:47
Royce Steven 24 minutes 47 seconds
Yeah.
ZW
Zhipeng Wang
24 minutes 47 seconds24:47
Zhipeng Wang 24 minutes 47 seconds
structure.
Zhipeng Wang 24 minutes 50 seconds
Last construction methods. Okay, I think for the, yeah, for the introduction, again, please extend a bit regarding adaptive signature. And maybe I will have more detailed comments later. I will try to add them maybe early next week, give you some time to update them.
RS
Royce Steven
25 minutes25:00
Royce Steven 25 minutes
And if there's...
Royce Steven 25 minutes 3 seconds
Yeah.
Royce Steven 25 minutes 8 seconds
Mm.
Royce Steven 25 minutes 11 seconds
Oh, okay.
ZW
Zhipeng Wang
25 minutes 12 seconds25:12
Zhipeng Wang 25 minutes 12 seconds
And regarding the methods, let's okay for today, sorry, I will have another meeting with in 10 minutes, but so let's go through it very quickly, but I will give you more comments. Or if you want, we can also have another meeting before the deadline next week, because I will be back next Monday.
RS
Royce Steven
25 minutes 17 seconds25:17
Royce Steven 25 minutes 17 seconds
Okay.
Royce Steven 25 minutes 19 seconds
Okay.
Royce Steven 25 minutes 25 seconds
Okay.
ZW
Zhipeng Wang
25 minutes 30 seconds25:30
Zhipeng Wang 25 minutes 30 seconds
But, anyway, this is for the motivation, and for the methods, I would like to say the structure at first, so we have the last construction, it can be straight edges, I intend to see the rest.
RS
Royce Steven
25 minutes 33 seconds25:33
Royce Steven 25 minutes 33 seconds
Mhm.
Royce Steven 25 minutes 39 seconds
Mhm.
ZW
Zhipeng Wang
25 minutes 49 seconds25:49
Zhipeng Wang 25 minutes 49 seconds
For.
Zhipeng Wang 25 minutes 51 seconds
Patient methods.
Zhipeng Wang 25 minutes 55 seconds
Evaluation results.
Zhipeng Wang 26 minutes
But what, what's the difference between...
Zhipeng Wang 26 minutes 3 seconds
Section 3 and Section 4.
RS
Royce Steven
26 minutes 8 seconds26:08
Royce Steven 26 minutes 8 seconds
Section 4.
ZW
Zhipeng Wang
26 minutes 8 seconds26:08
Zhipeng Wang 26 minutes 8 seconds
The evaluation is regarding the overall project.
RS
Royce Steven
26 minutes 13 seconds26:13
Royce Steven 26 minutes 13 seconds
I think in the section 4, I put evaluation and then achievement of the objectives, implementation challenges and limitations.
ZW
Zhipeng Wang
26 minutes 20 seconds26:20
Zhipeng Wang 26 minutes 20 seconds
Okay, okay, okay, got it, got it.
RS
Royce Steven
26 minutes 26 seconds26:26
Royce Steven 26 minutes 26 seconds
Uh, it's because they also ask about uh...
ZW
Zhipeng Wang
26 minutes 27 seconds26:27
Zhipeng Wang 26 minutes 27 seconds
Okay.
RS
Royce Steven
26 minutes 31 seconds26:31
Royce Steven 26 minutes 31 seconds
Twenty percent of the report is project achievement.
ZW
Zhipeng Wang
26 minutes 34 seconds26:34
Zhipeng Wang 26 minutes 34 seconds
OKOK.
Zhipeng Wang 26 minutes 36 seconds
OKOK.
Zhipeng Wang 26 minutes 41 seconds
We got a message that we haven't, so method, so...
RS
Royce Steven
26 minutes 46 seconds26:46
Royce Steven 26 minutes 46 seconds
20% is evaluation and or reflection.
ZW
Zhipeng Wang
26 minutes 52 seconds26:52
Zhipeng Wang 26 minutes 52 seconds
Okay.
Zhipeng Wang 26 minutes 55 seconds
But again, regarding details, regarding detailed sections, I will give you more details, detailed happens later, but let's check the regarding the structure of all. OK, it's good that you have the critical reflection. I feel sure it would be different. I think, yeah, the structure looks OK for me. I mean, maybe you.
RS
Royce Steven
26 minutes 59 seconds26:59
Royce Steven 26 minutes 59 seconds
Yeah.
ZW
Zhipeng Wang
27 minutes 13 seconds27:13
Zhipeng Wang 27 minutes 13 seconds
Maybe you should change your bit regarding title, regarding for the subsections, but I will give you some comments later. But in general, you have already right in the things I would like to say, for example, regarding a...
RS
Royce Steven
27 minutes 16 seconds27:16
Royce Steven 27 minutes 16 seconds
Yeah.
Royce Steven 27 minutes 21 seconds
Okay.
ZW
Zhipeng Wang
27 minutes 28 seconds27:28
Zhipeng Wang 27 minutes 28 seconds
contributions, objectives, and it was some important theory in conclusions and critical reflection.
Zhipeng Wang 27 minutes 36 seconds
Okay.
Zhipeng Wang 27 minutes 40 seconds
And regarding the citations, let me have a look.
Zhipeng Wang 27 minutes 44 seconds
How many citations do you have?
Zhipeng Wang 27 minutes 48 seconds
Kay.
Zhipeng Wang 27 minutes 50 seconds
28. Okay, not bad.
Zhipeng Wang 28 minutes 12 seconds
challenges.
Zhipeng Wang 28 minutes 17 seconds
Okay, let's do this. I will read it in more detail and give you some comments. Which one, which way you prefer? Shall I just add some, for example, if I say something here, for example, I can add some.
RS
Royce Steven
28 minutes 18 seconds28:18
Royce Steven 28 minutes 18 seconds
Okay.
Royce Steven 28 minutes 20 seconds
Okay.
ZW
Zhipeng Wang
28 minutes 32 seconds28:32
Zhipeng Wang 28 minutes 32 seconds
some comments directly by using the view, or is it view, sorry. Is it this way, or you would like to me to highlight the text in the report directly? Which one you prefer?
RS
Royce Steven
28 minutes 44 seconds28:44
Royce Steven 28 minutes 44 seconds
Uh, which one is here for you to do?
ZW
Zhipeng Wang
28 minutes 46 seconds28:46
Zhipeng Wang 28 minutes 46 seconds
Sorry, which one you do? I'm fine with both.
RS
Royce Steven
28 minutes 49 seconds28:49
Royce Steven 28 minutes 49 seconds
Oh, okay.
Royce Steven 28 minutes 52 seconds
I think just highlight, I think.
ZW
Zhipeng Wang
28 minutes 55 seconds28:55
Zhipeng Wang 28 minutes 55 seconds
I like it.
RS
Royce Steven
28 minutes 55 seconds28:55
Royce Steven 28 minutes 55 seconds
And then, and put some comment.
ZW
Zhipeng Wang
28 minutes 59 seconds28:59
Zhipeng Wang 28 minutes 59 seconds
For example, if I, I would like to say, okay, for example, I would say, okay, what's the...
Zhipeng Wang 29 minutes 5 seconds
Uh, what's the meaning of this one, right? That, oh, very late, but...
RS
Royce Steven
29 minutes 8 seconds29:08
Royce Steven 29 minutes 8 seconds
Okay, okay.
ZW
Zhipeng Wang
29 minutes 14 seconds29:14
Zhipeng Wang 29 minutes 14 seconds
Uh...
Zhipeng Wang 29 minutes 16 seconds
For example, this one, I would like to say, okay, what's this? What's the meaning of this one? Then shall I ask questions here or shall I just highlight it?
RS
Royce Steven
29 minutes 20 seconds29:20
Royce Steven 29 minutes 20 seconds
Okay.
Royce Steven 29 minutes 21 seconds
Oh, okay.
Royce Steven 29 minutes 25 seconds
Oh yeah, you can, yeah, just put comments so I can see what's wrong with it. Yeah.
ZW
Zhipeng Wang
29 minutes 31 seconds29:31
Zhipeng Wang 29 minutes 31 seconds
Yeah, okay, okay, okay. Maybe it's easy. Okay, good, good. Yeah, let me know. Oh, yeah, right. Another thing is that maybe you can also share with me your video once you have recorded it, and I can also give you some comments together.
RS
Royce Steven
29 minutes 34 seconds29:34
Royce Steven 29 minutes 34 seconds
Okay, yes, yeah.
Royce Steven 29 minutes 47 seconds
Yes.
Royce Steven 29 minutes 49 seconds
And okay, I will send the feed you and the slide as well.
ZW
Zhipeng Wang
29 minutes 54 seconds29:54
Zhipeng Wang 29 minutes 54 seconds
Yeah, maybe by the end of this week, that would be helpful, then I will have time to to to watch it.
RS
Royce Steven
30 minutes30:00
Royce Steven 30 minutes
Oh, okay, so I probably like polish the report, the video and the slides by the end of by Friday, I guess.
ZW
Zhipeng Wang
30 minutes 8 seconds30:08
Zhipeng Wang 30 minutes 8 seconds
Okay, okay, then I will have some time maybe Sunday or Monday to read them and then I will give you some comments.
RS
Royce Steven
30 minutes 15 seconds30:15
Royce Steven 30 minutes 15 seconds
Yeah, is it okay if I polish it until Friday, 5pm, I guess?
ZW
Zhipeng Wang
30 minutes 20 seconds30:20
Zhipeng Wang 30 minutes 20 seconds
Yeah, it should be fine, fine, fine. It's fine. It's totally fine. I will only be available on Sunday, so you have time to work on it.
RS
Royce Steven
30 minutes 22 seconds30:22
Royce Steven 30 minutes 22 seconds
Yeah.
Royce Steven 30 minutes 25 seconds
Ohh...
Royce Steven 30 minutes 27 seconds
Yeah, so I'll send you the recording on Friday as well, hopefully.
ZW
Zhipeng Wang
30 minutes 32 seconds30:32
Zhipeng Wang 30 minutes 32 seconds
Okay, okay, yes, yes. And the deadline is next Thursday, right?
RS
Royce Steven
30 minutes 36 seconds30:36
Royce Steven 30 minutes 36 seconds
The deadline is of 4th of September.
Royce Steven 30 minutes 43 seconds
On Friday.
ZW
Zhipeng Wang
30 minutes 44 seconds30:44
Zhipeng Wang 30 minutes 44 seconds
Friday, okay. Then we could also have another meeting maybe on Thursday next week if you want. If you have already seen, if you see my comments, if you have any questions, we can also have another meeting next week. Okay, let's see, but let me first array it in more detail, okay?
RS
Royce Steven
30 minutes 55 seconds30:55
Royce Steven 30 minutes 55 seconds
Yes.
