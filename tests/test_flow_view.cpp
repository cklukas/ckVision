// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/flow_view.hpp"

#include "cvision/testing/cktest.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/scene/compositor.hpp"
#include "cvision/term/headless_terminal.hpp"
#include "cvision/term/presenter.hpp"
#include "cvision/ui/application.hpp"
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
        out.push_back({region.id, region.visible, region.anchor, region.image});
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

CK_TEST(flow_view_replaces_one_block_without_rebuilding_the_document_value) {
    Fixture fixture;
    FlowView view;
    view.set_document(FlowDocument{{FlowBlock{{FlowText{"first", ckv::Attr::Bold, std::nullopt}}},
                                   FlowBlock{{FlowText{"old", ckv::Attr{}, std::string("old")}}}}});
    draw(view, fixture, ckv::Size{20, 4});
    CK_CHECK(view.current_link() == 0);
    CK_CHECK(view.replace_block(1, FlowBlock{{FlowText{"replacement", ckv::Attr::Underline, std::nullopt}}}));
    CK_CHECK(view.document().blocks.size() == 2);
    CK_CHECK(std::get<FlowText>(view.document().blocks[0].content.front()).text == "first");
    CK_CHECK(std::get<FlowText>(view.document().blocks[1].content.front()).text == "replacement");
    CK_CHECK(!view.current_link());
    const Surface surface = draw(view, fixture, ckv::Size{20, 4});
    CK_CHECK(row_text(surface, 0).substr(0, 5) == "first");
    CK_CHECK(row_text(surface, 2).substr(0, 11) == "replacement");
    CK_CHECK(!view.replace_block(2, FlowBlock{{FlowText{"ignored", ckv::Attr{}, std::nullopt}}}));
    CK_CHECK(std::get<FlowText>(view.document().blocks[1].content.front()).text == "replacement");
}

CK_TEST(flow_view_replaces_the_final_block_without_losing_prior_link_navigation) {
    Fixture fixture;
    FlowView view;
    view.set_document(FlowDocument{{FlowBlock{{FlowText{"first", ckv::Attr{}, std::string("first")}}},
                                   FlowBlock{{FlowText{"old", ckv::Attr{}, std::string("old")}}}}});
    draw(view, fixture, ckv::Size{20, 4});
    CK_CHECK(view.current_link() == 0);
    CK_CHECK(view.replace_block(1, FlowBlock{{FlowText{"replacement", ckv::Attr{}, std::string("replacement")}}}));
    CK_CHECK(view.line_count() == 3);
    CK_CHECK(view.link_count() == 2);
    CK_CHECK(view.current_link() == 0);
    std::string activated;
    view.on_link_activate = [&activated](const std::string& target) { activated = target; };
    CK_CHECK(view.activate_current_link());
    CK_CHECK(activated == "first");
    CK_CHECK(view.on_key(key(Key::Tab)));
    CK_CHECK(view.activate_current_link());
    CK_CHECK(activated == "replacement");
}

CK_TEST(flow_view_places_an_inline_image_through_the_scene_raster_path) {
    Fixture fixture;
    FlowView view;
    auto image = std::make_shared<ckv::Image>(ckv::PixelSize{2, 2});
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
    auto image = std::make_shared<ckv::Image>(ckv::PixelSize{4, 4});
    image->set_pixel(0, 0, ckv::Image::Rgba{255, 0, 0, 255});
    view.set_document(FlowDocument{{FlowBlock{{FlowImage{image, ckv::Size{4, 2}, "chart"}}}}});
    draw(view, fixture, ckv::Size{8, 1});
    CK_CHECK(view.on_key(key(Key::Down)));
    const Surface surface = draw(view, fixture, ckv::Size{8, 1});
    CK_CHECK(surface.raster_regions().size() == 1);
    if (surface.raster_regions().size() != 1) return;
    // D-081: the whole picture moves up the scrolled row and the clip keeps
    // the row still in view. The picture itself is never cut into a new
    // image, so each of its rows keeps the pixels it had.
    const ckv::scene::RasterRegion& region = surface.raster_regions().front();
    CK_CHECK(region.anchor == (ckv::Rect{0, -1, 4, 2}));
    CK_CHECK(region.visible == (ckv::Rect{0, 0, 4, 1}));
    CK_CHECK(region.image == image);
}

CK_TEST(flow_view_clips_an_inline_image_wider_than_the_view_at_the_scrollbar_column) {
    Fixture fixture;
    FlowView view;
    auto image = std::make_shared<ckv::Image>(ckv::PixelSize{12, 2});
    view.set_document(FlowDocument{{FlowBlock{{FlowImage{image, ckv::Size{12, 1}, "chart"}}}}});
    const Surface surface = draw(view, fixture, ckv::Size{8, 2});
    CK_CHECK(surface.raster_regions().size() == 1);
    if (surface.raster_regions().size() != 1) return;
    // Seven text columns and the scrollbar's: the picture keeps its extent
    // and shows the columns left of the bar.
    CK_CHECK(surface.raster_regions().front().anchor == (ckv::Rect{0, 0, 12, 1}));
    CK_CHECK(surface.raster_regions().front().visible == (ckv::Rect{0, 0, 7, 1}));
}

CK_TEST(flow_view_raster_uses_sixel_when_available_and_its_text_fallback_when_not) {
    Fixture fixture;
    FlowView view;
    auto image = std::make_shared<ckv::Image>(ckv::PixelSize{4, 2});
    for (int y = 0; y < image->height(); ++y)
        for (int x = 0; x < image->width(); ++x) image->set_pixel(x, y, ckv::Image::Rgba{255, 0, 0, 255});
    view.set_document(FlowDocument{{FlowBlock{{FlowImage{image, ckv::Size{4, 1}, "chart"}}}}});
    const Surface surface = draw(view, fixture, ckv::Size{8, 2});
    const auto raster_slices = slices(surface);

    ckv::term::HeadlessTerminal sixel(ckv::Size{8, 2}, ckv::term::headless_sixel_profile());
    ckv::term::Presenter sixel_presenter(sixel);
    sixel_presenter.present(surface.view(), ckv::CursorState{}, 0, raster_slices);
    CK_CHECK(sixel.written_bytes().find("\x1B" "P") != std::string::npos);
    CK_CHECK(sixel.written_bytes().find("chart") == std::string::npos);
    CK_CHECK(sixel.display().has_raster_pixels());

    ckv::term::HeadlessTerminal fallback(ckv::Size{8, 2}, ckv::term::headless_no_graphics_profile());
    ckv::term::Presenter fallback_presenter(fallback);
    fallback_presenter.present(surface.view(), ckv::CursorState{}, 0, raster_slices);
    CK_CHECK(fallback.written_bytes().find("\x1B" "P") == std::string::npos);
    CK_CHECK(!fallback.display().has_raster_pixels());
}

// --- Word wrapping -----------------------------------------------------------

CK_TEST(a_word_that_does_not_fit_moves_to_the_next_row_whole) {
    Fixture fixture;
    FlowView view;
    view.set_document(FlowDocument{{FlowBlock{{FlowText{"alpha beta", ckv::Attr{}, std::nullopt}}}}});
    const Surface surface = draw(view, fixture, ckv::Size{8, 3});
    CK_CHECK(row_text(surface, 0) == "alpha   ");
    // The separator before a moved word belongs to neither row.
    CK_CHECK(row_text(surface, 1) == "beta    ");
}

CK_TEST(a_word_split_across_emphasis_still_moves_as_one) {
    Fixture fixture;
    FlowView view;
    view.set_document(FlowDocument{{FlowBlock{{FlowText{"ab cd", ckv::Attr{}, std::nullopt},
                                               FlowText{"ef", ckv::Attr::Bold, std::nullopt}}}}});
    const Surface surface = draw(view, fixture, ckv::Size{5, 3});
    CK_CHECK(row_text(surface, 0) == "ab   ");
    CK_CHECK(row_text(surface, 1) == "cdef ");
    CK_CHECK(ckv::has_attr(surface.at(ckv::Point{2, 1}).style().attrs, ckv::Attr::Bold));
}

CK_TEST(a_word_longer_than_a_row_breaks_by_grapheme) {
    // There is no row it would fit on, so it is the one thing split. Four
    // text columns: the view keeps its last column for the scrollbar.
    Fixture fixture;
    FlowView view;
    view.set_document(FlowDocument{{FlowBlock{{FlowText{"ab abcdefghij", ckv::Attr{}, std::nullopt}}}}});
    const Surface surface = draw(view, fixture, ckv::Size{5, 5});
    CK_CHECK(row_text(surface, 0) == "ab   ");
    CK_CHECK(row_text(surface, 1) == "abcd ");
    CK_CHECK(row_text(surface, 2) == "efgh ");
    CK_CHECK(row_text(surface, 3) == "ij   ");
}

CK_TEST(the_current_link_is_marked_only_while_the_view_holds_the_keyboard) {
    Fixture fixture;
    ckv::term::HeadlessTerminal term(ckv::Size{20, 3});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    FlowView view;
    view.set_document(FlowDocument{{FlowBlock{{FlowText{"one", ckv::Attr{}, std::string("one")}}}}});
    view.set_context(ckv::ui::Context{&fixture.theme, &fixture.registry, &app});
    view.set_bounds(Rect{0, 0, 20, 3});
    Surface surface(ckv::Size{20, 3}, ckv::Cell::from_grapheme(" ", ckv::Style{}));
    Painter painter(surface, Rect{0, 0, 20, 3});
    CK_CHECK(view.current_link() == 0);

    view.draw(painter);
    CK_CHECK(ckv::has_attr(surface.at(ckv::Point{0, 0}).style().attrs, ckv::Attr::Underline));
    CK_CHECK(!ckv::has_attr(surface.at(ckv::Point{0, 0}).style().attrs, ckv::Attr::Reverse));

    app.set_focus(&view);
    view.draw(painter);
    CK_CHECK(ckv::has_attr(surface.at(ckv::Point{0, 0}).style().attrs, ckv::Attr::Reverse));
}

CK_TEST(a_scripted_flow_view_scrolls_and_activates_links_by_key_by_click_and_by_wheel) {
    // Application-level script: keys, clicks and the wheel reach the view
    // through Application::dispatch, and step() shows where it scrolled to.
    ckv::term::HeadlessTerminal term(ckv::Size{30, 8});
    ckv::ManualClock clock;
    ckv::ui::Application app(term, clock);
    const StandardRoles roles = intern_standard_roles(app.roles());
    app.theme() = make_classic_theme(app.roles(), roles);
    auto* view = app.root().add(std::make_unique<FlowView>());
    view->set_bounds(Rect{0, 0, 20, 3});
    FlowDocument document;
    document.blocks.push_back(FlowBlock{{FlowText{"See ", ckv::Attr{}, std::nullopt},
                                         FlowText{"docs", ckv::Attr{}, std::string("docs")},
                                         FlowText{" or ", ckv::Attr{}, std::nullopt},
                                         FlowText{"faq", ckv::Attr{}, std::string("faq")}}});
    for (int block = 2; block <= 6; ++block)
        document.blocks.push_back(FlowBlock{{FlowText{"block " + std::to_string(block), ckv::Attr{}, std::nullopt}}});
    view->set_document(std::move(document));
    std::vector<std::string> activated;
    view->on_link_activate = [&](const std::string& target) { activated.push_back(target); };
    app.set_focus(view);
    app.step(0);
    const auto row = [&](int y) { return row_text(app.composed_surface(), y); };
    CK_CHECK(row(0).starts_with("See docs or faq"));

    // Tab walks the links and Enter follows the current one.
    CK_CHECK(app.dispatch(key(Key::Tab)));
    CK_CHECK(app.dispatch(key(Key::Enter)));
    CK_CHECK((activated == std::vector<std::string>{"faq"}));
    // A click follows the link under the pointer.
    CK_CHECK(app.dispatch(ckv::MouseEvent{ckv::MouseAction::Down, ckv::MouseButton::Left, ckv::Point{5, 0},
                                          std::nullopt, Modifier::None}));
    app.dispatch(ckv::MouseEvent{ckv::MouseAction::Up, ckv::MouseButton::Left, ckv::Point{5, 0}, std::nullopt,
                                 Modifier::None});
    CK_CHECK((activated == std::vector<std::string>{"faq", "docs"}));

    CK_CHECK(app.dispatch(key(Key::End)));
    app.step(0);
    CK_CHECK(row(2).starts_with("block 6"));
    CK_CHECK(row(0).find("docs") == std::string::npos);
    CK_CHECK(app.dispatch(ckv::MouseEvent{ckv::MouseAction::Wheel, ckv::MouseButton::WheelUp, ckv::Point{3, 1},
                                          std::nullopt, Modifier::None}));
    CK_CHECK(app.dispatch(key(Key::Home)));
    app.step(0);
    CK_CHECK(row(0).starts_with("See docs or faq"));
}
