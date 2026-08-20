// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "cvision/term/terminal_subsession.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/widgets/common_components.hpp"

namespace ckv::widgets {
class Desktop;
class Window;
}  // namespace ckv::widgets

namespace ckv::terminal_example {

class TerminalApp {
public:
    // The keys this example's own commands are declared under. A caller
    // that wants one — a script, a test, a menu definition — names it by
    // key and resolves the id through the registry, exactly as it would
    // for any command declared by any other party.
    static constexpr std::string_view kNewTerminalKey = "terminal.new-terminal";
    static constexpr std::string_view kNewSixelDemoKey = "terminal.new-sixel-demo";

    explicit TerminalApp(ui::Application& app);

    // The ids the registry assigned to the two commands above, for
    // callers that already hold this object.
    ui::CommandId new_terminal_command() const noexcept { return new_terminal_command_; }
    ui::CommandId new_sixel_demo_command() const noexcept { return new_sixel_demo_command_; }

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
    ui::CommandId new_terminal_command_ = ui::kInvalidCommand;
    ui::CommandId new_sixel_demo_command_ = ui::kInvalidCommand;
    widgets::Desktop* desktop_ = nullptr;
    widgets::ClockView* clock_ = nullptr;
    void open_calendar();
    std::size_t next_terminal_number_ = 1;
};

}  // namespace ckv::terminal_example
