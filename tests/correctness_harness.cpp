#include <cassert>
#include <fstream>
#include <functional>
#include <random>
#include <set>

#include "algorithms/abstract_solver.h"
#include "algorithms/emoa.h"
#include "algorithms/ltmoa.h"
#include "algorithms/sopmoa.h"
#include "algorithms/sopmoa_relaxed.h"
#include "problem/graph.h"
#include "utils/benchmark_metrics.h"

namespace {

struct QuerySpec {
    std::string label;
    std::string difficulty;
    size_t start = 0;
    size_t target = 0;
};

struct SyntheticFixture {
    std::string family;
    int num_obj = 0;
    size_t num_node = 0;
    std::vector<Edge> edges;
    std::vector<QuerySpec> queries;
};

struct SolverConfig {
    std::string label;
    int threads_requested = 1;
    int threads_effective = 1;
    bool require_exact_frontier = true;
    std::function<std::shared_ptr<AbstractSolver>(AdjacencyMatrix&, AdjacencyMatrix&, size_t, size_t)> make_solver;
};

std::array<cost_t, Edge::MAX_NUM_OBJ> make_costs(const std::vector<cost_t>& values) {
    std::array<cost_t, Edge::MAX_NUM_OBJ> result{};
    result.fill(0);
    for (size_t i = 0; i < values.size(); i++) {
        result[i] = values[i];
    }
    return result;
}

Edge make_edge(size_t head, size_t tail, const std::vector<cost_t>& costs) {
    return Edge(head, tail, make_costs(costs));
}

std::string read_file(const std::filesystem::path& file_path) {
    std::ifstream input(file_path);
    std::stringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::filesystem::path locate_repo_file(const std::filesystem::path& relative_path) {
    const std::filesystem::path direct = relative_path;
    if (std::filesystem::exists(direct)) {
        return direct;
    }

    const std::filesystem::path from_build = std::filesystem::path("..") / relative_path;
    if (std::filesystem::exists(from_build)) {
        return from_build;
    }

    return relative_path;
}

std::vector<std::string> split_csv_row(const std::string& row) {
    std::vector<std::string> fields;
    std::stringstream stream(row);
    std::string field;
    while (std::getline(stream, field, ',')) {
        fields.push_back(field);
    }
    return fields;
}

bool weakly_dominate_costs(const std::vector<cost_t>& lhs, const std::vector<cost_t>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (size_t i = 0; i < lhs.size(); i++) {
        if (rhs[i] < lhs[i]) {
            return false;
        }
    }
    return true;
}

bool lexicographically_smaller_costs(const std::vector<cost_t>& lhs, const std::vector<cost_t>& rhs) {
    const size_t limit = std::min(lhs.size(), rhs.size());
    for (size_t i = 0; i < limit; i++) {
        if (lhs[i] < rhs[i]) {
            return true;
        }
        if (lhs[i] > rhs[i]) {
            return false;
        }
    }
    return lhs.size() < rhs.size();
}

std::vector<std::vector<cost_t>> canonical_cost_frontier(const std::vector<FrontierPoint>& frontier) {
    std::vector<std::vector<cost_t>> canonical;
    const std::vector<FrontierPoint> sorted = sort_frontier_lexicographically(normalize_frontier(frontier));
    canonical.reserve(sorted.size());
    for (const FrontierPoint& point : sorted) {
        canonical.push_back(point.cost);
    }
    return canonical;
}

void assert_frontier_properties(const std::vector<FrontierPoint>& frontier) {
    const std::vector<FrontierPoint> normalized = normalize_frontier(frontier);
    const std::vector<FrontierPoint> sorted = sort_frontier_lexicographically(normalized);
    assert(frontier_points_equal(sorted, frontier));

    std::set<std::vector<cost_t>> unique_costs;
    for (size_t i = 0; i < frontier.size(); i++) {
        assert(unique_costs.insert(frontier[i].cost).second);
        if (i > 0) {
            assert(!lexicographically_smaller_costs(frontier[i].cost, frontier[i - 1].cost));
        }
        for (size_t j = 0; j < frontier.size(); j++) {
            if (i == j) {
                continue;
            }
            assert(!weakly_dominate_costs(frontier[j].cost, frontier[i].cost));
        }
    }
}

void assert_frontier_exact_match(const std::vector<FrontierPoint>& expected, const std::vector<FrontierPoint>& actual) {
    assert(canonical_cost_frontier(expected) == canonical_cost_frontier(actual));
}

AdjacencyMatrix build_graph(const SyntheticFixture& fixture, bool reverse = false) {
    std::vector<Edge> edges = fixture.edges;
    return AdjacencyMatrix(fixture.num_node, fixture.num_obj, edges, reverse);
}

SyntheticFixture make_grid_fixture() {
    SyntheticFixture fixture;
    fixture.family = "small_grid";
    fixture.num_obj = 2;
    fixture.num_node = 16;

    auto node_id = [](size_t row, size_t col) { return row * 4 + col; };
    for (size_t row = 0; row < 4; row++) {
        for (size_t col = 0; col < 4; col++) {
            const size_t from = node_id(row, col);
            if (col + 1 < 4) {
                fixture.edges.push_back(make_edge(from, node_id(row, col + 1), {
                    static_cast<cost_t>(1 + (row % 2)),
                    static_cast<cost_t>(2 + ((row + col) % 2)),
                }));
            }
            if (row + 1 < 4) {
                fixture.edges.push_back(make_edge(from, node_id(row + 1, col), {
                    static_cast<cost_t>(2 + (col % 2)),
                    static_cast<cost_t>(1 + ((row + col) % 2)),
                }));
            }
        }
    }

    fixture.queries = {
        {"easy", "easy", node_id(0, 0), node_id(1, 1)},
        {"medium", "medium", node_id(0, 0), node_id(2, 2)},
        {"hard", "hard", node_id(0, 0), node_id(3, 3)},
    };
    return fixture;
}

SyntheticFixture make_layered_fixture() {
    SyntheticFixture fixture;
    fixture.family = "layered_dag";
    fixture.num_obj = 3;
    fixture.num_node = 15;

    auto node_id = [](size_t layer, size_t slot) { return layer * 3 + slot; };
    for (size_t layer = 0; layer + 1 < 5; layer++) {
        for (size_t left = 0; left < 3; left++) {
            for (size_t right = 0; right < 3; right++) {
                fixture.edges.push_back(make_edge(node_id(layer, left), node_id(layer + 1, right), {
                    static_cast<cost_t>(1 + left + (right % 2)),
                    static_cast<cost_t>(1 + right + (layer % 2)),
                    static_cast<cost_t>(1 + ((left + right + layer) % 3)),
                }));
            }
        }
    }

    fixture.queries = {
        {"easy", "easy", node_id(0, 0), node_id(2, 1)},
        {"medium", "medium", node_id(0, 0), node_id(3, 1)},
        {"hard", "hard", node_id(0, 0), node_id(4, 2)},
    };
    return fixture;
}

SyntheticFixture make_random_sparse_fixture() {
    SyntheticFixture fixture;
    fixture.family = "random_sparse";
    fixture.num_obj = 2;
    fixture.num_node = 9;

    std::minstd_rand rng(17);
    std::uniform_int_distribution<int> coin(0, 99);

    for (size_t from = 0; from + 1 < fixture.num_node; from++) {
        fixture.edges.push_back(make_edge(from, from + 1, {
            static_cast<cost_t>(2 + (from % 3)),
            static_cast<cost_t>(2 + ((from + 1) % 3)),
        }));
    }

    for (size_t from = 0; from < fixture.num_node; from++) {
        for (size_t to = from + 2; to < std::min(fixture.num_node, from + 4); to++) {
            if (coin(rng) < 45) {
                fixture.edges.push_back(make_edge(from, to, {
                    static_cast<cost_t>(1 + ((from + to) % 5)),
                    static_cast<cost_t>(1 + ((from * 2 + to) % 5)),
                }));
            }
        }
    }

    fixture.queries = {
        {"easy", "easy", 0, 4},
        {"medium", "medium", 0, 6},
        {"hard", "hard", 0, 8},
    };
    return fixture;
}

SyntheticFixture make_correlated_fixture() {
    SyntheticFixture fixture;
    fixture.family = "correlated_weights";
    fixture.num_obj = 3;
    fixture.num_node = 10;

    for (size_t from = 0; from < fixture.num_node; from++) {
        for (size_t to = from + 1; to < std::min(fixture.num_node, from + 3); to++) {
            const cost_t base = static_cast<cost_t>(2 + ((from + to) % 4));
            fixture.edges.push_back(make_edge(from, to, {base, static_cast<cost_t>(base + 1), static_cast<cost_t>(base + 2)}));
        }
    }

    fixture.queries = {
        {"easy", "easy", 0, 4},
        {"medium", "medium", 0, 7},
        {"hard", "hard", 0, 9},
    };
    return fixture;
}

SyntheticFixture make_anti_correlated_fixture() {
    SyntheticFixture fixture;
    fixture.family = "anti_correlated";
    fixture.num_obj = 4;
    fixture.num_node = 10;

    for (size_t from = 0; from < fixture.num_node; from++) {
        for (size_t to = from + 1; to < std::min(fixture.num_node, from + 3); to++) {
            const cost_t a = static_cast<cost_t>(1 + ((from + to) % 4));
            const cost_t b = static_cast<cost_t>(6 - a);
            fixture.edges.push_back(make_edge(from, to, {
                a,
                b,
                static_cast<cost_t>(1 + ((from * 2 + to) % 5)),
                static_cast<cost_t>(5 - ((from + to) % 4)),
            }));
        }
    }

    fixture.queries = {
        {"easy", "easy", 0, 5},
        {"medium", "medium", 0, 7},
        {"hard", "hard", 0, 9},
    };
    return fixture;
}

SyntheticFixture make_tie_heavy_fixture() {
    SyntheticFixture fixture;
    fixture.family = "tie_heavy";
    fixture.num_obj = 4;
    fixture.num_node = 9;

    fixture.edges = {
        make_edge(0, 1, {1, 1, 2, 2}),
        make_edge(0, 2, {1, 1, 2, 2}),
        make_edge(0, 3, {2, 2, 1, 1}),
        make_edge(1, 4, {2, 2, 1, 1}),
        make_edge(2, 4, {2, 2, 1, 1}),
        make_edge(3, 4, {1, 1, 2, 2}),
        make_edge(4, 5, {1, 1, 1, 1}),
        make_edge(4, 6, {1, 1, 1, 1}),
        make_edge(5, 7, {2, 2, 2, 2}),
        make_edge(6, 7, {2, 2, 2, 2}),
        make_edge(7, 8, {1, 1, 1, 1}),
        make_edge(5, 8, {3, 3, 3, 3}),
        make_edge(6, 8, {3, 3, 3, 3}),
    };

    fixture.queries = {
        {"easy", "easy", 0, 4},
        {"medium", "medium", 0, 7},
        {"hard", "hard", 0, 8},
    };
    return fixture;
}

std::vector<SyntheticFixture> build_fixture_matrix() {
    return {
        make_grid_fixture(),
        make_layered_fixture(),
        make_random_sparse_fixture(),
        make_correlated_fixture(),
        make_anti_correlated_fixture(),
        make_tie_heavy_fixture(),
    };
}

std::vector<FrontierPoint> exhaustive_reference_frontier(const SyntheticFixture& fixture, size_t start, size_t target) {
    AdjacencyMatrix graph = build_graph(fixture, false);
    std::vector<FrontierPoint> frontier;
    std::vector<cost_t> cost(static_cast<size_t>(fixture.num_obj), 0);

    std::function<void(size_t)> dfs = [&](size_t node) {
        if (node == target) {
            insert_nondominated_frontier_point(frontier, FrontierPoint{cost, -1.0});
            return;
        }

        for (const Edge& edge : graph[node]) {
            for (int obj = 0; obj < fixture.num_obj; obj++) {
                cost[static_cast<size_t>(obj)] += edge.cost[static_cast<size_t>(obj)];
            }
            dfs(edge.tail);
            for (int obj = 0; obj < fixture.num_obj; obj++) {
                cost[static_cast<size_t>(obj)] -= edge.cost[static_cast<size_t>(obj)];
            }
        }
    };

    dfs(start);
    return sort_frontier_lexicographically(normalize_frontier(frontier));
}

RunMetrics run_solver(
    const SyntheticFixture& fixture,
    const QuerySpec& query,
    const SolverConfig& solver_config,
    const std::filesystem::path& artifact_root
) {
    AdjacencyMatrix graph = build_graph(fixture, false);
    AdjacencyMatrix inv_graph = build_graph(fixture, true);
    std::shared_ptr<AbstractSolver> solver =
        solver_config.make_solver(graph, inv_graph, query.start, query.target);

    BenchmarkRunConfig config;
    config.dataset_id = fixture.family + "_" + std::to_string(fixture.num_obj) + "obj";
    config.query_id = query.label;
    config.threads_requested = solver_config.threads_requested;
    config.threads_effective = solver_config.threads_effective;
    config.budget_sec = 5.0;
    config.trace_interval_ms = 0;
    solver->configure_benchmark_run(config);

    const std::string prefix = fixture.family + "__" + query.label + "__" + solver_config.label;
    const std::filesystem::path frontier_path = artifact_root / (prefix + "_frontier.csv");
    const std::filesystem::path trace_path = artifact_root / (prefix + "_trace.csv");
    solver->set_benchmark_frontier_artifact_path(frontier_path);
    solver->set_benchmark_trace_artifact_path(trace_path);

    solver->solve(5.0);
    if (!solver->benchmark_status_set()) {
        solver->set_benchmark_status(RunStatus::completed);
    }

    RunMetrics metrics = solver->finalize_benchmark_run();
    solver->benchmark_recorder().write_frontier_csv(frontier_path);
    solver->benchmark_recorder().write_trace_csv(trace_path);
    solver->benchmark_recorder().write_summary_row(artifact_root / "correctness_summary.csv");
    return metrics;
}

void assert_measurement_invariants(const RunMetrics& metrics) {
    assert(metrics.status == RunStatus::completed);
    assert(metrics.completed);
    assert(metrics.runtime_sec >= 0.0);
    assert(metrics.counters.generated_labels >= metrics.counters.expanded_labels);
    assert(metrics.counters.final_frontier_size <= metrics.counters.target_hits_raw);
    assert(metrics.counters.final_frontier_size == metrics.final_frontier.size());
}

void assert_export_matches_frontier(
    const RunMetrics& metrics,
    const std::filesystem::path& frontier_path
) {
    const std::string text = read_file(frontier_path);
    assert(text.find("time_found_sec,obj1") == 0);

    std::stringstream rows(text);
    std::string row;
    std::getline(rows, row);

    std::vector<std::vector<cost_t>> parsed_costs;
    while (std::getline(rows, row)) {
        if (row.empty()) {
            continue;
        }
        const std::vector<std::string> fields = split_csv_row(row);
        assert(fields.size() == metrics.num_objectives + 1);
        std::vector<cost_t> cost;
        for (size_t i = 1; i < fields.size(); i++) {
            cost.push_back(static_cast<cost_t>(std::stoul(fields[i])));
        }
        parsed_costs.push_back(cost);
    }

    assert(parsed_costs == canonical_cost_frontier(metrics.final_frontier));
}

void run_export_determinism_subcase(const std::filesystem::path& root) {
    BenchmarkRecorder recorder;
    RunMetrics metadata;
    metadata.solver_name = "export_example";
    metadata.dataset_id = "synthetic";
    metadata.query_id = "deterministic";
    metadata.num_objectives = 2;
    recorder.configure(metadata, 0);
    recorder.set_status(RunStatus::completed);

    const std::vector<FrontierPoint> frontier = {
        {{2, 5}, 0.1},
        {{5, 2}, 0.2},
    };
    recorder.finalize(frontier);

    const std::filesystem::path out_a = root / "deterministic_a.csv";
    const std::filesystem::path out_b = root / "deterministic_b.csv";
    recorder.write_frontier_csv(out_a);
    recorder.write_frontier_csv(out_b);

    const std::string expected = read_file(locate_repo_file("data/synthetic/expected_frontier_example.csv"));
    assert(read_file(out_a) == expected);
    assert(read_file(out_b) == expected);
}

void run_correctness_matrix_subcase(const std::filesystem::path& root) {
    const std::vector<SolverConfig> solver_configs = {
        {"LTMOA_t1", 1, 1, true, [](AdjacencyMatrix& graph, AdjacencyMatrix& inv_graph, size_t start, size_t target) {
            return get_LTMOA_solver(graph, inv_graph, start, target);
        }},
        {"EMOA_t1", 1, 1, true, [](AdjacencyMatrix& graph, AdjacencyMatrix& inv_graph, size_t start, size_t target) {
            return get_EMOA_solver(graph, inv_graph, start, target);
        }},
        {"SOPMOA_t1", 1, 1, true, [](AdjacencyMatrix& graph, AdjacencyMatrix& inv_graph, size_t start, size_t target) {
            return get_SOPMOA_solver(graph, inv_graph, start, target, 1);
        }},
        {"SOPMOA_t4", 4, 4, true, [](AdjacencyMatrix& graph, AdjacencyMatrix& inv_graph, size_t start, size_t target) {
            return get_SOPMOA_solver(graph, inv_graph, start, target, 4);
        }},
        {"SOPMOA_relaxed_t1", 1, 1, true, [](AdjacencyMatrix& graph, AdjacencyMatrix& inv_graph, size_t start, size_t target) {
            return get_SOPMOA_relaxed_solver(graph, inv_graph, start, target, 1);
        }},
        {"SOPMOA_relaxed_t4", 4, 4, false, [](AdjacencyMatrix& graph, AdjacencyMatrix& inv_graph, size_t start, size_t target) {
            return get_SOPMOA_relaxed_solver(graph, inv_graph, start, target, 4);
        }},
    };

    for (const SyntheticFixture& fixture : build_fixture_matrix()) {
        for (const QuerySpec& query : fixture.queries) {
            const std::vector<FrontierPoint> expected = exhaustive_reference_frontier(fixture, query.start, query.target);
            assert(!expected.empty());
            assert_frontier_properties(expected);

            std::map<std::string, std::vector<std::vector<cost_t>>> actual_frontiers;

            for (const SolverConfig& solver_config : solver_configs) {
                const std::filesystem::path artifact_root = root / fixture.family / query.label / solver_config.label;
                std::filesystem::create_directories(artifact_root);

                RunMetrics metrics = run_solver(fixture, query, solver_config, artifact_root);
                assert_measurement_invariants(metrics);
                assert_frontier_properties(metrics.final_frontier);
                if (solver_config.require_exact_frontier) {
                    assert_frontier_exact_match(expected, metrics.final_frontier);
                }
                assert_export_matches_frontier(metrics, artifact_root / (fixture.family + "__" + query.label + "__" + solver_config.label + "_frontier.csv"));

                actual_frontiers[solver_config.label] = canonical_cost_frontier(metrics.final_frontier);
            }

            assert(actual_frontiers["SOPMOA_t1"] == actual_frontiers["SOPMOA_relaxed_t1"]);
            assert(actual_frontiers["SOPMOA_t1"] == actual_frontiers["SOPMOA_t4"]);
        }
    }
}

}  // namespace

int main() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("sopmoa_correctness_harness_" + std::to_string(static_cast<long long>(std::time(nullptr))));

    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    run_export_determinism_subcase(root);
    run_correctness_matrix_subcase(root);

    std::filesystem::remove_all(root);
    return 0;
}
