// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/theme_format.hpp"

#include <string>
#include <string_view>
#include <vector>

#include "cvision/testing/cktest.hpp"
#include "cvision/ui/standard_roles.hpp"

using ckv::Attr;
using ckv::Color;
using ckv::ShadowStyle;
using ckv::Style;
using ckv::UnderlineShape;
using ckv::ui::format_color;
using ckv::ui::intern_standard_roles;
using ckv::ui::kMaxThemeRoleNameBytes;
using ckv::ui::kMaxThemeTextBytes;
using ckv::ui::parse_color;
using ckv::ui::parse_theme;
using ckv::ui::RoleId;
using ckv::ui::RoleRegistry;
using ckv::ui::serialize_theme;
using ckv::ui::StandardRoles;
using ckv::ui::Theme;
using ckv::ui::ThemeParseResult;

namespace {

// Two roles, interned in the order the widgets of a small program might
// first attach: not the order the text lists them in.
struct SmallRegistry {
    RoleRegistry registry;
    RoleId title = registry.intern("app.title", Style{Color::rgb(255, 255, 255), Color::indexed(4), Attr::Bold});
    RoleId body = registry.intern("app.body", Style{});
};

constexpr std::string_view kSmallText =
    "ckvision-theme 1\n"
    "shadow halve\n"
    "app.body fg default bg default attrs -\n"
    "app.title fg #FFFFFF bg @4 attrs bold\n";

// Every theme a parse of `text` over a fresh small registry refuses reports
// the line it refused.
int refused_line(std::string_view text) {
    const SmallRegistry small;
    const Theme base(small.registry);
    const ThemeParseResult result = parse_theme(text, base);
    CK_CHECK(!result);
    CK_CHECK(!result.theme.has_value());
    CK_CHECK(!result.error.message.empty());
    CK_CHECK(result.unknown_roles.empty());
    return result.error.line;
}

std::string with_body_line(std::string_view line) {
    return "ckvision-theme 1\n" + std::string(line) + "\n";
}

}  // namespace

CK_TEST(format_color_spells_default_palette_and_rgb_colours_as_the_golden_format_does) {
    CK_CHECK(format_color(Color::default_color()) == "default");
    CK_CHECK(format_color(Color::indexed(0)) == "@0");
    CK_CHECK(format_color(Color::indexed(255)) == "@255");
    CK_CHECK(format_color(Color::rgb(0, 170, 255)) == "#00AAFF");
}

CK_TEST(parse_color_reads_every_spelling_and_refuses_anything_else) {
    CK_CHECK(parse_color("default") == Color::default_color());
    CK_CHECK(parse_color("@17") == Color::indexed(17));
    CK_CHECK(parse_color("#00aaFF") == Color::rgb(0, 170, 255));
    for (const std::string_view bad : {"", "Default", "@", "@256", "@01", "@-1", "@+1", "@1x", "#12345", "#1234567",
                                       "#GG0000", "00AAFF", "red", "@ 1"})
        CK_CHECK(!parse_color(bad).has_value());
}

CK_TEST(a_theme_serializes_every_role_in_name_order_with_its_resolved_style) {
    SmallRegistry small;
    const Theme theme(small.registry);
    CK_CHECK(serialize_theme(theme) == kSmallText);
}

CK_TEST(serializing_does_not_depend_on_the_order_roles_were_interned_in) {
    RoleRegistry reversed;
    reversed.intern("app.body", Style{});
    reversed.intern("app.title", Style{Color::rgb(255, 255, 255), Color::indexed(4), Attr::Bold});
    CK_CHECK(serialize_theme(Theme(reversed)) == kSmallText);
}

CK_TEST(underline_shape_and_colour_are_written_only_on_an_underlined_style) {
    RoleRegistry registry;
    const RoleId marked = registry.intern(
        "app.error", Style{Color::indexed(1), Color::default_color(), Attr::Underline | Attr::Italic,
                           UnderlineShape::Curly, Color::rgb(255, 0, 0)});
    const RoleId plain = registry.intern("app.plain", Style{});
    Theme theme(registry);
    // A shape without the underline is not part of how the cell looks.
    theme.set(plain, Style{Color{}, Color{}, Attr::Bold, UnderlineShape::Dotted, Color::indexed(3)});
    CK_CHECK(serialize_theme(theme) ==
             "ckvision-theme 1\n"
             "shadow halve\n"
             "app.error fg @1 bg default attrs italic,underline underline curly ulcolor #FF0000\n"
             "app.plain fg default bg default attrs bold\n");
    (void)marked;
}

CK_TEST(every_built_in_scheme_round_trips_through_its_text_byte_for_byte_and_role_for_role) {
    RoleRegistry registry;
    const StandardRoles roles = intern_standard_roles(registry);
    // An extension role with every refinement a style can carry.
    const RoleId extension = registry.intern(
        "vendor.chart.axis", Style{Color::indexed(208), Color::rgb(1, 2, 3), Attr::Underline | Attr::Strike | Attr::Dim,
                                   UnderlineShape::Dashed, Color::indexed(9)});
    const Theme base(registry);
    for (Theme theme : {ckv::ui::make_classic_theme(registry, roles), ckv::ui::make_dark_theme(registry, roles),
                        ckv::ui::make_light_theme(registry, roles), ckv::ui::make_mono_theme(registry, roles),
                        ckv::ui::make_high_contrast_theme(registry, roles)}) {
        theme.set(roles.tooltip, Style{Color::default_color(), Color::indexed(11),
                                       Attr::Bold | Attr::Dim | Attr::Italic | Attr::Reverse});
        const std::string text = serialize_theme(theme);
        const ThemeParseResult parsed = parse_theme(text, base);
        CK_CHECK(static_cast<bool>(parsed));
        if (!parsed) continue;
        CK_CHECK(parsed.unknown_roles.empty());
        CK_CHECK(serialize_theme(*parsed.theme) == text);
        // The base halves; the text's own shadow wins, Classic's recolouring included.
        CK_CHECK(parsed.theme->shadow() == theme.shadow());
        for (std::size_t role = 0; role < registry.size(); ++role)
            CK_CHECK(parsed.theme->resolve(static_cast<RoleId>(role)) == theme.resolve(static_cast<RoleId>(role)));
        CK_CHECK(parsed.theme->resolve(extension) == registry.fallback(extension));
    }
}

CK_TEST(a_role_the_text_does_not_name_keeps_the_base_themes_style) {
    RoleRegistry registry;
    const StandardRoles roles = intern_standard_roles(registry);
    const Theme dark = ckv::ui::make_dark_theme(registry, roles);
    const std::string text = "ckvision-theme 1\nckv.tooltip fg @0 bg @11 attrs -\n";
    const ThemeParseResult parsed = parse_theme(text, dark);
    CK_CHECK(static_cast<bool>(parsed));
    if (!parsed) return;
    CK_CHECK(parsed.theme->resolve(roles.tooltip) == (Style{Color::indexed(0), Color::indexed(11), Attr{}}));
    CK_CHECK(parsed.theme->resolve(roles.dialog_frame) == dark.resolve(roles.dialog_frame));
    CK_CHECK(parsed.theme->resolve(roles.list_selected) == dark.resolve(roles.list_selected));
}

CK_TEST(a_role_the_registry_does_not_hold_is_reported_and_skipped_not_refused) {
    const SmallRegistry small;
    const Theme base(small.registry);
    const ThemeParseResult parsed = parse_theme(
        "ckvision-theme 1\n"
        "retired.role fg @1 bg @2 attrs -\n"
        "app.title fg @3 bg @4 attrs -\n"
        "vendor.new fg default bg default attrs -\n",
        base);
    CK_CHECK(static_cast<bool>(parsed));
    CK_CHECK(parsed.unknown_roles == (std::vector<std::string>{"retired.role", "vendor.new"}));
    if (parsed) CK_CHECK(parsed.theme->resolve(small.title) == (Style{Color::indexed(3), Color::indexed(4), Attr{}}));
}

CK_TEST(parse_accepts_the_attributes_in_any_order_and_writes_them_back_in_flag_order) {
    const SmallRegistry small;
    const Theme base(small.registry);
    const ThemeParseResult parsed =
        parse_theme("ckvision-theme 1\napp.title fg @1 bg @2 attrs strike,bold,reverse\n", base);
    CK_CHECK(static_cast<bool>(parsed));
    if (!parsed) return;
    CK_CHECK(parsed.theme->resolve(small.title).attrs == (Attr::Bold | Attr::Reverse | Attr::Strike));
    CK_CHECK(serialize_theme(*parsed.theme).find("app.title fg @1 bg @2 attrs bold,reverse,strike\n") !=
             std::string::npos);
}

CK_TEST(parse_refuses_a_text_that_is_not_whole) {
    CK_CHECK(refused_line("") == 0);
    CK_CHECK(refused_line("ckvision-theme 1") == 0);
    CK_CHECK(refused_line("ckvision-theme 1\napp.title fg @1 bg @2 attrs -") == 0);
    CK_CHECK(refused_line(std::string(kMaxThemeTextBytes + 1, '\n')) == 0);
    CK_CHECK(refused_line("ckvision-theme 2\n") == 1);
    CK_CHECK(refused_line("ckvision-theme 1 \n") == 1);
    CK_CHECK(refused_line("\n") == 1);
}

CK_TEST(parse_refuses_control_characters_and_bytes_outside_printable_ascii_anywhere) {
    CK_CHECK(refused_line("ckvision-theme 1\r\n") == 1);
    CK_CHECK(refused_line(with_body_line("app.title fg @1 bg @2 attrs -\r")) == 2);
    CK_CHECK(refused_line(with_body_line("app.title\tfg @1 bg @2 attrs -")) == 2);
    CK_CHECK(refused_line(with_body_line(std::string("app.title fg @1 bg @2 attrs -\0", 30))) == 2);
    CK_CHECK(refused_line(with_body_line("app.t\x1b[2Jitle fg @1 bg @2 attrs -")) == 2);
    CK_CHECK(refused_line(with_body_line("app.t\xc3\xa9tle fg @1 bg @2 attrs -")) == 2);
    CK_CHECK(refused_line(with_body_line("app.title fg @1 bg @2 attrs -\x7f")) == 2);
}

CK_TEST(parse_refuses_a_malformed_role_line_and_names_it) {
    const std::vector<std::string_view> malformed = {
        "",                                                        // an empty line
        "app.title  fg @1 bg @2 attrs -",                          // two spaces
        " app.title fg @1 bg @2 attrs -",                          // a leading space
        "app.title fg @1 bg @2 attrs - ",                          // a trailing space
        "app.title fg @1 bg @2",                                   // no attributes
        "app.title bg @1 fg @2 attrs -",                           // fields out of order
        "app.title fg @256 bg @2 attrs -",                         // an index past the palette
        "app.title fg #12345 bg @2 attrs -",                       // a short RGB colour
        "app.title fg @1 bg blue attrs -",                         // a colour name
        "app.title fg @1 bg @2 attrs blink",                       // an unknown attribute
        "app.title fg @1 bg @2 attrs bold,bold",                   // a duplicated attribute
        "app.title fg @1 bg @2 attrs bold,",                       // an empty attribute
        "app.title fg @1 bg @2 attrs ,bold",                       // an empty attribute
        "app.title fg @1 bg @2 attrs bold underline curly",        // a shape without the underline
        "app.title fg @1 bg @2 attrs underline underline wavy",    // an unknown shape
        "app.title fg @1 bg @2 attrs underline underline",         // a shape keyword with no shape
        "app.title fg @1 bg @2 attrs underline underline straight",  // the plain rule is never written
        "app.title fg @1 bg @2 attrs bold ulcolor @3",             // an underline colour without the underline
        "app.title fg @1 bg @2 attrs underline ulcolor default",   // an underline colour that is none
        "app.title fg @1 bg @2 attrs underline ulcolor @3 underline curly",  // refinements out of order
        "app.title fg @1 bg @2 attrs - extra",                     // a trailing field
    };
    for (const std::string_view line : malformed) CK_CHECK(refused_line(with_body_line(line)) == 2);
}

CK_TEST(parse_refuses_a_role_named_twice_and_a_role_name_longer_than_the_format_spells) {
    CK_CHECK(refused_line("ckvision-theme 1\n"
                          "app.title fg @1 bg @2 attrs -\n"
                          "app.body fg @1 bg @2 attrs -\n"
                          "app.title fg @3 bg @4 attrs -\n") == 4);
    const std::string longest(kMaxThemeRoleNameBytes, 'r');
    const SmallRegistry small;
    const Theme base(small.registry);
    const ThemeParseResult accepted = parse_theme(with_body_line(longest + " fg @1 bg @2 attrs -"), base);
    CK_CHECK(static_cast<bool>(accepted));
    CK_CHECK(accepted.unknown_roles == std::vector<std::string>{longest});
    CK_CHECK(refused_line(with_body_line(longest + "r fg @1 bg @2 attrs -")) == 2);
}

CK_TEST(every_single_byte_corruption_of_a_theme_text_is_refused_or_reads_back_canonically) {
    // Hostile input, exhaustively for one short text: replace each byte in
    // turn with each of a set of bytes that matter to the grammar. Nothing
    // crashes, a refusal always names a line, and anything accepted is a
    // theme whose own text reads back to itself.
    RoleRegistry registry;
    const RoleId title = registry.intern("app.title", Style{});
    registry.intern("app.body", Style{});
    const Theme base(registry);
    Theme theme(registry);
    theme.set(title, Style{Color::rgb(18, 52, 86), Color::indexed(200), Attr::Underline, UnderlineShape::Double,
                           Color::indexed(5)});
    const std::string text = serialize_theme(theme);
    for (std::size_t position = 0; position < text.size(); ++position) {
        for (const char replacement : {'\0', '\n', '\r', ' ', ',', '#', '@', '-', '0', '9', 'F', 'z', '\x7f', '\x80'}) {
            std::string corrupted = text;
            corrupted[position] = replacement;
            const ThemeParseResult parsed = parse_theme(corrupted, base);
            if (!parsed) {
                CK_CHECK(!parsed.error.message.empty());
                continue;
            }
            const std::string canonical = serialize_theme(*parsed.theme);
            const ThemeParseResult reparsed = parse_theme(canonical, base);
            CK_CHECK(static_cast<bool>(reparsed));
            if (reparsed) CK_CHECK(serialize_theme(*reparsed.theme) == canonical);
        }
    }
}

CK_TEST(the_shadow_line_names_a_halving_or_a_recolouring_shadow_right_after_the_header) {
    RoleRegistry registry;
    const StandardRoles roles = intern_standard_roles(registry);
    const std::string classic = serialize_theme(ckv::ui::make_classic_theme(registry, roles));
    const std::string dark = serialize_theme(ckv::ui::make_dark_theme(registry, roles));
    CK_CHECK(classic.rfind("ckvision-theme 1\nshadow recolor fg #555555 bg #000000\n", 0) == 0);
    CK_CHECK(dark.rfind("ckvision-theme 1\nshadow halve\n", 0) == 0);

    SmallRegistry small;
    const Theme base(small.registry);
    const ThemeParseResult recoloured =
        parse_theme("ckvision-theme 1\nshadow recolor fg @8 bg default\napp.body fg default bg default attrs -\n", base);
    CK_CHECK(static_cast<bool>(recoloured));
    if (recoloured)
        CK_CHECK(recoloured.theme->shadow() == ShadowStyle::recolor(Color::indexed(8), Color::default_color()));
}

CK_TEST(a_text_without_a_shadow_line_keeps_the_base_themes_shadow) {
    RoleRegistry registry;
    const StandardRoles roles = intern_standard_roles(registry);
    const Theme classic = ckv::ui::make_classic_theme(registry, roles);
    const ThemeParseResult parsed = parse_theme("ckvision-theme 1\nckv.tooltip fg @0 bg @11 attrs -\n", classic);
    CK_CHECK(static_cast<bool>(parsed));
    if (parsed) CK_CHECK(parsed.theme->shadow() == classic.shadow());
}

CK_TEST(parse_refuses_a_malformed_shadow_line_and_reads_one_only_after_the_header) {
    CK_CHECK(refused_line("ckvision-theme 1\nshadow halve now\n") == 2);
    CK_CHECK(refused_line("ckvision-theme 1\nshadow recolor fg #555555\n") == 2);
    CK_CHECK(refused_line("ckvision-theme 1\nshadow recolor fg grey bg #000000\n") == 2);
    CK_CHECK(refused_line("ckvision-theme 1\nshadow recolor bg #000000 fg #555555\n") == 2);
    // Anywhere else it is a role line, and not a well-formed one.
    CK_CHECK(refused_line("ckvision-theme 1\napp.body fg default bg default attrs -\nshadow halve\n") == 3);

    // A role that happens to be named `shadow` is still a role, on line 2 too.
    RoleRegistry registry;
    const RoleId shadow_role = registry.intern("shadow", Style{});
    const ThemeParseResult parsed = parse_theme("ckvision-theme 1\nshadow fg @1 bg @2 attrs -\n", Theme(registry));
    CK_CHECK(static_cast<bool>(parsed));
    if (parsed) {
        CK_CHECK(parsed.theme->resolve(shadow_role) == (Style{Color::indexed(1), Color::indexed(2), Attr{}}));
        CK_CHECK(parsed.theme->shadow() == ShadowStyle::halve());
    }
}
