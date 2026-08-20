// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/flow_view.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/scene/compositor.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/presenter.hpp"
#include "cvision/ui/context.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Key;
using ckv::KeyChord;
using ckv::Modifier;
using ckv::Rect;
using ckv::scene::Painter;
using ckv::scene::Surface;
using ckv::ui::intern_standard_roles;
using ckv::ui::make_classic_theme;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::widgets::FlowBlock;
using ckv::widgets::FlowDocument;
using ckv::widgets::FlowImage;
using ckv::widgets::FlowText;
using ckv::widgets::FlowView;

namespace {

struct Fixture {
    RoleRegistry registry;
    StandardRoles roles = intern_standard_roles(registry);
    Theme theme = make_classic_theme(registry, roles);
    ckv::ui::Context context() { return {&theme, &registry, nullptr}; }
};

ckv::KeyEvent key(Key value, Modifier modifier = Modifier::None) {
    return ckv::KeyEvent{KeyChord{value, modifier, ""}};
}

std::string row_text(const Surface& surface, int row) {
    std::string out;
    for (int x = 0; x < surface.size().width; ++x) out += surface.at(ckv::Point{x, row}).grapheme();
    return out;
}

Surface draw(FlowView& view, Fixture& fixture, ckv::Size size) {
    view.set_context(fixture.context());
    view.set_bounds(Rect{0, 0, size.width, size.height});
    Surface surface(size, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(surface, Rect{0, 0, size.width, size.height});
    view.draw(painter);
    return surface;
}

std::vector<ckv::RasterSlice> slices(const Surface& surface) {
    std::vector<ckv::RasterSlice> out;
    for (const ckv::scene::RasterRegion& region : surface.raster_regions())
        out.push_back({region.id, region.anchor, region.anchor, region.image, region.fallback_active});
    return out;
}

}  // namespace

CK_TEST(flow_view_wraps_styled_text_without_splitting_a_row) {
    Fixture fixture;
    FlowView view;
    view.set_document(FlowDocument{{FlowBlock{{FlowText{"alpha beta", ckv::Attr::Bold, std::nullopt}}}}});
    const Surface surface = draw(view, fixture, ckv::Size{6, 3});
    CK_CHECK(row_text(surface, 0).substr(0, 6) == "alpha ");
    CK_CHECK(row_text(surface, 1).substr(0, 4) == "beta");
    CK_CHECK(ckv::has_attr(surface.at(ckv::Point{0, 0}).style().attrs, ckv::Attr::Bold));
}

CK_TEST(flow_view_exposes_link_navigation_and_activation) {
    Fixture fixture;
    FlowView view;
    view.set_document(FlowDocument{{FlowBlock{{FlowText{"one", ckv::Attr{}, std::string("one")},
                                               FlowText{" two", ckv::Attr{}, std::string("two")}}}}});
    draw(view, fixture, ckv::Size{20, 3});
    std::string activated;
    view.on_link_activate = [&activated](const std::string& target) { activated = target; };
    CK_CHECK(view.current_link() == 0);
    CK_CHECK(view.on_key(key(Key::Tab)));
    CK_CHECK(view.current_link() == 1);
    CK_CHECK(view.on_key(key(Key::Enter)));
    CK_CHECK(activated == "two");
}

CK_TEST(flow_view_places_an_inline_image_through_the_scene_raster_path) {
    Fixture fixture;
    FlowView view;
    auto image = std::make_shared<ckv::Image>(2, 2);
    image->set_pixel(0, 0, ckv::Image::Rgba{255, 0, 0, 255});
    view.set_document(FlowDocument{{FlowBlock{{FlowImage{image, ckv::Size{5, 2}, "chart"}}}}});
    const Surface surface = draw(view, fixture, ckv::Size{10, 4});
    CK_CHECK(surface.raster_regions().size() == 1);
    CK_CHECK(surface.raster_regions().front().anchor == (ckv::Rect{0, 0, 5, 2}));
    CK_CHECK(row_text(surface, 0).substr(0, 5) == "chart");
}

CK_TEST(flow_view_scrolls_wrapped_display_rows) {
    Fixture fixture;
    FlowView view;
    view.set_document(FlowDocument{{FlowBlock{{FlowText{"one two three four five", ckv::Attr{}, std::nullopt}}}}});
    draw(view, fixture, ckv::Size{5, 2});
    CK_CHECK(view.on_key(key(Key::Down)));
    CK_CHECK(view.top_line() == 1);
}

CK_TEST(flow_view_clips_a_scrolled_inline_image_to_the_visible_flow_rows) {
    Fixture fixture;
    FlowView view;
    auto image = std::make_shared<ckv::Image>(4, 4);
    image->set_pixel(0, 0, ckv::Image::Rgba{255, 0, 0, 255});
    view.set_document(FlowDocument{{FlowBlock{{FlowImage{image, ckv::Size{4, 2}, "chart"}}}}});
    draw(view, fixture, ckv::Size{8, 1});
    CK_CHECK(view.on_key(key(Key::Down)));
    const Surface surface = draw(view, fixture, ckv::Size{8, 1});
    CK_CHECK(surface.raster_regions().size() == 1);
    CK_CHECK(surface.raster_regions().front().anchor == (ckv::Rect{0, 0, 4, 1}));
    CK_CHECK(surface.raster_regions().front().image->height() == 2);
}

CK_TEST(flow_view_raster_uses_sixel_when_available_and_its_text_fallback_when_not) {
    Fixture fixture;
    FlowView view;
    auto image = std::make_shared<ckv::Image>(4, 2);
    for (int y = 0; y < image->height(); ++y)
        for (int x = 0; x < image->width(); ++x) image->set_pixel(x, y, ckv::Image::Rgba{255, 0, 0, 255});
    view.set_document(FlowDocument{{FlowBlock{{FlowImage{image, ckv::Size{4, 1}, "chart"}}}}});
    const Surface surface = draw(view, fixture, ckv::Size{8, 2});
    const auto raster_slices = slices(surface);

    ckv::term::HeadlessTerminal sixel(ckv::Size{8, 2}, ckv::term::headless_sixel_profile());
    ckv::term::Presenter sixel_presenter(sixel);
    sixel_presenter.present(surface.view(), ckv::CursorState{}, raster_slices);
    CK_CHECK(sixel.written_bytes().find("\x1B" "P") != std::string::npos);
    CK_CHECK(sixel.written_bytes().find("chart") == std::string::npos);
    CK_CHECK(sixel.display().has_raster_pixels());

    ckv::term::HeadlessTerminal fallback(ckv::Size{8, 2}, ckv::term::headless_no_graphics_profile());
    ckv::term::Presenter fallback_presenter(fallback);
    fallback_presenter.present(surface.view(), ckv::CursorState{}, raster_slices);
    CK_CHECK(fallback.written_bytes().find("\x1B" "P") == std::string::npos);
    CK_CHECK(!fallback.display().has_raster_pixels());
}
