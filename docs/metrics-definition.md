# Metrics Definition

This document defines the benchmark metrics used by the canonical runtime,
benchmark runner, and aggregate pipeline.

## Runtime Counters

### `generated_labels`

Count of labels that were actually created and successfully enqueued.

This must not increase:

- when a label is merely popped
- when stale work is discarded
- when an attempted successor is rejected before enqueue

### `expanded_labels`

Count of labels that survived the required checks, were accepted for expansion,
and entered a valid successor-processing step.

This must not increase for labels rejected before a legitimate expansion event.

### `pruned_by_target`

Count of labels rejected by target-frontier dominance logic.

### `pruned_by_node`

Count of labels rejected by node-frontier dominance logic.

### `pruned_other`

Count of stale or uncategorized rejections that are not already classified as
`pruned_by_target` or `pruned_by_node`.

## Frontier Metrics

### `target_hits_raw`

Count of accepted target labels before final frontier normalization.

This is not the final answer size.

### `final_frontier_size`

Size of the final nondominated frontier at the target after normalization and
deterministic export.

This must not be replaced by raw target-hit counts.

### `frontier_size` in trace rows

Size of the target frontier snapshot associated with that trace sample.

## Time Metrics

### `runtime_sec`

Wall-clock runtime measured by `BenchmarkClock`, which is backed by
`std::chrono::steady_clock`.

### `wall_runtime_sec`

End-to-end process wall time measured by the Python benchmark runner.

This includes process startup, solver construction, search, benchmark
finalization, and process teardown.

### `time_to_first_solution_sec`

Also called `TTFS`.

Time from query start to the first accepted target-frontier solution.

If no solution is found, this field may be empty or left unset by the reporting
layer.

## Peak Metrics

### `peak_open_size`

Maximum observed queued-label count during the run.

### `peak_live_labels`

Maximum observed live-label count tracked by the solver during the run.

### `peak_rss_mb`

Peak resident set size in MB as measured by the benchmark runner's external
process monitor.

At present this is a runner-level metric, not an in-process C++ metric.

## Parallel Metrics

### `speedup`

For thread count `p`, define:

`S(p) = T(1) / T(p)`

Where:

- `T(1)` is the median runtime at one thread
- `T(p)` is the median runtime at thread count `p`

### `efficiency`

For thread count `p`, define:

`eta(p) = S(p) / p`

## Anytime Quality Metrics

### `hv_ratio`

Hypervolume ratio of a frontier relative to a reference frontier.

In the current aggregate pipeline:

- final-frontier HV ratio is implemented for `2` and `3` objectives
- higher-dimensional HV is not computed in Python

Trace-level `hv_ratio` currently comes from raw trace artifacts produced by the
solver-side trace exporter.

### `recall`

Exact-match recall of the run frontier relative to the chosen reference
frontier.

The aggregate interpretation is:

`recall = |run_frontier ∩ reference_frontier| / |reference_frontier|`

Trace-level `recall` currently comes from raw trace artifacts. Final-frontier
reference recall is recomputed in the aggregate layer.

## Status Fields

### `status`

Normalized benchmark-run status. Current values:

- `completed`
- `timeout`
- `crash`
- `build_error`
- `skipped`

### `completed`

Boolean field preserved from the solver or runner reporting path to indicate
whether the run finished under its configured conditions.

## Artifact Schema Locations

Primary raw schema locations:

- [README.md](/Users/macbook/Desktop/workspace/SOPMOA/README.md)
- [bench/README.md](/Users/macbook/Desktop/workspace/SOPMOA/bench/README.md)

Primary aggregate schema locations:

- `bench/results/figures/<analysis_id>/completion_summary.csv`
- `bench/results/figures/<analysis_id>/scaling_summary.csv`
- `bench/results/figures/<analysis_id>/anytime_summary.csv`
- `bench/results/figures/<analysis_id>/aggregate_manifest.json`
