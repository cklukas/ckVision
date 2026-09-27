// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Documentation and verification tooling, NOT part of the cvision library.
//
// The M4 form demo's key behaviour as two event scripts over the shipped
// examples/rootdialog object graph (the roadmap M4 exit): Tab, Shift-Tab,
// Alt+mnemonic and an Enter that validation vetoes, then a corrected form
// that Enter accepts; and a second session that Esc cancels. Played by
// tests/test_rootdialog_smoke.cpp and by generate_event_script_goldens.
#pragma once

#include <vector>

#include "cvision/core/clock.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "event_script.hpp"
#include "rootdialog_app.hpp"

namespace ckv::docgen {

// One run of the demo on an 80x24 headless terminal, ready to play `beats`.
struct RootDialogSession {
    explicit RootDialogSession(std::vector<ScriptBeat> beats);

    term::HeadlessTerminal terminal{Size{80, 24}};
    ManualClock clock;
    ui::Application app{terminal, clock};
    rootdialog::RootDialogApp form{app};
    ScriptPlayer player;
};

// initial, tab_to_email, tab_to_ok, shift_tab_to_email, enter_vetoed,
// name_typed, alt_e_to_email, email_typed, alt_n_to_name, enter_accepted.
std::vector<ScriptBeat> rootdialog_accept_script();
// initial, name_typed, tab_to_email, escape_cancelled.
std::vector<ScriptBeat> rootdialog_cancel_script();

}  // namespace ckv::docgen
