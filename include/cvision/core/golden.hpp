// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
//
// Golden dump format, version 1 — the textual frame representation that
// serves as the project's specification medium (the decision log D-014).
// Format definition: docs/golden-format.md.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ckv::golden {

inline constexpr std::string_view style_alphabet =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

// The three things a colour can be, spelled `default`, `@<index>` and
// `#RRGGBB`. The index form is kept rather than resolved so that a dump says
// what a program asked for — "the palette's red" is a different fact from
// "this particular red", and only the first can be re-themed.
struct Color {
    enum class Kind : std::uint8_t { Default, Indexed, Rgb };
    Kind kind = Kind::Default;
    std::uint8_t index = 0;  // Indexed only
    std::uint8_t r = 0;      // Rgb only
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

struct StyleSpec {
    Color fg;
    Color bg;
    std::vector<std::string> attrs;  // subset of the known attribute names
    // The shape of the underline, written only when the style is underlined
    // and the shape is not the plain rule; empty otherwise. One of
    // "double", "curly", "dotted", "dashed".
    std::string underline;
    // The underline's own colour, written only when the style is underlined
    // and the rule does not simply follow the text.
    Color underline_color;
};

struct Cursor {
    bool visible = false;
    int col = 0;
    int row = 0;
    std::string shape;  // "block" | "bar" | "underline" when visible
};

struct RasterRegion {
    int id = 0;
    int anchor_col = 0;
    int anchor_row = 0;
    int span_cols = 0;
    int span_rows = 0;
    int pixel_width = 0;
    int pixel_height = 0;
    std::string hash;  // lowercase hex
    bool fallback_active = false;
};

struct Document {
    int cols = 0;
    int rows = 0;
    Cursor cursor;
    std::vector<StyleSpec> styles;
    std::vector<std::string> grid;      // raw row bytes, one entry per row
    std::vector<std::string> stylemap;  // alphabet characters, one entry per row
    std::vector<RasterRegion> rasters;
};

struct Error {
    int line = 0;  // 1-based line number, 0 when the whole input is at fault
    std::string message;
};

struct ParseResult {
    std::optional<Document> document;
    Error error;
    explicit operator bool() const { return document.has_value(); }
};

// Parses a complete dump. On failure, `document` is empty and `error`
// carries the first problem found. Never throws.
ParseResult parse(std::string_view text);

// Emits the canonical form. Serializing a parsed canonical document
// reproduces the input byte-exactly. Never throws.
std::string serialize(const Document& doc);

}  // namespace ckv::golden
