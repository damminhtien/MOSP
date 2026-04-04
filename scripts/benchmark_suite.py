#!/usr/bin/env python3

import argparse
import json
import re
import statistics
import subprocess
from collections import defaultdict
from pathlib import Path


DEFAULT_ALGORITHMS = [
    "SOPMOA",
    "SOPMOA_relaxed",
    "LTMOA",
    "LazyLTMOA",
    "LTMOA_array",
    "LazyLTMOA_array",
    "EMOA",
    "NWMOA",
]

PARALLEL_ALGORITHMS = {
    "SOPMOA",
    "SOPMOA_relaxed",
}

DEFAULT_SCENARIO_FILES = [
    "scen/query2.json",
    "scen/query3.json",
    "scen/query4.json",
    "scen/query5.json",
    "scen/query6.json",
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


def safe_slug(text: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]+", "-", text).strip("-")


def median(values):
    return statistics.median(values) if values else 0.0


def load_queries(repo_root: Path, args) -> list[dict]:
    if args.scenario_files:
        queries = []
        for scenario_path_text in args.scenario_files:
            scenario_path = (repo_root / scenario_path_text).resolve()
            data = json.loads(scenario_path.read_text())
            limit = len(data) if args.queries_per_scenario <= 0 else min(len(data), args.queries_per_scenario)
            for index, entry in enumerate(data[:limit]):
                query_name = f"{scenario_path.stem}:{index}:{entry['name']}"
                queries.append(
                    {
                        "id": query_name,
                        "slug": safe_slug(query_name),
                        "scenario_file": scenario_path_text,
                        "start": int(entry["start_data"]),
                        "target": int(entry["end_data"]),
                    }
                )
        return queries

    return [
        {
            "id": f"single:{args.start}->{args.target}",
            "slug": f"single-{args.start}-{args.target}",
            "scenario_file": "",
            "start": args.start,
            "target": args.target,
        }
    ]


def thread_counts_for_algorithm(algorithm: str, args) -> list[int]:
    if algorithm in PARALLEL_ALGORITHMS:
        return args.parallel_threads
    return [1]


def run_solver(args, query: dict, algorithm: str, threads: int, run_index: int) -> dict:
    output_csv = args.output_dir / f"{algorithm}-t{threads}-{query['slug']}-run{run_index}.csv"
    command = [
        str(args.binary),
        "-m",
        *args.maps,
        "-s",
        str(query["start"]),
        "-t",
        str(query["target"]),
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
    metrics["query_id"] = query["id"]
    metrics["run"] = run_index
    metrics["start"] = query["start"]
    metrics["target"] = query["target"]
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


def summarize_query_group(query_group: dict[str, list[dict]]) -> dict:
    per_query = {query_id: summarize_runs(runs) for query_id, runs in query_group.items()}
    return {
        "queries": len(per_query),
        "per_query": per_query,
        "median_runtime_sec": median([summary["median_runtime_sec"] for summary in per_query.values()]),
        "median_expansions_per_sec": median([summary["median_expansions_per_sec"] for summary in per_query.values()]),
        "median_expanded": median([summary["median_expanded"] for summary in per_query.values()]),
        "median_generated": median([summary["median_generated"] for summary in per_query.values()]),
        "median_solutions": median([summary["median_solutions"] for summary in per_query.values()]),
    }


def render_single_thread_table(results: dict, algorithms: list[str]) -> str:
    lines = [
        "| Algorithm | Queries | Median runtime (s) | Median expansions/sec | Median expanded | Median generated | Median solutions |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for algorithm in algorithms:
        summary = results[algorithm][1]
        lines.append(
            f"| {algorithm} | {summary['queries']} | {summary['median_runtime_sec']:.5f} | "
            f"{summary['median_expansions_per_sec']:.0f} | {int(summary['median_expanded'])} | "
            f"{int(summary['median_generated'])} | {int(summary['median_solutions'])} |"
        )
    return "\n".join(lines)


def render_scalability_table(results: dict, algorithms: list[str]) -> str:
    lines = [
        "| Algorithm | Queries | 1-thread runtime (s) | 1-thread expansions/sec | 4-thread runtime (s) | 4-thread expansions/sec | Exp/sec scale |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for algorithm in algorithms:
        if 4 not in results[algorithm]:
            continue
        one = results[algorithm][1]
        four = results[algorithm][4]
        scale = (
            four["median_expansions_per_sec"] / one["median_expansions_per_sec"]
            if one["median_expansions_per_sec"] > 0
            else 0.0
        )
        lines.append(
            f"| {algorithm} | {one['queries']} | {one['median_runtime_sec']:.5f} | "
            f"{one['median_expansions_per_sec']:.0f} | {four['median_runtime_sec']:.5f} | "
            f"{four['median_expansions_per_sec']:.0f} | {scale:.2f}x |"
        )
    return "\n".join(lines)


def pairwise_summary(results: dict, left_algorithm: str, left_threads: int, right_algorithm: str, right_threads: int) -> dict:
    left = results[left_algorithm][left_threads]["per_query"]
    right = results[right_algorithm][right_threads]["per_query"]
    common_queries = sorted(set(left) & set(right))

    ratio_exp_s = []
    ratio_expanded = []
    left_wins_exp_s = 0
    left_wins_solutions = 0
    right_wins_exp_s = 0
    right_wins_solutions = 0

    for query_id in common_queries:
        left_query = left[query_id]
        right_query = right[query_id]

        if right_query["median_expansions_per_sec"] > 0:
            ratio_exp_s.append(left_query["median_expansions_per_sec"] / right_query["median_expansions_per_sec"])
        if right_query["median_expanded"] > 0:
            ratio_expanded.append(left_query["median_expanded"] / right_query["median_expanded"])

        if left_query["median_expansions_per_sec"] > right_query["median_expansions_per_sec"]:
            left_wins_exp_s += 1
        elif left_query["median_expansions_per_sec"] < right_query["median_expansions_per_sec"]:
            right_wins_exp_s += 1

        if left_query["median_solutions"] > right_query["median_solutions"]:
            left_wins_solutions += 1
        elif left_query["median_solutions"] < right_query["median_solutions"]:
            right_wins_solutions += 1

    return {
        "queries": len(common_queries),
        "median_expansions_per_sec_ratio": median(ratio_exp_s),
        "median_expanded_ratio": median(ratio_expanded),
        "left_wins_exp_s": left_wins_exp_s,
        "right_wins_exp_s": right_wins_exp_s,
        "left_wins_solutions": left_wins_solutions,
        "right_wins_solutions": right_wins_solutions,
    }


def render_head_to_head(results: dict, left_algorithm: str, left_threads: int, right_algorithm: str, right_threads: int) -> str:
    summary = pairwise_summary(results, left_algorithm, left_threads, right_algorithm, right_threads)
    lines = [
        f"| Pair | Queries | Median exp/sec ratio | Median expanded ratio | Exp/sec wins | Solution wins |",
        "| --- | ---: | ---: | ---: | ---: | ---: |",
        (
            f"| {left_algorithm} ({left_threads}t) vs {right_algorithm} ({right_threads}t) | "
            f"{summary['queries']} | {summary['median_expansions_per_sec_ratio']:.2f}x | "
            f"{summary['median_expanded_ratio']:.2f}x | "
            f"{summary['left_wins_exp_s']}-{summary['right_wins_exp_s']} | "
            f"{summary['left_wins_solutions']}-{summary['right_wins_solutions']} |"
        ),
    ]
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description="Run a longer, more diverse wall-clock benchmark suite.")
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    parser.add_argument("--binary", type=Path, default=Path("Release/main"))
    parser.add_argument("--output-dir", type=Path, default=Path("output/bench-suite"))
    parser.add_argument("--time-limit", type=int, default=5)
    parser.add_argument("--runs", type=int, default=1)
    parser.add_argument("--start", type=int, default=88959)
    parser.add_argument("--target", type=int, default=96072)
    parser.add_argument("--queries-per-scenario", type=int, default=1)
    parser.add_argument("--scenario-files", nargs="*", default=DEFAULT_SCENARIO_FILES)
    parser.add_argument("--algorithms", nargs="+", default=DEFAULT_ALGORITHMS)
    parser.add_argument("--parallel-threads", nargs="+", type=int, default=[1, 4])
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

    queries = load_queries(args.repo_root, args)
    nested_results: dict[str, dict[int, dict[str, list[dict]]]] = defaultdict(lambda: defaultdict(lambda: defaultdict(list)))

    for algorithm in args.algorithms:
        for threads in thread_counts_for_algorithm(algorithm, args):
            for query in queries:
                for run_index in range(1, args.runs + 1):
                    metrics = run_solver(args, query, algorithm, threads, run_index)
                    nested_results[algorithm][threads][query["id"]].append(metrics)

    results: dict[str, dict[int, dict]] = {}
    for algorithm, thread_groups in nested_results.items():
        results[algorithm] = {}
        for threads, query_group in thread_groups.items():
            results[algorithm][threads] = summarize_query_group(query_group)

    print("# Suite metadata")
    print(
        json.dumps(
            {
                "queries": len(queries),
                "runs_per_query": args.runs,
                "time_limit_sec": args.time_limit,
                "scenario_files": args.scenario_files,
            },
            indent=2,
        )
    )
    print()
    print("# Single-thread aggregate")
    print(render_single_thread_table(results, args.algorithms))
    print()
    print("# 4-thread scalability")
    print(render_scalability_table(results, args.algorithms))
    print()

    if "SOPMOA_relaxed" in results and "LTMOA_array" in results and 1 in results["LTMOA_array"]:
        print("# SOPMOA_relaxed vs LTMOA_array")
        if 1 in results["SOPMOA_relaxed"]:
            print(render_head_to_head(results, "SOPMOA_relaxed", 1, "LTMOA_array", 1))
            print()
        if 4 in results["SOPMOA_relaxed"]:
            print(render_head_to_head(results, "SOPMOA_relaxed", 4, "LTMOA_array", 1))
            print()

    print("# JSON")
    print(json.dumps(results, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
