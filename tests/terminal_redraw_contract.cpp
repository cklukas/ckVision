// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Contract: a private-terminal state change invalidates a TerminalView even
// when it is attached through the normal application view tree.  This guards
// against retained content remaining blank until unrelated chrome repaints.
#include <memory>
#include <string_view>

#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/terminal_emulator.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/widgets/terminal_view.hpp"

int main() {
    ckv::term::HeadlessTerminal terminal(ckv::Size{20, 6});
    ckv::ManualClock clock;
    ckv::ui::Application app(terminal, clock);
    ckv::term::TerminalCapabilityProfile profile = ckv::term::embedded_xterm_sixel_profile();
    profile.cells = ckv::Size{10, 2};
    ckv::term::TerminalEmulator session(profile);
    auto view = std::make_unique<ckv::widgets::TerminalView>(session);
    view->set_fills_root(false);
    view->set_bounds(ckv::Rect{1, 1, 10, 2});
    app.root().add_child(std::move(view));
    app.step(0);
    terminal.clear_written();

    session.feed_output("wake");
    app.root().notify_terminal_subsession_changed(session);
    app.step(0);
    return terminal.written_bytes().find("wake") == std::string_view::npos ? 1 : 0;
}
