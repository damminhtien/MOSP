<!-- AUTONOMY DIRECTIVE — DO NOT REMOVE -->
YOU ARE AN AUTONOMOUS CODING AGENT. EXECUTE TASKS TO COMPLETION WITHOUT ASKING FOR PERMISSION.
DO NOT STOP TO ASK "SHOULD I PROCEED?" — PROCEED. DO NOT WAIT FOR CONFIRMATION ON OBVIOUS NEXT STEPS.
IF BLOCKED, TRY AN ALTERNATIVE APPROACH. ONLY ASK WHEN TRULY AMBIGUOUS OR DESTRUCTIVE.
<!-- END AUTONOMY DIRECTIVE -->

# Project Guidance

This repository is a canonical-only MOSP codebase. Legacy benchmark and output
paths are intentionally removed. Do not reintroduce them.

## Current Stage State

The repo currently reflects four completed stages:

1. shared instrumentation core
2. unified measurement semantics
3. deterministic final-frontier export
4. correctness harness for exactness and regression safety

## Non-Negotiable Rules

- Keep the runtime model canonical-only.
- Do not add back `--output`, `--timelimit`, or ad hoc solution-log outputs.
- Use `BenchmarkClock` for runtime metrics.
- `generated_labels` means label created and enqueued.
- `expanded_labels` means accepted label that entered valid successor expansion.
- `final_frontier_size` must come from the final nondominated target frontier, never raw target hits.
- Final frontier export must remain deterministic and lex-sorted.
- Prefer deletion over compatibility shims.

## Main Code Map

- `src/main.cpp`
  CLI, solver dispatch, scenario execution, canonical artifact wiring.
- `inc/algorithms/abstract_solver.h`
  Shared solver hooks, benchmark integration, frontier finalization.
- `src/algorithms/`
  Solver implementations.
- `inc/algorithms/gcl/`
  Frontier containers and dominance structures.
- `tests/benchmark_metrics_smoke.cpp`
  Measurement semantics smoke tests.
- `tests/correctness_harness.cpp`
  Synthetic exactness and determinism checks.

## When Editing Code

- Keep diffs small and reversible.
- Reuse shared helpers before adding new solver-local instrumentation logic.
- If a solver needs special measurement handling, funnel the final semantics
  through the shared benchmark recorder rather than inventing a parallel path.
- Preserve deterministic artifact output.
- Prefer modern C++ standard-library facilities over C-style patterns.

## Required Verification

For code changes that affect runtime, solver behavior, or instrumentation, run:

```bash
/usr/local/bin/cmake --build Release -j4
ctest --test-dir Release --output-on-failure -R "benchmark_metrics_smoke|correctness_harness"
```

For CLI or artifact changes, also run at least one canonical smoke command from
`run_command.txt`.

## Documentation Contract

When behavior changes, update the relevant docs in the same pass:

- `README.md` for user-facing workflow
- `ARCHITECTURE.md` for maintainer-facing design
- `run_command.txt` for reproducible local commands
- `AGENTS.md` for future implementation guidance

## Known Limits

- `SOPMOA_bucket` remains experimental for broader correctness regression.
- External crash and OOM classification is still a runner concern.
- The correctness harness is intentionally compact, not a large benchmark suite.
