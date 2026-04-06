#include"algorithms/nwmoa.h"

template<int N>
void NWMOA<N>::solve(double time_limit) {
    std::cout << "start NWMOA\n";

    Label<N>* curr;
    Label<N>* succ;

    struct comparator_t{
        bool operator()(const LabelPtr<N> &a, const LabelPtr<N> &b) const {
            return !(a->f[0] < b->f[0]);
        };
    };
    comparator_t comparator;
    // std::vector<Label<N>*> open;
    // std::make_heap(open.begin(), open.end(), comparator);
    pq_bucket<N> open(1, 0, 1000);

    CostVec<N> start_g;
    start_g.fill(0);
    curr = new Label<N>(start_node, start_g, heuristic(start_node));

    all_labels.push_back(curr);
    // open.push_back(curr);
    // std::push_heap(open.begin(), open.end(), comparator);
        // std::cout << "HERE-1\n";

    open.push(curr);

    auto start_time = std::clock();
        // std::cout << "HERE0\n";

    while (!open.empty()) {
        if ((std::clock() - start_time)/CLOCKS_PER_SEC > time_limit){ return; }

        // std::pop_heap(open.begin(), open.end(), comparator);
        // curr = open.back();
        // open.pop_back();
        // std::cout << "HERE1\n";

        curr = open.top();
        open.pop();
        // std::cout << "HERE2\n";


        num_generation++;

        auto curr_f_tr = truncate<N>(curr->f);
        if (weakly_dominate<N-1>(cost_last_truncated[curr->node], curr_f_tr)) { continue; }
        if (weakly_dominate<N-1>(cost_last_truncated[target_node], curr_f_tr)) { continue; }
        if (gcl_ptr->frontier_check(curr->node, curr_f_tr)) { continue; }
        if (gcl_ptr->frontier_check(target_node, curr_f_tr)) { continue; }

        gcl_ptr->frontier_update(curr->node, curr_f_tr);
        cost_last_truncated[curr->node] = curr_f_tr;
        num_expansion++;

        if (curr->node == target_node) {
            auto rit = solutions.rbegin();
            while (rit != solutions.rend()) {
                CostVec<N> temp_cost;
                std::copy(rit->cost.begin(), rit->cost.end(), temp_cost.begin());
                if (curr->f[0] != temp_cost[0]) { break; }
                if (weakly_dominate<N>(curr->f, temp_cost)) {
                    rit = decltype(rit)(solutions.erase(std::next(rit).base()));
                } else { rit++; }
            }
            std::vector<cost_t> cost(curr->f.begin(), curr->f.end());
            solutions.emplace_back(cost, std::clock() - start_time);
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
        
            auto succ_f_tr = truncate<N>(succ_f);
            if (weakly_dominate<N-1>(cost_last_truncated[succ_node], succ_f_tr)) { continue; }
            if (weakly_dominate<N-1>(cost_last_truncated[target_node], succ_f_tr)) { continue; }

            succ = new Label<N>(succ_node, succ_f);
            all_labels.push_back(succ);
            // open.push_back(succ);
            // std::push_heap(open.begin(), open.end(), comparator);
        // std::cout << "HERE33\n";

            open.push(succ);
        // std::cout << "HERE44\n";

        }
    }
}

template class NWMOA<2>;
template class NWMOA<3>;
template class NWMOA<4>;
template class NWMOA<5>;

std::shared_ptr<AbstractSolver> get_NWMOA_solver(
    AdjacencyMatrix &graph, AdjacencyMatrix &inv_graph,
    size_t start_node, size_t target_node
) {
    int num_obj = graph.get_num_obj();
    if (num_obj == 2) {
        return std::make_shared<NWMOA<2>>(graph, inv_graph, start_node, target_node);
    } else if (num_obj == 3) {
        return std::make_shared<NWMOA<3>>(graph, inv_graph, start_node, target_node);
    } else if (num_obj == 4) {
        return std::make_shared<NWMOA<4>>(graph, inv_graph, start_node, target_node);
    } else if (num_obj == 5) {
        return std::make_shared<NWMOA<5>>(graph, inv_graph, start_node, target_node);
    } else {
        return nullptr;
    }
}
