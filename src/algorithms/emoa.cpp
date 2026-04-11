#include"algorithms/emoa.h"

template<int N>
void EMOA<N>::solve(double time_limit) {
    std::cout << "start EMOA\n";

    solutions.clear();
    target_frontier_.clear();
    num_generation = 0;
    num_expansion = 0;

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
    num_generation++;

    ThreadLocalSink* sink = nullptr;
    if (benchmark_enabled()) {
        sink = &benchmark_recorder().main_thread_sink();
        sink->on_label_generated(open.size(), all_labels.size());
    }

    const auto local_start_time = BenchmarkClock::now();
    auto elapsed_seconds = [this, &local_start_time]() {
        return benchmark_elapsed_sec(local_start_time);
    };
    auto snapshot_target_frontier = [this]() {
        return sort_frontier_lexicographically(normalize_frontier(target_frontier_));
    };
    const double trace_interval_sec = benchmark_trace_interval_sec();
    double next_trace_sample_sec = trace_interval_sec > 0.0 ? trace_interval_sec : std::numeric_limits<double>::infinity();

    while (!open.empty()) {
        const double elapsed_sec = elapsed_seconds();
        if (elapsed_sec > time_limit) {
            rebuild_solutions_from_frontier(snapshot_target_frontier());
            if (benchmark_enabled()) {
                set_benchmark_status(RunStatus::timeout);
            }
            return;
        }

        if (benchmark_interval_sample_due(elapsed_sec, next_trace_sample_sec, target_frontier_)) {
            emit_interval_frontier_sample(snapshot_target_frontier(), next_trace_sample_sec);
        }

        std::pop_heap(open.begin(), open.end(), comparator);
        curr = open.back();
        open.pop_back();
        if (sink != nullptr) {
            sink->observe_open_size(open.size());
        }

        auto curr_f_tr = truncate<N>(curr->f);
        if (curr->should_check_sol) {
            if (sink != nullptr) {
                sink->on_frontier_check_target();
            }
            if (gcl_ptr->frontier_check(target_node, curr_f_tr)) {
                if (sink != nullptr) {
                    sink->on_pruned_by_target();
                }
                continue;
            }
        }
        if (sink != nullptr) {
            sink->on_frontier_check_node();
        }
        if (gcl_ptr->frontier_check(curr->node, curr_f_tr)) {
            if (sink != nullptr) {
                sink->on_pruned_by_node();
            }
            continue;
        }

        gcl_ptr->frontier_update(curr->node, curr_f_tr, elapsed_sec);

        if (curr->node == target_node) {
            record_target_frontier_accept(
                target_frontier_,
                FrontierPoint{std::vector<cost_t>(curr->f.begin(), curr->f.end()), elapsed_sec},
                sink
            );
            continue;
        }

        num_expansion++;
        if (sink != nullptr) {
            sink->on_label_expanded(open.size(), all_labels.size());
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
                if (sink != nullptr) {
                    sink->on_frontier_check_target();
                }
                if (gcl_ptr->frontier_check(target_node, succ_f_tr)) {
                    if (sink != nullptr) {
                        sink->on_pruned_by_target();
                    }
                    continue;
                }
            }
            if (sink != nullptr) {
                sink->on_frontier_check_node();
            }
            if (gcl_ptr->frontier_check(succ_node, succ_f_tr)) {
                if (sink != nullptr) {
                    sink->on_pruned_by_node();
                }
                continue;
            }

            succ = new Label<N>(succ_node, succ_f);
            succ->should_check_sol = should_check_sol;

            all_labels.push_back(succ);
            open.push_back(succ);
            std::push_heap(open.begin(), open.end(), comparator);
            num_generation++;
            if (sink != nullptr) {
                sink->on_label_generated(open.size(), all_labels.size());
            }
        }
    }

    rebuild_solutions_from_frontier(snapshot_target_frontier());
    if (benchmark_enabled()) {
        set_benchmark_status(RunStatus::completed);
    }
}

template class EMOA<2>;
template class EMOA<3>;
template class EMOA<4>;
template class EMOA<5>;

std::shared_ptr<AbstractSolver> get_EMOA_solver(
    AdjacencyMatrix &graph, AdjacencyMatrix &inv_graph,
    size_t start_node, size_t target_node
) {
    int num_obj = graph.get_num_obj();
    if (num_obj == 2) {
        return std::make_shared<EMOA<2>>(graph, inv_graph, start_node, target_node);
    } else if (num_obj == 3) {
        return std::make_shared<EMOA<3>>(graph, inv_graph, start_node, target_node);
    } else if (num_obj == 4) {
        return std::make_shared<EMOA<4>>(graph, inv_graph, start_node, target_node);
    } else if (num_obj == 5) {
        return std::make_shared<EMOA<5>>(graph, inv_graph, start_node, target_node);
    } else {
        return nullptr;
    }
}
