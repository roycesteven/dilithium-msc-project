---
description: Append a compact project checkpoint to PROGRESS.md. Use after subagents finish, before compacting, before context gets full, or when the user says checkpoint/save progress.
when_to_use: User says checkpoint, save progress, update PROGRESS.md, context checkpoint, before compact, after subagent finished.
allowed-tools: Read Edit Write
---

# Checkpoint Skill

Append a compact checkpoint to `PROGRESS.md`.

This skill is for memory handoff only. It must not perform repository audit, documentation sync, report sync, testing, benchmarking, or code editing.

---

## Scope

- Only edit `PROGRESS.md`.
- Append only. Do not overwrite existing progress.
- If `PROGRESS.md` does not exist, create it.
- Do not edit `docs/`, `report/`, `README.md`, `evidence/`, `ref/`, `evm/`, or benchmark files.
- Do not run Bash.
- Do not run tests or benchmarks.
- Do not trigger `doc-sync` or `report-sync`.
- Do not inspect git status unless the status is already visible in the current conversation.
- Do not claim branch cleanliness, synced state, tests passed, or benchmarks passed unless visible in the current conversation or provided by the user.
- Use only the latest main-chat context and latest subagent findings available in the current conversation.
- If no subagent findings are available, write `None in current context`.
- Keep the checkpoint short: maximum 12 bullets total.
- Do not paste long logs, long diffs, or long report text.
- After editing, show the exact appended checkpoint.

---

## Compact Checkpoint Format

Append this structure:

```markdown
---

# Checkpoint - YYYY-MM-DD HH:MM

## Objective
-

## Latest Context
-

## Findings
-

## Risks
-

## Next Steps
1.
2.
3.

## Sync Needed
- doc-sync: Yes/No/Unknown
- report-sync: Yes/No/Unknown
- reason:

## Git Note
- Git state not rechecked by checkpoint unless visible in this conversation.
