// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// ckVision SysInfo — the interactive host. The real machine is injected
// here and nowhere else; everything the application does with it is in
// sysinfo_app.cpp, which never learns which probe it was given.
#include "../native_host.hpp"
#include <cstdlib>
#include <string>

#include "../example_diagnostics.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

#include "benchmark.hpp"
#if defined(_WIN32)
#include "windows_system_probe.hpp"
#else
#include "posix_system_probe.hpp"
#endif
#include "sysinfo_app.hpp"

namespace {

// Where the save dialog opens. Reading the environment is the host's job
// and never the application's: SysInfoApp is handed a directory, exactly
// as it is handed a probe and a filesystem.
std::string home_directory() {
    const auto home = ckv::examples::current_user().home;
    return home.empty() ? std::string(".") : home;
}

}  // namespace

int main() {
    return ckv::examples::run_reporting_failure([] {
        ckv::examples::NativeClock clock;
        const ckv::examples::ExampleDiagnostics diagnostics(clock);
        ckv::examples::NativeTerminal terminal(clock);
        diagnostics.attach(terminal);
        ckv::term::TerminalClipboardWriter clipboard(terminal);

#if defined(_WIN32)
        ckv::sysinfo::WindowsSystemProbe probe;
#else
        ckv::sysinfo::PosixSystemProbe probe;
#endif
        // The benchmark runner takes the same clock the application does. A
        // measurement that reached for a clock of its own would be the one
        // place in this example that broke the rule the rest of it teaches.
        ckv::sysinfo::MeasuredBenchmarkRunner runner(clock);
        ckv::examples::NativeFileSystem files;
        ckv::ui::Application app(terminal, clock, clipboard);
        diagnostics.attach(app);
        ckv::sysinfo::SysInfoApp sysinfo(app, probe, runner, files, home_directory());
        app.run();
    });
}
