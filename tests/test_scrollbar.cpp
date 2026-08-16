// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/scrollbar.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;
using ckv::Point;
using ckv::Rect;
using ckv::ui::Context;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::Orientation;
using ckv::widgets::Scrollbar;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
    Context ctx() { return Context{&theme, &registry, nullptr}; }
};

ckv::MouseEvent mouse(ckv::MouseAction action, Point p) {
    return ckv::MouseEvent{action, ckv::MouseButton::Left, p, std::nullopt, Modifier::None};
}
}  // namespace

// --- Range / position basics ------------------------------------------

CK_TEST(max_position_is_content_minus_viewport) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_range(100, 10);
    CK_CHECK(sb.max_position() == 90);
}

CK_TEST(content_smaller_than_viewport_has_zero_max_position) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_range(5, 10);
    CK_CHECK(sb.max_position() == 0);
}

CK_TEST(auto_hide_hides_the_scrollbar_only_when_no_scroll_range_exists) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_auto_hide(true);
    sb.set_range(5, 10);
    CK_CHECK(!sb.visible());
    sb.set_range(20, 10);
    CK_CHECK(sb.visible());
    sb.set_auto_hide(false);
    sb.set_range(5, 10);
    CK_CHECK(sb.visible());
}

CK_TEST(set_position_clamps_to_the_valid_range) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_range(100, 10);
    sb.set_position(-5);
    CK_CHECK(sb.position() == 0);
    sb.set_position(1000);
    CK_CHECK(sb.position() == 90);
}

CK_TEST(changing_the_range_re_clamps_an_out_of_range_position) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_range(100, 10);
    sb.set_position(90);
    sb.set_range(20, 10);  // new max_position is 10 — old position of 90 is now invalid
    CK_CHECK(sb.position() == 10);
}

CK_TEST(negative_or_zero_viewport_size_is_clamped_to_at_least_one) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_range(10, 0);
    CK_CHECK(sb.viewport_size() == 1);
    sb.set_range(10, -5);
    CK_CHECK(sb.viewport_size() == 1);
}

CK_TEST(setting_position_to_its_current_value_does_not_fire_on_position_changed) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_range(100, 10);
    int calls = 0;
    sb.on_position_changed = [&](int) { ++calls; };
    sb.set_position(0);  // already 0
    CK_CHECK(calls == 0);
}

CK_TEST(on_position_changed_reports_the_new_position) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_range(100, 10);
    int reported = -1;
    sb.on_position_changed = [&](int p) { reported = p; };
    sb.set_position(42);
    CK_CHECK(reported == 42);
}

// --- Keyboard -------------------------------------------------------

CK_TEST(vertical_scrollbar_ignores_left_right_and_responds_to_up_down) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_range(100, 10);
    sb.set_position(5);
    CK_CHECK(!sb.on_key(ckv::KeyEvent{KeyChord{Key::Left, Modifier::None, ""}}));
    CK_CHECK(sb.on_key(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}}));
    CK_CHECK(sb.position() == 6);
}

CK_TEST(horizontal_scrollbar_ignores_up_down_and_responds_to_left_right) {
    Fixture f;
    Scrollbar sb(Orientation::Horizontal);
    sb.set_range(100, 10);
    sb.set_position(5);
    CK_CHECK(!sb.on_key(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}}));
    CK_CHECK(sb.on_key(ckv::KeyEvent{KeyChord{Key::Right, Modifier::None, ""}}));
    CK_CHECK(sb.position() == 6);
}

CK_TEST(page_down_advances_by_the_viewport_size) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_range(100, 10);
    sb.on_key(ckv::KeyEvent{KeyChord{Key::PageDown, Modifier::None, ""}});
    CK_CHECK(sb.position() == 10);
}

CK_TEST(home_and_end_jump_to_the_bounds) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_range(100, 10);
    sb.on_key(ckv::KeyEvent{KeyChord{Key::End, Modifier::None, ""}});
    CK_CHECK(sb.position() == 90);
    sb.on_key(ckv::KeyEvent{KeyChord{Key::Home, Modifier::None, ""}});
    CK_CHECK(sb.position() == 0);
}

// --- Mouse: arrows, paging, drag ----------------------------------------

CK_TEST(clicking_the_start_arrow_decrements_by_one) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_bounds(Rect{0, 0, 3, 20});
    sb.set_range(100, 10);
    sb.set_position(5);
    sb.on_mouse(mouse(ckv::MouseAction::Down, Point{0, 0}));
    CK_CHECK(sb.position() == 4);
}

CK_TEST(clicking_the_end_arrow_increments_by_one) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_bounds(Rect{0, 0, 3, 20});
    sb.set_range(100, 10);
    sb.set_position(5);
    sb.on_mouse(mouse(ckv::MouseAction::Down, Point{0, 19}));
    CK_CHECK(sb.position() == 6);
}

CK_TEST(clicking_the_track_below_the_thumb_pages_toward_the_end) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_bounds(Rect{0, 0, 3, 20});
    sb.set_range(100, 10);  // position 0: thumb near the top of an 18-cell track
    sb.on_mouse(mouse(ckv::MouseAction::Down, Point{0, 15}));  // well below the thumb
    CK_CHECK(sb.position() == 10);  // paged forward by the viewport size
}

CK_TEST(clicking_the_track_above_the_thumb_pages_toward_the_start) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_bounds(Rect{0, 0, 3, 20});
    sb.set_range(100, 10);
    sb.set_position(90);  // thumb now near the bottom
    sb.on_mouse(mouse(ckv::MouseAction::Down, Point{0, 2}));  // well above the thumb
    CK_CHECK(sb.position() == 80);
}

CK_TEST(dragging_the_thumb_moves_position_proportionally) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_bounds(Rect{0, 0, 3, 20});
    sb.set_range(100, 10);  // track = 18, thumb_len = max(1, 18*10/100) = 1, available = 17
    sb.set_position(0);
    // Thumb starts at cell 0 (position 0); click on it (local y=1, the
    // first track cell) and drag to the bottom of the track.
    sb.on_mouse(mouse(ckv::MouseAction::Down, Point{0, 1}));
    sb.on_mouse(mouse(ckv::MouseAction::Move, Point{0, 18}));  // dragged 17 cells = full track
    CK_CHECK(sb.position() == 90);  // reached max_position
    sb.on_mouse(mouse(ckv::MouseAction::Up, Point{0, 18}));
}

CK_TEST(move_without_a_prior_down_is_unhandled) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_bounds(Rect{0, 0, 3, 20});
    sb.set_range(100, 10);
    CK_CHECK(!sb.on_mouse(mouse(ckv::MouseAction::Move, Point{0, 10})));
}

CK_TEST(click_outside_the_scrollbars_own_bounds_is_unhandled) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_bounds(Rect{5, 5, 3, 20});
    sb.set_range(100, 10);
    CK_CHECK(!sb.on_mouse(mouse(ckv::MouseAction::Down, Point{50, 50})));
}

// --- Degenerate sizes ------------------------------------------------

CK_TEST(a_scrollbar_with_no_content_still_draws_and_handles_clicks_without_crashing) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_context(f.ctx());
    sb.set_bounds(Rect{0, 0, 3, 3});  // track_length == 1
    sb.set_range(0, 1);
    sb.on_mouse(mouse(ckv::MouseAction::Down, Point{0, 1}));
    ckv::scene::Surface s(ckv::Size{3, 3}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(s, Rect{0, 0, 3, 3});
    sb.draw(painter);
    CK_CHECK(true);
}

CK_TEST(a_zero_height_scrollbar_does_not_crash_on_draw_or_mouse) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_context(f.ctx());
    sb.set_bounds(Rect{0, 0, 3, 0});
    sb.set_range(100, 10);
    CK_CHECK(!sb.on_mouse(mouse(ckv::MouseAction::Down, Point{0, 0})));
    ckv::scene::Surface s(ckv::Size{3, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(s, Rect{0, 0, 3, 1});
    sb.draw(painter);
    CK_CHECK(true);
}

// --- Glyphs (CP437 desktop convention) ------------------------------------

CK_TEST(a_vertical_scrollbar_draws_solid_triangle_end_arrows) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_context(f.ctx());
    sb.set_bounds(Rect{0, 0, 1, 10});
    sb.set_range(100, 5);
    ckv::scene::Surface s(ckv::Size{1, 10}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(s, Rect{0, 0, 1, 10});
    sb.draw(painter);
    CK_CHECK(s.at(Point{0, 0}).grapheme() == "▲");
    CK_CHECK(s.at(Point{0, 9}).grapheme() == "▼");
}

CK_TEST(a_horizontal_scrollbar_draws_solid_triangle_end_arrows) {
    Fixture f;
    Scrollbar sb(Orientation::Horizontal);
    sb.set_context(f.ctx());
    sb.set_bounds(Rect{0, 0, 10, 1});
    sb.set_range(100, 5);
    ckv::scene::Surface s(ckv::Size{10, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(s, Rect{0, 0, 10, 1});
    sb.draw(painter);
    CK_CHECK(s.at(Point{0, 0}).grapheme() == "◄");
    CK_CHECK(s.at(Point{9, 0}).grapheme() == "►");
}

CK_TEST(the_scrollbar_uses_the_classic_medium_shade_page_area_and_block_indicator) {
    // CP437's light-shade page area and full-block scroll box are distinct
    // in both glyph coverage and theme role.  In particular, do not replace
    // the track with the heavier medium shade (`▒`): browser renderers turn
    // it into an intrusive striped field.
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_context(f.ctx());
    sb.set_bounds(Rect{0, 0, 1, 12});
    sb.set_range(100, 2);  // a small thumb, leaving plenty of bare track
    ckv::scene::Surface s(ckv::Size{1, 12}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(s, Rect{0, 0, 1, 12});
    sb.draw(painter);

    bool found_track = false, found_thumb = false;
    for (int y = 1; y < 11; ++y) {
        const std::string_view g = s.at(Point{0, y}).grapheme();
        if (g == "▒") found_track = true;
        if (g == "■") found_thumb = true;
    }
    CK_CHECK(found_track);
    CK_CHECK(found_thumb);
}

CK_TEST(a_bar_with_nothing_to_scroll_fills_its_track_with_the_dark_shade) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_context(f.ctx());
    sb.set_bounds(Rect{0, 0, 1, 10});
    sb.set_range(5, 20);  // content smaller than viewport: nowhere to scroll
    ckv::scene::Surface s(ckv::Size{1, 10}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(s, Rect{0, 0, 1, 10});
    sb.draw(painter);
    for (int y = 1; y < 9; ++y) CK_CHECK(s.at(Point{0, y}).grapheme() == "▓");
}

CK_TEST(classic_scrollbar_styles_its_page_area_and_indicator_from_their_roles) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_context(f.ctx());
    sb.set_bounds(Rect{0, 0, 1, 12});
    sb.set_range(100, 2);
    ckv::scene::Surface s(ckv::Size{1, 12}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(s, Rect{0, 0, 1, 12});
    sb.draw(painter);

    bool found_track = false;
    bool found_thumb = false;
    for (int y = 1; y < 11; ++y) {
        const ckv::Cell& cell = s.at(Point{0, y});
        if (cell.grapheme() == "▒") {
            found_track = true;
            CK_CHECK(cell.style() == f.theme.resolve(f.roles.scrollbar_track));
        }
        if (cell.grapheme() == "■") {
            found_thumb = true;
            CK_CHECK(cell.style() == f.theme.resolve(f.roles.scrollbar_thumb));
        }
    }
    CK_CHECK(found_track);
    CK_CHECK(found_thumb);
}
