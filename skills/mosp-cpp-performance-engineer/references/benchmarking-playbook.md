# Benchmarking playbook

## Goal

Design reproducible, fair experiments for C/C++ graph algorithms, with emphasis on multi-objective shortest path and constrained shortest path variants.

## Default benchmark questions

Answer these questions whenever they are relevant:
1. Which variant is fastest at equivalent correctness?
2. How does runtime scale with thread count?
3. How does performance change with graph size, sparsity, and objective count?
4. Where is time spent: loading, preprocessing, expansion, dominance, queueing, synchronization, output?
5. Is the speedup stable across instances or only on easy cases?

## Experiment design

### Build matrix
Record at minimum:
- compiler and version
- c++ standard
- build type (`Release`, `RelWithDebInfo`, etc.)
- optimization flags
- architecture-specific flags when used
- parallel runtime (`OpenMP`, `TBB`, `std::thread`, `MPI`)

### Dataset matrix
Record at minimum:
- graph family
- node and edge counts
- directed versus undirected
- static versus dynamic
- number of objectives
- instance identifier or file hash

### Runtime matrix
Sweep at minimum when relevant:
- thread counts: `1, 2, 4, 8, ...`
- algorithm variants
- graph sizes or objective counts
- repetition count

## Metrics

Prefer these metrics:
- wall-clock runtime
- speedup versus single-thread baseline
- parallel efficiency
- memory footprint if available
- number of generated labels
- number of dominance checks
- queue pushes / pops when informative
- failures, timeouts, or non-deterministic runs

## Profiling guidance

Prefer tool choices that fit the environment available in the task. Useful defaults include:
- `perf` for linux hotspots and counters
- `valgrind --tool=callgrind` for call structure when absolute speed is secondary
- sanitizers for correctness and undefined behavior before aggressive tuning
- benchmark harnesses or custom timers when repo-local comparison is the main goal

Treat vendor tools as optional unless already part of the environment.

## Fairness rules

- Keep input sets identical across variants.
- Keep compiler and flags identical unless the point is compiler comparison.
- Warm up when startup effects matter.
- Repeat runs enough to reduce noise.
- Separate one-time preprocessing from per-query costs.
- Report when any implementation changes the algorithmic contract.

## Shared-memory parallelism checklist

Inspect:
- work partitioning balance
- granularity of tasks
- lock contention
- false sharing
- memory bandwidth saturation
- merge or reduction cost
- quality of thread-local buffering
- pinning or affinity assumptions if any

## MPI checklist

Inspect:
- partition quality
- communication volume
- synchronization frequency
- duplicated work across ranks
- frontier imbalance
- serialization overhead

## Result export

Prefer machine-readable artifacts:
- `csv` for tables and plotting
- `json` for richer metadata

At minimum, each result row should contain:
- algorithm
- dataset
- objectives
- threads
- compiler
- flags
- runtime_ms
- speedup
- notes

## Plot suggestions

Use when the task asks for visualization:
- runtime versus thread count
- speedup versus thread count
- runtime versus graph size
- runtime versus objective count
- scalability comparison across variants

## Interpretation guardrails

Do not overclaim.
- A faster runtime on one dataset is not a universal win.
- Better scaling can still lose in absolute time.
- Aggressive rewrites can reduce maintainability or determinism.
- A change that improves one objective count may regress another.
