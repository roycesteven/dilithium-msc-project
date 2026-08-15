# 6–8 minute project video — recording plan

**One artefact, one window: `video_deck.html`.** The whole video is the deck, opened in a
browser and put fullscreen with `F`. There is **no screen capture to record and cut in**,
and in particular **no terminal**: a transcript of a test binary shows that a run happened,
not what happened, and it reads as evidence rather than as a demonstration. Both
demonstrations are drawn and stepped inside the deck itself.

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

**Deck keys:** `→ ←` navigate · `N` speaker notes · `T` start the timer · `S` show the
talking-head safe area · `G` slide grid · `F` fullscreen · `video_deck.html#5` opens on
slide 5 for rehearsing one beat.

⚠ **Slides 5 and 9 have beats inside them.** Each **opens already on its first beat**; `→`
then advances to the next beat and only moves to the following slide once the beats are
exhausted (`←` walks back the same way). So slide 5 takes **three** presses to walk its four
beats, and slide 9 **one** press to walk its two. The footer prints `beat k/n` so you always
know where you are. Nothing else in the deck behaves differently.

---

## 1. Structure and timing

Planned total **7:12**, inside the 6–8 minute band — 48 s under the ceiling and 72 s over
the floor, so overrunning is the risk to rehearse against, not underrunning. The deck
prints each slide's planned time and the running clock in its own footer; the table below is
the same plan in one view.

| # | Slide | Plan | Job in the story |
|---|---|---|---|
| 1 | Title | 0:12 | who, what, one sentence |
| 2 | The gap | 0:35 | **beginning** — why this project exists |
| 3 | What an adaptor signature does | 0:40 | the mechanism, in four animated beats |
| 4 | What I built | 0:35 | additive architecture; two implementations, one digest |
| 5 | **DEMO A — walking the swap** | 0:45 | *complement*: the protocol, stepped and broken (4 beats) |
| 6 | Result 1 — the adaptor layer is cheap | 0:40 | **middle** — computation |
| 7 | Result 2 — the cost is bytes | 0:38 | communication |
| 8 | Result 3 — the proof dominates | 0:35 | the application-level surprise |
| 9 | **DEMO B — a real Bitcoin client** | 0:45 | *complement*: the node differential (2 beats) |
| 10 | On-chain verification | 0:32 | the negative result that turned over |
| 11 | Testing my own assumption | 0:28 | the ML-DSA experiment |
| 12 | What failed | 0:25 | three closed directions |
| 13 | Conclusion | 0:22 | **end** — what holds, what stays open |

**If you run long**, cut in this order: slide 11 → slide 2's third card → slide 8's middle
card. Do **not** cut a demonstration beat — the two demonstrations are where *Use of the
Medium* and *Complementing the Report* are earned, and those are 80 % of the video mark
between them.

---

## 2. The two demonstrations — how each is driven

### DEMO A — walking the swap (slide 5, ~45 s, 4 beats, 3 presses)

The board is u₁ · two chains · u₂, with the objects each party holds and the measured size
of each. The exchange has already happened when the slide opens, **on beat 1**.

| beat | reached by | what is on screen | the line to land |
|---|---|---|---|
| 1 | slide opens | opening state; rail step 1 lit | u₂ pre-signed only after **both** π and PreVerify(σ̂₁) passed — at this instant neither pre-signature is spendable by anyone, and nobody has risked a coin |
| 2 | 1st `→` | **REJECTED** stamp over the chains | u₂ holds σ̂₁ and tries to spend it: refused. A pre-signature is not a signature — this is the property the construction rests on |
| 3 | 2nd `→` | σ₂ appears at u₁; chain 2 turns settled | on chain it is an **ordinary payment** — no script, no hash lock, nothing to see |
| 4 | 3rd `→` | y′ appears at u₂; chain 1 turns settled | u₂ needs **nothing further from u₁**: it reads σ₂ off chain 2, extracts y′, adapts σ̂₁ with *that* value, and publishes |

Beat 4 is the one to slow down on — it is the whole reason a swap is atomic.

### DEMO B — a real Bitcoin client (slide 9, ~45 s, 2 beats, 1 press)

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
- Rehearse slides 5 and 9 with `#5` / `#9` until the beat presses land with the sentence —
  they are the only places where a keypress has to be timed to speech.
- Theme follows the system setting; both light and dark are laid out deliberately. Pick one
  and keep it for the whole recording.

---

## 4. What the rubric rewards, and where it is earned

| Criterion | Weight | Where |
|---|---|---|
| Use of the medium | 40 % | the protocol stepped and broken on screen (5); the node differential drawn as a two-column verdict (9); the mechanism animated (3); every chart generated from the evidence rather than screenshotted; talking-head overlay; no terminal transcripts anywhere |
| Complementing the report | 40 % | slide 5 makes the protocol *watchable* — the abort gate, a pre-signature refused, the witness falling out of a published signature — where the report can only assert that each step was asserted; slide 9 puts the patched and stock verdicts side by side as one contrast, which the appendix can only state in prose |
| Presentation | 20 % | fixed 7:12 plan with per-slide budgets and an on-screen clock; a clear beginning (2), middle (6–8), end (13) |

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
- **One LAS settlement transaction** weighs the weight units on slide 9 — that is one
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
