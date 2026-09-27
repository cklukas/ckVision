// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Documentation and verification tooling, NOT part of the cvision library.
//
// The menu system of a shipped example, operated end to end by the keyboard
// alone and by the mouse alone (VISION 2; the roadmap M5 exit). The stage is
// the Editor example exactly as a reader runs it (examples/editor), whose
// menus carry everything the menu system has to offer: a nested submenu
// (File > Open Sample), rows that cannot act (File > Save on an unmodified
// document; Edit > Cut and Copy with nothing selected), a checkable row (View
// > Line Numbers) beside a radio set (the schemes), and the editor's own
// context menu. Played by tests/test_editor_menu_scripts.cpp and by
// generate_editor_goldens, which writes the frames the beats name.
#pragma once

#include <vector>

#include "cvision/core/clock.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "editor_app.hpp"
#include "event_script.hpp"

namespace ckv::docgen {

// The Editor example on an 80x24 terminal without graphics: the menu bar on
// row 0 (File, Edit, Search, View), the status line on row 23, and the
// editor window holding the focus.
class EditorMenuStage {
public:
    explicit EditorMenuStage(std::vector<ScriptBeat> beats);

    term::HeadlessTerminal terminal{Size{80, 24}, term::headless_no_graphics_profile()};
    ManualClock clock;
    ui::Application app{terminal, clock};
    editor_example::EditorApp editor{app};
    ScriptPlayer player;

    // Cells a pointer script aims at. The bar's titles sit on row 0; a
    // dropdown's first row is row 2, inside its frame; the submenu opens
    // beside the row that owns it.
    static constexpr Point kFileTitle{3, 0};
    static constexpr Point kEditTitle{9, 0};
    static constexpr Point kViewTitle{23, 0};
    static constexpr Point kSaveRow{4, 2};          // File > Save
    static constexpr Point kSampleRow{4, 3};        // File > Open Sample
    static constexpr Point kJsonRow{24, 5};         // File > Open Sample > Open JSON sample
    static constexpr Point kLineNumbersRow{26, 2};  // View > Line Numbers
    // A cell of the document text, clear of every menu this script opens.
    static constexpr Point kTextCell{60, 18};
    // A cell of the document text where the context menu is asked for.
    static constexpr Point kContextCell{30, 8};
};

std::vector<ScriptBeat> editor_menu_keyboard_script();
std::vector<ScriptBeat> editor_menu_mouse_script();

}  // namespace ckv::docgen
