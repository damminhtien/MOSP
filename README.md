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

If `--logsols` is enabled, each solution file contains one Pareto vector per line:

```text
[12,45,78]
[13,43,81]
```

## Local Benchmark On April 4, 2026

The commands used below are also mirrored in `run_command.txt`.

Benchmark setup:

- binary: `./Release/main`
- maps: `maps/NY-d.txt maps/NY-t.txt maps/NY-m.txt`
- query: `start=88959`, `target=96072`
- requested budget: `--timelimit 2`
- thread flag: `-n 4`

Important caveats:

- This is a local smoke benchmark, not a publication-grade evaluation.
- Several baseline solvers exceeded the requested 2-second budget before their next termination check, so their recorded wall times are closer to `3.4-4.3s`.
- `SOPMOA` and `SOPMOA_relaxed` are the only solvers in the tables below that actually use the `-n 4` setting.

Reproduction command:

```bash
mkdir -p output/bench

for ALG in SOPMOA LTMOA LazyLTMOA LTMOA_array LazyLTMOA_array EMOA NWMOA; do
  ./Release/main \
    -m maps/NY-d.txt maps/NY-t.txt maps/NY-m.txt \
    -s 88959 \
    -t 96072 \
    -o "output/bench/${ALG}.csv" \
    -a "$ALG" \
    --timelimit 2 \
    -n 4
done
```

`SOPMOA_relaxed` 5-run comparison used for the new solver validation:

```bash
for ALG in SOPMOA LTMOA SOPMOA_relaxed; do
  for RUN in 1 2 3 4 5; do
    ./Release/main \
      -m maps/NY-d.txt maps/NY-t.txt maps/NY-m.txt \
      -s 88959 \
      -t 96072 \
      -o "output/bench/${ALG}-${RUN}.csv" \
      -a "$ALG" \
      --timelimit 2 \
      -n 4
  done
done
```

Median over 5 runs on this machine:

| Algorithm | Threads used | Median runtime (s) | Avg runtime (s) | Representative solutions |
| --- | ---: | ---: | ---: | ---: |
| `SOPMOA_relaxed` | 4 | 2.00048 | 2.00055 | 1,429 |
| `SOPMOA` | 4 | 2.00418 | 2.00490 | 400 |
| `LTMOA` | 1 | 3.47814 | 3.76170 | 876 |

Observed locally for the new solver:

- `SOPMOA_relaxed` matched `LTMOA` exactly on synthetic 2/3/4-objective test graphs for both `-n 1` and `-n 4`.
- On the NYC benchmark above, `SOPMOA_relaxed` beat `LTMOA` on median wall-clock and processed substantially more labels within the same 2-second budget.
- Compared with the original `SOPMOA`, `SOPMOA_relaxed` stayed on budget while expanding roughly `5-6x` more labels in the same setup.

Measured results on this machine:

| Algorithm | Threads used | Runtime (s) | Generated | Expanded | Solutions |
| --- | ---: | ---: | ---: | ---: | ---: |
| `SOPMOA` | 4 | 2.00611 | 105,082 | 102,946 | 380 |
| `SOPMOA_relaxed` | 4 | 2.00048 median over 5 runs | about 699,497 avg generated | about 657,426 avg expanded | about 1,395 avg |
| `LTMOA` | 1 | 3.46159 | 317,047 | 303,935 | 932 |
| `LazyLTMOA` | 1 | 3.87518 | 455,678 | 304,071 | 932 |
| `LTMOA_array` | 1 | 3.51381 | 1,074,754 | 982,023 | 1,936 |
| `LazyLTMOA_array` | 1 | 4.14031 | 1,119,471 | 664,652 | 1,545 |
| `EMOA` | 1 | 4.14829 | 445,188 | 421,791 | 1,142 |
| `NWMOA` | 1 | 4.1424 | 454,791 | 357,704 | 1,029 |

Observed locally:

- `LTMOA_array` produced the largest number of solutions in the one-shot serial comparison.
- `SOPMOA_relaxed` achieved the best wall-clock among the exact solvers tested repeatedly in the new 5-run comparison.
- `SOPMOA_bucket` was not included in the table because it aborted locally with exit code `134` and produced no CSV output on the same query.

## Project Layout

```text
.
├── CMakeLists.txt
├── README.md
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
