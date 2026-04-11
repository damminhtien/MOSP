# Evaluation Note: `LTMOA_array_superfast` and `LTMOA_parallel`

Date: 2026-04-10

## Scope

This note summarizes the first local evaluation pass for the two newly
implemented solvers:

- `LTMOA_array_superfast`
- `LTMOA_parallel`

The goal is not to claim publication-grade results yet, but to capture
what is already defensible for a paper draft: correctness status,
preliminary performance signals, and the main caveats that should be
stated explicitly.

## Environment

- Host: `Linux 6.8.0-1044-azure x86_64`
- CPU count visible to the process: `4`
- Binary: `./build-codex/main`
- Build/test status at the time of writing:
  - `cmake --build /workspaces/SOPMOA/build-codex -j2 --target ltmoa_frontier_regression benchmark_metrics_smoke main`
  - `ctest --test-dir /workspaces/SOPMOA/build-codex --output-on-failure -R 'ltmoa_frontier_regression'`
  - both succeeded locally

## Correctness Status

The strongest correctness evidence currently available is the regression
test target [ltmoa_frontier_regression.cpp](/workspaces/SOPMOA/tests/ltmoa_frontier_regression.cpp).

It checks:

- `LabelArena` rollover and reuse behavior
- `FastNodeFrontier` dominance rejection, swap-remove pruning, and growth
- `LTMOA_array_superfast == LTMOA_array` on synthetic 2/3/4-objective graphs
- `LTMOA_parallel == LTMOA_array` on the same synthetic graphs for `1` and `2` threads

Result:

- `ltmoa_frontier_regression`: passed

Interpretation:

- We have exactness evidence for the new frontier core and both new
  solvers on controlled complete cases.
- We do not yet have a large real-map completed exactness benchmark; the
  NYC experiments below are timeout-limited anytime runs.

## Benchmark Setup

Local comparison was run on the NYC 3-objective maps used elsewhere in
the repo:

- maps: `maps/NY-d.txt maps/NY-t.txt maps/NY-m.txt`
- query: `start=88959`, `target=96072`
- budget: `--budget-sec 0.5`
- repetitions: `3` per solver
- serial solvers: `1` thread effective
- parallel solvers: `2` threads effective
- raw canonical CSV: `/tmp/paper-eval/summary.csv`

Important caveat:

- Every run in this table hit the `0.5s` budget and ended with status
  `timeout`.
- Therefore `final_frontier_size` is an anytime frontier size, not the
  exact completed Pareto frontier cardinality.

## Median Results Over 3 Runs

| Solver | Threads | Median runtime (s) | Median generated | Median expanded | Median final frontier | Median time to first solution (s) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `LTMOA_array` baseline (`LTMOA(3obj)-array` in CSV) | 1 | 0.502240 | 76,415 | 44,293 | 201 | 0.001403 |
| `LTMOA_array_superfast` | 1 | 0.502186 | 68,324 | 39,557 | 183 | 0.003793 |
| `SOPMOA_relaxed` | 2 | 0.506090 | 39,103 | 23,300 | 58 | 0.078178 |
| `LTMOA_parallel` | 2 | 0.517784 | 62,814 | 38,058 | 80 | 0.019280 |

## Headline Comparisons

### 1. `LTMOA_array_superfast` vs `LTMOA_array`

Median change relative to `LTMOA_array`:

- runtime: `-0.01%`
- generated labels: `-10.59%`
- expanded labels: `-10.69%`
- anytime frontier size at `0.5s`: `-8.96%`
- time to first solution: `+170.34%`

Takeaway:

- The new serial core clearly reduces search work volume in the same
  time budget.
- That gain does not yet translate into a visibly better anytime
  frontier on this short timeout setting.
- In other words, `LTMOA_array_superfast` currently looks like a better
  memory/locality/work-efficiency baseline than a clearly stronger
  anytime solver on this query.

Likely interpretation:

- The arena allocator and packed adaptive frontier remove major hot-path
  overheads.
- However, on a budget-limited run, the changed pruning/order dynamics
  can still delay early target hits enough that frontier-size-at-time
  does not improve.

### 2. `LTMOA_parallel` vs `SOPMOA_relaxed`

Median change relative to `SOPMOA_relaxed` at `2` threads:

- runtime: `+2.31%`
- generated labels: `+60.64%`
- expanded labels: `+63.34%`
- anytime frontier size at `0.5s`: `+37.93%`
- time to first solution: `-75.34%`

Takeaway:

- This is the strongest early result from the new implementation pass.
- `LTMOA_parallel` reaches the first solution much earlier than
  `SOPMOA_relaxed`, explores substantially more labels within nearly the
  same wall-clock budget, and returns a larger anytime frontier on this
  query.
- That supports the intended design choice: owner-sharded truncated
  frontiers plus local heaps are promising as a lighter-weight parallel
  exact core than the current relaxed frontier design.

## What Is Safe To Say In A Paper Draft

Reasonably safe wording today:

- "`LTMOA_array_superfast` preserves exactness on synthetic complete
  instances while removing the major memory-allocation and frontier
  overheads of the original array baseline."
- "In preliminary NYC timeout-limited experiments, the superfast serial
  baseline reduced generated and expanded labels by about 10% at the
  same 0.5-second budget."
- "`LTMOA_parallel` preserved exactness on synthetic complete instances
  for 1- and 2-thread runs."
- "In preliminary 2-thread NYC anytime experiments, `LTMOA_parallel`
  produced earlier first solutions and larger partial frontiers than
  `SOPMOA_relaxed` under the same 0.5-second budget."

What should still be phrased carefully:

- Do not claim completed-map exactness from the NYC table; those runs
  all timed out.
- Do not claim strong scalability yet; the current exactness regression
  only covers `1` and `2` threads.
- Do not claim work stealing benefits for `LTMOA_parallel` yet; the
  current implementation still has a stubbed steal path.

## Recommended Next Experiments Before Freezing Paper Numbers

1. Add completed benchmarks on a smaller real-map query set, so final
   frontier equality can be shown on non-synthetic instances.
2. Run a scaling sweep for `LTMOA_parallel` at `1`, `2`, and `4`
   threads.
3. Measure memory usage / RSS explicitly; the packed frontier and arena
   design should show its biggest advantage there.
4. Re-run the NYC study with a longer budget and at least `5` repeats,
   reporting medians.
5. Implement real work stealing before making strong claims about load
   balancing or tail latency.

## Bottom Line

Current status is encouraging but asymmetric:

- `LTMOA_array_superfast` is a cleaner and leaner serial baseline, but
  the current short-budget benchmark does not yet show a dramatic wall
  clock gain.
- `LTMOA_parallel` already shows a promising anytime advantage against
  `SOPMOA_relaxed` in the first local benchmark pass, while keeping
  exactness on the synthetic regression suite.

If the paper needs one strong provisional message right now, it is this:

- the serial core refactor is technically sound and reduces work, and
- the parallelized LTMOA-style core is promising enough to justify a
  fuller benchmark campaign.
