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

### Merge conflict resolution pass
- Task: resolve the active merge conflicts and re-stabilize the benchmark/test harness around the newer canonical CLI flow while preserving the newer LTMOA-family solvers.
- Files touched: `AGENTS.md`, `ARCHITECTURE.md`, `CMakeLists.txt`, `README.md`, `inc/algorithms/abstract_solver.h`, `inc/algorithms/gcl/gcl_array.h`, `inc/algorithms/gcl/gcl_tree.h`, `inc/utils/thread_config.h`, `src/main.cpp`, `src/utils/benchmark_metrics.cpp`, `tests/benchmark_metrics_smoke.cpp`, `tests/thread_config_smoke.cpp`, `tests/cli_validation_smoke.py`, `tests/correctness_harness.cpp`
- Outcome: all merge markers and `UU` state were removed; the merged tree now builds cleanly and passes `thread_config_smoke`, `benchmark_metrics_smoke`, `ltmoa_frontier_regression`, `correctness_harness`, and `cli_validation_smoke`.
- Evidence:
  - `rg -n "^(<<<<<<<|=======|>>>>>>>)" /workspaces/SOPMOA --glob '!build-codex/**'` returned no matches
  - `git diff --name-only --diff-filter=U` returned no files
  - `ctest --test-dir build-codex --output-on-failure -R 'thread_config_smoke|benchmark_metrics_smoke|ltmoa_frontier_regression|correctness_harness|cli_validation_smoke'` passed
- Do not repeat blindly:
  - `SOPMOA` thread validation now intentionally caps requested `13` threads down to `12`; older CLI smoke expectations are stale
  - `SOPMOA_relaxed_t4` in `correctness_harness` is timing-sensitive under `ctest` capture, so exact-frontier equality should not be reintroduced there without first fixing the underlying solver nondeterminism

## 2026-04-12

### Repo-local skill install: `mosp-cpp-performance-engineer`
- Task: install the user-provided `skill.zip` into this repository as a repo-local skill instead of depending on the machine-global copy.
- Files touched: `skills/mosp-cpp-performance-engineer/SKILL.md`, `skills/mosp-cpp-performance-engineer/references/benchmarking-playbook.md`, `skills/mosp-cpp-performance-engineer/references/report-template.md`, `skills/mosp-cpp-performance-engineer/agents/openai.yaml`
- Outcome: the skill was vendored into the repo under `skills/mosp-cpp-performance-engineer/`; a global copy already existed under `~/.codex/skills`, but the repo now carries its own copy.
- Evidence:
  - source archive: `/Users/macbook/Downloads/skill.zip`
  - installed files present under `skills/mosp-cpp-performance-engineer/`
- Do not repeat blindly:
  - do not assume the repo-local copy and the global `~/.codex/skills` copy will stay in sync automatically
  - prefer updating one source of truth intentionally if the skill content changes later

### Larger NYC parallel benchmark rerun
- Task: read the archived NYC notes, rerun a larger `SOPMOA_relaxed` vs `LTMOA_parallel` benchmark on the same `88959 -> 96072` query, and preserve the results in repo-local notes.
- Files touched: `bench/manifests/query_sets/nyc_88959_96072.json`, `bench/configs/nyc_parallel_timecap_large.yaml`, `notes/benchmarks/2026-04-12-nyc-parallel-large/nyc_parallel_2thread_5x.csv`, `notes/benchmarks/2026-04-12-nyc-parallel-large/README.md`
- Outcome: the stable summary-only rerun completed `5` repeats at budgets `0.5/1.0/2.0/5.0` and showed a much stronger anytime frontier advantage for `LTMOA_parallel` than the 2026-04-11 archive on the same query, while runtime stayed near parity and first-solution timing remained slightly worse for `LTMOA_parallel`.
- Evidence:
  - verification passed before the sweep:
    - `ctest --test-dir Release --output-on-failure -R 'benchmark_metrics_smoke|correctness_harness'`
    - `./Release/ltmoa_frontier_regression`
  - raw repo-local CSV: `notes/benchmarks/2026-04-12-nyc-parallel-large/nyc_parallel_2thread_5x.csv`
  - median frontier sizes:
    - `0.5s`: `SOPMOA_relaxed=243`, `LTMOA_parallel=320`
    - `1.0s`: `SOPMOA_relaxed=306`, `LTMOA_parallel=461`
    - `2.0s`: `SOPMOA_relaxed=376`, `LTMOA_parallel=646`
    - `5.0s`: `SOPMOA_relaxed=511`, `LTMOA_parallel=972`
- Do not repeat blindly:
  - do not compare these numbers to the 2026-04-11 archive without stating the date and code-state difference explicitly
  - do not treat the config-runner path as stable evidence for `SOPMOA_relaxed` on this query until the post-run hang is fixed

### Config-runner instability on `SOPMOA_relaxed`
- Task: try the same larger NYC comparison through the manifest/config benchmark runner before falling back to manual commands.
- Files touched: `bench/results/nyc_parallel_timecap_large_20260412/summary.csv`, `bench/results/nyc_parallel_timecap_large_20260412/run_results.json`, `bench/results/nyc_parallel_timecap_large_20260412/resolved_config.json`
- Outcome: the run had to be interrupted because `SOPMOA_relaxed` intermittently printed its end-of-run stats and then failed to return control to the Python runner, which left placeholder wall-timeout rows with `exit_code = -15`.
- Evidence:
  - partial suite summary exists under `bench/results/nyc_parallel_timecap_large_20260412/summary.csv`
  - interrupted partial status count before fallback:
    - `SOPMOA_relaxed`, `0.5s`, `timeout`, `exit_code=-15`: `3`
    - `SOPMOA_relaxed`, `0.5s`, `timeout`, `exit_code=0`: `2`
    - `SOPMOA_relaxed`, `1.0s`, `timeout`, `exit_code=-15`: `3`
- Do not repeat blindly:
  - do not assume the benchmark runner’s wall-timeout placeholders are measuring solver search time here; some rows reflect an exit/handover hang after normal solver stdout
  - use the stable summary-only CLI loop for this exact query until the runner interaction is debugged

### PMR allocator pass for LTMOA solvers
- Task: replace hot per-label and scratch allocations in `LTMOA_array_superfast` and `LTMOA_parallel` with PMR-backed arenas and pools, then rerun focused NYC benchmarks.
- Files touched: `inc/utils/label_arena.h`, `inc/algorithms/ltmoa_array_superfast.h`, `src/algorithms/ltmoa_array_superfast.cpp`, `inc/algorithms/ltmoa_parallel.h`, `src/algorithms/ltmoa_parallel.cpp`, `bench/configs/nyc_ltmoa_array_pmr.yaml`, `bench/configs/nyc_ltmoa_parallel_pmr.yaml`, `notes/benchmarks/2026-04-12-pmr-allocators/README.md`, `notes/benchmarks/2026-04-12-pmr-allocators/nyc_ltmoa_array_pmr_3x_summary.csv`, `notes/benchmarks/2026-04-12-pmr-allocators/nyc_ltmoa_parallel_pmr_5x_summary.csv`
- Outcome: `LTMOA_array_superfast` now allocates labels from a per-solve monotonic arena and uses PMR scratch containers; `LTMOA_parallel` now uses private PMR pools per worker and a synchronized PMR pool for published snapshots. The post-change benchmark kept wall-clock runtime flat while improving the serial anytime frontier materially, but the parallel anytime frontier did not improve relative to the earlier 2-thread archive. In practice this was a real improvement for the serial solver, but not a clear performance win for the parallel solver on the archived NYC query.
- Evidence:
  - verification passed after the patch:
    - `cmake --build Release -j4`
    - `ctest --test-dir Release --output-on-failure -R 'benchmark_metrics_smoke|correctness_harness'`
    - `./Release/ltmoa_frontier_regression`
  - serial medians from `notes/benchmarks/2026-04-12-pmr-allocators/nyc_ltmoa_array_pmr_3x_summary.csv`:
    - `0.5s`: frontier `425 -> 514`, first solution `1.991ms -> 1.212ms`
    - `1.0s`: frontier `558 -> 743`, first solution `1.917ms -> 1.634ms`
    - `2.0s`: frontier `804 -> 1004`, first solution `1.947ms -> 1.363ms`
    - `5.0s`: frontier `1050 -> 1371`, first solution `1.808ms -> 1.378ms`
  - parallel medians from `notes/benchmarks/2026-04-12-pmr-allocators/nyc_ltmoa_parallel_pmr_5x_summary.csv` compared to `notes/benchmarks/2026-04-12-nyc-parallel-large/nyc_parallel_2thread_5x.csv`:
    - runtime stayed within `0.25%` of the earlier archive at every budget
    - frontier ratio new/old: `1.000x`, `0.974x`, `0.957x`, `0.879x`
  - serial runtime ratio `LTMOA_array_superfast / LTMOA_array` stayed near `1.0x`:
    - `0.5s`: `0.9980x`
    - `1.0s`: `1.0001x`
    - `2.0s`: `1.0002x`
    - `5.0s`: `0.9994x`
  - serial RSS ratio `LTMOA_array_superfast / LTMOA_array`:
    - `0.5s`: `0.8858x`
    - `1.0s`: `0.9250x`
    - `2.0s`: `0.9813x`
    - `5.0s`: `1.0572x`
  - parallel runtime ratio new/old stayed near `1.0x`:
    - `0.5s`: `0.9975x`
    - `1.0s`: `0.9998x`
    - `2.0s`: `1.0000x`
    - `5.0s`: `0.9995x`
  - the old `LTMOA_parallel` archive does not expose `peak_rss_mb`, so there is no direct before/after RSS baseline for the parallel solver
- Do not repeat blindly:
  - do not read the runner banner `Completed 0/N runs` as a failure for time-capped suites; these rows were canonical `status=timeout`, `exit_code=0` anytime outputs
  - do not assume allocator changes alone will improve `LTMOA_parallel` anytime quality; the current evidence says they mostly stabilized memory behavior without raising the frontier on this query

### PMR before/after findings persisted into notes
- Task: make the PMR allocator conclusions easier to recover later by writing the practical memory/runtime interpretation into the benchmark archive, repo memory, and roadmap.
- Files touched: `notes/benchmarks/2026-04-12-pmr-allocators/README.md`, `notes/agent_memory_log.md`, `notes/roadmap.md`
- Outcome: the repo notes now explicitly preserve that PMR was a real before/after improvement for `LTMOA_array_superfast`, while `LTMOA_parallel` stayed runtime-neutral and cannot claim a measured memory win against the older archive.
- Evidence:
  - benchmark note now includes concrete runtime, RSS, frontier, and first-solution ratios near the top
  - the PMR experiment memory entry now records the serial RSS ratios `0.8858x`, `0.9250x`, `0.9813x`, `1.0572x`
  - the roadmap now records that PMR alone should not be assumed to be a sufficient optimization direction for `LTMOA_parallel`
- Do not repeat blindly:
  - do not force future readers to reconstruct the PMR outcome from raw CSVs when the practical takeaway can be preserved directly in notes
  - do not describe the parallel PMR pass as a memory improvement without a matching old/new RSS baseline

### MOSP / MOA* paper checklist archived as a roadmap note
- Task: preserve the research-ordered MOSP / MOA* paper checklist as a repo note and connect it to the forward roadmap so papers can be turned into implementation work gradually.
- Files touched: `notes/paper_reading_roadmap_mosp_moa.md`, `notes/roadmap.md`
- Outcome: the literature checklist now exists as a standalone note grouped by exact-core, survey/theory, OR-nearby, engineering, approximate, dynamic, and background papers, with a short recommended reading sequence for future implementation passes.
- Evidence:
  - canonical note: `notes/paper_reading_roadmap_mosp_moa.md`
  - roadmap entry now points to that note as the phased literature-to-implementation backlog
- Do not repeat blindly:
  - do not leave large reading lists only in transient chat history when they are intended to guide future repo work
  - do not mix exact-core MOSP papers with approximate or MAPF extensions when deciding the next solver implementation target

### Runner clean-timeout accounting and `SOPMOA_relaxed` finalization stabilization
- Task: fix the benchmark runner so clean time-capped suites are not reported as `0/N completed`, and finish the previously left-dirty `SOPMOA_relaxed` / `benchmark_metrics` changes that were intended to avoid expensive post-run trace finalization.
- Files touched: `bench/scripts/run_benchmark.py`, `bench/README.md`, `tests/benchmark_runner_smoke.py`, `tests/benchmark_metrics_smoke.cpp`, `CMakeLists.txt`, `inc/algorithms/sopmoa_relaxed.h`, `src/algorithms/sopmoa_relaxed.cpp`, `inc/utils/benchmark_metrics.h`, `src/utils/benchmark_metrics.cpp`, `notes/agent_memory_log.md`, `notes/roadmap.md`
- Outcome: the runner now reports clean solver-side budget timeouts as successful runs and breaks out runner-driven timeouts separately; `SOPMOA_relaxed` now keeps a private exact target frontier for trace/finalization and can emit counters-only anytime events when full snapshot capture is intentionally skipped. The previously untouched `SOPMOA_relaxed` / `benchmark_metrics` work is now integrated and validated instead of being left as unrelated dirty code.
- Evidence:
  - targeted tests passed after the patch:
    - `ctest --test-dir Release --output-on-failure -R 'benchmark_metrics_smoke|correctness_harness|ltmoa_frontier_regression|cli_validation_smoke|benchmark_runner_smoke'`
  - runner smoke now expects:
    - `Successful runs: 1/1`
    - `Run status counts: completed=0, timed_out_cleanly=1, skipped=0, crash=0`
  - direct probe of the formerly problematic path succeeded:
    - config-driven `SOPMOA_relaxed`, `2` threads, query `88959 -> 96072`, budget `0.5s`, `export_frontier=true`, `export_trace=true`, `trace_interval_ms=0`
    - probe result: `returncode=0`, `process_timed_out=false`, `used_solver_summary=true`, suite status `timeout`
  - runner robustness also improved for absolute config paths outside the repo by falling back to an absolute `config_path` in `resolved_config.json`
- Do not repeat blindly:
  - do not treat every suite-level `timeout` as a runner failure; clean solver-side budget timeouts are now a first-class successful outcome in runner console summaries
  - do not reintroduce full frontier snapshot capture on every `SOPMOA_relaxed` target acceptance when `trace_interval_ms=0`; that was the expensive path behind the earlier handoff/finalization issue
