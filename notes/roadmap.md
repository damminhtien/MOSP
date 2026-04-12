# Roadmap

Purpose: keep a short forward-looking sequence of ideas and follow-up tasks after each meaningful change or experiment.

Status guide:
- `next`: good candidate for the next implementation pass
- `investigate`: needs measurement or design work before implementation
- `hold`: plausible, but blocked by stronger priorities or unclear value
- `do-not-repeat`: already tried or should not be revisited without new evidence

Recommended entry format:
- Date:
- Context:
- Status:
- Idea:
- Why:
- Evidence / related notes:

## 2026-04-11

### Stabilize `LTMOA_array_superfast` as the main baseline
- Status: `next`
- Idea: restore or keep the strongest known `LTMOA_array_superfast` baseline and avoid folding experimental scheduling changes directly into it unless they pass the NYC anytime gate.
- Why: the direct deferred-seed + mixed-side-heap pass kept exactness but regressed both frontier size and first-solution timing on the current benchmark query.
- Evidence / related notes: `notes/agent_memory_log.md`, `/tmp/ltmoa_superfast_mixed_compare.csv`, `/tmp/ltmoa_superfast_afterpass.csv`

### Make seed behavior observable before changing policy again
- Status: `next`
- Idea: add explicit benchmark/debug counters for seed attempts, seed accepts, first-incumbent source, and whether seed fired before or after the first natural target hit.
- Why: recent passes changed seeding policy several times, but the current metrics do not make seed usefulness obvious enough.
- Evidence / related notes: `notes/agent_memory_log.md`; current traces only show target-frontier triggers, not a compact seed summary.

### Find the real first-incumbent path on NYC
- Status: `investigate`
- Idea: instrument one benchmark query to capture the label/edge path characteristics that lead to the first accepted target in `LTMOA_array` and the best `LTMOA_array_superfast` baseline.
- Why: current attempts have optimized around hypotheses about early anytime behavior, but the actual first-incumbent path structure is still not written down.
- Evidence / related notes: `notes/paper_note_ltmoa_fast_parallel_2026-04-10.md`

### Try lighter incumbent guidance instead of a second full heap
- Status: `investigate`
- Idea: if scheduling is revisited, prefer a very light advisory mechanism such as periodic biased pops, bounded sampled rescoring, or a tiny side queue fed only by labels near current incumbents.
- Why: both the hard phase switch in `LTMOA_array_superfast_anytime` and the advisory side heap inside `LTMOA_array_superfast` narrowed skyline coverage too aggressively under timeout.
- Evidence / related notes: `notes/agent_memory_log.md`, `/tmp/ltmoa_anytime_compare.csv`, `/tmp/ltmoa_superfast_mixed_compare.csv`

### Keep `LTMOA_parallel` evaluation separate from serial scheduling experiments
- Status: `hold`
- Idea: avoid mixing serial anytime-policy changes into the parallel solver until the serial baseline is stable again.
- Why: `LTMOA_parallel` should inherit a strong serial core, not a moving target.
- Evidence / related notes: `notes/agent_memory_log.md`

### Avoid repeating the ALT/global-scalar OPEN direction without new evidence
- Status: `do-not-repeat`
- Idea: do not revisit the `LTMOA_array_superfast_lb` design as the next default direction unless a new dataset or proof suggests the lower-bound quality is truly the limiting factor.
- Why: exactness held, but anytime behavior collapsed and did not recover with larger timeouts on the current NYC query.
- Evidence / related notes: `notes/agent_memory_log.md`, `notes/paper_note_ltmoa_fast_parallel_2026-04-10.md`, `/tmp/ltmoa_superfast_lb_compare.csv`

### Keep experiment history current after every benchmark pass
- Status: `next`
- Idea: whenever a new benchmark sweep or solver experiment is run, append both the outcome and the key numbers to `notes/agent_memory_log.md` immediately.
- Why: a large part of recent iteration cost came from reconstructing what had already been tried and what had already failed.
- Evidence / related notes: `notes/agent_memory_log.md`

### Investigate `SOPMOA_relaxed_t4` correctness-harness nondeterminism
- Status: `investigate`
- Idea: reproduce and fix the exact-frontier mismatch for `SOPMOA_relaxed_t4` on the anti-correlated hard synthetic case under `ctest` output capture, then restore a stronger correctness assertion if the solver can be made stable.
- Why: the merged harness is green again, but one assertion had to be relaxed because the 4-thread relaxed solver is timing-sensitive in the current implementation.
- Evidence / related notes: `notes/agent_memory_log.md`, `tests/correctness_harness.cpp`

## 2026-04-12

### Turn the MOSP / MOA* paper checklist into a phased implementation backlog
- Status: `next`
- Idea: use `notes/paper_reading_roadmap_mosp_moa.md` as the canonical reading roadmap, with explicit paper IDs, per-paper status fields, and `Repo hook` slugs that can be attached to issues, benchmarks, and implementation passes one cluster at a time.
- Why: the literature list is now large enough that ad hoc reading will waste time; the repo needs a stable “read first, implement later” sequence aligned with the actual MOSP research arc, plus a mapping layer from papers to concrete implementation work.
- Evidence / related notes: `notes/paper_reading_roadmap_mosp_moa.md`, `skills/mosp-cpp-performance-engineer/SKILL.md`

### Keep paper tracking structured instead of relying on checkboxes alone
- Status: `next`
- Idea: whenever a paper is touched, update its status code in `notes/paper_reading_roadmap_mosp_moa.md` and reuse its `Repo hook` slug in the matching benchmark note, implementation branch, or memory-log entry.
- Why: plain checkboxes are too weak once multiple paper clusters are active; the repo needs a lightweight but explicit bridge between reading progress and implementation work.
- Evidence / related notes: `notes/paper_reading_roadmap_mosp_moa.md`, `notes/agent_memory_log.md`

### Decide whether repo-local skills should become the preferred project convention
- Status: `hold`
- Idea: if more skills are vendored into this repository, standardize whether they should live under `skills/` and whether a short repo note should describe how they relate to `~/.codex/skills`.
- Why: this install added a repo-local copy of `mosp-cpp-performance-engineer`, but there is already a global copy on the machine, so future updates could drift.
- Evidence / related notes: `notes/agent_memory_log.md`, `skills/mosp-cpp-performance-engineer/`

### Debug the config-runner exit hang for `SOPMOA_relaxed`
- Status: `next`
- Idea: reproduce why `SOPMOA_relaxed` sometimes prints normal end-of-run stats yet does not hand control back to `bench/scripts/run_benchmark.py` on the NYC `88959 -> 96072` query.
- Why: the manual summary-only benchmark path is currently stable, but the config-driven benchmark path produced wall-timeout placeholders with `exit_code=-15`, which makes the suite summary unreliable.
- Evidence / related notes: `notes/agent_memory_log.md`, `bench/results/nyc_parallel_timecap_large_20260412/summary.csv`, `notes/benchmarks/2026-04-12-nyc-parallel-large/README.md`

### Run a scaling follow-up for `LTMOA_parallel` on the stable NYC query
- Status: `investigate`
- Idea: extend the current `88959 -> 96072` benchmark to `1/2/4` threads once the benchmark path is stable enough to produce clean repeated rows.
- Why: the new 2-thread rerun is the strongest current positive signal for `LTMOA_parallel`, but it still leaves the scaling question open.
- Evidence / related notes: `notes/benchmarks/2026-04-12-nyc-parallel-large/README.md`, `notes/agent_memory_log.md`

### Investigate why PMR helps `LTMOA_array_superfast` much more than `LTMOA_parallel`
- Status: `investigate`
- Idea: profile `LTMOA_parallel` on the NYC `88959 -> 96072` query to separate allocator wins from remaining bottlenecks such as shard contention, duplicate work, and target-frontier publication cost.
- Why: the PMR rewrite preserved runtime but did not improve the 2-thread anytime frontier relative to the earlier archive, while the serial `LTMOA_array_superfast` comparison improved strongly.
- Evidence / related notes: `notes/benchmarks/2026-04-12-pmr-allocators/README.md`, `notes/agent_memory_log.md`

### Add a Swiss-table side index for exact duplicate detection
- Status: `investigate`
- Idea: add a Swiss-table-style side index for exact-cost duplicate detection and lightweight frontier metadata lookup, using a packed cost signature or truncated cost vector as the key and a frontier index/offset as the value, while keeping the main frontier as an order-aware dominance structure rather than a hash table.
- Why: frontier updates currently pay scan cost to reject exact duplicates before dominance checks; a side index could cut duplicate detection cost without pretending that dominance is an exact-key lookup problem.
- Evidence / related notes: `notes/agent_memory_log.md`, `inc/algorithms/gcl/gcl_fast_array.h`, `inc/algorithms/gcl/gcl_owner_sharded.h`

### Replace remote parallel inbox delivery with an MPMC bulk queue path
- Status: `investigate`
- Idea: keep each worker's local heap as a private `std::vector`/heap structure, but route remote label batches through a dedicated MPMC queue with bulk enqueue/dequeue semantics so the owner thread can merge batches into its local frontier in blocks instead of relying on heavier shared structures.
- Why: `LTMOA_parallel` still looks bottlenecked by remote delivery and publication overhead, and the parallel roadmap should prefer owner-local heaps plus batched inbox delivery over reviving a global concurrent priority queue design.
- Evidence / related notes: `notes/agent_memory_log.md`, `src/algorithms/ltmoa_parallel.cpp`, `inc/algorithms/ltmoa_parallel.h`

### Prototype a chunked SoA frontier for SIMD-friendly dominance
- Status: `investigate`
- Idea: prototype a promoted frontier layout that stays small and simple for tiny skylines, then switches to chunked structure-of-arrays storage with objective-wise contiguous arrays, alive masks, and optional side metadata once the frontier grows large enough to justify vectorized dominance scans.
- Why: the frontier dominance hot loop is likely paying too much per-entry metadata and branch cost in the current packed-entry layout; a chunked SoA design is the most plausible next step if the repo wants to turn dominance checks into a SIMD-friendly kernel instead of only tuning allocators and containers.
- Evidence / related notes: `notes/agent_memory_log.md`, `inc/algorithms/gcl/gcl_fast_array.h`, `notes/benchmarks/2026-04-12-pmr-allocators/README.md`

### Do not treat PMR alone as the next parallel optimization thesis
- Status: `hold`
- Idea: do not assume more allocator-only work will be a sufficient optimization direction for `LTMOA_parallel`; future parallel tuning should focus on contention, duplicate work, and target-frontier publication overhead first.
- Why: the PMR rewrite kept `LTMOA_parallel` runtime near `1.0x` of the earlier archive but did not improve the archived `2`-thread anytime frontier on the NYC `88959 -> 96072` query.
- Evidence / related notes: `notes/benchmarks/2026-04-12-pmr-allocators/README.md`, `notes/agent_memory_log.md`

### Do not revive a global concurrent-priority-queue center for parallel search
- Status: `do-not-repeat`
- Idea: do not move `LTMOA_parallel` back toward a design where remote delivery and scheduling are centered on a shared global concurrent priority queue; if concurrency plumbing changes again, keep the ownership model local-first and batch-oriented.
- Why: the current evidence points toward owner-local heaps and batched communication as the better direction for shared-memory exact MOSP, while a global concurrent-priority-queue center would reintroduce the exact contention pattern the repo has already been moving away from.
- Evidence / related notes: `notes/agent_memory_log.md`, `src/algorithms/ltmoa_parallel.cpp`, `inc/algorithms/gcl/gcl_owner_sharded.h`

### Keep `HybridCorridorPulseA` archived, not promoted
- Status: `hold`
- Idea: keep `HybridCorridorPulseA` as an archived exact serial experiment and do not fold it into the main LTMOA line unless a future revision materially improves the NYC anytime frontier relative to `LTMOA_array_superfast`.
- Why: the first exact-first prototype was exact and benchmark-clean, but on the stable NYC `88959 -> 96072` query it reached only `0.359x`, `0.376x`, `0.443x`, and `0.520x` of `LTMOA_array_superfast`'s median frontier at `0.5/1/2/5s`.
- Evidence / related notes: `notes/benchmarks/2026-04-12-hybrid-corridor-pulsea/README.md`, `notes/agent_memory_log.md`

### If the hybrid corridor line is revisited, target shutdown overhead before new tuning
- Status: `investigate`
- Idea: if `HybridCorridorPulseA` is revisited, profile and reduce the post-budget wall-time overhead before attempting more corridor or burst-policy tuning.
- Why: the prototype respected solver-side `runtime_sec` budgets, but the runner still observed median wall runtimes of `4.38s`, `5.06s`, `6.25s`, and `8.91s` at `0.5/1/2/5s`, which is too expensive for a summary-only serial suite.
- Evidence / related notes: `notes/benchmarks/2026-04-12-hybrid-corridor-pulsea/README.md`, `bench/results/nyc_hybrid_corridor_pulsea_20260412/run_results.json`

### Fix the runner completion banner for time-capped suites
- Status: `next`
- Idea: update `bench/scripts/run_benchmark.py` so summary reporting distinguishes clean anytime `status=timeout` rows from real failed runs.
- Why: both new PMR suites finished with canonical summary rows and `exit_code=0`, but the runner still printed `Completed 0/N runs`, which is misleading during experiment triage.
- Evidence / related notes: `notes/benchmarks/2026-04-12-pmr-allocators/README.md`, `bench/results/nyc_ltmoa_array_pmr_20260412/summary.csv`, `bench/results/nyc_ltmoa_parallel_pmr_20260412/summary.csv`

### Add mixed-status runner coverage if status logic changes again
- Status: `hold`
- Idea: if the runner status accounting changes again, add one more smoke case that mixes a clean solver timeout with a true runner timeout or crash so the console summary breakdown stays trustworthy.
- Why: the clean-timeout banner bug is now fixed, but the next likely regression surface is mixed suites where solver-level `timeout` and runner-level timeout need to stay visually distinct.
- Evidence / related notes: `tests/benchmark_runner_smoke.py`, `notes/agent_memory_log.md`

### Keep vendored repo-local skills discoverable and intentional
- Status: `hold`
- Idea: if more skills are vendored into `skills/`, keep a short note in repo memory about the source archive or source repo and avoid letting the repo-local copies drift silently from whatever external source they were installed from.
- Why: the repo now has at least two vendored local skills, including `mosp-cpp-performance-engineer` and `multiobjective-search-inventor`, so future updates can easily diverge unless the source-of-truth is recorded explicitly.
- Evidence / related notes: `notes/agent_memory_log.md`, `skills/mosp-cpp-performance-engineer/`, `skills/multiobjective-search-inventor/`

### Keep the repaired hybrid experimental until TTFS improves
- Status: `investigate`
- Idea: keep `HybridCorridorPulseA` as an experimental solver line for now, and treat `time_to_first_solution_sec` as the next primary bottleneck instead of startup wall time or frontier size.
- Why: the repair gate now passed on `4/4` NYC queries and fixed the big startup wall issue, but the hybrid still trails `LTMOA_array_superfast` by roughly `2.1x` to `3.3x` on early-budget TTFS.
- Evidence / related notes: `notes/benchmarks/2026-04-12-hybrid-corridor-pulsea-repair/README.md`, `notes/agent_memory_log.md`

### Do not reintroduce the heavy landmark oracle into the hybrid by default
- Status: `do-not-repeat`
- Idea: do not move `HybridCorridorPulseA` back to the full `LowerBoundOracle` startup path unless a future benchmark shows a clear net gain over the cheaper reverse-Dijkstra heuristic.
- Why: the repair pass showed that most of the prototype's large runner wall overhead came from the heavy bound construction, and the cheaper heuristic version still beat `LTMOA_array_superfast` on the broader NYC gate.
- Evidence / related notes: `notes/benchmarks/2026-04-12-hybrid-corridor-pulsea-repair/README.md`, `notes/agent_memory_log.md`

### Separate heavy-bound comparison lines from the main repair gate
- Status: `hold`
- Idea: if future hybrid or LTMOA benchmark suites include `LTMOA_array_superfast_lb`, either give that branch a larger timeout grace or keep it out of the primary accept/reject gate.
- Why: the repaired hybrid gate finished cleanly for `HybridCorridorPulseA` and `LTMOA_array_superfast`, but the fixed `budget + 5s` grace still produced `10` runner-level timeouts in the heavy-bound `_lb` branch.
- Evidence / related notes: `bench/results/nyc_hybrid_corridor_pulsea_repair_20260412/run_results.json`, `notes/agent_memory_log.md`
