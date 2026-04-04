#ifndef ALGORITHMS_GCL_RELAXED
#define ALGORITHMS_GCL_RELAXED

#include "../../common_inc.h"
#include "../../utils/cost.h"

#include <atomic>
#include <memory>
#include <shared_mutex>

template<int N, size_t HOT_CACHE_SIZE = 4>
class Gcl_relaxed {
public:
    struct FrontEntry {
        cost_t f0 = 0;
        CostVec<N - 1> tail{};
        double time_found = -1.0;

        FrontEntry() = default;

        explicit FrontEntry(const CostVec<N>& cost, double found_time = -1.0)
        : f0(cost[0]), time_found(found_time) {
            for (int i = 0; i < N - 1; i++) {
                tail[i] = cost[i + 1];
            }
        }

        CostVec<N> to_cost() const {
            CostVec<N> cost;
            cost[0] = f0;
            for (int i = 0; i < N - 1; i++) {
                cost[i + 1] = tail[i];
            }
            return cost;
        }
    };

    using Snapshot = std::vector<FrontEntry>;

    explicit Gcl_relaxed(size_t num_node)
    : frontiers(num_node + 1) {}

    std::string get_name() const { return "relaxed_vector"; }

    inline bool frontier_check(size_t node, const CostVec<N>& cost) const {
        if (node >= frontiers.size()) { return false; }

        FrontEntry candidate(cost);
        std::shared_lock<std::shared_mutex> guard(frontiers[node].lock);
        return is_dominated_locked(frontiers[node], candidate);
    }

    inline bool snapshot_check(const std::shared_ptr<const Snapshot>& snapshot, const CostVec<N>& cost) const {
        if (!snapshot) { return false; }
        return is_dominated_in_entries(*snapshot, FrontEntry(cost));
    }

    inline bool frontier_update(size_t node, const CostVec<N>& cost, double time_found = -1.0) {
        if (node >= frontiers.size()) {
            perror(("Invalid Gcl_relaxed index " + std::to_string(node)).c_str());
            exit(EXIT_FAILURE);
            return false;
        }

        FrontEntry candidate(cost, time_found);
        NodeFrontier& frontier = frontiers[node];
        std::unique_lock<std::shared_mutex> guard(frontier.lock);

        if (is_dominated_locked(frontier, candidate)) { return false; }

        auto it = std::upper_bound(
            frontier.entries.begin(),
            frontier.entries.end(),
            candidate.f0,
            [](cost_t value, const FrontEntry& entry) {
                return value < entry.f0;
            }
        );

        frontier.entries.insert(it, candidate);
        frontier.entries.erase(
            std::remove_if(
                frontier.entries.begin(),
                frontier.entries.end(),
                [&candidate](const FrontEntry& entry) {
                    if (entry.f0 == candidate.f0 && entry.tail == candidate.tail) {
                        return false;
                    }
                    return dominates(candidate, entry);
                }
            ),
            frontier.entries.end()
        );

        refresh_hot_cache(frontier);
        return true;
    }

    inline std::shared_ptr<Snapshot> snapshot(size_t node) const {
        if (node >= frontiers.size()) {
            return std::make_shared<Snapshot>();
        }

        std::shared_lock<std::shared_mutex> guard(frontiers[node].lock);
        return std::make_shared<Snapshot>(frontiers[node].entries.begin(), frontiers[node].entries.end());
    }

private:
    struct NodeFrontier {
        std::vector<FrontEntry> entries;
        std::array<FrontEntry, HOT_CACHE_SIZE> hot_cache{};
        size_t hot_cache_size = 0;
        mutable std::shared_mutex lock;

        NodeFrontier() {
            entries.reserve(16);
        }
    };

    static inline bool dominates(const FrontEntry& lhs, const FrontEntry& rhs) {
        return lhs.f0 <= rhs.f0 && weakly_dominate<N - 1>(lhs.tail, rhs.tail);
    }

    static inline bool is_dominated_in_entries(const Snapshot& entries, const FrontEntry& candidate) {
        auto it_ub = std::upper_bound(
            entries.begin(),
            entries.end(),
            candidate.f0,
            [](cost_t value, const FrontEntry& entry) {
                return value < entry.f0;
            }
        );

        for (auto it = entries.begin(); it != it_ub; ++it) {
            if (dominates(*it, candidate)) { return true; }
        }
        return false;
    }

    static inline bool is_dominated_locked(const NodeFrontier& frontier, const FrontEntry& candidate) {
        for (size_t i = 0; i < frontier.hot_cache_size; i++) {
            if (dominates(frontier.hot_cache[i], candidate)) {
                return true;
            }
        }
        return is_dominated_in_entries(frontier.entries, candidate);
    }

    static inline void refresh_hot_cache(NodeFrontier& frontier) {
        frontier.hot_cache_size = std::min(frontier.entries.size(), HOT_CACHE_SIZE);
        for (size_t i = 0; i < frontier.hot_cache_size; i++) {
            frontier.hot_cache[i] = frontier.entries[i];
        }
    }

    std::vector<NodeFrontier> frontiers;
};

template class Gcl_relaxed<2>;
template class Gcl_relaxed<3>;
template class Gcl_relaxed<4>;
template class Gcl_relaxed<5>;

#endif
