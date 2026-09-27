// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Documentation and verification tooling, NOT part of the cvision library.
//
// The menu system operated end to end by the keyboard alone and by the mouse
// alone (the roadmap M5 exit). One stage carries everything the menu system
// has to offer a reader: a bar with File and View menus, a nested submenu
// (File > Open recent), checkable rows (View > Wrap lines, Line numbers), a
// row that cannot be used (View > Print preview), a context menu on the
// document with a disabled Paste in it, and a status line that says why the
// highlighted row cannot be used (D-083). The keyboard and the pointer reach
// the open submenu as the same frame, so both scripts pin it with one golden.
// Played by tests/test_menu_operability_scripts.cpp and by
// generate_event_script_goldens.
#pragma once

#include <string>
#include <vector>

#include "cvision/core/clock.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/menu.hpp"
#include "cvision/widgets/status_line.hpp"
#include "event_script.hpp"

namespace ckv::docgen {

class DocumentPane;

// An 80x24 desktop: the menu bar on row 0, the status line on row 23, and a
// Document window whose pane holds the focus and opens the context menu on
// the Menu key, Shift-F10 or a right click.
class MenuStage {
public:
    explicit MenuStage(std::vector<ScriptBeat> beats);

    term::HeadlessTerminal terminal{Size{80, 24}};
    ManualClock clock;
    ui::Application app{terminal, clock};
    ScriptPlayer player;

    widgets::Desktop& desktop() noexcept { return *desktop_; }
    widgets::MenuBar& bar() noexcept { return *bar_; }
    // The status line the open menus explain their unusable rows on.
    widgets::StatusLine& status() noexcept;
    // The focused document pane the context menu belongs to.
    ui::View* document() const noexcept;
    // Every row the reader chose, in order ("new", "alpha", "wrap", "copy"...).
    const std::vector<std::string>& chosen() const noexcept { return chosen_; }
    bool wrap_lines() const noexcept { return wrap_lines_; }
    bool line_numbers() const noexcept { return line_numbers_; }

    // Cells a pointer script aims at. The bar's titles sit on row 0; a
    // dropdown's rows start on row 2, inside its frame; the submenu opens
    // beside the row that owns it.
    static constexpr Point kFileTitle{2, 0};
    static constexpr Point kViewTitle{8, 0};
    static constexpr Point kWrapRow{9, 2};     // View > Wrap lines
    static constexpr Point kRecentRow{4, 3};   // File > Open recent
    static constexpr Point kAlphaRow{24, 4};   // File > Open recent > Alpha
    static constexpr Point kDocumentCell{20, 8};
    static constexpr Point kDesktopCell{70, 20};

private:
    std::vector<widgets::MenuItem> context_items();
    // Shows why the highlighted row cannot be used, or nothing.
    void explain(const widgets::MenuHighlight& highlight);
    void choose(std::string what);
    void refresh_document();

    ui::StandardRoles roles_;
    widgets::Desktop* desktop_ = nullptr;
    widgets::MenuBar* bar_ = nullptr;
    widgets::StatusLine* status_ = nullptr;
    DocumentPane* document_ = nullptr;
    std::vector<std::string> chosen_;
    bool wrap_lines_ = false;
    bool line_numbers_ = true;
};

std::vector<ScriptBeat> menu_keyboard_script();
std::vector<ScriptBeat> menu_mouse_script();

}  // namespace ckv::docgen
