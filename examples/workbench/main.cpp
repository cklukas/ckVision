// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "../native_host.hpp"
#include <cstdlib>
#include <string>

#include "../example_diagnostics.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

#include "workbench_app.hpp"

namespace {

// The edited theme lives beside the TODO example's data, under the reader's
// home directory; the example reads the environment here, in main, so the
// application itself only ever sees the injected file system and a path.
std::string theme_path() {
    const auto home = ckv::examples::current_user().home;
    const auto directory = home.empty() ? std::string(".") : home;
    return directory + "/.ckvision/workbench.theme";
}

}  // namespace

int main() {
    return ckv::examples::run_reporting_failure([] {
        ckv::examples::NativeClock clock;
        const ckv::examples::ExampleDiagnostics diagnostics(clock);
        ckv::examples::NativeTerminal terminal(clock);
        diagnostics.attach(terminal);
        ckv::term::TerminalClipboardWriter clipboard(terminal);
        ckv::examples::NativeFileSystem files;
        ckv::ui::Application app(terminal, clock, clipboard);
        diagnostics.attach(app);
        ckv::workbench::WorkbenchApp workbench(app, {&files, theme_path()});
        app.run();
    });
}
