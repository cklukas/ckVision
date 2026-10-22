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
#include "cvision/core/version.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

#include <iostream>
#include <string_view>

#include "gallery_app.hpp"

namespace {

void run_example() {
    ckv::examples::NativeClock clock;
    const ckv::examples::ExampleDiagnostics diagnostics(clock);
    ckv::examples::NativeTerminal terminal(clock);
    diagnostics.attach(terminal);
    ckv::term::TerminalClipboardWriter clipboard(terminal);
    ckv::ui::Application app(terminal, clock, clipboard);
    diagnostics.attach(app);
    ckv::gallery::GalleryApp gallery(app);
    app.run();
}

}  // namespace

// `--version` is the release packages' smoke test: it runs without a terminal
// session and names the installed ckVision release.
int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--version") {
        std::cout << "ckvision_gallery " << ckv::version_string() << '\n';
        return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        std::cout << "Usage: ckvision_gallery [--version | --help]\n";
        return 0;
    }
    if (argc != 1) {
        std::cerr << "Usage: ckvision_gallery [--version | --help]\n";
        return 2;
    }
    return ckv::examples::run_reporting_failure(run_example);
}
