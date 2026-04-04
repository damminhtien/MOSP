#ifndef ALGORITHM_SOPMOA_BUCKET
#define ALGORITHM_SOPMOA_BUCKET

#include <omp.h>
#include <tbb/concurrent_priority_queue.h>
#include <tbb/concurrent_vector.h>

#include"../common_inc.h"
#include"../problem/graph.h"
#include"../problem/heuristic.h"
#include"../utils/cost.h"
#include"../utils/label.h"
#include"../utils/pq_bucket.h"
#include"abstract_solver.h"
#include"gcl/gcl_sopmoa.h"

const int NUM_THREADS_ = 16;

template <int N>
class SOPMOA_bucket: public AbstractSolver {
public:
    Heuristic<N> heuristic;

    SOPMOA_bucket(AdjacencyMatrix &adj_matrix, AdjacencyMatrix &inv_graph, size_t start_node, size_t target_node, int num_threads)
    : AbstractSolver(adj_matrix, start_node, target_node),
    gcl_ptr(std::make_unique<Gcl_SOPMOA<N>>(adj_matrix.get_num_node())),
    heuristic(Heuristic<N>(target_node, inv_graph)), 
    num_threads(num_threads > 0 ? std::min(num_threads, NUM_THREADS_) : NUM_THREADS_) {
        open = pq_bucket<N>(1, 0, 1000);
    }

    ~SOPMOA_bucket() {
        for (auto ptr : all_labels){ delete ptr; }
    }

    virtual std::string get_name() override {return "SOPMOA_bucket(" + std::to_string(N)+"obj|"+ std::to_string(num_threads)+"threads)-" + gcl_ptr->get_name(); }
    void solve(unsigned int time_limit=UINT_MAX) override;

private:
    int num_threads;
    std::list<Label<N>*> all_labels;
    // tbb::concurrent_vector<Label<N>*> all_labels;

    std::unique_ptr<Gcl_SOPMOA<N>> gcl_ptr;

    pq_bucket<N> open;
    std::mutex open_lock;
    std::mutex sols_lock;
    std::array<bool, NUM_THREADS_> is_thread_activating;

    virtual void thread_solve(int thread_ID, unsigned int time_limit);
};

std::shared_ptr<AbstractSolver> get_SOPMOA_bucket_solver(
    AdjacencyMatrix &graph, AdjacencyMatrix &inv_graph,
    size_t start_node, size_t target_node, int num_threads
);

#endif
