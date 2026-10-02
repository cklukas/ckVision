// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/splitter.hpp"

#include "cvision/term/headless_terminal.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/desktop.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;
using ckv::Rect;
using ckv::ui::Context;
using ckv::ui::intern_standard_roles;
using ckv::ui::kUnboundedExtent;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::SizeHint;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::ui::View;
using ckv::widgets::Orientation;
using ckv::widgets::Splitter;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
    Context ctx() { return Context{&theme, &registry, nullptr}; }
};

// A pane with a non-zero minimum on both axes, so clamp behavior is
// actually exercised (a plain View's default hint has min == 0, which
// would let split_position() drift to 0 or the full extent unchecked).
class MinSizedView : public View {
public:
    explicit MinSizedView(int min) : min_(min) {}
    void set_min(int min) {
        min_ = min;
        size_hint_changed();
    }
    SizeHint horizontal_size_hint() const override { return {min_, min_, kUnboundedExtent}; }
    SizeHint vertical_size_hint() const override { return {min_, min_, kUnboundedExtent}; }

private:
    int min_;
};

ckv::KeyEvent key(Key k) { return ckv::KeyEvent{KeyChord{k, Modifier::None, ""}}; }

ckv::MouseEvent mouse(ckv::MouseAction action, int x, int y,
                      ckv::MouseButton button = ckv::MouseButton::Left) {
    return ckv::MouseEvent{action, button, ckv::Point{x, y}, std::nullopt, Modifier::None};
}
}  // namespace

// --- Construction / default split ---------------------------------------

CK_TEST(a_horizontal_splitter_defaults_to_an_exact_50_50_split) {
    Splitter splitter(Rect{0, 0, 21, 10}, std::make_unique<View>(), std::make_unique<View>());
    CK_CHECK(splitter.split_position() == 10);  // (21 - 1 divider) / 2
    CK_CHECK(splitter.first()->bounds() == (Rect{0, 0, 10, 10}));
    CK_CHECK(splitter.second()->bounds() == (Rect{11, 0, 10, 10}));
}

CK_TEST(a_vertical_splitter_defaults_to_an_exact_50_50_split) {
    Splitter splitter(Rect{0, 0, 10, 21}, std::make_unique<View>(), std::make_unique<View>(),
                       Orientation::Vertical);
    CK_CHECK(splitter.split_position() == 10);
    CK_CHECK(splitter.first()->bounds() == (Rect{0, 0, 10, 10}));
    CK_CHECK(splitter.second()->bounds() == (Rect{0, 11, 10, 10}));
}

CK_TEST(first_and_second_and_orientation_accessors_report_what_was_constructed) {
    auto first = std::make_unique<View>();
    auto second = std::make_unique<View>();
    View* first_ptr = first.get();
    View* second_ptr = second.get();
    Splitter splitter(Rect{0, 0, 21, 10}, std::move(first), std::move(second),
                       Orientation::Vertical);
    CK_CHECK(splitter.first() == first_ptr);
    CK_CHECK(splitter.second() == second_ptr);
    CK_CHECK(splitter.orientation() == Orientation::Vertical);
}

// --- Keyboard adjustment --------------------------------------------------

CK_TEST(left_and_right_move_a_horizontal_splitters_split_position_by_one) {
    Splitter splitter(Rect{0, 0, 21, 10}, std::make_unique<View>(), std::make_unique<View>());
    CK_CHECK(splitter.on_key(key(Key::Right)));
    CK_CHECK(splitter.split_position() == 11);
    CK_CHECK(splitter.first()->bounds().width == 11);
    CK_CHECK(splitter.on_key(key(Key::Left)));
    CK_CHECK(splitter.on_key(key(Key::Left)));
    CK_CHECK(splitter.split_position() == 9);
}

CK_TEST(up_and_down_move_a_vertical_splitters_split_position_by_one) {
    Splitter splitter(Rect{0, 0, 10, 21}, std::make_unique<View>(), std::make_unique<View>(),
                       Orientation::Vertical);
    CK_CHECK(splitter.on_key(key(Key::Down)));
    CK_CHECK(splitter.split_position() == 11);
    CK_CHECK(splitter.on_key(key(Key::Up)));
    CK_CHECK(splitter.on_key(key(Key::Up)));
    CK_CHECK(splitter.split_position() == 9);
}

CK_TEST(a_horizontal_splitter_ignores_up_and_down_and_a_vertical_one_ignores_left_and_right) {
    Splitter h(Rect{0, 0, 21, 10}, std::make_unique<View>(), std::make_unique<View>());
    CK_CHECK(!h.on_key(key(Key::Up)));
    CK_CHECK(!h.on_key(key(Key::Down)));
    CK_CHECK(h.split_position() == 10);

    Splitter v(Rect{0, 0, 10, 21}, std::make_unique<View>(), std::make_unique<View>(),
               Orientation::Vertical);
    CK_CHECK(!v.on_key(key(Key::Left)));
    CK_CHECK(!v.on_key(key(Key::Right)));
    CK_CHECK(v.split_position() == 10);
}

CK_TEST(an_unrelated_key_is_not_consumed) {
    Splitter splitter(Rect{0, 0, 21, 10}, std::make_unique<View>(), std::make_unique<View>());
    CK_CHECK(!splitter.on_key(key(Key::Enter)));
}

// --- Clamping ---------------------------------------------------------

CK_TEST(the_split_cannot_move_the_first_pane_below_its_own_minimum) {
    Splitter splitter(Rect{0, 0, 21, 10}, std::make_unique<MinSizedView>(3),
                       std::make_unique<View>());
    for (int i = 0; i < 20; ++i) splitter.on_key(key(Key::Left));
    CK_CHECK(splitter.split_position() == 3);
    CK_CHECK(splitter.first()->bounds().width == 3);
}

CK_TEST(the_split_cannot_shrink_the_second_pane_below_its_own_minimum) {
    Splitter splitter(Rect{0, 0, 21, 10}, std::make_unique<View>(),
                       std::make_unique<MinSizedView>(4));
    for (int i = 0; i < 20; ++i) splitter.on_key(key(Key::Right));
    // usable = 20, second min 4 -> split_position caps at 16
    CK_CHECK(splitter.split_position() == 16);
    CK_CHECK(splitter.second()->bounds().width == 4);
}

CK_TEST(set_split_position_clamps_directly_the_same_way_keyboard_adjustment_does) {
    Splitter splitter(Rect{0, 0, 21, 10}, std::make_unique<MinSizedView>(3),
                       std::make_unique<View>());
    splitter.set_split_position(-100);
    CK_CHECK(splitter.split_position() == 3);
    splitter.set_split_position(1000);
    CK_CHECK(splitter.split_position() == 20);  // usable(20) - second min(0)
}

// --- Resize behavior: absolute position preserved, not a ratio ------------

CK_TEST(growing_the_splitter_keeps_the_first_panes_absolute_size_and_grows_the_second) {
    Splitter splitter(Rect{0, 0, 21, 10}, std::make_unique<View>(), std::make_unique<View>());
    splitter.set_split_position(6);

    splitter.set_bounds(Rect{0, 0, 41, 10});

    CK_CHECK(splitter.split_position() == 6);
    CK_CHECK(splitter.first()->bounds().width == 6);
    CK_CHECK(splitter.second()->bounds() == (Rect{7, 0, 34, 10}));  // 41 - 1(divider) - 6(first)
}

CK_TEST(shrinking_the_splitter_below_the_current_split_position_re_clamps_it) {
    Splitter splitter(Rect{0, 0, 21, 10}, std::make_unique<View>(), std::make_unique<View>());
    splitter.set_split_position(15);

    splitter.set_bounds(Rect{0, 0, 11, 10});  // usable is now 10 — 15 no longer fits

    CK_CHECK(splitter.split_position() == 10);
    CK_CHECK(splitter.second()->bounds().width == 0);
}

CK_TEST(a_pane_growing_its_own_hint_without_a_splitter_resize_re_clamps_the_split) {
    auto* pane = new MinSizedView(3);
    Splitter splitter(Rect{0, 0, 21, 10}, std::unique_ptr<View>(pane), std::make_unique<View>());
    splitter.set_split_position(5);
    CK_CHECK(splitter.split_position() == 5);

    pane->set_min(8);  // no splitter resize — only the pane's own hint changed

    CK_CHECK(splitter.split_position() == 8);
}

// --- Aggregate size hints ------------------------------------------------

CK_TEST(a_horizontal_splitters_own_horizontal_hint_sums_both_panes_plus_the_divider) {
    Splitter splitter(Rect{0, 0, 21, 10}, std::make_unique<MinSizedView>(5),
                       std::make_unique<MinSizedView>(7));
    CK_CHECK(splitter.horizontal_size_hint().min == 5 + 1 + 7);
}

CK_TEST(a_horizontal_splitters_own_vertical_hint_is_the_max_of_both_panes) {
    Splitter splitter(Rect{0, 0, 21, 10}, std::make_unique<MinSizedView>(5),
                       std::make_unique<MinSizedView>(7));
    CK_CHECK(splitter.vertical_size_hint().min == 7);
}

CK_TEST(a_vertical_splitters_own_vertical_hint_sums_both_panes_plus_the_divider) {
    Splitter splitter(Rect{0, 0, 10, 21}, std::make_unique<MinSizedView>(5),
                       std::make_unique<MinSizedView>(7), Orientation::Vertical);
    CK_CHECK(splitter.vertical_size_hint().min == 5 + 1 + 7);
}

// --- Drawing ------------------------------------------------------------

CK_TEST(a_horizontal_splitter_draws_a_vertical_divider_line_at_the_split_position) {
    Fixture f;
    Splitter splitter(Rect{0, 0, 5, 3}, std::make_unique<View>(), std::make_unique<View>());
    splitter.set_context(f.ctx());
    ckv::scene::Surface s(ckv::Size{5, 3}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(s, Rect{0, 0, 5, 3});

    splitter.draw(painter);

    CK_CHECK(s.at(ckv::Point{splitter.split_position(), 0}).grapheme() == "\xe2\x94\x82");  // "│"
    CK_CHECK(s.at(ckv::Point{splitter.split_position(), 2}).grapheme() == "\xe2\x94\x82");
}

CK_TEST(a_vertical_splitter_draws_a_horizontal_divider_line_at_the_split_position) {
    Fixture f;
    Splitter splitter(Rect{0, 0, 5, 5}, std::make_unique<View>(), std::make_unique<View>(),
                       Orientation::Vertical);
    splitter.set_context(f.ctx());
    ckv::scene::Surface s(ckv::Size{5, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(s, Rect{0, 0, 5, 5});

    splitter.draw(painter);

    CK_CHECK(s.at(ckv::Point{0, splitter.split_position()}).grapheme() == "\xe2\x94\x80");  // "─"
    CK_CHECK(s.at(ckv::Point{4, splitter.split_position()}).grapheme() == "\xe2\x94\x80");
}

CK_TEST(a_zero_size_splitter_does_not_crash_on_draw_or_resize) {
    Fixture f;
    Splitter splitter(Rect{0, 0, 0, 0}, std::make_unique<View>(), std::make_unique<View>());
    splitter.set_context(f.ctx());
    ckv::scene::Surface s(ckv::Size{1, 1}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(s, Rect{0, 0, 1, 1});
    splitter.draw(painter);
    CK_CHECK(true);
}

// --- Mouse ---------------------------------------------------------------

CK_TEST(pressing_the_divider_and_moving_drags_the_split) {
    Splitter splitter(Rect{0, 0, 21, 10}, std::make_unique<View>(), std::make_unique<View>());
    CK_CHECK(splitter.split_position() == 10);
    CK_CHECK(!splitter.dragging());

    CK_CHECK(splitter.on_mouse(mouse(ckv::MouseAction::Down, 10, 4)));
    CK_CHECK(splitter.dragging());
    CK_CHECK(splitter.on_mouse(mouse(ckv::MouseAction::Move, 6, 4)));
    CK_CHECK(splitter.split_position() == 6);
    CK_CHECK(splitter.first()->bounds() == (Rect{0, 0, 6, 10}));
    CK_CHECK(splitter.second()->bounds() == (Rect{7, 0, 14, 10}));

    CK_CHECK(splitter.on_mouse(mouse(ckv::MouseAction::Up, 6, 4)));
    CK_CHECK(!splitter.dragging());
}

CK_TEST(a_press_beside_the_divider_belongs_to_the_pane_not_the_splitter) {
    // Claiming it would swallow the click a reader aimed at a list row.
    Splitter splitter(Rect{0, 0, 21, 10}, std::make_unique<View>(), std::make_unique<View>());
    CK_CHECK(!splitter.on_mouse(mouse(ckv::MouseAction::Down, 3, 4)));
    CK_CHECK(!splitter.dragging());
    CK_CHECK(!splitter.on_mouse(mouse(ckv::MouseAction::Down, 17, 4)));
    CK_CHECK(!splitter.dragging());
    CK_CHECK(splitter.split_position() == 10);
}

CK_TEST(motion_without_a_press_moves_nothing) {
    Splitter splitter(Rect{0, 0, 21, 10}, std::make_unique<View>(), std::make_unique<View>());
    CK_CHECK(!splitter.on_mouse(mouse(ckv::MouseAction::Move, 4, 4)));
    CK_CHECK(splitter.split_position() == 10);
}

CK_TEST(a_drag_obeys_the_same_minimums_the_keys_do) {
    // The pointer may leave the widget entirely; the panes' own minimums
    // are what stop the divider, exactly as for Left/Right.
    Splitter splitter(Rect{0, 0, 21, 10}, std::make_unique<MinSizedView>(4),
                      std::make_unique<MinSizedView>(5));
    CK_CHECK(splitter.on_mouse(mouse(ckv::MouseAction::Down, splitter.split_position(), 2)));
    splitter.on_mouse(mouse(ckv::MouseAction::Move, -30, 2));
    CK_CHECK(splitter.split_position() == 4);
    splitter.on_mouse(mouse(ckv::MouseAction::Move, 900, 2));
    CK_CHECK(splitter.split_position() == 15);  // 20 usable - 5 second min
}

CK_TEST(a_vertical_splitter_drags_along_its_own_axis) {
    Splitter splitter(Rect{0, 0, 10, 21}, std::make_unique<View>(), std::make_unique<View>(),
                      Orientation::Vertical);
    CK_CHECK(splitter.split_position() == 10);
    CK_CHECK(splitter.on_mouse(mouse(ckv::MouseAction::Down, 5, 10)));
    CK_CHECK(splitter.on_mouse(mouse(ckv::MouseAction::Move, 5, 3)));
    CK_CHECK(splitter.split_position() == 3);
}

CK_TEST(a_right_button_press_is_not_a_drag) {
    Splitter splitter(Rect{0, 0, 21, 10}, std::make_unique<View>(), std::make_unique<View>());
    CK_CHECK(!splitter.on_mouse(mouse(ckv::MouseAction::Down, 10, 4, ckv::MouseButton::Right)));
    CK_CHECK(!splitter.dragging());
}

CK_TEST(the_divider_roles_can_be_overridden) {
    // A splitter on a dialog surface must be able to wear that surface.
    Fixture f;
    Splitter splitter(Rect{0, 0, 21, 10}, std::make_unique<View>(), std::make_unique<View>());
    splitter.set_context(f.ctx());
    splitter.set_role_override(f.roles.dialog_background, f.roles.dialog_background);
    ckv::scene::Surface s(ckv::Size{21, 10}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(s, Rect{0, 0, 21, 10});

    splitter.draw(painter);

    const ckv::Style expected = f.theme.resolve(f.roles.dialog_background);
    CK_CHECK(s.at(ckv::Point{splitter.split_position(), 0}).style().bg == expected.bg);
}

// --- Resize anchor ---------------------------------------------------------

CK_TEST(a_splitter_anchored_on_its_second_pane_keeps_that_panes_extent_as_it_resizes) {
    Splitter splitter(Rect{0, 0, 41, 10}, std::make_unique<View>(), std::make_unique<View>());
    splitter.set_resize_anchor(ckv::widgets::SplitterPane::Second);
    splitter.set_anchored_extent(12);
    CK_CHECK(splitter.second()->bounds() == (Rect{29, 0, 12, 10}));  // 28 + divider

    splitter.set_bounds(Rect{0, 0, 61, 10});
    CK_CHECK(splitter.second()->bounds().width == 12);
    CK_CHECK(splitter.first()->bounds().width == 48);
    CK_CHECK(splitter.anchored_extent() == 12);
}

CK_TEST(the_anchored_extent_survives_a_splitter_briefly_too_small_for_it) {
    Splitter splitter(Rect{0, 0, 41, 10}, std::make_unique<View>(), std::make_unique<MinSizedView>(4));
    splitter.set_anchored_extent(30);
    splitter.set_bounds(Rect{0, 0, 21, 10});
    CK_CHECK(splitter.first()->bounds().width == 16);  // 20 usable - 4 second min
    splitter.set_bounds(Rect{0, 0, 41, 10});
    CK_CHECK(splitter.first()->bounds().width == 30);
}

CK_TEST(an_extent_asked_for_before_the_splitter_has_a_size_is_honoured_at_its_first_layout) {
    Splitter splitter(Rect{0, 0, 0, 0}, std::make_unique<View>(), std::make_unique<View>());
    splitter.set_resize_anchor(ckv::widgets::SplitterPane::Second);
    splitter.set_anchored_extent(20);
    splitter.set_bounds(Rect{0, 0, 81, 24});
    CK_CHECK(splitter.second()->bounds().width == 20);
    CK_CHECK(splitter.split_position() == 60);
}

CK_TEST(changing_the_resize_anchor_keeps_the_current_arrangement) {
    Splitter splitter(Rect{0, 0, 41, 10}, std::make_unique<View>(), std::make_unique<View>());
    splitter.set_split_position(10);
    splitter.set_resize_anchor(ckv::widgets::SplitterPane::Second);
    CK_CHECK(splitter.split_position() == 10);
    CK_CHECK(splitter.anchored_extent() == 30);
}

// --- Moves the reader makes ------------------------------------------------

CK_TEST(the_reader_moving_the_divider_is_reported_and_a_programmatic_move_is_not) {
    Splitter splitter(Rect{0, 0, 21, 10}, std::make_unique<View>(), std::make_unique<View>());
    int moves = 0;
    splitter.on_split_moved = [&] { ++moves; };

    splitter.set_split_position(6);
    splitter.set_anchored_extent(8);
    CK_CHECK(moves == 0);

    CK_CHECK(splitter.on_key(key(Key::Right)));
    CK_CHECK(moves == 1);
    CK_CHECK(splitter.anchored_extent() == 9);

    CK_CHECK(splitter.on_mouse(mouse(ckv::MouseAction::Down, 9, 2)));
    CK_CHECK(splitter.on_mouse(mouse(ckv::MouseAction::Move, 4, 2)));
    CK_CHECK(splitter.on_mouse(mouse(ckv::MouseAction::Move, 4, 2)));  // no movement, no report
    CK_CHECK(splitter.on_mouse(mouse(ckv::MouseAction::Up, 4, 2)));
    CK_CHECK(moves == 2);
    CK_CHECK(splitter.anchored_extent() == 4);
}

// --- A hidden pane ----------------------------------------------------------

CK_TEST(a_hidden_pane_takes_no_room_and_leaves_no_divider_behind) {
    Fixture f;
    Splitter splitter(Rect{0, 0, 21, 5}, std::make_unique<View>(), std::make_unique<View>());
    splitter.set_context(f.ctx());
    splitter.set_split_position(6);

    splitter.second()->set_visible(false);
    CK_CHECK(splitter.collapsed());
    CK_CHECK(splitter.first()->bounds() == (Rect{0, 0, 21, 5}));
    CK_CHECK(splitter.focus_policy() == ckv::ui::FocusPolicy::None);
    CK_CHECK(!splitter.on_key(key(Key::Left)));
    CK_CHECK(!splitter.on_mouse(mouse(ckv::MouseAction::Down, 6, 2)));
    ckv::scene::Surface s(ckv::Size{21, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    ckv::scene::Painter painter(s, Rect{0, 0, 21, 5});
    splitter.draw(painter);
    CK_CHECK(s.at(ckv::Point{6, 0}).grapheme() == " ");

    // Shown again, the pane comes back at the extent it had.
    splitter.second()->set_visible(true);
    CK_CHECK(!splitter.collapsed());
    CK_CHECK(splitter.first()->bounds() == (Rect{0, 0, 6, 5}));
    CK_CHECK(splitter.second()->bounds() == (Rect{7, 0, 14, 5}));
    CK_CHECK(splitter.focus_policy() == ckv::ui::FocusPolicy::TabStop);

    splitter.first()->set_visible(false);
    CK_CHECK(splitter.second()->bounds() == (Rect{0, 0, 21, 5}));
}

CK_TEST(a_scripted_splitter_moves_its_divider_by_keys_and_by_a_dispatched_drag) {
    // Application-level script: the splitter holds the focus in a real
    // Application, on a Desktop that repaints what its panes leave bare;
    // arrows and a press-move-release on the divider reach it through
    // dispatch, and step() shows the divider where they left it.
    ckv::term::HeadlessTerminal term(ckv::Size{30, 12});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    const StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* desktop = app.root().add(std::make_unique<ckv::widgets::Desktop>(app.root().bounds()));
    auto* splitter = desktop->add(std::make_unique<Splitter>(
        Rect{0, 0, 21, 10}, std::make_unique<MinSizedView>(3), std::make_unique<MinSizedView>(3)));
    int reported = 0;
    splitter->on_split_moved = [&] { ++reported; };
    app.set_focus(splitter);
    app.step(0);
    const auto divider_at = [&](int x) { return app.composed_surface().at(ckv::Point{x, 4}).grapheme() == "│"; };
    CK_CHECK(splitter->split_position() == 10);
    CK_CHECK(divider_at(10));

    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Left, Modifier::None, ""}}));
    CK_CHECK(app.dispatch(ckv::KeyEvent{KeyChord{Key::Left, Modifier::None, ""}}));
    app.step(0);
    CK_CHECK(splitter->split_position() == 8);
    CK_CHECK(divider_at(8));
    CK_CHECK(!divider_at(10));

    CK_CHECK(app.dispatch(mouse(ckv::MouseAction::Down, 8, 4)));
    app.dispatch(mouse(ckv::MouseAction::Move, 15, 4));
    app.dispatch(mouse(ckv::MouseAction::Up, 15, 4));
    app.step(0);
    CK_CHECK(splitter->split_position() == 15);
    CK_CHECK(divider_at(15));
    // The drag obeys the second pane's minimum as the keys do.
    app.dispatch(mouse(ckv::MouseAction::Down, 15, 4));
    app.dispatch(mouse(ckv::MouseAction::Move, 20, 4));
    app.dispatch(mouse(ckv::MouseAction::Up, 20, 4));
    CK_CHECK(splitter->split_position() == 17);
    CK_CHECK(reported >= 3);
}


CK_TEST(splitter_styles_share_clipped_handle_geometry_and_keep_the_anchor) {
    using ckv::widgets::SplitterPresentation;
    for (const auto orientation : {Orientation::Horizontal, Orientation::Vertical}) {
        Splitter split(Rect{0, 0, 20, 20}, std::make_unique<View>(), std::make_unique<View>(), orientation);
        split.set_split_position(7);
        for (const auto style : {SplitterPresentation::CentralGrip, SplitterPresentation::Gutter, SplitterPresentation::Line}) {
            split.set_presentation(style);
            CK_CHECK(split.anchored_extent() == 7);
            CK_CHECK(split.split_position() == 7);
            const auto handle = split.divider_bounds();
            const int thickness = style == SplitterPresentation::Gutter ? 3 : 1;
            CK_CHECK((orientation == Orientation::Horizontal ? handle.width : handle.height) == thickness);
            const ckv::Point point = orientation == Orientation::Horizontal ? ckv::Point{7, 3} : ckv::Point{3, 7};
            CK_CHECK(split.pointer_shape_at(point) == (orientation == Orientation::Horizontal ? ckv::PointerShape::ResizeEastWest : ckv::PointerShape::ResizeNorthSouth));
            CK_CHECK(!split.pointer_shape_at(ckv::Point{0, 0}));
        }
        split.set_resize_anchor(ckv::widgets::SplitterPane::Second);
        const int extent = split.anchored_extent();
        split.set_presentation(SplitterPresentation::Gutter);
        CK_CHECK(split.anchored_extent() == extent);
        CK_CHECK((orientation == Orientation::Horizontal ? split.second()->bounds().width : split.second()->bounds().height) == extent);
    }
}

CK_TEST(splitter_gutter_drags_keep_the_grab_offset_and_cancel_on_reconfiguration) {
    Splitter split(Rect{2, 2, 20, 8}, std::make_unique<View>(), std::make_unique<View>());
    split.set_presentation(ckv::widgets::SplitterPresentation::Gutter);
    split.set_split_position(7);
    CK_CHECK(split.on_mouse(mouse(ckv::MouseAction::Down, 11, 4))); // Right edge, offset two.
    CK_CHECK(split.on_mouse(mouse(ckv::MouseAction::Move, 11, 4)));
    CK_CHECK(split.split_position() == 7);
    split.on_mouse(mouse(ckv::MouseAction::Move, 13, 4));
    CK_CHECK(split.split_position() == 9);
    split.set_presentation(ckv::widgets::SplitterPresentation::CentralGrip);
    CK_CHECK(!split.dragging());
    CK_CHECK(!split.on_mouse(mouse(ckv::MouseAction::Move, 15, 4)));
    CK_CHECK(split.split_position() == 9);
    CK_CHECK(!split.on_mouse(mouse(ckv::MouseAction::Down, 11, 90)));
    split.set_enabled(false);
    CK_CHECK(!split.on_key(key(Key::Right)));
    CK_CHECK(!split.on_mouse(mouse(ckv::MouseAction::Down, 11, 4)));
    CK_CHECK(split.pointer_shape_at(ckv::Point{9, 2}) == ckv::PointerShape::NotAllowed);
    split.set_enabled(true);
    auto release = key(Key::Right);
    release.action = ckv::KeyAction::Release;
    CK_CHECK(!split.on_key(release));
    CK_CHECK(split.split_position() == 9);
}

CK_TEST(splitter_grips_and_tiny_gutters_paint_only_the_handle) {
    Fixture f;
    Splitter split(Rect{0, 0, 15, 7}, std::make_unique<View>(), std::make_unique<View>());
    split.set_context(f.ctx());
    split.set_split_position(6);
    split.set_presentation(ckv::widgets::SplitterPresentation::CentralGrip);
    ckv::scene::Surface surface(ckv::Size{15, 7}, ckv::Cell::from_grapheme(".", ckv::Style{}));
    ckv::scene::Painter painter(surface, Rect{0, 0, 15, 7});
    split.draw(painter);
    CK_CHECK(surface.at(ckv::Point{6, 3}).grapheme() == "⋮");
    CK_CHECK(surface.at(ckv::Point{5, 3}).grapheme() == ".");
    split.set_presentation(ckv::widgets::SplitterPresentation::Gutter);
    split.set_bounds(Rect{0, 0, 1, 1});
    CK_CHECK(split.split_position() == 0);
    CK_CHECK(split.divider_bounds() == (Rect{0, 0, 1, 1}));
    split.draw(painter);
    CK_CHECK(surface.at(ckv::Point{1, 0}).grapheme() == ".");
    split.second()->set_visible(false);
    CK_CHECK(split.divider_bounds().empty());
    CK_CHECK(!split.pointer_shape_at(ckv::Point{0, 0}));
}

CK_TEST(splitter_collapsed_hints_follow_only_the_visible_pane) {
    Splitter split(Rect{0, 0, 20, 8}, std::make_unique<MinSizedView>(2), std::make_unique<MinSizedView>(5));
    split.set_presentation(ckv::widgets::SplitterPresentation::Gutter);
    CK_CHECK(split.horizontal_size_hint().min == 10);
    split.second()->set_visible(false);
    CK_CHECK(split.horizontal_size_hint().min == 2);
    CK_CHECK(split.vertical_size_hint().min == 2);
    split.first()->set_visible(false);
    CK_CHECK(split.horizontal_size_hint().max == 0);
    CK_CHECK(split.vertical_size_hint().preferred == 0);
    split.second()->set_visible(true);
    CK_CHECK(split.horizontal_size_hint().min == 5);
    CK_CHECK(split.second()->bounds() == (Rect{0, 0, 20, 8}));
    split.first()->set_visible(true);
    split.set_bounds(Rect{0, 0, 1, 1});
    CK_CHECK(split.second()->bounds() == (Rect{1, 0, 0, 1}));
}
