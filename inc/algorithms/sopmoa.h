#ifndef ALGORITHM_SOPMOA
#define ALGORITHM_SOPMOA

#include <omp.h>
#include <tbb/concurrent_priority_queue.h>
#include <tbb/concurrent_vector.h>

#include"../common_inc.h"
#include"../problem/graph.h"
#include"../problem/heuristic.h"
#include"../utils/cost.h"
#include"../utils/label.h"
#include"abstract_solver.h"
#include"gcl/gcl_sopmoa.h"

const int NUM_THREADS = 12;

template <int N>
class SOPMOA: public AbstractSolver {
public:
    Heuristic<N> heuristic;

    SOPMOA(AdjacencyMatrix &adj_matrix, AdjacencyMatrix &inv_graph, size_t start_node, size_t target_node, int num_threads)
    : AbstractSolver(adj_matrix, start_node, target_node),
    gcl_ptr(std::make_unique<Gcl_SOPMOA<N>>(adj_matrix.get_num_node())),
    heuristic(Heuristic<N>(target_node, inv_graph)), 
    num_threads(num_threads > 0 ? std::min(num_threads, NUM_THREADS) : NUM_THREADS) {}

    ~SOPMOA() {
        for (auto ptr : all_labels){ delete ptr; }
        gcl_ptr.reset();
    }

    virtual std::string get_name() override {return "SOPMOA(" + std::to_string(N)+"obj|"+ std::to_string(num_threads)+"threads)-" + gcl_ptr->get_name(); }
    void solve(unsigned int time_limit=UINT_MAX) override;

private:
    int num_threads;
    std::list<Label<N>*> all_labels;
    // tbb::concurrent_vector<Label<N>*> all_labels;

    std::unique_ptr<Gcl_SOPMOA<N>> gcl_ptr;

    tbb::concurrent_priority_queue<Label<N>*, typename Label<N>::lex_larger_for_PQ> open;
    std::mutex sols_lock;
    std::array<bool, NUM_THREADS> is_thread_activating;
    TimePoint search_start;

    virtual void thread_solve(int thread_ID, unsigned int time_limit);
};

std::shared_ptr<AbstractSolver> get_SOPMOA_solver(
    AdjacencyMatrix &graph, AdjacencyMatrix &inv_graph,
    size_t start_node, size_t target_node, int num_threads
);

#endif
