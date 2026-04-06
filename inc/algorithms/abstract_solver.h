#ifndef ALGORITHM_ABSTRACT_SOLVER
#define ALGORITHM_ABSTRACT_SOLVER

#include"../common_inc.h"
#include"../problem/graph.h"
#include"../problem/heuristic.h"
#include"../utils/benchmark_metrics.h"
#include"../utils/cost.h"
#include"../utils/label.h"

struct Solution {
    std::vector<cost_t> cost;
    double time_found;

    Solution(std::vector<cost_t> _cost = {}, double time=-1)
    : cost(_cost), time_found(time) {}
};

struct BenchmarkRunConfig {
    std::string dataset_id;
    std::string query_id;
    int threads_requested = 1;
    int threads_effective = 1;
    double budget_sec = std::numeric_limits<double>::infinity();
    uint64_t trace_interval_ms = 0;
    std::string seed;
};

class AbstractSolver {
public:
    std::vector<Solution> solutions;
    
    AbstractSolver(AdjacencyMatrix &adj_matrix, size_t start_node, size_t target_node)
    : graph(adj_matrix), start_node(start_node), target_node(target_node) {
        solutions.clear();
    }
    virtual ~AbstractSolver(){}

    virtual std::string get_name() {return "AbstractSolver"; }
    virtual bool supports_canonical_benchmark_output() const { return false; }

    size_t get_num_expansion(){ return num_expansion; }
    size_t get_num_generation(){ return num_generation; }

    virtual void solve(double time_limit = std::numeric_limits<double>::infinity()){
        perror("Solver not implemented");
        exit(EXIT_FAILURE);
        return; 
    };

    void configure_benchmark_run(const BenchmarkRunConfig& config) {
        RunMetrics template_metrics;
        template_metrics.solver_name = get_name();
        template_metrics.dataset_id = config.dataset_id;
        template_metrics.query_id = config.query_id;
        template_metrics.start_node = start_node;
        template_metrics.target_node = target_node;
        template_metrics.num_objectives = graph.get_num_obj();
        template_metrics.threads_requested = config.threads_requested;
        template_metrics.threads_effective = config.threads_effective;
        template_metrics.budget_sec = config.budget_sec;
        template_metrics.seed = config.seed;

        benchmark_recorder_.configure(template_metrics, config.trace_interval_ms);
        benchmark_enabled_ = true;
    }

    bool benchmark_enabled() const { return benchmark_enabled_ && benchmark_recorder_.is_configured(); }
    BenchmarkRecorder& benchmark_recorder() { return benchmark_recorder_; }
    const BenchmarkRecorder& benchmark_recorder() const { return benchmark_recorder_; }

    void set_benchmark_status(RunStatus status) {
        if (benchmark_enabled()) {
            benchmark_recorder_.set_status(status);
        }
    }

    bool benchmark_status_set() const {
        return benchmark_enabled() && benchmark_recorder_.has_status();
    }

    uint64_t benchmark_trace_interval_ms() const {
        return benchmark_enabled() ? benchmark_recorder_.trace_interval_ms() : 0;
    }

    void set_benchmark_frontier_artifact_path(const std::filesystem::path& csv_path) {
        if (benchmark_enabled()) {
            benchmark_recorder_.set_frontier_artifact_path(csv_path);
        }
    }

    void set_benchmark_trace_artifact_path(const std::filesystem::path& csv_path) {
        if (benchmark_enabled()) {
            benchmark_recorder_.set_trace_artifact_path(csv_path);
        }
    }

    RunMetrics finalize_benchmark_run() {
        if (!benchmark_enabled()) {
            return RunMetrics();
        }

        sync_benchmark_recorder();
        RunMetrics metrics = benchmark_recorder_.finalize(collect_final_frontier());
        num_generation = metrics.counters.generated_labels;
        num_expansion = metrics.counters.expanded_labels;
        return metrics;
    }

    std::string get_result_str(){
        std::stringstream ss;
        ss << get_name() << ","
        << start_node << "," << target_node << "," 
        << get_num_generation() << "," 
        << get_num_expansion() << "," 
        << solutions.size(); 
        std::string result = ss.str();
        return result;
    }

    std::string get_all_sols_str() {
        std::stringstream ss;
        for(auto sol : solutions) {
            ss << "[" << sol.cost[0];
            for (int i = 1; i < sol.cost.size(); i++) {
                ss << "," << sol.cost[i];
            }
            ss << "]" << std::endl;
        }
        std::string result = ss.str();
        return result;
    }

protected:
    virtual void sync_benchmark_recorder() {}

    double benchmark_elapsed_sec(const BenchmarkClock::time_point& local_start) const {
        if (benchmark_enabled()) {
            return benchmark_recorder_.elapsed_sec();
        }
        return BenchmarkClock::seconds_since(local_start);
    }

    void rebuild_solutions_from_frontier(const std::vector<FrontierPoint>& frontier) {
        const std::vector<FrontierPoint> sorted_frontier = sort_frontier_lexicographically(normalize_frontier(frontier));
        solutions.clear();
        solutions.reserve(sorted_frontier.size());

        for (const FrontierPoint& point : sorted_frontier) {
            solutions.emplace_back(point.cost, point.time_found_sec);
        }
    }

    virtual std::vector<FrontierPoint> collect_final_frontier() const {
        std::vector<FrontierPoint> frontier;
        frontier.reserve(solutions.size());

        for (const Solution& solution : solutions) {
            frontier.push_back({solution.cost, solution.time_found});
        }

        return sort_frontier_lexicographically(normalize_frontier(frontier));
    }

    AdjacencyMatrix & graph;
    BenchmarkRecorder benchmark_recorder_;
    bool benchmark_enabled_ = false;

    size_t num_expansion = 0;
    size_t num_generation = 0;

    size_t start_node;
    size_t target_node;
};

#endif
