# Supervisor deliverables — coverage & gap analysis (Meetings 1–6)

*Written 2026-07-20. Cross-references the cleaned transcripts (`meeting{1..6}_cleaned_transcript.md`,
including the 2026-07-20 addenda that restore content ChatGPT dropped) against
`docs/STATUS.md` and the current report/repo state. Deadline for the Meeting-6
deliverables: **Friday 24 July 2026.***

Legend: ✅ done & in the report/repo · 🟡 partial / needs a pass · ⬜ not done.

---

## 1. Meeting-6 deliverables (the live list — due 24 July 2026)

Source: `meeting6_cleaned_transcript.md §D`. These are the current sprint.

| # | Deliverable (Meeting 6) | Status | Where / what remains |
|---|---|:--:|---|
| M6.1 | **Two-tier timing** (core vs with-pack/unpack), tiers named and never conflated | ✅ | Report §2.6 defines *core tier* + *packed tier* (renamed from "full-protocol" 2026-07-20 to avoid implying an end-to-end protocol cost); `tab:overhead-l3` shows both. |
| M6.2 | **Discussion of pack/unpack cost** — additional, amortisable, one-time, out-of-scope to optimise | ✅ | Report §3.2 (packed premium is serialization, amortisable by caching decoded objects) + §2.3 challenge-encoding trade-off. |
| M6.3 | **Update all tables AND figures with pack/unpack numbers** | ✅ | `tab_overhead_target.tex` packed block present; figures regenerated 2026-07-20. |
| M6.4 | **Measure & report PreSign attempts next to Sign** at same parameters | ✅ | New `tab:rejstats` (model vs 4 measured samples) + `fig:rejdist` (attempt distribution). Sign 2.719 / PreSign 2.775 theory; measured within gate. |
| M6.5 | **C vs Rust comparison** (fair — identical parameters) | ✅ | Report §3.3 `tab:rust`; ≤1.07× spread stated; "C reference" terminology removed (both are modified simplified ML-DSA). |
| M6.6 | **Classical ECDSA: 3–4-column layout** with the packed-API caveat | ✅ | `tab:classical` = ECDSA (hybrid native API) + LAS core + LAS packed; per-operation hybrid boundary now spelled out (2026-07-20). |
| M6.7 | **Seed-vs-expanded challenge** (32-byte c̃, FIPS-204) trade-off in report | ✅ | Report §2.3 "Challenge encoding: seed, not polynomial", cites FIPS 204 `\cite{1274601}`. |
| M6.8 | **High-quality figures (PNG/PDF), readable label/legend fonts** | ✅ | `scripts/plot_las_paper_figures.py` given a shared `_style()` (≥10.5 pt fonts, recessive axes); 4 body PDFs regenerated. |
| M6.9 | **Keep the GitHub PR updated** | 🟡 | Artefacts ready (`dilithium-baseline` branch, `CODE_DIFF_VIEW.md`). **Action for Royce:** push current branch, confirm the PR shows the diff. Claude cannot push. |
| M6.10 | **Start the atomic-swap study (HTLC → adaptor history)** in the report background | ✅ | Report §1.1 new paragraph: custodial → HTLC → limitations → adaptor signatures, cited. |
| M6.11 | **Video (6–8 min)** — highlight essentials, detail on slides | ⬜ | Storyboard in `STATUS.md §5`; not started. Final-stage item, not blocking. |
| M6.12 | *(offer)* relay LAS-paper questions to the author | n/a | Wang's standing offer; no action. |

**Meeting-6 verdict:** the *reporting* deliverables (M6.1–M6.8, M6.10) are done in the
LaTeX. Two human-only items remain: **push/confirm the PR (M6.9)** and the **video (M6.11)**.

---

## 2. Carried-forward deliverables from Meetings 2–5 still open

| From | Deliverable | Status | Note |
|---|---|:--:|---|
| M4 / M5 | **Open the clean-Dilithium → LAS PR, invite Wang** | 🟡 | Named in Meetings 4 AND 5; still the #1 open action in `STATUS.md §6`. Human-only. |
| M4 (addendum §F.4) | **Push the benchmark *figures* to the repo Wang reviews** | ✅ | Figures live in `report/latex/figures/` and `evidence/*/tables/`, versioned. |
| M5.5 | **Top-down "what changed (simplified Dilithium → LAS)" written summary** | 🟡 | Pieces in `UPSTREAM_TO_LAS_WALKTHROUGH.md` + bridge; not yet one consolidated top-down narrative. |
| M5.9 | **Classical overhead as columns pulled into Stage-1 tables** | ✅ | `tab:classical` present with the security-level caveat. |
| M5.10 | **Trim to the important figures; rest → appendix** | ✅ | Body: timing, components, rejection-distribution, criterion, repo/flow diagrams. Overhead-sweep + per-set tables in appendix. |
| M3 | **≥5-run mean±SD, same machine** | ✅ | 5×(500/1000); mean±SD everywhere. |
| M3 | **Fair same-security comparison, parameters stated** | ✅ | `tab:params`/`tab:notation`; LAS-vs-own-base is same-parameter by construction; classical = functionality-matched with explicit caveat. |
| M2 (addendum §A.1/§A.2) | **Video question + high-level design diagram** | 🟡/✅ | Diagram: `fig:flow` + `fig:repostructure` done. Video: still ⬜ (= M6.11). |

---

## 3. What is fully closed (no action)

- Stage-1 LAS scheme + 8-point correctness contract, 1000 iters modes 2/3/5 (D1, D2, D23).
- Both benchmark baselines: LAS-vs-own-base (primary) and classical ECDSA adaptor (D11, D20).
- Component-level size breakdown; response `z` = 99.3–99.7 % of the signature (D6, §3.4).
- Rejection theory + direct measurement + 5-σ validity gate (D8; §2.6, §3.2, `tab:rejstats`).
- Rust cross-implementation + shared KAT digest `bb6ad0da…260c` (D7; §3.3).
- Atomic swap verbatim to the LAS-paper Fig. 1 incl. π (D3, D25; §3.5).
- Serialization + validating decoder + tamper test (all byte-flips rejected) (D6; §3.1).
- Function map, 0 upstream functions modified; two-branch diff (D12, D21).
- Reproducibility README + recorded toolchain (D13).

---

## 4. Correctness / perfection audits — run 2026-07-20 (read-only)

Requested: *"check the correctness and perfection of the simplified base and LAS
implementation and benchmark comparison and its analysis."* Three read-only audits run;
all three **confirm** the report's claims.

1. **Classical-adaptor benchmark** (`ref/test/bench_classical.c`) — ✅ **confirmed.**
   The report's per-operation *hybrid native-API boundary* is exactly what the code
   times. `adaptor[i]` is a raw `uint8_t[162]`, so `ecdsa_adaptor_encrypt` (PreSign)
   writes it and `..._verify`/`..._decrypt`/`..._recover` (PreVerify/Adapt/Ext) parse
   it — all *inside* the timed `MEASURE(...)` loop; whereas `ec_pubkey_create`,
   `ecdsa_sign`, `ecdsa_verify` (KeyGen/Sign/Verify) exchange in-memory structs
   (`secp256k1_pubkey`, `secp256k1_ecdsa_signature`), with DER serialization done
   *outside* the loop (lines 150–151, size reporting only). RUNS×NITER = 10×1000
   matches `\clReps`/`\clIters` in the report caption.

2. **Atomic-swap implementation** (`ref/test/test_swap.c`) — ✅ **confirmed** against
   report §3.5, line-for-line: Alice (witness holder) commits first with
   `Gen → π → PreSign`, sends `{Y, π, σ̂₁, tx₁}` (lines 89–96); Bob's abort gate is
   `π-verify AND PreVerify` (98–104); π is rejected against a different statement
   `Y_evil` (106–114); both pre-signatures fail the byte-level tripwire
   (`pack_pre_signature → unpack_signature → base_verify` must fail, 123–136);
   post-Ext asserts `‖y′‖∞ ≤ 1` **and** `y′ == y` (149–156).

3. **Simplified base + LAS correctness** (`ref/las.c`, C ⇄ Rust ⇄ `2020-845.md`) — ✅
   **confirmed.** PreSign folds the shifted commitment `w + Y` into the SHAKE256 oracle
   (las.c:229, 237–248); the signature stores only the 32-byte `c_tilde`
   (`LAS_CTILDEBYTES = 32`) and re-derives `c = SampleInBall(c_tilde)` locally
   (las.c:250, 379) — the FIPS-204 seed-not-polynomial encoding of M6.7; PreVerify
   recomputes `w′ = A ẑ − c t`, re-adds `Y`, and re-hashes (las.c:349–386). Bound
   convention is internally consistent: `chknorm` rejects on `|coeff| ≥ bound`, with
   `BOUND_SIGN = γ−κ+1` (acceptance `γ−κ`) and `BOUND_PRESIGN = γ−κ` (acceptance
   `γ−κ−1`, the tighter bound the report describes). C↔Rust parity is enforced by the
   pinned KAT digest `bb6ad0da…260c` and the early-abort `chknorm` note now in §2.5.

**Overall audit verdict:** the classical comparison, the atomic-swap demonstration, and
the base/LAS core are all faithfully described by the report. No corrections required;
the terminology and provenance edits made this session bring the *prose* into line with
what the *code* already did.

---

## 5. On-chain / gas — status & the one gate before more work

`STATUS.md §3` (D17) now records a **complete, working native verifier**. The earlier
op-count *probe* (≈16.7 M gas estimate; `evm/test/LASVerifyCost.t.sol`) has been
**superseded** by a real byte-level verifier — `evm/src/LASVerifier.sol` (`library
LASVerify`, reusing vendored ZKNox ETHDILITHIUM primitives), validated end-to-end against
the C reference (accepts the golden adapted signature, rejects tamper) — wired into the
swap as `claimLASVerified` and **measured at 56,538,682 gas** at the D3 set. The floor
path `claimLAS` (calldata + keccak, no verify) remains 289,930 gas. The measured verifier
is **≈3.4× the EIP-7825 per-transaction gas cap (16,777,216)**, so it cannot run
as one mainnet transaction. One caveat holds:

- The Foundry local-chain swap *with full verification* is now done; the remaining
  on-chain work — a **deployed** precompile / zk proof / optimistic (Naysayer) scheme and
  a live-testnet swap — is **gated on Stage 1 being signed off** and must not displace the
  Meeting-6 reporting deliverables.

**Recommended order:** finish §4 audits → confirm the PR (M6.9) → then, if Stage 1 is
signed off, refresh the gas figures and extend the on-chain analysis.
