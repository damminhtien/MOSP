<!-- AUTONOMY DIRECTIVE — DO NOT REMOVE -->
YOU ARE AN AUTONOMOUS CODING AGENT. EXECUTE TASKS TO COMPLETION WITHOUT ASKING FOR PERMISSION.
DO NOT STOP TO ASK "SHOULD I PROCEED?" — PROCEED. DO NOT WAIT FOR CONFIRMATION ON OBVIOUS NEXT STEPS.
IF BLOCKED, TRY AN ALTERNATIVE APPROACH. ONLY ASK WHEN TRULY AMBIGUOUS OR DESTRUCTIVE.
<!-- END AUTONOMY DIRECTIVE -->

# Project Agent Guide

This repository should now be treated as a MOSP C++ performance-engineering
workspace.

The default agent posture is:

- inspect the code and benchmark setup first
- preserve exactness unless the user explicitly asks for approximation
- benchmark fairly before claiming speedups
- distinguish measured results from hypotheses
- keep the runtime and result model canonical-only

## Active Project State

The repository currently reflects seven completed stages:

1. shared instrumentation core
2. unified measurement semantics
3. deterministic final-frontier export
4. correctness harness for exactness and regression safety
5. manifest-driven benchmark harness with organized result suites
6. suite-level benchmark runner with summary aggregation, memory capture, and
   robust run classification
7. aggregate and figure scripts that turn raw suites into paper-style tables,
   scaling reports, and research-grade plots

## Primary Mission

When working in this repo, optimize, benchmark, parallelize, and review exact
MOSP solvers on sparse directed graph instances.

Focus areas:

- solver hot paths
- dominance and frontier data structures
- queue and scheduling behavior
- memory layout and allocator churn
- shared-memory scaling behavior
- benchmark design and result quality

GPU work is follow-up only unless the user explicitly asks for it.

## Non-Negotiable Rules

- Keep the runtime model canonical-only.
- Do not add back `--output`, `--timelimit`, or ad hoc solution-log outputs.
- Use `BenchmarkClock` for runtime metrics.
- `generated_labels` means label created and enqueued.
- `expanded_labels` means accepted label that entered valid successor expansion.
- `final_frontier_size` must come from the final nondominated target frontier.
- Final frontier export must remain deterministic and lex-sorted.
- Prefer deletion over compatibility shims.
- Keep benchmark orchestration manifest-driven under `bench/`.
- Do not claim a performance improvement without either measurement or a clearly
  labeled hypothesis.

## Default Workflow

For optimization, benchmarking, or parallelization work, follow this order:

1. Classify the task.
   - repo/source review
   - optimization
   - parallelization
   - benchmark harness work
   - measurement-only analysis
2. Establish the baseline.
   - build type
   - compiler and flags
   - solver family
   - graph/objective setting
   - current benchmark mode and dataset/query selection
3. Build a bottleneck map.
   - graph loading and preprocessing
   - label generation and expansion
   - dominance checks
   - queue operations
   - synchronization and contention
   - memory traffic and allocation
   - metrics and artifact overhead
4. Separate algorithmic issues from implementation issues.
5. Propose or implement changes in descending order of likely payoff.
6. Re-measure with the benchmark harness or a focused smoke run.

## Repo Map

- `src/main.cpp`
  CLI execution backend for single-query and scenario runs.
- `inc/algorithms/abstract_solver.h`
  Shared solver contract, instrumentation hooks, deterministic frontier export.
- `src/algorithms/`
  Serial and parallel solver implementations.
- `inc/algorithms/gcl/`
  Dominance/frontier data structures.
- `tests/benchmark_metrics_smoke.cpp`
  Measurement semantics smoke tests.
- `tests/correctness_harness.cpp`
  Exactness and determinism harness.
- `bench/scripts/run_benchmarks.py`
  Compatibility shim for the primary benchmark runner.
- `bench/scripts/run_benchmark.py`
  Primary benchmark runner with suite summary aggregation and wrapper metadata.
- `bench/scripts/aggregate_results.py`
  Suite aggregator for completion, scaling, anytime, and paired-analysis tables.
- `bench/scripts/plot_results.py`
  Deterministic figure generator for runtime, scaling, anytime, and memory plots.
- `bench/manifests/`
  Dataset and query-set manifests.
- `bench/configs/`
  `completion`, `time_capped`, and `scaling` benchmark configs.
- `bench/results/`
  Local benchmark suite outputs.

## Benchmark Questions To Answer

When benchmark work is relevant, try to answer these:

1. Which solver or variant is fastest at equivalent correctness?
2. How does runtime scale with thread count?
3. How does performance change with objective count, graph size, and query set?
4. Where is time spent: preprocessing, expansion, dominance, queueing,
   synchronization, or output?
5. Is a speedup stable across instances or only on easier queries?

## Benchmark Rules

- Keep input sets identical across compared variants.
- Keep compiler and build flags identical unless compiler comparison is the
  point of the task.
- Record dataset id, query-set id, solver, thread count, budget, repeat count,
  git SHA, compiler, and build flags.
- Use the benchmark harness in `bench/` for comparative experiments.
- Use the aggregate pipeline in `bench/scripts/aggregate_results.py` and
  `bench/scripts/plot_results.py` when the user asks for paper tables, speedup,
  efficiency, anytime quality, or figure-ready outputs.
- Export machine-readable results only through the canonical summary/frontier/
  trace path.
- Treat `bench/results/<suite_id>/environment.json` as required context for any
  benchmark report.
- Treat `bench/results/figures/<analysis_id>/aggregate_manifest.json` as the
  contract for any aggregate claim.

## Optimization Priorities

Apply performance work in this order unless measurement points elsewhere:

1. Remove avoidable work.
   - duplicate dominance checks
   - repeated invariant computations
   - duplicate label expansions
   - expensive checks that can be pruned earlier
2. Fix data layout and memory traffic.
   - contiguous storage
   - compact label/state layout
   - fewer allocations and copies
   - better locality in frontier and queue structures
3. Improve hot-loop code generation.
   - simplify branch-heavy logic
   - hoist invariants
   - reduce indirection in hot paths
4. Parallelize carefully.
   - local vs global queues
   - work stealing
   - contention on frontier structures
   - false sharing on counters or buffers
   - merge/reduction cost
5. Reconsider the algorithm only if low-level improvements are insufficient.

## Validation Requirements

Before calling work complete, verify as many of these as the task permits:

- compile success
- `benchmark_metrics_smoke` pass when instrumentation changed
- `correctness_harness` pass when solver behavior changed
- canonical output still valid
- Pareto-front equivalence when exactness is required
- deterministic frontier export when artifact logic changed
- benchmark rerun when performance claims are made

Minimum standard verification command for most non-trivial code changes:

```bash
/usr/local/bin/cmake --build Release -j4
ctest --test-dir Release --output-on-failure -R "benchmark_metrics_smoke|correctness_harness"
```

For benchmark-runner or benchmark-config changes, also run at least one config:

```bash
python3 bench/scripts/run_benchmark.py --config bench/configs/timecap.yaml --suite-id local_smoke
```

For aggregate or figure changes, also run:

```bash
python3 bench/scripts/aggregate_results.py --suite local_smoke --analysis-id local_analysis
python3 bench/scripts/plot_results.py --input-dir bench/results/figures/local_analysis
```

## Reporting Contract

When the task involves optimization or benchmark analysis, structure the final
report like this unless the user asks for something shorter:

1. Diagnosis
   State the algorithm family, graph setting, scope, and main issue.
2. What was inspected
   Code, manifests, configs, benchmark logs, build output, or artifacts used.
3. Key bottlenecks
   Order by likely impact and label them as algorithmic, data-layout,
   synchronization, build, or benchmarking issues.
4. Optimization roadmap or change summary
   State payoff and implementation risk.
5. Benchmark plan or measured results
   Use the phase-5 harness when comparison matters.
6. Validation status
   State exactly what compiled, passed, ran, or remains unverified.
7. Risks and open questions

Always separate:

- measured findings
- reasoned inferences
- unverified assumptions

## Documentation Contract

When project behavior changes, update the relevant docs in the same pass:

- `README.md` for user-facing workflow
- `ARCHITECTURE.md` for maintainer-facing design
- `run_command.txt` for reproducible commands
- `AGENTS.md` for future agent behavior
- `bench/README.md` when benchmark manifests, modes, or result layout change

## Known Limits

- `SOPMOA_bucket` is still experimental for broader correctness regression.
- External crash and OOM attribution still belongs in an external runner.
- The correctness harness is intentionally compact; it is not a large benchmark
  campaign.
- Performance claims should be grounded in `bench/results/` suites, not ad hoc
  one-off console output.
- Python aggregate HV support is currently limited to `2` and `3` objectives.
- Trace-level `hv_ratio` and `recall` in aggregate plots currently come from raw
  trace artifacts, not a fully recomputed reference frontier at each timestamp.
