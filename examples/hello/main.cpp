// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// ckVision Hello — a compact visual integration example; see
// docs/hello-example.md for its behavior contract and golden coverage.
#include "../native_host.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

#include "hello_app.hpp"

int main() {
    ckv::examples::NativeClock clock; ckv::examples::NativeTerminal terminal(clock);
    ckv::term::TerminalClipboardWriter clipboard(terminal);
    ckv::ui::Application app(terminal, clock, clipboard); ckv::hello::HelloApp hello(app);
    app.run(); return 0;
}
