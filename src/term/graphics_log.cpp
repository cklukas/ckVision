// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/term/graphics_log.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <stdlib.h>
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
    // The first ckVision process to open this file starts it empty: a log
    // that accumulates every run since it was first named makes the reader
    // hunt for where the run they care about began, and the run they care
    // about is the one they just made.
    //
    // A contained child session inherits this variable, and its lines are
    // the point of reading a host and a child together in one file, so it
    // must not begin by erasing its host's. The marker below travels
    // through the environment for exactly that reason: this process sets
    // it after truncating, so anything it launches appends instead.
    constexpr const char* kOwnerMarker = "CKVISION_GRAPHICS_LOG_OWNED";
    const char* const owned = std::getenv(kOwnerMarker);
    const bool already_owned = owned != nullptr && *owned != '\0';
    log.stream = std::fopen(target.c_str(), already_owned ? "a" : "w");
    if (log.stream == nullptr) return false;
    if (!already_owned) {
#if defined(_WIN32)
        // MSVC has never had POSIX `setenv`; `_putenv_s` is the spelling, and
        // it always overwrites, which is what the `1` on the POSIX call asks
        // for. The two diverge on REMOVAL — `_putenv_s(name, "")` deletes the
        // variable where POSIX needs a separate `unsetenv` — so this shim is
        // deliberately not a general one: nothing here removes, and the value
        // is the literal "1", so the divergence cannot be reached from this
        // call site. Anyone widening it to take a caller's value has to
        // decide what an empty one means before they do.
        (void)::_putenv_s(kOwnerMarker, "1");
#else
        (void)::setenv(kOwnerMarker, "1", 1);
#endif
    }
    return true;
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
