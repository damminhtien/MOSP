#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import os
import platform
import re
import shlex
import subprocess
import sys
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def write_json(path: Path, payload: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2, sort_keys=True)
        handle.write("\n")


def sanitize_component(value: Any) -> str:
    sanitized = re.sub(r"[^A-Za-z0-9._-]+", "_", str(value).strip())
    sanitized = sanitized.strip("._-")
    return sanitized or "value"


def format_budget_component(value: float) -> str:
    return sanitize_component(f"{value:g}".replace(".", "p"))


def run_capture(command: list[str], cwd: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=cwd,
        text=True,
        capture_output=True,
        check=False,
    )


def git_capture(repo_root: Path, *args: str) -> str:
    result = run_capture(["git", *args], repo_root)
    if result.returncode != 0:
        return ""
    return result.stdout.strip()


def parse_cmake_cache(cache_path: Path) -> dict[str, str]:
    if not cache_path.exists():
        return {}

    values: dict[str, str] = {}
    with cache_path.open("r", encoding="utf-8") as handle:
        for raw_line in handle:
            line = raw_line.strip()
            if not line or line.startswith("#") or line.startswith("//"):
                continue
            if "=" not in line or ":" not in line:
                continue
            key_and_type, value = line.split("=", 1)
            key, _cache_type = key_and_type.split(":", 1)
            values[key] = value
    return values


def summary_row_count(summary_path: Path) -> int:
    if not summary_path.exists():
        return 0

    with summary_path.open("r", encoding="utf-8") as handle:
        line_count = sum(1 for _ in handle)
    return max(0, line_count - 1)


def build_scenario_payload(queries: list[dict[str, Any]]) -> list[dict[str, Any]]:
    payload = []
    for query in queries:
        payload.append(
            {
                "name": query["query_id"],
                "start_data": query["start"],
                "end_data": query["target"],
            }
        )
    return payload


@dataclass(frozen=True)
class SolverSpec:
    name: str
    thread_counts: list[int]


@dataclass(frozen=True)
class RunSpec:
    run_id: str
    mode: str
    execution_kind: str
    solver_name: str
    thread_count: int
    dataset: dict[str, Any]
    query_set: dict[str, Any]
    query: dict[str, Any] | None
    budget_sec: float
    repeat_index: int
    binary_path: Path
    build_dir: Path
    export_frontier: bool
    export_trace: bool
    trace_interval_ms: int
    seed: str
    suite_dir: Path
    git_sha: str


def resolve_solver_specs(config: dict[str, Any]) -> list[SolverSpec]:
    global_thread_counts = config.get("thread_counts")
    solver_specs: list[SolverSpec] = []

    for entry in config["solvers"]:
        if isinstance(entry, str):
            solver_specs.append(SolverSpec(name=entry, thread_counts=list(global_thread_counts or [1])))
            continue

        thread_counts = entry.get("thread_counts", global_thread_counts or [1])
        solver_specs.append(SolverSpec(name=entry["name"], thread_counts=list(thread_counts)))

    return solver_specs


def validate_config(config: dict[str, Any], solver_specs: list[SolverSpec]) -> None:
    mode = config["mode"]
    budgets = config["budgets_sec"]

    if mode == "completion" and len(budgets) != 1:
        raise ValueError("completion mode expects exactly one budget ceiling")
    if mode == "scaling":
        if len(budgets) != 1:
            raise ValueError("scaling mode expects exactly one budget")
        if not any(len(spec.thread_counts) > 1 for spec in solver_specs):
            raise ValueError("scaling mode expects at least one solver with multiple thread counts")

    execution_kind = config["execution"]["kind"]
    if execution_kind not in {"scenario", "per_query"}:
        raise ValueError("execution.kind must be scenario or per_query")


def build_environment_manifest(repo_root: Path, build_dir: Path, binary_path: Path) -> dict[str, Any]:
    cmake_cache = parse_cmake_cache(build_dir / "CMakeCache.txt")
    compiler = cmake_cache.get("CMAKE_CXX_COMPILER", "")
    compiler_version = ""

    if compiler:
        compiler_result = run_capture([compiler, "--version"], repo_root)
        if compiler_result.returncode == 0:
            compiler_version = compiler_result.stdout.strip()

    git_sha = git_capture(repo_root, "rev-parse", "HEAD")
    git_short_sha = git_capture(repo_root, "rev-parse", "--short", "HEAD")
    git_branch = git_capture(repo_root, "rev-parse", "--abbrev-ref", "HEAD")
    git_dirty = bool(git_capture(repo_root, "status", "--porcelain"))

    return {
        "captured_at_utc": utc_now_iso(),
        "machine": {
            "hostname": platform.node(),
            "system": platform.system(),
            "release": platform.release(),
            "version": platform.version(),
            "machine": platform.machine(),
            "processor": platform.processor(),
            "cpu_count": os.cpu_count(),
        },
        "python": {
            "executable": sys.executable,
            "version": sys.version,
        },
        "git": {
            "sha": git_sha,
            "short_sha": git_short_sha,
            "branch": git_branch,
            "dirty": git_dirty,
        },
        "build": {
            "build_dir": str(build_dir),
            "binary": str(binary_path),
            "build_type": cmake_cache.get("CMAKE_BUILD_TYPE", ""),
            "cxx_compiler": compiler,
            "cxx_compiler_version": compiler_version,
            "cxx_flags": cmake_cache.get("CMAKE_CXX_FLAGS", ""),
            "cxx_flags_debug": cmake_cache.get("CMAKE_CXX_FLAGS_DEBUG", ""),
            "cxx_flags_release": cmake_cache.get("CMAKE_CXX_FLAGS_RELEASE", ""),
            "prefix_path": cmake_cache.get("CMAKE_PREFIX_PATH", ""),
        },
    }


def load_dataset_manifest(repo_root: Path, dataset_id: str) -> dict[str, Any]:
    manifest_path = repo_root / "bench" / "manifests" / "datasets" / f"{dataset_id}.json"
    manifest = load_json(manifest_path)
    if manifest["dataset_id"] != dataset_id:
        raise ValueError(f"dataset manifest mismatch for {dataset_id}")
    return manifest


def load_query_manifest(repo_root: Path, query_set_id: str) -> dict[str, Any]:
    manifest_path = repo_root / "bench" / "manifests" / "query_sets" / f"{query_set_id}.json"
    manifest = load_json(manifest_path)
    if manifest["query_set_id"] != query_set_id:
        raise ValueError(f"query-set manifest mismatch for {query_set_id}")
    return manifest


def build_run_specs(
    repo_root: Path,
    config: dict[str, Any],
    solver_specs: list[SolverSpec],
    dataset_manifests: dict[str, dict[str, Any]],
    query_set_manifests: dict[str, dict[str, Any]],
    suite_dir: Path,
    binary_path: Path,
    build_dir: Path,
    git_short_sha: str,
) -> list[RunSpec]:
    execution_kind = config["execution"]["kind"]
    repeat_count = int(config["repeat_count"])
    export_frontier = bool(config["output"]["export_frontier"])
    export_trace = bool(config["output"]["export_trace"])
    trace_interval_ms = int(config.get("trace_interval_ms", 0))
    seed = str(config.get("seed", ""))
    mode = config["mode"]

    run_specs: list[RunSpec] = []
    for dataset_id in config["datasets"]:
        dataset = dataset_manifests[dataset_id]
        for query_set_id in config["query_sets"]:
            query_set = query_set_manifests[query_set_id]
            if query_set["dataset_id"] != dataset_id:
                continue

            queries = query_set["queries"]
            for solver in solver_specs:
                for thread_count in solver.thread_counts:
                    for budget_sec in config["budgets_sec"]:
                        for repeat_index in range(repeat_count):
                            if execution_kind == "scenario":
                                query_scope = None
                                query_component = "scenario"
                                run_id = "__".join(
                                    [
                                        sanitize_component(mode),
                                        sanitize_component(solver.name),
                                        f"t{thread_count}",
                                        sanitize_component(dataset_id),
                                        sanitize_component(query_set_id),
                                        query_component,
                                        f"b{format_budget_component(budget_sec)}",
                                        f"r{repeat_index + 1:02d}",
                                        f"g{sanitize_component(git_short_sha or 'nogit')}",
                                    ]
                                )
                                run_specs.append(
                                    RunSpec(
                                        run_id=run_id,
                                        mode=mode,
                                        execution_kind=execution_kind,
                                        solver_name=solver.name,
                                        thread_count=thread_count,
                                        dataset=dataset,
                                        query_set=query_set,
                                        query=query_scope,
                                        budget_sec=float(budget_sec),
                                        repeat_index=repeat_index,
                                        binary_path=binary_path,
                                        build_dir=build_dir,
                                        export_frontier=export_frontier,
                                        export_trace=export_trace,
                                        trace_interval_ms=trace_interval_ms,
                                        seed=seed,
                                        suite_dir=suite_dir,
                                        git_sha=git_short_sha,
                                    )
                                )
                            else:
                                for query in queries:
                                    run_id = "__".join(
                                        [
                                            sanitize_component(mode),
                                            sanitize_component(solver.name),
                                            f"t{thread_count}",
                                            sanitize_component(dataset_id),
                                            sanitize_component(query_set_id),
                                            sanitize_component(query["query_id"]),
                                            f"b{format_budget_component(budget_sec)}",
                                            f"r{repeat_index + 1:02d}",
                                            f"g{sanitize_component(git_short_sha or 'nogit')}",
                                        ]
                                    )
                                    run_specs.append(
                                        RunSpec(
                                            run_id=run_id,
                                            mode=mode,
                                            execution_kind=execution_kind,
                                            solver_name=solver.name,
                                            thread_count=thread_count,
                                            dataset=dataset,
                                            query_set=query_set,
                                            query=query,
                                            budget_sec=float(budget_sec),
                                            repeat_index=repeat_index,
                                            binary_path=binary_path,
                                            build_dir=build_dir,
                                            export_frontier=export_frontier,
                                            export_trace=export_trace,
                                            trace_interval_ms=trace_interval_ms,
                                            seed=str(query.get("seed", seed)),
                                            suite_dir=suite_dir,
                                            git_sha=git_short_sha,
                                        )
                                    )

    return run_specs


def build_command(spec: RunSpec, run_dir: Path) -> tuple[list[str], int]:
    dataset_files = [str(spec.suite_dir.parents[2] / path) for path in spec.dataset["objective_files"]]
    summary_path = run_dir / "summary.csv"
    frontier_dir = run_dir / "frontiers"
    trace_dir = run_dir / "traces"

    command = [
        str(spec.binary_path),
        "-m",
        *dataset_files,
        "-a",
        spec.solver_name,
        "--budget-sec",
        f"{spec.budget_sec:g}",
        "--summary-output",
        str(summary_path),
        "--dataset-id",
        spec.dataset["dataset_id"],
        "--trace-interval-ms",
        str(spec.trace_interval_ms),
    ]

    if spec.thread_count > 0:
        command.extend(["-n", str(spec.thread_count)])
    if spec.seed:
        command.extend(["--seed", spec.seed])
    if spec.export_frontier:
        command.extend(["--frontier-output-dir", str(frontier_dir)])
    if spec.export_trace:
        command.extend(["--trace-output-dir", str(trace_dir)])

    expected_rows = 1
    if spec.execution_kind == "scenario":
        scenario_path = run_dir / "scenario.json"
        write_json(scenario_path, build_scenario_payload(spec.query_set["queries"]))
        command.extend(
            [
                "--scenario",
                str(scenario_path),
                "--from",
                "0",
                "--to",
                str(len(spec.query_set["queries"])),
                "--query-id",
                spec.run_id,
            ]
        )
        expected_rows = len(spec.query_set["queries"])
    else:
        assert spec.query is not None
        command.extend(
            [
                "-s",
                str(spec.query["start"]),
                "-t",
                str(spec.query["target"]),
                "--query-id",
                spec.run_id,
            ]
        )

    return command, expected_rows


def execute_run(spec: RunSpec, failure_policy: dict[str, Any]) -> dict[str, Any]:
    max_retries = int(failure_policy.get("max_retries", 0))
    run_dir = spec.suite_dir / "runs" / spec.run_id
    run_dir.mkdir(parents=True, exist_ok=True)

    run_manifest = {
        "run_id": spec.run_id,
        "mode": spec.mode,
        "execution_kind": spec.execution_kind,
        "solver": spec.solver_name,
        "thread_count": spec.thread_count,
        "dataset_id": spec.dataset["dataset_id"],
        "query_set_id": spec.query_set["query_set_id"],
        "query": spec.query,
        "budget_sec": spec.budget_sec,
        "repeat_index": spec.repeat_index,
        "git_sha": spec.git_sha,
        "created_at_utc": utc_now_iso(),
    }
    write_json(run_dir / "run.json", run_manifest)

    command, expected_rows = build_command(spec, run_dir)
    write_json(run_dir / "command.json", {"argv": command, "shell": " ".join(shlex.quote(arg) for arg in command)})

    status_payload: dict[str, Any] = {
        "run_id": spec.run_id,
        "attempts": [],
        "expected_summary_rows": expected_rows,
        "status": "pending",
    }

    for attempt_index in range(max_retries + 1):
        stdout_path = run_dir / "stdout.log"
        stderr_path = run_dir / "stderr.log"
        summary_path = run_dir / "summary.csv"

        if summary_path.exists():
            summary_path.unlink()

        with stdout_path.open("w", encoding="utf-8") as stdout_handle, stderr_path.open("w", encoding="utf-8") as stderr_handle:
            result = subprocess.run(
                command,
                cwd=spec.suite_dir.parents[2],
                text=True,
                stdout=stdout_handle,
                stderr=stderr_handle,
                check=False,
            )

        observed_rows = summary_row_count(summary_path)
        attempt_payload = {
            "attempt_index": attempt_index,
            "returncode": result.returncode,
            "observed_summary_rows": observed_rows,
        }
        status_payload["attempts"].append(attempt_payload)

        if result.returncode == 0 and observed_rows == expected_rows:
            status_payload["status"] = "completed"
            status_payload["completed_at_utc"] = utc_now_iso()
            status_payload["summary_path"] = str(summary_path)
            write_json(run_dir / "status.json", status_payload)
            return status_payload

    status_payload["status"] = "failed"
    status_payload["completed_at_utc"] = utc_now_iso()
    write_json(run_dir / "status.json", status_payload)

    if failure_policy.get("on_failure", "fail_fast") == "fail_fast":
        raise RuntimeError(f"benchmark run failed: {spec.run_id}")
    return status_payload


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run manifest-driven benchmarks")
    parser.add_argument("--config", required=True, help="Path to benchmark config")
    parser.add_argument("--suite-id", default="", help="Optional explicit suite id")
    parser.add_argument("--results-root", default="", help="Optional override for result root")
    parser.add_argument("--build-dir", default="", help="Optional override for build directory")
    parser.add_argument("--dry-run", action="store_true", help="Write plan only, do not execute runs")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = Path(__file__).resolve().parents[2]
    config_path = (repo_root / args.config).resolve()
    config = load_json(config_path)

    solver_specs = resolve_solver_specs(config)
    validate_config(config, solver_specs)

    build_dir = (repo_root / (args.build_dir or config.get("build_dir", "Release"))).resolve()
    binary_path = (repo_root / config.get("binary", "./Release/main")).resolve()
    results_root = (repo_root / (args.results_root or config["output"]["root"])).resolve()

    git_short_sha = git_capture(repo_root, "rev-parse", "--short", "HEAD")
    config_stem = sanitize_component(config_path.stem)
    default_suite_id = "__".join(
        [
            datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ"),
            sanitize_component(config["mode"]),
            config_stem,
            sanitize_component(git_short_sha or "nogit"),
        ]
    )
    suite_id = sanitize_component(args.suite_id or default_suite_id)
    suite_dir = results_root / suite_id
    suite_dir.mkdir(parents=True, exist_ok=True)

    dataset_manifests = {dataset_id: load_dataset_manifest(repo_root, dataset_id) for dataset_id in config["datasets"]}
    query_set_manifests = {query_set_id: load_query_manifest(repo_root, query_set_id) for query_set_id in config["query_sets"]}

    environment_manifest = build_environment_manifest(repo_root, build_dir, binary_path)
    write_json(suite_dir / "environment.json", environment_manifest)

    resolved_config = {
        "config_path": str(config_path.relative_to(repo_root)),
        "suite_id": suite_id,
        "loaded_at_utc": utc_now_iso(),
        "config": config,
        "datasets": dataset_manifests,
        "query_sets": query_set_manifests,
    }
    write_json(suite_dir / "resolved_config.json", resolved_config)

    run_specs = build_run_specs(
        repo_root=repo_root,
        config=config,
        solver_specs=solver_specs,
        dataset_manifests=dataset_manifests,
        query_set_manifests=query_set_manifests,
        suite_dir=suite_dir,
        binary_path=binary_path,
        build_dir=build_dir,
        git_short_sha=git_short_sha,
    )

    run_plan = []
    for spec in run_specs:
        run_plan.append(
            {
                "run_id": spec.run_id,
                "mode": spec.mode,
                "execution_kind": spec.execution_kind,
                "solver": spec.solver_name,
                "thread_count": spec.thread_count,
                "dataset_id": spec.dataset["dataset_id"],
                "query_set_id": spec.query_set["query_set_id"],
                "query_id": "" if spec.query is None else spec.query["query_id"],
                "budget_sec": spec.budget_sec,
                "repeat_index": spec.repeat_index,
                "git_sha": spec.git_sha,
            }
        )
    write_json(suite_dir / "run_plan.json", run_plan)

    if args.dry_run:
        print(f"Dry run created suite plan at {suite_dir}")
        return 0

    failure_policy = config["failure_policy"]
    run_results = []
    for spec in run_specs:
        result = execute_run(spec, failure_policy)
        run_results.append(result)

    write_json(suite_dir / "run_results.json", run_results)
    completed = sum(1 for result in run_results if result["status"] == "completed")
    print(f"Completed {completed}/{len(run_results)} runs")
    print(f"Suite output: {suite_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
