#ifndef ALGORITHMS_GCL_OWNER_SHARDED
#define ALGORITHMS_GCL_OWNER_SHARDED

#include "../../common_inc.h"
#include "../../utils/cost.h"
#include "gcl_fast_array.h"

#include <memory>

template<int M>
class OwnerShardedGcl {
public:
    using Snapshot = std::vector<CostVec<M>>;

    OwnerShardedGcl(size_t num_node, size_t num_owners)
    : frontiers_(num_node + 1),
    owner_of_node_(num_node + 1, 0) {
        const size_t owner_count = std::max<size_t>(1, num_owners);
        for (size_t node = 0; node < owner_of_node_.size(); node++) {
            owner_of_node_[node] = static_cast<uint32_t>(node % owner_count);
        }
    }

    std::string get_name() const {
        return "owner_sharded_fast_array";
    }

    size_t owner_of(size_t node) const {
        if (node >= owner_of_node_.size()) {
            return 0;
        }
        return owner_of_node_[node];
    }

    bool owner_dominated(size_t owner, size_t node, const CostVec<M>& x) const {
        validate_owner(owner, node);
        return frontiers_[node].exact.dominated(x);
    }

    bool owner_try_insert_or_prune(size_t owner, size_t node, const CostVec<M>& x) {
        validate_owner(owner, node);
        const bool inserted = frontiers_[node].exact.try_insert_or_prune(x);
        if (inserted) {
            publish(node);
        }
        return inserted;
    }

    bool read_dominated(size_t node, const CostVec<M>& x) const {
        if (node >= frontiers_.size()) {
            return false;
        }

        std::shared_ptr<const Snapshot> snapshot = std::atomic_load_explicit(
            &frontiers_[node].published,
            std::memory_order_acquire
        );
        if (!snapshot) {
            return false;
        }

        for (const auto& cost : *snapshot) {
            if (weakly_dominate<M>(cost, x)) {
                return true;
            }
        }
        return false;
    }

    Snapshot snapshot(size_t node) const {
        if (node >= frontiers_.size()) {
            return {};
        }
        return frontiers_[node].exact.snapshot();
    }

    void clear() {
        for (auto& frontier : frontiers_) {
            frontier.exact.clear();
            std::atomic_store_explicit(
                &frontier.published,
                std::shared_ptr<const Snapshot>(),
                std::memory_order_release
            );
        }
    }

    std::vector<size_t> live_frontier_sizes() const {
        std::vector<size_t> sizes;
        sizes.reserve(frontiers_.size());
        for (const auto& frontier : frontiers_) {
            if (!frontier.exact.empty()) {
                sizes.push_back(frontier.exact.size());
            }
        }
        return sizes;
    }

private:
    struct NodeFrontier {
        FastNodeFrontier<M> exact;
        std::shared_ptr<const Snapshot> published;
    };

    void validate_owner(size_t owner, size_t node) const {
        if (node >= owner_of_node_.size() || owner_of_node_[node] != owner) {
            perror(("OwnerShardedGcl owner mismatch at node " + std::to_string(node)).c_str());
            exit(EXIT_FAILURE);
        }
    }

    void publish(size_t node) {
        auto snapshot = std::make_shared<const Snapshot>(frontiers_[node].exact.snapshot());
        std::atomic_store_explicit(&frontiers_[node].published, snapshot, std::memory_order_release);
    }

    std::vector<NodeFrontier> frontiers_;
    std::vector<uint32_t> owner_of_node_;
};

#endif
