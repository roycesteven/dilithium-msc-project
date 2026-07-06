---
description: Inspect repository changes and update only non-report project documentation. Never edit report files. Use when the user says sync docs, documentation sync, update project docs, before commit, after benchmark, after tests pass, or after implementation changes.
when_to_use: User says sync docs, doc-sync, documentation sync, update docs, before commit, after benchmark, after tests pass, after implementation changes.
allowed-tools: Bash Read Edit Write
---

# Documentation Sync Skill

Synchronise **non-report project documentation** based on actual repository changes.

This skill keeps project documentation centralised and prevents every file from becoming a progress tracker. It is **not** for editing the MSc report.

---

## Absolute Report Rule

Do **not** edit anything under:

```text
report/
report/REPORT_DRAFT.md
report/latex/
```

Even if report files appear outdated, do not edit them during `doc-sync`.

Instead, add a short reminder to `PROGRESS.md` under:

```markdown
## Report Updates Needed
-
```

Only edit report files if the user explicitly invokes a separate report task, such as:

```text
sync report
report-sync
update report
revise report
edit LaTeX report
update report with final benchmark evidence
```

If the user says only `sync docs`, report files are out of scope.

---

## Allowed Documentation Targets

This skill may update only these files when needed:

```text
PROGRESS.md
README.md
docs/STATUS.md
docs/04-evaluation/PROJECT_HISTORY_EXPLAINED.md
docs/LAS.md
docs/02-methodology/THEORY_IMPL_BRIDGE.md
docs/02-methodology/FUNCTION_MAP.md
docs/02-methodology/CODE_DIFF_VIEW.md
docs/01-introduction/LAS_WALKTHROUGH.md
docs/03-results/GAS_LIMIT_INVESTIGATION.md
```

This skill may read evidence logs, source files, tests, benchmark files, and report files to determine what documentation needs updating, but it must not edit them unless they are listed above.

This skill must not edit:

```text
report/
ref/
evm/
evidence/
scripts/
.github/
.claude/
```

Exception: edit `.claude/skills/...` only if the user explicitly asks to revise a skill.

---

## Source-of-Truth Rules

### `PROGRESS.md`

Purpose:

```text
handoff dashboard, latest checkpoint, current risks, next steps
```

Update when:

```text
- current objective changed
- risks/blockers changed
- next steps changed
- report/doc updates are needed
- user is about to compact/reset context
- user asks for sync docs and a concise summary is useful
```

Do not put long benchmark tables, full implementation history, or full report text here.

---

### `docs/STATUS.md`

Purpose:

```text
deliverable/test/benchmark status and reproduce commands
```

Update only when:

```text
- deliverable status changed
- test result changed
- benchmark result changed
- reproduce command changed
- accepted evidence path changed
```

Do not invent test results. Only use visible command output, committed logs, or user-provided results.

---

### `docs/04-evaluation/PROJECT_HISTORY_EXPLAINED.md`

Purpose:

```text
implementation history and rationale
```

Update only when:

```text
- major module added
- major file family added
- architecture changed
- benchmark family added
- EVM/application experiment changed
- implementation rationale changed
```

Do not update for small edits, typo fixes, or normal checkpoint updates.

---

### `docs/LAS.md`

Purpose:

```text
main technical reference for the LAS implementation and dissertation source material
```

Update only when:

```text
- LAS design explanation changed
- LAS parameter explanation changed
- algorithm behaviour changed
- accepted/final evaluation interpretation changed
- application or gas conclusion changed
- technical caveat or limitation changed
```

Do not update for:

```text
- normal checkpoint
- branch cleanup only
- raw evidence log changes that are not accepted as final
- report prose changes
```

---

### `docs/02-methodology/THEORY_IMPL_BRIDGE.md`

Purpose:

```text
paper equation / algorithm step to C function and file mapping
```

Update only when:

```text
- paper-to-code mapping changed
- function name changed
- algorithm step moved to another file/function
- parameter notation changed
- implementation deviates from the previous explanation
```

Do not update for:

```text
- benchmark number changes
- gas number changes
- report prose changes
- raw evidence log changes
```

---

### `docs/02-methodology/FUNCTION_MAP.md`

Purpose:

```text
function-level classification: Dilithium functions called as-is, modified, unused, or new
```

Update only when:

```text
- an upstream Dilithium function/file is modified
- LAS starts calling a new Dilithium primitive
- new LAS public/internal function is added
- function is renamed, moved, or removed
- the "zero upstream functions modified" claim changes
```

Do not update for:

```text
- benchmark result changes
- test output changes
- report prose changes
- evidence log changes
```

---

### `docs/02-methodology/CODE_DIFF_VIEW.md`

Purpose:

```text
human-readable code contribution summary for supervisor/report:
which files were reused unchanged, modified, added, or removed
```

Update only when:

```text
- branch comparison policy changed
- baseline branch changed
- review branch changed
- reused / modified / added / removed file categories changed
- code-contribution narrative became misleading
```

Do not update for:

```text
- normal checkpoint
- benchmark number changes
- report prose changes
- raw evidence log changes
```

GitHub Pull Request remains the canonical **live exact diff** for supervisor review.
`docs/02-methodology/CODE_DIFF_VIEW.md` is the explanatory summary, not a progress tracker.

---

### `docs/01-introduction/LAS_WALKTHROUGH.md`

Purpose:

```text
plain-English explanation and video-script spine
```

Update only when:

```text
- project story/framing changed
- demo flow changed
- final result interpretation changed
- caveat/limitation explanation changed
- explanation for non-cryptographer audience needs correction
```

Do not update for:

```text
- small function-level changes
- raw benchmark logs
- branch cleanup only
```

---

### `docs/03-results/GAS_LIMIT_INVESTIGATION.md`

Purpose:

```text
plain-English reasoning and evidence for the EVM gas investigation
```

Update only when:

```text
- EVM gas experiment changed
- LASVerifyCost Solidity/test files changed
- gas result changed
- block gas conclusion changed
- calldata/gas interpretation changed
```

Do not update for:

```text
- non-EVM benchmark changes
- ordinary C test changes
- report prose changes
```

---

### `README.md`

Purpose:

```text
build, run, reproduce instructions
```

Update only when:

```text
- dependencies changed
- setup changed
- commands changed
- reproduction workflow changed
```

Do not use README as a progress tracker.

---

### `evidence/`

Purpose:

```text
raw logs only
```

Rules:

```text
- Do not edit existing logs.
- Do not rewrite logs.
- Do not overwrite logs.
- Only reference evidence paths from docs if needed.
```

---

### GitHub Pull Request

Purpose:

```text
canonical live code diff and review thread
```

Rules:

```text
- Use GitHub PR for exact line-level code review.
- Use docs/02-methodology/CODE_DIFF_VIEW.md only for human-readable reused/modified/added summary.
- Do not merge to main before supervisor verification.
```

---

## Required First Step

Before editing anything, run:

```bash
git status -sb
git branch --show-current
git diff --name-status origin/main...HEAD
git diff --stat origin/main...HEAD
```

Then decide which documentation files actually need updating.

If `origin/main` is unavailable or the diff command fails, do not guess. Update only `PROGRESS.md` with a note that the diff could not be checked and ask the user to verify manually.

---

## Branch Safety Rule

`main` is the active working branch for this project, so doc-sync runs normally on `main`.

Only stop if the repository is in a detached-HEAD state or no branch can be determined
(e.g. `git branch --show-current` returns empty). In that case, do not edit any files and
tell the user to check out a branch first.

---

## Decision Rules

### If only chat context changed

Update only:

```text
PROGRESS.md
```

---

### If tests were added or test commands changed

Update:

```text
docs/STATUS.md
README.md if reproduce commands changed
PROGRESS.md summary
```

Do not edit report.

---

### If benchmark source or benchmark logs changed

Update:

```text
docs/STATUS.md if results/status/evidence path changed
docs/LAS.md if accepted/final evaluation interpretation changed
PROGRESS.md summary
```

Add a `Report Updates Needed` note to `PROGRESS.md`.

Do not edit report.

---

### If new major C/Solidity modules were added

Update:

```text
docs/04-evaluation/PROJECT_HISTORY_EXPLAINED.md
docs/STATUS.md if deliverable status changed
docs/02-methodology/CODE_DIFF_VIEW.md if file categories changed
docs/02-methodology/FUNCTION_MAP.md if function classification changed
docs/LAS.md if technical explanation changed
docs/02-methodology/THEORY_IMPL_BRIDGE.md if paper-to-code mapping changed
PROGRESS.md summary
```

Do not edit report.

---

### If LAS technical explanation changed

Update:

```text
docs/LAS.md
docs/02-methodology/THEORY_IMPL_BRIDGE.md if paper-to-code mapping changed
docs/02-methodology/FUNCTION_MAP.md if function classification changed
PROGRESS.md summary
```

Do not edit report.

---

### If function-level classification changed

Update:

```text
docs/02-methodology/FUNCTION_MAP.md
docs/02-methodology/CODE_DIFF_VIEW.md if file categories changed
PROGRESS.md summary
```

Do not edit report.

---

### If code-diff narrative or file categories changed

Update:

```text
docs/02-methodology/CODE_DIFF_VIEW.md
PROGRESS.md summary
```

Examples requiring `docs/02-methodology/CODE_DIFF_VIEW.md` update:

```text
- branch name changed from main to las-work-cleanup
- baseline comparison changed
- new source/test/benchmark file added
- modified-file list changed
- removed-file list changed
- old wording says main contains unverified LAS work
```

Do not edit report.

---

### If build/run instructions changed

Update:

```text
README.md
docs/STATUS.md if reproduce command changed
PROGRESS.md summary
```

Do not edit report.

---

### If plain-English story or video explanation changed

Update:

```text
docs/01-introduction/LAS_WALKTHROUGH.md
PROGRESS.md summary
```

Do not edit report.

---

### If EVM/gas experiment or conclusion changed

Update:

```text
docs/03-results/GAS_LIMIT_INVESTIGATION.md
docs/LAS.md if the main technical/evaluation interpretation changed
docs/STATUS.md if gas benchmark/evidence status changed
README.md if reproduce commands changed
PROGRESS.md summary
```

Do not edit report.

---

### If only report files changed

Do not edit report files.

Update only:

```text
PROGRESS.md summary
```

Example note:

```markdown
## Report Updates Needed
- Report files changed or appear outdated. Use `report-sync`, not `doc-sync`, before final submission.
```

---

### General docs rule

Do not update every file in `docs/`.

Only update a documentation file if its trigger is met.

If a file looks possibly outdated but the trigger is unclear, leave it unchanged and list it under:

```markdown
Manual checks still needed:
-
```

---

## Safety Rules

- Do not edit C source files.
- Do not edit benchmark source files.
- Do not edit Solidity files.
- Do not edit existing evidence logs.
- Do not edit report files.
- Do not edit `.claude/` unless the user explicitly asks to revise a skill.
- Do not merge to `main`.
- Do not commit automatically.
- Do not push automatically.
- Only stage, commit, or push if the user explicitly asks.
- Do not claim tests or benchmarks passed unless visible in current context, committed logs, or user-provided output.
- Do not use `docs/02-methodology/CODE_DIFF_VIEW.md` as a progress tracker.
- If unsure, update only `PROGRESS.md` and list manual checks.

---

## Output to User

After syncing, show:

```markdown
## Documentation Sync Summary

Updated:
-

Not updated:
-

Report touched:
- No

Reason:
-

Manual checks still needed:
-
```

If report appears outdated, say:

```markdown
Report update needed, but not performed by doc-sync.
Use `sync report` or `report-sync` explicitly.
```

Do not commit automatically unless the user explicitly asks.
