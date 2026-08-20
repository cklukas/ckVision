// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/core/golden.hpp"

#include <algorithm>
#include <array>
#include <charconv>

namespace ckv::golden {
namespace {

constexpr std::array<std::string_view, 6> known_attrs = {
    "bold", "dim", "italic", "underline", "reverse", "strike"};

constexpr std::array<std::string_view, 3> known_shapes = {"block", "bar", "underline"};

// The underline shapes that are written down. The plain rule is what an
// underline is unless something says otherwise, so it is spelled by its
// absence and never appears here.
constexpr std::array<std::string_view, 4> known_underlines = {"double", "curly", "dotted",
                                                              "dashed"};

bool is_hex_digit_lower(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::vector<std::string_view> tokenize(std::string_view line) {
    std::vector<std::string_view> tokens;
    std::size_t pos = 0;
    while (pos <= line.size()) {
        const std::size_t space = line.find(' ', pos);
        const std::size_t end = (space == std::string_view::npos) ? line.size() : space;
        tokens.push_back(line.substr(pos, end - pos));
        if (space == std::string_view::npos) break;
        pos = space + 1;
    }
    return tokens;
}

// Canonical non-negative decimal only: digits, no sign, no leading zero
// (the literal "0" excepted) — anything else would parse without
// re-serializing byte-exactly.
bool parse_int(std::string_view token, int& out) {
    if (token.empty() || token[0] == '-' || token[0] == '+') return false;
    if (token.size() > 1 && token[0] == '0') return false;
    const char* first = token.data();
    const char* last = token.data() + token.size();
    const auto [ptr, ec] = std::from_chars(first, last, out);
    return ec == std::errc{} && ptr == last;
}

bool parse_color(std::string_view token, Color& out) {
    if (token == "default") {
        out = Color{};
        return true;
    }
    if (!token.empty() && token[0] == '@') {
        int index = -1;
        if (!parse_int(token.substr(1), index) || index > 255) return false;
        out = Color{};
        out.kind = Color::Kind::Indexed;
        out.index = static_cast<std::uint8_t>(index);
        return true;
    }
    if (token.size() != 7 || token[0] != '#') return false;
    int v[6];
    for (std::size_t i = 0; i < 6; ++i) {
        v[i] = hex_value(token[i + 1]);
        if (v[i] < 0) return false;
    }
    out = Color{};
    out.kind = Color::Kind::Rgb;
    out.r = static_cast<std::uint8_t>(v[0] * 16 + v[1]);
    out.g = static_cast<std::uint8_t>(v[2] * 16 + v[3]);
    out.b = static_cast<std::uint8_t>(v[4] * 16 + v[5]);
    return true;
}

void append_color(std::string& out, const Color& c) {
    switch (c.kind) {
        case Color::Kind::Default:
            out += "default";
            return;
        case Color::Kind::Indexed:
            out += '@';
            out += std::to_string(static_cast<int>(c.index));
            return;
        case Color::Kind::Rgb:
            break;
    }
    constexpr char digits[] = "0123456789ABCDEF";
    out += '#';
    for (const std::uint8_t byte : {c.r, c.g, c.b}) {
        out += digits[byte / 16];
        out += digits[byte % 16];
    }
}

bool known_attr(std::string_view name) {
    for (const std::string_view a : known_attrs)
        if (a == name) return true;
    return false;
}

bool known_shape(std::string_view name) {
    for (const std::string_view s : known_shapes)
        if (s == name) return true;
    return false;
}

bool known_underline(std::string_view name) {
    for (const std::string_view s : known_underlines)
        if (s == name) return true;
    return false;
}

// Splits attrs token ("-" or "a,b,c"); returns false on unknown or
// duplicate attribute names or empty list entries.
bool parse_attrs(std::string_view token, std::vector<std::string>& out) {
    out.clear();
    if (token == "-") return true;
    std::size_t pos = 0;
    while (pos <= token.size()) {
        const std::size_t comma = token.find(',', pos);
        const std::size_t end = (comma == std::string_view::npos) ? token.size() : comma;
        const std::string_view name = token.substr(pos, end - pos);
        if (name.empty() || !known_attr(name)) return false;
        for (const std::string& seen : out)
            if (seen == name) return false;
        out.emplace_back(name);
        if (comma == std::string_view::npos) break;
        pos = comma + 1;
    }
    return !out.empty();
}

struct Parser {
    std::vector<std::string_view> lines;
    std::size_t index = 0;  // next line to consume (0-based)
    Error error;

    bool fail(std::string message) {
        error.line = static_cast<int>(index);  // index is 1 past the consumed line
        error.message = std::move(message);
        return false;
    }

    bool next(std::string_view& out) {
        if (index >= lines.size()) {
            error.line = static_cast<int>(lines.size());
            error.message = "unexpected end of input";
            return false;
        }
        out = lines[index];
        ++index;
        return true;
    }
};

// Extracts the content between the leading and trailing '|' of a grid or
// stylemap line.
bool pipe_content(std::string_view line, std::string_view& out) {
    if (line.size() < 2 || line.front() != '|' || line.back() != '|') return false;
    out = line.substr(1, line.size() - 2);
    return true;
}

int style_index_of(char c) {
    const std::size_t pos = style_alphabet.find(c);
    return pos == std::string_view::npos ? -1 : static_cast<int>(pos);
}

}  // namespace

ParseResult parse(std::string_view text) {
    ParseResult result;
    Parser p;

    if (text.empty() || text.back() != '\n') {
        result.error = {0, "document must end with a final newline"};
        return result;
    }
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t nl = text.find('\n', pos);
        p.lines.push_back(text.substr(pos, nl - pos));
        pos = nl + 1;
    }

    Document doc;
    std::string_view line;

    if (!p.next(line)) {
        result.error = p.error;
        return result;
    }
    if (line != "ckvision-golden 1") {
        p.fail("expected header 'ckvision-golden 1'");
        result.error = p.error;
        return result;
    }

    const auto parse_all = [&]() -> bool {
        // frame
        if (!p.next(line)) return false;
        {
            const auto t = tokenize(line);
            if (t.size() != 3 || t[0] != "frame" || !parse_int(t[1], doc.cols) ||
                !parse_int(t[2], doc.rows) || doc.cols < 1 || doc.rows < 1)
                return p.fail("expected 'frame <cols> <rows>' with positive sizes");
        }

        // cursor
        if (!p.next(line)) return false;
        {
            const auto t = tokenize(line);
            if (t.size() == 2 && t[0] == "cursor" && t[1] == "hidden") {
                doc.cursor = Cursor{};
            } else if (t.size() == 4 && t[0] == "cursor" && parse_int(t[1], doc.cursor.col) &&
                       parse_int(t[2], doc.cursor.row) && known_shape(t[3])) {
                doc.cursor.visible = true;
                doc.cursor.shape = std::string(t[3]);
                if (doc.cursor.col < 0 || doc.cursor.col >= doc.cols || doc.cursor.row < 0 ||
                    doc.cursor.row >= doc.rows)
                    return p.fail("cursor position outside the frame");
            } else {
                return p.fail("expected 'cursor hidden' or 'cursor <col> <row> <shape>'");
            }
        }

        // styles
        int style_count = 0;
        if (!p.next(line)) return false;
        {
            const auto t = tokenize(line);
            if (t.size() != 2 || t[0] != "styles" || !parse_int(t[1], style_count) ||
                style_count < 0)
                return p.fail("expected 'styles <count>'");
            if (style_count > static_cast<int>(style_alphabet.size()))
                return p.fail("style count exceeds 62 (version-1 limit)");
        }
        doc.styles.reserve(static_cast<std::size_t>(style_count));
        for (int i = 0; i < style_count; ++i) {
            if (!p.next(line)) return false;
            const auto t = tokenize(line);
            StyleSpec style;
            int declared = -1;
            if (t.size() < 7 || !parse_int(t[0], declared) || declared != i || t[1] != "fg" ||
                !parse_color(t[2], style.fg) || t[3] != "bg" || !parse_color(t[4], style.bg) ||
                t[5] != "attrs" || !parse_attrs(t[6], style.attrs))
                return p.fail("expected '<index> fg <color> bg <color> attrs <attrs>' with "
                              "contiguous indices");
            // Both refinements describe an underline, so neither is
            // meaningful without one, and each is written only where it says
            // something the default does not — that is what keeps one
            // appearance from having two spellings.
            const bool underlined =
                std::find(style.attrs.begin(), style.attrs.end(), "underline") != style.attrs.end();
            std::size_t token = 7;
            if (token + 1 < t.size() && t[token] == "underline") {
                if (!underlined || !known_underline(t[token + 1]))
                    return p.fail("'underline <shape>' needs an underlined style and one of "
                                  "double, curly, dotted, dashed");
                style.underline = std::string(t[token + 1]);
                token += 2;
            }
            if (token + 1 < t.size() && t[token] == "ulcolor") {
                if (!underlined || !parse_color(t[token + 1], style.underline_color) ||
                    style.underline_color.kind == Color::Kind::Default)
                    return p.fail("'ulcolor <color>' needs an underlined style and a colour that "
                                  "is not the default");
                token += 2;
            }
            if (token != t.size())
                return p.fail("unexpected trailing tokens on a style line");
            doc.styles.push_back(std::move(style));
        }

        // grid
        if (!p.next(line)) return false;
        if (line != "grid") return p.fail("expected 'grid'");
        doc.grid.reserve(
            std::min(static_cast<std::size_t>(doc.rows), p.lines.size() - p.index));
        for (int r = 0; r < doc.rows; ++r) {
            if (!p.next(line)) return false;
            std::string_view content;
            if (!pipe_content(line, content))
                return p.fail("expected '|<row text>|' grid line");
            doc.grid.emplace_back(content);
        }

        // stylemap
        if (!p.next(line)) return false;
        if (line != "stylemap") return p.fail("expected 'stylemap'");
        doc.stylemap.reserve(
            std::min(static_cast<std::size_t>(doc.rows), p.lines.size() - p.index));
        for (int r = 0; r < doc.rows; ++r) {
            if (!p.next(line)) return false;
            std::string_view content;
            if (!pipe_content(line, content))
                return p.fail("expected '|<row style characters>|' stylemap line");
            if (content.size() != static_cast<std::size_t>(doc.cols))
                return p.fail("stylemap line must have exactly one character per column");
            for (const char c : content) {
                const int idx = style_index_of(c);
                if (idx < 0 || idx >= style_count)
                    return p.fail("stylemap character references an undeclared style");
            }
            doc.stylemap.emplace_back(content);
        }

        // rasters, then 'end'
        while (true) {
            if (!p.next(line)) return false;
            if (line == "end") break;
            const auto t = tokenize(line);
            RasterRegion region;
            if (t.size() != 15 || t[0] != "raster" || !parse_int(t[1], region.id) ||
                t[2] != "anchor" || !parse_int(t[3], region.anchor_col) ||
                !parse_int(t[4], region.anchor_row) || t[5] != "span" ||
                !parse_int(t[6], region.span_cols) || !parse_int(t[7], region.span_rows) ||
                t[8] != "pixels" || !parse_int(t[9], region.pixel_width) ||
                !parse_int(t[10], region.pixel_height) || t[11] != "hash" || t[12].empty() ||
                t[13] != "fallback" || (t[14] != "active" && t[14] != "hidden"))
                return p.fail("expected raster record or 'end'");
            for (const char c : t[12])
                if (!is_hex_digit_lower(c))
                    return p.fail("raster hash must be lowercase hex");
            if (region.id < 1) return p.fail("raster id must be positive");
            for (const RasterRegion& seen : doc.rasters)
                if (seen.id == region.id) return p.fail("duplicate raster id");
            if (region.span_cols < 1 || region.span_rows < 1 || region.pixel_width < 1 ||
                region.pixel_height < 1)
                return p.fail("raster span and pixel sizes must be positive");
            // Subtraction form: the additive spelling overflows int for
            // absurd anchors and would wrap into acceptance.
            if (region.anchor_col >= doc.cols || region.anchor_row >= doc.rows ||
                region.span_cols > doc.cols - region.anchor_col ||
                region.span_rows > doc.rows - region.anchor_row)
                return p.fail("raster region extends outside the frame");
            region.hash = std::string(t[12]);
            region.fallback_active = (t[14] == "active");
            doc.rasters.push_back(std::move(region));
        }

        if (p.index != p.lines.size()) {
            // Point at the first offending line, not at 'end' itself.
            p.error.line = static_cast<int>(p.index) + 1;
            p.error.message = "content after 'end'";
            return false;
        }
        return true;
    };

    if (!parse_all()) {
        result.error = p.error;
        return result;
    }
    result.document = std::move(doc);
    return result;
}

std::string serialize(const Document& doc) {
    std::string out;
    out += "ckvision-golden 1\n";
    out += "frame " + std::to_string(doc.cols) + ' ' + std::to_string(doc.rows) + '\n';
    if (doc.cursor.visible) {
        out += "cursor " + std::to_string(doc.cursor.col) + ' ' + std::to_string(doc.cursor.row) +
               ' ' + doc.cursor.shape + '\n';
    } else {
        out += "cursor hidden\n";
    }
    out += "styles " + std::to_string(doc.styles.size()) + '\n';
    for (std::size_t i = 0; i < doc.styles.size(); ++i) {
        const StyleSpec& style = doc.styles[i];
        out += std::to_string(i) + " fg ";
        append_color(out, style.fg);
        out += " bg ";
        append_color(out, style.bg);
        out += " attrs ";
        if (style.attrs.empty()) {
            out += '-';
        } else {
            for (std::size_t a = 0; a < style.attrs.size(); ++a) {
                if (a > 0) out += ',';
                out += style.attrs[a];
            }
        }
        if (!style.underline.empty()) out += " underline " + style.underline;
        if (style.underline_color.kind != Color::Kind::Default) {
            out += " ulcolor ";
            append_color(out, style.underline_color);
        }
        out += '\n';
    }
    out += "grid\n";
    for (const std::string& row : doc.grid) out += '|' + row + "|\n";
    out += "stylemap\n";
    for (const std::string& row : doc.stylemap) out += '|' + row + "|\n";
    for (const RasterRegion& region : doc.rasters) {
        out += "raster " + std::to_string(region.id) + " anchor " +
               std::to_string(region.anchor_col) + ' ' + std::to_string(region.anchor_row) +
               " span " + std::to_string(region.span_cols) + ' ' +
               std::to_string(region.span_rows) + " pixels " +
               std::to_string(region.pixel_width) + ' ' + std::to_string(region.pixel_height) +
               " hash " + region.hash + " fallback " +
               (region.fallback_active ? "active" : "hidden") + '\n';
    }
    out += "end\n";
    return out;
}

}  // namespace ckv::golden
