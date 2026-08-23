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
#include <string>
#include <string_view>
#include <vector>

#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/table.hpp"

#include "system_probe.hpp"

namespace ckv::widgets {
class Progress;
class Window;
}  // namespace ckv::widgets

namespace ckv::sysinfo {

class SysInfoApp {
public:
    // `probe` must outlive the application; it is read on every refresh.
    SysInfoApp(ui::Application& app, const SystemProbe& probe);

    // Command keys, named here so a test asks for a command the way the
    // menu does — by key — rather than by where it happens to sit.
    static constexpr std::string_view kSystemWindowKey = "sysinfo.system-window";
    static constexpr std::string_view kMemoryWindowKey = "sysinfo.memory-window";
    static constexpr std::string_view kVolumesWindowKey = "sysinfo.volumes-window";
    static constexpr std::string_view kRefreshKey = "sysinfo.refresh";

    widgets::Desktop& desktop() noexcept { return *desktop_; }

    widgets::Window* system_window() const noexcept { return system_window_; }
    widgets::Table* system_table() const noexcept { return system_table_; }
    widgets::Window* memory_window() const noexcept { return memory_window_; }
    widgets::Table* memory_table() const noexcept { return memory_table_; }
    widgets::Progress* memory_bar() const noexcept { return memory_bar_; }
    widgets::Window* volumes_window() const noexcept { return volumes_window_; }
    widgets::Table* volumes_table() const noexcept { return volumes_table_; }
    widgets::Progress* volumes_bar() const noexcept { return volumes_bar_; }

    // Re-reads the live reports and repopulates whichever panes are open.
    // The refresh timer calls this; so does the Refresh command, so a
    // reader who wants a figure now does not have to wait a second for it.
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

    void fill_system_table();
    void fill_memory_pane();
    void fill_volumes_table();
    void show_volume_usage(std::size_t row);

    ui::Application& app_;
    const SystemProbe& probe_;
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

    // The row each pane's cursor is on, by the identity Table handed out.
    widgets::TableRowId system_cursor_ = widgets::kInvalidTableRowId;
    widgets::TableRowId memory_cursor_ = widgets::kInvalidTableRowId;
    widgets::TableRowId volumes_cursor_ = widgets::kInvalidTableRowId;

    std::vector<VolumeReport> volumes_;
    int refresh_count_ = 0;
};

}  // namespace ckv::sysinfo
