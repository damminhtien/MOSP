#include"common_inc.h"

#include"algorithms/abstract_solver.h"
#include"algorithms/sopmoa.h"
#include"algorithms/sopmoa_relaxed.h"
#include"algorithms/sopmoa_bucket.h"
#include"algorithms/ltmoa.h"
#include"algorithms/lazy_ltmoa.h"
#include"algorithms/ltmoa_array.h"
#include"algorithms/ltmoa_array_superfast.h"
#include"algorithms/ltmoa_array_superfast_anytime.h"
#include"algorithms/ltmoa_array_superfast_lb.h"
#include"algorithms/ltmoa_parallel.h"
#include"algorithms/lazy_ltmoa_array.h"
#include"algorithms/emoa.h"
#include"algorithms/nwmoa.h"

#include"utils/data_io.h"
#include"utils/thread_config.h"

#include <string>
#include <thread>
#include <boost/program_options.hpp>

namespace po = boost::program_options;
using namespace std;

namespace {

[[noreturn]] void fail_cli(const string& message) {
    throw std::runtime_error("CLI error: " + message);
}

bool option_was_explicit(const po::variables_map& vm, const char* option_name) {
    auto it = vm.find(option_name);
    return it != vm.end() && !it->second.defaulted();
}

bool is_supported_algorithm(const string& algorithm) {
    static const std::array<std::string_view, 13> supported = {
        "SOPMOA",
        "SOPMOA_relaxed",
        "SOPMOA_bucket",
        "LTMOA",
        "LazyLTMOA",
        "LTMOA_array",
        "LTMOA_array_superfast",
        "LTMOA_array_superfast_anytime",
        "LTMOA_array_superfast_lb",
        "LTMOA_parallel",
        "LazyLTMOA_array",
        "EMOA",
        "NWMOA",
    };

    return std::find(supported.begin(), supported.end(), algorithm) != supported.end();
}

size_t validated_node_id(const char* option_name, int raw_value, size_t num_node) {
    if (raw_value < 0) {
        fail_cli(string("--") + option_name + " must be >= 0");
    }

    const size_t value = static_cast<size_t>(raw_value);
    if (value >= num_node) {
        fail_cli(
            string("--") + option_name + " out of range for graph with "
            + to_string(num_node) + " nodes"
        );
    }

    return value;
}

uint64_t validate_trace_interval_ms(const po::variables_map& vm) {
    const long long trace_interval_ms = vm["trace-interval-ms"].as<long long>();
    if (trace_interval_ms < 0) {
        fail_cli("--trace-interval-ms must be >= 0");
    }
    return static_cast<uint64_t>(trace_interval_ms);
}

double validate_budget_sec(const po::variables_map& vm) {
    const double budget_sec = vm["budget-sec"].as<double>();
    if (!std::isfinite(budget_sec) || budget_sec < 0.0) {
        fail_cli("--budget-sec must be finite and >= 0");
    }
    return budget_sec;
}

void ensure_directory_path(const string& path_string, const char* option_name) {
    if (path_string.empty()) {
        return;
    }

    const std::filesystem::path path(path_string);
    std::error_code ec;
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) {
        fail_cli(string("cannot access ") + option_name + ": " + ec.message());
    }

    if (exists) {
        const bool is_directory = std::filesystem::is_directory(path, ec);
        if (ec) {
            fail_cli(string("cannot inspect ") + option_name + ": " + ec.message());
        }
        if (!is_directory) {
            fail_cli(string(option_name) + " must point to a directory");
        }
        return;
    }

    std::filesystem::create_directories(path, ec);
    if (ec) {
        fail_cli(string("cannot create ") + option_name + ": " + ec.message());
    }
}

void ensure_parent_directory_for_file(const string& file_path_string, const char* option_name) {
    if (file_path_string.empty()) {
        return;
    }

    const std::filesystem::path parent = std::filesystem::path(file_path_string).parent_path();
    if (!parent.empty()) {
        ensure_directory_path(parent.string(), option_name);
    }
}

void validate_map_file_count(const vector<string>& cost_files) {
    if (cost_files.size() < 2 || cost_files.size() > Edge::MAX_NUM_OBJ) {
        fail_cli("--map requires between 2 and 5 objective files");
    }
}

void validate_mode_specific_arguments(const po::variables_map& vm, bool scenario_mode) {
    if (scenario_mode) {
        if (option_was_explicit(vm, "start") || option_was_explicit(vm, "target")) {
            fail_cli("--scenario cannot be combined with --start or --target");
        }
        return;
    }

    if (option_was_explicit(vm, "from") || option_was_explicit(vm, "to")) {
        fail_cli("--from and --to require --scenario");
    }

    if (!option_was_explicit(vm, "start") || !option_was_explicit(vm, "target")) {
        fail_cli("single-query mode requires both --start and --target");
    }
}

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
    const SolverThreadConfig& thread_config,
    uint64_t trace_interval_ms,
    po::variables_map& vm
) {
    std::shared_ptr<AbstractSolver> solver;

    if (algorithm == "SOPMOA") {
        solver = get_SOPMOA_solver(graph, inv_graph, start_node, target_node, thread_config.effective_threads);
    } else if (algorithm == "SOPMOA_relaxed") {
        solver = get_SOPMOA_relaxed_solver(graph, inv_graph, start_node, target_node, thread_config.effective_threads);
    } else if (algorithm == "SOPMOA_bucket") {
        solver = get_SOPMOA_bucket_solver(graph, inv_graph, start_node, target_node, thread_config.effective_threads);
    } else if (algorithm == "LTMOA") {
        solver = get_LTMOA_solver(graph, inv_graph, start_node, target_node);
    } else if (algorithm == "LazyLTMOA") {
        solver = get_LazyLTMOA_solver(graph, inv_graph, start_node, target_node);
    } else if (algorithm == "LTMOA_array") {
        solver = get_LTMOA_array_solver(graph, inv_graph, start_node, target_node);
    } else if (algorithm == "LTMOA_array_superfast") {
        solver = get_LTMOA_array_superfast_solver(graph, inv_graph, start_node, target_node);
    } else if (algorithm == "LTMOA_array_superfast_anytime") {
        solver = get_LTMOA_array_superfast_anytime_solver(graph, inv_graph, start_node, target_node);
    } else if (algorithm == "LTMOA_array_superfast_lb") {
        solver = get_LTMOA_array_superfast_lb_solver(graph, inv_graph, start_node, target_node);
    } else if (algorithm == "LTMOA_parallel") {
        int num_threads = vm["numthreads"].as<int>();
        solver = get_LTMOA_parallel_solver(graph, inv_graph, start_node, target_node, num_threads);
    } else if (algorithm == "LazyLTMOA_array") {
        solver = get_LazyLTMOA_array_solver(graph, inv_graph, start_node, target_node);
    } else if (algorithm == "EMOA") {
        solver = get_EMOA_solver(graph, inv_graph, start_node, target_node);
    } else if (algorithm == "NWMOA") {
        solver = get_NWMOA_solver(graph, inv_graph, start_node, target_node);
    } else {
        fail_cli("unknown algorithm: " + algorithm);
    }

    if (solver == nullptr) {
        fail_cli(
            "solver " + algorithm + " does not support objective count "
            + to_string(graph.get_num_obj())
        );
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
        config.threads_requested = thread_config.requested_threads;
        config.threads_effective = thread_config.effective_threads;
        config.budget_sec = budget_sec;
        config.trace_interval_ms = trace_interval_ms;
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
    const SolverThreadConfig& thread_config,
    uint64_t trace_interval_ms,
    po::variables_map& vm
) {
    auto scenarios = read_scenario_file(scen_path);

    const int from_query = vm["from"].as<int>();
    const int raw_to_query = vm["to"].as<int>();
    const int to_query = option_was_explicit(vm, "to") ? raw_to_query : static_cast<int>(scenarios.size());

    if (from_query < 0) {
        fail_cli("--from must be >= 0");
    }
    if (to_query < 0) {
        fail_cli("--to must be >= 0");
    }
    if (from_query > to_query) {
        fail_cli("--from must be <= --to");
    }
    if (to_query > static_cast<int>(scenarios.size())) {
        fail_cli("--to exceeds scenario count");
    }
    if (from_query >= static_cast<int>(scenarios.size())) {
        fail_cli("--from exceeds scenario count");
    }

    for (int i = from_query; i < to_query; i++){
        const std::string& scenario_name = std::get<0>(scenarios[i]);
        size_t start = std::get<1>(scenarios[i]);
        size_t target = std::get<2>(scenarios[i]);

        if (start >= static_cast<size_t>(graph.get_num_node()) || target >= static_cast<size_t>(graph.get_num_node())) {
            fail_cli(
                "scenario " + scenario_name + " has node ids outside graph bounds"
            );
        }

        std::cout << "Scenario " << i << "/" << scenarios.size() << ": ";
        std::cout << "start "<< start << " - target "<< target << std::endl;

        single_run(
            graph, inv_graph, 
            start, target, 
            algorithm,
            dataset_id,
            resolve_query_id(vm, scenario_name, true),
            budget_sec,
            thread_config,
            trace_interval_ms,
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
        ("algorithm,a", po::value<std::string>()->default_value("SOPMOA"), "solvers [SOPMOA, SOPMOA_relaxed, SOPMOA_bucket, LTMOA, LazyLTMOA, LTMOA_array, LTMOA_array_superfast, LTMOA_array_superfast_anytime, LTMOA_array_superfast_lb, LTMOA_parallel, LazyLTMOA_array, EMOA, NWMOA]")
        ("budget-sec", po::value<double>()->default_value(300.0), "benchmark budget in seconds")
        ("summary-output", po::value<std::string>()->default_value(""), "summary CSV path")
        ("frontier-output-dir", po::value<std::string>()->default_value(""), "directory for final frontier CSV artifacts")
        ("trace-output-dir", po::value<std::string>()->default_value(""), "directory for anytime trace CSV artifacts")
        ("dataset-id", po::value<std::string>()->default_value(""), "optional dataset identifier for benchmark artifacts")
        ("query-id", po::value<std::string>()->default_value(""), "optional query identifier override")
        ("trace-interval-ms", po::value<long long>()->default_value(0), "trace sampling interval in milliseconds; 0 logs only on frontier changes")
        ("seed", po::value<std::string>()->default_value(""), "optional seed metadata for benchmark artifacts")
        ("numthreads,n", po::value<int>()->default_value(-1), "number of threads for SOPMOA, SOPMOA_relaxed, SOPMOA_bucket and LTMOA_parallel");
    
    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help")) {
            std::cout << desc << std::endl;
            return 0;
        }

        po::notify(vm);

        const string algorithm = vm["algorithm"].as<std::string>();
        if (!is_supported_algorithm(algorithm)) {
            fail_cli("unknown algorithm: " + algorithm);
        }

        validate_map_file_count(cost_files);

        const bool scenario_mode = !vm["scenario"].as<std::string>().empty();
        validate_mode_specific_arguments(vm, scenario_mode);

        SolverThreadConfig thread_config;
        try {
            thread_config = resolve_solver_thread_config(
                algorithm,
                vm["numthreads"].as<int>(),
                option_was_explicit(vm, "numthreads")
            );
        } catch (const std::invalid_argument& ex) {
            fail_cli(ex.what());
        }
        const uint64_t trace_interval_ms = validate_trace_interval_ms(vm);
        const double budget_sec = validate_budget_sec(vm);
        emit_solver_thread_config_log(std::cerr, algorithm, thread_config);

        if (
            vm["summary-output"].as<std::string>().empty()
            && vm["frontier-output-dir"].as<std::string>().empty()
            && vm["trace-output-dir"].as<std::string>().empty()
        ) {
            fail_cli("expected at least one output target: --summary-output, --frontier-output-dir, or --trace-output-dir");
        }

        ensure_parent_directory_for_file(vm["summary-output"].as<std::string>(), "--summary-output");
        ensure_directory_path(vm["frontier-output-dir"].as<std::string>(), "--frontier-output-dir");
        ensure_directory_path(vm["trace-output-dir"].as<std::string>(), "--trace-output-dir");

        size_t num_node = 0;
        size_t num_obj = 0;
        std::vector<Edge> edges;

        for (const auto& file_path : cost_files){
            std::cout << file_path << std::endl;
        }

        read_graph_files(cost_files, num_node, num_obj, edges);
        if (num_obj < 2 || num_obj > Edge::MAX_NUM_OBJ) {
            fail_cli("solver objective count must be between 2 and 5");
        }

        std::cout << "Num node: " << num_node << std::endl;

        AdjacencyMatrix graph(num_node, num_obj, edges);
        AdjacencyMatrix inv_graph(num_node, num_obj, edges, true);

        const std::string dataset_id = vm["dataset-id"].as<std::string>().empty()
            ? build_default_dataset_id(cost_files)
            : sanitize_artifact_component(vm["dataset-id"].as<std::string>());

        if (scenario_mode) {
            scenarios_run(
                graph, inv_graph,
                vm["scenario"].as<std::string>(),
                algorithm,
                dataset_id,
                budget_sec,
                thread_config,
                trace_interval_ms,
                vm
            );
        } else {
            const size_t start_node = validated_node_id("start", vm["start"].as<int>(), num_node);
            const size_t target_node = validated_node_id("target", vm["target"].as<int>(), num_node);
            single_run(
                graph, inv_graph,
                start_node, target_node,
                algorithm,
                dataset_id,
                resolve_query_id(vm, build_single_query_id(start_node, target_node), false),
                budget_sec,
                thread_config,
                trace_interval_ms,
                vm
            );
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
