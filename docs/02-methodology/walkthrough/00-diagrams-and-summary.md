# Stage-1 diagrams & one-page summary (Meeting-5 M5.2 / M5.3 / M5.4 / M5.7)

*The **visual** top of the walkthrough. The A–F folders explain the same story in
prose; this file draws it. Every diagram below is Mermaid (renders on GitHub) and is
the source for the 1–2 slide summary. Notation follows the paper
([`docs/paper/LAS_2020_845_NOTATION.md`](../../paper/LAS_2020_845_NOTATION.md)):
ring degree `d = 256`, `γ = κ·d·(n+ℓ)`, per-set challenge weight `κ`. Headline setting
throughout is **Simplified Dilithium-III**, engineering set `(n, ℓ, κ) = (6, 5, 49)`.*

Read order: **(1) base signature → (2) LAS adaptor → (3) repository structure**, then
the two summary tables. This mirrors Wang's instruction: *start from the basic
`KeyGen/Sign/Verify` API, then show how LAS adds/modifies it.*

---

## Diagram 1 — Base signature API (`KeyGen` / `Sign` / `Verify`)

The ordinary simplified-Dilithium signature, our fair-comparison baseline
(`ref/basesig.c` · `rust/fips204-las/src/basesig.rs`). A Fiat–Shamir-with-aborts
Σ-protocol; **no statement `Y` anywhere**.

```mermaid
flowchart TB
  S0(["public seed · 32 B"]) --> SP["<b>Setup</b> · setup_public_params<br/>pp = A = [ I&nbsp;|&nbsp;A' ]"]:::prim
  SP --> KG["<b>KeyGen</b> · base_keygen<br/>r ← S₁ &nbsp;(ternary secret)<br/>t = A · r"]:::base
  KG --> PK["pk = t"]:::obj
  KG --> SK["sk = r"]:::obj

  subgraph SIGN["<b>Sign</b> · base_sign — Fiat–Shamir with aborts"]
    direction TB
    Y1["y ← S_γ &nbsp;(masking vector)"] --> W1["w = A · y"]
    W1 --> C1["c = H( pk , <b>w</b> , M )<br/>— hash of the commitment, <b>no Y</b>"]
    C1 --> Z1["z = y + c · r"]
    Z1 --> R1{"‖z‖∞ ≤ γ−κ ?<br/>(BOUND_SIGN = γ−κ+1)"}
    R1 -- "no · restart (≈2.7× avg)" --> Y1
    R1 -- yes --> SIG(["σ = ( c̃ , z )"])
  end

  PK -. into hash .-> C1
  SK --> Z1
  M0(["message M"]) --> C1
  SIG --> VER["<b>Verify</b> · base_verify<br/>w' = A·z − c·t<br/>accept iff &nbsp;c̃ = H( pk , w' , M )"]:::base
  PK -. into hash .-> VER

  classDef prim  fill:#e8f5e9,stroke:#2e7d32,color:#1b5e20;
  classDef base  fill:#e3f2fd,stroke:#1565c0,color:#0d47a1;
  classDef obj   fill:#fff,stroke:#607d8b,color:#37474f,stroke-dasharray:3 3;
```

---

## Diagram 2 — LAS adaptor API (base **+** `PreSign` / `PreVerify` / `Adapt` / `Ext`)

LAS reuses the *same* `KeyGen`/`Verify` and adds four operations
(`ref/las.c` · `rust/fips204-las/src/las.rs`). **Wang's question — "where is `Y` set
up?" — answered: in a separate relation step (`relation_gen`), *not* in KeyGen and
*not* inside Sign.** `Y` is structurally a second key pair but semantically the
adaptor *lock*.

```mermaid
flowchart TB
  KGref["<b>KeyGen</b> (unchanged) · base_keygen<br/>pk = t , sk = r"]:::base

  REL["<b>Adaptor setup</b> · relation_gen &nbsp;(the NEW step)<br/>y ← S₁^{n+ℓ} &nbsp;(witness — secret)<br/><b>Y = A · y</b> &nbsp;(statement — the lock)"]:::adaptor

  subgraph PS["<b>PreSign</b> · las_presign — signer, knows sk, sees Y (not y)"]
    direction TB
    Y2["y' ← S_γ"] --> W2["w = A · y'"]
    W2 --> C2["c = H( pk , <b>w + Y</b> , M )<br/>— statement folded into the hash"]
    C2 --> Z2["ẑ = y' + c · r"]
    Z2 --> R2{"‖ẑ‖∞ ≤ γ−κ−1 ?<br/>(BOUND_PRESIGN = γ−κ, tighter)"}
    R2 -- "no · restart" --> Y2
    R2 -- yes --> PSIG(["σ̂ = ( c̃ , ẑ )"])
  end

  REL -. Y into hash .-> C2
  KGref --> PS

  PSIG --> PV["<b>PreVerify</b> · las_preverify<br/>w' = A·ẑ − c·t<br/>accept iff c̃ = H( pk , <b>w' + Y</b> , M )"]:::adaptor

  PSIG --> AD["<b>Adapt</b> · las_adapt &nbsp;(needs witness y)<br/><b>z = ẑ + y</b><br/>⇒ σ = ( c̃ , z )"]:::adaptor
  REL -. witness y .-> AD
  AD --> VERok["ordinary <b>Verify</b> now passes:<br/>A·z − c·t = (A·ẑ − c·t) + A·y = w' + Y ✓"]:::base

  VERok --> EXT["<b>Ext</b> · las_ext &nbsp;(anyone with σ and σ̂)<br/><b>y = z − ẑ</b> ; return iff A·y = Y<br/>— recovers the witness"]:::adaptor
  PSIG -. σ̂ .-> EXT
  EXT --> SWAP(["publishing the adapted σ leaks y<br/>→ this is what makes an atomic swap atomic"]):::note

  classDef base     fill:#e3f2fd,stroke:#1565c0,color:#0d47a1;
  classDef adaptor  fill:#fff3e0,stroke:#e65100,color:#bf360c;
  classDef note     fill:#f3e5f5,stroke:#6a1b9a,color:#4a148c;
```

**The one identity to remember:** `Sign` hashes `w`; `PreSign` hashes `w + Y`. `Adapt`
adds the ternary witness `y` (‖y‖∞ ≤ 1) to `ẑ`; because `PreSign` rejected at the
*tighter* bound `γ−κ−1`, the adapted `z = ẑ + y` still satisfies `‖z‖∞ ≤ γ−κ` and clears
ordinary `Verify`. `Ext` inverts the fold to recover `y`.

---

## Diagram 3 — Repository structure (base primitives · C LAS · Rust LAS)

Both implementations are **additive modules layered on unmodified primitives**. The
green box is reused as-is in each language; the blue/orange modules are the new LAS
code, in one-to-one correspondence C ⇄ Rust. The two are locked together by a shared
known-answer test (byte-identical outputs).

```mermaid
flowchart TB
  subgraph PRIM["Reused primitives — UNMODIFIED, treated as black boxes"]
    direction LR
    P1["NTT / inverse NTT"]
    P2["SHAKE-128/256<br/>(Keccak · FIPS 202)"]
    P3["modular reduction<br/>(Montgomery / Barrett)"]
    P4["polynomial<br/>add / sub / pointwise"]
    P5["uniform + short sampling<br/>· randombytes"]
  end

  subgraph CLAS["C LAS — <code>ref/</code> (new)"]
    direction TB
    C_setup["setup.{c,h} — pp = A = [I|A']"]
    C_rel["relation.{c,h} — statement Y / witness y"]
    C_types["las_types.h — the six object types"]
    C_base["basesig.{c,h} — Algorithm 1 (base sig)"]
    C_las["las.{c,h} — Algorithm 2 (adaptor)"]
    C_ser["serialize.{c,h} — wire codec + verify-from-bytes"]
    C_app["amhl.{c,h} · chain.{c,h} — Stage-2 demo (C only)"]:::stage2
  end

  subgraph RLAS["Rust LAS — <code>rust/fips204-las/src/</code> (new)"]
    direction TB
    R_setup["setup.rs"]
    R_rel["relation.rs"]
    R_types["las_types.rs"]
    R_base["basesig.rs — Algorithm 1"]
    R_las["las.rs — Algorithm 2"]
    R_ser["serialize.rs"]
  end

  PRIM ==> CLAS
  PRIM ==> RLAS
  CLAS <-. "byte-identical objects · KAT digest bb6ad0da…260c" .-> RLAS

  classDef stage2 fill:#eceff1,stroke:#90a4ae,color:#455a64,stroke-dasharray:4 3;
```

C reuses the vendored **pq-crystals/dilithium** reference (commit `2374d22`):
`poly.c`, `ntt.c`, `reduce.c`, `fips202.c`, `symmetric-shake.c`, `randombytes.c`,
`params.h` (`N=256`, `Q=8380417`). Rust reuses the vendored **`fips204`** crate
(commit `c948882`, v0.4.6): `ntt.rs`, `hashing.rs`, `encodings.rs`, `conversion.rs`,
`helpers.rs`, `high_low.rs`, `ml_dsa.rs`. **Zero upstream functions modified in either
language** (per-function audit: [`FUNCTION_MAP.md`](../FUNCTION_MAP.md); diff view:
[`CODE_DIFF_VIEW.md`](../CODE_DIFF_VIEW.md)).

---

## Table A — Reused / modified / newly-added, at a glance (M5.4 front)

| Layer | Reused unmodified | Newly added for LAS | Why it is (not) modified |
|---|---|---|---|
| Lattice arithmetic (NTT, reduction, poly ops) | ✅ all of it | — | Correct and constant across schemes; LAS only *calls* it — modifying it would break the KAT lock and the clean diff |
| Hashing / sampling (SHAKE, `randombytes`, uniform/short) | ✅ all of it | — | The random oracle `H` and samplers are scheme-agnostic; reused verbatim |
| Size-optimisation layer (Power2Round, Decompose, hints, ω-bound) | present but **bypassed** | — | The simplified scheme **omits** it so the identity `A·z − c·t = w + Y` is exact — the property `Adapt`/`Ext` depend on |
| Base signature (`KeyGen`/`Sign`/`Verify`) | — | `setup`, `basesig` | New relation `[I\|A']`, ternary keys, *full*-`w` hash — a self-contained module, not an edit to `sign.c` |
| Adaptor (`PreSign`/`PreVerify`/`Adapt`/`Ext`) | — | `relation`, `las` | The four new operations + statement fold `w+Y`; this is the project's contribution |
| Wire encoding | — | `serialize` | Object shapes differ from Dilithium's; on-chain byte interface + validating decoder |

Verdict: **the entire arithmetic/hash layer is reused; every LAS-specific line is new;
nothing upstream is edited.** That is the "clean diff = visible contribution" design.

---

## Table B — C ⇄ Rust size cross-check (M5.7)

Communication sizes are **fixed** (no dispersion) and **identical across the two
implementations** — the evidence that the two ports are the same scheme, not
look-alikes. Byte-for-byte identity is proven by the shared KAT digest
`bb6ad0dab998c1f90ca4d3cc0f5d3dfa723e89f79aff18fce2698a08c96e260c`
(`make test/test_kat3` ⇔ `cargo test --test las_kat`).

| Object (packed bytes) | Simplified Dilithium-III `(6,5,49)` | Paper set `(4,4,60)` | C = Rust? |
|---|--:|--:|:--:|
| public key `pk = t` | 4416 | 2944 | ✅ |
| secret key `sk = r` | 704 | 512 | ✅ |
| statement `Y = t'` | 4416 (= pk) | 2944 | ✅ |
| witness `y` | 704 (= sk) | 512 | ✅ |
| signature `σ = (c̃, z)` | 6720 | 4640 | ✅ |
| pre-signature `σ̂ = (c̃, ẑ)` | 6720 (= σ) | 4640 | ✅ |
| adapted signature | 6720 (= σ) | 4640 | ✅ |

**What drives the size:** the response `z` is **≈99.5 %** of the signature (D3: 6688 of
6720 B; the challenge `c̃` is a fixed 32 B). Signature = pre-signature = adapted
signature, because `Adapt` only adds the ternary witness (‖y‖∞ ≤ 1), which does not
widen the packed `z`. The Rust `examples/size_report.rs` hard-asserts these equal the C
evidence row, so the size claim is cross-language by construction.

---

## Table C — Rejection sampling: theory vs measured (M5.8)

Lattice Fiat–Shamir signatures restart until the response is short enough; this drives
the timing and its variance. The per-attempt acceptance probability is
`((2·bound−1)/(2γ+1))^{(n+ℓ)·d} ≈ e^{−1} ≈ 36.8 %`, so the expected number of attempts
per output is `≈ 1/0.368 ≈ 2.7`. Intuitively: each of the `(n+ℓ)·d` response
coefficients must land inside the acceptance window, and the window is sized (via `γ`)
so the whole vector clears it about once every `e` tries — the paper's deliberate
design target.

| Quantity @ Simplified Dilithium-III | Theory (exact) | Measured (direct counter) |
|---|--:|--:|
| `Sign` attempts / signature | 2.71875 | ≈2.72 |
| `PreSign` attempts / pre-signature | 2.77483 | ≈2.81 |
| acceptance / attempt | ≈36.8 % | ≈37 % |

`PreSign`'s tighter bound (`γ−κ−1` vs `γ−κ`) makes it restart *slightly* more often —
the gap between the two theory values re-confirms both bounds. Every benchmark run
hard-asserts the measured attempts against this theory within 5σ, so a run with the
wrong restart rate aborts instead of producing invalid numbers.

---

## Headline findings (2–3 sentences, for the slide) + environment

Compared with the simplified Dilithium-style **base** signature at matched parameters,
the LAS **adaptor** layer adds only a few percent of computation per operation at the
**core / struct-API tier** (`PreSign` +1.6 %, `PreVerify` +5.1 %, `Adapt` +9.0 % vs the
basic op each mirrors; C reference — pure adaptor math). The **full-protocol end-to-end
tier**, which also packs/unpacks every object, is larger (`+20.8 % / +35.3 % / +79.5 %`,
C), because each operation must decode the pk-sized statement `Y`; that serialization
cost, not the adaptor math, drives it. The adaptor layer **does not enlarge the
signature** — pre-signature = adapted signature =
base signature (`Adapt` only adds the ternary witness `y`, ‖y‖∞ ≤ 1). Its **one extra
communication object is the pk-sized statement `Y`** (4416 B, ≈ 65.7 % of a signature) —
not zero, but a single object that does not scale with the message. The dominant
post-quantum cost is **communication** (`pk`/`sig`/`Y` in the low kilobytes, `z` ≈ 99.5 %
of the signature), **not computation** (sub-millisecond ops). C and Rust produce
byte-identical objects (same KAT digest), confirming the implementation is consistent.

> **Timing provenance:** the percentages above are from evidence run
> `20260717_084012` (Simplified Dilithium-III, C reference, core tier); absolute
> microseconds are machine-dependent — **re-run the full Stage-1 suite once on the
> submission machine before quoting final numbers.**
> **Environment of record:** AMD Ryzen 7 7745HX, Ubuntu 24.04 on WSL2, `gcc 13.3.0`
> `-O3` (zero warnings) for C; `cargo` release + Criterion (300 samples/op) for Rust.
> Both languages, every compared scheme, run on the **same machine** (the only fairness
> requirement). Sizes are exact and machine-independent.
