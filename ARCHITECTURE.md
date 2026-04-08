# Architecture

This document is the maintainer view of the current codebase after the legacy
benchmark path was removed. It explains how execution flows, where measurement
semantics live, how correctness is checked, how benchmark suites are
orchestrated and recorded, and how raw suites are aggregated into paper-style
tables and figures. The research-grade benchmark methodology itself is
documented under `docs/`.

## Project Shape

The repository has one runtime model:

- load a multi-objective graph
- run one exact solver
- emit canonical benchmark artifacts
- verify exactness and regressions with the built-in harness

There is no secondary legacy output mode.

## Execution Flow

The main entry point is `src/main.cpp`.

For a normal run the program does the following:

1. Parse CLI options.
2. Load one map file per objective through `read_graph_files(...)`.
3. Build forward and reverse adjacency structures.
4. Construct the requested solver.
5. Configure the benchmark run if any canonical artifact path is requested.
6. Execute either one `(start, target)` query or a scenario slice.
7. Finalize the run through `BenchmarkRecorder`.
8. Write summary, frontier, and trace CSV artifacts.

Scenario mode is just a thin loop over repeated single-query execution with
stable dataset and query identifiers.

On top of that CLI, phase 5 adds a Python benchmark runner that expands dataset
manifests, query manifests, and benchmark configs into organized suites under
`bench/results/`.

## Main Runtime Files

- `src/main.cpp`
  Solver selection, dataset/query ids, artifact naming, scenario dispatch.
- `src/utils/data_io.cpp`
  Map loading and scenario JSON loading.
- `src/problem/graph.cpp`
  Forward and reverse graph construction.
- `src/problem/heuristic.cpp`
  Reverse single-objective Dijkstra heuristics used by exact solvers.
- `bench/scripts/run_benchmarks.py`
  Config-driven benchmark orchestration, environment capture, run identity, and
  result layout.
  The phase-6 primary entrypoint is `bench/scripts/run_benchmark.py`; the plural
  script is only a thin compatibility shim.

## Benchmark Documentation Layer

Phase 8 adds a dedicated documentation layer for benchmark methodology:

- `docs/benchmark-protocol.md`
- `docs/benchmark-quickstart.md`
- `docs/metrics-definition.md`
- `docs/reproducibility.md`

These files define the recommended benchmark path. The older CLI examples remain
only as legacy smoke checks.

## Shared Solver Layer

`inc/algorithms/abstract_solver.h` is the shared contract for migrated solvers.

It provides:

- common result storage
- benchmark-run configuration
- access to `BenchmarkRecorder`
- target-frontier helper logic
- deterministic final frontier collection
- a single elapsed-time helper based on `BenchmarkClock`

The important rule is that solver-local code reports events, while the final
artifact semantics are owned by the shared benchmark layer.

## Measurement Semantics

The instrumentation core lives in:

- `inc/algorithms/abstract_solver.h`
- `inc/utils/benchmark_metrics.h`
- `src/utils/benchmark_metrics.cpp`

The codebase now uses one clock for runtime metrics:

- `BenchmarkClock`
- backed by `std::chrono::steady_clock`

The migrated counter meanings are:

- `generated_labels`
  Increment only when a start or successor label is actually created and
  enqueued.
- `expanded_labels`
  Increment only when a label survives the required checks and enters a valid
  successor expansion.
- `pruned_by_target`
  Rejected by target-frontier dominance.
- `pruned_by_node`
  Rejected by node-frontier dominance.
- `pruned_other`
  Frontier-update rejection, stale work, or uncategorized prune.
- `target_hits_raw`
  Accepted target labels before final frontier normalization.
- `final_frontier_size`
  Final nondominated target frontier size, never raw target-hit count.
- `peak_open_size`
  Peak queued-label count.
- `peak_live_labels`
  Peak live-label count owned by the solver.

## Solver Groups

### Serial exact solvers

Implemented under `src/algorithms/`:

- `ltmoa.cpp`
- `lazy_ltmoa.cpp`
- `ltmoa_array.cpp`
- `lazy_ltmoa_array.cpp`
- `emoa.cpp`
- `nwmoa.cpp`

These now share the same benchmark timing and target-frontier recording model.

### Parallel solvers

- `sopmoa.cpp`
- `sopmoa_relaxed.cpp`
- `sopmoa_bucket.cpp`

These keep solver-specific scheduling and frontier structures, but report
measurement through the same benchmark model. Shared counters are synchronized
through atomic accumulation and recorder-sync hooks rather than ad hoc writes.

## Frontier Structures

Node-local and target-frontier data structures live under `inc/algorithms/gcl/`.

Important variants:

- `gcl.h`
- `gcl_array.h`
- `gcl_tree.h`
- `gcl_nwmoa.h`
- `gcl_sopmoa.h`
- `gcl_relaxed.h`

The key repository rule is that final frontier export must be canonical even if
the internal frontier representation is solver-specific.

## Deterministic Artifacts

The final exported frontier is always:

- normalized
- nondominated
- lexicographically sorted
- written in a deterministic CSV format

The same rule applies to trace rows and summary-row field ordering.

This keeps regression checks stable across serial and parallel solvers.

## Correctness Harness

Phase 4 introduced the correctness harness in `tests/correctness_harness.cpp`.

It uses small generated DAG fixtures across these families:

- `small_grid`
- `layered_dag`
- `random_sparse`
- `correlated_weights`
- `anti_correlated`
- `tie_heavy`

It checks:

- exact frontier equality against an exhaustive reference
- deterministic frontier export
- no duplicates
- no dominated points
- measurement invariants
- 1-thread vs multi-thread final-frontier consistency on the supported parallel solvers

This harness is wired into CTest and can be run through
`scripts/run_correctness_harness.sh`.

## Benchmark Harness

Phase 5 introduced a manifest-driven benchmark layer under `bench/`.

Key directories:

- `bench/manifests/datasets/`
- `bench/manifests/query_sets/`
- `bench/configs/`
- `bench/scripts/`
- `bench/results/`

The runner in `bench/scripts/run_benchmark.py` does not replace `main`. It
wraps `main` and standardizes:

- dataset selection
- query-set selection
- benchmark mode selection
- per-run identity
- repeat handling
- retry or fail-fast policy
- environment capture
- result placement
- suite-level summary aggregation
- wrapper-level peak RSS capture
- timeout and crash classification

Current supported modes:

- `completion`
- `time_capped`
- `scaling`

Each suite writes:

- `environment.json`
- `resolved_config.json`
- `run_plan.json`
- `run_results.json`
- `summary.csv`
- `runs/<run_id>/...`

Each run directory contains canonical solver outputs plus runner metadata and
captured stdout/stderr.

## Aggregate And Figure Pipeline

Phase 7 adds two post-processing scripts under `bench/scripts/`:

- `aggregate_results.py`
- `plot_results.py`

`aggregate_results.py` consumes one or more suite-level `summary.csv` files and
their referenced frontier/trace artifacts, then writes a deterministic analysis
directory under `bench/results/figures/<analysis_id>/`.

That analysis directory currently contains:

- `all_runs_enriched.csv`
- `trace_samples.csv`
- `reference_frontiers.json`
- `completion_summary.csv`
- `scaling_summary.csv`
- `anytime_summary.csv`
- `trace_curve_points.csv`
- `wilcoxon_runtime_pairs.csv`
- `aggregate_manifest.json`

`plot_results.py` reads those aggregate tables and emits paper-oriented figures
as both `.png` and `.svg`.

Current plot set:

- runtime distribution boxplot
- speedup curve
- efficiency curve
- anytime HV-ratio curve
- anytime recall curve
- median peak-RSS bar chart

Current aggregate metric limits:

- final-frontier hypervolume is computed in Python only for `2` and `3`
  objectives
- higher-dimensional aggregate comparison falls back to recall,
  frontier size, and TTFS
- trace-level `hv_ratio` and `recall` are currently sourced directly from raw
  trace artifacts rather than being recomputed against the selected reference
  frontier at every timestamp

## Stage Map

The repository state after the current cleanup is:

1. Stage 1
   Canonical instrumentation core exists.
2. Stage 2
   Main solvers report metrics through shared semantics.
3. Stage 3
   Deterministic final-frontier export is enforced.
4. Stage 4
   Correctness harness covers exactness and regression safety.
5. Stage 5
   Benchmark harness covers datasets, query sets, benchmark modes, and
   organized result suites.
6. Stage 6
   Benchmark runner writes suite-level summary rows, peak RSS, and robust
   timeout/crash/build-error artifacts.
7. Stage 7
   Aggregate scripts and deterministic figures reduce raw suites into
   paper-style summaries, scaling tables, and anytime plots.
8. Stage 8
   Benchmark protocol, metric semantics, quickstart, and reproducibility docs
   define the recommended research-grade workflow.

## Known Limits

- `SOPMOA_bucket` is still considered experimental for broader regression use.
- External crash and OOM attribution is still outside the in-process runtime.
- The correctness harness is intentionally compact; it is not a substitute for
  long-running benchmark campaigns.
- Python aggregate HV support is intentionally limited to `2` and `3`
  objectives.
