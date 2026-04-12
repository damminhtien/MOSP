#ifndef ALGORITHM_HYBRID_CORRIDOR_PULSEA
#define ALGORITHM_HYBRID_CORRIDOR_PULSEA

#include <memory_resource>

#include "../common_inc.h"
#include "../problem/graph.h"
#include "../problem/heuristic.h"
#include "../utils/cost.h"
#include "../utils/label.h"
#include "../utils/label_arena.h"
#include "abstract_solver.h"
#include "gcl/gcl_fast_array.h"

template <int N>
class HybridCorridorPulseA: public AbstractSolver {
public:
    struct Config {
        size_t max_burst_depth = 8;
        size_t max_burst_labels = 32;
        size_t max_burst_branching = 1;
        double seed_fraction = 0.02;
        double seed_cap_sec = 0.010;
    };

    HybridCorridorPulseA(
        AdjacencyMatrix &adj_matrix,
        AdjacencyMatrix &inv_graph,
        size_t start_node,
        size_t target_node,
        Config config = {}
    );

    std::string get_name() override {
        return "HybridCorridorPulseA(" + std::to_string(N) + "obj)-" + gcl_.get_name();
    }

    bool supports_canonical_benchmark_output() const override { return true; }
    void solve(double time_limit = std::numeric_limits<double>::infinity()) override;

private:
    static constexpr size_t LABEL_RESOURCE_INITIAL_BYTES = 4 * 1024 * 1024;

    struct ExactTargetPoint {
        CostVec<N> cost{};
        double time_found_sec = -1.0;
    };

    struct ScalarSeedEntry {
        size_t node = 0;
        uint64_t g_scalar = 0;
        uint64_t f_scalar = 0;
        CostVec<N> g_vec{};

        struct larger_for_pq {
            bool operator()(const ScalarSeedEntry& lhs, const ScalarSeedEntry& rhs) const;
        };
    };

    struct LexSeedEntry {
        size_t node = 0;
        CostVec<N> g_vec{};
        CostVec<N> f_vec{};

        struct larger_for_pq {
            bool operator()(const LexSeedEntry& lhs, const LexSeedEntry& rhs) const;
        };
    };

    struct ScalarSeedState {
        uint64_t g_scalar = std::numeric_limits<uint64_t>::max();
        CostVec<N> g_vec{};
        uint32_t stamp = 0;
    };

    struct BurstFrame {
        Label<N>* label = nullptr;
        size_t depth = 0;
    };

    template<bool EnableMetrics>
    void solve_impl(double time_limit);
    template<bool EnableMetrics>
    void seed_incumbents(double seed_deadline_sec, const BenchmarkClock::time_point& local_start_time);
    bool run_scalar_seed(
        const CostVec<N>& weights,
        double seed_deadline_sec,
        const BenchmarkClock::time_point& local_start_time,
        CostVec<N>& solution_cost
    );
    bool run_lex_seed(
        double seed_deadline_sec,
        const BenchmarkClock::time_point& local_start_time,
        CostVec<N>& solution_cost
    );
    bool target_dominated(const CostVec<N>& cost) const;
    bool accept_target(const CostVec<N>& cost, double elapsed_sec);
    void recompute_target_component_min() const;
    void materialize_target_frontier_export_cache() const;
    const std::vector<FrontierPoint>& target_frontier_export_cache() const;
    void rebuild_solutions_from_exact_target_frontier();
    double seed_deadline_sec(double time_limit) const;
    bool burst_allowed_for_child(
        const Label<N>* child,
        const std::pmr::vector<Label<N>*>& global_open,
        size_t depth,
        size_t burst_processed
    ) const;

    static std::vector<CostVec<N>> build_seed_weights();
    static uint64_t guarded_add_u64(uint64_t lhs, uint64_t rhs);
    static uint64_t weighted_dot(const CostVec<N>& weights, const CostVec<N>& values);
    static uint64_t weighted_dot_edge(const CostVec<N>& weights, const std::array<cost_t, Edge::MAX_NUM_OBJ>& values);
    static bool lex_less(const CostVec<N>& lhs, const CostVec<N>& rhs);
    static bool better_scalar_state(
        uint64_t lhs_scalar,
        const CostVec<N>& lhs_vec,
        uint64_t rhs_scalar,
        const CostVec<N>& rhs_vec
    );

    Config config_;
    Heuristic<N> heuristic_;
    std::array<std::byte, LABEL_RESOURCE_INITIAL_BYTES> label_resource_buffer_{};
    std::pmr::monotonic_buffer_resource label_resource_;
    std::pmr::unsynchronized_pool_resource scratch_resource_;
    PmrLabelArena<N> arena_;
    FastGclArray<N-1> gcl_;
    std::vector<ScalarSeedState> scalar_seed_states_;
    uint32_t scalar_seed_stamp_ = 0;
    std::vector<ExactTargetPoint> target_frontier_exact_;
    mutable std::vector<FrontierPoint> target_frontier_export_cache_;
    mutable bool target_frontier_export_cache_dirty_ = false;
    mutable CostVec<N> target_component_min_{};
    mutable bool target_component_min_dirty_ = false;

    std::vector<FrontierPoint> collect_final_frontier() const override {
        return sort_frontier_lexicographically(normalize_frontier(target_frontier_export_cache()));
    }
};

std::shared_ptr<AbstractSolver> get_HybridCorridorPulseA_solver(
    AdjacencyMatrix &graph, AdjacencyMatrix &inv_graph,
    size_t start_node, size_t target_node
);

#endif
