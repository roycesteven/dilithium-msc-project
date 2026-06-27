# LAS 2020/845 Paper Notation Guide

## 1. Paper identity

- **Title:** "Post-Quantum Adaptor Signatures and Payment Channel Networks"
- **Authors:** Muhammed F. Esgin, Oğuzhan Ersoy, Zekeriya Erkin (ESORICS 2020).
- **ePrint:** 2020/845.
- **Scheme:** **LAS** — a lattice-based adaptor signature, secure under
  Module-SIS (M-SIS) and Module-LWE (M-LWE).
- **Note on the underlying signature:** the ordinary signature underneath LAS is
  *a simplified version of Dilithium* — the paper states this explicitly and says
  it "do[es] not employ the optimizations in Dilithium in order to simplify the
  presentation" (§2.2, p.4; §3.2, p.12; Contributions, §1, p.2). It is **not**
  full Dilithium.
- **Source pointers:** local PDF `2020-845.pdf` (20 pages). Core construction is
  §3 (p.7), Table 1 (p.7), Algorithm 1 (p.7–8), Algorithm 2 (p.8); parameters in
  §3.2 + Eq. (7) (p.11–12); applications in §4 (p.12+).

## 2. Source-of-truth rule

- This file is a **curated, hand-checked guide extracted from `2020-845.pdf`**.
  It is the repo's *working* source of truth for LAS paper notation.
- **If this Markdown conflicts with `2020-845.pdf`, the PDF wins.**
- If a notation detail is not explicitly in the PDF, or is unclear from parsing,
  it is marked **TODO/UNCERTAIN** here rather than guessed.
- **Do not invent notation.** Do not rename the paper's variables casually in
  report figures, captions, or code comments that claim to follow the paper.

## 3. Core notation from the paper

Source: §3 (p.7), Table 1, Algorithms 1–2, Definition 3 (§2, p.5).

- **Public parameters:** `pp = (A, H)` (§3, p.7).
  - `A = [ I_n ‖ A' ]  ∈  R_q^{n×(n+ℓ)}` — Hermite-normal-form matrix; the right
    block is sampled `A' ←$ R_q^{n×ℓ}` (§3, p.7). The Ajtai/SIS map is
    `f_A(x) = A·x` over `R_q`, additively homomorphic.
    > **Notation note (confirmed):** use the paper's `A = [ I_n ‖ A' ]` in this
    > guide and in report text. Treat `A0` / `A_0` **only** as an
    > implementation/code alias, and only if the C code actually names the block
    > `A0`. Parsed PDF text can render `A'` as `A0`/`A_0`; the **rendered PDF
    > wins** — quote `A'`.
  - `H : {0,1}* → C` — random-oracle hash, **produces the challenge `c`**, range
    is the challenge set `C` (Table 1).
- **Key pair (KeyGen, = Gen):**
  - `pk = t`  where `t = A·r`.
  - `sk = r`  where `r ← S_1^{n+ℓ}` (ternary, `‖r‖∞ ≤ 1`).
- **Statement / witness (`Gen` runs exactly as `KeyGen`, §3 p.7):**
  - statement `Y = t'`  (a public-key-like value).
  - witness  `y = r'`  with `t' = A·r'`, `‖r'‖∞ ≤ 1` for the base relation `R_A`.
- **Challenge:** `c = H(...)`, with `c ∈ C`.
- **Ordinary signature:** `σ = (c, z)` (Algorithm 1, Sign).
- **Pre-signature:** `σ̂ = (c, ẑ)` (Algorithm 2, PreSign).
- **Adapted signature:** `σ = (c, z)` where `z = ẑ + r'` (Algorithm 2, Adapt) —
  i.e. an adapted pre-signature is structurally an *ordinary* signature `(c, z)`.
- **Ext output:** `s = z − ẑ`, returned iff `t' = A·s` (Algorithm 2, Ext).
Algorithm 2 names the Ext output s = z − z_hat; Definition 3 uses y only as the generic witness name.

### ASCII-safe fallbacks (use these in code/plain-text contexts)

These are ASCII renderings of the paper's symbols for code, logs, and CSV columns
only. They are **fallbacks, not new mathematical notation**: in report text and in
rendered figure labels use the paper symbols (`ẑ`, `σ̂`, `ℓ`, `κ`, `γ`, …).

| Unicode | ASCII fallback |
|---------|----------------|
| `σ`     | `sigma`        |
| `σ̂`     | `sigma_hat`    |
| `ẑ`     | `z_hat`        |
| `ℓ`     | `ell`          |
| `κ`     | `kappa`        |
| `γ`     | `gamma`        |
| `δ`     | `delta`        |
| `‖·‖∞`  | `||.||_inf`    |

## 4. Algorithm 1: ordinary lattice-based signature

Source: **Algorithm 1, "Lattice-Based Signature"** (p.7–8). Compact exact summary
(ASCII; the paper steps are reproduced faithfully, not paraphrased):

```
KeyGen():                         # "same as Gen"
    r    <- S_1^(n+ell)           # ternary secret, ||r||_inf <= 1
    t    =  A r
    return (pk, sk) = (t, r)

Sign((pk, sk), M):
    y    <- S_gamma^(n+ell)       # masking randomness, ||y||_inf <= gamma
    w    =  A y
    c    =  H(pk, w, M)
    z    =  y + c r               # r := sk
    if ||z||_inf > gamma - kappa: Restart       # rejection sampling
    return sigma = (c, z)

Verify(pk, sigma, M):
    parse (c, z) := sigma
    if ||z||_inf > gamma - kappa: return 0
    w'   =  A z - c t             # t := pk
    return 1 iff c = H(pk, w', M)  else 0
```

- Bound used by Sign/Verify: `‖z‖∞ ≤ γ − κ` (Algorithm 1, Steps 11 & 16).
- This is Fiat–Shamir-with-Aborts (the rejection at Step 11), per §2.2/§3.

## 5. Algorithm 2: LAS adaptor signature

Source: **Algorithm 2, "LAS: Lattice-Based Adaptor Signature"** (p.8). The four
algorithms; signatures of inputs/outputs follow Definition 3 (§2, p.5).

```
PreSign((pk, sk), Y, M):
    y    <- S_gamma^(n+ell)       # masking randomness (NOT the witness)
    w    =  A y
    c    =  H(pk, w + t', M)      # t' := Y  -- statement folded into the hash
    z_hat = y + c r               # r := sk
    if ||z_hat||_inf > gamma - kappa - 1: Restart   # TIGHTER bound, -1
    return sigma_hat = (c, z_hat)

PreVerify(Y, pk, sigma_hat, M):
    parse (c, z_hat) := sigma_hat ;  t' := Y
    if ||z_hat||_inf > gamma - kappa - 1: return 0
    w'   =  A z_hat - c t         # t := pk
    return 1 iff c = H(pk, w' + t', M)  else 0

Adapt((Y, y), pk, sigma_hat, M):
    if PreVerify(Y, pk, sigma_hat, M) = 0: return ⊥
    parse (c, z_hat) := sigma_hat ;  r' := y     # r' is the witness
    return sigma = (c, z_hat + r')               # => z = z_hat + r'

Ext(Y, sigma, sigma_hat):
    parse (c, z) := sigma ;  (c_hat, z_hat) := sigma_hat ;  t' := Y
    s    =  z - z_hat
    if t' != A s: return ⊥
    return s
```

- **Key difference vs Algorithm 1:** the statement `t' = Y` is added inside the
  hash — Sign uses `H(pk, w, M)`, PreSign/PreVerify use `H(pk, w + t', M)`
  (Algorithm 2, Steps 4 & 15).
- **Why the `−1` tighter bound** (`γ − κ − 1` in PreSign/PreVerify vs `γ − κ` in
  Sign/Verify): the honest witness has `‖r'‖∞ ≤ 1`, and Adapt forms `z = ẑ + r'`;
  the tighter PreSign bound guarantees the adapted `z` still satisfies the
  ordinary `‖z‖∞ ≤ γ − κ` and so passes `Verify` (paper text p.8, §3).
- **`Ext` returns `s`**, the extracted witness, checked by `t' = A·s`.

## 6. Parameter notes

Source: **Table 1** (p.7) and **§3.2 "Parameter Setting and Performance
Analysis"** incl. **Eq. (7)** (p.11–12).

- `d = 256` — power-of-2 ring dimension (Table 1).
- `R_q = Z_q[X]/(X^d + 1)`, with `log q ≈ 24`, i.e. `q ≈ 2^24` (Table 1; §3.2).
- `n = 4` (M-SIS rank), `ℓ = 4` (M-LWE rank) — paper's default `n = ℓ = 4`
  (Table 1; §3.2).
- `κ = 60` (challenge weight): `C = { c ∈ R : ‖c‖₁ = κ ∧ ‖c‖∞ = 1 }`; `κ = 60`,
  `d = 256` give `|C| > 2^256` (Table 1; §3.2).
- `γ = κ·d·(n + ℓ)` — "maximum absolute coefficient of a masking randomness"
  (Table 1). Chosen so the **average number of restarts in Sign and PreSign is
  about `e < 3`** (§3.2) — i.e. per-attempt acceptance `≈ 1/e ≈ 37%`.
- **Base vs extended relation (the "knowledge gap"):**
  - base `R_A`: `(t, r)` with `t = A·r` and `‖r‖∞ ≤ 1`.
  - extended `R'_A`: `(t, r)` with `t = A·r` and `‖r‖∞ ≤ 2(γ − κ)`; `R_A ⊆ R'_A`.
  - An *honest* witness has `‖r'‖∞ ≤ 1`; an *extracted* witness `s` is only
    guaranteed `‖s‖∞ ≤ 2(γ − κ)` (Table 1; p.9).
- **M-SIS / M-LWE setting:** security requires `M-SIS_{n, n+ℓ+1, q, β}` and
  `M-LWE_{ℓ, n, q}`. **Two forms of `β` appear in the paper and must NOT be
  silently merged:**
  - **§3 hard-relation explanation (p.7):** `β = 2γ·d(n + ℓ)` (no square root).
  - **Lemma 2 (witness extractability), Lemma 3 (unforgeability), and §3.2
    parameter/security discussion:** `β = 2γ·√(d(n + ℓ))` (with square root).
  - **For report parameter/security discussion, use `β = 2γ·√(d(n + ℓ))`** (the
    §3.2 / Lemma form).
  - Both forms are recorded verbatim from the parsed PDF text. The difference is
    **not** asserted here to be an OCR error — check the rendered PDF before
    reconciling them. The root Hermite factor `δ < 1.0045` (≈128-bit PQ
    security target) is stated in §3.2 alongside the square-root form.
- **Analytical signature size:** `|σ| = d(n + ℓ)·log(2γ)/8 + 32 bytes ≈ 3210
  bytes` (Eq. (7), §3.2). Paper compares this to Dilithium's 2701 bytes and
  attributes the gap to *not* applying Dilithium's optimisations.
- **Modulus freedom (important for our repo):** the paper states *"Only the size
  of the modulus `q` is important, and therefore the concrete value can be chosen
  to allow fast computation such as Number Theoretic Transformation (NTT)"*
  (§3.2). This is the paper-sanctioned justification for the implementation using
  Dilithium's `Q = 8380417 (≈2^23)` rather than an exact `2^24`.
  - **Label rule:** because this build uses `Q = 8380417` and a concrete
    fixed-width / bit-packed encoding (not the paper's analytical
    `log(2γ)/8 + 32` formula), describe such settings as the
    **"paper-derived / default LAS setting"**, *not* "the paper's exact
    parameters / encoding". The repo's measured packed size and the paper's
    ~3210 B analytical estimate are **not** directly comparable.

## 7. Paper notation → implementation/report-label mapping

| Paper notation | Meaning | Implementation / report label |
|----------------|---------|-------------------------------|
| `d` | ring dimension (256) | `N` (if code uses `N`) |
| `q` | modulus (`≈2^24` in paper; `8380417` in this build) | `Q` (if code uses `Q`) |
| `ℓ` / `ell` | M-LWE rank (4) | `ell` |
| `n` | M-SIS rank (4) | `n` |
| `M = n + ℓ` | response-vector module dimension | `M` |
| `κ` / `kappa` | challenge weight (60) | `kappa` |
| `γ` / `gamma` | masking / rejection bound `κd(n+ℓ)` | `gamma` |
| `C` | challenge set / range of `H` | challenge set |
| `pk = t` | public key | `pk` |
| `sk = r` | secret key | `sk` |
| `Y = t'` | statement / lock | `Y` |
| `y` (sampled in Sign / PreSign) | masking randomness | `y` |
| `y` in `(Y, y)`; `r' := y` in Adapt | witness | `r'` |
| `c` | challenge | `c` |
| `ẑ` / `z_hat` | pre-signature response | `z_hat` |
| `z` | final / adapted response | `z` |
| `σ̂ = (c, ẑ)` | pre-signature | `pre-signature` |
| `σ = (c, z)` | ordinary / adapted signature | `signature` / `adapted signature` |
| `s` | extracted witness (`Ext`) | extracted witness |

> **Notation subtlety (preserve the paper's `y`; do NOT introduce aliases):**
> The paper overloads `y`. In `Sign`/`PreSign`, `y` is the sampled masking
> randomness. In `Adapt((Y, y), …)`, `y` is the witness, and Algorithm 2 writes
> `r' := y`. This guide preserves the paper notation and does not introduce
> aliases. When prose needs to disambiguate, do it in words — "the `y` sampled in
> PreSign" vs "the `y` in the statement–witness pair `(Y, y)`" (the paper's
> `r'`) — never with invented symbols such as `y_mask` or `y_witness`.

## 8. Report and figure label rules

- Use **"ordinary lattice-based signature"** (or "ordinary signature") for
  **Algorithm 1**.
- Use **"LAS adaptor signature"** (or "LAS adaptor operations") for
  **Algorithm 2**.
- Avoid **"BASE"** in final report figures *unless* the caption explicitly
  defines it as the ordinary lattice-based signature from Algorithm 1.
- Operation labels (canonical set): **KeyGen, Sign, Verify, PreSign, PreVerify,
  Adapt, Ext**.
- Do **not** use informal labels in report-clean figures — e.g. "slow path",
  "fast path", "no base op", "all single-digit %".
- Do **not** call `L2` / `L3` / `L5` formal **NIST-equivalent security levels**.
  Call them **"L2-like / L3-like / L5-like engineering scaling settings"**
  derived from simplified-Dilithium dimensions, unless a formal security
  proof/mapping is added. (The paper itself sets one parameter set targeting
  ~128-bit PQ security via `δ < 1.0045`, §3.2; it does not define L2/L3/L5.)
- Do **not** say "exact paper parameters / encoding" when the build uses
  `Q = 8380417` or a different concrete encoding; say **"paper-derived / default
  LAS setting"** (see §6).
- Do **not** say Adapt **appends the witness as a new serialized field**. Say
  **Adapt computes `z = ẑ + r'`** (ASCII `z = z_hat + r'`), so the encoded
  structure stays `σ = (c, z)` — same shape as an ordinary signature.

## 9. Plot typography rules for thesis figures

- Report-clean figures should **not** bake long titles into the image; figure
  explanation belongs in the **LaTeX caption**.
- Machine / environment details belong in the **methodology / evaluation setup**
  section, not repeated inside every report figure.
- Standalone / evidence figures **may** carry a machine/footer line if useful.
- **PDF** figures are preferred for LaTeX inclusion; **PNG** is for
  preview/README only.

## 10. Stage separation

- **Stage 1 (main report figures):** parameter table, per-operation timing,
  adaptor overhead, communication component sizes. These describe the standalone
  signature/adaptor (Algorithms 1–2, §3) and its benchmarks.
- **Stage 2 / application figures:** atomic swap, AMHL, PCN, multi-hop payloads
  — application terminology only, from **§4** ("Applications": atomic swaps §4.1,
  payment channel networks §4.2).
- Do **not** mix Stage-1 communication component-size figures with Stage-2
  off-chain / settlement payload figures.

---

### Source sections used from `2020-845.pdf`

- §1 Introduction / Contributions (p.1–2) — "first post-quantum adaptor
  signature; underlying signature is a simplified Dilithium".
- §2.2 (p.4) — simplified-Dilithium framing, rejection sampling.
- Definition 3, Adaptor Signature Scheme (§2, p.5) — `PreSign/PreVerify/Adapt/Ext`
  input–output signatures.
- §3 (p.7) — `pp = (A, H)`, `A = [I_n ‖ A']`, `f_A`.
- **Table 1, Identifiers for LAS** (p.7) — `d, R_q, S_c, n, ℓ, C, κ, γ, R_A,
  R'_A`.
- **Algorithm 1** (p.7–8) — KeyGen / Sign / Verify.
- **Algorithm 2** (p.8) — PreSign / PreVerify / Adapt / Ext; plus the `γ−κ−1`
  justification text (p.8) and the knowledge-gap text (p.9).
- **§3.2 + Eq. (7)** (p.11–12) — parameter setting, `β = 2γ√(d(n+ℓ))`, `δ`,
  `|σ| ≈ 3210 bytes`, modulus-freedom statement.
- §4 / §4.1 / §4.2 (p.12+) — application terminology only (atomic swaps, PCN,
  UTXO model). **Not** used for Stage-1 benchmark labels.

### Open TODO / UNCERTAIN

- **OPEN (do not auto-resolve):** the M-SIS norm bound `β` appears as
  `2γ·d(n+ℓ)` in the §3 hard-relation explanation but as `2γ·√(d(n+ℓ))` in
  Lemma 2, Lemma 3, and §3.2 (see §6 above). Recorded as-is; do **not** declare
  either an OCR error or merge them without checking the rendered PDF. Report
  parameter/security text uses the square-root form.
- **UNCERTAIN (cosmetic):** Table 1's `R'_A` row in the extracted text repeats
  the glyph "`∈ R_A`" where it should read `∈ R'_A`; the *bound* `‖r‖∞ ≤ 2(γ−κ)`
  for the extended relation is unambiguous and is corroborated by the p.9 text.
  Treated as an extraction artefact, not a real paper change. Verify against the
  rendered PDF before quoting the row header verbatim.
- No other notation ambiguities found in the targeted sections. If a future edit
  needs a symbol not listed here (e.g. exact `S_c` usage in proofs, or §3.1
  security-proof variables), check `2020-845.pdf` directly and extend this guide
  rather than guessing.
