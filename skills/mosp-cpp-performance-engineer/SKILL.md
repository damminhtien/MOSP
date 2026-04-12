---
name: mosp-cpp-performance-engineer
description: optimize, benchmark, parallelize, and review c or c++ implementations for multi-objective shortest path, constrained shortest path, and related graph algorithms. use when chatgpt needs to inspect a github repository, uploaded .cpp/.h files, benchmark datasets, or a research paper; derive implementable pseudocode; map a paper into c++ modules; prepare build instructions; find bottlenecks; propose or implement moderate-to-aggressive speed optimizations; design cpu-first benchmarking and scalability experiments; compare algorithm variants; or suggest shared-memory, simd, mpi, and gpu-follow-up opportunities.
---

# Mosp Cpp Performance Engineer

## Overview

Use this skill to analyze and improve high-performance C/C++ graph code, especially multi-objective shortest path and constrained shortest path implementations on sparse directed road-network-style graphs. Prefer CPU-first engineering. Treat GPU work as follow-up suggestions unless the user explicitly asks for implementation.

Default assumptions:
- Target `g++`, `clang++`, and `MSVC` when possible.
- Prefer `C++17` or `C++20`.
- Support `CMake`, `Make`, or an existing build system already present in the repo.
- Treat MPI as important when it fits the decomposition, but do not force it onto fundamentally shared-memory bottlenecks.
- Accept analysis-only, patch planning, code diffs, rewrites, benchmark harness design, and parallelization guidance.

## Workflow decision tree

1. Classify the request.
   - **Repo or source review**: inspect layout, build system, core data structures, hot paths, and algorithm family.
   - **Paper to code**: derive implementable pseudocode, define module boundaries, identify data structures, and note unproven implementation details.
   - **Benchmarking**: create a reproducible experiment plan and compare variants fairly.
   - **Optimization / parallelization**: find bottlenecks, preserve correctness, and prioritize the highest-payoff changes first.

2. Establish the baseline before proposing large changes.
   - Identify compiler, standard, build flags, target OS, and available hardware.
   - Identify whether the algorithm is label-setting, label-correcting, approximation, exact Pareto, or constrained.
   - Identify graph model: sparse or dense, directed or undirected, static or dynamic, number of objectives, and expected Pareto-front size.

3. Choose the depth of intervention.
   - **Analysis only**: explain bottlenecks and risks.
   - **Patch plan**: give prioritized edits and expected payoff.
   - **Code changes**: propose patches or rewrite modules.
   - **Benchmark harness**: add reproducible runners, metrics, and exports.

4. Keep the output grounded.
   - Distinguish measured findings from hypotheses.
   - Mark any assumption that was not verified from code, build output, or benchmark results.
   - State what was compiled, tested, benchmarked, or only inferred.

## Repo and source review workflow

1. Identify entry points: executable targets, library targets, benchmark targets, and tests.
2. Identify the graph representation, label representation, queue/worklist design, dominance structures, memory ownership model, and synchronization strategy.
3. Build a bottleneck map:
   - graph loading and preprocessing
   - label generation and relaxation
   - dominance checks and pruning
   - queue operations and contention
   - memory allocation, copying, and cache locality
   - serialization / logging / metrics overhead
4. Separate algorithmic issues from implementation issues.
   - **Algorithmic**: unnecessary asymptotic work, weak pruning, poor decomposition, approximation trade-offs.
   - **Implementation**: branchy code, fragmented memory, virtual dispatch, allocator churn, false sharing, lock contention, poor data layout.
5. Recommend changes in descending order of expected impact.

## Paper-to-code workflow

1. Extract the problem formulation:
   - objective vector
   - constraints
   - state definition
   - optimality / dominance conditions
   - claimed complexity and speedup arguments
2. Derive implementable pseudocode.
3. Map the method into C++ modules. Use this default split unless the paper suggests a better one:
   - `graph/` for graph storage and loaders
   - `core/` for labels, states, dominance, priority rules
   - `algorithms/` for variant-specific search kernels
   - `parallel/` for shared-memory or MPI execution helpers
   - `bench/` for runners, result capture, and scaling experiments
   - `tests/` for correctness and regression checks
4. Call out missing paper details explicitly, such as tie-breaking, data layout, stopping criteria, pruning order, or concurrency semantics.
5. Compare paper claims to observed repo behavior if both are available.

## Optimization playbook

Apply optimizations in this order unless profiling shows otherwise.

### 1. Remove avoidable work
- Reduce repeated dominance checks.
- Cache or precompute invariant values.
- Eliminate duplicate label expansions.
- Tighten pruning early, especially before expensive allocations or queue inserts.

### 2. Fix data layout and memory traffic
- Prefer contiguous storage and compact label/state layouts.
- Minimize pointer chasing.
- Reduce temporary allocations and copies.
- Consider structure-of-arrays for vectorized or batched operations.
- Reuse buffers, arenas, or pools when lifetime patterns are regular.

### 3. Improve hot-loop code generation
- Simplify branch-heavy logic.
- Hoist loop invariants.
- Reduce indirect calls in hot paths.
- Consider vectorization-friendly layouts for objective comparisons and dominance tests.

### 4. Parallelize carefully
For shared-memory CPU parallelism, examine:
- label expansion partitioning
- local versus global work queues
- work stealing
- sharded dominance structures
- batched synchronization
- false sharing on counters and label containers
- reduction or merge cost after local processing

For MPI, check whether the graph and frontier can be decomposed without excessive communication or duplicate exploration.

For GPU, provide only follow-up opportunities unless the user explicitly asks for implementation. Focus on whether the workload is regular enough and whether transfers, divergence, and irregular memory access would dominate.

### 5. Reconsider the algorithm
If low-level tuning is not enough, consider:
- switching between label-setting and label-correcting
- approximation or epsilon-dominance
- bounded or constrained variants
- better heuristics or lower bounds
- preprocessing or contraction-like techniques where valid

## Benchmarking workflow

Use the detailed checklist in `references/benchmarking-playbook.md`.

Always design benchmarks to answer specific questions:
- Which algorithm variant is fastest at fixed correctness requirements?
- How does performance change with objective count, graph size, and Pareto-front size?
- What is the speedup curve from 1 thread to N threads?
- Where does scaling saturate?
- Is the improvement algorithmic, implementation-level, or hardware-specific?

Minimum benchmark expectations:
- compile all compared variants consistently
- record compiler, standard, flags, hardware, dataset identifiers, thread count, and repetition count
- report runtime, speedup, efficiency, memory if available, and failures or timeouts
- export machine-readable results such as CSV or JSON
- present a markdown summary with clear caveats

## Validation requirements

Before calling an optimization successful, try to verify as many of these as the task context permits:
- compilation succeeds
- unit tests pass when available
- outputs match a baseline on representative instances
- Pareto fronts are equivalent when exactness is required
- results are deterministic across repeated runs when determinism is expected
- scaling trends do not collapse under contention or communication overhead

If only some checks were possible, state exactly which checks ran and which did not.

## Output format

Use the report template in `references/report-template.md`.

Prefer this order:
1. one-paragraph diagnosis
2. prioritized optimization roadmap
3. patch plan or module rewrite plan
4. benchmark design or measured comparison table
5. parallelization recommendations
6. risks, assumptions, and validation gaps

## Examples

### Example 1
Input: a github repository implementing k-objective label-setting search on sparse directed road graphs, plus a request to make it faster on a 32-core cpu.

Output: build instructions, bottleneck map, prioritized optimization roadmap, concrete patch plan for data layout and queue contention, benchmark protocol for 1/2/4/8/16/32 threads, and gpu follow-up suggestions only if the workload looks regular enough.

### Example 2
Input: a paper describing a parallel constrained multi-objective shortest path algorithm and a partial c++ codebase.

Output: implementable pseudocode, c++ module mapping, list of ambiguous paper details, comparison between claimed and observed behavior, and a plan to integrate the algorithm into the existing benchmark harness.
