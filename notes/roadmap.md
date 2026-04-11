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
