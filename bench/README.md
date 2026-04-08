# Benchmark Harness

The canonical benchmark methodology is documented in:

- [docs/benchmark-protocol.md](/Users/macbook/Desktop/workspace/SOPMOA/docs/benchmark-protocol.md)
- [docs/benchmark-quickstart.md](/Users/macbook/Desktop/workspace/SOPMOA/docs/benchmark-quickstart.md)
- [docs/metrics-definition.md](/Users/macbook/Desktop/workspace/SOPMOA/docs/metrics-definition.md)
- [docs/reproducibility.md](/Users/macbook/Desktop/workspace/SOPMOA/docs/reproducibility.md)

The repository benchmark flow is now config-driven.

Directory layout:

- `bench/datasets/`
  Notes about checked-in dataset assets or future local dataset copies.
- `bench/queries/`
  Notes about query-set sources or future checked-in query assets.
- `bench/configs/`
  Benchmark mode configs.
- `bench/scripts/`
  Orchestration entrypoints.
- `bench/results/`
  Local benchmark outputs. Ignored by git except for this README.
- `bench/manifests/`
  Dataset and query-set manifests used by the runner.

Runner entrypoint:

```bash
python3 bench/scripts/run_benchmark.py --config bench/configs/timecap.yaml
```

The configs use a JSON-compatible YAML subset so they can be parsed by the
stdlib-only Python runner without external dependencies.

Phase 7 adds a second layer on top of raw suites:

- `bench/scripts/aggregate_results.py`
- `bench/scripts/plot_results.py`

These scripts reduce raw benchmark suites into paper-style tables and figures
under `bench/results/figures/<analysis_id>/`.

## Dataset Manifest

Location:

- `bench/manifests/datasets/<dataset_id>.json`

Fields:

- `manifest_version`
- `dataset_id`
- `description`
- `objective_files`
- `num_objectives`
- `source_type`
- `notes`

Example:

```json
{
  "manifest_version": "dataset_manifest_v1",
  "dataset_id": "nyc_3obj",
  "description": "NYC example graph with 3 objectives",
  "objective_files": ["maps/NY-d.txt", "maps/NY-t.txt", "maps/NY-m.txt"],
  "num_objectives": 3,
  "source_type": "real",
  "notes": ["Used by README smoke runs and benchmark configs"]
}
```

## Query-Set Manifest

Location:

- `bench/manifests/query_sets/<query_set_id>.json`

Fields:

- `manifest_version`
- `query_set_id`
- `dataset_id`
- `description`
- `source`
- `queries`

Each query contains:

- `query_id`
- `start`
- `target`
- optional `difficulty`
- optional `seed`
- optional `source`

## Config Modes

Provided modes:

- `completion`
  Run with a generous completion budget ceiling and expect completion-oriented
  behavior.
- `time_capped`
  Run with explicit time budgets.
- `scaling`
  Run the same workload across multiple thread counts.

Each config contains:

- `config_version`
- `mode`
- `description`
- `build_dir`
- `binary`
- `build`
- `execution.kind`
- `solvers`
- `datasets`
- `query_sets`
- `budgets_sec`
- `repeat_count`
- `runtime_control`
- `failure_policy`
- `output`

## Result Layout

Each benchmark suite creates:

- `bench/results/<suite_id>/environment.json`
- `bench/results/<suite_id>/resolved_config.json`
- `bench/results/<suite_id>/run_plan.json`
- `bench/results/<suite_id>/run_results.json`
- `bench/results/<suite_id>/summary.csv`
- `bench/results/<suite_id>/runs/<run_id>/...`

Aggregate analyses create:

- `bench/results/figures/<analysis_id>/aggregate_manifest.json`
- `bench/results/figures/<analysis_id>/completion_summary.csv`
- `bench/results/figures/<analysis_id>/scaling_summary.csv`
- `bench/results/figures/<analysis_id>/anytime_summary.csv`
- `bench/results/figures/<analysis_id>/trace_curve_points.csv`
- `bench/results/figures/<analysis_id>/wilcoxon_runtime_pairs.csv`
- `bench/results/figures/<analysis_id>/figures/*.png`
- `bench/results/figures/<analysis_id>/figures/*.svg`

Each run directory contains:

- `run.json`
- `status.json`
- `command.json`
- `summary.csv`
- `frontiers/`
- `traces/`
- `stdout.log`
- `stderr.log`
- `scenario.json` when execution kind is `scenario`

## Suite Summary Schema

The suite-level `summary.csv` contains one row per query execution with these
stable fields:

- `run_id`
- `dataset_id`
- `query_set_id`
- `query_id`
- `solver`
- `reported_solver_name`
- `threads`
- `threads_requested`
- `threads_effective`
- `mode`
- `budget_sec`
- `repeat_index`
- `status`
- `raw_solver_status`
- `completed`
- `exit_code`
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

Status values are normalized at the runner layer:

- `completed`
- `timeout`
- `crash`
- `build_error`
- `skipped`

`peak_rss_mb` is currently measured by the runner with an external process
monitor based on `ps`, which works for the local macOS/Linux workflow. It is
not yet sourced from the C++ runtime itself.

## Aggregate Commands

End-to-end example:

```bash
python3 bench/scripts/run_benchmark.py --config bench/configs/completion.yaml --suite-id demo_completion
python3 bench/scripts/run_benchmark.py --config bench/configs/timecap.yaml --suite-id demo_timecap
python3 bench/scripts/run_benchmark.py --config bench/configs/scaling.yaml --suite-id demo_scaling

python3 bench/scripts/aggregate_results.py \
  --suite demo_completion \
  --suite demo_timecap \
  --suite demo_scaling \
  --analysis-id demo_analysis

python3 bench/scripts/plot_results.py \
  --input-dir bench/results/figures/demo_analysis
```

## Aggregate Semantics

`completion_summary.csv` groups by `dataset_id / solver / threads` and reports:

- `num_runs`
- `success_count`
- `timeout_count`
- `crash_count`
- `median_runtime_sec`
- `mean_runtime_sec`
- `iqr_runtime_sec`
- `std_runtime_sec`
- `median_generated_labels`
- `median_expanded_labels`
- `median_final_frontier_size`
- `median_peak_rss_mb`

`scaling_summary.csv` groups by
`dataset_id / query_set_id / solver / threads` and reports:

- median `T(p)`
- `speedup = T(1) / T(p)`
- `efficiency = speedup / p`

`anytime_summary.csv` groups by `dataset_id / solver / threads / budget_sec`
and reports:

- median trace `hv_ratio`
- median trace `recall`
- median trace `frontier_size`
- median `time_to_first_solution_sec`
- median final-frontier reference HV ratio when available
- median final-frontier reference recall

Current limits:

- Hypervolume is implemented in the Python aggregate layer for `2` and `3`
  objectives only.
- Higher-dimensional aggregate analysis falls back to recall, frontier size,
  and TTFS.
- `wilcoxon_runtime_pairs.csv` is a paired export for later statistical tests;
  no SciPy dependency is required by default.
