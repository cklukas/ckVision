// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "../example_diagnostics.hpp"

#if defined(_WIN32)
#include "cvision/term/windows_clock.hpp"
#include "cvision/term/windows_terminal.hpp"
#include <cstdio>
#include <cstdlib>
#include <exception>
#else
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"
#endif
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

#include "terminal_app.hpp"

namespace {

void run_example() {
#if defined(_WIN32)
    ckv::term::WindowsClock clock;
    ckv::term::WindowsTerminal terminal(clock);
#else
    ckv::term::PosixClock clock;
    ckv::term::PosixTerminal terminal(clock);
#endif
    const ckv::examples::ExampleDiagnostics diagnostics(clock);
#if !defined(_WIN32)
    diagnostics.attach(terminal);
#endif
    ckv::term::TerminalClipboardWriter clipboard(terminal);
    ckv::ui::Application app(terminal, clock, clipboard);
    diagnostics.attach(app);
    ckv::terminal_example::TerminalApp example(app);
    app.run();
}

}  // namespace

int main() { return ckv::examples::run_reporting_failure(run_example); }
