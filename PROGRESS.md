

## Checkpoint — 2026-07-23 (On-chain LAS verifier: Stages 1–3 DONE + validated)

Branch: restructure

Current goal:
- Build a REAL on-chain (Solidity) LAS verifier (poqeth on-chain mode; poqeth 2025/091
  itself excludes Dilithium + has no zk — see 2025-091.md). Staged, each stage validated
  vs C golden before the next. No protocol simplification.

Done (this session):
- 2025-091.pdf -> 2025-091.md (faithful working guide; key correction up top: poqeth =
  native on-chain + Naysayer optimistic, NOT zk; excludes lattice/Dilithium).
- Stage 1: ref/test/export_verify_vector.c (+Makefile target) — golden vectors
  (pp_normal A', t, M, packed sig, negacyclic-conv golden). BUILT+RAN; C base_verify
  ACCEPTS the golden adapted sig. Vectors in evm/test/vectors/*.bin.
- Stage 2: vendored ZKNox SHAKE256 -> evm/lib/zknox/ZKNOX_shake.sol (+NOTICE, MIT,
  upstream fc09dff). evm/test/ZKNoxShake.t.sol 4/4 PASS (NIST KAT + multi-absorb +
  streaming-squeeze).
- Stage 3: reuse ZKNox normal-domain NTT (nttFw/nttInv/vecMulMod) -> vendored
  ZKNOX_NTT_dilithium.sol + ZKNOX_dilithium_utils.sol. evm/test/LASNtt.t.sol 2/2 PASS
  (round-trip + conv == C schoolbook negacyclic-conv golden).

Key decisions:
- Reuse ZKNox primitives (SHAKE/NTT/SampleInBall), assemble OUR OWN hint-less LAS
  base_verify (A=[I|A'], c=H(pk,w,M), BitPack19 z). ETHDILITHIUM's ML-DSA top verifier
  NOT reused (hints, 4x4).
- NTT is normal-domain (not ref/ntt.c Montgomery); feed A'/t normal-domain; w' identical
  after canonicalisation => equivalent, not a simplification.
- A' registered as public param (paper Verify takes pp as given) -> only SHAKE256 needed
  on-chain, no SHAKE128 A'-expansion.

Files touched: 2025-091.md; ref/test/export_verify_vector.c; ref/Makefile;
evm/lib/zknox/{ZKNOX_shake,ZKNOX_NTT_dilithium,ZKNOX_dilithium_utils}.sol + NOTICE.md;
evm/test/{ZKNoxShake,LASNtt}.t.sol; evm/test/vectors/*.bin. third_party/ETHDILITHIUM
cloned (git-ignored).

Evidence: forge test ZKNoxShakeTest 4/4; LASNttTest 2/2; C exporter self-check ACCEPT.

Open risks / notes:
- Gas: 1 SHAKE256 ~200k; 1 conv ~1.2M. Full verify likely several M (consistent w/ ~16.7M est).
- SampleInBall validation needs a C challenge golden (b_poly_challenge is static in
  basesig.c) — export via a local copy in the exporter, or rely on Stage-5 end-to-end.

Next action:
- Stage 4: reuse ZKNOX_SampleInBall (tau=kappa=49) validated vs C challenge golden;
  BitPack19 z-decode validated vs exported z; norm check ||z||inf<=gamma-kappa.
- Then Stage 5 (assemble base_verify, end-to-end vs sig.bin), Stage 6 (wire claimLAS +
  gas report), and the paper two-timeout refund fix.

## Checkpoint — 2026-07-23b (On-chain LAS verifier WORKS end-to-end — Stage 5 DONE)

Branch: restructure

MILESTONE: a numerically-complete, validated on-chain (Solidity) LAS base verifier exists.
evm/src/LASVerifier.sol (library LASVerify) reproduces ref/basesig.c base_verify_internal;
evm/test/LASVerifier.t.sol 6/6 PASS — test_verify_accepts_golden ACCEPTS the real adapted
signature (matches C), and rejects tampered sig / c_tilde / message.

Stages (all validated vs C ground truth):
- 2 SHAKE256 (ZKNox vendored): 4/4  (incl. large block-crossing multi-absorb)
- 3 NTT (ZKNox nttFw/nttInv/vecMulMod): 2/2 vs C schoolbook conv
- 4 SampleInBall(tau=kappa=49) + BitPack19 z-decode + norm: 3/3 vs c.bin/z.bin
- 5 full verify: 6/6 (accept golden + reject tamper + w' vs w_prime.bin + oracle vs c_tilde)

KEY BUG FOUND+FIXED (Dilithium NTT-domain gotcha): poly_uniform samples A' DIRECTLY in
NTT domain (Â'); the verifier's ZKNox NTT needs NORMAL A'. export_verify_vector.c now
recovers normal A' via the "multiply-by-1" idiom (pointwise_montgomery(Â',ntt(1)) +
invntt_tomont). Decisive C self-check added: SHAKE256(pack(t)||pack(w')||M)==c_tilde -> OK.

Vendored (evm/lib/zknox, MIT, upstream fc09dff): ZKNOX_shake, ZKNOX_NTT_dilithium,
ZKNOX_dilithium_utils, ZKNOX_SampleInBall, ZKNOX_keccak_prng (+NOTICE.md). Full clone in
git-ignored third_party/ETHDILITHIUM. foundry.toml: via_ir=true (verify has many locals).

Design decisions locked: A' registered in NTT domain (LASVerify.toNttDomain, once) — NEVER
NTT'd per verify; per-verify op budget = 12 fwd NTT (5 z_bot + 1 c + 6 t) + 36 pointwise +
12 inv NTT, matching base_verify_internal.

GAS (measured, honest): verify ~77M — EXCEEDS a 30M block. Unoptimized; the bit-by-bit
BitPack19 z-decode (_readBits, ~17M) + Solidity NTT/pack overhead dominate. Optimization
(byte-aligned z-decode, etc.) is Stage 6.

Next action:
- Stage 6: wire LASVerify.verify into AdaptorSwap.claimLAS (replace floor stub); optimize
  z-decode toward the block limit; forge --gas-report.
- Then paper two-timeout (t2<t1) refund fix in chain.c + AdaptorSwap.sol.

## Checkpoint — 2026-07-23c (Stage 6 DONE — verified on-chain swap + gas report)

Branch: restructure

Stage 6 complete. AdaptorSwap.sol now has claimLASVerified (FULL LASVerify.verify) beside
claimLAS (floor). SECURE: fundLASVerified commits keccak256(abi.encode(A',t,M)); claim
re-derives + checks it (rejects substituted pk — test_LASVerifiedSwap_rejects_wrong_context).
Title fixed HTLC->SCRIPTLESS (no hash-preimage; chain never checks Y; timeout=refund only).

GAS REPORT (via_ir, deterministic):
  claimClassical      75,751   (full ecrecover verify)
  claimLAS (floor)   289,930   (calldata+keccak, NO verify)
  claimLASVerified 56,538,682  (FULL native LAS base_verify)  <- headline
Apples-to-apples full verify: 75,751 -> 56,538,682 = ~746x. ECDSA=precompile, LAS=Solidity.
56.5M EXCEEDS EIP-7825 per-TX cap 16,777,216 (~3.4x) => not one mainnet tx. (Block now 30M
target/60M max, which 56.5M would fit — binding limit is the per-tx cap, NOT the block.)
Old 16.7M was an ESTIMATE (incl ~2.76M calculated SHAKE); real adds Solidity SHAKE + decode
+ packing + ABI/memory + settlement overhead.

Full suite: 22/22 pass (7 suites). z-decode optimized bit-by-bit->byte-window (77M->68M verify;
claim 56.5M). foundry.toml via_ir=true.

Docs updated: evm/README.md (claimLASVerified bullet + result table + apples-to-apples),
AdaptorSwap.sol header+claimLASVerified comment. STILL TODO doc-sync: docs/LAS.md §8.4 +
docs/03-results/GAS_LIMIT_INVESTIGATION.md still cite the 16.7M estimate as headline.

Corrections applied from Royce review: function name base_verify (not base_sign_verify_internal);
scope claim to "evaluated D3 Solidity verifier" not "all PQ"; EIP-7825 per-tx cap framing;
~746x precise; SCRIPTLESS not HTLC; chain doesn't check Y; secure settlement via (A',t,M) commit.

Next action:
- Paper two-timeout (t2<t1) refund fix: chain.c/chain.h (single timeout -> t1>t2 asymmetric)
  + AdaptorSwap.sol refund. Then doc-sync LAS.md §8.4 / GAS_LIMIT_INVESTIGATION.md.

## Checkpoint — 2026-07-23 (Two-timeout refund fix — EVM only)

Branch: restructure

Current goal:
- Apply paper §4.1 two-timeout (t2<t1) refund rule to the EVM adaptor swap.

Done:
- AdaptorSwap.sol: added TWO-TIMEOUT RULE NatSpec (first-claimed leg = shorter t2, second = longer t1, gap = u2 safety window); clarified refund enforces only this leg's own timeout.
- AdaptorSwap.t.sol: new test_TwoTimeoutSafetyWindow (2 classical legs, asserts t2<t1, u1 claims 1st at ~t2, second leg still un-refundable => u2 keeps window, then u2 claims).
- evm/README.md: added "Two-timeout refund rule (§4.1)" subsection.

Files touched/inspected:
- evm/src/AdaptorSwap.sol, evm/test/AdaptorSwap.t.sol, evm/README.md

Evidence used:
- none (forge NOT run per guardrails)

Open risks:
- pcn C ledger (test_pcn.c scen 1&2) has same reversed-timeout bug but is OUT OF SCOPE (Royce), left unfixed.
- New test not yet run (should be 23rd EVM test).

Next action:
- Royce: cd evm && forge test --match-test test_TwoTimeoutSafetyWindow -vv (then full suite).
