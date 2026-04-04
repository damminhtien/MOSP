#include"algorithms/ltmoa.h"

template<int N>
void LTMOA<N>::solve(unsigned int time_limit) {
    std::cout << "start LTMOA\n";

    Label<N>* curr;
    Label<N>* succ;

    typename Label<N>::lex_larger_for_PQ comparator;
    std::vector<Label<N>*> open;
    std::make_heap(open.begin(), open.end(), comparator);

    CostVec<N> start_g;
    start_g.fill(0);
    curr = new Label<N>(start_node, start_g, heuristic(start_node));

    all_labels.push_back(curr);
    open.push_back(curr);
    std::push_heap(open.begin(), open.end(), comparator);

    const auto start_time = wall_now();

    while (!open.empty()) {
        if (time_limit_exceeded(start_time, time_limit)){ return; }

        std::pop_heap(open.begin(), open.end(), comparator);
        curr = open.back();
        open.pop_back();

        num_generation++;

        auto curr_f_tr = truncate<N>(curr->f);
        if (curr->should_check_sol) {
            if (gcl_ptr->frontier_check(target_node, curr_f_tr)) { continue; }
        }
        if (gcl_ptr->frontier_check(curr->node, curr_f_tr)) { continue; }

        gcl_ptr->frontier_update(curr->node, curr_f_tr);
        num_expansion++;

        if (curr->node == target_node) {
            std::vector<cost_t> cost(curr->f.begin(), curr->f.end());
            solutions.emplace_back(cost, elapsed_seconds(start_time));
            continue;
        }

        auto curr_h = heuristic(curr->node);
        CostVec<N> curr_g;
        for (int i = 0; i < N; i++){ curr_g[i] = curr->f[i] - curr_h[i]; }

        const std::vector<Edge> &outgoing_edges = graph[curr->node];
        for (auto it = outgoing_edges.begin(); it != outgoing_edges.end(); it++) {
            size_t succ_node = it->tail;
            auto succ_h = heuristic(succ_node);
            CostVec<N> succ_f;
            for (int i = 0; i < N; i++){ succ_f[i] = curr_g[i] + it->cost[i] + succ_h[i]; }

            bool should_check_sol = true;
            if (succ_f == curr->f) { should_check_sol = false; }

            auto succ_f_tr = truncate<N>(succ_f);
            if (should_check_sol) {
                if (gcl_ptr->frontier_check(target_node, succ_f_tr)) { continue; }
            }
            if (gcl_ptr->frontier_check(succ_node, succ_f_tr)) { continue; }

            succ = new Label<N>(succ_node, succ_f);
            succ->should_check_sol = should_check_sol;

            all_labels.push_back(succ);
            open.push_back(succ);
            std::push_heap(open.begin(), open.end(), comparator);
        }
    }
}

template class LTMOA<2>;
template class LTMOA<3>;
template class LTMOA<4>;
template class LTMOA<5>;

std::shared_ptr<AbstractSolver> get_LTMOA_solver(
    AdjacencyMatrix &graph, AdjacencyMatrix &inv_graph,
    size_t start_node, size_t target_node
) {
    int num_obj = graph.get_num_obj();
    if (num_obj == 2) {
        return std::make_shared<LTMOA<2>>(graph, inv_graph, start_node, target_node);
    } else if (num_obj == 3) {
        return std::make_shared<LTMOA<3>>(graph, inv_graph, start_node, target_node);
    } else if (num_obj == 4) {
        return std::make_shared<LTMOA<4>>(graph, inv_graph, start_node, target_node);
    } else if (num_obj == 5) {
        return std::make_shared<LTMOA<5>>(graph, inv_graph, start_node, target_node);
    } else {
        return nullptr;
    }
}
