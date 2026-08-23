// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// ckVision SysInfo — the interactive host. The real machine is injected
// here and nowhere else; everything the application does with it is in
// sysinfo_app.cpp, which never learns which probe it was given.
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"
#include "cvision/term/terminal_clipboard.hpp"
#include "cvision/ui/application.hpp"

#include "posix_system_probe.hpp"
#include "sysinfo_app.hpp"

int main() {
    ckv::term::PosixClock clock;
    ckv::term::PosixTerminal terminal(clock);
    ckv::term::TerminalClipboardWriter clipboard(terminal);

    ckv::sysinfo::PosixSystemProbe probe;
    ckv::ui::Application app(terminal, clock, clipboard);
    ckv::sysinfo::SysInfoApp sysinfo(app, probe);
    app.run();
    return 0;
}
