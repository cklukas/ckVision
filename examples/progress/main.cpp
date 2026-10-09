// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Progress Lab: every presentation and timing option, interactively.
#include <cstdio>
#include <cstdlib>

#include "../example_diagnostics.hpp"
#if defined(_WIN32)
#include "cvision/term/windows_clock.hpp"
#include "cvision/term/windows_terminal.hpp"
#else
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"
#endif
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

#include "progress_lab_app.hpp"

int main() {
    return ckv::examples::run_reporting_failure([] {
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
        ckv::progress_lab::ProgressLabApp lab(app);
        app.run();
    });
}
