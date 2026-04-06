#include "utils/benchmark_metrics.h"

#include <cctype>
#include <cmath>

namespace {

bool weakly_dominate_vector(const std::vector<cost_t>& lhs, const std::vector<cost_t>& rhs) {
    if (lhs.size() != rhs.size()) { return false; }

    for (size_t i = 0; i < lhs.size(); i++) {
        if (rhs[i] < lhs[i]) { return false; }
    }
    return true;
}

bool lexicographically_smaller_vector(const std::vector<cost_t>& lhs, const std::vector<cost_t>& rhs) {
    const size_t limit = std::min(lhs.size(), rhs.size());
    for (size_t i = 0; i < limit; i++) {
        if (lhs[i] < rhs[i]) { return true; }
        if (lhs[i] > rhs[i]) { return false; }
    }
    return lhs.size() < rhs.size();
}

void ensure_parent_directory(const std::filesystem::path& file_path) {
    if (file_path.empty()) { return; }

    const std::filesystem::path parent = file_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
}

bool file_is_empty_or_missing(const std::filesystem::path& file_path) {
    if (!std::filesystem::exists(file_path)) { return true; }
    return std::filesystem::file_size(file_path) == 0;
}

std::string csv_bool(bool value) {
    return value ? "true" : "false";
}

std::string csv_string(const std::string& value) {
    bool needs_quotes = false;
    std::string escaped;
    escaped.reserve(value.size());

    for (char ch : value) {
        if (ch == '"' || ch == ',' || ch == '\n' || ch == '\r') {
            needs_quotes = true;
        }

        if (ch == '"') {
            escaped += "\"\"";
        } else {
            escaped.push_back(ch);
        }
    }

    if (!needs_quotes) {
        return escaped;
    }
    return "\"" + escaped + "\"";
}

std::string csv_double(double value) {
    if (std::isnan(value)) { return "nan"; }

    std::ostringstream ss;
    ss << std::setprecision(12) << value;
    return ss.str();
}

}  // namespace

BenchmarkClock::time_point BenchmarkClock::now() {
    return clock::now();
}

double BenchmarkClock::seconds_since(const time_point& start, const time_point& end) {
    return std::chrono::duration<double>(end - start).count();
}

double BenchmarkClock::seconds_since(const time_point& start) {
    return seconds_since(start, now());
}

void ThreadLocalSink::on_label_generated(size_t open_size, size_t live_labels) {
    counters_.generated_labels++;
    observe_open_size(open_size);
    observe_live_labels(live_labels);
}

void ThreadLocalSink::on_label_expanded(size_t open_size, size_t live_labels) {
    counters_.expanded_labels++;
    observe_open_size(open_size);
    observe_live_labels(live_labels);
}

void ThreadLocalSink::on_pruned_by_target() {
    counters_.pruned_by_target++;
}

void ThreadLocalSink::on_pruned_by_node() {
    counters_.pruned_by_node++;
}

void ThreadLocalSink::on_pruned_other() {
    counters_.pruned_other++;
}

void ThreadLocalSink::on_target_hit_raw() {
    counters_.target_hits_raw++;
}

void ThreadLocalSink::observe_open_size(size_t open_size) {
    if (open_size != SIZE_MAX) {
        counters_.peak_open_size = std::max<uint64_t>(counters_.peak_open_size, open_size);
    }
}

void ThreadLocalSink::observe_live_labels(size_t live_labels) {
    if (live_labels != SIZE_MAX) {
        counters_.peak_live_labels = std::max<uint64_t>(counters_.peak_live_labels, live_labels);
    }
}

void BenchmarkRecorder::configure(const RunMetrics& template_metadata, uint64_t trace_interval_ms) {
    metrics_ = template_metadata;
    metrics_.runtime_sec = 0.0;
    metrics_.time_to_first_solution_sec = -1.0;
    metrics_.status = RunStatus::aborted;
    metrics_.completed = false;
    metrics_.counters = CounterSet();
    metrics_.final_frontier.clear();
    metrics_.anytime_trace.clear();
    metrics_.frontier_artifact_path.clear();
    metrics_.trace_artifact_path.clear();

    main_thread_sink_ = ThreadLocalSink();
    start_time_ = BenchmarkClock::now();
    last_frontier_snapshot_.clear();
    trace_interval_ms_ = trace_interval_ms;
    last_trace_time_sec_ = -1.0;
    frontier_updates_ = 0;
    configured_ = true;
    finalized_ = false;
    status_set_ = false;
}

ThreadLocalSink BenchmarkRecorder::make_thread_local_sink() const {
    return ThreadLocalSink();
}

ThreadLocalSink& BenchmarkRecorder::main_thread_sink() {
    return main_thread_sink_;
}

void BenchmarkRecorder::merge(const ThreadLocalSink& sink) {
    metrics_.counters.generated_labels += sink.counters_.generated_labels;
    metrics_.counters.expanded_labels += sink.counters_.expanded_labels;
    metrics_.counters.pruned_by_target += sink.counters_.pruned_by_target;
    metrics_.counters.pruned_by_node += sink.counters_.pruned_by_node;
    metrics_.counters.pruned_other += sink.counters_.pruned_other;
    metrics_.counters.target_hits_raw += sink.counters_.target_hits_raw;
    metrics_.counters.peak_open_size = std::max(metrics_.counters.peak_open_size, sink.counters_.peak_open_size);
    metrics_.counters.peak_live_labels = std::max(metrics_.counters.peak_live_labels, sink.counters_.peak_live_labels);
}

double BenchmarkRecorder::elapsed_sec() const {
    if (!configured_) {
        return 0.0;
    }
    return BenchmarkClock::seconds_since(start_time_);
}

void BenchmarkRecorder::on_frontier_check_target() {
    metrics_.counters.target_frontier_checks++;
}

void BenchmarkRecorder::on_frontier_check_node() {
    metrics_.counters.node_frontier_checks++;
}

void BenchmarkRecorder::on_frontier_update() {
    frontier_updates_++;
}

CounterSet BenchmarkRecorder::current_counters_snapshot() const {
    CounterSet snapshot = metrics_.counters;
    snapshot.generated_labels += main_thread_sink_.counters_.generated_labels;
    snapshot.expanded_labels += main_thread_sink_.counters_.expanded_labels;
    snapshot.pruned_by_target += main_thread_sink_.counters_.pruned_by_target;
    snapshot.pruned_by_node += main_thread_sink_.counters_.pruned_by_node;
    snapshot.pruned_other += main_thread_sink_.counters_.pruned_other;
    snapshot.target_hits_raw += main_thread_sink_.counters_.target_hits_raw;
    snapshot.peak_open_size = std::max(snapshot.peak_open_size, main_thread_sink_.counters_.peak_open_size);
    snapshot.peak_live_labels = std::max(snapshot.peak_live_labels, main_thread_sink_.counters_.peak_live_labels);
    return snapshot;
}

bool BenchmarkRecorder::should_record_trace(const std::vector<FrontierPoint>& frontier, double elapsed_sec) const {
    const std::vector<FrontierPoint> normalized = sort_frontier_lexicographically(frontier);
    if (!frontier_points_equal(normalized, last_frontier_snapshot_)) {
        return true;
    }

    if (trace_interval_ms_ == 0) {
        return false;
    }

    if (last_trace_time_sec_ < 0.0) {
        return true;
    }

    const double trace_interval_sec = static_cast<double>(trace_interval_ms_) / 1000.0;
    return (elapsed_sec - last_trace_time_sec_) >= trace_interval_sec;
}

void BenchmarkRecorder::on_target_frontier_changed(const std::vector<FrontierPoint>& frontier, const char* trigger) {
    if (!configured_) { return; }

    const double elapsed_sec = BenchmarkClock::seconds_since(start_time_);
    const std::vector<FrontierPoint> normalized = sort_frontier_lexicographically(frontier);

    if (normalized.empty()) {
        last_frontier_snapshot_.clear();
        return;
    }

    if (!should_record_trace(normalized, elapsed_sec)) {
        last_frontier_snapshot_ = normalized;
        return;
    }

    if (metrics_.time_to_first_solution_sec < 0.0) {
        metrics_.time_to_first_solution_sec = elapsed_sec;
    }

    AnytimePoint point;
    point.time_sec = elapsed_sec;
    point.frontier_size = normalized.size();
    const CounterSet counters = current_counters_snapshot();
    point.generated_labels = counters.generated_labels;
    point.expanded_labels = counters.expanded_labels;
    point.pruned_by_target = counters.pruned_by_target;
    point.pruned_by_node = counters.pruned_by_node;
    point.pruned_other = counters.pruned_other;
    point.target_hits_raw = counters.target_hits_raw;
    point.trigger = trigger == nullptr ? "frontier_change" : trigger;

    metrics_.anytime_trace.push_back(point);
    last_frontier_snapshot_ = normalized;
    last_trace_time_sec_ = elapsed_sec;
}

void BenchmarkRecorder::set_status(RunStatus status) {
    metrics_.status = status;
    metrics_.completed = (status == RunStatus::completed);
    status_set_ = true;
}

bool BenchmarkRecorder::has_status() const {
    return status_set_;
}

bool BenchmarkRecorder::is_configured() const {
    return configured_;
}

uint64_t BenchmarkRecorder::trace_interval_ms() const {
    return trace_interval_ms_;
}

void BenchmarkRecorder::set_frontier_artifact_path(const std::filesystem::path& csv_path) {
    metrics_.frontier_artifact_path = csv_path.string();
}

void BenchmarkRecorder::set_trace_artifact_path(const std::filesystem::path& csv_path) {
    metrics_.trace_artifact_path = csv_path.string();
}

RunMetrics BenchmarkRecorder::finalize(const std::vector<FrontierPoint>& final_frontier) {
    if (!configured_) {
        return metrics_;
    }

    if (!finalized_) {
        merge(main_thread_sink_);
        metrics_.runtime_sec = BenchmarkClock::seconds_since(start_time_);
        metrics_.final_frontier = sort_frontier_lexicographically(normalize_frontier(final_frontier));
        metrics_.counters.final_frontier_size = metrics_.final_frontier.size();
        finalized_ = true;
    }

    return metrics_;
}

const RunMetrics& BenchmarkRecorder::metrics() const {
    return metrics_;
}

void BenchmarkRecorder::write_summary_header(std::ofstream& output) const {
    output
        << "schema_version,solver_name,dataset_id,query_id,start_node,target_node,num_objectives,"
        << "threads_requested,threads_effective,budget_sec,runtime_sec,status,completed,seed,"
        << "generated_labels,expanded_labels,pruned_by_target,pruned_by_node,pruned_other,"
        << "target_hits_raw,final_frontier_size,peak_open_size,peak_live_labels,"
        << "target_frontier_checks,node_frontier_checks,time_to_first_solution_sec,"
        << "frontier_artifact_path,trace_artifact_path"
        << std::endl;
}

void BenchmarkRecorder::write_summary_row(const std::filesystem::path& csv_path) const {
    ensure_parent_directory(csv_path);

    const bool needs_header = file_is_empty_or_missing(csv_path);
    std::ofstream output(csv_path, std::fstream::app);
    if (!output.is_open()) {
        perror(("Cannot open summary CSV " + csv_path.string()).c_str());
        exit(EXIT_FAILURE);
    }

    if (needs_header) {
        write_summary_header(output);
    }

    output
        << csv_string(metrics_.schema_version) << ","
        << csv_string(metrics_.solver_name) << ","
        << csv_string(metrics_.dataset_id) << ","
        << csv_string(metrics_.query_id) << ","
        << metrics_.start_node << ","
        << metrics_.target_node << ","
        << metrics_.num_objectives << ","
        << metrics_.threads_requested << ","
        << metrics_.threads_effective << ","
        << csv_double(metrics_.budget_sec) << ","
        << csv_double(metrics_.runtime_sec) << ","
        << csv_string(run_status_to_string(metrics_.status)) << ","
        << csv_bool(metrics_.completed) << ","
        << csv_string(metrics_.seed) << ","
        << metrics_.counters.generated_labels << ","
        << metrics_.counters.expanded_labels << ","
        << metrics_.counters.pruned_by_target << ","
        << metrics_.counters.pruned_by_node << ","
        << metrics_.counters.pruned_other << ","
        << metrics_.counters.target_hits_raw << ","
        << metrics_.counters.final_frontier_size << ","
        << metrics_.counters.peak_open_size << ","
        << metrics_.counters.peak_live_labels << ","
        << metrics_.counters.target_frontier_checks << ","
        << metrics_.counters.node_frontier_checks << ","
        << csv_double(metrics_.time_to_first_solution_sec) << ","
        << csv_string(metrics_.frontier_artifact_path) << ","
        << csv_string(metrics_.trace_artifact_path)
        << std::endl;
}

void BenchmarkRecorder::write_frontier_csv(const std::filesystem::path& csv_path) const {
    ensure_parent_directory(csv_path);

    std::ofstream output(csv_path, std::fstream::trunc);
    if (!output.is_open()) {
        perror(("Cannot open frontier CSV " + csv_path.string()).c_str());
        exit(EXIT_FAILURE);
    }

    output << "time_found_sec";
    for (size_t idx = 0; idx < metrics_.num_objectives; idx++) {
        output << ",obj" << (idx + 1);
    }
    output << std::endl;

    for (const FrontierPoint& point : metrics_.final_frontier) {
        output << csv_double(point.time_found_sec);
        const std::vector<cost_t> row = frontier_cost_row(point, metrics_.num_objectives);
        for (cost_t value : row) {
            output << "," << value;
        }
        output << std::endl;
    }
}

void BenchmarkRecorder::write_trace_csv(const std::filesystem::path& csv_path) const {
    ensure_parent_directory(csv_path);

    std::ofstream output(csv_path, std::fstream::trunc);
    if (!output.is_open()) {
        perror(("Cannot open trace CSV " + csv_path.string()).c_str());
        exit(EXIT_FAILURE);
    }

    output
        << "time_sec,trigger,frontier_size,generated_labels,expanded_labels,"
        << "pruned_by_target,pruned_by_node,pruned_other,target_hits_raw,hv_ratio,recall"
        << std::endl;

    for (const AnytimePoint& point : metrics_.anytime_trace) {
        output
            << csv_double(point.time_sec) << ","
            << csv_string(point.trigger) << ","
            << point.frontier_size << ","
            << point.generated_labels << ","
            << point.expanded_labels << ","
            << point.pruned_by_target << ","
            << point.pruned_by_node << ","
            << point.pruned_other << ","
            << point.target_hits_raw << ","
            << csv_double(point.hv_ratio) << ","
            << csv_double(point.recall)
            << std::endl;
    }
}

std::string run_status_to_string(RunStatus status) {
    switch (status) {
        case RunStatus::completed: return "completed";
        case RunStatus::timeout: return "timeout";
        case RunStatus::crash: return "crash";
        case RunStatus::oom: return "oom";
        case RunStatus::aborted: return "aborted";
    }
    return "aborted";
}

std::string sanitize_artifact_component(const std::string& raw) {
    std::string cleaned;
    cleaned.reserve(raw.size());

    for (char ch : raw) {
        if (std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '-' || ch == '_' || ch == '.') {
            cleaned.push_back(ch);
        } else {
            cleaned.push_back('_');
        }
    }

    if (cleaned.empty()) {
        return "artifact";
    }
    return cleaned;
}

bool frontier_costs_equal(const std::vector<cost_t>& lhs, const std::vector<cost_t>& rhs) {
    return lhs == rhs;
}

bool frontier_points_equal(const std::vector<FrontierPoint>& lhs, const std::vector<FrontierPoint>& rhs) {
    if (lhs.size() != rhs.size()) { return false; }

    for (size_t i = 0; i < lhs.size(); i++) {
        if (!frontier_costs_equal(lhs[i].cost, rhs[i].cost)) { return false; }
    }
    return true;
}

bool insert_nondominated_frontier_point(std::vector<FrontierPoint>& frontier, const FrontierPoint& point) {
    for (FrontierPoint& existing : frontier) {
        if (frontier_costs_equal(existing.cost, point.cost)) {
            if (existing.time_found_sec < 0.0 || (point.time_found_sec >= 0.0 && point.time_found_sec < existing.time_found_sec)) {
                existing.time_found_sec = point.time_found_sec;
            }
            return false;
        }
        if (weakly_dominate_vector(existing.cost, point.cost)) {
            return false;
        }
    }

    frontier.erase(
        std::remove_if(
            frontier.begin(),
            frontier.end(),
            [&point](const FrontierPoint& existing) {
                return weakly_dominate_vector(point.cost, existing.cost);
            }
        ),
        frontier.end()
    );

    frontier.push_back(point);
    return true;
}

std::vector<FrontierPoint> normalize_frontier(const std::vector<FrontierPoint>& frontier) {
    std::vector<FrontierPoint> normalized;
    normalized.reserve(frontier.size());

    for (const FrontierPoint& point : frontier) {
        insert_nondominated_frontier_point(normalized, point);
    }

    return normalized;
}

std::vector<FrontierPoint> sort_frontier_lexicographically(const std::vector<FrontierPoint>& frontier) {
    std::vector<FrontierPoint> sorted = frontier;
    std::sort(
        sorted.begin(),
        sorted.end(),
        [](const FrontierPoint& lhs, const FrontierPoint& rhs) {
            if (lexicographically_smaller_vector(lhs.cost, rhs.cost)) { return true; }
            if (lexicographically_smaller_vector(rhs.cost, lhs.cost)) { return false; }
            return lhs.time_found_sec < rhs.time_found_sec;
        }
    );
    return sorted;
}

std::vector<cost_t> frontier_cost_row(const FrontierPoint& point, size_t num_objectives) {
    std::vector<cost_t> row;
    const size_t limit = std::min(point.cost.size(), num_objectives);
    row.reserve(limit);

    for (size_t idx = 0; idx < limit; idx++) {
        row.push_back(point.cost[idx]);
    }
    return row;
}
