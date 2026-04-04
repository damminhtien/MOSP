# SOPMOA*: Shared-OPEN Parallel Multi-Objective Search

[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](#)
[![CMake](https://img.shields.io/badge/CMake-3.16%2B-blue.svg)](#)
[![OpenMP](https://img.shields.io/badge/OpenMP-enabled-brightgreen.svg)](#)
[![oneTBB](https://img.shields.io/badge/oneTBB-enabled-brightgreen.svg)](#)
[![License: AGPL v3](https://img.shields.io/badge/License-AGPLv3-orange.svg)](#license)

SOPMOA* is a C++17 codebase for exact multi-objective shortest-path (MOSP) search on large graphs. The repository contains the parallel SOPMOA family together with several exact baseline solvers used for comparison.

![image](sopmoa_flow.png)

## Implemented Solvers

- `SOPMOA`
- `SOPMOA_relaxed`
- `SOPMOA_bucket`
- `LTMOA`
- `LazyLTMOA`
- `LTMOA_array`
- `LazyLTMOA_array`
- `EMOA`
- `NWMOA`

## Requirements

- CMake 3.16+
- A C++17 compiler
- Boost `program_options`
- OpenMP
- oneTBB

### macOS (verified locally on April 4, 2026)

Install the dependencies used for the local run in this repository:

```bash
brew install cmake boost tbb libomp
```

Verified local environment:

- `macOS 26.3.1 (build 25D771280a)`
- `x86_64`
- `Intel(R) Core(TM) i7-1068NG7 CPU @ 2.30GHz`
- `Apple clang 17.0.0`
- `cmake 4.3.1`
- `boost 1.90.0_1`
- `tbb 2022.3.0`
- `libomp 22.1.2`

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install -y build-essential cmake libboost-program-options-dev libomp-dev libtbb-dev
```

## Build

Use a fresh build directory such as `Release/`. The checked-in `build/` directory contains stale machine-specific CMake cache from another environment and should not be reused as-is.

### macOS / Homebrew

```bash
PREFIXES="$(brew --prefix boost);$(brew --prefix tbb);$(brew --prefix libomp)"

cmake -S . -B Release \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$PREFIXES" \
  -DOpenMP_ROOT="$(brew --prefix libomp)"

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

The current binary exposes these options:

- `-m, --map <file1> [file2 ...]`: weight files, one file per objective
- `-a, --algorithm <NAME>`: `SOPMOA | SOPMOA_relaxed | SOPMOA_bucket | LTMOA | LazyLTMOA | LTMOA_array | LazyLTMOA_array | EMOA | NWMOA`
- `-s, --start <node>`: single-query start node
- `-t, --target <node>`: single-query target node
- `--scenario <file.json>`: batch queries from a scenario JSON file
- `--from <i>` / `--to <j>`: slice the scenario range
- `-o, --output <file.csv>`: required output CSV path
- `--logsols <dir>`: optional directory for dumping solution fronts
- `--timelimit <seconds>`: requested per-query time limit
- `-n, --numthreads <N>`: used by `SOPMOA`, `SOPMOA_relaxed`, and `SOPMOA_bucket`

Threading note:

- `SOPMOA` currently clamps `-n` to at most `12` threads.
- `SOPMOA_relaxed` uses the requested `-n` directly.
- `SOPMOA_bucket` currently clamps `-n` to at most `16` threads.
- The other solvers ignore `-n` in the current CLI.

## Quick Start

Create a local output directory and run a single 3-objective query:

```bash
mkdir -p output

./Release/main \
  -m maps/NY-d.txt maps/NY-t.txt maps/NY-m.txt \
  -s 88959 \
  -t 96072 \
  -o output/sopmoa_smoke.csv \
  -a SOPMOA \
  --timelimit 2 \
  -n 4
```

Run the first query from a scenario file and dump the Pareto front:

```bash
mkdir -p output/SOPMOA

./Release/main \
  -m maps/NY-d.txt maps/NY-t.txt maps/NY-m.txt \
  --scenario scen/query3.json \
  --from 0 \
  --to 1 \
  -o output/sopmoa_query3.csv \
  --logsols output/SOPMOA \
  -a SOPMOA \
  --timelimit 2 \
  -n 4
```

## Input Data

### Graph Files

The loader expects DIMACS-style arc files:

```text
c optional comment
a <from> <to> <weight>
```

When you provide multiple map files, they must contain the same edge list in the same order. The loader aligns objective values by edge position and checks that each `(from, to)` pair matches across files.

Example map files in this repository:

- `maps/NY-d.txt`
- `maps/NY-t.txt`
- `maps/NY-m.txt`
- `maps/NY-r.txt`
- `maps/NY-l.txt`

### Scenario JSON

Scenario files are JSON arrays of objects with `name`, `start_data`, and `end_data`:

```json
[
  { "name": "query3_1", "start_data": 88959, "end_data": 96072 },
  { "name": "query3_2", "start_data": 172570, "end_data": 197762 }
]
```

## Output Format

The current CSV output format is:

```text
solver_name,start,target,generated,expanded,num_solutions,runtime_sec
```

Example:

```text
SOPMOA(3obj|4threads)-custom_list,88959,96072,105082,102946,380,2.00611
```

Each run also prints `Expansions/sec` to stdout so throughput can be compared under the same time budget without post-processing the CSV.

If `--logsols` is enabled, each solution file contains one Pareto vector per line:

```text
[12,45,78]
[13,43,81]
```

## Common Wall-Clock Benchmark Protocol On April 4, 2026

The commands used below are mirrored in `run_command.txt` and automated by `scripts/benchmark_wall_clock.py`.

Benchmark setup:

- binary: `./Release/main`
- maps: `maps/NY-d.txt maps/NY-t.txt maps/NY-m.txt`
- query: `start=88959`, `target=96072`
- requested budget: `--timelimit 2`
- statistic: median over `5` runs
- cutoff semantics: every solver now checks the same `steady_clock` wall-clock budget internally

Important caveats:

- This is still a local machine benchmark, not a publication-grade evaluation.
- Small runtime overshoot can remain because each solver checks the cutoff once per main loop iteration; on this machine `NWMOA` landed at about `2.02s`.
- `SOPMOA_bucket` is excluded because it still aborts locally on the benchmark query.

Canonical reproduction command:

```bash
mkdir -p output/bench-wallclock
python3 scripts/benchmark_wall_clock.py --runs 5 | tee output/bench-wallclock/benchmark.txt
```

### 1-Thread Comparison

All solvers were run under the same 2-second wall-clock cutoff. For the parallel solvers, this table uses `-n 1`.

| Algorithm | Threads | Median runtime (s) | Median expansions/sec | Median expanded | Median generated | Median solutions |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `SOPMOA` | 1 | 2.00146 | 30,912 | 61,868 | 62,777 | 256 |
| `SOPMOA_relaxed` | 1 | 2.00100 | 255,575 | 511,412 | 546,582 | 1,283 |
| `LTMOA` | 1 | 2.00002 | 105,555 | 211,112 | 217,865 | 714 |
| `LazyLTMOA` | 1 | 2.00002 | 117,660 | 238,166 | 342,488 | 779 |
| `LTMOA_array` | 1 | 2.00001 | 461,069 | 922,144 | 1,006,260 | 1,885 |
| `LazyLTMOA_array` | 1 | 2.00094 | 386,745 | 773,827 | 1,341,390 | 1,704 |
| `EMOA` | 1 | 2.00002 | 220,373 | 440,749 | 465,899 | 1,189 |
| `NWMOA` | 1 | 2.01987 | 189,676 | 382,975 | 489,887 | 1,078 |

### 4-Thread Scalability

This table compares only the solvers that currently consume `-n`.

| Algorithm | 1-thread runtime (s) | 1-thread expansions/sec | 4-thread runtime (s) | 4-thread expansions/sec | Exp/sec scale |
| --- | ---: | ---: | ---: | ---: | ---: |
| `SOPMOA` | 2.00146 | 30,912 | 2.00170 | 65,168 | 2.11x |
| `SOPMOA_relaxed` | 2.00100 | 255,575 | 2.00584 | 801,846 | 3.14x |

Observed locally:

- `LTMOA_array` remains the fastest exact baseline in the fair 1-thread comparison.
- `SOPMOA_relaxed` is substantially faster than `LTMOA`, `LazyLTMOA`, and the original `SOPMOA`, but still trails the array-based baselines on single-thread throughput.
- On this NYC query, the current `SOPMOA_relaxed` also beats `LTMOA_array` once `-n 4` is enabled, reaching about `1.74x` the serial `LTMOA_array` throughput under the same 2-second wall-clock budget.
- The latest work-first scheduler plus blocked skyline backend improved `SOPMOA_relaxed` scalability from roughly `1.10x` to about `3.14x` on this benchmark query.
- Runtime is now close to the requested `2s` for every solver, so throughput (`expansions/sec`) is the more meaningful comparison column.

### Longer Diverse Suite

The single-query benchmark above is useful for stability checks, but it is too narrow to judge cross-query behavior. The repository now also includes `scripts/benchmark_suite.py` for longer budgets and multiple scenario files.

Canonical all-solver suite command:

```bash
mkdir -p output/bench-suite

python3 scripts/benchmark_suite.py \
  --scenario-files scen/query2.json scen/query3.json scen/query4.json scen/query5.json scen/query6.json \
  --queries-per-scenario 1 \
  --time-limit 5 \
  --runs 1 | tee output/bench-suite/full-suite.txt
```

Focused head-to-head run measured locally on April 4, 2026:

- suite: first `2` queries from each of `scen/query2.json` to `scen/query6.json`
- total query families: `10`
- budget: `--timelimit 5`
- repeats: `1`
- compared solvers: `SOPMOA_relaxed` and `LTMOA_array`

Command:

```bash
mkdir -p output/bench-suite

python3 scripts/benchmark_suite.py \
  --algorithms SOPMOA_relaxed LTMOA_array \
  --scenario-files scen/query2.json scen/query3.json scen/query4.json scen/query5.json scen/query6.json \
  --queries-per-scenario 2 \
  --time-limit 5 \
  --runs 1 | tee output/bench-suite/relaxed-vs-ltmoa-array.txt
```

Aggregate result on that suite:

| Pair | Queries | Median exp/sec ratio | Median expanded ratio | Exp/sec wins | Solution wins |
| --- | ---: | ---: | ---: | ---: | ---: |
| `SOPMOA_relaxed (1t)` vs `LTMOA_array (1t)` | 10 | 0.54x | 0.88x | 0-10 | 0-5 |
| `SOPMOA_relaxed (4t)` vs `LTMOA_array (1t)` | 10 | 1.14x | 2.15x | 7-3 | 0-7 |

Aggregate medians for the same suite:

| Algorithm | Threads | Queries | Median runtime (s) | Median expansions/sec | Median expanded | Median generated | Median solutions |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `SOPMOA_relaxed` | 1 | 10 | 4.72818 | 174,323 | 467,745 | 575,001 | 1,840 |
| `SOPMOA_relaxed` | 4 | 10 | 5.00474 | 447,528 | 1,827,848 | 2,160,692 | 688 |
| `LTMOA_array` | 1 | 10 | 2.68740 | 385,109 | 836,463 | 997,235 | 2,134 |

Interpretation:

- On this broader 10-query suite, `SOPMOA_relaxed` still loses to `LTMOA_array` at `1` thread, but its `4`-thread configuration now pulls ahead on aggregate throughput.
- The larger suite is materially harsher than the single NYC query: `SOPMOA_relaxed` wins throughput on `7/10` queries at `4` threads, not all of them.
- Several `LTMOA_array` runs finish before the 5-second budget, so runtime becomes meaningful again in the diverse-suite setting, not just `expansions/sec`.

If you want a heavier suite, increase `--queries-per-scenario`, `--runs`, and `--time-limit`. For example, `--queries-per-scenario 2 --runs 2 --time-limit 10` gives a much stronger but longer local benchmark.

## Project Layout

```text
.
├── CMakeLists.txt
├── README.md
├── run_command.txt
├── scripts/
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

## Cite

If you use SOPMOA* or this codebase in academic work, please cite:

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

This repository builds on prior exact MOSP solvers in the literature, especially LTMOA, EMOA, and NWMOA, and uses Boost, OpenMP, and oneTBB in the implementation.
