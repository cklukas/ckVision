// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The menu system operated end to end (the roadmap M5 exit: "menu system
// fully operable by keyboard alone and mouse alone"). Two event scripts over
// one stage with a menu bar, a nested submenu, checkable rows, an unusable
// row and a context menu (tools/docgen/menu_operability_script.hpp): one
// uses nothing but keys, the other nothing but the pointer. Every event goes
// through HeadlessTerminal::inject_event and Application::step; the frames
// are pinned in tests/golden/menu_*.dump, which generate_event_script_goldens
// writes from the same scripts.
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "cvision/testing/cktest.hpp"
#include "cvision/widgets/menu.hpp"
#include "event_script.hpp"
#include "menu_operability_script.hpp"

using ckv::docgen::MenuStage;
using ckv::docgen::ScriptBeat;
using ckv::widgets::DropdownMenu;

namespace {

std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool plays_to_pinned_frame(MenuStage& stage, const char* name) {
    const ScriptBeat* beat = stage.player.play_to(name);
    if (beat == nullptr || beat->golden.empty()) return false;
    const std::string pinned = read_file("golden/" + beat->golden);
    return !pinned.empty() && ckv::docgen::capture_frame(stage.app) == pinned;
}

// The open menus, outermost first: a bar's dropdown or a context menu, then
// the submenu it has open, if any.
std::vector<const DropdownMenu*> open_menus(MenuStage& stage) {
    std::vector<const DropdownMenu*> menus;
    for (const ckv::ui::View* popup : stage.desktop().popups())
        if (const auto* menu = dynamic_cast<const DropdownMenu*>(popup)) menus.push_back(menu);
    return menus;
}

// The label of the row the innermost open menu highlights, or empty.
std::string highlighted_row(MenuStage& stage) {
    const std::vector<const DropdownMenu*> menus = open_menus(stage);
    if (menus.empty()) return {};
    const DropdownMenu& innermost = *menus.back();
    if (innermost.highlighted() < 0) return {};
    return innermost.items()[static_cast<std::size_t>(innermost.highlighted())].label();
}

std::vector<std::string> chosen(std::initializer_list<const char*> rows) {
    return std::vector<std::string>(rows.begin(), rows.end());
}

}  // namespace

CK_TEST(the_menu_system_is_fully_operable_by_the_keyboard_alone) {
    MenuStage stage(ckv::docgen::menu_keyboard_script());
    CK_CHECK(plays_to_pinned_frame(stage, "initial"));
    CK_CHECK(stage.app.focused() == stage.document());

    // F10 walks onto the bar; Down opens the menu under the walk.
    CK_CHECK(stage.player.play_to("f10_bar") != nullptr);
    CK_CHECK(stage.bar().active());
    CK_CHECK(open_menus(stage).empty());
    CK_CHECK(stage.player.play_to("file_opened") != nullptr);
    CK_CHECK(open_menus(stage).size() == 1);
    CK_CHECK(highlighted_row(stage) == "&New");

    // Into the nested submenu and out again.
    CK_CHECK(stage.player.play_to("recent_highlighted") != nullptr);
    CK_CHECK(highlighted_row(stage) == "Open &recent");
    CK_CHECK(plays_to_pinned_frame(stage, "submenu_opened"));
    CK_CHECK(open_menus(stage).size() == 2);
    CK_CHECK(highlighted_row(stage) == "&Alpha");
    CK_CHECK(stage.player.play_to("submenu_down") != nullptr);
    CK_CHECK(highlighted_row(stage) == "&Beta");
    CK_CHECK(stage.player.play_to("submenu_left") != nullptr);
    CK_CHECK(open_menus(stage).size() == 1);
    CK_CHECK(highlighted_row(stage) == "Open &recent");
    CK_CHECK(stage.player.play_to("submenu_reentered") != nullptr);
    CK_CHECK(open_menus(stage).size() == 2);

    // Esc closes one level at a time: the submenu alone first, then the
    // top-level menu, which leaves the walk standing on its title, and then
    // the walk, which leaves the menu system and gives the focus back.
    CK_CHECK(stage.player.play_to("escape_submenu") != nullptr);
    CK_CHECK(open_menus(stage).size() == 1);
    CK_CHECK(highlighted_row(stage) == "Open &recent");
    CK_CHECK(stage.bar().active());
    CK_CHECK(plays_to_pinned_frame(stage, "escape_menu"));
    CK_CHECK(open_menus(stage).empty());
    CK_CHECK(stage.bar().active());
    CK_CHECK(stage.app.input_capture() == nullptr);
    CK_CHECK(stage.player.play_to("escape_walk") != nullptr);
    CK_CHECK(!stage.bar().active());
    CK_CHECK(stage.app.focused() == stage.document());
    // Esc on the bar walk itself, with no menu open, leaves it the same way.
    CK_CHECK(stage.player.play_to("f10_again") != nullptr);
    CK_CHECK(stage.bar().active());
    CK_CHECK(stage.player.play_to("escape_bar") != nullptr);
    CK_CHECK(!stage.bar().active());
    CK_CHECK(stage.app.focused() == stage.document());
    CK_CHECK(stage.chosen().empty());

    // Alt+V opens View directly; Enter toggles the checkable row under the
    // highlight and hands the focus back.
    CK_CHECK(stage.player.play_to("alt_v_opened") != nullptr);
    CK_CHECK(highlighted_row(stage) == "&Wrap lines");
    CK_CHECK(stage.player.play_to("wrap_chosen") != nullptr);
    CK_CHECK(stage.wrap_lines());
    CK_CHECK(stage.chosen() == chosen({"wrap"}));
    CK_CHECK(open_menus(stage).empty());
    CK_CHECK(stage.app.focused() == stage.document());

    // The row that cannot be used is where the arrows stop (D-083): it can
    // be walked onto, the status line says why it cannot be used, and Enter
    // on it does nothing.
    CK_CHECK(stage.player.play_to("view_reopened") != nullptr);
    CK_CHECK(stage.status().current_hint().empty());
    CK_CHECK(plays_to_pinned_frame(stage, "disabled_highlighted"));
    CK_CHECK(highlighted_row(stage) == "&Print preview");
    CK_CHECK(!open_menus(stage).back()->highlight().enabled);
    CK_CHECK(open_menus(stage).back()->highlight().disabled_reason == "No printer is configured.");
    CK_CHECK(stage.status().current_hint() == "No printer is configured.");
    CK_CHECK(stage.player.play_to("disabled_enter") != nullptr);
    CK_CHECK(open_menus(stage).size() == 1);
    CK_CHECK(highlighted_row(stage) == "&Print preview");
    CK_CHECK(stage.chosen() == chosen({"wrap"}));
    CK_CHECK(stage.player.play_to("escape_view") != nullptr);
    CK_CHECK(open_menus(stage).empty());
    CK_CHECK(stage.status().current_hint().empty());  // the reason leaves with its menu
    CK_CHECK(stage.bar().active());
    CK_CHECK(stage.player.play_to("escape_view_walk") != nullptr);
    CK_CHECK(!stage.bar().active());
    CK_CHECK(stage.app.focused() == stage.document());

    // Shift-F10 is the context menu at the focus; Esc takes it away again.
    CK_CHECK(plays_to_pinned_frame(stage, "context_opened"));
    CK_CHECK(open_menus(stage).size() == 1);
    CK_CHECK(highlighted_row(stage) == "Cu&t");
    const ckv::Rect at_focus = open_menus(stage).front()->bounds();
    CK_CHECK(stage.player.play_to("context_copy_chosen") != nullptr);
    CK_CHECK(stage.chosen() == chosen({"wrap", "copy"}));
    CK_CHECK(open_menus(stage).empty());
    CK_CHECK(stage.app.focused() == stage.document());
    // The Menu key opens the same menu at the same place.
    CK_CHECK(stage.player.play_to("context_reopened") != nullptr);
    CK_CHECK(open_menus(stage).size() == 1);
    CK_CHECK(open_menus(stage).front()->bounds() == at_focus);
    CK_CHECK(highlighted_row(stage) == "Cu&t");
    // Its unusable row explains itself on the status line as well.
    CK_CHECK(stage.player.play_to("context_disabled_highlighted") != nullptr);
    CK_CHECK(highlighted_row(stage) == "&Paste");
    CK_CHECK(stage.status().current_hint() == "The clipboard is empty.");
    CK_CHECK(stage.player.play_to("context_escaped") != nullptr);
    CK_CHECK(open_menus(stage).empty());
    CK_CHECK(stage.status().current_hint().empty());
    CK_CHECK(stage.app.input_capture() == nullptr);
    CK_CHECK(stage.app.focused() == stage.document());
    CK_CHECK(stage.chosen() == chosen({"wrap", "copy"}));
}

CK_TEST(the_menu_system_is_fully_operable_by_the_mouse_alone) {
    MenuStage stage(ckv::docgen::menu_mouse_script());
    CK_CHECK(plays_to_pinned_frame(stage, "initial"));

    // Press on File, drag along the bar to View and down onto a row: the
    // open menu follows the pointer, and the release chooses the row.
    CK_CHECK(stage.player.play_to("bar_pressed") != nullptr);
    CK_CHECK(open_menus(stage).size() == 1);
    CK_CHECK(open_menus(stage).front()->items().front().label() == "&New");
    CK_CHECK(stage.player.play_to("dragged_to_view") != nullptr);
    CK_CHECK(open_menus(stage).size() == 1);
    CK_CHECK(open_menus(stage).front()->items().front().label() == "&Wrap lines");
    CK_CHECK(plays_to_pinned_frame(stage, "dragged_to_wrap"));
    CK_CHECK(highlighted_row(stage) == "&Wrap lines");
    CK_CHECK(stage.chosen().empty());
    CK_CHECK(stage.player.play_to("released_on_wrap") != nullptr);
    CK_CHECK(stage.wrap_lines());
    CK_CHECK(stage.chosen() == chosen({"wrap"}));
    CK_CHECK(open_menus(stage).empty());

    // A click opens File and leaves it open; hovering the submenu row opens
    // the submenu, and a click inside it chooses and closes the chain.
    CK_CHECK(stage.player.play_to("file_clicked") != nullptr);
    CK_CHECK(open_menus(stage).size() == 1);
    CK_CHECK(plays_to_pinned_frame(stage, "recent_hovered"));
    CK_CHECK(open_menus(stage).size() == 2);
    CK_CHECK(stage.player.play_to("alpha_hovered") != nullptr);
    CK_CHECK(highlighted_row(stage) == "&Alpha");
    CK_CHECK(stage.player.play_to("alpha_clicked") != nullptr);
    CK_CHECK(stage.chosen() == chosen({"wrap", "alpha"}));
    CK_CHECK(open_menus(stage).empty());
    CK_CHECK(stage.app.input_capture() == nullptr);

    // A right click opens the context menu where the pointer is; a click
    // anywhere else takes it away without choosing anything.
    CK_CHECK(plays_to_pinned_frame(stage, "context_opened"));
    CK_CHECK(open_menus(stage).size() == 1);
    CK_CHECK(open_menus(stage).front()->bounds().x == MenuStage::kDocumentCell.x);
    CK_CHECK(open_menus(stage).front()->bounds().y == MenuStage::kDocumentCell.y);
    CK_CHECK(plays_to_pinned_frame(stage, "outside_dismissed"));
    CK_CHECK(open_menus(stage).empty());
    CK_CHECK(stage.app.input_capture() == nullptr);
    CK_CHECK(stage.chosen() == chosen({"wrap", "alpha"}));
    CK_CHECK(stage.app.focused() == stage.document());
}
