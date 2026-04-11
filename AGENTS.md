# AGENTS.md

You are an autonomous coding agent working in this repository.

## Core Rules

- Execute clear tasks to completion without asking for permission.
- Only pause to ask when a choice is destructive, irreversible, or materially ambiguous.
- Prefer direct evidence over assumptions.
- Verify changes before claiming completion.
- Do not revert unrelated user changes.

## Coding Rules

- Keep diffs small, readable, and reversible.
- Reuse existing code and patterns before adding new abstractions.
- Do not add dependencies unless explicitly requested.
- Prefer deletion over addition when simplifying code.
- For cleanup or refactor work, write a short plan first.
- Lock behavior with regression tests before risky cleanup when possible.

## Editing Rules

- Use `apply_patch` for manual file edits.
- Prefer `rg` and `rg --files` for search.
- Avoid destructive git commands unless explicitly requested.
- Do not amend commits unless explicitly requested.

## Verification

- Run the relevant build, tests, and checks after changes.
- Report what was verified and what was not.
- Final reports must include:
  - changed files
  - simplifications made
  - remaining risks

## Repo Memory

- After every meaningful change, improvement, idea, or experiment, append a short note to `notes/agent_memory_log.md`.
- Each memory note should capture:
  - date
  - task or change
  - files touched
  - outcome
  - evidence
  - directions that should not be repeated blindly

## Roadmap

- Keep follow-up ideas in `notes/roadmap.md`.
- After each meaningful work item or experiment, append short next steps or decision branches.
- Mark roadmap items as one of:
  - `next`
  - `investigate`
  - `hold`
  - `do-not-repeat`

## Benchmark Notes

- Important benchmark artifacts should not live only in `/tmp`.
- If a benchmark matters, move or rewrite it into `notes/benchmarks/`.
- Clearly distinguish raw archived artifacts from reconstructed summaries.

## Communication

- Keep progress updates short and concrete.
- Be honest about uncertainty, regressions, and failed experiments.
