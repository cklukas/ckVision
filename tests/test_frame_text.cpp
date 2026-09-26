// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/frame_text.hpp"

#include <memory>
#include <string>

#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"
#include "cvision/widgets/window.hpp"

using ckv::Point;
using ckv::Rect;
using ckv::scene::Painter;
using ckv::scene::Surface;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::FrameSlot;
using ckv::widgets::FrameText;
using ckv::widgets::Window;

namespace {

struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
    std::unique_ptr<Window> window = std::make_unique<Window>("Document");
    Surface surface{ckv::Size{30, 6}, ckv::Cell::from_grapheme(" ", ckv::Style{})};

    Fixture() {
        window->set_context(ckv::ui::Context{&theme, &registry, nullptr});
        window->on_attached();
        window->set_bounds(Rect{0, 0, 30, 6});
    }
    /// The window and then its overlay, as the desktop composes them.
    void draw(FrameText& text) {
        Painter frame(surface, Rect{0, 0, 30, 6});
        window->draw(frame);
        Painter overlay(surface, text.bounds(), Point{text.bounds().x, text.bounds().y});
        text.draw(overlay);
    }
    std::string row(int y) const {
        std::string out;
        for (int x = 0; x < surface.size().width; ++x) out += surface.at(Point{x, y}).grapheme();
        return out;
    }
};

}  // namespace

CK_TEST(frame_text_sits_on_the_border_in_the_frames_style_following_activation) {
    Fixture f;
    auto* const text = f.window->add_frame_overlay(std::make_unique<FrameText>("12:3"),
                                                   FrameSlot{ckv::widgets::Edge::Bottom, ckv::ui::Alignment::Start, 1});
    CK_CHECK(text->horizontal_size_hint().preferred == 6);  // the text and a space either side
    f.window->set_active(true);
    f.draw(*text);
    const int x = text->bounds().x;
    CK_CHECK(f.row(5).find(" 12:3 ") != std::string::npos);
    CK_CHECK(f.surface.at(Point{x + 1, 5}).style() == f.window->frame_style());

    f.window->set_active(false);
    f.draw(*text);
    CK_CHECK(f.surface.at(Point{x + 1, 5}).style() == f.window->frame_style());
    CK_CHECK(f.window->frame_style() == f.theme.resolve(f.registry.find("ckv.window.frame.inactive")));
}

CK_TEST(a_reserved_width_keeps_the_readout_still_and_the_rest_stays_border) {
    Fixture f;
    auto* const text = f.window->add_frame_overlay(std::make_unique<FrameText>("1:1"), FrameSlot{});
    text->set_reserved_width(12);
    text->set_alignment(ckv::ui::Alignment::End);
    const Rect placed = text->bounds();
    text->set_text("120:45");
    CK_CHECK(text->bounds() == placed);  // the hint did not move with the text
    f.window->set_active(true);
    f.draw(*text);
    // The text ends at the reserved box's end; the cells before it are line.
    CK_CHECK(f.surface.at(Point{placed.x + placed.width - 2, 5}).grapheme() == "5");
    CK_CHECK(f.surface.at(Point{placed.x, 5}).grapheme() != " ");
}

CK_TEST(frame_text_outside_a_window_draws_nothing) {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
    FrameText text("orphan");
    text.set_context(ckv::ui::Context{&theme, &registry, nullptr});
    text.set_bounds(Rect{0, 0, 8, 1});
    Surface surface{ckv::Size{8, 1}, ckv::Cell::from_grapheme(".", ckv::Style{})};
    Painter painter(surface, Rect{0, 0, 8, 1});
    text.draw(painter);
    CK_CHECK(surface.at(Point{1, 0}).grapheme() == ".");
}

CK_TEST(frame_text_given_less_room_than_it_asked_for_is_elided_inside_its_padding) {
    Fixture f;
    auto* const text = f.window->add_frame_overlay(std::make_unique<FrameText>("abcdefghij"), FrameSlot{});
    const Rect asked = text->bounds();
    text->set_bounds(Rect{asked.x, asked.y, 6, 1});
    f.window->set_active(true);
    f.draw(*text);
    const int x = asked.x;
    std::string shown;
    for (int column = 0; column < 6; ++column) shown += f.surface.at(Point{x + column, asked.y}).grapheme();
    CK_CHECK(shown == " abc… ");
}

CK_TEST(a_scripted_frame_text_follows_its_window_through_a_click_that_deactivates_it) {
    // Application-level script: the window loses activation to a dispatched
    // click on another one, and the next step() shows the readout in the
    // inactive border's style and with the text the host set meanwhile.
    ckv::term::HeadlessTerminal term(ckv::Size{60, 16});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    const StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* desktop = app.root().add(std::make_unique<ckv::widgets::Desktop>(app.root().bounds()));
    auto* document = desktop->add_window(std::make_unique<Window>("Document"));
    document->set_bounds(Rect{0, 0, 30, 8});
    auto* readout = document->add_frame_overlay(std::make_unique<FrameText>("1:1"),
                                                FrameSlot{ckv::widgets::Edge::Bottom, ckv::ui::Alignment::Start, 5});
    auto* other = desktop->add_window(std::make_unique<Window>("Other"));
    other->set_bounds(Rect{32, 0, 20, 8});
    desktop->activate(document);
    app.step(0);
    const auto bottom_row = [&] {
        std::string out;
        for (int x = 0; x < 30; ++x) out += app.composed_surface().at(Point{x, 7}).grapheme();
        return out;
    };
    const auto readout_style = [&] {
        return app.composed_surface().at(Point{readout->absolute_bounds().x + 1, 7}).style();
    };
    CK_CHECK(bottom_row().find(" 1:1 ") != std::string::npos);
    CK_CHECK(readout_style() == document->frame_style());
    const ckv::Style active = document->frame_style();

    app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, Point{40, 4}, std::nullopt,
                                 ckv::Modifier::None});
    app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, Point{40, 4}, std::nullopt,
                                 ckv::Modifier::None});
    readout->set_text("1:12");
    app.step(0);
    CK_CHECK(desktop->active_window() == other);
    CK_CHECK(bottom_row().find(" 1:12 ") != std::string::npos);
    CK_CHECK(readout_style() == document->frame_style());
    CK_CHECK(!(document->frame_style() == active));
}
