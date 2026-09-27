// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Documentation and verification tooling, NOT part of the cvision library.
//
// Close-veto event scripts (the roadmap M5 exit: "close-veto (including quit
// sweep) event scripts green"). Every close path is started by terminal input
// against a window that refuses to close -- a Draft with unsaved changes, and
// a modal review dialog that has not been ticked off -- and each script then
// removes the reason (Alt+S saves the draft; Space ticks the review) and
// takes the same path again, so the same input is seen to close once nothing
// refuses. The close control and the Close chord are two routes to one
// request, and both end on the one pinned refusal frame. Played by
// tests/test_close_veto_scripts.cpp and by generate_event_script_goldens.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "cvision/core/clock.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/dialog.hpp"
#include "event_script.hpp"

namespace ckv::widgets {
class Button;
class StaticText;
class Window;
}  // namespace ckv::widgets

namespace ckv::docgen {

// A 60x20 desktop holding a Notes window that closes whenever asked and, in
// front of it, a Draft window that refuses every close until it is saved.
// F2 opens a modal Review dialog over them that refuses to close until its
// "Reviewed" box is ticked.
class CloseVetoStage {
public:
    explicit CloseVetoStage(std::vector<ScriptBeat> beats);

    term::HeadlessTerminal terminal{Size{60, 20}};
    ManualClock clock;
    ui::Application app{terminal, clock};
    ScriptPlayer player;

    widgets::Desktop& desktop() noexcept { return *desktop_; }
    widgets::Window* notes() const noexcept { return notes_; }
    widgets::Window* draft() const noexcept { return draft_; }
    widgets::Button* save_button() const noexcept { return save_button_; }
    // The open Review dialog, or nullptr.
    widgets::Window* review() const noexcept { return review_; }
    int draft_refusals() const noexcept { return draft_refusals_; }
    int review_refusals() const noexcept { return review_refusals_; }

    // Where the Draft's close control is, for the click that asks it to close.
    static constexpr Point kDraftCloseControl{6, 5};

private:
    void open_review();

    ui::StandardRoles roles_;
    widgets::Desktop* desktop_ = nullptr;
    widgets::Window* notes_ = nullptr;
    widgets::Window* draft_ = nullptr;
    widgets::StaticText* draft_status_ = nullptr;
    widgets::Button* save_button_ = nullptr;
    bool saved_ = false;
    int draft_refusals_ = 0;
    widgets::DialogDescriptor review_descriptor_;
    widgets::Window* review_ = nullptr;
    widgets::Label* review_note_ = nullptr;
    widgets::CheckGroup* reviewed_ = nullptr;
    int review_refusals_ = 0;
};

// initial, control_refused, saved, control_closed.
std::vector<ScriptBeat> close_control_script();
// initial, chord_refused, saved, chord_closed.
std::vector<ScriptBeat> close_chord_script();
// initial, review_opened, escape_refused, reviewed_ticked, escape_closed.
std::vector<ScriptBeat> dialog_escape_script();
// initial, notes_raised, sweep_refused, saved, sweep_quit.
std::vector<ScriptBeat> quit_sweep_script();

}  // namespace ckv::docgen
