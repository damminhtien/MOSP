#include"common_inc.h"

#include"algorithms/abstract_solver.h"
#include"algorithms/sopmoa.h"
#include"algorithms/sopmoa_relaxed.h"
#include"algorithms/sopmoa_bucket.h"
#include"algorithms/ltmoa.h"
#include"algorithms/lazy_ltmoa.h"
#include"algorithms/ltmoa_array.h"
#include"algorithms/lazy_ltmoa_array.h"
#include"algorithms/emoa.h"
#include"algorithms/nwmoa.h"

#include"utils/data_io.h"

#include <string>
#include <thread>
#include <boost/program_options.hpp>

namespace po = boost::program_options;
using namespace std;

namespace {

string build_default_dataset_id(const vector<string>& cost_files) {
    if (cost_files.empty()) {
        return "dataset";
    }

    string dataset_id;
    for (size_t i = 0; i < cost_files.size(); i++) {
        if (i > 0) {
            dataset_id += "__";
        }
        dataset_id += std::filesystem::path(cost_files[i]).stem().string();
    }
    return sanitize_artifact_component(dataset_id);
}

string build_single_query_id(size_t start_node, size_t target_node) {
    return "single_" + to_string(start_node) + "_" + to_string(target_node);
}

string resolve_query_id(const po::variables_map& vm, const string& fallback_query_id, bool scenario_mode) {
    const string explicit_query_id = vm["query-id"].as<string>();
    if (explicit_query_id.empty()) {
        return fallback_query_id;
    }

    if (!scenario_mode) {
        return sanitize_artifact_component(explicit_query_id);
    }

    return sanitize_artifact_component(explicit_query_id + "__" + fallback_query_id);
}

int effective_threads_for_algorithm(const string& algorithm, int requested_threads) {
    if (algorithm == "SOPMOA") {
        return requested_threads > 0 ? min(requested_threads, 12) : 12;
    }
    if (algorithm == "SOPMOA_bucket") {
        return requested_threads > 0 ? min(requested_threads, 16) : 16;
    }
    if (algorithm == "SOPMOA_relaxed") {
        if (requested_threads > 0) {
            return requested_threads;
        }
        return max(1u, thread::hardware_concurrency());
    }
    return 1;
}

bool canonical_output_enabled(const po::variables_map& vm) {
    return !vm["summary-output"].as<string>().empty()
        || !vm["frontier-output-dir"].as<string>().empty()
        || !vm["trace-output-dir"].as<string>().empty();
}

bool solver_supports_canonical_output(const AbstractSolver& solver) {
    return solver.supports_canonical_benchmark_output();
}

std::filesystem::path build_artifact_path(const std::filesystem::path& directory, const string& solver_name, const string& dataset_id, const string& query_id, const char* suffix) {
    if (directory.empty()) {
        return {};
    }

    const string filename =
        sanitize_artifact_component(solver_name) + "__"
        + sanitize_artifact_component(dataset_id) + "__"
        + sanitize_artifact_component(query_id)
        + suffix;
    return directory / filename;
}

}  // namespace

void single_run(
    AdjacencyMatrix& graph,
    AdjacencyMatrix& inv_graph,
    size_t start_node,
    size_t target_node,
    const string& algorithm,
    const string& dataset_id,
    const string& query_id,
    double budget_sec,
    po::variables_map& vm
) {
    std::shared_ptr<AbstractSolver> solver;

    if (algorithm == "SOPMOA") {
        int num_threads = vm["numthreads"].as<int>();
        solver = get_SOPMOA_solver(graph, inv_graph, start_node, target_node, num_threads);
    } else if (algorithm == "SOPMOA_relaxed") {
        int num_threads = vm["numthreads"].as<int>();
        solver = get_SOPMOA_relaxed_solver(graph, inv_graph, start_node, target_node, num_threads);
    } else if (algorithm == "SOPMOA_bucket") {
        int num_threads = vm["numthreads"].as<int>();
        solver = get_SOPMOA_bucket_solver(graph, inv_graph, start_node, target_node, num_threads);
    } else if (algorithm == "LTMOA") {
        solver = get_LTMOA_solver(graph, inv_graph, start_node, target_node);
    } else if (algorithm == "LazyLTMOA") {
        solver = get_LazyLTMOA_solver(graph, inv_graph, start_node, target_node);
    } else if (algorithm == "LTMOA_array") {
        solver = get_LTMOA_array_solver(graph, inv_graph, start_node, target_node);
    } else if (algorithm == "LazyLTMOA_array") {
        solver = get_LazyLTMOA_array_solver(graph, inv_graph, start_node, target_node);
    } else if (algorithm == "EMOA") {
        solver = get_EMOA_solver(graph, inv_graph, start_node, target_node);
    } else if (algorithm == "NWMOA") {
        solver = get_NWMOA_solver(graph, inv_graph, start_node, target_node);
    } else {
        exit(-1);
    }

    const bool use_canonical_output = canonical_output_enabled(vm);
    const auto external_start = BenchmarkClock::now();

    if (use_canonical_output && !solver_supports_canonical_output(*solver)) {
        std::cerr
            << "Canonical benchmark output is not available for solver "
            << solver->get_name()
            << std::endl;
        exit(EXIT_FAILURE);
    }

    if (use_canonical_output) {
        BenchmarkRunConfig config;
        config.dataset_id = dataset_id;
        config.query_id = query_id;
        config.threads_requested = vm["numthreads"].as<int>();
        config.threads_effective = effective_threads_for_algorithm(algorithm, config.threads_requested);
        config.budget_sec = budget_sec;
        config.trace_interval_ms = vm["trace-interval-ms"].as<uint64_t>();
        config.seed = vm["seed"].as<string>();
        solver->configure_benchmark_run(config);

        const std::filesystem::path frontier_path = build_artifact_path(
            vm["frontier-output-dir"].as<string>(),
            solver->get_name(),
            dataset_id,
            query_id,
            ".csv"
        );
        const std::filesystem::path trace_path = build_artifact_path(
            vm["trace-output-dir"].as<string>(),
            solver->get_name(),
            dataset_id,
            query_id,
            ".csv"
        );
        solver->set_benchmark_frontier_artifact_path(frontier_path);
        solver->set_benchmark_trace_artifact_path(trace_path);
    }

    RunMetrics metrics;
    double runtime = BenchmarkClock::seconds_since(external_start);
    bool failed = false;
    std::string failure_message;

    try {
        solver->solve(budget_sec);
    } catch (const std::bad_alloc& ex) {
        failed = true;
        failure_message = std::string("Solver ran out of memory: ") + ex.what();
        if (use_canonical_output) {
            solver->set_benchmark_status(RunStatus::oom);
        }
    } catch (const std::exception& ex) {
        failed = true;
        failure_message = std::string("Solver aborted with exception: ") + ex.what();
        if (use_canonical_output) {
            solver->set_benchmark_status(RunStatus::aborted);
        }
    } catch (...) {
        failed = true;
        failure_message = "Solver aborted with unknown exception";
        if (use_canonical_output) {
            solver->set_benchmark_status(RunStatus::aborted);
        }
    }

    if (use_canonical_output) {
        if (!solver->benchmark_status_set()) {
            solver->set_benchmark_status(failed ? RunStatus::aborted : RunStatus::completed);
        }
        metrics = solver->finalize_benchmark_run();
        runtime = metrics.runtime_sec;

        const std::string summary_output = vm["summary-output"].as<string>();
        if (!summary_output.empty()) {
            solver->benchmark_recorder().write_summary_row(summary_output);
        }
        if (!metrics.frontier_artifact_path.empty()) {
            solver->benchmark_recorder().write_frontier_csv(metrics.frontier_artifact_path);
        }
        if (!metrics.trace_artifact_path.empty()) {
            solver->benchmark_recorder().write_trace_csv(metrics.trace_artifact_path);
        }
    }

    if (failed) {
        std::cerr << failure_message << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Num solution: " << solver->solutions.size() << std::endl;
    std::cout << "Num expansion: " << solver->get_num_expansion() << std::endl;
    std::cout << "Num generation: " << solver->get_num_generation() << std::endl;
    std::cout << "Runtime: " << runtime << std::endl;

    solver.reset();
    
}

void scenarios_run(
    AdjacencyMatrix& graph,
    AdjacencyMatrix& inv_graph,
    const std::string& scen_path,
    const std::string& algorithm,
    const std::string& dataset_id,
    double budget_sec,
    po::variables_map& vm
) {
    auto scenarios = read_scenario_file(scen_path);

    int from_query = vm["from"].as<int>(); 
    int to_query = std::min(int(scenarios.size()), vm["to"].as<int>());

    for (int i = from_query; i < to_query; i++){
        const std::string& scenario_name = std::get<0>(scenarios[i]);
        size_t start = std::get<1>(scenarios[i]);
        size_t target = std::get<2>(scenarios[i]);

        std::cout << "Scenario " << i << "/" << scenarios.size() << ": ";
        std::cout << "start "<< start << " - target "<< target << std::endl;

        single_run(
            graph, inv_graph, 
            start, target, 
            algorithm,
            dataset_id,
            resolve_query_id(vm, scenario_name, true),
            budget_sec,
            vm
        );
    }
}

int main(int argc, char* argv[]) {
    std::vector<string> cost_files;

    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("start,s", po::value<int>()->default_value(-1), "start location")
        ("target,t", po::value<int>()->default_value(-1), "goal location")
        ("scenario", po::value<std::string>()->default_value(""), "the scenario file")
        ("from", po::value<int>()->default_value(0), "start from the i-th line of the scenario file")
        ("to", po::value<int>()->default_value(INT_MAX), "up to the i-th line of the scenario file")
        ("map,m",po::value< std::vector<string> >(&cost_files)->multitoken(), "files for edge weight")
        ("algorithm,a", po::value<std::string>()->default_value("SOPMOA"), "solvers [SOPMOA, SOPMOA_relaxed, SOPMOA_bucket, LTMOA, LazyLTMOA, LTMOA_array, LazyLTMOA_array, EMOA, NWMOA]")
        ("budget-sec", po::value<double>()->default_value(300.0), "benchmark budget in seconds")
        ("summary-output", po::value<std::string>()->default_value(""), "summary CSV path")
        ("frontier-output-dir", po::value<std::string>()->default_value(""), "directory for final frontier CSV artifacts")
        ("trace-output-dir", po::value<std::string>()->default_value(""), "directory for anytime trace CSV artifacts")
        ("dataset-id", po::value<std::string>()->default_value(""), "optional dataset identifier for benchmark artifacts")
        ("query-id", po::value<std::string>()->default_value(""), "optional query identifier override")
        ("trace-interval-ms", po::value<uint64_t>()->default_value(0), "trace sampling interval in milliseconds; 0 logs only on frontier changes")
        ("seed", po::value<std::string>()->default_value(""), "optional seed metadata for benchmark artifacts")
        ("numthreads,n", po::value<int>()->default_value(-1), "number of threads for SOPMOA, SOPMOA_relaxed and SOPMOA_bucket");
    
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 1;
    }

    po::notify(vm);

    const double budget_sec = vm["budget-sec"].as<double>();

    size_t num_node;
    size_t num_obj;
    std::vector<Edge> edges;

    for (auto file_path : cost_files){
        std::cout << file_path << std::endl;
    }

    read_graph_files(cost_files, num_node, num_obj, edges);
    std::cout << "Num node: " << num_node << std::endl;

    AdjacencyMatrix graph(num_node, num_obj, edges);
    AdjacencyMatrix inv_graph(num_node, num_obj, edges, true);

    const std::string dataset_id = vm["dataset-id"].as<std::string>().empty()
        ? build_default_dataset_id(cost_files)
        : sanitize_artifact_component(vm["dataset-id"].as<std::string>());

    if (
        vm["summary-output"].as<std::string>().empty()
        && vm["frontier-output-dir"].as<std::string>().empty()
        && vm["trace-output-dir"].as<std::string>().empty()
    ) {
        std::cerr << "Expected at least one output target: --summary-output, --frontier-output-dir, or --trace-output-dir" << std::endl;
        return 1;
    }

    if (vm["frontier-output-dir"].as<std::string>() != "") {
        std::filesystem::create_directories(vm["frontier-output-dir"].as<std::string>());
    }
    if (vm["trace-output-dir"].as<std::string>() != "") {
        std::filesystem::create_directories(vm["trace-output-dir"].as<std::string>());
    }

    if (vm["scenario"].as<std::string>() != ""){
        scenarios_run(
            graph, inv_graph, 
            vm["scenario"].as<std::string>(), 
            vm["algorithm"].as<std::string>(),
            dataset_id,
            budget_sec,
            vm
        );
    } else {
        single_run(
            graph, inv_graph, 
            vm["start"].as<int>(), vm["target"].as<int>(), 
            vm["algorithm"].as<std::string>(),
            dataset_id,
            resolve_query_id(vm, build_single_query_id(vm["start"].as<int>(), vm["target"].as<int>()), false),
            budget_sec,
            vm
        );
    }
    return 0;
}
