// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The command palette as a popup: opened by the standard command_palette
// command (Ctrl+Shift+P), dismissed as a menu is, and running the chosen
// command where the reader was working.
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/window.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;
using ckv::widgets::CommandPalette;

namespace {

// A desktop with one document window whose editor view holds the focus and
// names the "document" command context, and three palette-visible commands:
// one bound to that context, one global, one global and disabled on demand.
struct PaletteFixture {
    ckv::term::HeadlessTerminal term;
    ckv::ManualClock clock;
    ckv::ui::Application app{term, clock};
    ckv::widgets::Desktop* desktop = nullptr;
    ckv::ui::View* editor = nullptr;
    int saved = 0;
    int printed = 0;
    int reindexed = 0;
    bool reindex_enabled = true;
    ckv::ui::CommandId save{};
    ckv::ui::CommandId print{};
    ckv::ui::CommandId reindex{};

    explicit PaletteFixture(ckv::term::Capabilities caps = ckv::term::baseline_capabilities())
        : term(ckv::Size{70, 20}, caps) {
        const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
        app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
        desktop = app.root().add(std::make_unique<ckv::widgets::Desktop>(ckv::Rect{0, 0, 70, 20}));
        auto window = std::make_unique<ckv::widgets::Window>("Notes");
        window->set_bounds(ckv::Rect{2, 4, 50, 12});
        auto* placed = desktop->add_window(std::move(window));
        editor = placed->content_pane().add(std::make_unique<ckv::ui::View>());
        editor->set_bounds(ckv::Rect{0, 0, 10, 2});
        editor->set_focus_policy(ckv::ui::FocusPolicy::TabStop);
        editor->set_command_context("document");
        app.set_focus(editor);

        ckv::ui::CommandRegistry& commands = app.commands();
        save = commands.declare({.key = "test.save", .title = "&Save", .category = "File",
                                 .scope = {.contexts = {"document"}}, .chord = "Ctrl+S",
                                 .handler = [this] { ++saved; }});
        print = commands.declare(
            {.key = "test.print", .title = "&Print", .category = "File", .handler = [this] { ++printed; }});
        reindex = commands.declare({.key = "test.reindex", .title = "Re&index", .category = "Tools",
                                    .handler = [this] { ++reindexed; }});
        commands.set_enabled_predicate(reindex, [this] { return reindex_enabled; });
        app.step(0);
    }

    bool dispatch_key(Key k, Modifier modifiers = Modifier::None, std::string text = {}) {
        const bool handled = app.dispatch(ckv::KeyEvent{KeyChord{k, modifiers, std::move(text)}});
        app.step(0);
        return handled;
    }
    bool open() { return dispatch_key(Key::Char, Modifier::Ctrl | Modifier::Shift, "p"); }
    void type(std::string_view text) {
        for (const char c : text) dispatch_key(Key::Char, Modifier::None, std::string(1, c));
    }
    void click(ckv::Point cell) {
        app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, cell, std::nullopt,
                                     Modifier::None});
        app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, cell, std::nullopt,
                                     Modifier::None});
        app.step(0);
    }

    CommandPalette* palette() const {
        for (ckv::ui::View* popup : desktop->popups())
            if (auto* found = dynamic_cast<CommandPalette*>(popup)) return found;
        return nullptr;
    }

    std::string row(int y) const {
        std::string out;
        const ckv::FrameView frame = app.current_frame();
        for (int x = 0; x < frame.size().width; ++x) out += frame.at(ckv::Point{x, y}).grapheme();
        return out;
    }
    // The screen row showing `needle`, or -1.
    int row_showing(std::string_view needle) const {
        for (int y = 0; y < app.current_frame().size().height; ++y)
            if (row(y).find(needle) != std::string::npos) return y;
        return -1;
    }
};

}  // namespace

CK_TEST(ctrl_shift_p_opens_the_palette_as_a_framed_modal_popup_holding_the_keyboard) {
    PaletteFixture f;
    CK_CHECK(f.palette() == nullptr);
    CK_CHECK(f.open());
    CommandPalette* const palette = f.palette();
    CK_CHECK(palette != nullptr);
    if (palette == nullptr) return;
    CK_CHECK(palette->framed());
    CK_CHECK(palette->casts_shadow());
    CK_CHECK(f.app.is_modal());
    CK_CHECK(f.app.focused() == palette);
    CK_CHECK(f.app.input_capture() == palette);
    // Centred across the desktop, near its top, and framed on screen.
    const ckv::Rect at = palette->absolute_bounds();
    CK_CHECK(at.x == (70 - at.width) / 2);
    CK_CHECK(at.y == 1);
    CK_CHECK(f.row(at.y).find("┌") != std::string::npos);
    // It lists what the editor allows, chords beside the titles; the standard
    // set, its own command included, is hidden.
    CK_CHECK(f.row_showing("Save  (Ctrl+S)") >= 0);
    CK_CHECK(f.row_showing("Print") >= 0);
    CK_CHECK(f.row_showing("Command Palette") < 0);

    // A second Ctrl+Shift+P while it is up opens no second palette.
    f.open();
    CK_CHECK(f.desktop->popups().size() == 1U);
}

CK_TEST(typing_filters_the_palette_and_enter_runs_the_command_where_the_reader_was) {
    PaletteFixture f;
    f.open();
    f.type("sa");
    CommandPalette* const palette = f.palette();
    CK_CHECK(palette != nullptr);
    if (palette == nullptr) return;
    CK_CHECK(palette->query() == "sa");
    CK_CHECK(palette->filtered_commands().size() == 1U);
    CK_CHECK(f.row_showing("Print") < 0);

    // Save is bound to the editor's context. It runs because the palette
    // answers for the place it was opened from, and the palette is gone by
    // the time it runs: the focus is back on the editor.
    CK_CHECK(f.dispatch_key(Key::Enter));
    CK_CHECK(f.saved == 1);
    CK_CHECK(f.palette() == nullptr);
    CK_CHECK(!f.app.is_modal());
    CK_CHECK(f.app.focused() == f.editor);
    CK_CHECK(f.app.input_capture() == nullptr);
    CK_CHECK(f.row_showing("Save  (Ctrl+S)") < 0);
}

CK_TEST(escape_dismisses_the_palette_and_gives_the_focus_back) {
    PaletteFixture f;
    f.open();
    f.type("pr");
    CK_CHECK(f.dispatch_key(Key::Escape));
    CK_CHECK(f.palette() == nullptr);
    CK_CHECK(f.app.focused() == f.editor);
    CK_CHECK(!f.app.is_modal());
    CK_CHECK(f.printed == 0);
    CK_CHECK(f.row_showing("Print") < 0);
}

CK_TEST(a_press_outside_the_palette_dismisses_it_and_runs_nothing) {
    PaletteFixture f;
    f.open();
    CommandPalette* const palette = f.palette();
    CK_CHECK(palette != nullptr);
    if (palette == nullptr) return;
    const ckv::Rect at = palette->absolute_bounds();
    f.click(ckv::Point{at.x + 1, at.bottom() + 1});
    CK_CHECK(f.palette() == nullptr);
    CK_CHECK(f.app.focused() == f.editor);
    CK_CHECK(f.saved + f.printed + f.reindexed == 0);
}

CK_TEST(a_click_on_a_listed_command_runs_it_and_the_wheel_moves_the_highlight) {
    PaletteFixture f;
    f.open();
    CommandPalette* const palette = f.palette();
    CK_CHECK(palette != nullptr);
    if (palette == nullptr) return;
    const ckv::Rect at = palette->absolute_bounds();
    f.app.dispatch(ckv::MouseEvent{ckv::MouseAction::Wheel, ckv::MouseButton::WheelDown,
                                   ckv::Point{at.x + 4, at.y + 5}, std::nullopt, Modifier::None});
    CK_CHECK(palette->highlighted_command() == f.print);
    const int print_row = f.row_showing("Print");
    CK_CHECK(print_row > at.y);
    f.click(ckv::Point{at.x + 4, print_row});
    CK_CHECK(f.printed == 1);
    CK_CHECK(f.palette() == nullptr);
}

CK_TEST(a_disabled_command_is_listed_greyed_and_the_highlight_passes_over_it) {
    PaletteFixture f;
    f.reindex_enabled = false;
    f.open();
    CommandPalette* const palette = f.palette();
    CK_CHECK(palette != nullptr);
    if (palette == nullptr) return;
    // Listed: it applies here, it just cannot run now.
    const int reindex_row = f.row_showing("Reindex");
    CK_CHECK(reindex_row >= 0);
    const ckv::FrameView frame = f.app.current_frame();
    const int print_row = f.row_showing("Print");
    const int column = palette->absolute_bounds().x + 3;
    CK_CHECK(!(frame.at(ckv::Point{column, reindex_row}).style() == frame.at(ckv::Point{column, print_row}).style()));

    // Down from Save goes to Print; Down again stays, since Reindex is the
    // only row past it and cannot be chosen. A click on it does nothing.
    CK_CHECK(f.dispatch_key(Key::Down));
    CK_CHECK(palette->highlighted_command() == f.print);
    CK_CHECK(f.dispatch_key(Key::Down));
    CK_CHECK(palette->highlighted_command() == f.print);
    f.click(ckv::Point{column, reindex_row});
    CK_CHECK(f.palette() == palette);
    CK_CHECK(f.reindexed == 0);

    // Enablement is read as the palette draws: enabled again, it is chosen.
    f.reindex_enabled = true;
    CK_CHECK(f.dispatch_key(Key::Down));
    CK_CHECK(palette->highlighted_command() == f.reindex);
    CK_CHECK(f.dispatch_key(Key::Enter));
    CK_CHECK(f.reindexed == 1);
}

CK_TEST(the_palette_follows_rebinding_and_retraction_while_it_is_open) {
    PaletteFixture f;
    f.open();
    CK_CHECK(f.row_showing("Save  (Ctrl+S)") >= 0);
    f.app.commands().unbind_key(*KeyChord::parse("Ctrl+S"));
    f.app.commands().bind_key(*KeyChord::parse("F2"), f.save);
    f.app.step(0);
    CK_CHECK(f.row_showing("Save  (F2)") >= 0);
    f.app.commands().withdraw(f.print);
    f.app.step(0);
    CK_CHECK(f.row_showing("Print") < 0);
    CK_CHECK(f.palette() != nullptr);
}

CK_TEST(ctrl_shift_p_from_a_kitty_keyboard_opens_the_palette_and_a_legacy_ctrl_p_does_not) {
    // Only a terminal speaking the kitty keyboard protocol reports Shift with
    // a Ctrl+letter; the legacy encoding folds Ctrl+Shift+P into the same
    // control byte as Ctrl+P, which must not open the palette by accident.
    ckv::term::Capabilities kitty = ckv::term::baseline_capabilities();
    kitty.keyboard_protocol = ckv::term::KeyboardProtocol::Kitty;
    PaletteFixture f(kitty);
    f.term.inject_bytes("\x1B[112;6u", 0);  // 'p' with Ctrl and Shift (1 + 4 + 1)
    f.app.step(0);
    CK_CHECK(f.palette() != nullptr);
    CK_CHECK(f.dispatch_key(Key::Escape));
    CK_CHECK(f.palette() == nullptr);

    PaletteFixture legacy;
    legacy.term.inject_bytes("\x10", 0);  // Ctrl+P, and Ctrl+Shift+P, in the legacy encoding
    legacy.app.step(0);
    CK_CHECK(legacy.palette() == nullptr);
}
