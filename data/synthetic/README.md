# Synthetic Correctness Data

Phase 4 correctness testing uses generated DAG fixtures rather than large
checked-in graph files.

Covered families:

- `small_grid`
- `layered_dag`
- `random_sparse`
- `correlated_weights`
- `anti_correlated`
- `tie_heavy`

Covered objective counts:

- `2`
- `3`
- `4`

Each family exposes small `easy`, `medium`, and `hard` queries inside
`tests/correctness_harness.cpp`.

The checked-in artifact in this directory is:

- `expected_frontier_example.csv`

That file is used by the correctness harness to prove that frontier export is
deterministic and format-stable for a known finalized frontier.
