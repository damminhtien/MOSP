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

### Do not treat PMR alone as the next parallel optimization thesis
- Status: `hold`
- Idea: do not assume more allocator-only work will be a sufficient optimization direction for `LTMOA_parallel`; future parallel tuning should focus on contention, duplicate work, and target-frontier publication overhead first.
- Why: the PMR rewrite kept `LTMOA_parallel` runtime near `1.0x` of the earlier archive but did not improve the archived `2`-thread anytime frontier on the NYC `88959 -> 96072` query.
- Evidence / related notes: `notes/benchmarks/2026-04-12-pmr-allocators/README.md`, `notes/agent_memory_log.md`

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
