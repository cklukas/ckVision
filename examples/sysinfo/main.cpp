// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// ckVision SysInfo — the interactive host. The real machine is injected
// here and nowhere else; everything the application does with it is in
// sysinfo_app.cpp, which never learns which probe it was given.
#include <cstdlib>
#include <string>

#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_filesystem.hpp"
#include "cvision/term/posix_terminal.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

#include "benchmark.hpp"
#include "posix_system_probe.hpp"
#include "sysinfo_app.hpp"

namespace {

// Where the save dialog opens. Reading the environment is the host's job
// and never the application's: SysInfoApp is handed a directory, exactly
// as it is handed a probe and a filesystem.
std::string home_directory() {
    const char* const home = std::getenv("HOME");
    return home != nullptr && *home != '\0' ? std::string(home) : std::string("/");
}

}  // namespace

int main() {
    ckv::term::PosixClock clock;
    ckv::term::PosixTerminal terminal(clock);
    ckv::term::TerminalClipboardWriter clipboard(terminal);

    ckv::sysinfo::PosixSystemProbe probe;
    // The benchmark runner takes the same clock the application does. A
    // measurement that reached for a clock of its own would be the one
    // place in this example that broke the rule the rest of it teaches.
    ckv::sysinfo::MeasuredBenchmarkRunner runner(clock);
    // How many threads the scaling run may use is a fact about the host, so
    // it comes from the probe rather than from the runner asking the
    // standard library behind the application's back.
    runner.set_maximum_threads(probe.processor().logical_cores.value_or(1));
    ckv::term::PosixFileSystem files;
    ckv::ui::Application app(terminal, clock, clipboard);
    ckv::sysinfo::SysInfoApp sysinfo(app, probe, runner, files, home_directory());
    app.run();
    return 0;
}
