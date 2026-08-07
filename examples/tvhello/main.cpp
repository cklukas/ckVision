// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// ckVision TV-Hello — a faithful port of Turbo Vision's own canonical
// minimal example ("hello.cpp"), for a golden regression test and a
// worked side-by-side comparison in docs/turbo-vision-conversion.md.
#include <csignal>

#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"
#include "cvision/ui/application.hpp"

#include "tvhello_app.hpp"

namespace {
volatile std::sig_atomic_t g_interrupted = 0;
void on_sigint(int) { g_interrupted = 1; }
}  // namespace

int main() {
    std::signal(SIGINT, on_sigint);

    ckv::term::PosixTerminal terminal;
    ckv::term::PosixClock clock;
    ckv::ui::Application app(terminal, clock);
    ckv::tvhello::TvHelloApp tvhello(app);

    while (!g_interrupted && !tvhello.should_quit()) {
        const std::int64_t deadline = ckv::term::PosixTerminal::monotonic_now_nanos() + 200'000'000;
        app.step(deadline);
    }
    return 0;
}
