// Copyright (c) 2026 C. Klukas. All rights reserved.
#include "cvision/widgets/scroll_viewport.hpp"

#include "cktest.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;
using ckv::Rect;
using ckv::Size;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::ui::View;
using ckv::widgets::ScrollViewport;

namespace {
struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
};

std::unique_ptr<View> make_content(int w, int h) {
    auto v = std::make_unique<View>();
    v->set_preferred_size(Size{w, h});
    return v;
}
}  // namespace

CK_TEST(content_smaller_than_the_viewport_is_not_scrollable) {
    Fixture f;
    ScrollViewport viewport(f.theme, f.roles.dialog_frame, f.roles.dialog_background);
    viewport.set_bounds(Rect{0, 0, 40, 20});
    viewport.set_content(make_content(10, 5));
    viewport.set_scroll(100, 100);  // attempt to scroll far past the (empty) scrollable range
    CK_CHECK(viewport.scroll_x() == 0);
    CK_CHECK(viewport.scroll_y() == 0);
}

CK_TEST(content_larger_than_the_viewport_positions_at_a_negative_offset_when_scrolled) {
    Fixture f;
    ScrollViewport viewport(f.theme, f.roles.dialog_frame, f.roles.dialog_background);
    viewport.set_bounds(Rect{0, 0, 20, 10});
    viewport.set_content(make_content(100, 50));
    viewport.set_scroll(10, 5);
    CK_CHECK(viewport.scroll_x() == 10);
    CK_CHECK(viewport.scroll_y() == 5);
    CK_CHECK(viewport.content()->bounds().x == -10);
    CK_CHECK(viewport.content()->bounds().y == -5);
}

CK_TEST(set_scroll_clamps_to_the_maximum_reachable_offset) {
    Fixture f;
    ScrollViewport viewport(f.theme, f.roles.dialog_frame, f.roles.dialog_background);
    viewport.set_bounds(Rect{0, 0, 20, 10});  // content area: 19x9
    viewport.set_content(make_content(100, 50));  // max scroll: (100-19, 50-9) = (81, 41)
    viewport.set_scroll(1000, 1000);
    CK_CHECK(viewport.scroll_x() == 81);
    CK_CHECK(viewport.scroll_y() == 41);
}

CK_TEST(replacing_content_resets_scroll_to_the_origin) {
    Fixture f;
    ScrollViewport viewport(f.theme, f.roles.dialog_frame, f.roles.dialog_background);
    viewport.set_bounds(Rect{0, 0, 20, 10});
    viewport.set_content(make_content(100, 50));
    viewport.set_scroll(30, 20);
    CK_CHECK(viewport.scroll_y() == 20);
    viewport.set_content(make_content(100, 50));
    CK_CHECK(viewport.scroll_x() == 0);
    CK_CHECK(viewport.scroll_y() == 0);
}

CK_TEST(replacing_content_returns_ownership_of_the_previous_content) {
    Fixture f;
    ScrollViewport viewport(f.theme, f.roles.dialog_frame, f.roles.dialog_background);
    viewport.set_bounds(Rect{0, 0, 20, 10});
    viewport.set_content(make_content(100, 50));
    View* first = viewport.content();
    auto returned = viewport.set_content(make_content(30, 30));
    CK_CHECK(returned.get() == first);
    CK_CHECK(viewport.content() != first);
}

CK_TEST(resizing_the_viewport_shrinks_the_reachable_scroll_range) {
    Fixture f;
    ScrollViewport viewport(f.theme, f.roles.dialog_frame, f.roles.dialog_background);
    viewport.set_bounds(Rect{0, 0, 20, 10});
    viewport.set_content(make_content(100, 50));
    viewport.set_scroll(81, 41);  // at the maximum for the 20x10 viewport
    viewport.set_bounds(Rect{0, 0, 40, 20});  // now much larger — max scroll shrinks accordingly
    CK_CHECK(viewport.scroll_x() < 81);
    CK_CHECK(viewport.scroll_y() < 41);
}

CK_TEST(down_arrow_scrolls_vertically_by_one) {
    Fixture f;
    ScrollViewport viewport(f.theme, f.roles.dialog_frame, f.roles.dialog_background);
    viewport.set_bounds(Rect{0, 0, 20, 10});
    viewport.set_content(make_content(100, 50));
    CK_CHECK(viewport.on_key(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}}));
    CK_CHECK(viewport.scroll_y() == 1);
}

CK_TEST(right_arrow_scrolls_horizontally_by_one) {
    Fixture f;
    ScrollViewport viewport(f.theme, f.roles.dialog_frame, f.roles.dialog_background);
    viewport.set_bounds(Rect{0, 0, 20, 10});
    viewport.set_content(make_content(100, 50));
    CK_CHECK(viewport.on_key(ckv::KeyEvent{KeyChord{Key::Right, Modifier::None, ""}}));
    CK_CHECK(viewport.scroll_x() == 1);
}

CK_TEST(page_down_scrolls_by_the_viewports_visible_height) {
    Fixture f;
    ScrollViewport viewport(f.theme, f.roles.dialog_frame, f.roles.dialog_background);
    viewport.set_bounds(Rect{0, 0, 20, 10});  // content area height: 9
    viewport.set_content(make_content(100, 50));
    viewport.on_key(ckv::KeyEvent{KeyChord{Key::PageDown, Modifier::None, ""}});
    CK_CHECK(viewport.scroll_y() == 9);
}

CK_TEST(mouse_wheel_scrolls_vertically) {
    Fixture f;
    ScrollViewport viewport(f.theme, f.roles.dialog_frame, f.roles.dialog_background);
    viewport.set_bounds(Rect{0, 0, 20, 10});
    viewport.set_content(make_content(100, 50));
    CK_CHECK(viewport.on_mouse(ckv::MouseEvent{ckv::MouseAction::Wheel, ckv::MouseButton::WheelDown,
                                                ckv::Point{5, 5}, std::nullopt, Modifier::None}));
    CK_CHECK(viewport.scroll_y() == 1);
    CK_CHECK(viewport.on_mouse(ckv::MouseEvent{ckv::MouseAction::Wheel, ckv::MouseButton::WheelUp,
                                                ckv::Point{5, 5}, std::nullopt, Modifier::None}));
    CK_CHECK(viewport.scroll_y() == 0);
}

CK_TEST(a_non_wheel_mouse_event_is_unhandled_by_the_viewport_itself) {
    Fixture f;
    ScrollViewport viewport(f.theme, f.roles.dialog_frame, f.roles.dialog_background);
    viewport.set_bounds(Rect{0, 0, 20, 10});
    viewport.set_content(make_content(100, 50));
    CK_CHECK(!viewport.on_mouse(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{5, 5},
                                                 std::nullopt, Modifier::None}));
}

CK_TEST(no_content_installed_does_not_crash_on_key_mouse_or_resize) {
    // The scrollbars always exist (created in the constructor) and
    // consume their recognized keys/wheel regardless of whether
    // content is installed — with nothing to scroll, position simply
    // stays clamped at 0. The assertion here is "does not crash";
    // scroll position staying at 0 is checked explicitly.
    Fixture f;
    ScrollViewport viewport(f.theme, f.roles.dialog_frame, f.roles.dialog_background);
    viewport.set_bounds(Rect{0, 0, 20, 10});
    viewport.on_key(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}});
    CK_CHECK(viewport.scroll_y() == 0);
    viewport.on_mouse(ckv::MouseEvent{ckv::MouseAction::Wheel, ckv::MouseButton::WheelDown, ckv::Point{5, 5},
                                       std::nullopt, Modifier::None});
    CK_CHECK(viewport.scroll_y() == 0);
    viewport.set_bounds(Rect{0, 0, 30, 15});
    CK_CHECK(true);
}

CK_TEST(a_degenerate_one_cell_viewport_does_not_crash) {
    Fixture f;
    ScrollViewport viewport(f.theme, f.roles.dialog_frame, f.roles.dialog_background);
    viewport.set_bounds(Rect{0, 0, 1, 1});
    viewport.set_content(make_content(100, 50));
    CK_CHECK(true);
}
