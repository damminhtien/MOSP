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

    Gcl(size_t num_node) : gcl(num_node + 1) {
        for (auto& frontier : gcl) {
            frontier.reserve(INITIAL_FRONTIER_CAPACITY);
        }
    }
    std::string get_name(){ return "list"; }
    
    inline bool frontier_check(size_t node, const CostVec<N> & cost) {
        if (node >= gcl.size()) { return false; }

        const NodeFrontier& frontier = gcl[node];
        for (const FrontEntry& entry : frontier) {
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

        NodeFrontier& frontier = gcl[node];
        for (FrontEntry& existing : frontier) {
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

        size_t write_idx = 0;
        for (size_t read_idx = 0; read_idx < frontier.size(); read_idx++) {
            if (weakly_dominate<N>(cost, frontier[read_idx].cost)) {
                continue;
            }

            if (write_idx != read_idx) {
                frontier[write_idx] = frontier[read_idx];
            }
            write_idx++;
        }
        frontier.resize(write_idx);

        frontier.push_back(FrontEntry{cost, time_found});
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

template class Gcl<1>;
template class Gcl<2>;
template class Gcl<3>;
template class Gcl<4>;
template class Gcl<5>;

#endif
