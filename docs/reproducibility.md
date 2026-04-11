# Reproducibility

This document defines the minimum information needed to rerun a benchmark suite
or audit a reported result.

## Required Reproducibility Inputs

Every reported benchmark should preserve or cite:

- machine manifest
- compiler and build flags
- git SHA
- config snapshot
- dataset manifest
- query-set manifest
- output directory containing raw and aggregate artifacts

## Machine Manifest

The benchmark runner writes machine and environment metadata to:

- `bench/results/<suite_id>/environment.json`

This file should be kept with the suite whenever results are shared.

## Compiler And Build Flags

Build information should be captured by the configured build directory and the
suite environment metadata.

At minimum, benchmark reports should state:

- build type
- compiler family and version if known
- key dependency versions if they materially affect performance

Use a release build unless the benchmark question explicitly concerns another
build mode.

## Git SHA

Run identity should include the git SHA when available.

The benchmark runner already incorporates git context into run identity and
suite metadata where possible. Reports should cite the commit used for the
benchmark campaign.

## Config Snapshot

The exact resolved benchmark config is written to:

- `bench/results/<suite_id>/resolved_config.json`

Do not rely on a mutable config file path alone when sharing results. Preserve
the resolved snapshot from the suite output.

## Output Locations

Raw suite outputs go to:

- `bench/results/<suite_id>/`

Aggregate outputs go to:

- `bench/results/figures/<analysis_id>/`

These directories contain the artifacts needed to audit most benchmark claims.

## Deterministic Behavior Notes

The repository aims for deterministic artifact formatting even when the solver
itself is parallel.

Deterministic guarantees currently include:

- canonical summary field ordering
- canonical frontier export ordering
- deterministic figure filenames
- stable aggregate table schemas

Important caveat:

- parallel runtime behavior may still vary in timing, but exported final
  frontiers are expected to remain exact and deterministic on supported solvers

## Recommended Sharing Bundle

For a benchmark campaign intended for external review, preserve:

1. `bench/results/<suite_id>/environment.json`
2. `bench/results/<suite_id>/resolved_config.json`
3. `bench/results/<suite_id>/summary.csv`
4. referenced run-level frontier and trace artifacts
5. `bench/results/figures/<analysis_id>/aggregate_manifest.json`
6. aggregate CSV tables
7. rendered figures

## Legacy Smoke Commands

The single-query examples in `README.md` and `run_command.txt` are not a full
reproducibility protocol.

They are intentionally kept only as quick validation examples. For reproducible
benchmarking, prefer suite outputs plus aggregate outputs from the standard
runner and analysis pipeline.
