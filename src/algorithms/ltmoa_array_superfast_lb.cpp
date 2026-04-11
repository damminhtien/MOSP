#include "algorithms/ltmoa_array_superfast_lb.h"

#include <cmath>
#include <queue>

template<int N>
LTMOA_array_superfast_lb<N>::LTMOA_array_superfast_lb(
    AdjacencyMatrix &adj_matrix,
    AdjacencyMatrix &inv_graph,
    size_t start_node,
    size_t target_node
)
: AbstractSolver(adj_matrix, start_node, target_node),
  lower_bound_(adj_matrix, inv_graph, start_node, target_node),
  gcl_(FastGclArray<N-1>(adj_matrix.get_num_node())) {
    const CostVec<N>& start_bound = lower_bound_(start_node);
    for (int idx = 0; idx < N; idx++) {
        start_scale_[idx] = std::max<cost_t>(1, start_bound[idx]);
    }
}

template<int N>
bool LTMOA_array_superfast_lb<N>::OpenEntry::larger_for_pq::operator()(
    const OpenEntry& lhs,
    const OpenEntry& rhs
) const {
    if (lhs.scalar_key != rhs.scalar_key) {
        return lhs.scalar_key > rhs.scalar_key;
    }
    if (LTMOA_array_superfast_lb<N>::lex_less(rhs.label->f, lhs.label->f)) {
        return true;
    }
    if (LTMOA_array_superfast_lb<N>::lex_less(lhs.label->f, rhs.label->f)) {
        return false;
    }
    return lhs.label->node > rhs.label->node;
}

template<int N>
bool LTMOA_array_superfast_lb<N>::ScalarOpenEntry::larger_for_pq::operator()(
    const ScalarOpenEntry& lhs,
    const ScalarOpenEntry& rhs
) const {
    if (lhs.f_scalar != rhs.f_scalar) {
        return lhs.f_scalar > rhs.f_scalar;
    }
    if (LTMOA_array_superfast_lb<N>::lex_less(rhs.g_vec, lhs.g_vec)) {
        return true;
    }
    if (LTMOA_array_superfast_lb<N>::lex_less(lhs.g_vec, rhs.g_vec)) {
        return false;
    }
    return lhs.node > rhs.node;
}

template<int N>
std::vector<CostVec<N>> LTMOA_array_superfast_lb<N>::build_warm_start_weights() {
    std::vector<CostVec<N>> weights;
    weights.reserve(2 * N + 1);

    CostVec<N> all_ones{};
    all_ones.fill(1);
    weights.push_back(all_ones);

    for (int idx = 0; idx < N; idx++) {
        CostVec<N> one_hot{};
        one_hot.fill(0);
        one_hot[idx] = 1;
        weights.push_back(one_hot);
    }

    for (int idx = 0; idx < N; idx++) {
        CostVec<N> corner{};
        corner.fill(1);
        corner[idx] = 8;
        weights.push_back(corner);
    }

    return weights;
}

template<int N>
void LTMOA_array_superfast_lb<N>::solve(double time_limit) {
    if (benchmark_enabled()) {
        solve_impl<true>(time_limit);
        return;
    }
    solve_impl<false>(time_limit);
}

template<int N>
uint64_t LTMOA_array_superfast_lb<N>::guarded_add_u64(uint64_t lhs, uint64_t rhs) {
    const uint64_t limit = std::numeric_limits<uint64_t>::max();
    if (lhs > limit - rhs) {
        return limit;
    }
    return lhs + rhs;
}

template<int N>
uint64_t LTMOA_array_superfast_lb<N>::weighted_dot(
    const CostVec<N>& weights,
    const CostVec<N>& values
) {
    uint64_t total = 0;
    for (int idx = 0; idx < N; idx++) {
        total = guarded_add_u64(
            total,
            static_cast<uint64_t>(weights[idx]) * static_cast<uint64_t>(values[idx])
        );
    }
    return total;
}

template<int N>
uint64_t LTMOA_array_superfast_lb<N>::weighted_dot_edge(
    const CostVec<N>& weights,
    const std::array<cost_t, Edge::MAX_NUM_OBJ>& values
) {
    uint64_t total = 0;
    for (int idx = 0; idx < N; idx++) {
        total = guarded_add_u64(
            total,
            static_cast<uint64_t>(weights[idx]) * static_cast<uint64_t>(values[idx])
        );
    }
    return total;
}

template<int N>
bool LTMOA_array_superfast_lb<N>::lex_less(const CostVec<N>& lhs, const CostVec<N>& rhs) {
    for (int idx = 0; idx < N; idx++) {
        if (lhs[idx] < rhs[idx]) {
            return true;
        }
        if (lhs[idx] > rhs[idx]) {
            return false;
        }
    }
    return false;
}

template<int N>
bool LTMOA_array_superfast_lb<N>::better_scalar_state(
    uint64_t lhs_scalar,
    const CostVec<N>& lhs_vec,
    uint64_t rhs_scalar,
    const CostVec<N>& rhs_vec
) {
    if (lhs_scalar != rhs_scalar) {
        return lhs_scalar < rhs_scalar;
    }
    return lex_less(lhs_vec, rhs_vec);
}

template<int N>
double LTMOA_array_superfast_lb<N>::open_scalar_key(const CostVec<N>& f) const {
    double scalar_key = 0.0;
    for (int idx = 0; idx < N; idx++) {
        scalar_key += static_cast<double>(f[idx]) / static_cast<double>(start_scale_[idx]);
    }
    return scalar_key;
}

template<int N>
bool LTMOA_array_superfast_lb<N>::target_dominated(const CostVec<N>& cost) const {
    for (const auto& point : target_frontier_exact_) {
        if (weakly_dominate<N>(point.cost, cost)) {
            return true;
        }
    }
    return false;
}

template<int N>
bool LTMOA_array_superfast_lb<N>::accept_target(const CostVec<N>& cost, double elapsed_sec) {
    size_t idx = 0;
    while (idx < target_frontier_exact_.size()) {
        ExactTargetPoint& existing = target_frontier_exact_[idx];
        if (existing.cost == cost) {
            if (existing.time_found_sec < 0.0 || elapsed_sec < existing.time_found_sec) {
                existing.time_found_sec = elapsed_sec;
                target_frontier_export_cache_dirty_ = true;
                return true;
            }
            return false;
        }
        if (weakly_dominate<N>(existing.cost, cost)) {
            return false;
        }
        if (weakly_dominate<N>(cost, existing.cost)) {
            target_frontier_exact_[idx] = target_frontier_exact_.back();
            target_frontier_exact_.pop_back();
            continue;
        }
        idx++;
    }

    target_frontier_exact_.push_back({cost, elapsed_sec});
    target_frontier_export_cache_dirty_ = true;
    return true;
}

template<int N>
void LTMOA_array_superfast_lb<N>::materialize_target_frontier_export_cache() const {
    if (!target_frontier_export_cache_dirty_) {
        return;
    }

    target_frontier_export_cache_.clear();
    target_frontier_export_cache_.reserve(target_frontier_exact_.size());
    for (const auto& point : target_frontier_exact_) {
        target_frontier_export_cache_.push_back({
            std::vector<cost_t>(point.cost.begin(), point.cost.end()),
            point.time_found_sec
        });
    }
    target_frontier_export_cache_dirty_ = false;
}

template<int N>
const std::vector<FrontierPoint>& LTMOA_array_superfast_lb<N>::target_frontier_export_cache() const {
    materialize_target_frontier_export_cache();
    return target_frontier_export_cache_;
}

template<int N>
void LTMOA_array_superfast_lb<N>::rebuild_solutions_from_exact_target_frontier() {
    rebuild_solutions_from_frontier(target_frontier_export_cache());
}

template<int N>
bool LTMOA_array_superfast_lb<N>::run_scalar_warm_start(
    const CostVec<N>& weights,
    double warm_start_deadline_sec,
    const BenchmarkClock::time_point& local_start_time,
    CostVec<N>& solution_cost
) const {
    const size_t num_nodes = static_cast<size_t>(graph.get_num_node());
    std::vector<uint64_t> best_scalar(num_nodes, std::numeric_limits<uint64_t>::max());
    std::vector<CostVec<N>> best_vec(num_nodes);
    std::vector<bool> seen(num_nodes, false);

    for (auto& value : best_vec) {
        value.fill(MAX_COST);
    }

    std::priority_queue<
        ScalarOpenEntry,
        std::vector<ScalarOpenEntry>,
        typename ScalarOpenEntry::larger_for_pq
    > open;

    CostVec<N> start_g{};
    start_g.fill(0);
    best_scalar[start_node] = 0;
    best_vec[start_node] = start_g;
    seen[start_node] = true;
    open.push({
        start_node,
        0,
        weighted_dot(weights, lower_bound_(start_node)),
        start_g
    });

    while (!open.empty()) {
        if (benchmark_elapsed_sec(local_start_time) >= warm_start_deadline_sec) {
            return false;
        }

        const ScalarOpenEntry curr = open.top();
        open.pop();

        if (!seen[curr.node]) {
            continue;
        }
        if (curr.g_scalar != best_scalar[curr.node] || curr.g_vec != best_vec[curr.node]) {
            continue;
        }

        if (curr.node == target_node) {
            solution_cost = curr.g_vec;
            return true;
        }

        const std::vector<Edge>& outgoing_edges = graph[curr.node];
        for (const auto& edge : outgoing_edges) {
            CostVec<N> next_g_vec = curr.g_vec;
            for (int idx = 0; idx < N; idx++) {
                next_g_vec[idx] += edge.cost[idx];
            }

            const uint64_t next_g_scalar = guarded_add_u64(curr.g_scalar, weighted_dot_edge(weights, edge.cost));
            if (seen[edge.tail] && !better_scalar_state(next_g_scalar, next_g_vec, best_scalar[edge.tail], best_vec[edge.tail])) {
                continue;
            }

            best_scalar[edge.tail] = next_g_scalar;
            best_vec[edge.tail] = next_g_vec;
            seen[edge.tail] = true;
            open.push({
                edge.tail,
                next_g_scalar,
                guarded_add_u64(next_g_scalar, weighted_dot(weights, lower_bound_(edge.tail))),
                next_g_vec
            });
        }
    }

    return false;
}

template<int N>
template<bool EnableMetrics>
void LTMOA_array_superfast_lb<N>::warm_start(
    double warm_start_deadline_sec,
    const BenchmarkClock::time_point& local_start_time
) {
    if (warm_start_deadline_sec <= 0.0) {
        return;
    }

    const std::vector<CostVec<N>> weights = build_warm_start_weights();
    int consecutive_no_improvement = 0;

    for (const CostVec<N>& weight : weights) {
        if (benchmark_elapsed_sec(local_start_time) >= warm_start_deadline_sec) {
            break;
        }
        if (consecutive_no_improvement >= N) {
            break;
        }

        CostVec<N> solution_cost{};
        if (!run_scalar_warm_start(weight, warm_start_deadline_sec, local_start_time, solution_cost)) {
            consecutive_no_improvement++;
            continue;
        }

        const double elapsed_sec = benchmark_elapsed_sec(local_start_time);
        if constexpr (EnableMetrics) {
            benchmark_recorder().main_thread_sink().on_target_hit_raw();
        }
        if (accept_target(solution_cost, elapsed_sec)) {
            consecutive_no_improvement = 0;
            if constexpr (EnableMetrics) {
                benchmark_recorder().on_target_frontier_changed(target_frontier_export_cache(), "warm_start");
            }
        } else {
            consecutive_no_improvement++;
        }
    }
}

template<int N>
template<bool EnableMetrics>
void LTMOA_array_superfast_lb<N>::solve_impl(double time_limit) {
    std::cout << "start LTMOA_array_superfast_lb\n";

    solutions.clear();
    target_frontier_exact_.clear();
    target_frontier_export_cache_.clear();
    target_frontier_export_cache_dirty_ = false;
    num_generation = 0;
    num_expansion = 0;
    arena_.clear();
    gcl_.clear();

    const auto local_start_time = BenchmarkClock::now();
    const auto elapsed_seconds = [&]() {
        return benchmark_elapsed_sec(local_start_time);
    };

    double warm_start_deadline_sec = 0.0;
    if (std::isfinite(time_limit)) {
        if (time_limit >= 0.020) {
            warm_start_deadline_sec = std::min(0.10 * time_limit, 0.050);
        }
    } else {
        warm_start_deadline_sec = 0.050;
    }
    warm_start<EnableMetrics>(warm_start_deadline_sec, local_start_time);

    typename OpenEntry::larger_for_pq comparator;
    std::vector<OpenEntry> open;
    std::make_heap(open.begin(), open.end(), comparator);

    CostVec<N> start_f = lower_bound_(start_node);
    Label<N>* start_label = arena_.create(start_node, start_f);
    open.push_back({start_label, open_scalar_key(start_f)});
    std::push_heap(open.begin(), open.end(), comparator);
    num_generation++;

    ThreadLocalSink* sink = nullptr;
    if constexpr (EnableMetrics) {
        sink = &benchmark_recorder().main_thread_sink();
        sink->on_label_generated(open.size(), arena_.allocated());
    }

    double next_trace_sample_sec = std::numeric_limits<double>::infinity();
    double trace_interval_sec = 0.0;
    if constexpr (EnableMetrics) {
        trace_interval_sec = static_cast<double>(benchmark_trace_interval_ms()) / 1000.0;
        if (trace_interval_sec > 0.0) {
            next_trace_sample_sec = trace_interval_sec;
        }
    }

    while (!open.empty()) {
        const double elapsed_sec = elapsed_seconds();
        if (elapsed_sec > time_limit) {
            rebuild_solutions_from_exact_target_frontier();
            if constexpr (EnableMetrics) {
                set_benchmark_status(RunStatus::timeout);
            }
            return;
        }

        if constexpr (EnableMetrics) {
            if (trace_interval_sec > 0.0 && !target_frontier_exact_.empty() && elapsed_sec >= next_trace_sample_sec) {
                benchmark_recorder().on_target_frontier_changed(target_frontier_export_cache(), "interval_sample");
                next_trace_sample_sec += trace_interval_sec;
            }
        }

        std::pop_heap(open.begin(), open.end(), comparator);
        const OpenEntry curr_entry = open.back();
        open.pop_back();
        Label<N>* curr = curr_entry.label;

        if constexpr (EnableMetrics) {
            sink->observe_open_size(open.size());
        }

        const CostVec<N-1> curr_truncated = truncate<N>(curr->f);
        if (curr->should_check_sol) {
            if constexpr (EnableMetrics) {
                benchmark_recorder().on_frontier_check_target();
            }
            if (target_dominated(curr->f)) {
                if constexpr (EnableMetrics) {
                    sink->on_pruned_by_target();
                }
                continue;
            }
        }

        if (curr->node != target_node) {
            if constexpr (EnableMetrics) {
                benchmark_recorder().on_frontier_check_node();
            }
            if (!gcl_.try_insert_or_prune(curr->node, curr_truncated)) {
                if constexpr (EnableMetrics) {
                    sink->on_pruned_by_node();
                }
                continue;
            }

            if constexpr (EnableMetrics) {
                benchmark_recorder().on_frontier_update();
            }
        }

        if (curr->node == target_node) {
            if constexpr (EnableMetrics) {
                sink->on_target_hit_raw();
            }
            if (accept_target(curr->f, elapsed_sec)) {
                if constexpr (EnableMetrics) {
                    benchmark_recorder().on_target_frontier_changed(target_frontier_export_cache(), "target_accept");
                }
            }
            continue;
        }

        num_expansion++;
        if constexpr (EnableMetrics) {
            sink->on_label_expanded(open.size(), arena_.allocated());
        }

        const CostVec<N>& curr_lb = lower_bound_(curr->node);
        CostVec<N> curr_g;
        for (int idx = 0; idx < N; idx++) {
            curr_g[idx] = curr->f[idx] - curr_lb[idx];
        }

        const std::vector<Edge>& outgoing_edges = graph[curr->node];
        for (const auto& edge : outgoing_edges) {
            const size_t succ_node = edge.tail;
            const CostVec<N>& succ_lb = lower_bound_(succ_node);
            CostVec<N> succ_f;
            for (int idx = 0; idx < N; idx++) {
                succ_f[idx] = curr_g[idx] + edge.cost[idx] + succ_lb[idx];
            }

            const bool should_check_sol = succ_f != curr->f;
            const CostVec<N-1> succ_truncated = truncate<N>(succ_f);

            if (should_check_sol) {
                if constexpr (EnableMetrics) {
                    benchmark_recorder().on_frontier_check_target();
                }
                if (target_dominated(succ_f)) {
                    if constexpr (EnableMetrics) {
                        sink->on_pruned_by_target();
                    }
                    continue;
                }
            }

            if (succ_node != target_node) {
                if constexpr (EnableMetrics) {
                    benchmark_recorder().on_frontier_check_node();
                }
                if (gcl_.dominated(succ_node, succ_truncated)) {
                    if constexpr (EnableMetrics) {
                        sink->on_pruned_by_node();
                    }
                    continue;
                }
            }

            Label<N>* succ = arena_.create(succ_node, succ_f);
            succ->should_check_sol = should_check_sol;

            open.push_back({succ, open_scalar_key(succ_f)});
            std::push_heap(open.begin(), open.end(), comparator);
            num_generation++;
            if constexpr (EnableMetrics) {
                sink->on_label_generated(open.size(), arena_.allocated());
            }
        }
    }

    rebuild_solutions_from_exact_target_frontier();
    if constexpr (EnableMetrics) {
        set_benchmark_status(RunStatus::completed);
    }
}

template class LTMOA_array_superfast_lb<2>;
template class LTMOA_array_superfast_lb<3>;
template class LTMOA_array_superfast_lb<4>;
template class LTMOA_array_superfast_lb<5>;

std::shared_ptr<AbstractSolver> get_LTMOA_array_superfast_lb_solver(
    AdjacencyMatrix &graph, AdjacencyMatrix &inv_graph,
    size_t start_node, size_t target_node
) {
    const int num_obj = graph.get_num_obj();
    if (num_obj == 2) {
        return std::make_shared<LTMOA_array_superfast_lb<2>>(graph, inv_graph, start_node, target_node);
    } else if (num_obj == 3) {
        return std::make_shared<LTMOA_array_superfast_lb<3>>(graph, inv_graph, start_node, target_node);
    } else if (num_obj == 4) {
        return std::make_shared<LTMOA_array_superfast_lb<4>>(graph, inv_graph, start_node, target_node);
    } else if (num_obj == 5) {
        return std::make_shared<LTMOA_array_superfast_lb<5>>(graph, inv_graph, start_node, target_node);
    }
    return nullptr;
}
