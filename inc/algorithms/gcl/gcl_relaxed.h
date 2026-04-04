#ifndef ALGORITHMS_GCL_RELAXED
#define ALGORITHMS_GCL_RELAXED

#include "../../common_inc.h"
#include "../../utils/cost.h"

#include <atomic>
#include <memory>
#include <mutex>

template<int N, size_t HOT_CACHE_SIZE = 4, size_t INITIAL_CAPACITY = 8>
class Gcl_relaxed {
public:
    struct FrontEntry {
        CostVec<N> cost{};
        std::atomic<uint8_t> alive{0};
        double time_found = -1.0;

        FrontEntry() = default;

        FrontEntry(const CostVec<N>& cost, double time_found)
        : cost(cost), time_found(time_found) {
            alive.store(1, std::memory_order_relaxed);
        }

        FrontEntry(const FrontEntry& other)
        : cost(other.cost), time_found(other.time_found) {
            alive.store(other.alive.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }

        FrontEntry& operator=(const FrontEntry& other) {
            if (this == &other) { return *this; }
            cost = other.cost;
            time_found = other.time_found;
            alive.store(other.alive.load(std::memory_order_relaxed), std::memory_order_relaxed);
            return *this;
        }
    };

    struct SnapshotEntry {
        CostVec<N> cost{};
        double time_found = -1.0;
    };

    using Snapshot = std::vector<SnapshotEntry>;

    explicit Gcl_relaxed(size_t num_node)
    : frontiers(num_node + 1) {}

    std::string get_name() const { return "optimistic_vector"; }

    inline bool frontier_check(size_t node, const CostVec<N>& cost) const {
        if (node >= frontiers.size()) { return false; }

        const Storage* storage = frontiers[node].active.load(std::memory_order_acquire);
        if (storage == nullptr) { return false; }

        const size_t published_size = storage->published_size.load(std::memory_order_acquire);
        const size_t hot_cache_size = storage->hot_cache_size.load(std::memory_order_acquire);
        for (size_t i = 0; i < hot_cache_size; i++) {
            const size_t idx = storage->hot_cache_idx[i].load(std::memory_order_relaxed);
            if (idx >= published_size) { continue; }

            const FrontEntry& entry = storage->entries[idx];
            if (entry.alive.load(std::memory_order_acquire) == 0) { continue; }
            if (weakly_dominate<N>(entry.cost, cost)) { return true; }
        }

        for (size_t idx = 0; idx < published_size; idx++) {
            const FrontEntry& entry = storage->entries[idx];
            if (entry.alive.load(std::memory_order_acquire) == 0) { continue; }
            if (weakly_dominate<N>(entry.cost, cost)) { return true; }
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

        Storage* storage = frontier.active.load(std::memory_order_acquire);
        std::vector<size_t> dominated_indices;
        dominated_indices.reserve(8);

        if (storage != nullptr) {
            const size_t published_size = storage->published_size.load(std::memory_order_acquire);
            for (size_t idx = 0; idx < published_size; idx++) {
                const FrontEntry& entry = storage->entries[idx];
                if (entry.alive.load(std::memory_order_acquire) == 0) { continue; }

                if (weakly_dominate<N>(entry.cost, cost)) {
                    return false;
                }
                if (weakly_dominate<N>(cost, entry.cost)) {
                    dominated_indices.push_back(idx);
                }
            }
        }

        const size_t projected_live = frontier.live_count + 1 - dominated_indices.size();
        const size_t projected_dead = frontier.dead_count + dominated_indices.size();
        const bool need_compact = storage != nullptr
            && storage->published_size.load(std::memory_order_relaxed) >= 32
            && projected_dead >= projected_live;
        const bool need_grow = storage == nullptr
            || storage->published_size.load(std::memory_order_relaxed) == storage->capacity;

        if (need_grow || need_compact) {
            const size_t next_capacity = std::max(
                INITIAL_CAPACITY,
                std::max(projected_live, size_t(1)) * 2
            );
            auto next_storage = std::make_unique<Storage>(next_capacity);
            size_t next_size = 0;

            if (storage != nullptr) {
                const size_t published_size = storage->published_size.load(std::memory_order_acquire);
                for (size_t idx = 0; idx < published_size; idx++) {
                    const FrontEntry& entry = storage->entries[idx];
                    if (entry.alive.load(std::memory_order_acquire) == 0) { continue; }
                    if (weakly_dominate<N>(cost, entry.cost)) { continue; }

                    next_storage->entries[next_size] = entry;
                    next_storage->entries[next_size].alive.store(1, std::memory_order_relaxed);
                    next_size++;
                }
            }

            next_storage->entries[next_size] = FrontEntry(cost, time_found);
            next_size++;
            next_storage->published_size.store(next_size, std::memory_order_release);

            if (need_compact) {
                num_compactions.fetch_add(1, std::memory_order_relaxed);
            }

            if (frontier.active_owner) {
                frontier.retired.push_back(std::move(frontier.active_owner));
            }
            frontier.active_owner = std::move(next_storage);
            frontier.active.store(frontier.active_owner.get(), std::memory_order_release);
            frontier.live_count = next_size;
            frontier.dead_count = 0;
            refresh_hot_cache_locked(frontier, next_size - 1);
            return true;
        }

        for (size_t idx : dominated_indices) {
            storage->entries[idx].alive.store(0, std::memory_order_release);
        }
        frontier.dead_count += dominated_indices.size();
        frontier.live_count -= dominated_indices.size();

        const size_t next_idx = storage->published_size.load(std::memory_order_relaxed);
        storage->entries[next_idx] = FrontEntry(cost, time_found);
        storage->published_size.store(next_idx + 1, std::memory_order_release);
        frontier.live_count++;
        refresh_hot_cache_locked(frontier, next_idx);
        return true;
    }

    inline std::shared_ptr<Snapshot> snapshot(size_t node) const {
        auto result = std::make_shared<Snapshot>();
        if (node >= frontiers.size()) {
            return result;
        }

        const Storage* storage = frontiers[node].active.load(std::memory_order_acquire);
        if (storage == nullptr) {
            return result;
        }

        const size_t published_size = storage->published_size.load(std::memory_order_acquire);
        result->reserve(frontiers[node].live_count);
        for (size_t idx = 0; idx < published_size; idx++) {
            const FrontEntry& entry = storage->entries[idx];
            if (entry.alive.load(std::memory_order_acquire) == 0) { continue; }

            result->push_back({entry.cost, entry.time_found});
        }
        return result;
    }

    inline size_t get_compaction_count() const {
        return num_compactions.load(std::memory_order_relaxed);
    }

    inline std::vector<size_t> live_frontier_sizes() const {
        std::vector<size_t> sizes;
        sizes.reserve(frontiers.size());
        for (const auto& frontier : frontiers) {
            if (frontier.live_count > 0) {
                sizes.push_back(frontier.live_count);
            }
        }
        return sizes;
    }

private:
    struct Storage {
        explicit Storage(size_t capacity)
        : entries(new FrontEntry[capacity]), capacity(capacity) {
            for (auto& idx : hot_cache_idx) {
                idx.store(std::numeric_limits<size_t>::max(), std::memory_order_relaxed);
            }
        }

        std::unique_ptr<FrontEntry[]> entries;
        size_t capacity;
        std::atomic<size_t> published_size{0};
        std::array<std::atomic<size_t>, HOT_CACHE_SIZE> hot_cache_idx{};
        std::atomic<size_t> hot_cache_size{0};
    };

    struct NodeFrontier {
        std::atomic<Storage*> active{nullptr};
        std::unique_ptr<Storage> active_owner;
        std::vector<std::unique_ptr<Storage>> retired;
        size_t live_count = 0;
        size_t dead_count = 0;
        mutable std::mutex writer_lock;
    };

    inline void refresh_hot_cache_locked(NodeFrontier& frontier, size_t preferred_idx) {
        Storage* storage = frontier.active.load(std::memory_order_relaxed);
        if (storage == nullptr) { return; }

        const size_t published_size = storage->published_size.load(std::memory_order_relaxed);
        std::array<size_t, HOT_CACHE_SIZE> picks{};
        size_t picked = 0;

        auto remember = [&](size_t idx) {
            if (idx >= published_size) { return; }
            if (storage->entries[idx].alive.load(std::memory_order_relaxed) == 0) { return; }

            for (size_t i = 0; i < picked; i++) {
                if (picks[i] == idx) { return; }
            }
            picks[picked++] = idx;
        };

        remember(preferred_idx);
        for (size_t idx = published_size; idx > 0 && picked < HOT_CACHE_SIZE; idx--) {
            remember(idx - 1);
        }

        for (size_t i = 0; i < picked; i++) {
            storage->hot_cache_idx[i].store(picks[i], std::memory_order_relaxed);
        }
        for (size_t i = picked; i < HOT_CACHE_SIZE; i++) {
            storage->hot_cache_idx[i].store(std::numeric_limits<size_t>::max(), std::memory_order_relaxed);
        }
        storage->hot_cache_size.store(picked, std::memory_order_release);
    }

    std::vector<NodeFrontier> frontiers;
    std::atomic<size_t> num_compactions{0};
};

template class Gcl_relaxed<2>;
template class Gcl_relaxed<3>;
template class Gcl_relaxed<4>;
template class Gcl_relaxed<5>;

#endif
