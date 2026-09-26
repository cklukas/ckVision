// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/ui/theme_format.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <set>
#include <utility>

#include "cvision/core/assert.hpp"

namespace ckv::ui {
namespace {

// The attribute flags in the order a line writes them: bit order, so the
// spelling of a set never depends on how it was built.
struct AttrName {
    Attr flag;
    std::string_view name;
};
constexpr std::array<AttrName, 6> kAttrNames{{
    {Attr::Bold, "bold"},
    {Attr::Dim, "dim"},
    {Attr::Italic, "italic"},
    {Attr::Underline, "underline"},
    {Attr::Reverse, "reverse"},
    {Attr::Strike, "strike"},
}};

// The underline shapes that are written down. The plain rule is what an
// underline is unless something says otherwise, so it is spelled by its
// absence and never appears here.
struct ShapeName {
    UnderlineShape shape;
    std::string_view name;
};
constexpr std::array<ShapeName, 4> kShapeNames{{
    {UnderlineShape::Double, "double"},
    {UnderlineShape::Curly, "curly"},
    {UnderlineShape::Dotted, "dotted"},
    {UnderlineShape::Dashed, "dashed"},
}};

int hex_value(char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Canonical decimal 0-255: digits only, no leading zero but "0" itself.
std::optional<std::uint8_t> parse_index(std::string_view token) noexcept {
    if (token.empty() || token.size() > 3) return std::nullopt;
    if (token.size() > 1 && token[0] == '0') return std::nullopt;
    int value = 0;
    const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
    if (error != std::errc{} || end != token.data() + token.size() || value < 0 || value > 255) return std::nullopt;
    return static_cast<std::uint8_t>(value);
}

std::string format_attrs(Attr attrs) {
    std::string out;
    for (const AttrName& entry : kAttrNames) {
        if (!has_attr(attrs, entry.flag)) continue;
        if (!out.empty()) out += ',';
        out += entry.name;
    }
    return out.empty() ? std::string{"-"} : out;
}

// "-" or a comma-separated list of attribute names, each at most once.
std::optional<Attr> parse_attrs(std::string_view token) noexcept {
    Attr attrs = static_cast<Attr>(0);
    if (token == "-") return attrs;
    std::size_t position = 0;
    while (true) {
        const std::size_t comma = token.find(',', position);
        const std::string_view name =
            token.substr(position, comma == std::string_view::npos ? std::string_view::npos : comma - position);
        const auto found = std::find_if(kAttrNames.begin(), kAttrNames.end(),
                                        [name](const AttrName& entry) { return entry.name == name; });
        if (found == kAttrNames.end() || has_attr(attrs, found->flag)) return std::nullopt;
        attrs |= found->flag;
        if (comma == std::string_view::npos) return attrs;
        position = comma + 1;
    }
}

std::string format_style(const Style& style) {
    std::string out = "fg " + format_color(style.fg) + " bg " + format_color(style.bg) + " attrs " +
                      format_attrs(style.attrs);
    if (!has_attr(style.attrs, Attr::Underline)) return out;
    for (const ShapeName& entry : kShapeNames)
        if (entry.shape == style.underline) out += " underline " + std::string(entry.name);
    if (!style.underline_color.is_default()) out += " ulcolor " + format_color(style.underline_color);
    return out;
}

// Splits a line at single spaces. An empty field — two spaces in a row, or a
// space at either end — is refused by returning nothing.
std::optional<std::vector<std::string_view>> fields_of(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t position = 0;
    while (true) {
        const std::size_t space = line.find(' ', position);
        const std::string_view field =
            line.substr(position, space == std::string_view::npos ? std::string_view::npos : space - position);
        if (field.empty()) return std::nullopt;
        fields.push_back(field);
        if (space == std::string_view::npos) return fields;
        position = space + 1;
    }
}

// One role line's style, or the reason it is not one. Fields [1, size) of
// `fields` are the style; field 0 is the role name, checked by the caller.
std::optional<Style> parse_style_fields(const std::vector<std::string_view>& fields, std::string& why) {
    if (fields.size() < 7 || fields[1] != "fg" || fields[3] != "bg" || fields[5] != "attrs") {
        why = "expected '<role> fg <color> bg <color> attrs <attrs>'";
        return std::nullopt;
    }
    Style style;
    const std::optional<Color> fg = parse_color(fields[2]);
    const std::optional<Color> bg = parse_color(fields[4]);
    const std::optional<Attr> attrs = parse_attrs(fields[6]);
    if (!fg || !bg) {
        why = "a colour is not 'default', '@<0-255>' or '#RRGGBB'";
        return std::nullopt;
    }
    if (!attrs) {
        why = "attributes are not '-' or distinct names from bold, dim, italic, underline, reverse, strike";
        return std::nullopt;
    }
    style.fg = *fg;
    style.bg = *bg;
    style.attrs = *attrs;
    const bool underlined = has_attr(style.attrs, Attr::Underline);
    std::size_t next = 7;
    if (next < fields.size() && fields[next] == "underline") {
        const std::string_view shape = next + 1 < fields.size() ? fields[next + 1] : std::string_view{};
        const auto found = std::find_if(kShapeNames.begin(), kShapeNames.end(),
                                        [shape](const ShapeName& entry) { return entry.name == shape; });
        if (!underlined || found == kShapeNames.end()) {
            why = "'underline <shape>' needs an underlined style and one of double, curly, dotted, dashed";
            return std::nullopt;
        }
        style.underline = found->shape;
        next += 2;
    }
    if (next < fields.size() && fields[next] == "ulcolor") {
        const std::optional<Color> color =
            next + 1 < fields.size() ? parse_color(fields[next + 1]) : std::optional<Color>{};
        if (!underlined || !color || color->is_default()) {
            why = "'ulcolor <color>' needs an underlined style and a colour other than default";
            return std::nullopt;
        }
        style.underline_color = *color;
        next += 2;
    }
    if (next != fields.size()) {
        why = "unexpected fields after the style";
        return std::nullopt;
    }
    return style;
}

// The keyword that opens the shadow line, and the two transforms it names.
constexpr std::string_view kShadowKeyword = "shadow";
constexpr std::string_view kShadowHalve = "halve";
constexpr std::string_view kShadowRecolor = "recolor";

std::string format_shadow(const ShadowStyle& shadow) {
    std::string out{kShadowKeyword};
    out += ' ';
    if (shadow.kind() == ShadowStyle::Kind::Halve) {
        out += kShadowHalve;
        return out;
    }
    out += kShadowRecolor;
    out += " fg " + format_color(shadow.foreground()) + " bg " + format_color(shadow.background());
    return out;
}

// Whether `fields` is a shadow line rather than a role line. A role line's
// second field is always `fg`, so a role that happens to be named `shadow` is
// never mistaken for one.
bool is_shadow_line(const std::vector<std::string_view>& fields) noexcept {
    return fields.size() >= 2 && fields[0] == kShadowKeyword &&
           (fields[1] == kShadowHalve || fields[1] == kShadowRecolor);
}

// A shadow line's transform, or the reason it is not one.
std::optional<ShadowStyle> parse_shadow_fields(const std::vector<std::string_view>& fields, std::string& why) {
    if (fields[1] == kShadowHalve) {
        if (fields.size() == 2) return ShadowStyle::halve();
        why = "unexpected fields after 'shadow halve'";
        return std::nullopt;
    }
    if (fields.size() != 6 || fields[2] != "fg" || fields[4] != "bg") {
        why = "expected 'shadow recolor fg <color> bg <color>'";
        return std::nullopt;
    }
    const std::optional<Color> fg = parse_color(fields[3]);
    const std::optional<Color> bg = parse_color(fields[5]);
    if (!fg || !bg) {
        why = "a colour is not 'default', '@<0-255>' or '#RRGGBB'";
        return std::nullopt;
    }
    return ShadowStyle::recolor(*fg, *bg);
}

ThemeParseResult failure(int line, std::string message) {
    ThemeParseResult result;
    result.error = ThemeParseError{line, std::move(message)};
    return result;
}

}  // namespace

bool is_theme_role_name(std::string_view name) noexcept {
    if (name.empty() || name.size() > kMaxThemeRoleNameBytes) return false;
    return std::all_of(name.begin(), name.end(), [](char c) { return c > ' ' && c < '\x7f'; });
}

std::string format_color(Color color) {
    if (color.is_default()) return "default";
    if (color.is_indexed()) return "@" + std::to_string(static_cast<int>(color.index()));
    constexpr std::string_view digits = "0123456789ABCDEF";
    std::string out = "#";
    for (const std::uint8_t channel : {color.r(), color.g(), color.b()}) {
        out += digits[channel / 16];
        out += digits[channel % 16];
    }
    return out;
}

std::optional<Color> parse_color(std::string_view text) noexcept {
    if (text == "default") return Color::default_color();
    if (!text.empty() && text.front() == '@') {
        const std::optional<std::uint8_t> index = parse_index(text.substr(1));
        if (!index) return std::nullopt;
        return Color::indexed(*index);
    }
    if (text.size() != 7 || text.front() != '#') return std::nullopt;
    std::array<std::uint8_t, 3> channels{};
    for (std::size_t channel = 0; channel < channels.size(); ++channel) {
        const int high = hex_value(text[1 + channel * 2]);
        const int low = hex_value(text[2 + channel * 2]);
        if (high < 0 || low < 0) return std::nullopt;
        channels[channel] = static_cast<std::uint8_t>(high * 16 + low);
    }
    return Color::rgb(channels[0], channels[1], channels[2]);
}

std::string serialize_theme(const Theme& theme) {
    const RoleRegistry& registry = theme.registry();
    std::vector<RoleId> roles(registry.size());
    for (std::size_t index = 0; index < roles.size(); ++index) roles[index] = static_cast<RoleId>(index);
    std::sort(roles.begin(), roles.end(),
              [&registry](RoleId a, RoleId b) { return registry.name(a) < registry.name(b); });
    std::string out{kThemeTextHeader};
    out += '\n';
    out += format_shadow(theme.shadow());
    out += '\n';
    for (const RoleId role : roles) {
        const std::string& name = registry.name(role);
        CKV_ASSERT(is_theme_role_name(name));
        out += name;
        out += ' ';
        out += format_style(theme.resolve(role));
        out += '\n';
    }
    return out;
}

ThemeParseResult parse_theme(std::string_view text, const Theme& base) {
    if (text.size() > kMaxThemeTextBytes) return failure(0, "the text is longer than a theme can be");
    if (text.empty() || text.back() != '\n') return failure(0, "the last line is not terminated by a newline");

    const RoleRegistry& registry = base.registry();
    Theme theme = base;
    std::vector<std::string> unknown;
    std::set<std::string_view> named;
    int line_number = 0;
    std::size_t position = 0;
    while (position < text.size()) {
        const std::size_t end = text.find('\n', position);
        const std::string_view line = text.substr(position, end - position);
        position = end + 1;
        ++line_number;
        if (!std::all_of(line.begin(), line.end(), [](char c) { return c >= ' ' && c < '\x7f'; }))
            return failure(line_number, "a control character or a byte outside printable ASCII");
        if (line_number == 1) {
            if (line != kThemeTextHeader) return failure(1, "expected the header 'ckvision-theme 1'");
            continue;
        }
        if (line.empty()) return failure(line_number, "an empty line");
        const std::optional<std::vector<std::string_view>> fields = fields_of(line);
        if (!fields) return failure(line_number, "fields must be separated by exactly one space");
        if (line_number == 2 && is_shadow_line(*fields)) {
            std::string why;
            const std::optional<ShadowStyle> shadow = parse_shadow_fields(*fields, why);
            if (!shadow) return failure(line_number, std::move(why));
            theme.set_shadow(*shadow);
            continue;
        }
        const std::string_view name = fields->front();
        if (!is_theme_role_name(name)) return failure(line_number, "the role name is longer than a role name can be");
        if (!named.insert(name).second) return failure(line_number, "the role is named twice");
        std::string why;
        const std::optional<Style> style = parse_style_fields(*fields, why);
        if (!style) return failure(line_number, std::move(why));
        const RoleId role = registry.find(name);
        if (role == kInvalidRole)
            unknown.emplace_back(name);
        else
            theme.set(role, *style);
    }
    ThemeParseResult result;
    result.theme = std::move(theme);
    result.unknown_roles = std::move(unknown);
    return result;
}

}  // namespace ckv::ui
