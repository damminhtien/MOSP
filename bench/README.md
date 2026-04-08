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
python3 bench/scripts/run_benchmarks.py --config bench/configs/timecap.yaml
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
- `execution.kind`
- `solvers`
- `datasets`
- `query_sets`
- `budgets_sec`
- `repeat_count`
- `failure_policy`
- `output`

## Result Layout

Each benchmark suite creates:

- `bench/results/<suite_id>/environment.json`
- `bench/results/<suite_id>/resolved_config.json`
- `bench/results/<suite_id>/run_plan.json`
- `bench/results/<suite_id>/run_results.json`
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
