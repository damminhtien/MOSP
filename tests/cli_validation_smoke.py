#!/usr/bin/env python3

from __future__ import annotations

import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path


def write_graph(path: Path, weight: int) -> None:
    path.write_text(
        "\n".join(
            [
                "c tiny graph",
                f"a 0 1 {weight}",
                f"a 1 0 {weight}",
            ]
        )
        + "\n",
        encoding="utf-8",
    )


def run_case(binary: Path, args: list[str]) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(binary), *args],
        text=True,
        capture_output=True,
        check=False,
    )


def assert_failed(result: subprocess.CompletedProcess[str], expected_substring: str) -> None:
    combined = result.stdout + result.stderr
    if result.returncode == 0:
        raise AssertionError(f"expected failure, got success: {combined}")
    if expected_substring not in combined:
        raise AssertionError(f"missing expected error substring {expected_substring!r} in output: {combined}")


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


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: cli_validation_smoke.py <main-binary>")

    binary = Path(sys.argv[1]).resolve()
    if not binary.exists():
        raise SystemExit(f"missing binary: {binary}")

    with tempfile.TemporaryDirectory(prefix="mosp-cli-smoke-") as tmp_dir_str:
        tmp_dir = Path(tmp_dir_str)
        map_a = tmp_dir / "map_a.gr"
        map_b = tmp_dir / "map_b.gr"
        write_graph(map_a, 1)
        write_graph(map_b, 2)

        summary_path = tmp_dir / "summary.csv"
        sopmoa_summary_path = tmp_dir / "sopmoa_summary.csv"
        relaxed_summary_path = tmp_dir / "relaxed_summary.csv"
        scenario_path = tmp_dir / "scenario.json"
        scenario_path.write_text(
            json.dumps(
                [
                    {"name": "ok", "start_data": 0, "end_data": 1},
                    {"name": "self", "start_data": 1, "end_data": 1},
                ]
            ),
            encoding="utf-8",
        )
        bad_schema_path = tmp_dir / "bad_schema.json"
        bad_schema_path.write_text(json.dumps({"query_set_id": "wrong"}), encoding="utf-8")

        not_a_dir = tmp_dir / "not_a_dir"
        not_a_dir.write_text("x", encoding="utf-8")

        ok_args = [
            "--summary-output",
            str(summary_path),
            "-m",
            str(map_a),
            str(map_b),
            "-a",
            "LTMOA",
            "-s",
            "0",
            "-t",
            "0",
        ]

        assert_succeeded(run_case(binary, ["--help"]))
        assert_failed(run_case(binary, ["--summary-output", str(summary_path)]), "--map requires between 2 and 5 objective files")
        assert_failed(run_case(binary, ["--summary-output", str(summary_path), "-m", str(map_a)]), "--map requires between 2 and 5 objective files")
        assert_failed(run_case(binary, ["--summary-output", str(summary_path), "-m", str(map_a), str(map_b)]), "single-query mode requires both --start and --target")
        assert_failed(run_case(binary, ["--summary-output", str(summary_path), "-m", str(map_a), str(map_b), "-s", "-1", "-t", "0"]), "--start must be >= 0")
        assert_failed(run_case(binary, ["--summary-output", str(summary_path), "-m", str(map_a), str(map_b), "-s", "0", "-t", "99"]), "--target out of range")
        assert_failed(run_case(binary, ["--summary-output", str(summary_path), "-m", str(map_a), str(map_b), "--scenario", str(tmp_dir / "missing.json")]), "Cannot open scenario file")
        assert_failed(run_case(binary, ["--summary-output", str(summary_path), "-m", str(map_a), str(map_b), "--scenario", str(bad_schema_path)]), "Scenario file must be a JSON array")
        assert_failed(run_case(binary, ["--summary-output", str(summary_path), "-m", str(map_a), str(map_b), "--scenario", str(scenario_path), "--from", "-1", "--to", "1"]), "--from must be >= 0")
        assert_failed(run_case(binary, ["--summary-output", str(summary_path), "-m", str(map_a), str(map_b), "--scenario", str(scenario_path), "--from", "1", "--to", "0"]), "--from must be <= --to")
        assert_failed(run_case(binary, ["--summary-output", str(summary_path), "-m", str(map_a), str(map_b), "--budget-sec", "-1", "-s", "0", "-t", "0"]), "--budget-sec must be finite and >= 0")
        assert_failed(run_case(binary, ["--summary-output", str(summary_path), "-m", str(map_a), str(map_b), "--budget-sec", "nope", "-s", "0", "-t", "0"]), "the argument ('nope') for option '--budget-sec' is invalid")
        assert_failed(run_case(binary, ["--summary-output", str(summary_path), "-m", str(map_a), str(map_b), "--trace-interval-ms", "-1", "-s", "0", "-t", "0"]), "--trace-interval-ms must be >= 0")
        assert_failed(run_case(binary, ["--summary-output", str(summary_path), "-m", str(map_a), str(map_b), "-a", "DOES_NOT_EXIST", "-s", "0", "-t", "0"]), "unknown algorithm")
        assert_failed(run_case(binary, ["--summary-output", str(summary_path), "-m", str(map_a), str(map_b), "-a", "LTMOA", "-n", "2", "-s", "0", "-t", "0"]), "--numthreads is only valid as 1 for serial solvers")
        assert_failed(run_case(binary, ["--summary-output", str(summary_path), "-m", str(map_a), str(map_b), "-a", "SOPMOA_relaxed", "-n", "0", "-s", "0", "-t", "0"]), "--numthreads must be positive")
        assert_failed(run_case(binary, ["--summary-output", str(summary_path), "--frontier-output-dir", str(not_a_dir), "-m", str(map_a), str(map_b), "-s", "0", "-t", "0"]), "--frontier-output-dir must point to a directory")
        assert_failed(run_case(binary, ["--summary-output", str(summary_path), "-m", str(map_a), str(map_b), "--scenario", str(scenario_path), "-s", "0", "-t", "0"]), "--scenario cannot be combined with --start or --target")
        assert_failed(run_case(binary, ["--summary-output", str(summary_path), "-m", str(map_a), str(map_b), "--from", "0", "-s", "0", "-t", "0"]), "--from and --to require --scenario")

        ok_result = run_case(binary, ok_args)
        assert_succeeded(ok_result)
        if not summary_path.exists():
            raise AssertionError("expected summary.csv to be created for valid run")
        ok_row = read_summary_row(summary_path)
        if ok_row["threads_requested"] != "1" or ok_row["threads_effective"] != "1":
            raise AssertionError(f"expected serial thread metadata to stay at 1: {ok_row}")

        sopmoa_result = run_case(
            binary,
            [
                "--summary-output",
                str(sopmoa_summary_path),
                "-m",
                str(map_a),
                str(map_b),
                "-a",
                "SOPMOA",
                "-n",
                "13",
                "-s",
                "0",
                "-t",
                "0",
            ],
        )
        assert_succeeded(sopmoa_result)
        sopmoa_row = read_summary_row(sopmoa_summary_path)
        if sopmoa_row["threads_requested"] != "13" or sopmoa_row["threads_effective"] != "12":
            raise AssertionError(f"expected SOPMOA to cap 13 requested threads down to 12: {sopmoa_row}")

        relaxed_result = run_case(
            binary,
            [
                "--summary-output",
                str(relaxed_summary_path),
                "-m",
                str(map_a),
                str(map_b),
                "-a",
                "SOPMOA_relaxed",
                "-n",
                "4",
                "-s",
                "0",
                "-t",
                "0",
            ],
        )
        assert_succeeded(relaxed_result)
        relaxed_row = read_summary_row(relaxed_summary_path)
        if relaxed_row["threads_requested"] != "4" or relaxed_row["threads_effective"] != "4":
            raise AssertionError(f"expected SOPMOA_relaxed to honor 4 threads: {relaxed_row}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
