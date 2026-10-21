// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// ckVision File Browser — a master-detail example: a folder-hierarchy
// TreeView drives a ListView showing the selected folder's contents in
// a second pane, browsing the REAL filesystem via PosixFileSystem.
// Answers the "is a tree-driven folder browser supported, and is
// there an example" question directly rather than only in the docs.
#include "../native_host.hpp"
#include "../example_diagnostics.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

#include "filebrowser_app.hpp"

int run_main(const std::vector<std::string>& args) {
    const auto argc = args.size();
    const std::string root = argc > 1 ? args[1].c_str() : ".";
    return ckv::examples::run_reporting_failure([&] {
        ckv::examples::NativeClock clock;
        const ckv::examples::ExampleDiagnostics diagnostics(clock);
        ckv::examples::NativeTerminal terminal(clock);
        diagnostics.attach(terminal);
        ckv::term::TerminalClipboardWriter clipboard(terminal);
        ckv::ui::Application app(terminal, clock, clipboard);
        diagnostics.attach(app);
        ckv::examples::NativeFileSystem fs;
        ckv::filebrowser::FileBrowserApp browser(app, fs, root);
        app.run();
    });
}

#if defined(_WIN32)
int wmain(int argc, wchar_t** argv) {
#else
int main(int argc, char** argv) {
#endif
    return ckv::examples::run_reporting_failure([&] { return run_main(ckv::examples::native_arguments(argc, argv)); });
}
