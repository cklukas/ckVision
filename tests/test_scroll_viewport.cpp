// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/scroll_viewport.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::ManualClock;
using ckv::Modifier;
using ckv::Rect;
using ckv::Size;
using ckv::term::HeadlessTerminal;
using ckv::ui::Application;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::ui::View;
using ckv::widgets::ScrollbarPolicy;
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

// Content whose height depends on the width it is given — the shape of
// every wrapped text view, and the case a width-independent size hint
// cannot describe.
class WrappedContent final : public View {
public:
    explicit WrappedContent(int total_cells) : total_(total_cells) {
        set_preferred_size(ckv::Size{20, 1});
    }

    void set_total(int total_cells) {
        total_ = total_cells;
        size_hint_changed();
    }

    int height_for_width(int width) const override {
        return width <= 0 ? 0 : (total_ + width - 1) / width;
    }

private:
    int total_;
};

class WheelIgnoringContent final : public View {
public:
    int wheel_events = 0;

    WheelIgnoringContent(int w, int h) { set_preferred_size(Size{w, h}); }

    bool on_mouse(const ckv::MouseEvent& event) override {
        if (event.action == ckv::MouseAction::Wheel) ++wheel_events;
        return false;
    }
};
}  // namespace

CK_TEST(content_smaller_than_the_viewport_is_not_scrollable) {
    Fixture f;
    ScrollViewport viewport;
    viewport.set_bounds(Rect{0, 0, 40, 20});
    viewport.set_content(make_content(10, 5));
    viewport.set_scroll(100, 100);  // attempt to scroll far past the (empty) scrollable range
    CK_CHECK(viewport.scroll_x() == 0);
    CK_CHECK(viewport.scroll_y() == 0);
    CK_CHECK(viewport.content()->bounds().width == 40);
    CK_CHECK(viewport.content()->bounds().height == 20);
}

CK_TEST(always_visible_scrollbars_reserve_their_tracks_for_fitting_content) {
    Fixture f;
    ScrollViewport viewport;
    viewport.set_bounds(Rect{0, 0, 40, 20});
    viewport.set_scrollbars_always_visible(true);
    viewport.set_content(make_content(10, 5));
    CK_CHECK(viewport.scrollbars_always_visible());
    CK_CHECK(viewport.content()->bounds().width == 39);
    CK_CHECK(viewport.content()->bounds().height == 19);
}

CK_TEST(vertical_auto_hide_reclaims_horizontal_content_space_when_only_horizontal_scroll_is_unneeded) {
    Fixture f;
    ScrollViewport viewport;
    viewport.set_bounds(Rect{0, 0, 20, 10});
    viewport.set_content(make_content(10, 50));
    CK_CHECK(viewport.content()->bounds().width == 19);
    CK_CHECK(viewport.content()->bounds().height == 50);
}

CK_TEST(horizontal_auto_hide_reclaims_vertical_content_space_when_only_vertical_scroll_is_unneeded) {
    Fixture f;
    ScrollViewport viewport;
    viewport.set_bounds(Rect{0, 0, 20, 10});
    viewport.set_content(make_content(100, 5));
    CK_CHECK(viewport.content()->bounds().width == 100);
    CK_CHECK(viewport.content()->bounds().height == 9);
}

CK_TEST(content_larger_than_the_viewport_positions_at_a_negative_offset_when_scrolled) {
    Fixture f;
    ScrollViewport viewport;
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
    ScrollViewport viewport;
    viewport.set_bounds(Rect{0, 0, 20, 10});  // content area: 19x9
    viewport.set_content(make_content(100, 50));  // max scroll: (100-19, 50-9) = (81, 41)
    viewport.set_scroll(1000, 1000);
    CK_CHECK(viewport.scroll_x() == 81);
    CK_CHECK(viewport.scroll_y() == 41);
}

CK_TEST(replacing_content_resets_scroll_to_the_origin) {
    Fixture f;
    ScrollViewport viewport;
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
    ScrollViewport viewport;
    viewport.set_bounds(Rect{0, 0, 20, 10});
    viewport.set_content(make_content(100, 50));
    View* first = viewport.content();
    auto returned = viewport.set_content(make_content(30, 30));
    CK_CHECK(returned.get() == first);
    CK_CHECK(viewport.content() != first);
}

CK_TEST(resizing_the_viewport_shrinks_the_reachable_scroll_range) {
    Fixture f;
    ScrollViewport viewport;
    viewport.set_bounds(Rect{0, 0, 20, 10});
    viewport.set_content(make_content(100, 50));
    viewport.set_scroll(81, 41);  // at the maximum for the 20x10 viewport
    viewport.set_bounds(Rect{0, 0, 40, 20});  // now much larger — max scroll shrinks accordingly
    CK_CHECK(viewport.scroll_x() < 81);
    CK_CHECK(viewport.scroll_y() < 41);
}

CK_TEST(down_arrow_scrolls_vertically_by_one) {
    Fixture f;
    ScrollViewport viewport;
    viewport.set_bounds(Rect{0, 0, 20, 10});
    viewport.set_content(make_content(100, 50));
    CK_CHECK(viewport.on_key(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}}));
    CK_CHECK(viewport.scroll_y() == 1);
}

CK_TEST(right_arrow_scrolls_horizontally_by_one) {
    Fixture f;
    ScrollViewport viewport;
    viewport.set_bounds(Rect{0, 0, 20, 10});
    viewport.set_content(make_content(100, 50));
    CK_CHECK(viewport.on_key(ckv::KeyEvent{KeyChord{Key::Right, Modifier::None, ""}}));
    CK_CHECK(viewport.scroll_x() == 1);
}

CK_TEST(page_down_scrolls_by_the_viewports_visible_height) {
    Fixture f;
    ScrollViewport viewport;
    viewport.set_bounds(Rect{0, 0, 20, 10});  // content area height: 9
    viewport.set_content(make_content(100, 50));
    viewport.on_key(ckv::KeyEvent{KeyChord{Key::PageDown, Modifier::None, ""}});
    CK_CHECK(viewport.scroll_y() == 9);
}

CK_TEST(mouse_wheel_scrolls_vertically) {
    Fixture f;
    ScrollViewport viewport;
    viewport.set_bounds(Rect{0, 0, 20, 10});
    viewport.set_content(make_content(100, 50));
    CK_CHECK(viewport.on_mouse(ckv::MouseEvent{ckv::MouseAction::Wheel, ckv::MouseButton::WheelDown,
                                                ckv::Point{5, 5}, std::nullopt, Modifier::None}));
    CK_CHECK(viewport.scroll_y() == 1);
    CK_CHECK(viewport.on_mouse(ckv::MouseEvent{ckv::MouseAction::Wheel, ckv::MouseButton::WheelUp,
                                                ckv::Point{5, 5}, std::nullopt, Modifier::None}));
    CK_CHECK(viewport.scroll_y() == 0);
}

CK_TEST(application_routes_wheel_events_over_descendant_content_to_the_scroll_viewport) {
    HeadlessTerminal term(Size{80, 24});
    ManualClock clock;
    Application app(term, clock);
    auto viewport = std::make_unique<ScrollViewport>();
    viewport->set_fills_root(false);
    auto* viewport_ptr = app.root().add(std::move(viewport));
    viewport_ptr->set_bounds(Rect{0, 0, 20, 10});
    auto content = std::make_unique<WheelIgnoringContent>(100, 50);
    auto* content_ptr = content.get();
    viewport_ptr->set_content(std::move(content));

    CK_CHECK(app.dispatch(ckv::MouseEvent{ckv::MouseAction::Wheel, ckv::MouseButton::WheelDown,
                                           ckv::Point{5, 5}, std::nullopt, Modifier::None}));
    CK_CHECK(content_ptr->wheel_events == 1);
    CK_CHECK(viewport_ptr->scroll_y() == 1);
}

CK_TEST(a_non_wheel_mouse_event_is_unhandled_by_the_viewport_itself) {
    Fixture f;
    ScrollViewport viewport;
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
    ScrollViewport viewport;
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
    ScrollViewport viewport;
    viewport.set_bounds(Rect{0, 0, 1, 1});
    viewport.set_content(make_content(100, 50));
    CK_CHECK(true);
}

// --- Per-axis policy, and revealing a descendant (U4-g) ------------------

CK_TEST(scrollbar_policies_are_per_axis_and_default_to_auto) {
    Fixture f;
    ScrollViewport viewport;
    CK_CHECK(viewport.vertical_scrollbar_policy() == ScrollbarPolicy::Auto);
    CK_CHECK(viewport.horizontal_scrollbar_policy() == ScrollbarPolicy::Auto);
    viewport.set_horizontal_scrollbar_policy(ScrollbarPolicy::Hidden);
    CK_CHECK(viewport.horizontal_scrollbar_policy() == ScrollbarPolicy::Hidden);
    CK_CHECK(viewport.vertical_scrollbar_policy() == ScrollbarPolicy::Auto);  // unchanged
}

CK_TEST(a_hidden_bar_holds_its_axis_to_the_visible_extent_so_nothing_can_be_scrolled_out_of_reach) {
    Fixture f;
    ScrollViewport viewport;
    viewport.set_bounds(Rect{0, 0, 20, 10});
    viewport.set_horizontal_scrollbar_policy(ScrollbarPolicy::Hidden);
    viewport.set_content(make_content(100, 50));
    // The vertical bar still takes its column, and the content is given
    // exactly the 19 that leaves — not the 100 it asked for.
    CK_CHECK(viewport.content()->bounds() == (Rect{0, 0, 19, 50}));
    CK_CHECK(!viewport.can_scroll_horizontally());
    CK_CHECK(viewport.can_scroll_vertically());
    CK_CHECK(!viewport.on_key(ckv::KeyEvent{KeyChord{Key::Right, Modifier::None, ""}}));
    CK_CHECK(viewport.scroll_x() == 0);
}

CK_TEST(a_viewport_with_nothing_to_scroll_consumes_neither_keys_nor_wheel) {
    Fixture f;
    ScrollViewport viewport;
    viewport.set_bounds(Rect{0, 0, 40, 20});
    viewport.set_content(make_content(10, 5));
    CK_CHECK(!viewport.can_scroll_vertically());
    CK_CHECK(!viewport.on_key(ckv::KeyEvent{KeyChord{Key::Down, Modifier::None, ""}}));
    CK_CHECK(!viewport.on_key(ckv::KeyEvent{KeyChord{Key::PageDown, Modifier::None, ""}}));
    CK_CHECK(!viewport.on_mouse(ckv::MouseEvent{ckv::MouseAction::Wheel, ckv::MouseButton::WheelDown,
                                                 ckv::Point{5, 5}, std::nullopt, Modifier::None}));
}

CK_TEST(ensure_visible_scrolls_the_least_it_can_and_only_when_it_must) {
    Fixture f;
    ScrollViewport viewport;
    viewport.set_bounds(Rect{0, 0, 20, 10});
    auto content = make_content(10, 50);
    View* const content_view = content.get();
    viewport.set_content(std::move(content));
    View* const child = content_view->add_child(std::make_unique<View>());
    child->set_bounds(Rect{0, 30, 10, 2});

    // Ten visible rows; the child's last row becomes the band's last row.
    CK_CHECK(viewport.ensure_visible(*child));
    CK_CHECK(viewport.scroll_y() == 22);
    CK_CHECK(!viewport.ensure_visible(*child));  // already in view: nothing moves
    CK_CHECK(viewport.scroll_y() == 22);

    // Coming back from below, the near edge wins: the child's FIRST row
    // becomes the band's first row.
    viewport.set_scroll(0, 40);
    CK_CHECK(viewport.ensure_visible(*child));
    CK_CHECK(viewport.scroll_y() == 30);
}

CK_TEST(ensure_visible_declines_a_view_that_is_not_inside_this_viewport) {
    Fixture f;
    ScrollViewport viewport;
    viewport.set_bounds(Rect{0, 0, 20, 10});
    viewport.set_content(make_content(10, 50));
    viewport.set_scroll(0, 20);
    View outsider;
    outsider.set_bounds(Rect{0, 0, 5, 1});
    CK_CHECK(!viewport.ensure_visible(outsider));
    CK_CHECK(!viewport.ensure_visible(*viewport.content()));  // the content has no place in itself
    CK_CHECK(viewport.scroll_y() == 20);
}

CK_TEST(wrapped_content_is_measured_at_the_width_it_is_given) {
    // The defect this pins: measuring by the width-independent hint
    // alone reports content one row tall, so no bar appears and every
    // line past the first is unreachable.
    Fixture f;
    ScrollViewport viewport;
    viewport.set_horizontal_scrollbar_policy(ScrollbarPolicy::Hidden);
    viewport.set_bounds(Rect{0, 0, 40, 5});
    viewport.set_content(std::make_unique<WrappedContent>(400));

    // 400 cells over the 39 columns left beside the vertical bar is 11
    // rows, well past the 5 on screen — so the bar is shown and the
    // content is laid out at its true height.
    CK_CHECK(viewport.content()->bounds().height > 5);
    CK_CHECK(viewport.vertical_scrollbar_policy() == ScrollbarPolicy::Auto);
    viewport.set_scroll(0, 100);
    CK_CHECK(viewport.scroll_y() > 0);  // there is somewhere to scroll to
}

CK_TEST(content_that_grows_after_layout_re_measures_the_viewport) {
    // A viewport filled in after construction - prose replaced when a
    // selection changes, a log being appended to - kept the extent its
    // content had when empty, so the bar never appeared however much
    // arrived later.
    Fixture f;
    ScrollViewport viewport;
    viewport.set_horizontal_scrollbar_policy(ScrollbarPolicy::Hidden);
    viewport.set_bounds(Rect{0, 0, 40, 5});
    auto content = std::make_unique<WrappedContent>(0);
    WrappedContent* const raw = content.get();
    viewport.set_content(std::move(content));
    CK_CHECK(viewport.content()->bounds().height == 5);  // nothing to scroll yet

    raw->set_total(400);
    CK_CHECK(viewport.content()->bounds().height > 5);
    viewport.set_scroll(0, 100);
    CK_CHECK(viewport.scroll_y() > 0);
}

CK_TEST(a_view_whose_height_ignores_width_is_measured_exactly_as_before) {
    // The generalization must not move any existing case: height_for_width
    // defaults to the stored preferred height, so a plain view still
    // reports what its hint says.
    Fixture f;
    ScrollViewport viewport;
    viewport.set_bounds(Rect{0, 0, 40, 20});
    viewport.set_content(make_content(10, 5));
    CK_CHECK(viewport.content()->bounds().height == 20);
    CK_CHECK(viewport.content()->bounds().width == 40);
}
