// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "editor_app.hpp"

#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"

int main() {
    ckv::term::PosixClock clock;
    ckv::term::PosixTerminal terminal(clock);
    ckv::ui::Application application(terminal, clock);
    ckv::editor_example::EditorApp editor(application);
    application.run();
}
