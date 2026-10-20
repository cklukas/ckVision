// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "../examples/logmonitor/native_sources.hpp"
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/core/filesystem.hpp"
#include <chrono>
#include <iostream>
#include <thread>
#include <unistd.h>
#if defined(__APPLE__)
#include <os/log.h>
#else
#include <syslog.h>
#endif
// A native integration executable, separate from deterministic replay suites.
// Uses the same app graph and real POSIX subscription; no real window claim.
namespace {
class UnusedExport final : public ckv::logmonitor::ExportWriter {
public:
    bool submit(ckv::logmonitor::ExportJob&) override { return false; }
    bool take(ckv::logmonitor::ExportResult&) override { return false; }
};
}
int main() {
    using namespace ckv;
    term::PosixClock clock;
    auto activity = std::make_shared<LogActivity>();
#if defined(__APPLE__)
    logmonitor::NativeLogSources native(clock, activity, {.apple_process = std::to_string(::getpid())});
#else
    logmonitor::NativeLogSources native(clock, activity, {.journal_identifier = "ckvision-logmonitor-proof"});
#endif
    auto catalog = native.catalog();
    if (catalog.empty() || !catalog[0].available) { std::cerr << "Native capability unavailable: " << (catalog.empty() ? "empty catalog" : catalog[0].explanation) << '\n'; return 77; }
    MemoryFileSystem filesystem;
    // No file source is opened by this integration case.
    class EmptyFiles final : public LogFileService { public: std::unique_ptr<LogFileSession> open_session() override { return {}; } } files;
    UnusedExport exports;
    term::HeadlessTerminal terminal({100, 30}); ui::Application app(terminal, clock);
    logmonitor::LogMonitorApp monitor(app, files, filesystem, exports, activity, std::move(catalog));
    if (!monitor.open_native(0)) return 1;
    const auto step = [&] {
        monitor.step(clock.now_nanos());
        if (app.frames_awaiting_terminal()) terminal.inject_bytes("\x1B[0n", clock.now_nanos());
        app.step(clock.now_nanos());
    };
    step();
#if defined(__APPLE__)
    struct Logger { os_log_t handle = os_log_create("org.ckvision.logmonitor.proof", "controlled"); ~Logger() { os_release(handle); }
        void emit(const std::string& tag) { os_log_with_type(handle, OS_LOG_TYPE_DEFAULT, "controlled public test event %{public}s", tag.c_str()); } } logger;
#else
    struct Logger { Logger() { ::openlog("ckvision-logmonitor-proof", LOG_PID, LOG_USER); } ~Logger() { ::closelog(); }
        void emit(const std::string& tag) { ::syslog(LOG_NOTICE, "controlled public test event %s", tag.c_str()); } } logger;
#endif
    const std::string tag = "ckvision-native-proof-" + std::to_string(clock.now_nanos());
    const auto deadline = clock.now_nanos() + 20'000'000'000;
    auto next_emit = clock.now_nanos(); bool found = false;
    while (clock.now_nanos() < deadline) {
        if (clock.now_nanos() >= next_emit) {
            logger.emit(tag);
            next_emit = clock.now_nanos() + 100'000'000;
        }
        step();
        const auto* model = monitor.model(0);
        for (std::size_t index = 0; model && index < model->size(); ++index) if (model->at(index)->raw.find(tag) != std::string_view::npos) { found = true; break; }
        if (found) break;
        if (auto* panel = monitor.panel(0); panel && panel->source_status().state == LogSourceState::Failed) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!found) { std::cerr << "Controlled event not observed. Source state=" << static_cast<unsigned>(monitor.panel(0)->source_status().state) << " native error=" << monitor.panel(0)->source_status().native_error << '\n'; return 1; }
    const auto old_query = monitor.panel(0)->provider().query_revision();
    monitor.set_filter(tag);
    const auto filter_deadline = clock.now_nanos() + 2'000'000'000;
    while (clock.now_nanos() < filter_deadline && monitor.panel(0)->provider().query_revision() <= old_query) { step(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    auto* panel = monitor.panel(0);
    if (!panel || !panel->provider().size()) { std::cerr << "Tagged local filter did not publish\n"; return 1; }
    const auto record = panel->provider().at(0);
    if (!record || record->raw.find(tag) == std::string_view::npos || !record->timestamp_nanos) {
        std::cerr << "Tagged filtered record/timestamp check failed. exists=" << record.has_value() << " timestamp=" << (record && record->timestamp_nanos.has_value()) << '\n';
        if (record) for (const auto& value : record->metadata().fields) if (value.name == "timestamp") std::cerr << "Tagged source timestamp format: " << value.value << '\n';
        return 1;
    }
    panel->view().set_top_record(0);
    panel->invoke(widgets::LogPanelAction::Pause);
    const auto resume_tag = tag + "-resume";
    next_emit = clock.now_nanos();
    const auto resume_deadline = clock.now_nanos() + 10'000'000'000;
    bool intake = false;
    while (clock.now_nanos() < resume_deadline && !intake) {
        if (clock.now_nanos() >= next_emit) { logger.emit(resume_tag); next_emit = clock.now_nanos() + 100'000'000; }
        step(); const auto* model = monitor.model(0);
        for (std::size_t index = 0; index < model->size(); ++index) if (model->at(index)->raw.find(resume_tag) != std::string_view::npos) { intake = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    panel->invoke(widgets::LogPanelAction::Resume); step();
#if defined(__APPLE__)
    const bool detail = monitor.detail_text().find("org.ckvision.logmonitor.proof") != std::string::npos;
#else
    const bool detail = monitor.detail_text().find("ckvision-logmonitor-proof") != std::string::npos;
#endif
    const bool resumed = panel->view().state().following;
    monitor.close_source(0); step();
    if (!intake || !resumed || !detail || monitor.source_count()) {
        std::cerr << "Live checks: tagged paused intake=" << intake << " resumed=" << resumed << " subsystem detail=" << detail << " remaining sources=" << monitor.source_count() << '\n'; return 1;
    }
    std::cout << "Native POSIX tagged event, timestamp, local filter, detail, live pause/resume and source retirement passed. No native window inspected.\n";
    return 0;
}
