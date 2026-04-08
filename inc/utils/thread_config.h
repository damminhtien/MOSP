#ifndef MOSP_THREAD_CONFIG_H
#define MOSP_THREAD_CONFIG_H

#include "../common_inc.h"

#include <ostream>
#include <stdexcept>
#include <string_view>

struct SolverThreadConfig {
    int requested_threads = 1;
    int effective_threads = 1;
    unsigned int hardware_threads = 1;
    bool used_hardware_default = false;
    bool oversubscribes_hardware = false;
    bool capped = false;
    std::string cap_reason;
};

inline unsigned int normalized_hardware_threads(unsigned int hardware_threads = std::thread::hardware_concurrency()) {
    return std::max(1u, hardware_threads);
}

inline bool solver_uses_parallel_threads(std::string_view algorithm) {
    return algorithm == "SOPMOA"
        || algorithm == "SOPMOA_relaxed"
        || algorithm == "SOPMOA_bucket";
}

inline int resolve_parallel_worker_threads(
    int requested_threads,
    unsigned int hardware_threads = std::thread::hardware_concurrency()
) {
    if (requested_threads > 0) {
        return requested_threads;
    }
    return static_cast<int>(normalized_hardware_threads(hardware_threads));
}

inline SolverThreadConfig resolve_solver_thread_config(
    std::string_view algorithm,
    int requested_threads,
    bool explicit_request,
    unsigned int hardware_threads = std::thread::hardware_concurrency()
) {
    const unsigned int normalized_hardware = normalized_hardware_threads(hardware_threads);
    SolverThreadConfig config;
    config.hardware_threads = normalized_hardware;

    if (!solver_uses_parallel_threads(algorithm)) {
        if (explicit_request && requested_threads != 1) {
            throw std::invalid_argument("--numthreads is only valid as 1 for serial solvers");
        }
        config.requested_threads = 1;
        config.effective_threads = 1;
        return config;
    }

    if (explicit_request) {
        if (requested_threads <= 0) {
            throw std::invalid_argument("--numthreads must be positive when explicitly provided");
        }
        config.requested_threads = requested_threads;
        config.effective_threads = requested_threads;
    } else {
        config.requested_threads = static_cast<int>(normalized_hardware);
        config.effective_threads = static_cast<int>(normalized_hardware);
        config.used_hardware_default = true;
    }

    config.oversubscribes_hardware =
        static_cast<unsigned int>(config.effective_threads) > normalized_hardware;
    return config;
}

inline void emit_solver_thread_config_log(
    std::ostream& os,
    std::string_view algorithm,
    const SolverThreadConfig& config
) {
    os << "Thread config for " << algorithm
       << ": requested=" << config.requested_threads
       << ", effective=" << config.effective_threads
       << ", hardware=" << config.hardware_threads;

    if (!solver_uses_parallel_threads(algorithm)) {
        os << " (serial solver)";
    } else if (config.used_hardware_default) {
        os << " (defaulted to hardware_concurrency)";
    } else if (config.oversubscribes_hardware) {
        os << " (oversubscribing hardware)";
    }

    if (config.capped) {
        os << " (capped: " << config.cap_reason << ")";
    }

    os << '\n';
}

#endif
