On.
Royce Steven 0 minutes 8 seconds
Yeah, basically, I try to summarize in the slide.
Royce Steven 0 minutes 18 seconds
Um, I think this is the...
Royce Steven 0 minutes 21 seconds
Research questions, the research question, I mean, like our my project, like the motivations, yeah, the motivation that there is no enough implementations, demonstration, or...
Royce Steven 0 minutes 41 seconds
Kind of a...
Royce Steven 0 minutes 43 seconds
That analyzed the the paper on practice, uh, sorry, one comment there. Can I go back? Yeah, so here, maybe...
Royce Steven 0 minutes 57 seconds
It would be safer that you make this, that you solve for this sentence because you say they don't have the exotic ones. It's a bit tricky because they do have the, they do have it, the proposed one. You can see that in practice, maybe they have the, let's say.
Royce Steven 1 minute 16 seconds
Implemented all exotic ones. OK, like that, right, 'cause in practice some people are trying 'cause for exotic ones, exotic signatures will.
Royce Steven 1 minute 28 seconds
contain a lot of signature schemes. What we are doing is adapt the signature. There was a multi-signature, ring signature, and also other signatures, advanced signature schemes. For others, some developers, they are trying to implement them in practice.
Royce Steven 1 minute 47 seconds
So, I mean, OK, just try to make it safer. Oh, OK, about the classical one. Yeah, no, no. The classical adapter or the post quantum. No, for the exotic. So, what is the exotic signature? Exotic signature is the they are adding some functionality. Yeah, yeah, yeah. So, so.
Royce Steven 2 minutes 9 seconds
Besides adapt signature.
Royce Steven 2 minutes 12 seconds
See, I was other, like, yeah, yeah, yeah, so for other signature, like, especially multi-signatures, many people are working on it, many people are implementing it, so that's why I'm saying this is, yeah, they don't have the exotic ones, maybe, yeah, they have, so just try to...
Royce Steven 2 minutes 31 seconds
Yes, you change a bit of the wording here. OK, thank you.
Royce Steven 2 minutes 35 seconds
Yeah, OK. This is probably the core algorithm functions of the...
Royce Steven 2 minutes 43 seconds
latest adapted signature, that is based at the signatures resign, start with pre-designing. It's not a fully signatures until the other party who has to secret adapt it.
Royce Steven 3 minutes 2 seconds
Ohh...
Royce Steven 3 minutes 4 seconds
After the can publish it on chain to claim.
Royce Steven 3 minutes 11 seconds
Yeah, and then the other party just, it leaks, it will leaks the secret, and the other party will extract and adapt on.
Royce Steven 3 minutes 22 seconds
OK, OK, yeah, you, you, you go first. Let's try, yeah, maybe you can first go through all the slides, then I will OK try to add some, give you some comments later, yeah, and then...
Royce Steven 3 minutes 36 seconds
Yeah, the latest at the bus nature is built on top of the basic signature, and we just...
Royce Steven 3 minutes 45 seconds
Um...
Royce Steven 3 minutes 49 seconds
All the primitives are reused for um, challenged internations, and...
Royce Steven 3 minutes 57 seconds
all the arithmetic calculations.
Royce Steven 4 minutes 1 second
And for the sampling as well, as well.
Royce Steven 4 minutes 7 seconds
And then how we do it is...
Royce Steven 4 minutes 11 seconds
Firstly, the person who holds the witness.
Royce Steven 4 minutes 19 seconds
We'll create a statement Y or the adapter public adapter statement Y.
Royce Steven 4 minutes 28 seconds
And, and...
Royce Steven 4 minutes 32 seconds
Yeah, if they try to...
Royce Steven 4 minutes 37 seconds
Spend a.
Royce Steven 4 minutes 40 seconds
Pre-signatures it will rejected, and it has not been.
Royce Steven 4 minutes 46 seconds
Uh, a fully a signature, and then...
Royce Steven 4 minutes 52 seconds
Yeah, after.
Royce Steven 4 minutes 54 seconds
The pre-signatures.
Royce Steven 4 minutes 58 seconds
Is.
Royce Steven 5 minutes
adapted with the person who holds the witness, it will become a full signature and they can fix the coin. After it publish, it will leak the secret to other party.
Royce Steven 5 minutes 15 seconds
Yeah, basically, it's the secret will be extracted, and then...
Royce Steven 5 minutes 20 seconds
the other party just learned the weakness by extracting and then adapt on the extracted weakness and then we can claim the coin from user one or the other party on chain one.
Royce Steven 5 minutes 40 seconds
Uh, basically, um...
Royce Steven 5 minutes 45 seconds
The pre-signing is the most expensive one in terms of timing, and then the second one is a pre-verification and a depth is.
Royce Steven 5 minutes 59 seconds
Since adapt is including pre-verify, so the adapt itself is only 2.9, like 6.9% deep.
Royce Steven 6 minutes 11 seconds
Minus 4%, yeah, 2.9 and the extract is the least.
Royce Steven 6 minutes 18 seconds
Of timing.
Royce Steven 6 minutes 22 seconds
Because of the adapter computation is not the last itself, even we optimize the last algorithm, it won't make any much difference, since...
Royce Steven 6 minutes 39 seconds
Oh, I'm sorry, I think this one is... oh, yeah, this one is the last signature and...
Royce Steven 6 minutes 47 seconds
Yeah, it it's.
Royce Steven 6 minutes 50 seconds
99% of it, like it's a response. The challenge is only like 0.7%.
Royce Steven 7 minutes 3 seconds
And this is the proof, like, from end to end, it's ninety-nine percent of the end-to-end timing, so if the loss is optimized, it doesn't.
Royce Steven 7 minutes 17 seconds
Oh.
Royce Steven 7 minutes 20 seconds
is not effective, like if we only optimize the loss, the core bottleneck is the proof that takes a lot of time. So even if we try to optimize loss, this does not make a huge difference. It will be more effective if we...
Royce Steven 7 minutes 39 seconds
Read, try to some way to to reduce the proof.
Royce Steven 7 minutes 45 seconds
to generate the proof, the time.
Royce Steven 7 minutes 50 seconds
Um...
Royce Steven 7 minutes 52 seconds
I think this is the demonstration of the Bitcoin, and then...
Royce Steven 7 minutes 59 seconds
On.
Royce Steven 8 minutes 2 seconds
On the original Bitcoin, everything is accepted since the Bitcoin doesn't verify last, but I try to make some modifications and yeah.
Royce Steven 8 minutes 16 seconds
To modify the rules and policy, and yeah, it after it verify last the negative.
Royce Steven 8 minutes 28 seconds
signature is the tempered signature is rejected.
Royce Steven 8 minutes 38 seconds
Yeah, after we try to make some optimizations.
Royce Steven 8 minutes 43 seconds
On the.
Royce Steven 8 minutes 46 seconds
On 10 verification.
Royce Steven 8 minutes 48 seconds
Yeah, we try to.
Royce Steven 8 minutes 51 seconds
Make the on the VTM 3.
Royce Steven 8 minutes 55 seconds
It's on the boundary of the gas limit, but for the lithium 5, it needs more optimization to fit in one transaction.
Royce Steven 9 minutes 8 seconds
Soe.
Royce Steven 9 minutes 12 seconds
I did some experiment also on the unmodified FIPS to 0.4 or MLDSA for some modification is unavoidable.
Royce Steven 9 minutes 27 seconds
Um...
Royce Steven 9 minutes 31 seconds
Like, resign and verify.
Royce Steven 9 minutes 35 seconds
If there are new algorithm, they need modifications for, but for the 45 is not, we can just use the unmodified verifier.
Royce Steven 9 minutes 46 seconds
For the optimized last, the signatures will be smaller than the statement Y.
Royce Steven 9 minutes 53 seconds
Soe.
Royce Steven 9 minutes 54 seconds
Yeah, the compression signatures makes the statement wise larger than the signature itself.
Royce Steven 10 minutes 3 seconds
But yeah, this has not been analyzed if the...
Royce Steven 10 minutes 9 seconds
If the signature is unforgeability or not.
Royce Steven 10 minutes 24 seconds
Yeah, I think that's probably it. I'm not sure. Okay.
Royce Steven 10 minutes 30 seconds
Ohh.
Royce Steven 10 minutes 32 seconds
You finished all?
Royce Steven 10 minutes 37 seconds
Yeah, OK, so maybe we can start from the beginning.
Royce Steven 10 minutes 43 seconds
Yeah, just go to the first slides. We can go through them one by one. I think here is okay, the motivation is good. And what's the next slide?
Royce Steven 10 minutes 54 seconds
Okay, here I think maybe it would be helpful to add some slides or maybe one slide to show.
Royce Steven 11 minutes 3 seconds
the motivation, the background of the application of adaptive signature. So imagine that people will say, OK, here we know that there are different signatures, right? There are some different exotic signatures. But the question is, why we would like to focus on the adaptive signature? So you will show the importance of adaptive signature. So what's the importance of adaptive signature? We will see that it can be
Royce Steven 11 minutes 27 seconds
Applied, we should show that it can be applied in auto mix web, in channel, something like this, right? So, here you don't need to introduce details, but you should show that the it's very important to build the adapt signature to make it the post quantum secure. You know what I mean, right? So, here you maybe you...
Royce Steven 11 minutes 45 seconds
Start the technical details too far, too early, too early. You know what I mean, right? So, imagine the second examiner, he's not that expert in blockchain crypto, but they would like to understand what's the motivation, why you would like to do this. You should tell them, right? You should assume that the audience, they have some computer science background in general.
Royce Steven 12 minutes 6 seconds
but not very specific for security for crypto for blockchain. Okay. Okay. And then here, yeah, regarding the technical details, I think it's good. It's okay. Yeah, you introduce, but maybe you should pay attention to the timing, right? If you allocate some time to the motivation, to the background, to show the importance.
Royce Steven 12 minutes 28 seconds
Maybe you should just highlight the most important implementation results you have already achieved. OK.
Royce Steven 12 minutes 39 seconds
Yeah, I, I think here it's good, right? You have the motivation of the of the you, you have the OK, this is for automatic swap, right? OK, yeah, this is for automatic swap.
Royce Steven 12 minutes 52 seconds
That's a machine.
Royce Steven 12 minutes 56 seconds
So I was wondering, maybe you should, in somewhere, you should highlight, you should briefly introduce the method. You, you, you, you, OK, yeah, so where is it? So, basically, yeah, you people will ask you, OK, how did you?
Royce Steven 13 minutes 15 seconds
Modify the existing base code; you need some signature to make it to be a last.
Royce Steven 13 minutes 22 seconds
I mean, so for here, my understanding, my suggestion is that, because we only have how many minutes? Six or 8? Six to 8 minutes. So yeah, so that's why you don't need to give too many technical details here, but you should try to summarize the concept. Yeah, the concept and it was the method at a very high level. So for example, if you can
Royce Steven 13 minutes 46 seconds
Give me a picture to summarize the process of your method, right? I can easily get it, but now when I was listening to you, maybe I'm okay 'cause I know the details, right? But imagine as a reader or as an audience, I don't know the background of this, but I would like to quickly capture what you have done, what you...
Royce Steven 14 minutes 6 seconds
Why you would like to do this, right? The motivation, and how did you do that?
Royce Steven 14 minutes 11 seconds
the method and then what's the most important or what had the most interesting results and then what had the most important takeaways. That should be enough, right? Okay, so try to reorganize this. It's kind of, I mean, it's okay that we have a lot of sound tactic details. Of course, it can support your conclusion.
Royce Steven 14 minutes 31 seconds
But again, imagine this is a bit different, right? When you are doing the presentation, it's more like that you should present your results in just 6 minutes. Yeah. And you know, very, how to say that, easy to follow it. Easy to follow. Yeah, people can easily capture what you have done.
Royce Steven 14 minutes 51 seconds
And maybe finally, yeah, here is what you have done. We finally, you can also...
Royce Steven 14 minutes 59 seconds
Go back to the to the to the motivation you proposed at the very beginning, but just to show, OK, if we use our...
Royce Steven 15 minutes 8 seconds
Last, though, can we be able to post the quantum secure automated swap protocol? OK, if yes, what are the loss or what are the what are the what are the the costs we have to suffer from? So, basically, you at the beginning, you propose some motivation, propose the challenges, propose the questions, and finally, you need to summarize.
Royce Steven 15 minutes 29 seconds
OK, have you addressed the questions you have you proposed at the very beginning? If not, OK, what kind of conclusion we can have for now? What are the questions for the future work?
Royce Steven 15 minutes 41 seconds
No, I think that's a we we we had a better better structure for the for the presentation, I see.
Royce Steven 15 minutes 49 seconds
Since.
Royce Steven 15 minutes 52 seconds
Yeah, here you have some, yeah, you can see, you have some, have some, yes, you have some interesting conclusion. We always have open questions, right? But again, because this is...
Royce Steven 16 minutes 5 seconds
We are more focusing on the, yeah, we are on a on a, I mean, we would like to in this project, right? We would, we we are focusing on that signature for blockchains, yeah, right? So, you should for your conclusion that you should be more specific, yeah, for...
Royce Steven 16 minutes 24 seconds
This one should I, oh, why it's not working. Should I show this probably is not the main point of the. Yeah, yeah, you can. Anything one slide should be fine, but you don't need to highlight the details. Personally, I would like to have some pictures.
Royce Steven 16 minutes 43 seconds
If you can draw some.
Royce Steven 16 minutes 46 seconds
attractive or beautiful pictures to show the concept, to show the complicated method or complex method you adopted, then I think you have a better understanding of what you have done, right? Makes sense. So here, if I'm listening to you, do I have to read all the text, which is quite a puzzle to be honest, to see that, but yeah, that's true, right? When people want to see that,
Royce Steven 17 minutes 9 seconds
So you can show with me what is high bid, what is low bid, why we would like to do that. Then if you can show some pictures, people will say, maybe they cannot get all the details, but they can get the high level concept of your method. Then that's the, that's goal. That's why we would like to do the presentation here, right? Otherwise people have.
Royce Steven 17 minutes 29 seconds
They can easily go to the, yeah, go to your report, report, yeah, they can see all the details, but here it's more like you should summarize the the method, you should summarize the complicated or the complex concepts in a very, how to say that, in a in a in a more friendly way, yeah, then people can can follow you.
Royce Steven 17 minutes 48 seconds
Because it's only 6 to 8 minutes, probably, yeah, yeah, one slide is probably just, yeah, one slide, probably like 30 seconds, yeah, I think, so, yeah, yeah, you must, how many slides, how many slides you have, 13 is too much, 13, yeah, maybe you can just, I don't know, that's OK, but it depends on how fast you would like to go.
Royce Steven 18 minutes 7 seconds
When you were when you are presenting that those slides.
Royce Steven 18 minutes 11 seconds
Yeah, so it's better to make like visual that people can easily capture, like get image in quickly. Yes, and also at the beginning, you can also have, I don't know if you have a time, but you, at the beginning, maybe you can also have the counter.
Royce Steven 18 minutes 31 seconds
The table content to to to show the structure, or you can use another way to to show the structure, and then this at the top, Meghan, you can also show, okay, where are we? Okay, for example, there is the pollution, is the is the is the introduction, there is a method, and then there is a...
Royce Steven 18 minutes 51 seconds
This is the conclusion, then it's another helpful, helpful method to let audience, let's say, follow your presentation better.
Royce Steven 19 minutes 4 seconds
But anyway, I mean, content wise, it's OK, but it's just, yeah, for the presentation wise, you can improve it a bit, yeah, the structure of the slides and also the visualization of the of the results, and also the conclusion of the of the presentation.
Royce Steven 19 minutes 23 seconds
Yeah, so I wanna ask about the rejection sampling, like the simplified loss and the MLDSA, yeah, plus is a bit different, yeah, like we or I'm using the original MLDSA, the rejection sampling is...
Royce Steven 19 minutes 42 seconds
Higher, yeah, so...
Royce Steven 19 minutes 45 seconds
Should I just like...
Royce Steven 19 minutes 47 seconds
frame this project as the implementation of the paper or implementation? I mean, yeah, you guys, it's more for, I think that it's more for the practical loss. So you maybe you don't need to limit yourself in the scope of the paper.
Royce Steven 20 minutes 6 seconds
OK, you cause you have already done right? You have already attempted the optimization, so you should have summarize this, you should report it, which is also helpful, because in the...
Royce Steven 20 minutes 18 seconds
In the report, I just...
Royce Steven 20 minutes 21 seconds
Like, there is no table for it, just in one section, I believe, one section.
Royce Steven 20 minutes 31 seconds
Yeah, just this one. I mean, that's fine. Then, in the slides, you could just either use half of the slide to to show the one slide to show that you have attempted. Yeah, so...
Royce Steven 20 minutes 49 seconds
Yeah, I did. I just like presents the number in the paragraph. I don't.
Royce Steven 20 minutes 59 seconds
I don't like show it on the...
Royce Steven 21 minutes 2 seconds
On the like tables or figures, yeah, it's fine. In a report, you can do that like this, but in a in a in a slides you can add some some table, right? The sum, right? Yeah, because the MLD is a last is just the rejection sampling is twice, so I just like a report as like finding like...
Royce Steven 21 minutes 23 seconds
Do you think he is an interesting finding?
Royce Steven 21 minutes 27 seconds
If you, if yes, maybe you can highlight it. If you think that it's just some trail, I don't know, solution trail results, then just leave it, because I'm afraid that people are more interested, like into the benchmarking, like how we, if we use the MLDSA.
Royce Steven 21 minutes 46 seconds
You have, you can, you can calculate it as a as as future work, right? In a future work, I mean, of course, at the beginning, it's not the focus of this project, yeah, but you have extended the the the the todos by capturing or by attempting implement something like this, yeah.
Royce Steven 22 minutes 30 seconds
So, the marker should...
Royce Steven 22 minutes 33 seconds
Um, like if they are read the paper, they should...
Royce Steven 22 minutes 38 seconds
Be able to understand what the main technology without probably any access to the...
Royce Steven 22 minutes 43 seconds
A repo, a repository, or...
Royce Steven 22 minutes 47 seconds
So, what I mean, you, you, you mean...
Royce Steven 22 minutes 51 seconds
Should we add the link of the report?
Royce Steven 22 minutes 54 seconds
What's your question? My question is...
Royce Steven 22 minutes 58 seconds
I assume that the market does not have access to the, yeah, yeah, they don't, they probably don't want to look at it, they just don't think they have time to look at it, but if they're interested, maybe they will check, so they just probably have to understand everything through the video, and yeah, yes, yes, and the report, but the video is to summarize what we have done, and to help the...
Royce Steven 23 minutes 19 seconds
Similar to to to understand what you what you did very quickly, and then if you are interested, then they will also go into the details of your report of all, what we call dissertation, what you what your latest.
Royce Steven 23 minutes 37 seconds
So, for...
Royce Steven 23 minutes 42 seconds
She was good.
Royce Steven 23 minutes 44 seconds
So, regarding the number for the...
Royce Steven 23 minutes 54 seconds
For the...
Royce Steven 23 minutes 57 seconds
A laser.
Royce Steven 24 minutes 1 second
Because.
Royce Steven 24 minutes 4 seconds
The size of the...
Royce Steven 24 minutes 6 seconds
Proof for laser is like depends on the.
Royce Steven 24 minutes 12 seconds
Signature like this.
Royce Steven 24 minutes 16 seconds
But for last and growth 16, I think it's fixed because it's based on elliptic curve. Yeah. They have like fixed setting for growth 16. Yes. For laser, it depends on the input size. So that's why the laser is increased. So, okay.
Royce Steven 24 minutes 36 seconds
Would that be a questions? No, no, I think, yeah, because you just follow the schemes, yeah, yeah, you use them as black boxes, they are not modify them, yeah, then it's fine, it just report why you achieved this honestly, then that should be fine. It's not your fault, right? That's the yeah.
Royce Steven 24 minutes 55 seconds
Not sure if this is the correct demonstration. Like the first demonstration, optimic swap, and the second demonstration is...
Royce Steven 25 minutes 7 seconds
I don't know if this is interesting.
Royce Steven 25 minutes 13 seconds
Like, is it the core idea of the project to throw like?
Royce Steven 25 minutes 20 seconds
The modification of the...
Royce Steven 25 minutes 23 seconds
The Bitcoin something.
Royce Steven 25 minutes 25 seconds
So, what's your question? Um, I think this is the modification on Bitcoin. Is it like interesting to show Bitcoin? Uh, you modify the Bitcoin or not? Um, I'm modifying on the...
Royce Steven 25 minutes 43 seconds
Modified, so that...
Royce Steven 25 minutes 45 seconds
Um...
Royce Steven 25 minutes 48 seconds
Invalid signature will be rejected. Yeah, see, that's interesting. Yeah, yeah, it's OK, because here you just show how we should implement this in practice, right? Yeah, so yeah, it's also important to show that.
Royce Steven 26 minutes 4 seconds
Again, as we discussed at the very beginning, right, for this project, we should have at least two steps. The first step is to implement last itself without involving any blockchain or Bitcoin components. And then the second step is to think of it is to try to.
Royce Steven 26 minutes 23 seconds
integrate the last into existing blockchain system, Bitcoin or Ethereum. So when we are integrating it, then we will have some issues. Of course, you can report what you did to address the issues to make it SE.
Royce Steven 26 minutes 43 seconds
Can be plugged into plugged into the the system, yeah, OK. So, it's confirmed that GitHub reporting doesn't have does not have to be in the report, right? No, no, just concept, but you can you can add it if you want, OK.
Royce Steven 27 minutes 2 seconds
Um, yeah, it just...
Royce Steven 27 minutes 10 seconds
Last time.
Royce Steven 27 minutes 18 seconds
So, if this figure is already...
Royce Steven 27 minutes 22 seconds
Um...
Royce Steven 27 minutes 25 seconds
Usually it's OK, like easy to understand for me, yes. OK, OK, but you can also add this is for what? This is for automic swag or the or the signature. OK, automic swag. OK, so if this figure caption is acceptable, Doctor, if this link.
Royce Steven 27 minutes 44 seconds
Is it too? Yeah, I think for me it's accepted, but let me see. But again, you can see it here. It's still bad. Yeah, there is no paragraph. And maybe at the beginning of the application, you can add a more colorful, but simpler, simpler diagram diagram to show how automate this web.
Royce Steven 28 minutes 5 seconds
Worker in general, right? So, this is more technical, right? Technical, which is good, which will show the technical details, but at the beginning, maybe when you are talking about automated swap, people will will be an occurrence. So, how automated swap will look like? OK, we have two participants, the one is on one chain, one is on the other chain, then...
Royce Steven 28 minutes 26 seconds
One have some Bitcoin, one have some Ether, then when they would like to do the exchange, then how we will build the automated swap, just add some icon, some colorful, yeah, yeah, yeah, diagram, then the people will be, I know it will be more brandy and probably.
Royce Steven 28 minutes 45 seconds
I think, because this is, I'm following like the paper using you and you to probably like make it more friendly, like Alice Bob, yeah, Alice and Bob, that's Alice and Bob for, yeah, yeah, yeah, that's it's more general or friend.
Royce Steven 28 minutes 59 seconds
This, yeah, still too much details, yeah, yeah, yeah, yeah, well, 6 minutes to, yeah.
Royce Steven 29 minutes 6 seconds
This is more like, like, half an hour, yeah, regarding what else if I could?
Royce Steven 29 minutes 17 seconds
I think last time you made a comment about the...
Royce Steven 29 minutes 22 seconds
The figure that I take directly from, and this is a bit like modified, is it? OK, that's good. I mean, the font size, right? The size of text, it looks good, looks better. Yeah, OK, good. To me, I like this finger, yeah, but yeah, it's OK.
Royce Steven 29 minutes 41 seconds
Is there any any figure that you still don't like from the I? I mean, I I don't have fingers don't like, but I my my suggestion that we can always make finger more good, but for that, for for this I think it's it's good. Regarding this, yeah, it's good, it's good. Is it good?
Royce Steven 30 minutes 2 seconds
So what's that? I think this is the Bitcoin structure. Bitcoin structure, yeah. Okay, good. Yes, it's good.
Royce Steven 30 minutes 12 seconds
Is it better to, like, some people say that it's better to use tables from Excel or something, and then for what? Excel or Word, and then why?
Royce Steven 30 minutes 26 seconds
Because I heard that some people says that, I think from the workshop, like previous year students says with presentation that it's better to make like they both. I don't know. Yeah, I think for me, I mean, they are not crypto. I mean, they are not a crypto researchers. Yeah, yeah, yeah, for crypto researchers, who are you seeing?
Royce Steven 30 minutes 45 seconds
Okay, Excel, I mean, I think even for many, because they say that latex label is sometimes like, sometimes it's like this, like out of, but you can always adjust at the side, right? You can auto adjust the data, right? Yeah, okay, so it's just use latex format.
Royce Steven 31 minutes 6 seconds
Table like this, yeah, yeah, yeah, but, but again, you should keep them in the in the page, page, yeah, page, the the the size, of course, it's easy to do that, right? I mean, yeah, for me, for for me, I have I haven't used Excel for quite a long time, especially when I have to do some budget breakdown where I'm applying for grants that they asked me to use Excel, of course, I have to use it.
Royce Steven 31 minutes 30 seconds
But for me, I think, I mean, I like coding, right? I'm glad it's kind of coding, coding to, for writing, right? So I can easily control which position I would like to make, and which color I would like to add. I mean, okay, yeah, up to you, up to you. But for me, my suggestion is that. Just use that, that text, yeah, yeah, tables, yeah.
Royce Steven 31 minutes 51 seconds
Other than that, what else? So, if you use Excel right, then how can you put that into, like, as a like image, image? OK, so if you want to modify it, then you have to go to Excel to modify it, yeah, it's the image, yeah, it's gosh, I don't know what to say, but yeah, you, you, you decide.
Royce Steven 32 minutes 13 seconds
I do, yeah.
Royce Steven 32 minutes 17 seconds
Phone.
Royce Steven 32 minutes 25 seconds
And, probably.
Royce Steven 32 minutes 28 seconds
Or.
Royce Steven 32 minutes 29 seconds
This one, probably I shouldn't say papers, is I should more explicit, which paper is it? The paper, which paper? Yeah, that's why I need to probably like, yeah, if you would like to refer to that paper, then you can add a citation, or you can say in general, how do we do that?
Royce Steven 32 minutes 50 seconds
This is a finger, or this is probably is is it this is a finger, or this is the you you draw it by using that text. This this one is like everything in that text. I I don't know when to use OK, like Excel and make it like OK, picture out of it and yeah, yeah, yeah, sometimes maybe if you want to like...
Royce Steven 33 minutes 10 seconds
Draw some, let's say, more beautiful, more colorful fingers. Of course, you can use DrawPoint I/O. Sometimes I use DrawPoint I/O or use PowerPoint, and then I will export the the what the PDF, and then I put a PDF into inside the PDF.
Royce Steven 33 minutes 29 seconds
So, it will export as PDF, so, so it's higher, high quality, yeah, high quality, draw point, IO, draw point, draw, draw, draw IO, draw IO, yeah.
Royce Steven 33 minutes 43 seconds
Hello, are you? Yeah.
Royce Steven 33 minutes 46 seconds
Oh, I know this one, yeah.
Royce Steven 33 minutes 49 seconds
Yeah.
Royce Steven 33 minutes 51 seconds
And then...
Royce Steven 33 minutes 57 seconds
Yeah, I'm not sure if...
Royce Steven 34 minutes
It's a bit too bad because in the paper, the paper we can like probably explain.
Royce Steven 34 minutes 6 seconds
Like the, there is no like word restriction or something. I don't know. No, no, what? Because sometimes I can't, like, I have, like, I don't know, like, there's too many variables, like...
Royce Steven 34 minutes 20 seconds
Sometimes I don't have the word.
Royce Steven 34 minutes 23 seconds
Enough like...
Royce Steven 34 minutes 25 seconds
What budget to OK? Some, I'm afraid that people, yeah, for for for today.
Royce Steven 34 minutes 34 seconds
Do we need to count to the word no in openings? No, so you can add the table in your projects, what we call a notation table, right? Yeah, summarize what's the meaning of the of the parameter.
Royce Steven 34 minutes 50 seconds
Yeah, I think this is the modification that I try to make from the...
Royce Steven 34 minutes 56 seconds
Original paper, uh-huh, since original, I think it's good, OK.
Royce Steven 35 minutes 2 seconds
But if you read this one or the one, do you understand like there is a C&C&C&C builder?
Royce Steven 35 minutes 12 seconds
But it worked. OK, we have CI can see C, but how did we get the C? I explained in this one, like...
Royce Steven 35 minutes 24 seconds
It's just like a digest of it, like, ah, I see. So, maybe it would be better to add the pain functions inside it, then people will, people might be confused, right? So, this is C, then why this is say, say, say head or say, what's that?
Royce Steven 35 minutes 43 seconds
how to call that.
Royce Steven 35 minutes 46 seconds
Yes.
Royce Steven 35 minutes 51 seconds
Oh, Jesus.
Royce Steven 35 minutes 59 seconds
It makes sense. Yeah, so yeah, I, I, for me, well, I'm reading this, I would ask, OK, here is C, then what's this one? How did we get it?
Royce Steven 36 minutes 10 seconds
Yeah.
Royce Steven 36 minutes 12 seconds
Like, I should probably add like this one. There should be like this. Yeah, yeah, yeah, something like this. There should be one function. Yeah, this one should include sample input. Otherwise people like will be confused, like, why is how the need? Yeah, yeah, yeah, yeah, that's true.
Royce Steven 36 minutes 32 seconds
Yeah, notice you can make another pass.
Royce Steven 36 minutes 36 seconds
Reading it.
Royce Steven 36 minutes 42 seconds
So, I don't know, like this line is like this kind of caption like this like.
Royce Steven 36 minutes 51 seconds
Or is it too long? It's OK. It's OK. It's OK.
Royce Steven 36 minutes 56 seconds
But yeah, and can I... Yeah, you can try to make it shorter and add something into the text. Again, they don't count the caption. I believe they don't. Okay, just double check. I don't know, to be honest.
Royce Steven 37 minutes 15 seconds
Yeah, but it makes like my paragraph is very short, right? Yeah, very short. The caption is too long, paragraph is too short.
Royce Steven 37 minutes 24 seconds
I mean, yeah, you can keep, you can, I don't know, modify a bit to keep the balance.
Royce Steven 37 minutes 32 seconds
So, the marker is will be like people in from cryptography or, or any people from department. Sometimes we will have external sector marker. External means other universities.
Royce Steven 37 minutes 47 seconds
Yeah, I don't know. I mean, it depends. Maybe sometimes because yeah.
Royce Steven 37 minutes 55 seconds
I think maybe from other university students, but that's why they call the external.
Royce Steven 38 minutes 3 seconds
No worries. I mean, I mean, we are, we are strong, right? You have done good work. Don't worry about it. Let's try our best to make it good.
Royce Steven 38 minutes 14 seconds
Yeah.
Royce Steven 38 minutes 30 seconds
So, for this 13th slide, is it do you think it's a good number amount of slides, or it should be? Yeah, I think maybe I would choose less slides, but more, yeah, more visual results, yeah, 'cause imagine if you had 12 slides, right? Which means that for each slide you only have...
Royce Steven 38 minutes 52 seconds
Search media, search seconds.
Royce Steven 38 minutes 55 seconds
Do you think that you can you can present one slide within 30 seconds? Probably maximum 10 slides, I guess. Yeah, 10 slides, more or less 10 slides.
Royce Steven 39 minutes 6 seconds
That's not true, yeah.
Royce Steven 39 minutes 13 seconds
SoE.
Royce Steven 39 minutes 30 seconds
Ohh, I'm wondering one thing as well: if I put the chapter as a conclusion, critical reflection, and future work, is it OK on the header is this conclusion and future work, or it must match that exactly as the how did you generate the headers? I just use the the...
Royce Steven 39 minutes 49 seconds
Template that they gave me.
Royce Steven 39 minutes 53 seconds
Like, I guess.
Royce Steven 39 minutes 56 seconds
You can modify it if you want.
Royce Steven 40 minutes
I just use the template that they gave me.
Royce Steven 40 minutes 3 seconds
OK, from, yeah, I think you should be fine, but if you would like to move safer than just follow, because if I put everything, it's just too long, yeah, it's too long.
Royce Steven 40 minutes 21 seconds
It's fine. Yeah.
Royce Steven 40 minutes 39 seconds
OK, yeah, good. So, yeah, I just find it a bit difficult, um, to how to present it. Yeah, just, yeah, just practice it. Yeah, you still have two more than two weeks, right? Yeah, just try.
Royce Steven 40 minutes 51 seconds
Yeah, you have download, and in terms of the implementation, it's just for the coding, but I think is I can just freeze it up. I think so. If you think they are ready, because I haven't checked the details. I don't think I have time to check all the details, but yeah, you, you decide, right? Your call.
Royce Steven 41 minutes 11 seconds
Yeah.
Royce Steven 41 minutes 13 seconds
But I believe that it's more...
Royce Steven 41 minutes 16 seconds
Valuable to spend the time, like, to, yeah, understand, yeah, yeah, in details, yeah, yeah, and the concept, so I can, like, yeah, makes sense, makes sense. Now, it's good to focus on the quality, yeah, of the quantity, yeah, the quality of the presentation, yeah, especially the presentation where it's the writing and also the...
Royce Steven 41 minutes 36 seconds
The the the video does forecast and make sure that the results is verify all the results, yeah, yeah, make sure, yeah, verify the results, yes, verify the results, you don't need to do new things, I think you have enough content, so now you should make that all of them, they are correct, they are precise, yeah.
Royce Steven 41 minutes 56 seconds
I think from what I'm still liking, like delivering the method that that I'm using, so still not clear, like probably the marker still questions from my presentations, like what is the method? Yeah, yeah, I think methodology is very important, yeah, yeah, yeah, yeah.
Royce Steven 42 minutes 16 seconds
It's not only just the resource, but how you get the resource, yeah, yeah, OK.