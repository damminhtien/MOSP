#include"algorithms/sopmoa.h"

template<int N>
void SOPMOA<N>::solve(double time_limit) {
    std::cout << "start SOPMOA " + std::to_string(num_threads) + " threads\n";

    is_thread_activating.fill(false);
    std::fill_n(is_thread_activating.begin(), num_threads, true);

    CostVec<N> start_g;
    start_g.fill(0);
    auto start_label = new Label<N>(start_node, start_g, heuristic(start_node));

    all_labels.push_back(start_label);
    open.push(start_label);

    #pragma omp parallel for num_threads(num_threads)
    for (int i = 0; i < num_threads; i++) {
        thread_solve(i, time_limit);
    }
}

template <int N>
void SOPMOA<N>::thread_solve(int thread_ID, double time_limit) {
    auto start_time = omp_get_wtime(); 
    
    Label<N>* curr;
    Label<N>* succ;

    while (std::any_of(
        is_thread_activating.begin(), is_thread_activating.end(), 
        [](bool activated) { return activated; }
    )) {
        if ((omp_get_wtime() - start_time) > time_limit) {
            is_thread_activating[thread_ID] = false;
            return;
        }

        if (!open.try_pop(curr)) {
            is_thread_activating[thread_ID] = false;
            continue;
        }

        is_thread_activating[thread_ID] = true;
        num_generation++;

        if (curr->should_check_sol) {
            if (gcl_ptr->frontier_check(target_node, curr->f)) { continue; }
        }
        if (gcl_ptr->frontier_check(curr->node, curr->f)) { continue; }

        gcl_ptr->frontier_update(curr->node, curr->f);
        num_expansion++;

        if (curr->node == target_node) {
            std::vector<cost_t> cost(curr->f.begin(), curr->f.end());
            sols_lock.lock();
            solutions.emplace_back(cost, std::clock() - start_time);
            sols_lock.unlock();
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

            if (should_check_sol) {
                if (gcl_ptr->frontier_check(target_node, succ_f)) { continue; }
            }
            if (gcl_ptr->frontier_check(succ_node, succ_f)) { continue; }

            succ = new Label<N>(succ_node, succ_f);
            succ->should_check_sol = should_check_sol;

            all_labels.push_back(succ);
            open.push(succ);
        }
    }
}

template class SOPMOA<2>;
template class SOPMOA<3>;
template class SOPMOA<4>;
template class SOPMOA<5>;

std::shared_ptr<AbstractSolver> get_SOPMOA_solver(
    AdjacencyMatrix &graph, AdjacencyMatrix &inv_graph,
    size_t start_node, size_t target_node, int num_threads
) {
    int num_obj = graph.get_num_obj();
    if (num_obj == 2) {
        return std::make_shared<SOPMOA<2>>(graph, inv_graph, start_node, target_node, num_threads);
    } else if (num_obj == 3) {
        return std::make_shared<SOPMOA<3>>(graph, inv_graph, start_node, target_node, num_threads);
    } else if (num_obj == 4) {
        return std::make_shared<SOPMOA<4>>(graph, inv_graph, start_node, target_node, num_threads);
    } else if (num_obj == 5) {
        return std::make_shared<SOPMOA<5>>(graph, inv_graph, start_node, target_node, num_threads);
    } else {
        return nullptr;
    }
}
