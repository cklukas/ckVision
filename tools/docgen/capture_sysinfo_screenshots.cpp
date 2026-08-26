// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Captures the shipped SysInfo example through its ordinary public
// Application path. FixedSystemProbe, FixedBenchmarkRunner, and
// MemoryFileSystem make the machine, measurements, and report directory
// deterministic; the widgets and terminal presentation are the same ones the
// interactive executable uses.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "cvision/core/filesystem.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/widgets/combo_box.hpp"

#include "fixed_benchmark_runner.hpp"
#include "fixed_system_probe.hpp"
#include "frame_svg.hpp"
#include "sysinfo_app.hpp"

namespace {

[[noreturn]] void fail(std::string_view message) {
    std::fprintf(stderr, "sysinfo screenshot capture: %.*s\n", static_cast<int>(message.size()), message.data());
    std::exit(1);
}

void write_svg(const std::filesystem::path& directory, std::string_view name,
               const ckv::term::VirtualDisplay& display) {
    const std::filesystem::path path = directory / (std::string(name) + ".svg");
    std::ofstream out(path);
    if (!out) fail("could not open an output SVG");
    out << ckv::docgen::render_virtual_display_svg(display);
    if (!out) fail("could not write an output SVG");
    std::fprintf(stderr, "wrote %s (%dx%d cells, raster=%s)\n", path.string().c_str(),
                 display.size().width, display.size().height,
                 display.has_raster_pixels() ? "yes" : "no");
}

bool execute(ckv::ui::Application& app, std::string_view key) {
    const auto command = app.commands().id_for(std::string(key));
    return command.has_value() && app.execute_command(*command);
}

void finish_benchmarks(ckv::ui::Application& app, ckv::sysinfo::SysInfoApp& sysinfo) {
    sysinfo.set_scratch_directory("/scratch");
    if (!execute(app, ckv::sysinfo::SysInfoApp::kRunBenchmarksKey))
        fail("the Run benchmarks command was unavailable");
    sysinfo.benchmarks().wait_until_idle();
    for (int turn = 0; turn < 8; ++turn) app.step(0);
    if (sysinfo.current_results().size() != ckv::sysinfo::benchmark_catalogue().size())
        fail("the scripted benchmark run did not deliver every result");
}

void capture_summary_help_and_report(const std::filesystem::path& directory) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{100, 30}, ckv::term::headless_no_graphics_profile());
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    ckv::sysinfo::FixedSystemProbe probe;
    ckv::sysinfo::FixedBenchmarkRunner runner;
    ckv::MemoryFileSystem files;
    files.add_directory("/reports");
    ckv::sysinfo::SysInfoApp sysinfo(app, probe, runner, files, "/reports");

    app.step(0);
    write_svg(directory, "sysinfo-summary", terminal.display());

    app.dispatch(ckv::KeyEvent{ckv::KeyChord{ckv::Key::F1, ckv::Modifier::None, ""}});
    app.step(0);
    write_svg(directory, "sysinfo-help", terminal.display());

    // Use a fresh application for the save dialog so this is the report path,
    // not a dialog layered over the help viewer left open above.
    ckv::term::HeadlessTerminal save_terminal(ckv::Size{100, 30}, ckv::term::headless_no_graphics_profile());
    ckv::ManualClock save_clock;
    ckv::ui::Application save_app(save_terminal, save_clock);
    ckv::sysinfo::FixedSystemProbe save_probe;
    ckv::sysinfo::FixedBenchmarkRunner save_runner;
    ckv::MemoryFileSystem save_files;
    save_files.add_directory("/reports");
    ckv::sysinfo::SysInfoApp save_sysinfo(save_app, save_probe, save_runner, save_files, "/reports");
    if (!execute(save_app, ckv::sysinfo::SysInfoApp::kSaveMarkdownKey))
        fail("the Save Markdown report command was unavailable");
    save_app.step(0);
    write_svg(directory, "sysinfo-report-save", save_terminal.display());
}

void capture_benchmarks(const std::filesystem::path& directory) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{100, 30}, ckv::term::headless_no_graphics_profile());
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    ckv::sysinfo::FixedSystemProbe probe;
    ckv::sysinfo::FixedBenchmarkRunner runner;
    ckv::MemoryFileSystem files;
    files.add_directory("/reports");
    files.add_directory("/scratch");
    ckv::sysinfo::SysInfoApp sysinfo(app, probe, runner, files, "/reports");

    finish_benchmarks(app, sysinfo);

    ckv::widgets::ComboBox* const comparison = sysinfo.compare_picker();
    if (comparison == nullptr) fail("the benchmark comparison picker was not created");
    std::size_t memory_choice = 0;
    for (std::size_t index = 1; index < comparison->items().size(); ++index) {
        if (comparison->items()[index].find("Memory") == std::string::npos) continue;
        memory_choice = index;
        break;
    }
    if (memory_choice == 0) fail("the Memory reference comparison was unavailable");
    comparison->set_selected_index(memory_choice);
    if (comparison->on_select) comparison->on_select(memory_choice);

    for (std::size_t page = 0;
         page < sysinfo.benchmark_page_count() && sysinfo.benchmark_page_title() != "Memory bandwidth";
         ++page) {
        if (!execute(app, ckv::sysinfo::SysInfoApp::kNextPageKey))
            fail("the Next benchmark topic command was unavailable");
    }
    if (sysinfo.benchmark_page_title() != "Memory bandwidth")
        fail("the Memory bandwidth chart page was unavailable");
    app.step(0);
    write_svg(directory, "sysinfo-benchmarks", terminal.display());
}

void capture_latency(const std::filesystem::path& directory, ckv::term::Capabilities capabilities,
                     std::string_view name, bool expect_raster) {
    ckv::term::HeadlessTerminal terminal(ckv::Size{100, 30}, capabilities);
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    ckv::sysinfo::FixedSystemProbe probe;
    ckv::sysinfo::FixedBenchmarkRunner runner;
    ckv::MemoryFileSystem files;
    files.add_directory("/reports");
    files.add_directory("/scratch");
    ckv::sysinfo::SysInfoApp sysinfo(app, probe, runner, files, "/reports");

    finish_benchmarks(app, sysinfo);
    if (!execute(app, ckv::sysinfo::SysInfoApp::kLatencyPlotKey))
        fail("the Cache latency plot command was unavailable");
    app.step(0);
    if (terminal.display().has_raster_pixels() != expect_raster)
        fail("the latency capture did not use the expected graphics path");
    write_svg(directory, name, terminal.display());
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <output-directory>\n", argv[0]);
        return 1;
    }
    const std::filesystem::path directory = argv[1];
    std::filesystem::create_directories(directory);

    capture_summary_help_and_report(directory);
    capture_benchmarks(directory);
    capture_latency(directory, ckv::term::headless_sixel_profile(), "sysinfo-latency-sixel", true);
    capture_latency(directory, ckv::term::headless_no_graphics_profile(), "sysinfo-latency-no-graphics", false);
    return 0;
}
