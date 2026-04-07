#ifndef ALGORITHMS_GCL
#define ALGORITHMS_GCL

#include"../../common_inc.h"
#include"../../utils/cost.h"

template<int N>
class Gcl {
public:
    struct FrontEntry {
        CostVec<N> cost{};
        double time_found = -1.0;
    };

    using Snapshot = std::vector<FrontEntry>;

    Gcl(size_t num_node) : gcl(num_node + 1) {};
    std::string get_name(){ return "list"; }
    
    inline bool frontier_check(size_t node, const CostVec<N> & cost) {
        if (node >= gcl.size()) { return false; }

        for (const auto& entry : gcl[node]) {
            if (weakly_dominate<N>(entry.cost, cost)) { return true; }
        }
        return false;
    }

    inline bool frontier_update(size_t node, const CostVec<N> & cost, double time_found = -1.0) {
        if (node >= gcl.size()) { 
            perror(("Invalid Gcl index " + std::to_string(node)).c_str());
            exit(EXIT_FAILURE);
            return false; 
        }

        for (auto& existing : gcl[node]) {
            if (existing.cost == cost) {
                if (existing.time_found < 0.0 || (time_found >= 0.0 && time_found < existing.time_found)) {
                    existing.time_found = time_found;
                }
                return false;
            }
            if (weakly_dominate<N>(existing.cost, cost)) {
                return false;
            }
        }

        auto it = gcl[node].begin();
        while (it != gcl[node].end()) {
            if (weakly_dominate<N>(cost, it->cost)) {
                it = gcl[node].erase(it);
            } else { it++; }
        }

        gcl[node].push_front(FrontEntry{cost, time_found});
        return true;
    }

    inline Snapshot snapshot(size_t node) const {
        Snapshot copy;
        if (node >= gcl.size()) { return copy; }

        copy.reserve(gcl[node].size());
        for (const auto& entry : gcl[node]) {
            copy.push_back(entry);
        }
        return copy;
    }

private:
    std::vector<std::list<FrontEntry>> gcl;
};

template class Gcl<1>;
template class Gcl<2>;
template class Gcl<3>;
template class Gcl<4>;
template class Gcl<5>;

#endif
