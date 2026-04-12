#ifndef UTILS_BENCHMARK_METRICS
#define UTILS_BENCHMARK_METRICS

#include <atomic>

#include "../common_inc.h"
#include "cost.h"

// RunStatus records how a solver invocation stopped.
enum class RunStatus {
    completed,
    timeout,
    crash,
    oom,
    aborted
};

// BenchmarkClock is the single wall-clock source for benchmark metrics.
struct BenchmarkClock {
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    static time_point now();
    static double seconds_since(const time_point& start, const time_point& end);
    static double seconds_since(const time_point& start);
};

// FrontierPoint stores one nondominated objective vector and the time it first appeared.
struct FrontierPoint {
    std::vector<cost_t> cost;
    double time_found_sec = -1.0;
};

// AnytimePoint stores one exported observation of the target frontier state.
struct AnytimePoint {
    double time_sec = 0.0;
    size_t frontier_size = 0;
    uint64_t generated_labels = 0;
    uint64_t expanded_labels = 0;
    uint64_t pruned_by_target = 0;
    uint64_t pruned_by_node = 0;
    uint64_t pruned_other = 0;
    uint64_t target_hits_raw = 0;
    double hv_ratio = std::numeric_limits<double>::quiet_NaN();
    double recall = std::numeric_limits<double>::quiet_NaN();
    std::string trigger;
};

// CounterSet groups the shared solver metrics with stable meanings.
struct CounterSet {
    // generated_labels increments when a label is created and enqueued successfully.
    uint64_t generated_labels = 0;
    // expanded_labels increments when an accepted label actually enters successor expansion.
    uint64_t expanded_labels = 0;
    // pruned_by_target increments when the target frontier dominance check rejects a label.
    uint64_t pruned_by_target = 0;
    // pruned_by_node increments when the current-node frontier dominance check rejects a label.
    uint64_t pruned_by_node = 0;
    // pruned_other increments for uncategorized/stale/duplicate rejections.
    uint64_t pruned_other = 0;
    // target_hits_raw counts accepted target labels before final frontier normalization.
    uint64_t target_hits_raw = 0;
    // final_frontier_size is set during finalize from the cleaned target frontier snapshot.
    uint64_t final_frontier_size = 0;
    // peak_open_size tracks the maximum observed OPEN/inbox/local-heap size.
    uint64_t peak_open_size = 0;
    // peak_live_labels tracks the maximum observed count of live solver-owned labels.
    uint64_t peak_live_labels = 0;
    // target_frontier_checks counts target frontier dominance checks.
    uint64_t target_frontier_checks = 0;
    // node_frontier_checks counts node frontier dominance checks.
    uint64_t node_frontier_checks = 0;
};

struct RunMetrics {
    std::string schema_version = "paper_benchmark_v1";
    std::string solver_name;
    std::string dataset_id;
    std::string query_id;
    size_t start_node = 0;
    size_t target_node = 0;
    size_t num_objectives = 0;
    int threads_requested = 1;
    int threads_effective = 1;
    double budget_sec = std::numeric_limits<double>::infinity();
    double runtime_sec = 0.0;
    double time_to_first_solution_sec = -1.0;
    RunStatus status = RunStatus::aborted;
    bool completed = false;
    std::string seed;
    CounterSet counters;
    std::vector<FrontierPoint> final_frontier;
    std::vector<AnytimePoint> anytime_trace;
    std::string frontier_artifact_path;
    std::string trace_artifact_path;
};

// ThreadLocalSink is the per-thread event collector that solvers update in hot loops.
class ThreadLocalSink {
public:
    // Call when a label has been created and enqueued successfully.
    void on_label_generated(size_t open_size = SIZE_MAX, size_t live_labels = SIZE_MAX);
    // Call when an accepted label is about to generate successors.
    void on_label_expanded(size_t open_size = SIZE_MAX, size_t live_labels = SIZE_MAX);
    // Call when target-frontier dominance prunes a label.
    void on_pruned_by_target();
    // Call when node-frontier dominance prunes a label.
    void on_pruned_by_node();
    // Call when the target frontier is checked for dominance.
    void on_frontier_check_target();
    // Call when the node frontier is checked for dominance.
    void on_frontier_check_node();
    // Call when a label is rejected for another reason.
    void on_pruned_other();
    // Call when an accepted target label is observed before frontier cleanup.
    void on_target_hit_raw();
    // Record the latest OPEN size for peak tracking.
    void observe_open_size(size_t open_size);
    // Record the latest live-label count for peak tracking.
    void observe_live_labels(size_t live_labels);
    const CounterSet& counters() const { return counters_; }

private:
    friend class BenchmarkRecorder;

    CounterSet counters_;
};

class BenchmarkRecorder {
public:
    void configure(const RunMetrics& template_metadata, uint64_t trace_interval_ms = 0);

    ThreadLocalSink make_thread_local_sink() const;
    ThreadLocalSink& main_thread_sink();
    void merge(const ThreadLocalSink& sink);
    double elapsed_sec() const;
    void set_counters(const CounterSet& counters);

    // Record one target frontier check event.
    void on_frontier_check_target();
    // Record one node frontier check event.
    void on_frontier_check_node();
    // Record one accepted frontier update event.
    void on_frontier_update();
    // Export a new anytime point when the target frontier changes or interval sampling is due.
    void on_target_frontier_changed(const std::vector<FrontierPoint>& frontier, const char* trigger = "frontier_change");
    void on_target_frontier_changed(const std::vector<FrontierPoint>& frontier, const CounterSet& counters, const char* trigger);
    // Record a counters-only anytime event when full frontier snapshots are intentionally skipped.
    void record_anytime_event(double elapsed_sec, size_t frontier_size, const CounterSet& counters, const char* trigger);
    void set_status(RunStatus status);

    bool has_status() const;
    bool is_configured() const;
    uint64_t trace_interval_ms() const;
    bool trace_details_enabled() const;
    // Preserve first-solution timing even when trace export is disabled or snapshot capture is skipped.
    void note_first_solution(double elapsed_sec);

    void set_frontier_artifact_path(const std::filesystem::path& csv_path);
    void set_trace_artifact_path(const std::filesystem::path& csv_path);

    RunMetrics finalize(const std::vector<FrontierPoint>& final_frontier);
    const RunMetrics& metrics() const;

    void write_summary_row(const std::filesystem::path& csv_path) const;
    void write_frontier_csv(const std::filesystem::path& csv_path) const;
    void write_trace_csv(const std::filesystem::path& csv_path);

private:
    void annotate_anytime_quality_metrics();
    void ensure_anytime_quality_metrics();
    CounterSet current_counters_snapshot() const;
    bool should_record_trace(const std::vector<FrontierPoint>& frontier, double elapsed_sec) const;
    bool capture_trace_details() const;
    void write_summary_header(std::ofstream& output) const;

    RunMetrics metrics_;
    ThreadLocalSink main_thread_sink_;
    BenchmarkClock::time_point start_time_{};
    std::vector<FrontierPoint> last_frontier_snapshot_;
    std::vector<std::vector<FrontierPoint>> trace_frontier_snapshots_;
    uint64_t trace_interval_ms_ = 0;
    double last_trace_time_sec_ = -1.0;
    std::atomic<uint64_t> frontier_updates_{0};
    bool configured_ = false;
    bool finalized_ = false;
    bool status_set_ = false;
    bool anytime_quality_metrics_ready_ = false;
};

std::string run_status_to_string(RunStatus status);
std::string sanitize_artifact_component(const std::string& raw);

bool frontier_costs_equal(const std::vector<cost_t>& lhs, const std::vector<cost_t>& rhs);
bool frontier_points_equal(const std::vector<FrontierPoint>& lhs, const std::vector<FrontierPoint>& rhs);
bool insert_nondominated_frontier_point(std::vector<FrontierPoint>& frontier, const FrontierPoint& point);
std::vector<FrontierPoint> normalize_frontier(const std::vector<FrontierPoint>& frontier);
std::vector<FrontierPoint> sort_frontier_lexicographically(const std::vector<FrontierPoint>& frontier);
std::vector<cost_t> frontier_cost_row(const FrontierPoint& point, size_t num_objectives);

#endif
