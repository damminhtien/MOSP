# NYC Reliable Benchmark Archive

Date: 2026-04-11

This directory stores repo-local benchmark artifacts for the most important NYC runs that were still worth preserving after `/tmp` had already been cleared.

## Reliability Tier

These files are the highest-confidence local artifacts currently archived in-repo because they satisfy all of the following:
- they were rerun after the latest local build in this workspace
- `ltmoa_frontier_regression` and `benchmark_metrics_smoke` had already passed in the same code state
- they are raw canonical summary CSV outputs, not hand-copied tables
- each study was repeated `3` times

Important scope note:
- `nyc_serial_core_3x.csv` reflects the **current repo state on 2026-04-11**, where `LTMOA_array_superfast` already includes the deferred-seed + mixed-side-heap experiment
- this means it should not be confused with older historical `superfast` numbers summarized elsewhere in notes

## Files

### `nyc_serial_core_3x.csv`
- Query: NYC 3-objective `88959 -> 96072`
- Solvers: `LTMOA_array`, `LTMOA_array_superfast`
- Budgets: `0.25s`, `0.5s`, `1.0s`
- Repeats: `3`
- Threads: serial
- Purpose: current serial baseline comparison

Median summary:

| Solver | Budget | Median runtime (s) | Median frontier | Median first solution (ms) | Median generated | Median expanded |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `LTMOA_array` | `0.25` | `0.251547` | `147` | `1.311` | `54,848` | `31,508` |
| `LTMOA_array_superfast` | `0.25` | `0.251127` | `99` | `5.762` | `38,592` | `22,078` |
| `LTMOA_array` | `0.5` | `0.503768` | `213` | `1.299` | `81,875` | `47,435` |
| `LTMOA_array_superfast` | `0.5` | `0.502562` | `154` | `5.749` | `59,285` | `34,109` |
| `LTMOA_array` | `1.0` | `1.004292` | `290` | `1.290` | `122,629` | `71,020` |
| `LTMOA_array_superfast` | `1.0` | `1.005294` | `224` | `5.775` | `88,151` | `51,163` |

Reading:
- this archive is reliable as raw evidence for the **current** code state
- it also confirms that the current in-place scheduling experiment on `LTMOA_array_superfast` regressed anytime quality relative to `LTMOA_array`

### `nyc_parallel_relaxed_3x.csv`
- Query: NYC 3-objective `88959 -> 96072`
- Solvers: `SOPMOA_relaxed`, `LTMOA_parallel`
- Budgets: `0.5s`, `1.0s`
- Repeats: `3`
- Threads: `2`
- Purpose: current parallel core comparison

Median summary:

| Solver | Budget | Median runtime (s) | Median frontier | Median first solution (ms) | Median generated | Median expanded |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `SOPMOA_relaxed` | `0.5` | `0.503621` | `60` | `50.276` | `36,710` | `21,729` |
| `LTMOA_parallel` | `0.5` | `0.516254` | `112` | `17.530` | `88,679` | `53,162` |
| `SOPMOA_relaxed` | `1.0` | `1.004613` | `77` | `65.299` | `52,343` | `30,956` |
| `LTMOA_parallel` | `1.0` | `1.018225` | `162` | `18.179` | `129,054` | `77,860` |

Reading:
- this remains the strongest repo-local benchmark signal in favor of the new parallel core
- all runs are timeout-limited anytime runs, not completed exact real-map runs

## Command Provenance

Serial study:

```bash
for rep in 1 2 3; do
  for budget in 0.25 0.5 1.0; do
    ./build-codex/main -m maps/NY-d.txt maps/NY-t.txt maps/NY-m.txt \
      -a LTMOA_array -s 88959 -t 96072 \
      --budget-sec "$budget" --seed "rep${rep}" \
      --summary-output notes/benchmarks/2026-04-11-nyc-reliable/nyc_serial_core_3x.csv

    ./build-codex/main -m maps/NY-d.txt maps/NY-t.txt maps/NY-m.txt \
      -a LTMOA_array_superfast -s 88959 -t 96072 \
      --budget-sec "$budget" --seed "rep${rep}" \
      --summary-output notes/benchmarks/2026-04-11-nyc-reliable/nyc_serial_core_3x.csv
  done
done
```

Parallel study:

```bash
for rep in 1 2 3; do
  for budget in 0.5 1.0; do
    ./build-codex/main -m maps/NY-d.txt maps/NY-t.txt maps/NY-m.txt \
      -a SOPMOA_relaxed -n 2 -s 88959 -t 96072 \
      --budget-sec "$budget" --seed "rep${rep}" \
      --summary-output notes/benchmarks/2026-04-11-nyc-reliable/nyc_parallel_relaxed_3x.csv

    ./build-codex/main -m maps/NY-d.txt maps/NY-t.txt maps/NY-m.txt \
      -a LTMOA_parallel -n 2 -s 88959 -t 96072 \
      --budget-sec "$budget" --seed "rep${rep}" \
      --summary-output notes/benchmarks/2026-04-11-nyc-reliable/nyc_parallel_relaxed_3x.csv
  done
done
```

## Related Files

- [Historical Summary From Notes](/workspaces/SOPMOA/notes/benchmarks/2026-04-11-nyc-reliable/historical_summary_from_notes.md)
- [Agent Memory Log](/workspaces/SOPMOA/notes/agent_memory_log.md)
- [Roadmap](/workspaces/SOPMOA/notes/roadmap.md)
