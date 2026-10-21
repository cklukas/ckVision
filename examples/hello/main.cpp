// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// ckVision Hello — a compact visual integration example; see
// docs/hello-example.md for its behavior contract and golden coverage.
#include "../example_diagnostics.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

#include "hello_app.hpp"

int main() {
    ckv::examples::NativeClock clock;
    const ckv::examples::ExampleDiagnostics diagnostics(clock);
    ckv::examples::NativeTerminal terminal(clock);
    diagnostics.attach(terminal);
    ckv::term::TerminalClipboardWriter clipboard(terminal);
    ckv::ui::Application app(terminal, clock, clipboard);
    diagnostics.attach(app);
    ckv::hello::HelloApp hello(app);
    app.run(); return 0;
}
