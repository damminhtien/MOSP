# MOSP / MOA* Paper Reading Roadmap

Date: 2026-04-12

Purpose: keep a research-ordered reading checklist for exact MOSP / MOA*
work, nearby shortest-path literature, and approximate / dynamic extensions so
future implementation passes can be grounded in the right paper cluster first.

Conventions:

- `★`: read first
- `seed`: the 4 papers that were already selected as the initial seed set
- `nearby`: closely related, but no longer core exact MOA* / exact MOSP

## 1) Core Exact MOSP / MOA* / BOS

- [ ] **★ A Simple and Fast Bi-Objective Search Algorithm** — ICAPS 2020 — BOA* origin point.
  Link: <https://ojs.aaai.org/index.php/ICAPS/article/view/6655?utm_source=chatgpt.com>
- [ ] **★ Simple and efficient bi-objective search algorithms via fast dominance checks** — Artificial Intelligence 2023 — journal BOA* / BOD follow-up.
  Link: <https://www.sciencedirect.com/science/article/pii/S0004370222001473?utm_source=chatgpt.com>
- [ ] **★ Enhanced Multi-Objective A* Using Balanced Binary Search Trees** — SoCS 2022 — EMOA*, extends BOA* to more objectives. `seed`
  Link: <https://ojs.aaai.org/index.php/SOCS/article/view/21764?utm_source=chatgpt.com>
- [ ] **★ Multi-objective Search via Lazy and Efficient Dominance Checks** — IJCAI 2023 — LTMOA*, LazyLTMOA*. `seed`
  Link: <https://www.ijcai.org/proceedings/2023/0850.pdf?utm_source=chatgpt.com>
- [ ] **★ Exact Multi-objective Path Finding with Negative Weights** — ICAPS 2024 — NWMOA*. `seed`
  Link: <https://ojs.aaai.org/index.php/ICAPS/article/view/31455?utm_source=chatgpt.com>
- [ ] **EMOA*: A framework for search-based multi-objective path planning** — Artificial Intelligence 2025 — unified EMOA* framework.
  Link: <https://www.sciencedirect.com/science/article/abs/pii/S0004370224001966?utm_source=chatgpt.com>
- [ ] **Parallelizing Multi-objective A* Search** — ICAPS 2025 — parallel MOA*.
  Link: <https://ojs.aaai.org/index.php/ICAPS/article/view/36109?utm_source=chatgpt.com>
- [ ] **Deeper Treatment of the Bi-objective Search Framework** — AAAI 2026 — deeper BOS treatment.
  Link: <https://ojs.aaai.org/index.php/AAAI/article/view/41047?utm_source=chatgpt.com>

## 2) Survey / Theory

- [ ] **★ Heuristic-Search Approaches for the Multi-Objective Shortest-Path Problem: Progress and Research Opportunities** — IJCAI 2023 — field survey tuned to MOSP.
  Link: <https://www.ijcai.org/proceedings/2023/0757.pdf?utm_source=chatgpt.com>
- [ ] **★ Theoretical Study on Multi-objective Heuristic Search** — IJCAI 2024 — heuristic-search theory frame.
  Link: <https://www.ijcai.org/proceedings/2024/0776.pdf?utm_source=chatgpt.com>
- [ ] **Multi-Objective Search: Algorithms, Applications, and Emerging Directions** — AAAI 2026 — broader survey framing MOS as a general search framework.
  Link: <https://ojs.aaai.org/index.php/AAAI/article/view/42134/46588?utm_source=chatgpt.com>

## 3) Very Close but More OR / Exact Shortest-Path Oriented

- [ ] **★ An Improved Multiobjective Shortest Path Algorithm** — Computers & Operations Research 2021 — MDA.
  Link: <https://www.sciencedirect.com/science/article/abs/pii/S0305054821001842?utm_source=chatgpt.com>
- [ ] **★ Targeted multiobjective Dijkstra algorithm** — Networks 2023 — T-MDA, one-to-one MOSP and very close to this line.
  Link: <https://onlinelibrary.wiley.com/doi/abs/10.1002/net.22174?utm_source=chatgpt.com>
- [ ] **Bi-Objective Search with Bi-Directional A*** — SoCS 2021 / ESA 2021 — bidirectional BOS speedup direction.
  Link: <https://ojs.aaai.org/index.php/SOCS/article/view/18563?utm_source=chatgpt.com>
- [ ] **Cost-Optimal FOND Planning as Bi-Objective Best-First Search** — ICAPS 2025 — `nearby`, BOS in planning.
  Link: <https://ojs.aaai.org/index.php/ICAPS/article/view/36110?utm_source=chatgpt.com>
- [ ] **A Fast Heuristic Search Approach for Energy-Optimal Profile Routing for Electric Vehicles** — AAAI 2026 — `nearby`, EV / energy routing.
  Link: <https://ojs.aaai.org/index.php/AAAI/article/view/41005?utm_source=chatgpt.com>

## 4) Heuristic / Dominance-Check / Engineering Optimization

- [ ] **Towards Effective Multi-Valued Heuristics for Bi-objective Shortest-Path Algorithms via Differential Heuristics** — SoCS 2023 — heuristic design for BOS.
  Link: <https://ojs.aaai.org/index.php/SOCS/article/view/27288?utm_source=chatgpt.com>
- [ ] **Speeding Up Dominance Checks in Multi-Objective Search: New Techniques and Data Structures** — SoCS 2024 — attacks the dominance-check bottleneck.
  Link: <https://ojs.aaai.org/index.php/SOCS/article/view/31564?utm_source=chatgpt.com>
- [ ] **Efficient Set Dominance Checks in Multi-Objective Shortest-Path Algorithms via Vectorized Operations** — SoCS 2024 — vectorized dominance checks.
  Link: <https://ojs.aaai.org/index.php/SOCS/article/view/31560?utm_source=chatgpt.com>

## 5) Approximate / Anytime / Subset Approximation

- [ ] **★ A*pex: Efficient Approximate Multi-Objective Search on Graphs** — ICAPS 2022 — core approximate MOS paper.
  Link: <https://ojs.aaai.org/index.php/ICAPS/article/view/19825?utm_source=chatgpt.com>
- [ ] **Anytime Approximate Bi-Objective Search** — SoCS 2022 — A-BOA*.
  Link: <https://ojs.aaai.org/index.php/SOCS/article/view/21768?utm_source=chatgpt.com>
- [ ] **Subset Approximation of Pareto Regions with Bi-objective A*** — AAAI 2022 — representative subset of the frontier.
  Link: <https://ojs.aaai.org/index.php/AAAI/article/view/21276?utm_source=chatgpt.com>
- [ ] **Finding a Small, Diverse Subset of the Pareto Solution Set in Bi-Objective Search** — SoCS 2024 — extended abstract emphasizing diversity.
  Link: <https://ojs.aaai.org/index.php/SOCS/article/view/31568?utm_source=chatgpt.com>
- [ ] **A Preprocessing Framework for Efficient Approximate Bi-Objective Shortest-Path Computation in the Presence of Correlated Objectives** — SoCS 2025 — newer approximate BOSP direction.
  Link: <https://ojs.aaai.org/index.php/SOCS/article/view/35977?utm_source=chatgpt.com>
- [ ] **Bi-Objective Search for the Traveling Salesman Problem with Time Windows and Vacant Penalties** — SoCS 2025 — BOS transferred to TSP-TW-VP.
  Link: <https://ojs.aaai.org/index.php/SOCS/article/view/35989?utm_source=chatgpt.com>

## 6) Replanning / Dynamic Obstacles / Multi-Agent Extensions

- [ ] **Multi-Objective Path-Based D* Lite** — RA-L / ICRA 2022 — incremental replanning.
  Link: <https://wonderren.github.io/files/ren22_mopbd-RAL_ICRA22.pdf?utm_source=chatgpt.com>
- [ ] **Multi-Objective Safe-Interval Path Planning With Dynamic Obstacles** — RA-L 2022 — dynamic obstacles.
  Link: <https://wonderren.github.io/files/ren22_mosipp_RAL_IROS22.pdf?utm_source=chatgpt.com>
- [ ] **Multi-objective Conflict-Based Search for Multi-agent Path Finding** — ICRA 2021 — MO-CBS.
  Link: <https://wonderren.github.io/files/ren21_mocbs_icra21.pdf?utm_source=chatgpt.com>
- [ ] **Binary Branching Multi-Objective Conflict-Based Search for Multi-Agent Path Finding** — ICAPS 2023 — newer MO-CBS branch.
  Link: <https://ojs.aaai.org/index.php/ICAPS/article/view/27214?utm_source=chatgpt.com>
- [ ] **Efficient Approximate Search for Multi-Objective Multi-Agent Path Finding** — ICAPS 2024 — approximate MO-MAPF.
  Link: <https://ojs.aaai.org/index.php/ICAPS/article/view/31524?utm_source=chatgpt.com>
- [ ] **Finding All Optimal Solutions in Multi-Agent Path Finding** — SoCS 2025 — `nearby`, enumerating all optimal solutions in MAPF.
  Link: <https://ojs.aaai.org/index.php/SOCS/article/view/35972?utm_source=chatgpt.com>
- [ ] **Minimizing Fuel in Multi-Agent Pathfinding** — SoCS 2025 — `nearby`, new objective in MAPF.
  Link: <https://ojs.aaai.org/index.php/SOCS/article/view/35979?utm_source=chatgpt.com>
- [ ] **Inapproximability of Optimal Multi-Agent Pathfinding Problems** — AAAI 2025 — `nearby`, hardness results.
  Link: <https://ojs.aaai.org/index.php/AAAI/article/view/34873?utm_source=chatgpt.com>

## 7) Older Background Papers

- [ ] **A comparison of heuristic best-first algorithms for bicriterion shortest path problems** — EJOR 2012 — pre-BOA* BOS background.
  Link: <https://www.sciencedirect.com/science/article/pii/S0377221711008010?utm_source=chatgpt.com>
- [ ] **Multiobjective shortest path problems with lexicographic goal-based preferences** — EJOR 2014 — preference-based MOSP.
  Link: <https://www.sciencedirect.com/science/article/pii/S0377221714004159?utm_source=chatgpt.com>
- [ ] **Dimensionality reduction in multiobjective shortest path search** — Computers & Operations Research 2015 — dimensionality reduction in MOSP.
  Link: <https://www.sciencedirect.com/science/article/pii/S0305054815001240?utm_source=chatgpt.com>
- [ ] **Analysis of FPTASes for the multi-objective shortest path problem** — Computers & Operations Research 2017 — approximation with guarantees.
  Link: <https://www.sciencedirect.com/science/article/pii/S030505481630154X?utm_source=chatgpt.com>

## 8) If Only 12 Papers Are Read First

- [ ] A Simple and Fast Bi-Objective Search Algorithm
- [ ] Simple and efficient bi-objective search algorithms via fast dominance checks
- [ ] Enhanced Multi-Objective A* Using Balanced Binary Search Trees
- [ ] Multi-objective Search via Lazy and Efficient Dominance Checks
- [ ] Exact Multi-objective Path Finding with Negative Weights
- [ ] Parallelizing Multi-objective A* Search
- [ ] Heuristic-Search Approaches for the Multi-Objective Shortest-Path Problem
- [ ] Theoretical Study on Multi-objective Heuristic Search
- [ ] An Improved Multiobjective Shortest Path Algorithm
- [ ] Targeted multiobjective Dijkstra algorithm
- [ ] A*pex: Efficient Approximate Multi-Objective Search on Graphs
- [ ] Deeper Treatment of the Bi-objective Search Framework

Suggested reading order for actual repo work:

1. Read the 5 core exact papers first: BOA*, BOA* journal, EMOA*, LTMOA*, NWMOA*.
2. Read the IJCAI 2023 survey and IJCAI 2024 theory paper before writing any strong related-work or novelty claim.
3. Read MDA and T-MDA before making claims about exact one-to-one MOSP competitiveness outside AI-search venues.
4. Read the dominance / heuristic optimization cluster before changing data structures or pruning logic again.
5. Read the approximate / anytime cluster only when moving from exact skyline search toward bounded-quality or anytime subset goals.
6. Read dynamic / MAPF extensions only when the repo scope expands beyond single-agent static-road MOSP.
