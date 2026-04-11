#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import json
import os
import platform
import re
import shlex
import subprocess
import sys
import time
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


SUITE_SUMMARY_FIELDS = [
    "run_id",
    "dataset_id",
    "query_set_id",
    "query_id",
    "solver",
    "reported_solver_name",
    "threads",
    "threads_requested",
    "threads_effective",
    "mode",
    "budget_sec",
    "repeat_index",
    "status",
    "raw_solver_status",
    "completed",
    "exit_code",
    "runtime_sec",
    "generated_labels",
    "expanded_labels",
    "pruned_by_target",
    "pruned_by_node",
    "pruned_other",
    "final_frontier_size",
    "peak_open_size",
    "peak_live_labels",
    "peak_rss_mb",
    "time_to_first_solution_sec",
    "trace_file",
    "frontier_file",
    "stdout_file",
    "stderr_file",
]


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def load_document(path: Path) -> Any:
    raw = read_text(path)
    try:
        return json.loads(raw)
    except json.JSONDecodeError:
        try:
            import yaml  # type: ignore
        except ImportError as exc:
            raise ValueError(
                f"Cannot parse {path}. Install PyYAML for non-JSON YAML or keep configs JSON-compatible."
            ) from exc
        return yaml.safe_load(raw)


def write_json(path: Path, payload: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2, sort_keys=True)
        handle.write("\n")


def write_csv(path: Path, fieldnames: list[str], rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


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
    for raw_line in read_text(cache_path).splitlines():
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
        return max(0, sum(1 for _ in handle) - 1)


def build_scenario_payload(queries: list[dict[str, Any]]) -> list[dict[str, Any]]:
    return [
        {
            "name": query["query_id"],
            "start_data": query["start"],
            "end_data": query["target"],
        }
        for query in queries
    ]


def parse_bool(value: Any) -> bool:
    if isinstance(value, bool):
        return value
    return str(value).strip().lower() == "true"


def parse_float(value: Any, default: float = 0.0) -> float:
    if value in (None, ""):
        return default
    return float(value)


def parse_int(value: Any, default: int = 0) -> int:
    if value in (None, ""):
        return default
    return int(value)


def measure_total_memory_mb() -> float | None:
    if sys.platform == "darwin":
        result = run_capture(["/usr/sbin/sysctl", "-n", "hw.memsize"], Path.cwd())
        if result.returncode == 0:
            try:
                return int(result.stdout.strip()) / (1024.0 * 1024.0)
            except ValueError:
                return None

    if hasattr(os, "sysconf"):
        try:
            page_size = os.sysconf("SC_PAGE_SIZE")
            page_count = os.sysconf("SC_PHYS_PAGES")
            return (page_size * page_count) / (1024.0 * 1024.0)
        except (ValueError, OSError, AttributeError):
            return None

    return None


def measure_process_rss_mb(pid: int) -> float | None:
    result = run_capture(["ps", "-o", "rss=", "-p", str(pid)], Path.cwd())
    if result.returncode != 0:
        return None

    raw = result.stdout.strip()
    if not raw:
        return None

    try:
        rss_kb = int(raw)
    except ValueError:
        return None
    return rss_kb / 1024.0


def normalize_path(value: str | Path) -> str:
    return str(Path(value).resolve())


def optional_path(value: str | Path | None) -> str:
    if value is None:
        return ""
    if isinstance(value, str) and value == "":
        return ""
    return normalize_path(value)


@dataclass(frozen=True)
class SolverSpec:
    name: str
    thread_counts: list[int]
    enabled: bool
    skip_reason: str


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
    wall_timeout_sec: float
    skip_reason: str


def resolve_solver_specs(config: dict[str, Any]) -> list[SolverSpec]:
    global_thread_counts = config.get("thread_counts")
    solver_specs: list[SolverSpec] = []

    for entry in config["solvers"]:
        if isinstance(entry, str):
            solver_specs.append(
                SolverSpec(
                    name=entry,
                    thread_counts=list(global_thread_counts or [1]),
                    enabled=True,
                    skip_reason="",
                )
            )
            continue

        thread_counts = entry.get("thread_counts", global_thread_counts or [1])
        enabled = bool(entry.get("enabled", True))
        solver_specs.append(
            SolverSpec(
                name=entry["name"],
                thread_counts=list(thread_counts),
                enabled=enabled,
                skip_reason=str(entry.get("skip_reason", "")),
            )
        )

    return solver_specs


def validate_config(config: dict[str, Any], solver_specs: list[SolverSpec]) -> None:
    mode = config["mode"]
    budgets = config["budgets_sec"]

    if mode == "completion" and len(budgets) != 1:
        raise ValueError("completion mode expects exactly one budget ceiling")
    if mode == "scaling":
        if len(budgets) != 1:
            raise ValueError("scaling mode expects exactly one budget")
        if not any(len(spec.thread_counts) > 1 for spec in solver_specs if spec.enabled):
            raise ValueError("scaling mode expects at least one enabled solver with multiple thread counts")

    execution_kind = config["execution"]["kind"]
    if execution_kind not in {"scenario", "per_query"}:
        raise ValueError("execution.kind must be scenario or per_query")

    failure_mode = config.get("failure_policy", {}).get("mode", config.get("failure_policy", {}).get("on_failure", "fail_fast"))
    if failure_mode not in {"fail_fast", "continue_on_error"}:
        raise ValueError("failure_policy.mode must be fail_fast or continue_on_error")


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
            "total_memory_mb": measure_total_memory_mb(),
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
    manifest = load_document(manifest_path)
    if manifest["dataset_id"] != dataset_id:
        raise ValueError(f"dataset manifest mismatch for {dataset_id}")
    return manifest


def load_query_manifest(repo_root: Path, query_set_id: str) -> dict[str, Any]:
    manifest_path = repo_root / "bench" / "manifests" / "query_sets" / f"{query_set_id}.json"
    manifest = load_document(manifest_path)
    if manifest["query_set_id"] != query_set_id:
        raise ValueError(f"query-set manifest mismatch for {query_set_id}")
    return manifest


def expected_queries_for_spec(spec: RunSpec) -> list[dict[str, Any]]:
    if spec.execution_kind == "scenario":
        return list(spec.query_set["queries"])
    assert spec.query is not None
    return [spec.query]


def compute_wall_timeout_sec(config: dict[str, Any], budget_sec: float, query_count: int) -> float:
    runtime_control = config.get("runtime_control", {})
    if runtime_control.get("wall_timeout_sec") not in (None, ""):
        return float(runtime_control["wall_timeout_sec"])
    timeout_grace_sec = float(runtime_control.get("timeout_grace_sec", 30.0))
    return max((budget_sec * max(query_count, 1)) + timeout_grace_sec, budget_sec)


def build_run_specs(
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

            for solver in solver_specs:
                for thread_count in solver.thread_counts:
                    for budget_sec in config["budgets_sec"]:
                        for repeat_index in range(repeat_count):
                            if execution_kind == "scenario":
                                wall_timeout_sec = compute_wall_timeout_sec(
                                    config,
                                    float(budget_sec),
                                    len(query_set["queries"]),
                                )
                                run_id = "__".join(
                                    [
                                        sanitize_component(mode),
                                        sanitize_component(solver.name),
                                        f"t{thread_count}",
                                        sanitize_component(dataset_id),
                                        sanitize_component(query_set_id),
                                        "scenario",
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
                                        query=None,
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
                                        wall_timeout_sec=wall_timeout_sec,
                                        skip_reason=solver.skip_reason if not solver.enabled else "",
                                    )
                                )
                            else:
                                for query in query_set["queries"]:
                                    wall_timeout_sec = compute_wall_timeout_sec(config, float(budget_sec), 1)
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
                                            wall_timeout_sec=wall_timeout_sec,
                                            skip_reason=solver.skip_reason if not solver.enabled else "",
                                        )
                                    )

    return run_specs


def build_command(spec: RunSpec, repo_root: Path, run_dir: Path) -> tuple[list[str], int]:
    dataset_files = [str((repo_root / path).resolve()) for path in spec.dataset["objective_files"]]
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


def load_solver_summary_rows(summary_path: Path) -> list[dict[str, str]]:
    if not summary_path.exists():
        return []
    with summary_path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def has_complete_solver_summary(solver_rows: list[dict[str, str]], expected_rows: int) -> bool:
    return expected_rows > 0 and len(solver_rows) == expected_rows


def logical_query_id_from_row(spec: RunSpec, row_query_id: str) -> str:
    prefix = spec.run_id + "__"
    if row_query_id.startswith(prefix):
        return row_query_id[len(prefix):]
    if spec.query is not None:
        return spec.query["query_id"]
    return row_query_id


def suite_status_from_solver_row(raw_status: str) -> str:
    if raw_status == "completed":
        return "completed"
    if raw_status == "timeout":
        return "timeout"
    return "crash"


def synthetic_summary_rows(
    spec: RunSpec,
    status: str,
    exit_code: int,
    runtime_sec: float,
    peak_rss_mb: float,
    stdout_path: Path,
    stderr_path: Path,
) -> list[dict[str, Any]]:
    rows = []
    for query in expected_queries_for_spec(spec):
        synthetic_run_id = spec.run_id if spec.execution_kind == "per_query" else f"{spec.run_id}__{sanitize_component(query['query_id'])}"
        rows.append(
            {
                "run_id": synthetic_run_id,
                "dataset_id": spec.dataset["dataset_id"],
                "query_set_id": spec.query_set["query_set_id"],
                "query_id": query["query_id"],
                "solver": spec.solver_name,
                "reported_solver_name": "",
                "threads": spec.thread_count,
                "threads_requested": spec.thread_count,
                "threads_effective": spec.thread_count,
                "mode": spec.mode,
                "budget_sec": f"{spec.budget_sec:g}",
                "repeat_index": spec.repeat_index + 1,
                "status": status,
                "raw_solver_status": "",
                "completed": "false",
                "exit_code": exit_code,
                "runtime_sec": f"{runtime_sec:.12g}",
                "generated_labels": "",
                "expanded_labels": "",
                "pruned_by_target": "",
                "pruned_by_node": "",
                "pruned_other": "",
                "final_frontier_size": "",
                "peak_open_size": "",
                "peak_live_labels": "",
                "peak_rss_mb": f"{peak_rss_mb:.6f}",
                "time_to_first_solution_sec": "",
                "trace_file": "",
                "frontier_file": "",
                "stdout_file": optional_path(stdout_path),
                "stderr_file": optional_path(stderr_path),
            }
        )
    return rows


def suite_summary_rows_from_solver_summary(
    spec: RunSpec,
    solver_rows: list[dict[str, str]],
    exit_code: int,
    peak_rss_mb: float,
    stdout_path: Path,
    stderr_path: Path,
) -> list[dict[str, Any]]:
    rows = []
    for row in solver_rows:
        logical_query_id = logical_query_id_from_row(spec, row["query_id"])
        rows.append(
            {
                "run_id": row["query_id"],
                "dataset_id": row["dataset_id"],
                "query_set_id": spec.query_set["query_set_id"],
                "query_id": logical_query_id,
                "solver": spec.solver_name,
                "reported_solver_name": row["solver_name"],
                "threads": parse_int(row.get("threads_effective", spec.thread_count), spec.thread_count),
                "threads_requested": parse_int(row.get("threads_requested", spec.thread_count), spec.thread_count),
                "threads_effective": parse_int(row.get("threads_effective", spec.thread_count), spec.thread_count),
                "mode": spec.mode,
                "budget_sec": row["budget_sec"],
                "repeat_index": spec.repeat_index + 1,
                "status": suite_status_from_solver_row(row["status"]),
                "raw_solver_status": row["status"],
                "completed": "true" if parse_bool(row["completed"]) else "false",
                "exit_code": exit_code,
                "runtime_sec": row["runtime_sec"],
                "generated_labels": row["generated_labels"],
                "expanded_labels": row["expanded_labels"],
                "pruned_by_target": row["pruned_by_target"],
                "pruned_by_node": row["pruned_by_node"],
                "pruned_other": row["pruned_other"],
                "final_frontier_size": row["final_frontier_size"],
                "peak_open_size": row["peak_open_size"],
                "peak_live_labels": row["peak_live_labels"],
                "peak_rss_mb": f"{peak_rss_mb:.6f}",
                "time_to_first_solution_sec": row["time_to_first_solution_sec"],
                "trace_file": row["trace_artifact_path"],
                "frontier_file": row["frontier_artifact_path"],
                "stdout_file": optional_path(stdout_path),
                "stderr_file": optional_path(stderr_path),
            }
        )
    return rows


def append_or_replace_missing_rows(
    spec: RunSpec,
    existing_rows: list[dict[str, Any]],
    status: str,
    exit_code: int,
    runtime_sec: float,
    peak_rss_mb: float,
    stdout_path: Path,
    stderr_path: Path,
) -> list[dict[str, Any]]:
    present_query_ids = {row["query_id"] for row in existing_rows}
    for query in expected_queries_for_spec(spec):
        if query["query_id"] in present_query_ids:
            continue
        synthetic_run_id = spec.run_id if spec.execution_kind == "per_query" else f"{spec.run_id}__{sanitize_component(query['query_id'])}"
        existing_rows.append(
            {
                "run_id": synthetic_run_id,
                "dataset_id": spec.dataset["dataset_id"],
                "query_set_id": spec.query_set["query_set_id"],
                "query_id": query["query_id"],
                "solver": spec.solver_name,
                "reported_solver_name": "",
                "threads": spec.thread_count,
                "threads_requested": spec.thread_count,
                "threads_effective": spec.thread_count,
                "mode": spec.mode,
                "budget_sec": f"{spec.budget_sec:g}",
                "repeat_index": spec.repeat_index + 1,
                "status": status,
                "raw_solver_status": "",
                "completed": "false",
                "exit_code": exit_code,
                "runtime_sec": f"{runtime_sec:.12g}",
                "generated_labels": "",
                "expanded_labels": "",
                "pruned_by_target": "",
                "pruned_by_node": "",
                "pruned_other": "",
                "final_frontier_size": "",
                "peak_open_size": "",
                "peak_live_labels": "",
                "peak_rss_mb": f"{peak_rss_mb:.6f}",
                "time_to_first_solution_sec": "",
                "trace_file": "",
                "frontier_file": "",
                "stdout_file": optional_path(stdout_path),
                "stderr_file": optional_path(stderr_path),
            }
        )
    return existing_rows


def determine_retryable_status(summary_rows: list[dict[str, Any]]) -> bool:
    return any(row["status"] in {"timeout", "crash"} for row in summary_rows)


def execute_process_with_monitor(
    command: list[str],
    cwd: Path,
    stdout_path: Path,
    stderr_path: Path,
    wall_timeout_sec: float,
) -> dict[str, Any]:
    stdout_path.parent.mkdir(parents=True, exist_ok=True)
    stderr_path.parent.mkdir(parents=True, exist_ok=True)

    start_time = time.monotonic()
    peak_rss_mb = 0.0
    timed_out = False

    with stdout_path.open("w", encoding="utf-8") as stdout_handle, stderr_path.open("w", encoding="utf-8") as stderr_handle:
        process = subprocess.Popen(
            command,
            cwd=cwd,
            stdout=stdout_handle,
            stderr=stderr_handle,
            text=True,
        )

        while True:
            rss_mb = measure_process_rss_mb(process.pid)
            if rss_mb is not None:
                peak_rss_mb = max(peak_rss_mb, rss_mb)

            returncode = process.poll()
            elapsed_sec = time.monotonic() - start_time
            if returncode is not None:
                break

            if elapsed_sec > wall_timeout_sec:
                timed_out = True
                process.terminate()
                try:
                    process.wait(timeout=1.0)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
                returncode = process.returncode
                break

            time.sleep(0.05)

        rss_mb = measure_process_rss_mb(process.pid)
        if rss_mb is not None:
            peak_rss_mb = max(peak_rss_mb, rss_mb)

    return {
        "returncode": returncode if returncode is not None else 1,
        "wall_runtime_sec": time.monotonic() - start_time,
        "peak_rss_mb": peak_rss_mb,
        "timed_out": timed_out,
    }


def failure_mode(config: dict[str, Any]) -> str:
    failure_policy = config.get("failure_policy", {})
    return str(failure_policy.get("mode", failure_policy.get("on_failure", "fail_fast")))


def normalize_build_command(command: Any) -> list[str]:
    if isinstance(command, list):
        return [str(part) for part in command]
    if isinstance(command, str):
        return shlex.split(command)
    return []


def run_build_step(config: dict[str, Any], repo_root: Path, suite_dir: Path) -> dict[str, Any]:
    build_config = config.get("build", {})
    command = normalize_build_command(build_config.get("command", []))
    enabled = bool(build_config.get("enabled", False))
    build_dir = suite_dir / "build"
    build_dir.mkdir(parents=True, exist_ok=True)

    if not enabled or not command:
        return {
            "enabled": False,
            "status": "skipped",
            "returncode": 0,
        }

    stdout_path = build_dir / "stdout.log"
    stderr_path = build_dir / "stderr.log"
    with stdout_path.open("w", encoding="utf-8") as stdout_handle, stderr_path.open("w", encoding="utf-8") as stderr_handle:
        result = subprocess.run(
            command,
            cwd=repo_root,
            stdout=stdout_handle,
            stderr=stderr_handle,
            text=True,
            check=False,
        )

    payload = {
        "enabled": True,
        "command": command,
        "returncode": result.returncode,
        "status": "completed" if result.returncode == 0 else "build_error",
        "stdout_file": normalize_path(stdout_path),
        "stderr_file": normalize_path(stderr_path),
    }
    write_json(build_dir / "status.json", payload)
    return payload


def build_error_rows_for_suite(run_specs: list[RunSpec], stdout_path: str, stderr_path: str) -> list[dict[str, Any]]:
    rows = []
    for spec in run_specs:
        rows.extend(
            synthetic_summary_rows(
                spec=spec,
                status="build_error",
                exit_code=1,
                runtime_sec=0.0,
                peak_rss_mb=0.0,
                stdout_path=Path(stdout_path),
                stderr_path=Path(stderr_path),
            )
        )
    return rows


def update_suite_summary(summary_path: Path, summary_rows: list[dict[str, Any]]) -> None:
    ordered_rows = sorted(summary_rows, key=lambda row: (row["run_id"], row["query_id"]))
    write_csv(summary_path, SUITE_SUMMARY_FIELDS, ordered_rows)


def execute_run(spec: RunSpec, repo_root: Path, config: dict[str, Any]) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    run_dir = spec.suite_dir / "runs" / spec.run_id
    run_dir.mkdir(parents=True, exist_ok=True)

    stdout_path = run_dir / "stdout.log"
    stderr_path = run_dir / "stderr.log"

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
        "repeat_index": spec.repeat_index + 1,
        "git_sha": spec.git_sha,
        "wall_timeout_sec": spec.wall_timeout_sec,
        "created_at_utc": utc_now_iso(),
    }
    write_json(run_dir / "run.json", run_manifest)

    if spec.skip_reason:
        skipped_rows = synthetic_summary_rows(
            spec=spec,
            status="skipped",
            exit_code=0,
            runtime_sec=0.0,
            peak_rss_mb=0.0,
            stdout_path=stdout_path,
            stderr_path=stderr_path,
        )
        status_payload = {
            "run_id": spec.run_id,
            "status": "skipped",
            "skip_reason": spec.skip_reason,
            "attempts": [],
            "completed_at_utc": utc_now_iso(),
        }
        write_json(run_dir / "status.json", status_payload)
        return status_payload, skipped_rows

    command, expected_rows = build_command(spec, repo_root, run_dir)
    write_json(run_dir / "command.json", {"argv": command, "shell": " ".join(shlex.quote(arg) for arg in command)})

    failure_policy = config.get("failure_policy", {})
    max_retries = int(failure_policy.get("max_retries", 0))
    status_payload: dict[str, Any] = {
        "run_id": spec.run_id,
        "status": "pending",
        "expected_summary_rows": expected_rows,
        "attempts": [],
    }

    for attempt_index in range(max_retries + 1):
        summary_path = run_dir / "summary.csv"
        if summary_path.exists():
            summary_path.unlink()

        process_result = execute_process_with_monitor(
            command=command,
            cwd=repo_root,
            stdout_path=stdout_path,
            stderr_path=stderr_path,
            wall_timeout_sec=spec.wall_timeout_sec,
        )

        solver_rows = load_solver_summary_rows(summary_path)
        suite_rows = suite_summary_rows_from_solver_summary(
            spec=spec,
            solver_rows=solver_rows,
            exit_code=int(process_result["returncode"]),
            peak_rss_mb=float(process_result["peak_rss_mb"]),
            stdout_path=stdout_path,
            stderr_path=stderr_path,
        )

        attempt_status = "completed"
        if process_result["timed_out"]:
            if has_complete_solver_summary(solver_rows, expected_rows):
                attempt_status = suite_rows[0]["status"] if any(row["status"] in {"timeout", "crash"} for row in suite_rows) else "completed"
            else:
                attempt_status = "timeout"
                suite_rows = synthetic_summary_rows(
                    spec=spec,
                    status="timeout",
                    exit_code=int(process_result["returncode"]),
                    runtime_sec=float(process_result["wall_runtime_sec"]),
                    peak_rss_mb=float(process_result["peak_rss_mb"]),
                    stdout_path=stdout_path,
                    stderr_path=stderr_path,
                )
        elif int(process_result["returncode"]) != 0:
            attempt_status = "crash"
            suite_rows = synthetic_summary_rows(
                spec=spec,
                status="crash",
                exit_code=int(process_result["returncode"]),
                runtime_sec=float(process_result["wall_runtime_sec"]),
                peak_rss_mb=float(process_result["peak_rss_mb"]),
                stdout_path=stdout_path,
                stderr_path=stderr_path,
            )
        elif len(solver_rows) != expected_rows:
            attempt_status = "crash"
            suite_rows = append_or_replace_missing_rows(
                spec=spec,
                existing_rows=suite_rows,
                status="crash",
                exit_code=int(process_result["returncode"]),
                runtime_sec=float(process_result["wall_runtime_sec"]),
                peak_rss_mb=float(process_result["peak_rss_mb"]),
                stdout_path=stdout_path,
                stderr_path=stderr_path,
            )
        elif any(row["status"] in {"timeout", "crash"} for row in suite_rows):
            attempt_status = suite_rows[0]["status"]

        status_payload["attempts"].append(
            {
                "attempt_index": attempt_index,
                "returncode": int(process_result["returncode"]),
                "observed_summary_rows": len(solver_rows),
                "peak_rss_mb": float(process_result["peak_rss_mb"]),
                "wall_runtime_sec": float(process_result["wall_runtime_sec"]),
                "status": attempt_status,
                "process_timed_out": bool(process_result["timed_out"]),
                "used_solver_summary": has_complete_solver_summary(solver_rows, expected_rows),
            }
        )

        if attempt_status == "completed" or attempt_index == max_retries:
            status_payload["status"] = attempt_status
            status_payload["completed_at_utc"] = utc_now_iso()
            write_json(run_dir / "status.json", status_payload)
            return status_payload, suite_rows

    status_payload["status"] = "crash"
    status_payload["completed_at_utc"] = utc_now_iso()
    write_json(run_dir / "status.json", status_payload)
    return status_payload, []


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run manifest-driven MOSP benchmarks")
    parser.add_argument("--config", required=True, help="Path to benchmark config")
    parser.add_argument("--suite-id", default="", help="Optional explicit suite id")
    parser.add_argument("--results-root", default="", help="Optional override for result root")
    parser.add_argument("--build-dir", default="", help="Optional override for build directory")
    parser.add_argument("--binary", default="", help="Optional override for solver binary")
    parser.add_argument("--dry-run", action="store_true", help="Write plan only, do not execute runs")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = Path(__file__).resolve().parents[2]
    config_path = (repo_root / args.config).resolve()
    config = load_document(config_path)

    solver_specs = resolve_solver_specs(config)
    validate_config(config, solver_specs)

    build_dir = (repo_root / (args.build_dir or config.get("build_dir", "Release"))).resolve()
    binary_path = (repo_root / (args.binary or config.get("binary", "./Release/main"))).resolve()
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
                "wall_timeout_sec": spec.wall_timeout_sec,
                "repeat_index": spec.repeat_index + 1,
                "git_sha": spec.git_sha,
                "skip_reason": spec.skip_reason,
            }
        )
    write_json(suite_dir / "run_plan.json", run_plan)

    if args.dry_run:
        print(f"Dry run created suite plan at {suite_dir}")
        return 0

    summary_rows: list[dict[str, Any]] = []
    suite_summary_path = suite_dir / "summary.csv"
    update_suite_summary(suite_summary_path, summary_rows)

    build_status = run_build_step(config, repo_root, suite_dir)
    write_json(suite_dir / "build_status.json", build_status)

    if build_status["status"] == "build_error" or not binary_path.exists():
        if build_status["status"] != "build_error":
            build_status = {
                "enabled": False,
                "status": "build_error",
                "returncode": 1,
                "stdout_file": "",
                "stderr_file": "",
            }
            write_json(suite_dir / "build_status.json", build_status)
        summary_rows = build_error_rows_for_suite(
            run_specs=run_specs,
            stdout_path=build_status.get("stdout_file", ""),
            stderr_path=build_status.get("stderr_file", ""),
        )
        update_suite_summary(suite_summary_path, summary_rows)
        write_json(
            suite_dir / "run_results.json",
            [{"run_id": spec.run_id, "status": "build_error"} for spec in run_specs],
        )
        print(f"Build failed for suite {suite_id}")
        return 1

    run_results = []
    fail_fast = failure_mode(config) == "fail_fast"
    for spec in run_specs:
        run_status, run_summary_rows = execute_run(spec, repo_root, config)
        run_results.append(run_status)
        summary_rows.extend(run_summary_rows)
        update_suite_summary(suite_summary_path, summary_rows)
        write_json(suite_dir / "run_results.json", run_results)
        if fail_fast and run_status["status"] not in {"completed", "skipped"}:
            print(f"Stopping early due to {run_status['status']} in {spec.run_id}")
            break

    completed = sum(1 for result in run_results if result["status"] == "completed")
    print(f"Completed {completed}/{len(run_results)} runs")
    print(f"Suite output: {suite_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
