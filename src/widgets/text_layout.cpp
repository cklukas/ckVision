// Copyright (c) 2026 C. Klukas. All rights reserved.
// SPDX-License-Identifier: MIT
#include "cvision/widgets/text_layout.hpp"

#include <algorithm>

#include "cvision/core/text.hpp"

namespace ckv::widgets {
namespace {

// One indexable unit of a line — a grapheme cluster — reduced to what the
// wrap rule actually consults. Both entry points build this, so the rule
// itself exists once.
struct Piece {
    std::size_t offset = 0;  // where this piece starts, in the caller's units
    int width = 1;
    bool is_space = false;
};

// Nothing to decide: the whole line is one row. Answered before any
// per-cluster work, because a document that is not being wrapped must not pay
// to walk every grapheme in it on every relayout.
bool trivially_one_row(const WrapOptions& options) {
    return options.mode == WrapMode::None || options.width <= 0;
}

std::vector<WrapSegment> wrap_pieces(const std::vector<Piece>& pieces, std::size_t end_offset,
                                     const WrapOptions& options) {
    std::vector<WrapSegment> segments;
    const std::size_t count = pieces.size();
    const auto offset_of = [&](std::size_t index) {
        return index < count ? pieces[index].offset : end_offset;
    };

    if (options.mode == WrapMode::None || options.width <= 0 || count == 0) {
        segments.push_back(WrapSegment{0, end_offset});
        return segments;
    }

    // A continuation marker only earns its cell where content would still
    // fit beside it; at one cell wide, progress beats decoration.
    const int usable = options.continuation_reserve > 0 && options.width > options.continuation_reserve
                           ? options.width - options.continuation_reserve
                           : options.width;

    std::size_t begin = 0;
    while (begin < count) {
        std::size_t end = begin;
        int width = 0;
        std::size_t last_break = 0;  // one past the last space that fits
        while (end < count && width + pieces[end].width <= usable) {
            width += pieces[end].width;
            ++end;
            if (end < count && pieces[end - 1].is_space) last_break = end;
        }

        if (end >= count) {
            segments.push_back(WrapSegment{offset_of(begin), end_offset});
            return segments;
        }
        if (options.mode == WrapMode::Character) {
            // Break where the edge falls. A cluster wider than the whole row
            // still takes one, or nothing would be emitted and the loop would
            // not terminate.
            if (end == begin) end = begin + 1;
        } else if (last_break > begin) {
            end = last_break;  // break at the space, keeping the word whole
        } else if (end == begin) {
            // A single cluster wider than the whole row: take it anyway, or
            // nothing would ever be emitted and this would not terminate.
            end = begin + 1;
        } else {
            // No space to break at, so the word is wider than the row. It
            // stays whole and overflows: breaking it would hide that a path
            // or an identifier is wider than the window.
            while (end < count && !pieces[end].is_space) ++end;
        }
        segments.push_back(WrapSegment{offset_of(begin), offset_of(end)});
        begin = end;
    }
    if (segments.empty()) segments.push_back(WrapSegment{0, end_offset});
    return segments;
}

}  // namespace

std::vector<WrapSegment> wrap_graphemes(std::span<const std::string> graphemes, const WrapOptions& options) {
    if (trivially_one_row(options)) return {WrapSegment{0, graphemes.size()}};
    std::vector<Piece> pieces;
    pieces.reserve(graphemes.size());
    for (std::size_t i = 0; i < graphemes.size(); ++i)
        pieces.push_back(Piece{i, std::max(1, text::grapheme_width(graphemes[i])), graphemes[i] == " "});
    return wrap_pieces(pieces, graphemes.size(), options);
}

std::vector<WrapSegment> wrap_text(std::string_view text, const WrapOptions& options) {
    if (trivially_one_row(options)) return {WrapSegment{0, text.size()}};
    std::vector<Piece> pieces;
    for (std::size_t offset = 0; offset < text.size();) {
        const std::size_t next = text::grapheme_end(text, offset);
        const std::string_view cluster = text.substr(offset, next - offset);
        pieces.push_back(Piece{offset, std::max(1, text::grapheme_width(cluster)), cluster == " "});
        offset = next;
    }
    return wrap_pieces(pieces, text.size(), options);
}

ScrollGeometry resolve_scroll_geometry(Size bounds, ScrollbarPolicy vertical, ScrollbarPolicy horizontal,
                                        const std::function<Size(int)>& measure) {
    const auto shows = [](ScrollbarPolicy policy, int content, int viewport) {
        switch (policy) {
            case ScrollbarPolicy::Always:
                return true;
            case ScrollbarPolicy::Hidden:
                return false;
            case ScrollbarPolicy::Auto:
                break;
        }
        return content > viewport;
    };

    ScrollGeometry geometry;
    // Settle it by repetition rather than by guessing an order. Three passes
    // is past the point where anything still moves: each bar can flip at most
    // once, and the third pass confirms the pair.
    for (int pass = 0; pass < 3; ++pass) {
        const int viewport_width = std::max(0, bounds.width - (geometry.show_vertical ? 1 : 0));
        const int viewport_height = std::max(0, bounds.height - (geometry.show_horizontal ? 1 : 0));
        const Size content = measure(viewport_width);
        const bool next_vertical = shows(vertical, content.height, std::max(1, viewport_height));
        const bool next_horizontal = shows(horizontal, content.width, std::max(1, viewport_width));
        if (next_vertical == geometry.show_vertical && next_horizontal == geometry.show_horizontal) break;
        geometry.show_vertical = next_vertical;
        geometry.show_horizontal = next_horizontal;
    }
    geometry.viewport_width = std::max(0, bounds.width - (geometry.show_vertical ? 1 : 0));
    geometry.viewport_height = std::max(0, bounds.height - (geometry.show_horizontal ? 1 : 0));
    return geometry;
}

}  // namespace ckv::widgets
