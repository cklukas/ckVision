// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
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
#if defined(_WIN32)
    ckv::term::WindowsClock clock;
    ckv::term::WindowsTerminal terminal(clock);
#else
    ckv::term::PosixClock clock;
    ckv::term::PosixTerminal terminal(clock);
#endif
    ckv::ui::Application application(terminal, clock);
    ckv::editor_example::EditorApp editor(application);
    application.run();
}

}  // namespace

int main() {
#if defined(_WIN32)
    try {
        run_example();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return EXIT_FAILURE;
    }
#else
    run_example();
#endif
    return 0;
}
