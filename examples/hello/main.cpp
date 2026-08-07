// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// ckVision ckVision Hello — a faithful port of Turbo Vision's own canonical
// minimal example ("hello.cpp"), for a golden regression test and a
// worked side-by-side comparison in docs/turbo-vision-conversion.md.
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"
#include "cvision/ui/application.hpp"

#include "hello_app.hpp"

int main() {
    ckv::term::PosixTerminal terminal;
    ckv::term::PosixClock clock;
    ckv::ui::Application app(terminal, clock);
    ckv::hello::HelloApp hello(app);
    app.run();
    return 0;
}
