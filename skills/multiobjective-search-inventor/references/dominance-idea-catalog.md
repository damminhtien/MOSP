# Dominance-check idea catalog

Use this catalog to generate grounded algorithm ideas. Focus on one primary bottleneck first. A good dominance-check paper usually changes only one major mechanism and supports it with careful proofs and ablations.

## 1. Diagnose the bottleneck before proposing a variant

Ask these questions in order:

1. Is the cost dominated by **many queries** or by **expensive queries**?
2. Are front sets mostly **tiny**, **medium**, or **occasionally huge**?
3. Are objectives highly correlated, weakly correlated, or mixed?
4. Is the workload single-threaded, batched, or naturally parallel?
5. Is the target setting exact, approximate, or anytime?

Choose a mechanism only after the bottleneck is explicit.

## 2. Safety rule for exact methods

For an exact algorithm, every fast filter must obey this rule:

- it may safely say **"not dominated"** only if that claim is exact, or after an exact fallback
- it may safely say **"maybe dominated"** and trigger a slower exact check
- if a learned or approximate component is used, it must only reorder or shortlist checks unless the method is intentionally approximate

This single rule prevents many unsound ideas.

## 3. Low-risk idea families for exact dominance checks

| family | mechanism | best regime | nearest lineage | proof burden | key ablation |
|---|---|---|---|---|---|
| two-stage filter | cheap necessary-condition screen plus exact fallback | many easy rejects | boa*, ltmoa* | prove the screen never causes false domination | screen on/off |
| adaptive representation | switch array vs tree vs bucketed layout by front size | mixed front sizes | emoa*, ltmoa* | prove representation changes only implementation, not semantics | fixed layout vs adaptive |
| sorted projection index | keep fronts sorted by one or more objective projections and short-circuit scans | moderate objectives, structured fronts | boa*, emoa* | prove skipped regions cannot contain dominators | one projection vs many |
| witness caching | cache last dominator or recent successful witness for recurring query shapes | repeated local query patterns | ltmoa* | handle invalidation after front updates | cache on/off |
| batched or simd checks | structure-of-arrays layout with vectorized comparison batches | heavy inner-loop cost | vectorized SDC papers | preserve exact decision logic under batching | scalar vs batched |
| duplicate-query suppression | detect repeated checks or redundant candidate labels before full dominance work | many near-duplicate labels | lazy dominance papers | prove redundancy criterion is sound | suppression on/off |
| frontier segmentation | partition fronts by coarse primary-cost or envelope ranges | large fronts with structure | emoa*, t-mda style ordering | prove segment pruning is exact | segmented vs flat front |
| parallel dominance service | submit exact checks in batches to workers while preserving semantics | large exact workloads with spare cores | parallel moa* | race-free update and deterministic semantics | one thread vs many |

## 4. Medium-risk idea families

These are still plausible, but they create more proof and engineering burden.

| family | mechanism | main risk |
|---|---|---|
| multi-projection ensemble | maintain several sorted views and choose the view adaptively | index maintenance may dominate runtime |
| lazy buffered updates | accumulate inserts and prune in batches | stale fronts may interact badly with queue ordering |
| bound-aware filtering | use stronger lower bounds to eliminate impossible dominators or candidates earlier | heuristic assumptions become central to correctness |
| regime-switch heuristics | tune representation or checking policy from online statistics | adaptation overhead and unstable thresholds |

## 5. High-risk or approximate ideas

Use these only when you are explicit about the risk.

| family | safe use | unsafe use |
|---|---|---|
| learned classifier | reorder or shortlist exact checks | direct pruning without exact fallback |
| quantized buckets | approximate prefilter with exact fallback | exact claim from coarse bucket order alone |
| epsilon-dominance front | approximation algorithm with stated guarantee | calling the method exact |
| compressed front sketches | quick negative tests with exact fallback | full replacement for the front set |

## 6. Good concept-note patterns

### Pattern A: adaptive exact dominance index

Use when front sizes vary sharply across vertices.

- small fronts: flat array scan
- medium fronts: sorted structure-of-arrays with projection filter
- large fronts: balanced tree or segmented index
- exact fallback at every stage

Likely contribution: better average-case runtime without changing search semantics.

### Pattern B: certificate-backed lazy dominance

Use when the same region of objective space is queried repeatedly.

- maintain certificates or witness pointers from previous successful dominators
- verify certificate validity after front updates
- fall back to exact search when the certificate fails

Likely contribution: fewer repeated full scans on road-network style workloads.

### Pattern C: simd-friendly lazy LTMOA*

Use when the inner loop is already dominated by tuple comparisons.

- convert front storage to structure-of-arrays
- batch several candidate checks
- keep the same exact semantics as scalar LTMOA*

Likely contribution: constant-factor speedups that remain easy to explain.

## 7. How to turn an idea into a credible paper pitch

For each idea, always state:

1. nearest three ancestors
2. exact delta
3. why the old method slows down on the intended regime
4. why the new mechanism should help there
5. what must be proved
6. what graphs and objective counts should expose the benefit

If any of these six answers is weak, the idea is not ready.
