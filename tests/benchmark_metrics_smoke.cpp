#include <cassert>
#include <cmath>
#include <functional>
#include <thread>

#include "algorithms/abstract_solver.h"
#include "algorithms/emoa.h"
#include "algorithms/gcl/gcl.h"
#include "algorithms/gcl/gcl_array.h"
#include "algorithms/gcl/gcl_nwmoa.h"
#include "algorithms/gcl/gcl_tree.h"
#include "algorithms/lazy_ltmoa.h"
#include "algorithms/lazy_ltmoa_array.h"
#include "algorithms/ltmoa.h"
#include "algorithms/ltmoa_array.h"
#include "algorithms/ltmoa_parallel.h"
#include "algorithms/ltmoa_array_superfast.h"
#include "algorithms/ltmoa_array_superfast_anytime.h"
#include "algorithms/ltmoa_array_superfast_lb.h"
#include "algorithms/nwmoa.h"
#include "algorithms/sopmoa.h"
#include "algorithms/sopmoa_bucket.h"
#include "algorithms/sopmoa_relaxed.h"
#include "problem/graph.h"
#include "utils/benchmark_metrics.h"

namespace {

std::array<cost_t, Edge::MAX_NUM_OBJ> edge_costs(cost_t c1, cost_t c2) {
    std::array<cost_t, Edge::MAX_NUM_OBJ> values{};
    values.fill(0);
    values[0] = c1;
    values[1] = c2;
    return values;
}

std::string read_file(const std::filesystem::path& file_path) {
    std::ifstream input(file_path);
    std::stringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void run_recorder_subcase(const std::filesystem::path& root) {
    BenchmarkRecorder recorder;

    RunMetrics metadata;
    metadata.solver_name = "RecorderOnly";
    metadata.dataset_id = "unit,dataset";
    metadata.query_id = "query\"alpha";
    metadata.start_node = 1;
    metadata.target_node = 9;
    metadata.num_objectives = 2;
    metadata.threads_requested = 1;
    metadata.threads_effective = 1;
    metadata.budget_sec = 3.5;
    metadata.seed = "42,seed";

    recorder.configure(metadata, 0);
    recorder.set_frontier_artifact_path(root / "recorder_frontier.csv");
    recorder.set_trace_artifact_path(root / "recorder_trace.csv");

    ThreadLocalSink& sink = recorder.main_thread_sink();
    sink.on_label_generated(1, 1);
    sink.on_label_generated(2, 2);
    sink.on_pruned_by_target();
    sink.on_label_expanded(2, 2);
    sink.on_target_hit_raw();

    std::vector<FrontierPoint> frontier_a = {
        {{2, 10}, 0.1},
    };
    std::vector<FrontierPoint> frontier_b = {
        {{2, 10}, 0.1},
        {{10, 2}, 0.2},
    };

    recorder.on_frontier_check_target();
    recorder.on_frontier_check_node();
    recorder.on_target_frontier_changed(frontier_a, "frontier,change");
    recorder.on_target_frontier_changed(frontier_b, "frontier,change");
    recorder.set_status(RunStatus::completed);
    RunMetrics finalized = recorder.finalize(frontier_b);

    recorder.write_summary_row(root / "summary.csv");
    recorder.write_frontier_csv(root / "recorder_frontier.csv");
    recorder.write_trace_csv(root / "recorder_trace.csv");

    assert(finalized.completed);
    assert(finalized.status == RunStatus::completed);
    assert(finalized.counters.generated_labels == 2);
    assert(finalized.counters.expanded_labels == 1);
    assert(finalized.counters.pruned_by_target == 1);
    assert(finalized.counters.target_hits_raw == 1);
    assert(finalized.counters.final_frontier_size == 2);
    assert(finalized.time_to_first_solution_sec >= 0.0);
    assert(finalized.anytime_trace.size() == 2);
    assert(std::isfinite(finalized.anytime_trace.back().hv_ratio));
    assert(finalized.anytime_trace.back().hv_ratio == 1.0);
    assert(finalized.anytime_trace.back().recall == 1.0);

    const std::string summary_text = read_file(root / "summary.csv");
    const std::string frontier_text = read_file(root / "recorder_frontier.csv");
    const std::string trace_text = read_file(root / "recorder_trace.csv");

    assert(summary_text.find("schema_version,solver_name,dataset_id,query_id") != std::string::npos);
    assert(summary_text.find("RecorderOnly") != std::string::npos);
    assert(summary_text.find("\"unit,dataset\"") != std::string::npos);
    assert(summary_text.find("\"query\"\"alpha\"") != std::string::npos);
    assert(summary_text.find("\"42,seed\"") != std::string::npos);
    assert(frontier_text.find("time_found_sec,obj1,obj2") != std::string::npos);
    assert(frontier_text.find("0.1,2,10") != std::string::npos);
    assert(trace_text.find("time_sec,trigger,frontier_size") != std::string::npos);
    assert(trace_text.find("\"frontier,change\"") != std::string::npos);
}

void run_recorder_summary_only_subcase() {
    BenchmarkRecorder recorder;

    RunMetrics metadata;
    metadata.solver_name = "RecorderSummaryOnly";
    metadata.dataset_id = "unit_dataset";
    metadata.query_id = "summary_only";
    metadata.start_node = 1;
    metadata.target_node = 9;
    metadata.num_objectives = 2;
    metadata.threads_requested = 1;
    metadata.threads_effective = 1;
    metadata.budget_sec = 1.0;

    recorder.configure(metadata, 1);
    std::vector<FrontierPoint> frontier = {
        {{2, 10}, 0.1},
        {{10, 2}, 0.2},
    };

    recorder.main_thread_sink().on_target_hit_raw();
    recorder.on_target_frontier_changed(frontier, "summary_only_first");
    recorder.set_status(RunStatus::completed);
    RunMetrics finalized = recorder.finalize(frontier);

    assert(finalized.completed);
    assert(finalized.time_to_first_solution_sec >= 0.0);
    assert(finalized.final_frontier.size() == 2);
    assert(finalized.anytime_trace.empty());
}

void run_recorder_counter_only_trace_subcase(const std::filesystem::path& root) {
    BenchmarkRecorder recorder;

    RunMetrics metadata;
    metadata.solver_name = "RecorderCounterOnlyTrace";
    metadata.dataset_id = "unit_dataset";
    metadata.query_id = "counter_only_trace";
    metadata.start_node = 1;
    metadata.target_node = 9;
    metadata.num_objectives = 2;
    metadata.threads_requested = 1;
    metadata.threads_effective = 1;
    metadata.budget_sec = 1.0;

    recorder.configure(metadata, 0);
    recorder.set_trace_artifact_path(root / "counter_only_trace.csv");

    CounterSet counters;
    counters.generated_labels = 7;
    counters.expanded_labels = 3;
    counters.pruned_by_target = 1;
    counters.target_hits_raw = 2;

    recorder.record_anytime_event(0.25, 2, counters, "counter_only_target_accept");
    recorder.set_status(RunStatus::timeout);

    const std::vector<FrontierPoint> final_frontier = {
        {{2, 10}, 0.1},
        {{10, 2}, 0.2},
    };
    RunMetrics finalized = recorder.finalize(final_frontier);

    assert(!finalized.completed);
    assert(finalized.status == RunStatus::timeout);
    assert(finalized.time_to_first_solution_sec == 0.25);
    assert(finalized.anytime_trace.size() == 1);
    assert(finalized.anytime_trace[0].frontier_size == 2);
    assert(std::isnan(finalized.anytime_trace[0].recall));
    assert(std::isnan(finalized.anytime_trace[0].hv_ratio));
}

void run_frontier_size_semantics_subcase() {
    BenchmarkRecorder recorder;

    RunMetrics metadata;
    metadata.solver_name = "FrontierSizeSemantics";
    metadata.dataset_id = "toy";
    metadata.query_id = "frontier_size";
    metadata.start_node = 0;
    metadata.target_node = 1;
    metadata.num_objectives = 2;

    recorder.configure(metadata, 0);
    ThreadLocalSink& sink = recorder.main_thread_sink();
    sink.on_target_hit_raw();
    sink.on_target_hit_raw();
    recorder.set_status(RunStatus::completed);

    const std::vector<FrontierPoint> final_frontier = {
        {{4, 4}, 0.1},
    };
    RunMetrics metrics = recorder.finalize(final_frontier);
    assert(metrics.counters.target_hits_raw == 2);
    assert(metrics.counters.final_frontier_size == 1);
}

class ClockSmokeSolver final : public AbstractSolver {
public:
    ClockSmokeSolver(AdjacencyMatrix& graph, size_t start_node, size_t target_node)
    : AbstractSolver(graph, start_node, target_node) {}

    std::string get_name() override { return "ClockSmokeSolver"; }
    bool supports_canonical_benchmark_output() const override { return true; }

    void solve(double /*time_limit*/) override {
        frontier_.clear();
        const auto local_start = BenchmarkClock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        record_target_frontier_accept(
            frontier_,
            FrontierPoint{{1, 1}, benchmark_elapsed_sec(local_start)},
            &benchmark_recorder().main_thread_sink()
        );
        set_benchmark_status(RunStatus::completed);
    }

private:
    std::vector<FrontierPoint> frontier_;

    std::vector<FrontierPoint> collect_final_frontier() const override {
        return frontier_;
    }
};
AdjacencyMatrix make_graph() {
    std::vector<Edge> edges;
    edges.push_back(Edge(0, 1, edge_costs(1, 5)));
    edges.push_back(Edge(1, 3, edge_costs(1, 5)));
    edges.push_back(Edge(0, 2, edge_costs(5, 1)));
    edges.push_back(Edge(2, 3, edge_costs(5, 1)));
    edges.push_back(Edge(0, 3, edge_costs(6, 6)));
    edges.push_back(Edge(1, 2, edge_costs(1, 1)));
    edges.push_back(Edge(2, 1, edge_costs(1, 1)));
    return AdjacencyMatrix(4, 2, edges);
}

AdjacencyMatrix make_inv_graph() {
    std::vector<Edge> edges;
    edges.push_back(Edge(0, 1, edge_costs(1, 5)));
    edges.push_back(Edge(1, 3, edge_costs(1, 5)));
    edges.push_back(Edge(0, 2, edge_costs(5, 1)));
    edges.push_back(Edge(2, 3, edge_costs(5, 1)));
    edges.push_back(Edge(0, 3, edge_costs(6, 6)));
    edges.push_back(Edge(1, 2, edge_costs(1, 1)));
    edges.push_back(Edge(2, 1, edge_costs(1, 1)));
    return AdjacencyMatrix(4, 2, edges, true);
}

void assert_valid_frontier(const RunMetrics& metrics) {
    assert(!metrics.final_frontier.empty());
    assert(!metrics.anytime_trace.empty());
    for (const AnytimePoint& point : metrics.anytime_trace) {
        assert(std::isfinite(point.recall));
        assert(std::isfinite(point.hv_ratio));
    }
    assert(metrics.anytime_trace.back().recall == 1.0);
    assert(metrics.anytime_trace.back().hv_ratio == 1.0);
}

AdjacencyMatrix make_counter_semantics_graph() {
    std::vector<Edge> edges;
    edges.push_back(Edge(0, 1, edge_costs(1, 1)));
    edges.push_back(Edge(0, 2, edge_costs(5, 5)));
    edges.push_back(Edge(1, 3, edge_costs(1, 1)));
    edges.push_back(Edge(2, 3, edge_costs(1, 1)));
    return AdjacencyMatrix(4, 2, edges);
}

AdjacencyMatrix make_counter_semantics_inv_graph() {
    std::vector<Edge> edges;
    edges.push_back(Edge(0, 1, edge_costs(1, 1)));
    edges.push_back(Edge(0, 2, edge_costs(5, 5)));
    edges.push_back(Edge(1, 3, edge_costs(1, 1)));
    edges.push_back(Edge(2, 3, edge_costs(1, 1)));
    return AdjacencyMatrix(4, 2, edges, true);
}

void run_counter_semantics_solver_subcase() {
    AdjacencyMatrix graph = make_counter_semantics_graph();
    AdjacencyMatrix inv_graph = make_counter_semantics_inv_graph();
    std::shared_ptr<AbstractSolver> solver = get_LTMOA_solver(graph, inv_graph, 0, 3);

    BenchmarkRunConfig config;
    config.dataset_id = "counter_graph";
    config.query_id = "ltmoa_counter_semantics";
    config.threads_requested = 1;
    config.threads_effective = 1;
    config.budget_sec = 5.0;
    solver->configure_benchmark_run(config);

    solver->solve(5.0);
    if (!solver->benchmark_status_set()) {
        solver->set_benchmark_status(RunStatus::completed);
    }

    RunMetrics metrics = solver->finalize_benchmark_run();
    assert(metrics.status == RunStatus::completed);
    assert(metrics.counters.generated_labels == 4);
    assert(metrics.counters.expanded_labels == 2);
    assert(metrics.counters.pruned_by_target == 1);
    assert(metrics.counters.pruned_by_node == 0);
    assert(metrics.counters.pruned_other == 0);
    assert(metrics.counters.target_hits_raw == 1);
    assert(metrics.counters.final_frontier_size == 1);
}

void run_clock_solver_subcase() {
    AdjacencyMatrix graph = make_counter_semantics_graph();
    ClockSmokeSolver solver(graph, 0, 3);

    BenchmarkRunConfig config;
    config.dataset_id = "clock_graph";
    config.query_id = "clock_smoke";
    config.threads_requested = 1;
    config.threads_effective = 1;
    config.budget_sec = 1.0;
    solver.configure_benchmark_run(config);

    const auto external_start = BenchmarkClock::now();
    solver.solve(1.0);
    RunMetrics metrics = solver.finalize_benchmark_run();
    const double external_runtime = BenchmarkClock::seconds_since(external_start);

    assert(metrics.status == RunStatus::completed);
    assert(metrics.runtime_sec >= 0.015);
    assert(metrics.runtime_sec <= external_runtime + 0.05);
    assert(metrics.time_to_first_solution_sec >= 0.015);
}

void assert_expected_frontier(const RunMetrics& metrics) {
    assert(metrics.final_frontier.size() == 3);
    assert(metrics.final_frontier[0].cost == std::vector<cost_t>({2, 10}));
    assert(metrics.final_frontier[1].cost == std::vector<cost_t>({6, 6}));
    assert(metrics.final_frontier[2].cost == std::vector<cost_t>({10, 2}));
    assert_valid_frontier(metrics);
}

template <typename GclType>
void run_gcl_snapshot_subcase(GclType& gcl) {
    CostVec<2> first{};
    first[0] = 2;
    first[1] = 8;

    CostVec<2> second{};
    second[0] = 8;
    second[1] = 2;

    assert(gcl.frontier_update(1, first, 0.4));
    assert(gcl.frontier_update(1, second, 0.6));
    assert(!gcl.frontier_update(1, first, 0.2));

    auto snapshot = gcl.snapshot(1);
    std::vector<FrontierPoint> frontier;
    frontier.reserve(snapshot.size());
    for (const auto& entry : snapshot) {
        frontier.push_back({
            std::vector<cost_t>(entry.cost.begin(), entry.cost.end()),
            entry.time_found
        });
    }

    frontier = sort_frontier_lexicographically(normalize_frontier(frontier));
    assert(frontier.size() == 2);
    assert(frontier[0].cost == std::vector<cost_t>({2, 8}));
    assert(frontier[0].time_found_sec == 0.2);
    assert(frontier[1].cost == std::vector<cost_t>({8, 2}));
    assert(frontier[1].time_found_sec == 0.6);
}

void run_gcl_nwmoa_ordering_subcase() {
    Gcl_NWMOA<2> gcl(4);

    CostVec<2> mid{};
    mid[0] = 6;
    mid[1] = 6;

    CostVec<2> low{};
    low[0] = 2;
    low[1] = 8;

    CostVec<2> high{};
    high[0] = 8;
    high[1] = 2;

    assert(gcl.frontier_update(1, mid, 0.5));
    assert(gcl.frontier_update(1, high, 0.6));
    assert(gcl.frontier_update(1, low, 0.4));

    auto snapshot = gcl.snapshot(1);
    assert(snapshot.size() == 3);
    assert(snapshot[0].cost == low);
    assert(snapshot[1].cost == mid);
    assert(snapshot[2].cost == high);
    assert(gcl.frontier_check(1, low));
    assert(gcl.frontier_check(1, mid));
    assert(gcl.frontier_check(1, high));
}

void run_gcl_tree_ordering_subcase() {
    Gcl_tree<2> gcl(4);

    CostVec<2> mid{};
    mid[0] = 6;
    mid[1] = 6;

    CostVec<2> low{};
    low[0] = 2;
    low[1] = 8;

    CostVec<2> high{};
    high[0] = 8;
    high[1] = 2;

    assert(gcl.frontier_update(1, mid, 0.5));
    assert(gcl.frontier_update(1, high, 0.6));
    assert(gcl.frontier_update(1, low, 0.4));

    auto snapshot = gcl.snapshot(1);
    assert(snapshot.size() == 3);
    assert(snapshot[0].cost == low);
    assert(snapshot[1].cost == mid);
    assert(snapshot[2].cost == high);
    assert(gcl.frontier_check(1, low));
    assert(gcl.frontier_check(1, mid));
    assert(gcl.frontier_check(1, high));
}

void run_gcl_matrix_subcase() {
    Gcl<2> list_gcl(4);
    Gcl_array<2> array_gcl(4);
    Gcl_tree<2> tree_gcl(4);
    Gcl_NWMOA<2> nwmoa_gcl(4);

    run_gcl_snapshot_subcase(list_gcl);
    run_gcl_snapshot_subcase(array_gcl);
    run_gcl_snapshot_subcase(tree_gcl);
    run_gcl_snapshot_subcase(nwmoa_gcl);
    run_gcl_tree_ordering_subcase();
    run_gcl_nwmoa_ordering_subcase();
}

void run_solver_subcase(
    const std::filesystem::path& root,
    const std::string& label,
    bool expect_exact_frontier,
    const std::function<std::shared_ptr<AbstractSolver>(AdjacencyMatrix&, AdjacencyMatrix&)>& make_solver
) {
    AdjacencyMatrix graph = make_graph();
    AdjacencyMatrix inv_graph = make_inv_graph();
    std::shared_ptr<AbstractSolver> solver = make_solver(graph, inv_graph);

    BenchmarkRunConfig config;
    config.dataset_id = "toy_graph";
    config.query_id = label;
    config.threads_requested = 2;
    config.threads_effective = 2;
    config.budget_sec = 5.0;
    config.trace_interval_ms = 1;
    solver->configure_benchmark_run(config);
    solver->set_benchmark_frontier_artifact_path(root / (label + "_frontier.csv"));
    solver->set_benchmark_trace_artifact_path(root / (label + "_trace.csv"));

    solver->solve(5.0);
    if (!solver->benchmark_status_set()) {
        solver->set_benchmark_status(RunStatus::completed);
    }

    RunMetrics metrics = solver->finalize_benchmark_run();
    solver->benchmark_recorder().write_frontier_csv(root / (label + "_frontier.csv"));
    solver->benchmark_recorder().write_trace_csv(root / (label + "_trace.csv"));
    solver->benchmark_recorder().write_summary_row(root / "solver_summary.csv");

    assert(metrics.status == RunStatus::completed);
    assert(metrics.completed);
    assert(metrics.counters.generated_labels >= metrics.counters.expanded_labels);
    assert(metrics.counters.target_hits_raw >= metrics.counters.final_frontier_size);
    assert(metrics.counters.final_frontier_size > 0);
    assert(metrics.counters.peak_open_size > 0);
    assert(metrics.counters.peak_live_labels > 0);
    assert(metrics.time_to_first_solution_sec >= 0.0);
    assert(metrics.time_to_first_solution_sec <= metrics.runtime_sec);
    if (expect_exact_frontier) {
        assert_expected_frontier(metrics);
    } else {
        assert(!metrics.anytime_trace.empty());
        for (const AnytimePoint& point : metrics.anytime_trace) {
            assert(std::isfinite(point.recall));
            assert(std::isfinite(point.hv_ratio));
        }
    }

    const std::string trace_text = read_file(root / (label + "_trace.csv"));
    assert(trace_text.find("nan") == std::string::npos);
}

void run_solver_summary_only_subcase() {
    AdjacencyMatrix graph = make_graph();
    AdjacencyMatrix inv_graph = make_inv_graph();
    const std::vector<std::pair<std::string, std::function<std::shared_ptr<AbstractSolver>(AdjacencyMatrix&, AdjacencyMatrix&)>>> cases = {
        {"ltmoa_array_superfast_summary_only", [](AdjacencyMatrix& graph_ref, AdjacencyMatrix& inv_graph_ref) { return get_LTMOA_array_superfast_solver(graph_ref, inv_graph_ref, 0, 3); }},
        {"ltmoa_array_superfast_anytime_summary_only", [](AdjacencyMatrix& graph_ref, AdjacencyMatrix& inv_graph_ref) { return get_LTMOA_array_superfast_anytime_solver(graph_ref, inv_graph_ref, 0, 3); }},
        {"ltmoa_array_superfast_lb_summary_only", [](AdjacencyMatrix& graph_ref, AdjacencyMatrix& inv_graph_ref) { return get_LTMOA_array_superfast_lb_solver(graph_ref, inv_graph_ref, 0, 3); }},
        {"ltmoa_parallel_summary_only", [](AdjacencyMatrix& graph_ref, AdjacencyMatrix& inv_graph_ref) { return get_LTMOA_parallel_solver(graph_ref, inv_graph_ref, 0, 3, 2); }},
    };

    for (const auto& entry : cases) {
        std::shared_ptr<AbstractSolver> solver = entry.second(graph, inv_graph);

        BenchmarkRunConfig config;
        config.dataset_id = "toy_graph";
        config.query_id = entry.first;
        config.threads_requested = 1;
        config.threads_effective = 1;
        config.budget_sec = 5.0;
        config.trace_interval_ms = 1;
        solver->configure_benchmark_run(config);

        solver->solve(5.0);
        if (!solver->benchmark_status_set()) {
            solver->set_benchmark_status(RunStatus::completed);
        }

        RunMetrics metrics = solver->finalize_benchmark_run();
        assert(metrics.status == RunStatus::completed);
        assert(metrics.counters.final_frontier_size > 0);
        assert(metrics.time_to_first_solution_sec >= 0.0);
        assert(metrics.anytime_trace.empty());
    }
}

void run_solver_matrix_subcase(const std::filesystem::path& root) {
    struct SolverCase {
        std::string label;
        bool expect_exact_frontier;
        std::function<std::shared_ptr<AbstractSolver>(AdjacencyMatrix&, AdjacencyMatrix&)> make_solver;
    };

    const std::vector<SolverCase> cases = {
        {"ltmoa", true, [](AdjacencyMatrix& graph, AdjacencyMatrix& inv_graph) { return get_LTMOA_solver(graph, inv_graph, 0, 3); }},
        {"lazy_ltmoa", true, [](AdjacencyMatrix& graph, AdjacencyMatrix& inv_graph) { return get_LazyLTMOA_solver(graph, inv_graph, 0, 3); }},
        {"ltmoa_array", true, [](AdjacencyMatrix& graph, AdjacencyMatrix& inv_graph) { return get_LTMOA_array_solver(graph, inv_graph, 0, 3); }},
        {"ltmoa_array_superfast", true, [](AdjacencyMatrix& graph, AdjacencyMatrix& inv_graph) { return get_LTMOA_array_superfast_solver(graph, inv_graph, 0, 3); }},
        {"ltmoa_array_superfast_anytime", true, [](AdjacencyMatrix& graph, AdjacencyMatrix& inv_graph) { return get_LTMOA_array_superfast_anytime_solver(graph, inv_graph, 0, 3); }},
        {"ltmoa_array_superfast_lb", true, [](AdjacencyMatrix& graph, AdjacencyMatrix& inv_graph) { return get_LTMOA_array_superfast_lb_solver(graph, inv_graph, 0, 3); }},
        {"ltmoa_parallel", true, [](AdjacencyMatrix& graph, AdjacencyMatrix& inv_graph) { return get_LTMOA_parallel_solver(graph, inv_graph, 0, 3, 2); }},
        {"lazy_ltmoa_array", true, [](AdjacencyMatrix& graph, AdjacencyMatrix& inv_graph) { return get_LazyLTMOA_array_solver(graph, inv_graph, 0, 3); }},
        {"emoa", true, [](AdjacencyMatrix& graph, AdjacencyMatrix& inv_graph) { return get_EMOA_solver(graph, inv_graph, 0, 3); }},
        {"nwmoa", true, [](AdjacencyMatrix& graph, AdjacencyMatrix& inv_graph) { return get_NWMOA_solver(graph, inv_graph, 0, 3); }},
        {"sopmoa", false, [](AdjacencyMatrix& graph, AdjacencyMatrix& inv_graph) { return get_SOPMOA_solver(graph, inv_graph, 0, 3, 2); }},
        {"sopmoa_relaxed", true, [](AdjacencyMatrix& graph, AdjacencyMatrix& inv_graph) { return get_SOPMOA_relaxed_solver(graph, inv_graph, 0, 3, 2); }},
    };

    for (const auto& entry : cases) {
        run_solver_subcase(root, entry.label, entry.expect_exact_frontier, entry.make_solver);
    }
}

}  // namespace

int main() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("sopmoa_benchmark_metrics_smoke_" + std::to_string(static_cast<long long>(std::time(nullptr))));

    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    run_recorder_subcase(root);
    run_recorder_summary_only_subcase();
    run_recorder_counter_only_trace_subcase(root);
    run_frontier_size_semantics_subcase();
    run_counter_semantics_solver_subcase();
    run_clock_solver_subcase();
    run_gcl_matrix_subcase();
    run_solver_summary_only_subcase();
    run_solver_matrix_subcase(root);

    std::filesystem::remove_all(root);
    return 0;
}
