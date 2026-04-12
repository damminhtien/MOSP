#include <algorithm>
#include <cassert>
#include <memory>
#include <vector>

#include "algorithms/hybrid_corridor_pulsea.h"
#include "problem/graph.h"
#include "problem/heuristic.h"

namespace {

std::array<cost_t, Edge::MAX_NUM_OBJ> edge_costs(const std::vector<cost_t>& values) {
    std::array<cost_t, Edge::MAX_NUM_OBJ> result{};
    result.fill(0);
    for (size_t idx = 0; idx < values.size(); idx++) {
        result[idx] = values[idx];
    }
    return result;
}

struct GraphPair {
    AdjacencyMatrix graph;
    AdjacencyMatrix inv_graph;
};

GraphPair make_spillback_graph() {
    std::vector<Edge> edges;
    edges.emplace_back(0, 1, edge_costs({1, 1}));
    edges.emplace_back(1, 4, edge_costs({3, 6}));

    edges.emplace_back(0, 2, edge_costs({1, 1}));
    edges.emplace_back(2, 4, edge_costs({4, 4}));
    edges.emplace_back(2, 4, edge_costs({6, 3}));

    AdjacencyMatrix graph(5, 2, edges);
    AdjacencyMatrix inv_graph(5, 2, edges, true);
    return {std::move(graph), std::move(inv_graph)};
}

GraphPair make_corridor_fixture_graph() {
    std::vector<Edge> edges;
    edges.emplace_back(0, 1, edge_costs({2, 2}));
    edges.emplace_back(1, 4, edge_costs({3, 3}));

    edges.emplace_back(0, 2, edge_costs({6, 1}));
    edges.emplace_back(2, 4, edge_costs({0, 5}));

    edges.emplace_back(0, 3, edge_costs({4, 1}));
    edges.emplace_back(3, 4, edge_costs({0, 5}));

    AdjacencyMatrix graph(5, 2, edges);
    AdjacencyMatrix inv_graph(5, 2, edges, true);
    return {std::move(graph), std::move(inv_graph)};
}

GraphPair make_preferred_spillback_graph() {
    std::vector<Edge> edges;
    edges.emplace_back(0, 1, edge_costs({1, 10}));
    edges.emplace_back(1, 5, edge_costs({1, 0}));

    edges.emplace_back(0, 2, edge_costs({10, 1}));
    edges.emplace_back(2, 5, edge_costs({0, 1}));

    edges.emplace_back(0, 3, edge_costs({2, 2}));
    edges.emplace_back(3, 4, edge_costs({2, 2}));
    edges.emplace_back(4, 5, edge_costs({2, 2}));

    AdjacencyMatrix graph(6, 2, edges);
    AdjacencyMatrix inv_graph(6, 2, edges, true);
    return {std::move(graph), std::move(inv_graph)};
}

std::vector<std::vector<cost_t>> normalized_solution_costs(const std::shared_ptr<AbstractSolver>& solver) {
    std::vector<std::vector<cost_t>> costs;
    costs.reserve(solver->solutions.size());
    for (const auto& solution : solver->solutions) {
        costs.push_back(solution.cost);
    }
    std::sort(costs.begin(), costs.end());
    return costs;
}

template<size_t N>
CostVec<N> add_costs(const CostVec<N>& lhs, const CostVec<N>& rhs) {
    CostVec<N> result{};
    for (size_t idx = 0; idx < N; idx++) {
        result[idx] = lhs[idx] + rhs[idx];
    }
    return result;
}

template<size_t N>
bool skyline_blocks(const std::vector<CostVec<N>>& skyline, const CostVec<N>& optimistic_total) {
    for (const auto& incumbent : skyline) {
        if (weakly_dominate<N>(incumbent, optimistic_total)) {
            return true;
        }
    }
    return false;
}

void test_tiny_burst_spillback_exactness() {
    GraphPair graphs = make_spillback_graph();
    HybridCorridorPulseA<2>::Config config;
    config.max_burst_depth = 1;
    config.max_burst_labels = 1;
    config.max_burst_branching = 1;
    config.seed_fraction = 0.05;
    config.seed_cap_sec = 0.010;

    std::shared_ptr<AbstractSolver> solver = std::make_shared<HybridCorridorPulseA<2>>(
        graphs.graph,
        graphs.inv_graph,
        0,
        4,
        config
    );
    solver->solve(1.0);

    const std::vector<std::vector<cost_t>> expected = {
        {4, 7},
        {5, 5},
        {7, 4},
    };
    assert(normalized_solution_costs(solver) == expected);
}

void test_corridor_fixture_rule() {
    GraphPair graphs = make_corridor_fixture_graph();
    Heuristic<2> heuristic(4, graphs.inv_graph);

    const CostVec<2> dominated_prefix = {6, 1};
    const CostVec<2> survivor_prefix = {4, 1};
    const CostVec<2> dominated_optimistic = add_costs(dominated_prefix, heuristic(2));
    const CostVec<2> survivor_optimistic = add_costs(survivor_prefix, heuristic(3));
    const std::vector<CostVec<2>> incumbent_skyline = {
        CostVec<2>{5, 5},
    };

    assert(dominated_optimistic == CostVec<2>({6, 6}));
    assert(survivor_optimistic == CostVec<2>({4, 6}));
    assert(skyline_blocks(incumbent_skyline, dominated_optimistic));
    assert(!skyline_blocks(incumbent_skyline, survivor_optimistic));

    std::shared_ptr<AbstractSolver> solver = std::make_shared<HybridCorridorPulseA<2>>(
        graphs.graph,
        graphs.inv_graph,
        0,
        4
    );
    solver->solve(1.0);

    const std::vector<std::vector<cost_t>> expected = {
        {4, 6},
        {5, 5},
    };
    assert(normalized_solution_costs(solver) == expected);
}

void test_preferred_child_spillback_exactness() {
    GraphPair graphs = make_preferred_spillback_graph();
    HybridCorridorPulseA<2>::Config config;
    config.max_burst_depth = 8;
    config.max_burst_labels = 8;
    config.max_burst_branching = 1;
    config.seed_fraction = 0.02;
    config.seed_cap_sec = 0.010;

    std::shared_ptr<AbstractSolver> solver = std::make_shared<HybridCorridorPulseA<2>>(
        graphs.graph,
        graphs.inv_graph,
        0,
        5,
        config
    );
    solver->solve(1.0);

    const std::vector<std::vector<cost_t>> expected = {
        {2, 10},
        {6, 6},
        {10, 2},
    };
    assert(normalized_solution_costs(solver) == expected);
}

}  // namespace

int main() {
    test_tiny_burst_spillback_exactness();
    test_corridor_fixture_rule();
    test_preferred_child_spillback_exactness();
    return 0;
}
