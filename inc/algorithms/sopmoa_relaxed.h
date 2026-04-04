#ifndef ALGORITHM_SOPMOA_RELAXED
#define ALGORITHM_SOPMOA_RELAXED

#include <atomic>
#include <mutex>
#include <random>
#include <thread>

#include <tbb/concurrent_queue.h>

#include "../common_inc.h"
#include "../problem/graph.h"
#include "../problem/heuristic.h"
#include "../utils/cost.h"
#include "../utils/label.h"
#include "abstract_solver.h"
#include "gcl/gcl_relaxed.h"

template <int N>
class SOPMOA_relaxed: public AbstractSolver {
public:
    Heuristic<N> heuristic;

    SOPMOA_relaxed(AdjacencyMatrix &adj_matrix, AdjacencyMatrix &inv_graph, size_t start_node, size_t target_node, int num_threads);
    ~SOPMOA_relaxed() override = default;

    std::string get_name() override {
        return "SOPMOA_relaxed(" + std::to_string(N) + "obj|" + std::to_string(num_threads) + "threads)-" + gcl_ptr->get_name();
    }

    void solve(unsigned int time_limit = UINT_MAX) override;

private:
    static constexpr size_t BATCH_SIZE = 8;
    static constexpr size_t STEAL_K = 2;
    static constexpr size_t REMOTE_BATCH_SIZE = 16;
    static constexpr size_t INBOX_DRAIN_LIMIT = 128;
    static constexpr size_t LABEL_BLOCK_SIZE = 4096;

    template<size_t BLOCK_SIZE>
    class LabelArena {
    public:
        Label<N>* create(size_t node, const CostVec<N>& f) {
            Label<N>* slot = next_slot();
            *slot = Label<N>(node, f);
            slot->parent = nullptr;
            slot->should_check_sol = false;
            slot->set_next(nullptr);
            return slot;
        }

        Label<N>* create(size_t node, CostVec<N> g, CostVec<N>& h) {
            Label<N>* slot = next_slot();
            *slot = Label<N>(node, g, h);
            slot->parent = nullptr;
            slot->should_check_sol = false;
            slot->set_next(nullptr);
            return slot;
        }

    private:
        Label<N>* next_slot() {
            if (blocks.empty() || next_index == BLOCK_SIZE) {
                blocks.push_back(std::unique_ptr<Label<N>[]>(new Label<N>[BLOCK_SIZE]));
                next_index = 0;
            }

            return &blocks.back()[next_index++];
        }

        std::vector<std::unique_ptr<Label<N>[]>> blocks;
        size_t next_index = BLOCK_SIZE;
    };

    struct InboxBatch {
        std::array<Label<N>*, REMOTE_BATCH_SIZE> labels{};
        size_t size = 0;
    };

    struct WorkerState {
        std::vector<Label<N>*> heap;
        tbb::concurrent_queue<InboxBatch> inbox;
        std::vector<InboxBatch> pending_outboxes;
        std::mutex heap_lock;
        LabelArena<LABEL_BLOCK_SIZE> arena;
        size_t local_generated = 0;
        size_t local_expanded = 0;
        size_t local_steals = 0;
        size_t local_inbox_batch_flushes = 0;
        size_t local_inbox_labels_flushed = 0;
        size_t local_target_checks = 0;
        size_t local_node_checks = 0;
        long long frontier_check_ns = 0;
    };

    int num_threads;
    std::unique_ptr<Gcl_relaxed<N>> gcl_ptr;
    std::vector<std::unique_ptr<WorkerState>> workers;
    std::atomic<size_t> inflight_labels{0};
    std::atomic<bool> stop_requested{false};
    size_t total_steals = 0;
    size_t total_inbox_batch_flushes = 0;
    size_t total_inbox_labels_flushed = 0;
    size_t total_target_checks = 0;
    size_t total_node_checks = 0;
    long long total_frontier_check_ns = 0;
    std::chrono::steady_clock::time_point search_start;

    inline size_t owner_of(size_t node) const { return node % num_threads; }

    void initialize_workers();
    void worker_loop(size_t worker_id, unsigned int time_limit);
    void process_label(size_t worker_id, Label<N>* curr, unsigned int time_limit);
    void enqueue_label(size_t worker_id, size_t owner_id, Label<N>* label);
    void flush_outbox(size_t worker_id, size_t owner_id);
    void flush_all_outboxes(size_t worker_id);
    void drain_inbox(size_t worker_id, size_t limit = INBOX_DRAIN_LIMIT);
    bool steal_label(size_t worker_id, std::minstd_rand& rng, Label<N>* &label);
    bool target_dominated(size_t worker_id, const CostVec<N>& cost);
    bool node_dominated(size_t worker_id, size_t node, const CostVec<N>& cost);
    bool frontier_update(size_t node, const CostVec<N>& cost, double time_found = -1.0);
    void collect_final_solutions();
};

std::shared_ptr<AbstractSolver> get_SOPMOA_relaxed_solver(
    AdjacencyMatrix &graph, AdjacencyMatrix &inv_graph,
    size_t start_node, size_t target_node, int num_threads
);

#endif
