// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The M4 form demo (examples/rootdialog) driven headlessly, and its key
// behaviour as one scripted scenario (the roadmap M4 exit): a materialized
// descriptor dialog mounted as the Application root with no Desktop or
// Window, answering Tab, Shift-Tab, Alt+mnemonic, Enter (accept, vetoed by
// validation until the fields are right) and Esc (cancel, never validated).
// Every key arrives through HeadlessTerminal::inject_event and
// Application::step (tools/docgen/rootdialog_script.hpp); the frames are
// pinned in tests/golden/rootdialog_*.dump, which
// generate_event_script_goldens writes from the same script.
#include <fstream>
#include <sstream>
#include <string>

#include "cvision/testing/cktest.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/static_text.hpp"
#include "event_script.hpp"
#include "rootdialog_script.hpp"

using ckv::docgen::RootDialogSession;
using ckv::docgen::ScriptBeat;
using ckv::rootdialog::Outcome;

namespace {

std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

// Plays to `name` and reports whether the frame it ends on is its golden.
bool plays_to_pinned_frame(RootDialogSession& session, const char* name) {
    const ScriptBeat* beat = session.player.play_to(name);
    if (beat == nullptr || beat->golden.empty()) return false;
    const std::string pinned = read_file("golden/" + beat->golden);
    return !pinned.empty() && ckv::docgen::capture_frame(session.app) == pinned;
}

}  // namespace

CK_TEST(the_root_dialog_demo_is_an_undecorated_root_surface_with_the_four_m4_widgets) {
    RootDialogSession session(ckv::docgen::rootdialog_accept_script());
    // The one child of the root is the form surface itself: no Desktop, no
    // Window, so no frame and no chrome anywhere on screen.
    CK_CHECK(session.app.root().children().size() == 1);
    CK_CHECK(session.form.intro() != nullptr);
    CK_CHECK(session.form.intro()->text().find("no window") != std::string::npos);
    CK_CHECK(session.form.name_label()->text() == "&Name:");
    CK_CHECK(session.form.name_label()->buddy() == session.form.name_input());
    CK_CHECK(session.form.email_label()->buddy() == session.form.email_input());
    CK_CHECK(session.form.ok_button()->is_default());
    CK_CHECK(plays_to_pinned_frame(session, "initial"));
    CK_CHECK(session.app.focused() == session.form.name_input());
}

CK_TEST(tab_shift_tab_mnemonics_enter_veto_and_accept_run_as_one_scripted_session) {
    RootDialogSession session(ckv::docgen::rootdialog_accept_script());
    CK_CHECK(plays_to_pinned_frame(session, "initial"));

    // Tab and Shift-Tab: the Application's default keymap, over the root
    // tree in declaration order -- fields, then the button row.
    CK_CHECK(session.player.play_to("tab_to_email") != nullptr);
    CK_CHECK(session.app.focused() == session.form.email_input());
    CK_CHECK(session.player.play_to("tab_to_ok") != nullptr);
    CK_CHECK(session.app.focused() == session.form.ok_button());
    CK_CHECK(session.player.play_to("shift_tab_to_email") != nullptr);
    CK_CHECK(session.app.focused() == session.form.email_input());

    // Enter from a field is the dialog's accept. Both fields are empty, so
    // validation vetoes: the form stays, both fields are marked, and the
    // first invalid one takes the focus.
    CK_CHECK(plays_to_pinned_frame(session, "enter_vetoed"));
    CK_CHECK(session.form.vetoes() == 1);
    CK_CHECK(!session.form.outcome().has_value());
    CK_CHECK(!session.app.quit_requested());
    CK_CHECK(!session.form.name_input()->valid());
    CK_CHECK(!session.form.email_input()->valid());
    CK_CHECK(session.app.focused() == session.form.name_input());
    CK_CHECK(session.form.status()->text() == "A name is required.");

    // Typing answers the veto for that field; Alt+E and Alt+N reach the
    // fields through their labels' mnemonics.
    CK_CHECK(session.player.play_to("name_typed") != nullptr);
    CK_CHECK(session.form.name_input()->text() == "Ada");
    CK_CHECK(session.form.name_input()->valid());
    CK_CHECK(session.player.play_to("alt_e_to_email") != nullptr);
    CK_CHECK(session.app.focused() == session.form.email_input());
    CK_CHECK(session.player.play_to("email_typed") != nullptr);
    CK_CHECK(session.form.email_input()->text() == "ada@example.org");
    CK_CHECK(plays_to_pinned_frame(session, "alt_n_to_name"));
    CK_CHECK(session.app.focused() == session.form.name_input());
    CK_CHECK(session.form.vetoes() == 1);

    // Enter again: every validator passes, the form completes with its
    // answers and asks the Application to quit.
    CK_CHECK(plays_to_pinned_frame(session, "enter_accepted"));
    CK_CHECK(session.form.outcome() == (Outcome{true, "Ada", "ada@example.org"}));
    CK_CHECK(session.form.vetoes() == 1);
    CK_CHECK(session.app.quit_requested());
}

CK_TEST(escape_cancels_the_root_dialog_without_validating_or_keeping_answers) {
    RootDialogSession session(ckv::docgen::rootdialog_cancel_script());
    CK_CHECK(session.player.play_to("name_typed") != nullptr);
    CK_CHECK(session.player.play_to("tab_to_email") != nullptr);
    CK_CHECK(session.app.focused() == session.form.email_input());

    CK_CHECK(plays_to_pinned_frame(session, "escape_cancelled"));
    CK_CHECK(session.form.outcome() == Outcome{});
    CK_CHECK(session.form.vetoes() == 0);
    // Never judged: an empty email is not marked, because Esc never asks.
    CK_CHECK(session.form.email_input()->valid());
    CK_CHECK(session.app.quit_requested());
}

CK_TEST(alt_o_presses_the_accept_button_through_its_mnemonic_and_is_vetoed_like_enter) {
    RootDialogSession session({{"initial", {}, ""}, {"alt_o", {ckv::docgen::alt("o")}, ""}});
    CK_CHECK(session.player.play_to("alt_o") != nullptr);
    CK_CHECK(session.form.vetoes() == 1);
    CK_CHECK(!session.form.outcome().has_value());
    CK_CHECK(session.app.focused() == session.form.name_input());
}

CK_TEST(the_about_button_shows_the_project_copyright_in_place) {
    RootDialogSession session({{"initial", {}, ""}, {"alt_a", {ckv::docgen::alt("a")}, ""}});
    CK_CHECK(session.player.play_to("alt_a") != nullptr);
    const std::string notice = "Copyright (c) 2026 C. Klukas. All rights reserved.";
    CK_CHECK(session.form.intro()->text().find(notice) != std::string::npos);
    CK_CHECK(!session.form.outcome().has_value());
}

CK_TEST(alt_x_leaves_the_root_dialog_as_esc_does) {
    RootDialogSession session({{"initial", {}, ""}, {"alt_x", {ckv::docgen::alt("x")}, ""}});
    CK_CHECK(session.player.play_to("alt_x") != nullptr);
    CK_CHECK(session.form.outcome() == Outcome{});
    CK_CHECK(session.app.quit_requested());
}
