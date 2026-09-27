// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Independently launched ckVision child for private PTY/ConPTY integration.
// Windows keeps the frame live until private input reaches a focused field;
// POSIX keeps the initial frame briefly for its existing capture contract.
#include <chrono>
#include <memory>
#include <thread>

#if defined(_WIN32)
#include "cvision/term/windows_clock.hpp"
#include "cvision/term/windows_terminal.hpp"
#include "cvision/widgets/input_line.hpp"
#else
#include "cvision/term/posix_clock.hpp"
#include "cvision/term/posix_terminal.hpp"
#endif
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/label.hpp"

int main() {
    using namespace std::chrono_literals;
#if defined(_WIN32)
    ckv::term::WindowsClock clock;
    ckv::term::WindowsTerminal terminal(clock);
#else
    ckv::term::PosixClock clock;
    ckv::term::PosixTerminal terminal(clock);
#endif
    ckv::ui::Application app(terminal, clock);
    const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
    auto label = std::make_unique<ckv::widgets::Label>("NESTED-CKVISION");
    label->set_fills_root(false);
    label->set_bounds(ckv::Rect{1, 1, 20, 1});
    ckv::widgets::Label* const label_view = label.get();
    app.root().add_child(std::move(label));
#if defined(_WIN32)
    auto input = std::make_unique<ckv::widgets::InputLine>();
    input->set_fills_root(false);
    input->set_bounds(ckv::Rect{1, 3, 20, 1});
    ckv::widgets::InputLine* const input_view = input.get();
    app.root().add_child(std::move(input));
    app.set_focus(input_view);
#else
    (void)label_view;
#endif
    // The child console may initially have a host-dependent zero geometry;
    // let the parent adapter set its size before the first frame.
    std::this_thread::sleep_for(20ms);
    app.step(clock.now_nanos() + 20'000'000);
#if defined(_WIN32)
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    bool acknowledged = false;
    while (std::chrono::steady_clock::now() < deadline) {
        app.step(clock.now_nanos() + 20'000'000);
        const std::string text = input_view->text();
        if (!acknowledged && text == "go") {
            label_view->set_text("NESTED-INPUT-OK");
            acknowledged = true;
        }
        if (acknowledged && text == "goq") return 0;
    }
    return 1;
#else
    std::this_thread::sleep_for(200ms);
    return 0;
#endif
}
