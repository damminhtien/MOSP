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
gcl_ptr(std::make_unique<Gcl_relaxed<N>>(adj_matrix.get_num_node())) {
    initialize_workers();
}

template<int N>
void SOPMOA_relaxed<N>::initialize_workers() {
    workers.clear();
    workers.reserve(num_threads);
    for (int i = 0; i < num_threads; i++) {
        workers.push_back(std::make_unique<WorkerState>());
        workers.back()->heap.reserve(256);
        workers.back()->pending_outboxes.resize(num_threads);
    }
}

template<int N>
void SOPMOA_relaxed<N>::solve(double time_limit) {
    std::cout << "start SOPMOA_relaxed " + std::to_string(num_threads) + " threads\n";

    inflight_labels.store(0, std::memory_order_relaxed);
    stop_requested.store(false, std::memory_order_relaxed);
    total_steals = 0;
    total_inbox_batch_flushes = 0;
    total_inbox_labels_flushed = 0;
    total_target_checks = 0;
    total_node_checks = 0;
    total_frontier_check_ns = 0;
    solutions.clear();

    for (auto & worker : workers) {
        worker->heap.clear();
        worker->local_generated = 0;
        worker->local_expanded = 0;
        worker->local_steals = 0;
        worker->local_inbox_batch_flushes = 0;
        worker->local_inbox_labels_flushed = 0;
        worker->local_target_checks = 0;
        worker->local_node_checks = 0;
        worker->frontier_check_ns = 0;
        for (auto & batch : worker->pending_outboxes) {
            batch.size = 0;
        }

        InboxBatch stale_batch;
        while (worker->inbox.try_pop(stale_batch)) {}
    }

    search_start = std::chrono::steady_clock::now();

    const size_t start_owner = owner_of(start_node);
    CostVec<N> start_g;
    start_g.fill(0);
    auto & start_h = heuristic(start_node);
    Label<N>* start_label = workers[start_owner]->arena.create(start_node, start_g, start_h);
    inflight_labels.store(1, std::memory_order_release);
    enqueue_label(start_owner, start_owner, start_label);

    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (int worker_id = 0; worker_id < num_threads; worker_id++) {
        threads.emplace_back([this, worker_id, time_limit]() {
            worker_loop(worker_id, time_limit);
        });
    }

    for (auto & thread : threads) {
        thread.join();
    }

    for (auto & worker : workers) {
        num_generation += worker->local_generated;
        num_expansion += worker->local_expanded;
        total_steals += worker->local_steals;
        total_inbox_batch_flushes += worker->local_inbox_batch_flushes;
        total_inbox_labels_flushed += worker->local_inbox_labels_flushed;
        total_target_checks += worker->local_target_checks;
        total_node_checks += worker->local_node_checks;
        total_frontier_check_ns += worker->frontier_check_ns;
    }

    collect_final_solutions();

    auto frontier_sizes = gcl_ptr->live_frontier_sizes();
    size_t median_frontier_size = 0;
    if (!frontier_sizes.empty()) {
        auto mid = frontier_sizes.begin() + frontier_sizes.size() / 2;
        std::nth_element(frontier_sizes.begin(), mid, frontier_sizes.end());
        median_frontier_size = *mid;
    }

    std::cout << "Num steal: " << total_steals << std::endl;
    std::cout << "Frontier check time: " << (double(total_frontier_check_ns) / 1e6) << " ms" << std::endl;
    std::cout << "Target frontier checks: " << total_target_checks << std::endl;
    std::cout << "Node frontier checks: " << total_node_checks << std::endl;
    std::cout << "Inbox batch flushes: " << total_inbox_batch_flushes << std::endl;
    std::cout << "Inbox labels flushed: " << total_inbox_labels_flushed << std::endl;
    std::cout << "Frontier compactions: " << gcl_ptr->get_compaction_count() << std::endl;
    std::cout << "Median frontier size: " << median_frontier_size << std::endl;
}

template<int N>
void SOPMOA_relaxed<N>::worker_loop(size_t worker_id, double time_limit) {
    std::minstd_rand rng(static_cast<unsigned int>(worker_id + 1));

    while (true) {
        if (stop_requested.load(std::memory_order_acquire)) {
            break;
        }

        const double elapsed = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - search_start
        ).count();
        if (elapsed > time_limit) {
            stop_requested.store(true, std::memory_order_release);
            break;
        }

        drain_inbox(worker_id);

        std::vector<Label<N>*> batch;
        batch.reserve(BATCH_SIZE);

        {
            auto & worker = *workers[worker_id];
            std::lock_guard<std::mutex> guard(worker.heap_lock);
            while (batch.size() < BATCH_SIZE && !worker.heap.empty()) {
                std::pop_heap(worker.heap.begin(), worker.heap.end(), typename Label<N>::lex_larger_for_PQ());
                batch.push_back(worker.heap.back());
                worker.heap.pop_back();
            }
        }

        if (batch.empty()) {
            flush_all_outboxes(worker_id);
            drain_inbox(worker_id);

            {
                auto & worker = *workers[worker_id];
                std::lock_guard<std::mutex> guard(worker.heap_lock);
                while (batch.size() < BATCH_SIZE && !worker.heap.empty()) {
                    std::pop_heap(worker.heap.begin(), worker.heap.end(), typename Label<N>::lex_larger_for_PQ());
                    batch.push_back(worker.heap.back());
                    worker.heap.pop_back();
                }
            }
        }

        if (batch.empty()) {
            flush_all_outboxes(worker_id);

            Label<N>* stolen = nullptr;
            if (steal_label(worker_id, rng, stolen)) {
                batch.push_back(stolen);
            } else {
                flush_all_outboxes(worker_id);
                if (inflight_labels.load(std::memory_order_acquire) == 0) {
                    break;
                }
                std::this_thread::yield();
                continue;
            }
        }

        for (auto * label : batch) {
            if (stop_requested.load(std::memory_order_acquire)) {
                break;
            }
            process_label(worker_id, label, time_limit);
            flush_all_outboxes(worker_id);
        }

        flush_all_outboxes(worker_id);
    }
}

template<int N>
void SOPMOA_relaxed<N>::process_label(size_t worker_id, Label<N>* curr, double time_limit) {
    workers[worker_id]->local_generated++;

    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - search_start
    ).count();
    if (elapsed > time_limit) {
        stop_requested.store(true, std::memory_order_release);
        return;
    }

    if (curr->should_check_sol && target_dominated(worker_id, curr->f)) {
        inflight_labels.fetch_sub(1, std::memory_order_acq_rel);
        return;
    }
    if (node_dominated(worker_id, curr->node, curr->f)) {
        inflight_labels.fetch_sub(1, std::memory_order_acq_rel);
        return;
    }
    if (!frontier_update(curr->node, curr->f, curr->node == target_node ? elapsed : -1.0)) {
        inflight_labels.fetch_sub(1, std::memory_order_acq_rel);
        return;
    }

    workers[worker_id]->local_expanded++;

    if (curr->node == target_node) {
        inflight_labels.fetch_sub(1, std::memory_order_acq_rel);
        return;
    }

    const CostVec<N>& curr_h = heuristic(curr->node);
    const std::vector<Edge> &outgoing_edges = graph[curr->node];
    for (auto it = outgoing_edges.begin(); it != outgoing_edges.end(); ++it) {
        const size_t succ_node = it->tail;
        const CostVec<N>& succ_h = heuristic(succ_node);
        CostVec<N> succ_f;
        for (int i = 0; i < N; i++) {
            succ_f[i] = curr->f[i] + it->cost[i] + succ_h[i] - curr_h[i];
        }

        const bool should_check_sol = succ_f != curr->f;
        if (should_check_sol && target_dominated(worker_id, succ_f)) { continue; }
        if (node_dominated(worker_id, succ_node, succ_f)) { continue; }

        Label<N>* succ = workers[worker_id]->arena.create(succ_node, succ_f);
        succ->should_check_sol = should_check_sol;

        inflight_labels.fetch_add(1, std::memory_order_acq_rel);
        enqueue_label(worker_id, owner_of(succ_node), succ);
    }

    inflight_labels.fetch_sub(1, std::memory_order_acq_rel);
}

template<int N>
void SOPMOA_relaxed<N>::enqueue_label(size_t worker_id, size_t owner_id, Label<N>* label) {
    auto & owner = *workers[owner_id];
    if (owner_id == worker_id) {
        std::lock_guard<std::mutex> guard(owner.heap_lock);
        owner.heap.push_back(label);
        std::push_heap(owner.heap.begin(), owner.heap.end(), typename Label<N>::lex_larger_for_PQ());
        return;
    }

    InboxBatch& pending = workers[worker_id]->pending_outboxes[owner_id];
    pending.labels[pending.size++] = label;
    if (pending.size == REMOTE_BATCH_SIZE) {
        flush_outbox(worker_id, owner_id);
    }
}

template<int N>
void SOPMOA_relaxed<N>::flush_outbox(size_t worker_id, size_t owner_id) {
    InboxBatch& pending = workers[worker_id]->pending_outboxes[owner_id];
    if (pending.size == 0) { return; }

    workers[owner_id]->inbox.push(pending);
    workers[worker_id]->local_inbox_batch_flushes++;
    workers[worker_id]->local_inbox_labels_flushed += pending.size;
    pending.size = 0;
}

template<int N>
void SOPMOA_relaxed<N>::flush_all_outboxes(size_t worker_id) {
    for (size_t owner_id = 0; owner_id < workers[worker_id]->pending_outboxes.size(); owner_id++) {
        if (owner_id == worker_id) { continue; }
        flush_outbox(worker_id, owner_id);
    }
}

template<int N>
void SOPMOA_relaxed<N>::drain_inbox(size_t worker_id, size_t limit) {
    auto & worker = *workers[worker_id];
    std::vector<Label<N>*> inbox_labels;
    inbox_labels.reserve(limit);

    InboxBatch batch;
    while (inbox_labels.size() < limit && worker.inbox.try_pop(batch)) {
        for (size_t i = 0; i < batch.size; i++) {
            inbox_labels.push_back(batch.labels[i]);
        }
    }

    if (inbox_labels.empty()) { return; }

    std::lock_guard<std::mutex> guard(worker.heap_lock);
    const size_t old_size = worker.heap.size();
    worker.heap.insert(worker.heap.end(), inbox_labels.begin(), inbox_labels.end());

    if (inbox_labels.size() <= 8) {
        for (size_t i = old_size; i < worker.heap.size(); i++) {
            std::push_heap(worker.heap.begin(), worker.heap.begin() + i + 1, typename Label<N>::lex_larger_for_PQ());
        }
        return;
    }

    std::make_heap(worker.heap.begin(), worker.heap.end(), typename Label<N>::lex_larger_for_PQ());
}

template<int N>
bool SOPMOA_relaxed<N>::steal_label(size_t worker_id, std::minstd_rand& rng, Label<N>* &label) {
    if (num_threads <= 1) { return false; }

    std::uniform_int_distribution<int> dist(0, num_threads - 1);
    size_t best_victim = workers.size();
    Label<N>* best_label = nullptr;

    for (size_t i = 0; i < STEAL_K; i++) {
        size_t victim_id = static_cast<size_t>(dist(rng));
        if (victim_id == worker_id) { continue; }

        auto & victim = *workers[victim_id];
        std::lock_guard<std::mutex> guard(victim.heap_lock);
        if (victim.heap.empty()) { continue; }

        Label<N>* candidate = victim.heap.front();
        if (best_label == nullptr || lex_smaller<N>(candidate->f, best_label->f)) {
            best_label = candidate;
            best_victim = victim_id;
        }
    }

    if (best_victim == workers.size()) { return false; }

    auto & victim = *workers[best_victim];
    std::lock_guard<std::mutex> guard(victim.heap_lock);
    if (victim.heap.empty()) { return false; }

    std::pop_heap(victim.heap.begin(), victim.heap.end(), typename Label<N>::lex_larger_for_PQ());
    label = victim.heap.back();
    victim.heap.pop_back();
    workers[worker_id]->local_steals++;
    return true;
}

template<int N>
bool SOPMOA_relaxed<N>::target_dominated(size_t worker_id, const CostVec<N>& cost) {
    auto start_check = std::chrono::steady_clock::now();
    workers[worker_id]->local_target_checks++;
    bool dominated = gcl_ptr->frontier_check(target_node, cost);
    auto end_check = std::chrono::steady_clock::now();
    workers[worker_id]->frontier_check_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end_check - start_check).count();
    return dominated;
}

template<int N>
bool SOPMOA_relaxed<N>::node_dominated(size_t worker_id, size_t node, const CostVec<N>& cost) {
    auto start_check = std::chrono::steady_clock::now();
    workers[worker_id]->local_node_checks++;
    bool dominated = gcl_ptr->frontier_check(node, cost);
    auto end_check = std::chrono::steady_clock::now();
    workers[worker_id]->frontier_check_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end_check - start_check).count();
    return dominated;
}

template<int N>
bool SOPMOA_relaxed<N>::frontier_update(size_t node, const CostVec<N>& cost, double time_found) {
    return gcl_ptr->frontier_update(node, cost, time_found);
}

template<int N>
void SOPMOA_relaxed<N>::collect_final_solutions() {
    auto snapshot = gcl_ptr->snapshot(target_node);
    solutions.clear();
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
