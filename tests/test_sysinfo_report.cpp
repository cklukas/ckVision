// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The report the example exports, and the help it answers F1 with.
//
// The report is composed from the same functions the panes are, and this
// suite is where that is held to: every figure it contains is one the
// panes show, and it is written through the injected filesystem, so a test
// reads it back out of memory rather than off anybody's disk.
#include <string>
#include <vector>

#include "cvision/core/filesystem.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/widgets/canvas.hpp"
#include "cvision/widgets/help_viewer.hpp"
#include "cvision/widgets/option_group.hpp"
#include "cvision/widgets/table.hpp"
#include "cvision/widgets/text_view.hpp"

#include "fixed_benchmark_runner.hpp"
#include "fixed_system_probe.hpp"
#include "report_document.hpp"
#include "report_format.hpp"
#include "sysinfo_app.hpp"

using ckv::ManualClock;
using ckv::MemoryFileSystem;
using ckv::sysinfo::compose_report;
using ckv::sysinfo::FixedBenchmarkRunner;
using ckv::sysinfo::FixedSystemProbe;
using ckv::sysinfo::ReportFormat;
using ckv::sysinfo::SysInfoApp;
using ckv::ui::Application;

namespace {

struct Fixture {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    FixedSystemProbe probe;
    FixedBenchmarkRunner runner;
    MemoryFileSystem files;
    SysInfoApp sysinfo{app, probe, runner, files, "/reports"};

    Fixture() { files.add_directory("/reports"); }

    bool run_command(std::string_view key) {
        const std::optional<ckv::ui::CommandId> id = app.commands().id_for(std::string(key));
        return id.has_value() && app.execute_command(*id);
    }

    void run_benchmarks() {
        run_command(SysInfoApp::kRunBenchmarksKey);
        sysinfo.benchmarks().wait_until_idle();
        for (int turn = 0; turn < 8; ++turn) app.step(0);
    }
};

// A filesystem that refuses. MemoryFileSystem accepts a write to any path,
// which is right for a scripted tree and useless for testing the answer to
// a write that failed -- a full disk, a read-only volume, a directory the
// reader cannot write to.
class RefusingFileSystem final : public ckv::FileSystem {
public:
    std::vector<ckv::FileEntry> list_directory(std::string_view) const override { return {}; }
    bool exists(std::string_view) const noexcept override { return false; }
    bool is_directory(std::string_view) const noexcept override { return false; }
    ckv::FileWriteResult write_file_atomic(std::string_view, std::string_view,
                                           ckv::FileWriteExpectation) override {
        return ckv::FileWriteResult{ckv::FileWriteStatus::Error, std::nullopt};
    }
};

bool contains(const std::string& haystack, std::string_view needle) {
    return haystack.find(needle) != std::string::npos;
}

}  // namespace

CK_TEST(the_report_contains_every_pane_and_the_values_those_panes_show) {
    FixedSystemProbe probe;
    const std::string report = compose_report(probe, {}, "capability report goes here\n", ReportFormat::Text);

    for (const std::string_view section : {"System", "Memory", "Volumes", "Terminal", "Measurements"})
        CK_CHECK(contains(report, section));

    // The same figures, spelled the same way as on screen.
    CK_CHECK(contains(report, "sysinfo-demo"));
    CK_CHECK(contains(report, "ckVision Demo CPU"));
    CK_CHECK(contains(report, "4d 03:22:10"));
    CK_CHECK(contains(report, "16.0 GiB"));
    CK_CHECK(contains(report, "/Volumes/Archive (ro)"));
    CK_CHECK(contains(report, "capability report goes here"));
    // Including the absences, which are part of what the panes say.
    CK_CHECK(contains(report, std::string(ckv::sysinfo::kNotReported)));
}

CK_TEST(a_report_of_nothing_measured_says_so_rather_than_showing_an_empty_table) {
    FixedSystemProbe probe;
    const std::string report = compose_report(probe, {}, "", ReportFormat::Text);
    CK_CHECK(contains(report, "Nothing was measured"));
}

CK_TEST(a_measured_report_carries_the_scale_and_the_provenance_of_every_comparison) {
    FixedSystemProbe probe;
    FixedBenchmarkRunner runner;
    const std::string report = compose_report(probe, runner.results, "", ReportFormat::Text);

    CK_CHECK(contains(report, "Memory bandwidth"));
    CK_CHECK(contains(report, "20.00 GB/s"));
    // A reference row without its arithmetic and its source is a number
    // nobody can check, and the file is read where the legend is not.
    CK_CHECK(contains(report, "DDR4-3200"));
    CK_CHECK(contains(report, "3200 MT/s x 8 B/transfer"));
    CK_CHECK(contains(report, "JEDEC DDR4 (JESD79-4)"));
    CK_CHECK(contains(report, "This program did not measure them."));
    // And the curves, point by point, with the perfect result beside them.
    CK_CHECK(contains(report, "64.0 MiB"));
    CK_CHECK(contains(report, "8 threads"));
    CK_CHECK(contains(report, "8.00x"));
}

CK_TEST(the_markdown_report_is_markdown_and_the_text_report_is_not) {
    FixedSystemProbe probe;
    const std::string markdown = compose_report(probe, {}, "", ReportFormat::Markdown);
    const std::string text = compose_report(probe, {}, "", ReportFormat::Text);

    CK_CHECK(contains(markdown, "# ckVision SysInfo report"));
    CK_CHECK(contains(markdown, "| Field | Value |"));
    // Every table in the Markdown report is a table a renderer will
    // recognize, header rule and all -- including the small per-benchmark
    // ones, which had rows and no header until a rendered report showed it.
    const std::string measured = compose_report(probe, FixedBenchmarkRunner{}.results, "", ReportFormat::Markdown);
    CK_CHECK(contains(measured, "| Figure | Value |"));
    CK_CHECK(contains(measured, "| Reference | Index | Arithmetic | Source |"));
    CK_CHECK(contains(markdown, "|---|"));
    CK_CHECK(!contains(text, "|---|"));
    CK_CHECK(contains(text, "ckVision SysInfo report\n======"));

    CK_CHECK(ckv::sysinfo::default_report_name(ReportFormat::Markdown) == "sysinfo-report.md");
    CK_CHECK(ckv::sysinfo::default_report_name(ReportFormat::Text) == "sysinfo-report.txt");
}

// A Debug build's report says so as plainly as its window does: the file
// outlives the session that produced it, and is read by someone who was
// not watching the screen.
CK_TEST(the_report_carries_the_same_caveat_the_window_carries) {
    FixedSystemProbe probe;
    probe.build_report.build_type = "Debug";
    const std::string report = compose_report(probe, {}, "", ReportFormat::Text);
    CK_CHECK(contains(report, "Debug build"));
    CK_CHECK(contains(report, "greatly reduced"));
    CK_CHECK(contains(report, "Build"));
}

CK_TEST(saving_a_report_writes_the_shown_text_through_the_injected_filesystem) {
    Fixture f;
    f.run_benchmarks();

    CK_CHECK(f.sysinfo.save_report("/reports/machine.md", ReportFormat::Markdown));
    CK_CHECK(f.sysinfo.last_saved_report() == "/reports/machine.md");

    const std::optional<ckv::FileReadResult> written = f.files.read_file("/reports/machine.md");
    CK_CHECK(written.has_value());
    // Byte for byte what the application would show, not a second rendition
    // of the same numbers.
    CK_CHECK(written->contents == f.sysinfo.report_text(ReportFormat::Markdown));
    CK_CHECK(contains(written->contents, "20.00 GB/s"));
}

CK_TEST(a_report_that_cannot_be_written_is_reported_as_not_written) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    FixedSystemProbe probe;
    FixedBenchmarkRunner runner;
    RefusingFileSystem files;
    SysInfoApp sysinfo{app, probe, runner, files, "/"};

    CK_CHECK(!sysinfo.save_report("/nowhere/machine.txt", ReportFormat::Text));
    // And nothing is remembered as saved, because nothing was.
    CK_CHECK(sysinfo.last_saved_report().empty());
}

// F1 is answered by the pane the reader is looking at, so every pane must
// carry a key and every key must resolve. A topic that silently became
// "Not Found" is exactly the failure this catches.
CK_TEST(every_pane_and_every_benchmark_answers_f1_with_its_own_topic) {
    Fixture f;
    for (const std::string_view command : {SysInfoApp::kMemoryWindowKey, SysInfoApp::kVolumesWindowKey,
                                           SysInfoApp::kTerminalWindowKey, SysInfoApp::kBenchmarksWindowKey,
                                           SysInfoApp::kLatencyPlotKey})
        CK_CHECK(f.run_command(command));
    f.app.step(0);

    const ckv::widgets::HelpProvider& help = f.sysinfo.help();
    const std::vector<const ckv::ui::View*> panes{
        static_cast<const ckv::ui::View*>(f.sysinfo.system_table()),
        static_cast<const ckv::ui::View*>(f.sysinfo.memory_table()),
        static_cast<const ckv::ui::View*>(f.sysinfo.volumes_table()),
        static_cast<const ckv::ui::View*>(f.sysinfo.terminal_report()),
        static_cast<const ckv::ui::View*>(f.sysinfo.benchmark_picker()),
        static_cast<const ckv::ui::View*>(f.sysinfo.chart()),
        static_cast<const ckv::ui::View*>(f.sysinfo.latency_canvas())};
    for (const ckv::ui::View* const pane : panes) {
        CK_CHECK(pane != nullptr);
        CK_CHECK(pane->help_context_key().has_value());
        const ckv::widgets::HelpTopic topic = help.topic(*pane->help_context_key());
        CK_CHECK(!topic.title.empty());
        CK_CHECK(topic.title.find("Not Found") == std::string::npos);
        CK_CHECK(topic.body.size() > 40);
    }

    // And one topic per benchmark, named by the catalogue rather than by a
    // list written twice.
    for (const ckv::sysinfo::BenchmarkDescriptor& descriptor : ckv::sysinfo::benchmark_catalogue()) {
        const ckv::widgets::HelpTopic topic = help.topic("sysinfo." + std::string(descriptor.key));
        CK_CHECK(topic.title == std::string(descriptor.title));
        CK_CHECK(topic.title.find("Not Found") == std::string::npos);
    }
}
