// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/popup_list.hpp"

#include "cvision/term/headless_terminal.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;
using ckv::Point;
using ckv::Rect;
using ckv::widgets::Desktop;
using ckv::widgets::PopupList;
using ckv::widgets::show_popup_list;

namespace {
struct Fixture {
    ckv::term::HeadlessTerminal term{ckv::Size{40, 16}};
    ckv::ManualClock clock;
    ckv::ui::Application app{term, clock};
    Desktop* desktop = nullptr;

    Fixture() {
        const ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
        app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
        desktop = static_cast<Desktop*>(
            app.root().add_child(std::make_unique<Desktop>(app.root().bounds())));
        app.step(0);
    }

    void press(Key k) {
        app.dispatch(ckv::KeyEvent{KeyChord{k, Modifier::None, ""}});
        app.step(0);
    }

    void click(Point cell) {
        app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, cell, std::nullopt,
                                     Modifier::None});
        app.step(0);
    }

    std::string row(int y) const {
        std::string out;
        for (int x = 0; x < 40; ++x) out += app.composed_surface().at(Point{x, y}).grapheme();
        return out;
    }
};
}  // namespace

CK_TEST(a_popup_list_hangs_under_its_anchor_and_is_as_wide_as_its_longest_item) {
    Fixture f;
    int chosen = -1;
    PopupList* popup = show_popup_list(Rect{3, 2, 6, 1}, {"One", "Something longer"}, std::nullopt, f.app,
                                       *f.desktop, [&](std::size_t index) { chosen = static_cast<int>(index); });
    f.app.step(0);
    CK_CHECK(popup->bounds().y == 3);  // the row under the anchor
    CK_CHECK(popup->bounds().x == 3);  // left edges aligned
    // "Something longer" plus a blank either side and the frame.
    CK_CHECK(popup->bounds().width == 20);
    CK_CHECK(f.app.input_capture() == popup);
    CK_CHECK(chosen == -1);
}

CK_TEST(a_popup_list_that_does_not_fit_below_its_anchor_drops_upward) {
    Fixture f;
    // Sixteen rows of desktop, an anchor near the bottom, and a list too long
    // to hang under it: clamping it back down would put it over the control.
    PopupList* popup = show_popup_list(Rect{1, 13, 6, 1}, {"a", "b", "c", "d", "e", "f"}, std::nullopt, f.app,
                                       *f.desktop, {});
    f.app.step(0);
    CK_CHECK(popup->bounds().bottom() <= 13);
}

CK_TEST(enter_chooses_a_row_and_the_popup_closes_with_it) {
    Fixture f;
    int chosen = -1;
    show_popup_list(Rect{1, 1, 6, 1}, {"One", "Two", "Three"}, std::size_t{1}, f.app, *f.desktop,
                    [&](std::size_t index) { chosen = static_cast<int>(index); });
    f.app.step(0);
    // It opens on what was already chosen rather than on the first row.
    CK_CHECK(f.row(4).find("Two") != std::string::npos);  // frame, One, then Two
    f.press(Key::Down);
    f.press(Key::Enter);
    CK_CHECK(chosen == 2);
    CK_CHECK(f.desktop->popups().empty());
    CK_CHECK(f.app.input_capture() == nullptr);
    CK_CHECK(!f.app.is_modal());
}

CK_TEST(one_press_on_a_row_picks_it) {
    Fixture f;
    int chosen = -1;
    show_popup_list(Rect{1, 1, 6, 1}, {"One", "Two", "Three"}, std::nullopt, f.app, *f.desktop,
                    [&](std::size_t index) { chosen = static_cast<int>(index); });
    f.app.step(0);
    // A list that exists only to be picked from does not ask for a second
    // click the way a list inside a window does.
    f.click(Point{3, 4});  // frame at row 2, then One, Two
    CK_CHECK(chosen == 1);
    CK_CHECK(f.desktop->popups().empty());
}

CK_TEST(escape_and_a_press_outside_both_dismiss_without_choosing) {
    Fixture f;
    int chosen = -1;
    int dismissed = 0;
    show_popup_list(Rect{1, 1, 6, 1}, {"One", "Two"}, std::nullopt, f.app, *f.desktop,
                    [&](std::size_t index) { chosen = static_cast<int>(index); }, [&] { ++dismissed; });
    f.app.step(0);
    f.press(Key::Escape);
    CK_CHECK(chosen == -1);
    CK_CHECK(dismissed == 1);
    CK_CHECK(f.desktop->popups().empty());

    show_popup_list(Rect{1, 1, 6, 1}, {"One", "Two"}, std::nullopt, f.app, *f.desktop,
                    [&](std::size_t index) { chosen = static_cast<int>(index); }, [&] { ++dismissed; });
    f.app.step(0);
    f.click(Point{30, 12});  // well outside it
    CK_CHECK(chosen == -1);
    CK_CHECK(dismissed == 2);
    CK_CHECK(f.desktop->popups().empty());
}
