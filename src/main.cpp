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

#include <boost/program_options.hpp>
#include <boost/tokenizer.hpp>
#include <string>

namespace po = boost::program_options;
using namespace std;
using namespace std::chrono_literals;

void single_run(AdjacencyMatrix& graph, AdjacencyMatrix&inv_graph, size_t start_node, size_t target_node, std::string algorithm, std::ofstream& output, unsigned int time_limit, po::variables_map& vm) {
    int num_exp, num_gen;
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

    auto start_time_ = std::chrono::steady_clock::now();
    solver->solve(time_limit);
    auto end_time_ = std::chrono::steady_clock::now();
    double runtime = (end_time_ - start_time_) / 1.0s;

    std::cout << "Num solution: " << solver->solutions.size() << std::endl;
    std::cout << "Num expansion: " << solver->get_num_expansion() << std::endl;
    std::cout << "Num generation: " << solver->get_num_generation() << std::endl;
    std::cout << "Runtime: " << runtime << std::endl;

    output << solver->get_result_str() << "," << runtime << std::endl;

    if (vm["logsols"].as<std::string>() != "") {
        std::string log_path = vm["logsols"].as<std::string>();
        log_path += "/" + std::to_string(start_node) + "-" + std::to_string(target_node);
        log_path += "-" + std::to_string(graph.get_num_obj()) + "obj.txt";

        std::ofstream log_file;
        log_file.open(log_path, std::fstream::trunc);
        log_file << solver->get_all_sols_str() << std::endl;
        log_file.close();
    }

    solver.reset();
    
}

void scenarios_run(AdjacencyMatrix& graph, AdjacencyMatrix&inv_graph, std::string scen_path, std::string algorithm, std::ofstream& output, unsigned int time_limit, po::variables_map& vm) {
    auto scenarios = read_scenario_file(scen_path);

    int from_query = vm["from"].as<int>(); 
    int to_query = std::min(int(scenarios.size()), vm["to"].as<int>());

    for (int i = from_query; i < to_query; i++){
        size_t start = std::get<1>(scenarios[i]);
        size_t target = std::get<2>(scenarios[i]);

        std::cout << "Scenario " << i << "/" << scenarios.size() << ": ";
        std::cout << "start "<< start << " - target "<< target << std::endl;

        single_run(
            graph, inv_graph, 
            start, target, 
            algorithm,
            output, time_limit, vm
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
        ("timelimit", po::value<int>()->default_value(300), "cutoff time (seconds)")
        ("logsols", po::value<std::string>()->default_value(""), "if non-empty, dump solution cost to the directory")
        ("output,o", po::value<std::string>()->required(), "Name of the output file")
        ("numthreads,n", po::value<int>()->default_value(-1), "number of threads for SOPMOA, SOPMOA_relaxed and SOPMOA_bucket");
    
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 1;
    }

    po::notify(vm);
    srand((int)time(0));

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

    std::ofstream stats;
    stats.open(vm["output"].as<std::string>(), std::fstream::app);

    if (vm["logsols"].as<std::string>() != "") {
        std::filesystem::create_directory(vm["logsols"].as<std::string>());
    }

    if (vm["scenario"].as<std::string>() != ""){
        scenarios_run(
            graph, inv_graph, 
            vm["scenario"].as<std::string>(), 
            vm["algorithm"].as<std::string>(),
            stats, vm["timelimit"].as<int>(), vm
        );
    } else {
        single_run(
            graph, inv_graph, 
            vm["start"].as<int>(), vm["target"].as<int>(), 
            vm["algorithm"].as<std::string>(),
            stats, vm["timelimit"].as<int>(), vm
        );
    }
    
    stats.close();
    return 0;
}
