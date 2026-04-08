# MOSP: Exact Multi-Objective Shortest-Path Solvers

This repository is a canonical-only C++17 workbench for exact multi-objective
shortest-path search. The codebase centers on three things:

- exact frontier-search solvers
- one shared instrumentation and artifact model
- one fast correctness harness for regression safety

Backward-compatibility paths have been removed. There is no legacy CSV mode,
no `--output`, no `--timelimit`, and no ad hoc solution-log export path.

## Stage Status

The repository is currently aligned to six completed stages:

1. Instrumentation core
   One benchmark pipeline based on summary, frontier, and trace artifacts.
2. Measurement semantics
   Shared wall-clock runtime and unified counter meanings across migrated solvers.
3. Deterministic frontier export
   Final target-frontier artifacts are canonical, sorted, and stable.
4. Correctness harness
   Synthetic exactness and regression checks are built into CTest.
5. Benchmark harness
   Dataset manifests, query manifests, benchmark modes, and config-driven runs
   now live under `bench/`.
6. Benchmark runner
   Suite-level summary aggregation, wrapper RSS capture, timeout/crash handling,
   and standardized benchmark artifacts now live in the runner.

## Solver Status

| Solver | Category | Current status |
| --- | --- | --- |
| `SOPMOA` | parallel exact | migrated to canonical instrumentation |
| `SOPMOA_relaxed` | parallel exact | migrated to canonical instrumentation |
| `SOPMOA_bucket` | parallel experimental | instrumented, but still excluded from the small in-process correctness matrix |
| `LTMOA` | serial exact | migrated |
| `LazyLTMOA` | serial exact | migrated |
| `LTMOA_array` | serial exact | migrated |
| `LazyLTMOA_array` | serial exact | migrated |
| `EMOA` | serial exact | migrated |
| `NWMOA` | serial exact | migrated |

## Build

### macOS

```bash
brew install cmake boost tbb
cmake -S . -B Release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$(brew --prefix boost);$(brew --prefix tbb)"
cmake --build Release -j4
```

### Linux

```bash
sudo apt update
sudo apt install -y build-essential cmake libboost-program-options-dev libtbb-dev
cmake -S . -B Release -DCMAKE_BUILD_TYPE=Release
cmake --build Release -j4
```

OpenMP is optional. If present it is linked, but runtime measurement semantics
do not depend on OpenMP timing.

## CLI

The executable is `./Release/main`.

At least one canonical output target is required:

- `--summary-output <file>`
- `--frontier-output-dir <dir>`
- `--trace-output-dir <dir>`

Main options:

- `-m, --map <file1> [file2 ...]`
- `-a, --algorithm <NAME>`
- `-s, --start <node>`
- `-t, --target <node>`
- `--scenario <file.json>`
- `--from <i>`
- `--to <j>`
- `--budget-sec <seconds>`
- `-n, --numthreads <N>`
- `--dataset-id <id>`
- `--query-id <id>`
- `--seed <value>`
- `--trace-interval-ms <ms>`

Threading behavior:

- `SOPMOA` caps `-n` at `12`
- `SOPMOA_bucket` caps `-n` at `16`
- `SOPMOA_relaxed` uses the requested thread count directly
- serial solvers run single-threaded

## Quick Start

Single-query run:

```bash
mkdir -p output/frontiers output/traces

./Release/main \
  -m maps/NY-d.txt maps/NY-t.txt maps/NY-m.txt \
  -s 96000 \
  -t 38000 \
  -a LTMOA \
  --budget-sec 0.05 \
  --summary-output output/summary.csv \
  --frontier-output-dir output/frontiers \
  --trace-output-dir output/traces \
  --dataset-id nyc_3obj \
  --query-id single_smoke
```

Scenario slice:

```bash
mkdir -p output/frontiers output/traces

./Release/main \
  -m maps/NY-d.txt maps/NY-t.txt maps/NY-m.txt \
  --scenario scen/query6.json \
  --from 0 \
  --to 1 \
  -a SOPMOA_relaxed \
  -n 4 \
  --budget-sec 0.05 \
  --summary-output output/summary.csv \
  --frontier-output-dir output/frontiers \
  --trace-output-dir output/traces \
  --dataset-id nyc_3obj \
  --query-id scenario_smoke
```

## Canonical Artifacts

Every migrated solver reports through the same artifact model.

### Summary CSV

One row per query:

```text
schema_version,solver_name,dataset_id,query_id,start_node,target_node,num_objectives,
threads_requested,threads_effective,budget_sec,runtime_sec,status,completed,seed,
generated_labels,expanded_labels,pruned_by_target,pruned_by_node,pruned_other,
target_hits_raw,final_frontier_size,peak_open_size,peak_live_labels,
target_frontier_checks,node_frontier_checks,time_to_first_solution_sec,
frontier_artifact_path,trace_artifact_path
```

Counter meanings:

- `generated_labels`: labels created and enqueued successfully
- `expanded_labels`: accepted labels that entered valid successor expansion
- `pruned_by_target`: rejected by target-frontier dominance
- `pruned_by_node`: rejected by node-frontier dominance
- `pruned_other`: stale or uncategorized rejections
- `target_hits_raw`: accepted target labels before final frontier normalization
- `final_frontier_size`: size of the final nondominated target frontier
- `peak_open_size`: maximum observed queued-label count
- `peak_live_labels`: maximum observed live-label count

### Frontier CSV

```text
time_found_sec,obj1,obj2,...
```

The exported frontier is the final nondominated target frontier. Rows are
sorted deterministically in lexicographic objective order.

### Trace CSV

```text
time_sec,trigger,frontier_size,generated_labels,expanded_labels,
pruned_by_target,pruned_by_node,pruned_other,target_hits_raw,hv_ratio,recall
```

Runtime measurement uses one source everywhere: `BenchmarkClock` over
`std::chrono::steady_clock`.

## Correctness Harness

The phase-4 correctness harness is built into the project.

Primary command:

```bash
./scripts/run_correctness_harness.sh Release
```

Equivalent manual commands:

```bash
/usr/local/bin/cmake --build Release -j4
ctest --test-dir Release --output-on-failure -R correctness_harness
```

The harness checks:

- exact frontier equality against an internal exhaustive reference
- no dominated point in exported frontiers
- no duplicate point in exported frontiers
- deterministic lex-sorted frontier export
- `SOPMOA(1) == SOPMOA(4)` on the synthetic set
- `SOPMOA_relaxed(1) == SOPMOA_relaxed(4)` on the synthetic set
- `SOPMOA(1) == SOPMOA_relaxed(1)` on the synthetic set
- measurement invariants such as `generated_labels >= expanded_labels`

Covered synthetic families:

- `small_grid`
- `layered_dag`
- `random_sparse`
- `correlated_weights`
- `anti_correlated`
- `tie_heavy`

Covered objective counts:

- `2`
- `3`
- `4`

See `tests/correctness_harness.cpp` and `data/synthetic/README.md`.

## Benchmark Harness

Phase 5 adds a config-driven benchmark layer.

Primary command:

```bash
python3 bench/scripts/run_benchmark.py --config bench/configs/timecap.yaml
```

Supported benchmark modes:

- `completion`
- `time_capped`
- `scaling`

Benchmark structure:

- `bench/manifests/datasets/`
- `bench/manifests/query_sets/`
- `bench/configs/`
- `bench/scripts/`
- `bench/results/`

Current checked-in manifests:

- dataset: `bench/manifests/datasets/nyc_3obj.json`
- query sets:
  - `bench/manifests/query_sets/nyc_trivial_completion.json`
  - `bench/manifests/query_sets/nyc_query6_pair.json`
  - `bench/manifests/query_sets/nyc_query6_single.json`

Current example configs:

- `bench/configs/completion.yaml`
- `bench/configs/timecap.yaml`
- `bench/configs/scaling.yaml`

Each suite writes:

- `environment.json`
- `resolved_config.json`
- `run_plan.json`
- `run_results.json`
- `summary.csv`
- `runs/<run_id>/summary.csv`
- `runs/<run_id>/frontiers/`
- `runs/<run_id>/traces/`
- `runs/<run_id>/stdout.log`
- `runs/<run_id>/stderr.log`

Run identity includes solver, threads, dataset, query set, mode, budget, repeat
index, and git SHA.

The suite `summary.csv` is the phase-6 benchmark table. It records one row per
query execution with wrapper metadata and canonical artifact paths, including:

- `run_id`
- `dataset_id`
- `query_set_id`
- `query_id`
- `solver`
- `threads`
- `mode`
- `budget_sec`
- `status`
- `completed`
- `runtime_sec`
- `generated_labels`
- `expanded_labels`
- `pruned_by_target`
- `pruned_by_node`
- `pruned_other`
- `final_frontier_size`
- `peak_open_size`
- `peak_live_labels`
- `peak_rss_mb`
- `time_to_first_solution_sec`
- `trace_file`
- `frontier_file`
- `stdout_file`
- `stderr_file`

`peak_rss_mb` is currently captured by the runner on macOS/Linux from the
process wrapper, not from the in-process C++ metrics layer.

Example successful suite output from this phase:

- `bench/results/phase5_timecap_smoke/`
- `bench/results/phase6_timecap_demo_v2/`

## Repo Guide

- `src/main.cpp`: CLI, solver dispatch, benchmark artifact wiring
- `inc/algorithms/abstract_solver.h`: shared solver and instrumentation hooks
- `src/algorithms/`: solver implementations
- `inc/algorithms/gcl/`: frontier containers and dominance structures
- `tests/benchmark_metrics_smoke.cpp`: measurement semantics smoke tests
- `tests/correctness_harness.cpp`: phase-4 exactness harness
- `bench/`: phase-5/6 benchmark manifests, configs, runner, and result layout
- `scripts/run_correctness_harness.sh`: local correctness entrypoint
- `ARCHITECTURE.md`: maintainer-oriented code map
- `run_command.txt`: reproducible local command cookbook
- `AGENTS.md`: project guidance for future code changes

## Current Gaps

- `SOPMOA_bucket` remains unstable enough that it is not part of the small
  in-process correctness matrix.
- True `crash` and `oom` classification still belongs in an external runner.
- The correctness harness is intentionally small and fast; it is separate from
  the benchmark harness.
