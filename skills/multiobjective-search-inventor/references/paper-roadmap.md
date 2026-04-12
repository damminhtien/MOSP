# Seed roadmap for MOSP / MOA* / BOS

This roadmap is distilled from the user's uploaded checklist. Use it as a lineage map and reading roadmap, not as a claim that the bibliography is exhaustive.

## 1. Exact core MOSP / MOA* / BOS

- **A Simple and Fast Bi-Objective Search Algorithm** — ICAPS 2020. The BOA* baseline for simple and fast bi-objective search.
- **Simple and efficient bi-objective search algorithms via fast dominance checks** — Artificial Intelligence 2023. Journal refinement of the BOA* / BOD line.
- **Enhanced Multi-Objective A* Using Balanced Binary Search Trees** — SoCS 2022. EMOA*; balanced binary search trees for many-objective dominance checks.
- **Multi-objective Search via Lazy and Efficient Dominance Checks** — IJCAI 2023. LTMOA* and LazyLTMOA*.
- **Exact Multi-objective Path Finding with Negative Weights** — ICAPS 2024. NWMOA* for negative-weight settings.
- **EMOA*: A framework for search-based multi-objective path planning** — Artificial Intelligence 2025. A unifying EMOA* perspective.
- **Parallelizing Multi-objective A* Search** — ICAPS 2025. Parallel MOA*.
- **Deeper Treatment of the Bi-objective Search Framework** — AAAI 2026. Deeper BOS framing.

## 2. Surveys and theory

- **Heuristic-Search Approaches for the Multi-Objective Shortest-Path Problem: Progress and Research Opportunities** — IJCAI 2023. A highly relevant survey for this field.
- **Theoretical Study on Multi-objective Heuristic Search** — IJCAI 2024. Theoretical framing for multi-objective heuristic search.
- **Multi-Objective Search: Algorithms, Applications, and Emerging Directions** — AAAI 2026. A broader survey viewing MOS as a general framework.

## 3. OR-side exact shortest-path line

- **An Improved Multiobjective Shortest Path Algorithm** — Computers & Operations Research 2021. MDA.
- **Targeted multiobjective Dijkstra algorithm** — Networks 2023. T-MDA; especially relevant for one-to-one exact MOSP.
- **Bi-Objective Search with Bi-Directional A*** — SoCS 2021 / ESA 2021. Bidirectional BOS.
- **Cost-Optimal FOND Planning as Bi-Objective Best-First Search** — ICAPS 2025. Nearby transfer into planning rather than pure MOSP.
- **A Fast Heuristic Search Approach for Energy-Optimal Profile Routing for Electric Vehicles** — AAAI 2026. Nearby routing application.

## 4. Heuristic and dominance engineering

- **Towards Effective Multi-Valued Heuristics for Bi-objective Shortest-Path Algorithms via Differential Heuristics** — SoCS 2023. Heuristic design for BOS.
- **Speeding Up Dominance Checks in Multi-Objective Search: New Techniques and Data Structures** — SoCS 2024. Dominance-check bottleneck work.
- **Efficient Set Dominance Checks in Multi-Objective Shortest-Path Algorithms via Vectorized Operations** — SoCS 2024. Vectorized dominance checks.

## 5. Approximate, anytime, and subset methods

- **A*pex: Efficient Approximate Multi-Objective Search on Graphs** — ICAPS 2022. Approximate MOS.
- **Anytime Approximate Bi-Objective Search** — SoCS 2022. A-BOA*.
- **Subset Approximation of Pareto Regions with Bi-objective A*** — AAAI 2022. Representative subset extraction.
- **Finding a Small, Diverse Subset of the Pareto Solution Set in Bi-Objective Search** — SoCS 2024. Diversity-aware subset selection.
- **A Preprocessing Framework for Efficient Approximate Bi-Objective Shortest-Path Computation in the Presence of Correlated Objectives** — SoCS 2025. Approximate BOSP with correlated objectives.
- **Bi-Objective Search for the Traveling Salesman Problem with Time Windows and Vacant Penalties** — SoCS 2025. Transfer into TSP-TW-VP.

## 6. Dynamic, replanning, and multi-agent extensions

- **Multi-Objective Path-Based D* Lite** — RA-L / ICRA 2022. Incremental replanning.
- **Multi-Objective Safe-Interval Path Planning With Dynamic Obstacles** — RA-L 2022. Dynamic obstacles.
- **Multi-objective Conflict-Based Search for Multi-agent Path Finding** — ICRA 2021. MO-CBS.
- **Binary Branching Multi-Objective Conflict-Based Search for Multi-Agent Path Finding** — ICAPS 2023. Updated MO-CBS line.
- **Efficient Approximate Search for Multi-Objective Multi-Agent Path Finding** — ICAPS 2024. Approximate MO-MAPF.
- **Finding All Optimal Solutions in Multi-Agent Path Finding** — SoCS 2025. Nearby enumeration result.
- **Minimizing Fuel in Multi-Agent Pathfinding** — SoCS 2025. Nearby objective extension.
- **Inapproximability of Optimal Multi-Agent Pathfinding Problems** — AAAI 2025. Nearby hardness result.

## 7. Older background papers

- **A comparison of heuristic best-first algorithms for bicriterion shortest path problems** — EJOR 2012. Pre-BOA* background.
- **Multiobjective shortest path problems with lexicographic goal-based preferences** — EJOR 2014. Preference-based MOSP.
- **Dimensionality reduction in multiobjective shortest path search** — Computers & Operations Research 2015. Dimensionality reduction.
- **Analysis of FPTASes for the multi-objective shortest path problem** — Computers & Operations Research 2017. Approximation background.

## 8. Twelve-paper reading sequence

Use this order by default when the user wants a focused reading plan:

1. A Simple and Fast Bi-Objective Search Algorithm
2. Simple and efficient bi-objective search algorithms via fast dominance checks
3. Enhanced Multi-Objective A* Using Balanced Binary Search Trees
4. Multi-objective Search via Lazy and Efficient Dominance Checks
5. Exact Multi-objective Path Finding with Negative Weights
6. Parallelizing Multi-objective A* Search
7. Heuristic-Search Approaches for the Multi-Objective Shortest-Path Problem
8. Theoretical Study on Multi-objective Heuristic Search
9. An Improved Multiobjective Shortest Path Algorithm
10. Targeted multiobjective Dijkstra algorithm
11. A*pex: Efficient Approximate Multi-Objective Search on Graphs
12. Deeper Treatment of the Bi-objective Search Framework
