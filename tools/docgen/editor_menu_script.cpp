// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "editor_menu_script.hpp"

#include <utility>

namespace ckv::docgen {

EditorMenuStage::EditorMenuStage(std::vector<ScriptBeat> beats) : player(terminal, app, std::move(beats)) {}

std::vector<ScriptBeat> editor_menu_keyboard_script() {
    return {
        {"initial", {}, ""},
        // F10 walks onto the bar; Down drops File, on its first row: Save,
        // which cannot act on an unmodified document. The arrows stop there
        // (D-083), and Enter on it does nothing.
        {"f10_bar", {key(Key::F10)}, ""},
        {"file_opened", {key(Key::Down)}, "editor_menu_file.dump"},
        {"disabled_enter", {key(Key::Enter)}, ""},
        // Into the nested submenu and out again, by the arrows.
        {"sample_highlighted", {key(Key::Down)}, ""},
        {"submenu_opened", {key(Key::Right)}, "editor_menu_submenu.dump"},
        {"submenu_down", {key(Key::Down)}, ""},
        {"submenu_left", {key(Key::Left)}, ""},
        {"submenu_reentered", {key(Key::Right)}, ""},
        // Esc closes one level at a time: the submenu, the dropdown (the
        // walk stays on its title), then the walk.
        {"escape_submenu", {key(Key::Escape)}, ""},
        {"escape_menu", {key(Key::Escape)}, "editor_menu_escaped.dump"},
        {"escape_walk", {key(Key::Escape)}, ""},
        // Alt+V opens View on its checkable row; Enter toggles it, and the
        // gutter goes. Once more, and it is back.
        {"view_opened", {alt("v")}, "editor_menu_view.dump"},
        {"line_numbers_off", {key(Key::Enter)}, "editor_menu_no_line_numbers.dump"},
        {"line_numbers_on", {alt("v"), key(Key::Enter)}, ""},
        // Alt+F, and down into the submenu to open a sample from it.
        {"json_opened", {alt("f"), key(Key::Down), key(Key::Right), key(Key::Down), key(Key::Enter)},
         "editor_menu_json.dump"},
        // Shift+F10 is the editor's context menu at the caret, Cut and Copy
        // greyed with nothing selected; Select All runs from it, and the Menu
        // key then offers the same menu with Cut and Copy usable.
        {"context_opened", {key(Key::F10, Modifier::Shift)}, "editor_menu_context.dump"},
        {"select_all_chosen", {key(Key::Down), key(Key::Down), key(Key::Down), key(Key::Enter)}, ""},
        {"context_reopened", {key(Key::Menu)}, "editor_menu_context_selection.dump"},
        {"context_escaped", {key(Key::Escape)}, ""},
    };
}

std::vector<ScriptBeat> editor_menu_mouse_script() {
    using Stage = EditorMenuStage;
    return {
        {"initial", {}, ""},
        // Press on File, drag along the bar to View and down onto Line
        // Numbers: the open menu follows the pointer; the release chooses.
        {"bar_pressed", {press(Stage::kFileTitle)}, ""},
        {"dragged_to_edit", {move(Stage::kEditTitle, MouseButton::Left)}, ""},
        {"dragged_to_view", {move(Stage::kViewTitle, MouseButton::Left)}, ""},
        {"dragged_to_line_numbers", {move(Stage::kLineNumbersRow, MouseButton::Left)},
         "editor_menu_mouse_drag.dump"},
        {"released_on_line_numbers", {release(Stage::kLineNumbersRow)}, ""},
        // A click opens File and leaves it open; a click on Save, which
        // cannot act, does nothing; hovering Open Sample opens its submenu,
        // and a click in it opens that sample and closes the chain.
        {"file_clicked", {press(Stage::kFileTitle), release(Stage::kFileTitle)}, ""},
        {"save_clicked", {press(Stage::kSaveRow), release(Stage::kSaveRow)}, ""},
        {"sample_hovered", {move(Stage::kSampleRow)}, "editor_menu_mouse_submenu.dump"},
        {"json_hovered", {move(Stage::kJsonRow)}, ""},
        {"json_clicked", {press(Stage::kJsonRow), release(Stage::kJsonRow)}, ""},
        // A press outside every open menu ends the menu interaction: the
        // bar lets go and the editor has the keyboard again. The press is
        // only the dismissal; the caret does not move to where it landed.
        {"view_clicked", {press(Stage::kViewTitle), release(Stage::kViewTitle)}, ""},
        {"outside_pressed", {press(Stage::kTextCell), release(Stage::kTextCell)}, "editor_menu_mouse_dismissed.dump"},
        // A right click opens the editor's context menu where the pointer
        // is; a click elsewhere takes it away without choosing anything.
        {"context_opened",
         {press(Stage::kContextCell, MouseButton::Right), release(Stage::kContextCell, MouseButton::Right)},
         "editor_menu_mouse_context.dump"},
        {"context_dismissed", {press(Stage::kTextCell), release(Stage::kTextCell)}, ""},
    };
}

}  // namespace ckv::docgen
