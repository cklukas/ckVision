// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Independently launched ckVision child used by the private-PTY integration
// fixture.  It leaves one composed frame live long enough for the parent
// session to observe the nested application's alternate screen before normal
// terminal restoration on exit.
#include <chrono>
#include <memory>
#include <thread>

#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/label.hpp"

int main() {
    using namespace std::chrono_literals;
    ckv::term::PosixClock clock;
    ckv::term::PosixTerminal terminal(clock);
    ckv::ui::Application app(terminal, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
    auto label = std::make_unique<ckv::widgets::Label>("NESTED-CKVISION");
    label->set_fills_root(false);
    label->set_bounds(ckv::Rect{1, 1, 20, 1});
    app.root().add_child(std::move(label));
    // forkpty starts with a host-dependent zero geometry on some systems;
    // wait for the parent adapter's TIOCSWINSZ before the initial frame.
    std::this_thread::sleep_for(20ms);
    app.step(clock.now_nanos() + 20'000'000);
    std::this_thread::sleep_for(200ms);
    return 0;
}
