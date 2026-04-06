#ifndef ALGORITHM_LAZY_LTMOA
#define ALGORITHM_LAZY_LTMOA

#include"../common_inc.h"
#include"../problem/graph.h"
#include"../problem/heuristic.h"
#include"../utils/cost.h"
#include"../utils/label.h"
#include"abstract_solver.h"
#include"gcl/gcl.h"

template <int N>
class LazyLTMOA: public AbstractSolver {
public:
    Heuristic<N> heuristic;

    LazyLTMOA(AdjacencyMatrix &adj_matrix, AdjacencyMatrix &inv_graph, size_t start_node, size_t target_node)
    : AbstractSolver(adj_matrix, start_node, target_node),
    gcl_ptr(std::make_unique<Gcl<N-1>>(adj_matrix.get_num_node())),
    heuristic(Heuristic<N>(target_node, inv_graph)) {}

    ~LazyLTMOA() { for (auto ptr : all_labels){ delete ptr; } }

    virtual std::string get_name() override {return "LazyLTMOA(" + std::to_string(N)+"obj)-" + gcl_ptr->get_name(); }
    void solve(double time_limit = std::numeric_limits<double>::infinity()) override;
    
private:
    std::list<Label<N>*> all_labels;
    std::unique_ptr<Gcl<N-1>> gcl_ptr;
};

std::shared_ptr<AbstractSolver> get_LazyLTMOA_solver(
    AdjacencyMatrix &graph, AdjacencyMatrix &inv_graph,
    size_t start_node, size_t target_node
);

#endif
