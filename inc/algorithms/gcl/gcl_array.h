#ifndef ALGORITHMS_GCL_ARRAY
#define ALGORITHMS_GCL_ARRAY

#include"../../common_inc.h"
#include"../../utils/cost.h"

const int ARRAY_SIZE=50000;

template<int N>
class Gcl_array {
public:
    struct FrontEntry {
        CostVec<N> cost{};
        double time_found = -1.0;
    };

    using Snapshot = std::vector<FrontEntry>;

    Gcl_array(size_t num_node) : gcl(num_node + 1) {
        for (size_t i = 0; i < num_node; i++) {
            gcl[i].reserve(ARRAY_SIZE);
        }
    };
    std::string get_name(){ return "array"; }
    
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

        gcl[node].push_back(FrontEntry{cost, time_found});
        size_t idx = gcl[node].size();
        while (idx > 1) {
            --idx;
            const size_t i = idx - 1;
            if (weakly_dominate<N>(cost, gcl[node][i].cost)) {
                gcl[node][i] = gcl[node].back();
                gcl[node].pop_back();
            }
        }
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
    std::vector<std::vector<FrontEntry>> gcl;
};

template class Gcl_array<1>;
template class Gcl_array<2>;
template class Gcl_array<3>;
template class Gcl_array<4>;
template class Gcl_array<5>;

#endif
