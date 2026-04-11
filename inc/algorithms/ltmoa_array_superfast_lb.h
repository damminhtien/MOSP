#ifndef ALGORITHM_LTMOA_ARRAY_SUPERFAST_LB
#define ALGORITHM_LTMOA_ARRAY_SUPERFAST_LB

#include "../common_inc.h"
#include "../problem/graph.h"
#include "../problem/lower_bound_oracle.h"
#include "../utils/cost.h"
#include "../utils/label.h"
#include "../utils/label_arena.h"
#include "abstract_solver.h"
#include "gcl/gcl_fast_array.h"

template <int N>
class LTMOA_array_superfast_lb: public AbstractSolver {
public:
    LTMOA_array_superfast_lb(
        AdjacencyMatrix &adj_matrix,
        AdjacencyMatrix &inv_graph,
        size_t start_node,
        size_t target_node
    );

    std::string get_name() override {
        return "LTMOA_array_superfast_lb(" + std::to_string(N) + "obj)-" + gcl_.get_name();
    }

    bool supports_canonical_benchmark_output() const override { return true; }
    void solve(double time_limit = std::numeric_limits<double>::infinity()) override;

    static std::vector<CostVec<N>> build_warm_start_weights();

private:
    struct ExactTargetPoint {
        CostVec<N> cost{};
        double time_found_sec = -1.0;
    };

    struct OpenEntry {
        Label<N>* label = nullptr;
        double scalar_key = 0.0;

        struct larger_for_pq {
            bool operator()(const OpenEntry& lhs, const OpenEntry& rhs) const;
        };
    };

    struct ScalarOpenEntry {
        size_t node = 0;
        uint64_t g_scalar = 0;
        uint64_t f_scalar = 0;
        CostVec<N> g_vec{};

        struct larger_for_pq {
            bool operator()(const ScalarOpenEntry& lhs, const ScalarOpenEntry& rhs) const;
        };
    };

    template<bool EnableMetrics>
    void solve_impl(double time_limit);
    template<bool EnableMetrics>
    void warm_start(double warm_start_deadline_sec, const BenchmarkClock::time_point& local_start_time);

    bool run_scalar_warm_start(
        const CostVec<N>& weights,
        double warm_start_deadline_sec,
        const BenchmarkClock::time_point& local_start_time,
        CostVec<N>& solution_cost
    ) const;
    bool target_dominated(const CostVec<N>& cost) const;
    bool accept_target(const CostVec<N>& cost, double elapsed_sec);
    void materialize_target_frontier_export_cache() const;
    const std::vector<FrontierPoint>& target_frontier_export_cache() const;
    void rebuild_solutions_from_exact_target_frontier();
    double open_scalar_key(const CostVec<N>& f) const;

    static uint64_t weighted_dot(
        const CostVec<N>& weights,
        const CostVec<N>& values
    );
    static uint64_t weighted_dot_edge(
        const CostVec<N>& weights,
        const std::array<cost_t, Edge::MAX_NUM_OBJ>& values
    );
    static uint64_t guarded_add_u64(uint64_t lhs, uint64_t rhs);
    static bool lex_less(const CostVec<N>& lhs, const CostVec<N>& rhs);
    static bool better_scalar_state(
        uint64_t lhs_scalar,
        const CostVec<N>& lhs_vec,
        uint64_t rhs_scalar,
        const CostVec<N>& rhs_vec
    );

    LowerBoundOracle<N> lower_bound_;
    LabelArena<N> arena_;
    FastGclArray<N-1> gcl_;
    CostVec<N> start_scale_{};
    std::vector<ExactTargetPoint> target_frontier_exact_;
    mutable std::vector<FrontierPoint> target_frontier_export_cache_;
    mutable bool target_frontier_export_cache_dirty_ = false;

    std::vector<FrontierPoint> collect_final_frontier() const override {
        return sort_frontier_lexicographically(normalize_frontier(target_frontier_export_cache()));
    }
};

std::shared_ptr<AbstractSolver> get_LTMOA_array_superfast_lb_solver(
    AdjacencyMatrix &graph, AdjacencyMatrix &inv_graph,
    size_t start_node, size_t target_node
);

#endif
