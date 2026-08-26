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

    // The disk kernel's question is answered up front, so every test here
    // measures rather than waiting on a dialog. The one test about that
    // question clears it again.
    Fixture() { sysinfo.set_scratch_directory("/scratch"); }

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
    // delivered on this thread the way step() delivers them. The scratch
    // directory is answered up front; the test below covers what happens
    // when it has not been.
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

CK_TEST(sysinfo_about_is_separate_from_context_help_and_carries_the_project_copyright) {
    Fixture f;
    CK_CHECK(f.run_command("sysinfo.about"));
    f.app.step(0);
    CK_CHECK(f.app.is_modal());
    CK_CHECK(f.term.written_bytes().find(
                 "Copyright (c) 2026 C. Klukas. All rights reserved.") != std::string::npos);
}

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

// A machine has a dozen mount points that are the operating system's own
// bookkeeping. A reader asking what disks they have means none of them --
// but hiding something silently is its own defect, so the count is on the
// window and the whole list is one keystroke away.
CK_TEST(the_disk_pane_shows_disks_and_says_how_many_it_is_not_showing) {
    Fixture f;
    CK_CHECK(f.run_command(SysInfoApp::kVolumesWindowKey));
    f.app.step(0);

    CK_CHECK(!f.sysinfo.showing_all_mounts());
    CK_CHECK(f.sysinfo.hidden_mount_count() == 1);
    CK_CHECK(f.sysinfo.volumes_table()->row_count() == 2);
    CK_CHECK(f.sysinfo.volumes_window()->footer() == "1 system mount points hidden");

    const std::string hidden(f.term.written_bytes());
    CK_CHECK(hidden.find("/System/Volumes/") == std::string::npos);

    CK_CHECK(f.run_command(SysInfoApp::kShowAllMountsKey));
    f.app.step(0);
    CK_CHECK(f.sysinfo.showing_all_mounts());
    CK_CHECK(f.sysinfo.hidden_mount_count() == 0);
    CK_CHECK(f.sysinfo.volumes_table()->row_count() == 3);
    CK_CHECK(f.sysinfo.volumes_window()->footer().empty());
    // What reaches the screen is the mount column's 22 cells of it, so the
    // positive check is for what is actually drawn rather than for a path
    // the column has no room to finish.
    const std::string shown(f.term.written_bytes());
    CK_CHECK(shown.find("/System/Volumes/") != std::string::npos);

    // And the checkbox in the window says the same thing the command did.
    CK_CHECK(f.sysinfo.volumes_window() != nullptr);
    CK_CHECK(f.run_command(SysInfoApp::kShowAllMountsKey));
    CK_CHECK(!f.sysinfo.showing_all_mounts());
    CK_CHECK(f.sysinfo.hidden_mount_count() == 1);
}

CK_TEST(a_refresh_does_not_bring_hidden_mount_points_back) {
    Fixture f;
    CK_CHECK(f.run_command(SysInfoApp::kVolumesWindowKey));
    f.app.step(0);
    f.tick();
    CK_CHECK(f.sysinfo.volumes_table()->row_count() == 2);
    CK_CHECK(f.sysinfo.hidden_mount_count() == 1);
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

CK_TEST(the_benchmarks_page_is_one_surface_and_the_reading_windows_keep_their_own) {
    // The one page in this application built out of dialog furniture. A
    // Button paints `ckv.button.shadow`'s background into the cells around
    // its face, and that role is the DIALOG surface — so on a window wearing
    // the desktop's own palette every button sat in a rectangle of the wrong
    // colour with its shadow falling on nothing.
    //
    // Asserted as cells agreeing with each other rather than against a
    // resolved role: what was wrong was that two colours met on one window,
    // and a test that named either colour would be asserting about the theme
    // instead of about the defect.
    Fixture f;
    CK_CHECK(f.run_command(SysInfoApp::kSystemWindowKey));
    CK_CHECK(f.run_command(SysInfoApp::kBenchmarksWindowKey));
    f.app.step(0);
    const ckv::widgets::Window* const benchmarks = f.sysinfo.benchmarks_window();
    const ckv::widgets::Window* const system = f.sysinfo.system_window();
    CK_CHECK(benchmarks != nullptr);
    CK_CHECK(system != nullptr);

    const ckv::Rect b = benchmarks->bounds();
    const auto bg_at = [&](int x, int y) {
        return f.term.display().frame().at(ckv::Point{x, y}).style().bg;
    };
    // The button row is found by its text, not by counting back from the
    // frame: this test is not about how many rows the page has.
    int buttons_row = -1;
    for (int y = b.y; y < b.y + b.height; ++y) {
        std::string row;
        for (int x = b.x; x < b.x + b.width; ++x)
            row += f.term.display().frame().at(ckv::Point{x, y}).grapheme();
        if (row.find("Next") != std::string::npos) buttons_row = y;
    }
    CK_CHECK(buttons_row > 0);

    // One surface, from the frame through the interior to the cells beside
    // and beneath the buttons — the last two are where the shadow's grey
    // used to meet the window's blue.
    const ckv::Color surface = bg_at(b.x, b.y);
    CK_CHECK(bg_at(b.x + 1, b.y + 1) == surface);
    CK_CHECK(bg_at(b.x + 1, buttons_row) == surface);
    CK_CHECK(bg_at(b.x + 1, buttons_row + 1) == surface);
    CK_CHECK(bg_at(b.x + b.width - 2, buttons_row) == surface);

    // A button is still a button: its face stands out from the surface it is
    // drawn on. Without this the whole test would pass on a page that had
    // gone one flat colour.
    ckv::Color face = surface;
    for (int x = b.x + 1; x < b.x + b.width - 1; ++x)
        if (!(bg_at(x, buttons_row) == surface)) face = bg_at(x, buttons_row);
    CK_CHECK(!(face == surface));

    // And the change is scoped: a window that only reads numbers keeps the
    // desktop's palette, so this is a page dressed as a dialog rather than an
    // application that has gone grey.
    const ckv::Rect s = system->bounds();
    // Read off its own top frame, and checked to BE its own top frame: a
    // corner glyph is how this test knows the benchmarks window is not lying
    // over the cell it is about to draw a conclusion from.
    const std::string corner = std::string(f.term.display().frame().at(ckv::Point{s.x, s.y}).grapheme());
    CK_CHECK(corner == "\u2554" || corner == "\u250c");
    CK_CHECK(!(bg_at(s.x, s.y) == surface));

    // Dressed as a dialog, still a window: the reader can resize it, and the
    // frame keeps the controls a dialog does not have.
    CK_CHECK(benchmarks->resizable());
    CK_CHECK(benchmarks->minimizable());
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
    CK_CHECK(f.sysinfo.all_chart_bars().size() == expected_bars);
    CK_CHECK(f.sysinfo.all_chart_bars().front().highlighted);
    // One topic per page, each naming its own scale under its own axis --
    // a single caption could only ever name one of them.
    CK_CHECK(f.sysinfo.benchmark_page_count() == f.sysinfo.current_results().size() + 1);
    bool saw_memory_scale = false;
    for (std::size_t page = 0; page < f.sysinfo.benchmark_page_count(); ++page) {
        CK_CHECK(f.run_command(SysInfoApp::kNextPageKey));
        if (f.sysinfo.benchmark_page_title() != "Memory bandwidth") continue;
        saw_memory_scale = true;
        CK_CHECK(f.sysinfo.chart()->caption().find("1.0 = 1.00 GB/s") != std::string::npos);
        // And the page holds that topic's bars and nobody else's.
        for (const ckv::sysinfo::ChartBar& bar : f.sysinfo.chart()->bars())
            CK_CHECK(bar.group == "Memory bandwidth");
    }
    CK_CHECK(saw_memory_scale);
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
    CK_CHECK(!f.sysinfo.all_chart_bars().empty());
    for (const ckv::sysinfo::ChartBar& bar : f.sysinfo.all_chart_bars()) {
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
    for (const ckv::sysinfo::ChartBar& bar : f.sysinfo.all_chart_bars())
        CK_CHECK(bar.label.find("*") == std::string::npos);
    CK_CHECK(f.sysinfo.benchmark_footnote()->text().find("indicative") != std::string::npos);
}

CK_TEST(the_run_before_this_one_stays_on_the_chart_to_be_compared_against) {
    Fixture f;
    f.run_benchmarks();
    const std::size_t kernels = f.sysinfo.current_results().size();
    f.run_benchmarks();

    CK_CHECK(f.sysinfo.previous_results().size() == kernels);
    // Across every page, not just the one on screen.
    const std::vector<ckv::sysinfo::ChartBar>& bars = f.sysinfo.all_chart_bars();

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
    for (const ckv::sysinfo::ChartBar& bar : f.sysinfo.all_chart_bars())
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

    const std::vector<ckv::sysinfo::ChartBar>& bars = f.sysinfo.all_chart_bars();
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
    for (const ckv::sysinfo::ChartBar& bar : f.sysinfo.all_chart_bars())
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
    for (const ckv::sysinfo::ChartBar& bar : f.sysinfo.all_chart_bars()) {
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

// The one kernel that writes to the reader's machine does not start until
// they have said where. A run that quietly picked a directory would be a
// program writing to somebody's disk because they pressed "measure".
// Two bugs this test exists for, both found by running the program.
//
// First: a DialogPresentation IS its handler's lifetime -- dropping the
// handle withdraws the handler -- so a completion installed on a local that
// dies at the end of the function never runs. The question appeared, the
// reader answered it, and nothing happened.
//
// Second: the question was asked with a directory PICKER, whose own header
// says its tree is materialized eagerly and in full and is not for a real
// filesystem root. Pointed at a home directory it walked every file the
// reader owns, and the application stopped answering.
//
// So this drives the whole chain: ask, answer, and measure.
CK_TEST(answering_the_disk_question_actually_starts_the_run) {
    Fixture f;
    f.sysinfo.set_scratch_directory("");
    f.files.add_directory("/");
    CK_CHECK(f.run_command(SysInfoApp::kBenchmarksWindowKey));
    f.app.step(0);

    CK_CHECK(f.run_command(SysInfoApp::kRunBenchmarksKey));
    f.app.step(0);

    // The question is on screen, it says how much it will write, and
    // nothing has been measured or written yet.
    CK_CHECK(f.sysinfo.pending_directory_open());
    CK_CHECK(f.runner.runs() == 0);
    const std::string asked(f.term.written_bytes());
    CK_CHECK(asked.find("64.0 MiB") != std::string::npos);

    // Enter accepts the dialog's default button, its validator asks the
    // injected filesystem whether that directory exists, and the handler
    // that survives the dialog starts the run.
    f.app.dispatch(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}});
    for (int turn = 0; turn < 4; ++turn) f.app.step(0);
    f.sysinfo.benchmarks().wait_until_idle();
    for (int turn = 0; turn < 8; ++turn) f.app.step(0);

    CK_CHECK(f.sysinfo.scratch_directory() == "/");
    CK_CHECK(f.runner.last_options().scratch_directory == "/");
    CK_CHECK(f.sysinfo.current_results().size() == ckv::sysinfo::benchmark_catalogue().size());
}

CK_TEST(a_disk_measurement_asks_before_it_writes_anywhere) {
    Fixture f;
    f.sysinfo.set_scratch_directory("");
    CK_CHECK(f.run_command(SysInfoApp::kBenchmarksWindowKey));
    f.app.step(0);
    CK_CHECK(f.sysinfo.scratch_directory().empty());

    CK_CHECK(f.run_command(SysInfoApp::kRunBenchmarksKey));
    f.sysinfo.benchmarks().wait_until_idle();
    for (int turn = 0; turn < 8; ++turn) f.app.step(0);

    // Nothing ran, and nothing was measured: the question is on screen.
    CK_CHECK(f.runner.runs() == 0);
    CK_CHECK(f.sysinfo.current_results().empty());
    const std::string bytes(f.term.written_bytes());
    CK_CHECK(bytes.find("64.0 MiB") != std::string::npos);

    // With an answer, the same command runs everything.
    f.sysinfo.set_scratch_directory("/scratch");
    f.run_benchmarks();
    CK_CHECK(f.sysinfo.current_results().size() == ckv::sysinfo::benchmark_catalogue().size());
    CK_CHECK(f.runner.last_options().scratch_directory == "/scratch");
    // And the thread count the kernels were given is the machine's own,
    // as the probe reported it.
    CK_CHECK(f.runner.last_options().maximum_threads == f.probe.processor_report.logical_cores.value());
}

// One topic per page, the way the tools this example follows presented
// them: a page that chooses and runs the measurements, and a page per
// metric measured. A single column holding every chart at once is a list of
// numbers rather than a chart.
CK_TEST(the_benchmark_window_pages_one_topic_at_a_time) {
    Fixture f;
    CK_CHECK(f.run_command(SysInfoApp::kBenchmarksWindowKey));
    f.app.step(0);
    // Before anything is measured there is one page, and it is the one that
    // measures.
    CK_CHECK(f.sysinfo.benchmark_page() == 0);
    CK_CHECK(f.sysinfo.benchmark_page_count() == 1);
    CK_CHECK(f.sysinfo.benchmark_picker()->visible());
    CK_CHECK(!f.sysinfo.chart()->visible());

    f.run_benchmarks();

    // A finished run lands on the first topic rather than leaving the
    // reader on the page they pressed Run from.
    CK_CHECK(f.sysinfo.benchmark_page() == 1);
    CK_CHECK(f.sysinfo.chart()->visible());
    CK_CHECK(!f.sysinfo.benchmark_picker()->visible());
    CK_CHECK(f.sysinfo.benchmark_page_title() == "Integer mix");
    CK_CHECK(f.sysinfo.benchmarks_window()->title() == "Benchmarks - Integer mix");

    // Every page holds exactly one topic's bars.
    const std::size_t pages = f.sysinfo.benchmark_page_count();
    CK_CHECK(pages == ckv::sysinfo::benchmark_catalogue().size() + 1);
    for (std::size_t step = 1; step < pages; ++step) {
        CK_CHECK(f.run_command(SysInfoApp::kNextPageKey));
        const std::string& title = f.sysinfo.benchmark_page_title();
        for (const ckv::sysinfo::ChartBar& bar : f.sysinfo.chart()->bars()) CK_CHECK(bar.group == title);
    }

    // The walk above ended where it started, because Next wraps rather
    // than stopping dead on the last page.
    CK_CHECK(f.sysinfo.benchmark_page() == 0);
    CK_CHECK(f.run_command(SysInfoApp::kPreviousPageKey));
    CK_CHECK(f.sysinfo.benchmark_page() == pages - 1);
}
