#ifndef ALGORITHMS_GCL_NWMOA
#define ALGORITHMS_GCL_NWMOA

#include"../../common_inc.h"
#include"../../utils/cost.h"

template<int N>
class Gcl_NWMOA {
public:
    using Snapshot = std::vector<CostVec<N>>;

    Gcl_NWMOA(size_t num_node) : gcl(num_node + 1) {};
    std::string get_name(){ return "custom_list2"; }

    inline bool frontier_check(size_t node, const CostVec<N> & cost) {
        if (node >= gcl.size()) { return false; }

        for (auto & vec : gcl[node]) {
            if (lex_smaller<N>(cost, vec)) { return false; }
            if (weakly_dominate<N>(vec, cost)) { return true; }
        }
        return false;
    }

    inline bool frontier_update(size_t node, const CostVec<N> & cost) {
        if (node >= gcl.size()) { 
            perror(("Invalid Gcl index " + std::to_string(node)).c_str());
            exit(EXIT_FAILURE);
            return false; 
        }

        for (const auto& existing : gcl[node]) {
            if (weakly_dominate<N>(existing, cost)) {
                return false;
            }
        }

        auto rit = gcl[node].rbegin();
        while (rit != gcl[node].rend()) {
            if (lex_smaller<N>(*rit, cost)) { break; }
            if (weakly_dominate<N>(cost, *rit)) {
                rit = decltype(rit)(gcl[node].erase(std::next(rit).base()));
            } else { rit++; }
        }
        gcl[node].insert(rit.base(), cost);
        return true;
    }

    inline Snapshot snapshot(size_t node) const {
        Snapshot copy;
        if (node >= gcl.size()) { return copy; }

        copy.reserve(gcl[node].size());
        for (const auto& cost : gcl[node]) {
            copy.push_back(cost);
        }
        return copy;
    }

private:
    std::vector<std::list<CostVec<N>>> gcl;
};

template class Gcl_NWMOA<1>;
template class Gcl_NWMOA<2>;
template class Gcl_NWMOA<3>;
template class Gcl_NWMOA<4>;
template class Gcl_NWMOA<5>;

#endif
