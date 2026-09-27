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
#include "../example_diagnostics.hpp"

#if defined(_WIN32)
#include "cvision/term/windows_clock.hpp"
#include "cvision/term/windows_terminal.hpp"
#include <cstdio>
#include <cstdlib>
#include <exception>
#else
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"
#endif
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

#include "gallery_app.hpp"

namespace {

void run_example() {
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
    ckv::gallery::GalleryApp gallery(app);
    app.run();
}

}  // namespace

int main() { return ckv::examples::run_reporting_failure(run_example); }
