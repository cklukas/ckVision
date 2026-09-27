// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// ckVision File Browser — a master-detail example: a folder-hierarchy
// TreeView drives a ListView showing the selected folder's contents in
// a second pane, browsing the REAL filesystem via PosixFileSystem.
// Answers the "is a tree-driven folder browser supported, and is
// there an example" question directly rather than only in the docs.
#include "../example_diagnostics.hpp"
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_filesystem.hpp"
#include "cvision/term/posix_terminal.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

#include "filebrowser_app.hpp"

int main(int argc, char** argv) {
    const std::string root = argc > 1 ? argv[1] : ".";
    return ckv::examples::run_reporting_failure([&] {
        ckv::term::PosixClock clock;
        ckv::term::PosixTerminal terminal(clock);
        const ckv::examples::ExampleDiagnostics diagnostics(clock);
        diagnostics.attach(terminal);
        ckv::term::TerminalClipboardWriter clipboard(terminal);
        ckv::ui::Application app(terminal, clock, clipboard);
        diagnostics.attach(app);
        ckv::term::PosixFileSystem fs;
        ckv::filebrowser::FileBrowserApp browser(app, fs, root);
        app.run();
    });
}
