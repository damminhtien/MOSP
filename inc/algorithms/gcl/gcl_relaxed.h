#ifndef ALGORITHMS_GCL_RELAXED
#define ALGORITHMS_GCL_RELAXED

#include "../../common_inc.h"
#include "../../utils/cost.h"

#include <atomic>
#include <mutex>

template<int N, size_t HOT_CACHE_SIZE = 4, uint32_t BLOCK_SIZE = 32>
class Gcl_relaxed {
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

    explicit Gcl_relaxed(size_t num_node)
    : frontiers(num_node + 1) {}

    std::string get_name() const { return "blocked_skyline"; }

    inline bool frontier_check(
        size_t node,
        const CostVec<N>& cost,
        CheckStats* stats = nullptr,
        SnapshotEntry* witness = nullptr
    ) const {
        if (node >= frontiers.size()) { return false; }

        const NodeFrontier& frontier = frontiers[node];
        for (int dim = 0; dim < N; dim++) {
            if (frontier.frontier_min[dim].load(std::memory_order_acquire) > cost[dim]) {
                return false;
            }
        }

        const uint32_t hot_size = frontier.hot_size.load(std::memory_order_acquire);
        for (uint32_t i = 0; i < hot_size; i++) {
            CostVec<N> cached_cost;
            for (int dim = 0; dim < N; dim++) {
                cached_cost[dim] = frontier.hot_costs[i][dim].load(std::memory_order_relaxed);
            }
            if (!weakly_dominate<N>(cached_cost, cost)) { continue; }

            if (witness != nullptr) {
                witness->cost = cached_cost;
                witness->time_found = frontier.hot_times[i].load(std::memory_order_relaxed);
            }
            return true;
        }

        Block* block = frontier.head.load(std::memory_order_acquire);
        while (block != nullptr) {
            if (stats != nullptr) {
                stats->blocks_scanned++;
            }

            if (block_cannot_dominate(block, cost)) {
                block = block->next.load(std::memory_order_acquire);
                continue;
            }

            const uint32_t count = block->count.load(std::memory_order_acquire);
            const uint32_t live_mask = block->live_mask.load(std::memory_order_acquire);

            for (uint32_t idx = 0; idx < count; idx++) {
                const uint32_t bit = (1u << idx);
                if ((live_mask & bit) == 0) {
                    if (stats != nullptr) {
                        stats->dead_entries_encountered++;
                    }
                    continue;
                }

                if (stats != nullptr) {
                    stats->live_entries_scanned++;
                }
                if (!weakly_dominate<N>(block->costs[idx], cost)) { continue; }

                if (witness != nullptr) {
                    witness->cost = block->costs[idx];
                    witness->time_found = block->time_found[idx];
                }
                return true;
            }

            block = block->next.load(std::memory_order_acquire);
        }

        return false;
    }

    inline bool frontier_update(size_t node, const CostVec<N>& cost, double time_found = -1.0) {
        if (node >= frontiers.size()) {
            perror(("Invalid Gcl_relaxed index " + std::to_string(node)).c_str());
            exit(EXIT_FAILURE);
            return false;
        }

        NodeFrontier& frontier = frontiers[node];
        std::lock_guard<std::mutex> guard(frontier.writer_lock);

        struct DominatedBlockMask {
            Block* block = nullptr;
            uint32_t mask = 0;
        };
        std::vector<DominatedBlockMask> dominated;
        dominated.reserve(4);

        Block* block = frontier.head.load(std::memory_order_relaxed);
        while (block != nullptr) {
            const uint32_t count = block->count.load(std::memory_order_relaxed);
            const uint32_t live_mask = block->live_mask.load(std::memory_order_relaxed);

            DominatedBlockMask local;
            local.block = block;

            for (uint32_t idx = 0; idx < count; idx++) {
                const uint32_t bit = (1u << idx);
                if ((live_mask & bit) == 0) { continue; }

                const CostVec<N>& existing = block->costs[idx];
                if (weakly_dominate<N>(existing, cost)) {
                    return false;
                }
                if (weakly_dominate<N>(cost, existing)) {
                    local.mask |= bit;
                }
            }

            if (local.mask != 0) {
                dominated.push_back(local);
            }

            block = block->next.load(std::memory_order_relaxed);
        }

        for (const auto& entry : dominated) {
            uint32_t live_mask = entry.block->live_mask.load(std::memory_order_relaxed);
            live_mask &= ~entry.mask;
            entry.block->live_mask.store(live_mask, std::memory_order_release);
            frontier.live_count -= popcount32(entry.mask);
            update_block_min_locked(entry.block);
        }

        Block* tail = frontier.tail;
        if (tail == nullptr || tail->count.load(std::memory_order_relaxed) == BLOCK_SIZE) {
            frontier.blocks.push_back(std::make_unique<Block>());
            Block* new_block = frontier.blocks.back().get();
            if (tail == nullptr) {
                frontier.head.store(new_block, std::memory_order_release);
            } else {
                tail->next.store(new_block, std::memory_order_release);
            }
            frontier.tail = new_block;
            frontier.block_count++;
            tail = new_block;
        }

        const uint32_t idx = tail->count.load(std::memory_order_relaxed);
        tail->costs[idx] = cost;
        tail->time_found[idx] = time_found;
        update_block_min_with_candidate_locked(tail, cost);

        uint32_t live_mask = tail->live_mask.load(std::memory_order_relaxed);
        live_mask |= (1u << idx);
        tail->live_mask.store(live_mask, std::memory_order_release);
        tail->count.store(idx + 1, std::memory_order_release);

        frontier.live_count++;
        refresh_frontier_min_locked(frontier);
        refresh_hot_cache_locked(frontier);
        return true;
    }

    inline std::shared_ptr<Snapshot> snapshot(size_t node) const {
        auto result = std::make_shared<Snapshot>();
        if (node >= frontiers.size()) {
            return result;
        }

        const NodeFrontier& frontier = frontiers[node];
        result->reserve(frontier.live_count);

        Block* block = frontier.head.load(std::memory_order_acquire);
        while (block != nullptr) {
            const uint32_t count = block->count.load(std::memory_order_acquire);
            const uint32_t live_mask = block->live_mask.load(std::memory_order_acquire);
            for (uint32_t idx = 0; idx < count; idx++) {
                const uint32_t bit = (1u << idx);
                if ((live_mask & bit) == 0) { continue; }
                result->push_back({block->costs[idx], block->time_found[idx]});
            }
            block = block->next.load(std::memory_order_acquire);
        }

        return result;
    }

    inline double average_blocks_per_visited_node() const {
        size_t total_blocks = 0;
        size_t visited_nodes = 0;
        for (const auto& frontier : frontiers) {
            if (frontier.live_count == 0) { continue; }
            visited_nodes++;
            total_blocks += frontier.block_count;
        }

        if (visited_nodes == 0) { return 0.0; }
        return static_cast<double>(total_blocks) / static_cast<double>(visited_nodes);
    }

private:
    struct Block {
        std::array<CostVec<N>, BLOCK_SIZE> costs{};
        std::array<double, BLOCK_SIZE> time_found{};
        std::atomic<uint32_t> count{0};
        std::atomic<uint32_t> live_mask{0};
        std::array<std::atomic<cost_t>, N> block_min{};
        std::atomic<Block*> next{nullptr};

        Block() {
            for (int dim = 0; dim < N; dim++) {
                block_min[dim].store(MAX_COST, std::memory_order_relaxed);
            }
        }
    };

    struct NodeFrontier {
        std::vector<std::unique_ptr<Block>> blocks;
        std::atomic<Block*> head{nullptr};
        Block* tail = nullptr;
        size_t live_count = 0;
        size_t block_count = 0;
        mutable std::mutex writer_lock;
        std::array<std::array<std::atomic<cost_t>, N>, HOT_CACHE_SIZE> hot_costs{};
        std::array<std::atomic<double>, HOT_CACHE_SIZE> hot_times{};
        std::atomic<uint32_t> hot_size{0};
        std::array<std::atomic<cost_t>, N> frontier_min{};

        NodeFrontier() {
            for (size_t slot = 0; slot < HOT_CACHE_SIZE; slot++) {
                hot_times[slot].store(-1.0, std::memory_order_relaxed);
                for (int dim = 0; dim < N; dim++) {
                    hot_costs[slot][dim].store(MAX_COST, std::memory_order_relaxed);
                }
            }
            for (int dim = 0; dim < N; dim++) {
                frontier_min[dim].store(MAX_COST, std::memory_order_relaxed);
            }
        }
    };

    static inline uint32_t popcount32(uint32_t value) {
        return static_cast<uint32_t>(__builtin_popcount(value));
    }

    static inline bool block_cannot_dominate(const Block* block, const CostVec<N>& cost) {
        for (int dim = 0; dim < N; dim++) {
            if (block->block_min[dim].load(std::memory_order_acquire) > cost[dim]) {
                return true;
            }
        }
        return false;
    }

    static inline void update_block_min_locked(Block* block) {
        CostVec<N> min_cost;
        min_cost.fill(MAX_COST);

        const uint32_t count = block->count.load(std::memory_order_relaxed);
        const uint32_t live_mask = block->live_mask.load(std::memory_order_relaxed);
        for (uint32_t idx = 0; idx < count; idx++) {
            const uint32_t bit = (1u << idx);
            if ((live_mask & bit) == 0) { continue; }

            for (int dim = 0; dim < N; dim++) {
                min_cost[dim] = std::min(min_cost[dim], block->costs[idx][dim]);
            }
        }

        for (int dim = 0; dim < N; dim++) {
            block->block_min[dim].store(min_cost[dim], std::memory_order_release);
        }
    }

    static inline void update_block_min_with_candidate_locked(Block* block, const CostVec<N>& cost) {
        for (int dim = 0; dim < N; dim++) {
            cost_t current = block->block_min[dim].load(std::memory_order_relaxed);
            if (cost[dim] < current) {
                block->block_min[dim].store(cost[dim], std::memory_order_release);
            }
        }
    }

    inline void refresh_hot_cache_locked(NodeFrontier& frontier) {
        std::array<SnapshotEntry, HOT_CACHE_SIZE> latest{};
        size_t total = 0;

        Block* block = frontier.head.load(std::memory_order_relaxed);
        while (block != nullptr) {
            const uint32_t count = block->count.load(std::memory_order_relaxed);
            const uint32_t live_mask = block->live_mask.load(std::memory_order_relaxed);
            for (uint32_t idx = 0; idx < count; idx++) {
                const uint32_t bit = (1u << idx);
                if ((live_mask & bit) == 0) { continue; }

                SnapshotEntry entry{block->costs[idx], block->time_found[idx]};
                if (total < HOT_CACHE_SIZE) {
                    latest[total] = entry;
                } else {
                    latest[total % HOT_CACHE_SIZE] = entry;
                }
                total++;
            }
            block = block->next.load(std::memory_order_relaxed);
        }

        const size_t hot_entries = std::min<size_t>(HOT_CACHE_SIZE, total);
        const size_t start = total > HOT_CACHE_SIZE ? (total % HOT_CACHE_SIZE) : 0;

        for (size_t slot = 0; slot < hot_entries; slot++) {
            const SnapshotEntry& entry = latest[(start + slot) % HOT_CACHE_SIZE];
            for (int dim = 0; dim < N; dim++) {
                frontier.hot_costs[slot][dim].store(entry.cost[dim], std::memory_order_relaxed);
            }
            frontier.hot_times[slot].store(entry.time_found, std::memory_order_relaxed);
        }
        frontier.hot_size.store(static_cast<uint32_t>(hot_entries), std::memory_order_release);
    }

    inline void refresh_frontier_min_locked(NodeFrontier& frontier) {
        CostVec<N> min_cost;
        min_cost.fill(MAX_COST);

        Block* block = frontier.head.load(std::memory_order_relaxed);
        while (block != nullptr) {
            for (int dim = 0; dim < N; dim++) {
                min_cost[dim] = std::min(
                    min_cost[dim],
                    block->block_min[dim].load(std::memory_order_relaxed)
                );
            }
            block = block->next.load(std::memory_order_relaxed);
        }

        for (int dim = 0; dim < N; dim++) {
            frontier.frontier_min[dim].store(min_cost[dim], std::memory_order_release);
        }
    }

    std::vector<NodeFrontier> frontiers;
};

template class Gcl_relaxed<2>;
template class Gcl_relaxed<3>;
template class Gcl_relaxed<4>;
template class Gcl_relaxed<5>;

#endif
