#include "problem/lower_bound_oracle.h"

#include <queue>

namespace {

struct DijkstraNode {
    size_t idx = 0;
    cost_t distance = 0;

    struct larger_for_pq {
        bool operator()(const DijkstraNode& lhs, const DijkstraNode& rhs) const {
            if (lhs.distance != rhs.distance) {
                return lhs.distance > rhs.distance;
            }
            return lhs.idx > rhs.idx;
        }
    };
};

}  // namespace

template<int N>
LowerBoundOracle<N>::LowerBoundOracle(
    AdjacencyMatrix& graph,
    AdjacencyMatrix& inv_graph,
    size_t start_node,
    size_t target_node
)
: base_bound_(graph.get_num_node()),
  combined_bound_(graph.get_num_node()) {
    for (auto& bound : base_bound_) {
        bound.fill(MAX_COST);
    }
    for (auto& bound : combined_bound_) {
        bound.fill(MAX_COST);
    }

    build_base_bound(inv_graph, target_node);
    select_landmarks(graph, inv_graph, start_node, target_node);
    precompute_landmark_tables(graph, inv_graph);
    build_combined_bounds(target_node);
}

template<int N>
cost_t LowerBoundOracle<N>::guarded_add(cost_t lhs, cost_t rhs) {
    if (lhs == MAX_COST || rhs == MAX_COST) {
        return MAX_COST;
    }

    const uint64_t sum = static_cast<uint64_t>(lhs) + static_cast<uint64_t>(rhs);
    if (sum > static_cast<uint64_t>(MAX_COST)) {
        return MAX_COST;
    }
    return static_cast<cost_t>(sum);
}

template<int N>
cost_t LowerBoundOracle<N>::guarded_diff(cost_t lhs, cost_t rhs) {
    if (lhs == MAX_COST || rhs == MAX_COST || lhs <= rhs) {
        return 0;
    }
    return lhs - rhs;
}

template<int N>
cost_t LowerBoundOracle<N>::max_finite(cost_t lhs, cost_t rhs) {
    const bool lhs_finite = lhs != MAX_COST;
    const bool rhs_finite = rhs != MAX_COST;
    if (!lhs_finite && !rhs_finite) {
        return 0;
    }
    if (!lhs_finite) {
        return rhs;
    }
    if (!rhs_finite) {
        return lhs;
    }
    return std::max(lhs, rhs);
}

template<int N>
std::vector<cost_t> LowerBoundOracle<N>::dijkstra_single_objective(
    size_t source,
    size_t cost_idx,
    const AdjacencyMatrix& adj_matrix
) {
    std::vector<cost_t> distance(adj_matrix.get_num_node(), MAX_COST);
    if (source >= distance.size()) {
        return distance;
    }

    std::priority_queue<
        DijkstraNode,
        std::vector<DijkstraNode>,
        DijkstraNode::larger_for_pq
    > open;

    distance[source] = 0;
    open.push({source, 0});

    while (!open.empty()) {
        const DijkstraNode curr = open.top();
        open.pop();

        if (curr.distance != distance[curr.idx]) {
            continue;
        }

        const std::vector<Edge>& outgoing_edges = adj_matrix[curr.idx];
        for (const auto& edge : outgoing_edges) {
            const cost_t next_distance = guarded_add(curr.distance, edge.cost[cost_idx]);
            if (next_distance < distance[edge.tail]) {
                distance[edge.tail] = next_distance;
                open.push({edge.tail, next_distance});
            }
        }
    }

    return distance;
}

template<int N>
void LowerBoundOracle<N>::build_base_bound(const AdjacencyMatrix& inv_graph, size_t target_node) {
    for (int obj_idx = 0; obj_idx < N; obj_idx++) {
        const std::vector<cost_t> distance = dijkstra_single_objective(target_node, static_cast<size_t>(obj_idx), inv_graph);
        for (size_t node = 0; node < distance.size(); node++) {
            base_bound_[node][obj_idx] = distance[node];
        }
    }
}

template<int N>
void LowerBoundOracle<N>::select_landmarks(
    const AdjacencyMatrix& graph,
    const AdjacencyMatrix& inv_graph,
    size_t start_node,
    size_t target_node
) {
    const size_t num_nodes = static_cast<size_t>(graph.get_num_node());
    if (num_nodes == 0) {
        return;
    }

    const size_t max_landmarks = std::min(NUM_LANDMARKS, num_nodes);
    std::vector<bool> selected(num_nodes, false);
    std::vector<cost_t> min_sep(num_nodes, MAX_COST);

    auto update_min_sep = [&](size_t landmark_node) {
        const std::vector<cost_t> from_landmark = dijkstra_single_objective(landmark_node, 0, graph);
        const std::vector<cost_t> to_landmark = dijkstra_single_objective(landmark_node, 0, inv_graph);

        for (size_t node = 0; node < num_nodes; node++) {
            const cost_t separation = max_finite(from_landmark[node], to_landmark[node]);
            if (min_sep[node] == MAX_COST) {
                min_sep[node] = separation;
            } else {
                min_sep[node] = std::min(min_sep[node], separation);
            }
        }
    };

    auto add_landmark = [&](size_t node) {
        if (node >= num_nodes || selected[node]) {
            return;
        }
        selected[node] = true;
        landmarks_.push_back(node);
        update_min_sep(node);
    };

    add_landmark(start_node);
    add_landmark(target_node);

    while (landmarks_.size() < max_landmarks) {
        size_t best_node = num_nodes;
        cost_t best_score = 0;
        for (size_t node = 0; node < num_nodes; node++) {
            if (selected[node] || min_sep[node] == MAX_COST) {
                continue;
            }
            if (best_node == num_nodes || min_sep[node] > best_score) {
                best_node = node;
                best_score = min_sep[node];
            }
        }

        if (best_node == num_nodes || best_score == 0) {
            break;
        }
        add_landmark(best_node);
    }
}

template<int N>
void LowerBoundOracle<N>::precompute_landmark_tables(
    const AdjacencyMatrix& graph,
    const AdjacencyMatrix& inv_graph
) {
    landmark_tables_.clear();
    landmark_tables_.reserve(landmarks_.size());

    for (size_t landmark_node : landmarks_) {
        LandmarkDistances table;
        for (int obj_idx = 0; obj_idx < N; obj_idx++) {
            table.from_landmark[obj_idx] = dijkstra_single_objective(landmark_node, static_cast<size_t>(obj_idx), graph);
            table.to_landmark[obj_idx] = dijkstra_single_objective(landmark_node, static_cast<size_t>(obj_idx), inv_graph);
        }
        landmark_tables_.push_back(std::move(table));
    }
}

template<int N>
void LowerBoundOracle<N>::build_combined_bounds(size_t target_node) {
    for (size_t node = 0; node < combined_bound_.size(); node++) {
        for (int obj_idx = 0; obj_idx < N; obj_idx++) {
            cost_t best = base_bound_[node][obj_idx];

            for (const auto& table : landmark_tables_) {
                const cost_t from_target = table.from_landmark[obj_idx][target_node];
                const cost_t from_node = table.from_landmark[obj_idx][node];
                const cost_t to_node = table.to_landmark[obj_idx][node];
                const cost_t to_target = table.to_landmark[obj_idx][target_node];
                const cost_t alt_bound = std::max(
                    guarded_diff(from_target, from_node),
                    guarded_diff(to_node, to_target)
                );
                best = std::max(best, alt_bound);
            }

            combined_bound_[node][obj_idx] = best;
        }
    }
}

template class LowerBoundOracle<2>;
template class LowerBoundOracle<3>;
template class LowerBoundOracle<4>;
template class LowerBoundOracle<5>;
