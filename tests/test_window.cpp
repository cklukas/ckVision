// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/window.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/label.hpp"
#include "cvision/widgets/static_text.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/ui/layout.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;
using ckv::Point;
using ckv::Rect;
using ckv::scene::Painter;
using ckv::scene::Surface;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::ui::View;
using ckv::widgets::Edge;
using ckv::widgets::FrameSlot;
using ckv::widgets::Label;
using ckv::widgets::Window;
namespace ui = ckv::ui;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
    ckv::ui::Context ctx() { return ckv::ui::Context{&theme, &registry, nullptr}; }
};

std::unique_ptr<Window> make_window(Fixture& f, std::string title = "Test") {
    auto w = std::make_unique<Window>(std::move(title));
    w->set_context(f.ctx());
    return w;
}

ckv::MouseEvent mouse(ckv::MouseAction action, Point p) {
    return ckv::MouseEvent{action, ckv::MouseButton::Left, p, std::nullopt, Modifier::None};
}
}  // namespace

// --- Content ownership -----------------------------------------------------

CK_TEST(set_content_positions_it_to_fill_the_interior_inside_the_frame_border) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 10});
    w->set_content(std::make_unique<View>());
    CK_CHECK(w->content()->bounds() == (Rect{1, 1, 18, 8}));
}

CK_TEST(replacing_content_returns_the_previous_view_and_installs_the_new_one) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 10});
    w->set_content(std::make_unique<View>());
    View* first = w->content();
    auto returned = w->set_content(std::make_unique<View>());
    CK_CHECK(returned.get() == first);
    CK_CHECK(w->content() != first);
}

CK_TEST(setting_content_to_null_removes_the_current_content) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 10});
    w->set_content(std::make_unique<View>());
    w->set_content(nullptr);
    CK_CHECK(w->content() == nullptr);
}

CK_TEST(resizing_the_window_repositions_content_to_fill_the_new_interior) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 10});
    w->set_content(std::make_unique<View>());
    w->set_bounds(Rect{0, 0, 30, 15});
    CK_CHECK(w->content()->bounds() == (Rect{1, 1, 28, 13}));
}

CK_TEST(a_window_too_small_for_any_interior_reports_a_zero_size_content_rect_without_crashing) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 2, 2});  // no room for a 1-cell border on every side
    w->set_content(std::make_unique<View>());
    CK_CHECK(w->content()->bounds().width == 0);
    CK_CHECK(w->content()->bounds().height == 0);
}

CK_TEST(size_constraints_preserve_an_unpositioned_windows_centering_sentinel) {
    Fixture f;
    auto w = make_window(f);
    w->set_min_size(ckv::Size{30, 12});
    w->set_max_size(ckv::Size{60, 24});
    CK_CHECK(w->bounds() == (Rect{}));
    CK_CHECK(w->horizontal_size_hint().min >= 30);
    CK_CHECK(w->vertical_size_hint().min >= 12);
}

// --- content_pane(): the free-placement, resize-aware content (M10/WP-18) --

CK_TEST(content_pane_lazily_creates_an_anchor_pane_sized_to_the_interior) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 10});
    ui::AnchorPane& pane = w->content_pane();
    CK_CHECK(w->content() == &pane);
    CK_CHECK(pane.bounds() == (Rect{1, 1, 18, 8}));
}

CK_TEST(content_pane_returns_the_same_pane_on_repeated_calls) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 10});
    CK_CHECK(&w->content_pane() == &w->content_pane());
}

CK_TEST(an_anchored_child_added_through_content_pane_keeps_its_corner_on_window_resize) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 10});
    auto* label = w->content_pane().add_item(std::make_unique<View>(Rect{14, 6, 4, 1}),
                                              ui::Anchors{.right = true, .bottom = true});

    w->set_bounds(Rect{0, 0, 30, 15});

    // Interior grew from {18,8} to {28,13} (10 more wide, 5 more
    // tall); the label stays flush with the interior's right edge and
    // 1 row above its bottom edge, exactly as it was before.
    CK_CHECK(label->bounds() == (Rect{24, 11, 4, 1}));
}

CK_TEST(content_pane_on_a_window_whose_content_is_something_else_aborts) {
    CK_EXPECT_ABORT({
        Fixture f;
        auto w = make_window(f);
        w->set_content(std::make_unique<View>());  // NOT an AnchorPane
        w->content_pane();
    });
}

// --- Close protocol (vetoable) ----------------------------------------------

CK_TEST(close_with_no_handler_installed_proceeds_and_fires_on_closed) {
    Fixture f;
    auto w = make_window(f);
    bool closed = false;
    w->on_closed = [&] { closed = true; };
    CK_CHECK(w->close());
    CK_CHECK(closed);
}

CK_TEST(a_close_request_returning_false_vetoes_the_close_and_on_closed_never_fires) {
    Fixture f;
    auto w = make_window(f);
    bool closed = false;
    w->close_request = [] { return false; };
    w->on_closed = [&] { closed = true; };
    CK_CHECK(!w->close());
    CK_CHECK(!closed);
}

CK_TEST(a_close_request_returning_true_proceeds_to_on_closed) {
    Fixture f;
    auto w = make_window(f);
    bool closed = false;
    w->close_request = [] { return true; };
    w->on_closed = [&] { closed = true; };
    CK_CHECK(w->close());
    CK_CHECK(closed);
}

CK_TEST(a_recursive_close_from_a_close_callback_is_an_idempotent_no_op) {
    Fixture f;
    auto w = make_window(f);
    int close_requests = 0;
    int closed_calls = 0;
    w->close_request = [&] {
        ++close_requests;
        return true;
    };
    w->on_closed = [&] {
        ++closed_calls;
        CK_CHECK(w->close());
    };

    CK_CHECK(w->close());
    CK_CHECK(close_requests == 1);
    CK_CHECK(closed_calls == 1);
}

CK_TEST(clicking_the_close_control_invokes_close) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{5, 5, 20, 10});
    bool closed = false;
    w->on_closed = [&] { closed = true; };
    // A frame control acts on release, so the click has to be completed.
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 3, 5})));  // inside "[x]" at local x=2..4
    CK_CHECK(!closed);                                                       // the press alone decides nothing
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Up, Point{5 + 3, 5})));
    CK_CHECK(closed);
}

// --- Zoom/restore ------------------------------------------------------

CK_TEST(toggle_zoom_grows_to_the_supplied_available_rect_and_toggling_again_restores) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(!w->zoomed());
    w->toggle_zoom(Rect{0, 0, 80, 24});
    CK_CHECK(w->zoomed());
    CK_CHECK(w->bounds() == (Rect{0, 0, 80, 24}));
    w->toggle_zoom(Rect{0, 0, 80, 24});
    CK_CHECK(!w->zoomed());
    CK_CHECK(w->bounds() == (Rect{5, 5, 20, 10}));
}

CK_TEST(clicking_the_zoom_control_toggles_zoom) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 17, 5})));  // inside "[]" near the top-right
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Up, Point{5 + 17, 5})));
    CK_CHECK(w->zoomed());
}

CK_TEST(a_standalone_windows_zoom_control_uses_its_own_current_bounds_as_a_harmless_target) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 17, 5})));
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Up, Point{5 + 17, 5})));
    CK_CHECK(w->zoomed());
    CK_CHECK(w->bounds() == (Rect{5, 5, 20, 10}));  // unchanged: no provider, nothing else to target
}

CK_TEST(fill_resizes_to_the_target_rect_honoring_min_and_max_size) {
    Fixture f;
    auto w = make_window(f);
    w->set_min_size(ckv::Size{10, 4});
    w->set_max_size(ckv::Size{50, 20});
    w->set_bounds(Rect{5, 5, 20, 10});

    w->fill(Rect{2, 3, 60, 25});  // exceeds max in both dimensions
    CK_CHECK(w->bounds() == (Rect{2, 3, 50, 20}));

    w->fill(Rect{1, 1, 5, 2});  // below min in both dimensions
    CK_CHECK(w->bounds() == (Rect{1, 1, 10, 4}));
}

CK_TEST(refresh_zoom_area_is_a_no_op_unless_zoomed) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{5, 5, 20, 10});
    w->refresh_zoom_area(Rect{0, 0, 80, 24});
    CK_CHECK(w->bounds() == (Rect{5, 5, 20, 10}));  // untouched — never zoomed
}

CK_TEST(refresh_zoom_area_refills_the_new_target_and_leaves_restored_bounds_untouched) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{5, 5, 20, 10});
    w->toggle_zoom(Rect{0, 0, 80, 24});
    CK_CHECK(w->bounds() == (Rect{0, 0, 80, 24}));

    w->refresh_zoom_area(Rect{0, 0, 120, 40});  // e.g. the desktop grew while zoomed
    CK_CHECK(w->zoomed());
    CK_CHECK(w->bounds() == (Rect{0, 0, 120, 40}));

    w->toggle_zoom(Rect{0, 0, 120, 40});  // un-zoom: restores the ORIGINAL pre-zoom bounds
    CK_CHECK(!w->zoomed());
    CK_CHECK(w->bounds() == (Rect{5, 5, 20, 10}));
}

// --- Mouse move and resize --------------------------------------------------

CK_TEST(dragging_the_title_bar_moves_the_window) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 10, 5})));  // title bar, not a control
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Move, Point{5 + 10 + 3, 5 + 2})));
    CK_CHECK(w->bounds() == (Rect{8, 7, 20, 10}));
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Up, Point{8 + 10, 7})));
}

CK_TEST(a_non_movable_window_ignores_a_title_bar_drag) {
    Fixture f;
    auto w = make_window(f);
    w->set_movable(false);
    w->set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(!w->on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 10, 5})));
}

CK_TEST(dragging_the_resize_grip_resizes_the_window_without_moving_its_origin) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 19, 5 + 9})));  // bottom-right corner
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Move, Point{5 + 19 + 5, 5 + 9 + 3})));
    CK_CHECK(w->bounds() == (Rect{5, 5, 25, 13}));
}

CK_TEST(dragging_the_bottom_left_resize_grip_resizes_while_anchoring_the_right_edge) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{5, 5 + 9})));
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Move, Point{5 - 4, 5 + 9 + 3})));
    CK_CHECK(w->bounds() == (Rect{1, 5, 24, 13}));
}

CK_TEST(resizing_below_the_minimum_size_clamps_rather_than_shrinking_further) {
    Fixture f;
    auto w = make_window(f);
    w->set_min_size(ckv::Size{10, 4});
    w->set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 19, 5 + 9})));
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Move, Point{5 - 100, 5 - 100})));  // drag far past the minimum
    CK_CHECK(w->bounds().width == 10);
    CK_CHECK(w->bounds().height == 4);
}

CK_TEST(resizing_above_the_maximum_size_clamps) {
    Fixture f;
    auto w = make_window(f);
    w->set_max_size(ckv::Size{25, 12});
    w->set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 19, 5 + 9})));
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Move, Point{5 + 100, 5 + 100})));
    CK_CHECK(w->bounds().width == 25);
    CK_CHECK(w->bounds().height == 12);
}

CK_TEST(a_non_resizable_window_ignores_a_grip_drag) {
    Fixture f;
    auto w = make_window(f);
    w->set_resizable(false);
    w->set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(!w->on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 19, 5 + 9})));
    CK_CHECK(!w->on_mouse(mouse(ckv::MouseAction::Down, Point{5, 5 + 9})));
}

CK_TEST(a_click_in_the_interior_is_not_handled_by_the_window_itself) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(!w->on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 10, 5 + 5})));
}

CK_TEST(move_and_up_without_a_prior_down_are_unhandled) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(!w->on_mouse(mouse(ckv::MouseAction::Move, Point{5 + 10, 5 + 10})));
    CK_CHECK(!w->on_mouse(mouse(ckv::MouseAction::Up, Point{5 + 10, 5 + 10})));
}

// --- Keyboard move/resize mode -----------------------------------------

CK_TEST(arrow_keys_are_unhandled_when_not_in_keyboard_mode) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(!w->on_key(ckv::KeyEvent{KeyChord{Key::Left, Modifier::None, ""}}));
}

CK_TEST(move_mode_arrow_keys_translate_the_window) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{5, 5, 20, 10});
    w->enter_move_mode();
    CK_CHECK(w->in_keyboard_mode());
    w->on_key(ckv::KeyEvent{KeyChord{Key::Right, Modifier::None, ""}});
    w->on_key(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}});
    CK_CHECK(w->bounds() == (Rect{6, 6, 20, 10}));
}

CK_TEST(resize_mode_arrow_keys_change_size_not_position) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{5, 5, 20, 10});
    w->enter_resize_mode();
    w->on_key(ckv::KeyEvent{KeyChord{Key::Right, Modifier::None, ""}});
    w->on_key(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}});
    CK_CHECK(w->bounds() == (Rect{5, 5, 21, 11}));
}

CK_TEST(escape_in_keyboard_mode_reverts_to_the_bounds_at_mode_entry) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{5, 5, 20, 10});
    w->enter_move_mode();
    w->on_key(ckv::KeyEvent{KeyChord{Key::Right, Modifier::None, ""}});
    w->on_key(ckv::KeyEvent{KeyChord{Key::Right, Modifier::None, ""}});
    w->on_key(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}});
    CK_CHECK(w->bounds() == (Rect{5, 5, 20, 10}));
    CK_CHECK(!w->in_keyboard_mode());
}

CK_TEST(enter_in_keyboard_mode_confirms_and_keeps_the_new_bounds) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{5, 5, 20, 10});
    w->enter_move_mode();
    w->on_key(ckv::KeyEvent{KeyChord{Key::Right, Modifier::None, ""}});
    w->on_key(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}});
    CK_CHECK(w->bounds() == (Rect{6, 5, 20, 10}));
    CK_CHECK(!w->in_keyboard_mode());
}

CK_TEST(resize_mode_respects_the_minimum_size_when_nudging_smaller) {
    Fixture f;
    auto w = make_window(f);
    w->set_min_size(ckv::Size{10, 4});
    w->set_bounds(Rect{5, 5, 10, 4});  // already at the minimum
    w->enter_resize_mode();
    w->on_key(ckv::KeyEvent{KeyChord{Key::Left, Modifier::None, ""}});   // would shrink width below min
    w->on_key(ckv::KeyEvent{KeyChord{Key::Up, Modifier::None, ""}});     // would shrink height below min
    CK_CHECK(w->bounds().width == 10);
    CK_CHECK(w->bounds().height == 4);
}

// --- Active / inactive ---------------------------------------------------

CK_TEST(a_new_window_starts_inactive) {
    Fixture f;
    auto w = make_window(f);
    CK_CHECK(!w->active());
}

CK_TEST(set_active_toggles_and_setting_the_same_value_is_a_no_op) {
    Fixture f;
    auto w = make_window(f);
    int calls = 0;
    w->set_dirty_rect_sink([&](Rect) { ++calls; });
    w->set_active(true);
    CK_CHECK(w->active());
    const int after_first = calls;
    w->set_active(true);  // no-op
    CK_CHECK(calls == after_first);
}

// --- Rendering does not crash ------------------------------------------

CK_TEST(draw_does_not_crash_for_a_window_too_narrow_to_show_controls_or_title) {
    Fixture f;
    Surface s(ckv::Size{4, 3}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f, "Some very long title that will not fit");
    w->set_bounds(Rect{0, 0, 4, 3});
    Painter painter(s, Rect{0, 0, 4, 3});
    w->draw(painter);
    CK_CHECK(true);
}

CK_TEST(draw_truncates_a_title_longer_than_the_available_space) {
    Fixture f;
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f, "A title far too long to fit in this window");
    w->set_bounds(Rect{0, 0, 20, 5});
    Painter painter(s, Rect{0, 0, 20, 5});
    w->draw(painter);  // must not crash or write past bounds (surface itself is the bound)
    CK_CHECK(true);
}

CK_TEST(window_title_elision_never_splits_an_emoji_sequence) {
    Fixture f;
    Surface s(ckv::Size{14, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    const std::string family = "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9";
    auto w = make_window(f, "A" + family + "BC");
    w->set_bounds(Rect{0, 0, 14, 5});
    Painter painter(s, Rect{0, 0, 14, 5});
    w->draw(painter);

    // Four title columns are available: A + the two-column sequence + U+2026.
    CK_CHECK(s.at(ckv::Point{5, 0}).grapheme() == "A");
    CK_CHECK(s.at(ckv::Point{6, 0}).grapheme() == family);
    CK_CHECK(s.at(ckv::Point{8, 0}).grapheme() == "\xE2\x80\xA6");
}

CK_TEST(the_title_is_centered_with_a_padding_space_on_each_side_not_left_aligned) {
    // The title width is bounded by frame controls, then centered with
    // one-cell padding on both sides. Previously the title was drawn
    // left-aligned at a fixed column, immediately abutting the close
    // control with no padding.
    Fixture f;
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f, "Test");  // 4 columns wide
    w->set_bounds(Rect{0, 0, 20, 5});
    Painter painter(s, Rect{0, 0, 20, 5});
    w->draw(painter);

    // width 20, title width 4: start = (20 - 4) / 2 = 8.
    CK_CHECK(s.at(Point{7, 0}).grapheme() == " ");   // padding before
    CK_CHECK(s.at(Point{8, 0}).grapheme() == "T");   // title starts here
    CK_CHECK(s.at(Point{11, 0}).grapheme() == "t");  // title ends here ("Test"[3])
    CK_CHECK(s.at(Point{12, 0}).grapheme() == " ");  // padding after
}

CK_TEST(a_short_title_never_abuts_the_close_control_directly) {
    Fixture f;
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f, "X");
    w->set_bounds(Rect{0, 0, 20, 5});
    Painter painter(s, Rect{0, 0, 20, 5});
    w->draw(painter);
    // Close control occupies columns 2-4 ("[■]"); the title, centered,
    // must start well clear of it with its own padding cell besides.
    CK_CHECK(s.at(Point{4, 0}).grapheme() != "X");
}

CK_TEST(a_fixed_dialog_title_is_centered_on_the_complete_frame) {
    Fixture f;
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f, "Fixed");
    w->set_resizable(false);
    w->set_bounds(Rect{0, 0, 20, 5});
    Painter painter(s, Rect{0, 0, 20, 5});
    w->draw(painter);

    // width 20, title width 5: (20 - 5) / 2 = 7. The close control
    // remains present, but it does not displace the visual centre.
    CK_CHECK(s.at(Point{7, 0}).grapheme() == "F");
    CK_CHECK(s.at(Point{11, 0}).grapheme() == "d");
}

// --- Content interior fill (windowed-desktop fidelity) --------------------

CK_TEST(the_content_interior_fills_with_the_frame_style_not_left_blank) {
    // Regression: a window with sparse content (content covering only
    // some cells, not a full-bleed panel) used to show raw, unstyled
    // Surface fill anywhere a child widget didn't happen to paint —
    // reading as a visibly broken patch rather than one solid window.
    Fixture f;
    Surface s(ckv::Size{20, 10}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 10});
    // set_content() is not even called — an entirely childless window
    // must still fill its whole interior.
    Painter painter(s, Rect{0, 0, 20, 10});
    w->draw(painter);

    const ckv::Style expected = f.theme.resolve(f.roles.window_frame_inactive);
    for (int y = 1; y < 9; ++y)
        for (int x = 1; x < 19; ++x) CK_CHECK(s.at(Point{x, y}).style() == expected);
}

CK_TEST(the_content_fill_uses_the_active_frame_style_when_the_window_is_active) {
    Fixture f;
    Surface s(ckv::Size{20, 10}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 10});
    w->set_active(true);
    Painter painter(s, Rect{0, 0, 20, 10});
    w->draw(painter);

    const ckv::Style expected = f.theme.resolve(f.roles.window_frame_active);
    CK_CHECK(s.at(Point{10, 5}).style() == expected);
}

// --- Frame style and control glyphs (windowed-desktop fidelity) ----------

CK_TEST(an_inactive_window_draws_with_single_line_box_glyphs) {
    Fixture f;
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 5});
    Painter painter(s, Rect{0, 0, 20, 5});
    w->draw(painter);
    CK_CHECK(s.at(Point{0, 0}).grapheme() == "┌");
    CK_CHECK(s.at(Point{19, 0}).grapheme() == "┐");
    CK_CHECK(s.at(Point{10, 4}).grapheme() == "─");
    CK_CHECK(s.at(Point{0, 2}).grapheme() == "│");
}

CK_TEST(an_active_window_draws_with_double_line_box_glyphs) {
    // The convention's own visual signal for "this window has focus" —
    // distinct GLYPHS, not merely a color change, so it still reads on
    // a monochrome terminal.
    Fixture f;
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 5});
    w->set_active(true);
    Painter painter(s, Rect{0, 0, 20, 5});
    w->draw(painter);
    CK_CHECK(s.at(Point{0, 0}).grapheme() == "╔");
    CK_CHECK(s.at(Point{19, 0}).grapheme() == "╗");
    // Three corners are plain double-line frame; the bottom right carries
    // the resize mark, which is single-line by convention.
    CK_CHECK(s.at(Point{0, 4}).grapheme() == "╚");
    CK_CHECK(s.at(Point{19, 4}).grapheme() == "┘");
    CK_CHECK(s.at(Point{10, 4}).grapheme() == "═");
    CK_CHECK(s.at(Point{0, 2}).grapheme() == "║");
}

CK_TEST(an_active_non_resizable_window_keeps_double_line_lower_corners) {
    Fixture f;
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 5});
    w->set_active(true);
    w->set_resizable(false);
    Painter painter(s, Rect{0, 0, 20, 5});
    w->draw(painter);
    CK_CHECK(s.at(Point{0, 4}).grapheme() == "╚");
    CK_CHECK(s.at(Point{19, 4}).grapheme() == "╝");
}

CK_TEST(reactivating_and_deactivating_a_window_switches_its_frame_style_back_and_forth) {
    Fixture f;
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 5});
    Painter painter(s, Rect{0, 0, 20, 5});

    w->set_active(true);
    w->draw(painter);
    CK_CHECK(s.at(Point{10, 4}).grapheme() == "═");

    w->set_active(false);
    w->draw(painter);
    CK_CHECK(s.at(Point{10, 4}).grapheme() == "─");
}

// --- Role resolution (M9 WP-7, D-028) ---------------------------------

CK_TEST(an_attached_window_resolves_the_default_document_window_roles) {
    Fixture f;
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 5});
    w->set_active(true);
    Painter painter(s, Rect{0, 0, 20, 5});
    w->draw(painter);
    // The frame's own style comes from ckv.window.frame.active by
    // default — never explicitly passed to the constructor.
    CK_CHECK(s.at(Point{0, 0}).style() == f.theme.resolve(f.roles.window_frame_active));
}

CK_TEST(set_role_override_redirects_all_four_roles_and_takes_effect_immediately) {
    Fixture f;
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_role_override(f.roles.dialog_frame, f.roles.dialog_background, f.roles.dialog_frame,
                          f.roles.dialog_background);
    w->set_bounds(Rect{0, 0, 20, 5});
    w->set_active(true);
    Painter painter(s, Rect{0, 0, 20, 5});
    w->draw(painter);
    // Dialog-gray chrome, NOT the default document-window blue.
    CK_CHECK(s.at(Point{0, 0}).style() == f.theme.resolve(f.roles.dialog_frame));
    CK_CHECK(s.at(Point{0, 0}).style() != f.theme.resolve(f.roles.window_frame_active));
}

CK_TEST(set_role_override_called_before_attachment_is_not_overwritten_by_on_attached) {
    Fixture f;
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Window w("W");
    w.set_role_override(f.roles.dialog_frame, f.roles.dialog_background, f.roles.dialog_frame,
                         f.roles.dialog_background);
    w.set_context(f.ctx());  // fires on_attached() — must not clobber the override set above
    w.set_bounds(Rect{0, 0, 20, 5});
    w.set_active(true);
    Painter painter(s, Rect{0, 0, 20, 5});
    w.draw(painter);
    CK_CHECK(s.at(Point{0, 0}).style() == f.theme.resolve(f.roles.dialog_frame));
}

CK_TEST(the_close_control_draws_a_filled_square_glyph) {
    Fixture f;
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 5});
    w->set_active(true);  // controls wear their own colour on the active window
    Painter painter(s, Rect{0, 0, 20, 5});
    w->draw(painter);
    CK_CHECK(s.at(Point{1, 0}).grapheme() == "═");  // active frames are double-line
    CK_CHECK(s.at(Point{2, 0}).grapheme() == "[");
    CK_CHECK(s.at(Point{3, 0}).grapheme() == "■");
    CK_CHECK(s.at(Point{4, 0}).grapheme() == "]");
    CK_CHECK(s.at(Point{3, 0}).style() == f.theme.resolve(f.roles.window_control));
}

CK_TEST(the_zoom_control_draws_an_up_arrow_when_not_zoomed) {
    Fixture f;
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 5});
    w->set_active(true);  // controls wear their own colour on the active window
    Painter painter(s, Rect{0, 0, 20, 5});
    w->draw(painter);
    CK_CHECK(!w->zoomed());
    CK_CHECK(!w->maximized());
    CK_CHECK(s.at(Point{16, 0}).grapheme() == "↑");
    CK_CHECK(s.at(Point{16, 0}).style() == f.theme.resolve(f.roles.window_control));
}

CK_TEST(a_non_resizable_window_omits_and_ignores_the_zoom_control) {
    Fixture f;
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_resizable(false);
    w->set_bounds(Rect{0, 0, 20, 5});
    Painter painter(s, Rect{0, 0, 20, 5});
    w->draw(painter);

    CK_CHECK(s.at(Point{16, 0}).grapheme() != "↑");
    (void)w->on_mouse(mouse(ckv::MouseAction::Down, Point{17, 0}));
    CK_CHECK(!w->zoomed());
}

CK_TEST(the_zoom_control_draws_an_updown_arrow_once_zoomed) {
    Fixture f;
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 5});
    w->toggle_zoom(Rect{0, 0, 20, 5});
    Painter painter(s, Rect{0, 0, 20, 5});
    w->draw(painter);
    CK_CHECK(w->zoomed());
    CK_CHECK(w->maximized());
    CK_CHECK(s.at(Point{16, 0}).grapheme() == "↕");
}

CK_TEST(a_permanently_filling_window_draws_the_restore_glyph_without_entering_restorable_zoom) {
    Fixture f;
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 5});
    w->set_grow_policy(ckv::widgets::DesktopGrowPolicy::KeepFilling);
    Painter painter(s, Rect{0, 0, 20, 5});
    w->draw(painter);
    CK_CHECK(!w->zoomed());
    CK_CHECK(w->maximized());
    CK_CHECK(s.at(Point{16, 0}).grapheme() == "↕");
}

CK_TEST(toggling_zoom_off_again_restores_the_up_arrow_glyph) {
    Fixture f;
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 5});
    Painter painter(s, Rect{0, 0, 20, 5});

    w->toggle_zoom(Rect{0, 0, 20, 5});
    w->toggle_zoom(Rect{0, 0, 20, 5});
    CK_CHECK(!w->zoomed());
    w->draw(painter);
    CK_CHECK(s.at(Point{16, 0}).grapheme() == "↑");
}

CK_TEST(the_zoom_control_hit_test_region_is_unaffected_by_the_glyph_change) {
    // point_in_zoom_control's own hit-test geometry assumes a fixed
    // 3-cell-wide "[X]" control regardless of what X is — the glyph
    // swap must not have silently widened/narrowed the clickable
    // region (both "↑" and "↕" are single-width like the "x"/blank
    // they replaced).
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 17, 5})));
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Up, Point{5 + 17, 5})));
    CK_CHECK(w->zoomed());
}

// --- reposition_within (the architecture §5 "Sizing policy") ---------------

CK_TEST(reposition_within_is_a_no_op_when_the_window_already_fits) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{2, 2, 20, 10});
    w->reposition_within(Rect{0, 0, 80, 24});
    CK_CHECK(w->bounds() == (Rect{2, 2, 20, 10}));
}

CK_TEST(reposition_within_shrinks_a_window_wider_than_the_available_area) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 60, 10});
    w->reposition_within(Rect{0, 0, 40, 24});
    CK_CHECK(w->bounds().width == 40);
}

CK_TEST(reposition_within_never_shrinks_below_min_size) {
    Fixture f;
    auto w = make_window(f);
    w->set_min_size(ckv::Size{15, 6});
    w->set_bounds(Rect{0, 0, 20, 10});
    w->reposition_within(Rect{0, 0, 10, 4});  // smaller than min_size in both dimensions
    CK_CHECK(w->bounds().width == 15);
    CK_CHECK(w->bounds().height == 6);
}

CK_TEST(reposition_within_pins_to_the_available_areas_origin_when_the_window_cannot_fully_fit) {
    Fixture f;
    auto w = make_window(f);
    w->set_min_size(ckv::Size{15, 6});
    w->set_bounds(Rect{50, 50, 20, 10});
    w->reposition_within(Rect{2, 3, 10, 4});  // available is smaller than min_size
    CK_CHECK(w->bounds().x == 2);
    CK_CHECK(w->bounds().y == 3);
}

CK_TEST(reposition_within_pulls_a_window_back_that_moved_off_the_shrunk_area) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{70, 20, 15, 6});  // was reachable in an 80x24 desktop
    w->reposition_within(Rect{0, 0, 40, 24});  // desktop shrunk to 40 wide
    CK_CHECK(w->bounds().x + w->bounds().width <= 40);
    CK_CHECK(w->bounds().x >= 0);
}

CK_TEST(reposition_within_leaves_a_window_alone_if_only_its_position_already_fits) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{5, 5, 15, 6});
    w->reposition_within(Rect{0, 0, 40, 24});
    CK_CHECK(w->bounds() == (Rect{5, 5, 15, 6}));
}

// --- Frame overlays (M10/WP-20: plural, positioned, non-content children
// living on the border itself) --------------------------------------------

namespace {
class FixedWidthView : public View {
public:
    explicit FixedWidthView(int preferred_width) : preferred_width_(preferred_width) {}
    void set_preferred_width(int width) {
        preferred_width_ = width;
        size_hint_changed();
    }
    ckv::ui::SizeHint horizontal_size_hint() const override {
        return ckv::ui::SizeHint{0, preferred_width_, ckv::ui::kUnboundedExtent};
    }

private:
    int preferred_width_;
};
}  // namespace

CK_TEST(the_default_slot_positions_an_overlay_on_the_bottom_border_right_aligned) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 30, 10});
    auto* overlay = w->add_frame_overlay(std::make_unique<FixedWidthView>(6), FrameSlot{});
    CK_CHECK(overlay->bounds() == (Rect{30 - 2 - 6, 9, 6, 1}));
}

CK_TEST(frame_overlay_repositions_on_window_resize) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 30, 10});
    auto* overlay = w->add_frame_overlay(std::make_unique<FixedWidthView>(4), FrameSlot{});
    w->set_bounds(Rect{0, 0, 50, 20});
    CK_CHECK(overlay->bounds() == (Rect{50 - 2 - 4, 19, 4, 1}));
}

CK_TEST(frame_overlay_width_is_clamped_to_fit_a_narrow_window) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 8, 5});  // interior margin leaves very little room
    auto* overlay = w->add_frame_overlay(std::make_unique<FixedWidthView>(20), FrameSlot{});
    CK_CHECK(overlay->bounds().width <= 8 - 4);
    CK_CHECK(overlay->bounds().x >= 0);
}

CK_TEST(a_side_frame_overlay_stands_on_the_border_between_the_corners) {
    // The classic desktop's place for a scrollbar: the window's own border,
    // one cell wide, spanning the rows between the title's corner cell and
    // the resize grip's — so it costs the content nothing.
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 30, 10});
    auto* right = w->add_frame_overlay(std::make_unique<View>(),
                                       FrameSlot{Edge::Right, ckv::ui::Alignment::Fill});
    CK_CHECK(right->bounds() == (Rect{29, 1, 1, 8}));
    auto* left = w->add_frame_overlay(std::make_unique<View>(),
                                      FrameSlot{Edge::Left, ckv::ui::Alignment::Fill});
    CK_CHECK(left->bounds() == (Rect{0, 1, 1, 8}));

    // And it keeps standing there as the window resizes.
    w->set_bounds(Rect{0, 0, 50, 20});
    CK_CHECK(right->bounds() == (Rect{49, 1, 1, 18}));
    CK_CHECK(left->bounds() == (Rect{0, 1, 1, 18}));
}

CK_TEST(remove_frame_overlay_returns_ownership_and_stops_tracking_it) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 30, 10});
    ui::View* raw = w->add_frame_overlay(std::make_unique<FixedWidthView>(5), FrameSlot{});

    auto owned = w->remove_frame_overlay(raw);
    CK_CHECK(owned != nullptr);
    CK_CHECK(owned.get() == raw);

    w->set_bounds(Rect{0, 0, 60, 20});  // must not crash touching a stale spec
}

CK_TEST(remove_frame_overlay_for_a_view_not_owned_by_this_window_returns_null) {
    Fixture f;
    auto w = make_window(f);
    View stray;
    CK_CHECK(w->remove_frame_overlay(&stray) == nullptr);
}

CK_TEST(frame_overlay_and_content_coexist_as_independent_children) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 30, 10});
    w->set_content(std::make_unique<View>());
    auto* overlay = w->add_frame_overlay(std::make_unique<FixedWidthView>(6), FrameSlot{});
    CK_CHECK(w->content() != nullptr);
    CK_CHECK(overlay != nullptr);
    CK_CHECK(w->content() != overlay);
    // content() still fills the ordinary interior, untouched by the overlay.
    CK_CHECK(w->content()->bounds() == (Rect{1, 1, 28, 8}));
}

// --- Multiple, positioned overlays (the acceptance scenario: a path
// indicator bottom-left, a line:col readout bottom-right) -----------------

CK_TEST(two_overlays_at_different_bottom_alignments_coexist_without_overlapping) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 40, 10});
    const FrameSlot bottom_start{.edge = Edge::Bottom, .alignment = ui::Alignment::Start};
    auto* path = w->add_frame_overlay(std::make_unique<FixedWidthView>(6), bottom_start);
    // Default slot is Bottom/End.
    auto* line_col = w->add_frame_overlay(std::make_unique<FixedWidthView>(5), FrameSlot{});

    CK_CHECK(path->bounds() == (Rect{2, 9, 6, 1}));
    CK_CHECK(line_col->bounds() == (Rect{40 - 2 - 5, 9, 5, 1}));
    CK_CHECK(path->bounds().x + path->bounds().width <= line_col->bounds().x);  // no overlap
}

CK_TEST(a_top_start_overlay_sits_flush_after_the_close_control) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 30, 10});  // wide enough for the close control (b.width > 6)
    const FrameSlot top_start{.edge = Edge::Top, .alignment = ui::Alignment::Start};
    auto* overlay = w->add_frame_overlay(std::make_unique<FixedWidthView>(4), top_start);
    CK_CHECK(overlay->bounds() == (Rect{5, 0, 4, 1}));  // close control occupies columns 2-4
}

CK_TEST(a_top_end_overlay_sits_flush_before_the_zoom_control) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 30, 10});  // wide enough for the zoom control (b.width > 8)
    const FrameSlot top_end{.edge = Edge::Top, .alignment = ui::Alignment::End};
    auto* overlay = w->add_frame_overlay(std::make_unique<FixedWidthView>(4), top_end);
    // The right-hand controls are one group, and at 30 columns that is three
    // of them: minimize at width-8..width-6 and zoom at width-5..width-3,
    // so an End overlay clears seven cells rather than four.
    CK_CHECK(overlay->bounds() == (Rect{30 - 7 - 4, 0, 4, 1}));
}

CK_TEST(a_top_start_overlay_on_a_narrow_window_only_clears_the_corner) {
    Fixture f;
    auto w = make_window(f);
    // b.width == 6: too narrow for the close control to draw at all.
    w->set_bounds(Rect{0, 0, 6, 5});
    const FrameSlot top_start{.edge = Edge::Top, .alignment = ui::Alignment::Start};
    auto* overlay = w->add_frame_overlay(std::make_unique<FixedWidthView>(2), top_start);
    CK_CHECK(overlay->bounds().x == 1);
}

CK_TEST(an_offset_nudges_the_overlay_from_its_aligned_position) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 30, 10});
    auto* overlay = w->add_frame_overlay(
        std::make_unique<FixedWidthView>(4),
        FrameSlot{.edge = Edge::Bottom, .alignment = ui::Alignment::Start, .offset = 3});
    CK_CHECK(overlay->bounds().x == 2 + 3);  // the plain Start margin (2), pushed right by 3
}

CK_TEST(an_offset_is_clamped_so_the_overlay_never_leaves_its_own_row) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 30, 10});
    auto* overlay = w->add_frame_overlay(
        std::make_unique<FixedWidthView>(4),
        FrameSlot{.edge = Edge::Bottom, .alignment = ui::Alignment::Start, .offset = 1000});
    // Right margin boundary: still inside it, never past it.
    CK_CHECK(overlay->bounds().x + overlay->bounds().width <= 30 - 2);
}

CK_TEST(a_second_overlay_at_an_already_occupied_slot_aborts) {
    CK_EXPECT_ABORT({
        Fixture f;
        auto w = make_window(f);
        w->set_bounds(Rect{0, 0, 30, 10});
        w->add_frame_overlay(std::make_unique<FixedWidthView>(4), FrameSlot{});
        // Same default slot again.
        w->add_frame_overlay(std::make_unique<FixedWidthView>(3), FrameSlot{});
    });
}

CK_TEST(a_top_centered_or_filled_overlay_aborts_since_the_title_owns_that_position) {
    CK_EXPECT_ABORT({
        Fixture f;
        auto w = make_window(f);
        w->set_bounds(Rect{0, 0, 30, 10});
        w->add_frame_overlay(std::make_unique<FixedWidthView>(4),
                              FrameSlot{.edge = Edge::Top, .alignment = ui::Alignment::Center});
    });
}

// --- Size-hint-change propagation (M9/WP-16), now per-overlay -------------

CK_TEST(the_overlay_repositions_automatically_when_its_own_preferred_width_changes) {
    // A changed preferred size propagates through size_hint_changed()
    // (M9/WP-16) — no manual relayout call needed.
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 30, 10});
    auto* overlay = w->add_frame_overlay(std::make_unique<FixedWidthView>(4), FrameSlot{});
    CK_CHECK(overlay->bounds().width == 4);

    static_cast<FixedWidthView*>(overlay)->set_preferred_width(10);
    CK_CHECK(overlay->bounds().width == 10);
    CK_CHECK(overlay->bounds() == (Rect{30 - 2 - 10, 9, 10, 1}));
}

CK_TEST(a_content_child_changing_its_own_size_hint_does_not_touch_any_overlay) {
    // on_child_size_hint_changed() fires for ANY direct child — content()
    // is one too — but content_rect() depends only on Window's own
    // bounds(), never on content()'s hint, so this must be a no-op.
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 30, 10});
    auto* overlay = w->add_frame_overlay(std::make_unique<FixedWidthView>(4), FrameSlot{});
    const Rect before = overlay->bounds();

    auto content = std::make_unique<FixedWidthView>(6);
    FixedWidthView* content_ptr = content.get();
    w->set_content(std::move(content));
    content_ptr->set_preferred_width(20);

    CK_CHECK(overlay->bounds() == before);
}

CK_TEST(one_overlays_hint_changing_does_not_reposition_a_different_overlay) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 40, 10});
    const FrameSlot bottom_start{.edge = Edge::Bottom, .alignment = ui::Alignment::Start};
    auto* start = w->add_frame_overlay(std::make_unique<FixedWidthView>(4), bottom_start);
    // Default slot is Bottom/End.
    auto* end = w->add_frame_overlay(std::make_unique<FixedWidthView>(4), FrameSlot{});
    const Rect end_before = end->bounds();

    static_cast<FixedWidthView*>(start)->set_preferred_width(10);

    CK_CHECK(end->bounds() == end_before);
}

CK_TEST(a_growing_overlay_label_stays_right_aligned_live) {
    // The concrete acceptance scenario (M9/WP-16): filebrowser_app.cpp's
    // path_label_ used to need a manual relayout_frame_overlay() call
    // after set_text() — now it doesn't.
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 40, 10});
    auto* overlay = w->add_frame_overlay(std::make_unique<Label>("/a"), FrameSlot{});
    const Rect narrow = overlay->bounds();

    static_cast<Label*>(overlay)->set_text("/a/much/longer/path");

    const Rect wide = overlay->bounds();
    CK_CHECK(wide.width > narrow.width);
    const int right_edge = narrow.x + narrow.width;
    CK_CHECK(wide.x + wide.width == right_edge);  // still flush against the same margin
}

CK_TEST(a_resize_grip_reaches_along_the_border_and_the_whole_reach_resizes) {
    Fixture f;
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 5});
    w->set_active(true);
    w->set_resizable(true);
    Painter painter(s, Rect{0, 0, 20, 5});
    w->draw(painter);

    // The corner is still the corner; the cell beside it now belongs to the
    // grip too, drawn as border so it costs no content space.
    // Idle, the bottom right alone is marked -- the mark the classic desktop
    // uses -- and the cell beside it belongs to the grip too.
    CK_CHECK(s.at(Point{19, 4}).grapheme() == "┘");
    CK_CHECK(s.at(Point{18, 4}).grapheme() == "─");
    CK_CHECK(s.at(Point{0, 4}).grapheme() == "╚");  // quiet until a resize starts

    // ...and the widened cells resize, exactly as the corners do. An
    // affordance the pointer can see but not use is worse than none.
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{18, 4})));
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Move, Point{24, 7})));
    CK_CHECK(w->bounds().width > 20);
    CK_CHECK(w->bounds().height > 5);
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Up, Point{24, 7})));
}

CK_TEST(the_left_grips_widened_cell_resizes_from_the_left_edge) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{5, 5, 20, 10});
    w->set_resizable(true);
    // One cell in from the bottom-left corner.
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 1, 5 + 9})));
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Move, Point{5 - 3, 5 + 12})));
    // Dragging the left grip outward moves the left edge and widens.
    CK_CHECK(w->bounds().width > 20);
    CK_CHECK(w->bounds().x < 5);
}

CK_TEST(a_window_too_narrow_for_two_wide_grips_keeps_them_a_cell_each) {
    Fixture f;
    Surface s(ckv::Size{8, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 4, 5});
    w->set_active(true);
    w->set_resizable(true);
    Painter painter(s, Rect{0, 0, 8, 5});
    w->draw(painter);
    // Two two-cell grips would meet and read as one continuous edge, so a
    // narrow window keeps them one cell each.
    CK_CHECK(s.at(Point{3, 4}).grapheme() == "┘");
    CK_CHECK(s.at(Point{2, 4}).grapheme() != "─");
}

CK_TEST(a_resize_grip_wears_the_control_colour_not_the_frames) {
    // A grip is something to take hold of, like the close and zoom
    // controls, and the convention colours all three alike. Drawn as frame,
    // the one part of the border you can grab looks like the parts you
    // cannot.
    Fixture f;
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 5});
    w->set_active(true);
    w->set_resizable(true);
    Painter painter(s, Rect{0, 0, 20, 5});
    w->draw(painter);

    const ckv::Style control = f.theme.resolve(f.roles.window_control);
    const ckv::Style frame = f.theme.resolve(f.roles.window_frame_active);
    CK_CHECK(control.fg != frame.fg);  // the test would prove nothing otherwise
    // The marked corner and the widened cell beside it.
    for (const int x : {18, 19}) CK_CHECK(s.at(Point{x, 4}).style().fg == control.fg);
    // The close glyph already wore it; the brackets around it stay frame.
    CK_CHECK(s.at(Point{3, 0}).style().fg == control.fg);
    CK_CHECK(s.at(Point{2, 0}).style().fg == frame.fg);
    // Plain border between the grips is still border.
    CK_CHECK(s.at(Point{9, 4}).style().fg == frame.fg);
}

CK_TEST(an_inactive_windows_controls_fall_back_to_its_frame) {
    // The colour is what marks the window you are working in; a desktop of
    // windows each showing live controls has nothing to point at.
    Fixture f;
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 5});
    w->set_active(false);
    w->set_resizable(true);
    Painter painter(s, Rect{0, 0, 20, 5});
    w->draw(painter);

    const ckv::Style inactive = f.theme.resolve(f.roles.window_frame_inactive);
    CK_CHECK(s.at(Point{3, 0}).style().fg == inactive.fg);  // close glyph
    CK_CHECK(s.at(Point{2, 0}).style().fg == inactive.fg);  // its bracket
}

CK_TEST(a_window_gives_up_its_bottom_margin_to_a_row_of_buttons) {
    // The margin works itself out rather than each caller remembering which
    // of its dialogs happens to end in buttons. A button's cast shadow is
    // already the gap; a margin under it is a second one.
    Fixture f;
    auto w = make_window(f);
    w->set_content_margin(1, 1);

    auto column = std::make_unique<ckv::ui::Column>();
    column->add_item(std::make_unique<ckv::widgets::StaticText>("body"), ckv::ui::LayoutSpec{ckv::ui::SizePolicy::Fixed, 1});
    auto row = std::make_unique<ckv::ui::Row>();
    row->add_item(std::make_unique<ckv::widgets::Button>("OK"), ckv::ui::LayoutSpec{ckv::ui::SizePolicy::Fixed, 1});
    column->add_item(std::move(row), ckv::ui::LayoutSpec{ckv::ui::SizePolicy::Fixed, 2});
    w->set_content(std::move(column));

    CK_CHECK(w->effective_top_margin() == 1);
    CK_CHECK(w->effective_bottom_margin() == 0);
    // Sizing must agree with painting, or the window reserves a gap it does
    // not draw and the freed row simply reappears elsewhere in the dialog --
    // which is what happened when only the paint path knew the rule.
    CK_CHECK(w->vertical_size_hint().preferred ==
             w->content()->vertical_size_hint().preferred + 2 + 1);  // frame + top margin, no bottom

    // Content that does not end in buttons keeps its margin, and is sized
    // for it.
    auto plain = std::make_unique<ckv::ui::Column>();
    plain->add_item(std::make_unique<ckv::widgets::StaticText>("body"), ckv::ui::LayoutSpec{ckv::ui::SizePolicy::Fixed, 1});
    plain->add_item(std::make_unique<ckv::widgets::StaticText>("more"), ckv::ui::LayoutSpec{ckv::ui::SizePolicy::Fixed, 1});
    w->set_content(std::move(plain));
    CK_CHECK(w->effective_bottom_margin() == 1);
    CK_CHECK(w->vertical_size_hint().preferred ==
             w->content()->vertical_size_hint().preferred + 2 + 1 + 1);
}

CK_TEST(a_row_mixing_a_button_with_other_content_keeps_the_margin) {
    // Only a row of nothing but buttons is all shadow along its bottom.
    Fixture f;
    auto w = make_window(f);
    w->set_content_margin(1, 1);
    auto row = std::make_unique<ckv::ui::Row>();
    row->add_item(std::make_unique<ckv::widgets::Button>("OK"), ckv::ui::LayoutSpec{ckv::ui::SizePolicy::Fixed, 1});
    row->add_item(std::make_unique<ckv::widgets::StaticText>("note"), ckv::ui::LayoutSpec{ckv::ui::SizePolicy::Fixed, 1});
    w->set_content(std::move(row));
    CK_CHECK(w->effective_bottom_margin() == 1);
}

// --- Four corners resize; one says so ---------------------------------------

CK_TEST(an_idle_window_marks_only_the_corner_the_convention_marks) {
    Fixture f;
    Surface s(ckv::Size{20, 6}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 6});
    w->set_active(true);
    w->set_resizable(true);
    Painter painter(s, Rect{0, 0, 20, 6});
    w->draw(painter);
    // Bottom right marked, the other three plain frame. Four marks on a
    // quiet window is three more than it needs.
    CK_CHECK(s.at(Point{19, 5}).grapheme() == "┘");
    CK_CHECK(s.at(Point{0, 5}).grapheme() == "╚");
    CK_CHECK(s.at(Point{0, 0}).grapheme() == "╔");
    CK_CHECK(s.at(Point{19, 0}).grapheme() == "╗");
}

CK_TEST(every_corner_shows_its_grip_while_a_resize_is_under_way) {
    Fixture f;
    Surface s(ckv::Size{20, 6}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 6});
    w->set_active(true);
    w->set_resizable(true);
    Painter painter(s, Rect{0, 0, 20, 6});

    // Grab the top-left corner: the other three are worth showing now, and
    // only now.
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{0, 0})));
    w->draw(painter);
    CK_CHECK(s.at(Point{0, 0}).grapheme() == "┌");
    CK_CHECK(s.at(Point{19, 0}).grapheme() == "┐");
    CK_CHECK(s.at(Point{0, 5}).grapheme() == "└");
    CK_CHECK(s.at(Point{19, 5}).grapheme() == "┘");
    // The three temporary ones change the frame's shape, not its colour --
    // three corners lighting up mid-gesture says more than it needs to.
    const ckv::Style frame = f.theme.resolve(f.roles.window_frame_active);
    const ckv::Style control = f.theme.resolve(f.roles.window_control);
    CK_CHECK(frame.fg != control.fg);  // the check would prove nothing otherwise
    for (const Point at : {Point{0, 0}, Point{19, 0}, Point{0, 5}})
        CK_CHECK(s.at(at).style().fg == frame.fg);
    CK_CHECK(s.at(Point{19, 5}).style().fg == control.fg);  // the permanent one

    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Up, Point{0, 0})));
    w->draw(painter);
    CK_CHECK(s.at(Point{0, 0}).grapheme() == "╔");  // quiet again
    CK_CHECK(s.at(Point{19, 5}).grapheme() == "┘");
}

CK_TEST(each_corner_resizes_and_leaves_the_opposite_one_where_it_was) {
    struct Case {
        Point grab;
        Point to;
        bool right_edge_moves;
        bool bottom_edge_moves;
    };
    // A window at (10,5) sized 20x8: corners (10,5) (29,5) (10,12) (29,12).
    const Case cases[] = {
        {Point{10, 5}, Point{6, 2}, false, false},    // top left
        {Point{29, 5}, Point{33, 2}, true, false},    // top right
        {Point{10, 12}, Point{6, 15}, false, true},   // bottom left
        {Point{29, 12}, Point{33, 15}, true, true},   // bottom right
    };
    for (const Case& c : cases) {
        Fixture f;
        auto w = make_window(f);
        w->set_bounds(Rect{10, 5, 20, 8});
        w->set_resizable(true);
        const Rect before = w->bounds();
        CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, c.grab)));
        CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Move, c.to)));
        CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Up, c.to)));
        const Rect after = w->bounds();
        CK_CHECK(after.width > before.width);
        CK_CHECK(after.height > before.height);
        // The corner opposite the one grabbed does not move.
        CK_CHECK((after.x + after.width != before.x + before.width) == c.right_edge_moves);
        CK_CHECK((after.y + after.height != before.y + before.height) == c.bottom_edge_moves);
    }
}

CK_TEST(a_top_corner_resizes_rather_than_moving_the_window) {
    // The top corners sit on the title row, which is also the move handle.
    // A reader who aimed at a corner meant the corner.
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{10, 5, 20, 8});
    w->set_resizable(true);
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{10, 5})));
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Move, Point{8, 3})));
    CK_CHECK(w->bounds().width != 20);  // resized, not merely relocated

    // The title bar between the corners still moves it.
    Fixture g;
    auto m = make_window(g);
    m->set_bounds(Rect{10, 5, 20, 8});
    m->set_resizable(true);
    CK_CHECK(m->on_mouse(mouse(ckv::MouseAction::Down, Point{20, 5})));
    CK_CHECK(m->on_mouse(mouse(ckv::MouseAction::Move, Point{22, 7})));
    CK_CHECK(m->bounds().width == 20);
    CK_CHECK(m->bounds().x == 12);
}

// --- A drag that never gets its release --------------------------------------

CK_TEST(pressing_again_ends_a_drag_that_never_got_its_release) {
    // Drag a corner out of the terminal and let go: the release is delivered
    // to somebody else, and this window is still holding the gesture. The
    // next press is the first news it gets, so it takes it as such.
    Fixture f;
    Surface s(ckv::Size{20, 6}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 6});
    w->set_active(true);
    w->set_resizable(true);
    Painter painter(s, Rect{0, 0, 20, 6});

    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{0, 0})));
    w->draw(painter);
    CK_CHECK(s.at(Point{19, 0}).grapheme() == "┐");  // all four showing

    // No Up ever arrives. Press the title bar to move the window instead.
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{10, 0})));
    w->draw(painter);
    CK_CHECK(s.at(Point{19, 0}).grapheme() == "╗");  // back to one mark
    CK_CHECK(s.at(Point{0, 0}).grapheme() == "╔");
}

CK_TEST(motion_with_no_button_held_ends_the_drag_instead_of_following_it) {
    // The pointer returning without a button down proves the release
    // happened out of sight. Otherwise the window keeps following a pointer
    // nobody is pressing.
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{10, 5, 20, 8});
    w->set_resizable(true);
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{29, 12})));
    const Rect held = w->bounds();

    ckv::MouseEvent drifting{ckv::MouseAction::Move, ckv::MouseButton::None, Point{40, 20},
                             std::nullopt, Modifier::None};
    CK_CHECK(!w->on_mouse(drifting));
    CK_CHECK(w->bounds() == held);  // did not follow

    // ...and it really is over: further motion changes nothing either.
    CK_CHECK(!w->on_mouse(mouse(ckv::MouseAction::Move, Point{50, 30})));
    CK_CHECK(w->bounds() == held);
}

CK_TEST(deactivating_a_window_ends_a_drag_it_still_believed_in) {
    Fixture f;
    Surface s(ckv::Size{20, 6}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 6});
    w->set_active(true);
    w->set_resizable(true);
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{0, 5})));

    w->set_active(false);
    w->set_active(true);
    Painter painter(s, Rect{0, 0, 20, 6});
    w->draw(painter);
    CK_CHECK(s.at(Point{0, 5}).grapheme() == "╚");  // not still held
}

// --- Frame controls act on release ------------------------------------------

CK_TEST(the_close_control_acts_on_release_not_on_press) {
    // A press is a question; the release is the answer. Closing on the press
    // gives the reader no chance to change their mind, and closing is not
    // something they can take back.
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 6});
    bool asked = false;
    w->close_request = [&] { asked = true; return false; };

    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{3, 0})));
    CK_CHECK(!asked);
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Up, Point{3, 0})));
    CK_CHECK(asked);
}

CK_TEST(releasing_away_from_the_close_control_takes_the_press_back) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 6});
    bool asked = false;
    w->close_request = [&] { asked = true; return false; };

    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{3, 0})));
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Move, Point{9, 3})));
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Up, Point{9, 3})));
    CK_CHECK(!asked);
}

CK_TEST(the_zoom_control_acts_on_release_and_can_be_taken_back_too) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 6});
    w->set_resizable(true);
    const Rect before = w->bounds();

    // Press, leave, release: nothing happens.
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{16, 0})));
    CK_CHECK(w->bounds() == before);
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Move, Point{9, 3})));
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Up, Point{9, 3})));
    CK_CHECK(w->bounds() == before);

    // Press and release on it: it zooms.
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{16, 0})));
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Up, Point{16, 0})));
    CK_CHECK(w->zoomed());
}

// --- A window stays reachable -------------------------------------------------

CK_TEST(a_moved_window_keeps_its_title_bar_where_the_mouse_can_reach_it) {
    // The title bar is the only way to move a window, so it is the part that
    // must stay reachable. Dragged under the menu bar or past the footer,
    // there would be nothing left to grab it by.
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{5, 5, 20, 6});
    w->set_move_bounds(Rect{0, 1, 40, 10});  // desktop content: rows 1..10

    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{15, 5})));
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Move, Point{15, -20})));
    CK_CHECK(w->bounds().y == 1);  // not one row further under the chrome
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Move, Point{15, 60})));
    CK_CHECK(w->bounds().y == 10);  // the last row of the content area

    // Sideways, enough of the title bar stays inside to aim at.
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Move, Point{-90, 5})));
    CK_CHECK(w->bounds().x + w->bounds().width >= 8);
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Move, Point{200, 5})));
    CK_CHECK(w->bounds().x <= 40 - 8);
}

CK_TEST(a_window_with_no_move_bounds_is_unconstrained) {
    // Constraining is the owner's decision; a Window on its own does not
    // invent a boundary it was never told about.
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{5, 5, 20, 6});
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{15, 5})));
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Move, Point{15, -20})));
    CK_CHECK(w->bounds().y < 0);
}

CK_TEST(a_held_frame_control_highlights_and_lets_go_when_the_pointer_leaves) {
    // The highlight is the window saying what would happen if the button
    // came up now. Leaving withdraws that promise; returning renews it.
    Fixture f;
    Surface s(ckv::Size{20, 6}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 6});
    w->set_active(true);
    Painter painter(s, Rect{0, 0, 20, 6});
    const ckv::Style pressed = f.theme.resolve(f.roles.window_control_pressed);
    const ckv::Style control = f.theme.resolve(f.roles.window_control);
    CK_CHECK(pressed.bg != control.bg);  // the check would prove nothing otherwise

    w->draw(painter);
    CK_CHECK(s.at(Point{3, 0}).style().bg != pressed.bg);  // idle

    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{3, 0})));
    w->draw(painter);
    // Brackets included: one pressed thing, not a recoloured glyph.
    for (const int x : {2, 3, 4}) CK_CHECK(s.at(Point{x, 0}).style().bg == pressed.bg);

    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Move, Point{10, 3})));
    w->draw(painter);
    for (const int x : {2, 3, 4}) CK_CHECK(s.at(Point{x, 0}).style().bg != pressed.bg);

    // Back on it: armed again, and it fires on release.
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Move, Point{3, 0})));
    w->draw(painter);
    CK_CHECK(s.at(Point{3, 0}).style().bg == pressed.bg);
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Up, Point{3, 0})));
    w->draw(painter);
    CK_CHECK(s.at(Point{3, 0}).style().bg != pressed.bg);  // released, highlight gone
}

CK_TEST(the_zoom_control_highlights_while_held_too) {
    Fixture f;
    Surface s(ckv::Size{20, 6}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 6});
    w->set_active(true);
    w->set_resizable(true);
    Painter painter(s, Rect{0, 0, 20, 6});
    const ckv::Style pressed = f.theme.resolve(f.roles.window_control_pressed);

    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{16, 0})));
    w->draw(painter);
    for (const int x : {15, 16, 17}) CK_CHECK(s.at(Point{x, 0}).style().bg == pressed.bg);
}

// --- A footer on the bottom border ------------------------------------

CK_TEST(a_window_footer_is_drawn_on_the_bottom_border) {
    Fixture f;
    Window window("Assistant");
    window.set_context(f.ctx());
    window.set_bounds(Rect{0, 0, 30, 6});
    window.set_footer("$0.0031 this call");
    Surface s(ckv::Size{30, 6}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 30, 6});
    window.draw(painter);
    std::string bottom;
    for (int x = 0; x < 30; ++x) bottom += s.at(ckv::Point{x, 5}).grapheme();
    CK_CHECK(bottom.find("$0.0031") != std::string::npos);
    // The top border still says what the window is.
    std::string top;
    for (int x = 0; x < 30; ++x) top += s.at(ckv::Point{x, 0}).grapheme();
    CK_CHECK(top.find("Assistant") != std::string::npos);
}

CK_TEST(a_footer_too_long_for_the_border_is_elided_rather_than_overrunning_it) {
    Fixture f;
    Window window("W");
    window.set_context(f.ctx());
    window.set_bounds(Rect{0, 0, 16, 4});
    window.set_footer("a very long running total indeed");
    Surface s(ckv::Size{16, 4}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 16, 4});
    window.draw(painter);
    // The corner is still a corner: the footer never reaches it.
    CK_CHECK(s.at(ckv::Point{15, 3}).grapheme() != "a");
    CK_CHECK(s.at(ckv::Point{0, 3}).grapheme() != "a");
}

CK_TEST(a_window_with_no_footer_draws_an_unbroken_bottom_border) {
    Fixture f;
    Window plain("W");
    plain.set_context(f.ctx());
    plain.set_bounds(Rect{0, 0, 20, 4});
    Surface s(ckv::Size{20, 4}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(s, Rect{0, 0, 20, 4});
    plain.draw(painter);
    for (int x = 1; x < 19; ++x) CK_CHECK(s.at(ckv::Point{x, 3}).grapheme() != " ");
}
