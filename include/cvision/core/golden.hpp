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

// The stylemap characters, in index order: the character at position i names style i, so a
// version-1 dump can declare at most 62 styles.
inline constexpr std::string_view style_alphabet =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

// The three things a colour can be, spelled `default`, `@<index>` and
// `#RRGGBB`. The index form is kept rather than resolved so that a dump says
// what a program asked for — "the palette's red" is a different fact from
// "this particular red", and only the first can be re-themed.
struct Color {
    // Which spelling applies: `default`, `@<index>` or `#RRGGBB`.
    enum class Kind : std::uint8_t { Default, Indexed, Rgb };
    // The kind, then its payload: the palette index (0-255) for Indexed and the red, green and
    // blue channels (0-255) for Rgb. Fields that do not belong to the kind are ignored when
    // serializing and zero after parsing.
    Kind kind = Kind::Default;
    std::uint8_t index = 0;  // Indexed only
    std::uint8_t r = 0;      // Rgb only
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

// One declared style: the line `<index> fg <color> bg <color> attrs <attrs>` plus its optional
// underline refinements.
struct StyleSpec {
    // Foreground and background colours.
    Color fg;
    Color bg;
    // Attribute names drawn from bold, dim, italic, underline, reverse and strike, without
    // duplicates, in the order written; empty is spelled "-".
    std::vector<std::string> attrs;  // subset of the known attribute names
    // The shape of the underline, written only when the style is underlined
    // and the shape is not the plain rule; empty otherwise. One of
    // "double", "curly", "dotted", "dashed".
    std::string underline;
    // The underline's own colour, written only when the style is underlined
    // and the rule does not simply follow the text.
    Color underline_color;
};

// The `cursor` line: `cursor hidden`, or `cursor <col> <row> <shape>`.
struct Cursor {
    // Whether the cursor is shown; when false the other fields are not written. When true, col and
    // row are its 0-based cell position (parse rejects one outside the frame) and shape is
    // "block", "bar" or "underline".
    bool visible = false;
    int col = 0;
    int row = 0;
    std::string shape;  // "block" | "bar" | "underline" when visible
};

// One `raster` record: a cell-anchored image region of the frame, recorded symbolically. It
// proves where an image is and which image it is, not that any terminal drew it.
struct RasterRegion {
    // Positive, unique within the dump.
    int id = 0;
    // The region's top-left cell (0-based) and its size in cells; parse requires a positive span
    // that lies entirely inside the frame.
    int anchor_col = 0;
    int anchor_row = 0;
    int span_cols = 0;
    int span_rows = 0;
    // The image's own size in pixels, both positive.
    int pixel_width = 0;
    int pixel_height = 0;
    // A non-empty lowercase-hex content hash of the image data, so a dump pins the image without
    // embedding it.
    std::string hash;  // lowercase hex
};

// One `link` record: a horizontal run of cells that are part of a hyperlink to
// one target (D-088). A run is maximal — the cells either side of it are not
// part of a link to the same target — so one frame has one spelling.
struct Link {
    // The run's first cell (0-based) and its length in cells, wide glyphs'
    // continuation columns included; parse requires a positive length that
    // stays inside the row.
    int col = 0;
    int row = 0;
    int cols = 0;
    // The hyperlink target, a valid terminal hyperlink
    // (is_valid_hyperlink_target) and therefore a single token.
    std::string target;
};

// A whole parsed or captured frame, field for field as the dump spells it. serialize writes the
// fields as they are without validating them; parse only ever produces documents that satisfy
// the format's rules.
struct Document {
    // The frame size in cells, both at least 1 in a parsed document.
    int cols = 0;
    int rows = 0;
    // The cursor line.
    Cursor cursor;
    // The declared styles; the stylemap character for styles[i] is style_alphabet[i].
    std::vector<StyleSpec> styles;
    // One entry per frame row, without the enclosing '|' delimiters. Grid rows are the cells'
    // text as raw bytes (their byte length is not tied to cols); stylemap rows hold exactly cols
    // style characters each.
    std::vector<std::string> grid;      // raw row bytes, one entry per row
    std::vector<std::string> stylemap;  // alphabet characters, one entry per row
    // The link records, in row-major order of their first cell.
    std::vector<Link> links;
    // The raster records, in the order written.
    std::vector<RasterRegion> rasters;
};

// Where and why a parse failed.
struct Error {
    // The line the problem was found on and a human-readable description of it. The message is
    // for people; its wording is not a stable interface.
    int line = 0;  // 1-based line number, 0 when the whole input is at fault
    std::string message;
};

// The outcome of parse: exactly one of a document or a meaningful error.
struct ParseResult {
    // The parsed document, empty on failure.
    std::optional<Document> document;
    // The first problem found; default (line 0, empty message) on success.
    Error error;
    // True when parsing succeeded.
    explicit operator bool() const { return document.has_value(); }
};

// Parses a complete dump. On failure, `document` is empty and `error`
// carries the first problem found. Never throws.
ParseResult parse(std::string_view text);

// Emits the canonical form. Serializing a parsed canonical document
// reproduces the input byte-exactly. Never throws.
std::string serialize(const Document& doc);

}  // namespace ckv::golden
