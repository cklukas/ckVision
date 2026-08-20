// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
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

CK_TEST(the_scrollbar_policy_decides_visibility_from_whether_content_fits) {
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    // Auto: on screen exactly while there is something to scroll to, so a bar
    // present means there is more and a bar absent means there is not.
    sb.set_policy(ckv::widgets::ScrollbarPolicy::Auto);
    sb.set_range(5, 10);
    CK_CHECK(!sb.visible());
    CK_CHECK(!sb.should_show());
    sb.set_range(20, 10);
    CK_CHECK(sb.visible());
    CK_CHECK(sb.should_show());

    // Always: keeps its column whether or not it can scroll, so a surface
    // whose content changes constantly does not reflow around it.
    sb.set_policy(ckv::widgets::ScrollbarPolicy::Always);
    sb.set_range(5, 10);
    CK_CHECK(sb.visible());

    // Hidden: for a view whose scrolling a containing viewport owns.
    sb.set_policy(ckv::widgets::ScrollbarPolicy::Hidden);
    sb.set_range(50, 10);
    CK_CHECK(!sb.visible());
    CK_CHECK(!sb.should_show());
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

CK_TEST(the_trough_is_marked_by_colour_rather_than_by_a_shaded_glyph) {
    // The empty half of a half-block cell shows plain background. A textured
    // track would therefore meet the thumb at a visible seam — the pattern
    // stopping mid-cell for no reason a reader can name — so the trough is a
    // blank cell wearing the track role's own colour instead.
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
        const ckv::Cell& cell = s.at(Point{0, y});
        const std::string_view g = cell.grapheme();
        CK_CHECK(g != "▒" && g != "▓");  // no shaded fills anywhere in the track
        if (g == " ") {
            found_track = true;
            CK_CHECK(cell.style() == f.theme.resolve(f.roles.scrollbar_track));
        }
        if (g == "█" || g == "▀" || g == "▄") found_thumb = true;
    }
    CK_CHECK(found_track);
    CK_CHECK(found_thumb);
}

CK_TEST(a_half_covered_cell_shares_the_troughs_background_exactly) {
    // This is the whole point of dropping the shaded glyph: the uncovered
    // half of a half block and the cell next to it must be the same colour,
    // or the join shows.
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_context(f.ctx());
    sb.set_bounds(Rect{0, 0, 1, 12});
    sb.set_range(41, 20);  // deliberately not a whole number of cells
    ckv::scene::Surface s(ckv::Size{1, 12}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(s, Rect{0, 0, 1, 12});
    sb.draw(painter);

    const ckv::Color trough = f.theme.resolve(f.roles.scrollbar_track).bg;
    for (int y = 1; y < 11; ++y) {
        const ckv::Cell& cell = s.at(Point{0, y});
        const std::string_view g = cell.grapheme();
        if (g == "▀" || g == "▄" || g == " ") CK_CHECK(cell.style().bg == trough);
    }
}

CK_TEST(a_bar_with_nothing_to_scroll_shows_a_thumb_the_length_of_its_track) {
    // A full-length thumb is the honest statement that the whole content is
    // on screen, and it is the same thing thumb_length() reports.
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_context(f.ctx());
    sb.set_bounds(Rect{0, 0, 1, 10});
    sb.set_range(5, 20);  // content smaller than viewport: nowhere to scroll
    ckv::scene::Surface s(ckv::Size{1, 10}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(s, Rect{0, 0, 1, 10});
    sb.draw(painter);
    for (int y = 1; y < 9; ++y) CK_CHECK(s.at(Point{0, y}).grapheme() == "█");
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
        if (cell.grapheme() == " ") {
            found_track = true;
            CK_CHECK(cell.style() == f.theme.resolve(f.roles.scrollbar_track));
        }
        // A fully covered cell is a solid block in the thumb's own style; a
        // half-covered one is a half block that keeps the track's background,
        // so the uncovered half still reads as track.
        if (cell.grapheme() == "█") {
            found_thumb = true;
            CK_CHECK(cell.style() == f.theme.resolve(f.roles.scrollbar_thumb));
        }
        if (cell.grapheme() == "▀" || cell.grapheme() == "▄") {
            found_thumb = true;
            CK_CHECK(cell.style().fg == f.theme.resolve(f.roles.scrollbar_thumb).fg);
            CK_CHECK(cell.style().bg == f.theme.resolve(f.roles.scrollbar_track).bg);
        }
    }
    CK_CHECK(found_track);
    CK_CHECK(found_thumb);
}

CK_TEST(the_thumb_length_reports_how_much_of_the_content_is_visible) {
    // The thumb's size is the share of the content on screen — the question a
    // one-cell marker could not answer at all.
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_bounds(Rect{0, 0, 1, 22});  // 20 cells of track, minus two arrows

    // Half the content visible: about half the track.
    sb.set_range(40, 20);
    const int half = sb.thumb_length();
    CK_CHECK(half >= 8 && half <= 12);

    // A tenth visible: a much shorter thumb, but never nothing.
    sb.set_range(200, 20);
    const int tenth = sb.thumb_length();
    CK_CHECK(tenth >= 1);
    CK_CHECK(tenth < half);

    // An enormous document still leaves something to see and to grab.
    sb.set_range(100000, 20);
    CK_CHECK(sb.thumb_length() >= 1);
}

CK_TEST(a_scrollable_thumb_always_leaves_a_cell_of_track_to_move_through) {
    // A thumb filling its track says "everything is visible", which is
    // exactly what is not true while there is anything to scroll to. The gap
    // is also what keeps every position reachable by dragging.
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_bounds(Rect{0, 0, 1, 22});

    // Only just scrollable: the proportional length would fill the track.
    sb.set_range(21, 20);
    CK_CHECK(sb.max_position() > 0);
    CK_CHECK(sb.thumb_length() <= 20 - 1);

    // Nothing to scroll: now the thumb may fill it, which is the honest
    // signal that the whole content is on screen.
    sb.set_range(20, 20);
    CK_CHECK(sb.max_position() == 0);
    CK_CHECK(sb.thumb_length() == 20);
}

CK_TEST(the_thumb_is_drawn_as_a_continuous_handle_with_half_cell_ends) {
    // Block glyphs give the thumb twice the resolution of the cell grid: a
    // fully covered cell is solid, and a cell the thumb only half covers
    // shows the matching half block instead of being rounded away.
    Fixture f;
    Scrollbar sb(Orientation::Vertical);
    sb.set_context(f.ctx());
    sb.set_bounds(Rect{0, 0, 1, 12});   // 10 cells of track = 20 half cells
    sb.set_range(40, 20);               // half the content visible
    ckv::scene::Surface s(ckv::Size{1, 12}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(s, Rect{0, 0, 1, 12});
    sb.draw(painter);

    // The thumb's cells are contiguous — a handle, not a row of beads.
    int first = -1, last = -1;
    for (int y = 1; y < 11; ++y) {
        const std::string g{s.at(Point{0, y}).grapheme()};
        if (g == "█" || g == "▀" || g == "▄") {
            if (first < 0) first = y;
            last = y;
        }
    }
    CK_CHECK(first > 0);
    for (int y = first; y <= last; ++y) {
        const std::string g{s.at(Point{0, y}).grapheme()};
        CK_CHECK(g == "█" || g == "▀" || g == "▄");
    }
    // Half the content visible: about half the track, and the arrows are
    // never part of the thumb.
    CK_CHECK(last - first + 1 >= 4);
    CK_CHECK(s.at(Point{0, 0}).grapheme() == "▲");
    CK_CHECK(s.at(Point{0, 11}).grapheme() == "▼");
}

CK_TEST(a_horizontal_thumb_uses_the_left_and_right_half_blocks) {
    // The same rule read along the other axis: a half-covered cell shows
    // which half is covered, rather than rounding to a whole one.
    Fixture f;
    Scrollbar sb(Orientation::Horizontal);
    sb.set_context(f.ctx());
    sb.set_bounds(Rect{0, 0, 12, 1});
    sb.set_range(41, 20);  // deliberately not a whole number of cells
    ckv::scene::Surface s(ckv::Size{12, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(s, Rect{0, 0, 12, 1});
    sb.draw(painter);

    bool any_thumb = false;
    for (int x = 1; x < 11; ++x) {
        const std::string g{s.at(Point{x, 0}).grapheme()};
        // Never the vertical halves on a horizontal bar.
        CK_CHECK(g != "▀" && g != "▄");
        if (g == "█" || g == "▌" || g == "▐") any_thumb = true;
    }
    CK_CHECK(any_thumb);
    CK_CHECK(s.at(Point{0, 0}).grapheme() == "◄");
    CK_CHECK(s.at(Point{11, 0}).grapheme() == "►");
}
