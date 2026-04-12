# Reusable algorithm design patterns

Use these patterns to generate grounded algorithm variants instead of free-form brainstorming. Pick one primary bottleneck first, then choose a mechanism that directly attacks it.

## 1. Dominance-check acceleration

Typical mechanisms:

- balanced binary search trees
- lazy dominance checks
- batched or vectorized set checks
- specialized dominance indexes

Use this line when the nondominated label sets and dominance queries dominate runtime.

Check carefully:

- soundness of every pruning decision
- whether the data structure changes asymptotic or only constant factors
- amortized versus worst-case dominance cost
- memory growth of the dominance index

Typical nearest baselines: BOA*, EMOA*, LTMOA*, LazyLTMOA*.

## 2. Search ordering and expansion policy

Typical mechanisms:

- lexicographic or layered expansion keys
- targeted expansion for one-to-one settings
- deferred or staged dominance evaluation
- tighter lower-bound ordering

Use this line when exactness is acceptable but expansion order causes waste.

Check carefully:

- ordering changes efficiency only; it must not change the final nondominated set unless the method is approximate
- duplicate handling under the new ordering
- whether tie-breaking interacts with laziness or reopening

Typical nearest baselines: BOA*, T-MDA, targeted MOSP methods.

## 3. Heuristic strengthening

Typical mechanisms:

- differential heuristics
- multi-valued heuristics
- objective-wise lower-bound portfolios
- domain-specific admissible relaxations

Use this line when a better lower bound can shrink the search dramatically.

Check carefully:

- admissibility of each objective component
- consistency, or else a correct reopening policy
- how multiple lower bounds are combined without deleting pareto-optimal labels

Typical nearest baselines: BOS with differential heuristics, targeted exact methods.

## 4. Harder cost models and negative weights

Typical mechanisms:

- negative-weight-safe label reopening
- cycle control and explicit negative-cycle assumptions
- potential reweighting or reduced-cost reasoning when valid

Use this line when the cost model breaks the assumptions of standard MOA* variants.

Check carefully:

- termination under cycles
- distinction between negative weights and negative cycles
- which proofs need to be reworked from scratch

Typical nearest baseline: NWMOA*.

## 5. Parallelization and systems scaling

Typical mechanisms:

- partitioned open lists
- shared or replicated dominance indexes
- work stealing by graph region or objective bucket
- batched dominance checks

Use this line when the algorithm is exact but hardware utilization is poor.

Check carefully:

- race conditions in duplicate detection
- deterministic versus nondeterministic output order
- synchronization cost versus search savings

Typical nearest baseline: Parallelizing Multi-objective A* Search.

## 6. Approximation, anytime, and subset methods

Typical mechanisms:

- epsilon-dominance
- representative subset extraction
- anytime refinement schedules
- preprocessing and warm starts

Use this line when exact Pareto fronts are too large or latency matters more than completeness.

Check carefully:

- exact meaning of the approximation guarantee
- diversity versus coverage trade-offs
- whether the method remains anytime after each pruning rule is added

Typical nearest baselines: A*pex, A-BOA*, subset approximation papers.

## 7. Dynamic, replanning, and multi-agent transfer

Typical mechanisms:

- incremental repair of pareto sets
- safe-interval reasoning
- CBS-style decomposition with pareto-aware low level search

Use this line when the environment changes over time or when conflicts couple multiple agents.

Check carefully:

- what remains local to a single agent and what must be re-proved globally
- whether replanning preserves previously computed labels soundly
- how conflicts interact with multi-objective optimality

Typical nearest baselines: path-based D* Lite, safe-interval methods, MO-CBS.

## 8. Good hybrid proposals

Useful hybrids often combine one exact-core line with one well-justified extension, for example:

- EMOA* plus lazy dominance
- T-MDA style targeting plus stronger MOS heuristics
- negative-weight-safe search plus targeted expansion
- exact core plus approximate warm start

For hybrids, keep one primary contribution and treat the second mechanism as support. If both mechanisms are equally central, the paper becomes much harder to explain, prove, and ablate.
