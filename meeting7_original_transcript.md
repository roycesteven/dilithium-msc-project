# Meeting 7 - Royce / Zhipeng PGT Project (original transcript)

Recorded 2026-07-24 11:31 (Teams), duration 41:34, 209 caption entries.
Extracted from the transcript pane of the SharePoint Stream recording; wording is
Teams' uncorrected automatic speech recognition.

**Speaker attribution is NOT usable in this transcript.** Teams tagged all
209 entries with a single speaker (Royce Steven), i.e. it never
separated the two voices, so the labels below are the raw ASR labels and do not
identify who spoke. Meetings 1-6 came from a tool that diarised into
`Speaker 1` / `Speaker 2`; this one does not. Attribute turns from context when
writing the cleaned version - do not trust the label.

---

Unknown (--:--)
Royce Steven started transcription

Royce Steven (00:03)
And.

Royce Steven (00:04)
Mm.

Royce Steven (00:10)
For the atomic for the migration, is it like I migrate like someone's like Bitcoin or like wallet?

Royce Steven (00:22)
So, what's your question? I think I found like a money or Bitcoin wallet. So, do you mean by migration, like...

Royce Steven (00:32)
I use their like architecture and then change from ECDSK to like class.

Royce Steven (00:43)
Maybe you could try. Yeah, sure, if that's the most most professional way, yeah, that same way.

Royce Steven (00:56)
So, have you found any adaptive signature implementation by using the non-postal content version? I believe there are. I think this one is using atomic swap on Monero Bitcoin. OK.

Royce Steven (01:13)
I believe they also like have like an app, but do we have, I mean, because for Monero it's a bit complicated because Monero they also provided the privacy preserving, yeah, features, but I think here we don't need to consider the ideally you could find 2 chains, they are exactly dissimilar to Bitcoin.

Royce Steven (01:33)
You know what I mean? Yeah, so, so, so you don't need to consider more narrow of Bitcoin. Maybe you can consider more narrow Bitcoin or another Bitcoin, another chain which is very similar to Bitcoin. I mean, there's some...

Royce Steven (01:49)
The easier, the better. I don't want to. I mean, of course you can try Monero, but they have other other features. It's more complicated. More complicated. OK. Is it the one that I should look for? Probably this is because it's the one that I make for the classical comparison, but this is for let me check. This is for multi-signature, right?

Royce Steven (02:11)
I believe they also have a CDS adapter signature. What is adapter signature? Oh, yeah, they have like experimental. Oh, okay, yeah, yeah, even try this one as well. Okay, just replace this one with the postal content version you have already done. Hmm, but...

Royce Steven (02:30)
But this one is not already like an application, right? It's just, I mean it's fine, it's a demo, it's a demo, of course, yeah, of course, if you have this ready, you can deploy it on.

Royce Steven (02:39)
I mean, that's for further state, because I don't know if we can do it on Bitcoin testnet. Of course, you shouldn't deploy it on attribute coin network, because it's very expensive to deploy things there. Oh, okay. You should issue transactions, you imagine that. You should pay the gas fees. So, which means you have to use attribute coin. You don't need to do that.

Royce Steven (03:01)
Yeah, so let's do this. Maybe, yeah, this could be one. I'm not sure if this is, are they updating the code? Yes, they are updating it, which means they are maintaining. Yes, this is good with me.

Royce Steven (03:17)
Yeah, so OK, so two things: one thing is that you can use this one to develop your code by replacing the the what the the signatures adapt the signatures here from the classic one to the post part one, which means the ones you have already implemented, and the second thing is that if they are using ZKP stuff.

Royce Steven (03:37)
So let's just assume that we don't touch the ZKP stuff for now. We will have the, let's see, we first replaced the signatures. And then if you have time, you can also replace the group 16 by using with some post-quantum version ZKPs. Yeah. But of course they are, they are.

Royce Steven (03:58)
I think it's less efficient, but let's say I do believe that we we we have some postal quantum ZK piece, and also last time I'm trying to use the ZK proof. OK, laser, if I'm not, yeah, laser, yeah, that's that's that this is the one that we proposed. It seems like it's work.

Royce Steven (04:16)
Oh, really?

Royce Steven (04:24)
And do I need to like implement it on Solidity? Like, um, that's for next step. You can, you can, you can, you can do it for what, for 'cause in Solidity it means that we can use smart contracts to do that, so for smart contracts we may have other uh solutions.

Royce Steven (04:43)
Besides adaptive signature, so for adaptive signature, we can make sure that for Bitcoin, right, for Bitcoin, the cost chain of Bitcoin and Ethereum, maybe, yeah, you can also try the the the the the bridges for Bitcoin and and and Ethereum, but I think that's for next step. Let's first focus on this, but if you have time you can also.

Royce Steven (05:05)
Export it in shall we do we need to implement it in solidity, because in this way of course you can also do that, but but I personally I I think that because smart contracts are more powerful than Bitcoin, I mean features you can provide more features, so...

Royce Steven (05:24)
That's why in practice, I'm not sure some people has already implemented the adaptive signatures in Solidity. Maybe you can search a bit, you can see if people are using that or not. So normally what I had understood is that people prefer to use adaptive signatures for the bottoming swap, which are related to Bitcoin or other UTXO.

Royce Steven (05:44)
Based chains, rather than smart contract-based chains. Ohh, OK. OK, but let's do it step by step. I think this is, I mean, the things we discussed is very challenging now. So, I was wondering, you said that you tried to use the laser stuff? Yeah, OK. Interesting. Can they work?

Royce Steven (06:03)
They work, and then I'm trying to make like full native verification, and seems like the gas cost is really is really high. This is.

Royce Steven (06:18)
But you are doing a event, right? Yeah, EVM. Okay, which means you are using solidity to implement it? Yeah, solidity. Okay, interesting.

Royce Steven (06:33)
I mean, this is a bit impossible. You know why? Because...

Royce Steven (06:39)
For one blog.

Royce Steven (06:42)
SoE.

Royce Steven (06:44)
So, in for one block, we have the guess limit. Yeah, guess limit for one block. Do you know how much it is? I'm looking for the latest number, if I'm not mistaken, this one I guess is it? Yeah, something like that. If I remember correctly, previous, this one is like 30 million, and now they change it to a large number. So it means that, okay, this is...

Royce Steven (07:09)
is not feasible, but for this one, have you used Google 60?

Royce Steven (07:16)
No, I've not tried. OK, sorry, this is not for atomic swap, right? This is for last, or not? I'm not sure, 'cause, like on-chain verification during the atomic swap. OK, on-chain verification during swap, yeah.

Royce Steven (07:32)
Because, like, the last itself is like happening of change, so OK, the difficult part is the verification, I guess. OK, maybe you can try group 16 first. Group 16, yeah, you can add another bar here. It's like last with group 16.

Royce Steven (07:51)
In a, if the, if the verification is feasible or not, I think if using group 16 is gonna be very much more, yeah, yeah, of course, but I would like to see, and I'm also trying the pocket methods, I guess, the what the paper from pocket like the...

Royce Steven (08:11)
Which would work?

Royce Steven (08:17)
This paper.

Royce Steven (08:19)
that they using naysayer like optimistic verification. I think this is the paper that you sent me last time. Oh, right, right, right, yes, yes, yes. They are, have they implemented the...

Royce Steven (08:36)
Ohh, adapt signature, like I briefly, no, they do not use, but they just implemented the fake signatures, right? I think, yeah, on Spinks or May or something, I think they they only consider the the what the...

Royce Steven (08:55)
Yeah.

Royce Steven (08:58)
The hash-based signature scheme. Sorry, yeah, you can see, yeah, you can also try to adapt to their solution. I mean, they have the Lee, how to spell, Lee, yeah, Lee, yeah, for this, maybe they, in this way, you can, we can kind of reduce a bit the...

Royce Steven (09:18)
The verification, yeah, but it's still huge, if I'm not mistaken. I think I should have remember, ohh, right, yeah, OK, interesting.

Royce Steven (09:28)
Um...

Royce Steven (09:30)
I think it's...

Royce Steven (09:34)
I think it's expensive.

Royce Steven (09:38)
Kay.

Royce Steven (09:39)
Just.

Royce Steven (09:45)
Should have to know. Oh, yes.

Royce Steven (09:51)
Like, for the honest settlement is 1.1 million, but if someone like make like not honest witness, like, OK, this even this number, it's fine, right? It's less than this one. Ohh, yeah, OK, but the hash dispute can...

Royce Steven (10:11)
Probably like cost um 30 million guess if someone using probably like they.

Royce Steven (10:19)
Use their own public key or something. OK, OK. I was wondering, OK, this is for the post content version, right? Do you know if some have you have you found any implementation for the non post content version in Solidity? Non post quantum version, I believe they have. Yeah, I would really like to see how much what what are the costs they have.

Royce Steven (10:40)
But then, yeah, we we will have a, I think it's quite small. I think I've found it like for...

Royce Steven (10:51)
It's pretty small because they have like pre-compile verification already, but have they introduced the ZKPs?

Royce Steven (11:01)
In the implementation, probably not everyone that implements A.K.P. like, but then how can they achieve the construction of automic swap? Ohh, automic swap.

Royce Steven (11:14)
So, you are talking about automatic swap or or or just a just a last? I think automatic swap, yeah.

Royce Steven (11:25)
OK, so let's do that in this. So, for Automic Swap, we we can choose to implement solidity or not, right? So, let's do this, do the implementation without involving solidity, because in solidity, as you said, we have some limitation, we have some limit, we have some...

Royce Steven (11:44)
That.

Royce Steven (11:46)
They need to just that this the guest calls right, so let's try.

Royce Steven (11:53)
The implementation for Bitcoin.

Royce Steven (11:57)
The question Bitcoin, I mean, OK, this is for this one. Ohh, yeah, this one is they have like, but I think the graph, yeah, this this job, I don't think they have already implemented that in practice. Yeah, I think there is one GitHub report that do the pre-compile, but it doesn't have many star, probably like only seven star, OK, not mistaken. I think that's for next day.

Royce Steven (12:19)
But if I want to do this, it means that I have to convert the last into the full. Yeah, it's totally. It's very communicated. It's a lot of engineer work that you have to do. So as I said, I think, yeah, let's touch even later. So let's first focus on the.

Royce Steven (12:38)
Be coincide, yeah, to make sure that we have a feasible implementation for adaptive signature in practice, adaptive signature for automatic swap in practice, 'cause, as you said, that you have tried some implementation by using.

Royce Steven (12:56)
Solidity, since it just seems that they are very expensive. So, OK, as I said, I think auto adapt signature are very useful for the automatic swap relates to Bitcoin transactions or cross-chain transactions for Bitcoin and also other blockchains.

Royce Steven (13:15)
So, let's try this one, because, in this way, maybe it's still very slow, but we don't have a limit, right? And you don't, you don't, people won't argue that your solution is not feasible in practice, yeah, yeah, if you want to achieve achieve the the the post-income security, you we have to lose something, lose the performance, yeah, but it's fire, right? It depends, it depends on option of users.

Royce Steven (13:40)
Yeah, the only thing that I haven't tried is the post-quantum ZKP, I guess. Yeah, post-quantum, so I would have of growth 16 as well, yeah, yeah, yeah, so let me, let's let's try to do this, so we have the for Tommy, can I write something here, yeah?

Royce Steven (14:00)
We have to automate swap, right? So let's try to automate swap for BTC and also another chain, another UTXO, or I don't know, other other chain as well. So maybe you can, we have built this based on the repo you shared with me. So we will build three things.

Royce Steven (14:19)
The first is the classical one. So classical one, I guess they will also use some ZKPs, right? Okay, maybe this should be group 16. Okay, then you will evaluate this, their performance.

Royce Steven (14:37)
And then, let's try the second one. Second one is the classical AS, let's try last, and also ZKP's good 60. Okay, then let's see, okay, what kind of performance they will have. And then finally,

Royce Steven (14:55)
We will use last class post content with AKPs, right? Then maybe this should be the last there if I put something like this and then compare the performance. OK, and then this one we will have the first product or first application ready.

Royce Steven (15:15)
I mean, for Bitcoin, right? For Bitcoin, and then if we have time, of course, I believe you will have time, then we'll go try EVM and Etherms and Solidity. OK, I see. Solidity. OK, let's do it step by step, because for this, you can make sure that you don't do.

Royce Steven (15:34)
Anything related to the EVM, we can make sure that we have a feasible application ready, right? That's for Bitcoin, at least people can use Bitcoin to to do the to do the automatic swap, and then we can do once we would like to move them to EVM, then let's say, OK, how much cost, how many costs we will have, but OK.

Royce Steven (15:54)
Before you do that, please also do some preparation preparation just to check how people how the classical event classical adapt signature or classical atomic swap is implemented in Solidity, because I I'm not sure if I I have any say anything related to this. I'm quite curious to see their performance.

Royce Steven (16:18)
Ohh, okay. Yeah, then we have a benchmark right in the future, if we want to implement the postal counter version of Solidity running on EVM, then we can compare with the benchmark baseline, I see. I see. Okay, so for Bitcoin, Bitcoin, I don't think they use EVM, yeah, they do, no, no, they don't use EVM, it means for Eastern.

Royce Steven (16:42)
Virtual machine for Bitcoin, and what kind of environment? Yeah, that that that's totally different. Yeah, yeah, we here we don't have limit for the for the gas costs. Oh, really? Oh, OK. And you and Bitcoin, we even we don't have the gas cost this time, this item. Ohh, OK. OK. So here you just pay the transaction fees. Ohh, so if we...

Royce Steven (17:03)
Perform some complex, how to say that expensive calculation off-chain, then you don't need to pay pay the gas gas phase. OK, OK, so that's the that's the plan for this, because the ZEP that I implement, like using laser, is the because they...

Royce Steven (17:23)
require proof, exchange the pre-signature.

Royce Steven (17:31)
Ping this, this bit, OK, like one party must send the proof if later that they can like extract from the OK, you can assume that the two, two, two, of course, the two, this one, I'm using laser.

Royce Steven (17:50)
Yeah, you think, but this one I think is off chain, I guess. Yeah, it's off chain. I mean, it's off chain 'cause you it's like you and me, right? You have launch, I have some Bitcoin, you have some other tokens, other blockchain, then we we of course we we can assume that we too have some secure communication channel before we are doing the doing the the the the.

Royce Steven (18:09)
Exchange, so this probably can be fully in C. or RAS, yeah, C. or RAS, depends on you, yes. Code 16, code 16 is in RAS, RAS, P.O. used by using RAS for.

Royce Steven (18:25)
Yes, right, yes, I don't have the information.

Royce Steven (18:36)
I.

Royce Steven (18:40)
Yeah, I think I made some modelling as well in terms of the rejection sampling, but I'm not sure if this is. OK, interesting. OK, so yeah, so previously we discussed that this is for stage one, right? The first stage. So we discussed that if we change a bit to the norm.

Royce Steven (18:58)
By one, right? So...

Royce Steven (19:02)
Okay, I'm not sure. So I need to read all the details. Yeah, I need to read that as well. Okay, so this is this one is the. Yeah, I think I have the date. Okay, this is increased a little bit. It's fine. I think it's acceptable. I have the number on chapter 3.

Royce Steven (19:22)
But you have already for this one, right? So, you have the Overleaf project for this, for Overleaf project? Ohh, yeah, yeah, using like latex, yeah, latex, yeah, yeah, you using you are using X locally or you are using X overleaf? You know, have you ever used Overleaf? Yeah, OK, you can share with me when you when you think that you have a...

Royce Steven (19:44)
More ready version. Ohh, I can also have a look. Share on Overleaf. Yeah, Overleaf. Ohh, OK. Do I need to your to know your username or something? Yes, give me my e-mail address. That will be fine. OK. You know, my my e-mail address, yeah, OK. Manchester, yeah, yeah.

Royce Steven (20:04)
Ping.

Royce Steven (20:40)
Yeah, using the geometric model, like the approximation, like it should be around 36.8.

Royce Steven (20:48)
Okay, what is geometric model? The model that I approximate the...

Royce Steven (20:56)
The rejection sampling.

Royce Steven (20:59)
This one. OK, you call it geometry. OK, let me, let me check this. So, we'll do the.

Royce Steven (21:16)
Okay, kind of makes sense.

Royce Steven (21:25)
Yeah, so accept from this one.

Royce Steven (21:29)
Previously, it was OK, and then we got minus one, minus one.

Royce Steven (21:35)
Okay.

Royce Steven (21:38)
I think it kind of makes sense. Yeah, and then if I try it to run like large enough, I guess it's probably should be close to the approximation. Okay, okay. This is the sample numbers you draw there here. Yes. Okay.

Royce Steven (21:57)
So, this is the theoretical results. We, this is approximate theoretical. OK, go inside.

Royce Steven (22:07)
separate.

Royce Steven (22:12)
Yeah, I said, yeah, it's interesting, yeah, it is good, and I think I also have the diagram for it, like...

Royce Steven (22:33)
Which one? Try to plot the probability. Probability of what? The rejection. OK, I guess it should be. Let me check. So this is the attempts. OK, this is the probability of exact. Can you explain a bit?

Royce Steven (22:52)
I'm not sure if I understood this. I know the X means how many times you try, right? Yep.

Royce Steven (23:02)
What does Y mean? Probably it means if it's 15 attempts until accepted, it means that it's probably the probabilities below 5% that I need 15 attempts until it's being accepted, like for the pre-signature or signature. How do we explain things here?

Royce Steven (23:22)
So, if it's one...

Royce Steven (23:24)
Why is it so high?

Royce Steven (23:28)
Um, I think it's because, like, um...

Royce Steven (23:33)
Because...

Royce Steven (23:35)
The bound is should be small, like, I mean, like, yeah.

Royce Steven (23:40)
It's random, actually. OK, it's a random setting, 'cause I thought that the more you tried, right?

Royce Steven (23:50)
Are you close?

Royce Steven (23:52)
SoeHub.

Royce Steven (23:54)
Mm.

Royce Steven (23:56)
Yup, I mean...

Royce Steven (24:00)
That's the range of probability, the probability of the exact K entrance.

Royce Steven (24:07)
I'm not sure if I understood this one.

Royce Steven (24:10)
That's something fine.

Royce Steven (24:13)
So it's like...

Royce Steven (24:16)
It's less likely to for you to sample until 15 times until it's been accepted. So it should. Does this mean I thought that if we sample multiple times, then the probability will be higher, right?

Royce Steven (24:30)
But why is?

Royce Steven (24:32)
Decreasing here. Let's see people accepting once, twice, third time they usually is already accepted. They don't have to try until 15.

Royce Steven (24:43)
OK, 50 times, ohh, but I don't know what is the maximum, like the worst case scenario, I think, ohh, let me, let me see.

Royce Steven (24:54)
But...

Royce Steven (24:56)
But the user said normally they should try.

Royce Steven (25:00)
This number, right? Yeah.

Royce Steven (25:06)
Wait, wait, on average should be around just two to three times here already, so which means in many times.

Royce Steven (25:14)
Or try once, then it will be accepted. The percentage is more than thirty-five. OK, yeah, OK, and...

Royce Steven (25:25)
Exact, okay, hello, okay, let me think. I would put something like this. For example, you can see, you can put something like this. This is the number 1 till 15. And you could put something like this.

Royce Steven (25:44)
The probability of accepted of acceptance, ohh yeah, that's more accessible, and then maybe from one, you know, we are like, I don't know, thirty-five, something like that, then we can we can see that, OK, it will gradually increase it, right? Right then, in this way, you will see that even we're trying a lot of times, it will increase very slow.

Royce Steven (26:06)
So, which means, OK, we can see that this one, maybe, yeah, I think it's more easier for, it's easier for readers to understand, yeah, ohh, you know, right, OK, we try one times, OK, the probability is like this, OK, if we try multiple times, of course, you will have a high probability.

Royce Steven (26:26)
To for acceptance, but I got the in.

Royce Steven (26:31)
It will increase very slowly, because it's becoming almost stable, right? It's very close to 100%. Ohh, OK, because, like, once it accepted, it should be not trying anymore. It, yes, like, yes, yes, ohh, it's like maximum trying. Ohh, OK, so it's very likely, like...

Royce Steven (26:50)
On the 15 times, it's being rejected, yeah, yeah, yeah, ohh, yeah, it's more makes sense, yeah, 'cause in this way, it's kind of, I don't know, country, country, yeah, contradictory, so people will say that why I try a lot of times, the probability still worry, but after you explain it to me, I think I got it.

Royce Steven (27:10)
But let's try to make this that that the radar is not a user, yeah.

Royce Steven (27:16)
Um...

Royce Steven (27:17)
Whatever these things.

Royce Steven (27:26)
SE.

Royce Steven (27:37)
Um...

Royce Steven (27:39)
The ohh, yeah, so yeah, I think I haven't made the graph for it, but this is the overhead if using I call it like back tier, so ohh, OK, you mean without just doing the decoding, encoding, ohh, right, yeah, OK, using decoding, encoding it.

Royce Steven (27:58)
Increase quite significantly up to 80% on on adapt. OK, it makes sense. I think encoding and decoding is very consuming, and then what else?

Royce Steven (28:17)
It is.

Royce Steven (28:22)
Yeah, and with the classical things, I don't know how to make the comparison, like for the key generation.

Royce Steven (28:32)
They don't encode the code, they just pass it from the data structure. Yeah, yeah, yeah. So I call it like hybrid native. OK, OK, OK.

Royce Steven (28:42)
Ohh, yeah.

Royce Steven (28:46)
One sec, so this is without, this is without packing. OK, this is with OK, and kind of make sense, and I believe this is with packing, unpacking, except the key generation. OK.

Royce Steven (29:02)
So, you mean here they have already added some packing and impacting stuff? Yeah, OK. Yeah, OK. It's fine, I mean it's fine.

Royce Steven (29:15)
Yeah, you cannot remove the pack and packet stuff, right? They have already put that in there in existing APIs, yeah, okay, yeah, it's fine, it's fine, we can still say, okay, there's, so the only thing that they don't unpack is only the key generation and then the full signature, they just the intermediate.

Royce Steven (29:35)
A product that they pack and unpack. OK, and yeah, this is the guest cost.

Royce Steven (29:43)
Um, yeah, still not sure.

Royce Steven (29:55)
And then...

Royce Steven (29:58)
I don't know, should be like a challenge just be included in the report of the work. The challenges for what? Because for chapter 4, I'm really don't know what to include. Oh, okay. So it's like, normally, normally it should be evaluation and I would put the reflection into another chapter.

Royce Steven (30:17)
Chapter Five. Ohh, OK. We have the summary for conclusion and critical reflection of critical T code for collection on Chapter Five. Yeah, on top on another independent chapter. So evaluation means that it's still in your.

Royce Steven (30:37)
work, right? You are still report, you are still reporting the important results, important findings you find, right? It's very important because I would like to divide it into another another paper, sorry, another chapter. And then in this chapter 5, you will summarize what we have already achieved from the theoretical part and also the evaluation part.

Royce Steven (30:59)
And then you will have another subsection to show that, okay, what you failed and then how would you do it better if we give you, if you could be giving another chance to do that. Okay, so it's like this. You will check the paper, right? If you check the paper, they also have the conclusion.

Royce Steven (31:20)
Yeah, you can see, but here the conclusion is very shorter.

Royce Steven (31:26)
Very shorted hours, shorted hours. Ohh, OK.

Royce Steven (31:30)
Okay, yeah, I don't know. I just only have like 2 pages for the conclusion. Yeah, yeah, it's fine. It's fine conclusion. So evaluation, what should be like in the evaluation?

Royce Steven (31:42)
****, forget, sorry, what, what, what's this? This is conclusion of future work, and what is chapter 3? Results, what, what discussion? OK, results, results.

Royce Steven (31:56)
Evaluation, so, so normally when we are writing papers, evaluation means the the results, the implementation and evaluation results, something like that, so you maybe you can change another name.

Royce Steven (32:15)
What are they called? Okay.

Royce Steven (32:19)
Evaluation and a reflection conclusion.

Royce Steven (32:23)
Okay, methods, your reflection. You can see they have the method, methodology, right? They were, you should also present your results.

Royce Steven (32:34)
Why do you present your results? So, is combined with results. OK, I would like to prefer, I prefer to put them separately, separately, yeah, yeah, some people were easier to see that, otherwise I first need to understand your method, and then I will OK, you can see you should evaluation testing.

Royce Steven (32:53)
Right, correct. Okay, you can also put some, okay, critical reflection evaluation results, but this document says it that the structure doesn't have to be like this, it's just, yeah, yeah, they then I would prefer to put the evaluation independently in in one.

Royce Steven (33:12)
chapter and move the reflection to chapter 5. Yeah, yeah, conclusion, yeah. Because I mean, we are talking about the evaluation, right? Well, of course, maybe you are talking about the results, you can do some reflection already. But yeah, it's fine. I mean, maybe you can also add a shorter paragraph.

Royce Steven (33:32)
Regarding that reflection on your evaluation results, but finally, I would like to say...

Royce Steven (33:39)
It would be better to have an independent subsection of this to do the critical reflection to summarize what we have done. This is all in Chapter 5. Yeah, Chapter 5. So, where did you find this one? On the canvas. Canvas. Let me check.

Royce Steven (34:01)
Well, that they have shown, yeah.

Royce Steven (34:18)
And.

Royce Steven (34:20)
Yes.

Royce Steven (34:24)
So, where can I find it? It should be in the MSC report video, this one.

Royce Steven (34:38)
Just.

Royce Steven (34:40)
Okay, yeah, that's good.

Royce Steven (34:45)
Okay, so for the this, I think they have to, oh don't know, we see different thing. For probably for the this one, if should I like this one must be strict, they give proof or if I can relax the proofing or like this skip.

Royce Steven (35:05)
Or should I like follow this protocol like strictly? Ideally, it would be better to follow the. You can also check their actual implementation. Maybe they have done some optimization in the implementation, but in general, I think the structure should like almost the same. Yeah.

Royce Steven (35:23)
Yeah, so what I'm doing right now is just implement the laser on C and Rust, whatever language that I'm using, I just call the the laser on C. OK, yeah, I mean, yeah, so laser is in in C, right, yeah, not in Rust, in Rust, but if you use Rust to call.

Royce Steven (35:43)
Say, maybe, yeah, it's I don't know how how how efficient it would be.

Royce Steven (35:49)
Yeah, but anyway, let's try to have a ready version. Uh, yeah, we can, we can try to improve the efficiency if you have time, yeah, and for the future works, I guess.

Royce Steven (36:02)
I'm not sure if soe.

Royce Steven (36:06)
Yeah, let's first do this. If we can finish this one, I mean, if we have time to finish this one, then this one can maybe don't need to be included in the in the future work. This, this, I think this is important for the for the next classical as performance. Yeah, 'cause we were also, as we discussed at the very beginning, we were also do the.

Royce Steven (36:25)
Benchmark or the where was it to the comparison with the a postal content version. So, if Bitcoin they don't have like guest cost metric, what kind of comparison that I should? OK, the time, what's that? The time and also the communication costs, communication costs, even for off-chain components right here, you can see here I send something to you, you send something to me.

Royce Steven (36:48)
Okay, of course it was, it also increased my communication cost and also you also introduced the computation costs right, which means where I am issued A transaction, I have to do some preparation before I do that, which means I cannot, maybe sometimes I cannot do it on my phone, I have to use some.

Royce Steven (37:08)
Heavy machines, some PC, dedicated PC to do that. OK, yeah, this will also affect the usability in practice, right? OK, and communication, yeah.

Royce Steven (37:22)
Ops.

Royce Steven (37:29)
But it must, it doesn't have to be like one person in using like one port local port and the other person using like local port. It doesn't have to be, it's just. What do you mean protocol? Do you mean different blockchains? It's like the simulation has to be like very real, like probably someone is using certain like local port and.

Royce Steven (37:50)
Another person using.

Royce Steven (37:52)
Like another like port local port number, and then they communicate like.

Royce Steven (37:59)
Ohh, yeah, I got what you mean, yeah, yeah, no, no, you don't need to, you don't need to, you just ask me that, OK, yeah, we are doing implementation, of course, you can, you can, you can do some certification, yeah, but assume that, OK, the messages can be easy to transfer to others, but in the future, if you would have done, if you would like to simulate the actual.

Royce Steven (38:18)
Auto mix well, of course, you can open 2 ports for them and then you assume clear communication with each other. Oh, okay. But for now, let's assume that, okay, I can just send a message to you directly. You assume that, okay, you have two users, they can both access to your machine. Oh, yeah.

Royce Steven (38:38)
Makes sense to them, OK.

Royce Steven (38:41)
I think that's clear, right? For the, for, for the, yeah, plan for the next week.

Royce Steven (38:48)
And does it have to implement like refund or type out as well? What like the atomic song sometimes they can like fail like do I have like to implement like the protocol how the refund refund like the refund protocol or type out protocol? Yes, but that's for the...

Royce Steven (39:06)
Age cases, I would say, so let's first focus on the normal cases. We assume that they are, of course, they are both honest, yeah, but of course, if some of them they are not honest, honest, yeah, it means that the funds will also transfer back to the to the honest users, so they won't suffer any loss, any costs.

Royce Steven (39:26)
Any loss? Okay.

Royce Steven (39:30)
SoE.

Royce Steven (39:32)
Does the protocol still?

Royce Steven (39:35)
Use like packing, unpacking, or yeah, it depends on you. I mean, it depends on you. If you think that it's efficient enough, you will use pack and unpacking. It's not gonna be efficient, like if you, yeah, yeah, if it's not very efficient, let's just ignore it, ignore it, and then you also add this limitation in your critical reflection. Ohh, OK, OK, mate, for this is just exploration, right? You don't need to build a cap.

Royce Steven (39:57)
Product already exploration. Yeah, demo, right? Just explore if this is feasible or not. OK. Classical adaptive signature.

Royce Steven (40:14)
Classic.

Royce Steven (40:18)
There is.

Royce Steven (40:21)
Soe.

Royce Steven (40:23)
Yeah, that's right.

Royce Steven (40:25)
So, can I like benchmark using the...

Royce Steven (40:28)
No.

Royce Steven (40:30)
This one, doctor one, is it possible to benchmark the performance of this? You mean the class code? Yeah, yeah, yeah, I don't know, but this one, this is this is the one I suggested. Maybe you could find a better implementation that you can build things from there, yeah, but if you think this is the best one, yeah, of course you should.

Royce Steven (40:50)
Benchmark as well, yeah, because if I'm not mistaken, some repo it doesn't have extract protocol, it's just adapt, and then you just stop, OK, on the adapt, they don't have extraction, you mean, yeah, some some demo they just like stop at adapt, OK, and they, I mean, of course, you should have SE.

Royce Steven (41:09)
Extraction, but extractions always very fast, right? We just need to do the...

Royce Steven (41:15)
The final state, yeah.

Royce Steven (41:18)
Kay.

Royce Steven (41:25)
And...

Royce Steven (41:27)
Should be what was on?

Royce Steven (41:34)
Yeah, I think that's it for them. Okay, good, good. Okay. I'm looking forward to seeing your results. Yeah, that's good.
