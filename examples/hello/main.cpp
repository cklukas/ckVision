// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// ckVision Hello — a compact visual integration example; see
// docs/hello-example.md for its behavior contract and golden coverage.
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"
#include "cvision/ui/application.hpp"

#include "hello_app.hpp"

int main() {
    ckv::term::PosixTerminal terminal; ckv::term::PosixClock clock;
    ckv::ui::Application app(terminal, clock); ckv::hello::HelloApp hello(app);
    app.run(); return 0;
}
