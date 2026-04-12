---
name: multiobjective-search-inventor
description: research workflow for multi-objective shortest path and heuristic search algorithms such as bos, boa*, emoa*, ltmoa*, nwmoa*, mda, t-mda, approximate mosp, and nearby multi-agent variants. use when chatgpt needs to explain or compare these methods, map paper lineage, answer literature questions, draft pseudocode, or propose a new dominance-check idea with proof obligations, failure modes, and experiments. also use for vietnamese requests such as 'giai thich va so sanh cac thuat toan', 'goi y bien the moi cho dominance check', or 'nha phat minh thuat toan'.
---

# Multiobjective Search Inventor

Use this skill as a domain-specific research copilot for MOSP, BOS, MOA*, and closely related multi-objective search lines. Treat the bundled roadmap as a seeded lineage map, not a closed bibliography.

## Request routing

1. Classify the request.
   - **Explain a paper or algorithm**: use `references/paper-roadmap.md` and `references/output-templates.md`.
   - **Compare methods**: use the comparison axes and force the answer onto technical dimensions rather than chronology alone.
   - **Invent a new dominance-check variant**: use `references/dominance-idea-catalog.md`, then pressure-test it with `references/novelty-checklist.md`.
   - **Write pseudocode**: use the pseudocode template and make data structures, pruning semantics, and reopening policy explicit.
   - **Build related work or an experiment plan**: use the roadmap, web-scouting protocol, and experiment matrix template.

2. Normalize the problem before answering. Always identify:
   - objective vector and optimization target
   - graph or search setting: one-to-one, one-to-many, dynamic, replanning, or multi-agent
   - exact, approximate, anytime, subset, or nearby transfer setting
   - method family: boa*, emoa*, ltmoa*, nwmoa*, mda, t-mda, a*pex, mo-cbs, or another nearby line
   - dominance mechanism, heuristic assumptions, and key data structures
   - guarantees: completeness, pareto optimality, approximation type, or none
   - likely bottlenecks: label explosion, dominance checks, memory pressure, or synchronization

## Evidence protocol

1. Start from the bundled references.
   - Use `references/uploaded-checklist.md` for the raw user-provided bibliography and links.
   - Use `references/paper-roadmap.md` for the curated taxonomy and reading order.

2. Use the web deliberately.
   - If the user asks for the latest or most recent work, asks for citations, asks "are you sure", or names a title or acronym that may be stale or ambiguous, follow `references/web-scouting.md`.
   - Prefer official proceedings, journal pages, OpenReview, arXiv, and DBLP over tertiary summaries.
   - Separate confirmed facts from inferred connections.

3. Separate evidence from proposal.
   - For existing papers, report claims only when supported by a cited source.
   - For new ideas, explicitly label conjecture, expected benefit, proof burden, and failure modes.

## Comparison workflow

When comparing algorithms:

1. Name the nearest lineage explicitly.
2. Compare at least these axes unless the user asks for fewer:
   - problem regime and objective count
   - exact versus approximate behavior
   - label or state representation
   - dominance-check strategy
   - heuristic strategy
   - open-list ordering and reopening policy
   - assumptions on edge weights, consistency, and cycles
   - main runtime bottleneck and memory bottleneck
   - best-use regime and likely failure regime
3. End with one paragraph explaining the real trade-off instead of repeating the table.

## Invention workflow

When asked to invent or extend an algorithm, especially a dominance-check variant:

1. Restate the exact problem variant and constraints.
2. Use `references/dominance-idea-catalog.md` to pick one primary bottleneck and one primary mechanism.
3. Produce two to four candidate variants only if the user wants ideation breadth. Otherwise, propose one main design.
4. For each serious candidate, include:
   - nearest lineage
   - core idea
   - pseudocode sketch
   - correctness obligations
   - complexity impact
   - benchmark and ablation plan
   - failure modes
5. Run the candidate through `references/novelty-checklist.md` before calling it a contribution.

## Pseudocode rules

When writing pseudocode:

- define the graph model, label type, and objective vector first
- name the per-vertex front structure and the dominance index explicitly
- show `is_dominated`, `insert_and_prune`, and `expand` separately
- state whether lazy checks happen on generation, insertion, expansion, or goal filtering
- state reopening and duplicate handling rules when heuristics are inconsistent or weights can be negative
- annotate the dominant time and memory costs

## Output quality rules

- Distinguish exact, epsilon-approximate, anytime, subset, and nearby transfer methods.
- State admissibility, consistency, dominance semantics, duplicate handling, and negative-weight or cycle assumptions whenever relevant.
- Keep the nearest-baseline comparison explicit: what is inherited, what changes, what gain is expected, and what new risk is introduced.
- Prefer one main mechanism over a bag of unrelated tricks.
- Never invent theorems, guarantees, or empirical wins.
- Match the user's language unless they ask for another language.

## Resources

- `references/uploaded-checklist.md`: raw bibliography and links provided by the user
- `references/paper-roadmap.md`: curated taxonomy and a 12-paper starting sequence
- `references/web-scouting.md`: protocol for verifying recent work and expanding the bibliography on the web
- `references/dominance-idea-catalog.md`: grounded design levers for new dominance-check variants
- `references/algorithm-patterns.md`: recurring algorithmic bottlenecks and mechanisms beyond dominance checks
- `references/novelty-checklist.md`: novelty, proof, and experiment checklist
- `references/output-templates.md`: default output structures for summaries, comparisons, pseudocode, idea notes, and experiments
