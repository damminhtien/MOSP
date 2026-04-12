# Hybrid Corridor Pulse-A* NYC Benchmark

Date: 2026-04-12

Question: does the exact-first serial `HybridCorridorPulseA` prototype beat the
current serial LTMOA baselines on the stable NYC `88959 -> 96072` 3-objective
query under time-capped anytime evaluation?

Prototype summary:

- lower-bound corridor gating via `g(u) + h(u)` against the incumbent skyline
- short seed phase using one lexicographic pass plus the scalar warm-start
  weight family from `LTMOA_array_superfast_lb`
- iterative DFS-style burst scheduling with exact spillback to the global
  lexicographic OPEN
- dimensionality-reduced node dominance via `truncate(f)` and `FastGclArray`

Benchmark command:

```bash
python3 bench/scripts/run_benchmark.py \
  --config bench/configs/nyc_hybrid_corridor_pulsea.yaml \
  --suite-id nyc_hybrid_corridor_pulsea_20260412
```

Raw archived artifact:

- [nyc_hybrid_corridor_pulsea_5x_summary.csv](/Users/macbook/Desktop/workspace/SOPMOA/notes/benchmarks/2026-04-12-hybrid-corridor-pulsea/nyc_hybrid_corridor_pulsea_5x_summary.csv)

## Practical Takeaway

`HybridCorridorPulseA` passed exactness and canonical benchmark instrumentation,
but it is a `no-go` for promotion into the serial mainline on this archived NYC
query.

- Against `LTMOA_array_superfast`, runtime stayed near parity, but anytime
  frontier quality was much worse at every budget:
  - `0.5s`: `184` vs `512` frontier, ratio `0.359x`
  - `1.0s`: `262` vs `696` frontier, ratio `0.376x`
  - `2.0s`: `401` vs `906` frontier, ratio `0.443x`
  - `5.0s`: `691` vs `1328` frontier, ratio `0.520x`
- First-solution timing also lost to `LTMOA_array_superfast` at every budget:
  - `0.5s`: `3.252ms` vs `1.764ms`, ratio `1.844x`
  - `1.0s`: `3.295ms` vs `1.632ms`, ratio `2.019x`
  - `2.0s`: `3.724ms` vs `1.377ms`, ratio `2.704x`
  - `5.0s`: `3.292ms` vs `1.693ms`, ratio `1.944x`
- RSS was also consistently higher than `LTMOA_array_superfast`:
  - `0.5s`: `230.0 MB` vs `174.9 MB`
  - `1.0s`: `251.2 MB` vs `191.5 MB`
  - `2.0s`: `289.8 MB` vs `211.6 MB`
  - `5.0s`: `373.6 MB` vs `254.1 MB`

The prototype did beat `LTMOA_array_superfast_lb`, so the corridor + burst
design is not useless in absolute terms:

- frontier ratio vs `_lb`: `1.64x`, `1.70x`, `1.84x`, `1.91x`
- first-solution ratio vs `_lb`: `0.41x`, `0.37x`, `0.48x`, `0.43x`
- runtime stayed essentially flat vs `_lb`

That still does not clear the repo's actual bar, which is the strongest serial
baseline, not the regressed lower-bound branch.

## Additional Observation

The C++ benchmark `runtime_sec` respected the search budgets, but the
runner-observed wall runtime for `HybridCorridorPulseA` was much higher:

- `0.5s` budget: median wall runtime `4.38s`
- `1.0s` budget: median wall runtime `5.06s`
- `2.0s` budget: median wall runtime `6.25s`
- `5.0s` budget: median wall runtime `8.91s`

That points to significant post-budget overhead or slow solver shutdown even in
summary-only mode. This is not the primary reason for the no-go decision, but
it is a real secondary risk if this line is revisited.

## Verdict

- keep the prototype archived as an exact serial experiment
- do not fold it into `LTMOA_array_superfast`
- if revisited, first target corridor selectivity and post-budget overhead
  before asking it to compete with the current serial baseline again
