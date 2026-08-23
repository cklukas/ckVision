// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "sysinfo_app.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cvision/ui/layout.hpp"
#include "cvision/widgets/application_shell.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/option_group.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/message_box.hpp"
#include "cvision/widgets/progress.hpp"
#include "cvision/widgets/status_line.hpp"
#include "cvision/widgets/table.hpp"
#include "cvision/widgets/window.hpp"

#include "report_format.hpp"

namespace ckv::sysinfo {

SysInfoApp::SysInfoApp(ui::Application& app, const SystemProbe& probe, const BenchmarkRunner& runner)
    : app_(app), probe_(probe), roles_(ui::intern_standard_roles(app.roles())), benchmarks_(app, runner) {
    build_chrome();

    open_system_window();
    refresh();

    // The live figures age visibly on this kind of program's screen — an
    // uptime that never advances is the first thing a reader notices and
    // the last thing they trust. One second is what the eye reads as
    // "current" without the pane becoming something that flickers.
    app_.start_timer(kRefreshIntervalNanos, true, [this] { refresh(); });

    widgets::install_about_help(app_, *desktop_, roles_, "ckVision SysInfo example",
                                "What this machine is, and how fast it is, read through one injected probe.");
}

void SysInfoApp::build_chrome() {
    const ui::CommandId system_command = app_.commands().declare(
        {.key = std::string(kSystemWindowKey), .title = "&Summary", .category = "System",
         .handler = [this] { open_system_window(); }});
    const ui::CommandId memory_command = app_.commands().declare(
        {.key = std::string(kMemoryWindowKey), .title = "&Memory", .category = "System",
         .handler = [this] { open_memory_window(); }});
    const ui::CommandId volumes_command = app_.commands().declare(
        {.key = std::string(kVolumesWindowKey), .title = "&Disks", .category = "System",
         .handler = [this] { open_volumes_window(); }});
    const ui::CommandId refresh_command = app_.commands().declare(
        {.key = std::string(kRefreshKey), .title = "&Refresh", .category = "System", .chord = "F2",
         .handler = [this] { refresh(); }});

    widgets::MenuBarItem system_menu{
        "&System",
        {
            widgets::MenuItem::command(widgets::CommandPresentation{system_command}),
            widgets::MenuItem::command(widgets::CommandPresentation{memory_command}),
            widgets::MenuItem::command(widgets::CommandPresentation{volumes_command}),
            widgets::MenuItem::separator(),
            widgets::MenuItem::command(widgets::CommandPresentation{refresh_command}),
            widgets::MenuItem::separator(),
            widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().quit, "E&xit"}),
        }};
    const ui::CommandId benchmarks_command = app_.commands().declare(
        {.key = std::string(kBenchmarksWindowKey), .title = "&Benchmarks", .category = "Benchmarks",
         .handler = [this] { open_benchmarks_window(); }});
    const ui::CommandId run_command = app_.commands().declare(
        {.key = std::string(kRunBenchmarksKey), .title = "&Run selected", .category = "Benchmarks", .chord = "F9",
         .handler = [this] { start_benchmarks(); }});
    const ui::CommandId cancel_command = app_.commands().declare(
        {.key = std::string(kCancelBenchmarksKey), .title = "&Cancel run", .category = "Benchmarks",
         .handler = [this] { cancel_benchmarks(); }});

    widgets::MenuBarItem benchmarks_menu{
        "&Benchmarks",
        {
            widgets::MenuItem::command(widgets::CommandPresentation{benchmarks_command}),
            widgets::MenuItem::separator(),
            widgets::MenuItem::command(widgets::CommandPresentation{run_command}),
            widgets::MenuItem::command(widgets::CommandPresentation{cancel_command}),
        }};
    widgets::MenuBarItem window_menu{
        "&Window",
        {
            widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().next_window}),
            widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().previous_window}),
            widgets::MenuItem::separator(),
            widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().tile}),
            widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().cascade}),
        }};
    widgets::MenuBarItem help_menu{
        "&Help",
        {widgets::MenuItem::command(widgets::CommandPresentation{app_.commands().standard().help, "&About..."})}};

    widgets::ApplicationShell shell(
        app_, {.theme = ui::make_classic_theme(app_.roles(), roles_),
               .menus = {std::move(system_menu), std::move(benchmarks_menu), std::move(window_menu),
                         std::move(help_menu)},
               .status_items = {
                   widgets::StatusLineItem{widgets::CommandPresentation{app_.commands().standard().menu}},
                   widgets::StatusLineItem{widgets::CommandPresentation{refresh_command}},
                   widgets::StatusLineItem{widgets::CommandPresentation{run_command}},
                   widgets::StatusLineItem{widgets::CommandPresentation{app_.commands().standard().next_window}},
                   widgets::StatusLineItem{widgets::CommandPresentation{app_.commands().standard().quit}}}});
    desktop_ = &shell.desktop();

    app_.commands().set_handler(app_.commands().standard().quit, [this] { app_.request_quit(); });
}

void SysInfoApp::open_system_window() {
    if (system_window_ != nullptr) {
        desktop_->activate(system_window_);
        return;
    }
    auto window = std::make_unique<widgets::Window>("System");
    window->set_bounds(Rect{1, 1, 62, 18});
    window->set_min_size(Size{34, 8});
    window->set_grow_policy(widgets::DesktopGrowPolicy::AnchorEdges);

    auto table = std::make_unique<widgets::Table>();
    table->set_columns({widgets::TableColumn{"Field", 22, 10}, widgets::TableColumn{"Value", 34, 12}});
    table->on_selection_changed = [this](widgets::TableCellRef reference) { system_cursor_ = reference.row; };
    system_table_ = table.get();
    window->set_content(std::move(table));

    widgets::Window* const opened = window.get();
    window->on_closed = [this, opened] {
        system_window_ = nullptr;
        system_cursor_ = widgets::kInvalidTableRowId;
        system_table_ = nullptr;
        widgets::schedule_self_detach(*opened, app_);
    };
    system_window_ = desktop_->add_window(std::move(window));
    fill_system_table();
    app_.set_focus(system_table_);
}

void SysInfoApp::open_memory_window() {
    if (memory_window_ != nullptr) {
        desktop_->activate(memory_window_);
        return;
    }
    auto window = std::make_unique<widgets::Window>("Memory");
    window->set_bounds(Rect{6, 4, 56, 14});
    window->set_min_size(Size{30, 7});
    window->set_grow_policy(widgets::DesktopGrowPolicy::AnchorEdges);

    auto column = std::make_unique<ui::Column>();
    column->set_spacing(1);
    auto bar = std::make_unique<widgets::Progress>();
    memory_bar_ = bar.get();
    column->add_item(std::move(bar), ui::LayoutSpec{ui::SizePolicy::Fixed});
    auto table = std::make_unique<widgets::Table>();
    table->set_columns({widgets::TableColumn{"Category", 20, 10}, widgets::TableColumn{"Size", 16, 10}});
    table->on_selection_changed = [this](widgets::TableCellRef reference) { memory_cursor_ = reference.row; };
    memory_table_ = table.get();
    column->add_item(std::move(table), ui::LayoutSpec{ui::SizePolicy::Expanding});
    window->set_content(std::move(column));

    widgets::Window* const opened = window.get();
    window->on_closed = [this, opened] {
        memory_window_ = nullptr;
        memory_cursor_ = widgets::kInvalidTableRowId;
        memory_table_ = nullptr;
        memory_bar_ = nullptr;
        widgets::schedule_self_detach(*opened, app_);
    };
    memory_window_ = desktop_->add_window(std::move(window));
    fill_memory_pane();
    app_.set_focus(memory_table_);
}

void SysInfoApp::open_volumes_window() {
    if (volumes_window_ != nullptr) {
        desktop_->activate(volumes_window_);
        return;
    }
    auto window = std::make_unique<widgets::Window>("Disks");
    window->set_bounds(Rect{3, 3, 72, 16});
    window->set_min_size(Size{36, 7});
    window->set_grow_policy(widgets::DesktopGrowPolicy::AnchorEdges);

    auto column = std::make_unique<ui::Column>();
    column->set_spacing(1);
    auto table = std::make_unique<widgets::Table>();
    table->set_columns({widgets::TableColumn{"Mounted on", 22, 10}, widgets::TableColumn{"Filesystem", 10, 6},
                        widgets::TableColumn{"Capacity", 11, 8}, widgets::TableColumn{"Free", 11, 8},
                        // Wide enough for the absence text: a column that
                        // clips "not reported" to "not re" has turned a
                        // stated absence into a typographical accident.
                        widgets::TableColumn{"Used", 12, 5}});
    volumes_table_ = table.get();
    // The bar follows the cursor, so the number under it is always the
    // volume the reader is looking at rather than the one that happened to
    // be first when the window opened.
    table->on_selection_changed = [this](widgets::TableCellRef reference) {
        volumes_cursor_ = reference.row;
        if (volumes_table_ == nullptr) return;
        const int row = volumes_table_->cursor_row();
        if (row >= 0) show_volume_usage(static_cast<std::size_t>(row));
    };
    column->add_item(std::move(table), ui::LayoutSpec{ui::SizePolicy::Expanding});
    auto bar = std::make_unique<widgets::Progress>();
    volumes_bar_ = bar.get();
    column->add_item(std::move(bar), ui::LayoutSpec{ui::SizePolicy::Fixed});
    window->set_content(std::move(column));

    widgets::Window* const opened = window.get();
    window->on_closed = [this, opened] {
        volumes_window_ = nullptr;
        volumes_cursor_ = widgets::kInvalidTableRowId;
        volumes_table_ = nullptr;
        volumes_bar_ = nullptr;
        widgets::schedule_self_detach(*opened, app_);
    };
    volumes_window_ = desktop_->add_window(std::move(window));
    fill_volumes_table();
    app_.set_focus(volumes_table_);
}

void SysInfoApp::open_benchmarks_window() {
    if (benchmarks_window_ != nullptr) {
        desktop_->activate(benchmarks_window_);
        return;
    }
    auto window = std::make_unique<widgets::Window>("Benchmarks");
    // Tall enough on an 80x24 terminal for the picker, the controls, the
    // chart with one bar per kernel, and the footnote under it -- the
    // whole point of the window is that all four are visible at once.
    window->set_bounds(Rect{4, 1, 68, 21});
    window->set_min_size(Size{40, 12});
    window->set_grow_policy(widgets::DesktopGrowPolicy::AnchorEdges);

    auto column = std::make_unique<ui::Column>();
    column->set_spacing(1);

    std::vector<std::string> labels;
    for (const BenchmarkDescriptor& descriptor : benchmark_catalogue())
        labels.emplace_back(descriptor.title);
    auto picker = std::make_unique<widgets::CheckGroup>(std::move(labels));
    picker->set_group_label("Measure");
    for (std::size_t index = 0; index < benchmark_catalogue().size(); ++index) picker->set_checked(index, true);
    benchmark_picker_ = picker.get();
    column->add_item(std::move(picker), ui::LayoutSpec{ui::SizePolicy::Fixed});

    auto buttons = std::make_unique<ui::Row>();
    buttons->set_spacing(2);
    auto run = std::make_unique<widgets::Button>("&Run");
    run->on_press = [this] { start_benchmarks(); };
    auto stop = std::make_unique<widgets::Button>("&Cancel");
    stop->on_press = [this] { cancel_benchmarks(); };
    buttons->add_item(std::move(run), ui::LayoutSpec{ui::SizePolicy::Fixed});
    buttons->add_item(std::move(stop), ui::LayoutSpec{ui::SizePolicy::Fixed});
    column->add_item(std::move(buttons), ui::LayoutSpec{ui::SizePolicy::Fixed});

    auto progress = std::make_unique<widgets::Progress>();
    progress->set_label("idle");
    benchmark_progress_ = progress.get();
    column->add_item(std::move(progress), ui::LayoutSpec{ui::SizePolicy::Fixed});

    auto note = std::make_unique<widgets::StaticText>("Nothing measured yet.");
    benchmark_note_ = note.get();
    column->add_item(std::move(note), ui::LayoutSpec{ui::SizePolicy::Fixed});

    auto chart = std::make_unique<BarChartView>();
    chart->set_placeholder("No measurements yet - press Run.");
    chart_ = chart.get();
    column->add_item(std::move(chart), ui::LayoutSpec{ui::SizePolicy::Expanding});

    // Under the chart, where a footnote belongs and where the asterisk on
    // every measured bar is answered.
    auto footnote = std::make_unique<widgets::StaticText>(measurement_caveat_text(probe_.build()));
    benchmark_footnote_ = footnote.get();
    column->add_item(std::move(footnote), ui::LayoutSpec{ui::SizePolicy::Fixed});

    window->set_content(std::move(column));

    // Esc stops a run rather than closing the window: a reader who wants
    // out of a measurement that is taking too long presses the key that
    // means "stop", and losing the results already charted would be a
    // second surprise on top of the first.
    window->cancel_request = [this] {
        if (!benchmarks_.running()) return false;
        cancel_benchmarks();
        return true;
    };

    widgets::Window* const opened = window.get();
    window->on_closed = [this, opened] {
        // The run keeps its own subscriber token, so a run in flight stops
        // delivering to a chart that no longer exists; cancelling as well
        // means it also stops measuring for nobody.
        cancel_benchmarks();
        benchmarks_window_ = nullptr;
        benchmark_picker_ = nullptr;
        benchmark_progress_ = nullptr;
        benchmark_note_ = nullptr;
        benchmark_footnote_ = nullptr;
        chart_ = nullptr;
        widgets::schedule_self_detach(*opened, app_);
    };
    benchmarks_window_ = desktop_->add_window(std::move(window));
    update_chart();
    app_.set_focus(benchmark_picker_);
}

void SysInfoApp::start_benchmarks() {
    open_benchmarks_window();
    if (benchmarks_.running() || chart_ == nullptr) return;

    std::vector<BenchmarkId> plan;
    const std::vector<BenchmarkDescriptor>& catalogue = benchmark_catalogue();
    for (std::size_t index = 0; index < catalogue.size(); ++index)
        if (benchmark_picker_ == nullptr || benchmark_picker_->checked(index)) plan.push_back(catalogue[index].id);
    if (plan.empty()) {
        if (benchmark_progress_ != nullptr) benchmark_progress_->set_label("nothing selected");
        return;
    }

    // The completed run becomes the comparison, and the new one starts
    // empty. A machine compared against itself an hour ago is the only
    // comparison this program can make honestly.
    if (!current_results_.empty() && !last_run_cancelled_) previous_results_ = current_results_;
    current_results_.clear();
    last_run_cancelled_ = false;
    update_chart();

    const std::size_t total = plan.size();
    benchmarks_.start(
        std::move(plan), chart_->lifetime_token(),
        [this, total](BenchmarkService::Progress progress) {
            if (benchmark_progress_ == nullptr) return;
            benchmark_progress_->set_fraction(total == 0 ? 0.0
                                                         : static_cast<double>(progress.completed) /
                                                               static_cast<double>(total));
            benchmark_progress_->set_label(std::string(describe(progress.current).title) + " (" +
                                           std::to_string(progress.completed + 1) + " of " +
                                           std::to_string(total) + ") - Esc cancels");
        },
        [this](BenchmarkResult result) {
            current_results_.push_back(result);
            if (benchmark_note_ != nullptr)
                benchmark_note_->set_text(std::string(describe(result.id).title) + ": " + result.rate_text +
                                          " = index " + result.index_text);
            update_chart();
        },
        [this](bool cancelled) {
            last_run_cancelled_ = cancelled;
            if (benchmark_progress_ == nullptr) return;
            benchmark_progress_->set_fraction(cancelled ? benchmark_progress_->fraction() : 1.0);
            benchmark_progress_->set_label(cancelled ? "cancelled" : "done");
        });
}

void SysInfoApp::cancel_benchmarks() { benchmarks_.cancel(); }

void SysInfoApp::update_chart() {
    if (chart_ == nullptr) return;
    // Every bar on this chart was measured by this build, so every bar
    // carries the build's mark -- an asterisk the footnote answers, or
    // nothing at all when the build is one whose speed means something.
    const std::string marker = measured_bar_marker(probe_.build());
    std::vector<ChartBar> bars;
    bars.reserve(current_results_.size() + previous_results_.size());
    for (const BenchmarkResult& result : current_results_)
        bars.push_back(
            ChartBar{std::string(describe(result.id).title) + marker, result.index_text, result.index, true});
    for (const BenchmarkResult& result : previous_results_)
        bars.push_back(ChartBar{std::string(describe(result.id).title) + marker + " (previous)",
                                result.index_text, result.index, false});
    chart_->set_bars(std::move(bars));
    // The scale, spelled out under the chart, because an index whose
    // definition is somewhere else is a number nobody can check.
    chart_->set_caption("index scale: 10 M steps/s, 100 MFLOPS or 1 GB/s = 1.0");
}

void SysInfoApp::refresh() {
    ++refresh_count_;
    if (system_table_ != nullptr) fill_system_table();
    if (memory_table_ != nullptr) fill_memory_pane();
    if (volumes_table_ != nullptr) fill_volumes_table();
}

// Table::set_rows() re-seats the cursor on the first row, and these panes
// replace their rows once a second: without putting it back, a reader who
// scrolled down would be dragged to the top on every tick of the clock —
// the pane would be unusable precisely because it is current.
//
// The key is the row identity Table itself handed out through
// on_selection_changed, which is what its contract says is durable. A
// display index is exactly the thing that is not: it means a different row
// the moment the list underneath changes length.
void SysInfoApp::refill(widgets::Table& table, widgets::TableRowId cursor,
                        std::vector<std::vector<std::string>> rows) {
    table.set_rows(std::move(rows));
    // A row that no longer exists leaves the cursor at the top, which is
    // where set_rows already put it.
    if (cursor != widgets::kInvalidTableRowId) table.set_selected_cell(widgets::TableCellRef{cursor, 0});
}

void SysInfoApp::fill_system_table() { refill(*system_table_, system_cursor_, system_rows(probe_)); }

void SysInfoApp::fill_memory_pane() {
    const MemoryReport memory = probe_.memory();
    if (memory_bar_ != nullptr) {
        memory_bar_->set_fraction(used_fraction(memory.total_bytes, memory.available_bytes));
        memory_bar_->set_label(memory_usage_text(memory));
    }
    refill(*memory_table_, memory_cursor_, memory_rows(memory));
}

void SysInfoApp::fill_volumes_table() {
    volumes_ = probe_.volumes();
    refill(*volumes_table_, volumes_cursor_, volume_rows(volumes_));
    // A restore notifies and the bar follows; a first fill does not, so
    // the bar is told about the row the table settled on either way.
    const int cursor = volumes_table_->cursor_row();
    show_volume_usage(cursor >= 0 ? static_cast<std::size_t>(cursor) : 0);
}

void SysInfoApp::show_volume_usage(std::size_t row) {
    if (volumes_bar_ == nullptr) return;
    if (row >= volumes_.size()) {
        volumes_bar_->set_fraction(0.0);
        volumes_bar_->set_label("no volume reported");
        return;
    }
    const VolumeReport& volume = volumes_[row];
    volumes_bar_->set_fraction(used_fraction(volume.capacity_bytes, volume.free_bytes));
    volumes_bar_->set_label(volume_usage_text(volume));
}

}  // namespace ckv::sysinfo
