#ifndef ALGORITHM_LTMOA_PARALLEL
#define ALGORITHM_LTMOA_PARALLEL

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
#include "../utils/label_arena.h"
#include "abstract_solver.h"
#include "gcl/gcl_owner_sharded.h"

template <int N>
class LTMOA_parallel: public AbstractSolver {
public:
    LTMOA_parallel(AdjacencyMatrix &adj_matrix, AdjacencyMatrix &inv_graph, size_t start_node, size_t target_node, int num_threads);
    ~LTMOA_parallel() override = default;

    std::string get_name() override {
        return "LTMOA_parallel(" + std::to_string(N) + "obj|" + std::to_string(num_threads_) + "threads)-" + gcl_.get_name();
    }

    bool supports_canonical_benchmark_output() const override { return true; }
    void solve(double time_limit = std::numeric_limits<double>::infinity()) override;

private:
    static constexpr size_t REMOTE_BATCH_SIZE = 16;
    static constexpr size_t INBOX_DRAIN_LIMIT = 128;
    static constexpr size_t STEAL_K = 2;
    using TargetSnapshot = std::vector<CostVec<N>>;

    struct InboxBatch {
        std::array<Label<N>*, REMOTE_BATCH_SIZE> labels{};
        size_t size = 0;
    };

    struct WorkerState {
        std::vector<Label<N>*> heap;
        tbb::concurrent_queue<InboxBatch> inbox;
        std::vector<InboxBatch> pending_outboxes;
        std::mutex heap_lock;
        // A single thread may process a shard at a time, even when work is stolen.
        std::atomic<bool> processing_active{false};
        LabelArena<N> arena;
    };

    void sync_benchmark_recorder() override;

    void initialize_workers();
    void worker_loop(size_t worker_id, double time_limit);
    bool pop_local(size_t worker_id, Label<N>*& label);
    bool try_acquire_owner_shard(size_t owner_id);
    void release_owner_shard(size_t owner_id);
    void process_label(size_t worker_id, size_t frontier_owner_id, Label<N>* curr, double time_limit);
    void enqueue_label(size_t worker_id, size_t owner_id, Label<N>* label);
    void flush_outbox(size_t worker_id, size_t owner_id);
    void flush_all_outboxes(size_t worker_id);
    void drain_inbox(size_t worker_id, size_t limit = INBOX_DRAIN_LIMIT);
    bool steal_label(size_t worker_id, std::minstd_rand& rng, size_t& frontier_owner_id, Label<N>*& label);
    bool target_dominated(const CostVec<N>& cost);
    bool node_dominated(size_t frontier_owner_id, size_t node, const CostVec<N-1>& cost);
    bool accept_target(const CostVec<N>& cost, double elapsed_sec);
    void observe_peak(std::atomic<uint64_t>& peak, uint64_t value);
    CounterSet counter_snapshot() const;
    inline size_t owner_of(size_t node) const { return gcl_.owner_of(node); }

    std::vector<FrontierPoint> collect_final_frontier() const override {
        return sort_frontier_lexicographically(normalize_frontier(target_frontier_exact_));
    }

    Heuristic<N> heuristic_;
    int num_threads_;
    OwnerShardedGcl<N-1> gcl_;
    std::vector<std::unique_ptr<WorkerState>> workers_;
    std::atomic<size_t> inflight_labels_{0};
    std::atomic<size_t> queued_labels_{0};
    std::atomic<size_t> allocated_labels_{0};
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> timed_out_{false};
    std::atomic<uint64_t> generated_labels_total_{0};
    std::atomic<uint64_t> expanded_labels_total_{0};
    std::atomic<uint64_t> pruned_by_target_total_{0};
    std::atomic<uint64_t> pruned_by_node_total_{0};
    std::atomic<uint64_t> pruned_other_total_{0};
    std::atomic<uint64_t> target_hits_raw_total_{0};
    std::atomic<uint64_t> target_frontier_checks_total_{0};
    std::atomic<uint64_t> node_frontier_checks_total_{0};
    std::atomic<uint64_t> peak_open_size_{0};
    std::atomic<uint64_t> peak_live_labels_{0};
    std::chrono::steady_clock::time_point search_start_;
    mutable std::mutex benchmark_trace_lock_;
    mutable std::mutex target_frontier_lock_;
    std::vector<FrontierPoint> target_frontier_exact_;
    std::shared_ptr<const TargetSnapshot> published_target_snapshot_;
};

std::shared_ptr<AbstractSolver> get_LTMOA_parallel_solver(
    AdjacencyMatrix &graph, AdjacencyMatrix &inv_graph,
    size_t start_node, size_t target_node, int num_threads
);

#endif
