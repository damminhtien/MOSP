# NYC Parallel Larger Benchmark

Date: 2026-04-12

This directory archives a larger rerun of the NYC 3-objective parallel anytime
comparison on the same focused query used by the earlier note archive:

- query: `88959 -> 96072`
- solvers: `SOPMOA_relaxed`, `LTMOA_parallel`
- threads: `2`
- budgets: `0.5s`, `1.0s`, `2.0s`, `5.0s`
- repeats: `5`

Important scope note:

- every run in the raw CSV is still a timeout-limited anytime run, not a
  completed exact skyline computation on the full NYC graph
- this archive reflects the **current repo state on 2026-04-12**
- it should not be compared to the 2026-04-11 archive as if they were the same
  code state

## Raw Artifact

- `nyc_parallel_2thread_5x.csv`
  Raw canonical summary rows from the stable summary-only benchmark path.

## Verification Context

The benchmark was run after a fresh local rebuild and after these checks passed:

- `ctest --test-dir Release --output-on-failure -R 'benchmark_metrics_smoke|correctness_harness'`
- `./Release/ltmoa_frontier_regression`

## Median Summary

| Solver | Budget | Median runtime (s) | Median frontier | Median first solution (ms) | Median generated | Median expanded |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `SOPMOA_relaxed` | `0.5` | `0.503487` | `243` | `7.205` | `165,532` | `98,230` |
| `LTMOA_parallel` | `0.5` | `0.508775` | `320` | `7.974` | `235,638` | `139,605` |
| `SOPMOA_relaxed` | `1.0` | `1.004384` | `306` | `6.878` | `215,606` | `128,013` |
| `LTMOA_parallel` | `1.0` | `1.009542` | `461` | `7.645` | `348,975` | `207,126` |
| `SOPMOA_relaxed` | `2.0` | `2.005167` | `376` | `7.390` | `290,170` | `171,672` |
| `LTMOA_parallel` | `2.0` | `2.013214` | `646` | `7.958` | `506,372` | `302,331` |
| `SOPMOA_relaxed` | `5.0` | `5.010327` | `511` | `7.547` | `409,949` | `242,245` |
| `LTMOA_parallel` | `5.0` | `5.017894` | `972` | `7.822` | `848,437` | `511,509` |

## Reading

- `LTMOA_parallel` now shows a much stronger anytime frontier advantage than the
  older 2026-04-11 archive on the same query.
- The median frontier advantage grows with budget:
  - `0.5s`: `1.32x`
  - `1.0s`: `1.51x`
  - `2.0s`: `1.72x`
  - `5.0s`: `1.90x`
- Runtime stayed close between the two solvers at every budget, with
  `LTMOA_parallel` only `0.15%` to `1.05%` slower on median wall clock.
- `LTMOA_parallel` did **not** improve first-solution timing on this rerun; its
  median `time_to_first_solution_sec` was about `4%` to `11%` slower than
  `SOPMOA_relaxed`.

## Runner Attempt

The config-driven runner was attempted first with:

- config: `bench/configs/nyc_parallel_timecap_large.yaml`
- suite id: `nyc_parallel_timecap_large_20260412`

That path was interrupted and not used as the primary evidence source because
`SOPMOA_relaxed` intermittently printed end-of-run stats and then failed to
return control to the runner, which caused wall-timeout placeholder rows such as
`exit_code = -15` instead of canonical summary rows.

Partial runner evidence remains under:

- `bench/results/nyc_parallel_timecap_large_20260412/summary.csv`

## Command Provenance

Stable manual rerun:

```bash
mkdir -p notes/benchmarks/2026-04-12-nyc-parallel-large
rm -f notes/benchmarks/2026-04-12-nyc-parallel-large/nyc_parallel_2thread_5x.csv

for rep in 1 2 3 4 5; do
  for budget in 0.5 1.0 2.0 5.0; do
    ./Release/main -m maps/NY-d.txt maps/NY-t.txt maps/NY-m.txt \
      -a SOPMOA_relaxed -n 2 -s 88959 -t 96072 \
      --budget-sec "$budget" --seed "rep${rep}" \
      --summary-output notes/benchmarks/2026-04-12-nyc-parallel-large/nyc_parallel_2thread_5x.csv

    ./Release/main -m maps/NY-d.txt maps/NY-t.txt maps/NY-m.txt \
      -a LTMOA_parallel -n 2 -s 88959 -t 96072 \
      --budget-sec "$budget" --seed "rep${rep}" \
      --summary-output notes/benchmarks/2026-04-12-nyc-parallel-large/nyc_parallel_2thread_5x.csv
  done
done
```
