#ifndef PROBLEM_HEURISTIC
#define PROBLEM_HEURISTIC

#include"../common_inc.h"
#include"../utils/cost.h"
#include"graph.h"

template<int N>
class Heuristic {
public:
    Heuristic(size_t source, AdjacencyMatrix &adj_matrix, bool reverse=false);
    inline CostVec<N> & operator()(size_t node_id){ return this->data[node_id]; }
    inline const CostVec<N>* raw_data() const { return this->data.data(); }

private:
    std::vector<CostVec<N>> data;
    void dijkstra_ota(size_t source, size_t cost_idx, AdjacencyMatrix& adj_matrix, bool reverse);
};

#endif
