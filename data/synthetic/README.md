# Synthetic Correctness Data

Phase 4 correctness tests use programmatically generated DAG fixtures instead of
large checked-in graph files.

Covered families:

- `small_grid`
- `layered_dag`
- `random_sparse`
- `correlated_weights`
- `anti_correlated`
- `tie_heavy`

Objective counts covered by the harness:

- `2`
- `3`
- `4`

Each family exposes a small query set labeled `easy`, `medium`, and `hard`
inside `tests/correctness_harness.cpp`.

The checked-in CSV below is a fixed frontier artifact used to prove that
frontier export formatting is deterministic for a known finalized metric set:

- `expected_frontier_example.csv`
