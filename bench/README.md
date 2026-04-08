# Benchmark Harness

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
