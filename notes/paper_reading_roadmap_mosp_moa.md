# MOSP / MOA* Paper Reading Roadmap

Date: 2026-04-12

Purpose: keep a research-ordered reading tracker for exact MOSP / MOA* work,
nearby shortest-path literature, and approximate / dynamic extensions so
future implementation passes can be grounded in the right paper cluster first.

Conventions:

- `★`: read first
- `seed`: the 4 papers that were already selected as the initial seed set
- `nearby`: closely related, but no longer core exact MOA* / exact MOSP

Reading status codes:

- `unread`: not started
- `queued`: selected as part of the next reading batch
- `reading`: actively being read
- `skimmed`: quick pass done, but no stable notes yet
- `notes-written`: paper notes or extraction notes exist in repo
- `implemented`: paper ideas have already influenced repo code or benchmarks
- `parked`: intentionally deferred

Suggested per-paper update rule:

1. Move `unread -> queued` when the paper enters the next research batch.
2. Move `queued -> reading` while actively reading.
3. Move `reading -> skimmed` or `notes-written` after the first pass.
4. Move `notes-written -> implemented` only when a concrete repo change,
   benchmark pass, or paper note ties back to that paper.

## Cluster-to-Implementation Mapping

| Cluster | Scope | Primary repo hook | Candidate artifact / issue shape | Trigger |
| --- | --- | --- | --- | --- |
| `C1` | Core exact MOSP / MOA* / BOS | exact solver structure, OPEN policy, dominance pruning, label/frontier design | solver pass on `LTMOA_array_superfast`, `LTMOA_parallel`, `SOPMOA_relaxed`; correctness harness update; benchmark comparison note | use before changing exact search logic |
| `C2` | Survey / theory | related-work framing, novelty boundaries, claim discipline | paper note, intro/related-work draft, theorem or assumption checklist | use before making strong novelty claims |
| `C3` | OR-nearby exact shortest path | external baseline framing and one-to-one exact MOSP comparisons | benchmark note, baseline comparison task, issue about MDA / T-MDA style ideas | use before claiming competitiveness outside AI-search |
| `C4` | Heuristic / dominance / engineering | heuristic design, dominance data structures, vectorization, low-level pruning | issue or branch for heuristic experiments, dominance-check microbenchmark, CPU optimization pass | use before touching hot-path pruning or heuristic code again |
| `C5` | Approximate / anytime / subset | anytime and bounded-quality search directions | new approximate solver branch, benchmark suite for quality-vs-time, archived approximation note | use when repo scope moves beyond exact skyline search |
| `C6` | Dynamic / replanning / MAPF | non-static-road, replanning, and multi-agent extensions | separate extension note, new module branch, architecture decision note | use only if repo scope expands beyond single-agent static MOSP |
| `C7` | Older background | deep background, taxonomy cleanup, historical framing | supporting literature note or appendix note | use only when core exact reading leaves a gap |

Issue / implementation mapping rule:

- When a concrete task appears, create a short issue-style slug such as
  `impl-exact-open-policy`, `bench-mda-baseline`, or `note-mos-theory`, then
  attach that slug to the relevant paper IDs in the tables below.
- If multiple papers map to one implementation pass, keep the shared slug in
  the `Repo hook` field and capture the final outcome in
  `notes/agent_memory_log.md`.

## 1) Core Exact MOSP / MOA* / BOS

| ID | Status | Priority | Paper | Repo hook |
| --- | --- | --- | --- | --- |
| `C1-P01` | `unread` | `★` | [A Simple and Fast Bi-Objective Search Algorithm](https://ojs.aaai.org/index.php/ICAPS/article/view/6655?utm_source=chatgpt.com) — ICAPS 2020 | `impl-exact-bos-baseline` |
| `C1-P02` | `unread` | `★` | [Simple and efficient bi-objective search algorithms via fast dominance checks](https://www.sciencedirect.com/science/article/pii/S0004370222001473?utm_source=chatgpt.com) — Artificial Intelligence 2023 | `impl-fast-dominance-baseline` |
| `C1-P03` | `unread` | `★`, `seed` | [Enhanced Multi-Objective A* Using Balanced Binary Search Trees](https://ojs.aaai.org/index.php/SOCS/article/view/21764?utm_source=chatgpt.com) — SoCS 2022 | `impl-emoa-structure` |
| `C1-P04` | `unread` | `★`, `seed` | [Multi-objective Search via Lazy and Efficient Dominance Checks](https://www.ijcai.org/proceedings/2023/0850.pdf?utm_source=chatgpt.com) — IJCAI 2023 | `impl-ltmoa-lazy-dominance` |
| `C1-P05` | `unread` | `★`, `seed` | [Exact Multi-objective Path Finding with Negative Weights](https://ojs.aaai.org/index.php/ICAPS/article/view/31455?utm_source=chatgpt.com) — ICAPS 2024 | `investigate-negative-weights` |
| `C1-P06` | `unread` |  | [EMOA*: A framework for search-based multi-objective path planning](https://www.sciencedirect.com/science/article/abs/pii/S0004370224001966?utm_source=chatgpt.com) — Artificial Intelligence 2025 | `note-emoa-framework` |
| `C1-P07` | `unread` |  | [Parallelizing Multi-objective A* Search](https://ojs.aaai.org/index.php/ICAPS/article/view/36109?utm_source=chatgpt.com) — ICAPS 2025 | `impl-parallel-moa` |
| `C1-P08` | `unread` |  | [Deeper Treatment of the Bi-objective Search Framework](https://ojs.aaai.org/index.php/AAAI/article/view/41047?utm_source=chatgpt.com) — AAAI 2026 | `note-bos-framework-depth` |

## 2) Survey / Theory

| ID | Status | Priority | Paper | Repo hook |
| --- | --- | --- | --- | --- |
| `C2-P01` | `unread` | `★` | [Heuristic-Search Approaches for the Multi-Objective Shortest-Path Problem: Progress and Research Opportunities](https://www.ijcai.org/proceedings/2023/0757.pdf?utm_source=chatgpt.com) — IJCAI 2023 | `note-mosp-survey-framing` |
| `C2-P02` | `unread` | `★` | [Theoretical Study on Multi-objective Heuristic Search](https://www.ijcai.org/proceedings/2024/0776.pdf?utm_source=chatgpt.com) — IJCAI 2024 | `note-mos-theory` |
| `C2-P03` | `unread` |  | [Multi-Objective Search: Algorithms, Applications, and Emerging Directions](https://ojs.aaai.org/index.php/AAAI/article/view/42134/46588?utm_source=chatgpt.com) — AAAI 2026 | `note-mos-survey-2026` |

## 3) Very Close but More OR / Exact Shortest-Path Oriented

| ID | Status | Priority | Paper | Repo hook |
| --- | --- | --- | --- | --- |
| `C3-P01` | `unread` | `★` | [An Improved Multiobjective Shortest Path Algorithm](https://www.sciencedirect.com/science/article/abs/pii/S0305054821001842?utm_source=chatgpt.com) — Computers & Operations Research 2021 | `bench-mda-baseline` |
| `C3-P02` | `unread` | `★` | [Targeted multiobjective Dijkstra algorithm](https://onlinelibrary.wiley.com/doi/abs/10.1002/net.22174?utm_source=chatgpt.com) — Networks 2023 | `bench-tmda-baseline` |
| `C3-P03` | `unread` |  | [Bi-Objective Search with Bi-Directional A*](https://ojs.aaai.org/index.php/SOCS/article/view/18563?utm_source=chatgpt.com) — SoCS 2021 / ESA 2021 | `investigate-bidirectional-bos` |
| `C3-P04` | `unread` | `nearby` | [Cost-Optimal FOND Planning as Bi-Objective Best-First Search](https://ojs.aaai.org/index.php/ICAPS/article/view/36110?utm_source=chatgpt.com) — ICAPS 2025 | `park-planning-nearby` |
| `C3-P05` | `unread` | `nearby` | [A Fast Heuristic Search Approach for Energy-Optimal Profile Routing for Electric Vehicles](https://ojs.aaai.org/index.php/AAAI/article/view/41005?utm_source=chatgpt.com) — AAAI 2026 | `park-ev-routing-nearby` |

## 4) Heuristic / Dominance-Check / Engineering Optimization

| ID | Status | Priority | Paper | Repo hook |
| --- | --- | --- | --- | --- |
| `C4-P01` | `unread` |  | [Towards Effective Multi-Valued Heuristics for Bi-objective Shortest-Path Algorithms via Differential Heuristics](https://ojs.aaai.org/index.php/SOCS/article/view/27288?utm_source=chatgpt.com) — SoCS 2023 | `investigate-differential-heuristics` |
| `C4-P02` | `unread` |  | [Speeding Up Dominance Checks in Multi-Objective Search: New Techniques and Data Structures](https://ojs.aaai.org/index.php/SOCS/article/view/31564?utm_source=chatgpt.com) — SoCS 2024 | `impl-dominance-data-structures` |
| `C4-P03` | `unread` |  | [Efficient Set Dominance Checks in Multi-Objective Shortest-Path Algorithms via Vectorized Operations](https://ojs.aaai.org/index.php/SOCS/article/view/31560?utm_source=chatgpt.com) — SoCS 2024 | `investigate-vectorized-dominance` |

## 5) Approximate / Anytime / Subset Approximation

| ID | Status | Priority | Paper | Repo hook |
| --- | --- | --- | --- | --- |
| `C5-P01` | `unread` | `★` | [A*pex: Efficient Approximate Multi-Objective Search on Graphs](https://ojs.aaai.org/index.php/ICAPS/article/view/19825?utm_source=chatgpt.com) — ICAPS 2022 | `investigate-approximate-mos` |
| `C5-P02` | `unread` |  | [Anytime Approximate Bi-Objective Search](https://ojs.aaai.org/index.php/SOCS/article/view/21768?utm_source=chatgpt.com) — SoCS 2022 | `investigate-anytime-bos` |
| `C5-P03` | `unread` |  | [Subset Approximation of Pareto Regions with Bi-objective A*](https://ojs.aaai.org/index.php/AAAI/article/view/21276?utm_source=chatgpt.com) — AAAI 2022 | `investigate-subset-approximation` |
| `C5-P04` | `unread` |  | [Finding a Small, Diverse Subset of the Pareto Solution Set in Bi-Objective Search](https://ojs.aaai.org/index.php/SOCS/article/view/31568?utm_source=chatgpt.com) — SoCS 2024 | `investigate-diverse-pareto-subset` |
| `C5-P05` | `unread` |  | [A Preprocessing Framework for Efficient Approximate Bi-Objective Shortest-Path Computation in the Presence of Correlated Objectives](https://ojs.aaai.org/index.php/SOCS/article/view/35977?utm_source=chatgpt.com) — SoCS 2025 | `investigate-correlated-objective-preprocessing` |
| `C5-P06` | `unread` |  | [Bi-Objective Search for the Traveling Salesman Problem with Time Windows and Vacant Penalties](https://ojs.aaai.org/index.php/SOCS/article/view/35989?utm_source=chatgpt.com) — SoCS 2025 | `park-tsp-transfer` |

## 6) Replanning / Dynamic Obstacles / Multi-Agent Extensions

| ID | Status | Priority | Paper | Repo hook |
| --- | --- | --- | --- | --- |
| `C6-P01` | `unread` |  | [Multi-Objective Path-Based D* Lite](https://wonderren.github.io/files/ren22_mopbd-RAL_ICRA22.pdf?utm_source=chatgpt.com) — RA-L / ICRA 2022 | `park-replanning-extension` |
| `C6-P02` | `unread` |  | [Multi-Objective Safe-Interval Path Planning With Dynamic Obstacles](https://wonderren.github.io/files/ren22_mosipp_RAL_IROS22.pdf?utm_source=chatgpt.com) — RA-L 2022 | `park-dynamic-obstacles-extension` |
| `C6-P03` | `unread` |  | [Multi-objective Conflict-Based Search for Multi-agent Path Finding](https://wonderren.github.io/files/ren21_mocbs_icra21.pdf?utm_source=chatgpt.com) — ICRA 2021 | `park-mo-mapf-extension` |
| `C6-P04` | `unread` |  | [Binary Branching Multi-Objective Conflict-Based Search for Multi-Agent Path Finding](https://ojs.aaai.org/index.php/ICAPS/article/view/27214?utm_source=chatgpt.com) — ICAPS 2023 | `park-mo-mapf-extension` |
| `C6-P05` | `unread` |  | [Efficient Approximate Search for Multi-Objective Multi-Agent Path Finding](https://ojs.aaai.org/index.php/ICAPS/article/view/31524?utm_source=chatgpt.com) — ICAPS 2024 | `park-approximate-mo-mapf` |
| `C6-P06` | `unread` | `nearby` | [Finding All Optimal Solutions in Multi-Agent Path Finding](https://ojs.aaai.org/index.php/SOCS/article/view/35972?utm_source=chatgpt.com) — SoCS 2025 | `park-mapf-nearby` |
| `C6-P07` | `unread` | `nearby` | [Minimizing Fuel in Multi-Agent Pathfinding](https://ojs.aaai.org/index.php/SOCS/article/view/35979?utm_source=chatgpt.com) — SoCS 2025 | `park-mapf-nearby` |
| `C6-P08` | `unread` | `nearby` | [Inapproximability of Optimal Multi-Agent Pathfinding Problems](https://ojs.aaai.org/index.php/AAAI/article/view/34873?utm_source=chatgpt.com) — AAAI 2025 | `park-mapf-hardness` |

## 7) Older Background Papers

| ID | Status | Priority | Paper | Repo hook |
| --- | --- | --- | --- | --- |
| `C7-P01` | `unread` |  | [A comparison of heuristic best-first algorithms for bicriterion shortest path problems](https://www.sciencedirect.com/science/article/pii/S0377221711008010?utm_source=chatgpt.com) — EJOR 2012 | `background-bos-history` |
| `C7-P02` | `unread` |  | [Multiobjective shortest path problems with lexicographic goal-based preferences](https://www.sciencedirect.com/science/article/pii/S0377221714004159?utm_source=chatgpt.com) — EJOR 2014 | `background-preference-mosp` |
| `C7-P03` | `unread` |  | [Dimensionality reduction in multiobjective shortest path search](https://www.sciencedirect.com/science/article/pii/S0305054815001240?utm_source=chatgpt.com) — Computers & Operations Research 2015 | `background-dimensionality-reduction` |
| `C7-P04` | `unread` |  | [Analysis of FPTASes for the multi-objective shortest path problem](https://www.sciencedirect.com/science/article/pii/S030505481630154X?utm_source=chatgpt.com) — Computers & Operations Research 2017 | `background-approximation-guarantees` |

## 8) If Only 12 Papers Are Read First

| Rank | Paper ID | Paper | Why first |
| --- | --- | --- | --- |
| `1` | `C1-P01` | A Simple and Fast Bi-Objective Search Algorithm | BOS origin point |
| `2` | `C1-P02` | Simple and efficient bi-objective search algorithms via fast dominance checks | strongest BOA* / dominance follow-up |
| `3` | `C1-P03` | Enhanced Multi-Objective A* Using Balanced Binary Search Trees | first exact multi-objective extension to prioritize |
| `4` | `C1-P04` | Multi-objective Search via Lazy and Efficient Dominance Checks | closest exact-core paper to current LTMOA-style work |
| `5` | `C1-P05` | Exact Multi-objective Path Finding with Negative Weights | broadens exact core beyond nonnegative assumptions |
| `6` | `C1-P07` | Parallelizing Multi-objective A* Search | direct bridge to `LTMOA_parallel` |
| `7` | `C2-P01` | Heuristic-Search Approaches for the Multi-Objective Shortest-Path Problem | field survey for related work and gap finding |
| `8` | `C2-P02` | Theoretical Study on Multi-objective Heuristic Search | theory guardrail for claims |
| `9` | `C3-P01` | An Improved Multiobjective Shortest Path Algorithm | closest OR-side exact baseline |
| `10` | `C3-P02` | Targeted multiobjective Dijkstra algorithm | one-to-one exact baseline comparison |
| `11` | `C5-P01` | A*pex: Efficient Approximate Multi-Objective Search on Graphs | main approximate branch if scope expands |
| `12` | `C1-P08` | Deeper Treatment of the Bi-objective Search Framework | latest BOS framework deepening |

Suggested reading order for actual repo work:

1. Read `C1-P01` through `C1-P05` first before changing exact solver logic.
2. Read `C2-P01` and `C2-P02` before writing any strong novelty or scope claim.
3. Read `C3-P01` and `C3-P02` before claiming competitiveness outside AI-search venues.
4. Read `C4-P01` through `C4-P03` before another dominance or heuristic rewrite.
5. Read `C5-*` only when the repo intentionally moves from exact skyline search to bounded-quality or anytime approximation.
6. Read `C6-*` only when the repo scope expands beyond single-agent static-road MOSP.

Tracking note:

- The tables above are the canonical per-paper status tracker.
- When a paper gets a repo note, benchmark, or code change, add the paper ID to
  the corresponding note title or memory-log entry so the mapping stays usable.
