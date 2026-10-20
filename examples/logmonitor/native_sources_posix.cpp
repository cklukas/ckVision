// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "native_sources.hpp"
#include "process_json_feed.hpp"
#include "cvision/term/process_stream.hpp"
#include "cvision/term/process_runner.hpp"
#include <unistd.h>
#include <stdexcept>

namespace ckv::logmonitor {
struct NativeLogSources::Impl {
    term::NativeProcessStreamService streams;
    std::string executable, unavailable;
    NativeLogSourceOptions options;
    explicit Impl(const Clock& clock, std::shared_ptr<const LogWake> wake, NativeLogSourceOptions settings) : streams(clock, 8, std::move(wake)), options(std::move(settings)) {
        if (options.apple_process.size() > 256 || options.apple_process.find('\0') != std::string::npos) throw std::invalid_argument("native source process limit exceeds bounds");
        if (options.journal_identifier.size() > 256 || options.journal_identifier.find('\0') != std::string::npos) throw std::invalid_argument("native source journal identifier exceeds bounds");
#if defined(__APPLE__)
        executable = "/usr/bin/log";
#else
        for (auto path : {"/usr/bin/journalctl", "/bin/journalctl"}) if (::access(path, X_OK) == 0) { executable = path; break; }
#endif
        if (executable.empty() || ::access(executable.c_str(), X_OK) != 0) { unavailable = "Native helper unavailable. Choose an application log file or Sample."; executable.clear(); return; }
        term::NativeProcessRunner probe(clock); core::ProcessRunRequest request;
#if defined(__APPLE__)
        request.launch = core::ProcessLaunchSpec::program(executable, {"help", "stream"});
#else
        request.launch = core::ProcessLaunchSpec::program(executable, {"--help"});
#endif
        request.descendants = core::ProcessDescendantPolicy::TerminateOnCompletion;
        request.max_stdout_bytes = 64 * 1024; request.max_stderr_bytes = 4096; request.idle_budget_nanos = 2'000'000'000;
        const auto result = probe.run(request);
#if defined(__APPLE__)
        const bool format = supports_native_json(NativeJsonSchema::Apple, result);
#else
        const bool format = supports_native_json(NativeJsonSchema::Journal, result);
#endif
        if (!format) {
            unavailable = "Native structured-mode probe failed";
            if (result.error.code) unavailable += " (native error " + std::to_string(result.error.code) + ")";
            if (result.exit) unavailable += " (exit " + std::to_string(result.exit->code) + ")";
            if (result.state == core::ProcessRunState::IdleTimeout) unavailable += " (helper progress timeout)";
            if (result.stdout_capture.truncated || result.stderr_capture.truncated) unavailable += " (help output exceeded probe bound)";
            unavailable += ". Installed helper must advertise the required JSON follow mode. Retry discovery at startup, or choose File/Sample.";
            executable.clear();
        }
    }
    std::unique_ptr<StreamFeed> open(LogSourceId id, std::shared_ptr<const LogWake> wake, bool user) {
        if (executable.empty()) return {};
        core::ProcessStreamRequest request;
#if defined(__APPLE__)
        (void)user;
        request.launch = core::ProcessLaunchSpec::program(executable, {"stream", "--style", "ndjson", "--level", "info", "--type", "log", "--color", "none"});
        constexpr auto schema = NativeJsonSchema::Apple;
        std::string explanation = "Apple live info/default stream (debug excluded). Raw NDJSON and redaction retained. No startup history; retry may leave a gap. Local filters do not change this subscription.";
        if (!options.apple_process.empty()) { request.launch.arguments.insert(request.launch.arguments.end(), {"--process", options.apple_process}); explanation += " Explicit source process limit: " + options.apple_process; }
#else
        request.launch = core::ProcessLaunchSpec::program(executable, {"--no-pager", "--follow", "--lines=100", "--output=json"});
        if (user) request.launch.arguments.emplace_back("--user");
        if (!options.journal_identifier.empty()) request.launch.arguments.emplace_back("SYSLOG_IDENTIFIER=" + options.journal_identifier);
        constexpr auto schema = NativeJsonSchema::Journal;
        std::string explanation = "journalctl: last 100 accessible records then live, in one invocation. Raw JSON/cursors retained. Retry restarts recent history: duplicates/gaps possible. Local filters do not change this subscription.";
        if (!options.journal_identifier.empty()) explanation += " Explicit source identifier limit: " + options.journal_identifier;
#endif
        request.stdout_source = 1; request.stderr_source = 2; request.chunk_bytes = 16 * 1024;
        request.stdout_queue = {4, 16 * 1024, LogOverflowPolicy::Backpressure};
        request.stderr_queue = {2, 16 * 1024, LogOverflowPolicy::Backpressure};
        auto opened = streams.start(std::move(request)); if (!opened.session) return {};
        return std::make_unique<ProcessJsonFeed>(id, schema, std::move(opened.session), std::move(wake), std::move(explanation));
    }
};
NativeLogSources::NativeLogSources(const Clock& clock, std::shared_ptr<const LogWake> wake, NativeLogSourceOptions options) : impl_(std::make_unique<Impl>(clock, std::move(wake), std::move(options))) {}
NativeLogSources::~NativeLogSources() = default;
std::vector<NativeSourceDescriptor> NativeLogSources::catalog() {
    std::vector<NativeSourceDescriptor> result;
    const bool available = !impl_->executable.empty();
#if defined(__APPLE__)
    result.push_back({"apple-live", impl_->options.apple_process.empty() ? "macOS system log (live info/default)" : "macOS process " + impl_->options.apple_process,
                      available ? "Live subscription; no history. Channel permissions and redaction apply." : impl_->unavailable, available,
                      [this](LogSourceId id, std::shared_ptr<const LogWake> wake) { return impl_->open(id, std::move(wake), false); }});
#else
    for (const bool user : {false, true}) result.push_back({user ? "journal-user" : "journal-accessible", user ? "Linux user journal" : "Linux accessible journal",
        available ? "Recent 100 records and live. Source permissions apply; File/Sample always available." : impl_->unavailable, available,
        [this, user](LogSourceId id, std::shared_ptr<const LogWake> wake) { return impl_->open(id, std::move(wake), user); }});
#endif
    return result;
}
}  // namespace ckv::logmonitor
