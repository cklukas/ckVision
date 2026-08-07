// Copyright (c) 2026 C. Klukas. All rights reserved.
//
// ckVision Gallery — a real, runnable demonstration application
// (ROADMAP.md M8 "examples finalized"), inspired in spirit by
// tvision-sixel's tvdemo/sixeldemo/tvforms examples: a desktop with a
// menu bar, a status line, movable/resizable shadowed windows, a
// controls form (label, input line, buttons), and an image view
// proving the Sixel graphics path end to end. Every piece here is
// exercised headlessly (byte-for-byte) by
// tests/test_gallery_smoke.cpp — this file is the interactive shell
// around exactly that same, already-tested object graph.
#include <csignal>
#include <cstdio>

#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/image_view.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/status_line.hpp"
#include "cvision/widgets/window.hpp"

#include "gallery_app.hpp"

namespace {
volatile std::sig_atomic_t g_interrupted = 0;
void on_sigint(int) { g_interrupted = 1; }
}  // namespace

int main() {
    std::signal(SIGINT, on_sigint);

    ckv::term::PosixTerminal terminal;
    ckv::term::PosixClock clock;
    ckv::ui::Application app(terminal, clock);

    ckv::gallery::GalleryApp gallery(app);

    while (!g_interrupted && !gallery.should_quit()) {
        const std::int64_t deadline = ckv::term::PosixTerminal::monotonic_now_nanos() + 200'000'000;
        app.step(deadline);
    }
    return 0;
}
