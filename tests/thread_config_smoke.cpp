#include "utils/thread_config.h"

#include <cassert>
#include <sstream>
#include <stdexcept>

int main() {
    const unsigned int hardware_threads = 8;

    const SolverThreadConfig serial_default =
        resolve_solver_thread_config("LTMOA", -1, false, hardware_threads);
    assert(serial_default.requested_threads == 1);
    assert(serial_default.effective_threads == 1);
    assert(!serial_default.used_hardware_default);
    assert(!serial_default.oversubscribes_hardware);

    const SolverThreadConfig sopmoa_default =
        resolve_solver_thread_config("SOPMOA", -1, false, hardware_threads);
    assert(sopmoa_default.requested_threads == 8);
    assert(sopmoa_default.effective_threads == 8);
    assert(sopmoa_default.used_hardware_default);
    assert(!sopmoa_default.oversubscribes_hardware);

    const SolverThreadConfig sopmoa_four =
        resolve_solver_thread_config("SOPMOA", 4, true, hardware_threads);
    assert(sopmoa_four.requested_threads == 4);
    assert(sopmoa_four.effective_threads == 4);
    assert(!sopmoa_four.used_hardware_default);
    assert(!sopmoa_four.oversubscribes_hardware);

    const SolverThreadConfig sopmoa_sixteen =
        resolve_solver_thread_config("SOPMOA", 16, true, hardware_threads);
    assert(sopmoa_sixteen.requested_threads == 16);
    assert(sopmoa_sixteen.effective_threads == 12);
    assert(sopmoa_sixteen.oversubscribes_hardware);
    assert(sopmoa_sixteen.capped);

    assert(resolve_parallel_worker_threads(-1, 0) == 1);
    assert(resolve_parallel_worker_threads(13, hardware_threads) == 13);

    const SolverThreadConfig parallel_default =
        resolve_solver_thread_config("LTMOA_parallel", -1, false, hardware_threads);
    assert(parallel_default.requested_threads == 8);
    assert(parallel_default.effective_threads == 8);
    assert(parallel_default.used_hardware_default);

    bool threw = false;
    try {
        (void)resolve_solver_thread_config("LTMOA", 2, true, hardware_threads);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        (void)resolve_solver_thread_config("SOPMOA_relaxed", 0, true, hardware_threads);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);

    std::ostringstream oversubscribe_log;
    emit_solver_thread_config_log(oversubscribe_log, "SOPMOA", sopmoa_sixteen);
    const std::string oversubscribe_message = oversubscribe_log.str();
    assert(oversubscribe_message.find("requested=16") != std::string::npos);
    assert(oversubscribe_message.find("effective=12") != std::string::npos);
    assert(oversubscribe_message.find("hardware=8") != std::string::npos);
    assert(oversubscribe_message.find("capped") != std::string::npos);

    std::ostringstream default_log;
    emit_solver_thread_config_log(default_log, "SOPMOA", sopmoa_default);
    assert(default_log.str().find("defaulted to hardware_concurrency") != std::string::npos);

    return 0;
}
