// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cstdlib>
#include <string>

#include "../example_diagnostics.hpp"
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_filesystem.hpp"
#include "cvision/term/posix_terminal.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

#include "workbench_app.hpp"

namespace {

// The edited theme lives beside the TODO example's data, under the reader's
// home directory; the example reads the environment here, in main, so the
// application itself only ever sees the injected file system and a path.
std::string theme_path() {
    const char* const home = std::getenv("HOME");
    const std::string directory = home != nullptr && *home != '\0' ? std::string(home) : std::string("/");
    return directory + "/.ckvision/workbench.theme";
}

}  // namespace

int main() {
    return ckv::examples::run_reporting_failure([] {
        ckv::term::PosixClock clock;
        ckv::term::PosixTerminal terminal(clock);
        const ckv::examples::ExampleDiagnostics diagnostics(clock);
        diagnostics.attach(terminal);
        ckv::term::TerminalClipboardWriter clipboard(terminal);
        ckv::term::PosixFileSystem files;
        ckv::ui::Application app(terminal, clock, clipboard);
        diagnostics.attach(app);
        ckv::workbench::WorkbenchApp workbench(app, {&files, theme_path()});
        app.run();
    });
}
