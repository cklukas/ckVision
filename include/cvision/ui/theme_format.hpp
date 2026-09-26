// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// The text form of a theme: what an application writes when it saves a theme
// the reader edited, and reads back when it starts again. Version 1 is a line
// naming the theme's shadow, then one line per role naming the style that role
// resolves to, so a saved theme is the whole truth D-007 promises of a printed
// one — no line depends on another, and nothing about the theme's appearance
// depends on which scheme happened to be the base.
// The grammar and its limits are documented in docs/themes-and-rendering.md,
// "Saving and loading a theme".
//
// Serializing is deterministic: roles are written in byte order of their
// names, never in interning order, which depends on the order widgets first
// attached. Parsing never throws; it either yields a whole theme or names the
// first line it refused, so a damaged or hostile file cannot half-apply.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cvision/core/color.hpp"
#include "cvision/ui/theme.hpp"

namespace ckv::ui {

// The first line of every version-1 theme text.
inline constexpr std::string_view kThemeTextHeader = "ckvision-theme 1";
// The longest text parse_theme reads, in bytes. A theme of a thousand roles
// is under a hundred kilobytes; a file ten times that size is not a theme.
inline constexpr std::size_t kMaxThemeTextBytes = std::size_t{1} << 20;
// The longest role name the format spells, in bytes.
inline constexpr std::size_t kMaxThemeRoleNameBytes = 128;

// Whether `name` can be written as a role in a theme text: one to
// kMaxThemeRoleNameBytes bytes, each printable ASCII other than the space
// that separates a line's fields. Every role ckVision interns qualifies, and
// the namespaced-name convention (`ckv.button.normal`) keeps a third party's
// roles inside it too.
bool is_theme_role_name(std::string_view name) noexcept;

// A colour as the theme text spells it: `default` for the terminal's own
// colour, `@<index>` (decimal, 0-255) for a palette entry, and `#RRGGBB`
// (uppercase hexadecimal) for a 24-bit colour — the golden format's spelling,
// so a style reads the same in a saved theme as in a frame dump.
std::string format_color(Color color);
// The colour `text` spells, or nullopt when it spells none. Hexadecimal
// digits are accepted in either case; an index must be canonical decimal
// (no sign, no leading zero) and at most 255.
std::optional<Color> parse_color(std::string_view text) noexcept;

// The version-1 text of `theme`: the header line, the shadow line, then one
// line for every role of the theme's registry, sorted by name, each ending in
// '\n':
//
//     shadow halve
//     shadow recolor fg <color> bg <color>
//     <role> fg <color> bg <color> attrs <attrs>[ underline <shape>][ ulcolor <color>]
//
// The shadow line is the first of its two forms for a halving shadow and the
// second for a recolouring one (ShadowStyle).
// Each line states the style the role resolves to — an override or the
// registry's fallback alike. Every role name must satisfy is_theme_role_name
// (asserted): a name the format cannot spell is a registry the application
// built outside the naming convention. An underline's shape and colour are
// written only while the style is underlined, as the Style contract reads
// them.
std::string serialize_theme(const Theme& theme);

// Where and why parse_theme refused a text.
struct ThemeParseError {
    // The 1-based line the problem is on, or 0 when the whole text is at
    // fault (too long, not terminated). The message is for people; its
    // wording is not a stable interface.
    int line = 0;
    std::string message;
};

// The outcome of parse_theme: a theme, or the first problem found.
struct ThemeParseResult {
    // The parsed theme, empty on failure.
    std::optional<Theme> theme;
    // The first problem found; default (line 0, empty message) on success.
    ThemeParseError error;
    // Well-formed lines naming a role the registry does not hold, in the
    // order they appear. They are not errors: a theme saved by a build with
    // more widgets, or before a role was retired, still loads, and the
    // caller decides whether to mention what it skipped. Empty on failure.
    std::vector<std::string> unknown_roles;
    // True when parsing succeeded.
    explicit operator bool() const noexcept { return theme.has_value(); }
};

// Reads a version-1 theme text over `base`: the result is a copy of `base`
// with every role the text names set to the style the text gives it, so a
// role the text does not mention — one added after the theme was saved —
// keeps the look of the scheme the application chose. The shadow line is read
// only as the text's second line; a text without one keeps `base`'s shadow,
// as it keeps the roles it does not name. The whole text is checked before
// anything is returned. Refused: a text longer than kMaxThemeTextBytes, one
// whose last line has no '\n', a wrong header, a control character or a byte
// outside printable ASCII anywhere, fields not separated by exactly one space,
// a role name is_theme_role_name rejects, a role named twice, a malformed
// shadow line, an unknown keyword, colour, attribute or underline shape, a
// duplicated attribute, and an underline shape or colour on a style that is
// not underlined. Never throws; `base`'s registry must outlive the result.
ThemeParseResult parse_theme(std::string_view text, const Theme& base);

}  // namespace ckv::ui
