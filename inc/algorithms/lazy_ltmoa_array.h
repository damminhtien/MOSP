#ifndef ALGORITHM_LAZY_LTMOA_ARRAY
#define ALGORITHM_LAZY_LTMOA_ARRAY

#include"../common_inc.h"
#include"../problem/graph.h"
#include"../problem/heuristic.h"
#include"../utils/cost.h"
#include"../utils/label.h"
#include"abstract_solver.h"
#include"gcl/gcl_array.h"

template <int N>
class LazyLTMOA_array: public AbstractSolver {
public:
    Heuristic<N> heuristic;

    LazyLTMOA_array(AdjacencyMatrix &adj_matrix, AdjacencyMatrix &inv_graph, size_t start_node, size_t target_node)
    : AbstractSolver(adj_matrix, start_node, target_node),
    gcl_ptr(std::make_unique<Gcl_array<N-1>>(adj_matrix.get_num_node())),
    heuristic(Heuristic<N>(target_node, inv_graph)) {}

    ~LazyLTMOA_array() { for (auto ptr : all_labels){ delete ptr; } }

    virtual std::string get_name() override {return "LazyLTMOA(" + std::to_string(N)+"obj)-" + gcl_ptr->get_name(); }
    bool supports_canonical_benchmark_output() const override { return true; }
    void solve(double time_limit = std::numeric_limits<double>::infinity()) override;
    
private:
    std::list<Label<N>*> all_labels;
    std::unique_ptr<Gcl_array<N-1>> gcl_ptr;
    std::vector<FrontierPoint> target_frontier_;

    std::vector<FrontierPoint> collect_final_frontier() const override {
        return sort_frontier_lexicographically(normalize_frontier(target_frontier_));
    }
};

std::shared_ptr<AbstractSolver> get_LazyLTMOA_array_solver(
    AdjacencyMatrix &graph, AdjacencyMatrix &inv_graph,
    size_t start_node, size_t target_node
);

#endif
