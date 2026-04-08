#!/usr/bin/env python3

from __future__ import annotations

import argparse
import csv
from pathlib import Path
from typing import Any

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Plot MOSP aggregate benchmark results")
    parser.add_argument("--input-dir", required=True, help="Aggregate result directory from aggregate_results.py")
    return parser.parse_args()


def read_csv_rows(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle))


def parse_float(value: Any, default: float = float("nan")) -> float:
    if value in (None, "", "nan"):
        return default
    return float(value)


def save_figure(fig: plt.Figure, output_dir: Path, stem: str) -> None:
    output_dir.mkdir(parents=True, exist_ok=True)
    fig.tight_layout()
    fig.savefig(output_dir / f"{stem}.png", dpi=220, bbox_inches="tight")
    fig.savefig(output_dir / f"{stem}.svg", bbox_inches="tight")
    plt.close(fig)


def plot_runtime_boxplot(all_runs: list[dict[str, str]], output_dir: Path) -> None:
    groups: dict[str, list[float]] = {}
    for row in all_runs:
        if row["status"] in {"crash", "build_error", "skipped"} or row["mode"] == "completion":
            continue
        label = f"{row['solver']} [p={row['threads']}]"
        groups.setdefault(label, []).append(parse_float(row["runtime_sec"]))

    labels = sorted(groups)
    if not labels:
        return

    data = [groups[label] for label in labels]
    fig, ax = plt.subplots(figsize=(9, 5))
    ax.boxplot(data, tick_labels=labels, patch_artist=True)
    ax.set_title("Runtime Distribution by Solver")
    ax.set_ylabel("Runtime (s)")
    ax.set_xlabel("Solver")
    ax.grid(axis="y", alpha=0.3)
    ax.tick_params(axis="x", rotation=20)
    save_figure(fig, output_dir, "runtime_boxplot")


def plot_scaling_curve(rows: list[dict[str, str]], output_dir: Path, value_key: str, title: str, ylabel: str, stem: str) -> None:
    groups: dict[str, list[tuple[int, float]]] = {}
    for row in rows:
        value = parse_float(row.get(value_key))
        if np.isnan(value):
            continue
        label = f"{row['solver']} @ {row['dataset_id']}/{row['query_set_id']}"
        groups.setdefault(label, []).append((int(row["threads"]), value))

    if not groups:
        return

    fig, ax = plt.subplots(figsize=(8, 5))
    for label in sorted(groups):
        points = sorted(groups[label], key=lambda item: item[0])
        ax.plot([point[0] for point in points], [point[1] for point in points], marker="o", label=label)
    ax.set_title(title)
    ax.set_xlabel("Threads p")
    ax.set_ylabel(ylabel)
    ax.grid(alpha=0.3)
    ax.legend()
    save_figure(fig, output_dir, stem)


def plot_anytime_curves(rows: list[dict[str, str]], output_dir: Path, metric_key: str, title: str, ylabel: str, stem: str) -> None:
    groups: dict[str, list[tuple[float, float]]] = {}
    for row in rows:
        value = parse_float(row.get(metric_key))
        if np.isnan(value):
            continue
        label = f"{row['mode']} | {row['solver']} [p={row['threads']}] @ {row['dataset_id']}"
        groups.setdefault(label, []).append((parse_float(row["time_sec"]), value))

    if not groups:
        return

    fig, ax = plt.subplots(figsize=(9, 5))
    for label in sorted(groups):
        points = sorted(groups[label], key=lambda item: item[0])
        ax.plot([point[0] for point in points], [point[1] for point in points], label=label)
    ax.set_title(title)
    ax.set_xlabel("Time (s)")
    ax.set_ylabel(ylabel)
    ax.grid(alpha=0.3)
    ax.legend(fontsize=8)
    save_figure(fig, output_dir, stem)


def plot_memory_bar(all_runs: list[dict[str, str]], output_dir: Path) -> None:
    groups: dict[str, list[float]] = {}
    for row in all_runs:
        if row["status"] in {"crash", "build_error", "skipped"}:
            continue
        label = f"{row['solver']} [p={row['threads']}]"
        groups.setdefault(label, []).append(parse_float(row["peak_rss_mb"]))

    labels = sorted(groups)
    if not labels:
        return

    medians = [float(np.median(groups[label])) for label in labels]
    fig, ax = plt.subplots(figsize=(9, 5))
    ax.bar(labels, medians)
    ax.set_title("Median Peak RSS by Solver")
    ax.set_ylabel("Peak RSS (MB)")
    ax.set_xlabel("Solver")
    ax.grid(axis="y", alpha=0.3)
    ax.tick_params(axis="x", rotation=20)
    save_figure(fig, output_dir, "memory_bar")


def main() -> int:
    args = parse_args()
    input_dir = Path(args.input_dir).resolve()
    figures_dir = input_dir / "figures"

    all_runs = read_csv_rows(input_dir / "all_runs_enriched.csv")
    scaling_rows = read_csv_rows(input_dir / "scaling_summary.csv")
    trace_curve_rows = read_csv_rows(input_dir / "trace_curve_points.csv")

    plot_runtime_boxplot(all_runs, figures_dir)
    plot_scaling_curve(scaling_rows, figures_dir, "speedup", "Speedup Curve", "Speedup S(p)", "speedup_curve")
    plot_scaling_curve(scaling_rows, figures_dir, "efficiency", "Efficiency Curve", "Efficiency eta(p)", "efficiency_curve")
    plot_anytime_curves(trace_curve_rows, figures_dir, "median_hv_ratio", "Anytime HV Ratio vs Time", "Median HV Ratio", "anytime_hv_ratio")
    plot_anytime_curves(trace_curve_rows, figures_dir, "median_recall", "Recall vs Time", "Median Recall", "recall_curve")
    plot_memory_bar(all_runs, figures_dir)

    print(f"Figure output: {figures_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
