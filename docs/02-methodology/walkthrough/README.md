# Walkthrough: from upstream ML-DSA to our simplified LAS

This folder is the **Meeting-5 deliverable**: a top-down explanation of what we
built, organised so a reader can start at the highest level and zoom in only as
far as they want. Each zoom-in level is a **separate short file**, and files of
the same kind live in the same lettered folder.

Paper authority throughout: eprint 2020/845 (Esgin–Ersoy–Erkin), read via
[`docs/paper/LAS_2020_845_NOTATION.md`](../../paper/LAS_2020_845_NOTATION.md).
Report maths use the paper's `d` (ring degree = 256), `γ = κ·d·(n+ℓ)`, and a
per-set `κ`.

---

## The drawn diagrams (start here for the visual)

**[00 · Diagrams & one-page summary](00-diagrams-and-summary.md)** — the three
rendered Mermaid diagrams (base-signature API, LAS adaptor API with *where `Y` is set
up*, and the base-C / LAS-C / LAS-Rust repository structure), plus the reused/new
front table, the C ⇄ Rust size cross-check, and the rejection theory-vs-measured table.
This is the source for the 1–2 slide summary.

## The one-picture summary

Three signature schemes on one lineage. Each arrow is a small, deliberate step.

```
   ML-DSA / Dilithium (FIPS 204)          the NIST standard: a size-OPTIMISED lattice signature
        │
        │   step 1:  REMOVE the size optimisations
        │            (no key compression, no hint vector)          paper §2.2
        ▼
   Simplified Dilithium signature          "Algorithm 1" in the paper — the ordinary signature
        │                                   our files: ref/basesig.c · rust .../basesig.rs
        │   step 2:  ADD a statement Y into the hash,
        │            tighten one bound, add 4 operations           paper Algorithm 2
        ▼
   LAS adaptor signature                    "Algorithm 2" in the paper — the adaptor signature
                                            our files: ref/las.c · rust .../las.rs
```

All three are built from the **same low-level primitives**, which we reuse
unmodified and treat as black boxes:

```
   ┌─────────────────────────────────────────────────────────────────┐
   │   NTT   ·   SHAKE-128/256 (Keccak)   ·   modular reduction   ·    │   ← reused as-is,
   │   polynomial add/sub   ·   uniform & short sampling              │     never modified
   └─────────────────────────────────────────────────────────────────┘
```

The whole project is: **reuse those primitives, drop the compression layer, and
add a thin adaptor layer on top.** No upstream function is modified in either
language.

---

## Reading order (top → bottom is high-level → low-level)

Start at A. Only open C/D if you want the line-level detail.

| Folder | What it answers | Zoom level |
|---|---|---|
| **[A. Protocol walkthroughs](A-protocol-walkthroughs/)** | How each of the three schemes does KeyGen / Sign / Verify (and PreSign / PreVerify / Adapt / Ext for LAS), step by step | High level |
| **[B. Modifications](B-modifications/)** | Exactly what we changed at each step, with a separate **Why** and **How** for every change | Mid level |
| **[C. Code usage maps](C-code-usage-maps/)** | Line-by-line: which parts of the upstream code we use, which we bypass, and why; plus the reused/new accounting and repo map | Low level |
| **[D. Rejection sampling](D-rejection-sampling/)** | The ≈2.7-attempts / ≈36.8%-acceptance rate, derived from the paper in our own words, and the measured confirmation | Low level (maths) |
| **[E. Benchmark methodology](E-benchmark-methodology/)** | How the C and Rust benchmarks are made to measure the same thing the same way | Mid level |
| **[F. Project tracking](F-project-tracking/)** | Meeting-5 request checklist, this session's fixes, and what is still open | Meta |

### Files, in order

- A. Protocol walkthroughs
  - [01 · Upstream ML-DSA](A-protocol-walkthroughs/01-upstream-ml-dsa.md) — how the vendored C and Rust standards do it
  - [02 · Simplified Dilithium (our base signature)](A-protocol-walkthroughs/02-simplified-dilithium.md) — **how our `basesig.c` / `basesig.rs` do it**
  - [03 · LAS adaptor (our adaptor scheme)](A-protocol-walkthroughs/03-las-adaptor.md) — **how our `las.c` / `las.rs` do it**
- B. Modifications
  - [01 · ML-DSA → Simplified Dilithium](B-modifications/01-ml-dsa-to-simplified.md)
  - [02 · Simplified Dilithium → LAS](B-modifications/02-simplified-to-las.md)
- C. Code usage maps
  - [01 · `ref/sign.c` usage map](C-code-usage-maps/01-ref-sign-c.md)
  - [02 · `ml_dsa.rs` usage map](C-code-usage-maps/02-ml-dsa-rs.md)
  - [03 · Reused / bypassed / new + repo map](C-code-usage-maps/03-reused-bypassed-new.md)
- D. Rejection sampling
  - [01 · Theory and measured](D-rejection-sampling/01-theory-and-measured.md)
- E. Benchmark methodology
  - [01 · C↔Rust parity](E-benchmark-methodology/01-c-rust-parity.md)
- F. Project tracking
  - [01 · Meeting-5 checklist](F-project-tracking/01-meeting5-checklist.md)
  - [02 · This session's fixes](F-project-tracking/02-session-fixes.md)
  - [03 · Still open](F-project-tracking/03-open-items.md)

---

## The single most important idea (say this on the slide)

About **half** of ML-DSA's Sign/Verify logic — Power2Round, Decompose,
MakeHint/UseHint, the ω bound, the extra rejection tests — exists **only to make
the public key and signature smaller**. It is a *compression layer* bolted onto a
simple lattice Σ-protocol.

Our simplified scheme **removes that entire layer**. That is why our public key is
bigger — but it is also exactly what makes the adaptor work: `Adapt` and `Ext`
need the verifier's algebra to be *exact*, and compression destroys that
exactness. See [A/02](A-protocol-walkthroughs/02-simplified-dilithium.md) and
[B/01](B-modifications/01-ml-dsa-to-simplified.md).
