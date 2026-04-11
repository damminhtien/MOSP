# Architecture

This document is a short map of the SOPMOA* codebase. It complements
[README.md](/workspaces/SOPMOA/README.md) by focusing on how the code is
organized, how execution flows through the system, and where benchmark
results come from.

## Purpose

The repository implements exact multi-objective shortest-path (MOSP)
solvers on large graphs. The main focus is the parallel `SOPMOA` family,
with several exact baseline solvers included for comparison.

## High-Level Flow

1. `main` parses CLI options and selects one solver.
2. Graph weight files are loaded from one or more map files.
3. A forward graph and reverse graph are constructed.
4. The chosen solver precomputes a per-objective heuristic from the
   reverse graph.
5. The solver runs either a single `(start, target)` query or a batch of
   queries from a scenario JSON file.
6. Statistics are appended to a CSV output file.
7. If requested, the Pareto front is written to per-query text files.

The main entry point is [src/main.cpp](/workspaces/SOPMOA/src/main.cpp).

## Directory Map

### Root files

- [CMakeLists.txt](/workspaces/SOPMOA/CMakeLists.txt): build target and
  dependency wiring.
- [README.md](/workspaces/SOPMOA/README.md): user-facing build, CLI, data,
  and benchmark documentation.
- [run_command.txt](/workspaces/SOPMOA/run_command.txt): local build and
  benchmark command log.
- [Preprint.pdf](/workspaces/SOPMOA/Preprint.pdf),
  [Poster.pdf](/workspaces/SOPMOA/Poster.pdf),
  [sopmoa_flow.png](/workspaces/SOPMOA/sopmoa_flow.png): research context.

### Source tree

- `src/`: implementations.
- `inc/`: headers and templates.
- `lib/json/`: vendored `nlohmann::json`.
- `maps/`: example NYC objective files.
- `scen/`: batch query sets in JSON format.

## Core Runtime Pieces

### Program entry

- [src/main.cpp](/workspaces/SOPMOA/src/main.cpp)
  - parses CLI options
  - dispatches to the selected solver
  - handles single-query vs scenario mode
  - measures wall-clock runtime
  - writes CSV rows and optional solution logs

### Data loading

- [src/utils/data_io.cpp](/workspaces/SOPMOA/src/utils/data_io.cpp)
  - `read_graph_files(...)` loads one file per objective
  - verifies all map files share the same edge order
  - `read_scenario_file(...)` parses JSON query batches

### Graph model

- [inc/problem/graph.h](/workspaces/SOPMOA/inc/problem/graph.h)
- [src/problem/graph.cpp](/workspaces/SOPMOA/src/problem/graph.cpp)

`AdjacencyMatrix` stores adjacency lists, despite the name. The program
builds both:

- a forward graph for expansion
- a reverse graph for heuristic computation

### Heuristic

- [inc/problem/heuristic.h](/workspaces/SOPMOA/inc/problem/heuristic.h)
- [src/problem/heuristic.cpp](/workspaces/SOPMOA/src/problem/heuristic.cpp)

Each solver builds a heuristic by running reverse Dijkstra once per
objective. This produces an admissible vector lower bound used in the
label `f = g + h`.

### Shared solver API

- [inc/algorithms/abstract_solver.h](/workspaces/SOPMOA/inc/algorithms/abstract_solver.h)

All solvers inherit from `AbstractSolver`, which provides:

- solution storage
- `generated` and `expanded` counters
- a common result string for CSV output

## Solver Layout

### Parallel solvers

- [src/algorithms/sopmoa.cpp](/workspaces/SOPMOA/src/algorithms/sopmoa.cpp):
  OpenMP-based shared-OPEN solver using a concurrent priority queue.
- [src/algorithms/sopmoa_relaxed.cpp](/workspaces/SOPMOA/src/algorithms/sopmoa_relaxed.cpp):
  worker-local heaps, batched inboxes, work stealing, and a relaxed
  concurrent frontier.
- [src/algorithms/sopmoa_bucket.cpp](/workspaces/SOPMOA/src/algorithms/sopmoa_bucket.cpp):
  parallel variant with a bucket priority queue.
- [src/algorithms/ltmoa_parallel.cpp](/workspaces/SOPMOA/src/algorithms/ltmoa_parallel.cpp):
  LTMOA-style parallel search with per-thread heaps and arenas, batched
  remote delivery, owner-sharded truncated node frontiers, and an exact
  full-cost target frontier snapshot for pruning.

### Serial baselines

- [src/algorithms/ltmoa.cpp](/workspaces/SOPMOA/src/algorithms/ltmoa.cpp)
- [src/algorithms/lazy_ltmoa.cpp](/workspaces/SOPMOA/src/algorithms/lazy_ltmoa.cpp)
- [src/algorithms/ltmoa_array.cpp](/workspaces/SOPMOA/src/algorithms/ltmoa_array.cpp)
- [src/algorithms/ltmoa_array_superfast.cpp](/workspaces/SOPMOA/src/algorithms/ltmoa_array_superfast.cpp)
- [src/algorithms/ltmoa_array_superfast_anytime.cpp](/workspaces/SOPMOA/src/algorithms/ltmoa_array_superfast_anytime.cpp)
- [src/algorithms/ltmoa_array_superfast_lb.cpp](/workspaces/SOPMOA/src/algorithms/ltmoa_array_superfast_lb.cpp)
- [src/algorithms/lazy_ltmoa_array.cpp](/workspaces/SOPMOA/src/algorithms/lazy_ltmoa_array.cpp)
- [src/algorithms/emoa.cpp](/workspaces/SOPMOA/src/algorithms/emoa.cpp)
- [src/algorithms/nwmoa.cpp](/workspaces/SOPMOA/src/algorithms/nwmoa.cpp)

These solvers share the same overall pattern:

1. initialize a label at the start node
2. pop the next label from OPEN
3. prune with dominance checks
4. update the frontier for the current node
5. expand outgoing edges
6. record a solution when the target is reached

`LTMOA_array_superfast_anytime` is the incumbent-first variant of the
packed-array LTMOA baseline: it keeps the same exact node frontier and
target frontier semantics as `LTMOA_array_superfast`, but delays a small
seed burst until the search has run briefly without an incumbent and
switches OPEN to an incumbent-aware priority policy only after the first
accepted target point.

## Frontier / Dominance Data Structures

The `inc/algorithms/gcl/` directory contains node-local frontier
representations used for dominance pruning:

- [gcl.h](/workspaces/SOPMOA/inc/algorithms/gcl/gcl.h): list-based frontier
- [gcl_array.h](/workspaces/SOPMOA/inc/algorithms/gcl/gcl_array.h): vector-based frontier
- [gcl_fast_array.h](/workspaces/SOPMOA/inc/algorithms/gcl/gcl_fast_array.h): packed contiguous frontier with small-start adaptive growth and fused insert-or-prune
- [gcl_tree.h](/workspaces/SOPMOA/inc/algorithms/gcl/gcl_tree.h): AVL-tree frontier
- [gcl_nwmoa.h](/workspaces/SOPMOA/inc/algorithms/gcl/gcl_nwmoa.h): NWMOA-specific ordered frontier
- [gcl_sopmoa.h](/workspaces/SOPMOA/inc/algorithms/gcl/gcl_sopmoa.h): shared frontier with locks
- [gcl_relaxed.h](/workspaces/SOPMOA/inc/algorithms/gcl/gcl_relaxed.h): relaxed concurrent frontier with snapshots and compaction
- [gcl_owner_sharded.h](/workspaces/SOPMOA/inc/algorithms/gcl/gcl_owner_sharded.h): owner-partitioned wrapper over the packed truncated frontier for `LTMOA_parallel`

This is one of the main design axes in the repo: different solvers use
different OPEN queues and different frontier containers, but they all
solve the same MOSP search problem.

Related allocation utility:

- [label_arena.h](/workspaces/SOPMOA/inc/utils/label_arena.h): block allocator used by `LTMOA_array_superfast` and `LTMOA_parallel` to remove per-label `new` from the hot path

## Data Formats

### Map files

The loader expects DIMACS-like arc lines:

```text
a <from> <to> <weight>
```

Multiple map files represent multiple objectives and must share the same
edge list in the same order.

### Scenario files

Scenario files in `scen/` are JSON arrays with:

- `name`
- `start_data`
- `end_data`

They are used for batch evaluation through `--scenario`.

## Benchmark Notes

The current benchmark process is script-based rather than framework-based.
The main references are:

- [README.md](/workspaces/SOPMOA/README.md)
- [run_command.txt](/workspaces/SOPMOA/run_command.txt)

Current local benchmarking works by running `./Release/main` repeatedly
on fixed NYC map files and recording CSV rows.

Key points:

- runtime is measured in [src/main.cpp](/workspaces/SOPMOA/src/main.cpp)
  around `solver->solve(...)` using `std::chrono::steady_clock`
- CSV rows contain solver name, query endpoints, generated count,
  expanded count, solution count, and runtime
- some solvers enforce the time limit differently, so `--timelimit` is a
  requested cutoff rather than a strict global guarantee
- `LTMOA_parallel` is the additional parallel baseline that respects
  `-n`; the serial baselines still ignore that flag
- `SOPMOA_bucket` is documented in the README as currently aborting on
  the local benchmark query

## Suggested Reading Order

For a quick onboarding pass:

1. [README.md](/workspaces/SOPMOA/README.md)
2. [src/main.cpp](/workspaces/SOPMOA/src/main.cpp)
3. [src/utils/data_io.cpp](/workspaces/SOPMOA/src/utils/data_io.cpp)
4. [src/problem/heuristic.cpp](/workspaces/SOPMOA/src/problem/heuristic.cpp)
5. one baseline solver such as [src/algorithms/ltmoa.cpp](/workspaces/SOPMOA/src/algorithms/ltmoa.cpp)
6. one parallel solver such as [src/algorithms/sopmoa_relaxed.cpp](/workspaces/SOPMOA/src/algorithms/sopmoa_relaxed.cpp)
7. the corresponding frontier structure in `inc/algorithms/gcl/`

That sequence gives the shortest path from "how do I run this?" to
"where is the main algorithmic complexity?"
