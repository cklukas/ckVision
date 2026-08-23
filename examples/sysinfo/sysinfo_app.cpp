// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "sysinfo_app.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cvision/ui/layout.hpp"
#include "cvision/widgets/application_shell.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/widgets/canvas.hpp"
#include "cvision/widgets/directory_picker.hpp"
#include "cvision/widgets/file_dialog.hpp"
#include "cvision/widgets/combo_box.hpp"
#include "cvision/widgets/option_group.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/message_box.hpp"
#include "cvision/widgets/progress.hpp"
#include "cvision/widgets/status_line.hpp"
#include "cvision/widgets/table.hpp"
#include "cvision/widgets/text_view.hpp"
#include "cvision/widgets/window.hpp"

#include "latency_plot.hpp"
#include "report_format.hpp"

namespace ckv::sysinfo {

SysInfoApp::SysInfoApp(ui::Application& app, const SystemProbe& probe, const BenchmarkRunner& runner,
                       FileSystem& files, std::string report_directory)
    : app_(app),
      probe_(probe),
      files_(files),
      report_directory_(std::move(report_directory)),
      roles_(ui::intern_standard_roles(app.roles())),
      benchmarks_(app, runner) {
    install_help();
    build_chrome();

    open_system_window();
    refresh();

    // The live figures age visibly on this kind of program's screen — an
    // uptime that never advances is the first thing a reader notices and
    // the last thing they trust. One second is what the eye reads as
    // "current" without the pane becoming something that flickers.
    app_.start_timer(kRefreshIntervalNanos, true, [this] { refresh(); });

    // F1 answers with the topic for whatever the reader is looking at --
    // the pane, or the benchmark -- rather than with one page about the
    // program. A number nobody can interpret is not information, and the
    // interpretation is what these topics are.
    app_.set_help_provider([this](const std::string& key) {
        auto presentation = widgets::present_help_viewer(help_, key, app_, *desktop_, roles_);
        presentation.set_completion_handler([](widgets::HelpViewerResult) {});
    });
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
    const ui::CommandId save_text_command = app_.commands().declare(
        {.key = std::string(kSaveTextKey), .title = "Save as &text...", .category = "Report",
         .handler = [this] { save_report_with_dialog(ReportFormat::Text); }});
    const ui::CommandId save_markdown_command = app_.commands().declare(
        {.key = std::string(kSaveMarkdownKey), .title = "Save as &Markdown...", .category = "Report",
         .handler = [this] { save_report_with_dialog(ReportFormat::Markdown); }});
    const ui::CommandId terminal_command = app_.commands().declare(
        {.key = std::string(kTerminalWindowKey), .title = "&Terminal", .category = "System",
         .handler = [this] { open_terminal_window(); }});
    const ui::CommandId all_mounts_command = app_.commands().declare(
        {.key = std::string(kShowAllMountsKey), .title = "Show &all mount points", .category = "System",
         .handler = [this] {
             open_volumes_window();
             set_show_all_mounts(!show_all_mounts_);
         }});
    const ui::CommandId refresh_command = app_.commands().declare(
        {.key = std::string(kRefreshKey), .title = "&Refresh", .category = "System", .chord = "F2",
         .handler = [this] { refresh(); }});

    widgets::MenuBarItem system_menu{
        "&System",
        {
            widgets::MenuItem::command(widgets::CommandPresentation{system_command}),
            widgets::MenuItem::command(widgets::CommandPresentation{memory_command}),
            widgets::MenuItem::command(widgets::CommandPresentation{volumes_command}),
            widgets::MenuItem::command(widgets::CommandPresentation{all_mounts_command}),
            widgets::MenuItem::command(widgets::CommandPresentation{terminal_command}),
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
    const ui::CommandId plot_command = app_.commands().declare(
        {.key = std::string(kLatencyPlotKey), .title = "Cache latency &plot", .category = "Benchmarks",
         .handler = [this] { open_latency_plot_window(); }});
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
            widgets::MenuItem::separator(),
            widgets::MenuItem::command(widgets::CommandPresentation{plot_command}),
        }};
    widgets::MenuBarItem report_menu{
        "&Report",
        {
            widgets::MenuItem::command(widgets::CommandPresentation{save_text_command}),
            widgets::MenuItem::command(widgets::CommandPresentation{save_markdown_command}),
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
               .menus = {std::move(system_menu), std::move(benchmarks_menu), std::move(report_menu),
                         std::move(window_menu), std::move(help_menu)},
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
    table->set_help_context_key("sysinfo.system");
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
    table->set_help_context_key("sysinfo.memory");
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
    // 76 wide, which is what the five columns plus the table's scrollbar
    // need and still fits an 80-column terminal. At 72 the last column was
    // one cell short and drew "not reporte", which is how a stated absence
    // turns back into a typographical accident.
    window->set_bounds(Rect{2, 3, 76, 16});
    window->set_min_size(Size{36, 7});
    window->set_grow_policy(widgets::DesktopGrowPolicy::AnchorEdges);

    auto column = std::make_unique<ui::Column>();
    column->set_spacing(1);

    // The default is the disks a reader means when they say "disks"; the
    // rest are one keystroke away and are counted in the window's footer
    // so that hiding them is never silent.
    auto filter = std::make_unique<widgets::CheckGroup>(std::vector<std::string>{"Show &all mount points"});
    filter->set_checked(0, show_all_mounts_);
    filter->on_changed = [this](std::size_t, bool checked) { set_show_all_mounts(checked); };
    mount_filter_ = filter.get();
    column->add_item(std::move(filter), ui::LayoutSpec{ui::SizePolicy::Fixed});

    auto table = std::make_unique<widgets::Table>();
    table->set_columns({widgets::TableColumn{"Mounted on", 20, 10}, widgets::TableColumn{"Filesystem", 10, 6},
                        widgets::TableColumn{"Capacity", 11, 8}, widgets::TableColumn{"Free", 11, 8},
                        // Wide enough for the absence text: a column that
                        // clips "not reported" to "not re" has turned a
                        // stated absence into a typographical accident.
                        // Thirteen, not twelve: the table's scrollbar takes
                        // the last cell of the last column, and twelve left
                        // "not reported" one letter short on screen while
                        // being exactly wide enough in the model.
                        widgets::TableColumn{"Used", 13, 5}});
    table->set_help_context_key("sysinfo.volumes");
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
        mount_filter_ = nullptr;
        widgets::schedule_self_detach(*opened, app_);
    };
    volumes_window_ = desktop_->add_window(std::move(window));
    fill_volumes_table();
    app_.set_focus(volumes_table_);
}

// The cache-latency series, drawn the way this terminal can show it. Both
// renderings exist and both are correct; which one is on screen is decided
// by asking the terminal, not by assuming an answer. That question is the
// whole content of this window.
void SysInfoApp::open_latency_plot_window() {
    if (latency_plot_window_ != nullptr) {
        desktop_->activate(latency_plot_window_);
        return;
    }
    auto window = std::make_unique<widgets::Window>("Cache latency");
    window->set_bounds(Rect{8, 4, 60, 16});
    window->set_min_size(Size{32, 8});
    window->set_grow_policy(widgets::DesktopGrowPolicy::AnchorEdges);

    auto column = std::make_unique<ui::Column>();

    // One Canvas, always -- not a choice between two widgets made once at
    // construction. A terminal's answer about graphics can change while an
    // application is running, and Canvas's mandatory fallback painter is
    // the design's answer to that: the same data, drawn in cells, by the
    // same composition the cell chart uses.
    auto canvas = std::make_unique<widgets::Canvas>();
    // Canvas never asks the terminal for its cell metric itself; the owner
    // injects it (D-039), which is what keeps the picture's proportions
    // right on a terminal with tall thin cells and on one with square ones.
    canvas->set_cell_metrics(app_.terminal_cell_pixels());
    canvas->set_draw_callback([this](Image& image) { draw_latency_plot(image, latency_series()); });
    const ui::RoleId fallback_role = app_.roles().find("ckv.list.normal");
    canvas->set_fallback_painter([this, fallback_role](scene::Painter& painter, Rect area) {
        const Style style = app_.theme().resolve(fallback_role);
        const std::vector<std::string> rows = chart_rows(latency_bars(), area.width);
        for (int row = 0; row < area.height && static_cast<std::size_t>(row) < rows.size(); ++row)
            painter.draw_text(Point{0, row}, rows[static_cast<std::size_t>(row)], style);
    });
    canvas->set_help_context_key("sysinfo.latency");
    latency_canvas_ = canvas.get();
    column->add_item(std::move(canvas), ui::LayoutSpec{ui::SizePolicy::Expanding});

    auto footer = std::make_unique<widgets::StaticText>("");
    latency_plot_footer_ = footer.get();
    column->add_item(std::move(footer), ui::LayoutSpec{ui::SizePolicy::Fixed});
    window->set_content(std::move(column));

    widgets::Window* const opened = window.get();
    window->on_closed = [this, opened] {
        latency_plot_window_ = nullptr;
        latency_canvas_ = nullptr;
        latency_plot_footer_ = nullptr;
        widgets::schedule_self_detach(*opened, app_);
    };
    latency_plot_window_ = desktop_->add_window(std::move(window));
    update_latency_plot();
}

const std::vector<SeriesPoint>& SysInfoApp::latency_series() const {
    static const std::vector<SeriesPoint> none;
    const BenchmarkResult* const result = result_for(current_results_, BenchmarkId::CacheLatency);
    return result != nullptr ? result->series : none;
}

std::vector<ChartBar> SysInfoApp::latency_bars() const {
    std::vector<ChartBar> bars;
    for (const SeriesPoint& point : latency_series())
        bars.push_back(ChartBar{"Cache latency - ns per access", point.label, point.value_text, point.value,
                                BarKind::Measured, false});
    return bars;
}

void SysInfoApp::update_latency_plot() {
    const std::vector<SeriesPoint>& series = latency_series();
    if (latency_canvas_ != nullptr) latency_canvas_->invalidate_content();
    if (latency_plot_footer_ == nullptr) return;
    if (series.empty()) {
        latency_plot_footer_->set_text(app_.terminal_shows_graphics()
                                           ? "This terminal shows pictures; run Cache latency to draw one."
                                           : "This terminal shows no pictures, so the same data is drawn in cells.");
        return;
    }
    latency_plot_footer_->set_text(series.front().label + " " + series.front().value_text + "  ...  " +
                                   series.back().label + " " + series.back().value_text +
                                   (app_.terminal_shows_graphics() ? "   (drawn in pixels)"
                                                                   : "   (drawn in cells)"));
}

// The one report in this program that is not about the machine: what the
// terminal on the other end of the connection can do. A client choosing
// between a Canvas and a cell-drawn fallback is choosing on exactly these
// answers, and until now the only way to see them was to write a program.
void SysInfoApp::open_terminal_window() {
    if (terminal_window_ != nullptr) {
        desktop_->activate(terminal_window_);
        return;
    }
    auto window = std::make_unique<widgets::Window>("Terminal");
    window->set_bounds(Rect{5, 3, 64, 17});
    window->set_min_size(Size{36, 8});
    window->set_grow_policy(widgets::DesktopGrowPolicy::AnchorEdges);

    auto report = std::make_unique<widgets::TextView>();
    report->set_help_context_key("sysinfo.terminal");
    terminal_report_ = report.get();
    window->set_content(std::move(report));

    widgets::Window* const opened = window.get();
    window->on_closed = [this, opened] {
        terminal_window_ = nullptr;
        terminal_report_ = nullptr;
        widgets::schedule_self_detach(*opened, app_);
    };
    terminal_window_ = desktop_->add_window(std::move(window));
    fill_terminal_report();
    app_.set_focus(terminal_report_);
}

void SysInfoApp::fill_terminal_report() {
    if (terminal_report_ == nullptr) return;
    const Size cell = app_.terminal_cell_pixels();
    const bool graphics = app_.terminal_shows_graphics();

    std::string text = app_.terminal_capability_report_text();
    if (!text.empty() && text.back() != '\n') text += "\n";
    text += "\n";
    // The two lines a client actually decides on, spelled out rather than
    // left to be inferred from the capability list above them.
    text += "Cell size: ";
    text += cell.width > 0 && cell.height > 0
                ? std::to_string(cell.width) + " x " + std::to_string(cell.height) + " pixels"
                : std::string("not reported - a picture cannot be sized in true proportion");
    text += "\n";
    text += graphics ? "Pictures: this terminal decodes them, so Canvas and ImageView draw in pixels.\n"
                     : "Pictures: not available here, so Canvas and ImageView draw their cell fallback.\n";
    text += "\nThis is the same report a ckVision application reads to decide what to draw.\n";
    terminal_report_->set_text(std::move(text));
}

void SysInfoApp::open_benchmarks_window() {
    if (benchmarks_window_ != nullptr) {
        desktop_->activate(benchmarks_window_);
        return;
    }
    auto window = std::make_unique<widgets::Window>("Benchmarks");
    // The chart is the window: it is given whatever height the desktop can
    // spare, because a comparison the reader has to scroll is a comparison
    // they cannot make.
    const Rect available = desktop_->content_area();
    const int height = std::clamp(available.height, 12, 26);
    window->set_bounds(Rect{2, available.y, std::min(available.width - 4, 72), height});
    window->set_min_size(Size{44, 12});
    window->set_grow_policy(widgets::DesktopGrowPolicy::AnchorEdges);

    auto column = std::make_unique<ui::Column>();

    std::vector<std::string> labels;
    for (const BenchmarkDescriptor& descriptor : benchmark_catalogue())
        labels.emplace_back(descriptor.title);
    auto picker = std::make_unique<widgets::CheckGroup>(std::move(labels));
    picker->set_group_label("Measure  (F9 runs, Esc cancels)");
    for (std::size_t index = 0; index < benchmark_catalogue().size(); ++index) picker->set_checked(index, true);
    picker->set_help_context_key("sysinfo.benchmarks");
    benchmark_picker_ = picker.get();
    column->add_item(std::move(picker), ui::LayoutSpec{ui::SizePolicy::Fixed});

    // Which metric gets its comparison bars. One at a time on purpose: six
    // reference rows for each of three kernels is a screen of numbers
    // nobody reads, and the question a reader has is about one of them.
    auto compare = std::make_unique<widgets::ComboBox>(widgets::ComboBoxMode::PickOnly);
    compare->set_items(comparison_choices());
    compare->set_selected_index(0);
    compare->on_select = [this](std::size_t) { update_chart(); };
    compare_picker_ = compare.get();
    column->add_item(std::move(compare), ui::LayoutSpec{ui::SizePolicy::Fixed});

    auto progress = std::make_unique<widgets::Progress>();
    progress->set_label("idle");
    benchmark_progress_ = progress.get();
    column->add_item(std::move(progress), ui::LayoutSpec{ui::SizePolicy::Fixed});

    auto chart = std::make_unique<BarChartView>();
    chart->set_placeholder("No measurements yet - press F9 to run.");
    chart->set_help_context_key("sysinfo.benchmarks");
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
        compare_picker_ = nullptr;
        benchmark_progress_ = nullptr;
        benchmark_footnote_ = nullptr;
        chart_ = nullptr;
        widgets::schedule_self_detach(*opened, app_);
    };
    benchmarks_window_ = desktop_->add_window(std::move(window));
    update_chart();
    app_.set_focus(benchmark_picker_);
}

// The comparison picker's items: no comparison, then every kernel this
// program has published reference figures for.
std::vector<std::string> SysInfoApp::comparison_choices() {
    std::vector<std::string> choices{"Compare with: this run only"};
    for (const BenchmarkDescriptor& descriptor : benchmark_catalogue())
        if (!reference_points_for(descriptor.id).empty())
            choices.push_back("Compare with: " + std::string(descriptor.title) + " references");
    return choices;
}

std::optional<BenchmarkId> SysInfoApp::comparison_kernel() const {
    if (compare_picker_ == nullptr) return std::nullopt;
    const std::optional<std::size_t> selected = compare_picker_->selected_index();
    if (!selected.has_value() || *selected == 0) return std::nullopt;
    std::size_t offset = 1;
    for (const BenchmarkDescriptor& descriptor : benchmark_catalogue()) {
        if (reference_points_for(descriptor.id).empty()) continue;
        if (offset == *selected) return descriptor.id;
        ++offset;
    }
    return std::nullopt;
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

    // The disk kernel writes to the reader's machine, so it asks first --
    // once, and then remembers. Asking every run would train the reader to
    // dismiss the question, which is worse than not asking.
    const bool wants_disk = std::find(plan.begin(), plan.end(), BenchmarkId::DiskThroughput) != plan.end();
    if (wants_disk && scratch_directory_.empty()) {
        ask_for_scratch_directory();
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
        std::move(plan), run_options(), chart_->lifetime_token(),
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
            update_latency_plot();
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

RunOptions SysInfoApp::run_options() const {
    RunOptions options;
    // From the probe, not from the standard library behind the
    // application's back: how many cores this machine has is a fact about
    // the host, and the host is what SystemProbe is for.
    options.maximum_threads = std::max(1, probe_.processor().logical_cores.value_or(1));
    options.scratch_directory = scratch_directory_;
    return options;
}

void SysInfoApp::ask_for_scratch_directory() {
    // Says how much it will write before it asks where, because "choose a
    // directory" is not a question a reader can answer without that.
    auto message = widgets::present_message_box(
        app_, *desktop_, roles_,
        {widgets::MessageBoxKind::Confirm, "Measure disk throughput?",
         "This writes " + format_bytes(RunOptions::kDiskBytes) +
             " to a directory you choose, reads it back, and removes it. Choose the directory next.",
         widgets::MessageBoxButtons::OkCancel});
    message.set_completion_handler([this](widgets::MessageBoxResult answer) {
        if (answer != widgets::MessageBoxResult::Ok) {
            if (benchmark_progress_ != nullptr) benchmark_progress_->set_label("disk measurement declined");
            return;
        }
        auto picker = widgets::present_directory_picker(files_, report_directory_, app_, *desktop_, roles_);
        picker.set_completion_handler([this](widgets::DirectoryPickerResult chosen) {
            if (!chosen.accepted || chosen.path.empty()) return;
            scratch_directory_ = chosen.path;
            // The reader answered the question they were asked; running is
            // what they asked for.
            start_benchmarks();
        });
    });
}

void SysInfoApp::update_chart() {
    if (chart_ == nullptr) return;
    // Every measured bar carries the build's mark -- an asterisk the
    // footnote answers, or nothing at all when the build is one whose speed
    // means something.
    const std::string marker = measured_bar_marker(probe_.build());
    const std::optional<BenchmarkId> compared = comparison_kernel();

    std::vector<ChartBar> bars;
    bool any_reference = false;
    bool any_ideal = false;
    bool any_index = false;
    for (const BenchmarkDescriptor& descriptor : benchmark_catalogue()) {
        const BenchmarkResult* const current = result_for(current_results_, descriptor.id);
        const BenchmarkResult* const previous = result_for(previous_results_, descriptor.id);
        const bool comparing = compared.has_value() && *compared == descriptor.id;
        if (current == nullptr && previous == nullptr && !comparing) continue;

        // A kernel that measured a curve draws the curve: one bar per
        // point, in its own group, with the perfect result beside each
        // point that has one.
        if (current != nullptr && !current->series.empty()) {
            const std::string group = std::string(descriptor.title) + " - " + current->series_caption;
            for (const SeriesPoint& point : current->series) {
                bars.push_back(ChartBar{group, point.label + marker, point.value_text, point.value,
                                        BarKind::Measured, false});
                if (point.ideal <= 0.0) continue;
                bars.push_back(ChartBar{group, point.label + ", perfect", format_decimal(point.ideal, 2) + "x",
                                        point.ideal, BarKind::Reference, false});
                any_ideal = true;
            }
            continue;
        }

        // Each metric is its own chart: bars are drawn against the longest
        // bar in their own group and against nothing else -- and each
        // carries its own 1.0 in its heading. A single caption under the
        // chart could only name one scale, and named the wrong one for
        // every other group the moment there was more than one: the disk
        // index is per 100 MB/s where the memory index is per 1 GB/s.
        const std::string group = std::string(descriptor.title) + " - index, 1.0 = " +
                                  format_rate(descriptor.id, unit_rate(descriptor.id));
        any_index = true;
        if (current != nullptr)
            bars.push_back(ChartBar{group, "This computer" + marker, current->index_text, current->index,
                                    BarKind::Measured, true});
        if (previous != nullptr && previous->series.empty())
            bars.push_back(ChartBar{group, "Previous run" + marker, previous->index_text, previous->index,
                                    BarKind::Measured, false});
        if (!comparing) continue;
        for (const ReferencePoint& point : reference_points_for(descriptor.id)) {
            const double index = point.rate / unit_rate(descriptor.id);
            bars.push_back(ChartBar{group, std::string(point.label), format_decimal(index, 1), index,
                                    BarKind::Reference, false});
            any_reference = true;
        }
    }
    chart_->set_bars(std::move(bars));
    // Which ink means what. Only while both kinds are on the chart: a
    // legend for bars that are not there teaches a distinction the reader
    // cannot see.
    // Naming the shaded ink for what it is on THIS chart: a figure from a
    // standard, or the result a perfect machine would have shown. Both are
    // things nobody measured, and neither is the other.
    std::string legend;
    if (any_reference && any_ideal)
        legend = "\xE2\x96\x88 measured here   \xE2\x96\x92 not measured: a published ceiling or a perfect result";
    else if (any_reference)
        legend = "\xE2\x96\x88 measured here   \xE2\x96\x92 published ceiling, not a measurement";
    else if (any_ideal)
        legend = "\xE2\x96\x88 measured here   \xE2\x96\x92 perfect scaling, not a measurement";
    chart_->set_legend(std::move(legend));
    // Each index group names its own 1.0 in its heading, so the caption
    // says the one thing that is true of the whole chart rather than a
    // scale that belongs to one group of it.
    chart_->set_caption(any_index ? "each group is drawn against its own longest bar" : "");
}

const BenchmarkResult* SysInfoApp::result_for(const std::vector<BenchmarkResult>& results, BenchmarkId id) {
    for (const BenchmarkResult& result : results)
        if (result.id == id) return &result;
    return nullptr;
}

// One topic per pane and one per benchmark, keyed by the same strings the
// views carry as their help context. Written here rather than in a file:
// this is what the numbers mean, and it belongs beside the code that
// produces them, where it goes stale visibly rather than quietly.
void SysInfoApp::install_help() {
    help_.add_topic("sysinfo.system",
                    widgets::HelpTopic{"System summary",
                                       "Everything this program could read about the machine and about its own "
                                       "build. A field the host did not answer reads \"not reported\": a plausible "
                                       "zero would be indistinguishable from a measurement.\n\nAll of it arrives "
                                       "through one interface, SystemProbe, which is the only part of this example "
                                       "that touches the platform.",
                                       {{"sysinfo.memory", "Memory"}, {"sysinfo.benchmarks", "Benchmarks"}}});
    help_.add_topic("sysinfo.memory",
                    widgets::HelpTopic{"Memory",
                                       "Total and available, then the host's own accounting in the host's own "
                                       "words. Operating systems disagree deeply about what \"used\" means, so this "
                                       "pane does not translate their categories into a common vocabulary it would "
                                       "have had to invent.\n\nThe bar shows total minus available.",
                                       {{"sysinfo.system", "System summary"}}});
    help_.add_topic("sysinfo.volumes",
                    widgets::HelpTopic{"Disks",
                                       "Every mounted filesystem with a capacity worth showing; the kernel's own "
                                       "pseudo-filesystems and Time Machine's local snapshots are left out.\n\nA "
                                       "volume can report more free space than capacity -- on a shared APFS "
                                       "container it routinely does, because the capacity is the volume's and the "
                                       "free space is the container's. There is no used share to compute then, so "
                                       "none is shown.",
                                       {{"sysinfo.system", "System summary"}}});
    help_.add_topic("sysinfo.terminal",
                    widgets::HelpTopic{"Terminal",
                                       "What the terminal on the other end can do: colour depth, mouse protocol, "
                                       "the pixel size of one character cell, and whether it decodes pictures.\n\n"
                                       "This is the report a ckVision application reads to decide whether to draw "
                                       "a Canvas in pixels or fall back to cells -- and it is about the terminal, "
                                       "not about the machine.",
                                       {{"sysinfo.benchmarks", "Benchmarks"}}});
    help_.add_topic("sysinfo.benchmarks",
                    widgets::HelpTopic{"Benchmarks",
                                       "Select what to measure, press F9, and press Esc to stop. Each kernel runs "
                                       "a fixed quantum of work several times and the fastest pass is reported: "
                                       "the slow passes are the ones something else on the machine interrupted."
                                       "\n\nThe index is this program's own scale, printed under the chart. Bars "
                                       "drawn solid were measured here; shaded bars are published ceilings or "
                                       "perfect results, which nobody measured. An unoptimized build marks every "
                                       "bar it measured with an asterisk, because its numbers describe the build.",
                                       {{"sysinfo.integer", "Integer mix"},
                                        {"sysinfo.float", "Floating point"},
                                        {"sysinfo.memory-bandwidth", "Memory bandwidth"},
                                        {"sysinfo.latency", "Cache latency"},
                                        {"sysinfo.scaling", "Thread scaling"}}});
    for (const BenchmarkDescriptor& descriptor : benchmark_catalogue())
        help_.add_topic("sysinfo." + std::string(descriptor.key),
                        widgets::HelpTopic{std::string(descriptor.title),
                                           std::string(descriptor.explanation) + "\n\nMeasured in " +
                                               std::string(descriptor.rate_unit) + ".",
                                           {{"sysinfo.benchmarks", "Benchmarks"}}});
}

std::string SysInfoApp::report_text(ReportFormat format) const {
    // The terminal section comes from the application because only the
    // application has a terminal; everything else the document composes
    // from the same functions the panes use.
    return compose_report(probe_, current_results_, app_.terminal_capability_report_text(), format);
}

bool SysInfoApp::save_report(const std::string& path, ReportFormat format) {
    const FileWriteResult written = files_.write_file_atomic(path, report_text(format));
    if (written.status != FileWriteStatus::Ok) return false;
    last_saved_report_ = path;
    return true;
}

void SysInfoApp::save_report_with_dialog(ReportFormat format) {
    // Non-blocking, from a command handler: the dialog is presented and
    // this returns, and the write happens when the reader has chosen.
    auto dialog = widgets::present_file_dialog(widgets::FileDialogMode::Save, report_directory_, files_, app_,
                                               *desktop_, roles_);
    dialog.set_completion_handler([this, format](widgets::FileDialogResult result) {
        if (!result.accepted || result.path.empty()) return;
        const bool written = save_report(result.path, format);
        auto message = widgets::present_message_box(
            app_, *desktop_, roles_,
            {written ? widgets::MessageBoxKind::Info : widgets::MessageBoxKind::Error,
             written ? "Report saved" : "Report not saved",
             written ? result.path : "This program could not write " + result.path,
             widgets::MessageBoxButtons::Ok});
        message.set_completion_handler([](widgets::MessageBoxResult) {});
    });
}

void SysInfoApp::refresh() {
    ++refresh_count_;
    if (system_table_ != nullptr) fill_system_table();
    if (memory_table_ != nullptr) fill_memory_pane();
    if (volumes_table_ != nullptr) fill_volumes_table();
    // The terminal can change under a running application -- a resize, a
    // capability answer arriving late -- so its report is refreshed with
    // everything else rather than frozen at the moment the pane opened.
    fill_terminal_report();
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
    const std::vector<VolumeReport> reported = probe_.volumes();
    volumes_.clear();
    hidden_mounts_ = 0;
    for (const VolumeReport& volume : reported) {
        if (!show_all_mounts_ && volume.system) {
            ++hidden_mounts_;
            continue;
        }
        volumes_.push_back(volume);
    }
    if (volumes_window_ != nullptr)
        volumes_window_->set_footer(hidden_mounts_ == 0
                                        ? std::string()
                                        : std::to_string(hidden_mounts_) + " system mount points hidden");
    refill(*volumes_table_, volumes_cursor_, volume_rows(volumes_));
    // A restore notifies and the bar follows; a first fill does not, so
    // the bar is told about the row the table settled on either way.
    const int cursor = volumes_table_->cursor_row();
    show_volume_usage(cursor >= 0 ? static_cast<std::size_t>(cursor) : 0);
}

void SysInfoApp::set_show_all_mounts(bool show) {
    if (show_all_mounts_ == show) return;
    show_all_mounts_ = show;
    // The cursor was on a row of the old list; the new list is a different
    // list, so it starts at the top rather than on whatever now happens to
    // sit at that position.
    volumes_cursor_ = widgets::kInvalidTableRowId;
    if (mount_filter_ != nullptr && mount_filter_->checked(0) != show) mount_filter_->set_checked(0, show);
    if (volumes_table_ != nullptr) fill_volumes_table();
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
