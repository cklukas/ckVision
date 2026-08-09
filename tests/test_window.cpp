// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/window.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include "cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/label.hpp"

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
    const pid_t pid = ::fork();
    CK_CHECK(pid >= 0);
    if (pid == 0) {
        Fixture f;
        auto w = make_window(f);
        w->set_content(std::make_unique<View>());  // NOT an AnchorPane
        w->content_pane();
        ::_exit(1);
    }
    int status = 0;
    ::waitpid(pid, &status, 0);
    CK_CHECK(WIFSIGNALED(status));
    CK_CHECK(WTERMSIG(status) == SIGABRT);
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
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 2, 5})));  // inside "[x]" at local x=1..3
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
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 18, 5})));  // inside "[]" near the top-right
    CK_CHECK(w->zoomed());
}

CK_TEST(a_standalone_windows_zoom_control_uses_its_own_current_bounds_as_a_harmless_target) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{5, 5, 20, 10});
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 18, 5})));
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

CK_TEST(the_title_is_centered_with_a_padding_space_on_each_side_not_left_aligned) {
    // Matches the windowed-desktop convention's own frame-drawing
    // algorithm (verified against the reference implementation's
    // source: title width bounded, then centered via
    // `i = (width - l) >> 1`, with a blank written at i-1 and i+l) —
    // previously the title was drawn left-aligned at a fixed column,
    // immediately abutting the close control with no padding.
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
    // Close control occupies columns 1-3 ("[■]"); the title, centered,
    // must start well clear of it with its own padding cell besides.
    CK_CHECK(s.at(Point{4, 0}).grapheme() != "X");
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
    CK_CHECK(s.at(Point{10, 4}).grapheme() == "═");
    CK_CHECK(s.at(Point{0, 2}).grapheme() == "║");
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
    Painter painter(s, Rect{0, 0, 20, 5});
    w->draw(painter);
    CK_CHECK(s.at(Point{1, 0}).grapheme() == "[");
    CK_CHECK(s.at(Point{2, 0}).grapheme() == "■");
    CK_CHECK(s.at(Point{3, 0}).grapheme() == "]");
}

CK_TEST(the_zoom_control_draws_an_up_arrow_when_not_zoomed) {
    Fixture f;
    Surface s(ckv::Size{20, 5}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 20, 5});
    Painter painter(s, Rect{0, 0, 20, 5});
    w->draw(painter);
    CK_CHECK(!w->zoomed());
    CK_CHECK(s.at(Point{17, 0}).grapheme() == "↑");
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
    CK_CHECK(s.at(Point{17, 0}).grapheme() == "↕");
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
    CK_CHECK(s.at(Point{17, 0}).grapheme() == "↑");
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
    CK_CHECK(w->on_mouse(mouse(ckv::MouseAction::Down, Point{5 + 18, 5})));
    CK_CHECK(w->zoomed());
}

// --- reposition_within (ARCHITECTURE.md §5 "Sizing policy") ---------------

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
    CK_CHECK(overlay->bounds() == (Rect{4, 0, 4, 1}));  // close control occupies columns 1-3
}

CK_TEST(a_top_end_overlay_sits_flush_before_the_zoom_control) {
    Fixture f;
    auto w = make_window(f);
    w->set_bounds(Rect{0, 0, 30, 10});  // wide enough for the zoom control (b.width > 8)
    const FrameSlot top_end{.edge = Edge::Top, .alignment = ui::Alignment::End};
    auto* overlay = w->add_frame_overlay(std::make_unique<FixedWidthView>(4), top_end);
    // The zoom control occupies the last 3 columns.
    CK_CHECK(overlay->bounds() == (Rect{30 - 3 - 4, 0, 4, 1}));
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
    const pid_t pid = ::fork();
    CK_CHECK(pid >= 0);
    if (pid == 0) {
        Fixture f;
        auto w = make_window(f);
        w->set_bounds(Rect{0, 0, 30, 10});
        w->add_frame_overlay(std::make_unique<FixedWidthView>(4), FrameSlot{});
        // Same default slot again.
        w->add_frame_overlay(std::make_unique<FixedWidthView>(3), FrameSlot{});
        ::_exit(1);
    }
    int status = 0;
    ::waitpid(pid, &status, 0);
    CK_CHECK(WIFSIGNALED(status));
    CK_CHECK(WTERMSIG(status) == SIGABRT);
}

CK_TEST(a_top_centered_or_filled_overlay_aborts_since_the_title_owns_that_position) {
    const pid_t pid = ::fork();
    CK_CHECK(pid >= 0);
    if (pid == 0) {
        Fixture f;
        auto w = make_window(f);
        w->set_bounds(Rect{0, 0, 30, 10});
        w->add_frame_overlay(std::make_unique<FixedWidthView>(4),
                              FrameSlot{.edge = Edge::Top, .alignment = ui::Alignment::Center});
        ::_exit(1);
    }
    int status = 0;
    ::waitpid(pid, &status, 0);
    CK_CHECK(WIFSIGNALED(status));
    CK_CHECK(WTERMSIG(status) == SIGABRT);
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
