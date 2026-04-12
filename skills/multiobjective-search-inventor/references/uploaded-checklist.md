Tôi gom lại thành một **checklist đọc paper** theo đúng mạch nghiên cứu.
Quy ước: **★ = nên đọc trước**, **seed = 4 paper bạn đưa ban đầu**, **nearby = gần họ nhưng không còn là lõi MOA*/MOSP exact**.

## 1) Lõi exact MOSP / MOA* / BOS — đọc trước

* [ ] **★ A Simple and Fast Bi-Objective Search Algorithm** — ICAPS 2020 — mốc BOA* gốc. ([AAAI Open Research Library][1])
* [ ] **★ Simple and efficient bi-objective search algorithms via fast dominance checks** — Artificial Intelligence 2023 — bản journal của nhánh BOA*/BOD. ([ScienceDirect][2])
* [ ] **★ Enhanced Multi-Objective A* Using Balanced Binary Search Trees** — SoCS 2022 — **EMOA***, mở BOA* sang nhiều mục tiêu. *(seed)* ([AAAI Open Research Library][3])
* [ ] **★ Multi-objective Search via Lazy and Efficient Dominance Checks** — IJCAI 2023 — **LTMOA***, **LazyLTMOA***. *(seed)* ([IJCAI][4])
* [ ] **★ Exact Multi-objective Path Finding with Negative Weights** — ICAPS 2024 — **NWMOA***. *(seed)* ([AAAI Open Research Library][5])
* [ ] **EMOA*: A framework for search-based multi-objective path planning** — Artificial Intelligence 2025 — khung hợp nhất/hệ thống hóa nhánh EMOA*. ([ScienceDirect][6])
* [ ] **Parallelizing Multi-objective A* Search** — ICAPS 2025 — song song hóa MOA*. ([AAAI Open Research Library][7])
* [ ] **Deeper Treatment of the Bi-objective Search Framework** — AAAI 2026 — đào sâu framework BOS. ([AAAI Open Research Library][8])

## 2) Survey / theory — để viết related work và framing

* [ ] **★ Heuristic-Search Approaches for the Multi-Objective Shortest-Path Problem: Progress and Research Opportunities** — IJCAI 2023 — survey rất trúng field. ([IJCAI][9])
* [ ] **★ Theoretical Study on Multi-objective Heuristic Search** — IJCAI 2024 — khung lý thuyết MOS heuristic search. ([IJCAI][10])
* [ ] **Multi-Objective Search: Algorithms, Applications, and Emerging Directions** — AAAI 2026 — survey rộng hơn, nhìn MOS như framework chung. ([AAAI Open Research Library][11])

## 3) Nhánh rất gần nhưng nghiêng OR / shortest-path exact hơn AI-search

* [ ] **★ An Improved Multiobjective Shortest Path Algorithm** — Computers & Operations Research 2021 — **MDA**. ([ScienceDirect][12])
* [ ] **★ Targeted multiobjective Dijkstra algorithm** — Networks 2023 — **T-MDA**, one-to-one MOSP rất gần dòng này. ([Thư viện trực tuyến Wiley][13])
* [ ] **Bi-Objective Search with Bi-Directional A*** — SoCS 2021 / ESA 2021 — tăng tốc BOS theo hướng bidirectional. ([AAAI Open Research Library][14])
* [ ] **Cost-Optimal FOND Planning as Bi-Objective Best-First Search** — ICAPS 2025 — nearby; không còn là MOSP thuần nhưng dùng BOS trong planning. ([AAAI Open Research Library][15])
* [ ] **A Fast Heuristic Search Approach for Energy-Optimal Profile Routing for Electric Vehicles** — AAAI 2026 — nearby; heuristic search trên routing năng lượng/EV. ([AAAI Open Research Library][16])

## 4) Heuristic / dominance-check / engineering optimization

* [ ] **Towards Effective Multi-Valued Heuristics for Bi-objective Shortest-Path Algorithms via Differential Heuristics** — SoCS 2023 — nhánh heuristic design cho BOS. ([AAAI Open Research Library][17])
* [ ] **Speeding Up Dominance Checks in Multi-Objective Search: New Techniques and Data Structures** — SoCS 2024 — tiếp tục đập vào bottleneck dominance checks. ([AAAI Open Research Library][18])
* [ ] **Efficient Set Dominance Checks in Multi-Objective Shortest-Path Algorithms via Vectorized Operations** — SoCS 2024 — dominance checks bằng vectorized operations. ([AAAI Open Research Library][19])

## 5) Approximate / anytime / subset approximation

* [ ] **★ A*pex: Efficient Approximate Multi-Objective Search on Graphs** — ICAPS 2022 — approximate MOS rất quan trọng. ([AAAI Open Research Library][20])
* [ ] **Anytime Approximate Bi-Objective Search** — SoCS 2022 — **A-BOA***. ([AAAI Open Research Library][21])
* [ ] **Subset Approximation of Pareto Regions with Bi-objective A*** — AAAI 2022 — lấy tập con đại diện của frontier. ([AAAI Open Research Library][22])
* [ ] **Finding a Small, Diverse Subset of the Pareto Solution Set in Bi-Objective Search** — SoCS 2024 — extended abstract, nhấn mạnh diversity. ([AAAI Open Research Library][23])
* [ ] **A Preprocessing Framework for Efficient Approximate Bi-Objective Shortest-Path Computation in the Presence of Correlated Objectives** — SoCS 2025 — nhánh approximate BOSP mới hơn. ([AAAI Open Research Library][24])
* [ ] **Bi-Objective Search for the Traveling Salesman Problem with Time Windows and Vacant Penalties** — SoCS 2025 — BOS được chuyển sang TSP-TW-VP. ([AAAI Open Research Library][25])

## 6) Mở rộng sang replanning / dynamic obstacles / multi-agent

* [ ] **Multi-Objective Path-Based D* Lite** — RA-L / ICRA 2022 — incremental replanning đa mục tiêu. ([Zhongqiang Richard Ren][26])
* [ ] **Multi-Objective Safe-Interval Path Planning With Dynamic Obstacles** — RA-L 2022 — dynamic obstacles. ([Zhongqiang Richard Ren][27])
* [ ] **Multi-objective Conflict-Based Search for Multi-agent Path Finding** — ICRA 2021 — **MO-CBS**. ([Zhongqiang Richard Ren][28])
* [ ] **Binary Branching Multi-Objective Conflict-Based Search for Multi-Agent Path Finding** — ICAPS 2023 — nhánh MO-CBS mới hơn. ([AAAI Open Research Library][29])
* [ ] **Efficient Approximate Search for Multi-Objective Multi-Agent Path Finding** — ICAPS 2024 — approximate MO-MAPF. ([AAAI Open Research Library][30])
* [ ] **Finding All Optimal Solutions in Multi-Agent Path Finding** — SoCS 2025 — nearby; liệt kê toàn bộ nghiệm tối ưu trong MAPF. ([AAAI Open Research Library][31])
* [ ] **Minimizing Fuel in Multi-Agent Pathfinding** — SoCS 2025 — nearby; một objective mới trong MAPF. ([AAAI Open Research Library][32])
* [ ] **Inapproximability of Optimal Multi-Agent Pathfinding Problems** — AAAI 2025 — nearby; kết quả hardness cho MAPF tối ưu. ([AAAI Open Research Library][33])

## 7) Nền tảng cũ hơn mà tôi đã nhắc thoáng qua — đọc khi cần background sâu

* [ ] **A comparison of heuristic best-first algorithms for bicriterion shortest path problems** — EJOR 2012 — nền BOS đời trước BOA*. ([ScienceDirect][34])
* [ ] **Multiobjective shortest path problems with lexicographic goal-based preferences** — EJOR 2014 — preference-based MOSP. ([ScienceDirect][35])
* [ ] **Dimensionality reduction in multiobjective shortest path search** — Computers & Operations Research 2015 — giảm chiều trong MOSP. ([ScienceDirect][36])
* [ ] **Analysis of FPTASes for the multi-objective shortest path problem** — Computers & Operations Research 2017 — nhánh approximation có bảo đảm. ([ScienceDirect][37])

## 8) Nếu chỉ chọn 12 bài để đọc trước

Theo thứ tự này là hợp lý nhất:

* [ ] A Simple and Fast Bi-Objective Search Algorithm. ([AAAI Open Research Library][1])
* [ ] Simple and efficient bi-objective search algorithms via fast dominance checks. ([ScienceDirect][2])
* [ ] Enhanced Multi-Objective A* Using Balanced Binary Search Trees. ([AAAI Open Research Library][3])
* [ ] Multi-objective Search via Lazy and Efficient Dominance Checks. ([IJCAI][4])
* [ ] Exact Multi-objective Path Finding with Negative Weights. ([AAAI Open Research Library][5])
* [ ] Parallelizing Multi-objective A* Search. ([AAAI Open Research Library][7])
* [ ] Heuristic-Search Approaches for the Multi-Objective Shortest-Path Problem. ([IJCAI][9])
* [ ] Theoretical Study on Multi-objective Heuristic Search. ([IJCAI][10])
* [ ] An Improved Multiobjective Shortest Path Algorithm. ([ScienceDirect][12])
* [ ] Targeted multiobjective Dijkstra algorithm. ([Thư viện trực tuyến Wiley][13])
* [ ] A*pex: Efficient Approximate Multi-Objective Search on Graphs. ([AAAI Open Research Library][20])
* [ ] Deeper Treatment of the Bi-objective Search Framework. ([AAAI Open Research Library][8])

Nếu bạn muốn, tôi sẽ biến checklist này thành **1 bảng reading roadmap theo 3 mức: must-read / should-read / optional**, hoặc thành **BibTeX/IEEE references sạch để chèn vào paper**.

[1]: https://ojs.aaai.org/index.php/ICAPS/article/view/6655?utm_source=chatgpt.com "A Simple and Fast Bi-Objective Search Algorithm"
[2]: https://www.sciencedirect.com/science/article/pii/S0004370222001473?utm_source=chatgpt.com "Simple and efficient bi-objective search algorithms via fast ..."
[3]: https://ojs.aaai.org/index.php/SOCS/article/view/21764?utm_source=chatgpt.com "Enhanced Multi-Objective A* Using Balanced Binary ..."
[4]: https://www.ijcai.org/proceedings/2023/0850.pdf?utm_source=chatgpt.com "Multi-objective Search via Lazy and Efficient Dominance ..."
[5]: https://ojs.aaai.org/index.php/ICAPS/article/view/31455?utm_source=chatgpt.com "Exact Multi-objective Path Finding with Negative Weights"
[6]: https://www.sciencedirect.com/science/article/abs/pii/S0004370224001966?utm_source=chatgpt.com "EMOA*: A framework for search-based multi-objective path ..."
[7]: https://ojs.aaai.org/index.php/ICAPS/article/view/36109?utm_source=chatgpt.com "Parallelizing Multi-objective A* Search"
[8]: https://ojs.aaai.org/index.php/AAAI/article/view/41047?utm_source=chatgpt.com "Deeper Treatment of the Bi-objective Search Framework"
[9]: https://www.ijcai.org/proceedings/2023/0757.pdf?utm_source=chatgpt.com "Heuristic-Search Approaches for the Multi-Objective ..."
[10]: https://www.ijcai.org/proceedings/2024/0776.pdf?utm_source=chatgpt.com "Theoretical Study on Multi-objective Heuristic Search"
[11]: https://ojs.aaai.org/index.php/AAAI/article/view/42134/46588?utm_source=chatgpt.com "Algorithms, Applications, and Emerging Directions"
[12]: https://www.sciencedirect.com/science/article/abs/pii/S0305054821001842?utm_source=chatgpt.com "An Improved Multiobjective Shortest Path Algorithm"
[13]: https://onlinelibrary.wiley.com/doi/abs/10.1002/net.22174?utm_source=chatgpt.com "Targeted multiobjective Dijkstra algorithm"
[14]: https://ojs.aaai.org/index.php/SOCS/article/view/18563?utm_source=chatgpt.com "Bi-Objective Search with Bi-directional A* (Extended ..."
[15]: https://ojs.aaai.org/index.php/ICAPS/article/view/36110?utm_source=chatgpt.com "Cost-Optimal FOND Planning as Bi-Objective Best-First ..."
[16]: https://ojs.aaai.org/index.php/AAAI/article/view/41005?utm_source=chatgpt.com "A Fast Heuristic Search Approach for Energy-Optimal ..."
[17]: https://ojs.aaai.org/index.php/SOCS/article/view/27288?utm_source=chatgpt.com "Towards Effective Multi-Valued Heuristics for Bi-objective ..."
[18]: https://ojs.aaai.org/index.php/SOCS/article/view/31564?utm_source=chatgpt.com "Speeding Up Dominance Checks in Multi-Objective Search"
[19]: https://ojs.aaai.org/index.php/SOCS/article/view/31560?utm_source=chatgpt.com "Efficient Set Dominance Checks in Multi-Objective Shortest ..."
[20]: https://ojs.aaai.org/index.php/ICAPS/article/view/19825?utm_source=chatgpt.com "Efficient Approximate Multi-Objective Search on Graphs"
[21]: https://ojs.aaai.org/index.php/SOCS/article/view/21768?utm_source=chatgpt.com "Anytime Approximate Bi-Objective Search"
[22]: https://ojs.aaai.org/index.php/AAAI/article/view/21276?utm_source=chatgpt.com "Subset Approximation of Pareto Regions with Bi-objective A*"
[23]: https://ojs.aaai.org/index.php/SOCS/article/view/31568?utm_source=chatgpt.com "Finding a Small, Diverse Subset of the Pareto Solution Set ..."
[24]: https://ojs.aaai.org/index.php/SOCS/article/view/35977?utm_source=chatgpt.com "A Preprocessing Framework for Efficient Approximate Bi ..."
[25]: https://ojs.aaai.org/index.php/SOCS/article/view/35989?utm_source=chatgpt.com "Bi-Objective Search for the Traveling Salesman Problem ..."
[26]: https://wonderren.github.io/files/ren22_mopbd-RAL_ICRA22.pdf?utm_source=chatgpt.com "Multi-Objective Path-Based D* Lite"
[27]: https://wonderren.github.io/files/ren22_mosipp_RAL_IROS22.pdf?utm_source=chatgpt.com "Multi-Objective Safe-Interval Path Planning with Dynamic ..."
[28]: https://wonderren.github.io/files/ren21_mocbs_icra21.pdf?utm_source=chatgpt.com "Multi-objective Conflict-based Search for Multi-agent Path ..."
[29]: https://ojs.aaai.org/index.php/ICAPS/article/view/27214?utm_source=chatgpt.com "Binary Branching Multi-Objective Conflict-Based Search for ..."
[30]: https://ojs.aaai.org/index.php/ICAPS/article/view/31524?utm_source=chatgpt.com "Efficient Approximate Search for Multi-Objective Multi- ..."
[31]: https://ojs.aaai.org/index.php/SOCS/article/view/35972?utm_source=chatgpt.com "Finding All Optimal Solutions in Multi-Agent Path Finding"
[32]: https://ojs.aaai.org/index.php/SOCS/article/view/35979?utm_source=chatgpt.com "Minimizing Fuel in Multi-Agent Pathfinding"
[33]: https://ojs.aaai.org/index.php/AAAI/article/view/34873?utm_source=chatgpt.com "Inapproximability of Optimal Multi-Agent Pathfinding ..."
[34]: https://www.sciencedirect.com/science/article/pii/S0377221711008010?utm_source=chatgpt.com "A comparison of heuristic best-first algorithms for bicriterion ..."
[35]: https://www.sciencedirect.com/science/article/pii/S0377221714004159?utm_source=chatgpt.com "Multiobjective shortest path problems with lexicographic ..."
[36]: https://www.sciencedirect.com/science/article/pii/S0305054815001240?utm_source=chatgpt.com "Dimensionality reduction in multiobjective shortest path ..."
[37]: https://www.sciencedirect.com/science/article/pii/S030505481630154X?utm_source=chatgpt.com "Analysis of FPTASes for the multi-objective shortest path ..."
