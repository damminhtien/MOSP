#ifndef ALGORITHM_SOPMOA
#define ALGORITHM_SOPMOA

#include <atomic>
#include <tbb/concurrent_priority_queue.h>
#include <tbb/concurrent_vector.h>

#include"../common_inc.h"
#include"../problem/graph.h"
#include"../problem/heuristic.h"
#include"../utils/cost.h"
#include"../utils/label.h"
#include"../utils/thread_config.h"
#include"abstract_solver.h"
#include"gcl/gcl_sopmoa.h"

template <int N>
class SOPMOA: public AbstractSolver {
public:
    Heuristic<N> heuristic;

    SOPMOA(AdjacencyMatrix &adj_matrix, AdjacencyMatrix &inv_graph, size_t start_node, size_t target_node, int num_threads)
    : AbstractSolver(adj_matrix, start_node, target_node),
    gcl_ptr(std::make_unique<Gcl_SOPMOA<N>>(adj_matrix.get_num_node())),
    heuristic(Heuristic<N>(target_node, inv_graph)), 
    num_threads(resolve_parallel_worker_threads(num_threads)),
    is_thread_activating(static_cast<size_t>(resolve_parallel_worker_threads(num_threads))) {}

    ~SOPMOA() {
        for (auto ptr : all_labels){ delete ptr; }
        gcl_ptr.reset();
    }

    virtual std::string get_name() override {return "SOPMOA(" + std::to_string(N)+"obj|"+ std::to_string(num_threads)+"threads)-" + gcl_ptr->get_name(); }
    bool supports_canonical_benchmark_output() const override { return true; }
    void solve(double time_limit = std::numeric_limits<double>::infinity()) override;

private:
    int num_threads;
    std::list<Label<N>*> all_labels;
    std::unique_ptr<Gcl_SOPMOA<N>> gcl_ptr;
    tbb::concurrent_priority_queue<Label<N>*, typename Label<N>::lex_larger_for_PQ> open;
    std::mutex all_labels_lock;
    mutable std::mutex benchmark_trace_lock;
    std::vector<std::atomic<bool>> is_thread_activating;
    std::vector<FrontierPoint> target_frontier_;
    std::atomic<bool> timed_out{false};
    std::atomic<uint64_t> generated_labels_total_{0};
    std::atomic<uint64_t> expanded_labels_total_{0};
    std::atomic<uint64_t> pruned_by_target_total_{0};
    std::atomic<uint64_t> pruned_by_node_total_{0};
    std::atomic<uint64_t> pruned_other_total_{0};
    std::atomic<uint64_t> target_hits_raw_total_{0};
    std::atomic<uint64_t> target_frontier_checks_total_{0};
    std::atomic<uint64_t> node_frontier_checks_total_{0};
    std::atomic<uint64_t> queued_labels{0};
    std::atomic<uint64_t> peak_open_size_{0};
    std::atomic<uint64_t> allocated_labels{0};
    std::atomic<uint64_t> peak_live_labels_{0};
    std::atomic<uint64_t> next_trace_sample_ms_{0};
    BenchmarkClock::time_point search_start_{};

    virtual void thread_solve(int thread_ID, double time_limit);
    bool any_thread_active() const;
    double elapsed_sec() const;
    void observe_peak(std::atomic<uint64_t>& peak, uint64_t value);
    CounterSet counter_snapshot() const;
    void maybe_record_interval_sample(double elapsed_sec);
    std::vector<FrontierPoint> snapshot_target_frontier() const;
    void sync_benchmark_recorder() override {
        benchmark_recorder().set_counters(counter_snapshot());
    }

    std::vector<FrontierPoint> collect_final_frontier() const override {
        return snapshot_target_frontier();
    }
};

std::shared_ptr<AbstractSolver> get_SOPMOA_solver(
    AdjacencyMatrix &graph, AdjacencyMatrix &inv_graph,
    size_t start_node, size_t target_node, int num_threads
);

#endif
