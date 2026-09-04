# 6–8 minute project video — recording plan

**One artefact, one window: `video_deck.html`.** The whole video is the deck, opened in a
browser and put fullscreen with `F`. There is **no screen capture to record and cut in**,
and in particular **no terminal**: a transcript of a test binary shows that a run happened,
not what happened, and it reads as evidence rather than as a demonstration. Both
demonstrations are drawn and stepped inside the deck itself.

The deck carries the **University of Manchester** house style: Arial, purple `#7800a2`
headlines, the dotted `#660066` rule on the title slide, and the Manchester mark top-left on
every slide. The mark is reproduced from the university's own 16:9 master,
`Master_169 presentation(2).pptx` (see `assets/README.md`), and is embedded into the deck so
it stays one self-contained file. The deck's blue / orange / green / red accents are **not**
brand colours — they are semantic marks shared with the report's figures — so brand purple is
used for chrome only. A PowerPoint conversion exists as `video_deck_uom.pptx` (built by
`scripts/gen_slides_pptx.py`) **for submission only**; rebuild and re-audit it after HTML
changes. The HTML deck is what gets recorded and is the authoritative wording.

`swap_console.html` remains in the repository as a standalone interactive artefact — the
same protocol with a configuration switch, a tripwire button and a per-step communication
ledger. It is **not** part of the recording; keep it for anyone who wants to drive the
protocol themselves, and as a fallback if a live viewer asks to see another configuration.

Both files are **generated**, not hand-written: `scripts/gen_slides.py` fills every number
in the two `*.template.html` files from `report/latex/generated/*.tex`, the same macros the
report reads. Edit the **template**, never the output, then:

```bash
python3 scripts/gen_slides.py           # rebuild the deck
python3 scripts/gen_slides.py --check   # non-zero if the committed deck is stale
```

Re-run it after any `scripts/sync_report.sh`, or the slides will quote an older evidence
run than the report does.

**Deck keys:** `→ ←` navigate · `N` speaker notes · `B` the BACKUP half of the notes · `T` start
the timer · `S` show the talking-head safe area · `G` slide grid · `F` fullscreen · `H` the key
map · `video_deck.html#7` opens on slide 7 for rehearsing one beat.

⚠ **The notes panel is a teleprompter, not a dump.** `data-notes` holds `SPOKEN: … || BACKUP: …`
and only the SPOKEN half is delivered, so `N` shows that half alone, at reading size; `B` reveals
BACKUP underneath when a question needs it. Before this split reached the viewer the panel printed
both halves at 15 px, which read as an over-long script and invited cutting narration that was
never going to be spoken — do not re-merge them.

⚠ **The key-map overlay now defaults to OFF** — it is drawn over the page, so it used to sit in
shot for the whole take. Press `H` while rehearsing, and it stays hidden when you record.

**A section stepper (Why · Method · Results · Takeaways) sits top-right**, lighting the part the
current slide belongs to; the title slide shows none. That is Wang's "where are we?" marker — the
thin progress bar says how *far*, not *where*.

⚠ **Slides 7 and 12 have beats inside them.** Each **opens already on its first beat**; `→`
then advances to the next beat and only moves to the following slide once the beats are
exhausted (`←` walks back the same way). So slide 7 takes **three** presses to walk its four
beats, and slide 12 **one** press to walk its two. The footer prints `beat k/n` so you always
know where you are. Nothing else in the deck behaves differently.

---

## 1. Structure and timing

**Fifteen slides**, planned deck budget **7:52** — inside the 6–8 minute band, but with only
about eight seconds of footer margin, so the timed rehearsal decides, not this number.

⚠ **`data-time` is a rehearsal budget, not computed by `gen_slides.py`.** Re-count the SPOKEN
half after every narration edit and adjust the budget deliberately. The M12 rehearsal ran
≈10:40, and 150 wpm was the figure that made the old budget look safe when it was not. The
current SPOKEN script is **797 words**; at the measured **112 wpm** that is about **7:07** of
speech, leaving the remaining deck budget for beat presses, breaths and slide transitions.
The count sits above Meeting 10's "more or less 10" because later feedback kept distinct
beats visible: the quantum urgency (2), the transaction structures of both venues (11), the
deployment verdict with takeaways for Bitcoin and blockchain developers (15), and the
split cost/check slides that prevent mixed reading directions.

The deck prints each slide's planned time and the running clock in its own footer; the table
below is the same plan in one view, with the part each slide belongs to — which is what the
top-right stepper lights.

| # | Slide | Part | Plan | Job in the story |
|---|---|---|---|---|
| 1 | Title | — | 0:13 | who, what, one sentence |
| 2 | **Why now — the quantum clock** | Why | 0:40 | **the urgency**: one current key-recovery estimate, the permanence of a ledger, and NIST's retirement date |
| 3 | Why this matters | Why | 0:39 | what it protects — two chains, one shared secret, real coins at the real market rate |
| 4 | Why post-quantum, **and why this scheme** | Why | 0:38 | two drawn scenes: the signature stack and the HTLC-vs-adaptor contrast, then the three questions |
| 5 | What an adaptor signature does | Method | 0:34 | the four functions as one flow, each led by a plain-English line |
| 6 | The method | Method | 0:39 | additive architecture, the challenge-hash substitution, and C/Rust byte agreement |
| 7 | **DEMO A — walking the swap** | Results | 0:39 | *complement*: the protocol, stepped and broken (4 beats, 3 presses) |
| 8 | Cost in time | Results | 0:30 | **two steps**: post-quantum migration is the expensive step; the measured adaptor layer is small and benchmarked carefully |
| 9 | The same step, done classically | Results | 0:16 | the classical adaptor layer costs more relative to its own base, while still winning absolute time |
| 10 | Cost in bytes | Results | 0:32 | communication is the most pronounced post-quantum price |
| 11 | **What actually goes on chain** | Results | 0:35 | **in practice**: both venues' real transaction structures, field by field |
| 12 | **DEMO B — a real Bitcoin client** | Results | 0:34 | *complement*: the node differential (2 beats, 1 press) |
| 13 | Does it fit on chain? | Results | 0:26 | the EVM cap result plus two shortcuts tested rather than hand-waved |
| 14 | Does the adaptor need a simplified base? | Results | 0:16 | the functional ML-DSA-65 boundary check |
| 15 | **Takeaways — what it means in practice** | Takeaways | 0:41 | **the close**: the three questions answered, the verdict, and one consequence per audience |

**If you run long**, cut in this order: slide 13's shortcut sentence after the gas answer
→ slide 14's Esgin-background sentence → slide 10's final size-target sentence.
⚠ **The fourth entry, "slide 2's second card (the NIST date)", is WITHDRAWN (2026-08-30) as a
PROJECT-LEVEL presentation choice:** it is the single concrete deadline/urgency marker left on
that slide after Meeting 12's density feedback, and it mirrors the revised `fig:whynow`
argument. ⚠ **It is NOT a Wang video requirement** — his "in how many years" line belongs to
M12 §8, spoken over the *report* PDF about Figure 1.1(a), while his feedback on this *slide*
ran the other way (too many numbers, "which one is the most important?"). Do not cite him for
keeping it; revisit it if a timed rehearsal still exceeds 8:00 after the cuts above. Do **not**
cut a demonstration beat — the two demonstrations are where *Use of the Medium* and *Complementing
the Report* are earned, and those are 80 % of the video mark between them. **If you run
short**, slow down on slide 7's fourth beat and slide 12's patched-vs-stock contrast.

⚠ **What the video must NOT be is the report read aloud** (Royce, 2026-08-21). Every slide
here either *shows* something the report can only assert — the protocol stepped and broken,
the two clients disagreeing, the two transaction structures side by side — or states a
consequence *for a named audience*, which a dissertation chapter does not do. Where the deck
carries a fact the report does not, it is cited on the slide itself: slide 2's quantum
estimates and NIST date, and slide 3's spot prices, are **cited** claims, not measured ones.
⚠ **Corrected 2026-08-26:** slide 2's three estimates and the NIST transition date were once
deck-only. They are **now in the report** — `refs.bib` gains `gidney2021factoring`,
`gidney2025factoring`, `babbush2026securing`, `nistir8547`, and they appear in §1.1 with
`fig:whynow` — so the deck no longer asserts anything the report lacks. Do not reinstate the
older "deck-only motivation" wording.

### Narration — how the SPOKEN half is written (rewritten 2026-08-26)

The scripts were rewritten after reading five past MSc video transcripts (`past_report/`).
**Three of the five (2, 4, 5) worked**: they opened from the audience's own world, *showed* the
problem rather than asserting it, carried one plain analogy for the hard step, said what each
number **meant** immediately after saying it, and closed on the questions the opening posed. The
**other two (1, 3)** read as a report aloud — an agenda slide, chained jargon, and figures
recited without interpretation. Five rules follow, and the first four are also what buys the time:

1. **The slide carries the figures; the narration carries the meaning.** Text already on screen
   is the cheapest thing to cut, and the largest trims in the 2026-08-26 pass came from exactly
   there (slide 2's "chain cannot wait" cards, slide 8's three percentages, slide 15's three
   implication cards).
2. **Open each part with a question the audience would ask**, and answer it in the next breath.
3. **One analogy per hard idea** — slide 5's "a signature that is deliberately incomplete" is the
   deck's only one.
4. **What must be SPOKEN regardless of the slide:** the base-naming two-step (slide 8) and the
   UTXO-with-UTXO scope note (slide 7). Nothing on screen carries either, and both are rulings.
5. ⚠ **Methodology is not jargon** — Wang's M10 ruling is *"it's not only just the results, but
   how you get the results"*, so how a thing was measured stays in the script; it is said in plain
   words instead ("run back to back in the same session to reduce drift", not "paired and
   interleaved"). Cutting jargon must never cut the method.

## 2. The two demonstrations — how each is driven

### DEMO A — walking the swap (slide 7, ~44 s, 4 beats, 3 presses)

The board is u₁ · two chains · u₂ — **two UTXO ledgers**, which the eyebrow and the opening
narration now state (Meeting 11 scope ruling), with the objects each party holds and the measured size
of each. The exchange has already happened when the slide opens, **on beat 1**.

| beat | reached by | what is on screen | the line to land |
|---|---|---|---|
| 1 | slide opens | opening state; rail step 1 lit | u₂ pre-signed only after **both** π and PreVerify(σ̂₁) passed — at this instant neither pre-signature is spendable by anyone, and nobody has risked a coin |
| 2 | 1st `→` | **REJECTED** stamp over the chains | u₂ holds σ̂₁ and tries to spend it: refused. A pre-signature is not a signature — this is the property the construction rests on |
| 3 | 2nd `→` | σ₂ appears at u₁; chain 2 turns settled | on chain it is an **ordinary payment** — no script, no hash lock, nothing to see |
| 4 | 3rd `→` | y′ appears at u₂; chain 1 turns settled | u₂ needs **nothing further from u₁**: it reads σ₂ off chain 2, extracts y′, adapts σ̂₁ with *that* value, and publishes |

Beat 4 is the one to slow down on — it is the whole reason a swap is atomic.

### DEMO B — a real Bitcoin client (slide 12, ~34 s, 2 beats, 1 press)

The same spend, byte for byte, put to two clients of the same release.

| beat | reached by | what is on screen | the line to land |
|---|---|---|---|
| 1 | slide opens | both columns show the valid spend accepted | a spend with **no elliptic-curve signature at all** was accepted and mined by the patched node — and the stock node accepted the same block |
| 2 | 1st `→` | the control rows appear: patched **REJECTED**, stock **ACCEPTED** | a stock node still treats the opcode as `OP_SUCCESS`, so it accepts every one of these variants regardless of the signature — it cannot be reacting to the signature at all. The difference between the columns is the new rule, **and nothing else** |

Say that verdicts are **consensus** decisions (block validation), not mempool policy — the
distinction is on the slide because it is what makes the result about validity rather than
about relay.

**To re-run the experiment** (not needed for the recording — the numbers on the slide come
from the macros): the script takes four required environment variables and exits at once
without them. These are the paths the last successful run recorded in
`evidence/btc_las_node/latest/environment.txt`; all pin gates were re-checked and pass.

```bash
BTC_TAG=v31.1 \
BTC_SRC=/home/melly/btc-stage2/src-31.1 \
BTC_BIN_PATCHED=/home/melly/btc-stage2/src-31.1/build/bin/bitcoind \
BTC_BIN_STOCK=/home/melly/btc-stage2/bitcoin-31.1/bin/bitcoind \
scripts/run_btc_las_node.sh
```

If it fails, the message names which gate refused; a `FAIL` run keeps its evidence
directory, so read `evidence/btc_las_node/<run>/verdict.txt`.

---

## 3. Recording setup

- **Deck at 1280×720**, fullscreen (`F`). The stage is a fixed 16:9 canvas scaled to the
  window, so what you see is exactly what records. One window for the whole take.
- **Talking-head overlay bottom-right.** Press `S` to show the safe area while you set the
  camera, then press it again — no slide puts content there.
- **Turn the speaker notes OFF (`N`) before recording.** They are a rehearsal aid and are
  drawn over the page.
- **Press `T` when you start speaking.** The clock turns red past 8:00.
- Rehearse slides 7 and 12 with `#7` / `#12` until the beat presses land with the sentence —
  they are the only places where a keypress has to be timed to speech.
- Theme follows the system setting; both light and dark are laid out deliberately. Pick one
  and keep it for the whole recording.

**Checking the layout without opening a browser.** Windows Chrome is reachable from WSL, and the
deck deep-links every slide, so any slide can be screenshotted headless — which is how the voids
and one clipped table were found on 2026-08-17. Stage a copy under a Windows path (the CSP-free
`file://` origin and the embedded figures make it self-contained), then:

```bash
WT=/mnt/c/Users/Royce/AppData/Local/Temp/deckshot; mkdir -p "$WT"
# force a theme by stamping the root element; omit the sed to follow the host setting
sed 's|<html lang="en">|<html lang="en" data-theme="light">|' report/slides/video_deck.html > "$WT/deck.html"
"/mnt/c/Program Files/Google/Chrome/Application/chrome.exe" --headless=new --disable-gpu \
  --window-size=1280,720 --virtual-time-budget=6000 \
  --screenshot="C:\\Users\\Royce\\AppData\\Local\\Temp\\deckshot\\s4.png" \
  "file:///C:/Users/Royce/AppData/Local/Temp/deckshot/deck.html#4"
```

`--virtual-time-budget` must exceed the scene animations (the four stages finish at ~2.4 s) or the
shot catches them mid-build. `#N` opens slide N **on its first beat**. **Later beats are reachable
after all**: append a script that dispatches `KeyboardEvent('keydown',{key:'ArrowRight'})` n times
on `document`, then kills `transition`/`animation` on every element ~1 s later, or the shot catches
a fade half-done (that is not a defect — re-shoot before reporting one).

**Measuring overflow instead of eyeballing it** (`report/slides/audit_overflow.js`, added
2026-08-26). Append it to a staged copy and run Chrome with `--dump-dom`; it walks **every slide and
every beat** and prints, into a `<pre id="zzAUDITOUT">`, anything painting outside the slide's
content box, any child escaping the card that frames it, and any SVG `<text>` wider than the `<rect>`
behind it. Two lessons from building it: measure against `slide.clientWidth`, not a padding guess,
or every container reports a uniform false ~8 px overrun; and its SVG text↔rect pairing is a
heuristic, so discard hits where the text sits wholly outside the rect it was matched to. It found
what four rounds of eyeballing had missed — a 67 px clipped line on slide 5 and hidden beat blocks
pushing slide 7's cards over the rail. ⚠ Screenshots still decide: the audit says *where*, the
render says whether it reads.

⚠ **THE AUDIT CANNOT SEE FOOTER COLLISIONS — a clean run is not a clean slide (2026-08-30).** It
tests escapes from a slide's content box and from the card that frames an element; `#foot` is
positioned separately, so content *reaching* it is neither. Slide 13 passed the audit while its
three columns painted 21 px through the footer and their coloured rules struck out the slide
number — caught only by the screenshot. **Shoot the slide as well as auditing it**, and when a
headline grows to two lines, re-measure the slides below it against `#foot`, not just the audit.
The cause there is worth knowing: `.v3 .cav` is a `<p>` and had no `margin:0`, so it carried the
UA default `1em`. Check that on any new text element before blaming the layout.

---

## 4. What the rubric rewards, and where it is earned

| Criterion | Weight | Where |
|---|---|---|
| Use of the medium | 40 % | the protocol stepped and broken on screen (7); the node differential drawn as a two-column verdict (12); the quantum timeline, the swap scenario, the signature stack with its migration arrow, the HTLC-vs-adaptor contrast, the four functions and the architecture each drawn and built in stages (2–6); the adaptor-layer cost drawn as bars on shared scales (8–9); both transaction structures laid out field by field (11); every chart generated from the evidence rather than screenshotted; talking-head overlay; no terminal transcripts anywhere |
| Complementing the report | 40 % | slide 7 makes the protocol *watchable* — the abort gate, a pre-signature refused, the witness falling out of a published signature — where the report can only assert that each step was asserted; slide 12 puts the patched and stock verdicts side by side as one contrast, which the appendix can only state in prose; slide 15 turns the results into consequences **for a named audience**, which the dissertation never does |
| Presentation | 20 % | planned 7:52 deck budget, 797 spoken words, and an on-screen clock; a section stepper for orientation; a clear beginning (2–4), middle (5–14), end (15) |

---

## 5. Say this, not that — claim discipline on camera

The same rules that govern the report govern the narration. Every one of these has a reason
recorded in `CLAUDE.md`; none is stylistic.

- **The known-answer digest binds outputs, not internals.** It folds the packed key pair,
  signature, pre-signature and adapted signature of four fixed vectors. PreVerify and
  Extract are *asserted* per vector, never hashed — do not say it "covers all four adaptor
  functions", and do not say "any divergence flips it".
- **Quote π at the size it was measured on the wire**, which is what the slide prints. The
  parameter set's own stated proof size is a different figure and a different kind of claim;
  it belongs in the report, with its label, not on camera.
- **u₂ needs nothing further from u₁** after the leak — it does still publish its own
  settlement transaction, so never say it "sends nothing". And `Ext` takes the statement as
  well as the two signatures.
- **The rejection gate is over sign-class calls.** Attempts are counted, never inferred from
  timing, and the benchmark aborts unless the attempts summed over every timed Sign and
  PreSign call match the closed-form prediction within five standard errors. Verify-class
  calls have no attempt counter, and the figures on the slide are the distribution sample's,
  not the gated aggregate's.
- **One LAS settlement transaction** weighs the weight units on slides 10 and 11 — that is one
  transaction against the standardness *weight* ceiling, not the whole two-leg swap, and
  fitting that ceiling is not the same as being standard: the carriage run was refused by
  default relay policy.
- **Do not quote LaBRADOR figures.** Meeting 9 ruled the un-refined direction stays
  discussion *without* numbers so no conflicting result appears. The deck reports none.
- **A patched node is not Bitcoin.** Say the rule was added experimentally; "cannot settle
  on Bitcoin as it stands" remains true. Its security is **not** analysed.
- **The one-transaction result is scoped:** Simplified Dilithium-III, a 32-byte signed
  message, that EVM revision. Reading the remaining margin as a message-length budget is a
  **derivation**, not a measurement — the margin itself is one instance's measured headroom.
  ⚠ Updated 2026-08-15 — **D2 and D5 have since been evaluated**, so "not evaluated" is now
  wrong: **D2 measured, fits at ~65% of the cap**; **D5 derived to exceed one transaction** (a
  measured *lower bound*, so never quote a D5 gas total, and never say it "needs more
  optimisation"). ⚠ Every measured row is **one signature instance** — `SampleInBall` and
  `_decodeZ` are data-dependent — so say "the measured instance fits", and do not claim
  instance variation is negligible at any set; it is unquantified.
- **The classical baseline is functionality-matched, not security-matched**, and it is an
  ECDSA *adaptor*, never plain ECDSA.
- **The ML-DSA result is a functional demonstration.** Whether committing to
  `HighBits(w+Y)` preserves unforgeability is not analysed.
- **PreSign / PreVerify / Adapt / Extract are functions, not protocols**, and "transaction"
  never means "the signed message" — what travels is the transaction, what is signed is its
  sighash.
- Do not claim the STARK gadget as π — it is not zero-knowledge, and it is excluded from
  both the report and this deck.

- **The quantum slide is three CITED estimates, not a forecast.** Say "resource estimates for
  hardware that does not exist". The fall quoted on camera is for **one fixed target**
  (RSA-2048, 20 M qubits in 2019 → under 1 M in 2025); the secp256k1 figure is a separate
  2026 estimate, so never divide one by the other and call the quotient a trend. The NIST row
  is the **≥128-bit** one — *disallowed after 2035* — because that is where secp256k1 sits;
  the widely quoted "deprecated after 2030" belongs to the 112-bit row and would be the wrong
  row to attach to a blockchain curve. Never give a date for when a machine will exist.
- **The prices are cited, and they are the ratio.** The two legs are *value-matched at spot*,
  so what the picture asserts is the 1 : 32.4 exchange ratio, sourced and time-stamped on the
  slide. **Re-read both sources before recording on another day** — a stale price is a wrong
  number, not a rounding — and re-run `gen_slides.py` after editing the template.
- **The two transaction structures are different KINDS of figure.** Bitcoin's sizes are
  *derived*: measured object sizes projected onto the wire format. Ethereum's gas is
  *measured*: one real client's receipt. The slide tags each; the narration must not blur them
  into "we measured both".
- **Never say "the adaptor layer is nearly free" without naming the base.** Wang read exactly
  that headline as a comparison against Bitcoin's ECDSA, where nothing here is free (2026-08-21).
  The narration is the **two-step migration**: step 1, classical → post-quantum *basic*, is the
  expensive one and is the step organisations are already taking; step 2, basic → *adaptor*, is
  what this project measured, **against the same base signature at identical parameters**. Say
  "against its own post-quantum base", never a bare percentage. The classical comparator is the
  **same step charged the same way** — the ECDSA adaptor's PreSign over its own Sign, ×`\clOvPreSignX`
  — and it is **derived from that harness's unpaired per-operation means**, so it may not be
  called a paired overhead; quote the LAS byte-tier figure beside it, because the claim has to
  survive the conservative pairing. Never let the slide imply LAS is *faster* than ECDSA in
  absolute time: it is not, and that cost belongs to step 1.
- **Step 1's ×72 is *this build's* simplified lattice base**, not the standardised FIPS 204 one —
  the ML-DSA route on slide 14 measures a smaller signature. Saying "post-quantum costs ×72"
  flatly overstates the price of the step the audience is being told they are already paying.
- **The measured swap is UTXO-with-UTXO** (Wang, Meeting 11: *"it would be safer to just talk
  about UTXO"*). DEMO A's eyebrow and opening line say so out loud. Bitcoin↔Ethereum is the
  *motivating scenario*, never the artefact — Wang confirms it is doable and that "the only
  different thing is the verification", which is exactly how the narration frames it. Never
  imply a live BTC↔ETH swap was run, and **never use ETH↔USDC as an example** — ERC-20 tokens
  swap inside Ethereum via contracts and need no atomic swap; the case that needs one is going
  *outside* Ethereum, and Ethereum itself uses bridges for that.
- **The coins never move between chains** (Wang, Meeting 11, laboured twice). Slide 3 now draws
  each payment as a transaction that settles ON its own chain, with the shared secret as the
  only element crossing the middle; the headline caption says "neither coin ever leaves its
  chain". Never reintroduce a card-to-card coin arrow, and say "settles on its own ledger —
  the swap is the linkage" on camera.
- **Venue vocabulary is Wang's own** (Meeting 11, replacing a framing he called "a bit
  misleading"): smart contracts are **"more flexible"**; Bitcoin is **"more restricted,
  because [its fields] cannot be modified"** — miners verify directly against the transaction
  content, while a contract compiles to opcodes the EVM executes. This sits alongside, and
  does not replace, "the blocker is a consensus rule, not engineering".
- **No TPS number, in any form.** Meeting 11 closed on a whiteboard TPS derivation
  (transactions-per-block ÷ block interval); every number in it was improvised ("let's say
  200", "just Google it") and the derivation collides with a documented, once-retracted rule.
  Until Royce rules on it, the deck and narration carry **no throughput figure** — the
  supported quantities remain the per-transaction cap percentage, the serial verification
  rate (never called a network throughput), and the block-weight ceiling, each on its own.
- **The verdict is a judgement, not a measurement.** "Good enough to build on, not yet to
  deploy" is supported by three stated facts — the parameter set's concrete security is
  unanalysed by design, the consensus rule's security is unanalysed, and nothing settled on a
  live network. Say those, not "it is not secure enough", which claims an analysis nobody ran.
