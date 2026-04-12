#!/usr/bin/env python3

from __future__ import annotations

import csv
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def assert_succeeded(result: subprocess.CompletedProcess[str]) -> None:
    if result.returncode != 0:
        raise AssertionError(f"expected success, got exit {result.returncode}: {result.stdout}{result.stderr}")


def read_summary_row(path: Path) -> dict[str, str]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        reader = csv.DictReader(handle)
        row = next(reader, None)
    if row is None:
        raise AssertionError(f"expected one summary row in {path}")
    return row


def write_config(path: Path, payload: dict[str, object]) -> None:
    path.write_text(json.dumps(payload), encoding="utf-8")


def run_runner(
    repo_root: Path,
    runner: Path,
    config_path: Path,
    suite_id: str,
    results_root: Path,
    main_binary: Path,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            sys.executable,
            str(runner),
            "--config",
            str(config_path),
            "--suite-id",
            suite_id,
            "--results-root",
            str(results_root),
            "--binary",
            str(main_binary),
            "--build-dir",
            str(main_binary.parent),
        ],
        cwd=repo_root,
        text=True,
        capture_output=True,
        check=False,
    )


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: benchmark_runner_smoke.py <main-binary>")

    main_binary = Path(sys.argv[1]).resolve()
    if not main_binary.exists():
        raise SystemExit(f"missing binary: {main_binary}")

    repo_root = Path(__file__).resolve().parents[1]
    runner = repo_root / "bench" / "scripts" / "run_benchmark.py"

    tmp_dir = Path(tempfile.mkdtemp(prefix="mosp-runner-smoke-")).resolve()
    try:
        results_root = tmp_dir / "results"
        timecap_config_path = tmp_dir / "runner_smoke_timecap.json"
        write_config(
            timecap_config_path,
            {
                "config_version": "benchmark_config_v1",
                "mode": "time_capped",
                "description": "Smoke test for clean time-capped runner summaries.",
                "build_dir": str(main_binary.parent),
                "binary": str(main_binary),
                "build": {
                    "enabled": False,
                    "command": ["/usr/bin/false"],
                },
                "execution": {
                    "kind": "per_query",
                },
                "solvers": [
                    {
                        "name": "LTMOA",
                        "thread_counts": [1],
                    }
                ],
                "datasets": ["nyc_3obj"],
                "query_sets": ["nyc_query6_single"],
                "budgets_sec": [0.01],
                "repeat_count": 1,
                "trace_interval_ms": 0,
                "runtime_control": {
                    "timeout_grace_sec": 2.0,
                },
                "failure_policy": {
                    "mode": "continue_on_error",
                    "max_retries": 0,
                },
                "output": {
                    "root": str(results_root),
                    "export_frontier": False,
                    "export_trace": False,
                },
            },
        )

        result = run_runner(
            repo_root=repo_root,
            runner=runner,
            config_path=timecap_config_path,
            suite_id="runner_smoke_timecap",
            results_root=results_root,
            main_binary=main_binary,
        )
        assert_succeeded(result)

        if "Successful runs: 1/1" not in result.stdout:
            raise AssertionError(f"missing successful run summary in output: {result.stdout}")
        if "timed_out_cleanly=1" not in result.stdout:
            raise AssertionError(f"missing clean timeout count in output: {result.stdout}")
        if "Completed 0/1 runs" in result.stdout:
            raise AssertionError(f"stale completed banner still present: {result.stdout}")

        suite_dir = results_root / "runner_smoke_timecap"
        summary_row = read_summary_row(suite_dir / "summary.csv")
        if summary_row["status"] != "timeout" or summary_row["exit_code"] != "0":
            raise AssertionError(f"expected clean solver timeout row, got: {summary_row}")
        if "wall_runtime_sec" not in summary_row:
            raise AssertionError(f"missing wall_runtime_sec in suite summary: {summary_row}")
        if float(summary_row["wall_runtime_sec"]) <= 0.0:
            raise AssertionError(f"expected positive wall_runtime_sec, got: {summary_row}")

        run_results = json.loads((suite_dir / "run_results.json").read_text(encoding="utf-8"))
        if len(run_results) != 1:
            raise AssertionError(f"expected one run result, got: {run_results}")
        attempt = run_results[0]["attempts"][-1]
        if run_results[0]["status"] != "timeout":
            raise AssertionError(f"expected timeout run status, got: {run_results[0]}")
        if attempt.get("process_timed_out"):
            raise AssertionError(f"runner should not have timed out the process: {attempt}")
        if not attempt.get("used_solver_summary"):
            raise AssertionError(f"expected solver summary-backed timeout semantics: {attempt}")

        completion_config_path = tmp_dir / "runner_smoke_completion.json"
        write_config(
            completion_config_path,
            {
                "config_version": "benchmark_config_v1",
                "mode": "completion",
                "description": "Smoke test for wall-runtime summary columns on clean completion runs.",
                "build_dir": str(main_binary.parent),
                "binary": str(main_binary),
                "build": {
                    "enabled": False,
                    "command": ["/usr/bin/false"],
                },
                "execution": {
                    "kind": "per_query",
                },
                "solvers": [
                    {
                        "name": "LTMOA",
                        "thread_counts": [1],
                    }
                ],
                "datasets": ["nyc_3obj"],
                "query_sets": ["nyc_trivial_completion"],
                "budgets_sec": [0.01],
                "repeat_count": 1,
                "trace_interval_ms": 0,
                "runtime_control": {
                    "timeout_grace_sec": 2.0,
                },
                "failure_policy": {
                    "mode": "continue_on_error",
                    "max_retries": 0,
                },
                "output": {
                    "root": str(results_root),
                    "export_frontier": False,
                    "export_trace": False,
                },
            },
        )

        completion_result = run_runner(
            repo_root=repo_root,
            runner=runner,
            config_path=completion_config_path,
            suite_id="runner_smoke_completion",
            results_root=results_root,
            main_binary=main_binary,
        )
        assert_succeeded(completion_result)

        completion_row = read_summary_row((results_root / "runner_smoke_completion" / "summary.csv"))
        if completion_row["status"] != "completed" or completion_row["completed"] != "true":
            raise AssertionError(f"expected completed suite row, got: {completion_row}")
        if "wall_runtime_sec" not in completion_row:
            raise AssertionError(f"missing wall_runtime_sec in completion row: {completion_row}")
        if float(completion_row["wall_runtime_sec"]) < float(completion_row["runtime_sec"]):
            raise AssertionError(
                "expected runner wall_runtime_sec to be >= solver runtime_sec, "
                f"got: {completion_row}"
            )
    finally:
        shutil.rmtree(tmp_dir, ignore_errors=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
