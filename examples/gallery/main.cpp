// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// ckVision Gallery — a real, runnable demonstration application
// (the roadmap M8 "examples finalized"), inspired in spirit by
// tvision-sixel's tvdemo/sixeldemo/tvforms examples: a desktop with a
// menu bar, a status line, movable/resizable shadowed windows, a
// controls form (label, input line, buttons), and an image view
// proving the Sixel graphics path end to end. Every piece here is
// exercised headlessly (byte-for-byte) by
// tests/test_gallery_smoke.cpp — this file is the interactive shell
// around exactly that same, already-tested object graph.
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

#include "gallery_app.hpp"

int main() {
    ckv::term::PosixClock clock;
    ckv::term::PosixTerminal terminal(clock);
    ckv::term::TerminalClipboardWriter clipboard(terminal);
    ckv::ui::Application app(terminal, clock, clipboard);
    ckv::gallery::GalleryApp gallery(app);
    app.run();
    return 0;
}
