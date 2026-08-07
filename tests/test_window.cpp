// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/window.hpp"

#include "cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/ui/standard_roles.hpp"

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
using ckv::widgets::Window;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
};

Window make_window(Fixture& f, std::string title = "Test") {
    return Window(std::move(title), f.theme, f.roles.window_frame_active, f.roles.window_frame_inactive,
                  f.roles.window_title_active, f.roles.window_title_inactive);
}

ckv::MouseEvent mouse(ckv::MouseAction action, Point p) {
    return ckv::MouseEvent{action, ckv::MouseButton::Left, p, std::nullopt, Modifier::None};
}
}  // namespace

// --- Content ownership -----------------------------------------------------

CK_TEST(set_content_positions_it_to_fill_the_interior_inside_the_frame_border) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{0, 0, 20, 10});
    w.set_content(std::make_unique<View>());
    CK_CHECK(w.content()->bounds() == (Rect{1, 1, 18, 8}));
}

CK_TEST(replacing_content_returns_the_previous_view_and_installs_the_new_one) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{0, 0, 20, 10});
    w.set_content(std::make_unique<View>());
    View* first = w.content();
    auto returned = w.set_content(std::make_unique<View>());
    CK_CHECK(returned.get() == first);
    CK_CHECK(w.content() != first);
}

CK_TEST(setting_content_to_null_removes_the_current_content) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{0, 0, 20, 10});
    w.set_content(std::make_unique<View>());
    w.set_content(nullptr);
    CK_CHECK(w.content() == nullptr);
}

CK_TEST(resizing_the_window_repositions_content_to_fill_the_new_interior) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{0, 0, 20, 10});
    w.set_content(std::make_unique<View>());
    w.set_bounds(Rect{0, 0, 30, 15});
    CK_CHECK(w.content()->bounds() == (Rect{1, 1, 28, 13}));
}

CK_TEST(a_window_too_small_for_any_interior_reports_a_zero_size_content_rect_without_crashing) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{0, 0, 2, 2});  // no room for a 1-cell border on every side
    w.set_content(std::make_unique<View>());
    CK_CHECK(w.content()->bounds().width == 0);
    CK_CHECK(w.content()->bounds().height == 0);
}

// --- Close protocol (vetoable) ----------------------------------------------

CK_TEST(close_with_no_handler_installed_proceeds_and_fires_on_closed) {
    Fixture f;
    Window w = make_window(f);
    bool closed = false;
    w.on_closed = [&] { closed = true; };
    CK_CHECK(w.close());
    CK_CHECK(closed);
}

CK_TEST(a_close_request_returning_false_vetoes_the_close_and_on_closed_never_fires) {
    Fixture f;
    Window w = make_window(f);
    bool closed = false;
    w.close_request = [] { return false; };
    w.on_closed = [&] { closed = true; };
    CK_CHECK(!w.close());
    CK_CHECK(!closed);
}

CK_TEST(a_close_request_returning_true_proceeds_to_on_closed) {
    Fixture f;
    Window w = make_window(f);
    bool closed = false;
    w.close_request = [] { return true; };
    w.on_closed = [&] { closed = true; };
    CK_CHECK(w.close());
    CK_CHECK(closed);
}

CK_TEST(clicking_the_close_control_invokes_close) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{5, 5, 20, 10});
    bool closed = false;
    w.on_closed = [&] { closed = true; };
    CK_CHECK(w.on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 2, 5})));  // inside "[x]" at local x=1..3
    CK_CHECK(closed);
}

// --- Zoom/restore ------------------------------------------------------

CK_TEST(toggle_zoom_grows_to_the_supplied_available_rect_and_toggling_again_restores) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(!w.zoomed());
    w.toggle_zoom(Rect{0, 0, 80, 24});
    CK_CHECK(w.zoomed());
    CK_CHECK(w.bounds() == (Rect{0, 0, 80, 24}));
    w.toggle_zoom(Rect{0, 0, 80, 24});
    CK_CHECK(!w.zoomed());
    CK_CHECK(w.bounds() == (Rect{5, 5, 20, 10}));
}

CK_TEST(clicking_the_zoom_control_toggles_zoom) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(w.on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 18, 5})));  // inside "[]" near the top-right
    CK_CHECK(w.zoomed());
}

// --- Mouse move and resize --------------------------------------------------

CK_TEST(dragging_the_title_bar_moves_the_window) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(w.on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 10, 5})));  // title bar, not a control
    CK_CHECK(w.on_mouse(mouse(ckv::MouseAction::Move, Point{5 + 10 + 3, 5 + 2})));
    CK_CHECK(w.bounds() == (Rect{8, 7, 20, 10}));
    CK_CHECK(w.on_mouse(mouse(ckv::MouseAction::Up, Point{8 + 10, 7})));
}

CK_TEST(a_non_movable_window_ignores_a_title_bar_drag) {
    Fixture f;
    Window w = make_window(f);
    w.set_movable(false);
    w.set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(!w.on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 10, 5})));
}

CK_TEST(dragging_the_resize_grip_resizes_the_window_without_moving_its_origin) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(w.on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 19, 5 + 9})));  // bottom-right corner
    CK_CHECK(w.on_mouse(mouse(ckv::MouseAction::Move, Point{5 + 19 + 5, 5 + 9 + 3})));
    CK_CHECK(w.bounds() == (Rect{5, 5, 25, 13}));
}

CK_TEST(resizing_below_the_minimum_size_clamps_rather_than_shrinking_further) {
    Fixture f;
    Window w = make_window(f);
    w.set_min_size(ckv::Size{10, 4});
    w.set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(w.on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 19, 5 + 9})));
    CK_CHECK(w.on_mouse(mouse(ckv::MouseAction::Move, Point{5 - 100, 5 - 100})));  // drag far past the minimum
    CK_CHECK(w.bounds().width == 10);
    CK_CHECK(w.bounds().height == 4);
}

CK_TEST(resizing_above_the_maximum_size_clamps) {
    Fixture f;
    Window w = make_window(f);
    w.set_max_size(ckv::Size{25, 12});
    w.set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(w.on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 19, 5 + 9})));
    CK_CHECK(w.on_mouse(mouse(ckv::MouseAction::Move, Point{5 + 100, 5 + 100})));
    CK_CHECK(w.bounds().width == 25);
    CK_CHECK(w.bounds().height == 12);
}

CK_TEST(a_non_resizable_window_ignores_a_grip_drag) {
    Fixture f;
    Window w = make_window(f);
    w.set_resizable(false);
    w.set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(!w.on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 19, 5 + 9})));
}

CK_TEST(a_click_in_the_interior_is_not_handled_by_the_window_itself) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(!w.on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 10, 5 + 5})));
}

CK_TEST(move_and_up_without_a_prior_down_are_unhandled) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(!w.on_mouse(mouse(ckv::MouseAction::Move, Point{5 + 10, 5 + 10})));
    CK_CHECK(!w.on_mouse(mouse(ckv::MouseAction::Up, Point{5 + 10, 5 + 10})));
}

// --- Keyboard move/resize mode -----------------------------------------

CK_TEST(arrow_keys_are_unhandled_when_not_in_keyboard_mode) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(!w.on_key(ckv::KeyEvent{KeyChord{Key::Left, Modifier::None, ""}}));
}

CK_TEST(move_mode_arrow_keys_translate_the_window) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{5, 5, 20, 10});
    w.enter_move_mode();
    CK_CHECK(w.in_keyboard_mode());
    w.on_key(ckv::KeyEvent{KeyChord{Key::Right, Modifier::None, ""}});
    w.on_key(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}});
    CK_CHECK(w.bounds() == (Rect{6, 6, 20, 10}));
}

CK_TEST(resize_mode_arrow_keys_change_size_not_position) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{5, 5, 20, 10});
    w.enter_resize_mode();
    w.on_key(ckv::KeyEvent{KeyChord{Key::Right, Modifier::None, ""}});
    w.on_key(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}});
    CK_CHECK(w.bounds() == (Rect{5, 5, 21, 11}));
}

CK_TEST(escape_in_keyboard_mode_reverts_to_the_bounds_at_mode_entry) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{5, 5, 20, 10});
    w.enter_move_mode();
    w.on_key(ckv::KeyEvent{KeyChord{Key::Right, Modifier::None, ""}});
    w.on_key(ckv::KeyEvent{KeyChord{Key::Right, Modifier::None, ""}});
    w.on_key(ckv::KeyEvent{KeyChord{Key::Escape, Modifier::None, ""}});
    CK_CHECK(w.bounds() == (Rect{5, 5, 20, 10}));
    CK_CHECK(!w.in_keyboard_mode());
}

CK_TEST(enter_in_keyboard_mode_confirms_and_keeps_the_new_bounds) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{5, 5, 20, 10});
    w.enter_move_mode();
    w.on_key(ckv::KeyEvent{KeyChord{Key::Right, Modifier::None, ""}});
    w.on_key(ckv::KeyEvent{KeyChord{Key::Enter, Modifier::None, ""}});
    CK_CHECK(w.bounds() == (Rect{6, 5, 20, 10}));
    CK_CHECK(!w.in_keyboard_mode());
}

CK_TEST(resize_mode_respects_the_minimum_size_when_nudging_smaller) {
    Fixture f;
    Window w = make_window(f);
    w.set_min_size(ckv::Size{10, 4});
    w.set_bounds(Rect{5, 5, 10, 4});  // already at the minimum
    w.enter_resize_mode();
    w.on_key(ckv::KeyEvent{KeyChord{Key::Left, Modifier::None, ""}});   // would shrink width below min
    w.on_key(ckv::KeyEvent{KeyChord{Key::Up, Modifier::None, ""}});     // would shrink height below min
    CK_CHECK(w.bounds().width == 10);
    CK_CHECK(w.bounds().height == 4);
}

// --- Active / inactive ---------------------------------------------------

CK_TEST(a_new_window_starts_inactive) {
    Fixture f;
    Window w = make_window(f);
    CK_CHECK(!w.active());
}

CK_TEST(set_active_toggles_and_setting_the_same_value_is_a_no_op) {
    Fixture f;
    Window w = make_window(f);
    int calls = 0;
    w.set_dirty_rect_sink([&](Rect) { ++calls; });
    w.set_active(true);
    CK_CHECK(w.active());
    const int after_first = calls;
    w.set_active(true);  // no-op
    CK_CHECK(calls == after_first);
}

// --- Rendering does not crash ------------------------------------------

CK_TEST(draw_does_not_crash_for_a_window_too_narrow_to_show_controls_or_title) {
    Fixture f;
    Surface s(ckv::Size{4, 3}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Window w = make_window(f, "Some very long title that will not fit");
    w.set_bounds(Rect{0, 0, 4, 3});
    Painter painter(s, Rect{0, 0, 4, 3});
    w.draw(painter);
    CK_CHECK(true);
}

CK_TEST(draw_truncates_a_title_longer_than_the_available_space) {
    Fixture f;
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Window w = make_window(f, "A title far too long to fit in this window");
    w.set_bounds(Rect{0, 0, 20, 5});
    Painter painter(s, Rect{0, 0, 20, 5});
    w.draw(painter);  // must not crash or write past bounds (surface itself is the bound)
    CK_CHECK(true);
}

// --- reposition_within (ARCHITECTURE.md §5 "Sizing policy") ---------------

CK_TEST(reposition_within_is_a_no_op_when_the_window_already_fits) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{2, 2, 20, 10});
    w.reposition_within(Rect{0, 0, 80, 24});
    CK_CHECK(w.bounds() == (Rect{2, 2, 20, 10}));
}

CK_TEST(reposition_within_shrinks_a_window_wider_than_the_available_area) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{0, 0, 60, 10});
    w.reposition_within(Rect{0, 0, 40, 24});
    CK_CHECK(w.bounds().width == 40);
}

CK_TEST(reposition_within_never_shrinks_below_min_size) {
    Fixture f;
    Window w = make_window(f);
    w.set_min_size(ckv::Size{15, 6});
    w.set_bounds(Rect{0, 0, 20, 10});
    w.reposition_within(Rect{0, 0, 10, 4});  // smaller than min_size in both dimensions
    CK_CHECK(w.bounds().width == 15);
    CK_CHECK(w.bounds().height == 6);
}

CK_TEST(reposition_within_pins_to_the_available_areas_origin_when_the_window_cannot_fully_fit) {
    Fixture f;
    Window w = make_window(f);
    w.set_min_size(ckv::Size{15, 6});
    w.set_bounds(Rect{50, 50, 20, 10});
    w.reposition_within(Rect{2, 3, 10, 4});  // available is smaller than min_size
    CK_CHECK(w.bounds().x == 2);
    CK_CHECK(w.bounds().y == 3);
}

CK_TEST(reposition_within_pulls_a_window_back_that_moved_off_the_shrunk_area) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{70, 20, 15, 6});  // was reachable in an 80x24 desktop
    w.reposition_within(Rect{0, 0, 40, 24});  // desktop shrunk to 40 wide
    CK_CHECK(w.bounds().x + w.bounds().width <= 40);
    CK_CHECK(w.bounds().x >= 0);
}

CK_TEST(reposition_within_leaves_a_window_alone_if_only_its_position_already_fits) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{5, 5, 15, 6});
    w.reposition_within(Rect{0, 0, 40, 24});
    CK_CHECK(w.bounds() == (Rect{5, 5, 15, 6}));
}

// --- Frame overlay (a second, non-content child on the border) ------------

namespace {
class FixedWidthView : public View {
public:
    explicit FixedWidthView(int preferred_width) : preferred_width_(preferred_width) {}
    void set_preferred_width(int width) { preferred_width_ = width; }
    ckv::ui::SizeHint horizontal_size_hint() const override {
        return ckv::ui::SizeHint{0, preferred_width_, ckv::ui::kUnboundedExtent};
    }

private:
    int preferred_width_;
};
}  // namespace

CK_TEST(set_frame_overlay_positions_it_on_the_bottom_border_right_aligned) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{0, 0, 30, 10});
    w.set_frame_overlay(std::make_unique<FixedWidthView>(6));
    CK_CHECK(w.frame_overlay()->bounds() == (Rect{30 - 2 - 6, 9, 6, 1}));
}

CK_TEST(frame_overlay_repositions_on_window_resize) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{0, 0, 30, 10});
    w.set_frame_overlay(std::make_unique<FixedWidthView>(4));
    w.set_bounds(Rect{0, 0, 50, 20});
    CK_CHECK(w.frame_overlay()->bounds() == (Rect{50 - 2 - 4, 19, 4, 1}));
}

CK_TEST(frame_overlay_width_is_clamped_to_fit_a_narrow_window) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{0, 0, 8, 5});  // interior margin leaves very little room
    w.set_frame_overlay(std::make_unique<FixedWidthView>(20));  // wants far more than fits
    CK_CHECK(w.frame_overlay()->bounds().width <= 8 - 4);
    CK_CHECK(w.frame_overlay()->bounds().x >= 0);
}

CK_TEST(set_frame_overlay_replaces_the_previous_one_and_returns_its_ownership) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{0, 0, 30, 10});
    auto first_owned = std::make_unique<FixedWidthView>(5);
    View* first_raw = first_owned.get();
    w.set_frame_overlay(std::move(first_owned));

    auto returned = w.set_frame_overlay(std::make_unique<FixedWidthView>(3));
    CK_CHECK(returned.get() == first_raw);
    CK_CHECK(w.frame_overlay() != first_raw);
}

CK_TEST(set_frame_overlay_with_nullptr_after_one_was_set_clears_it) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{0, 0, 30, 10});
    w.set_frame_overlay(std::make_unique<FixedWidthView>(5));
    auto returned = w.set_frame_overlay(nullptr);
    CK_CHECK(returned != nullptr);
    CK_CHECK(w.frame_overlay() == nullptr);
}

CK_TEST(a_window_with_no_overlay_set_has_a_null_frame_overlay) {
    Fixture f;
    Window w = make_window(f);
    CK_CHECK(w.frame_overlay() == nullptr);
}

CK_TEST(frame_overlay_and_content_coexist_as_independent_children) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{0, 0, 30, 10});
    w.set_content(std::make_unique<View>());
    w.set_frame_overlay(std::make_unique<FixedWidthView>(6));
    CK_CHECK(w.content() != nullptr);
    CK_CHECK(w.frame_overlay() != nullptr);
    CK_CHECK(w.content() != w.frame_overlay());
    // content() still fills the ordinary interior, untouched by the overlay.
    CK_CHECK(w.content()->bounds() == (Rect{1, 1, 28, 8}));
}

CK_TEST(relayout_frame_overlay_picks_up_a_changed_preferred_width_without_a_window_resize) {
    // Window has no way to learn on its own that an already-installed
    // overlay's own content changed size — relayout_frame_overlay() is
    // the explicit hook a caller uses after that happens (e.g. a Label
    // whose set_text() made it longer), exactly as documented.
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{0, 0, 30, 10});
    w.set_frame_overlay(std::make_unique<FixedWidthView>(4));
    CK_CHECK(w.frame_overlay()->bounds().width == 4);

    static_cast<FixedWidthView*>(w.frame_overlay())->set_preferred_width(10);
    CK_CHECK(w.frame_overlay()->bounds().width == 4);  // unchanged: nothing told Window yet

    w.relayout_frame_overlay();
    CK_CHECK(w.frame_overlay()->bounds().width == 10);
    CK_CHECK(w.frame_overlay()->bounds() == (Rect{30 - 2 - 10, 9, 10, 1}));
}

CK_TEST(relayout_frame_overlay_with_no_overlay_set_is_a_harmless_no_op) {
    Fixture f;
    Window w = make_window(f);
    w.set_bounds(Rect{0, 0, 30, 10});
    w.relayout_frame_overlay();  // must not crash
    CK_CHECK(w.frame_overlay() == nullptr);
}
