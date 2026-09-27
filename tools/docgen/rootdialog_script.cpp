// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "rootdialog_script.hpp"

#include <utility>

namespace ckv::docgen {

RootDialogSession::RootDialogSession(std::vector<ScriptBeat> beats)
    : player(terminal, app, std::move(beats)) {}

std::vector<ScriptBeat> rootdialog_accept_script() {
    return {
        {"initial", {}, "rootdialog_initial.dump"},
        {"tab_to_email", {key(Key::Tab)}, ""},
        {"tab_to_ok", {key(Key::Tab)}, ""},
        {"shift_tab_to_email", {key(Key::Tab, Modifier::Shift)}, ""},
        // Both fields are empty: the name's validator refuses first, so the
        // name field is marked and takes the focus, and the form stays.
        {"enter_vetoed", {key(Key::Enter)}, "rootdialog_veto.dump"},
        {"name_typed", {typed("Ada")}, ""},
        {"alt_e_to_email", {alt("e")}, ""},
        {"email_typed", {typed("ada@example.org")}, ""},
        {"alt_n_to_name", {alt("n")}, "rootdialog_filled.dump"},
        {"enter_accepted", {key(Key::Enter)}, "rootdialog_accepted.dump"},
    };
}

std::vector<ScriptBeat> rootdialog_cancel_script() {
    return {
        {"initial", {}, ""},
        {"name_typed", {typed("Grace")}, ""},
        {"tab_to_email", {key(Key::Tab)}, ""},
        // Esc bypasses validation: the empty email is never judged.
        {"escape_cancelled", {key(Key::Escape)}, "rootdialog_cancelled.dump"},
    };
}

}  // namespace ckv::docgen
