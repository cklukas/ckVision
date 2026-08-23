// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The SysInfo example against a scripted machine. Every assertion here is
// about what the application does with what a probe told it, so this suite
// runs and produces the same answers on any host — which is the whole
// argument for the probe boundary it is testing.
#include <optional>
#include <string_view>
#include <string>
#include <vector>

#include "cvision/core/filesystem.hpp"
#include "cvision/core/key.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/option_group.hpp"
#include "cvision/widgets/progress.hpp"
#include "cvision/widgets/canvas.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/text_view.hpp"
#include "cvision/widgets/table.hpp"
#include "cvision/widgets/window.hpp"

#include "fixed_benchmark_runner.hpp"
#include "reference_points.hpp"
#include "fixed_system_probe.hpp"
#include "report_format.hpp"
#include "sysinfo_app.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::sysinfo::FixedBenchmarkRunner;
using ckv::sysinfo::FixedSystemProbe;
using ckv::sysinfo::kNotReported;
using ckv::sysinfo::SysInfoApp;
using ckv::ui::Application;

namespace {

struct Fixture {
    ckv::term::HeadlessTerminal term{ckv::Size{80, 24}};
    ManualClock clock;
    Application app{term, clock};
    FixedSystemProbe probe;
    FixedBenchmarkRunner runner;
    ckv::MemoryFileSystem files;
    SysInfoApp sysinfo{app, probe, runner, files, "/"};

    // By key, the way the menu asks for it: a command's numeric id is the
    // registry's business and a test that knew one would be asserting
    // about the registry instead of about this application.
    bool run_command(std::string_view key) {
        const std::optional<ckv::ui::CommandId> id = app.commands().id_for(std::string(key));
        return id.has_value() && app.execute_command(*id);
    }

    // A second of application time, delivered the way the run loop
    // delivers it.
    void tick() {
        clock.advance(SysInfoApp::kRefreshIntervalNanos);
        app.step(0);
    }

    // A benchmark run, start to finish, with the worker's posted results
    // delivered on this thread the way step() delivers them.
    void run_benchmarks() {
        run_command(SysInfoApp::kRunBenchmarksKey);
        sysinfo.benchmarks().wait_until_idle();
        for (int turn = 0; turn < 8; ++turn) app.step(0);
    }
};

// The value beside a named field, or an empty string if the pane has no
// such row. Rows are found by their name because a row's position is not
// part of what this application promises.
std::string value_of(const std::vector<std::vector<std::string>>& rows, std::string_view field) {
    for (const std::vector<std::string>& row : rows)
        if (row.size() >= 2 && row[0] == field) return row[1];
    return {};
}

}  // namespace

CK_TEST(sysinfo_opens_on_the_summary_of_the_machine_it_was_given) {
    Fixture f;
    f.app.step(0);
    const std::string bytes(f.term.written_bytes());

    CK_CHECK(f.sysinfo.system_window() != nullptr);
    CK_CHECK(bytes.find("System") != std::string::npos);
    CK_CHECK(bytes.find("sysinfo-demo") != std::string::npos);
    CK_CHECK(bytes.find("ckVision Demo CPU") != std::string::npos);
    // Chrome the reference vocabulary promises: a way into the menu and a
    // way out of the program.
    CK_CHECK(bytes.find("Menu") != std::string::npos);
    CK_CHECK(bytes.find("Quit") != std::string::npos || bytes.find("Exit") != std::string::npos);
}

CK_TEST(the_summary_names_every_report_it_read) {
    Fixture f;
    const auto rows = ckv::sysinfo::system_rows(f.probe);

    CK_CHECK(value_of(rows, "Host name") == "sysinfo-demo");
    CK_CHECK(value_of(rows, "Operating system") == "ckVision Demo OS 1.0");
    CK_CHECK(value_of(rows, "Uptime") == "4d 03:22:10");
    CK_CHECK(value_of(rows, "Load (1 min)") == "1.25");
    CK_CHECK(value_of(rows, "Processor") == "ckVision Demo CPU");
    CK_CHECK(value_of(rows, "Logical cores") == "8");
    CK_CHECK(value_of(rows, "Nominal frequency") == "3.2 GHz");
    CK_CHECK(value_of(rows, "L2 cache") == "4.0 MiB");
    CK_CHECK(value_of(rows, "Power source") == "battery");
    CK_CHECK(value_of(rows, "Language") == "C++20");
    CK_CHECK(value_of(rows, "Build") == "Release");
    // The fixture's processor genuinely has no level-3 cache to report.
    CK_CHECK(value_of(rows, "L3 cache") == std::string(kNotReported));
}

// One cleared field per report structure, because the failure this guards
// against is per-structure: a pane that composes one report through
// `value_or(0)` is invisible while every other pane is honest.
CK_TEST(a_field_the_host_did_not_answer_reports_absence_in_every_report) {
    Fixture f;
    f.probe.host_report.os_name = std::nullopt;
    f.probe.processor_report.physical_cores = std::nullopt;
    f.probe.power_report.charge_percent = std::nullopt;
    f.probe.memory_report.total_bytes = std::nullopt;
    f.probe.volume_reports[0].capacity_bytes = std::nullopt;

    const auto rows = ckv::sysinfo::system_rows(f.probe);
    CK_CHECK(value_of(rows, "Operating system") == std::string(kNotReported));
    CK_CHECK(value_of(rows, "Physical cores") == std::string(kNotReported));
    CK_CHECK(value_of(rows, "Battery") == std::string(kNotReported));

    const auto memory = ckv::sysinfo::memory_rows(f.probe.memory_report);
    CK_CHECK(value_of(memory, "Total") == std::string(kNotReported));
    CK_CHECK(value_of(memory, "Available") == "6.0 GiB");

    const auto volumes = ckv::sysinfo::volume_rows(f.probe.volume_reports);
    CK_CHECK(volumes.at(0).at(2) == std::string(kNotReported));  // capacity
    CK_CHECK(volumes.at(0).at(4) == std::string(kNotReported));  // used share
    CK_CHECK(volumes.at(1).at(2) == "2.0 TiB");
}

CK_TEST(the_refresh_timer_re_reads_the_live_reports_and_leaves_the_rest_alone) {
    Fixture f;
    CK_CHECK(f.run_command(SysInfoApp::kMemoryWindowKey));
    f.app.step(0);

    const int host_reads = f.probe.host_reads();
    const int memory_reads = f.probe.memory_reads();
    const int refreshes = f.sysinfo.refresh_count();

    f.tick();

    CK_CHECK(f.sysinfo.refresh_count() == refreshes + 1);
    CK_CHECK(f.probe.host_reads() > host_reads);
    CK_CHECK(f.probe.memory_reads() > memory_reads);
}

CK_TEST(the_refresh_command_does_now_what_the_timer_would_do_in_a_second) {
    Fixture f;
    const int refreshes = f.sysinfo.refresh_count();
    CK_CHECK(f.run_command(SysInfoApp::kRefreshKey));
    CK_CHECK(f.sysinfo.refresh_count() == refreshes + 1);
}

CK_TEST(each_pane_opens_by_command_and_closing_one_leaves_the_others_alone) {
    Fixture f;
    CK_CHECK(f.sysinfo.memory_window() == nullptr);
    CK_CHECK(f.run_command(SysInfoApp::kMemoryWindowKey));
    CK_CHECK(f.run_command(SysInfoApp::kVolumesWindowKey));
    f.app.step(0);
    CK_CHECK(f.sysinfo.memory_window() != nullptr);
    CK_CHECK(f.sysinfo.volumes_window() != nullptr);

    // Asking again activates the pane that is already open rather than
    // stacking a second copy of it on top of itself.
    ckv::widgets::Window* const memory = f.sysinfo.memory_window();
    CK_CHECK(f.run_command(SysInfoApp::kMemoryWindowKey));
    CK_CHECK(f.sysinfo.memory_window() == memory);

    CK_CHECK(memory->close());
    f.app.step(0);
    CK_CHECK(f.sysinfo.memory_window() == nullptr);
    CK_CHECK(f.sysinfo.volumes_window() != nullptr);

    // The refresh that follows a close must not go looking for the pane
    // that is gone.
    f.tick();
    CK_CHECK(f.sysinfo.volumes_window() != nullptr);
}

CK_TEST(the_memory_bar_states_the_same_two_figures_its_table_lists) {
    Fixture f;
    CK_CHECK(f.run_command(SysInfoApp::kMemoryWindowKey));
    f.app.step(0);

    ckv::widgets::Progress* const bar = f.sysinfo.memory_bar();
    CK_CHECK(bar != nullptr);
    CK_CHECK(bar->label() == "10.0 GiB of 16.0 GiB used (63%)");
    CK_CHECK(bar->fraction() == 0.625);

    const auto rows = ckv::sysinfo::memory_rows(f.probe.memory_report);
    CK_CHECK(value_of(rows, "Total") == "16.0 GiB");
    CK_CHECK(value_of(rows, "Available") == "6.0 GiB");
    CK_CHECK(value_of(rows, "Wired") == "3.0 GiB");
    CK_CHECK(value_of(rows, "Swap used") == "512.0 MiB");
}

CK_TEST(the_disk_bar_follows_the_cursor_rather_than_the_first_row) {
    Fixture f;
    CK_CHECK(f.run_command(SysInfoApp::kVolumesWindowKey));
    f.app.step(0);

    ckv::widgets::Table* const table = f.sysinfo.volumes_table();
    CK_CHECK(table != nullptr);
    CK_CHECK(f.sysinfo.volumes_window()->title() == "Disks");

    ckv::widgets::Progress* const bar = f.sysinfo.volumes_bar();
    CK_CHECK(bar != nullptr);
    CK_CHECK(table->cursor_row() == 0);
    CK_CHECK(bar->label() == "/: 128.0 GiB free of 512.0 GiB");
    CK_CHECK(bar->fraction() == 0.75);

    CK_CHECK(table->on_key(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}}));
    CK_CHECK(table->cursor_row() == 1);
    CK_CHECK(bar->label() == "/Volumes/Archive: 96.0 GiB free of 2.0 TiB");

    // And a refresh does not quietly move it back to the top.
    f.tick();
    CK_CHECK(bar->label() == "/Volumes/Archive: 96.0 GiB free of 2.0 TiB");
}

CK_TEST(a_benchmark_run_charts_one_bar_per_kernel_and_names_the_scale) {
    Fixture f;
    CK_CHECK(f.run_command(SysInfoApp::kBenchmarksWindowKey));
    f.app.step(0);
    CK_CHECK(f.sysinfo.benchmarks_window() != nullptr);
    CK_CHECK(f.sysinfo.chart() != nullptr);
    // Nothing measured yet, and the chart says that rather than drawing an
    // empty axis.
    CK_CHECK(f.sysinfo.chart()->bars().empty());

    f.run_benchmarks();

    CK_CHECK(f.sysinfo.current_results().size() == ckv::sysinfo::benchmark_catalogue().size());
    CK_CHECK(!f.sysinfo.last_run_cancelled());
    // One bar per kernel that measured a single rate, and one bar per point
    // for the two that measured a curve.
    std::size_t expected_bars = 0;
    for (const ckv::sysinfo::BenchmarkResult& result : f.sysinfo.current_results()) {
        if (result.series.empty()) {
            ++expected_bars;
            continue;
        }
        for (const ckv::sysinfo::SeriesPoint& point : result.series)
            expected_bars += point.ideal > 0.0 ? 2 : 1;
    }
    CK_CHECK(f.sysinfo.chart()->bars().size() == expected_bars);
    CK_CHECK(f.sysinfo.chart()->bars().front().highlighted);
    // The scale is named under the chart, because an index whose definition
    // is somewhere else is a number nobody can check.
    CK_CHECK(f.sysinfo.chart()->caption().find("MFLOPS") != std::string::npos);
    CK_CHECK(f.sysinfo.chart()->caption().find("1.0") != std::string::npos);
    CK_CHECK(f.sysinfo.benchmark_progress()->label() == "done");
    CK_CHECK(f.sysinfo.benchmark_progress()->fraction() == 1.0);
}

CK_TEST(a_debug_build_marks_every_bar_it_measured_and_answers_the_mark_below_the_chart) {
    Fixture f;
    f.probe.build_report.build_type = "Debug";
    f.run_benchmarks();

    // The mark is on every bar this build measured -- and on none of the
    // bars it did not, because a perfect-scaling bar was not measured by
    // any build.
    CK_CHECK(!f.sysinfo.chart()->bars().empty());
    for (const ckv::sysinfo::ChartBar& bar : f.sysinfo.chart()->bars()) {
        if (bar.kind == ckv::sysinfo::BarKind::Measured)
            CK_CHECK(bar.label.find(" *") != std::string::npos);
        else
            CK_CHECK(bar.label.find(" *") == std::string::npos);
    }
    // ... and the footnote under the chart is what it points at.
    CK_CHECK(f.sysinfo.benchmark_footnote() != nullptr);
    CK_CHECK(f.sysinfo.benchmark_footnote()->text().rfind("* Debug build", 0) == 0);
    CK_CHECK(f.sysinfo.benchmark_footnote()->text().find("greatly reduced") != std::string::npos);

    const std::string bytes(f.term.written_bytes());
    CK_CHECK(bytes.find("Debug build") != std::string::npos);
}

CK_TEST(an_optimized_build_marks_nothing_because_it_has_nothing_to_explain) {
    Fixture f;  // the scripted machine reports a Release build
    f.run_benchmarks();
    for (const ckv::sysinfo::ChartBar& bar : f.sysinfo.chart()->bars())
        CK_CHECK(bar.label.find("*") == std::string::npos);
    CK_CHECK(f.sysinfo.benchmark_footnote()->text().find("indicative") != std::string::npos);
}

CK_TEST(the_run_before_this_one_stays_on_the_chart_to_be_compared_against) {
    Fixture f;
    f.run_benchmarks();
    const std::size_t kernels = f.sysinfo.current_results().size();
    f.run_benchmarks();

    CK_CHECK(f.sysinfo.previous_results().size() == kernels);
    const std::vector<ckv::sysinfo::ChartBar>& bars = f.sysinfo.chart()->bars();

    // Every metric measured as a single rate shows this run and the one
    // before it, in that order, with the newest the row the eye is sent to.
    // A curve has no "previous" row: two stair-steps drawn over each other
    // would be unreadable, and the newest is the one being asked about.
    std::size_t pairs = 0;
    for (std::size_t index = 0; index + 1 < bars.size(); ++index) {
        if (bars[index].label.rfind("This computer", 0) != 0) continue;
        ++pairs;
        CK_CHECK(bars[index].highlighted);
        CK_CHECK(bars[index].group == bars[index + 1].group);
        CK_CHECK(bars[index + 1].label.rfind("Previous run", 0) == 0);
        CK_CHECK(!bars[index + 1].highlighted);
    }
    std::size_t rate_kernels = 0;
    for (const ckv::sysinfo::BenchmarkResult& result : f.sysinfo.current_results())
        if (result.series.empty()) ++rate_kernels;
    CK_CHECK(pairs == rate_kernels);
}

CK_TEST(a_pane_the_reader_did_not_ask_for_is_not_measured) {
    Fixture f;
    CK_CHECK(f.run_command(SysInfoApp::kBenchmarksWindowKey));
    f.app.step(0);
    for (std::size_t index = 0; index < ckv::sysinfo::benchmark_catalogue().size(); ++index)
        f.sysinfo.benchmark_picker()->set_checked(index, index == 1);

    f.run_benchmarks();
    CK_CHECK(f.sysinfo.current_results().size() == 1);
    CK_CHECK(f.sysinfo.current_results().front().id == ckv::sysinfo::benchmark_catalogue()[1].id);
}

CK_TEST(cancelling_a_run_leaves_the_application_running_and_the_chart_honest) {
    Fixture f;
    CK_CHECK(f.run_command(SysInfoApp::kBenchmarksWindowKey));
    f.app.step(0);
    f.runner.hold();
    CK_CHECK(f.run_command(SysInfoApp::kRunBenchmarksKey));
    f.runner.wait_until_entered(1);

    CK_CHECK(f.run_command(SysInfoApp::kCancelBenchmarksKey));
    f.runner.release();
    f.sysinfo.benchmarks().wait_until_idle();
    for (int turn = 0; turn < 8; ++turn) f.app.step(0);

    CK_CHECK(f.sysinfo.last_run_cancelled());
    CK_CHECK(f.sysinfo.current_results().empty());
    CK_CHECK(f.sysinfo.benchmark_progress()->label() == "cancelled");
    // And the rest of the application is untouched by it.
    f.tick();
    CK_CHECK(f.sysinfo.system_window() != nullptr);
    CK_CHECK(f.sysinfo.refresh_count() > 0);
}

CK_TEST(a_comparison_adds_published_bars_to_one_chart_and_marks_them_as_published) {
    Fixture f;
    f.run_benchmarks();
    // "this run only" is the default: nothing on the chart from outside
    // this program except the perfect-scaling line, which is arithmetic on
    // the thread count rather than a figure about anybody's hardware.
    for (const ckv::sysinfo::ChartBar& bar : f.sysinfo.chart()->bars())
        if (bar.kind == ckv::sysinfo::BarKind::Reference)
            CK_CHECK(bar.label.find("perfect") != std::string::npos);

    // Ask for the memory comparison, and only that group grows.
    ckv::widgets::ComboBox* const compare = f.sysinfo.compare_picker();
    CK_CHECK(compare != nullptr);
    std::size_t memory_choice = 0;
    for (std::size_t index = 0; index < compare->items().size(); ++index)
        if (compare->items()[index].find("Memory") != std::string::npos) memory_choice = index;
    CK_CHECK(memory_choice != 0);
    compare->set_selected_index(memory_choice);
    if (compare->on_select) compare->on_select(memory_choice);

    const std::vector<ckv::sysinfo::ChartBar>& bars = f.sysinfo.chart()->bars();
    std::size_t references = 0;
    for (const ckv::sysinfo::ChartBar& bar : bars) {
        if (bar.kind != ckv::sysinfo::BarKind::Reference) continue;
        if (bar.label.find("perfect") != std::string::npos) continue;  // the scaling ideal
        ++references;
        // A published bar sits in the group of the metric it is published
        // for, and nowhere else.
        CK_CHECK(bar.group.find("Memory") != std::string::npos);
    }
    CK_CHECK(references == ckv::sysinfo::reference_points_for(ckv::sysinfo::BenchmarkId::MemoryBandwidth).size());
    // And the legend appears only once there are two kinds of ink to tell
    // apart.
    CK_CHECK(f.sysinfo.chart()->legend().find("published ceiling") != std::string::npos);
}

CK_TEST(a_comparison_can_be_asked_for_before_anything_has_been_measured) {
    Fixture f;
    CK_CHECK(f.run_command(SysInfoApp::kBenchmarksWindowKey));
    f.app.step(0);
    ckv::widgets::ComboBox* const compare = f.sysinfo.compare_picker();
    compare->set_selected_index(1);
    if (compare->on_select) compare->on_select(1);
    f.app.step(0);

    // The comparison stands on its own: the reader can see what the
    // published figures are before deciding to spend a second measuring.
    CK_CHECK(!f.sysinfo.chart()->bars().empty());
    for (const ckv::sysinfo::ChartBar& bar : f.sysinfo.chart()->bars())
        CK_CHECK(bar.kind == ckv::sysinfo::BarKind::Reference);
    CK_CHECK(f.sysinfo.chart()->legend().find("published ceiling") != std::string::npos);
}

CK_TEST(the_terminal_pane_reports_what_this_terminal_can_do) {
    Fixture f;
    CK_CHECK(f.run_command(SysInfoApp::kTerminalWindowKey));
    f.app.step(0);

    CK_CHECK(f.sysinfo.terminal_window() != nullptr);
    const std::string text = f.sysinfo.terminal_report()->text();
    CK_CHECK(text.find("Cell size:") != std::string::npos);
    CK_CHECK(text.find("Pictures:") != std::string::npos);
    // The headless terminal shows no pictures, and the pane says so rather
    // than leaving the reader to infer it from a capability list.
    CK_CHECK(!f.app.terminal_shows_graphics());
    CK_CHECK(text.find("cell fallback") != std::string::npos);
}

CK_TEST(the_latency_plot_falls_back_to_the_same_data_in_cells) {
    Fixture f;
    CK_CHECK(f.run_command(SysInfoApp::kLatencyPlotKey));
    f.app.step(0);
    CK_CHECK(f.sysinfo.latency_plot_window() != nullptr);
    // One Canvas, whatever the terminal can do: the fallback is the
    // widget's, not a second widget chosen at construction.
    CK_CHECK(f.sysinfo.latency_canvas() != nullptr);

    f.run_benchmarks();
    const std::vector<ckv::sysinfo::ChartBar> bars = f.sysinfo.latency_bars_for_test();
    CK_CHECK(bars.size() == 8);  // the scripted stair-step
    CK_CHECK(bars.front().label == "4.0 KiB");
    CK_CHECK(bars.back().label == "64.0 MiB");
    CK_CHECK(bars.back().value > bars.front().value);

    // And what the fallback would draw is the same series, row for row.
    const std::vector<std::string> rows = ckv::sysinfo::chart_rows(bars, 40);
    CK_CHECK(rows.size() == bars.size() + 1);  // one heading
    CK_CHECK(rows[1].find("4.0 KiB") != std::string::npos);
    CK_CHECK(rows.back().find("96.0 ns") != std::string::npos);
}

// The same window, on a terminal that decodes pictures. The Canvas is
// given a real pixel size from the terminal's cell metric and paints the
// series into it -- and this is the assertion that the graphics path is
// exercised at all, since every other test here runs on a terminal that
// shows none.
CK_TEST(where_the_terminal_shows_pictures_the_plot_is_drawn_in_pixels) {
    ckv::term::HeadlessTerminal term(ckv::Size{80, 24}, ckv::term::headless_sixel_profile());
    ManualClock clock;
    Application app(term, clock);
    FixedSystemProbe probe;
    FixedBenchmarkRunner runner;
    ckv::MemoryFileSystem files;
    SysInfoApp sysinfo{app, probe, runner, files, "/"};

    CK_CHECK(app.terminal_shows_graphics());
    const std::optional<ckv::ui::CommandId> plot = app.commands().id_for(std::string(SysInfoApp::kLatencyPlotKey));
    CK_CHECK(plot.has_value() && app.execute_command(*plot));
    app.step(0);

    ckv::widgets::Canvas* const canvas = sysinfo.latency_canvas();
    CK_CHECK(canvas != nullptr);
    // Sized from the terminal's own cell metric rather than from an
    // assumed one, which is what keeps the picture in proportion.
    CK_CHECK(canvas->cell_metrics() == term.capabilities().cell_pixels);
    CK_CHECK(canvas->pixel_size().width > 0);
    CK_CHECK(canvas->pixel_size().height > 0);

    const std::optional<ckv::ui::CommandId> run = app.commands().id_for(std::string(SysInfoApp::kRunBenchmarksKey));
    CK_CHECK(run.has_value() && app.execute_command(*run));
    sysinfo.benchmarks().wait_until_idle();
    for (int turn = 0; turn < 8; ++turn) app.step(0);

    // The picture reached the terminal: a Sixel introducer in the bytes.
    const std::string bytes(term.written_bytes());
    CK_CHECK(bytes.find("\x1bP") != std::string::npos);
}

CK_TEST(a_curve_is_charted_as_a_curve_and_never_as_an_index) {
    Fixture f;
    f.run_benchmarks();

    // The scripted run measures all five kernels; the two that answer with
    // a curve contribute one bar per point, and no index bar at all.
    bool saw_latency_group = false;
    bool saw_scaling_ideal = false;
    for (const ckv::sysinfo::ChartBar& bar : f.sysinfo.chart()->bars()) {
        if (bar.group.find("Cache latency") != std::string::npos) {
            saw_latency_group = true;
            CK_CHECK(bar.label.find("This computer") == std::string::npos);
        }
        if (bar.group.find("Thread scaling") != std::string::npos &&
            bar.kind == ckv::sysinfo::BarKind::Reference) {
            saw_scaling_ideal = true;
            CK_CHECK(bar.label.find("perfect") != std::string::npos);
        }
    }
    CK_CHECK(saw_latency_group);
    CK_CHECK(saw_scaling_ideal);
    CK_CHECK(f.sysinfo.chart()->legend().find("perfect") != std::string::npos);
}
