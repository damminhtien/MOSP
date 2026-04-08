#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
import json
import math
from collections import defaultdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import numpy as np


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Aggregate MOSP benchmark result suites")
    parser.add_argument("--results-root", default="bench/results", help="Root directory containing benchmark suites")
    parser.add_argument("--suite", action="append", default=[], help="Suite id or path to include; repeatable")
    parser.add_argument("--analysis-id", default="", help="Output analysis id")
    parser.add_argument("--output-dir", default="", help="Explicit output directory override")
    return parser.parse_args()


def read_csv_rows(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def write_csv(path: Path, fieldnames: list[str], rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def write_json(path: Path, payload: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2, sort_keys=True)
        handle.write("\n")


def parse_float(value: Any, default: float = math.nan) -> float:
    if value in (None, "", "nan"):
        return default
    return float(value)


def parse_int(value: Any, default: int = 0) -> int:
    if value in (None, ""):
        return default
    return int(float(value))


def median(values: list[float]) -> float:
    if not values:
        return math.nan
    return float(np.median(np.array(values, dtype=float)))


def mean(values: list[float]) -> float:
    if not values:
        return math.nan
    return float(np.mean(np.array(values, dtype=float)))


def std(values: list[float]) -> float:
    if not values:
        return math.nan
    return float(np.std(np.array(values, dtype=float), ddof=0))


def iqr(values: list[float]) -> float:
    if not values:
        return math.nan
    lo, hi = np.percentile(np.array(values, dtype=float), [25, 75])
    return float(hi - lo)


def sanitize_component(value: str) -> str:
    return "".join(ch if ch.isalnum() or ch in "-_." else "_" for ch in value) or "value"


def discover_suites(results_root: Path, requested: list[str]) -> list[Path]:
    suites: list[Path] = []
    if requested:
        for raw in requested:
            candidate = Path(raw)
            if not candidate.is_absolute():
                if (results_root / raw).exists():
                    candidate = results_root / raw
                else:
                    candidate = (Path.cwd() / raw).resolve()
            suites.append(candidate.resolve())
    else:
        for child in sorted(results_root.iterdir()):
            if child.is_dir() and (child / "summary.csv").exists():
                suites.append(child.resolve())
    return [suite for suite in suites if (suite / "summary.csv").exists()]


def frontier_points_from_csv(path: Path) -> tuple[int, list[tuple[tuple[int, ...], float]]]:
    rows = read_csv_rows(path)
    if not rows:
        return 0, []
    fieldnames = list(rows[0].keys())
    objective_fields = [field for field in fieldnames if field.startswith("obj")]
    points: list[tuple[tuple[int, ...], float]] = []
    for row in rows:
        cost = tuple(parse_int(row[field]) for field in objective_fields)
        time_found = parse_float(row.get("time_found_sec"), -1.0)
        points.append((cost, time_found))
    return len(objective_fields), points


def trace_rows_from_csv(path: Path) -> list[dict[str, float]]:
    raw_rows = read_csv_rows(path)
    rows: list[dict[str, float]] = []
    for row in raw_rows:
        rows.append(
            {
                "time_sec": parse_float(row.get("time_sec"), 0.0),
                "frontier_size": parse_float(row.get("frontier_size"), 0.0),
                "hv_ratio": parse_float(row.get("hv_ratio"), math.nan),
                "recall": parse_float(row.get("recall"), math.nan),
            }
        )
    return rows


def lex_smaller(lhs: tuple[int, ...], rhs: tuple[int, ...]) -> bool:
    return lhs < rhs


def frontier_match_count(lhs: list[tuple[int, ...]], rhs: list[tuple[int, ...]]) -> int:
    lhs_sorted = sorted(lhs)
    rhs_sorted = sorted(rhs)
    lhs_idx = 0
    rhs_idx = 0
    matches = 0
    while lhs_idx < len(lhs_sorted) and rhs_idx < len(rhs_sorted):
        if lhs_sorted[lhs_idx] == rhs_sorted[rhs_idx]:
            matches += 1
            lhs_idx += 1
            rhs_idx += 1
        elif lex_smaller(lhs_sorted[lhs_idx], rhs_sorted[rhs_idx]):
            lhs_idx += 1
        else:
            rhs_idx += 1
    return matches


def collect_reference_point(final_frontier: list[tuple[int, ...]], snapshots: list[list[tuple[int, ...]]], dimension: int) -> list[float]:
    reference = [1.0] * dimension
    has_points = False
    for frontier in [final_frontier, *snapshots]:
        for point in frontier:
            for idx in range(min(dimension, len(point))):
                reference[idx] = max(reference[idx], float(point[idx]) + 1.0)
            has_points = True
    return reference if has_points else []


def hypervolume_recursive(points: list[list[float]], reference: list[float], dimension: int) -> float:
    if not points or dimension == 0:
        return 0.0
    if dimension == 1:
        minimum = reference[0]
        for point in points:
            minimum = min(minimum, point[0])
        return max(0.0, reference[0] - minimum)

    axis = dimension - 1
    points = sorted(points, key=lambda point: (point[axis], point))
    volume = 0.0
    previous = reference[axis]
    while points:
        bound = points[-1][axis]
        if bound < previous:
            projected = [point[:axis] for point in points]
            volume += hypervolume_recursive(projected, reference[:axis], axis) * (previous - bound)
            previous = bound
        while points and points[-1][axis] == previous:
            points.pop()
    return volume


def frontier_hypervolume(frontier: list[tuple[int, ...]], reference: list[float], dimension: int) -> float:
    if dimension == 0 or dimension > 3 or not frontier or len(reference) != dimension:
        return math.nan if dimension > 3 else 0.0
    points = [[float(value) for value in point[:dimension]] for point in frontier]
    return hypervolume_recursive(points, reference, dimension)


def choose_reference_rows(rows: list[dict[str, Any]]) -> dict[tuple[str, str], dict[str, Any]]:
    grouped: dict[tuple[str, str], list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        frontier_file = row.get("frontier_file", "")
        if frontier_file and Path(frontier_file).exists():
            grouped[(row["dataset_id"], row["query_id"])].append(row)

    selected: dict[tuple[str, str], dict[str, Any]] = {}
    for key, candidates in grouped.items():
        def priority(row: dict[str, Any]) -> tuple[int, int, float]:
            if row["status"] == "completed" and row["mode"] == "completion":
                rank = 0
            elif row["status"] == "completed":
                rank = 1
            else:
                rank = 2
            return (
                rank,
                -parse_int(row.get("final_frontier_size"), 0),
                parse_float(row.get("runtime_sec"), math.inf),
            )

        selected[key] = sorted(candidates, key=priority)[0]
    return selected


def enrich_rows(rows: list[dict[str, Any]], reference_rows: dict[tuple[str, str], dict[str, Any]]) -> tuple[list[dict[str, Any]], list[dict[str, Any]], list[dict[str, Any]]]:
    enriched: list[dict[str, Any]] = []
    trace_samples: list[dict[str, Any]] = []
    reference_manifest: list[dict[str, Any]] = []

    for key, reference_row in sorted(reference_rows.items()):
        reference_manifest.append(
            {
                "dataset_id": key[0],
                "query_id": key[1],
                "reference_run_id": reference_row["run_id"],
                "reference_mode": reference_row["mode"],
                "reference_status": reference_row["status"],
                "reference_frontier_file": reference_row["frontier_file"],
            }
        )

    for row in rows:
        enriched_row = dict(row)
        final_reference_hv_ratio = math.nan
        final_reference_recall = math.nan
        reference_kind = "missing"
        num_objectives = 0

        reference_row = reference_rows.get((row["dataset_id"], row["query_id"]))
        frontier_file = row.get("frontier_file", "")
        if reference_row is not None and frontier_file and Path(frontier_file).exists():
            run_dimension, run_points = frontier_points_from_csv(Path(frontier_file))
            ref_dimension, ref_points = frontier_points_from_csv(Path(reference_row["frontier_file"]))
            num_objectives = max(run_dimension, ref_dimension)
            run_frontier = [cost for cost, _time in run_points]
            ref_frontier = [cost for cost, _time in ref_points]
            if ref_frontier:
                final_reference_recall = frontier_match_count(run_frontier, ref_frontier) / float(len(ref_frontier))
                reference_point = collect_reference_point(ref_frontier, [run_frontier], num_objectives)
                if num_objectives <= 3 and reference_point:
                    ref_hv = frontier_hypervolume(ref_frontier, reference_point, num_objectives)
                    run_hv = frontier_hypervolume(run_frontier, reference_point, num_objectives)
                    final_reference_hv_ratio = run_hv / ref_hv if ref_hv > 0.0 else math.nan
                reference_kind = "completion_reference" if reference_row["mode"] == "completion" else "best_available_frontier"

        trace_rows = []
        trace_file = row.get("trace_file", "")
        if trace_file and Path(trace_file).exists():
            trace_rows = trace_rows_from_csv(Path(trace_file))
            for trace_row in trace_rows:
                trace_samples.append(
                    {
                        "dataset_id": row["dataset_id"],
                        "query_set_id": row["query_set_id"],
                        "query_id": row["query_id"],
                        "solver": row["solver"],
                        "reported_solver_name": row["reported_solver_name"],
                        "threads": row["threads"],
                        "mode": row["mode"],
                        "budget_sec": row["budget_sec"],
                        "run_id": row["run_id"],
                        "time_sec": trace_row["time_sec"],
                        "frontier_size": trace_row["frontier_size"],
                        "hv_ratio": trace_row["hv_ratio"],
                        "recall": trace_row["recall"],
                    }
                )

        last_trace = trace_rows[-1] if trace_rows else None
        enriched_row["last_trace_hv_ratio"] = "" if last_trace is None or math.isnan(last_trace["hv_ratio"]) else f"{last_trace['hv_ratio']:.12g}"
        enriched_row["last_trace_recall"] = "" if last_trace is None or math.isnan(last_trace["recall"]) else f"{last_trace['recall']:.12g}"
        enriched_row["last_trace_frontier_size"] = "" if last_trace is None else int(last_trace["frontier_size"])
        enriched_row["final_reference_hv_ratio"] = "" if math.isnan(final_reference_hv_ratio) else f"{final_reference_hv_ratio:.12g}"
        enriched_row["final_reference_recall"] = "" if math.isnan(final_reference_recall) else f"{final_reference_recall:.12g}"
        enriched_row["reference_kind"] = reference_kind
        enriched_row["num_objectives"] = num_objectives
        enriched.append(enriched_row)

    return enriched, trace_samples, reference_manifest


def completion_summary(rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
    grouped: dict[tuple[str, str, str], list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        if row["mode"] != "completion":
            continue
        grouped[(row["dataset_id"], row["solver"], row["threads"])].append(row)

    output = []
    for key in sorted(grouped):
        group = grouped[key]
        runtime_values = [parse_float(row["runtime_sec"]) for row in group]
        generated_values = [parse_float(row["generated_labels"]) for row in group]
        expanded_values = [parse_float(row["expanded_labels"]) for row in group]
        frontier_values = [parse_float(row["final_frontier_size"]) for row in group]
        rss_values = [parse_float(row["peak_rss_mb"]) for row in group]
        output.append(
            {
                "dataset_id": key[0],
                "solver": key[1],
                "threads": key[2],
                "num_runs": len(group),
                "success_count": sum(1 for row in group if row["status"] == "completed"),
                "timeout_count": sum(1 for row in group if row["status"] == "timeout"),
                "crash_count": sum(1 for row in group if row["status"] == "crash"),
                "median_runtime_sec": f"{median(runtime_values):.12g}",
                "mean_runtime_sec": f"{mean(runtime_values):.12g}",
                "iqr_runtime_sec": f"{iqr(runtime_values):.12g}",
                "std_runtime_sec": f"{std(runtime_values):.12g}",
                "median_generated_labels": f"{median(generated_values):.12g}",
                "median_expanded_labels": f"{median(expanded_values):.12g}",
                "median_final_frontier_size": f"{median(frontier_values):.12g}",
                "median_peak_rss_mb": f"{median(rss_values):.12g}",
            }
        )
    return output


def scaling_summary(rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
    grouped: dict[tuple[str, str, str, str], list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        if row["mode"] != "scaling":
            continue
        grouped[(row["dataset_id"], row["query_set_id"], row["solver"], row["threads"])].append(row)

    grouped_by_solver: dict[tuple[str, str, str], dict[int, float]] = defaultdict(dict)
    for key in sorted(grouped):
        runtime_values = [parse_float(row["runtime_sec"]) for row in grouped[key]]
        grouped_by_solver[(key[0], key[1], key[2])][parse_int(key[3])] = median(runtime_values)

    output = []
    for solver_key in sorted(grouped_by_solver):
        baseline = grouped_by_solver[solver_key].get(1, math.nan)
        for threads in sorted(grouped_by_solver[solver_key]):
            median_runtime = grouped_by_solver[solver_key][threads]
            speedup = baseline / median_runtime if baseline > 0 and median_runtime > 0 else math.nan
            efficiency = speedup / threads if threads > 0 and not math.isnan(speedup) else math.nan
            output.append(
                {
                    "dataset_id": solver_key[0],
                    "query_set_id": solver_key[1],
                    "solver": solver_key[2],
                    "threads": threads,
                    "median_runtime_sec": f"{median_runtime:.12g}",
                    "speedup": "" if math.isnan(speedup) else f"{speedup:.12g}",
                    "efficiency": "" if math.isnan(efficiency) else f"{efficiency:.12g}",
                }
            )
    return output


def anytime_summary(rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
    grouped: dict[tuple[str, str, str, str], list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        trace_value = parse_float(row.get("last_trace_hv_ratio"), math.nan)
        final_reference_value = parse_float(row.get("final_reference_hv_ratio"), math.nan)
        if math.isnan(trace_value) and math.isnan(final_reference_value):
            continue
        grouped[(row["dataset_id"], row["solver"], row["threads"], row["budget_sec"])].append(row)

    output = []
    for key in sorted(grouped):
        group = grouped[key]
        trace_hv = [parse_float(row.get("last_trace_hv_ratio"), math.nan) for row in group if not math.isnan(parse_float(row.get("last_trace_hv_ratio"), math.nan))]
        trace_recall = [parse_float(row.get("last_trace_recall"), math.nan) for row in group if not math.isnan(parse_float(row.get("last_trace_recall"), math.nan))]
        trace_frontier = [parse_float(row.get("last_trace_frontier_size"), math.nan) for row in group if not math.isnan(parse_float(row.get("last_trace_frontier_size"), math.nan))]
        ttfs = [parse_float(row.get("time_to_first_solution_sec"), math.nan) for row in group if not math.isnan(parse_float(row.get("time_to_first_solution_sec"), math.nan))]
        final_ref_hv = [parse_float(row.get("final_reference_hv_ratio"), math.nan) for row in group if not math.isnan(parse_float(row.get("final_reference_hv_ratio"), math.nan))]
        final_ref_recall = [parse_float(row.get("final_reference_recall"), math.nan) for row in group if not math.isnan(parse_float(row.get("final_reference_recall"), math.nan))]
        output.append(
            {
                "dataset_id": key[0],
                "solver": key[1],
                "threads": key[2],
                "budget_sec": key[3],
                "num_runs": len(group),
                "median_hv_ratio": "" if not trace_hv else f"{median(trace_hv):.12g}",
                "median_recall": "" if not trace_recall else f"{median(trace_recall):.12g}",
                "median_frontier_size": "" if not trace_frontier else f"{median(trace_frontier):.12g}",
                "median_ttfs_sec": "" if not ttfs else f"{median(ttfs):.12g}",
                "median_reference_final_hv_ratio": "" if not final_ref_hv else f"{median(final_ref_hv):.12g}",
                "median_reference_final_recall": "" if not final_ref_recall else f"{median(final_ref_recall):.12g}",
                "reference_kind": sorted({row["reference_kind"] for row in group})[0],
            }
        )
    return output


def build_trace_curve_points(trace_samples: list[dict[str, Any]]) -> list[dict[str, Any]]:
    grouped: dict[tuple[str, str, str, str, str], list[dict[str, Any]]] = defaultdict(list)
    for sample in trace_samples:
        grouped[(sample["dataset_id"], sample["mode"], sample["solver"], str(sample["threads"]), sample["budget_sec"])].append(sample)

    output = []
    for key in sorted(grouped):
        per_run: dict[str, list[dict[str, Any]]] = defaultdict(list)
        for sample in grouped[key]:
            per_run[sample["run_id"]].append(sample)

        budget = parse_float(key[4], 0.0)
        max_time = max(max(run_sample["time_sec"] for run_sample in samples) for samples in per_run.values())
        grid_end = max(budget, max_time)
        grid = np.linspace(0.0, grid_end, 100)

        hv_stack = []
        recall_stack = []
        frontier_stack = []
        for run_id in sorted(per_run):
            samples = sorted(per_run[run_id], key=lambda row: row["time_sec"])
            times = np.array([row["time_sec"] for row in samples], dtype=float)
            hv_values = np.array([0.0 if math.isnan(row["hv_ratio"]) else row["hv_ratio"] for row in samples], dtype=float)
            recall_values = np.array([0.0 if math.isnan(row["recall"]) else row["recall"] for row in samples], dtype=float)
            frontier_values = np.array([row["frontier_size"] for row in samples], dtype=float)
            indices = np.searchsorted(times, grid, side="right") - 1
            hv_stack.append(np.where(indices >= 0, hv_values[np.clip(indices, 0, len(hv_values) - 1)], 0.0))
            recall_stack.append(np.where(indices >= 0, recall_values[np.clip(indices, 0, len(recall_values) - 1)], 0.0))
            frontier_stack.append(np.where(indices >= 0, frontier_values[np.clip(indices, 0, len(frontier_values) - 1)], 0.0))

        hv_array = np.vstack(hv_stack)
        recall_array = np.vstack(recall_stack)
        frontier_array = np.vstack(frontier_stack)
        for idx, time_sec in enumerate(grid):
            output.append(
                {
                    "dataset_id": key[0],
                    "mode": key[1],
                    "solver": key[2],
                    "threads": key[3],
                    "budget_sec": key[4],
                    "time_sec": f"{time_sec:.12g}",
                    "median_hv_ratio": f"{float(np.median(hv_array[:, idx])):.12g}",
                    "median_recall": f"{float(np.median(recall_array[:, idx])):.12g}",
                    "median_frontier_size": f"{float(np.median(frontier_array[:, idx])):.12g}",
                    "num_runs": len(per_run),
                }
            )
    return output


def build_wilcoxon_ready_pairs(rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
    grouped: dict[tuple[str, str, str, str, str, str], list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        if row["status"] in {"crash", "build_error", "skipped"}:
            continue
        grouped[(row["dataset_id"], row["query_set_id"], row["query_id"], row["mode"], row["budget_sec"], row["threads"])].append(row)

    output = []
    for key in sorted(grouped):
        candidates = sorted(grouped[key], key=lambda row: row["solver"])
        for i in range(len(candidates)):
            for j in range(i + 1, len(candidates)):
                output.append(
                    {
                        "dataset_id": key[0],
                        "query_set_id": key[1],
                        "query_id": key[2],
                        "mode": key[3],
                        "budget_sec": key[4],
                        "threads": key[5],
                        "solver_a": candidates[i]["solver"],
                        "solver_b": candidates[j]["solver"],
                        "runtime_a_sec": candidates[i]["runtime_sec"],
                        "runtime_b_sec": candidates[j]["runtime_sec"],
                        "status_a": candidates[i]["status"],
                        "status_b": candidates[j]["status"],
                    }
                )
    return output


def main() -> int:
    args = parse_args()
    repo_root = Path.cwd()
    results_root = (repo_root / args.results_root).resolve()
    analysis_id = args.analysis_id or f"{datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')}_phase7_analysis"
    output_dir = (repo_root / args.output_dir).resolve() if args.output_dir else (results_root / "figures" / sanitize_component(analysis_id))
    output_dir.mkdir(parents=True, exist_ok=True)

    suites = discover_suites(results_root, args.suite)
    source_suites = [{"suite_id": suite.name, "path": str(suite)} for suite in suites]
    write_json(output_dir / "source_suites.json", source_suites)

    raw_rows: list[dict[str, Any]] = []
    for suite in suites:
        for row in read_csv_rows(suite / "summary.csv"):
            row["suite_id"] = suite.name
            raw_rows.append(row)

    raw_rows = sorted(raw_rows, key=lambda row: (row["suite_id"], row["run_id"]))
    if not raw_rows:
        raise SystemExit("No suite summaries found to aggregate.")

    reference_rows = choose_reference_rows(raw_rows)
    enriched_rows, trace_samples, reference_manifest = enrich_rows(raw_rows, reference_rows)

    enriched_fieldnames = list(enriched_rows[0].keys()) if enriched_rows else []
    trace_fieldnames = list(trace_samples[0].keys()) if trace_samples else ["dataset_id", "mode", "solver", "threads", "budget_sec", "run_id", "time_sec", "frontier_size", "hv_ratio", "recall"]

    write_csv(output_dir / "all_runs_enriched.csv", enriched_fieldnames, enriched_rows)
    write_csv(output_dir / "trace_samples.csv", trace_fieldnames, trace_samples)
    write_json(output_dir / "reference_frontiers.json", reference_manifest)

    completion_rows = completion_summary(enriched_rows)
    scaling_rows = scaling_summary(enriched_rows)
    anytime_rows = anytime_summary(enriched_rows)
    trace_curve_rows = build_trace_curve_points(trace_samples)
    wilcoxon_rows = build_wilcoxon_ready_pairs(enriched_rows)

    if completion_rows:
        write_csv(output_dir / "completion_summary.csv", list(completion_rows[0].keys()), completion_rows)
    if scaling_rows:
        write_csv(output_dir / "scaling_summary.csv", list(scaling_rows[0].keys()), scaling_rows)
    if anytime_rows:
        write_csv(output_dir / "anytime_summary.csv", list(anytime_rows[0].keys()), anytime_rows)
    if trace_curve_rows:
        write_csv(output_dir / "trace_curve_points.csv", list(trace_curve_rows[0].keys()), trace_curve_rows)
    if wilcoxon_rows:
        write_csv(output_dir / "wilcoxon_runtime_pairs.csv", list(wilcoxon_rows[0].keys()), wilcoxon_rows)

    write_json(
        output_dir / "aggregate_manifest.json",
        {
            "created_at_utc": utc_now_iso(),
            "results_root": str(results_root),
            "output_dir": str(output_dir),
            "suite_count": len(suites),
            "raw_row_count": len(raw_rows),
            "notes": [
                "Hypervolume is computed only for 2 and 3 objectives in the Python aggregate pipeline.",
                "For higher dimensions the aggregate pipeline falls back to recall, frontier size, and TTFS.",
                "Reference frontiers prefer completed completion-mode runs and otherwise fall back to the best available final frontier.",
                "Trace hv_ratio and recall are read from raw trace artifacts and are currently self-normalized by the solver trace exporter.",
            ],
            "formulas": {
                "speedup": "S(p) = T(1) / T(p)",
                "efficiency": "eta(p) = S(p) / p",
                "runtime_iqr": "Q3(runtime) - Q1(runtime)",
            },
        },
    )

    print(f"Aggregated {len(raw_rows)} raw rows from {len(suites)} suites")
    print(f"Aggregate output: {output_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
