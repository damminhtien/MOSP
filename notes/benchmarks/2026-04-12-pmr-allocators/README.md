# PMR Allocator Benchmark

Date: 2026-04-12

This note archives the first benchmark pass after moving hot LTMOA allocation
paths onto PMR resources:

- `LTMOA_array_superfast`: labels now come from a per-solve
  `std::pmr::monotonic_buffer_resource`, and hot scratch containers use a
  `std::pmr::unsynchronized_pool_resource`
- `LTMOA_parallel`: each worker now owns private PMR pools for labels and
  scratch vectors, and published target snapshots use a
  `std::pmr::synchronized_pool_resource`

All runs below are still timeout-limited anytime runs on the NYC 3-objective
map for the focused archived query `88959 -> 96072`.

## Before vs After (Practical Takeaway)

- `LTMOA_array_superfast` versus `LTMOA_array`:
  - runtime ratio stayed essentially flat at `0.9980x`, `1.0001x`, `1.0002x`,
    and `0.9994x` for `0.5s`, `1.0s`, `2.0s`, and `5.0s`
  - RSS ratio improved to `0.8858x`, `0.9250x`, and `0.9813x` at
    `0.5s`, `1.0s`, and `2.0s`, then rose to `1.0572x` at `5.0s`
  - frontier ratio improved to `1.2094x`, `1.3315x`, `1.2488x`, and `1.3057x`
  - first-solution ratio improved to `0.6089x`, `0.8522x`, `0.7001x`, and
    `0.7620x`
  - practical conclusion: the PMR rewrite improved memory at shorter budgets
    and preserved wall-clock time while the solver explored a better anytime
    frontier
- `LTMOA_parallel` versus the earlier archived `2`-thread run:
  - runtime ratio stayed essentially flat at `0.9975x`, `0.9998x`, `1.0000x`,
    and `0.9995x`
  - frontier ratio was `1.0000x`, `0.9740x`, `0.9567x`, and `0.8786x`, so the
    archived anytime frontier did not improve overall on this query
  - the old parallel archive does not expose RSS, so a before/after memory
    improvement claim cannot be made for `LTMOA_parallel`
  - practical conclusion: the allocator rewrite preserved runtime and
    stabilized memory behavior, but it was not a clear anytime-quality win for
    the archived `2`-thread benchmark

## Raw Artifacts

- `nyc_ltmoa_array_pmr_3x_summary.csv`
- `nyc_ltmoa_parallel_pmr_5x_summary.csv`

## Verification Context

Benchmarks were run after:

- `cmake --build Release -j4`
- `ctest --test-dir Release --output-on-failure -R 'benchmark_metrics_smoke|correctness_harness'`
- `./Release/ltmoa_frontier_regression`

## Serial Median Summary

Config:

- `bench/configs/nyc_ltmoa_array_pmr.yaml`
- solvers: `LTMOA_array`, `LTMOA_array_superfast`
- threads: `1`
- budgets: `0.5s`, `1.0s`, `2.0s`, `5.0s`
- repeats: `3`
- output mode: summary-only runner

| Solver | Budget | Median runtime (s) | Median frontier | Median first solution (ms) | Median generated | Median expanded | Median RSS (MB) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `LTMOA_array` | `0.5` | `0.503246` | `425` | `1.991` | `198,062` | `114,984` | `203.1` |
| `LTMOA_array_superfast` | `0.5` | `0.502220` | `514` | `1.212` | `260,431` | `152,295` | `179.9` |
| `LTMOA_array` | `1.0` | `1.004143` | `558` | `1.917` | `272,555` | `159,014` | `211.0` |
| `LTMOA_array_superfast` | `1.0` | `1.004275` | `743` | `1.634` | `403,896` | `238,741` | `195.2` |
| `LTMOA_array` | `2.0` | `2.008622` | `804` | `1.947` | `416,071` | `245,602` | `224.7` |
| `LTMOA_array_superfast` | `2.0` | `2.008978` | `1,004` | `1.363` | `638,896` | `383,487` | `220.5` |
| `LTMOA_array` | `5.0` | `5.021557` | `1,050` | `1.808` | `610,416` | `364,952` | `242.2` |
| `LTMOA_array_superfast` | `5.0` | `5.018323` | `1,371` | `1.378` | `1,001,330` | `611,472` | `256.1` |

Reading:

- Runtime stayed effectively flat: `LTMOA_array_superfast` was within
  `-0.06%` to `+0.02%` of `LTMOA_array` on median wall clock.
- Anytime frontier quality improved materially at every budget:
  - `0.5s`: `1.21x`
  - `1.0s`: `1.33x`
  - `2.0s`: `1.25x`
  - `5.0s`: `1.31x`
- First-solution latency also improved at every budget:
  - `0.5s`: `0.61x` of `LTMOA_array`
  - `1.0s`: `0.85x`
  - `2.0s`: `0.70x`
  - `5.0s`: `0.76x`
- PMR did not reduce live-label count. The faster variant explores a larger
  frontier and correspondingly allocates more labels. RSS still improved at
  shorter budgets, but rose by about `5.7%` at `5.0s`.

## Parallel Median Summary

Config:

- `bench/configs/nyc_ltmoa_parallel_pmr.yaml`
- solver: `LTMOA_parallel`
- threads: `2`
- budgets: `0.5s`, `1.0s`, `2.0s`, `5.0s`
- repeats: `5`
- output mode: summary-only runner

New medians:

| Budget | Median runtime (s) | Median frontier | Median first solution (ms) | Median generated | Median expanded | Median peak live | Median RSS (MB) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `0.5` | `0.507521` | `320` | `7.213` | `240,195` | `142,350` | `240,195` | `164.8` |
| `1.0` | `1.009310` | `449` | `7.140` | `372,911` | `224,131` | `372,911` | `176.5` |
| `2.0` | `2.013282` | `618` | `8.099` | `501,480` | `299,330` | `501,480` | `185.4` |
| `5.0` | `5.015613` | `854` | `6.919` | `757,907` | `452,884` | `757,907` | `204.7` |

Comparison against the pre-PMR archive from
`notes/benchmarks/2026-04-12-nyc-parallel-large/nyc_parallel_2thread_5x.csv`:

| Budget | Runtime ratio (new/old) | Frontier ratio (new/old) | First-solution ratio (new/old) | Generated ratio (new/old) | Expanded ratio (new/old) |
| --- | ---: | ---: | ---: | ---: | ---: |
| `0.5` | `0.9975x` | `1.0000x` | `0.9046x` | `1.0193x` | `1.0197x` |
| `1.0` | `0.9998x` | `0.9740x` | `0.9341x` | `1.0686x` | `1.0821x` |
| `2.0` | `1.0000x` | `0.9567x` | `1.0177x` | `0.9903x` | `0.9901x` |
| `5.0` | `0.9995x` | `0.8786x` | `0.8845x` | `0.8933x` | `0.8854x` |

Reading:

- Median runtime stayed essentially unchanged after the allocator rewrite.
- The current `LTMOA_parallel` run does not show a consistent anytime-frontier
  gain. Frontier size was flat at `0.5s`, then `2.6%`, `4.3%`, and `12.1%`
  lower than the earlier archive at `1.0s`, `2.0s`, and `5.0s`.
- First-solution timing improved at `0.5s`, `1.0s`, and `5.0s`, but was
  slightly worse at `2.0s`.
- Peak RSS is now captured in the new summary-only suite and stayed well below
  the serial runs, but there is no matching RSS field in the earlier archive
  for a direct old/new comparison.

## Runner Note

Both suites finished without crash rows:

- array suite: `24/24` rows with `status = timeout`, `exit_code = 0`
- parallel suite: `20/20` rows with `status = timeout`, `exit_code = 0`

The runner currently prints `Completed 0/N runs` for these suites because it
counts only rows marked `completed`, while time-capped solver rows are reported
as `timeout` even when the child exits cleanly and writes canonical summary
metrics. For this repo state, those `timeout` rows are expected anytime outputs,
not failures.

## Command Provenance

```bash
python3 bench/scripts/run_benchmark.py \
  --config bench/configs/nyc_ltmoa_array_pmr.yaml \
  --suite-id nyc_ltmoa_array_pmr_20260412

python3 bench/scripts/run_benchmark.py \
  --config bench/configs/nyc_ltmoa_parallel_pmr.yaml \
  --suite-id nyc_ltmoa_parallel_pmr_20260412
```
