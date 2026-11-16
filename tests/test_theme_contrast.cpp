// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT

#include <array>
#include <cmath>
#include <tuple>
#include <vector>

#include "cvision/core/cell.hpp"
#include "cvision/scene/box_drawing.hpp"
#include "cvision/scene/painter.hpp"
#include "cvision/scene/surface.hpp"
#include "cvision/testing/cktest.hpp"
#include "cvision/ui/application.hpp"
#include "cvision/ui/standard_roles.hpp"
#include "cvision/widgets/button.hpp"
#include "cvision/widgets/input_line.hpp"
#include "cvision/widgets/memo.hpp"
#include "cvision/term/headless_terminal.hpp"

namespace {

using ckv::Attr;
using ckv::Cell;
using ckv::Color;
using ckv::Point;
using ckv::Rect;
using ckv::Size;
using ckv::Style;
using ckv::ui::RoleId;
using ckv::ui::RoleRegistry;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;

std::tuple<int, int, int> to_rgb(Color c) {
    if (c.is_rgb()) return {c.r(), c.g(), c.b()};
    // Classic 16-color VGA palette matching standard_roles.cpp
    static constexpr std::array<std::tuple<int, int, int>, 16> kAnsi{{
        {0, 0, 0},       {170, 0, 0},     {0, 170, 0},     {170, 85, 0},
        {0, 0, 170},     {170, 0, 170},   {0, 170, 170},   {170, 170, 170},
        {85, 85, 85},    {255, 85, 85},   {85, 255, 85},   {255, 255, 85},
        {85, 85, 255},   {255, 85, 255},  {85, 255, 255},  {255, 255, 255},
    }};
    if (c.is_indexed() && c.index() < 16) return kAnsi[c.index()];
    return {128, 128, 128};
}

double channel_luminance(int channel_8bit) {
    const double srgb = static_cast<double>(channel_8bit) / 255.0;
    return srgb <= 0.03928 ? srgb / 12.92 : std::pow((srgb + 0.055) / 1.055, 2.4);
}

double relative_luminance(Color c) {
    const auto [r, g, b] = to_rgb(c);
    return 0.2126 * channel_luminance(r) + 0.7152 * channel_luminance(g) +
           0.0722 * channel_luminance(b);
}

double contrast_ratio(Color fg, Color bg) {
    const double l1 = relative_luminance(fg);
    const double l2 = relative_luminance(bg);
    const double lighter = std::max(l1, l2);
    const double darker = std::min(l1, l2);
    return (lighter + 0.05) / (darker + 0.05);
}

bool is_grayscale(Color c) {
    const auto [r, g, b] = to_rgb(c);
    return r == g && g == b;
}

}  // namespace

CK_TEST(all_five_builtin_themes_have_zero_attr_reverse_and_wcag_editor_contrast) {
    RoleRegistry registry;
    const StandardRoles roles = ckv::ui::intern_standard_roles(registry);

    struct NamedTheme {
        const char* name;
        Theme theme;
        bool mono_only;
        bool high_contrast;
    };
    std::vector<NamedTheme> themes;
    themes.push_back({"Classic", ckv::ui::make_classic_theme(registry, roles), false, false});
    themes.push_back({"Dark", ckv::ui::make_dark_theme(registry, roles), false, false});
    themes.push_back({"Light", ckv::ui::make_light_theme(registry, roles), false, false});
    themes.push_back({"Mono", ckv::ui::make_mono_theme(registry, roles), true, false});
    themes.push_back({"HighContrast", ckv::ui::make_high_contrast_theme(registry, roles), true, true});

    const std::array key_roles{
        roles.desktop_background, roles.dialog_background, roles.label_text,
        roles.button_normal,      roles.button_focused,    roles.button_hovered,
        roles.button_default,     roles.button_pressed,    roles.input_normal,
        roles.input_focused,      roles.input_selection,   roles.input_invalid,
        roles.window_title_active, roles.window_frame_moving, roles.menu_bar_normal,
        roles.menu_bar_active,    roles.menu_dropdown_highlighted, roles.list_normal,
        roles.list_selected,      roles.table_header,      roles.option_focused,
        roles.splitter_focused,   roles.editor_text,       roles.editor_gutter,
        roles.editor_selection,   roles.editor_search,     roles.editor_syntax_plain,
        roles.editor_syntax_keyword, roles.editor_syntax_type, roles.editor_syntax_string,
        roles.editor_syntax_number, roles.editor_syntax_comment, roles.editor_syntax_error,
    };

    for (const NamedTheme& entry : themes) {
        for (const RoleId role : key_roles) {
            const Style s = entry.theme.resolve(role);
            CK_CHECK(!ckv::has_attr(s.attrs, Attr::Reverse));
            if (entry.high_contrast) CK_CHECK(!ckv::has_attr(s.attrs, Attr::Dim));
            if (entry.mono_only) {
                CK_CHECK(is_grayscale(s.fg));
                CK_CHECK(is_grayscale(s.bg));
            }
        }

        const std::array wcag_aa_editor_roles{
            roles.editor_text,          roles.editor_gutter,
            roles.editor_selection,     roles.editor_search,
            roles.editor_syntax_plain,  roles.editor_syntax_keyword,
            roles.editor_syntax_type,   roles.editor_syntax_string,
            roles.editor_syntax_number, roles.editor_syntax_comment,
            roles.editor_syntax_error,
        };
        for (const RoleId role : wcag_aa_editor_roles) {
            const Style s = entry.theme.resolve(role);
            CK_CHECK(contrast_ratio(s.fg, s.bg) >= 4.5);
        }
    }
}

CK_TEST(button_shadow_samples_underlying_surface_background) {
    ckv::term::HeadlessTerminal terminal(Size{20, 6});
    ckv::ManualClock clock;
    ckv::ui::Application app{terminal, clock};
    const StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);

    ckv::scene::Surface surface(Size{20, 6}, Cell::from_grapheme(" ", Style{ckv::Color::rgb(255, 255, 255), ckv::Color::rgb(0, 0, 170), Attr{}}));
    ckv::scene::Painter painter(surface, Rect{0, 0, 20, 6});
    ckv::widgets::Button button("OK");
    button.set_context(ckv::ui::Context{&app.theme(), &app.roles(), &app});
    button.set_bounds(Rect{2, 1, 10, 2});
    auto bp = painter.translated(Point{2, 1}, Rect{0, 0, 10, 2});
    button.draw(bp);

    // Bottom shadow cell must inherit the blue background underneath, not gray dialog background.
    CK_CHECK(surface.at(Point{4, 2}).style().bg == ckv::Color::rgb(0, 0, 170));
}

CK_TEST(mixed_single_double_box_drawing_junctions_round_trip) {
    using ckv::scene::Junction;
    using ckv::scene::LineStyle;
    CK_CHECK(ckv::scene::mixed_junction_glyph(Junction{true, true, false, true}, LineStyle::Double, LineStyle::Single) == "╞");
    CK_CHECK(ckv::scene::mixed_junction_glyph(Junction{true, true, true, false}, LineStyle::Double, LineStyle::Single) == "╡");
    CK_CHECK(ckv::scene::mixed_junction_glyph(Junction{false, true, true, true}, LineStyle::Double, LineStyle::Single) == "╤");
    CK_CHECK(ckv::scene::mixed_junction_glyph(Junction{true, false, true, true}, LineStyle::Double, LineStyle::Single) == "╧");
    CK_CHECK(ckv::scene::mixed_junction_glyph(Junction{true, true, true, true}, LineStyle::Double, LineStyle::Single) == "╪");

    const auto info = ckv::scene::classify_junction_glyph("╪");
    CK_CHECK(info.has_value());
    if (info) {
        CK_CHECK(info->horizontal_style == LineStyle::Double);
        CK_CHECK(info->style == LineStyle::Single);
        CK_CHECK(info->junction == (Junction{true, true, true, true}));
    }
}

CK_TEST(input_line_and_memo_publish_cursor_state_when_focused) {
    ckv::term::HeadlessTerminal terminal(Size{30, 10});
    ckv::ManualClock clock;
    ckv::ui::Application app{terminal, clock};
    const StandardRoles roles = ckv::ui::intern_standard_roles(app.roles());
    app.theme() = ckv::ui::make_classic_theme(app.roles(), roles);

    auto* input = app.root().make<ckv::widgets::InputLine>();
    input->set_bounds(Rect{1, 1, 12, 1});
    input->set_text("hello");
    app.set_focus(input);
    app.step(0);
    CK_CHECK(input->cursor_state().has_value());
    if (input->cursor_state()) {
        CK_CHECK(input->cursor_state()->position == (Point{6, 1}));
    }

    auto* memo = app.root().make<ckv::widgets::Memo>();
    memo->set_bounds(Rect{1, 3, 15, 4});
    memo->set_text("abc\ndef");
    app.set_focus(memo);
    app.step(0);
    CK_CHECK(memo->cursor_state().has_value());
    if (memo->cursor_state()) {
        CK_CHECK(memo->cursor_state()->position == (Point{1, 3}));
    }
}
