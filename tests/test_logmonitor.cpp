// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "logmonitor_test_fixture.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/term/log_file_service.hpp"
#include "scratch_directory.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <chrono>
#include <thread>
#if defined(_WIN32)
#include "cvision/term/windows_filesystem.hpp"
#else
#include "cvision/term/posix_filesystem.hpp"
#endif
using namespace ckv;
using namespace ckv::logmonitor;
CK_TEST(logmonitor_source_picker_keyboard_starts_a_sample_and_keeps_native_failure_recoverable) {
    testing::LogMonitorFixture fixture; fixture.step();
    CK_CHECK(fixture.monitor.source_count() == 0 && fixture.app.focused() == &fixture.monitor.path_field());
    CK_CHECK(!fixture.monitor.open_native(0) && fixture.monitor.notice().find("File or Sample") != std::string_view::npos);
    fixture.monitor.sample_choice().set_selected_index(0);
    auto* start = testing::logmonitor_button(fixture.app.root(), "&Start sample"); CK_CHECK(start != nullptr);
    fixture.app.set_focus(start); CK_CHECK(fixture.app.dispatch(KeyEvent{{Key::Enter, Modifier::None, ""}}));
    fixture.settle(); CK_CHECK(fixture.monitor.source_count() == 1 && fixture.monitor.model(0)->size() >= 1);
    CK_CHECK(fixture.app.dispatch(KeyEvent{{Key::F5, Modifier::None, ""}}));
    fixture.step(100'000'000); fixture.settle(); CK_CHECK(!fixture.monitor.panel(0)->view().state().following);
    CK_CHECK(fixture.app.dispatch(KeyEvent{{Key::F5, Modifier::None, ""}})); CK_CHECK(fixture.monitor.panel(0)->view().state().following);
}
CK_TEST(logmonitor_tabs_share_filters_and_preserve_independent_pause_and_close_state) {
    testing::LogMonitorFixture fixture;
    fixture.monitor.open_sample(SampleMode::Steady); fixture.settle(); fixture.monitor.open_sample(SampleMode::Burst); fixture.settle();
    fixture.monitor.select_source(0); fixture.monitor.panel(0)->view().pause();
    fixture.monitor.set_filter("event", "missing"); fixture.settle();
    CK_CHECK(fixture.monitor.panel(0)->filter_box().query() == "event" && fixture.monitor.panel(1)->filter_box().query() == "event");
    fixture.monitor.set_source_filter(fixture.monitor.model(0)->source()); fixture.settle();
    CK_CHECK(fixture.monitor.panel(1)->provider().size() == 0 && fixture.monitor.panel(0)->provider().size() > 0);
    fixture.monitor.set_source_filter({}); fixture.settle();
    CK_CHECK(fixture.monitor.close_source(1) && fixture.monitor.source_count() == 1);
    CK_CHECK(!fixture.monitor.panel(0)->view().state().following);
    CK_CHECK(fixture.monitor.close_source(0) && fixture.monitor.source_count() == 0 && fixture.monitor.tabs().tab_count() == 1);
    fixture.step(); CK_CHECK(fixture.app.focused() == &fixture.monitor.path_field());
}
CK_TEST(logmonitor_overload_reports_loss_and_severity_without_stopping_keyboard_progress) {
    testing::LogMonitorFixture fixture; fixture.monitor.open_sample(SampleMode::Overload); fixture.settle();
    CK_CHECK(fixture.monitor.panel(0)->source_status().dropped > 0 && fixture.monitor.model(0)->size() <= 2048);
    fixture.monitor.set_severity(1U << static_cast<unsigned>(LogSeverity::Error)); fixture.settle();
    const auto& provider = fixture.monitor.panel(0)->provider();
    for (std::size_t i = 0; i < provider.size(); ++i) CK_CHECK(provider.at(i)->severity == LogSeverity::Error);
    CK_CHECK(fixture.app.dispatch(KeyEvent{{Key::Char, Modifier::Ctrl, "o"}})); fixture.step();
    CK_CHECK(fixture.monitor.tabs().active_index() == 0);
}
CK_TEST(logmonitor_scripted_file_missing_retry_append_and_source_stop_keep_retention) {
    testing::LogMonitorFixture fixture; fixture.monitor.open_file("/logs/app.log"); fixture.settle();
    CK_CHECK(fixture.monitor.panel(0)->source_status().state == LogSourceState::Missing);
    fixture.fs.add_file("/logs/app.log", "one\ntwo\n"); fixture.monitor.retry_active(); fixture.settle(80);
    CK_CHECK(fixture.monitor.model(0)->size() == 2);
    fixture.fs.add_file("/logs/app.log", "one\ntwo\nthree\n"); fixture.files.hint(); fixture.activity->notify(); fixture.settle(80);
    CK_CHECK(fixture.monitor.model(0)->size() == 3);
    const auto previous = fixture.monitor.model(0)->generation(); fixture.monitor.stop_active(); fixture.settle(); fixture.monitor.retry_active(); fixture.settle(80);
    CK_CHECK(fixture.monitor.model(0)->generation() > previous && fixture.monitor.model(0)->size() >= 3);
}
CK_TEST(logmonitor_rotation_and_truncation_samples_exercise_real_follower_generations) {
    for (auto mode : {SampleMode::Rotation, SampleMode::Truncation}) {
        testing::LogMonitorFixture fixture; fixture.monitor.open_sample(mode); fixture.settle();
        const auto first = fixture.monitor.model(0)->generation();
        for (int i = 0; i < 24; ++i) { fixture.step(100'000'000); fixture.settle(12); }
        CK_CHECK(fixture.monitor.model(0)->generation() > first && fixture.monitor.model(0)->size() > 0);
    }
}
CK_TEST(logmonitor_export_captures_scope_and_preserves_existing_files_and_other_sources) {
    testing::LogMonitorFixture fixture; fixture.monitor.open_sample(SampleMode::Steady); fixture.settle();
    fixture.monitor.open_sample(SampleMode::Steady); fixture.settle(); fixture.monitor.select_source(0);
    fixture.monitor.set_filter("event"); fixture.settle();
    const auto before = fixture.monitor.model(1)->latest_sequence();
    CK_CHECK(fixture.monitor.begin_export("/exports/log.txt", ExportScope::FilteredRetained)); fixture.settle();
    CK_CHECK(fixture.exports.busy && fixture.exports.job.contents.find("filtered retained records") != std::string::npos);
    fixture.step(100'000'000); fixture.settle(); CK_CHECK(fixture.monitor.model(1)->latest_sequence() > before);
    fixture.exports.complete(); fixture.settle(); CK_CHECK(fixture.fs.read_file("/exports/log.txt").has_value());
    const auto saved = fixture.fs.read_file("/exports/log.txt")->contents;
    CK_CHECK(fixture.monitor.begin_export("/exports/log.txt", ExportScope::FilteredRetained)); fixture.settle(); fixture.exports.complete(); fixture.settle();
    CK_CHECK(fixture.fs.read_file("/exports/log.txt")->contents == saved && fixture.monitor.notice().find("exists") != std::string_view::npos);
}
CK_TEST(logmonitor_selected_record_export_and_detail_use_retained_identity_and_context) {
    testing::LogMonitorFixture fixture; fixture.monitor.open_sample(SampleMode::Steady); fixture.settle();
    for (int i = 0; i < 5; ++i) fixture.step(100'000'000);
    fixture.settle();
    const auto record = fixture.monitor.panel(0)->provider().at(1);
    CK_CHECK(record.has_value()); const std::string expected_text(record->text);
    fixture.monitor.panel(0)->view().set_selection({{record->id, 0}, {record->id, 4}});
    CK_CHECK(fixture.monitor.detail_text().find("Retained context") != std::string::npos);
    CK_CHECK(fixture.app.dispatch(KeyEvent{{Key::Char, Modifier::Ctrl, "c"}}));
    CK_CHECK(!fixture.app.clipboard_text().empty());
    CK_CHECK(fixture.monitor.begin_export("/exports/selection.txt", ExportScope::Selection)); fixture.settle();
    CK_CHECK(fixture.exports.job.scope.find("1 records") != std::string::npos && fixture.exports.job.contents.find(expected_text) != std::string::npos);
}
CK_TEST(logmonitor_monitors_a_real_growing_file_through_the_same_app_workflow) {
    testing::ScratchDirectory scratch("logmonitor-native"); const auto path = scratch.path() / "application.log";
    { std::ofstream file(path); file << "real first\n"; }
    ManualClock clock; term::HeadlessTerminal terminal({100, 30}); ui::Application app(terminal, clock);
    MemoryFileSystem paths; testing::MemoryExportWriter exports(paths); auto activity = std::make_shared<LogActivity>();
    term::NativeLogFileService files(1); LogMonitorApp monitor(app, files, paths, exports, activity);
    const auto utf8 = path.generic_u8string(); monitor.open_file({reinterpret_cast<const char*>(utf8.data()), utf8.size()});
    // Advance the injected clock by the real time that passed, not by a fixed
    // step per iteration: a 1 ms sleep lasts about 15.6 ms on Windows, so a
    // fixed step reaches only ~0.6 s of follower time in this 10 s deadline,
    // short of the 1 s reconciliation that recovers a coalesced notification.
    auto last = std::chrono::steady_clock::now();
    const auto wait = [&](std::size_t count) {
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        do {
            const auto now = std::chrono::steady_clock::now();
            clock.advance(std::max<std::int64_t>(1'000'000,
                std::chrono::duration_cast<std::chrono::nanoseconds>(now - last).count()));
            last = now; monitor.step(clock.now_nanos());
            if (app.frames_awaiting_terminal()) terminal.inject_bytes("\x1B[0n", clock.now_nanos());
            app.step(clock.now_nanos());
            if (monitor.model(0) && monitor.model(0)->size() >= count) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::steady_clock::now() < end);
        return false;
    };
    CK_CHECK(wait(1)); { std::ofstream file(path, std::ios::app); file << "real appended\n"; }
    CK_CHECK(wait(2) && monitor.model(0)->at(1)->text == "real appended");
    monitor.panel(0)->view().pause(); { std::ofstream file(path, std::ios::app); file << "while inspecting\n"; }
    CK_CHECK(wait(3) && !monitor.panel(0)->view().state().following);
    monitor.panel(0)->view().resume(); CK_CHECK(monitor.panel(0)->view().state().following);
}
CK_TEST(logmonitor_native_export_worker_publishes_create_only_results_and_wakes_the_app) {
    testing::ScratchDirectory scratch("logmonitor-export");
#if defined(_WIN32)
    term::WindowsFileSystem filesystem;
#else
    term::PosixFileSystem filesystem;
#endif
    auto activity = std::make_shared<LogActivity>(); NativeExportWriter writer(filesystem, activity);
    const auto utf8 = (scratch.path() / "snapshot.txt").generic_u8string();
    const std::string path(reinterpret_cast<const char*>(utf8.data()), utf8.size());
    ExportJob job{1, path, "snapshot 中 é\n", "filtered retained snapshot"};
    // submit() never waits: it reports Busy while the worker holds its lock,
    // even for bookkeeping, and leaves the job intact. Retry as the app does.
    const auto submit = [&] {
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        do { if (writer.submit(job)) return true; std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        while (std::chrono::steady_clock::now() < end);
        return false;
    };
    CK_CHECK(writer.has_wake() && submit());
    ExportResult result;
    const auto wait = [&] {
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        do { if (writer.take(result)) return true; std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        while (std::chrono::steady_clock::now() < end);
        return false;
    };
    CK_CHECK(wait() && result.state == FileWriteStatus::Ok && activity->pending());
    CK_CHECK(filesystem.read_file(path)->contents == "snapshot 中 é\n");
    job = {2, path, "replacement", "snapshot"}; CK_CHECK(submit());
    CK_CHECK(wait() && result.state == FileWriteStatus::Conflict);
    CK_CHECK(filesystem.read_file(path)->contents == "snapshot 中 é\n");
}
CK_TEST(logmonitor_golden_graph_covers_source_picker_live_inspection_overload_errors_and_export) {
    for (const auto name : testing::logmonitor_golden_names) {
        std::ifstream input(std::string(CKV_TEST_SOURCE_DIR) + "/golden/logmonitor_" + std::string(name) + ".dump", std::ios::binary);
        CK_CHECK(input.good()); const std::string expected{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        CK_CHECK(testing::logmonitor_golden(name) == expected);
    }
}
