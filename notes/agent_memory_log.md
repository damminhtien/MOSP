# Agent Memory Log

Purpose: preserve repo-specific changes, experiments, and rejected directions so later agents do not repeat the same work blindly.

Recommended entry format:
- Date:
- Task:
- Files touched:
- Outcome:
- Evidence:
- Do not repeat blindly:

## 2026-04-11

### LTMOA_array_superfast hot-path refactor
- Task: replace per-label heap allocation and heavy frontier behavior with arena allocation, packed adaptive frontier storage, exact target frontier cache, and benchmark fast path.
- Files touched: `inc/utils/label_arena.h`, `inc/algorithms/gcl/gcl_fast_array.h`, `inc/algorithms/ltmoa_array_superfast.h`, `src/algorithms/ltmoa_array_superfast.cpp`, `tests/ltmoa_frontier_regression.cpp`, `tests/benchmark_metrics_smoke.cpp`
- Outcome: exactness/regression passed; per-label cost improved, but anytime dominance over `LTMOA_array` did not materialize on the NYC benchmark used in local notes.
- Evidence: `ltmoa_frontier_regression` and `benchmark_metrics_smoke` passed; local benchmark artifacts recorded in `/tmp/ltmoa_superfast_compare.csv` and `/tmp/ltmoa_superfast_afterpass.csv`.
- Do not repeat blindly: optimizing frontier bookkeeping alone did not fix early anytime behavior; first-solution timing and OPEN policy remained the dominant issue.

### LTMOA_parallel owner-sharded implementation
- Task: build `LTMOA_parallel` on the same truncated frontier core with per-thread heaps/arenas and owner-sharded node frontiers.
- Files touched: `inc/algorithms/gcl/gcl_owner_sharded.h`, `inc/algorithms/ltmoa_parallel.h`, `src/algorithms/ltmoa_parallel.cpp`, `src/main.cpp`, `tests/ltmoa_frontier_regression.cpp`, `tests/benchmark_metrics_smoke.cpp`
- Outcome: exactness regression passed; local NYC smoke ran successfully; later pass added real work stealing with owner-shard leases instead of a stub.
- Evidence: `ltmoa_frontier_regression` and `benchmark_metrics_smoke` passed; local run summary previously captured in `/tmp/ltmoa_parallel_bench.csv`.
- Do not repeat blindly: do not reintroduce global-PQ or writer-lock relaxed frontier designs; the owner-sharded frontier was chosen specifically to avoid those costs.

### LTMOA_array_superfast_lb experiment
- Task: test stronger lower-bound and warm-start ideas via `LTMOA_array_superfast_lb` using reverse-SP + landmark ALT, query-specific scalar warm starts, and scalarized OPEN ordering.
- Files touched: `inc/problem/lower_bound_oracle.h`, `src/problem/lower_bound_oracle.cpp`, `inc/algorithms/ltmoa_array_superfast_lb.h`, `src/algorithms/ltmoa_array_superfast_lb.cpp`, tests and main integration files
- Outcome: exactness held, but anytime quality collapsed badly on NYC. Larger timeouts did not rescue it.
- Evidence: local benchmark artifacts in `/tmp/ltmoa_superfast_lb_compare.csv`; paper note summary in `notes/paper_note_ltmoa_fast_parallel_2026-04-10.md`.
- Do not repeat blindly: ALT-style componentwise lower bounds were not the limiting factor on this dataset, and global scalarized OPEN ordering pushed the search toward poor compromise regions.

### LTMOA_array_superfast_anytime experiment
- Task: create an incumbent-first variant with deferred seeding and a hard incumbent-aware OPEN phase switch.
- Files touched: `inc/algorithms/ltmoa_array_superfast_anytime.h`, `src/algorithms/ltmoa_array_superfast_anytime.cpp`, `src/main.cpp`, tests and docs
- Outcome: slight improvement in time-to-first-solution, but final anytime frontier size collapsed severely under timeout.
- Evidence: local benchmark artifact `/tmp/ltmoa_anytime_compare.csv`.
- Do not repeat blindly: scoring against `target_component_min_` and migrating the whole heap after the first incumbent was too aggressive and starved skyline coverage.

### LTMOA_array_superfast deferred-seed + mixed-side-heap experiment
- Task: modify `LTMOA_array_superfast` directly to remove upfront warm start, add a deferred seed burst, and add a skyline-aware advisory side heap with 7:1 lex/anytime cadence.
- Files touched: `inc/algorithms/ltmoa_array_superfast.h`, `src/algorithms/ltmoa_array_superfast.cpp`, `tests/ltmoa_frontier_regression.cpp`
- Outcome: exactness regression passed, but NYC anytime performance regressed badly versus the previous `LTMOA_array_superfast`.
- Evidence: `ltmoa_frontier_regression`, `benchmark_metrics_smoke`, and `ctest` passed; current benchmark artifact `/tmp/ltmoa_superfast_mixed_compare.csv`; trace artifact `/tmp/ltmoa_superfast_trace/LTMOA_array_superfast_3obj_-fast_array__NY-d__NY-t__NY-m__single_88959_96072.csv`.
- Do not repeat blindly:
  - deferred seed did not produce the first incumbent on the NYC query used for evaluation
  - removing upfront warm-start without a stronger replacement worsened `time_to_first_solution_sec`
  - even advisory side-heap scheduling against exact incumbent skyline still narrowed frontier coverage too much under timeout

### Benchmark summary-only fast path
- Task: cut canonical benchmark overhead when only `--summary-output` is requested.
- Files touched: `inc/utils/benchmark_metrics.h`, `src/utils/benchmark_metrics.cpp`, `tests/benchmark_metrics_smoke.cpp`
- Outcome: summary-only runs no longer keep full trace snapshots or annotate HV/recall; the benchmark schema stayed unchanged.
- Evidence: `benchmark_metrics_smoke` passed; summary-only artifacts were used in multiple local benchmark sweeps.
- Do not repeat blindly: summary-only mode now expects `anytime_trace` to be empty unless a trace artifact path is configured.

## 2026-04-11 Experiment History Backfill

### NYC anytime baseline sweep: `LTMOA_array` vs `LTMOA_array_superfast`
- Task: compare the serial baseline and the first `superfast` serial core on the NYC 3-objective query `88959 -> 96072`.
- Files touched: none beyond existing solver/benchmark integration; this was a measurement run.
- Outcome: `LTMOA_array_superfast` improved work efficiency per label, but did not beat the baseline on early anytime quality.
- Evidence:
  - `/tmp/ltmoa_superfast_afterpass.csv`
  - earlier local notes and `notes/paper_note_ltmoa_fast_parallel_2026-04-10.md`
  - representative numbers from the summary-only sweep:
    - `0.25s`: baseline frontier `128`, first solution `1.42ms`; `superfast` frontier `209`, first solution `3.83ms`
    - `0.5s`: baseline frontier `206`, first solution `1.42ms`; `superfast` frontier `316`, first solution `6.24ms`
    - `1.0s`: baseline frontier `285`, first solution `1.40ms`; `superfast` frontier `437`, first solution `6.24ms`
- Do not repeat blindly: “better runtime per generated/expanded label” did not automatically mean “better anytime behavior”; keep both signals separate in future evaluations.

### Parallel local comparison: `LTMOA_parallel` vs `SOPMOA_relaxed`
- Task: compare the new owner-sharded parallel core against `SOPMOA_relaxed` on the NYC 3-objective query under a short timeout.
- Files touched: none beyond solver/benchmark code already recorded above; this was a measurement run.
- Outcome: `LTMOA_parallel` showed the most promising early result among the new solvers, with better first-solution timing and larger anytime frontiers than `SOPMOA_relaxed` at similar runtime.
- Evidence:
  - `notes/paper_note_ltmoa_fast_parallel_2026-04-10.md`
  - prior local artifact `/tmp/ltmoa_parallel_bench.csv`
  - median `0.5s` comparison already recorded in the paper note:
    - `SOPMOA_relaxed`: runtime `0.506s`, frontier `58`, first solution `78.178ms`
    - `LTMOA_parallel`: runtime `0.518s`, frontier `80`, first solution `19.280ms`
- Do not repeat blindly: this was still a timeout-limited anytime result, not a completed exact real-map comparison.

### Larger-timeout check for `LTMOA_array_superfast_lb`
- Task: test whether the poor anytime behavior of `LTMOA_array_superfast_lb` improves when the timeout is increased.
- Files touched: none beyond existing solver/benchmark integration; this was a measurement run and analysis pass.
- Outcome: larger timeouts did not rescue the solver. It continued to expand heavily while producing very few target frontier points.
- Evidence:
  - `notes/paper_note_ltmoa_fast_parallel_2026-04-10.md`
  - `/tmp/ltmoa_superfast_lb_compare.csv`
  - recorded representative results:
    - `1s`: `LTMOA_array_superfast_lb` frontier `64`, first solution `40.7ms`
    - `2s`: legacy run produced `94` solutions with `366,893` expansions
    - `5s`: legacy run produced `110` solutions with `673,243` expansions
- Do not repeat blindly: if a future lower-bound experiment revisits this space, it needs a materially different OPEN policy or dataset motivation, not just “more timeout”.

### Hard-phase-switch anytime experiment: `LTMOA_array_superfast_anytime`
- Task: test whether deferred seeding plus a full incumbent-aware OPEN phase switch improves early anytime behavior.
- Files touched: `inc/algorithms/ltmoa_array_superfast_anytime.h`, `src/algorithms/ltmoa_array_superfast_anytime.cpp`, `tests/ltmoa_frontier_regression.cpp`, `tests/benchmark_metrics_smoke.cpp`, docs and CLI integration files.
- Outcome: first solution improved slightly, but frontier coverage collapsed so severely that the solver was not viable as the main anytime variant.
- Evidence:
  - `/tmp/ltmoa_anytime_compare.csv`
  - representative results:
    - `0.25s`: `superfast_anytime` frontier `5`, first solution `3.62ms`
    - `0.5s`: frontier `5`, first solution `3.86ms`
    - `1.0s`: frontier `8`
- Do not repeat blindly: a hard full-heap switch after the first incumbent is too aggressive on the current NYC query.

### Direct in-place scheduling experiment on `LTMOA_array_superfast`
- Task: test whether removing upfront warm-start and replacing it with deferred seeding plus a mixed skyline-aware side heap improves the main `superfast` baseline.
- Files touched: `inc/algorithms/ltmoa_array_superfast.h`, `src/algorithms/ltmoa_array_superfast.cpp`, `tests/ltmoa_frontier_regression.cpp`
- Outcome: exactness stayed intact, but NYC anytime quality regressed sharply and first-solution timing also got worse than the earlier `superfast` baseline.
- Evidence:
  - `/tmp/ltmoa_superfast_mixed_compare.csv`
  - `/tmp/ltmoa_superfast_trace/LTMOA_array_superfast_3obj_-fast_array__NY-d__NY-t__NY-m__single_88959_96072.csv`
  - representative results:
    - `0.25s`: baseline frontier `145`; earlier `superfast` frontier `209`; mixed-schedule `superfast` frontier `101`
    - `0.25s` first solution: baseline `1.203ms`; earlier `superfast` `3.835ms`; mixed-schedule `superfast` `5.747ms`
    - `0.5s`: earlier `superfast` frontier `316`; mixed-schedule `superfast` frontier `174`
- Do not repeat blindly:
  - deferred seed did not trigger the first incumbent on the measured NYC query
  - removing upfront warm-start without proving seed usefulness was a regression
  - a second advisory heap can still over-bias the search, even without a hard full-heap migration

### Algorithm audit / evaluation caveat pass
- Task: review the implemented algorithms for sources of incorrectness or misleading evaluation numbers.
- Files touched: read-only analysis across solver and benchmark files.
- Outcome: no immediate exactness bug was found in the newer LTMOA-family variants, but several evaluation caveats were identified and should be remembered.
- Evidence:
  - analysis notes captured in conversation and reflected in `notes/paper_note_ltmoa_fast_parallel_2026-04-10.md`
  - key findings retained for future runs:
    - `SOPMOA` legacy non-benchmark mode had a target-frontier race on shared frontier updates
    - target-pruning semantics differ between `LTMOA_array` and later `superfast` / `parallel` variants, so generated/expanded counts are not perfectly apples-to-apples
    - parallel solver frontier timestamps and summary timing can use different clock origins if mixed carelessly
- Do not repeat blindly: do not compare anytime metrics across solvers as if they were strictly identical algorithms unless the target-pruning semantics and timing source are aligned first.

### Repo-local benchmark archive recovery
- Task: move important benchmark evidence out of volatile `/tmp` storage into repo-local notes, prioritizing large repeated NYC studies.
- Files touched: `notes/benchmarks/2026-04-11-nyc-reliable/nyc_serial_core_3x.csv`, `notes/benchmarks/2026-04-11-nyc-reliable/nyc_parallel_relaxed_3x.csv`, `notes/benchmarks/2026-04-11-nyc-reliable/README.md`, `notes/benchmarks/2026-04-11-nyc-reliable/historical_summary_from_notes.md`
- Outcome: the old `/tmp` raw files were already gone, so the large reliable studies were rerun and archived directly into the repo; older important numbers were preserved as a reconstructed summary from notes.
- Evidence:
  - raw repo-local CSVs now exist under `notes/benchmarks/2026-04-11-nyc-reliable/`
  - serial study: `LTMOA_array` vs current `LTMOA_array_superfast`, 3 repeats, budgets `0.25/0.5/1.0`
  - parallel study: `SOPMOA_relaxed` vs `LTMOA_parallel`, 3 repeats, budgets `0.5/1.0`
- Do not repeat blindly:
  - do not rely on `/tmp` as the only home for important benchmark evidence
  - clearly separate raw rerun artifacts from reconstructed historical summaries when older raw CSVs are already lost
