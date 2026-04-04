#!/usr/bin/env python3

import argparse
import json
import re
import statistics
import subprocess
from pathlib import Path


SINGLE_THREAD_ALGORITHMS = [
    "SOPMOA",
    "SOPMOA_relaxed",
    "LTMOA",
    "LazyLTMOA",
    "LTMOA_array",
    "LazyLTMOA_array",
    "EMOA",
    "NWMOA",
]

PARALLEL_ALGORITHMS = [
    "SOPMOA",
    "SOPMOA_relaxed",
]


def parse_metrics(stdout: str) -> dict:
    patterns = {
        "runtime_sec": r"Runtime:\s+([0-9]+(?:\.[0-9]+)?)",
        "expanded": r"Num expansion:\s+([0-9]+)",
        "generated": r"Num generation:\s+([0-9]+)",
        "solutions": r"Num solution:\s+([0-9]+)",
        "expansions_per_sec": r"Expansions/sec:\s+([0-9]+(?:\.[0-9]+)?)",
    }

    metrics = {}
    for key, pattern in patterns.items():
        match = re.search(pattern, stdout)
        if match is None:
            raise RuntimeError(f"failed to parse {key} from solver output")
        value = float(match.group(1))
        metrics[key] = int(value) if key in {"expanded", "generated", "solutions"} else value
    return metrics


def median(values):
    return statistics.median(values) if values else 0.0


def run_solver(args, algorithm: str, threads: int, run_index: int) -> dict:
    output_csv = args.output_dir / f"{algorithm}-t{threads}-run{run_index}.csv"
    command = [
        str(args.binary),
        "-m",
        *args.maps,
        "-s",
        str(args.start),
        "-t",
        str(args.target),
        "-o",
        str(output_csv),
        "-a",
        algorithm,
        "--timelimit",
        str(args.time_limit),
        "-n",
        str(threads),
    ]
    completed = subprocess.run(
        command,
        cwd=args.repo_root,
        capture_output=True,
        text=True,
        check=True,
    )
    metrics = parse_metrics(completed.stdout)
    metrics["algorithm"] = algorithm
    metrics["threads"] = threads
    metrics["run"] = run_index
    return metrics


def summarize_runs(runs: list[dict]) -> dict:
    return {
        "runs": len(runs),
        "median_runtime_sec": median([run["runtime_sec"] for run in runs]),
        "median_expansions_per_sec": median([run["expansions_per_sec"] for run in runs]),
        "median_expanded": median([run["expanded"] for run in runs]),
        "median_generated": median([run["generated"] for run in runs]),
        "median_solutions": median([run["solutions"] for run in runs]),
    }


def render_single_thread_table(results: dict) -> str:
    lines = [
        "| Algorithm | Threads | Median runtime (s) | Median expansions/sec | Median expanded | Median generated | Median solutions |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for algorithm in SINGLE_THREAD_ALGORITHMS:
        summary = results[algorithm][1]
        lines.append(
            f"| {algorithm} | 1 | {summary['median_runtime_sec']:.5f} | {summary['median_expansions_per_sec']:.0f} | "
            f"{int(summary['median_expanded'])} | {int(summary['median_generated'])} | {int(summary['median_solutions'])} |"
        )
    return "\n".join(lines)


def render_scalability_table(results: dict) -> str:
    lines = [
        "| Algorithm | 1-thread runtime (s) | 1-thread expansions/sec | 4-thread runtime (s) | 4-thread expansions/sec | Exp/sec scale |",
        "| --- | ---: | ---: | ---: | ---: | ---: |",
    ]
    for algorithm in PARALLEL_ALGORITHMS:
        one = results[algorithm][1]
        four = results[algorithm][4]
        scale = (
            four["median_expansions_per_sec"] / one["median_expansions_per_sec"]
            if one["median_expansions_per_sec"] > 0
            else 0.0
        )
        lines.append(
            f"| {algorithm} | {one['median_runtime_sec']:.5f} | {one['median_expansions_per_sec']:.0f} | "
            f"{four['median_runtime_sec']:.5f} | {four['median_expansions_per_sec']:.0f} | {scale:.2f}x |"
        )
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description="Benchmark all solvers with a common wall-clock cutoff protocol.")
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    parser.add_argument("--binary", type=Path, default=Path("Release/main"))
    parser.add_argument("--output-dir", type=Path, default=Path("output/bench-wallclock"))
    parser.add_argument("--start", type=int, default=88959)
    parser.add_argument("--target", type=int, default=96072)
    parser.add_argument("--time-limit", type=int, default=2)
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument(
        "--maps",
        nargs="+",
        default=["maps/NY-d.txt", "maps/NY-t.txt", "maps/NY-m.txt"],
    )
    args = parser.parse_args()

    args.repo_root = args.repo_root.resolve()
    args.binary = (args.repo_root / args.binary).resolve()
    args.output_dir = (args.repo_root / args.output_dir).resolve()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    results: dict[str, dict[int, dict]] = {}

    for algorithm in SINGLE_THREAD_ALGORITHMS:
        results.setdefault(algorithm, {})
        thread_counts = [1]
        if algorithm in PARALLEL_ALGORITHMS:
            thread_counts.append(4)

        for threads in thread_counts:
            runs = []
            for run_index in range(1, args.runs + 1):
                metrics = run_solver(args, algorithm, threads, run_index)
                runs.append(metrics)
            results[algorithm][threads] = summarize_runs(runs)

    print("# Single-thread comparison")
    print(render_single_thread_table(results))
    print()
    print("# 4-thread scalability")
    print(render_scalability_table(results))
    print()
    print("# JSON")
    print(json.dumps(results, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
