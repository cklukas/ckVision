// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "../example_diagnostics.hpp"
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

#include "graphics_app.hpp"

int main() {
    return ckv::examples::run_reporting_failure([] {
        ckv::term::PosixClock clock;
        ckv::term::PosixTerminal terminal(clock);
        const ckv::examples::ExampleDiagnostics diagnostics(clock);
        diagnostics.attach(terminal);
        ckv::term::TerminalClipboardWriter clipboard(terminal);
        ckv::ui::Application app(terminal, clock, clipboard);
        diagnostics.attach(app);
        ckv::graphics::GraphicsApp graphics(app);
        app.run();
    });
}
