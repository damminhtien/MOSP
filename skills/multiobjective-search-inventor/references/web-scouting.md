# Web scouting protocol for MOSP / MOA* research

Use this file when the bundled roadmap may be incomplete or stale.

## When to browse

Browse the web when:

- the user asks for the latest, newest, current, or recent work
- the user wants direct citations, links, or venue verification
- a title, acronym, or author list may be ambiguous
- you need papers adjacent to the seed roadmap, especially for dominance checks, negative weights, parallelization, approximation, or transfer to MAPF or replanning

## Source priority

Prefer sources in this order:

1. official proceedings or journal pages
2. OpenReview for conference records that expose metadata or PDFs
3. arXiv and DBLP for discovery or preprint confirmation
4. institutional publication pages only as backups when the official page is hard to find

Do not rely on blog summaries when a primary source exists.

## Search patterns

Use exact-title search first. Good patterns include:

- `"paper title"`
- `site:ojs.aaai.org "paper title"`
- `site:ijcai.org "paper title"`
- `site:sciencedirect.com "paper title"`
- `site:openreview.net "paper title"`
- `site:dblp.org "paper title"`

For broader discovery, search by bottleneck and family, for example:

- `multi-objective search dominance checks`
- `mosp negative weights a*`
- `parallelizing multi-objective a*`
- `multi-objective search lazy dominance`
- `vectorized dominance checks mosp`

## What to extract

For each promising hit, record:

- exact title
- year and venue
- nearest lineage hook, such as BOA*, EMOA*, LTMOA*, NWMOA*, MDA, or approximate MOSP
- the claimed technical delta, such as a data structure, a search-order change, negative-weight handling, or a parallelization strategy
- whether the source is official or only a discovery proxy

## Suggested anchor papers to verify first

These titles are especially useful anchor points when expanding the bibliography:

- A Simple and Fast Bi-Objective Search Algorithm
- Simple and efficient bi-objective search algorithms via fast dominance checks
- Multi-objective Search via Lazy and Efficient Dominance Checks
- EMOA*: A framework for search-based multi-objective path planning
- Speeding Up Dominance Checks in Multi-Objective Search: New Techniques and Data Structures
- Efficient Set Dominance Checks in Multi-Objective Shortest-Path Algorithms via Vectorized Operations
- Exact Multi-objective Path Finding with Negative Weights
- Parallelizing Multi-objective A* Search

## Reporting format

When a web pass is needed, present a concise literature scan with these columns when possible:

| title | year | venue | lineage hook | why it matters |
|---|---|---|---|---|

Then end with one short paragraph stating what is confirmed, what is merely adjacent, and what remains uncertain.
