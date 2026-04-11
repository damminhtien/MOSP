#include <algorithm>
#include <cassert>
#include <functional>

#include "algorithms/gcl/gcl_fast_array.h"
#include "algorithms/ltmoa_array.h"
#include "algorithms/ltmoa_array_superfast.h"
#include "algorithms/ltmoa_array_superfast_anytime.h"
#include "algorithms/ltmoa_array_superfast_lb.h"
#include "algorithms/ltmoa_parallel.h"
#include "problem/graph.h"
#include "problem/heuristic.h"
#include "problem/lower_bound_oracle.h"
#include "utils/label_arena.h"

namespace {

std::array<cost_t, Edge::MAX_NUM_OBJ> edge_costs(const std::vector<cost_t>& values) {
    std::array<cost_t, Edge::MAX_NUM_OBJ> result{};
    result.fill(0);

    for (size_t idx = 0; idx < values.size(); idx++) {
        result[idx] = values[idx];
    }
    return result;
}

struct GraphPair {
    AdjacencyMatrix graph;
    AdjacencyMatrix inv_graph;
};

GraphPair make_tradeoff_graph(size_t num_obj) {
    const size_t start = 0;
    const size_t target = num_obj + 1;
    std::vector<Edge> edges;

    for (size_t path_idx = 0; path_idx < num_obj; path_idx++) {
        std::array<cost_t, Edge::MAX_NUM_OBJ> leg_a{};
        std::array<cost_t, Edge::MAX_NUM_OBJ> leg_b{};
        leg_a.fill(0);
        leg_b.fill(0);

        for (size_t obj_idx = 0; obj_idx < num_obj; obj_idx++) {
            const cost_t value = obj_idx == path_idx ? 1 : 4;
            leg_a[obj_idx] = value;
            leg_b[obj_idx] = value;
        }

        edges.emplace_back(start, path_idx + 1, leg_a);
        edges.emplace_back(path_idx + 1, target, leg_b);
    }

    std::vector<cost_t> dominated_costs(num_obj, 9);
    edges.emplace_back(start, target, edge_costs(dominated_costs));

    AdjacencyMatrix graph(target + 1, num_obj, edges);
    AdjacencyMatrix inv_graph(target + 1, num_obj, edges, true);
    return {std::move(graph), std::move(inv_graph)};
}

GraphPair make_target_frontier_graph() {
    std::vector<Edge> edges;

    edges.emplace_back(0, 4, edge_costs({9, 9}));
    edges.emplace_back(0, 1, edge_costs({2, 2}));
    edges.emplace_back(1, 4, edge_costs({3, 3}));
    edges.emplace_back(0, 2, edge_costs({1, 1}));
    edges.emplace_back(2, 3, edge_costs({1, 1}));
    edges.emplace_back(3, 4, edge_costs({2, 2}));
    edges.emplace_back(0, 3, edge_costs({3, 3}));

    AdjacencyMatrix graph(5, 2, edges);
    AdjacencyMatrix inv_graph(5, 2, edges, true);
    return {std::move(graph), std::move(inv_graph)};
}

GraphPair make_anytime_trigger_graph(size_t num_distractors = 524288) {
    const size_t start = 0;
    const size_t branch_b = num_distractors + 1;
    const size_t branch_c = num_distractors + 2;
    const size_t target = num_distractors + 3;
    std::vector<Edge> edges;
    edges.reserve((2 * num_distractors) + 4);

    for (size_t idx = 0; idx < num_distractors; idx++) {
        const size_t distractor = idx + 1;
        edges.emplace_back(start, distractor, edge_costs({1, 0}));
        edges.emplace_back(distractor, target, edge_costs({0, 1000}));
    }

    edges.emplace_back(start, branch_b, edge_costs({50, 20}));
    edges.emplace_back(branch_b, target, edge_costs({0, 50}));
    edges.emplace_back(start, branch_c, edge_costs({20, 50}));
    edges.emplace_back(branch_c, target, edge_costs({50, 0}));

    AdjacencyMatrix graph(target + 1, 2, edges);
    AdjacencyMatrix inv_graph(target + 1, 2, edges, true);
    return {std::move(graph), std::move(inv_graph)};
}

GraphPair make_deferred_seed_graph(size_t chain_length = 2300) {
    const size_t start = 0;
    const size_t first_chain = 1;
    const size_t branch_b = chain_length + 1;
    const size_t branch_c = chain_length + 2;
    const size_t target = chain_length + 3;
    std::vector<Edge> edges;
    edges.reserve(chain_length + 5);

    edges.emplace_back(start, first_chain, edge_costs({1, 0}));
    for (size_t idx = 0; idx + 1 < chain_length; idx++) {
        edges.emplace_back(first_chain + idx, first_chain + idx + 1, edge_costs({0, 0}));
    }
    edges.emplace_back(first_chain + chain_length - 1, target, edge_costs({0, 1000}));

    edges.emplace_back(start, branch_b, edge_costs({50, 20}));
    edges.emplace_back(branch_b, target, edge_costs({0, 50}));
    edges.emplace_back(start, branch_c, edge_costs({20, 50}));
    edges.emplace_back(branch_c, target, edge_costs({50, 0}));

    AdjacencyMatrix graph(target + 1, 2, edges);
    AdjacencyMatrix inv_graph(target + 1, 2, edges, true);
    return {std::move(graph), std::move(inv_graph)};
}

std::vector<std::vector<cost_t>> normalized_solution_costs(const std::shared_ptr<AbstractSolver>& solver) {
    std::vector<std::vector<cost_t>> costs;
    costs.reserve(solver->solutions.size());
    for (const auto& solution : solver->solutions) {
        costs.push_back(solution.cost);
    }
    std::sort(costs.begin(), costs.end());
    return costs;
}

void test_label_arena_rollover() {
    LabelArena<3, 2> arena;
    CostVec<3> cost_a = {1, 2, 3};
    CostVec<3> cost_b = {4, 5, 6};
    CostVec<3> cost_c = {7, 8, 9};

    Label<3>* a = arena.create(1, cost_a);
    Label<3>* b = arena.create(2, cost_b);
    Label<3>* c = arena.create(3, cost_c);

    assert(a != b);
    assert(b != c);
    assert(arena.allocated() == 3);
    assert(c->node == 3);
    assert(c->f == cost_c);

    arena.clear();
    assert(arena.allocated() == 0);
}

void test_fast_frontier_structure() {
    FastNodeFrontier<2> frontier;

    assert(frontier.try_insert_or_prune({5, 5}));
    assert(frontier.size() == 1);
    assert(frontier.capacity() == 4);

    assert(!frontier.try_insert_or_prune({6, 6}));
    assert(frontier.size() == 1);

    assert(frontier.try_insert_or_prune({4, 7}));
    assert(frontier.size() == 2);

    assert(frontier.try_insert_or_prune({4, 4}));
    assert(frontier.size() == 1);
    assert(frontier.snapshot().front() == CostVec<2>({4, 4}));
    assert(frontier.dominated({5, 6}));
    assert(!frontier.try_insert_or_prune({4, 4}));
    assert(frontier.size() == 1);

    FastNodeFrontier<2> prune_frontier;
    assert(prune_frontier.try_insert_or_prune({7, 4}));
    assert(prune_frontier.try_insert_or_prune({4, 7}));
    assert(prune_frontier.try_insert_or_prune({6, 6}));
    assert(prune_frontier.size() == 3);
    assert(prune_frontier.try_insert_or_prune({4, 4}));
    assert(prune_frontier.size() == 1);
    assert(prune_frontier.snapshot().front() == CostVec<2>({4, 4}));

    FastNodeFrontier<2> reject_after_partial_overlap;
    assert(reject_after_partial_overlap.try_insert_or_prune({7, 7}));
    assert(reject_after_partial_overlap.try_insert_or_prune({4, 4}));
    const auto before_reject = reject_after_partial_overlap.snapshot();
    assert(!reject_after_partial_overlap.try_insert_or_prune({6, 6}));
    assert(reject_after_partial_overlap.snapshot() == before_reject);

    FastNodeFrontier<2> growth_frontier;
    assert(growth_frontier.try_insert_or_prune({1, 10}));
    assert(growth_frontier.try_insert_or_prune({2, 9}));
    assert(growth_frontier.try_insert_or_prune({3, 8}));
    assert(growth_frontier.try_insert_or_prune({4, 7}));
    assert(growth_frontier.try_insert_or_prune({5, 6}));
    assert(growth_frontier.size() == 5);
    assert(growth_frontier.capacity() >= 8);

    const uint32_t retained_capacity = growth_frontier.capacity();
    growth_frontier.clear();
    assert(growth_frontier.size() == 0);
    assert(growth_frontier.try_insert_or_prune({10, 1}));
    assert(growth_frontier.capacity() == retained_capacity);
}

void test_lower_bound_oracle_and_warm_start_weights() {
    GraphPair graphs = make_tradeoff_graph(3);
    LowerBoundOracle<3> oracle(graphs.graph, graphs.inv_graph, 0, 4);
    Heuristic<3> exact_reverse_bound(4, graphs.inv_graph);

    const std::vector<size_t>& landmarks = oracle.landmark_nodes();
    assert(!landmarks.empty());
    assert(std::find(landmarks.begin(), landmarks.end(), 0) != landmarks.end());
    assert(std::find(landmarks.begin(), landmarks.end(), 4) != landmarks.end());

    std::set<size_t> unique_landmarks(landmarks.begin(), landmarks.end());
    assert(unique_landmarks.size() == landmarks.size());

    for (size_t node = 0; node < static_cast<size_t>(graphs.graph.get_num_node()); node++) {
        const CostVec<3>& base = oracle.base_bound(node);
        const CostVec<3>& bound = oracle(node);
        const CostVec<3>& exact = exact_reverse_bound(node);

        for (size_t obj_idx = 0; obj_idx < 3; obj_idx++) {
            assert(bound[obj_idx] >= base[obj_idx]);
            assert(bound[obj_idx] <= exact[obj_idx]);
        }
    }
    assert(oracle(4) == CostVec<3>({0, 0, 0}));

    const std::vector<CostVec<3>> weights = LTMOA_array_superfast_lb<3>::build_warm_start_weights();
    assert(weights.size() == 7);

    std::set<std::vector<cost_t>> unique_weights;
    for (const CostVec<3>& weight : weights) {
        unique_weights.insert(std::vector<cost_t>(weight.begin(), weight.end()));
    }
    assert(unique_weights.size() == weights.size());
    assert(unique_weights.count({1, 1, 1}) == 1);
    assert(unique_weights.count({1, 0, 0}) == 1);
    assert(unique_weights.count({0, 1, 0}) == 1);
    assert(unique_weights.count({0, 0, 1}) == 1);
    assert(unique_weights.count({8, 1, 1}) == 1);
    assert(unique_weights.count({1, 8, 1}) == 1);
    assert(unique_weights.count({1, 1, 8}) == 1);
}

void test_superfast_target_frontier_behavior() {
    GraphPair graphs = make_target_frontier_graph();
    std::shared_ptr<AbstractSolver> solver = get_LTMOA_array_superfast_solver(
        graphs.graph,
        graphs.inv_graph,
        0,
        4
    );

    BenchmarkRunConfig config;
    config.dataset_id = "toy_graph";
    config.query_id = "superfast_target_frontier";
    config.threads_requested = 1;
    config.threads_effective = 1;
    config.budget_sec = 5.0;
    config.trace_interval_ms = 0;
    solver->configure_benchmark_run(config);

    solver->solve(5.0);
    if (!solver->benchmark_status_set()) {
        solver->set_benchmark_status(RunStatus::completed);
    }

    const RunMetrics metrics = solver->finalize_benchmark_run();
    assert(metrics.status == RunStatus::completed);
    assert(metrics.counters.pruned_by_target >= 2);
    assert(metrics.counters.target_hits_raw >= metrics.counters.final_frontier_size);
    assert(metrics.final_frontier.size() == 1);
    assert(metrics.final_frontier.front().cost == std::vector<cost_t>({4, 4}));
    assert(metrics.final_frontier.front().time_found_sec >= 0.0);
    assert(metrics.time_to_first_solution_sec >= 0.0);
    assert(metrics.anytime_trace.empty());
}

void test_superfast_deferred_seed_and_mixed_schedule_behavior() {
    GraphPair baseline_graphs = make_deferred_seed_graph();
    GraphPair superfast_graphs = make_deferred_seed_graph();
    const size_t target = static_cast<size_t>(superfast_graphs.graph.get_num_node() - 1);

    std::shared_ptr<AbstractSolver> baseline = get_LTMOA_array_solver(
        baseline_graphs.graph,
        baseline_graphs.inv_graph,
        0,
        target
    );
    std::shared_ptr<AbstractSolver> superfast = get_LTMOA_array_superfast_solver(
        superfast_graphs.graph,
        superfast_graphs.inv_graph,
        0,
        target
    );

    baseline->solve(5.0);

    BenchmarkRunConfig config;
    config.dataset_id = "toy_graph";
    config.query_id = "superfast_deferred_seed";
    config.threads_requested = 1;
    config.threads_effective = 1;
    config.budget_sec = 5.0;
    config.trace_interval_ms = 1;
    superfast->configure_benchmark_run(config);
    superfast->set_benchmark_trace_artifact_path(
        std::filesystem::temp_directory_path() / "superfast_deferred_seed_trace.csv"
    );

    superfast->solve(5.0);
    if (!superfast->benchmark_status_set()) {
        superfast->set_benchmark_status(RunStatus::completed);
    }

    const RunMetrics metrics = superfast->finalize_benchmark_run();
    assert(metrics.status == RunStatus::completed);
    assert(metrics.counters.final_frontier_size == 3);
    assert(metrics.counters.target_hits_raw >= 1);
    assert(metrics.time_to_first_solution_sec >= 0.0);
    assert(normalized_solution_costs(baseline) == normalized_solution_costs(superfast));

    bool saw_deferred_seed = false;
    for (const auto& point : metrics.anytime_trace) {
        if (point.trigger == "deferred_seed") {
            saw_deferred_seed = true;
            break;
        }
    }
    assert(saw_deferred_seed);
}

void test_superfast_lb_target_frontier_behavior() {
    GraphPair graphs = make_target_frontier_graph();
    std::shared_ptr<AbstractSolver> solver = get_LTMOA_array_superfast_lb_solver(
        graphs.graph,
        graphs.inv_graph,
        0,
        4
    );

    BenchmarkRunConfig config;
    config.dataset_id = "toy_graph";
    config.query_id = "superfast_lb_target_frontier";
    config.threads_requested = 1;
    config.threads_effective = 1;
    config.budget_sec = 5.0;
    config.trace_interval_ms = 0;
    solver->configure_benchmark_run(config);

    solver->solve(5.0);
    if (!solver->benchmark_status_set()) {
        solver->set_benchmark_status(RunStatus::completed);
    }

    const RunMetrics metrics = solver->finalize_benchmark_run();
    assert(metrics.status == RunStatus::completed);
    assert(metrics.counters.final_frontier_size == 1);
    assert(metrics.final_frontier.front().cost == std::vector<cost_t>({4, 4}));
    assert(metrics.time_to_first_solution_sec >= 0.0);
}

void test_superfast_anytime_deferred_seed_and_lazy_rekey_behavior() {
    GraphPair anytime_graphs = make_anytime_trigger_graph();
    const size_t target = static_cast<size_t>(anytime_graphs.graph.get_num_node() - 1);

    std::shared_ptr<AbstractSolver> anytime = get_LTMOA_array_superfast_anytime_solver(
        anytime_graphs.graph,
        anytime_graphs.inv_graph,
        0,
        target
    );

    BenchmarkRunConfig config;
    config.dataset_id = "toy_graph";
    config.query_id = "superfast_anytime_trigger";
    config.threads_requested = 1;
    config.threads_effective = 1;
    config.budget_sec = 5.0;
    config.trace_interval_ms = 0;
    anytime->configure_benchmark_run(config);

    anytime->solve(5.0);
    if (!anytime->benchmark_status_set()) {
        anytime->set_benchmark_status(RunStatus::completed);
    }

    const RunMetrics metrics = anytime->finalize_benchmark_run();

    assert(metrics.status == RunStatus::completed);
    assert(metrics.counters.final_frontier_size == 3);
    assert(metrics.counters.target_hits_raw >= 1);
    assert(metrics.time_to_first_solution_sec >= 0.0);
    assert(normalized_solution_costs(anytime) == std::vector<std::vector<cost_t>>({
        {1, 1000},
        {50, 70},
        {70, 50},
    }));
}

void run_serial_exactness_case(size_t num_obj) {
    GraphPair baseline_graphs = make_tradeoff_graph(num_obj);
    GraphPair superfast_graphs = make_tradeoff_graph(num_obj);

    std::shared_ptr<AbstractSolver> baseline = get_LTMOA_array_solver(
        baseline_graphs.graph,
        baseline_graphs.inv_graph,
        0,
        num_obj + 1
    );
    std::shared_ptr<AbstractSolver> superfast = get_LTMOA_array_superfast_solver(
        superfast_graphs.graph,
        superfast_graphs.inv_graph,
        0,
        num_obj + 1
    );

    baseline->solve(5.0);
    superfast->solve(5.0);

    assert(normalized_solution_costs(baseline) == normalized_solution_costs(superfast));
}

void run_lb_exactness_case(size_t num_obj) {
    GraphPair baseline_graphs = make_tradeoff_graph(num_obj);
    GraphPair lb_graphs = make_tradeoff_graph(num_obj);

    std::shared_ptr<AbstractSolver> baseline = get_LTMOA_array_solver(
        baseline_graphs.graph,
        baseline_graphs.inv_graph,
        0,
        num_obj + 1
    );
    std::shared_ptr<AbstractSolver> superfast_lb = get_LTMOA_array_superfast_lb_solver(
        lb_graphs.graph,
        lb_graphs.inv_graph,
        0,
        num_obj + 1
    );

    baseline->solve(5.0);
    superfast_lb->solve(5.0);

    assert(normalized_solution_costs(baseline) == normalized_solution_costs(superfast_lb));
}

void run_anytime_exactness_case(size_t num_obj) {
    GraphPair baseline_graphs = make_tradeoff_graph(num_obj);
    GraphPair anytime_graphs = make_tradeoff_graph(num_obj);

    std::shared_ptr<AbstractSolver> baseline = get_LTMOA_array_solver(
        baseline_graphs.graph,
        baseline_graphs.inv_graph,
        0,
        num_obj + 1
    );
    std::shared_ptr<AbstractSolver> anytime = get_LTMOA_array_superfast_anytime_solver(
        anytime_graphs.graph,
        anytime_graphs.inv_graph,
        0,
        num_obj + 1
    );

    baseline->solve(5.0);
    anytime->solve(5.0);

    assert(normalized_solution_costs(baseline) == normalized_solution_costs(anytime));
}

void run_parallel_exactness_case(size_t num_obj, int num_threads) {
    GraphPair baseline_graphs = make_tradeoff_graph(num_obj);
    GraphPair parallel_graphs = make_tradeoff_graph(num_obj);

    std::shared_ptr<AbstractSolver> baseline = get_LTMOA_array_solver(
        baseline_graphs.graph,
        baseline_graphs.inv_graph,
        0,
        num_obj + 1
    );
    std::shared_ptr<AbstractSolver> parallel = get_LTMOA_parallel_solver(
        parallel_graphs.graph,
        parallel_graphs.inv_graph,
        0,
        num_obj + 1,
        num_threads
    );

    baseline->solve(5.0);
    parallel->solve(5.0);

    assert(normalized_solution_costs(baseline) == normalized_solution_costs(parallel));
}

}  // namespace

int main() {
    test_label_arena_rollover();
    test_fast_frontier_structure();
    test_lower_bound_oracle_and_warm_start_weights();
    test_superfast_target_frontier_behavior();
    test_superfast_deferred_seed_and_mixed_schedule_behavior();
    test_superfast_anytime_deferred_seed_and_lazy_rekey_behavior();
    test_superfast_lb_target_frontier_behavior();
    run_serial_exactness_case(2);
    run_serial_exactness_case(3);
    run_serial_exactness_case(4);
    run_anytime_exactness_case(2);
    run_anytime_exactness_case(3);
    run_anytime_exactness_case(4);
    run_lb_exactness_case(2);
    run_lb_exactness_case(3);
    run_lb_exactness_case(4);
    run_parallel_exactness_case(2, 1);
    run_parallel_exactness_case(2, 2);
    run_parallel_exactness_case(3, 1);
    run_parallel_exactness_case(3, 2);
    run_parallel_exactness_case(4, 1);
    run_parallel_exactness_case(4, 2);
    return 0;
}
