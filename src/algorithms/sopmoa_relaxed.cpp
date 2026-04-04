#include "algorithms/sopmoa_relaxed.h"

template<int N>
SOPMOA_relaxed<N>::SOPMOA_relaxed(
    AdjacencyMatrix &adj_matrix,
    AdjacencyMatrix &inv_graph,
    size_t start_node,
    size_t target_node,
    int num_threads
)
: AbstractSolver(adj_matrix, start_node, target_node),
heuristic(Heuristic<N>(target_node, inv_graph)),
num_threads(num_threads > 0 ? num_threads : std::max(1u, std::thread::hardware_concurrency())),
gcl_ptr(num_threads == 1 ? nullptr : std::make_unique<Gcl_relaxed<N, FRONTIER_HOT_CACHE_SIZE>>(adj_matrix.get_num_node())),
gcl_serial_ptr(num_threads == 1 ? std::make_unique<Gcl_relaxed_serial<N, FRONTIER_HOT_CACHE_SIZE>>(adj_matrix.get_num_node()) : nullptr) {
    build_packed_graph();
    heuristic_data = heuristic.raw_data();
    initialize_workers();
}

template<int N>
void SOPMOA_relaxed<N>::build_packed_graph() {
    const size_t num_nodes = static_cast<size_t>(graph.get_num_node());
    packed_graph.offsets.assign(num_nodes + 1, 0);

    size_t total_edges = 0;
    for (size_t node = 0; node < num_nodes; node++) {
        packed_graph.offsets[node] = total_edges;
        total_edges += graph[node].size();
    }
    packed_graph.offsets[num_nodes] = total_edges;

    packed_graph.tails.resize(total_edges);
    for (int dim = 0; dim < N; dim++) {
        packed_graph.costs[dim].resize(total_edges);
    }

    size_t edge_index = 0;
    for (size_t node = 0; node < num_nodes; node++) {
        const std::vector<Edge>& outgoing_edges = graph[node];
        for (const Edge& edge : outgoing_edges) {
            packed_graph.tails[edge_index] = static_cast<uint32_t>(edge.tail);
            for (int dim = 0; dim < N; dim++) {
                packed_graph.costs[dim][edge_index] = edge.cost[dim];
            }
            edge_index++;
        }
    }
}

template<int N>
void SOPMOA_relaxed<N>::initialize_workers() {
    workers.clear();
    workers.reserve(num_threads);
    for (int i = 0; i < num_threads; i++) {
        workers.push_back(std::make_unique<WorkerState>());
        workers.back()->hot_heap.reserve(256);
        workers.back()->spill_buffer.reserve(512);
        workers.back()->generated_batch.reserve(128);
        workers.back()->pop_batch.reserve(POP_BATCH);
    }
}

template<int N>
void SOPMOA_relaxed<N>::solve(unsigned int time_limit) {
    std::cout << "start SOPMOA_relaxed " + std::to_string(num_threads) + " threads\n";

    inflight_labels.store(0, std::memory_order_relaxed);
    stop_requested.store(false, std::memory_order_relaxed);
    target_epoch.store(0, std::memory_order_relaxed);
    total_donation_chunks_pushed = 0;
    total_donation_chunks_consumed = 0;
    total_spill_refills = 0;
    total_target_cache_hits = 0;
    total_target_cache_misses = 0;
    total_target_checks = 0;
    total_node_checks = 0;
    total_frontier_blocks_scanned = 0;
    total_frontier_live_entries_scanned = 0;
    total_frontier_dead_entries_encountered = 0;
    total_frontier_check_ns = 0;
    num_generation = 0;
    num_expansion = 0;
    solutions.clear();

    WorkChunk stale_chunk;
    while (donation_queue.try_pop(stale_chunk)) {}

    for (auto & worker : workers) {
        worker->hot_heap.clear();
        worker->spill_buffer.clear();
        worker->generated_batch.clear();
        worker->pop_batch.clear();
        worker->target_cache.epoch = 0;
        worker->target_cache.size = 0;
        worker->local_generated = 0;
        worker->local_expanded = 0;
        worker->local_donation_chunks_pushed = 0;
        worker->local_donation_chunks_consumed = 0;
        worker->local_spill_refills = 0;
        worker->local_target_cache_hits = 0;
        worker->local_target_cache_misses = 0;
        worker->local_target_checks = 0;
        worker->local_node_checks = 0;
        worker->local_frontier_blocks_scanned = 0;
        worker->local_frontier_live_entries_scanned = 0;
        worker->local_frontier_dead_entries_encountered = 0;
        worker->frontier_check_ns = 0;
    }

    search_start = std::chrono::steady_clock::now();

    RelaxedLabel* start_label = workers[0]->arena.create(
        static_cast<uint32_t>(start_node),
        CostVec<N>(heuristic_data[start_node]),
        static_cast<uint8_t>(0)
    );
    inflight_labels.store(1, std::memory_order_release);
    push_hot_label(0, start_label);

    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (int worker_id = 0; worker_id < num_threads; worker_id++) {
        threads.emplace_back([this, worker_id, time_limit]() {
            worker_loop(static_cast<size_t>(worker_id), time_limit);
        });
    }

    for (auto & thread : threads) {
        thread.join();
    }

    for (auto & worker : workers) {
        num_generation += worker->local_generated;
        num_expansion += worker->local_expanded;
        total_donation_chunks_pushed += worker->local_donation_chunks_pushed;
        total_donation_chunks_consumed += worker->local_donation_chunks_consumed;
        total_spill_refills += worker->local_spill_refills;
        total_target_cache_hits += worker->local_target_cache_hits;
        total_target_cache_misses += worker->local_target_cache_misses;
        total_target_checks += worker->local_target_checks;
        total_node_checks += worker->local_node_checks;
        total_frontier_blocks_scanned += worker->local_frontier_blocks_scanned;
        total_frontier_live_entries_scanned += worker->local_frontier_live_entries_scanned;
        total_frontier_dead_entries_encountered += worker->local_frontier_dead_entries_encountered;
        total_frontier_check_ns += worker->frontier_check_ns;
    }

    collect_final_solutions();

    std::cout << "Frontier check time: " << (double(total_frontier_check_ns) / 1e6) << " ms" << std::endl;
    std::cout << "Donation chunks pushed: " << total_donation_chunks_pushed << std::endl;
    std::cout << "Donation chunks consumed: " << total_donation_chunks_consumed << std::endl;
    std::cout << "Spill refills: " << total_spill_refills << std::endl;
    std::cout << "Target witness cache hits: " << total_target_cache_hits << std::endl;
    std::cout << "Target witness cache misses: " << total_target_cache_misses << std::endl;
    std::cout << "Target frontier checks: " << total_target_checks << std::endl;
    std::cout << "Node frontier checks: " << total_node_checks << std::endl;
    std::cout << "Frontier blocks scanned: " << total_frontier_blocks_scanned << std::endl;
    std::cout << "Frontier live entries scanned: " << total_frontier_live_entries_scanned << std::endl;
    std::cout << "Frontier dead entries encountered: " << total_frontier_dead_entries_encountered << std::endl;
    std::cout << "Average blocks per visited node: " << (
        gcl_serial_ptr ? gcl_serial_ptr->average_blocks_per_visited_node() : gcl_ptr->average_blocks_per_visited_node()
    ) << std::endl;
}

template<int N>
void SOPMOA_relaxed<N>::worker_loop(size_t worker_id, unsigned int time_limit) {
    WorkerState& worker = *workers[worker_id];
    LabelLexLarger heap_compare;

    while (true) {
        const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - search_start
        ).count();
        if (elapsed > static_cast<double>(time_limit)) {
            stop_requested.store(true, std::memory_order_release);
            break;
        }
        if (stop_requested.load(std::memory_order_acquire)) {
            break;
        }

        worker.pop_batch.clear();
        while (worker.pop_batch.size() < POP_BATCH && !worker.hot_heap.empty()) {
            std::pop_heap(worker.hot_heap.begin(), worker.hot_heap.end(), heap_compare);
            worker.pop_batch.push_back(worker.hot_heap.back());
            worker.hot_heap.pop_back();
        }

        if (worker.pop_batch.empty() && refill_hot_heap_from_spill(worker_id)) {
            while (worker.pop_batch.size() < POP_BATCH && !worker.hot_heap.empty()) {
                std::pop_heap(worker.hot_heap.begin(), worker.hot_heap.end(), heap_compare);
                worker.pop_batch.push_back(worker.hot_heap.back());
                worker.hot_heap.pop_back();
            }
        }

        if (worker.pop_batch.empty() && refill_hot_heap_from_donation(worker_id)) {
            while (worker.pop_batch.size() < POP_BATCH && !worker.hot_heap.empty()) {
                std::pop_heap(worker.hot_heap.begin(), worker.hot_heap.end(), heap_compare);
                worker.pop_batch.push_back(worker.hot_heap.back());
                worker.hot_heap.pop_back();
            }
        }

        if (worker.pop_batch.empty()) {
            if (inflight_labels.load(std::memory_order_acquire) == 0) {
                break;
            }
            std::this_thread::yield();
            continue;
        }

        for (RelaxedLabel* label : worker.pop_batch) {
            process_label(worker_id, label);
            if (stop_requested.load(std::memory_order_acquire)) {
                break;
            }
        }

        maybe_donate_work(worker_id);
    }
}

template<int N>
bool SOPMOA_relaxed<N>::refill_hot_heap_from_spill(size_t worker_id) {
    WorkerState& worker = *workers[worker_id];
    if (worker.spill_buffer.empty()) { return false; }

    const size_t old_size = worker.spill_buffer.size();
    const size_t take = std::min<size_t>(SPILL_REFILL_CHUNK, old_size);
    if (old_size > take) {
        std::nth_element(
            worker.spill_buffer.begin(),
            worker.spill_buffer.begin() + take,
            worker.spill_buffer.end(),
            [](const RelaxedLabel* lhs, const RelaxedLabel* rhs) {
                return label_lex_smaller(lhs, rhs);
            }
        );
    }

    for (size_t i = 0; i < take; i++) {
        worker.hot_heap.push_back(worker.spill_buffer[i]);
    }
    for (size_t i = 0; i < take; i++) {
        worker.spill_buffer[i] = worker.spill_buffer[old_size - take + i];
    }
    worker.spill_buffer.resize(old_size - take);

    std::make_heap(worker.hot_heap.begin(), worker.hot_heap.end(), LabelLexLarger());
    worker.local_spill_refills++;
    return true;
}

template<int N>
bool SOPMOA_relaxed<N>::refill_hot_heap_from_donation(size_t worker_id) {
    WorkChunk chunk;
    if (!donation_queue.try_pop(chunk)) {
        return false;
    }

    WorkerState& worker = *workers[worker_id];
    for (size_t i = 0; i < chunk.size; i++) {
        worker.hot_heap.push_back(chunk.labels[i]);
    }
    std::make_heap(worker.hot_heap.begin(), worker.hot_heap.end(), LabelLexLarger());
    worker.local_donation_chunks_consumed++;
    return true;
}

template<int N>
void SOPMOA_relaxed<N>::maybe_donate_work(size_t worker_id) {
    if (num_threads <= 1) { return; }

    WorkerState& worker = *workers[worker_id];
    if (worker.spill_buffer.size() < SPILL_DONATE_THRESHOLD) { return; }

    WorkChunk chunk;
    chunk.size = std::min<size_t>(DONATE_CHUNK, worker.spill_buffer.size());
    const size_t start = worker.spill_buffer.size() - chunk.size;
    for (size_t i = 0; i < chunk.size; i++) {
        chunk.labels[i] = worker.spill_buffer[start + i];
    }
    worker.spill_buffer.resize(start);
    donation_queue.push(chunk);
    worker.local_donation_chunks_pushed++;
}

template<int N>
void SOPMOA_relaxed<N>::push_hot_label(size_t worker_id, RelaxedLabel* label) {
    WorkerState& worker = *workers[worker_id];
    worker.hot_heap.push_back(label);
    std::push_heap(worker.hot_heap.begin(), worker.hot_heap.end(), LabelLexLarger());
}

template<int N>
void SOPMOA_relaxed<N>::process_label(size_t worker_id, RelaxedLabel* curr) {
    WorkerState& worker = *workers[worker_id];
    worker.local_generated++;

    if (curr->should_check_sol != 0 && target_dominated(worker_id, curr->f)) {
        inflight_labels.fetch_sub(1, std::memory_order_acq_rel);
        return;
    }
    if (node_dominated(worker_id, curr->node, curr->f)) {
        inflight_labels.fetch_sub(1, std::memory_order_acq_rel);
        return;
    }

    double time_found = -1.0;
    if (curr->node == target_node) {
        time_found = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - search_start
        ).count();
    }
    if (!frontier_update(curr->node, curr->f, time_found)) {
        inflight_labels.fetch_sub(1, std::memory_order_acq_rel);
        return;
    }

    worker.local_expanded++;

    if (curr->node == target_node) {
        inflight_labels.fetch_sub(1, std::memory_order_acq_rel);
        return;
    }

    worker.generated_batch.clear();

    const uint32_t curr_node = curr->node;
    const CostVec<N>& curr_h = heuristic_data[curr_node];
    const size_t edge_begin = packed_graph.offsets[curr_node];
    const size_t edge_end = packed_graph.offsets[curr_node + 1];
    for (size_t edge_idx = edge_begin; edge_idx < edge_end; edge_idx++) {
        const uint32_t succ_node = packed_graph.tails[edge_idx];
        const CostVec<N>& succ_h = heuristic_data[succ_node];

        CostVec<N> succ_f;
        for (int dim = 0; dim < N; dim++) {
            succ_f[dim] = curr->f[dim] + packed_graph.costs[dim][edge_idx] + succ_h[dim] - curr_h[dim];
        }

        const uint8_t should_check_sol = succ_f != curr->f ? 1 : 0;
        if (should_check_sol != 0 && target_dominated(worker_id, succ_f)) { continue; }
        if (node_dominated(worker_id, succ_node, succ_f)) { continue; }

        RelaxedLabel* succ = worker.arena.create(succ_node, succ_f, should_check_sol);
        worker.generated_batch.push_back(succ);
        inflight_labels.fetch_add(1, std::memory_order_acq_rel);
    }

    if (!worker.generated_batch.empty()) {
        const size_t desired_keep = num_threads == 1 ? LOCAL_KEEP : 1;
        const size_t keep = std::min<size_t>(desired_keep, worker.generated_batch.size());
        if (worker.generated_batch.size() > keep) {
            std::nth_element(
                worker.generated_batch.begin(),
                worker.generated_batch.begin() + keep,
                worker.generated_batch.end(),
                [](const RelaxedLabel* lhs, const RelaxedLabel* rhs) {
                    return label_lex_smaller(lhs, rhs);
                }
            );
        }

        for (size_t i = 0; i < keep; i++) {
            push_hot_label(worker_id, worker.generated_batch[i]);
        }
        for (size_t i = keep; i < worker.generated_batch.size(); i++) {
            worker.spill_buffer.push_back(worker.generated_batch[i]);
        }
    }

    maybe_donate_work(worker_id);
    inflight_labels.fetch_sub(1, std::memory_order_acq_rel);
}

template<int N>
bool SOPMOA_relaxed<N>::target_dominated(size_t worker_id, const CostVec<N>& cost) {
    WorkerState& worker = *workers[worker_id];
    worker.local_target_checks++;

    const size_t epoch = target_epoch.load(std::memory_order_acquire);
    if (worker.target_cache.epoch != epoch) {
        worker.target_cache.epoch = epoch;
        worker.target_cache.size = 0;
    }

    for (size_t i = 0; i < worker.target_cache.size; i++) {
        if (weakly_dominate<N>(worker.target_cache.costs[i], cost)) {
            worker.local_target_cache_hits++;
            return true;
        }
    }

    worker.local_target_cache_misses++;
    auto start_check = std::chrono::steady_clock::now();
    bool dominated = false;
    CostVec<N> witness_cost;
    if (gcl_serial_ptr) {
        typename Gcl_relaxed_serial<N, FRONTIER_HOT_CACHE_SIZE>::CheckStats stats;
        typename Gcl_relaxed_serial<N, FRONTIER_HOT_CACHE_SIZE>::SnapshotEntry witness;
        dominated = gcl_serial_ptr->frontier_check(target_node, cost, &stats, &witness);
        worker.local_frontier_blocks_scanned += stats.blocks_scanned;
        worker.local_frontier_live_entries_scanned += stats.live_entries_scanned;
        worker.local_frontier_dead_entries_encountered += stats.dead_entries_encountered;
        if (dominated) {
            witness_cost = witness.cost;
        }
    } else {
        typename Gcl_relaxed<N, FRONTIER_HOT_CACHE_SIZE>::CheckStats stats;
        typename Gcl_relaxed<N, FRONTIER_HOT_CACHE_SIZE>::SnapshotEntry witness;
        dominated = gcl_ptr->frontier_check(target_node, cost, &stats, &witness);
        worker.local_frontier_blocks_scanned += stats.blocks_scanned;
        worker.local_frontier_live_entries_scanned += stats.live_entries_scanned;
        worker.local_frontier_dead_entries_encountered += stats.dead_entries_encountered;
        if (dominated) {
            witness_cost = witness.cost;
        }
    }
    auto end_check = std::chrono::steady_clock::now();

    worker.frontier_check_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end_check - start_check).count();

    if (dominated) {
        if (worker.target_cache.size < TARGET_CACHE_SIZE) {
            worker.target_cache.costs[worker.target_cache.size++] = witness_cost;
        } else {
            for (size_t i = TARGET_CACHE_SIZE - 1; i > 0; i--) {
                worker.target_cache.costs[i] = worker.target_cache.costs[i - 1];
            }
            worker.target_cache.costs[0] = witness_cost;
        }
    }
    return dominated;
}

template<int N>
bool SOPMOA_relaxed<N>::node_dominated(size_t worker_id, uint32_t node, const CostVec<N>& cost) {
    WorkerState& worker = *workers[worker_id];
    worker.local_node_checks++;

    auto start_check = std::chrono::steady_clock::now();
    bool dominated = false;
    if (gcl_serial_ptr) {
        typename Gcl_relaxed_serial<N, FRONTIER_HOT_CACHE_SIZE>::CheckStats stats;
        dominated = gcl_serial_ptr->frontier_check(node, cost, &stats, nullptr);
        worker.local_frontier_blocks_scanned += stats.blocks_scanned;
        worker.local_frontier_live_entries_scanned += stats.live_entries_scanned;
        worker.local_frontier_dead_entries_encountered += stats.dead_entries_encountered;
    } else {
        typename Gcl_relaxed<N, FRONTIER_HOT_CACHE_SIZE>::CheckStats stats;
        dominated = gcl_ptr->frontier_check(node, cost, &stats, nullptr);
        worker.local_frontier_blocks_scanned += stats.blocks_scanned;
        worker.local_frontier_live_entries_scanned += stats.live_entries_scanned;
        worker.local_frontier_dead_entries_encountered += stats.dead_entries_encountered;
    }
    auto end_check = std::chrono::steady_clock::now();

    worker.frontier_check_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end_check - start_check).count();
    return dominated;
}

template<int N>
bool SOPMOA_relaxed<N>::frontier_update(uint32_t node, const CostVec<N>& cost, double time_found) {
    const bool updated = gcl_serial_ptr
        ? gcl_serial_ptr->frontier_update(node, cost, time_found)
        : gcl_ptr->frontier_update(node, cost, time_found);
    if (updated && node == target_node) {
        target_epoch.fetch_add(1, std::memory_order_acq_rel);
    }
    return updated;
}

template<int N>
void SOPMOA_relaxed<N>::collect_final_solutions() {
    solutions.clear();
    if (gcl_serial_ptr) {
        auto snapshot = gcl_serial_ptr->snapshot(target_node);
        solutions.reserve(snapshot->size());
        for (const auto & entry : *snapshot) {
            solutions.emplace_back(
                std::vector<cost_t>(entry.cost.begin(), entry.cost.end()),
                entry.time_found
            );
        }
        return;
    }

    auto snapshot = gcl_ptr->snapshot(target_node);
    solutions.reserve(snapshot->size());
    for (const auto & entry : *snapshot) {
        solutions.emplace_back(
            std::vector<cost_t>(entry.cost.begin(), entry.cost.end()),
            entry.time_found
        );
    }
}

template<int N>
std::shared_ptr<AbstractSolver> make_sopmoa_relaxed_solver(
    AdjacencyMatrix &graph,
    AdjacencyMatrix &inv_graph,
    size_t start_node,
    size_t target_node,
    int num_threads
) {
    return std::make_shared<SOPMOA_relaxed<N>>(graph, inv_graph, start_node, target_node, num_threads);
}

template class SOPMOA_relaxed<2>;
template class SOPMOA_relaxed<3>;
template class SOPMOA_relaxed<4>;
template class SOPMOA_relaxed<5>;

std::shared_ptr<AbstractSolver> get_SOPMOA_relaxed_solver(
    AdjacencyMatrix &graph, AdjacencyMatrix &inv_graph,
    size_t start_node, size_t target_node, int num_threads
) {
    int num_obj = graph.get_num_obj();
    if (num_obj == 2) {
        return make_sopmoa_relaxed_solver<2>(graph, inv_graph, start_node, target_node, num_threads);
    } else if (num_obj == 3) {
        return make_sopmoa_relaxed_solver<3>(graph, inv_graph, start_node, target_node, num_threads);
    } else if (num_obj == 4) {
        return make_sopmoa_relaxed_solver<4>(graph, inv_graph, start_node, target_node, num_threads);
    } else if (num_obj == 5) {
        return make_sopmoa_relaxed_solver<5>(graph, inv_graph, start_node, target_node, num_threads);
    } else {
        return nullptr;
    }
}
