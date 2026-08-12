# 6–8 minute project video — recording plan

The deck is `video_deck.html` (open it in a browser; `F` for fullscreen). It is
**generated**, not hand-written: `scripts/gen_slides.py` fills every number in
`video_deck.template.html` from `report/latex/generated/*.tex`, the same macros the
report reads. Edit the **template**, never the output, then:

```
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
| 5 | **DEMO A — the swap runs** | 0:45 | *complement*: the protocol executing |
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

### DEMO A — the swap protocol (slide 5, ~45 s)

```
cd ref && make test/test_swap3 && ./test/test_swap3
```

Keep on screen, in this order:

1. the abort gate — `u₂` pre-signs only after both π and `σ̂₁` verify;
2. the tripwire — a pre-signature **fails** ordinary `Verify`;
3. `Extract` returning the witness **exactly**;
4. both legs settling.

Speed-ramp any long pause rather than cutting, so the run reads as one continuous
execution.

### DEMO B — a real Bitcoin client (slide 9, ~48 s)

```
scripts/run_btc_las_node.sh          # carriage on stock Core, then the patched node
```

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
| Use of the medium | 40 % | the two captures; the animated mechanism on slide 3; charts generated from the evidence rather than screenshotted; talking-head overlay |
| Complementing the report | 40 % | slides 5 and 9 show artefacts *running* — the swap end to end, and a patched consensus rule accepting a lattice-authorised spend — which text and figures cannot convey |
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
