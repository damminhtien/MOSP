#ifndef ALGORITHM_SOPMOA_RELAXED
#define ALGORITHM_SOPMOA_RELAXED

#include <atomic>
#include <thread>

#include <tbb/concurrent_queue.h>

#include "../common_inc.h"
#include "../problem/graph.h"
#include "../problem/heuristic.h"
#include "../utils/cost.h"
#include "abstract_solver.h"
#include "gcl/gcl_relaxed.h"
#include "gcl/gcl_relaxed_serial.h"

template <int N>
class SOPMOA_relaxed: public AbstractSolver {
public:
    Heuristic<N> heuristic;

    SOPMOA_relaxed(AdjacencyMatrix &adj_matrix, AdjacencyMatrix &inv_graph, size_t start_node, size_t target_node, int num_threads);
    ~SOPMOA_relaxed() override = default;

    std::string get_name() override {
        const std::string frontier_name = gcl_serial_ptr ? gcl_serial_ptr->get_name() : gcl_ptr->get_name();
        return "SOPMOA_relaxed(" + std::to_string(N) + "obj|" + std::to_string(num_threads) + "threads)-" + frontier_name;
    }

    void solve(unsigned int time_limit = UINT_MAX) override;

private:
    static constexpr size_t POP_BATCH = 32;
    static constexpr size_t LOCAL_KEEP = 8;
    static constexpr size_t DONATE_CHUNK = 32;
    static constexpr size_t SPILL_DONATE_THRESHOLD = 8;
    static constexpr size_t SPILL_REFILL_CHUNK = 32;
    static constexpr size_t LABEL_BLOCK_SIZE = 4096;
    static constexpr size_t TARGET_CACHE_SIZE = 8;
    static constexpr size_t FRONTIER_HOT_CACHE_SIZE = 8;

    struct RelaxedLabel {
        uint32_t node = 0;
        CostVec<N> f{};
        uint8_t should_check_sol = 0;
    };

    template<typename T, size_t BLOCK_SIZE>
    class Arena {
    public:
        template<typename... Args>
        T* create(Args&&... args) {
            T* slot = next_slot();
            *slot = T{std::forward<Args>(args)...};
            return slot;
        }

    private:
        T* next_slot() {
            if (blocks.empty() || next_index == BLOCK_SIZE) {
                blocks.push_back(std::unique_ptr<T[]>(new T[BLOCK_SIZE]));
                next_index = 0;
            }
            return &blocks.back()[next_index++];
        }

        std::vector<std::unique_ptr<T[]>> blocks;
        size_t next_index = BLOCK_SIZE;
    };

    struct WorkChunk {
        std::array<RelaxedLabel*, DONATE_CHUNK> labels{};
        size_t size = 0;
    };

    struct TargetWitnessCache {
        size_t epoch = 0;
        size_t size = 0;
        std::array<CostVec<N>, TARGET_CACHE_SIZE> costs{};
    };

    struct WorkerState {
        std::vector<RelaxedLabel*> hot_heap;
        std::vector<RelaxedLabel*> spill_buffer;
        std::vector<RelaxedLabel*> generated_batch;
        std::vector<RelaxedLabel*> pop_batch;
        Arena<RelaxedLabel, LABEL_BLOCK_SIZE> arena;
        TargetWitnessCache target_cache;
        size_t local_generated = 0;
        size_t local_expanded = 0;
        size_t local_donation_chunks_pushed = 0;
        size_t local_donation_chunks_consumed = 0;
        size_t local_spill_refills = 0;
        size_t local_target_cache_hits = 0;
        size_t local_target_cache_misses = 0;
        size_t local_target_checks = 0;
        size_t local_node_checks = 0;
        size_t local_frontier_blocks_scanned = 0;
        size_t local_frontier_live_entries_scanned = 0;
        size_t local_frontier_dead_entries_encountered = 0;
        long long frontier_check_ns = 0;
    };

    struct PackedGraph {
        std::vector<size_t> offsets;
        std::vector<uint32_t> tails;
        std::array<std::vector<cost_t>, N> costs;
    };

    struct LabelLexLarger {
        bool operator()(const RelaxedLabel* a, const RelaxedLabel* b) const {
            for (int i = 0; i < N; i++) {
                if (a->f[i] != b->f[i]) { return a->f[i] > b->f[i]; }
            }
            return false;
        }
    };

    static inline bool label_lex_smaller(const RelaxedLabel* a, const RelaxedLabel* b) {
        return lex_smaller<N>(a->f, b->f);
    }

    int num_threads;
    std::unique_ptr<Gcl_relaxed<N, FRONTIER_HOT_CACHE_SIZE>> gcl_ptr;
    std::unique_ptr<Gcl_relaxed_serial<N, FRONTIER_HOT_CACHE_SIZE>> gcl_serial_ptr;
    PackedGraph packed_graph;
    const CostVec<N>* heuristic_data = nullptr;
    std::vector<std::unique_ptr<WorkerState>> workers;
    tbb::concurrent_queue<WorkChunk> donation_queue;
    std::atomic<size_t> inflight_labels{0};
    std::atomic<bool> stop_requested{false};
    std::atomic<size_t> target_epoch{0};
    size_t total_donation_chunks_pushed = 0;
    size_t total_donation_chunks_consumed = 0;
    size_t total_spill_refills = 0;
    size_t total_target_cache_hits = 0;
    size_t total_target_cache_misses = 0;
    size_t total_target_checks = 0;
    size_t total_node_checks = 0;
    size_t total_frontier_blocks_scanned = 0;
    size_t total_frontier_live_entries_scanned = 0;
    size_t total_frontier_dead_entries_encountered = 0;
    long long total_frontier_check_ns = 0;
    std::chrono::steady_clock::time_point search_start;

    void build_packed_graph();
    void initialize_workers();
    void worker_loop(size_t worker_id, unsigned int time_limit);
    bool refill_hot_heap_from_spill(size_t worker_id);
    bool refill_hot_heap_from_donation(size_t worker_id);
    void maybe_donate_work(size_t worker_id);
    void push_hot_label(size_t worker_id, RelaxedLabel* label);
    void process_label(size_t worker_id, RelaxedLabel* curr);
    bool target_dominated(size_t worker_id, const CostVec<N>& cost);
    bool node_dominated(size_t worker_id, uint32_t node, const CostVec<N>& cost);
    bool frontier_update(uint32_t node, const CostVec<N>& cost, double time_found = -1.0);
    void collect_final_solutions();
};

std::shared_ptr<AbstractSolver> get_SOPMOA_relaxed_solver(
    AdjacencyMatrix &graph, AdjacencyMatrix &inv_graph,
    size_t start_node, size_t target_node, int num_threads
);

#endif
