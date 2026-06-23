---

# Checkpoint - YYYY-MM-DD HH:MM

## Current Objective
-

## Latest Subagent Findings
-

## Confirmed / Safe
-

## Risks / Unresolved
-

## Next Exact Steps
1.
2.
3.

## Do Not Do
- Do not edit evidence logs.
- Do not edit C source during audit mode.
- Do not merge to main before verification.
- Do not rely on `/compact` as the only memory.

## Next Commands for User
```bash
# commands to run manually
```

---

# Checkpoint - 2026-06-23 (time n/a — Bash disabled)

## Objective
- Get all `.md` docs updated, then commit/push the LAS-only docs + on-chain gas work to GitHub.

## Latest Context
- Local commit `2ffcca4` on branch `feat/onchain-gas-and-las-only-docs`: EVM gas benchmark (AdaptorSwap.sol + export_packed), new `docs/LAS_WALKTHROUGH.md` + `docs/STATUS.md`, IAS removed from all docs, objectives consolidated (3 old files deleted), LAS.md `## 9` header fixed.
- `git push` FAILED — no GitHub credentials / no `gh` in this env; branch is local-only (not on origin).
- Post-`2ffcca4` work appeared via ambient file-change notes (NOT in that commit, so uncommitted): `evm/src/LASVerifyCost.sol`+test, `docs/PROJECT_HISTORY_EXPLAINED.md`, `docs/GAS_LIMIT_INVESTIGATION.md`, `docs/LAS.md §8.4.1`, Makefile `bench_levels{paper,2,3,5}` + `test_contract3`, updated AdaptorSwap.sol / evm README / walkthrough.

## Findings
- "exceeds block gas limit" claim CORRECTED → native LAS verify ≈12M gas (~158× classical claim, ~40% of a 30M block): prohibitive but within a block (LASVerifyCost probe; evm/README + LAS.md §8.4.1).
- Last visible verification earlier this session: 8/8 functional tests + EVM 3/3 passed, 4 C benchmarks reproduced — NOT re-run after the LASVerifyCost/bench_levels/test_contract additions.

## Risks
- Branch not on GitHub (push blocked); post-`2ffcca4` additions still uncommitted (need a 2nd commit).
- `report/REPORT_DRAFT.md` still says native verify "infeasible / exceeds block limit" (from 2ffcca4), now contradicted by the ≈12M-gas finding.

## Next Steps
1. From an authenticated terminal: `git push -u origin feat/onchain-gas-and-las-only-docs`, then PR or merge to main.
2. Commit the post-`2ffcca4` additions (LASVerifyCost, the 2 new beginner docs, §8.4.1, bench_levels, test_contract).
3. Reconcile "exceeds block limit" → "≈12M gas / ~40% of block" wording in the report; build & run the new Makefile targets to confirm.

## Sync Needed
- doc-sync: Unknown (LAS.md/evm-README/walkthrough seem updated for §8.4.1; bench_levels & test_contract may still need doc reflection)
- report-sync: Yes
- reason: report retains the superseded "exceeds block gas limit / infeasible" framing; must match the ≈12M-gas LASVerifyCost result.

## Git Note
- Git state not rechecked by checkpoint. Visible in conversation: branch `feat/onchain-gas-and-las-only-docs` @ `2ffcca4` (local); push failed (no creds); uncommitted post-commit changes present per ambient notes.