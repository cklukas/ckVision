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

#include "cvision/core/key.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/widgets/progress.hpp"
#include "cvision/widgets/table.hpp"
#include "cvision/widgets/window.hpp"

#include "fixed_system_probe.hpp"
#include "report_format.hpp"
#include "sysinfo_app.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
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
    SysInfoApp sysinfo{app, probe};

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
