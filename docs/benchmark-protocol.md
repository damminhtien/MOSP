# Benchmark Protocol

This document defines the recommended benchmark methodology for this repository.
It replaces the original smoke-style workflow as the primary benchmark path.

The repository originally only had small manual benchmark commands meant to
confirm that the binary ran and produced artifacts. Those commands are still
useful for local smoke checks, but they are not the research-grade path. The
recommended path is now:

1. run benchmark suites from manifests and configs
2. collect canonical summary, frontier, and trace artifacts
3. aggregate raw suites into paper-style tables
4. render deterministic figures from the aggregate outputs

## Goal

The benchmark system is designed to support four distinct claim types without
mixing their evidence.

1. Correctness
   Final frontiers are exact, nondominated, deterministic, and stable across
   instrumentation or refactoring changes.
2. Serial efficiency
   Serial exact solvers are compared by runtime, work counters, and memory on
   the same datasets and query sets.
3. Parallel scalability
   Parallel solvers are compared across thread counts with explicit speedup and
   efficiency definitions.
4. Anytime quality
   Under time budgets, solvers are compared by trace quality over time rather
   than only by final completion.

## Benchmark Claims

Each paper or README benchmark section should state which claim type it is
supporting.

Correctness claims should rely on:

- `tests/correctness_harness.cpp`
- deterministic frontier exports
- exact frontier equality checks where a reference is available

Serial-efficiency claims should rely on:

- `completion` mode when the workload is expected to finish
- identical datasets, query sets, build flags, and solver inputs
- aggregated runtime, work, and memory tables

Parallel-scalability claims should rely on:

- `scaling` mode
- fixed dataset and query set
- explicit thread counts
- speedup and efficiency computed from the `p=1` baseline

Anytime-quality claims should rely on:

- `time_capped` mode
- trace artifacts
- aggregated anytime summary tables
- plots over time instead of final runtime alone

## Dataset Tiers

The benchmark protocol distinguishes three dataset tiers.

### Synthetic

Purpose:

- exactness checks
- regression safety
- edge-case coverage
- tie-heavy and multi-objective stress on small instances

Typical location:

- `data/synthetic/`
- synthetic fixtures in `tests/correctness_harness.cpp`

### Real

Purpose:

- representative runtime and scaling measurement
- comparisons intended for README tables or paper sections

Typical location:

- manifests under `bench/manifests/datasets/`
- map files referenced by those manifests

### Stress

Purpose:

- heavy workloads
- long-running completion studies
- large-budget time-capped runs
- memory pressure and failure characterization

Stress datasets may be local-only. If they are not checked into the repository,
the manifest should still describe them clearly enough for reruns.

## Benchmark Modes

### Completion

Use this mode when the benchmark question is about exact completion cost,
runtime, work, or memory on workloads expected to finish.

Expected outputs:

- suite `summary.csv`
- run-level final frontier files
- run-level anytime trace files
- completion aggregate tables

### Time-Capped

Use this mode when the benchmark question is about time-bounded quality.

Expected outputs:

- suite `summary.csv`
- run-level trace files
- anytime aggregate tables
- anytime quality plots

### Scaling

Use this mode when the benchmark question is about parallel speedup and
efficiency.

Expected outputs:

- suite `summary.csv`
- scaling aggregate tables
- speedup and efficiency plots

## Canonical Output Artifacts

The benchmark protocol uses three raw canonical artifacts from the C++ runtime:

1. Summary rows
   One row per query execution.
2. Frontier files
   Final nondominated target frontier in deterministic sorted form.
3. Trace files
   Anytime trace rows sampled over wall-clock time.

The benchmark runner then adds suite-level metadata:

- `environment.json`
- `resolved_config.json`
- `build_status.json`
- `run_plan.json`
- `run_results.json`
- suite-level `summary.csv`

The aggregate pipeline writes analysis outputs under:

- `bench/results/figures/<analysis_id>/`

Primary aggregate outputs:

- `completion_summary.csv`
- `scaling_summary.csv`
- `anytime_summary.csv`
- `trace_curve_points.csv`
- `wilcoxon_runtime_pairs.csv`
- `aggregate_manifest.json`

## Metric Definitions

The authoritative metric semantics are documented in
[`docs/metrics-definition.md`](/Users/macbook/Desktop/workspace/SOPMOA/docs/metrics-definition.md).

The benchmark protocol requires these rules:

- `generated_labels` means label created and successfully enqueued
- `expanded_labels` means accepted label entering valid successor expansion
- `final_frontier_size` means final nondominated target frontier size
- `TTFS` means time to first solution
- `speedup` uses the `p=1` runtime baseline
- `efficiency` is speedup divided by thread count

Do not reinterpret counters on a per-solver basis.

## Statistical Reporting

Paper-grade reporting should separate raw runs from aggregates.

Minimum aggregate reporting:

- `num_runs`
- `success_count`
- `timeout_count`
- `crash_count`
- median runtime
- mean runtime
- runtime IQR
- runtime std
- median work counters
- median memory

For scaling:

- median `T(p)`
- `speedup = T(1) / T(p)`
- `efficiency = speedup / p`

For anytime:

- median trace `hv_ratio`
- median trace `recall`
- median frontier size
- median `TTFS`

Paired data for statistical testing should be emitted in a machine-readable
form. The current pipeline writes:

- `wilcoxon_runtime_pairs.csv`

This is the preferred input to later Wilcoxon signed-rank testing when the
environment has a statistics library installed.

## Failure Reporting

Benchmark reports must count failures explicitly. Do not hide them behind
filtered tables.

Normalized run statuses:

- `completed`
- `timeout`
- `crash`
- `build_error`
- `skipped`

The suite-level summary and aggregate tables should preserve enough information
to count each category by dataset, solver, thread count, and benchmark mode.

## Recommended Workflow

1. Build the release binary.
2. Run one or more benchmark suites through `bench/scripts/run_benchmark.py`.
3. Aggregate suites through `bench/scripts/aggregate_results.py`.
4. Render figures through `bench/scripts/plot_results.py`.
5. Cite the aggregate outputs, not ad hoc terminal output.

## Legacy Smoke Benchmark

The manual commands in `run_command.txt` and the quick CLI examples in
`README.md` are now classified as legacy smoke benchmarks.

They remain useful for:

- confirming a local build
- checking artifact wiring
- quick CLI validation

They are not sufficient for:

- paper tables
- scalability claims
- anytime-quality analysis
- reproducibility claims

Use the protocol in this document for benchmark work that will be shared,
reviewed, or cited.
