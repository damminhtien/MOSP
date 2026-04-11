#ifndef PROBLEM_LOWER_BOUND_ORACLE
#define PROBLEM_LOWER_BOUND_ORACLE

#include "../common_inc.h"
#include "../utils/cost.h"
#include "graph.h"

template<int N>
class LowerBoundOracle {
public:
    static constexpr size_t NUM_LANDMARKS = 6;

    LowerBoundOracle(
        AdjacencyMatrix& graph,
        AdjacencyMatrix& inv_graph,
        size_t start_node,
        size_t target_node
    );

    const CostVec<N>& operator()(size_t node_id) const {
        return combined_bound_[node_id];
    }

    const CostVec<N>& base_bound(size_t node_id) const {
        return base_bound_[node_id];
    }

    const std::vector<size_t>& landmark_nodes() const {
        return landmarks_;
    }

private:
    struct LandmarkDistances {
        std::array<std::vector<cost_t>, N> from_landmark;
        std::array<std::vector<cost_t>, N> to_landmark;
    };

    static cost_t guarded_add(cost_t lhs, cost_t rhs);
    static cost_t guarded_diff(cost_t lhs, cost_t rhs);
    static cost_t max_finite(cost_t lhs, cost_t rhs);
    static std::vector<cost_t> dijkstra_single_objective(
        size_t source,
        size_t cost_idx,
        const AdjacencyMatrix& adj_matrix
    );

    void build_base_bound(const AdjacencyMatrix& inv_graph, size_t target_node);
    void select_landmarks(
        const AdjacencyMatrix& graph,
        const AdjacencyMatrix& inv_graph,
        size_t start_node,
        size_t target_node
    );
    void precompute_landmark_tables(
        const AdjacencyMatrix& graph,
        const AdjacencyMatrix& inv_graph
    );
    void build_combined_bounds(size_t target_node);

    std::vector<CostVec<N>> base_bound_;
    std::vector<CostVec<N>> combined_bound_;
    std::vector<size_t> landmarks_;
    std::vector<LandmarkDistances> landmark_tables_;
};

#endif
