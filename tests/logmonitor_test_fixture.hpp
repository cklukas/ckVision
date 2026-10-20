// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once
#include "logmonitor_app.hpp"
#include "log_file_test_fixture.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/core/golden.hpp"
#include "cvision/scene/golden_capture.hpp"
namespace ckv::testing {
inline widgets::Button* logmonitor_button(ui::View& root, std::string_view caption) {
    if (auto* button = dynamic_cast<widgets::Button*>(&root); button && button->text() == caption) return button;
    for (const auto& child : root.children()) if (auto* button = logmonitor_button(*child, caption)) return button;
    return nullptr;
}
class MemoryExportWriter final : public logmonitor::ExportWriter {
public:
    explicit MemoryExportWriter(FileSystem& filesystem) : fs(filesystem) {}
    FileSystem& fs; logmonitor::ExportJob job; logmonitor::ExportResult result;
    bool busy = false, ready = false;
    bool submit(logmonitor::ExportJob& input) override {
        if (busy || ready) return false;
        job = std::move(input); busy = true; return true;
    }
    bool take(logmonitor::ExportResult& output) override {
        if (!ready) return false;
        output = std::move(result); ready = false; return true;
    }
    void complete() {
        const auto status = fs.write_file_atomic(job.path, job.contents, FileWriteExpectation::must_not_exist()).status;
        result = {job.revision, status, job.path, job.scope}; busy = false; ready = true;
    }
};
struct LogMonitorFixture {
    ManualClock clock;
    term::HeadlessTerminal terminal;
    ui::Application app;
    MemoryFileSystem fs;
    ScriptedLogFiles files;
    MemoryExportWriter exports{fs};
    std::shared_ptr<LogActivity> activity = std::make_shared<LogActivity>();
    logmonitor::LogMonitorApp monitor;
    std::size_t responses = 0;
    std::string opened_path;
    bool denied = false;
    explicit LogMonitorFixture(Size size = {100, 30})
        : terminal(size), app(terminal, clock), monitor(app, files, fs, exports, activity,
            {logmonitor::NativeSourceDescriptor{"unavailable", "System logs", "System source is unavailable", false, {}}}) {}
    void step(std::int64_t advance = 1'000'000) {
        clock.advance(advance);
        if (responses != files.request_count()) {
            responses = files.request_count(); const auto& request = files.requested();
            if (request.operation == LogFileOperation::Inspect) opened_path = request.path;
            const auto file = fs.read_file(opened_path);
            if (denied) files.publish(request.revision, {}, {}, LogSourceState::Denied);
            else if (!file) files.publish(request.revision, {}, {}, LogSourceState::Missing);
            else {
                const auto data = request.operation == LogFileOperation::Read && request.offset < file->contents.size() ?
                    std::string_view(file->contents).substr(static_cast<std::size_t>(request.offset), request.bytes) : std::string_view{};
                files.publish(request.revision, LogFileObservation{{1, 1, 0}, file->contents.size(), responses}, data);
            }
        }
        monitor.step(clock.now_nanos());
        if (app.frames_awaiting_terminal()) terminal.inject_bytes("\x1B[0n", clock.now_nanos());
        app.step(clock.now_nanos());
    }
    void settle(std::size_t turns = 40) { for (std::size_t i = 0; i < turns; ++i) step(); }
    std::string golden() { return golden::serialize(scene::capture_frame(app.compositor(), app.current_cursor())); }
};
inline constexpr std::array<std::string_view, 10> logmonitor_golden_names{
    "picker", "live", "paused", "filtered", "overload", "details", "unavailable", "file_error", "export", "narrow"
};
template<class Capture> inline auto with_logmonitor_scenario(std::string_view name, Capture capture) {
    LogMonitorFixture fixture(name == "narrow" ? Size{45, 18} : Size{100, 30});
    fixture.step();
    if (name == "unavailable") fixture.monitor.open_native(0);
    else if (name == "file_error") { fixture.monitor.open_file("/missing.log"); fixture.settle(); }
    else if (name != "picker") {
        fixture.monitor.open_sample(name == "overload" ? logmonitor::SampleMode::Overload : logmonitor::SampleMode::Steady);
        fixture.settle();
        for (int i = 0; i < 9; ++i) { fixture.step(100'000'000); fixture.settle(8); }
        if (name == "paused") { fixture.monitor.panel(0)->view().set_top_record(0); fixture.step(100'000'000); fixture.settle(); }
        if (name == "filtered") { fixture.monitor.set_filter("event 7"); fixture.settle(); }
        if (name == "details") fixture.monitor.show_details();
        if (name == "export") { fixture.monitor.begin_export("/exports/retained.txt", logmonitor::ExportScope::FilteredRetained); fixture.settle(); fixture.exports.complete(); }
    }
    fixture.step(); return capture(fixture);
}
inline std::string logmonitor_golden(std::string_view name) { return with_logmonitor_scenario(name, [](auto& fixture) { return fixture.golden(); }); }
}  // namespace ckv::testing
