// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Close-veto event scripts (the roadmap M5 exit): each close path started by
// terminal input -- the close control clicked, the Close command's chord
// (Alt+F3), Esc on a modal dialog, and the quit sweep's chord (Alt+X) --
// against a window that refuses, asserting the window count, modality and
// the pinned frame; then the reason for the refusal is removed by input and
// the same path is taken again, and this time closes. The scripts and their
// stage live in tools/docgen/close_veto_script.hpp, shared with the
// generator that writes tests/golden/close_veto_*.dump.
#include <fstream>
#include <sstream>
#include <string>

#include "close_veto_script.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/window.hpp"
#include "event_script.hpp"

using ckv::docgen::CloseVetoStage;
using ckv::docgen::ScriptBeat;

namespace {

std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool plays_to_pinned_frame(CloseVetoStage& stage, const char* name) {
    const ScriptBeat* beat = stage.player.play_to(name);
    if (beat == nullptr || beat->golden.empty()) return false;
    const std::string pinned = read_file("golden/" + beat->golden);
    return !pinned.empty() && ckv::docgen::capture_frame(stage.app) == pinned;
}

bool inside(const ckv::ui::View& ancestor, const ckv::ui::View* view) {
    for (; view != nullptr; view = view->parent())
        if (view == &ancestor) return true;
    return false;
}

}  // namespace

CK_TEST(clicking_the_close_control_of_a_vetoing_window_keeps_it_open_until_it_is_saved) {
    CloseVetoStage stage(ckv::docgen::close_control_script());
    CK_CHECK(plays_to_pinned_frame(stage, "initial"));
    CK_CHECK(stage.desktop().windows().size() == 2);
    CK_CHECK(stage.desktop().active_window() == stage.draft());

    CK_CHECK(plays_to_pinned_frame(stage, "control_refused"));
    CK_CHECK(stage.draft_refusals() == 1);
    CK_CHECK(stage.desktop().windows().size() == 2);
    CK_CHECK(stage.desktop().active_window() == stage.draft());
    CK_CHECK(!stage.app.is_modal());

    CK_CHECK(stage.player.play_to("saved") != nullptr);
    CK_CHECK(stage.player.play_to("control_closed") != nullptr);
    CK_CHECK(stage.draft() == nullptr);
    CK_CHECK(stage.draft_refusals() == 1);
    CK_CHECK(stage.desktop().windows().size() == 1);
    CK_CHECK(stage.desktop().active_window() == stage.notes());
    CK_CHECK(!stage.app.quit_requested());
}

CK_TEST(the_close_chord_against_a_vetoing_window_ends_on_the_same_refusal_as_the_control) {
    CloseVetoStage stage(ckv::docgen::close_chord_script());
    CK_CHECK(plays_to_pinned_frame(stage, "chord_refused"));
    CK_CHECK(stage.draft_refusals() == 1);
    CK_CHECK(stage.desktop().windows().size() == 2);
    CK_CHECK(stage.desktop().active_window() == stage.draft());
    CK_CHECK(!stage.app.is_modal());

    CK_CHECK(stage.player.play_to("saved") != nullptr);
    CK_CHECK(stage.player.play_to("chord_closed") != nullptr);
    CK_CHECK(stage.draft() == nullptr);
    CK_CHECK(stage.desktop().windows().size() == 1);
}

CK_TEST(escape_on_a_vetoing_modal_dialog_keeps_it_open_and_modal_until_its_reason_is_gone) {
    CloseVetoStage stage(ckv::docgen::dialog_escape_script());
    CK_CHECK(plays_to_pinned_frame(stage, "review_opened"));
    CK_CHECK(stage.review() != nullptr);
    CK_CHECK(stage.desktop().windows().size() == 3);
    CK_CHECK(stage.app.is_modal());
    CK_CHECK(stage.app.is_modal_root(*stage.review()));

    CK_CHECK(plays_to_pinned_frame(stage, "escape_refused"));
    CK_CHECK(stage.review_refusals() == 1);
    CK_CHECK(stage.review() != nullptr);
    CK_CHECK(stage.desktop().windows().size() == 3);
    CK_CHECK(stage.app.is_modal_root(*stage.review()));
    CK_CHECK(inside(*stage.review(), stage.app.focused()));

    CK_CHECK(stage.player.play_to("reviewed_ticked") != nullptr);
    CK_CHECK(stage.review() != nullptr);
    CK_CHECK(stage.player.play_to("escape_closed") != nullptr);
    CK_CHECK(stage.review() == nullptr);
    CK_CHECK(stage.review_refusals() == 1);
    CK_CHECK(stage.desktop().windows().size() == 2);
    CK_CHECK(!stage.app.is_modal());
    // The focus the dialog took goes back where it came from.
    CK_CHECK(stage.app.focused() == stage.save_button());
}

CK_TEST(the_quit_sweep_chord_closes_front_to_back_and_stops_at_the_vetoing_window) {
    CloseVetoStage stage(ckv::docgen::quit_sweep_script());
    CK_CHECK(stage.player.play_to("notes_raised") != nullptr);
    CK_CHECK(stage.desktop().active_window() == stage.notes());

    // Notes is in front and closes; the Draft behind it refuses, which ends
    // the sweep and cancels the quit.
    CK_CHECK(plays_to_pinned_frame(stage, "sweep_refused"));
    CK_CHECK(stage.notes() == nullptr);
    CK_CHECK(stage.draft_refusals() == 1);
    CK_CHECK(stage.desktop().windows().size() == 1);
    CK_CHECK(stage.desktop().active_window() == stage.draft());
    CK_CHECK(!stage.app.is_modal());
    CK_CHECK(!stage.app.quit_requested());

    CK_CHECK(stage.player.play_to("saved") != nullptr);
    CK_CHECK(stage.player.play_to("sweep_quit") != nullptr);
    CK_CHECK(stage.draft() == nullptr);
    CK_CHECK(stage.desktop().windows().empty());
    CK_CHECK(stage.app.quit_requested());
}

CK_TEST(the_quit_sweep_chord_is_scoped_out_while_the_vetoing_dialog_is_modal) {
    CloseVetoStage stage({{"initial", {}, ""},
                          {"review_opened", {ckv::docgen::key(ckv::Key::F2)}, ""},
                          {"quit_chord", {ckv::docgen::alt("x")}, ""}});
    CK_CHECK(stage.player.play_to("quit_chord") != nullptr);
    CK_CHECK(stage.review() != nullptr);
    CK_CHECK(stage.review_refusals() == 0);
    CK_CHECK(stage.draft_refusals() == 0);
    CK_CHECK(stage.desktop().windows().size() == 3);
    CK_CHECK(stage.app.is_modal());
    CK_CHECK(!stage.app.quit_requested());
}
