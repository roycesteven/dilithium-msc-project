# 6–8 minute project video — recording plan

Two artefacts, both opened in a browser:

- **`video_deck.html`** — the 13-slide deck (`F` for fullscreen).
- **`swap_console.html`** — an interactive walkthrough of the swap, used as Demo A.
  Step it with `→`, switch configuration with `1` `2` `3`, `T` fires the tripwire.
  `?step=4&cfg=3` opens on a given state, and the URL follows as you move, so a
  particular moment can be bookmarked for a retake.

Both are **generated**, not hand-written: `scripts/gen_slides.py` fills every number in
the two `*.template.html` files from `report/latex/generated/*.tex`, the same macros the
report reads. Edit the **template**, never the output, then:

```bash
python3 scripts/gen_slides.py           # rebuild the deck
python3 scripts/gen_slides.py --check   # non-zero if the committed deck is stale
```

Re-run it after any `scripts/sync_report.sh`, or the slides will quote an older
evidence run than the report does.

**Deck keys:** `→ ←` navigate · `N` speaker notes · `T` start the timer · `S` show the
talking-head safe area · `G` slide grid · `F` fullscreen · `video_deck.html#7` opens on
slide 7 for rehearsing one beat.

---

## 1. Structure and timing

Planned total **7:15**, inside the 6–8 minute band with ~45 s of slack either way. The
deck prints each slide's planned time and the running clock in its own footer; the table
below is the same plan in one view.

| # | Slide | Plan | Job in the story |
|---|---|---|---|
| 1 | Title | 0:12 | who, what, one sentence |
| 2 | The gap | 0:35 | **beginning** — why this project exists |
| 3 | What an adaptor signature does | 0:40 | the mechanism, in four animated beats |
| 4 | What I built | 0:35 | additive architecture; two implementations, one digest |
| 5 | **DEMO A — walking the swap** | 0:45 | *complement*: the protocol, stepped and broken |
| 6 | Result 1 — the adaptor layer is cheap | 0:40 | **middle** — computation |
| 7 | Result 2 — the cost is bytes | 0:38 | communication |
| 8 | Result 3 — the proof dominates | 0:35 | the application-level surprise |
| 9 | **DEMO B — a real Bitcoin client** | 0:48 | *complement*: carriage, then a patched node |
| 10 | On-chain verification | 0:32 | the negative result that turned over |
| 11 | Testing my own assumption | 0:28 | the ML-DSA experiment |
| 12 | What failed | 0:25 | three closed directions |
| 13 | Conclusion | 0:22 | **end** — what holds, what stays open |

**If you run long**, cut in this order: slide 11 → slide 2's third card → slide 8's
middle card. Do **not** cut a demo: they carry 40 % of the video mark.

---

## 2. Shot list — the two capture segments

Both demo slides are letterboxed frames sized for a **1280×720** capture, so a recording
made at that size drops in 1:1 with no rescaling. Record each *before* the voice-over,
then trim to the beats below.

### DEMO A — walking the swap (slide 5, ~45 s)

Record **`swap_console.html`**, not a terminal. A test binary printing lines is evidence,
not a demonstration: it shows the run happened, but not what happened, and the rubric
rewards "interactive visualisations … that help the audience better understand complex
concepts and workflows". The console is that — a replay of the measured run, driven by
the same macros as the report, with the protocol as a board you can step.

It carries a banner saying it is a replay, not live cryptography. **Leave that banner
on**: it is what keeps the demo honest.

Three beats, in this order:

1. **Step to 2 — the abort gate.** `u₂` commits nothing until both π and `σ̂₁` verify. Say
   the line on screen: at this instant *neither pre-signature is spendable by anyone*.
2. **Press the red button — the tripwire.** `u₂` holds `σ̂₁` and tries to spend it:
   REJECTED. This is the property that distinguishes an adaptor signature from a
   signature, and it is asserted on every one of the 1000 functional-test iterations.
3. **Step to 4 — the leak.** No message is sent. `u₂` reads `σ₂` off chain 2, subtracts
   its own pre-signature, and the witness falls out. Then **flip the configuration
   switch** (`1` → `3`) and let the byte bars move: same protocol, 941 B against ~80 kB.

Optional corroboration if a beat runs short — the same protocol executing for real:

```bash
cd ref && make test/test_swap3 && ./test/test_swap3
```

### DEMO B — a real Bitcoin client (slide 9, ~48 s)

⚠ The script takes **four required environment variables** and exits immediately without
them. The paths below are the ones the last successful run recorded in
`evidence/btc_las_node/latest/environment.txt`; all four pin gates were re-checked and
still pass:

```bash
BTC_TAG=v31.1 \
BTC_SRC=/home/melly/btc-stage2/src-31.1 \
BTC_BIN_PATCHED=/home/melly/btc-stage2/src-31.1/build/bin/bitcoind \
BTC_BIN_STOCK=/home/melly/btc-stage2/bitcoin-31.1/bin/bitcoind \
scripts/run_btc_las_node.sh
```

If it still fails, the message names which gate refused; a `FAIL` run keeps its evidence
directory, so read `evidence/btc_las_node/<run>/verdict.txt`.

The shot that matters is the **differential**: the same mutated spend put to both nodes,
patched **rejecting** and stock **accepting**. Show them side by side — that contrast is
the entire argument, and it is the thing the report can only describe. If you want a
third beat, `scripts/run_btc_las_bench.sh` prints the opcode's verification cost against
Schnorr and ECDSA.

Optional B-roll if a segment runs short: `scripts/run_onchain_one_tx.sh` mining the
one-transaction claim and printing the receipt (backs slide 10).

---

## 3. Recording setup

- **Deck at 1280×720**, fullscreen (`F`). The stage is a fixed 16:9 canvas scaled to the
  window, so what you see is exactly what records.
- **Talking-head overlay bottom-right.** Press `S` to show the safe area while you set
  the camera, then press it again — no slide puts content there.
- **Turn the speaker notes OFF (`N`) before recording.** They are a rehearsal aid and are
  drawn over the page.
- **Press `T` when you start speaking.** The clock turns red past 8:00.
- Theme follows the system setting; both light and dark are laid out deliberately. Pick
  one and keep it for the whole recording.

---

## 4. What the rubric rewards, and where it is earned

| Criterion | Weight | Where |
|---|---|---|
| Use of the medium | 40 % | the interactive swap console; the real-client capture; the animated mechanism on slide 3; charts generated from the evidence rather than screenshotted; talking-head overlay |
| Complementing the report | 40 % | slide 5 makes the protocol *manipulable* — stepping it, breaking it with the tripwire, switching the configuration under it — none of which a figure can do; slide 9 shows a patched consensus rule accepting a lattice-authorised spend that no report table conveys |
| Presentation | 20 % | fixed 7:15 plan with per-slide budgets and an on-screen clock; a clear beginning (2), middle (6–8), end (13) |

---

## 5. Say this, not that — claim discipline on camera

The same rules that govern the report govern the narration. Every one of these has a
reason recorded in `CLAUDE.md`; none is stylistic.

- **Do not quote LaBRADOR figures.** Meeting 9 ruled the un-refined direction stays
  discussion *without* numbers so no conflicting result appears. The deck reports none.
- **A patched node is not Bitcoin.** Say the rule was added experimentally; "cannot settle
  on Bitcoin as it stands" remains true. Its security is **not** analysed.
- **The one-transaction result is scoped:** Simplified Dilithium-III, a 32-byte signed
  message, that EVM revision. The remaining margin is a message-length budget, and it is
  **derived**, not measured. D2/D5 were **not evaluated** — never "it fails there".
- **The classical baseline is functionality-matched, not security-matched**, and it is an
  ECDSA *adaptor*, never plain ECDSA.
- **The ML-DSA result is a functional demonstration.** Whether committing to
  `HighBits(w+Y)` preserves unforgeability is not analysed.
- **PreSign / PreVerify / Adapt / Extract are functions, not protocols**, and
  "transaction" never means "the signed message".
- Do not claim the STARK gadget as π — it is not zero-knowledge, and it is excluded from
  both the report and this deck.
