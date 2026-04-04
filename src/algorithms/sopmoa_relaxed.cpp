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
gcl_ptr(std::make_unique<Gcl_relaxed<N>>(adj_matrix.get_num_node())),
target_snapshot(std::make_shared<const typename Gcl_relaxed<N>::Snapshot>()) {
    initialize_workers();
}

template<int N>
void SOPMOA_relaxed<N>::initialize_workers() {
    workers.clear();
    workers.reserve(num_threads);
    for (int i = 0; i < num_threads; i++) {
        workers.push_back(std::make_unique<WorkerState>());
        workers.back()->heap.reserve(256);
    }
}

template<int N>
void SOPMOA_relaxed<N>::solve(unsigned int time_limit) {
    std::cout << "start SOPMOA_relaxed " + std::to_string(num_threads) + " threads\n";

    inflight_labels.store(0, std::memory_order_relaxed);
    stop_requested.store(false, std::memory_order_relaxed);
    total_steals = 0;
    total_frontier_check_ns = 0;
    solutions.clear();

    for (auto & worker : workers) {
        worker->heap.clear();
        worker->local_generated = 0;
        worker->local_expanded = 0;
        worker->local_steals = 0;
        worker->frontier_check_ns = 0;
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
        total_frontier_check_ns += worker->frontier_check_ns;
    }

    collect_final_solutions();

    std::cout << "Num steal: " << total_steals << std::endl;
    std::cout << "Frontier check time: " << (double(total_frontier_check_ns) / 1e6) << " ms" << std::endl;
}

template<int N>
void SOPMOA_relaxed<N>::worker_loop(size_t worker_id, unsigned int time_limit) {
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
            Label<N>* stolen = nullptr;
            if (steal_label(worker_id, rng, stolen)) {
                batch.push_back(stolen);
            } else {
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
        }
    }
}

template<int N>
void SOPMOA_relaxed<N>::process_label(size_t worker_id, Label<N>* curr, unsigned int time_limit) {
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

    auto curr_h = heuristic(curr->node);
    CostVec<N> curr_g;
    for (int i = 0; i < N; i++) {
        curr_g[i] = curr->f[i] - curr_h[i];
    }

    const std::vector<Edge> &outgoing_edges = graph[curr->node];
    for (auto it = outgoing_edges.begin(); it != outgoing_edges.end(); ++it) {
        size_t succ_node = it->tail;
        auto succ_h = heuristic(succ_node);
        CostVec<N> succ_f;
        for (int i = 0; i < N; i++) {
            succ_f[i] = curr_g[i] + it->cost[i] + succ_h[i];
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

    owner.inbox.push(label);
}

template<int N>
void SOPMOA_relaxed<N>::drain_inbox(size_t worker_id, size_t limit) {
    auto & worker = *workers[worker_id];
    std::vector<Label<N>*> inbox_batch;
    inbox_batch.reserve(limit);

    Label<N>* label = nullptr;
    while (inbox_batch.size() < limit && worker.inbox.try_pop(label)) {
        inbox_batch.push_back(label);
    }

    if (inbox_batch.empty()) { return; }

    std::lock_guard<std::mutex> guard(worker.heap_lock);
    for (auto * item : inbox_batch) {
        worker.heap.push_back(item);
        std::push_heap(worker.heap.begin(), worker.heap.end(), typename Label<N>::lex_larger_for_PQ());
    }
}

template<int N>
bool SOPMOA_relaxed<N>::pop_local(size_t worker_id, Label<N>* &label) {
    auto & worker = *workers[worker_id];
    std::lock_guard<std::mutex> guard(worker.heap_lock);
    if (worker.heap.empty()) { return false; }

    std::pop_heap(worker.heap.begin(), worker.heap.end(), typename Label<N>::lex_larger_for_PQ());
    label = worker.heap.back();
    worker.heap.pop_back();
    return true;
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
    auto snapshot = std::atomic_load_explicit(&target_snapshot, std::memory_order_acquire);
    bool dominated = gcl_ptr->snapshot_check(snapshot, cost);
    auto end_check = std::chrono::steady_clock::now();
    workers[worker_id]->frontier_check_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end_check - start_check).count();
    return dominated;
}

template<int N>
bool SOPMOA_relaxed<N>::node_dominated(size_t worker_id, size_t node, const CostVec<N>& cost) {
    auto start_check = std::chrono::steady_clock::now();
    bool dominated = gcl_ptr->frontier_check(node, cost);
    auto end_check = std::chrono::steady_clock::now();
    workers[worker_id]->frontier_check_ns += std::chrono::duration_cast<std::chrono::nanoseconds>(end_check - start_check).count();
    return dominated;
}

template<int N>
bool SOPMOA_relaxed<N>::frontier_update(size_t node, const CostVec<N>& cost, double time_found) {
    bool inserted = gcl_ptr->frontier_update(node, cost, time_found);
    if (inserted && node == target_node) {
        publish_target_snapshot();
    }
    return inserted;
}

template<int N>
void SOPMOA_relaxed<N>::publish_target_snapshot() {
    auto snapshot = gcl_ptr->snapshot(target_node);
    std::shared_ptr<const typename Gcl_relaxed<N>::Snapshot> const_snapshot(snapshot);
    std::atomic_store_explicit(&target_snapshot, const_snapshot, std::memory_order_release);
}

template<int N>
void SOPMOA_relaxed<N>::collect_final_solutions() {
    auto snapshot = gcl_ptr->snapshot(target_node);
    solutions.clear();
    solutions.reserve(snapshot->size());

    for (const auto & entry : *snapshot) {
        CostVec<N> cost = entry.to_cost();
        solutions.emplace_back(
            std::vector<cost_t>(cost.begin(), cost.end()),
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
