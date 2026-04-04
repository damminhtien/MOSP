#ifndef ALGORITHMS_GCL_RELAXED_SERIAL
#define ALGORITHMS_GCL_RELAXED_SERIAL

#include "../../common_inc.h"
#include "../../utils/cost.h"

template<int N, size_t HOT_CACHE_SIZE = 8>
class Gcl_relaxed_serial {
public:
    struct SnapshotEntry {
        CostVec<N> cost{};
        double time_found = -1.0;
    };

    struct CheckStats {
        size_t blocks_scanned = 0;
        size_t live_entries_scanned = 0;
        size_t dead_entries_encountered = 0;
    };

    using Snapshot = std::vector<SnapshotEntry>;

    explicit Gcl_relaxed_serial(size_t num_node)
    : frontiers(num_node + 1), frontier_min(num_node + 1), hot_cache(num_node + 1), hot_size(num_node + 1, 0) {
        for (auto& entry : frontier_min) {
            entry.fill(MAX_COST);
        }
    }

    std::string get_name() const { return "serial_skyline"; }

    inline bool frontier_check(
        size_t node,
        const CostVec<N>& cost,
        CheckStats* stats = nullptr,
        SnapshotEntry* witness = nullptr
    ) const {
        if (node >= frontiers.size()) { return false; }
        for (int dim = 0; dim < N; dim++) {
            if (frontier_min[node][dim] > cost[dim]) {
                return false;
            }
        }

        for (size_t idx = 0; idx < hot_size[node]; idx++) {
            const SnapshotEntry& entry = hot_cache[node][idx];
            if (!weakly_dominate<N>(entry.cost, cost)) { continue; }
            if (witness != nullptr) {
                *witness = entry;
            }
            return true;
        }

        if (stats != nullptr && !frontiers[node].empty()) {
            stats->blocks_scanned++;
        }
        for (const SnapshotEntry& entry : frontiers[node]) {
            if (stats != nullptr) {
                stats->live_entries_scanned++;
            }
            if (!weakly_dominate<N>(entry.cost, cost)) { continue; }
            if (witness != nullptr) {
                *witness = entry;
            }
            return true;
        }
        return false;
    }

    inline bool frontier_update(size_t node, const CostVec<N>& cost, double time_found = -1.0) {
        if (node >= frontiers.size()) {
            perror(("Invalid Gcl_relaxed_serial index " + std::to_string(node)).c_str());
            exit(EXIT_FAILURE);
            return false;
        }

        std::vector<SnapshotEntry>& frontier = frontiers[node];
        CostVec<N> current_min = frontier_min[node];
        bool need_recompute_min = false;

        size_t idx = 0;
        while (idx < frontier.size()) {
            if (weakly_dominate<N>(frontier[idx].cost, cost)) {
                return false;
            }

            if (weakly_dominate<N>(cost, frontier[idx].cost)) {
                for (int dim = 0; dim < N; dim++) {
                    if (frontier[idx].cost[dim] == current_min[dim]) {
                        need_recompute_min = true;
                    }
                }
                frontier[idx] = frontier.back();
                frontier.pop_back();
                continue;
            }
            idx++;
        }

        frontier.push_back({cost, time_found});
        if (need_recompute_min) {
            recompute_min(node);
        } else {
            for (int dim = 0; dim < N; dim++) {
                frontier_min[node][dim] = std::min(frontier_min[node][dim], cost[dim]);
            }
        }
        refresh_hot_cache(node);
        return true;
    }

    inline std::shared_ptr<Snapshot> snapshot(size_t node) const {
        auto result = std::make_shared<Snapshot>();
        if (node >= frontiers.size()) {
            return result;
        }

        result->reserve(frontiers[node].size());
        for (const SnapshotEntry& entry : frontiers[node]) {
            result->push_back(entry);
        }
        return result;
    }

    inline double average_blocks_per_visited_node() const {
        size_t visited_nodes = 0;
        for (const auto& frontier : frontiers) {
            if (!frontier.empty()) {
                visited_nodes++;
            }
        }
        return visited_nodes == 0 ? 0.0 : 1.0;
    }

private:
    inline void recompute_min(size_t node) {
        frontier_min[node].fill(MAX_COST);
        for (const SnapshotEntry& entry : frontiers[node]) {
            for (int dim = 0; dim < N; dim++) {
                frontier_min[node][dim] = std::min(frontier_min[node][dim], entry.cost[dim]);
            }
        }
    }

    inline void refresh_hot_cache(size_t node) {
        const std::vector<SnapshotEntry>& frontier = frontiers[node];
        const size_t count = std::min<size_t>(HOT_CACHE_SIZE, frontier.size());
        hot_size[node] = count;
        for (size_t idx = 0; idx < count; idx++) {
            hot_cache[node][idx] = frontier[frontier.size() - count + idx];
        }
    }

    std::vector<std::vector<SnapshotEntry>> frontiers;
    std::vector<CostVec<N>> frontier_min;
    std::vector<std::array<SnapshotEntry, HOT_CACHE_SIZE>> hot_cache;
    std::vector<size_t> hot_size;
};

#endif
