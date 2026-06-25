---
description: Append a very short, cheap checkpoint to PROGRESS.md.
when_to_use: User says checkpoint, save progress, update PROGRESS.md, compact soon, or save state.
allowed-tools: Read Edit Write
---

# Cheap Checkpoint Skill

Append one short checkpoint to `PROGRESS.md`.

This skill is for memory handoff only. It is not an audit, not documentation sync, not benchmark review, and not repo analysis.

## Hard rules

- Edit only `PROGRESS.md`.
- Append only. Do not rewrite old checkpoints.
- Do not inspect the repository.
- Do not scan source files.
- Do not read docs.
- Do not read README files.
- Do not read evidence logs.
- Do not run tests, benchmarks, `make`, build commands, or git commands.
- Do not run `git status`, `git diff`, `git log`, or broad searches.
- Do not summarise the whole conversation.
- Do not repeat long project background.
- Do not include full diffs.
- Do not include transcripts.
- Do not include benchmark numbers unless the user explicitly provides them in the checkpoint request.

## Source of truth

Use only facts explicitly provided by the user in the current checkpoint request.

If information is missing, write `unknown`.

If the user gives too many details, compress them.

Do not open files to verify missing details.

## Length limit

Maximum checkpoint length: 120 words.

If keeping under 120 words conflicts with completeness, prioritise:
1. current goal,
2. done,
3. key files,
4. open risks,
5. next action.

## Compression rules

For `Done`:
- Use at most 3 bullets.
- If there are more than 3 completed actions, group them into at most 3 theme-level bullets.
- Do not list every small action.

For `Files touched/inspected`:
- Use at most 5 key files.
- If there are more than 5 files, list only the 5 most important files and add:
  `plus N other related files, not listed to keep checkpoint short.`
- If the exact number of omitted files is unknown, write:
  `plus other related files, not listed to keep checkpoint short.`

For `Evidence used`:
- Use at most 3 logs/files.
- If none are provided, write `none`.

For `Open risks`:
- Use at most 2 bullets.
- If none are provided, write `unknown`.

For `Next action`:
- Use exactly 1 bullet.
- It must be one concrete next step.

## Required format

## Checkpoint — YYYY-MM-DD HH:MM

Branch: <branch or unknown>

Current goal:
- <one sentence>

Done:
- <max 3 grouped bullets>

Files touched/inspected:
- <max 5 key files>
- plus <N> other related files, not listed to keep checkpoint short

Evidence used:
- <max 3 logs/files or none>

Open risks:
- <max 2 bullets or unknown>

Next action:
- <one exact next step>

## If the user provides a compact summary

Append it directly in the required format.

Do not expand it.

Do not verify it.

## If the user only says "checkpoint" without details

Append a minimal checkpoint using available explicit facts only.

Use `unknown` for missing fields.

Do not inspect the repo to fill gaps.

## Final response

After appending, reply only:

Checkpoint appended.
