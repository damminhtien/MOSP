#include"algorithms/sopmoa_bucket.h"

template<int N>
void SOPMOA_bucket<N>::solve(double time_limit) {
    std::cout << "start SOPMOA_bucket " + std::to_string(num_threads) + " threads\n";

    solutions.clear();
    target_frontier_.clear();
    num_generation = 0;
    num_expansion = 0;

    generated_labels_total_.store(0, std::memory_order_relaxed);
    expanded_labels_total_.store(0, std::memory_order_relaxed);
    pruned_by_target_total_.store(0, std::memory_order_relaxed);
    pruned_by_node_total_.store(0, std::memory_order_relaxed);
    pruned_other_total_.store(0, std::memory_order_relaxed);
    target_hits_raw_total_.store(0, std::memory_order_relaxed);
    target_frontier_checks_total_.store(0, std::memory_order_relaxed);
    node_frontier_checks_total_.store(0, std::memory_order_relaxed);
    queued_labels.store(0, std::memory_order_relaxed);
    peak_open_size_.store(0, std::memory_order_relaxed);
    allocated_labels.store(0, std::memory_order_relaxed);
    peak_live_labels_.store(0, std::memory_order_relaxed);
    timed_out.store(false, std::memory_order_relaxed);
    next_trace_sample_ms_.store(benchmark_trace_interval_ms(), std::memory_order_relaxed);

    for (auto& active : is_thread_activating) {
        active.store(false, std::memory_order_relaxed);
    }

    open = pq_bucket<N>(1, 0, 1000);
    search_start_ = BenchmarkClock::now();

    CostVec<N> start_g;
    start_g.fill(0);
    auto start_label = new Label<N>(start_node, start_g, heuristic(start_node));
    {
        std::lock_guard<std::mutex> guard(all_labels_lock);
        all_labels.push_back(start_label);
    }
    allocated_labels.store(1, std::memory_order_relaxed);
    peak_live_labels_.store(1, std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> guard(open_lock);
        open.push(start_label);
    }
    generated_labels_total_.store(1, std::memory_order_relaxed);
    queued_labels.store(1, std::memory_order_relaxed);
    peak_open_size_.store(1, std::memory_order_relaxed);

    std::vector<std::thread> workers;
    workers.reserve(num_threads);
    for (int i = 0; i < num_threads; i++) {
        workers.emplace_back([this, i, time_limit]() {
            thread_solve(i, time_limit);
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }

    num_generation = generated_labels_total_.load(std::memory_order_relaxed);
    num_expansion = expanded_labels_total_.load(std::memory_order_relaxed);
    rebuild_solutions_from_frontier(snapshot_target_frontier());

    if (benchmark_enabled()) {
        benchmark_recorder().set_counters(counter_snapshot());
        set_benchmark_status(
            timed_out.load(std::memory_order_relaxed) ? RunStatus::timeout : RunStatus::completed
        );
    }
}

template <int N>
void SOPMOA_bucket<N>::thread_solve(int thread_ID, double time_limit) {
    Label<N>* curr = nullptr;
    Label<N>* succ = nullptr;

    while (true) {
        const double elapsed = elapsed_sec();
        if (elapsed > time_limit) {
            timed_out.store(true, std::memory_order_release);
            break;
        }

        maybe_record_interval_sample(elapsed);

        bool has_item = false;
        {
            std::lock_guard<std::mutex> guard(open_lock);
            if (!open.empty()) {
                curr = open.top();
                open.pop();
                has_item = true;
            }
        }

        if (!has_item) {
            is_thread_activating[thread_ID].store(false, std::memory_order_release);
            if (queued_labels.load(std::memory_order_acquire) == 0 && !any_thread_active()) {
                break;
            }
            continue;
        }

        queued_labels.fetch_sub(1, std::memory_order_acq_rel);
        is_thread_activating[thread_ID].store(true, std::memory_order_release);

        if (curr->should_check_sol) {
            target_frontier_checks_total_.fetch_add(1, std::memory_order_relaxed);
            if (gcl_ptr->frontier_check(target_node, curr->f)) {
                pruned_by_target_total_.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
        }

        node_frontier_checks_total_.fetch_add(1, std::memory_order_relaxed);
        if (gcl_ptr->frontier_check(curr->node, curr->f)) {
            pruned_by_node_total_.fetch_add(1, std::memory_order_relaxed);
            continue;
        }

        if (!gcl_ptr->frontier_update(curr->node, curr->f, elapsed)) {
            pruned_other_total_.fetch_add(1, std::memory_order_relaxed);
            continue;
        }

        if (curr->node == target_node) {
            target_hits_raw_total_.fetch_add(1, std::memory_order_relaxed);
            if (benchmark_enabled()) {
                std::lock_guard<std::mutex> guard(benchmark_trace_lock);
                const FrontierPoint point{
                    std::vector<cost_t>(curr->f.begin(), curr->f.end()),
                    elapsed
                };
                if (insert_nondominated_frontier_point(target_frontier_, point)) {
                    const std::vector<FrontierPoint> frontier_snapshot =
                        sort_frontier_lexicographically(normalize_frontier(target_frontier_));
                    benchmark_recorder().on_target_frontier_changed(frontier_snapshot, counter_snapshot(), "target_accept");
                }
            } else {
                std::lock_guard<std::mutex> guard(benchmark_trace_lock);
                insert_nondominated_frontier_point(
                    target_frontier_,
                    FrontierPoint{std::vector<cost_t>(curr->f.begin(), curr->f.end()), elapsed}
                );
            }
            continue;
        }

        expanded_labels_total_.fetch_add(1, std::memory_order_relaxed);

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
                target_frontier_checks_total_.fetch_add(1, std::memory_order_relaxed);
                if (gcl_ptr->frontier_check(target_node, succ_f)) {
                    pruned_by_target_total_.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
            }

            node_frontier_checks_total_.fetch_add(1, std::memory_order_relaxed);
            if (gcl_ptr->frontier_check(succ_node, succ_f)) {
                pruned_by_node_total_.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            succ = new Label<N>(succ_node, succ_f);
            succ->should_check_sol = should_check_sol;

            {
                std::lock_guard<std::mutex> guard(all_labels_lock);
                all_labels.push_back(succ);
            }
            const uint64_t live_labels = allocated_labels.fetch_add(1, std::memory_order_acq_rel) + 1;
            observe_peak(peak_live_labels_, live_labels);

            {
                std::lock_guard<std::mutex> guard(open_lock);
                open.push(succ);
            }
            generated_labels_total_.fetch_add(1, std::memory_order_relaxed);
            const uint64_t open_size = queued_labels.fetch_add(1, std::memory_order_acq_rel) + 1;
            observe_peak(peak_open_size_, open_size);
        }
    }

    is_thread_activating[thread_ID].store(false, std::memory_order_release);
}

template<int N>
bool SOPMOA_bucket<N>::any_thread_active() const {
    for (int i = 0; i < num_threads; i++) {
        if (is_thread_activating[i].load(std::memory_order_acquire)) {
            return true;
        }
    }
    return false;
}

template<int N>
double SOPMOA_bucket<N>::elapsed_sec() const {
    return benchmark_elapsed_sec(search_start_);
}

template<int N>
void SOPMOA_bucket<N>::observe_peak(std::atomic<uint64_t>& peak, uint64_t value) {
    uint64_t current = peak.load(std::memory_order_relaxed);
    while (current < value && !peak.compare_exchange_weak(current, value, std::memory_order_relaxed)) {}
}

template<int N>
CounterSet SOPMOA_bucket<N>::counter_snapshot() const {
    CounterSet counters;
    counters.generated_labels = generated_labels_total_.load(std::memory_order_relaxed);
    counters.expanded_labels = expanded_labels_total_.load(std::memory_order_relaxed);
    counters.pruned_by_target = pruned_by_target_total_.load(std::memory_order_relaxed);
    counters.pruned_by_node = pruned_by_node_total_.load(std::memory_order_relaxed);
    counters.pruned_other = pruned_other_total_.load(std::memory_order_relaxed);
    counters.target_hits_raw = target_hits_raw_total_.load(std::memory_order_relaxed);
    counters.peak_open_size = peak_open_size_.load(std::memory_order_relaxed);
    counters.peak_live_labels = peak_live_labels_.load(std::memory_order_relaxed);
    counters.target_frontier_checks = target_frontier_checks_total_.load(std::memory_order_relaxed);
    counters.node_frontier_checks = node_frontier_checks_total_.load(std::memory_order_relaxed);
    return counters;
}

template<int N>
void SOPMOA_bucket<N>::maybe_record_interval_sample(double elapsed) {
    if (!benchmark_enabled()) { return; }

    const uint64_t interval_ms = benchmark_trace_interval_ms();
    if (interval_ms == 0) { return; }

    const uint64_t elapsed_ms = static_cast<uint64_t>(elapsed * 1000.0);
    uint64_t next_due = next_trace_sample_ms_.load(std::memory_order_relaxed);
    if (elapsed_ms < next_due || next_due == 0) { return; }

    std::lock_guard<std::mutex> guard(benchmark_trace_lock);
    next_due = next_trace_sample_ms_.load(std::memory_order_relaxed);
    if (elapsed_ms < next_due || next_due == 0) { return; }

    const std::vector<FrontierPoint> frontier_snapshot =
        sort_frontier_lexicographically(normalize_frontier(target_frontier_));
    if (frontier_snapshot.empty()) { return; }

    next_trace_sample_ms_.store(next_due + interval_ms, std::memory_order_relaxed);
    benchmark_recorder().on_target_frontier_changed(frontier_snapshot, counter_snapshot(), "interval_sample");
}

template<int N>
std::vector<FrontierPoint> SOPMOA_bucket<N>::snapshot_target_frontier() const {
    std::lock_guard<std::mutex> guard(benchmark_trace_lock);
    return sort_frontier_lexicographically(normalize_frontier(target_frontier_));
}

template class SOPMOA_bucket<2>;
template class SOPMOA_bucket<3>;
template class SOPMOA_bucket<4>;
template class SOPMOA_bucket<5>;

std::shared_ptr<AbstractSolver> get_SOPMOA_bucket_solver(
    AdjacencyMatrix &graph, AdjacencyMatrix &inv_graph,
    size_t start_node, size_t target_node, int num_threads
) {
    int num_obj = graph.get_num_obj();
    if (num_obj == 2) {
        return std::make_shared<SOPMOA_bucket<2>>(graph, inv_graph, start_node, target_node, num_threads);
    } else if (num_obj == 3) {
        return std::make_shared<SOPMOA_bucket<3>>(graph, inv_graph, start_node, target_node, num_threads);
    } else if (num_obj == 4) {
        return std::make_shared<SOPMOA_bucket<4>>(graph, inv_graph, start_node, target_node, num_threads);
    } else if (num_obj == 5) {
        return std::make_shared<SOPMOA_bucket<5>>(graph, inv_graph, start_node, target_node, num_threads);
    } else {
        return nullptr;
    }
}
