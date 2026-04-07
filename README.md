# MOSP: Exact Multi-Objective Shortest-Path Solvers

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](#)
[![CMake](https://img.shields.io/badge/CMake-3.16%2B-blue.svg)](#)
[![oneTBB](https://img.shields.io/badge/oneTBB-enabled-brightgreen.svg)](#)
[![License: AGPL v3](https://img.shields.io/badge/License-AGPLv3-orange.svg)](#license)

This repository is a C++17 project for exact multi-objective shortest-path
(MOSP) search on weighted graphs. It includes several serial exact baselines,
parallel experimental solvers, scenario runners, and benchmark artifact export
for final frontiers and anytime traces.

The repository originally centered on the SOPMOA family, but the codebase is
better understood as a broader MOSP solver workbench:

- exact serial baselines for comparison
- parallel SOPMOA-family variants
- shared graph loading and heuristic infrastructure
- legacy CSV output and canonical benchmark artifacts

## Implemented Solvers

| Solver | Category | Notes |
| --- | --- | --- |
| `SOPMOA` | parallel exact | Original shared-OPEN solver |
| `SOPMOA_relaxed` | parallel exact | Work-first relaxed solver with worker-local queues |
| `SOPMOA_bucket` | parallel experimental | Bucket-queue variant; still unstable on some benchmark runs |
| `LTMOA` | serial exact | List-backed exact baseline |
| `LazyLTMOA` | serial exact | Lazier variant of LTMOA |
| `LTMOA_array` | serial exact | Array-backed exact baseline |
| `LazyLTMOA_array` | serial exact | Lazy array-backed baseline |
| `EMOA` | serial exact | AVL-tree-backed exact baseline |
| `NWMOA` | serial exact | Ordered frontier / bucketed OPEN variant |

## Current Status

- The repository builds as a normal CMake C++17 project.
- Canonical benchmark artifacts are supported through:
  - `--summary-output`
  - `--frontier-output-dir`
  - `--trace-output-dir`
- Solver traces now export finite `hv_ratio` and `recall` when a run completes
  through the canonical instrumentation path.
- `SOPMOA_bucket` remains experimental; true `crash` and `oom` classification is
  still better handled by an external runner than by in-process tests.
- The checked-in `build/` directory contains stale machine-specific CMake cache
  and should not be reused.

## Requirements

- CMake 3.16+
- A C++17 compiler
- Boost `program_options`
- oneTBB

OpenMP is optional in the current build. If CMake finds it, it will be linked;
if not, the project still builds.

### macOS

```bash
brew install cmake boost tbb
```

Optional:

```bash
brew install libomp
```

### Ubuntu / Debian

```bash
sudo apt update
sudo apt install -y build-essential cmake libboost-program-options-dev libtbb-dev
```

Optional:

```bash
sudo apt install -y libomp-dev
```

## Build

Use a fresh build directory such as `Release/`.

### macOS / Homebrew

```bash
PREFIXES="$(brew --prefix boost);$(brew --prefix tbb)"

cmake -S . -B Release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$PREFIXES"

cmake --build Release -j
./Release/main --help
```

### Linux

```bash
cmake -S . -B Release -DCMAKE_BUILD_TYPE=Release
cmake --build Release -j
./Release/main --help
```

## CLI Summary

At least one output target is required:

- legacy CSV: `--output`
- canonical summary CSV: `--summary-output`
- canonical frontier artifacts: `--frontier-output-dir`
- canonical trace artifacts: `--trace-output-dir`

Main options:

- `-m, --map <file1> [file2 ...]`: one map file per objective
- `-a, --algorithm <NAME>`:
  `SOPMOA | SOPMOA_relaxed | SOPMOA_bucket | LTMOA | LazyLTMOA | LTMOA_array | LazyLTMOA_array | EMOA | NWMOA`
- `-s, --start <node>` / `-t, --target <node>`: single query
- `--scenario <file.json>`: batch mode from scenario file
- `--from <i>` / `--to <j>`: scenario slice
- `--timelimit <seconds>`: legacy cutoff argument
- `--budget-sec <seconds>`: canonical benchmark budget argument
- `-n, --numthreads <N>`: used by the parallel solvers
- `--logsols <dir>`: dump solution fronts as text files
- `--dataset-id <id>` / `--query-id <id>` / `--seed <value>`: benchmark metadata
- `--trace-interval-ms <ms>`: periodic trace export; `0` means frontier-change events only

Threading behavior:

- `SOPMOA` caps `-n` at `12`
- `SOPMOA_bucket` caps `-n` at `16`
- `SOPMOA_relaxed` uses the requested thread count directly
- serial baselines ignore `-n`

## Quick Start

Single-query run with legacy CSV output:

```bash
mkdir -p output

./Release/main \
  -m maps/NY-d.txt maps/NY-t.txt maps/NY-m.txt \
  -s 88959 \
  -t 96072 \
  -o output/ltmoa_array.csv \
  -a LTMOA_array \
  --timelimit 2
```

Single-query canonical benchmark export:

```bash
mkdir -p output/frontiers output/traces

./Release/main \
  -m maps/NY-d.txt maps/NY-t.txt maps/NY-m.txt \
  -s 88959 \
  -t 96072 \
  -a SOPMOA_relaxed \
  --budget-sec 2 \
  -n 4 \
  --summary-output output/summary.csv \
  --frontier-output-dir output/frontiers \
  --trace-output-dir output/traces \
  --dataset-id nyc_3obj \
  --query-id q0 \
  --trace-interval-ms 100
```

Scenario run with solution dumps:

```bash
mkdir -p output/sols

./Release/main \
  -m maps/NY-d.txt maps/NY-t.txt maps/NY-m.txt \
  --scenario scen/query3.json \
  --from 0 \
  --to 1 \
  -a SOPMOA_relaxed \
  --timelimit 2 \
  -n 4 \
  -o output/scenario.csv \
  --logsols output/sols
```

## Input Data

### Map Files

The loader expects DIMACS-style arc lines:

```text
c optional comment
a <from> <to> <weight>
```

When multiple map files are passed, they must describe the same edge list in
the same order. Each file contributes one objective.

Repository examples:

- `maps/NY-d.txt`
- `maps/NY-t.txt`
- `maps/NY-m.txt`
- `maps/NY-r.txt`
- `maps/NY-l.txt`

### Scenario JSON

Scenario files are JSON arrays of objects with:

- `name`
- `start_data`
- `end_data`

Example:

```json
[
  { "name": "query3_1", "start_data": 88959, "end_data": 96072 },
  { "name": "query3_2", "start_data": 172570, "end_data": 197762 }
]
```

## Output Formats

### Legacy CSV

Legacy `--output` rows use:

```text
solver_name,start,target,generated,expanded,num_solutions,runtime_sec
```

Example:

```text
LTMOA(3obj)-list,88959,96072,317047,303935,932,3.46159
```

### Solution Dumps

When `--logsols` is enabled, each query writes one text file:

```text
<start>-<target>-<num_objectives>obj.txt
```

Each line is one Pareto cost vector:

```text
[12,45,78]
[13,43,81]
```

### Canonical Benchmark Artifacts

Canonical benchmark mode can write:

- a summary CSV with run metadata, counters, status, and artifact paths
- one final-frontier CSV per query
- one anytime-trace CSV per query

Frontier artifacts store:

- `time_found_sec`
- `obj1, obj2, ...`

Trace artifacts store:

- `time_sec`
- `trigger`
- `frontier_size`
- generated / expanded / pruned counters
- `hv_ratio`
- `recall`

## Benchmark Snapshot

The table below is a historical local benchmark snapshot preserved from the
commands in `run_command.txt`. It is useful as a regression reference, not as a
publication claim.

Setup:

- date: April 4, 2026
- maps: `maps/NY-d.txt maps/NY-t.txt maps/NY-m.txt`
- query: `88959 -> 96072`
- requested budget: `2s`
- machine: local macOS laptop run noted in the previous branch history

| Configuration | Threads | Runtime (s) | Generated | Expanded | Solutions | Interpretation |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `SOPMOA_relaxed` | 4 | `2.00048` median over 5 runs | about `699,497` avg | about `657,426` avg | about `1,395` avg | Best exact parallel result on this one query snapshot |
| `LTMOA_array` | 1 | `3.51381` | `1,074,754` | `982,023` | `1,936` | Strongest serial baseline in the same historical notes |
| `SOPMOA` | 4 | `2.00611` | `105,082` | `102,946` | `380` | Original shared-OPEN parallel solver |
| `LTMOA` | 1 | `3.46159` | `317,047` | `303,935` | `932` | Reference exact baseline |
| `LazyLTMOA` | 1 | `3.87518` | `455,678` | `304,071` | `932` | Lazy LTMOA variant |
| `LazyLTMOA_array` | 1 | `4.14031` | `1,119,471` | `664,652` | `1,545` | Lazy array-backed variant |
| `EMOA` | 1 | `4.14829` | `445,188` | `421,791` | `1,142` | AVL-tree-backed exact solver |
| `NWMOA` | 1 | `4.14240` | `454,791` | `357,704` | `1,029` | Ordered-frontier exact solver |

Notes:

- `SOPMOA_bucket` is intentionally omitted from the table because it aborted on
  the same local benchmark query.
- These numbers are machine-specific and should be treated as historical local
  observations.
- The table is deliberately MOSP-wide: it compares the solver families in the
  repository instead of presenting the project as only a SOPMOA branch.

Practical reading:

- use `LTMOA_array` if you want the strongest historical 1-thread baseline
- use `SOPMOA_relaxed -n 4` if you want the strongest historical exact parallel
  result from the local branch notes
- rerun the benchmark on your own machine before drawing broader conclusions

## Project Layout

```text
.
├── CMakeLists.txt
├── README.md
├── ARCHITECTURE.md
├── run_command.txt
├── inc/
│   ├── algorithms/
│   ├── problem/
│   └── utils/
├── src/
│   ├── algorithms/
│   ├── problem/
│   └── utils/
├── maps/
├── scen/
├── query_generator.py
├── query_random.py
├── Poster.pdf
└── Preprint.pdf
```

## Citation

If you use this repository in academic work, cite the associated SOPMOA paper
when appropriate:

```bibtex
@inproceedings{YourSOPMOA2025,
  title     = {SOPMOA*: Shared-OPEN Parallel A* for Multi-Objective Shortest Paths},
  author    = {Truong, L.V., Dam, T.M., Nguyen, T.A., Nguyen, L.T.T., Dinh, D.T.},
  booktitle = {Proceedings of Computational Science - ICCS 2025. ICCS 2025. Lecture Notes in Computer Science, vol 15906. Springer, Cham},
  year      = {2025},
  note      = {Code: https://github.com/damminhtien/SOPMOA}
}
```

## Acknowledgments

This codebase builds on prior exact MOSP solvers in the literature, especially
LTMOA, EMOA, NWMOA, and the SOPMOA family, and uses Boost and oneTBB in the
implementation.
