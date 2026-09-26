// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// VISION 2 on a shipped example: the Editor's menu system operated end to
// end by the keyboard alone and by the mouse alone. Two event scripts over
// the example exactly as a reader runs it (tools/docgen/editor_menu_script.hpp):
// a nested submenu, rows that cannot act, a checkable row, the editor's
// context menu, Esc one level at a time, and the light dismiss. Every event
// goes through HeadlessTerminal::inject_event and Application::step; the
// frames are pinned in tests/golden/editor_menu_*.dump, which
// generate_editor_goldens writes from the same scripts.
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

#include "cvision/testing/cktest.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/editor_document.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/text_editor.hpp"
#include "cvision/widgets/window.hpp"
#include "editor_menu_script.hpp"
#include "event_script.hpp"

using ckv::docgen::EditorMenuStage;
using ckv::docgen::ScriptBeat;
using ckv::widgets::DropdownMenu;
using ckv::widgets::MenuBar;

namespace {

std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool plays_to_pinned_frame(EditorMenuStage& stage, const char* name) {
    const ScriptBeat* beat = stage.player.play_to(name);
    if (beat == nullptr || beat->golden.empty()) return false;
    const std::string pinned = read_file("golden/" + beat->golden);
    return !pinned.empty() && ckv::docgen::capture_frame(stage.app) == pinned;
}

bool plays_to(EditorMenuStage& stage, const char* name) { return stage.player.play_to(name) != nullptr; }

// The open menus, outermost first.
std::vector<const DropdownMenu*> open_menus(EditorMenuStage& stage) {
    std::vector<const DropdownMenu*> menus;
    for (const ckv::ui::View* popup : stage.editor.desktop()->popups())
        if (const auto* menu = dynamic_cast<const DropdownMenu*>(popup)) menus.push_back(menu);
    return menus;
}

// The label of the row the innermost open menu highlights, or empty.
std::string highlighted_row(EditorMenuStage& stage) {
    const std::vector<const DropdownMenu*> menus = open_menus(stage);
    if (menus.empty() || menus.back()->highlighted() < 0) return {};
    const ckv::widgets::MenuItem& row = menus.back()->items()[static_cast<std::size_t>(menus.back()->highlighted())];
    if (row.kind() != ckv::widgets::MenuItemKind::Command) return row.label();
    if (!row.presentation().label.empty()) return row.presentation().label;
    const ckv::ui::CommandInfo* const info = stage.app.commands().find(row.command());
    return info != nullptr ? info->title : std::string{};
}

const MenuBar* menu_bar(EditorMenuStage& stage) {
    return dynamic_cast<const MenuBar*>(stage.editor.desktop()->top_dock());
}

bool bar_active(EditorMenuStage& stage) {
    const MenuBar* bar = menu_bar(stage);
    return bar != nullptr && bar->active();
}

bool editor_has_keyboard(EditorMenuStage& stage) {
    return stage.app.focused() == stage.editor.editor() && !bar_active(stage) && stage.app.input_capture() == nullptr;
}

}  // namespace

CK_TEST(the_editor_menus_are_fully_operable_by_the_keyboard_alone) {
    EditorMenuStage stage(ckv::docgen::editor_menu_keyboard_script());
    CK_CHECK(plays_to(stage, "initial"));
    CK_CHECK(ckv::docgen::capture_frame(stage.app) == read_file("golden/editor_initial.dump"));
    CK_CHECK(editor_has_keyboard(stage));

    // F10 walks onto the bar; Down drops File on Save, which cannot act on
    // an unmodified document: the arrows stop on it and Enter does nothing.
    CK_CHECK(plays_to(stage, "f10_bar"));
    CK_CHECK(bar_active(stage));
    CK_CHECK(open_menus(stage).empty());
    CK_CHECK(plays_to_pinned_frame(stage, "file_opened"));
    CK_CHECK(open_menus(stage).size() == 1U);
    CK_CHECK(highlighted_row(stage) == "&Save");
    CK_CHECK(!open_menus(stage).back()->highlight().enabled);
    CK_CHECK(plays_to(stage, "disabled_enter"));
    CK_CHECK(open_menus(stage).size() == 1U);
    CK_CHECK(highlighted_row(stage) == "&Save");

    // Into the nested submenu and out again.
    CK_CHECK(plays_to(stage, "sample_highlighted"));
    CK_CHECK(highlighted_row(stage) == "Open &Sample");
    CK_CHECK(plays_to_pinned_frame(stage, "submenu_opened"));
    CK_CHECK(open_menus(stage).size() == 2U);
    CK_CHECK(highlighted_row(stage) == "Open &YAML sample");
    CK_CHECK(plays_to(stage, "submenu_down"));
    CK_CHECK(highlighted_row(stage) == "Open &JSON sample");
    CK_CHECK(plays_to(stage, "submenu_left"));
    CK_CHECK(open_menus(stage).size() == 1U);
    CK_CHECK(highlighted_row(stage) == "Open &Sample");
    CK_CHECK(plays_to(stage, "submenu_reentered"));
    CK_CHECK(open_menus(stage).size() == 2U);

    // Esc, one level at a time.
    CK_CHECK(plays_to(stage, "escape_submenu"));
    CK_CHECK(open_menus(stage).size() == 1U);
    CK_CHECK(highlighted_row(stage) == "Open &Sample");
    CK_CHECK(plays_to_pinned_frame(stage, "escape_menu"));
    CK_CHECK(open_menus(stage).empty());
    CK_CHECK(bar_active(stage));
    CK_CHECK(plays_to(stage, "escape_walk"));
    CK_CHECK(editor_has_keyboard(stage));

    // The checkable row, by Alt+V and Enter, both ways.
    CK_CHECK(stage.editor.editor()->show_line_numbers());
    CK_CHECK(plays_to_pinned_frame(stage, "view_opened"));
    CK_CHECK(highlighted_row(stage) == "Line &Numbers");
    CK_CHECK(open_menus(stage).back()->highlight().enabled);
    CK_CHECK(plays_to_pinned_frame(stage, "line_numbers_off"));
    CK_CHECK(!stage.editor.editor()->show_line_numbers());
    CK_CHECK(editor_has_keyboard(stage));
    CK_CHECK(plays_to(stage, "line_numbers_on"));
    CK_CHECK(stage.editor.editor()->show_line_numbers());
    CK_CHECK(editor_has_keyboard(stage));

    // A sample chosen from the submenu.
    CK_CHECK(plays_to_pinned_frame(stage, "json_opened"));
    CK_CHECK(stage.editor.window()->title() == "Editor — settings.json");
    CK_CHECK(editor_has_keyboard(stage));

    // The context menu at the caret: Cut and Copy greyed with nothing
    // selected. Select All runs from it; the Menu key then offers them.
    CK_CHECK(plays_to_pinned_frame(stage, "context_opened"));
    CK_CHECK(open_menus(stage).size() == 1U);
    CK_CHECK(highlighted_row(stage) == "Cu&t");
    CK_CHECK(!open_menus(stage).back()->highlight().enabled);
    CK_CHECK(plays_to(stage, "select_all_chosen"));
    CK_CHECK(open_menus(stage).empty());
    CK_CHECK(stage.editor.editor()->selection().has_value());
    CK_CHECK(editor_has_keyboard(stage));
    CK_CHECK(plays_to_pinned_frame(stage, "context_reopened"));
    CK_CHECK(open_menus(stage).back()->highlight().enabled);
    CK_CHECK(plays_to(stage, "context_escaped"));
    CK_CHECK(open_menus(stage).empty());
    CK_CHECK(editor_has_keyboard(stage));
    CK_CHECK(stage.editor.editor()->selection().has_value());
}

CK_TEST(the_editor_menus_are_fully_operable_by_the_mouse_alone) {
    EditorMenuStage stage(ckv::docgen::editor_menu_mouse_script());
    CK_CHECK(plays_to(stage, "initial"));
    CK_CHECK(editor_has_keyboard(stage));

    // Press, drag along the bar and down, release: the row under the
    // release is chosen.
    CK_CHECK(plays_to(stage, "bar_pressed"));
    CK_CHECK(open_menus(stage).size() == 1U);
    CK_CHECK(highlighted_row(stage).empty());  // a pressed menu waits for the pointer
    CK_CHECK(plays_to(stage, "dragged_to_edit"));
    CK_CHECK(open_menus(stage).size() == 1U);
    CK_CHECK(plays_to(stage, "dragged_to_view"));
    CK_CHECK(plays_to_pinned_frame(stage, "dragged_to_line_numbers"));
    CK_CHECK(highlighted_row(stage) == "Line &Numbers");
    CK_CHECK(stage.editor.editor()->show_line_numbers());
    CK_CHECK(plays_to(stage, "released_on_line_numbers"));
    CK_CHECK(!stage.editor.editor()->show_line_numbers());
    CK_CHECK(open_menus(stage).empty());
    CK_CHECK(editor_has_keyboard(stage));

    // A click opens File; a click on Save, which cannot act, leaves it open;
    // hovering Open Sample opens its submenu; a click there opens the sample.
    CK_CHECK(plays_to(stage, "file_clicked"));
    CK_CHECK(open_menus(stage).size() == 1U);
    CK_CHECK(plays_to(stage, "save_clicked"));
    CK_CHECK(open_menus(stage).size() == 1U);
    CK_CHECK(highlighted_row(stage) == "&Save");
    CK_CHECK(stage.editor.window()->title() == "Editor — config.yaml");
    CK_CHECK(plays_to_pinned_frame(stage, "sample_hovered"));
    CK_CHECK(open_menus(stage).size() == 2U);
    CK_CHECK(plays_to(stage, "json_hovered"));
    CK_CHECK(highlighted_row(stage) == "Open &JSON sample");
    CK_CHECK(plays_to(stage, "json_clicked"));
    CK_CHECK(open_menus(stage).empty());
    CK_CHECK(stage.editor.window()->title() == "Editor — settings.json");
    CK_CHECK(editor_has_keyboard(stage));

    // The light dismiss: a press on the text with View open closes it, lets
    // go of the bar and gives the editor the keyboard back -- and it is only
    // that: the caret stays where it was.
    const ckv::widgets::DocumentPosition caret = stage.editor.editor()->cursor();
    CK_CHECK(plays_to(stage, "view_clicked"));
    CK_CHECK(open_menus(stage).size() == 1U);
    CK_CHECK(bar_active(stage));
    CK_CHECK(plays_to_pinned_frame(stage, "outside_pressed"));
    CK_CHECK(open_menus(stage).empty());
    CK_CHECK(editor_has_keyboard(stage));
    CK_CHECK(stage.editor.editor()->cursor() == caret);

    // A right click opens the context menu at the pointer; a click
    // elsewhere takes it away with nothing chosen.
    CK_CHECK(plays_to_pinned_frame(stage, "context_opened"));
    CK_CHECK(open_menus(stage).size() == 1U);
    CK_CHECK(open_menus(stage).front()->bounds().x == EditorMenuStage::kContextCell.x);
    CK_CHECK(open_menus(stage).front()->bounds().y == EditorMenuStage::kContextCell.y);
    const ckv::widgets::DocumentPosition clicked = stage.editor.editor()->cursor();
    CK_CHECK(plays_to(stage, "context_dismissed"));
    CK_CHECK(open_menus(stage).empty());
    CK_CHECK(editor_has_keyboard(stage));
    CK_CHECK(stage.editor.editor()->cursor() == clicked);
    CK_CHECK(!stage.editor.editor()->selection().has_value());
}
