#ifndef ALGORITHM_LTMOA_ARRAY_SUPERFAST
#define ALGORITHM_LTMOA_ARRAY_SUPERFAST

#include "../common_inc.h"
#include "../problem/graph.h"
#include "../problem/heuristic.h"
#include "../utils/cost.h"
#include "../utils/label.h"
#include "../utils/label_arena.h"
#include "abstract_solver.h"
#include "gcl/gcl_fast_array.h"
#include <unordered_set>

template <int N>
class LTMOA_array_superfast: public AbstractSolver {
public:
    LTMOA_array_superfast(AdjacencyMatrix &adj_matrix, AdjacencyMatrix &inv_graph, size_t start_node, size_t target_node)
    : AbstractSolver(adj_matrix, start_node, target_node),
    heuristic_(Heuristic<N>(target_node, inv_graph)),
    gcl_(FastGclArray<N-1>(adj_matrix.get_num_node())),
    warm_start_states_(adj_matrix.get_num_node()) {
        target_component_min_.fill(MAX_COST);
    }

    std::string get_name() override {
        return "LTMOA_array_superfast(" + std::to_string(N) + "obj)-" + gcl_.get_name();
    }

    bool supports_canonical_benchmark_output() const override { return true; }
    void solve(double time_limit = std::numeric_limits<double>::infinity()) override;

private:
    struct ExactTargetPoint {
        CostVec<N> cost{};
        double time_found_sec = -1.0;
    };

    struct AcceptTargetResult {
        bool accepted = false;
        bool first_incumbent = false;
        bool skyline_changed = false;
    };

    struct AnytimeEntry {
        Label<N>* label = nullptr;
        uint32_t revision = 0;
        uint8_t nondominated_against_incumbents = 0;
        uint64_t best_margin_sum = 0;

        struct larger_for_pq {
            bool operator()(const AnytimeEntry& lhs, const AnytimeEntry& rhs) const;
        };
    };

    struct ScalarWarmStartEntry {
        size_t node = 0;
        uint64_t g_scalar = 0;
        uint64_t f_scalar = 0;
        CostVec<N> g_vec{};

        struct larger_for_pq {
            bool operator()(const ScalarWarmStartEntry& lhs, const ScalarWarmStartEntry& rhs) const;
        };
    };

    struct ScalarWarmStartState {
        uint64_t g_scalar = std::numeric_limits<uint64_t>::max();
        CostVec<N> g_vec{};
        uint32_t stamp = 0;
    };

    using TargetSnapshot = std::vector<CostVec<N>>;

    template<bool EnableMetrics>
    void solve_impl(double time_limit);
    template<bool EnableMetrics>
    void maybe_run_deferred_seed_burst(
        double time_limit,
        double elapsed_sec,
        const BenchmarkClock::time_point& local_start_time
    );
    bool run_scalar_seed(
        const CostVec<N>& weights,
        double seed_deadline_sec,
        const BenchmarkClock::time_point& local_start_time,
        CostVec<N>& solution_cost
    );
    bool target_dominated(const CostVec<N>& cost) const;
    AcceptTargetResult accept_target(const CostVec<N>& cost, double elapsed_sec);
    void recompute_target_component_min() const;
    void materialize_target_frontier_export_cache() const;
    const std::vector<FrontierPoint>& target_frontier_export_cache() const;
    void rebuild_solutions_from_exact_target_frontier();
    AnytimeEntry make_anytime_entry(Label<N>* label) const;
    bool pop_lex_open(std::vector<Label<N>*>& lex_open, Label<N>*& label);
    bool pop_anytime_open(std::vector<AnytimeEntry>& anytime_open, Label<N>*& label);
    static std::vector<CostVec<N>> build_warm_start_weights();
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

    Heuristic<N> heuristic_;
    LabelArena<N> arena_;
    FastGclArray<N-1> gcl_;
    std::vector<ScalarWarmStartState> warm_start_states_;
    uint32_t warm_start_stamp_ = 0;
    std::unordered_set<Label<N>*> popped_labels_;
    uint32_t target_revision_ = 0;
    bool anytime_open_active_ = false;
    bool deferred_seed_attempted_ = false;
    uint8_t mixed_pop_counter_ = 0;
    std::vector<ExactTargetPoint> target_frontier_exact_;
    mutable std::vector<FrontierPoint> target_frontier_export_cache_;
    mutable bool target_frontier_export_cache_dirty_ = false;
    mutable CostVec<N> target_component_min_{};
    mutable bool target_component_min_dirty_ = false;

    std::vector<FrontierPoint> collect_final_frontier() const override {
        return sort_frontier_lexicographically(normalize_frontier(target_frontier_export_cache()));
    }
};

std::shared_ptr<AbstractSolver> get_LTMOA_array_superfast_solver(
    AdjacencyMatrix &graph, AdjacencyMatrix &inv_graph,
    size_t start_node, size_t target_node
);

#endif
