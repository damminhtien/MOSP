#include "algorithms/hybrid_corridor_pulsea.h"

#include <algorithm>
#include <queue>

template<int N>
HybridCorridorPulseA<N>::HybridCorridorPulseA(
    AdjacencyMatrix &adj_matrix,
    AdjacencyMatrix &inv_graph,
    size_t start_node,
    size_t target_node,
    Config config
)
: AbstractSolver(adj_matrix, start_node, target_node),
  config_(config),
  heuristic_(target_node, inv_graph),
  label_resource_(label_resource_buffer_.data(), label_resource_buffer_.size()),
  arena_(label_resource_),
  gcl_(FastGclArray<N-1>(adj_matrix.get_num_node())),
  scalar_seed_states_(adj_matrix.get_num_node()) {
    target_component_min_.fill(MAX_COST);
}

template<int N>
bool HybridCorridorPulseA<N>::ScalarSeedEntry::larger_for_pq::operator()(
    const ScalarSeedEntry& lhs,
    const ScalarSeedEntry& rhs
) const {
    if (lhs.f_scalar != rhs.f_scalar) {
        return lhs.f_scalar > rhs.f_scalar;
    }
    if (HybridCorridorPulseA<N>::lex_less(rhs.g_vec, lhs.g_vec)) {
        return true;
    }
    if (HybridCorridorPulseA<N>::lex_less(lhs.g_vec, rhs.g_vec)) {
        return false;
    }
    return lhs.node > rhs.node;
}

template<int N>
bool HybridCorridorPulseA<N>::LexSeedEntry::larger_for_pq::operator()(
    const LexSeedEntry& lhs,
    const LexSeedEntry& rhs
) const {
    if (HybridCorridorPulseA<N>::lex_less(rhs.f_vec, lhs.f_vec)) {
        return true;
    }
    if (HybridCorridorPulseA<N>::lex_less(lhs.f_vec, rhs.f_vec)) {
        return false;
    }
    if (HybridCorridorPulseA<N>::lex_less(rhs.g_vec, lhs.g_vec)) {
        return true;
    }
    if (HybridCorridorPulseA<N>::lex_less(lhs.g_vec, rhs.g_vec)) {
        return false;
    }
    return lhs.node > rhs.node;
}

template<int N>
std::vector<CostVec<N>> HybridCorridorPulseA<N>::build_seed_weights() {
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
void HybridCorridorPulseA<N>::solve(double time_limit) {
    if (benchmark_enabled()) {
        solve_impl<true>(time_limit);
        return;
    }
    solve_impl<false>(time_limit);
}

template<int N>
uint64_t HybridCorridorPulseA<N>::guarded_add_u64(uint64_t lhs, uint64_t rhs) {
    const uint64_t limit = std::numeric_limits<uint64_t>::max();
    if (lhs > limit - rhs) {
        return limit;
    }
    return lhs + rhs;
}

template<int N>
uint64_t HybridCorridorPulseA<N>::weighted_dot(
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
uint64_t HybridCorridorPulseA<N>::weighted_dot_edge(
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
bool HybridCorridorPulseA<N>::lex_less(const CostVec<N>& lhs, const CostVec<N>& rhs) {
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
bool HybridCorridorPulseA<N>::better_scalar_state(
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
double HybridCorridorPulseA<N>::seed_deadline_sec(double time_limit) const {
    if (config_.seed_fraction <= 0.0 || config_.seed_cap_sec <= 0.0) {
        return 0.0;
    }

    if (std::isfinite(time_limit)) {
        if (time_limit <= 0.0) {
            return 0.0;
        }
        return std::min(config_.seed_fraction * time_limit, config_.seed_cap_sec);
    }

    return config_.seed_cap_sec;
}

template<int N>
bool HybridCorridorPulseA<N>::burst_allowed_for_child(
    const Label<N>* child,
    const std::pmr::vector<Label<N>*>& global_open,
    size_t depth,
    size_t burst_processed
) const {
    if (child == nullptr) {
        return false;
    }
    if (config_.max_burst_branching == 0) {
        return false;
    }
    if (depth >= config_.max_burst_depth) {
        return false;
    }
    if (burst_processed >= config_.max_burst_labels) {
        return false;
    }
    if (global_open.empty()) {
        return true;
    }

    const Label<N>* global_best = global_open.front();
    return !lex_less(global_best->f, child->f);
}

template<int N>
void HybridCorridorPulseA<N>::recompute_target_component_min() const {
    target_component_min_.fill(MAX_COST);
    for (const auto& point : target_frontier_exact_) {
        for (int idx = 0; idx < N; idx++) {
            target_component_min_[idx] = std::min(target_component_min_[idx], point.cost[idx]);
        }
    }
    target_component_min_dirty_ = false;
}

template<int N>
bool HybridCorridorPulseA<N>::target_dominated(const CostVec<N>& cost) const {
    if (target_frontier_exact_.empty()) {
        return false;
    }

    if (target_component_min_dirty_) {
        recompute_target_component_min();
    }

    for (int idx = 0; idx < N; idx++) {
        if (cost[idx] < target_component_min_[idx]) {
            return false;
        }
    }

    for (const auto& point : target_frontier_exact_) {
        if (weakly_dominate<N>(point.cost, cost)) {
            return true;
        }
    }
    return false;
}

template<int N>
bool HybridCorridorPulseA<N>::accept_target(const CostVec<N>& cost, double elapsed_sec) {
    bool removed_min_contributor = false;

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
            for (int cost_idx = 0; cost_idx < N; cost_idx++) {
                if (existing.cost[cost_idx] == target_component_min_[cost_idx]) {
                    removed_min_contributor = true;
                    break;
                }
            }
            target_frontier_exact_[idx] = target_frontier_exact_.back();
            target_frontier_exact_.pop_back();
            continue;
        }
        idx++;
    }

    const bool old_empty = target_frontier_exact_.empty();
    target_frontier_exact_.push_back({cost, elapsed_sec});
    target_frontier_export_cache_dirty_ = true;

    if (old_empty) {
        target_component_min_ = cost;
        target_component_min_dirty_ = false;
        return true;
    }

    if (removed_min_contributor) {
        target_component_min_dirty_ = true;
    }

    if (!target_component_min_dirty_) {
        for (int cost_idx = 0; cost_idx < N; cost_idx++) {
            target_component_min_[cost_idx] = std::min(target_component_min_[cost_idx], cost[cost_idx]);
        }
    } else {
        recompute_target_component_min();
    }

    return true;
}

template<int N>
void HybridCorridorPulseA<N>::materialize_target_frontier_export_cache() const {
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
const std::vector<FrontierPoint>& HybridCorridorPulseA<N>::target_frontier_export_cache() const {
    materialize_target_frontier_export_cache();
    return target_frontier_export_cache_;
}

template<int N>
void HybridCorridorPulseA<N>::rebuild_solutions_from_exact_target_frontier() {
    rebuild_solutions_from_frontier(target_frontier_export_cache());
}

template<int N>
bool HybridCorridorPulseA<N>::run_scalar_seed(
    const CostVec<N>& weights,
    double seed_deadline_sec,
    const BenchmarkClock::time_point& local_start_time,
    CostVec<N>& solution_cost
) {
    if (benchmark_elapsed_sec(local_start_time) >= seed_deadline_sec) {
        return false;
    }

    if (scalar_seed_stamp_ == std::numeric_limits<uint32_t>::max()) {
        for (auto& state : scalar_seed_states_) {
            state.stamp = 0;
        }
        scalar_seed_stamp_ = 0;
    }
    scalar_seed_stamp_++;

    std::pmr::vector<ScalarSeedEntry> open_storage(&scratch_resource_);
    std::priority_queue<
        ScalarSeedEntry,
        std::pmr::vector<ScalarSeedEntry>,
        typename ScalarSeedEntry::larger_for_pq
    > open(typename ScalarSeedEntry::larger_for_pq(), std::move(open_storage));

    CostVec<N> start_g{};
    start_g.fill(0);
    scalar_seed_states_[start_node].stamp = scalar_seed_stamp_;
    scalar_seed_states_[start_node].g_scalar = 0;
    scalar_seed_states_[start_node].g_vec = start_g;
    open.push({
        start_node,
        0,
        weighted_dot(weights, heuristic_(start_node)),
        start_g
    });

    while (!open.empty()) {
        if (benchmark_elapsed_sec(local_start_time) >= seed_deadline_sec) {
            return false;
        }

        const ScalarSeedEntry curr = open.top();
        open.pop();

        const ScalarSeedState& state = scalar_seed_states_[curr.node];
        if (state.stamp != scalar_seed_stamp_ || curr.g_scalar != state.g_scalar || curr.g_vec != state.g_vec) {
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
            ScalarSeedState& next_state = scalar_seed_states_[edge.tail];
            if (next_state.stamp == scalar_seed_stamp_
                && !better_scalar_state(next_g_scalar, next_g_vec, next_state.g_scalar, next_state.g_vec)) {
                continue;
            }

            next_state.stamp = scalar_seed_stamp_;
            next_state.g_scalar = next_g_scalar;
            next_state.g_vec = next_g_vec;
            open.push({
                edge.tail,
                next_g_scalar,
                guarded_add_u64(next_g_scalar, weighted_dot(weights, heuristic_(edge.tail))),
                next_g_vec
            });
        }
    }

    return false;
}

template<int N>
bool HybridCorridorPulseA<N>::run_lex_seed(
    double seed_deadline_sec,
    const BenchmarkClock::time_point& local_start_time,
    CostVec<N>& solution_cost
) {
    if (benchmark_elapsed_sec(local_start_time) >= seed_deadline_sec) {
        return false;
    }

    const size_t num_nodes = static_cast<size_t>(graph.get_num_node());
    std::vector<CostVec<N>> best_g(num_nodes);
    std::vector<bool> seen(num_nodes, false);
    for (auto& value : best_g) {
        value.fill(MAX_COST);
    }

    std::pmr::vector<LexSeedEntry> open_storage(&scratch_resource_);
    std::priority_queue<
        LexSeedEntry,
        std::pmr::vector<LexSeedEntry>,
        typename LexSeedEntry::larger_for_pq
    > open(typename LexSeedEntry::larger_for_pq(), std::move(open_storage));

    CostVec<N> start_g{};
    start_g.fill(0);
    best_g[start_node] = start_g;
    seen[start_node] = true;
    open.push({
        start_node,
        start_g,
        heuristic_(start_node)
    });

    while (!open.empty()) {
        if (benchmark_elapsed_sec(local_start_time) >= seed_deadline_sec) {
            return false;
        }

        const LexSeedEntry curr = open.top();
        open.pop();
        if (!seen[curr.node] || curr.g_vec != best_g[curr.node]) {
            continue;
        }

        if (curr.node == target_node) {
            solution_cost = curr.g_vec;
            return true;
        }

        const std::vector<Edge>& outgoing_edges = graph[curr.node];
        for (const auto& edge : outgoing_edges) {
            CostVec<N> next_g = curr.g_vec;
            for (int idx = 0; idx < N; idx++) {
                next_g[idx] += edge.cost[idx];
            }

            if (seen[edge.tail] && !lex_less(next_g, best_g[edge.tail])) {
                continue;
            }

            seen[edge.tail] = true;
            best_g[edge.tail] = next_g;

            CostVec<N> next_f = next_g;
            const CostVec<N>& next_h = heuristic_(edge.tail);
            for (int idx = 0; idx < N; idx++) {
                next_f[idx] += next_h[idx];
            }

            open.push({edge.tail, next_g, next_f});
        }
    }

    return false;
}

template<int N>
template<bool EnableMetrics>
void HybridCorridorPulseA<N>::seed_incumbents(
    double seed_deadline_sec,
    const BenchmarkClock::time_point& local_start_time
) {
    if (seed_deadline_sec <= 0.0) {
        return;
    }

    CostVec<N> solution_cost{};
    if (run_lex_seed(seed_deadline_sec, local_start_time, solution_cost)) {
        const double elapsed_sec = benchmark_elapsed_sec(local_start_time);
        if constexpr (EnableMetrics) {
            benchmark_recorder().main_thread_sink().on_target_hit_raw();
        }
        if (accept_target(solution_cost, elapsed_sec)) {
            if constexpr (EnableMetrics) {
                benchmark_recorder().on_target_frontier_changed(target_frontier_export_cache(), "lex_seed");
            }
        }
    }

    const std::vector<CostVec<N>> weights = build_seed_weights();
    int consecutive_no_improvement = 0;
    for (const CostVec<N>& weight : weights) {
        if (benchmark_elapsed_sec(local_start_time) >= seed_deadline_sec) {
            break;
        }
        if (consecutive_no_improvement >= N) {
            break;
        }

        if (!run_scalar_seed(weight, seed_deadline_sec, local_start_time, solution_cost)) {
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
                benchmark_recorder().on_target_frontier_changed(target_frontier_export_cache(), "scalar_seed");
            }
        } else {
            consecutive_no_improvement++;
        }
    }
}

template<int N>
template<bool EnableMetrics>
void HybridCorridorPulseA<N>::solve_impl(double time_limit) {
    std::cout << "start HybridCorridorPulseA\n";

    solutions.clear();
    target_frontier_exact_.clear();
    target_frontier_export_cache_.clear();
    target_frontier_export_cache_dirty_ = false;
    target_component_min_.fill(MAX_COST);
    target_component_min_dirty_ = false;
    num_generation = 0;
    num_expansion = 0;
    label_resource_.release();
    arena_.clear();
    scratch_resource_.release();
    gcl_.clear();

    const auto local_start_time = BenchmarkClock::now();
    const auto elapsed_seconds = [&]() {
        return benchmark_elapsed_sec(local_start_time);
    };

    seed_incumbents<EnableMetrics>(seed_deadline_sec(time_limit), local_start_time);

    typename Label<N>::lex_larger_for_PQ comparator;
    std::pmr::vector<Label<N>*> open(&scratch_resource_);
    std::pmr::vector<BurstFrame> burst_stack(&scratch_resource_);
    std::pmr::vector<Label<N>*> child_buffer(&scratch_resource_);
    std::make_heap(open.begin(), open.end(), comparator);

    auto total_queued_size = [&open, &burst_stack]() {
        return open.size() + burst_stack.size();
    };

    auto push_global = [&](Label<N>* label, ThreadLocalSink* sink) {
        open.push_back(label);
        std::push_heap(open.begin(), open.end(), comparator);
        num_generation++;
        if constexpr (EnableMetrics) {
            sink->on_label_generated(total_queued_size(), arena_.allocated());
        }
    };

    auto maybe_emit_interval_sample = [&](double elapsed_sec, double& next_trace_sample_sec, double trace_interval_sec) {
        if constexpr (EnableMetrics) {
            if (trace_interval_sec > 0.0 && !target_frontier_exact_.empty() && elapsed_sec >= next_trace_sample_sec) {
                benchmark_recorder().on_target_frontier_changed(target_frontier_export_cache(), "interval_sample");
                next_trace_sample_sec += trace_interval_sec;
            }
        }
    };

    ThreadLocalSink* sink = nullptr;
    if constexpr (EnableMetrics) {
        sink = &benchmark_recorder().main_thread_sink();
    }

    double next_trace_sample_sec = std::numeric_limits<double>::infinity();
    double trace_interval_sec = 0.0;
    if constexpr (EnableMetrics) {
        trace_interval_sec = static_cast<double>(benchmark_trace_interval_ms()) / 1000.0;
        if (trace_interval_sec > 0.0) {
            next_trace_sample_sec = trace_interval_sec;
        }
    }

    CostVec<N> start_g{};
    start_g.fill(0);
    Label<N>* start_label = arena_.create(start_node, start_g, heuristic_(start_node));
    push_global(start_label, sink);

    auto process_label = [&](Label<N>* curr, size_t depth, bool burst_enabled, size_t& burst_processed) -> bool {
        const double elapsed_sec = elapsed_seconds();
        if (elapsed_sec > time_limit) {
            rebuild_solutions_from_exact_target_frontier();
            if constexpr (EnableMetrics) {
                set_benchmark_status(RunStatus::timeout);
            }
            return true;
        }

        maybe_emit_interval_sample(elapsed_sec, next_trace_sample_sec, trace_interval_sec);

        const CostVec<N-1> curr_truncated = truncate<N>(curr->f);
        if (curr->should_check_sol) {
            if constexpr (EnableMetrics) {
                benchmark_recorder().on_frontier_check_target();
            }
            if (target_dominated(curr->f)) {
                if constexpr (EnableMetrics) {
                    sink->on_pruned_by_target();
                }
                return false;
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
                return false;
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
            return false;
        }

        num_expansion++;
        if constexpr (EnableMetrics) {
            sink->on_label_expanded(total_queued_size(), arena_.allocated());
        }

        const CostVec<N>& curr_lb = heuristic_(curr->node);
        CostVec<N> curr_g{};
        for (int idx = 0; idx < N; idx++) {
            curr_g[idx] = curr->f[idx] - curr_lb[idx];
        }

        child_buffer.clear();
        const std::vector<Edge>& outgoing_edges = graph[curr->node];
        for (const auto& edge : outgoing_edges) {
            const size_t succ_node = edge.tail;
            const CostVec<N>& succ_lb = heuristic_(succ_node);
            CostVec<N> succ_f{};
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
            succ->parent = curr;
            succ->should_check_sol = should_check_sol;
            child_buffer.push_back(succ);
        }

        if (child_buffer.empty()) {
            return false;
        }

        auto label_lex_less = [](const Label<N>* lhs, const Label<N>* rhs) {
            if (lex_less(lhs->f, rhs->f)) {
                return true;
            }
            if (lex_less(rhs->f, lhs->f)) {
                return false;
            }
            return lhs->node < rhs->node;
        };
        std::sort(child_buffer.begin(), child_buffer.end(), label_lex_less);

        if (!burst_enabled || target_frontier_exact_.size() < 2) {
            for (Label<N>* succ : child_buffer) {
                push_global(succ, sink);
            }
            return false;
        }

        Label<N>* preferred_child = child_buffer.front();
        if (!burst_allowed_for_child(preferred_child, open, depth, burst_processed)) {
            for (Label<N>* succ : child_buffer) {
                push_global(succ, sink);
            }
            return false;
        }

        burst_stack.push_back({preferred_child, depth + 1});
        burst_processed++;
        num_generation++;
        if constexpr (EnableMetrics) {
            sink->on_label_generated(total_queued_size(), arena_.allocated());
        }

        for (size_t idx = 1; idx < child_buffer.size(); idx++) {
            push_global(child_buffer[idx], sink);
        }
        return false;
    };

    while (!open.empty()) {
        const double elapsed_sec = elapsed_seconds();
        if (elapsed_sec > time_limit) {
            rebuild_solutions_from_exact_target_frontier();
            if constexpr (EnableMetrics) {
                set_benchmark_status(RunStatus::timeout);
            }
            return;
        }

        maybe_emit_interval_sample(elapsed_sec, next_trace_sample_sec, trace_interval_sec);

        std::pop_heap(open.begin(), open.end(), comparator);
        Label<N>* curr = open.back();
        open.pop_back();

        if constexpr (EnableMetrics) {
            sink->observe_open_size(total_queued_size());
        }

        if (target_frontier_exact_.size() < 2) {
            size_t burst_processed = 0;
            if (process_label(curr, 0, false, burst_processed)) {
                return;
            }
            continue;
        }

        burst_stack.clear();
        burst_stack.push_back({curr, 0});
        size_t burst_processed = 1;
        while (!burst_stack.empty()) {
            BurstFrame frame = burst_stack.back();
            burst_stack.pop_back();
            if constexpr (EnableMetrics) {
                sink->observe_open_size(total_queued_size());
            }
            if (process_label(frame.label, frame.depth, true, burst_processed)) {
                return;
            }
        }
    }

    rebuild_solutions_from_exact_target_frontier();
    if constexpr (EnableMetrics) {
        set_benchmark_status(RunStatus::completed);
    }
}

template class HybridCorridorPulseA<2>;
template class HybridCorridorPulseA<3>;
template class HybridCorridorPulseA<4>;
template class HybridCorridorPulseA<5>;

template<int N>
std::shared_ptr<AbstractSolver> make_hybrid_corridor_pulsea_solver(
    AdjacencyMatrix &graph,
    AdjacencyMatrix &inv_graph,
    size_t start_node,
    size_t target_node
) {
    return std::make_shared<HybridCorridorPulseA<N>>(graph, inv_graph, start_node, target_node);
}

std::shared_ptr<AbstractSolver> get_HybridCorridorPulseA_solver(
    AdjacencyMatrix &graph, AdjacencyMatrix &inv_graph,
    size_t start_node, size_t target_node
) {
    const int num_obj = graph.get_num_obj();
    if (num_obj == 2) {
        return make_hybrid_corridor_pulsea_solver<2>(graph, inv_graph, start_node, target_node);
    } else if (num_obj == 3) {
        return make_hybrid_corridor_pulsea_solver<3>(graph, inv_graph, start_node, target_node);
    } else if (num_obj == 4) {
        return make_hybrid_corridor_pulsea_solver<4>(graph, inv_graph, start_node, target_node);
    } else if (num_obj == 5) {
        return make_hybrid_corridor_pulsea_solver<5>(graph, inv_graph, start_node, target_node);
    }
    return nullptr;
}
