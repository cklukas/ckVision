// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/common_components.hpp"
#include "cvision/widgets/window.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::Point;
using ckv::Rect;
using ckv::ui::Application;
using namespace ckv::widgets;

namespace {

constexpr std::int64_t kMillisecond = 1'000'000;

// A window with two buttons, each with a tip, on a desktop, watched by a
// tooltip controller.
struct Tips {
    ckv::term::HeadlessTerminal term{ckv::Size{60, 16}};
    ManualClock clock;
    Application app{term, clock};
    ckv::ui::StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    Desktop* desktop = nullptr;
    Window* window = nullptr;
    ckv::ui::View* content = nullptr;
    Button* save = nullptr;
    Button* open = nullptr;
    int saves = 0;
    int opens = 0;
    int window_cancels = 0;
    std::optional<TooltipController> tips;

    Tips() {
        app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);
        desktop = app.root().add(std::make_unique<Desktop>(app.root().bounds()));
        auto frame = std::make_unique<Window>("Editor");
        frame->set_bounds(Rect{2, 1, 40, 10});
        auto body = std::make_unique<ckv::ui::View>();
        save = body->add(std::make_unique<Button>("Save"));
        save->set_bounds(Rect{1, 1, 10, 2});
        save->on_press = [this] { ++saves; };
        open = body->add(std::make_unique<Button>("Open"));
        open->set_bounds(Rect{14, 1, 10, 2});
        open->on_press = [this] { ++opens; };
        content = body.get();
        frame->set_content(std::move(body));
        frame->cancel_request = [this] { ++window_cancels; };
        window = desktop->add_window(std::move(frame));
        app.set_focus(save);
        app.step(0);
        tips.emplace(app, *desktop);
        tips->set_tip(*save, "Writes the file");
        tips->set_tip(*open, "Reads a file");
    }

    void wait(std::int64_t nanos) {
        clock.advance(nanos);
        app.step(clock.now_nanos());
    }
    void move_to(Point cell) {
        app.dispatch(ckv::MouseEvent{ckv::MouseAction::Move, ckv::MouseButton::None, cell, std::nullopt,
                                     Modifier::None});
        app.step(clock.now_nanos());
    }
    bool press(Key key, Modifier modifiers = Modifier::None, std::string text = {}) {
        const bool handled = app.dispatch(ckv::KeyEvent{KeyChord{key, modifiers, std::move(text)}});
        app.step(clock.now_nanos());
        return handled;
    }
    void click(Point cell) {
        app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, cell, std::nullopt,
                                     Modifier::None});
        app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, cell, std::nullopt,
                                     Modifier::None});
        app.step(clock.now_nanos());
    }
    // The graphemes of `width` cells of row `y` from column `x`.
    std::string cells(int x, int y, int width) const {
        std::string out;
        for (int column = x; column < x + width; ++column)
            out += app.composed_surface().at(Point{column, y}).grapheme();
        return out;
    }
    std::string row(int y) const { return cells(0, y, app.composed_surface().size().width); }
    Point inside(const Button& button) const {
        const Rect at = button.absolute_bounds();
        return Point{at.x + 2, at.y};
    }
};

}  // namespace

CK_TEST(resting_the_pointer_on_a_view_shows_its_tip_after_the_delay_and_moving_off_takes_it_away) {
    Tips t;
    const Point over_save = t.inside(*t.save);
    t.move_to(over_save);
    t.wait(499 * kMillisecond);
    CK_CHECK(t.tips->tooltip() == nullptr);
    t.wait(kMillisecond);
    CK_CHECK(t.tips->tooltip() != nullptr);
    if (t.tips->tooltip() == nullptr) return;
    CK_CHECK(t.tips->tooltip()->text() == "Writes the file");
    CK_CHECK(!t.tips->tooltip()->held());
    // Under the pointer's own cell, starting at its column.
    CK_CHECK(t.cells(over_save.x, over_save.y + 1, 17) == " Writes the file ");

    // Onto another view with a tip: this one goes at once, that one waits.
    t.move_to(t.inside(*t.open));
    CK_CHECK(t.tips->tooltip() == nullptr);
    CK_CHECK(t.row(over_save.y + 1).find("Writes the file") == std::string::npos);
    t.wait(500 * kMillisecond);
    CK_CHECK(t.tips->tooltip() != nullptr && t.tips->tooltip()->text() == "Reads a file");

    // Onto bare desktop: gone, and nothing waits to take its place.
    t.move_to(Point{55, 14});
    CK_CHECK(t.tips->tooltip() == nullptr);
    t.wait(2000 * kMillisecond);
    CK_CHECK(t.tips->tooltip() == nullptr);
}

CK_TEST(the_reader_may_move_the_pointer_onto_a_tip_to_read_it) {
    Tips t;
    const Point over_save = t.inside(*t.save);
    t.move_to(over_save);
    t.wait(500 * kMillisecond);
    CK_CHECK(t.tips->tooltip() != nullptr);
    // Onto the tip itself, below the pointer: it stays.
    t.move_to(Point{over_save.x + 3, over_save.y + 1});
    CK_CHECK(t.app.hovered_view() == t.tips->tooltip());
    CK_CHECK(t.tips->tooltip() != nullptr);
    // Off it onto bare desktop: it goes.
    t.move_to(Point{55, 14});
    CK_CHECK(t.tips->tooltip() == nullptr);
}

CK_TEST(text_input_puts_a_held_tip_away_as_a_key_does) {
    Tips t;
    CK_CHECK(t.press(Key::F1, Modifier::Ctrl));
    CK_CHECK(t.tips->tooltip() != nullptr);
    CK_CHECK(t.app.dispatch(ckv::TextEvent{"é", false}));
    t.app.step(0);
    CK_CHECK(t.tips->tooltip() == nullptr);
}

CK_TEST(a_pointer_that_moves_on_before_the_delay_shows_nothing) {
    Tips t;
    t.move_to(t.inside(*t.save));
    t.wait(300 * kMillisecond);
    t.move_to(Point{55, 14});
    t.wait(300 * kMillisecond);
    CK_CHECK(t.tips->tooltip() == nullptr);
}

CK_TEST(the_focus_arriving_on_a_view_shows_its_tip_under_the_view_after_the_delay) {
    Tips t;
    CK_CHECK(t.press(Key::Tab));
    CK_CHECK(t.app.focused() == t.open);
    t.wait(499 * kMillisecond);
    CK_CHECK(t.tips->tooltip() == nullptr);
    t.wait(kMillisecond);
    CK_CHECK(t.tips->tooltip() != nullptr);
    const Rect button = t.open->absolute_bounds();
    CK_CHECK(t.cells(button.x, button.bottom(), 14) == " Reads a file ");

    // The next key is the reader getting on with it: the tip goes, and the
    // focus moving brings the other view's after the same wait.
    CK_CHECK(t.press(Key::Tab, Modifier::Shift));
    CK_CHECK(t.tips->tooltip() == nullptr);
    t.wait(500 * kMillisecond);
    CK_CHECK(t.tips->tooltip() != nullptr && t.tips->tooltip()->text() == "Writes the file");
}

CK_TEST(a_press_takes_a_passive_tip_away_and_reaches_what_it_was_aimed_at) {
    Tips t;
    t.move_to(t.inside(*t.save));
    t.wait(500 * kMillisecond);
    CK_CHECK(t.tips->tooltip() != nullptr);
    t.click(t.inside(*t.save));
    CK_CHECK(t.tips->tooltip() == nullptr);
    CK_CHECK(t.saves == 1);
    // Nothing comes back by itself while the pointer stays where it is.
    t.wait(2000 * kMillisecond);
    CK_CHECK(t.tips->tooltip() == nullptr);
}

CK_TEST(the_tooltip_key_shows_the_focused_views_tip_at_once_and_escape_puts_only_it_away) {
    Tips t;
    CK_CHECK(t.app.focused() == t.save);
    CK_CHECK(t.press(Key::F1, Modifier::Ctrl));
    CK_CHECK(t.tips->tooltip() != nullptr);
    if (t.tips->tooltip() == nullptr) return;
    CK_CHECK(t.tips->tooltip()->held());
    const Rect button = t.save->absolute_bounds();
    CK_CHECK(t.cells(button.x, button.bottom(), 17) == " Writes the file ");
    // Held like an open menu: the key goes to it, not to what is behind it.
    CK_CHECK(t.press(Key::Escape));
    CK_CHECK(t.tips->tooltip() == nullptr);
    CK_CHECK(t.window_cancels == 0);
    CK_CHECK(t.app.focused() == t.save);
    CK_CHECK(!t.app.is_modal());
    CK_CHECK(t.row(button.bottom()).find("Writes the file") == std::string::npos);
    // With the tip gone, Escape is the window's again.
    t.press(Key::Escape);
    CK_CHECK(t.window_cancels == 1);
}

CK_TEST(any_key_puts_a_held_tip_away_without_reaching_the_application) {
    Tips t;
    int typed = 0;
    const ckv::ui::CommandId probe = t.app.commands().declare(
        ckv::ui::CommandDescriptor{.key = "test.probe", .title = "Probe", .category = "test", .chord = "F12",
                                   .handler = [&] { ++typed; }});
    (void)probe;
    CK_CHECK(t.press(Key::F1, Modifier::Ctrl));
    CK_CHECK(t.tips->tooltip() != nullptr);
    CK_CHECK(t.press(Key::F12));
    CK_CHECK(t.tips->tooltip() == nullptr);
    CK_CHECK(typed == 0);
    CK_CHECK(t.press(Key::F12));
    CK_CHECK(typed == 1);
}

CK_TEST(a_press_anywhere_puts_a_held_tip_away_as_it_puts_away_a_menu) {
    Tips t;
    CK_CHECK(t.press(Key::F1, Modifier::Ctrl));
    CK_CHECK(t.tips->tooltip() != nullptr);
    // A press outside is light dismissal: the tip goes, and the press does
    // not go on to the button it landed on.
    t.click(t.inside(*t.open));
    CK_CHECK(t.tips->tooltip() == nullptr);
    CK_CHECK(t.opens == 0);
    CK_CHECK(t.app.input_capture() == nullptr);
    t.click(t.inside(*t.open));
    CK_CHECK(t.opens == 1);
}

CK_TEST(the_tooltip_key_does_nothing_visible_for_a_view_without_a_tip) {
    Tips t;
    t.tips->set_tip(*t.save, "");
    CK_CHECK(t.press(Key::F1, Modifier::Ctrl));  // the command ran, and found nothing to say
    CK_CHECK(t.tips->tooltip() == nullptr);
    CK_CHECK(!t.app.is_modal());
}

CK_TEST(a_view_without_a_tip_of_its_own_shows_its_nearest_ancestors) {
    Tips t;
    t.tips->set_tip(*t.save, "");
    t.tips->set_tip(*t.content, "The editor's commands");
    CK_CHECK(t.tips->tip_for(*t.save) == "The editor's commands");
    CK_CHECK(t.tips->tip_for(*t.open) == "Reads a file");
    CK_CHECK(t.tips->tip_for(*t.window).empty());
    t.move_to(t.inside(*t.save));
    t.wait(500 * kMillisecond);
    CK_CHECK(t.tips->tooltip() != nullptr && t.tips->tooltip()->text() == "The editor's commands");
}

CK_TEST(the_tooltip_key_works_inside_a_modal_dialog_and_escape_leaves_the_dialog_up) {
    Tips t;
    auto dialog = std::make_unique<Window>("Confirm");
    dialog->set_bounds(Rect{10, 4, 30, 7});
    auto body = std::make_unique<ckv::ui::View>();
    Button* const yes = body->add(std::make_unique<Button>("Yes"));
    yes->set_bounds(Rect{1, 1, 8, 2});
    dialog->set_content(std::move(body));
    int dialog_cancels = 0;
    dialog->cancel_request = [&] { ++dialog_cancels; };
    t.desktop->present_modal(WindowHandle{std::move(dialog), yes}, t.app);
    t.app.step(0);
    t.tips->set_tip(*yes, "Goes ahead");
    CK_CHECK(t.app.focused() == yes);
    CK_CHECK(t.press(Key::F1, Modifier::Ctrl));
    CK_CHECK(t.tips->tooltip() != nullptr && t.tips->tooltip()->text() == "Goes ahead");
    CK_CHECK(t.press(Key::Escape));
    CK_CHECK(t.tips->tooltip() == nullptr);
    CK_CHECK(dialog_cancels == 0);
    CK_CHECK(t.app.is_modal());
    CK_CHECK(t.app.focused() == yes);
}

CK_TEST(the_controller_leaves_a_claimed_key_alone_and_gives_back_the_one_it_took) {
    {
        Tips t;
        const ckv::ui::CommandId command = t.app.commands().standard().tooltip;
        CK_CHECK(t.app.commands().has_handler(command));
        CK_CHECK(t.press(Key::F1, Modifier::Ctrl));
        CK_CHECK(t.tips->tooltip() != nullptr);
        t.tips.reset();  // takes its tip down with it
        CK_CHECK(!t.app.commands().has_handler(command));
        CK_CHECK(!t.app.is_modal());
        CK_CHECK(t.desktop->popups().empty());
    }
    {
        Tips t;
        t.tips.reset();
        int claimed = 0;
        const ckv::ui::CommandId command = t.app.commands().standard().tooltip;
        t.app.commands().set_handler(command, [&] { ++claimed; });
        t.tips.emplace(t.app, *t.desktop);
        t.tips->set_tip(*t.save, "Writes the file");
        CK_CHECK(t.press(Key::F1, Modifier::Ctrl));
        CK_CHECK(claimed == 1);
        CK_CHECK(t.tips->tooltip() == nullptr);
        t.tips.reset();
        CK_CHECK(t.app.commands().has_handler(command));
    }
}

CK_TEST(a_tooltip_is_placed_beside_its_anchor_by_one_fixed_rule_near_every_edge) {
    Tooltip tip{"Writes the file"};  // seventeen cells with its padding
    const Rect area{0, 0, 40, 10};
    // Room below: the row under the anchor, starting under its left edge.
    tip.show_near(Rect{5, 2, 6, 2}, area);
    CK_CHECK(tip.bounds() == (Rect{5, 4, 17, 1}));
    CK_CHECK(tip.shown());
    // Against the bottom edge: the row above.
    tip.show_near(Rect{5, 8, 6, 2}, area);
    CK_CHECK(tip.bounds() == (Rect{5, 7, 17, 1}));
    // An anchor filling the area's height: over its own bottom row.
    tip.show_near(Rect{5, 0, 6, 10}, area);
    CK_CHECK(tip.bounds() == (Rect{5, 9, 17, 1}));
    // Against the right edge: moved left until it ends inside.
    tip.show_near(Rect{35, 2, 3, 1}, area);
    CK_CHECK(tip.bounds() == (Rect{23, 3, 17, 1}));
    // Near the left edge of an area that does not start at zero.
    tip.show_near(Rect{0, 2, 3, 1}, Rect{4, 0, 36, 10});
    CK_CHECK(tip.bounds() == (Rect{4, 3, 17, 1}));
    // An area narrower than the text: as wide as the area, never wider.
    tip.show_near(Rect{3, 2, 3, 1}, Rect{0, 0, 9, 10});
    CK_CHECK(tip.bounds() == (Rect{0, 3, 9, 1}));
    // The same geometry, the same place, every time.
    tip.show_near(Rect{35, 2, 3, 1}, area);
    const Rect first = tip.bounds();
    tip.hide();
    tip.show_near(Rect{35, 2, 3, 1}, area);
    CK_CHECK(tip.bounds() == first);
}
