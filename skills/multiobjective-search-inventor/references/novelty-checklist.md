# Novelty, proof, and experiment checklist

Use this checklist before calling an idea a new algorithm.

## 1. Novelty test

Answer these questions explicitly:

1. What are the nearest three ancestors?
2. What exact delta is new: data structure, heuristic, search order, guarantee, problem setting, or application transfer?
3. Why do the nearest ancestors fail, slow down, or become awkward on this variant?
4. Is the contribution algorithmic, theoretical, engineering, empirical, or only applicational?
5. Which assumptions changed relative to the nearest baseline?

If the delta is only a benchmark-domain transfer or only a low-level implementation detail, describe it honestly as such.

## 2. Correctness obligations

Check all that apply:

- state and label representation are defined unambiguously
- dominance relation is explicit
- every pruning rule is sound
- duplicate detection and reopening are correct
- heuristic assumptions are stated objective-wise
- termination conditions are clear
- exactness versus approximation is explicit
- negative weights or cycles are handled separately when relevant
- laziness or deferred checks do not silently change semantics

## 3. Complexity obligations

Always discuss:

- worst-case labels per node
- dominance query cost
- open-list cost
- memory footprint of per-node pareto sets or indexes
- synchronization overhead for parallel methods

## 4. Experiment obligations

Design experiments that make the contribution falsifiable.

Include at least:

- strong baselines from the closest lineage
- graphs or domains that trigger the claimed bottleneck
- objective counts such as 2, 3, and larger where relevant
- objective correlation regimes when approximation or heuristics matter
- one-to-one versus broader settings when targeting matters
- metrics: wall-clock, expansions, generated labels, dominance checks, memory, and solution-set size
- ablations removing each claimed mechanism one at a time

## 5. Red flags

Treat these as warnings:

- compares only to weak or outdated baselines
- changes multiple mechanisms without ablation
- hides admissibility or consistency assumptions
- claims exactness without a full pruning proof
- claims novelty when only the application domain changed
- relies on negative weights without discussing negative cycles or reopening
