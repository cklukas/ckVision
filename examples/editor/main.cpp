// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "../example_diagnostics.hpp"
#include "editor_app.hpp"

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

namespace {

void run_example() {
    ckv::examples::NativeClock clock;
    const ckv::examples::ExampleDiagnostics diagnostics(clock);
    ckv::examples::NativeTerminal terminal(clock);
    diagnostics.attach(terminal);
    ckv::ui::Application application(terminal, clock);
    diagnostics.attach(application);
    ckv::editor_example::EditorApp editor(application);
    application.run();
}

}  // namespace

int main() { return ckv::examples::run_reporting_failure(run_example); }
