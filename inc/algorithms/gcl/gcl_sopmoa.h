#ifndef ALGORITHMS_GCL_SOPMOA
#define ALGORITHMS_GCL_SOPMOA

#include "../../common_inc.h"
#include "../../utils/cost.h"

#include <mutex>
#include <shared_mutex>

template <int N>
class Gcl_SOPMOA {
public:
    struct FrontEntry {
        CostVec<N> cost{};
        double time_found = -1.0;
    };

    using Snapshot = std::vector<FrontEntry>;

    explicit Gcl_SOPMOA(size_t num_node)
    : gcl(num_node + 1), locks(num_node + 1) {}

    std::string get_name(){ return "custom_list"; }

    inline bool frontier_check(size_t node, const CostVec<N> & cost) {
        if (node >= gcl.size()) { return false; }

        std::shared_lock<std::shared_mutex> guard(locks[node]);
        auto it_ub = std::lower_bound(
            gcl[node].rbegin(),
            gcl[node].rend(),
            cost,
            [](const FrontEntry& ele, const CostVec<N>& vec) {
                return lex_smaller<N>(vec, ele.cost);
            }
        ).base();

        for (auto it = gcl[node].begin(); it != it_ub; it++) {
            if (weakly_dominate_dr<N>(it->cost, cost)){ return true; }
        }
        return false;
    }

    inline bool frontier_update(size_t node, const CostVec<N> & cost, double time_found = -1.0) {
        if (node >= gcl.size()) {
            perror(("Invalid Gcl index " + std::to_string(node)).c_str());
            exit(EXIT_FAILURE);
            return false;
        }

        std::unique_lock<std::shared_mutex> guard(locks[node]);
        for (auto& existing : gcl[node]) {
            if (existing.cost == cost) {
                if (existing.time_found < 0.0 || (time_found >= 0.0 && time_found < existing.time_found)) {
                    existing.time_found = time_found;
                }
                return false;
            }
            if (weakly_dominate_dr<N>(existing.cost, cost)) {
                return false;
            }
        }

        auto it = gcl[node].begin();
        while (it != gcl[node].end()) {
            if (weakly_dominate_dr<N>(cost, it->cost)) {
                it = gcl[node].erase(it);
            } else {
                ++it;
            }
        }

        auto it_insert = std::find_if(
            gcl[node].rbegin(),
            gcl[node].rend(),
            [&cost](const FrontEntry& ele) {
                return lex_smaller<N>(ele.cost, cost);
            }
        ).base();

        gcl[node].insert(it_insert, FrontEntry{cost, time_found});
        return true;
    }

    inline Snapshot snapshot(size_t node) const {
        Snapshot copy;
        if (node >= gcl.size()) { return copy; }

        std::shared_lock<std::shared_mutex> guard(locks[node]);
        copy.reserve(gcl[node].size());
        for (const auto& entry : gcl[node]) {
            copy.push_back(entry);
        }
        return copy;
    }

private:
    std::vector<std::list<FrontEntry>> gcl;
    mutable std::vector<std::shared_mutex> locks;
};

template class Gcl_SOPMOA<2>;
template class Gcl_SOPMOA<3>;
template class Gcl_SOPMOA<4>;
template class Gcl_SOPMOA<5>;

#endif
