#include "algorithms/ltmoa_parallel.h"

template<int N>
LTMOA_parallel<N>::LTMOA_parallel(
    AdjacencyMatrix &adj_matrix,
    AdjacencyMatrix &inv_graph,
    size_t start_node,
    size_t target_node,
    int num_threads
)
: AbstractSolver(adj_matrix, start_node, target_node),
heuristic_(Heuristic<N>(target_node, inv_graph)),
num_threads_(num_threads > 0 ? num_threads : std::max(1u, std::thread::hardware_concurrency())),
gcl_(adj_matrix.get_num_node(), static_cast<size_t>(num_threads_)) {
    initialize_workers();
}

template<int N>
void LTMOA_parallel<N>::sync_benchmark_recorder() {
    if (benchmark_enabled()) {
        benchmark_recorder().set_counters(counter_snapshot());
    }
}

template<int N>
void LTMOA_parallel<N>::initialize_workers() {
    workers_.clear();
    workers_.reserve(num_threads_);
    for (int idx = 0; idx < num_threads_; idx++) {
        workers_.push_back(std::make_unique<WorkerState>());
        workers_.back()->heap.reserve(256);
        workers_.back()->pending_outboxes.resize(num_threads_);
    }
}

template<int N>
void LTMOA_parallel<N>::solve(double time_limit) {
    std::cout << "start LTMOA_parallel " << num_threads_ << " threads\n";

    num_generation = 0;
    num_expansion = 0;
    solutions.clear();
    target_frontier_exact_.clear();
    std::atomic_store_explicit(
        &published_target_snapshot_,
        std::shared_ptr<const TargetSnapshot>(),
        std::memory_order_release
    );
    gcl_.clear();

    inflight_labels_.store(0, std::memory_order_relaxed);
    queued_labels_.store(0, std::memory_order_relaxed);
    allocated_labels_.store(0, std::memory_order_relaxed);
    stop_requested_.store(false, std::memory_order_relaxed);
    timed_out_.store(false, std::memory_order_relaxed);
    generated_labels_total_.store(0, std::memory_order_relaxed);
    expanded_labels_total_.store(0, std::memory_order_relaxed);
    pruned_by_target_total_.store(0, std::memory_order_relaxed);
    pruned_by_node_total_.store(0, std::memory_order_relaxed);
    pruned_other_total_.store(0, std::memory_order_relaxed);
    target_hits_raw_total_.store(0, std::memory_order_relaxed);
    target_frontier_checks_total_.store(0, std::memory_order_relaxed);
    node_frontier_checks_total_.store(0, std::memory_order_relaxed);
    peak_open_size_.store(0, std::memory_order_relaxed);
    peak_live_labels_.store(0, std::memory_order_relaxed);

    for (auto& worker : workers_) {
        worker->arena.clear();
        worker->heap.clear();
        worker->processing_active.store(false, std::memory_order_release);
        for (auto& pending : worker->pending_outboxes) {
            pending.size = 0;
        }

        InboxBatch stale_batch;
        while (worker->inbox.try_pop(stale_batch)) {}
    }

    search_start_ = std::chrono::steady_clock::now();

    CostVec<N> start_g;
    start_g.fill(0);
    const size_t start_owner = owner_of(start_node);
    Label<N>* start_label = workers_[start_owner]->arena.create(start_node, start_g, heuristic_(start_node));
    allocated_labels_.store(1, std::memory_order_release);
    peak_live_labels_.store(1, std::memory_order_relaxed);
    inflight_labels_.store(1, std::memory_order_release);
    enqueue_label(start_owner, start_owner, start_label);

    std::vector<std::thread> threads;
    threads.reserve(num_threads_);
    for (int worker_id = 0; worker_id < num_threads_; worker_id++) {
        threads.emplace_back([this, worker_id, time_limit]() {
            worker_loop(static_cast<size_t>(worker_id), time_limit);
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    num_generation = generated_labels_total_.load(std::memory_order_relaxed);
    num_expansion = expanded_labels_total_.load(std::memory_order_relaxed);
    rebuild_solutions_from_frontier(target_frontier_exact_);

    if (benchmark_enabled()) {
        benchmark_recorder().set_counters(counter_snapshot());
        set_benchmark_status(
            timed_out_.load(std::memory_order_acquire) ? RunStatus::timeout : RunStatus::completed
        );
    }
}

template<int N>
void LTMOA_parallel<N>::worker_loop(size_t worker_id, double time_limit) {
    std::minstd_rand rng(static_cast<unsigned int>(worker_id + 1));

    for (;;) {
        if (stop_requested_.load(std::memory_order_acquire)) {
            break;
        }

        const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - search_start_
        ).count();
        if (elapsed > time_limit) {
            timed_out_.store(true, std::memory_order_release);
            stop_requested_.store(true, std::memory_order_release);
            break;
        }

        drain_inbox(worker_id);

        Label<N>* curr = nullptr;
        if (try_acquire_owner_shard(worker_id)) {
            if (pop_local(worker_id, curr)) {
                process_label(worker_id, worker_id, curr, time_limit);
                flush_all_outboxes(worker_id);
                release_owner_shard(worker_id);
                continue;
            }
            release_owner_shard(worker_id);
        }

        flush_all_outboxes(worker_id);
        drain_inbox(worker_id);
        if (try_acquire_owner_shard(worker_id)) {
            if (pop_local(worker_id, curr)) {
                process_label(worker_id, worker_id, curr, time_limit);
                flush_all_outboxes(worker_id);
                release_owner_shard(worker_id);
                continue;
            }
            release_owner_shard(worker_id);
        }

        size_t frontier_owner_id = worker_id;
        if (steal_label(worker_id, rng, frontier_owner_id, curr)) {
            process_label(worker_id, frontier_owner_id, curr, time_limit);
            flush_all_outboxes(worker_id);
            release_owner_shard(frontier_owner_id);
            continue;
        }

        if (inflight_labels_.load(std::memory_order_acquire) == 0) {
            break;
        }

        std::this_thread::yield();
    }

    flush_all_outboxes(worker_id);
}

template<int N>
bool LTMOA_parallel<N>::try_acquire_owner_shard(size_t owner_id) {
    bool expected = false;
    return workers_[owner_id]->processing_active.compare_exchange_strong(
        expected,
        true,
        std::memory_order_acq_rel,
        std::memory_order_relaxed
    );
}

template<int N>
void LTMOA_parallel<N>::release_owner_shard(size_t owner_id) {
    workers_[owner_id]->processing_active.store(false, std::memory_order_release);
}

template<int N>
bool LTMOA_parallel<N>::pop_local(size_t worker_id, Label<N>*& label) {
    auto& worker = *workers_[worker_id];
    std::lock_guard<std::mutex> guard(worker.heap_lock);
    if (worker.heap.empty()) {
        return false;
    }

    std::pop_heap(worker.heap.begin(), worker.heap.end(), typename Label<N>::lex_larger_for_PQ());
    label = worker.heap.back();
    worker.heap.pop_back();
    queued_labels_.fetch_sub(1, std::memory_order_acq_rel);
    return true;
}

template<int N>
void LTMOA_parallel<N>::process_label(
    size_t worker_id,
    size_t frontier_owner_id,
    Label<N>* curr,
    double time_limit
) {
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - search_start_
    ).count();
    if (elapsed > time_limit) {
        timed_out_.store(true, std::memory_order_release);
        stop_requested_.store(true, std::memory_order_release);
        inflight_labels_.fetch_sub(1, std::memory_order_acq_rel);
        return;
    }

    const CostVec<N-1> curr_truncated = truncate<N>(curr->f);
    if (curr->should_check_sol && target_dominated(curr->f)) {
        inflight_labels_.fetch_sub(1, std::memory_order_acq_rel);
        return;
    }
    if (curr->node != target_node) {
        if (node_dominated(frontier_owner_id, curr->node, curr_truncated)) {
            inflight_labels_.fetch_sub(1, std::memory_order_acq_rel);
            return;
        }
        if (!gcl_.owner_try_insert_or_prune(frontier_owner_id, curr->node, curr_truncated)) {
            pruned_other_total_.fetch_add(1, std::memory_order_relaxed);
            inflight_labels_.fetch_sub(1, std::memory_order_acq_rel);
            return;
        }
    }

    if (curr->node == target_node) {
        target_hits_raw_total_.fetch_add(1, std::memory_order_relaxed);
        accept_target(curr->f, elapsed);
        inflight_labels_.fetch_sub(1, std::memory_order_acq_rel);
        return;
    }

    expanded_labels_total_.fetch_add(1, std::memory_order_relaxed);

    const CostVec<N>& curr_h = heuristic_(curr->node);
    const std::vector<Edge>& outgoing_edges = graph[curr->node];
    for (const auto& edge : outgoing_edges) {
        const size_t succ_node = edge.tail;
        const CostVec<N>& succ_h = heuristic_(succ_node);
        CostVec<N> succ_f;
        for (int idx = 0; idx < N; idx++) {
            succ_f[idx] = curr->f[idx] + edge.cost[idx] + succ_h[idx] - curr_h[idx];
        }

        const bool should_check_sol = succ_f != curr->f;
        const CostVec<N-1> succ_truncated = truncate<N>(succ_f);
        if (should_check_sol && target_dominated(succ_f)) {
            continue;
        }
        if (succ_node != target_node && node_dominated(frontier_owner_id, succ_node, succ_truncated)) {
            continue;
        }

        Label<N>* succ = workers_[worker_id]->arena.create(succ_node, succ_f);
        succ->should_check_sol = should_check_sol;

        const uint64_t live_labels = allocated_labels_.fetch_add(1, std::memory_order_acq_rel) + 1;
        observe_peak(peak_live_labels_, live_labels);
        inflight_labels_.fetch_add(1, std::memory_order_acq_rel);
        enqueue_label(worker_id, owner_of(succ_node), succ);
    }

    inflight_labels_.fetch_sub(1, std::memory_order_acq_rel);
}

template<int N>
void LTMOA_parallel<N>::enqueue_label(size_t worker_id, size_t owner_id, Label<N>* label) {
    generated_labels_total_.fetch_add(1, std::memory_order_relaxed);
    const uint64_t open_size = queued_labels_.fetch_add(1, std::memory_order_acq_rel) + 1;
    observe_peak(peak_open_size_, open_size);

    auto& owner = *workers_[owner_id];
    if (owner_id == worker_id) {
        std::lock_guard<std::mutex> guard(owner.heap_lock);
        owner.heap.push_back(label);
        std::push_heap(owner.heap.begin(), owner.heap.end(), typename Label<N>::lex_larger_for_PQ());
        return;
    }

    InboxBatch& pending = workers_[worker_id]->pending_outboxes[owner_id];
    pending.labels[pending.size++] = label;
    if (pending.size == REMOTE_BATCH_SIZE) {
        flush_outbox(worker_id, owner_id);
    }
}

template<int N>
void LTMOA_parallel<N>::flush_outbox(size_t worker_id, size_t owner_id) {
    InboxBatch& pending = workers_[worker_id]->pending_outboxes[owner_id];
    if (pending.size == 0) {
        return;
    }

    workers_[owner_id]->inbox.push(pending);
    pending.size = 0;
}

template<int N>
void LTMOA_parallel<N>::flush_all_outboxes(size_t worker_id) {
    for (size_t owner_id = 0; owner_id < workers_[worker_id]->pending_outboxes.size(); owner_id++) {
        if (owner_id == worker_id) {
            continue;
        }
        flush_outbox(worker_id, owner_id);
    }
}

template<int N>
void LTMOA_parallel<N>::drain_inbox(size_t worker_id, size_t limit) {
    auto& worker = *workers_[worker_id];
    std::vector<Label<N>*> inbox_labels;
    inbox_labels.reserve(limit);

    InboxBatch batch;
    while (inbox_labels.size() < limit && worker.inbox.try_pop(batch)) {
        for (size_t idx = 0; idx < batch.size; idx++) {
            inbox_labels.push_back(batch.labels[idx]);
        }
    }

    if (inbox_labels.empty()) {
        return;
    }

    std::lock_guard<std::mutex> guard(worker.heap_lock);
    const size_t old_size = worker.heap.size();
    worker.heap.insert(worker.heap.end(), inbox_labels.begin(), inbox_labels.end());
    if (inbox_labels.size() <= 8) {
        for (size_t idx = old_size; idx < worker.heap.size(); idx++) {
            std::push_heap(worker.heap.begin(), worker.heap.begin() + idx + 1, typename Label<N>::lex_larger_for_PQ());
        }
        return;
    }

    std::make_heap(worker.heap.begin(), worker.heap.end(), typename Label<N>::lex_larger_for_PQ());
}

template<int N>
bool LTMOA_parallel<N>::steal_label(
    size_t worker_id,
    std::minstd_rand& rng,
    size_t& frontier_owner_id,
    Label<N>*& label
) {
    if (workers_.size() <= 1) {
        return false;
    }

    const size_t owner_count = workers_.size();
    const size_t max_attempts = std::min(STEAL_K, owner_count - 1);
    const size_t start = static_cast<size_t>(rng()) % owner_count;

    size_t attempts = 0;
    for (size_t offset = 0; offset < owner_count && attempts < max_attempts; offset++) {
        const size_t candidate = (start + offset) % owner_count;
        if (candidate == worker_id) {
            continue;
        }
        attempts++;

        if (!try_acquire_owner_shard(candidate)) {
            continue;
        }

        if (!pop_local(candidate, label)) {
            drain_inbox(candidate);
            if (!pop_local(candidate, label)) {
                release_owner_shard(candidate);
                continue;
            }
        }

        frontier_owner_id = candidate;
        return true;
    }
    return false;
}

template<int N>
bool LTMOA_parallel<N>::target_dominated(const CostVec<N>& cost) {
    target_frontier_checks_total_.fetch_add(1, std::memory_order_relaxed);
    std::shared_ptr<const TargetSnapshot> snapshot = std::atomic_load_explicit(
        &published_target_snapshot_,
        std::memory_order_acquire
    );
    if (!snapshot) {
        return false;
    }

    for (const auto& existing : *snapshot) {
        if (weakly_dominate<N>(existing, cost)) {
            pruned_by_target_total_.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
    }
    return false;
}

template<int N>
bool LTMOA_parallel<N>::node_dominated(
    size_t frontier_owner_id,
    size_t node,
    const CostVec<N-1>& cost
) {
    node_frontier_checks_total_.fetch_add(1, std::memory_order_relaxed);
    bool dominated = false;
    if (frontier_owner_id == owner_of(node)) {
        dominated = gcl_.owner_dominated(frontier_owner_id, node, cost);
    } else {
        dominated = gcl_.read_dominated(node, cost);
    }

    if (dominated) {
        pruned_by_node_total_.fetch_add(1, std::memory_order_relaxed);
    }
    return dominated;
}

template<int N>
bool LTMOA_parallel<N>::accept_target(const CostVec<N>& cost, double elapsed_sec) {
    FrontierPoint point{
        std::vector<cost_t>(cost.begin(), cost.end()),
        elapsed_sec
    };

    std::lock_guard<std::mutex> target_guard(target_frontier_lock_);
    if (!insert_nondominated_frontier_point(target_frontier_exact_, point)) {
        return false;
    }
    auto snapshot = std::make_shared<TargetSnapshot>();
    snapshot->reserve(target_frontier_exact_.size());
    for (const auto& existing : target_frontier_exact_) {
        CostVec<N> existing_cost{};
        for (int idx = 0; idx < N; idx++) {
            existing_cost[idx] = existing.cost[idx];
        }
        snapshot->push_back(existing_cost);
    }
    std::shared_ptr<const TargetSnapshot> published_snapshot = snapshot;
    std::atomic_store_explicit(
        &published_target_snapshot_,
        published_snapshot,
        std::memory_order_release
    );

    if (benchmark_enabled()) {
        std::lock_guard<std::mutex> guard(benchmark_trace_lock_);
        benchmark_recorder().on_target_frontier_changed(target_frontier_exact_, counter_snapshot(), "target_accept");
    }
    return true;
}

template<int N>
void LTMOA_parallel<N>::observe_peak(std::atomic<uint64_t>& peak, uint64_t value) {
    uint64_t current = peak.load(std::memory_order_relaxed);
    while (current < value && !peak.compare_exchange_weak(current, value, std::memory_order_relaxed)) {}
}

template<int N>
CounterSet LTMOA_parallel<N>::counter_snapshot() const {
    CounterSet counters;
    counters.generated_labels = generated_labels_total_.load(std::memory_order_relaxed);
    counters.expanded_labels = expanded_labels_total_.load(std::memory_order_relaxed);
    counters.pruned_by_target = pruned_by_target_total_.load(std::memory_order_relaxed);
    counters.pruned_by_node = pruned_by_node_total_.load(std::memory_order_relaxed);
    counters.pruned_other = pruned_other_total_.load(std::memory_order_relaxed);
    counters.target_hits_raw = target_hits_raw_total_.load(std::memory_order_relaxed);
    counters.final_frontier_size = target_frontier_exact_.size();
    counters.peak_open_size = peak_open_size_.load(std::memory_order_relaxed);
    counters.peak_live_labels = peak_live_labels_.load(std::memory_order_relaxed);
    counters.target_frontier_checks = target_frontier_checks_total_.load(std::memory_order_relaxed);
    counters.node_frontier_checks = node_frontier_checks_total_.load(std::memory_order_relaxed);
    return counters;
}

template class LTMOA_parallel<2>;
template class LTMOA_parallel<3>;
template class LTMOA_parallel<4>;
template class LTMOA_parallel<5>;

template<int N>
std::shared_ptr<AbstractSolver> make_ltmoa_parallel_solver(
    AdjacencyMatrix &graph,
    AdjacencyMatrix &inv_graph,
    size_t start_node,
    size_t target_node,
    int num_threads
) {
    return std::make_shared<LTMOA_parallel<N>>(graph, inv_graph, start_node, target_node, num_threads);
}

std::shared_ptr<AbstractSolver> get_LTMOA_parallel_solver(
    AdjacencyMatrix &graph, AdjacencyMatrix &inv_graph,
    size_t start_node, size_t target_node, int num_threads
) {
    const int num_obj = graph.get_num_obj();
    if (num_obj == 2) {
        return make_ltmoa_parallel_solver<2>(graph, inv_graph, start_node, target_node, num_threads);
    } else if (num_obj == 3) {
        return make_ltmoa_parallel_solver<3>(graph, inv_graph, start_node, target_node, num_threads);
    } else if (num_obj == 4) {
        return make_ltmoa_parallel_solver<4>(graph, inv_graph, start_node, target_node, num_threads);
    } else if (num_obj == 5) {
        return make_ltmoa_parallel_solver<5>(graph, inv_graph, start_node, target_node, num_threads);
    }
    return nullptr;
}
