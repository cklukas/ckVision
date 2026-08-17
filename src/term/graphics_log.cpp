// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/term/graphics_log.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>

#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace ckv::term {
namespace {

struct LogState {
    std::FILE* stream = nullptr;
    std::chrono::steady_clock::time_point start;
    std::mutex mutex;
};

LogState& state() {
    static LogState instance;
    return instance;
}

std::atomic<int> g_enabled{-1};  // -1 not yet decided, 0 off, 1 on

bool open_log() {
    const char* const destination = std::getenv("CKVISION_GRAPHICS_LOG");
    if (destination == nullptr || *destination == '\0') return false;
    LogState& log = state();
    log.start = std::chrono::steady_clock::now();
    const std::string target(destination);
    if (target == "-" || target == "stderr") {
        log.stream = stderr;
        return true;
    }
    log.stream = std::fopen(target.c_str(), "a");
    return log.stream != nullptr;
}

}  // namespace

bool graphics_log_enabled() noexcept {
    int decided = g_enabled.load(std::memory_order_relaxed);
    if (decided < 0) {
        static std::once_flag once;
        std::call_once(once, [] { g_enabled.store(open_log() ? 1 : 0, std::memory_order_relaxed); });
        decided = g_enabled.load(std::memory_order_relaxed);
    }
    return decided == 1;
}

void graphics_log(std::string_view message) {
    if (!graphics_log_enabled()) return;
    LogState& log = state();
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - log.start).count();
#if !defined(_WIN32)
    const long pid = static_cast<long>(::getpid());
#else
    const long pid = 0;
#endif
    const std::lock_guard<std::mutex> guard(log.mutex);
    std::fprintf(log.stream, "[%8.1fms pid %ld] %.*s\n", ms, pid, static_cast<int>(message.size()),
                 message.data());
    std::fflush(log.stream);
}

}  // namespace ckv::term
