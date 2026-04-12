# Hybrid Corridor Pulse-A* Repair Gate

Date: 2026-04-12

Question: after replacing the hybrid's heavy landmark oracle with the cheaper
reverse-Dijkstra heuristic and shrinking burst scheduling to a conservative
preferred-child spillback policy, does `HybridCorridorPulseA` still stay exact,
reduce runner-observed wall overhead, and beat `LTMOA_array_superfast` on the
broader NYC anytime gate?

Broader repair-gate benchmark command:

```bash
python3 bench/scripts/run_benchmark.py \
  --config bench/configs/nyc_hybrid_corridor_pulsea_repair.yaml \
  --suite-id nyc_hybrid_corridor_pulsea_repair_20260412
```

Raw archived artifact:

- [nyc_hybrid_corridor_pulsea_repair_5x_summary.csv](/Users/macbook/Desktop/workspace/SOPMOA/notes/benchmarks/2026-04-12-hybrid-corridor-pulsea-repair/nyc_hybrid_corridor_pulsea_repair_5x_summary.csv)

## Practical Takeaway

The repaired `HybridCorridorPulseA` is a real improvement over the archived v1
prototype.

- It cleared the broader NYC acceptance gate on `4/4` non-trivial queries.
- The large startup wall-time overhead on the archived hard query was mostly
  removed.
- The hybrid now beats `LTMOA_array_superfast` on anytime frontier size on all
  four repair-gate queries, though it still loses on time-to-first-solution.

The gate used the repo's non-trivial NYC queries:

- `single_88959_96072`
- `query3_6`
- `query3_1`
- `query3_2`

A query counted as a win if `HybridCorridorPulseA` matched or beat
`LTMOA_array_superfast` frontier size on at least `2` of the `0.5 / 1 / 2s`
budgets while staying within `1.05x` solver `runtime_sec` and `1.15x` runner
`wall_runtime_sec` on those same budgets.

Result:

- `single_88959_96072`: win
- `query3_6`: win
- `query3_1`: win
- `query3_2`: win

## Hard Query Before vs After

On the original archived NYC `88959 -> 96072` query, the repair flipped the
prototype from a clear no-go into a frontier-positive result against
`LTMOA_array_superfast`.

Median frontier ratio vs `LTMOA_array_superfast` on `single_88959_96072`:

- `0.5s`: `1.627x`
- `1.0s`: `1.565x`
- `2.0s`: `1.562x`
- `5.0s`: `1.411x`

Median runner `wall_runtime_sec` on the same query:

- archived v1 hybrid: `4.38s`, `5.06s`, `6.25s`, `8.91s`
- repaired hybrid: `2.31s`, `2.97s`, `3.91s`, `7.66s`
- wall-time ratio repaired / archived: `0.528x`, `0.588x`, `0.626x`, `0.859x`

Solver-side `runtime_sec` stayed near parity with `LTMOA_array_superfast`:

- `0.5s`: ratio `1.0059x`
- `1.0s`: ratio `1.0057x`
- `2.0s`: ratio `1.0048x`

## Broader Query Summary

On the easy and medium `nyc_query6_pair` queries, the hybrid moved to slight but
consistent frontier wins with acceptable wall/runtime ratios:

- `query3_1`: frontier ratio `1.006x` at `0.5 / 1 / 2s`
- `query3_2`: frontier ratio `1.048x` at `0.5 / 1 / 2s`

On the hard `nyc_query6_single` query, the hybrid stayed close at short budgets
and opened a large frontier lead by `2s`:

- `0.5s`: frontier ratio `1.016x`
- `1.0s`: frontier ratio `1.058x`
- `2.0s`: frontier ratio `2.152x`

The only early-budget gate miss inside a winning query was `query3_6` at `1s`,
where the hybrid's frontier improved but median wall time rose to `1.296x` of
`LTMOA_array_superfast`. The same query still passed at `0.5s` and `2.0s`, so
it remained a query-level win under the agreed gate.

## Remaining Risks

- First-solution timing is still materially worse than `LTMOA_array_superfast`
  on every query. Early-budget TTFS ratios were roughly `2.1x` to `3.3x`.
- The suite had `10` runner-level timeouts, all in `LTMOA_array_superfast_lb`
  rows on the hard query set or long-tail repeats. `HybridCorridorPulseA` and
  `LTMOA_array_superfast` completed cleanly across this repair gate.
- This archive is still NYC-only. The repaired hybrid is now worth keeping as an
  experimental line, but it is not yet a cross-dataset proof.
