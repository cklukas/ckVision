// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "../examples/logmonitor/native_sources.hpp"
#include "cvision/term/windows_clock.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/core/filesystem.hpp"
#include <chrono>
#include <iostream>
#include <thread>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
namespace {
class UnusedExport final : public ckv::logmonitor::ExportWriter {
public:
    bool submit(ckv::logmonitor::ExportJob&) override { return false; }
    bool take(ckv::logmonitor::ExportResult&) override { return false; }
};
class EmptyFiles final : public ckv::LogFileService {
public:
    std::unique_ptr<ckv::LogFileSession> open_session() override { return {}; }
};
struct Logger {
    HANDLE handle = ::RegisterEventSourceW(nullptr, L"ckVisionLogProof");
    ~Logger() { if (handle) ::DeregisterEventSource(handle); }
    bool emit(const std::string& tag) {
        const std::wstring text(tag.begin(), tag.end()); const wchar_t* strings[]{text.c_str()};
        return handle && ::ReportEventW(handle, EVENTLOG_INFORMATION_TYPE, 0, 123, nullptr, 1, 0, strings, nullptr);
    }
};
}
int main() {
    using namespace ckv;
    term::WindowsClock clock; auto activity = std::make_shared<LogActivity>();
    logmonitor::NativeLogSources native(clock, activity, {.windows_channel = "Application"});
    auto catalog = native.catalog();
    if (catalog.empty() || !catalog[0].available) { std::cerr << "Application channel unavailable\n"; return 77; }
    MemoryFileSystem filesystem; EmptyFiles files; UnusedExport exports;
    term::HeadlessTerminal terminal({100, 30}); ui::Application app(terminal, clock);
    logmonitor::LogMonitorApp monitor(app, files, filesystem, exports, activity, std::move(catalog));
    if (!monitor.open_native(0)) return 1;
    const auto step = [&] { monitor.step(clock.now_nanos()); if (app.frames_awaiting_terminal()) terminal.inject_bytes("\x1B[0n", clock.now_nanos()); app.step(clock.now_nanos()); };
    step(); Logger logger;
    if (!logger.handle) { std::cerr << "Test event writer denied: " << ::GetLastError() << '\n'; return 77; }
    const std::string tag = "ckvision-windows-proof-" + std::to_string(clock.now_nanos());
    const auto find = [&](const std::string& value) -> std::optional<widgets::LogRecord> {
        const auto* model = monitor.model(0);
        for (std::size_t index = 0; model && index < model->size(); ++index) { auto record = model->at(index); if (record && record->raw.find(value) != std::string_view::npos) return record; }
        return {};
    };
    const auto deadline = clock.now_nanos() + 20'000'000'000; auto next_emit = clock.now_nanos();
    while (clock.now_nanos() < deadline && !find(tag)) {
        if (clock.now_nanos() >= next_emit) { if (!logger.emit(tag)) { std::cerr << "Test event emission failed: " << ::GetLastError() << '\n'; return 1; } next_emit = clock.now_nanos() + 100'000'000; }
        step(); if (monitor.panel(0)->source_status().state == LogSourceState::Failed || monitor.panel(0)->source_status().state == LogSourceState::Denied) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const auto first = find(tag);
    if (!first || !first->timestamp_nanos) { std::cerr << "Tagged event/time not observed. State=" << static_cast<unsigned>(monitor.panel(0)->source_status().state) << " error=" << monitor.panel(0)->source_status().native_error << '\n'; return 1; }
    bool provider = false, event_id = false, missing_message = false; std::string bookmark;
    for (const auto& field : first->metadata().fields) {
        if (field.name == "provider" && field.value == "ckVisionLogProof") provider = true;
        if (field.name == "event_id" && field.value == "123") event_id = true;
        if (field.name == "bookmark") bookmark.assign(field.value);
        if (field.name == "render_error" && field.value != "0") missing_message = true;
    }
    if (!provider || !event_id || bookmark.empty() || !missing_message) { std::cerr << "Native metadata/bookmark/missing-message evidence incomplete\n"; return 1; }
    const auto old_query = monitor.panel(0)->provider().query_revision(); monitor.set_filter(tag);
    const auto query_deadline = clock.now_nanos() + 3'000'000'000;
    while (clock.now_nanos() < query_deadline && monitor.panel(0)->provider().query_revision() <= old_query) { step(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    auto* panel = monitor.panel(0);
    if (!panel->provider().size() || monitor.detail_text().find("ckVisionLogProof") == std::string::npos) return 1;
    panel->invoke(widgets::LogPanelAction::Pause); const auto resumed_tag = tag + "-resume"; next_emit = clock.now_nanos();
    const auto resume_deadline = clock.now_nanos() + 10'000'000'000;
    while (clock.now_nanos() < resume_deadline && !find(resumed_tag)) {
        if (clock.now_nanos() >= next_emit) { logger.emit(resumed_tag); next_emit = clock.now_nanos() + 100'000'000; }
        step(); std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!find(resumed_tag) || panel->view().state().following) return 1;
    panel->invoke(widgets::LogPanelAction::Resume); step(); if (!panel->view().state().following) return 1;
    monitor.close_source(0); step(); if (monitor.source_count()) return 1;
    // Explicit stale resume state must fail, never silently claim continuity.
    logmonitor::NativeLogSources stale(clock, activity, {.windows_channel = "Application", .windows_bookmark = "<BookmarkList><Bookmark Channel='Application' RecordId='18446744073709551615' IsCurrent='true'/></BookmarkList>"});
    auto stale_catalog = stale.catalog(); auto feed = stale_catalog[0].open(99, activity);
    if (!feed) return 1;
    const auto stale_deadline = clock.now_nanos() + 5'000'000'000;
    while (clock.now_nanos() < stale_deadline && feed->diagnostic().state == LogSourceState::Pending) { feed->step(clock.now_nanos()); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    if (feed->diagnostic().state != LogSourceState::Failed || !feed->diagnostic().native_error) { std::cerr << "Stale bookmark did not fail explicitly\n"; return 1; }
    feed->stop(clock.now_nanos()); feed.reset();
    logmonitor::NativeLogSources missing(clock, activity, {.windows_channel = "ckVision/nonexistent-controlled-channel"});
    if (missing.catalog().empty() || missing.catalog()[0].available) return 1;
    std::cout << "Native Windows tagged Application event, timestamp/provider/event ID/bookmark, missing-message raw XML, local filter, detail, pause/resume, stale resume failure and source retirement passed. No native window inspected.\n";
    return 0;
}
