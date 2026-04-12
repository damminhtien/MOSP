#include "algorithms/ltmoa_array_superfast.h"

#include <cmath>
#include <queue>

template<int N>
void LTMOA_array_superfast<N>::solve(double time_limit) {
    if (benchmark_enabled()) {
        solve_impl<true>(time_limit);
        return;
    }
    solve_impl<false>(time_limit);
}

template<int N>
bool LTMOA_array_superfast<N>::AnytimeEntry::larger_for_pq::operator()(
    const AnytimeEntry& lhs,
    const AnytimeEntry& rhs
) const {
    if (lhs.nondominated_against_incumbents != rhs.nondominated_against_incumbents) {
        return lhs.nondominated_against_incumbents < rhs.nondominated_against_incumbents;
    }
    if (lhs.best_margin_sum != rhs.best_margin_sum) {
        return lhs.best_margin_sum > rhs.best_margin_sum;
    }
    if (LTMOA_array_superfast<N>::lex_less(rhs.label->f, lhs.label->f)) {
        return true;
    }
    if (LTMOA_array_superfast<N>::lex_less(lhs.label->f, rhs.label->f)) {
        return false;
    }
    return lhs.label->node > rhs.label->node;
}

template<int N>
bool LTMOA_array_superfast<N>::ScalarWarmStartEntry::larger_for_pq::operator()(
    const ScalarWarmStartEntry& lhs,
    const ScalarWarmStartEntry& rhs
) const {
    if (lhs.f_scalar != rhs.f_scalar) {
        return lhs.f_scalar > rhs.f_scalar;
    }
    if (LTMOA_array_superfast<N>::lex_less(rhs.g_vec, lhs.g_vec)) {
        return true;
    }
    if (LTMOA_array_superfast<N>::lex_less(lhs.g_vec, rhs.g_vec)) {
        return false;
    }
    return lhs.node > rhs.node;
}

template<int N>
uint64_t LTMOA_array_superfast<N>::guarded_add_u64(uint64_t lhs, uint64_t rhs) {
    const uint64_t limit = std::numeric_limits<uint64_t>::max();
    if (lhs > limit - rhs) {
        return limit;
    }
    return lhs + rhs;
}

template<int N>
uint64_t LTMOA_array_superfast<N>::weighted_dot(const CostVec<N>& weights, const CostVec<N>& values) {
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
uint64_t LTMOA_array_superfast<N>::weighted_dot_edge(
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
bool LTMOA_array_superfast<N>::lex_less(const CostVec<N>& lhs, const CostVec<N>& rhs) {
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
bool LTMOA_array_superfast<N>::better_scalar_state(
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
std::vector<CostVec<N>> LTMOA_array_superfast<N>::build_warm_start_weights() {
    std::vector<CostVec<N>> weights;
    weights.reserve(N + 1);

    CostVec<N> all_ones{};
    all_ones.fill(1);
    weights.push_back(all_ones);

    for (int idx = 0; idx < N; idx++) {
        CostVec<N> one_hot{};
        one_hot.fill(0);
        one_hot[idx] = 1;
        weights.push_back(one_hot);
    }

    return weights;
}

template<int N>
void LTMOA_array_superfast<N>::recompute_target_component_min() const {
    target_component_min_.fill(MAX_COST);
    for (const auto& point : target_frontier_exact_) {
        for (int idx = 0; idx < N; idx++) {
            target_component_min_[idx] = std::min(target_component_min_[idx], point.cost[idx]);
        }
    }
    target_component_min_dirty_ = false;
}

template<int N>
bool LTMOA_array_superfast<N>::target_dominated(const CostVec<N>& cost) const {
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
typename LTMOA_array_superfast<N>::AcceptTargetResult
LTMOA_array_superfast<N>::accept_target(const CostVec<N>& cost, double elapsed_sec) {
    AcceptTargetResult result;
    const bool old_empty = target_frontier_exact_.empty();
    bool removed_min_contributor = false;

    size_t idx = 0;
    while (idx < target_frontier_exact_.size()) {
        ExactTargetPoint& existing = target_frontier_exact_[idx];
        if (existing.cost == cost) {
            if (existing.time_found_sec < 0.0 || elapsed_sec < existing.time_found_sec) {
                existing.time_found_sec = elapsed_sec;
                target_frontier_export_cache_dirty_ = true;
                result.accepted = true;
            }
            return result;
        }
        if (weakly_dominate<N>(existing.cost, cost)) {
            return result;
        }
        if (weakly_dominate<N>(cost, existing.cost)) {
            for (int cost_idx = 0; cost_idx < N; cost_idx++) {
                if (!target_frontier_exact_.empty() && existing.cost[cost_idx] == target_component_min_[cost_idx]) {
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

    target_frontier_exact_.push_back({cost, elapsed_sec});
    target_frontier_export_cache_dirty_ = true;
    result.accepted = true;
    result.first_incumbent = old_empty;
    result.skyline_changed = true;

    if (result.first_incumbent) {
        target_component_min_ = cost;
        target_component_min_dirty_ = false;
    } else {
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
    }

    target_revision_++;
    return result;
}

template<int N>
bool LTMOA_array_superfast<N>::run_scalar_seed(
    const CostVec<N>& weights,
    double seed_deadline_sec,
    const BenchmarkClock::time_point& local_start_time,
    CostVec<N>& solution_cost
) {
    if (benchmark_elapsed_sec(local_start_time) >= seed_deadline_sec) {
        return false;
    }

    if (warm_start_stamp_ == std::numeric_limits<uint32_t>::max()) {
        for (auto& state : warm_start_states_) {
            state.stamp = 0;
        }
        warm_start_stamp_ = 0;
    }
    warm_start_stamp_++;

    std::pmr::vector<ScalarWarmStartEntry> open_storage(&scratch_resource_);
    std::priority_queue<
        ScalarWarmStartEntry,
        std::pmr::vector<ScalarWarmStartEntry>,
        typename ScalarWarmStartEntry::larger_for_pq
    > open(typename ScalarWarmStartEntry::larger_for_pq(), std::move(open_storage));

    CostVec<N> start_g{};
    start_g.fill(0);
    warm_start_states_[start_node].stamp = warm_start_stamp_;
    warm_start_states_[start_node].g_scalar = 0;
    warm_start_states_[start_node].g_vec = start_g;
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

        const ScalarWarmStartEntry curr = open.top();
        open.pop();

        ScalarWarmStartState& curr_state = warm_start_states_[curr.node];
        if (curr_state.stamp != warm_start_stamp_) {
            continue;
        }
        if (curr_state.g_scalar != curr.g_scalar || curr_state.g_vec != curr.g_vec) {
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
            ScalarWarmStartState& next_state = warm_start_states_[edge.tail];
            if (next_state.stamp == warm_start_stamp_
                && !better_scalar_state(next_g_scalar, next_g_vec, next_state.g_scalar, next_state.g_vec)) {
                continue;
            }

            next_state.stamp = warm_start_stamp_;
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
template<bool EnableMetrics>
void LTMOA_array_superfast<N>::maybe_run_deferred_seed_burst(
    double time_limit,
    double elapsed_sec,
    const BenchmarkClock::time_point& local_start_time
) {
    if (deferred_seed_attempted_ || !target_frontier_exact_.empty()) {
        return;
    }

    if (std::isfinite(time_limit) && time_limit < 0.05) {
        deferred_seed_attempted_ = true;
        return;
    }

    const double trigger_time_sec = std::isfinite(time_limit)
        ? std::min(0.003, 0.02 * time_limit)
        : 0.003;
    if (elapsed_sec < trigger_time_sec && num_expansion < 2048) {
        return;
    }

    deferred_seed_attempted_ = true;

    double burst_budget_sec = std::isfinite(time_limit)
        ? std::min(0.002, 0.01 * time_limit)
        : 0.002;
    if (std::isfinite(time_limit)) {
        burst_budget_sec = std::min(burst_budget_sec, std::max(0.0, time_limit - elapsed_sec));
    }
    if (burst_budget_sec <= 0.0) {
        return;
    }

    const double seed_deadline_sec = elapsed_sec + burst_budget_sec;
    const std::vector<CostVec<N>> weights = build_warm_start_weights();
    size_t skyline_accepts = 0;

    for (const CostVec<N>& weight : weights) {
        if (benchmark_elapsed_sec(local_start_time) >= seed_deadline_sec || skyline_accepts >= 2) {
            break;
        }

        CostVec<N> solution_cost{};
        if (!run_scalar_seed(weight, seed_deadline_sec, local_start_time, solution_cost)) {
            if (benchmark_elapsed_sec(local_start_time) >= seed_deadline_sec) {
                break;
            }
            continue;
        }

        const double accept_elapsed_sec = benchmark_elapsed_sec(local_start_time);
        const AcceptTargetResult accept_result = accept_target(solution_cost, accept_elapsed_sec);
        if (!accept_result.accepted) {
            continue;
        }

        if constexpr (EnableMetrics) {
            benchmark_recorder().main_thread_sink().on_target_hit_raw();
            benchmark_recorder().on_target_frontier_changed(target_frontier_export_cache(), "deferred_seed");
        }

        if (accept_result.first_incumbent) {
            anytime_open_active_ = true;
            mixed_pop_counter_ = 0;
        }
        if (accept_result.skyline_changed) {
            skyline_accepts++;
        }
    }
}

template<int N>
void LTMOA_array_superfast<N>::materialize_target_frontier_export_cache() const {
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
const std::vector<FrontierPoint>& LTMOA_array_superfast<N>::target_frontier_export_cache() const {
    materialize_target_frontier_export_cache();
    return target_frontier_export_cache_;
}

template<int N>
void LTMOA_array_superfast<N>::rebuild_solutions_from_exact_target_frontier() {
    rebuild_solutions_from_frontier(target_frontier_export_cache());
}

template<int N>
void LTMOA_array_superfast<N>::reset_solve_storage() {
    label_resource_.release();
    arena_.clear();

    using PoppedLabelSet = std::pmr::unordered_set<Label<N>*>;
    std::destroy_at(std::addressof(popped_labels_));
    scratch_resource_.release();
    new (&popped_labels_) PoppedLabelSet(&scratch_resource_);
}

template<int N>
typename LTMOA_array_superfast<N>::AnytimeEntry
LTMOA_array_superfast<N>::make_anytime_entry(Label<N>* label) const {
    AnytimeEntry entry;
    entry.label = label;
    entry.revision = target_revision_;
    entry.nondominated_against_incumbents = 0;
    entry.best_margin_sum = 0;

    if (target_frontier_exact_.empty()) {
        return entry;
    }

    entry.best_margin_sum = std::numeric_limits<uint64_t>::max();
    for (const auto& incumbent : target_frontier_exact_) {
        if (!weakly_dominate<N>(incumbent.cost, label->f)) {
            entry.nondominated_against_incumbents++;
        }

        uint64_t margin_sum = 0;
        for (int idx = 0; idx < N; idx++) {
            if (label->f[idx] > incumbent.cost[idx]) {
                margin_sum = guarded_add_u64(
                    margin_sum,
                    static_cast<uint64_t>(label->f[idx] - incumbent.cost[idx])
                );
            }
        }
        entry.best_margin_sum = std::min(entry.best_margin_sum, margin_sum);
    }

    if (entry.best_margin_sum == std::numeric_limits<uint64_t>::max()) {
        entry.best_margin_sum = 0;
    }
    return entry;
}

template<int N>
bool LTMOA_array_superfast<N>::pop_lex_open(std::pmr::vector<Label<N>*>& lex_open, Label<N>*& label) {
    typename Label<N>::lex_larger_for_PQ comparator;
    while (!lex_open.empty()) {
        std::pop_heap(lex_open.begin(), lex_open.end(), comparator);
        Label<N>* candidate = lex_open.back();
        lex_open.pop_back();
        if (!popped_labels_.insert(candidate).second) {
            continue;
        }
        label = candidate;
        return true;
    }
    return false;
}

template<int N>
bool LTMOA_array_superfast<N>::pop_anytime_open(std::pmr::vector<AnytimeEntry>& anytime_open, Label<N>*& label) {
    typename AnytimeEntry::larger_for_pq comparator;
    while (!anytime_open.empty()) {
        std::pop_heap(anytime_open.begin(), anytime_open.end(), comparator);
        AnytimeEntry entry = anytime_open.back();
        anytime_open.pop_back();

        if (popped_labels_.find(entry.label) != popped_labels_.end()) {
            continue;
        }

        if (entry.revision != target_revision_) {
            AnytimeEntry refreshed = make_anytime_entry(entry.label);
            anytime_open.push_back(refreshed);
            std::push_heap(anytime_open.begin(), anytime_open.end(), comparator);
            continue;
        }

        popped_labels_.insert(entry.label);
        label = entry.label;
        return true;
    }
    return false;
}

template<int N>
template<bool EnableMetrics>
void LTMOA_array_superfast<N>::solve_impl(double time_limit) {
    std::cout << "start LTMOA_array_superfast\n";

    solutions.clear();
    target_frontier_exact_.clear();
    target_frontier_export_cache_.clear();
    target_frontier_export_cache_dirty_ = false;
    target_component_min_.fill(MAX_COST);
    target_component_min_dirty_ = false;
    num_generation = 0;
    num_expansion = 0;
    reset_solve_storage();
    gcl_.clear();
    target_revision_ = 0;
    anytime_open_active_ = false;
    deferred_seed_attempted_ = false;
    mixed_pop_counter_ = 0;

    const auto local_start_time = BenchmarkClock::now();
    auto elapsed_seconds = [this, &local_start_time]() {
        if constexpr (EnableMetrics) {
            return benchmark_recorder().elapsed_sec();
        }
        return benchmark_elapsed_sec(local_start_time);
    };

    typename Label<N>::lex_larger_for_PQ lex_comparator;
    typename AnytimeEntry::larger_for_pq anytime_comparator;
    std::pmr::vector<Label<N>*> lex_open(&scratch_resource_);
    std::pmr::vector<AnytimeEntry> anytime_open(&scratch_resource_);
    std::make_heap(lex_open.begin(), lex_open.end(), lex_comparator);
    std::make_heap(anytime_open.begin(), anytime_open.end(), anytime_comparator);

    CostVec<N> start_g{};
    start_g.fill(0);
    Label<N>* start_label = arena_.create(start_node, start_g, heuristic_(start_node));
    lex_open.push_back(start_label);
    std::push_heap(lex_open.begin(), lex_open.end(), lex_comparator);
    num_generation++;

    ThreadLocalSink* sink = nullptr;
    if constexpr (EnableMetrics) {
        sink = &benchmark_recorder().main_thread_sink();
        sink->on_label_generated(lex_open.size(), arena_.allocated());
    }

    double next_trace_sample_sec = std::numeric_limits<double>::infinity();
    double trace_interval_sec = 0.0;
    if constexpr (EnableMetrics) {
        trace_interval_sec = static_cast<double>(benchmark_trace_interval_ms()) / 1000.0;
        if (trace_interval_sec > 0.0) {
            next_trace_sample_sec = trace_interval_sec;
        }
    }

    Label<N>* curr = nullptr;
    while (true) {
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

        maybe_run_deferred_seed_burst<EnableMetrics>(time_limit, elapsed_sec, local_start_time);

        bool popped = false;
        if (anytime_open_active_ && mixed_pop_counter_ >= 7) {
            popped = pop_anytime_open(anytime_open, curr);
            if (popped) {
                mixed_pop_counter_ = 0;
            } else {
                popped = pop_lex_open(lex_open, curr);
                if (popped) {
                    mixed_pop_counter_ = 0;
                }
            }
        } else {
            popped = pop_lex_open(lex_open, curr);
            if (popped) {
                if (anytime_open_active_ && mixed_pop_counter_ < 7) {
                    mixed_pop_counter_++;
                } else {
                    mixed_pop_counter_ = 0;
                }
            } else {
                popped = pop_anytime_open(anytime_open, curr);
                if (popped) {
                    mixed_pop_counter_ = 0;
                }
            }
        }

        if (!popped) {
            break;
        }

        if constexpr (EnableMetrics) {
            sink->observe_open_size(lex_open.size());
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
            const AcceptTargetResult accept_result = accept_target(curr->f, elapsed_sec);
            if (accept_result.first_incumbent) {
                anytime_open_active_ = true;
                mixed_pop_counter_ = 0;
            }
            if (accept_result.accepted) {
                if constexpr (EnableMetrics) {
                    benchmark_recorder().on_target_frontier_changed(target_frontier_export_cache(), "target_accept");
                }
            }
            continue;
        }

        num_expansion++;
        if constexpr (EnableMetrics) {
            sink->on_label_expanded(lex_open.size(), arena_.allocated());
        }

        const CostVec<N>& curr_h = heuristic_(curr->node);
        CostVec<N> curr_g{};
        for (int idx = 0; idx < N; idx++) {
            curr_g[idx] = curr->f[idx] - curr_h[idx];
        }

        const std::vector<Edge>& outgoing_edges = graph[curr->node];
        for (const auto& edge : outgoing_edges) {
            const size_t succ_node = edge.tail;
            const CostVec<N>& succ_h = heuristic_(succ_node);
            CostVec<N> succ_f{};
            for (int idx = 0; idx < N; idx++) {
                succ_f[idx] = curr_g[idx] + edge.cost[idx] + succ_h[idx];
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

            lex_open.push_back(succ);
            std::push_heap(lex_open.begin(), lex_open.end(), lex_comparator);
            if (anytime_open_active_) {
                anytime_open.push_back(make_anytime_entry(succ));
                std::push_heap(anytime_open.begin(), anytime_open.end(), anytime_comparator);
            }

            num_generation++;
            if constexpr (EnableMetrics) {
                sink->on_label_generated(lex_open.size(), arena_.allocated());
            }
        }
    }

    rebuild_solutions_from_exact_target_frontier();
    if constexpr (EnableMetrics) {
        set_benchmark_status(RunStatus::completed);
    }
}

template class LTMOA_array_superfast<2>;
template class LTMOA_array_superfast<3>;
template class LTMOA_array_superfast<4>;
template class LTMOA_array_superfast<5>;

std::shared_ptr<AbstractSolver> get_LTMOA_array_superfast_solver(
    AdjacencyMatrix &graph, AdjacencyMatrix &inv_graph,
    size_t start_node, size_t target_node
) {
    const int num_obj = graph.get_num_obj();
    if (num_obj == 2) {
        return std::make_shared<LTMOA_array_superfast<2>>(graph, inv_graph, start_node, target_node);
    } else if (num_obj == 3) {
        return std::make_shared<LTMOA_array_superfast<3>>(graph, inv_graph, start_node, target_node);
    } else if (num_obj == 4) {
        return std::make_shared<LTMOA_array_superfast<4>>(graph, inv_graph, start_node, target_node);
    } else if (num_obj == 5) {
        return std::make_shared<LTMOA_array_superfast<5>>(graph, inv_graph, start_node, target_node);
    }
    return nullptr;
}
