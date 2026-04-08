#ifndef ALGORITHMS_GCL_TREE
#define ALGORITHMS_GCL_TREE

#include"../../common_inc.h"
#include"../../utils/cost.h"

template<int N>
class Gcl_tree {
public:
    struct FrontEntry {
        CostVec<N> cost{};
        double time_found = -1.0;
    };

    using Snapshot = std::vector<FrontEntry>;

    Gcl_tree(size_t num_node) : gcl(num_node + 1) {
        for (auto& frontier : gcl) {
            frontier.reserve(INITIAL_FRONTIER_CAPACITY);
        }
    }

    std::string get_name(){ return "avl_tree"; }
    
    inline bool frontier_check(size_t node, const CostVec<N> & cost) {
        if (node >= gcl.size()) { return false; }

        const NodeFrontier& frontier = gcl[node];
        const auto limit = std::lower_bound(
            frontier.begin(),
            frontier.end(),
            cost,
            [](const FrontEntry& entry, const CostVec<N>& rhs) {
                return lex_smaller<N>(entry.cost, rhs);
            }
        );

        if (limit != frontier.end() && limit->cost == cost) {
            return true;
        }

        for (auto it = frontier.begin(); it != limit; ++it) {
            if (weakly_dominate<N>(it->cost, cost)) { return true; }
        }
        return false;
    }

    inline bool frontier_update(size_t node, const CostVec<N> & cost, double time_found = -1.0) {
        if (node >= gcl.size()) { 
            perror(("Invalid Gcl_tree index " + std::to_string(node)).c_str());
            exit(EXIT_FAILURE);
            return false; 
        }

        NodeFrontier& frontier = gcl[node];
        const auto insert_it = std::lower_bound(
            frontier.begin(),
            frontier.end(),
            cost,
            [](const FrontEntry& entry, const CostVec<N>& rhs) {
                return lex_smaller<N>(entry.cost, rhs);
            }
        );

        if (insert_it != frontier.end() && insert_it->cost == cost) {
            if (insert_it->time_found < 0.0 || (time_found >= 0.0 && time_found < insert_it->time_found)) {
                insert_it->time_found = time_found;
            }
            return false;
        }

        for (auto it = frontier.begin(); it != insert_it; ++it) {
            if (weakly_dominate<N>(it->cost, cost)) {
                return false;
            }
        }

        const size_t insert_idx = static_cast<size_t>(insert_it - frontier.begin());
        auto erase_begin = std::remove_if(
            frontier.begin() + static_cast<std::ptrdiff_t>(insert_idx),
            frontier.end(),
            [&](const FrontEntry& existing) {
                return weakly_dominate<N>(cost, existing.cost);
            }
        );
        frontier.erase(erase_begin, frontier.end());
        frontier.insert(
            frontier.begin() + static_cast<std::ptrdiff_t>(insert_idx),
            FrontEntry{cost, time_found}
        );
        return true;
    }

    inline Snapshot snapshot(size_t node) const {
        Snapshot copy;
        if (node >= gcl.size()) { return copy; }

        copy = gcl[node];
        return copy;
    }

private:
    static constexpr size_t INITIAL_FRONTIER_CAPACITY = 8;
    using NodeFrontier = std::vector<FrontEntry>;

    std::vector<NodeFrontier> gcl;
};

template class Gcl_tree<1>;
template class Gcl_tree<2>;
template class Gcl_tree<3>;
template class Gcl_tree<4>;
template class Gcl_tree<5>;

#endif
