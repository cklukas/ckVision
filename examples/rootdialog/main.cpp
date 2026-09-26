// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// ckVision Root Dialog -- the form is the program. An accepted form prints
// its answers on standard output once the terminal has been restored and
// exits 0; a cancelled one prints nothing and exits 1, so a shell script can
// ask its question through it.
#include <cstdio>
#include <cstdlib>
#include <optional>

#include "../example_diagnostics.hpp"
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

#include "rootdialog_app.hpp"

int main() {
    std::optional<ckv::rootdialog::Outcome> outcome;
    const int status = ckv::examples::run_reporting_failure([&outcome] {
        ckv::term::PosixClock clock;
        ckv::term::PosixTerminal terminal(clock);
        const ckv::examples::ExampleDiagnostics diagnostics(clock);
        diagnostics.attach(terminal);
        ckv::term::TerminalClipboardWriter clipboard(terminal);
        ckv::ui::Application app(terminal, clock, clipboard);
        diagnostics.attach(app);
        ckv::rootdialog::RootDialogApp form(app);
        app.run();
        outcome = form.outcome();
    });
    if (status != EXIT_SUCCESS) return status;
    if (!outcome || !outcome->accepted) return EXIT_FAILURE;
    std::printf("name=%s\nemail=%s\n", outcome->name.c_str(), outcome->email.c_str());
    return EXIT_SUCCESS;
}
