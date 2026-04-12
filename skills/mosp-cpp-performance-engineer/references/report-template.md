# Report template

Use this structure by default. Adapt section names only when the task clearly needs a different shape.

# [Task title]

## Diagnosis
State the algorithm family, graph setting, codebase scope, and the main performance or correctness issue in one paragraph.

## What was inspected
List the sources used for the conclusion, such as repo modules, uploaded files, benchmark logs, papers, or build output.

## Key bottlenecks
List the main bottlenecks in descending order of likely impact.
For each bottleneck, label it as one of:
- algorithmic
- data layout / memory
- synchronization / communication
- build / compiler
- benchmarking / measurement

## Prioritized optimization roadmap
Use a numbered list. For each item include:
- change
- why it matters
- expected payoff
- implementation risk

## Patch plan
Break the work into concrete code or module edits.
Mention whether the plan is conservative, moderate, or aggressive.

## Benchmark plan or results
Include either:
- a measurement plan, or
- a comparison table if results already exist

Suggested columns:
- variant
- objectives
- threads
- dataset
- runtime
- speedup
- notes

## Parallelization notes
Explain what should stay shared-memory, what may benefit from MPI, and what is only a GPU follow-up candidate.

## Validation status
State exactly which of the following were verified:
- compile success
- unit tests
- output equivalence
- pareto-front equivalence
- determinism
- scalability

## Risks and open questions
State unverified assumptions, likely regressions, and missing information.
