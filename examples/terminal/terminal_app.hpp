// Copyright (c) 2026 C. Klukas. All rights reserved.
#pragma once

#include <cstddef>
#include <string>

#include "cvision/term/terminal_subsession.hpp"
#include "cvision/ui/application.hpp"

namespace ckv::widgets {
class Desktop;
class Window;
}  // namespace ckv::widgets

namespace ckv::terminal_example {

class TerminalApp {
public:
    inline static constexpr ui::CommandId kNewTerminalCommand = ui::kFirstApplicationCommandId;
    inline static constexpr ui::CommandId kNewSixelDemoCommand = ui::kFirstApplicationCommandId + 1;

    explicit TerminalApp(ui::Application& app);

    // Opens an independent interactive shell in a new modeless desktop window
    // and makes that shell the keyboard focus.
    widgets::Window* new_terminal();

    // Opens an interactive child whose first output is a bounded Sixel sample.
    // This makes the containment/degradation teaching path directly observable
    // without giving the child any access to the outer terminal.
    widgets::Window* new_sixel_demo();

    widgets::Desktop& desktop() noexcept { return *desktop_; }

private:
    widgets::Window* open_terminal(term::TerminalLaunchSpec launch, std::string title);

    ui::Application& app_;
    widgets::Desktop* desktop_ = nullptr;
    std::size_t next_terminal_number_ = 1;
};

}  // namespace ckv::terminal_example
