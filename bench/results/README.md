# Benchmark Results

Local benchmark outputs are written under this directory by the benchmark
runner.

The directory is ignored by git so benchmark suites do not pollute the repo.

Each suite creates a timestamped or user-supplied `suite_id` directory with:

- `environment.json`
- `resolved_config.json`
- `run_plan.json`
- `run_results.json`
- `summary.csv`
- `runs/<run_id>/...`
