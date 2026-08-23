// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// ckVision SysInfo: a system information and benchmarking application in
// the interaction grammar of the diagnostic tools of the early 1990s —
// a menu bar over a desktop of windows, every figure named, and nothing
// on screen that the program did not either read or measure.
//
// The application holds a SystemProbe reference and nothing else about the
// machine. That is the point of the example as much as the panes are: the
// whole object graph below is portable, testable code, and swapping the
// scripted machine in for the real one changes no line of it.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/core/filesystem.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/dialog.hpp"
#include "cvision/widgets/file_dialog.hpp"
#include "cvision/widgets/help_viewer.hpp"
#include "cvision/widgets/message_box.hpp"
#include "cvision/widgets/table.hpp"

#include "bar_chart_view.hpp"
#include "report_document.hpp"
#include "benchmark_service.hpp"
#include "reference_points.hpp"
#include "system_probe.hpp"

namespace ckv::widgets {
class Canvas;
class CheckGroup;
class ComboBox;
class TextView;
class Progress;
class StaticText;
class Window;
}  // namespace ckv::widgets

namespace ckv::sysinfo {

class SysInfoApp {
public:
    // `probe`, `runner` and `files` must all outlive the application: the
    // probe is read on every refresh, the runner on every benchmark run,
    // and the filesystem whenever a report is saved. `report_directory` is
    // where the save dialog opens -- a host decision, since only the host
    // knows whose machine this is.
    SysInfoApp(ui::Application& app, const SystemProbe& probe, const BenchmarkRunner& runner, FileSystem& files,
               std::string report_directory);

    // Command keys, named here so a test asks for a command the way the
    // menu does — by key — rather than by where it happens to sit.
    static constexpr std::string_view kSystemWindowKey = "sysinfo.system-window";
    static constexpr std::string_view kMemoryWindowKey = "sysinfo.memory-window";
    static constexpr std::string_view kVolumesWindowKey = "sysinfo.volumes-window";
    static constexpr std::string_view kShowAllMountsKey = "sysinfo.show-all-mounts";
    static constexpr std::string_view kRefreshKey = "sysinfo.refresh";
    static constexpr std::string_view kTerminalWindowKey = "sysinfo.terminal-window";
    static constexpr std::string_view kLatencyPlotKey = "sysinfo.latency-plot";
    static constexpr std::string_view kSaveTextKey = "sysinfo.save-text-report";
    static constexpr std::string_view kSaveMarkdownKey = "sysinfo.save-markdown-report";
    static constexpr std::string_view kBenchmarksWindowKey = "sysinfo.benchmarks-window";
    static constexpr std::string_view kRunBenchmarksKey = "sysinfo.run-benchmarks";
    static constexpr std::string_view kCancelBenchmarksKey = "sysinfo.cancel-benchmarks";
    static constexpr std::string_view kNextPageKey = "sysinfo.next-page";
    static constexpr std::string_view kPreviousPageKey = "sysinfo.previous-page";

    widgets::Desktop& desktop() noexcept { return *desktop_; }

    widgets::Window* system_window() const noexcept { return system_window_; }
    widgets::Table* system_table() const noexcept { return system_table_; }
    widgets::Window* memory_window() const noexcept { return memory_window_; }
    widgets::Table* memory_table() const noexcept { return memory_table_; }
    widgets::Progress* memory_bar() const noexcept { return memory_bar_; }
    widgets::Window* volumes_window() const noexcept { return volumes_window_; }
    widgets::Table* volumes_table() const noexcept { return volumes_table_; }
    bool showing_all_mounts() const noexcept { return show_all_mounts_; }
    // How many mount points the pane is currently not showing.
    int hidden_mount_count() const noexcept { return hidden_mounts_; }
    widgets::Progress* volumes_bar() const noexcept { return volumes_bar_; }
    widgets::Window* latency_plot_window() const noexcept { return latency_plot_window_; }
    widgets::Canvas* latency_canvas() const noexcept { return latency_canvas_; }
    // What the Canvas's cell fallback would draw, which is what a test can
    // assert without a terminal that decodes pictures.
    std::vector<ChartBar> latency_bars_for_test() const { return latency_bars(); }
    widgets::Window* terminal_window() const noexcept { return terminal_window_; }
    widgets::TextView* terminal_report() const noexcept { return terminal_report_; }
    widgets::Window* benchmarks_window() const noexcept { return benchmarks_window_; }
    widgets::CheckGroup* benchmark_picker() const noexcept { return benchmark_picker_; }
    widgets::ComboBox* compare_picker() const noexcept { return compare_picker_; }
    widgets::Progress* benchmark_progress() const noexcept { return benchmark_progress_; }
    BarChartView* chart() const noexcept { return chart_; }
    // Which topic the Benchmarks window is showing: 0 is the page that
    // chooses and runs them, and each measured metric has its own after it.
    std::size_t benchmark_page() const noexcept { return benchmark_page_; }
    std::size_t benchmark_page_count() const noexcept { return chart_groups_.size() + 1; }
    const std::string& benchmark_page_title() const noexcept { return benchmark_page_title_; }
    // Every bar the current results would draw, across all topics. The
    // chart itself holds only the page on screen.
    const std::vector<ChartBar>& all_chart_bars() const noexcept { return chart_bars_; }
    widgets::StaticText* benchmark_footnote() const noexcept { return benchmark_footnote_; }
    BenchmarkService& benchmarks() noexcept { return benchmarks_; }

    // The run being charted, and the one before it.
    const std::vector<BenchmarkResult>& current_results() const noexcept { return current_results_; }
    const std::vector<BenchmarkResult>& previous_results() const noexcept { return previous_results_; }
    // True once a run has ended, whether it completed or was cancelled.
    bool last_run_cancelled() const noexcept { return last_run_cancelled_; }

    // Re-reads the live reports and repopulates whichever panes are open.
    // The refresh timer calls this; so does the Refresh command, so a
    // reader who wants a figure now does not have to wait a second for it.
    // The whole report, exactly as the panes show it.
    std::string report_text(ReportFormat format) const;
    // The path of the last report written, for a test to read back.
    const std::string& last_saved_report() const noexcept { return last_saved_report_; }
    const std::string& scratch_directory() const noexcept { return scratch_directory_; }
    // Where the disk kernel may write. Normally set by the reader's own
    // answer in the directory picker; a host that already knows where its
    // scratch space is may set it up front, and a test may too.
    void set_scratch_directory(std::string path) { scratch_directory_ = std::move(path); }
    // Whether the directory question has actually reached a picker. The
    // dialog chain that asks it broke silently once; this is how a test
    // sees that it is still connected.
    bool pending_directory_open() const noexcept { return pending_scratch_dialog_.has_value(); }

    // Writes `format` to `path` through the injected filesystem, without a
    // dialog. The dialog path calls this; a test can too.
    bool save_report(const std::string& path, ReportFormat format);

    const widgets::HelpProvider& help() const noexcept { return help_; }

    void refresh();
    int refresh_count() const noexcept { return refresh_count_; }

    // The interval the application asks the framework for. Stated here
    // because a test that hard-codes its own idea of it proves nothing
    // about what the application actually scheduled.
    static constexpr std::int64_t kRefreshIntervalNanos = 1'000'000'000;

private:
    void build_chrome();
    void open_system_window();
    void open_memory_window();
    void open_volumes_window();

    // Replaces a pane's rows and puts the cursor back where the reader
    // left it -- see the definition for why a display index would be the
    // wrong key.
    void refill(widgets::Table& table, widgets::TableRowId cursor,
                std::vector<std::vector<std::string>> rows);

    RunOptions run_options() const;
    void ask_for_scratch_directory();
    void save_report_with_dialog(ReportFormat format);
    void install_help();

    void open_latency_plot_window();
    void update_latency_plot();
    const std::vector<SeriesPoint>& latency_series() const;
    // The same series as chart bars, for the Canvas's cell fallback.
    std::vector<ChartBar> latency_bars() const;
    void open_terminal_window();
    void fill_terminal_report();
    void open_benchmarks_window();
    void start_benchmarks();
    void cancel_benchmarks();
    void update_chart();
    void show_benchmark_page(std::size_t page);
    void step_benchmark_page(int direction);
    // The kernel whose published reference figures are on the chart, or
    // nothing when the reader asked for this run only.
    std::optional<BenchmarkId> comparison_kernel() const;
    static std::vector<std::string> comparison_choices();
    static const BenchmarkResult* result_for(const std::vector<BenchmarkResult>& results, BenchmarkId id);

    void fill_system_table();
    void fill_memory_pane();
    void fill_volumes_table();
    void show_volume_usage(std::size_t row);
    void set_show_all_mounts(bool show);

    ui::Application& app_;
    const SystemProbe& probe_;
    FileSystem& files_;
    std::string report_directory_;
    widgets::MemoryHelpProvider help_;
    std::string last_saved_report_;
    // Where the disk kernel has been given permission to write. Empty
    // until the reader says otherwise, and this program never fills it in
    // on their behalf.
    std::string scratch_directory_;

    // A dialog's completion handler lives exactly as long as the
    // presentation handle it was installed through: dropping the handle
    // withdraws the handler (dialog_presentation.hpp says so, and D-038 is
    // why). Held as members, so the question this application asks is
    // actually answered -- see ask_for_scratch_directory, which asked and
    // then discarded the answer until this was fixed.
    //
    // Members of SysInfoApp specifically, which every host constructs after
    // its Application and therefore destroys before it: a handler that
    // outlived its owner would be called from Application's teardown into
    // a dead frame, which is the failure that destructor is guarding.
    std::optional<widgets::MessageBoxPresentation> pending_message_;
    std::optional<widgets::DescriptorDialogPresentation> pending_scratch_dialog_;
    std::optional<widgets::FileDialogPresentation> pending_save_;
    ui::StandardRoles roles_;

    widgets::Desktop* desktop_ = nullptr;
    widgets::Window* system_window_ = nullptr;
    widgets::Table* system_table_ = nullptr;
    widgets::Window* memory_window_ = nullptr;
    widgets::Table* memory_table_ = nullptr;
    widgets::Progress* memory_bar_ = nullptr;
    widgets::Window* volumes_window_ = nullptr;
    widgets::Table* volumes_table_ = nullptr;
    widgets::Progress* volumes_bar_ = nullptr;
    widgets::CheckGroup* mount_filter_ = nullptr;
    bool show_all_mounts_ = false;
    int hidden_mounts_ = 0;

    // The row each pane's cursor is on, by the identity Table handed out.
    widgets::TableRowId system_cursor_ = widgets::kInvalidTableRowId;
    widgets::TableRowId memory_cursor_ = widgets::kInvalidTableRowId;
    widgets::TableRowId volumes_cursor_ = widgets::kInvalidTableRowId;

    widgets::Window* latency_plot_window_ = nullptr;
    widgets::Canvas* latency_canvas_ = nullptr;
    widgets::StaticText* latency_plot_footer_ = nullptr;
    widgets::Window* terminal_window_ = nullptr;
    widgets::TextView* terminal_report_ = nullptr;
    widgets::Window* benchmarks_window_ = nullptr;
    widgets::CheckGroup* benchmark_picker_ = nullptr;
    widgets::ComboBox* compare_picker_ = nullptr;
    widgets::Progress* benchmark_progress_ = nullptr;
    widgets::StaticText* benchmark_note_ = nullptr;
    widgets::StaticText* benchmark_footnote_ = nullptr;
    ui::View* benchmark_spacer_ = nullptr;
    BarChartView* chart_ = nullptr;

    // Every group the current results would draw, in order: the topics the
    // window pages through.
    std::vector<std::string> chart_groups_;
    // Each topic's units, shown under its axis.
    std::map<std::string, std::string> chart_captions_;
    std::vector<ChartBar> chart_bars_;
    std::size_t benchmark_page_ = 0;
    std::string benchmark_page_title_;

    std::vector<BenchmarkResult> current_results_;
    std::vector<BenchmarkResult> previous_results_;
    bool last_run_cancelled_ = false;

    std::vector<VolumeReport> volumes_;
    int refresh_count_ = 0;

    // Declared last, so it is destroyed first: its destructor cancels any
    // run and joins the worker, which must happen before anything the
    // handlers reach for goes away.
    BenchmarkService benchmarks_;
};

}  // namespace ckv::sysinfo
